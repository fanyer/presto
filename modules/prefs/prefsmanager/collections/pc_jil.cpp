/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Wojciech Maslowski(wmaslowski@opera.com)
*/

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_jil.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_jil_c.inl"

PrefsCollectionJIL *PrefsCollectionJIL::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcjil)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcjil = OP_NEW_L(PrefsCollectionJIL, (reader));
	return g_opera->prefs_module.m_pcjil;
}

PrefsCollectionJIL::~PrefsCollectionJIL()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCJIL_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCJIL_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcjil = NULL;
}

void PrefsCollectionJIL::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCJIL_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCJIL_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionJIL::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionJIL::CheckConditionsL(int which, const OpStringC &invalue,
                                          OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case DeviceWidgetEngineName:
	case DeviceWidgetEngineProvider:
	case DeviceWidgetEngineVersion:
	case JilNetworkResourcesHostName:
	case ExportAsVCardDestinationDirectory:
		break;
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

#endif // DOM_JIL_API_SUPPORT
