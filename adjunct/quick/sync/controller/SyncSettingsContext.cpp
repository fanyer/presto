/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/sync/controller/SyncSettingsContext.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
**  SyncSettingsContext::GetFeatureStringID
**  
************************************************************************************/
SyncSettingsContext::SyncSettingsContext() :
	m_supports_bookmarks(FALSE)
	, m_supports_speeddial(FALSE)
	, m_supports_personalbar(FALSE)
#ifdef SUPPORT_SYNC_NOTES
	, m_supports_notes(FALSE)
#endif // SUPPORT_SYNC_NOTES
#ifdef SYNC_TYPED_HISTORY
	, m_supports_typed_history(FALSE)
#endif // SYNC_TYPED_HISTORY
#ifdef SUPPORT_SYNC_SEARCHES
	, m_supports_searches(FALSE)
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_CONTENT_FILTERS
	, m_supports_urlfilter(FALSE)
#endif // SYNC_CONTENT_FILTERS
	, m_supports_password_manager(FALSE)
{
}


/***********************************************************************************
**  SyncSettingsContext::GetFeatureType
**  @return
************************************************************************************/
FeatureType SyncSettingsContext::GetFeatureType() const
{
	return FeatureTypeSync;
}


/***********************************************************************************
**  SyncSettingsContext::GetFeatureStringID
**  @return
************************************************************************************/
Str::LocaleString SyncSettingsContext::GetFeatureStringID() const
{
	return Str::D_SYNC_NAME;
}


/***********************************************************************************
**  SyncSettingsContext::GetFeatureLongStringID
**  @return
************************************************************************************/
Str::LocaleString SyncSettingsContext::GetFeatureLongStringID() const
{
	return Str::D_SYNC_NAME_LONG;
}


/***********************************************************************************
**  SyncSettingsContext::SupportsBookmarks
**  @return
************************************************************************************/
BOOL SyncSettingsContext::SupportsBookmarks() const
{
	return m_supports_bookmarks;
}


/***********************************************************************************
**  SyncSettingsContext::SupportsSpeedDial
**  @return
************************************************************************************/
BOOL SyncSettingsContext::SupportsSpeedDial() const
{
	return m_supports_speeddial;
}


/***********************************************************************************
**  SyncSettingsContext::SupportsPersonalbar
**  @return
************************************************************************************/
BOOL SyncSettingsContext::SupportsPersonalbar() const
{
	return m_supports_personalbar;
}


#ifdef SUPPORT_SYNC_NOTES
/***********************************************************************************
**  SyncSettingsContext::SupportsNotes
**  @return
************************************************************************************/
BOOL SyncSettingsContext::SupportsNotes() const
{
	return m_supports_notes;
}
#endif // SUPPORT_SYNC_NOTES


#ifdef SYNC_TYPED_HISTORY
/***********************************************************************************
**  SyncSettingsContext::SupportsTypedHistory
**  @return
************************************************************************************/
BOOL SyncSettingsContext::SupportsTypedHistory() const
{
	return m_supports_typed_history;
}
#endif // SYNC_TYPED_HISTORY


#ifdef SUPPORT_SYNC_SEARCHES
/***********************************************************************************
**  SyncSettingsContext::SupportsSearches
**  @return
************************************************************************************/
BOOL SyncSettingsContext::SupportsSearches() const
{
	return m_supports_searches;
}
#endif // SUPPORT_SYNC_SEARCHES

/***********************************************************************************
**  SyncSettingsContext::SupportsPassword
**  @return
************************************************************************************/
BOOL SyncSettingsContext::SupportsPasswordManager() const
{
	return m_supports_password_manager;
}

/***********************************************************************************
**  SyncSettingsContext::SetSupportsBookmarks
**  @param supports_bookmarks
************************************************************************************/
void SyncSettingsContext::SetSupportsBookmarks(BOOL supports_bookmarks)
{
	m_supports_bookmarks = supports_bookmarks;
}


/***********************************************************************************
**  SyncSettingsContext::SetSupportsSpeedDial
**  @param supports_speeddial
************************************************************************************/
void SyncSettingsContext::SetSupportsSpeedDial(BOOL supports_speeddial)
{
	m_supports_speeddial = supports_speeddial;
}


/***********************************************************************************
**  SyncSettingsContext::SetSupportsPersonalbar
**  @param supports_personalbar
************************************************************************************/
void SyncSettingsContext::SetSupportsPersonalbar(BOOL supports_personalbar)
{
	m_supports_personalbar = supports_personalbar;
}


