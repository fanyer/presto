/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SIMPLEWEBSERVERMANAGERIMPL_H
#define	SIMPLEWEBSERVERMANAGERIMPL_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "../WebServerStateListener.h"
#include "SimpleWebServerController.h"
#include "WebServerDeviceHandler.h"
#include "WebServerStarter.h"
#include "WebServerStateView.h"

#include "adjunct/quick/webserver/controller/WebServerSettingsContext.h"


/**
 * The entry point when it comes to SimpleWebServerManager internal
 * implementation.
 *
 * Initializes and manages the lifetimes of the different classes realizing the
 * web server management functions.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see SimpleWebServerManager
 */
class SimpleWebServerManagerImpl
		: public WebServerStateListener
// TODO:		  public OpPrefsListener,
// TODO:		  public MessageObject,
{
public:
	SimpleWebServerManagerImpl();
	virtual ~SimpleWebServerManagerImpl();

	OP_STATUS Init();
	OP_STATUS AddWebServerStateListener(WebServerStateListener& listener);

	OP_STATUS StartWebServer();

	OP_STATUS ShowWebServerStatus();

	//
	// WebServerStateListener
	//
	virtual OP_STATUS OnLoggedIn();

private:
	BOOL ConfigureWithDialog();
	BOOL LogInWithDialog();

	const WebServerSettingsContext& GetWebServerSettings();

	static WebServerSettingsContext s_web_server_settings;

	WebServerStarter m_web_server_starter;
	WebServerDeviceHandler m_device_handler;
	SimpleWebServerController m_controller;
	WebServerStateView m_web_server_state_view;

	BOOL m_configuration_pending;
};


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // SIMPLEWEBSERVERMANAGERIMPL_H
