/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ASK_COOKIE_CONTROLLER_H
#define ASK_COOKIE_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

#include "modules/windowcommander/OpCookieManager.h"


class QuickExpand;

/**
 *
 *	AskCookieController
 *
 *  Class controlls dialog which shows up if the users requested to be informed about incoming cookies.
 *  It displays cookie information, and asks if the cookie should be accepted or 
 *	refused, plus further options.
 *
 */
class AskCookieController : public OkCancelDialogContext
{

public:
	AskCookieController(OpCookieListener::AskCookieContext *context);

	virtual void OnOk();
	virtual void OnCancel();
	
private:
	virtual void InitL();
	OP_STATUS InitControls();

	/**
	*	Checks if the current cookie has an expiration date set or not.
	*/
	bool IsSessionCookie() { return m_cookie_context->GetExpire() == 0;}
	
	const TypedObjectCollection* m_widgets;

	OpCookieListener::AskCookieContext*		m_cookie_context;	//< The cookie context that presents cookie info and handles callbacks.
	OpCookieListener::CookieAction			m_cookie_action;	//< The cookie action that is going to be sent back with the cookie callback.

	static OpProperty<bool>  		s_is_details_shown;		//< Static variable to remember last-time choices (cookie details shown or not).
	static OpProperty<bool>  		s_is_saveall_server;	//< Static variable to remember last-time choices (server-checkbox checked or not).
	static OpProperty<bool>  		s_is_saveonly_session;	//< Static variable to remember last-time choices (session-checkbox checked or not).
		
	QuickExpand*	   m_details_expand;
};

#endif //ASK_COOKIE_CONTROLLER_H
