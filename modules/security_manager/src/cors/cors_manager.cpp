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
#include "modules/security_manager/src/cors/cors_manager.h"
#include "modules/security_manager/src/cors/cors_request.h"
#include "modules/security_manager/src/cors/cors_preflight.h"
#include "modules/security_manager/src/cors/cors_loader.h"
#include "modules/security_manager/src/cors/cors_utilities.h"
#include "modules/console/opconsoleengine.h"

/* static */ OP_STATUS
OpCrossOrigin_Manager::Make(OpCrossOrigin_Manager *&result, unsigned default_max_age)
{
	OpCrossOrigin_Manager *instance = OP_NEW(OpCrossOrigin_Manager, ());
	RETURN_OOM_IF_NULL(instance);
	if (OpStatus::IsError(OpCrossOrigin_PreflightCache::Make(instance->pre_cache, default_max_age)))
	{
		OP_DELETE(instance);
		return OpStatus::ERR_NO_MEMORY;
	}
	result = instance;
	return OpStatus::OK;
}

OpCrossOrigin_Manager::~OpCrossOrigin_Manager()
{
	OP_DELETE(pre_cache);
	active_requests.RemoveAll();
	/* If the user of CORS doesn't fetch out all the CORS request objects for the
	   requests that the network initiate for redirects (if allowed to by said user),
	   then this list will not be empty. Empty if everyone follows protocol. */
	OP_ASSERT(network_initiated_redirects.Empty() || !"Redirect list should have been emptied by CORS users");
	network_initiated_redirects.Clear();
}

/* static */ OP_STATUS
OpCrossOrigin_Manager::LogFailure(const uni_char *context, const URL &url, FailureReason reason)
{
	if (!g_console->IsLogging())
		return OpStatus::OK;

	OpConsoleEngine::Message message(OpConsoleEngine::EcmaScript, OpConsoleEngine::Information);
	const uni_char *str = NULL;
	switch (reason)
	{
	case IncorrectResponseCode:
		str = UNI_L("Unexpected response code");
		break;
	case MissingAllowOrigin:
		str = UNI_L("Access denied, Access-Control-Allow-Origin not included in response.");
		break;
	case MultipleAllowOrigin:
		str = UNI_L("More than one Access-Control-Allow-Origin header value");
		break;
	case NoMatchAllowOrigin:
		str = UNI_L("Access denied, Origin: not matching.");
		break;
	case InvalidAllowCredentials:
		str = UNI_L("Unrecognised Access-Control-Allow-Credentials value.");
		break;
	case UnsupportedScheme:
		str = UNI_L("Unrecognised scheme used in request or redirect.");
		break;
	case RedirectLoop:
		str = UNI_L("Redirect loop detected.");
		break;
	case UserinfoRedirect:
		str = UNI_L("Redirect URL contained userinfo, not allowed.");
		break;
	}
	RETURN_IF_ERROR(message.message.Set(str));
	RETURN_IF_ERROR(message.context.AppendFormat("Cross-origin resource sharing check [%s]", context));
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, message.url));

	TRAPD(status, g_console->PostMessageL(&message));
	return status;
}

OP_STATUS
OpCrossOrigin_Manager::VerifyCrossOriginAccess(OpCrossOrigin_Request *request, const URL &response_url, BOOL &allowed)
{
	DeregisterRequest(request);
	OP_BOOLEAN result;
	FailureReason reason;
	RETURN_IF_ERROR(result = ResourceSharingCheck(*request, response_url, reason));
	if (result == OpBoolean::IS_FALSE)
	{
		OpString response_origin;
		RETURN_IF_ERROR(OpCrossOrigin_Request::ToOriginString(response_origin, response_url));
		if (response_origin.Compare(request->GetOrigin()) != 0)
		{
			RETURN_IF_ERROR(LogFailure(UNI_L("response verification"), response_url, reason));
			RETURN_IF_ERROR(CacheNetworkFailure(*request, TRUE));
		}
		else
			result = OpBoolean::IS_TRUE;
	}
	else
	{
		OpString header;
		RETURN_IF_ERROR(OpCrossOrigin_Utilities::GetHeaderValue(response_url, "Access-Control-Expose-Headers", header));

		OpAutoVector<OpString> headers;
		BOOL matched_headers = FALSE;
		OP_STATUS status = OpCrossOrigin_Utilities::ParseAndMatchHeaderList(*request, header.CStr(), headers, matched_headers, TRUE);
		RETURN_IF_MEMORY_ERROR(status);
		if (OpStatus::IsSuccess(status))
			for (unsigned i = 0; i < headers.GetCount(); i++)
				RETURN_IF_ERROR(request->AddResponseHeader(headers.Get(i)->CStr(), TRUE));

		request->SetStatus(OpCrossOrigin_Request::Successful);
	}

	allowed = result == OpBoolean::IS_TRUE;
	return OpStatus::OK;
}

