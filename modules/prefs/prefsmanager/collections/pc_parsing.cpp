/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_parsing.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_parsing_c.inl"

PrefsCollectionParsing *PrefsCollectionParsing::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcparsing)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcparsing = OP_NEW_L(PrefsCollectionParsing, (reader));
	return g_opera->prefs_module.m_pcparsing;
}

PrefsCollectionParsing::~PrefsCollectionParsing()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCPARSING_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCPARSING_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcparsing = NULL;
}

void PrefsCollectionParsing::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCPARSING_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCPARSING_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionParsing::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case ShowHTMLParsingErrors:
		break;

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionParsing::CheckConditionsL(int which, const OpStringC &invalue,
                                              OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	// with a code like this : switch (static_cast<stringpref>(which))

	// Unhandled preference! For clarity, all preferenes not needing to
	// be checked should be put in an empty case something: break; clause
	// above.
	OP_ASSERT(!"Unhandled preference");

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE
