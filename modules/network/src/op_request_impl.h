/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OPREQUEST_IMPL_H__
#define __OPREQUEST_IMPL_H__

#include "modules/network/op_request.h"
#include "modules/network/src/op_response_impl.h"
#include "modules/url/url_loading.h"
#include "modules/cache/url_dd.h"
#include "modules/url/tools/url_util.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/otl/list.h"
#include "modules/util/simset.h"

/** @class OpRequestImpl
 *
 */

class OpRequestImpl: public OpRequest, public MessageObject, public ListElement<OpRequestImpl>
{
public:
	~OpRequestImpl();

	static OP_STATUS Make(OpRequest *&request, OpRequestListener *listener, OpURL url, URL_CONTEXT_ID context_id, OpURL referrer = OpURL());

	virtual URL_LoadPolicy &GetLoadPolicy() { return m_load_policy; }

	virtual OP_STATUS SendRequest();

	virtual OpResponse *GetResponse() { return m_responses.Last(); }

	virtual OpResponse *GetFirstResponse() { return m_responses.First(); }

	virtual OP_STATUS GenerateResponse(class OpGeneratedResponse *&response);

	virtual OP_STATUS SetHTTPMethod(HTTP_Request_Method method);

	virtual HTTP_Request_Method GetHTTPMethod() const { return (HTTP_Request_Method) m_url.GetAttribute(URL::KHTTP_Method); }

	virtual OP_STATUS SetCustomHTTPMethod(const char *special_method);

	virtual const char *GetCustomHTTPMethod() const { return m_url.GetAttribute(URL::KHTTPSpecialMethodStr).CStr(); }

	virtual URL_CONTEXT_ID GetContextId() const { return m_url.GetContextId(); }

	virtual void SetAccessed(BOOL visited) { m_url.Access(visited); }

	virtual OP_STATUS AddHTTPHeader(const char *name, const char *value);

	virtual OP_STATUS SetCookiesProcessingDisabled(BOOL value) { return m_url.SetAttribute(URL::KDisableProcessCookies, (UINT32) value);}

	virtual OP_STATUS SetNetTypeLimit(OpSocketAddressNetType value) { return m_url.SetAttribute(URL::KLimitNetType, (UINT32) value);}

	virtual OP_STATUS SetMaxRequestTime(UINT32 value) { return m_url.SetAttribute(URL::KTimeoutMaximum, value); }

	virtual OP_STATUS SetMaxResponseIdleTime(UINT32 value) { return m_url.SetAttribute(URL::KTimeoutPollIdle, value); }

	virtual OP_STATUS SetHTTPPriority(UINT32 value) { return m_url.SetAttribute(URL::KHTTP_Priority, value);}

	virtual OP_STATUS SetExternallyManagedConnection(BOOL value) { return m_url.SetAttribute(URL::KHTTP_Managed_Connection, (UINT32) value);}

	virtual OP_STATUS SetHTTPDataContentType(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTP_ContentType, value); }

	virtual const char *GetHTTPDataContentType() const { return m_url.GetAttribute(URL::KHTTP_ContentType).CStr(); }

	virtual OP_STATUS SetHTTPUsername(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTPUsername, value); }

	virtual OP_STATUS SetHTTPPassword(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTPPassword, value); }

	virtual OP_STATUS UseGenericAuthenticationHandling() { return m_url.SetAttribute(URL::KUseGenericAuthenticationHandling, TRUE); }

	virtual OP_STATUS SetOverrideRedirectDisabled(BOOL value) { return m_url.SetAttribute(URL::KOverrideRedirectDisabled, (UINT32) value);}

	virtual OP_STATUS SetLimitedRequestProcessing(BOOL value) { return m_url.SetAttribute(URL::KSpecialRedirectRestriction, (UINT32) value);}

	virtual void SetParallelConnectionsDisabled() { if (m_url.GetAttribute(URL::KServerName, NULL)) ((ServerName*)(m_url.GetAttribute(URL::KServerName, NULL)))->SetConnectionNumRestricted(TRUE); }

	virtual OpURL GetURL() { return m_original_op_url; }

	virtual OpURL GetReferrerURL() { return OpURL(m_url.GetAttribute(URL::KReferrerURL)); }

	virtual void SetIgnoreResponseCode(BOOL value) { m_ignore_response_code = value; }

	virtual OP_STATUS SetRangeStart(OpFileLength position) { return m_url.SetAttribute(URL::KHTTPRangeStart, (const void *)&position); }

	virtual OP_STATUS SetRangeEnd(OpFileLength position) { return m_url.SetAttribute(URL::KHTTPRangeEnd, (const void *)&position); }

	virtual OpRequest *Next() { return Suc(); }

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	virtual OP_STATUS Pause() { return m_url.SetAttribute(URL::KPauseDownload, (UINT32) TRUE);}

	virtual OP_STATUS Continue();
#endif

	virtual OP_STATUS SetHTTPData(const char* data, BOOL includes_headers = FALSE) { return m_url.SetHTTP_Data(data, TRUE, includes_headers); }

	virtual void SetHTTPData(Upload_Base* data, BOOL new_data) { m_url.SetHTTP_Data(data, new_data); }

