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

#ifdef PREFS_HAVE_MSWIN
#include "platforms/windows/prefs/PrefsCollectionMSWIN.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "platforms/windows/prefs/PrefsCollectionMSWIN_c.inl"
#include "platforms/windows/pi/DWriteOpFont.h"

PrefsCollectionMSWIN *PrefsCollectionMSWIN::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcmswin)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcmswin = OP_NEW_L(PrefsCollectionMSWIN, (reader));
	return g_opera->prefs_module.m_pcmswin;
}

PrefsCollectionMSWIN::~PrefsCollectionMSWIN()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCMSWIN_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCMSWIN_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcmswin = NULL;
}

void PrefsCollectionMSWIN::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCMSWIN_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCMSWIN_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionMSWIN::CheckConditionsL(int which, int *value, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
#ifdef _RAS_SUPPORT_
	case ShowCloseRasDialog:
#endif
	case OperaProduct:
	case DDEEnabled:
	case MaxCachedBitmaps:
	case MinAllowedGDIResources:
	case ShowWarningMSIMG32:
#ifdef DOXYGEN_DOCUMENTATION
	case MultiUser:
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	case UseScreenReaderMenus:
#endif
	case EnableAccessibilitySupport:
		break;

	case EnableTouchUI:
	{
		if(*value < 0 || *value > 2)
			*value = 1;
	}
	break;

	case UseGDIMeasuringMode:
	{
		if (*value < 0 || *value > 1)
			*value = 0;

		DWriteTextAnalyzer::SetMeasuringMode(*value);

		break;
	}

	case PaintThrottling:
		if (*value < 0)
			*value = 0;
		break;

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionMSWIN::CheckConditionsL(int which, const OpStringC &invalue,
                                            OpString **outvalue, const uni_char *host)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case OperaLabsName:
	case ApplicationIconFile:
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

#endif // PREFS_HAVE_MSWIN
