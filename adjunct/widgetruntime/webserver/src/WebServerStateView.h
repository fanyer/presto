/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WEBSERVERSTATEVIEW_H
#define	WEBSERVERSTATEVIEW_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "../WebServerStateListener.h"

#include "adjunct/quick/dialogs/Dialog.h"
#include "adjunct/quick/managers/OperaAccountManager.h"

class WebServerController;


/**
 * Responsible for visual presentation of the web server state.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WebServerStateView
		: public WebServerStateListener,
		  public DialogListener
{
public:
	WebServerStateView();

	OP_STATUS Init(WebServerController& web_server_controller);

	/**
	 * Displays a dialog showing the current web server state.
	 *
	 * @see WebServerStatusDialog
	 */
	OP_STATUS ShowStatusDialog();

	//
	// WebServerStateListener
	//
	virtual OP_STATUS OnLoggedIn();
	virtual OP_STATUS OnWebServerSetUpCompleted(const OpStringC& shared_secret);
	virtual OP_STATUS OnWebServerStarted();
	virtual void OnWebServerStopped();
	virtual void OnWebServerError(WebserverStatus web_server_status);

	//
	// DialogListener
	//
	virtual void OnClose(Dialog* dialog);

private:
	Dialog* m_status_dialog;

	WebServerController* m_web_server_controller;

	OperaAccountManager::OAM_Status m_oam_status;
	WebserverStatus m_web_server_status;
};


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // WEBSERVERSTATEVIEW_H
