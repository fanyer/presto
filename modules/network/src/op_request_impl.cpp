/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/network/src/op_request_impl.h"
#include "modules/network/src/op_response_impl.h"
#include "modules/network/src/op_generated_response_impl.h"
#include "modules/url/url2.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/url/tools/url_util.h"
#include "modules/url/url_tools.h"
#include "modules/url/url_module.h"
#include "modules/locale/locale-enum.h"
#include "modules/cache/url_dd.h"
#include "modules/url/url_dynattr.h"
#include "modules/url/special_attrs.h"
#include "modules/formats/keywordman.h"
#include "modules/url/tools/arrays.h"


OP_STATUS OpRequestImpl::Make(OpRequest *&req, OpRequestListener *listener, OpURL url, URL_CONTEXT_ID context_id, OpURL referrer)
{
	OpRequestImpl *temp;
	OpString8 urlpath;
	OP_STATUS op_err = url.GetNameWithFragmentUsername(urlpath, TRUE, OpURL::PasswordVisible_NOT_FOR_UI);
	if (OpStatus::IsError(op_err))
		return OpStatus::ERR_NO_MEMORY;

	OpURL fresh_url = OpURL(g_url_api->GetURL(referrer.GetURL(), urlpath, FALSE, context_id));
	temp = OP_NEW(OpRequestImpl,(fresh_url));

	if (temp)
	{
		if (OpStatus::IsError(temp->AddListener(listener)))
		{
			OP_DELETE(temp);
			return OpStatus::ERR_NO_MEMORY;
		}
		URL ref = referrer.GetURL();
		TRAPD(op_err, temp->m_url.SetAttributeL(URL::KReferrerURL, ref));
		OpStatus::Ignore(op_err);

		req = temp;
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

OpRequestImpl::OpRequestImpl(OpURL url)
:m_url(url.GetURL())
,m_op_url(url)
,m_original_op_url(url)
,m_last_response_was_redirect(FALSE)
,m_message_handler(g_main_message_handler)
,m_request_sent(FALSE)
,m_header_loaded(FALSE)
,m_finished(FALSE)
,m_ignore_response_code(FALSE)
,m_requestActive(FALSE)
,m_multipart_callback(NULL)
{
}

OpRequestImpl::~OpRequestImpl()
{
	m_loader.UnsetURL();
	if (m_multipart_callback)
		m_multipart_callback->SetRequestDeleted();

	if (m_requestActive)
		DeactivateRequest();
	Out();
}

OP_STATUS OpRequestImpl::SetHTTPMethod(HTTP_Request_Method method)
{
	if (method != HTTP_Get && method != HTTP_Head)
	{
		m_url.SetAttribute(URL::KUnique, (UINT32) TRUE);
	}

	return m_url.SetAttribute(URL::KHTTP_Method, (int) method);
}

OP_STATUS OpRequestImpl::AddHTTPHeader(const char *name, const char *value)
{
	URL_Custom_Header header_item;
	RETURN_IF_ERROR(header_item.name.Set(name));
	RETURN_IF_ERROR(header_item.value.Set(value));
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KAddHTTPHeader, &header_item));
	return OpStatus::OK;
}

OP_STATUS OpRequestImpl::GenerateResponse(OpGeneratedResponse *&response)
{
	if (m_responses.Empty())
	{
		response = OP_NEW(OpGeneratedResponseImpl,(m_op_url));

		OP_STATUS status;
		if (!response)
			status = OpStatus::ERR_NO_MEMORY;
		else
			status = response->CreateResource(this);

		if (OpStatus::IsError(status))
		{
			OP_DELETE(response);
			return status;
		}
		response->Into(&m_responses);
	}

	return OpStatus::OK;
}

