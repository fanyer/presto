/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef OPERA_AUTH_SUPPORT

#include "modules/opera_auth/opera_auth_module.h"
#include "modules/opera_auth/opera_auth_oauth.h"
#include "modules/opera_auth/opera_auth_myopera.h"
#include "modules/opera_auth/opera_account_manager.h"

void OperaAuthModule::InitL(const OperaInitInfo& info)
{
	CoreOperaAccountManager::CreateL(m_opera_account_manager);
	OperaOauthManager::CreateL(m_opera_oauth_manager);
}

void OperaAuthModule::Destroy()
{
	OP_DELETE(m_opera_oauth_manager);
	m_opera_oauth_manager = NULL;

	OP_DELETE(m_opera_account_manager);
	m_opera_account_manager = NULL;
}

#endif // OPERA_AUTH_SUPPORT
