/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OPBATCH_REQUEST_IMPL_H__
#define __OPBATCH_REQUEST_IMPL_H__

#include "modules/network/op_batch_request.h"
#include "modules/network/src/op_request_impl.h"

class OpBatchRequestImpl: public OpBatchRequest
{
public:
	static OP_STATUS Make(OpBatchRequest *&req, OpBatchRequestListener *listener);

	OpBatchRequestImpl(OpBatchRequestListener *listener);

	virtual ~OpBatchRequestImpl();

	/** Listen for notifications. */
	virtual void SetListener(OpBatchRequestListener *listener) { OP_ASSERT(m_listener == 0 || m_listener == listener); m_listener = listener; };

	/** Add requests to the batch and send. */
	virtual OP_STATUS SendRequest(OpRequest *req);

	/** Delete all request objects after a batch has completed. */
	virtual void ClearRequests();

	/** Retrieve the first request, and use Next() to traverse all requests in the batch. */
	virtual OpRequest *GetFirstRequest() { return (OpRequest*)m_requests.First(); }
private:
	virtual void OnRequestRedirected(OpRequest *req, OpResponse *res, OpURL from, OpURL to) { m_listener->OnRequestRedirected(req, res, from, to); }
	virtual void OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error);
	virtual void OnRequestDataSent(OpRequest *req) { m_listener->OnRequestDataSent(req); }
	virtual void OnResponseAvailable(OpRequest *req, OpResponse *res){ m_listener->OnResponseAvailable(req, res); }
	virtual void OnAuthenticationRequired(OpRequest *req, OpAuthenticationCallback *callback) { m_listener->OnAuthenticationRequired(req, callback); }
#ifdef _SSL_SUPPORT_
	virtual void OnCertificateBrowsingRequired(OpRequest *req,  OpSSLListener::SSLCertificateContext *context, OpSSLListener::SSLCertificateReason reason, OpSSLListener::SSLCertificateOption options) { m_listener->OnCertificateBrowsingRequired(req, context, reason, options); }
	virtual void OnSecurityPasswordRequired(OpRequest *req, OpSSLListener::SSLSecurityPasswordCallback *callback) { m_listener->OnSecurityPasswordRequired(req, callback); }
#endif // _SSL_SUPPORT_
#ifdef _ASK_COOKIE
	void OnCookieConfirmationRequired(OpRequest *req, OpCookieListener::AskCookieContext *callback) { m_listener->OnCookieConfirmationRequired(req, callback); }
#endif //_ASK_COOKIE
	virtual void OnResponseDataLoaded(OpRequest *req, OpResponse *res){ m_listener->OnResponseDataLoaded(req, res); }
	virtual void OnResponseMultipartBodyLoaded(OpRequest *req, OpResponse *res, OpMultipartCallback *callback){ m_listener->OnResponseMultipartBodyLoaded(req, res, callback); }
	virtual void OnResponseFinished(OpRequest *req, OpResponse *res);
	List<OpRequestImpl> m_requests;
	OpBatchRequestListener *m_listener;
	int m_requests_sent;
	int m_requests_finished;
};

#endif
