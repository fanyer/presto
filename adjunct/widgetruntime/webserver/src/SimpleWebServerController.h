/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SIMPLEWEBSERVERCONTROLLER_H
#define	SIMPLEWEBSERVERCONTROLLER_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "WebServerStateGenerator.h"

#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"

#include "modules/util/opstring.h"

#include "modules/webserver/webserver-api.h"


class FeatureSettingsContext;
class WebServerDeviceHandler;


/**
 * A no-op implementation of the WebserverEventListener interface.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class DefaultWebServerEventListener : public WebserverEventListener
{
public:
	virtual void OnWebserverStopped(WebserverStatus status)  {}
	virtual void OnWebserverUPnPPortsClosed(UINT16 port)  {}
	virtual void OnWebserverServiceStarted(const uni_char* service_name,
			const uni_char* service_path, BOOL is_root_service = FALSE)  {}
	virtual void OnWebserverServiceStopped(const uni_char* service_name,
			const uni_char* service_path, BOOL is_root_service = FALSE)  {}
	virtual void OnWebserverListenLocalStarted(unsigned int port)  {}
	virtual void OnWebserverListenLocalStopped()  {}
	virtual void OnWebserverListenLocalFailed(WebserverStatus status)  {}
#ifdef WEB_UPLOAD_SERVICE_LIST
	virtual void OnWebserverUploadServiceStatus(UploadServiceStatus status)  {}
#endif // WEB_UPLOAD_SERVICE_LIST
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual void OnProxyConnected()  {}
	virtual void OnProxyConnectionFailed(WebserverStatus status, BOOL retry)  {}
	virtual void OnProxyConnectionProblem(WebserverStatus status,  BOOL retry)
		{}
	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry)  {}
#endif
	virtual void OnDirectAccessStateChanged(BOOL direct_access,
			const char* direct_access_address)  {}
	virtual void OnNewDOMEventListener(const uni_char* service_name,
			const uni_char* evt, const uni_char* virtual_path = NULL)  {}
};



/**
 * A simplified WebServerController.
 * 
 * It is a kind of mediator between
 * <ul>
 *     <li>the user, the web server proper, and OperaAccountManager on the one
 *     side</li>,
 *     <li>and the other web server related classes within widget runtime on the
 *     other side.</li>
 * </ul>
 * It listens to OperaAccountManager::OAC_Listener and WebserverEventListener
 * events, and reacts accordingly by generating WebServerStateListener events.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class SimpleWebServerController
		: public WebServerController,
 		  public OperaAccountManager::OAC_Listener,
		  public DefaultWebServerEventListener,
		  public WebServerStateGenerator
{
public:
	SimpleWebServerController();

	OP_STATUS Init(WebServerDeviceHandler& device_handler);

	/**
	 * Execute any pending operations.
	 */
	OP_STATUS Flush();


	//
	// WebServerController
	//

	/**
	 * Triggers web server device registration.  Handles the case when the web
	 * server is being started as a result of configuration performed by the
	 * user in a dialog, with device take-over.
	 */
	virtual void EnableFeature(const FeatureSettingsContext* settings,
			BOOL force_device_release = TRUE);

	virtual OP_STATUS Restart();

	virtual OP_STATUS GetURLForComputerName(OpString& url,
			const OpStringC& computername)
		{ return OpStatus::OK; }

	virtual void SetConfiguring(BOOL configuring)
		{ m_configuring = configuring; }

	virtual BOOL IsConfiguring()
		{ return m_configuring; }

	virtual BOOL IsUserRequestingDeviceRelease(DesktopWindow* parent_window);


	//
	// FeatureController
	//

	/**
	 * In our case, this seems to only be used by the web server status dialog
	 * to enable/disable the Restart Unite button.  The dialog allows to restart
	 * if (IsFeatureEnabled()).
	 */
	virtual BOOL IsFeatureEnabled() const
		{ return !m_web_server_running; }

	/**
	 * Triggers web server device registration.  Handles the case when the web
	 * server is being started as a result of configuration performed by the
	 * user in a dialog, without the need for device take-over.
	 */
	virtual void EnableFeature(const FeatureSettingsContext* settings);

	virtual void DisableFeature()
		{}

	virtual void SetFeatureSettings(const FeatureSettingsContext* settings)
		{}

	virtual void InvokeMessage(OpMessage msg, MH_PARAM_1 param_1,
			MH_PARAM_2 param_2)
		{}

	
	//
	// OperaAccountManager::OAC_Listener
	//

	/**
	 * Triggers web server device registration.  Handles the case when the web
	 * server is being started with configuration previously stored in the
	 * prefs.
	 */
	virtual void OnOperaAccountAuth(OperaAuthError error,
			const OpStringC& shared_secret);

	virtual void OnOperaAccountDeviceCreate(OperaAuthError error,
			const OpStringC& shared_secret, const OpStringC& server_message);


	//
	// DefaultWebServerEventListener
	//
	virtual void OnWebserverServiceStarted(const uni_char* service_name,
			const uni_char* service_path, BOOL is_root_service = FALSE);
	virtual void OnWebserverStopped(WebserverStatus status);
	virtual void OnWebserverListenLocalFailed(WebserverStatus status);
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual void OnProxyConnectionFailed(WebserverStatus status, BOOL retry);
	virtual void OnProxyConnectionProblem(WebserverStatus status,  BOOL retry);
	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry);
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

	
private:
	void HandleWebServerError(WebserverStatus status);

	WebServerDeviceHandler* m_device_handler;
	BOOL m_web_server_running;
	BOOL m_configuring;
	BOOL m_device_registration_pending;
};


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // SIMPLEWEBSERVERCONTROLLER_H
