/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef COOKIE_WARNING_DIALOG_H
#define COOKIE_WARNING_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

struct cookiedomain_error_st{
	char *url;
	char *cookie;
	BOOL no_ip_address;
	UINT timer;
};


/***********************************************************************************
**
**	CookieWarningDialog
**
***********************************************************************************/

class CookieWarningDialog : public Dialog
{

		cookiedomain_error_st*	m_cookie_error;

	public:
	
								CookieWarningDialog(const char *url, const char *cookie, BOOL no_ip_address);
								~CookieWarningDialog();
		
		Type					GetType()				{return DIALOG_TYPE_COOKIEWARNING;}
		const char*				GetWindowName()			{return "Cookie Warning Dialog";}
		const char*				GetHelpAnchor()			{return "cookies.html";}
		BOOL					GetIsBlocking()			{return FALSE;}
		BOOL					GetDoNotShowAgain()		{return TRUE;}
		BOOL					HideWhenDesktopWindowInActive() {return TRUE;}

		void					OnInit();
		UINT32					OnOk();

};

#endif //COOKIE_WARNING_DIALOG_H