OP_STATUS OpRequestImpl::GenerateResponse()
{
	if (m_responses.Empty() || m_last_response_was_redirect)
	{
		OpResponseImpl *response = OP_NEW(OpResponseImpl,(m_op_url));

		OP_STATUS status;
		if (!response)
			status = OpStatus::ERR_NO_MEMORY;
		else
			status = response->CreateResource(this);

		if (OpStatus::IsError(status))
		{
			OP_DELETE(response);
			LoadingFailed();
			return status;
		}
		m_last_response_was_redirect = FALSE;
		response->Into(&m_responses);
	}
	return OpStatus::OK;
}


void OpRequestImpl::SendNotification(RequestEvent request_event, int locale_string_id)
{
	if (!GetResponse() && OpStatus::IsError(GenerateResponse()) && request_event != REQUEST_FAILED)
	{
		return;
	}

	switch(request_event)
	{
	case REQUEST_HEADER_LOADED:
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnResponseAvailable(this, GetResponse());
		break;
	case REQUEST_DATA_LOADED:
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnResponseDataLoaded(this, GetResponse());
		break;
	case REQUEST_REDIRECTED:
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnRequestRedirected(this, GetResponse(), m_redirect_from_url, m_url);
		break;
	case REQUEST_FAILED:
		DeactivateRequest();
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnRequestFailed(this, GetResponse(), ConvertUrlStatusToLocaleString(locale_string_id));
		break;
	case REQUEST_FINISHED:
		DeactivateRequest();
		if (!m_ignore_response_code && GetResponse()->GetHTTPResponseCode()>=400)
		{
			for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
				(*it)->OnRequestFailed(this, GetResponse(), ConvertUrlStatusToLocaleString(locale_string_id));
		}
		else
		{
			for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
				(*it)->OnResponseFinished(this, GetResponse());
		}
		break;
	case REQUEST_MULTIPART_BODY_LOADED:

		if (!m_multipart_callback)
			m_multipart_callback = OP_NEW(OpMultipartCallbackImpl,(this));
		m_nextMultipartReceived = TRUE;
		LoadNextMultipart();
		break;
	}
}

void OpRequestImpl::LoadNextMultipart()
{
	if (m_nextMultipartReceived && m_multipart_callback && m_multipart_callback->GetLoadNextPart())
	{
		m_nextMultipartReceived = FALSE;
		m_multipart_callback->PrepareForNextCallback();
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnResponseMultipartBodyLoaded(this, GetResponse(), m_multipart_callback);
	}
}

static const OpMessage op_request_loading_messages[] =
{
	MSG_HEADER_LOADED,
	MSG_URL_DATA_LOADED,
	MSG_MULTIPART_RELOAD,
	MSG_URL_LOADING_FAILED,
	MSG_URL_LOADING_FINISHED,
	MSG_URL_MOVED,
};

void OpRequestImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(m_url.Id() == (URL_ID) par1);

	switch(msg)
	{
	case MSG_HEADER_LOADED:
		if (!m_header_loaded)
		{
			if (OpStatus::IsError(GenerateResponse()))
				break;
			m_header_loaded = TRUE;
			if (GetHTTPMethod() == HTTP_METHOD_HEAD && GetResponse()->GetHTTPResponseCode()<400)
			{
				PostLoadingFinished(par1, par2);
			}
			SendNotification(REQUEST_HEADER_LOADED);
		}
		break;

	case MSG_URL_DATA_LOADED:
		{
			BOOL is_finished = (m_loader->GetAttribute(URL::KLoadStatus) != URL_LOADING);
			if (!m_finished && !is_finished)
				SendNotification(REQUEST_DATA_LOADED);
			else if (is_finished && !m_finished)
			{
				SendNotification(REQUEST_FINISHED);
			}
		}
		break;

	case MSG_MULTIPART_RELOAD:
		OP_ASSERT(!par2);
		if (m_header_loaded)
		{
			m_header_loaded = FALSE;
			BOOL is_finished = (m_loader->GetAttribute(URL::KLoadStatus) != URL_LOADING);
			if (is_finished && !m_finished)
			{
				PostLoadingFinished(par1, par2);
			}
			SendNotification(REQUEST_MULTIPART_BODY_LOADED);
		}
		else
			OP_ASSERT(!"Does this ever happen?");
		break;
	case MSG_URL_LOADING_FAILED:
		SendNotification(REQUEST_FAILED, par2);
		break;
	case MSG_URL_MOVED:
		{
			if (OpStatus::IsError(GenerateResponse())) // Make sure we have a response for the previous url
				break;

			URL redirect_target = m_url.GetAttribute(URL::KMovedToURL);
#if MAX_PLUGIN_CACHE_LEN>0
			if (url.GetAttribute(URL::KUseStreamCache))
				redirect_target.SetAttribute(URL::KUseStreamCache, 1);
#endif
			m_message_handler->RemoveCallBacks(this, m_url.Id());
			m_loader.SetURL(redirect_target);
			m_redirect_from_url = m_url;
			m_url = redirect_target;
			m_op_url = OpURL(redirect_target);
			m_last_response_was_redirect = TRUE;

			OP_STATUS op_err = m_message_handler->SetCallBackList(this, redirect_target.Id(), op_request_loading_messages, ARRAY_SIZE(op_request_loading_messages));
			if (OpStatus::IsError(op_err))
			{
				LoadingFailed();
			}
			else
				SendNotification(REQUEST_REDIRECTED);
		}
		break;
	case MSG_URL_LOADING_FINISHED:
		SendNotification(REQUEST_FINISHED);
		break;
	default:
		OP_ASSERT(!"We have to cover all messages we have configured.");
	}
}

