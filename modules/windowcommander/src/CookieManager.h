/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COOKIE_MANAGER_H
#define COOKIE_MANAGER_H

#include "modules/windowcommander/OpCookieManager.h"

#ifdef _ASK_COOKIE
class NullCookieListener : public OpCookieListener
{
public:
	void OnCookieSecurityWarning(OpWindowCommander* wic, const char *url, const char *cookie, BOOL no_ip_address) {}
	void OnAskCookie(OpWindowCommander* wic, AskCookieContext* callback)
	{
		callback->OnAskCookieCancel();
	}
};
#endif // _ASK_COOKIE

#ifdef WIC_COOKIES_LISTENER
class NullCookieIteratorListener : public OpCookieIteratorListener
{
public:
	void OnSetCookie(const Cookie& cookie) {}
	void OnDeleteCookie(const Cookie& cookie) {}
	void OnIterateCookies(const Cookie& cookie) {}
};
#endif // WIC_COOKIES_LISTENER

#ifdef WIC_COOKIE_MANAGER_LISTENER
class NullCookieManagerListener : public OpCookieManagerListener
{
public:
	void OnCookieFileSaveFinished(BOOL successful, BOOL requested_by_platform) {}
};
#endif

class CookieManager : public OpCookieManager
{
public:
	CookieManager();

#ifdef _ASK_COOKIE
	void SetCookieListener(OpCookieListener* listener);
	OpCookieListener* GetCookieListener();
#endif // _ASK_COOKIE

#ifdef WIC_COOKIES_LISTENER
	void SetCookieIteratorListener(OpCookieIteratorListener* listener);
	OpCookieIteratorListener* GetCookieIteratorListener();
	void IterateAllCookies();
#endif // WIC_COOKIES_LISTENER

#ifdef WIC_COOKIE_MANAGER_LISTENER
	void SetCookieManagerListener(OpCookieManagerListener* listener);
	OpCookieManagerListener* GetCookieManagerListener();
#endif
#ifdef COOKIE_MANUAL_MANAGEMENT
	void RequestSavingCookies();
#endif

private:
#ifdef _ASK_COOKIE
	OpCookieListener* m_cookieListener;
	NullCookieListener m_nullCookieListener;
#endif
#ifdef WIC_COOKIES_LISTENER
	OpCookieIteratorListener* m_cookieIteratorListener;
	NullCookieIteratorListener m_nullCookieIteratorListener;
#endif // WIC_COOKIES_LISTENER
#ifdef WIC_COOKIE_MANAGER_LISTENER
	OpCookieManagerListener* m_cookieManagerListener;
	NullCookieManagerListener m_nullCookieManagerListener;
#endif
};

#endif // COOKIE_MANAGER_H
