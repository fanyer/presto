/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef CORS_SUPPORT
#include "modules/security_manager/src/cors/cors_loader.h"
#include "modules/security_manager/src/cors/cors_request.h"
#include "modules/security_manager/src/cors/cors_utilities.h"
#include "modules/url/url2.h"

#include "modules/url/tools/url_util.h"
#include "modules/formats/hdsplit.h"


/* static */ OP_STATUS
OpCrossOrigin_Loader::Make(OpCrossOrigin_Loader *&loader, OpCrossOrigin_Manager *manager, OpCrossOrigin_Request *request, OpSecurityCheckCallback *security_callback)
{
	RETURN_OOM_IF_NULL(loader = OP_NEW(OpCrossOrigin_Loader, ()));
	URL url = g_url_api->GetURL(request->GetURL(), UNI_L(""), TRUE);
	RETURN_IF_ERROR(MakeRequestURL(url, *request));

	loader->url = url;
	loader->request = request;
	loader->manager = manager;
	loader->security_callback = security_callback;
	return OpStatus::OK;
}

/* virtual */
OpCrossOrigin_Loader::~OpCrossOrigin_Loader()
{
	if (mh)
	{
		url.StopLoading(mh);
		mh->UnsetCallBacks(this);
		OP_DELETE(mh);
	}
	if (request)
		request->SetLoader(NULL);
}

OP_STATUS
OpCrossOrigin_Loader::Start(const URL &ref_url)
{
	mh = OP_NEW(MessageHandler, (NULL));
	RETURN_OOM_IF_NULL(mh);
	RETURN_IF_ERROR(SetCallbacks());
	CommState state = url.Reload(mh, request->IsAnonymous() ? URL() : ref_url, FALSE, FALSE, FALSE);
	if (state == COMM_LOADING)
	{
		request->SetLoader(this);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

void
OpCrossOrigin_Loader::SignalListener()
{
	if (OpStatus::IsError(result))
		security_callback->OnSecurityCheckError(result);
	else if (result == OpBoolean::IS_FALSE)
		security_callback->OnSecurityCheckSuccess(FALSE);
	else
	{
		/* Preflight has completed; check it. */
		result = manager->HandlePreflightResponse(*request, url);
		if (OpStatus::IsError(result))
			security_callback->OnSecurityCheckError(result);
		else
			security_callback->OnSecurityCheckSuccess(result == OpBoolean::IS_TRUE);
	}
}

OP_STATUS
OpCrossOrigin_Loader::SetCallbacks()
{
	URL_ID id = url.Id(TRUE);
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_DATA_LOADED, id));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_HEADER_LOADED, id));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_NOT_MODIFIED, id));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_LOADING_FAILED, id));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_MOVED, id));

	return OpStatus::OK;
}

/* virtual */ void
OpCrossOrigin_Loader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	BOOL stopping = FALSE;

	switch (msg)
	{
	case MSG_URL_DATA_LOADED:
	case MSG_HEADER_LOADED:
	case MSG_NOT_MODIFIED:
	case MSG_URL_LOADING_FAILED:
		if (url.Status(TRUE) != URL_LOADING)
		{
			stopping = TRUE;
			unsigned response_code = url.GetAttribute(URL::KHTTP_Response_Code, TRUE);
			switch (response_code)
			{
			case HTTP_NOT_MODIFIED:
			case HTTP_OK:
				result = OpBoolean::IS_TRUE;
				break;
			default:
				result = OpBoolean::IS_FALSE;
				break;
			}
		}
		break;
	case MSG_URL_MOVED:
	{
		URL moved_to = url.GetAttribute(URL::KMovedToURL, TRUE);
		if (!moved_to.IsEmpty())
		{
			stopping = TRUE;
			result = OpBoolean::IS_FALSE;
			moved_to.StopLoading(mh);
		}
		break;
	}
	}

	if (!stopped && stopping)
	{
		stopped = stopping;
		mh->UnsetCallBacks(this);
		url.StopLoading(mh);
		if (request)
		{
			if (result == OpBoolean::IS_FALSE)
				request->SetStatus(OpCrossOrigin_Request::ErrorNetwork);

			SignalListener();
		}
		OP_DELETE(this);
	}
}

/* static */ OP_STATUS
OpCrossOrigin_Loader::MakeRequestURL(URL &url, const OpCrossOrigin_Request &request)
{
	OpString8 method8;

	OP_ASSERT(request.GetStatus() == OpCrossOrigin_Request::Initial || request.GetStatus() == OpCrossOrigin_Request::Active);

	RETURN_IF_ERROR(method8.Set("OPTIONS"));

	RETURN_IF_ERROR(url.SetAttribute(URL::KHTTPSpecialMethodStr, method8));

	URL_Custom_Header header_item;

	RETURN_IF_ERROR(header_item.name.Set("Origin"));
	RETURN_IF_ERROR(header_item.value.Set(request.GetOrigin()));
	RETURN_IF_ERROR(url.SetAttribute(URL::KAddHTTPHeader, &header_item));

	RETURN_IF_ERROR(header_item.name.Set("Access-Control-Request-Method"));
	RETURN_IF_ERROR(header_item.value.Set(request.GetMethod()));
	RETURN_IF_ERROR(url.SetAttribute(URL::KAddHTTPHeader, &header_item));

	unsigned count;
	if ((count = request.GetHeaders().GetCount()) > 0)
	{
		RETURN_IF_ERROR(header_item.name.Set("Access-Control-Request-Headers"));
		header_item.value.Empty();
		OpString8 headers_str;
		for (unsigned i = 0; i < count; i++)
		{
			OpString8 headers8;
			RETURN_IF_ERROR(headers8.SetUTF8FromUTF16(request.GetHeaders().Get(i)));
			RETURN_IF_ERROR(headers_str.Append(headers8));
			if (i < count - 1)
				RETURN_IF_ERROR(headers_str.Append(", "));
		}
		RETURN_IF_ERROR(header_item.value.Set(headers_str.CStr()));
		RETURN_IF_ERROR(url.SetAttribute(URL::KAddHTTPHeader, &header_item));
	}

	return OpStatus::OK;
}

void
OpCrossOrigin_Loader::DetachRequest()
{
	/* Drop connection to request; loader will terminate itself. */
	request = NULL;
}

void
OpCrossOrigin_Loader::CancelSecurityCheck()
{
	if (!stopped)
	{
		stopped = TRUE;
		result = OpStatus::ERR;
		if (mh)
		{
			url.StopLoading(mh);
			mh->UnsetCallBacks(this);
		}
		SignalListener();
	}
	if (request)
		request->SetStatus(OpCrossOrigin_Request::ErrorAborted);
	OP_DELETE(this);
}

#endif // CORS_SUPPORT
