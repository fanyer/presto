/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#ifdef CORS_SUPPORT

#include "modules/security_manager/include/security_manager.h"

OpSecurityManager_CrossOrigin::~OpSecurityManager_CrossOrigin()
{
	OP_DELETE(cross_origin_manager);
}

void
OpSecurityManager_CrossOrigin::ClearCrossOriginPreflightCache()
{
	if (cross_origin_manager)
		cross_origin_manager->InvalidatePreflightCache(TRUE);
}

/* static */ OP_BOOLEAN
OpSecurityManager_CrossOrigin::VerifyRedirect(URL &url, URL &moved_to)
{
	if (OpCrossOrigin_Manager *manager = g_secman_instance->GetCrossOriginManager(url))
	{
		BOOL matched_url;
		OP_BOOLEAN result = manager->CheckRedirect(url, moved_to, matched_url);
		if (result == OpBoolean::IS_TRUE && !matched_url)
		{
			/* An unregistered CORS redirect. Check if it is a redirect to a
			   cross-origin target. If it is, either initiate the redirect or
			   deny it. */
			OpSecurityContext source(url);
			OpSecurityContext target(moved_to);
			BOOL allowed;
			if (OpStatus::IsSuccess(g_secman_instance->CheckCrossOriginAccessible(source, target, allowed)) && allowed)
			{
				UINT32 cors_redirect = url.GetAttribute(URL::KFollowCORSRedirectRules);
				BOOL allow_redirect = cors_redirect == URL::CORSRedirectAllowSimpleCrossOrigin || cors_redirect == URL::CORSRedirectAllowSimpleCrossOriginAnon;
				const uni_char *method = UNI_L("GET");
				if (allow_redirect)
					switch (static_cast<HTTP_Method>(url.GetAttribute(URL::KHTTP_Method)))
					{
					case HTTP_METHOD_POST:
						method = UNI_L("POST");
						break;
					case HTTP_METHOD_GET:
						break;
					case HTTP_METHOD_HEAD:
						method = UNI_L("HEAD");
						break;
					default:
						OP_ASSERT(!"Not a simple method");
						allow_redirect = FALSE;
						break;
					}

				URL target_url = allow_redirect ? moved_to : url;
				BOOL disabled_cookie = url.GetAttribute(URL::KDisableProcessCookies);

				OpCrossOrigin_Request *request;
				if (OpStatus::IsSuccess(OpCrossOrigin_Request::Make(request, url, target_url, method, disabled_cookie, cors_redirect == URL::CORSRedirectAllowSimpleCrossOriginAnon)))
				{
					if (OpStatus::IsSuccess(request->PrepareRequestURL()))
					{
						if (allow_redirect)
							OpStatus::Ignore(moved_to.SetAttribute(URL::KFollowCORSRedirectRules, URL::CORSRedirectVerify));
						manager->RecordNetworkRedirect(request);
					}
					else
						OP_DELETE(request);

					return OpBoolean::IS_FALSE;
				}
			}
		}
		return result;
	}

	return OpBoolean::IS_TRUE;
}

OP_STATUS
OpSecurityManager_CrossOrigin::CheckCrossOriginAccessible(const OpSecurityContext& source, const OpSecurityContext& target, BOOL &allowed)
{
	/* A CORS admissibility predicate; check if an access (from the given source)
	   can be considered a cross-origin candidate. */

#if defined(GADGET_SUPPORT) && !defined(SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT)
	/* Gadgets have their own security model, including allowances for
	   cross-origin access. Do not consider them CORS candidates unless
	   SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT is defined
	   (see its documentation.) */
	if (source.IsGadget())
	{
		allowed = FALSE;
		return OpStatus::OK;
	}
#endif // GADGET_SUPPORT && !SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT

	allowed = target.GetURL().Type() != URL_DATA && !OpSecurityManager::OriginCheck(source.GetURL(), target);
	URLType target_type = target.GetURL().Type();
	if (allowed)
		allowed = target_type == URL_HTTP || target_type == URL_HTTPS;

	/* Note: this permits CORS requests from http to https. */
	return OpStatus::OK;
}

OP_STATUS
OpSecurityManager_CrossOrigin::CheckCrossOriginAccess(const OpSecurityContext& source, const OpSecurityContext& target, BOOL &allowed)
{
	allowed = FALSE;
	if (OpCrossOrigin_Request *request = source.GetCrossOriginRequest())
		if (OpCrossOrigin_Manager *manager = g_secman_instance->GetCrossOriginManager(source.GetURL()))
			return manager->VerifyCrossOriginAccess(request, target.GetURL(), allowed);

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityManager_CrossOrigin::CheckCrossOriginSecurity(const OpSecurityContext& source, const OpSecurityContext& target, OpSecurityCheckCallback* security_callback, OpSecurityCheckCancel **security_cancel)
{
	if (OpCrossOrigin_Request *request = source.GetCrossOriginRequest())
		if (OpCrossOrigin_Manager *manager = g_secman_instance->GetCrossOriginManager(source.GetURL()))
			return manager->HandleRequest(request, source.GetURL(), target.GetURL(), security_callback, security_cancel);

	security_callback->OnSecurityCheckSuccess(FALSE);
	return OpStatus::OK;
}

class OpCrossOrigin_SyncSecurityCallback
	: public OpSecurityCheckCallback
{
public:
	OpCrossOrigin_SyncSecurityCallback()
		: allowed(FALSE)
		, finished(FALSE)
		, error(OpStatus::OK)
	{
	}

	virtual void OnSecurityCheckSuccess(BOOL f, ChoicePersistenceType type)
	{
		finished = TRUE;
		allowed = f;
	}

	virtual void OnSecurityCheckError(OP_STATUS status)
	{
		error = status;
	}

	BOOL IsAllowed() { return allowed; }
	BOOL IsFinished() { return finished; }
	OP_STATUS GetError() { return error; }

private:
	BOOL allowed;
	BOOL finished;
	OP_STATUS error;
};

/* static */ OP_STATUS
OpSecurityManager_CrossOrigin::CheckCrossOriginSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL &allowed)
{
	allowed = FALSE;
	if (OpCrossOrigin_Request *request = source.GetCrossOriginRequest())
		if (OpCrossOrigin_Manager *manager = g_secman_instance->GetCrossOriginManager(source.GetURL()))
		{
			OP_ASSERT(request->IsSimple());
			OpCrossOrigin_SyncSecurityCallback callback;
			RETURN_IF_ERROR(manager->HandleRequest(request, source.GetURL(), target.GetURL(), &callback, NULL));
			OP_ASSERT(callback.IsFinished());
			allowed = callback.IsAllowed();
			RETURN_IF_ERROR(callback.GetError());
		}

	return OpStatus::OK;
}

OpCrossOrigin_Manager *
OpSecurityManager_CrossOrigin::GetCrossOriginManager(const URL &origin_url)
{
	if (!cross_origin_manager)
		if (OpStatus::IsError(OpCrossOrigin_Manager::Make(cross_origin_manager)))
			return NULL;

	return cross_origin_manager;
}

#endif // CORS_SUPPORT
