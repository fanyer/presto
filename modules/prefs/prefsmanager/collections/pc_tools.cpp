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

#ifdef SCOPE_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_tools_c.inl"

PrefsCollectionTools *PrefsCollectionTools::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pctools)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pctools = OP_NEW_L(PrefsCollectionTools, (reader));
	return g_opera->prefs_module.m_pctools;
}

PrefsCollectionTools::~PrefsCollectionTools()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCTOOLS_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCTOOLS_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pctools = NULL;
}

void PrefsCollectionTools::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCTOOLS_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCTOOLS_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionTools::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
#ifdef PREFS_TOOLS_PROXY
	case ProxyPort:
		if (*value < 1 || *value > 65535)
			*value = m_integerprefdefault[which].defval;
		break;
#endif

#ifdef PREFS_SCRIPT_DEBUG
	case ScriptDebugging:
		break; // Nothing to do
#endif

#ifdef PREFS_TOOLS_PROXY
	case ProxyAutoConnect:
		break; // Nothing to do
#endif

#ifdef WEBSERVER_SUPPORT
	case UPnPWebserverPort:
		if (*value < 1 || *value > 65535)
			*value = m_integerprefdefault[which].defval;
		break;
#endif

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionTools::CheckConditionsL(int which, const OpStringC &invalue,
                                            OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
#ifdef PREFS_TOOLS_PROXY
	case ProxyHost:
		// FIXME: Check if this is a valid IP address / host name?
		break;
#endif

#ifdef INTEGRATED_DEVTOOLS_SUPPORT
	case DevToolsUrl:
		break;
#endif
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

#endif // SCOPE_SUPPORT