/* static */ OP_BOOLEAN
OpCrossOrigin_Manager::ResourceSharingCheck(const OpCrossOrigin_Request &request, const URL &url, FailureReason &reason)
{
	/* Resource sharing check: [CORS: 7.2] */
	OpString header;
	RETURN_IF_ERROR(OpCrossOrigin_Utilities::GetHeaderValue(url, "Access-Control-Allow-Origin", header));
	if (!header.CStr())
	{
		reason = MissingAllowOrigin;
		return OpBoolean::IS_FALSE;
	}
	if (!OpCrossOrigin_Utilities::HasSingleFieldValue(header.CStr(), TRUE))
	{
		reason = MultipleAllowOrigin;
		return OpBoolean::IS_FALSE;
	}
	if (!request.WithCredentials() && OpCrossOrigin_Utilities::HasFieldValue(header.CStr(), UNI_L("*"), 1))
		return OpBoolean::IS_TRUE;

	if (!OpCrossOrigin_Utilities::HasFieldValue(header.CStr(), request.GetOrigin()))
	{
		reason = NoMatchAllowOrigin;
		return OpBoolean::IS_FALSE;
	}

	if (request.WithCredentials())
	{
		RETURN_IF_ERROR(OpCrossOrigin_Utilities::GetHeaderValue(url, "Access-Control-Allow-Credentials", header));
		if (!header.CStr() || !OpCrossOrigin_Utilities::HasSingleFieldValue(header, FALSE) || !OpCrossOrigin_Utilities::HasFieldValue(header, UNI_L("true"), 4))
		{
			reason = InvalidAllowCredentials;
			return OpBoolean::IS_FALSE;
		}
	}
	return OpBoolean::IS_TRUE;
}

BOOL
OpCrossOrigin_Manager::RequiresPreflight(const OpCrossOrigin_Request &request)
{
	if (force_preflight || request.IsPreflightRequired())
		return TRUE;

	if (!OpCrossOrigin_Utilities::IsSimpleMethod(request.GetMethod()) && (!pre_cache || !pre_cache->MethodMatch(request)))
		return TRUE;

	for (unsigned i = 0; i < request.GetHeaders().GetCount(); i++)
	{
		const uni_char *header = request.GetHeaders().Get(i);
		if (!OpCrossOrigin_Utilities::IsSimpleRequestHeader(header, NULL, !request.HasNonSimpleContentType()) && !pre_cache->HeaderMatch(request, header))
			return TRUE;
	}

	return FALSE;
}

/* static */ BOOL
OpCrossOrigin_Manager::IsSimpleCrossOriginRequest(const OpCrossOrigin_Request &request)
{
	return !request.IsPreflightRequired() && request.IsSimple();
}

OP_BOOLEAN
OpCrossOrigin_Manager::CacheNetworkFailure(OpCrossOrigin_Request &request, BOOL update_cache)
{
	URL request_url = request.GetURL();
	if (update_cache && pre_cache)
		pre_cache->RemoveOriginRequests(request.GetOrigin(), request_url);

#ifdef SECMAN_CROSS_ORIGIN_STRICT_FAILURE
	RETURN_IF_ERROR(OpCrossOrigin_Utilities::RemoveCookiesByURL(request_url));
#endif // SECMAN_CROSS_ORIGIN_STRICT_FAILURE
	request.SetStatus(OpCrossOrigin_Request::ErrorNetwork);
	return OpBoolean::IS_FALSE;
}