void OpRequestImpl::PostLoadingFinished(MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	g_main_message_handler->PostMessage(MSG_URL_LOADING_FINISHED, par1, par2);
}

void OpRequestImpl::LoadingFailed()
{
	SendNotification(REQUEST_FAILED, ERR_COMM_INTERNAL_ERROR);
}

OP_STATUS OpRequestImpl::SendRequest()
{
	OP_STATUS result = OpStatus::OK;

	if (m_url.IsEmpty() || m_request_sent)
	{
		result = OpStatus::ERR;
	}
	else
	{
		m_request_sent = TRUE;

		if (GetLoadPolicy().GetReloadPolicy() == URL_Resume)
		{
			if (m_url.ResumeLoad(m_message_handler, m_url.GetAttribute(URL::KReferrerURL),m_load_policy.IsInlineElement()) != COMM_LOADING)
				result = OpStatus::ERR_NO_SUCH_RESOURCE;
		}
		else
		{
			if (GetLoadPolicy().GetReloadPolicy() == URL_Reload_Exclusive)
			{
				g_url_api->MakeUnique(m_url);
			}

			if (m_url.LoadDocument(m_message_handler, m_url.GetAttribute(URL::KReferrerURL), m_load_policy) != COMM_LOADING)
			{
				//TODO: Make sure that !m_url.GetAttribute(URL::KStillSameUserAgent) leads to a reload of the resource.
				result = OpStatus::ERR_NO_SUCH_RESOURCE;
			}
		}

		if (OpStatus::IsSuccess(result))
		{
			if (m_url.GetRep()->GetDataStorage())
				m_url.GetRep()->GetDataStorage()->SetOpRequestImpl(this);

			// Set in use after we have started to load this url, to prevent others from reloading while this is loading
			m_loader = m_url;

			// TODO: For future reference, this step is necessary as long as we use the old URL class as back-end.
			// However, it prevents two simultaneous downloads of the same resource (e.g. with different request-headers).
			// In the future, if several OpRequests can be in progress at the same time for the same url, we should
			// do a separate request and not reuse the first OpResponse if the OpRequests are incompatible.
		}

		if (OpStatus::IsSuccess(result) && !m_requestActive)
		{
			m_requestActive = TRUE;
			for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
				IncActiveRequests(*it);
		}
	}

	if (OpStatus::IsSuccess(result))
		result = g_main_message_handler->SetCallBackList(this, m_url.Id(), op_request_loading_messages, ARRAY_SIZE(op_request_loading_messages));

	if (OpStatus::IsError(result))
	{
		LoadingFailed();
	}
	return result;
}

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
OP_STATUS OpRequestImpl::Continue()
{
	return m_url.SetAttribute(URL::KPauseDownload, (UINT32) FALSE);
}
#endif