#ifdef SUPPORT_SYNC_NOTES
/***********************************************************************************
**  SyncSettingsContext::SetSupportsNotes
**  @param supports_notes
************************************************************************************/
void SyncSettingsContext::SetSupportsNotes(BOOL supports_notes)
{
	m_supports_notes = supports_notes;
}
#endif // SUPPORT_SYNC_NOTES



#ifdef SYNC_TYPED_HISTORY
/***********************************************************************************
**  SyncSettingsContext::SetSupportsTypedHistory
**  @param supports_typed_history
************************************************************************************/
void SyncSettingsContext::SetSupportsTypedHistory(BOOL supports_typed_history)
{
	m_supports_typed_history = supports_typed_history;
}
#endif // SYNC_TYPED_HISTORY


#ifdef SUPPORT_SYNC_SEARCHES
/***********************************************************************************
**  SyncSettingsContext::SetSupportsSearches
**  @param supports_searches
************************************************************************************/
void SyncSettingsContext::SetSupportsSearches(BOOL supports_searches)
{
	m_supports_searches = supports_searches;
}
#endif // SUPPORT_SYNC_SEARCHES

/***********************************************************************************
**  SyncSettingsContext::SetSupportsPassword
**  @param supports_searches
************************************************************************************/
void SyncSettingsContext::SetSupportsPasswordManager(BOOL supports_password_manager)
{
	m_supports_password_manager = supports_password_manager;
}

	

BOOL SyncSettingsContext::GetSupportsType(OpSyncSupports supports_type)
{
	switch (supports_type)
	{
	case SYNC_SUPPORTS_PASSWORD_MANAGER:
		{
			return SupportsPasswordManager();
		}
	case SYNC_SUPPORTS_BOOKMARK:
		{
			return SupportsBookmarks();
		}
	case SYNC_SUPPORTS_SPEEDDIAL_2:
		{
			return SupportsSpeedDial();
		}

#ifdef SUPPORT_SYNC_NOTES
	case SYNC_SUPPORTS_NOTE:
		{
			return SupportsNotes();
		}
#endif // SUPPORT_SYNC_NOTES

#ifdef SUPPORT_SYNC_SEARCHES
	case SYNC_SUPPORTS_SEARCHES:
		{
			return SupportsSearches();
		}
#endif // SUPPORT_SYNC_SEARCHES

#ifdef SYNC_TYPED_HISTORY
	case SYNC_SUPPORTS_TYPED_HISTORY:
		{
			return SupportsTypedHistory();
		}
#endif // SYNC_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case SYNC_SUPPORTS_URLFILTER:
		{
			return SupportsURLFilter();
		}
#endif // SYNC_CONTENT_FILTERS
	case SYNC_SUPPORTS_CONTACT:
	case SYNC_SUPPORTS_FEED:
		{
			OP_ASSERT(!"not supported yet");
			break;
		}
	default:
		{
			OP_ASSERT(!"urecognized OpSyncSupports type");
		}
	}
	return FALSE;
}


void  SyncSettingsContext::SetSupportsType(OpSyncSupports supports_type, BOOL supported)
{
	switch (supports_type)
	{
	case SYNC_SUPPORTS_PASSWORD_MANAGER:
		{
			SetSupportsPasswordManager(supported);
			break;
		}
	case SYNC_SUPPORTS_BOOKMARK:
		{
			SetSupportsBookmarks(supported);
			break;
		}
	case SYNC_SUPPORTS_SPEEDDIAL_2:
		{
			SetSupportsSpeedDial(supported);
			break;
		}

#ifdef SUPPORT_SYNC_NOTES
	case SYNC_SUPPORTS_NOTE:
		{
			SetSupportsNotes(supported);
			break;
		}
#endif // SUPPORT_SYNC_NOTES

#ifdef SUPPORT_SYNC_SEARCHES
	case SYNC_SUPPORTS_SEARCHES:
		{
			SetSupportsSearches(supported);
			break;
		}
#endif // SUPPORT_SYNC_SEARCHES

#ifdef SYNC_TYPED_HISTORY
	case SYNC_SUPPORTS_TYPED_HISTORY:
		{
			SetSupportsTypedHistory(supported);
			break;
		}
#endif // SYNC_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case SYNC_SUPPORTS_URLFILTER:
		{
			SetSupportsURLFilter(supported);
			break;
		}
#endif // SYNC_CONTENT_FILTERS
	case SYNC_SUPPORTS_CONTACT:
	case SYNC_SUPPORTS_FEED:
		{
			OP_ASSERT(!"not supported yet");
			break;
		}
	default:
		{
			OP_ASSERT(!"urecognized OpSyncSupports type");
		}
	}
}


#endif // SUPPORT_DATA_SYNC