#ifdef HTTP_CONTENT_USAGE_INDICATION
	virtual OP_STATUS SetHTTPContentUsageIndication(HTTP_ContentUsageIndication value) { return m_url.SetAttribute(URL::KHTTP_ContentUsageIndication, (UINT32) value); }

	virtual HTTP_ContentUsageIndication GetHTTPContentUsageIndication() const { return (HTTP_ContentUsageIndication) m_url.GetAttribute(URL::KHTTP_ContentUsageIndication); }
#endif // HTTP_CONTENT_USAGE_INDICATION

#ifdef CORS_SUPPORT
	virtual OP_STATUS SetFollowCORSRedirectRules(BOOL value) { return m_url.SetAttribute(URL::KFollowCORSRedirectRules, (UINT32) value); }
#endif // CORS_SUPPORT

#ifdef ABSTRACT_CERTIFICATES
	virtual OP_STATUS SetCertificateRequested(BOOL value) { return m_url.SetAttribute(URL::KCertificateRequested, (UINT32) value);}
#endif

#ifdef URL_ALLOW_DISABLE_COMPRESS
	virtual OP_STATUS SetDisableCompress(BOOL value) { return m_url.SetAttribute(URL::KDisableCompress, (UINT32) value);}
#endif

	virtual void AuthenticationRequired(OpAuthenticationCallback* callback);
	virtual void AuthenticationCancelled(OpAuthenticationCallback* callback);

#ifdef _SSL_SUPPORT_
	virtual void CertificateBrowsingRequired(OpSSLListener::SSLCertificateContext* context, OpSSLListener::SSLCertificateReason reason, OpSSLListener::SSLCertificateOption options);

	virtual void SecurityPasswordRequired(OpSSLListener::SSLSecurityPasswordCallback* callback);
#endif // _SSL_SUPPORT_

#ifdef _ASK_COOKIE
	virtual void CookieConfirmationRequired(OpCookieListener::AskCookieContext* cookie_callback);
#endif //_ASK_COOKIE

	virtual void RequestDataSent();

	virtual OP_STATUS AddListener(OpRequestListener *listener);

	virtual OP_STATUS RemoveListener(OpRequestListener *listener);

private:
	enum RequestEvent {
		REQUEST_STARTED,
		REQUEST_HEADER_LOADED,
		REQUEST_DATA_LOADED,
		REQUEST_REDIRECTED,
		REQUEST_FINISHED,
		REQUEST_FAILED,
		REQUEST_MULTIPART_BODY_LOADED
	};

	URL_LoadPolicy m_load_policy;
	URL m_url;
	OpURL m_op_url;
	OpURL m_original_op_url;
	OtlList<OpRequestListener*> m_listeners;
	URL m_redirect_from_url;
	AutoDeleteList<OpResponseImpl> m_responses;
	BOOL m_last_response_was_redirect;
	MessageHandler *m_message_handler;
	IAmLoadingThisURL m_loader;
	BOOL m_request_sent;
	BOOL m_header_loaded;
	BOOL m_finished;
	BOOL m_ignore_response_code;
	BOOL m_requestActive;
	BOOL m_nextMultipartReceived;
	class OpMultipartCallbackImpl *m_multipart_callback;

	OpRequestImpl(OpURL url);
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void SendNotification(RequestEvent request_event, int locale_string_id = 0 );
	void LoadingFailed();
	void PostLoadingFinished(MH_PARAM_1 par1, MH_PARAM_2 par2);
	OP_STATUS GenerateResponse();
	void DeactivateRequest();
	void StopLoading();
	void LoadNextMultipart();
	void SetOpMultipartCallbackImpl(class OpMultipartCallbackImpl *callback) { m_multipart_callback = callback; }
	friend class OpBatchRequest;
	friend class OpMultipartCallbackImpl;
};

class OpMultipartCallbackImpl: public OpMultipartCallback
{
public:
	virtual void LoadNextMultipart() { m_load_next_part = TRUE; if (m_request) m_request->LoadNextMultipart(); }
	virtual void StopLoading() { m_load_next_part = FALSE; if (m_request) { m_request->StopLoading(); m_request->SetOpMultipartCallbackImpl(NULL); } OP_DELETE(this); }
	BOOL GetLoadNextPart() { return m_load_next_part; }
	void PrepareForNextCallback() { m_load_next_part = FALSE; }
	BOOL RequestWasDeleted() { return m_request == NULL; }
	void SetRequestDeleted() { m_request = NULL; }
	virtual void HandleAsSingleResponse();
	OpMultipartCallbackImpl(OpRequestImpl *request):m_request(request), m_load_next_part(TRUE) { OP_ASSERT(request); request->SetOpMultipartCallbackImpl(this); }
	~OpMultipartCallbackImpl() { if (m_request) m_request->SetOpMultipartCallbackImpl(NULL); }

private:
	OpRequestImpl *m_request;
	BOOL m_load_next_part;
};

#endif
