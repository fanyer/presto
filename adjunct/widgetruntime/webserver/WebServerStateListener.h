/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WEBSERVERSTATELISTENER_H
#define	WEBSERVERSTATELISTENER_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "modules/util/opstring.h"
#include "modules/webserver/webserver-api.h"


/**
 * A listener for web server events.  Compared to WebserverEventListener in
 * modules/webserver/webserver-api.h, the events in this class are a lot more
 * coarse-grained.  There are also some additional events related to the web
 * server start-up.
 *
 * A successful web server start-up involves the following three events, in
 * order:
 *    o  OnLoggedIn
 *    o  OnWebServerSetUpCompleted
 *    o  OnWebServerStarted
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see WebserverEventListener in modules/webserver/webserver-api.h
 */
class WebServerStateListener
{
public:
	virtual ~WebServerStateListener()  {}

	/**
	 * Called upon successful authentication at my.opera.com.
	 *
	 * The listeners are expected to acknowledge the event by returning
	 * OpStatus::OK.  Otherwise, web server start-up will be aborted.
	 *
	 * @return status
	 */
	virtual OP_STATUS OnLoggedIn()
		{ return OpStatus::OK; }

	/**
	 * Called upon successful registration of the web server device.
	 *
	 * The listeners are expected to acknowledge the event by returning
	 * OpStatus::OK.  Otherwise, web server start-up will be aborted.
	 *
	 * @return status
	 */
	virtual OP_STATUS OnWebServerSetUpCompleted(const OpStringC& shared_secret)
		{ return OpStatus::OK; }

	/**
	 * Called when the web server becomes functional.
	 *
	 * The listeners are expected to acknowledge the event by returning
	 * OpStatus::OK.  Otherwise, a web server error will be dispatched.
	 *
	 * @return status
	 */
	virtual OP_STATUS OnWebServerStarted()
		{ return OpStatus::OK; }

	/**
	 * Called when the web server is stopped.
	 */
	virtual void OnWebServerStopped()  {}

	/**
	 * Called when an error prevents the web server from starting.
	 */
	virtual void OnWebServerStartUpError()  {}

	/**
	 * Called when an error prevents the web server from functioning.
	 */
	virtual void OnWebServerError(WebserverStatus status)  {}
};

# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // WEBSERVERSTATELISTENER_H
