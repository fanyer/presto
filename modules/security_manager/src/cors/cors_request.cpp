/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef CORS_SUPPORT
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/cors/cors_request.h"
#include "modules/security_manager/src/cors/cors_manager.h"
#include "modules/security_manager/src/cors/cors_loader.h"
#include "modules/security_manager/src/cors/cors_preflight.h"
#include "modules/security_manager/src/cors/cors_utilities.h"

#include "modules/dom/domutils.h"
#include "modules/url/url2.h"
#include "modules/url/tools/url_util.h"
#include "modules/formats/hdsplit.h"
#include "modules/util/tempbuf.h"

/* static */ OP_STATUS
OpCrossOrigin_Request::Make(OpCrossOrigin_Request *&result, const URL &origin_url, const URL &request_url, const uni_char *method, BOOL with_credentials, BOOL is_anon)
{
	OpCrossOrigin_Request *request = OP_NEW(OpCrossOrigin_Request, (request_url, with_credentials));
	OpAutoPtr<OpCrossOrigin_Request> anchor_request(request);
	RETURN_OOM_IF_NULL(request);
	RETURN_IF_ERROR(request->method.Set(method));
	if (is_anon)
		RETURN_IF_ERROR(request->origin.Set(UNI_L("null")));
	else
		RETURN_IF_ERROR(OpCrossOrigin_Request::ToOriginString(request->origin, origin_url));

	request->is_anonymous = is_anon;
	request->SetIsSimple(OpCrossOrigin_Utilities::IsSimpleMethod(method));
	result = anchor_request.release();
	request->origin_url = origin_url;
	return OpStatus::OK;
}

OpCrossOrigin_Request::~OpCrossOrigin_Request()
{
	for (unsigned i = 0; i < headers.GetCount(); i++)
		OP_DELETEA(headers.Get(i));
	headers.Clear();

	for (unsigned i = 0; i < exposed_headers.GetCount(); i++)
		OP_DELETEA(exposed_headers.Get(i));
	exposed_headers.Clear();
	if (loader)
		loader->DetachRequest();

	Out();
}

/* static */ OP_STATUS
OpCrossOrigin_Request::ToOriginString(OpString &result, const URL &origin_url)
{
	TempBuffer buf;
	RETURN_IF_ERROR(DOM_Utils::GetSerializedOrigin(origin_url, buf));

	RETURN_IF_ERROR(result.Append(buf.GetStorage()));
	return OpStatus::OK;
}

OP_STATUS
OpCrossOrigin_Request::ToOriginString(OpString &result)
{
	if (is_anonymous)
		return result.Set(UNI_L("null"));
	OpString original_url_origin;
	URL original_url = redirects.GetCount() ? *redirects.Get(0) : url;
	RETURN_IF_ERROR(OpCrossOrigin_Request::ToOriginString(original_url_origin, original_url));
	OpString request_url_origin;
	RETURN_IF_ERROR(OpCrossOrigin_Request::ToOriginString(request_url_origin, url));
	if (original_url_origin.Compare(request_url_origin) != 0)
		return result.Set("null");
	return result.Set(origin);
}

OP_STATUS
OpCrossOrigin_Request::SetMethod(const uni_char *new_method)
{
	RETURN_IF_ERROR(method.Set(new_method));
	SetIsSimple(OpCrossOrigin_Utilities::IsSimpleMethod(new_method));

	return OpStatus::OK;
}

OP_STATUS
OpCrossOrigin_Request::AddHeader(const char *name, const char *value)
{
	OP_ASSERT(name);
	uni_char *name_str = NULL;
	RETURN_IF_ERROR(SetStr(name_str, name));
	OpAutoArray<uni_char> anchor_name(name_str);

	BOOL inserted = FALSE;
	for (unsigned i = 0; i < headers.GetCount(); i++)
	{
		int result = OpCrossOrigin_Utilities::CompareASCIICaseInsensitive(headers.Get(i), name_str, UINT_MAX, TRUE);
		if (result == 0)
			return OpStatus::OK;
		else if (result > 0)
		{
			RETURN_IF_ERROR(headers.Insert(i, name_str));
			inserted = TRUE;
			break;
		}
	}
	if (!inserted)
		RETURN_IF_ERROR(headers.Add(name_str));

	anchor_name.release();

	if (IsSimple() && !OpCrossOrigin_Utilities::IsSimpleRequestHeader(name_str, value, FALSE))
	{
		if (uni_stri_eq(name_str, "Content-Type"))
			SetHasNonSimpleContentType(TRUE);
		SetIsSimple(FALSE);
	}

	return OpStatus::OK;
}

