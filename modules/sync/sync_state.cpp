/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/sync/sync_state.h"

OpSyncState::OpSyncState()
	: m_dirty(FALSE)
{
	for (unsigned i = 0; i < SYNC_SUPPORTS_MAX; i++)
		m_sync_state_item_persistent[i] = SYNC_STATE_PERSISTENT;
}

OpSyncState& OpSyncState::operator=(const OpSyncState& src)
{
	m_sync_state.Set(src.m_sync_state.CStr());
	m_dirty = src.m_dirty;

	// Copy over all the individual sync states
	for (int i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		m_sync_state_per_item[i].Set(src.m_sync_state_per_item[i]);
		m_sync_state_item_persistent[i] = src.m_sync_state_item_persistent[i];
	}
	return *this;
}

OP_STATUS OpSyncState::Update(const OpSyncState& src)
{
	RETURN_IF_ERROR(m_sync_state.Set(src.m_sync_state));
	m_dirty = src.m_dirty;

	// Copy over all the individual sync states
	for (int i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		if (m_sync_state_item_persistent[i] != SYNC_STATE_RESET)
		{
			RETURN_IF_ERROR(m_sync_state_per_item[i].Set(src.m_sync_state_per_item[i]));
			m_sync_state_item_persistent[i] = src.m_sync_state_item_persistent[i];
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpSyncState::ReadPrefs()
{
	OP_NEW_DBG("OpSyncState::ReadPrefs()", "sync");
	OpStringC state;
	// Read the main sync state pref
	state = g_pcsync->GetStringPref(PrefsCollectionSync::SyncClientState);
	RETURN_IF_ERROR(m_sync_state.Set(state));
	OP_DBG((UNI_L("sync-state: %s"), m_sync_state.CStr()));

	// Read all the sync states for the other types
	for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		OP_DBG(("") << static_cast<OpSyncSupports>(i) << ":");
		// Convert the supports to a pref
		PrefsCollectionSync::stringpref pref =
			OpSyncState::Supports2SyncStatePref(static_cast<OpSyncSupports>(i));
		// skip unsupported supports:
		if (PrefsCollectionSync::DummyLastStringPref == pref)
		{
			/* If we have no pref to get the sync state, so only overwrite the
			 * current value with "0" if the current state is empty: */
			if (m_sync_state_per_item[i].IsEmpty())
				RETURN_IF_ERROR(m_sync_state_per_item[i].Set("0"));
		}
		else if (IsSyncStatePersistent(static_cast<OpSyncSupports>(i)))
		{
			state = g_pcsync->GetStringPref(pref);
			RETURN_IF_ERROR(m_sync_state_per_item[i].Set(state));
			OP_DBG((UNI_L(" sync-state: %s"), m_sync_state_per_item[i].CStr()));
		}
	}

	return OpStatus::OK;
}

OP_STATUS OpSyncState::WritePrefs() const
{
	OP_NEW_DBG("OpSyncState::WritePrefs()", "sync");
	// Write out the main sync state preference
	RETURN_IF_LEAVE(g_pcsync->WriteStringL(PrefsCollectionSync::SyncClientState, m_sync_state));
	OP_DBG((UNI_L("sync-state: %s"), m_sync_state.CStr()));

	// Write all the sync states for the other types
	OP_MEMORY_VAR unsigned int i;
	for (i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		// Convert the supports to a pref
		OP_MEMORY_VAR PrefsCollectionSync::stringpref pref =
			OpSyncState::Supports2SyncStatePref(static_cast<OpSyncSupports>(i));
		// skip unsupported supports:
		if (PrefsCollectionSync::DummyLastStringPref != pref)
		{
			OP_DBG(("") << static_cast<OpSyncSupports>(i) << ":");
			if (IsSyncStatePersistent(static_cast<OpSyncSupports>(i)))
				RETURN_IF_LEAVE(g_pcsync->WriteStringL(pref, m_sync_state_per_item[i]));
			OP_DBG((UNI_L(" sync-state: %s"), m_sync_state_per_item[i].CStr()));
		}
	}

	return OpStatus::OK;
}

/* static */
OpSyncSupports OpSyncState::EnablePref2Supports(int pref)
{
	switch (pref) {
#ifdef SYNC_HAVE_BOOKMARKS
	case PrefsCollectionSync::SyncBookmarks:	return SYNC_SUPPORTS_BOOKMARK;
#endif // SYNC_HAVE_BOOKMARKS
#ifdef SYNC_HAVE_FEEDS
	case PrefsCollectionSync::SyncFeeds:		return SYNC_SUPPORTS_FEED;
#endif // SYNC_HAVE_FEEDS
#ifdef SYNC_HAVE_EXTENSIONS
	case PrefsCollectionSync::SyncExtensions:	return SYNC_SUPPORTS_EXTENSION;
#endif // SYNC_HAVE_EXTENSIONS
#ifdef SYNC_HAVE_NOTES
	case PrefsCollectionSync::SyncNotes:		return SYNC_SUPPORTS_NOTE;
#endif // SYNC_HAVE_NOTES
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	case PrefsCollectionSync::SyncPasswordManager: return SYNC_SUPPORTS_PASSWORD_MANAGER;
#endif // SYNC_HAVE_PASSWORD_MANAGER
#ifdef SYNC_HAVE_SEARCHES
	case PrefsCollectionSync::SyncSearches:		return SYNC_SUPPORTS_SEARCHES;
#endif // SYNC_HAVE_SEARCHES
#ifdef SYNC_HAVE_SPEED_DIAL
# ifdef SYNC_HAVE_SPEED_DIAL_2
	case PrefsCollectionSync::SyncSpeeddial:	return SYNC_SUPPORTS_SPEEDDIAL_2;
# else
	case PrefsCollectionSync::SyncSpeeddial:	return SYNC_SUPPORTS_SPEEDDIAL;
# endif // SYNC_HAVE_SPEED_DIAL_2
#endif // SYNC_HAVE_SPEED_DIAL
#ifdef SYNC_HAVE_TYPED_HISTORY
	case PrefsCollectionSync::SyncTypedHistory:	return SYNC_SUPPORTS_TYPED_HISTORY;
#endif // SYNC_HAVE_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case PrefsCollectionSync::SyncURLFilter:	return SYNC_SUPPORTS_URLFILTER;
#endif // SYNC_CONTENT_FILTERS

#ifdef SYNC_HAVE_PERSONAL_BAR
	case PrefsCollectionSync::SyncPersonalbar:
		return SYNC_SUPPORTS_MAX;	// it's a pseudo type
#endif // SYNC_HAVE_PERSONAL_BAR
	default:
		OP_ASSERT(!"Cannot convert this pref to OpSyncSupports");
		return SYNC_SUPPORTS_MAX;	// unsupported
	}
}

/* static */
PrefsCollectionSync::integerpref OpSyncState::Supports2EnablePref(OpSyncSupports supports)
{
	switch (supports) {
#ifdef SYNC_HAVE_BOOKMARKS
	case SYNC_SUPPORTS_BOOKMARK:	return PrefsCollectionSync::SyncBookmarks;
#endif // SYNC_HAVE_BOOKMARKS
#ifdef SYNC_HAVE_FEEDS
	case SYNC_SUPPORTS_FEED:		return PrefsCollectionSync::SyncFeeds;
#endif // SYNC_HAVE_FEEDS
#ifdef SYNC_HAVE_EXTENSIONS
	case SYNC_SUPPORTS_EXTENSION:	return PrefsCollectionSync::SyncExtensions;
#endif // SYNC_HAVE_EXTENSIONS
#ifdef SYNC_HAVE_NOTES
	case SYNC_SUPPORTS_NOTE:		return PrefsCollectionSync::SyncNotes;
#endif // SYNC_HAVE_NOTES
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	case SYNC_SUPPORTS_PASSWORD_MANAGER: return PrefsCollectionSync::SyncPasswordManager;
#endif // SYNC_HAVE_PASSWORD_MANAGER
#ifdef SYNC_HAVE_SEARCHES
	case SYNC_SUPPORTS_SEARCHES:	return PrefsCollectionSync::SyncSearches;
#endif // SYNC_HAVE_SEARCHES
#ifdef SYNC_HAVE_SPEED_DIAL
# ifdef SYNC_HAVE_SPEED_DIAL_2
	case SYNC_SUPPORTS_SPEEDDIAL_2:	return PrefsCollectionSync::SyncSpeeddial;
# else
	case SYNC_SUPPORTS_SPEEDDIAL:	return PrefsCollectionSync::SyncSpeeddial;
# endif // SYNC_HAVE_SPEED_DIAL_2
#endif // SYNC_HAVE_SPEED_DIAL
#ifdef SYNC_HAVE_TYPED_HISTORY
	case SYNC_SUPPORTS_TYPED_HISTORY: return PrefsCollectionSync::SyncTypedHistory;
#endif // SYNC_HAVE_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case SYNC_SUPPORTS_URLFILTER:	return PrefsCollectionSync::SyncURLFilter;
#endif // SYNC_CONTENT_FILTERS

	case SYNC_SUPPORTS_ENCRYPTION:
		/* encryption support is not enabled by a pref, it depends on other
		 * support types. So return DummyLastStringPref here. */
	default:
		return PrefsCollectionSync::DummyLastIntegerPref;	// unsupported
	}
}

/* static */
PrefsCollectionSync::stringpref OpSyncState::Supports2SyncStatePref(OpSyncSupports supports)
{
	switch (supports)
	{
#ifdef SYNC_HAVE_BOOKMARKS
	case SYNC_SUPPORTS_BOOKMARK:	return PrefsCollectionSync::SyncClientStateBookmarks;
#endif // SYNC_HAVE_BOOKMARKS
#ifdef SYNC_HAVE_EXTENSIONS
	case SYNC_SUPPORTS_EXTENSION:	return PrefsCollectionSync::SyncClientStateExtensions;
#endif // SYNC_HAVE_EXTENSIONS
#ifdef SYNC_HAVE_NOTES
	case SYNC_SUPPORTS_NOTE:		return PrefsCollectionSync::SyncClientStateNotes;
#endif // SYNC_HAVE_NOTES
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	case SYNC_SUPPORTS_PASSWORD_MANAGER: return PrefsCollectionSync::SyncClientStatePasswordManager;
#endif // SYNC_HAVE_PASSWORD_MANAGER
#ifdef SYNC_HAVE_SEARCHES
	case SYNC_SUPPORTS_SEARCHES:	return PrefsCollectionSync::SyncClientStateSearches;
#endif // SYNC_HAVE_SEARCHES
#ifdef SYNC_HAVE_SPEED_DIAL
# ifdef SYNC_HAVE_SPEED_DIAL_2
	case SYNC_SUPPORTS_SPEEDDIAL_2:	return PrefsCollectionSync::SyncClientStateSpeeddial2;
# else
	case SYNC_SUPPORTS_SPEEDDIAL:	return PrefsCollectionSync::SyncClientStateSpeeddial;
# endif // SYNC_HAVE_SPEED_DIAL_2
#endif // SYNC_HAVE_SPEED_DIAL
#ifdef SYNC_TYPED_HISTORY
	case SYNC_SUPPORTS_TYPED_HISTORY: return PrefsCollectionSync::SyncClientStateTypedHistory;
#endif // SYNC_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case SYNC_SUPPORTS_URLFILTER:	return PrefsCollectionSync::SyncClientStateURLFilter;
#endif // SYNC_CONTENT_FILTERS

	case SYNC_SUPPORTS_ENCRYPTION:
		/* encryption support does not write the sync state as a pref. So return
		 * DummyLastStringPref here. */
	case SYNC_SUPPORTS_CONTACT:
	case SYNC_SUPPORTS_FEED:
		/* These are not supported yet but need to be here to make the
		 * loops work. Ugly hack the unsupported types just shouldn't
		 * be here or ifdefed */
	default:
		return PrefsCollectionSync::DummyLastStringPref;
	}
}

#endif // SUPPORT_DATA_SYNC
