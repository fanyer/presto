/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef _PRINT_SUPPORT_
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/dochand/winman_constants.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#ifdef PREFS_HAVE_PRINT_JOB_TYPE
# include "platforms/photon/photon_opprintercontroller.h"
#endif 

#include "modules/prefs/prefsmanager/collections/pc_print_c.inl"

PrefsCollectionPrint *PrefsCollectionPrint::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcprint)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcprint = OP_NEW_L(PrefsCollectionPrint, (reader));
	return g_opera->prefs_module.m_pcprint;
}

PrefsCollectionPrint::~PrefsCollectionPrint()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCPRINT_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCPRINT_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcprint = NULL;
}

void PrefsCollectionPrint::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCPRINT_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCPRINT_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionPrint::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
	                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_VALIDATE
void PrefsCollectionPrint::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case PrinterScale:
		if (*value > 400)
			*value = 400;
		else if (*value < 20)
			*value = 20;
		break;

	case MarginLeft:
	case MarginRight:
	case MarginTop:
	case MarginBottom:
		if (*value < 0 || *value > 500)
			*value = GetDefaultIntegerPref(static_cast<integerpref>(which));
		break;

	case ShowPrintHeader:
	case PrintBackground:
#ifdef GENERIC_PRINTING
	case DefaultFramesPrintType:
#endif
#ifdef _PRINT_SUPPORT_
	case FitToWidthPrint:
#endif
	case PrintToFileWidth:
	case PrintToFileHeight:
	case PrintToFileDPI:
		break; // Nothing to do.

#ifdef PREFS_HAVE_PRINT_JOB_TYPE
	case PrintJobType:
		break; // Nothing to do.
#endif

#ifdef _UNIX_DESKTOP_
	case PrinterToFile:
		break;
#endif

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionPrint::CheckConditionsL(int which, const OpStringC &invalue,
                                            OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
#if defined GENERIC_PRINTING
	switch (static_cast<stringpref>(which))
	{
#ifdef GENERIC_PRINTING
	case PrintLeftHeaderString:
	case PrintRightHeaderString:
	case PrintLeftFooterString:
	case PrintRightFooterString:
		break; // Nothing to do.
#endif

#ifdef _UNIX_DESKTOP_
	case PrinterPrinterName:
	case PrinterFileName:
		break;
#endif

	default:
#else // GENERIC_PRINTING
	{
#endif // GENERIC_PRINTING

		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;

}
#endif // PREFS_VALIDATE
#endif // _PRINT_SUPPORT_
