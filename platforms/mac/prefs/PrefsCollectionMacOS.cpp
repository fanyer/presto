/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#include "core/pch.h"

#ifdef PREFS_HAVE_MAC

# include "platforms/mac/prefs/PrefsCollectionMacOS.h"
# include "modules/prefs/prefsmanager/collections/prefs_macros.h"

# include "platforms/mac/prefs/PrefsCollectionMacOS_c.inl"

PrefsCollectionMacOS *PrefsCollectionMacOS::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcmacos)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcmacos = OP_NEW_L(PrefsCollectionMacOS, (reader));
	return g_opera->prefs_module.m_pcmacos;
}

PrefsCollectionMacOS::~PrefsCollectionMacOS()
{
# ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCMACOS_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCMACOS_NUMBEROFINTEGERPREFS);
# endif

	g_opera->prefs_module.m_pcmacos = NULL;
}

void PrefsCollectionMacOS::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCMACOS_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCMACOS_NUMBEROFINTEGERPREFS);
}

# ifdef PREFS_VALIDATE
void PrefsCollectionMacOS::CheckConditionsL(int which, int *value, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case CombinedMailAndFeedNotifications:
		break;

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionMacOS::CheckConditionsL(int which, const OpStringC &invalue,
											OpString **outvalue, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
# endif // PREFS_VALIDATE

#endif // PREFS_HAVE_MAC
