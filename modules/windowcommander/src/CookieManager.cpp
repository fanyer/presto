/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/windowcommander/src/CookieManager.h"
#include "modules/cookies/cookie_man.h"
#include "modules/url/url_module.h"
#ifdef COOKIE_MANUAL_MANAGEMENT
#include "modules/url/url_man.h"
#endif

/*static*/
OpCookieManager* OpCookieManager::CreateL()
{
	return OP_NEW_L(CookieManager, ());
}

CookieManager::CookieManager()
{
#ifdef _ASK_COOKIE
	m_cookieListener = &m_nullCookieListener;
#endif // _ASK_COOKIE
#ifdef WIC_COOKIES_LISTENER
	m_cookieIteratorListener = &m_nullCookieIteratorListener;
#endif // WIC_COOKIES_LISTENER
#ifdef WIC_COOKIE_MANAGER_LISTENER
	m_cookieManagerListener = &m_nullCookieManagerListener;
#endif
}

#ifdef _ASK_COOKIE
void CookieManager::SetCookieListener(OpCookieListener* listener)
{
	m_cookieListener = listener ? listener : &m_nullCookieListener;
}

OpCookieListener* CookieManager::GetCookieListener()
{
	return m_cookieListener;
}
#endif // _ASK_COOKIE

#ifdef WIC_COOKIES_LISTENER
void CookieManager::SetCookieIteratorListener(OpCookieIteratorListener* listener)
{
	m_cookieIteratorListener = listener ? listener : &m_nullCookieIteratorListener;
}

OpCookieIteratorListener* CookieManager::GetCookieIteratorListener()
{
	return m_cookieIteratorListener;
}

void CookieManager::IterateAllCookies()
{
	g_cookieManager->IterateAllCookies();
}
#endif // WIC_COOKIES_LISTENER

#ifdef WIC_COOKIE_MANAGER_LISTENER
void CookieManager::SetCookieManagerListener(OpCookieManagerListener* listener)
{
	m_cookieManagerListener = listener ? listener : &m_nullCookieManagerListener;
}

OpCookieManagerListener* CookieManager::GetCookieManagerListener()
{
	return m_cookieManagerListener;
}
#endif

#ifdef COOKIE_MANUAL_MANAGEMENT
void CookieManager::RequestSavingCookies()
{
	g_cookieManager->RequestSavingCookies();
}
#endif
