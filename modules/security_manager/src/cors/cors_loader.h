/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef CORS_LOADER_H
#define CORS_LOADER_H

#include "modules/security_manager/include/security_manager.h"

/** Handle the sending of a preflight OPTIONS request. */
class OpCrossOrigin_Loader
	: public MessageObject
	, public OpSecurityCheckCancel
{
public:
	static OP_STATUS Make(OpCrossOrigin_Loader *&loader, OpCrossOrigin_Manager *manager, OpCrossOrigin_Request *request, OpSecurityCheckCallback *security_callback);
	/**< Create a loader for a preflight request.

	     The resource's URL is used when constructing the preflight
	     request, but none of the headers associated with the origining
	     URL are supplied in the preflight request.

	     Upon completion of the preflight step, the loader will advance
	     the 'request' state to either PreflightComplete or error (network.)

	     @param [out] loader The resulting loader object.
	     @param manager The owning manager object.
	     @param request The cross-origin resource.
	     @param security_callback Upon completion (success/failure), the
	            callback to inform of the outcome of the preflight step.
	     @result OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual ~OpCrossOrigin_Loader();

	OP_STATUS Start(const URL &ref_url);
	/**< Initiate preflight loading, using the given origin URL as
	     referer for non-anonymous requests.

	     @return OpStatus::OK if loading has commenced, OpStatus::ERR
	             if it failed to initiate, OpStatus::ERR_NO_MEMORY on OOM */

	void DetachRequest();
	/**< Sever the connection to the request; performed if the request is
	     aborted before completion of the load operation. */

	// From OpSecurityCheckCancel
	void CancelSecurityCheck();
	/**< Abort or cancel preflight loading, stopping the loading
	     of the external resource. Aborting an already completed
	     request has no effect. */

private:
	OpCrossOrigin_Loader()
		: mh(NULL)
		, stopped(FALSE)
		, result(OpStatus::ERR)
		, request(NULL)
		, manager(NULL)
		, security_callback(NULL)
	{
	}

	MessageHandler *mh;

	URL url;

	BOOL stopped;
	OP_BOOLEAN result;

	OpCrossOrigin_Request *request;
	OpCrossOrigin_Manager *manager;
	OpSecurityCheckCallback *security_callback;

	// From MessageObject:
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_STATUS SetCallbacks();

	void SignalListener();
	/**< The load has completed; signal the context that called Start(). */

	static OP_STATUS MakeRequestURL(URL &preflight_url, const OpCrossOrigin_Request &request);
	/**< Construct the URL for the preflight request corresponding to a CORS request. */
};

#endif // CORS_LOADER_H
