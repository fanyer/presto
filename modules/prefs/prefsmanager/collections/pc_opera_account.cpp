/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef PREFS_HAVE_OPERA_ACCOUNT
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_opera_account_c.inl"

PrefsCollectionOperaAccount *PrefsCollectionOperaAccount::CreateL(PrefsFile *reader)
{
    if (g_opera->prefs_module.m_pcopera_account)
		LEAVE(OpStatus::ERR);
    g_opera->prefs_module.m_pcopera_account = OP_NEW_L(PrefsCollectionOperaAccount, (reader));
    return g_opera->prefs_module.m_pcopera_account;
}

PrefsCollectionOperaAccount::~PrefsCollectionOperaAccount()
{
#ifdef PREFS_COVERAGE
    CoverageReport(
	m_stringprefdefault, PCOPERA_ACCOUNT_NUMBEROFSTRINGPREFS,
	m_integerprefdefault, PCOPERA_ACCOUNT_NUMBEROFINTEGERPREFS);
#endif

    g_opera->prefs_module.m_pcopera_account = NULL;
}

void PrefsCollectionOperaAccount::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
    // Read everything
    OpPrefsCollection::ReadAllPrefsL(
	m_stringprefdefault, PCOPERA_ACCOUNT_NUMBEROFSTRINGPREFS,
	m_integerprefdefault, PCOPERA_ACCOUNT_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionOperaAccount::CheckConditionsL(int which, int *value, const uni_char *)
{
    // Check any post-read/pre-write conditions and adjust value accordingly
    switch (static_cast<integerpref>(which))
    {
	case OperaAccountUsed:
	case SavePassword:
	break; // Nothing to do.

    default:
	// Unhandled preference! For clarity, all preferenes not needing to
	// be checked should be put in an empty case something: break; clause
	// above.
	OP_ASSERT(!"Unhandled preference");
    }
}

BOOL PrefsCollectionOperaAccount::CheckConditionsL(int which, const OpStringC &invalue, OpString **outvalue, const uni_char *)
{
    // Check any post-read/pre-write conditions and adjust value accordingly
    switch (static_cast<stringpref>(which))
    {
#ifdef WEBSERVER_SUPPORT
	case ComputerID:
#endif
	case ServerAddress:
	case Username:
	case OAuthOperaAccountConsumerSecret:
	case OAuthOperaAccountConsumerKey:
	case OAuthOperaAccountAccessTokenURI:


	break; // Nothing to do.

    default:
	// Unhandled preference! For clarity, all preferenes not needing to
	// be checked should be put in an empty case something: break; clause
	// above.
	OP_ASSERT(!"Unhandled preference");
    }

    // When FALSE is returned, no OpString is created for outvalue
    return FALSE;
}
#endif // PREFS_VALIDATE

#endif // PREFS_HAVE_OPERA_ACCOUNT
