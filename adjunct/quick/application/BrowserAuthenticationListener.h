/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef BROWSER_AUTHENTICATION_LISTENER
#define BROWSER_AUTHENTICATION_LISTENER

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/util/adt/opvector.h"
#include "adjunct/quick/application/DesktopOpAuthenticationListener.h"

class PasswordDialog;

class BrowserAuthenticationListener
	: public DesktopOpAuthenticationListener
	, public DialogListener
{
public:
	virtual ~BrowserAuthenticationListener();

	// From DesktopOpAuthenticationListener
	virtual void OnAuthenticationRequired(OpAuthenticationCallback* callback);
	virtual void OnAuthenticationCancelled(const OpAuthenticationCallback* callback) { ClosePasswordDialogs(callback->UrlId()); }
	virtual void OnAuthenticationRequired(OpAuthenticationCallback* callback, OpWindowCommander *commander, DesktopWindow* parent_window);

	// From DialogListener
	virtual void OnOk(Dialog* dialog, UINT32 result);
	virtual void OnClose(Dialog* dialog);

private:
	void ClosePasswordDialogs(URL_ID authid, PasswordDialog* request_from = 0);
	
	OpVector<PasswordDialog> m_open_dialogs;
};

#endif
