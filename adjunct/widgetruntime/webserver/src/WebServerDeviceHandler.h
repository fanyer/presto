/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WEBSERVERDEVICEHANDLER_H
#define	WEBSERVERDEVICEHANDLER_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "adjunct/quick/managers/OperaAccountManager.h"

#include "modules/util/opstring.h"

class WebServerSettingsContext;


/**
 * Handles tasks related to the web server device registration.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WebServerDeviceHandler : public OperaAccountManager::OAC_Listener
{
public:
	WebServerDeviceHandler();
	virtual ~WebServerDeviceHandler();

	OP_STATUS Init();

	/**
	 * Requests for web server device registration.
	 *
	 * The user must already be authenticated at my.opera.com and the
	 * preferences must contain the necessary configuration data (device name).
	 *
	 * OperaAccountManager sends out an
	 * OperaAccountManager::OAC_Listener::OnOperaAccountDeviceCreate() message
	 * when the registration succeeds or fails.
	 *
	 * @return status
	 */
	OP_STATUS RegisterDevice();

	/**
	 * Resuests for web server device release, followed by web server device
	 * registration.  Used in the "device take-over" use case.
	 *
	 * @return status
	 * @see ReleaseDevice
	 * @see RegisterDevice
	 */
	OP_STATUS TakeOverDevice();


	//
	// OperaAccountManager::OAC_Listener
	//
	/**
	 * This is when we can register the device if we're doing device take-over.
	 */
	virtual void OnOperaAccountReleaseDevice(OperaAuthError error);


	/**
	 * Fills a WebServerSettingsContext instance with the default web server
	 * device names.
	 *
	 * FIXME: Duplicates parts of WebServerManager::GetWebserverSettings().
	 *
	 * @return status
	 */
	static OP_STATUS AddDefaultDeviceNames(WebServerSettingsContext& settings);

private:
	BOOL m_taking_over_device;

	OP_STATUS ReleaseDevice();
};


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // WEBSERVERDEVICEHANDLER_H