void OpRequestImpl::AuthenticationRequired(OpAuthenticationCallback* callback)
{
	OP_ASSERT(m_requestActive);
	if (m_requestActive)
	{
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnAuthenticationRequired(this, callback);
	}
}

void OpRequestImpl::AuthenticationCancelled(OpAuthenticationCallback* callback)
{
	OP_ASSERT(m_requestActive);
	if (m_requestActive)
	{
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnAuthenticationCancelled(this, callback);
	}
}

#ifdef _SSL_SUPPORT_
void OpRequestImpl::CertificateBrowsingRequired(OpSSLListener::SSLCertificateContext* context, OpSSLListener::SSLCertificateReason reason, OpSSLListener::SSLCertificateOption options)
{
	OP_ASSERT(m_requestActive);
	if (m_requestActive)
	{
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnCertificateBrowsingRequired(this, context, reason, options);
	}
}

void OpRequestImpl::SecurityPasswordRequired(OpSSLListener::SSLSecurityPasswordCallback* callback)
{
	OP_ASSERT(m_requestActive);
	if (m_requestActive)
	{
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnSecurityPasswordRequired(this, callback);
	}
}
#endif // _SSL_SUPPORT_


#ifdef _ASK_COOKIE
void OpRequestImpl::CookieConfirmationRequired(OpCookieListener::AskCookieContext* cookie_callback)
{
	OP_ASSERT(m_requestActive);
	if (m_requestActive)
	{
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnCookieConfirmationRequired(this, cookie_callback);
	}
}
#endif //_ASK_COOKIE

void OpRequestImpl::RequestDataSent()
{
	OP_ASSERT(m_requestActive);
	if (m_requestActive)
	{
		for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
			(*it)->OnRequestDataSent(this);
	}
}


OP_STATUS OpRequestImpl::AddListener(OpRequestListener *listener)
{
	OP_ASSERT(listener);
	if (!listener)
		return OpStatus::ERR;
	OP_STATUS status = m_listeners.Append(listener);
	if (OpStatus::IsSuccess(status) && m_requestActive)
		IncActiveRequests(listener);
	return status;
}

OP_STATUS OpRequestImpl::RemoveListener(OpRequestListener *listener)
{
	OP_ASSERT(listener);
	if (!listener)
		return OpStatus::ERR;
	bool removed = m_listeners.RemoveItem(listener);
	if (removed && m_requestActive)
		DecActiveRequests(listener);
	return removed ? OpStatus::OK : OpStatus::ERR;
}

void OpRequestImpl::DeactivateRequest()
{
	if (!m_requestActive)
		return;

	if (m_url.GetRep()->GetDataStorage())
		m_url.GetRep()->GetDataStorage()->SetOpRequestImpl(NULL);

	for (OtlList<OpRequestListener*>::Range it(m_listeners.All()); it; ++it)
		DecActiveRequests(*it);

	m_requestActive = FALSE;
	m_finished = TRUE;
	m_message_handler->UnsetCallBacks(this);
	if (m_multipart_callback)
	{
		OP_DELETE(m_multipart_callback);
		m_multipart_callback = NULL;
	}
}

void OpRequestImpl::StopLoading()
{
	if(m_url.IsValid() && m_url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADING)
		m_url.StopLoading(m_message_handler);
}

void OpMultipartCallbackImpl::HandleAsSingleResponse()
{
	if (!RequestWasDeleted())
		for (OtlList<OpRequestListener*>::Range it(m_request->m_listeners.All()); it; ++it)
			(*it)->OnResponseAvailable(m_request, m_request->GetResponse());

	if (!RequestWasDeleted())
		for (OtlList<OpRequestListener*>::Range it(m_request->m_listeners.All()); it; ++it)
			(*it)->OnResponseFinished(m_request, m_request->GetResponse());

	StopLoading();
}

