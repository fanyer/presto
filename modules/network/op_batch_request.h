/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OPBATCH_REQUEST_H__
#define __OPBATCH_REQUEST_H__

#include "modules/network/op_request.h"

/** @class OpBatchRequestListener
 *
 *  OpBatchRequestListener is used for observing multiple OpRequests sent through the OpBatchRequest class.
 *  Basically it is the same as a OpRequestListener, but in addition you get a OnBatchResponsesFinished when
 *  all requests have been processed.
 *  Usage:
 *
 *  @code
 *
 *	class Listener : public OpBatchRequestListener
 *	{
 *		virtual void OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error) { ... }
 *		virtual void OnResponseAvailable(OpRequest *req, OpResponse *res) { ... }
 *		virtual void OnResponseDataLoaded(OpRequest *req, OpResponse *res) { ... }
 *		virtual void OnResponseFinished(OpRequest *req, OpResponse *res) { ... }
 *		virtual void OnBatchResponsesFinished() { ... }
 *	};
 *
 *	OpBatchRequest *batch_request;
 *	Listener *listener = OP_NEW(Listener,());
 *	OpBatchRequest::Make(batch_request, listener);
 *	OpURL url1 = OpURL::Make("http://t/core/networking/http/cache/data/blue.jpg");
 *	OpURL url2 = OpURL::Make("http://t/core/networking/http/cache/data/yellow.jpg");
 *	OpURL referrer;
 *	OpRequest *request;
 *	OP_STATUS result = OpRequest::Make(request, NULL, url1, referrer);
 *	batch_request.SendRequest(request)
 *	result = OpRequest::Make(request, NULL, url2, referrer);
 *	batch_request->SendRequest(request)
 *
 *	@endcode
 */
class OpBatchRequestListener:public OpRequestListener
{
public:
	virtual void OnBatchResponsesFinished() = 0;
};

/** @class OpBatchRequest
 *
 *  OpBatchRequest is used for sending multiple OpRequests at once. After all requests
 *  have been processed you will get a OnBatchResponsesFinished callback through an
 *  OpBatchRequestListener. Deleting the batch request deletes (and aborts) all the contained requests.
 */
class OpBatchRequest: public OpRequestListener
{
public:

	virtual ~OpBatchRequest() {};

	static OP_STATUS Make(OpBatchRequest *&req, OpBatchRequestListener *listener);

	/** Add requests to the batch and send. If you have delays between sending each request it is possible to receive
	 *  the OnBatchResponsesFinished() callback in between sending the requests.
	 */
	virtual OP_STATUS SendRequest(OpRequest *req) = 0;

	/** Delete all request objects after a batch has completed. */
	virtual void ClearRequests() = 0;

	/** Retrieve the first request, and use Next() to traverse all requests in the batch. */
	virtual OpRequest *GetFirstRequest() = 0;
};

#endif
