/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SECURITY_PASSWORD_DIALOG_H
# define SECURITY_PASSWORD_DIALOG_H

# include "adjunct/quick_toolkit/widgets/Dialog.h"
# include "modules/libssl/ssl_api.h"
# include "modules/windowcommander/OpWindowCommander.h"

/**
 * Dialog class to get the master password.
 */
class SecurityPasswordDialog : public Dialog
{
private:
	OpString									m_title;
	OpString									m_txt;
	BOOL*										m_retval;
	BOOL										m_creating_new_password;
	BOOL										m_ok_enabled;
	OpSSLListener::SSLSecurityPasswordCallback*	m_callback;
public:
		/**
		 * Constructor to use with the non blocking ssl ui interface in 
		 * windowcommander. When initalized in this context, the dialog 
		 * will be non-blocking, unless explicitly set to be blocking, 
		 * which should only be done if there is no desktop window to 
		 * display on top of.
		 *
		 * @param callback The object holding the callback to use to signal 
		 * that the user finished.
		 * @param block If true, the dialog needs to block. Set this to true 
		 * if and only if there is no desktop window to 
		 */
								SecurityPasswordDialog(OpSSLListener::SSLSecurityPasswordCallback* callback);

		Type					GetType()				{return DIALOG_TYPE_SECURITY_PASSWORD;}
		const char*				GetWindowName()			{return "Security Password Dialog";}
		const char*				GetHelpAnchor()			{return "certificates.html#master-pwd";}
		BOOL					GetModality()			{return TRUE;}
		void					OnChange(OpWidget *widget, BOOL changed_by_mouse);
		void					OnInit();
		UINT32					OnOk();
		void					OnCancel();
		BOOL					OnInputAction(OpInputAction* action);
};

#endif //SECURITY_PASSWORD_DIALOG_H
