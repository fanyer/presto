/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WEBSERVERSTARTER_H
#define	WEBSERVERSTARTER_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "../WebServerStateListener.h"
#include "WebServerStateGenerator.h"

#include "modules/util/opstring.h"


/**
 * Implements the actual starting and stopping of the web server.
 *
 * Before the web server can be started, the user must be authenticated at
 * my.opera.com and the device must be registered.  A WebServerStarter is
 * notified about this via OnWebServerSetUpCompleted().
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WebServerStarter
		: public WebServerStateListener,
		  public WebServerStateGenerator
{
public:
	OP_STATUS Init();

	BOOL IsStarted();

	OP_STATUS Stop();

	//
	// WebServerStateListener
	//
	virtual OP_STATUS OnWebServerSetUpCompleted(const OpStringC& shared_secret);
	virtual void OnWebServerError(WebserverStatus status);

private:
	OP_STATUS Start();
};

# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // WEBSERVERSTARTER_H
