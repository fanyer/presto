/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPERA_AUTH_MODULE_H
#define OPERA_AUTH_MODULE_H

#ifdef OPERA_AUTH_SUPPORT
#include "modules/hardcore/opera/module.h"

class MyOperaAuthentication;
class CoreOperaAccountManager;
class OperaOauthManager;

class OperaAuthModule : public OperaModule
{
public:
	OperaAuthModule()	: OperaModule()
		, m_opera_account_manager(NULL)
		, m_opera_oauth_manager(NULL)
		{}

	void InitL(const OperaInitInfo& info);
	void Destroy();

	CoreOperaAccountManager	*m_opera_account_manager;
	OperaOauthManager *m_opera_oauth_manager;
};

#define g_opera_account_manager g_opera->opera_auth_module.m_opera_account_manager
#define g_opera_oauth_manager g_opera->opera_auth_module.m_opera_oauth_manager

#define OPERA_AUTH_MODULE_REQUIRED

#endif // OPERA_AUTH_SUPPORT
#endif // OPERA_AUTH_MODULE_H
