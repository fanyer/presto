/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef GEOLOCATION_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_geoloc_c.inl"

PrefsCollectionGeolocation *PrefsCollectionGeolocation::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcgeolocation)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcgeolocation = OP_NEW_L(PrefsCollectionGeolocation, (reader));
	return g_opera->prefs_module.m_pcgeolocation;
}

PrefsCollectionGeolocation::~PrefsCollectionGeolocation()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCGEOLOCATION_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCGEOLOCATION_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcgeolocation = NULL;
}

void PrefsCollectionGeolocation::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCGEOLOCATION_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCGEOLOCATION_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionGeolocation::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case EnableGeolocation:
	case SendLocationRequestOnlyOnChange:
		break; // Nothing to do

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionGeolocation::CheckConditionsL(int which, const OpStringC &invalue,
                                                  OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case LocationProviderUrl:
	case Google2011LocationProviderAccessToken:
		break; // Nothing to do

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

#endif // GEOLOCATION_SUPPORT
