/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_sync_c.inl"

PrefsCollectionSync *PrefsCollectionSync::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcsync)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcsync = OP_NEW_L(PrefsCollectionSync, (reader));
	return g_opera->prefs_module.m_pcsync;
}

PrefsCollectionSync::~PrefsCollectionSync()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCSYNC_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCSYNC_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcsync = NULL;
}

void PrefsCollectionSync::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCSYNC_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCSYNC_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionSync::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case CompleteSync:
	case LastCachedAccess:
	case LastCachedAccessNum:
#ifdef SYNC_HAVE_BOOKMARKS
	case SyncBookmarks:
#endif // SYNC_HAVE_BOOKMARKS
	case SyncEnabled:
#ifdef SYNC_HAVE_EXTENSIONS
	case SyncExtensions:
#endif // SYNC_HAVE_EXTENSIONS
#ifdef SYNC_HAVE_FEEDS
	case SyncFeeds:
#endif // SYNC_HAVE_FEEDS
	case SyncLastUsed:
	case SyncLogTraffic:
#ifdef SYNC_HAVE_NOTES
	case SyncNotes:
#endif // SYNC_HAVE_NOTES
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	case SyncPasswordManager:
#endif // SYNC_HAVE_PASSWORD_MANAGER
#ifdef SYNC_HAVE_PERSONAL_BAR
	case SyncPersonalbar:
#endif // SYNC_HAVE_PERSONAL_BAR
#ifdef SYNC_HAVE_SEARCHES
	case SyncSearches:
#endif // SYNC_HAVE_SEARCHES
#ifdef SYNC_HAVE_SPEED_DIAL
	case SyncSpeeddial:
#endif // SYNC_HAVE_SPEED_DIAL
#ifdef SYNC_HAVE_TYPED_HISTORY
	case SyncTypedHistory:
#endif // SYNC_HAVE_TYPES_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case SyncURLFilter:
#endif // SYNC_CONTENT_FILTERS
	case SyncUsed:
		break; // Nothing to do.

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionSync::CheckConditionsL(int which, const OpStringC &invalue,
                                           OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case ServerAddress:
	case SyncClientState:
	case SyncClientStateBookmarks:
#ifdef SYNC_HAVE_EXTENSIONS
	case SyncClientStateExtensions:
#endif // SYNC_HAVE_EXTENSIONS
#ifdef SYNC_HAVE_NOTES
	case SyncClientStateNotes:
#endif // SYNC_HAVE_NOTES
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	case SyncClientStatePasswordManager:
#endif // SYNC_HAVE_PASSWORD_MANAGER
#ifdef SYNC_HAVE_SEARCHES
	case SyncClientStateSearches:
#endif // SYNC_HAVE_SEARCHES
#ifdef SYNC_HAVE_SPEED_DIAL
# ifdef SYNC_HAVE_SPEED_DIAL_2
	case SyncClientStateSpeeddial2:
# else // !SYNC_HAVE_SPEED_DIAL_2
	case SyncClientStateSpeeddial:
# endif // SYNC_HAVE_SPEED_DIAL_2
#endif // SYNC_HAVE_SPEED_DIAL
#ifdef SYNC_TYPED_HISTORY
	case SyncClientStateTypedHistory:
#endif // SYNC_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case SyncClientStateURLFilter:
#endif // SYNC_CONTENT_FILTERS
	case SyncDataProvider:
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

#endif // SUPPORT_DATA_SYNC