OP_BOOLEAN
OpCrossOrigin_Manager::HandlePreflightResponse(OpCrossOrigin_Request &request, const URL &target_url)
{
	FailureReason reason;
	OP_BOOLEAN result;
	RETURN_IF_ERROR(result = ResourceSharingCheck(request, target_url, reason));
	if (result == OpBoolean::IS_FALSE)
	{
		RETURN_IF_ERROR(LogFailure(UNI_L("preflight response"), target_url, reason));
		return CacheNetworkFailure(request, TRUE);
	}

	OpString header;
	RETURN_IF_ERROR(OpCrossOrigin_Utilities::GetHeaderValue(target_url, "Access-Control-Allow-Methods", header));

	/* Parse all the methods, but check if there is a match while doing so.
	   Delay reporting the error until allowed in the algorithm. (step 3) */
	OpAutoVector<OpString> methods;
	BOOL matched_method = FALSE;
	OP_STATUS status = OpCrossOrigin_Utilities::ParseAndMatchMethodList(request, header.CStr(), methods, matched_method, TRUE);
	RETURN_IF_MEMORY_ERROR(status);
	if (OpStatus::IsError(status))
		return CacheNetworkFailure(request, TRUE);

	/* Same for headers (step 5). */
	RETURN_IF_ERROR(OpCrossOrigin_Utilities::GetHeaderValue(target_url, "Access-Control-Allow-Headers", header));

	OpAutoVector<OpString> headers;
	BOOL matched_headers = FALSE;
	RETURN_IF_MEMORY_ERROR(status = OpCrossOrigin_Utilities::ParseAndMatchHeaderList(request, header.CStr(), headers, matched_headers, TRUE));
	if (OpStatus::IsError(status))
		return CacheNetworkFailure(request, TRUE);

	/* Step 6 */
	if (!matched_method && !OpCrossOrigin_Utilities::IsSimpleMethod(request.GetMethod()))
		return CacheNetworkFailure(request, TRUE);

	/* Step 7 */
	if (!matched_headers)
		return CacheNetworkFailure(request, TRUE);

	if (pre_cache && pre_cache->AllowUpdates(request))
	{
		unsigned max_age = pre_cache->GetDefaultMaxAge();
		RETURN_IF_ERROR(OpCrossOrigin_Utilities::GetHeaderValue(target_url, "Access-Control-Max-Age", header));
		if (header.CStr() && OpCrossOrigin_Utilities::HasSingleFieldValue(header.CStr(), TRUE))
		{
			const uni_char *max_age_str = header.CStr();
			if (OpCrossOrigin_Utilities::StripLWS(max_age_str))
			{
				int val = uni_atoi(max_age_str);
				if (val >= 0)
					max_age = MIN(pre_cache->GetMaximumMaxAge(), static_cast<unsigned>(val));
				else
					max_age = 0;
			}
		}
		RETURN_IF_ERROR(pre_cache->UpdateMethodCache(methods, request, max_age));
		RETURN_IF_ERROR(pre_cache->UpdateHeaderCache(headers, request, max_age));
	}

	request.SetStatus(OpCrossOrigin_Request::PreflightComplete);
	return OpBoolean::IS_TRUE;
}

void
OpCrossOrigin_Manager::InvalidatePreflightCache(BOOL expire_all)
{
	if (pre_cache)
		pre_cache->InvalidateCache(expire_all);
}

static BOOL
IsSupportedScheme(URLType source, URLType t)
{
	/* Strict; only support same-protocol redirects. */
	return (source == t);
}

OP_BOOLEAN
OpCrossOrigin_Manager::HandleRedirect(OpCrossOrigin_Request *request, URL &moved_to_url)
{
	URLType type = moved_to_url.Type();

	/* Check if compatible/supported scheme. */
	if (!IsSupportedScheme(request->GetURL().Type(), type))
	{
		RETURN_IF_ERROR(LogFailure(UNI_L("request redirect"), request->GetURL(), UnsupportedScheme));
		return CacheNetworkFailure(*request, TRUE);
	}

	if (!request->CheckRedirect(moved_to_url))
	{
		RETURN_IF_ERROR(LogFailure(UNI_L("request redirect"), request->GetURL(), RedirectLoop));
		return CacheNetworkFailure(*request, TRUE);
	}

	OpString user_password;
	RETURN_IF_ERROR(moved_to_url.GetAttribute(URL::KUserName, user_password));
	if (!user_password.IsEmpty())
	{
		RETURN_IF_ERROR(LogFailure(UNI_L("request redirect"), request->GetURL(), UserinfoRedirect));
		return CacheNetworkFailure(*request, TRUE);
	}

	/* Note: failure does not trigger a cache-network error. */
	FailureReason reason;
	OP_BOOLEAN result;

	RETURN_IF_ERROR(result = ResourceSharingCheck(*request, request->GetURL(), reason));
	if (result == OpBoolean::IS_FALSE)
	{
		RETURN_IF_ERROR(LogFailure(UNI_L("request redirect resource sharing check"), request->GetURL(), reason));
		return OpBoolean::IS_FALSE;
	}

	/* Record redirect as current & follow it. */
	RETURN_IF_ERROR(request->AddRedirect(moved_to_url));
	RETURN_IF_ERROR(request->PrepareRequestURL());
	RETURN_IF_ERROR(moved_to_url.SetAttribute(URL::KFollowCORSRedirectRules, URL::CORSRedirectVerify));
	return OpBoolean::IS_TRUE;
}

