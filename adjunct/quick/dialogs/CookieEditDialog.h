/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** 
*/

#ifndef __COOKIE_EDIT_DIALOG_H__
#define __COOKIE_EDIT_DIALOG_H__

#include "ServerManagerDialog.h"
#include <time.h>
class Cookie;
class OpEdit;

class CookieEditDialog : public Dialog, public ServerManagerListener
{
public:
	CookieEditDialog(const uni_char* server_name, Cookie* cookie, BOOL readonly = FALSE);
	~CookieEditDialog();

	virtual void				OnInit();
	BOOL						OnInputAction(OpInputAction* action);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_PLUGIN_PATH;}
	virtual const char*			GetWindowName()			{return "Cookie Edit Dialog";}

	INT32						GetButtonCount() { return 2; };


	void 						SetListener( CookieEditorListener* listener) { m_listener = listener; }
	void 						OnCookieRemoved(Cookie* cookie);

	void 						SetTime( OpEdit* edit, time_t t, BOOL date );
	BOOL 						GetTime( OpEdit* date_edit, OpEdit* time_edit, time_t &t );

private:
	OpString m_server_name;
	Cookie*  m_cookie;
	BOOL	 m_readonly;	// read only when in privacy mode
	CookieEditorListener*		m_listener;
};



#endif
