 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  *
  * Manuela Hutter
  */

/** @file DesktopCookieManager.h
  *
  * Contains the declaration of the class Test.
  * Part of the test suite for Opera.
  *
  * @author Manuela Hutter
  */
#ifndef __DESKTOP_COOKIE_MANAGER_H__
#define __DESKTOP_COOKIE_MANAGER_H__

#ifdef _ASK_COOKIE

#include "modules/windowcommander/OpWindowCommander.h"
#include "adjunct/quick/managers/DesktopManager.h"


// Redefine for a more Opera like name
#define g_desktop_cookie_manager (DesktopCookieManager::GetInstance())


/**
  * This class implements the cookie listener interface to handle cookie events coming up from WindowCommander.
  * It shows the according cookie dialogs.
  */
class DesktopCookieManager : public DesktopManager<DesktopCookieManager>, public OpCookieListener
{
public:
	DesktopCookieManager();
	~DesktopCookieManager();

	OP_STATUS Init() { return OpStatus::OK; }

	///--------------------- OPCookieListener ---------------------///
	/**
	 * OpCookieListener implementation. Opens a CookieWarningDialog.
	 *
	 */
	void	OnCookieSecurityWarning(OpWindowCommander* wic, const char* url, const char* cookie, BOOL no_ip_address);
	
	/**
	 * OpCookieListener implementation. Opens an AskCookieDialog.
	 *
	 */
	void	OnAskCookie(OpWindowCommander* wic, AskCookieContext* context);
};

#endif //_ASK_COOKIE
#endif //__COOKIE_MANAGER_H__