#ifdef SELFTEST
void
OpCrossOrigin_Request::ClearHeaders()
{
	for (unsigned i = 0; i < headers.GetCount(); i++)
		OP_DELETEA(headers.Get(i));
	headers.Clear();
}
#endif // SELFTEST

BOOL
OpCrossOrigin_Request::CheckRedirect(const URL &moved_to_url)
{
	for (unsigned i = 0; i < redirects.GetCount(); i++)
		if (*redirects.Get(i) == moved_to_url)
			return FALSE;

	return TRUE;
}

void
OpCrossOrigin_Request::SetNetworkError()
{
	status = ErrorNetwork;
	Out();
}

OP_STATUS
OpCrossOrigin_Request::AddRedirect(const URL &moved_to_url)
{
	URL *next = OP_NEW(URL, ());
	RETURN_OOM_IF_NULL(next);
	*next = this->url;
	RETURN_IF_ERROR(redirects.Add(next));
	this->url = moved_to_url;
	return OpStatus::OK;
}

BOOL
OpCrossOrigin_Request::CanExposeResponseHeader(const uni_char *name, unsigned length)
{
	if (length == UINT_MAX)
		length = uni_strlen(name);

	/* [CORS: simple response headers] */
	if (uni_strnicmp("Cache-Control", name, length) == 0||
	    uni_strnicmp("Content-Language", name, length) == 0 ||
	    uni_strnicmp("Content-Type", name, length) == 0 ||
	    uni_strnicmp("Expires", name, length) == 0 ||
	    uni_strnicmp("Last-Modified", name, length) == 0||
	    uni_strnicmp("Pragma", name, length) == 0)
		return TRUE;

	for (unsigned i = 0; i < exposed_headers.GetCount(); i++)
	{
		int result = OpCrossOrigin_Utilities::CompareASCIICaseInsensitive(exposed_headers.Get(i), name, length, FALSE);
		if (result == 0)
			return TRUE;
	}

	return FALSE;
}

OP_STATUS
OpCrossOrigin_Request::AddResponseHeader(const uni_char *name, BOOL copy)
{
	OP_ASSERT(name);
	uni_char *name_str = copy ? UniSetNewStr(name) : const_cast<uni_char *>(name);
	RETURN_OOM_IF_NULL(name_str);
	OpAutoArray<uni_char> anchor_name(name_str);
	if (!copy)
		anchor_name.release();

	BOOL inserted = FALSE;
	for (unsigned i = 0; i < exposed_headers.GetCount(); i++)
	{
		int result = OpCrossOrigin_Utilities::CompareASCIICaseInsensitive(exposed_headers.Get(i), name, UINT_MAX, TRUE);
		if (result == 0)
			return OpStatus::OK;
		else if (result > 0)
		{
			RETURN_IF_ERROR(exposed_headers.Insert(i, name_str));
			inserted = TRUE;
			break;
		}
	}
	if (!inserted)
		RETURN_IF_ERROR(exposed_headers.Add(name_str));

	if (copy)
		anchor_name.release();

	return OpStatus::OK;
}

OP_STATUS
OpCrossOrigin_Request::PrepareRequestURL(URL *request_url)
{
	if (request_url)
		RETURN_IF_ERROR(request_url->SetAttribute(URL::KFollowCORSRedirectRules, URL::CORSRedirectVerify));
	else
		RETURN_IF_ERROR(url.SetAttribute(URL::KFollowCORSRedirectRules, URL::CORSRedirectVerify));

	URL_Custom_Header header_item;
	RETURN_IF_ERROR(header_item.name.Set("Origin"));

	OpString origin_str;
	RETURN_IF_ERROR(ToOriginString(origin_str));
	RETURN_IF_ERROR(header_item.value.Set(origin_str));

	if (request_url)
		RETURN_IF_ERROR(request_url->SetAttribute(URL::KAddHTTPHeader, &header_item));
	else
		RETURN_IF_ERROR(url.SetAttribute(URL::KAddHTTPHeader, &header_item));

	return OpStatus::OK;
}

/* static */ BOOL
OpCrossOrigin_Request::IsSimpleMethod(const uni_char *method)
{
	return OpCrossOrigin_Utilities::IsSimpleMethod(method);
}

/* static */ BOOL
OpCrossOrigin_Request::IsSimpleHeader(const char *name, const char *value)
{
	OpString name_str;
	RETURN_VALUE_IF_ERROR(name_str.Set(name), FALSE);
	return OpCrossOrigin_Utilities::IsSimpleRequestHeader(name_str.CStr(), value, FALSE);
}

#endif // CORS_SUPPORT