OP_STATUS
OpCrossOrigin_Manager::HandleRequest(OpCrossOrigin_Request *request, const URL &origin_url, const URL &target_url, OpSecurityCheckCallback *security_callback, OpSecurityCheckCancel **security_cancel)
{
	if (request->IsInRedirect())
	{
		request->SetInRedirect(FALSE);
		security_callback->OnSecurityCheckSuccess(request->AllowsRedirect());
	}
	else if (IsSimpleCrossOriginRequest(*request))
	{
		request->SetStatus(OpCrossOrigin_Request::Active);
		RETURN_IF_ERROR(request->PrepareRequestURL(&const_cast<URL &>(target_url)));
		RecordActiveRequest(request);
		security_callback->OnSecurityCheckSuccess(TRUE);
	}
	else if (request->GetStatus() != OpCrossOrigin_Request::PreflightComplete && RequiresPreflight(*request))
	{
		OpCrossOrigin_Loader *loader;
		RETURN_IF_ERROR(OpCrossOrigin_Loader::Make(loader, this, request, security_callback));
		/* Start preflight step; it will notify on completion. */
		OP_STATUS status = loader->Start(origin_url);
		if (OpStatus::IsError(status))
		{
			loader->CancelSecurityCheck();
			request->SetStatus(OpCrossOrigin_Request::ErrorNetwork);
			return status;
		}
		if (security_cancel)
			*security_cancel = loader;
		request->SetStatus(OpCrossOrigin_Request::PreflightActive);
	}
	else
	{
		request->SetStatus(OpCrossOrigin_Request::Active);
		RETURN_IF_ERROR(request->PrepareRequestURL(&const_cast<URL &>(target_url)));
		RecordActiveRequest(request);
		security_callback->OnSecurityCheckSuccess(TRUE);
	}

	return OpStatus::OK;
}

void
OpCrossOrigin_Manager::RecordActiveRequest(OpCrossOrigin_Request *r)
{
	r->Into(&active_requests);
}

void
OpCrossOrigin_Manager::RecordNetworkRedirect(OpCrossOrigin_Request *r)
{
	r->Into(&network_initiated_redirects);
}

OpCrossOrigin_Request *
OpCrossOrigin_Manager::FindNetworkRedirect(const URL &url)
{
	for (OpCrossOrigin_Request *req = network_initiated_redirects.First(); req; req = req->Suc())
	{
		if (req->GetOriginURL() == url)
		{
			req->Out();
			return req;
		}
	}
	return NULL;
}

void
OpCrossOrigin_Manager::DeregisterRequest(OpCrossOrigin_Request *r)
{
	r->Out();
}

OP_BOOLEAN
OpCrossOrigin_Manager::CheckRedirect(URL &url, URL &moved_to, BOOL &matched_url)
{
	/* A redirect is being executed; check if it can go ahead. Dependent on
	   CORS request being recorded before they're started (and marked as such.) */
	matched_url = FALSE;
	for (OpCrossOrigin_Request *req = active_requests.First(); req; req = req->Suc())
	{
		if (req->GetURL() == url)
		{
			OP_BOOLEAN result = HandleRedirect(req, moved_to);
			if (result == OpBoolean::IS_FALSE)
				req->Out();
			matched_url = TRUE;
			return result;
		}
	}
	for (OpCrossOrigin_Request *req = network_initiated_redirects.First(); req; req = req->Suc())
	{
		if (req->GetURL() == url)
		{
			OP_BOOLEAN result = HandleRedirect(req, moved_to);
			if (result == OpBoolean::IS_FALSE)
				req->Out();
			matched_url = TRUE;
			return result;
		}
	}

	return OpBoolean::IS_TRUE;
}

#endif // CORS_SUPPORT
