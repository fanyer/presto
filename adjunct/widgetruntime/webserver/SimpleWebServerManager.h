/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SIMPLEWEBSERVERMANAGER_H
#define	SIMPLEWEBSERVERMANAGER_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "adjunct/quick/managers/DesktopManager.h"

class SimpleWebServerManagerImpl;
class WebServerStateListener;


/**
 * A DesktopManager responsible for managing the Opera's built-in web server.
 *
 * Contrary to WebServerManager, SimpleWebServerManager's only dependency is
 * OperaAccountManager.
 *
 * This class is a facade exposing the simplest interface possible, and
 * shielding the clients from implementation details.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see WebServerManager
 * @see SimpleWebServerManagerImpl
 */
class SimpleWebServerManager : public DesktopManager<SimpleWebServerManager>
{
public:
	SimpleWebServerManager();
	virtual ~SimpleWebServerManager();

	virtual OP_STATUS Init();

	/**
	 * Registers a web server state listener that will be notified about events
	 * related to the web server start-up and status.
	 *
	 * @param listener the listener
	 * @return status
	 */
	OP_STATUS AddWebServerStateListener(WebServerStateListener& listener);

	/**
	 * Starts the web server.
	 *
	 * More precisely, starts the web server start-up procedure and returns.
	 * The actual startup involves network communication, and its progress and
	 * status can be monitored by registering as a WebServerStateListener.
	 *
	 * @return OpStatus::OK iff the start-up procedure has been initiated
	 *		successfully
	 * @see AddWebServerStateListener
	 */
	OP_STATUS StartWebServer();

	/**
	 * Displays the web server status to the user.
	 *
	 * @return status
	 */
	OP_STATUS ShowWebServerStatus();

private:
	SimpleWebServerManagerImpl* m_impl;

	// FIXME: DesktopManager should declare these as private.
	SimpleWebServerManager(const SimpleWebServerManager&);
	SimpleWebServerManager& operator=(const SimpleWebServerManager&);
};

# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // SIMPLEWEBSERVERMANAGER_H
