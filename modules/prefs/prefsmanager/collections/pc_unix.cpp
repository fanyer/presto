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

#ifdef PREFS_HAVE_UNIX

#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_unix_c.inl"

PrefsCollectionUnix *PrefsCollectionUnix::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcunix)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcunix = OP_NEW_L(PrefsCollectionUnix, (reader));
	return g_opera->prefs_module.m_pcunix;
}

PrefsCollectionUnix::~PrefsCollectionUnix()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCUNIX_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCUNIX_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcunix = NULL;
}

void PrefsCollectionUnix::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCUNIX_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCUNIX_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionUnix::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case OpenDialogUnderMouse:
	case WindowPlacementByOpera:
	case PanningMode:
#ifdef PREFS_HAVE_X_FONTS
	case DrawAntiAliasedFonts:
	case ForceDPI:
#endif
#ifdef PREFS_HAVE_META_KEY
	case MapMetaButtonToAltButton:
#endif
#ifdef PREFS_HAVE_APPEND_DEFAULT_EXT
	case AppendExtensionToFilename:
#endif
#ifdef PREFS_HAVE_UNIX_PLUGIN
	case HasShownPluginError:
#endif
#ifdef _UNIX_DESKTOP_
	case EnableEditTripleClick:
	case LocalhostInDnD:
	case HideMouseCursor:
	case HasRestoredExtenstions:
	case HasRestoredMIMEFlag:
	case HasShownKDEShortcutMessage:
	case WindowMenuMaxItemWidth:
	case PreferFontconfigSettings:
	case ShowWindowBorder:
#endif
#ifdef PREFS_HAVE_WORKSPACE
	case WorkSpaceButtonStyle:
	case WorkspaceWindowBorderWidth:
	case WorkspaceButtonType:
#endif
#ifdef PREFS_HAVE_UNIX_PLUGIN
	case ReadingPlugins:
#endif
#ifdef PREFS_HAVE_FILESELECTOR
	case FileSelectorShowHiddenFiles:
	case FileSelectorShowDetails:
	case FileSelectorToolkit:
#endif
		break;

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionUnix::CheckConditionsL(int which, const OpStringC &invalue,
                                           OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
#ifdef _UNIX_DESKTOP_
	case CustomColors:
		break;
#endif

#ifdef PREFS_HAVE_NEW_WIN_SIZE
	case NewWinSize:
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

#endif // PREFS_HAVE_UNIX
