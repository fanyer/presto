/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/url/url2.h"
#include "modules/network/op_url.h"
#include "modules/network/op_request.h"
#include "modules/network/src/op_request_impl.h"
#include "modules/network/src/op_batch_request_impl.h"
#include "modules/url/tools/url_util.h"
#include "modules/locale/locale-enum.h"
#include "modules/cache/url_dd.h"

OP_STATUS OpBatchRequestImpl::Make(OpBatchRequest *&req, OpBatchRequestListener *listener)
{
	OP_ASSERT(listener);
	OpBatchRequestImpl *result = OP_NEW(OpBatchRequestImpl,(listener));
	if (result)
	{
		req = (OpBatchRequest *)result;
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

OpBatchRequestImpl::OpBatchRequestImpl(OpBatchRequestListener *listener)
:m_listener(listener),
 m_requests_sent(0),
 m_requests_finished(0)
{
}

OpBatchRequestImpl::~OpBatchRequestImpl()
{
}

OP_STATUS OpBatchRequestImpl::SendRequest(OpRequest *req)
{
	OpRequestImpl *request = static_cast<OpRequestImpl*>(req);
	OP_ASSERT(m_listener);

	request->Into(&m_requests);
	m_requests_sent++;
	return static_cast<OpRequestImpl*>(req)->SendRequest();
}

void OpBatchRequestImpl::ClearRequests()
{
	OpRequestImpl *temp = static_cast<OpRequestImpl*>(GetFirstRequest());
	while (temp)
	{
		OpRequestImpl *next = temp->Suc();
		OP_DELETE(temp);
		temp = next;
	}

}

void OpBatchRequestImpl::OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error)
{
	m_requests_finished++;
	m_listener->OnRequestFailed(req, res, error);
	if (m_requests_finished == m_requests_sent)
		m_listener->OnBatchResponsesFinished();
}

void OpBatchRequestImpl::OnResponseFinished(OpRequest *req, OpResponse *res)
{
	m_requests_finished++;
	m_listener->OnResponseFinished(req, res);
	if (m_requests_finished == m_requests_sent)
		m_listener->OnBatchResponsesFinished();
}
