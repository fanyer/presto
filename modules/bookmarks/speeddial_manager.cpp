/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CORE_SPEED_DIAL_SUPPORT

#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/speeddial_manager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

SpeedDialManager::SpeedDialManager() :
	m_speed_dial_folder(NULL), m_speed_dial_pages(0)
{
	g_bookmark_manager->RegisterBookmarkManagerListener(this);
}

OP_STATUS SpeedDialManager::Init()
{
#ifdef OPERASPEEDDIAL_URL
	RETURN_IF_ERROR(m_url_generator.Construct("speeddial", FALSE));
	g_url_api->RegisterOperaURL(&m_url_generator);
#endif
	
	for (int i = 0; i < NUM_SPEEDDIALS; i++)
	{
		SpeedDial* speed_dial = OP_NEW(SpeedDial, (i));

		if (!speed_dial || OpStatus::IsError(m_speed_dials.Add(speed_dial)))
		{
			OP_DELETE(speed_dial);

			m_speed_dials.DeleteAll();
			break;
		}
	}
	
	return OpStatus::OK;
}
	
SpeedDialManager::~SpeedDialManager()
{
	g_bookmark_manager->UnregisterBookmarkManagerListener(this);
}


OP_STATUS SpeedDialManager::SetSpeedDial(int index, const uni_char* url)
{
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSpeedDial))
		return OpStatus::ERR;

	RETURN_IF_ERROR(InitBookmarkStorage());

	m_speed_dials.Get(index)->SetSpeedDial(url);
	return OpStatus::OK;
}

OP_STATUS SpeedDialManager::ReloadSpeedDial(int index)
{
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSpeedDial))
		return OpStatus::ERR;

	SpeedDial* sd = m_speed_dials.Get(index);

	if (sd && !sd->IsEmpty())
		return sd->Reload();

	return OpStatus::OK;
}

OP_STATUS SpeedDialManager::SetSpeedDialReloadInterval(int index, int seconds)
{
	SpeedDial* sd = m_speed_dials.Get(index);

	return sd->SetReload(seconds ? TRUE : FALSE, seconds);
}

OP_STATUS SpeedDialManager::SwapSpeedDials(int id1, int id2)
{
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSpeedDial))
		return OpStatus::ERR;

	if (id1 != id2)
	{
		SpeedDial* sd1 = m_speed_dials.Get(id1);
		SpeedDial* sd2 = m_speed_dials.Get(id2);

		if (!(sd1->GetStorage() && sd2->GetStorage()))
			RETURN_IF_ERROR(InitBookmarkStorage());

		int temp_pos = sd1->GetPosition();
		sd1->SetPosition(sd2->GetPosition());
		sd2->SetPosition(temp_pos);

		RETURN_IF_ERROR(m_speed_dials.Replace(id1, sd2));
		RETURN_IF_ERROR(m_speed_dials.Replace(id2, sd1));

		RETURN_IF_ERROR(sd1->NotifySpeedDialSwapped());
		RETURN_IF_ERROR(sd2->NotifySpeedDialSwapped());
		
		if(OpStatus::IsError(g_bookmark_manager->Swap(sd1->GetStorage(), sd2->GetStorage())))
		{
			OP_ASSERT(!"This is bad. The speed dials or the bookmark lists appears to be in an inconsistent state!");
		}

#ifdef SUPPORT_DATA_SYNC
		BookmarkItem *bookmark1 = sd1->GetStorage();
		bookmark1->ClearSyncFlags();
		if (sd1->IsEmpty())
			bookmark1->SetDeleted(TRUE);
		else
		{
			if (sd2->IsEmpty())
				bookmark1->SetAdded(TRUE);
			else
				bookmark1->SetModified(TRUE);

			bookmark1->SetSyncAttribute(BOOKMARK_TITLE, TRUE);
			bookmark1->SetSyncAttribute(BOOKMARK_URL, TRUE);
			bookmark1->SetSyncAttribute(BOOKMARK_FAVICON_FILE, TRUE);
			bookmark1->SetSyncAttribute(BOOKMARK_THUMBNAIL_FILE, TRUE);
			bookmark1->SetSyncAttribute(BOOKMARK_SD_RELOAD_ENABLED, TRUE);
			bookmark1->SetSyncAttribute(BOOKMARK_SD_RELOAD_INTERVAL, TRUE);
			bookmark1->SetSyncAttribute(BOOKMARK_SD_RELOAD_EXPIRED, TRUE);
		}

		BookmarkItem *bookmark2 = sd2->GetStorage();
		bookmark2->ClearSyncFlags();
		if (sd2->IsEmpty())
			bookmark2->SetDeleted(TRUE);
		else
		{
			if (sd1->IsEmpty())
				bookmark2->SetAdded(TRUE);
			else
				bookmark2->SetModified(TRUE);

			bookmark2->SetSyncAttribute(BOOKMARK_TITLE, TRUE);
			bookmark2->SetSyncAttribute(BOOKMARK_URL, TRUE);
			bookmark2->SetSyncAttribute(BOOKMARK_FAVICON_FILE, TRUE);
			bookmark2->SetSyncAttribute(BOOKMARK_THUMBNAIL_FILE, TRUE);
			bookmark2->SetSyncAttribute(BOOKMARK_SD_RELOAD_ENABLED, TRUE);
			bookmark2->SetSyncAttribute(BOOKMARK_SD_RELOAD_INTERVAL, TRUE);
			bookmark2->SetSyncAttribute(BOOKMARK_SD_RELOAD_EXPIRED, TRUE);
		}

		g_bookmark_manager->NotifyBookmarkChanged(bookmark1);
		g_bookmark_manager->NotifyBookmarkChanged(bookmark2);

		bookmark1->ClearSyncFlags();
		bookmark2->ClearSyncFlags();
#endif // SUPPORT_DATA_SYNC
	}	

	// Trigger a bookmark save.
	g_bookmark_manager->SetSaveTimer();
	
	return OpStatus::OK;
}

OP_STATUS SpeedDialManager::AddSpeedDialListener(OpSpeedDialListener *listener)
{
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSpeedDial))
		return OpStatus::ERR;

	for (int i=0; i < NUM_SPEEDDIALS; i++)
	{
		SpeedDial* sd = m_speed_dials.Get(i);
		
		sd->AddListener(listener);

#ifdef SUPPORT_GENERATE_THUMBNAILS
		if (m_speed_dial_pages == 0)
			g_thumbnail_manager->AddListener(sd);

 		if (!sd->IsEmpty())
			RETURN_IF_ERROR(sd->StartLoading(FALSE));
#endif // SUPPORT_GENERATE_THUMBNAILS
	}

	m_speed_dial_pages++;

	return OpStatus::OK;
}

void SpeedDialManager::RemoveSpeedDialListener(OpSpeedDialListener *listener)
{
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSpeedDial))
		return;

	m_speed_dial_pages--;

	for (int i=0; i < NUM_SPEEDDIALS; i++)
	{
		m_speed_dials.Get(i)->RemoveListener(listener);

#ifdef SUPPORT_GENERATE_THUMBNAILS
		if (m_speed_dial_pages == 0)
			g_thumbnail_manager->RemoveListener(m_speed_dials.Get(i));
#endif
	}
}

void SpeedDialManager::OnBookmarksSaved(OP_STATUS ret, UINT32 operation_count)
{
	// Nothing to do
}

void SpeedDialManager::OnBookmarksLoaded(OP_STATUS ret, UINT32 operation_count)
{
	if (OpStatus::IsError(ret))
		return;

	m_speed_dial_folder = g_bookmark_manager->GetSpeedDialFolder();

	if (!m_speed_dial_folder)
	{
		m_speed_dial_folder = OP_NEW(BookmarkItem, ());
		if (!m_speed_dial_folder)
			return;

		m_speed_dial_folder->SetFolderType(FOLDER_SPEED_DIAL_FOLDER);
		OP_STATUS res = g_bookmark_manager->AddBookmark(m_speed_dial_folder, g_bookmark_manager->GetRootFolder());
		if (OpStatus::IsError(res))
		{
			OP_DELETE(m_speed_dial_folder);
			m_speed_dial_folder = NULL;
			return;
		}

		BookmarkAttribute attr;
		attr.SetAttributeType(BOOKMARK_TITLE);
		attr.SetTextValue(UNI_L("Speed dial folder"));	// Need localized string here?
		m_speed_dial_folder->SetAttribute(BOOKMARK_TITLE, &attr);
	}
	
	unsigned bm_loaded = 0;
	for (BookmarkItem* bm = (BookmarkItem*)m_speed_dial_folder->FirstChild(); bm && bm_loaded < NUM_SPEEDDIALS; bm = (BookmarkItem*)bm->Suc(), bm_loaded++)
	{
		m_speed_dials.Get(bm_loaded)->SetBookmarkStorage(bm);
	}

	for (int i = bm_loaded; i < NUM_SPEEDDIALS; i++)
	{
		BookmarkItem* bm = OP_NEW(BookmarkItem, ());
		if (!bm)
			return;
		if (OpStatus::IsError(g_bookmark_manager->AddBookmark(bm, m_speed_dial_folder)))
		{
			OP_DELETE(bm);
			return;
		}

		m_speed_dials.Get(i)->SetBookmarkStorage(bm);
	}

	// If we have any open speed dials we start loading the previews.
#ifdef SUPPORT_GENERATE_THUMBNAILS
	if (m_speed_dial_pages > 0)
		for (int i = 0; i < NUM_SPEEDDIALS; i++)
		{
			if (!m_speed_dials.Get(i)->IsEmpty())
				OpStatus::Ignore(m_speed_dials.Get(i)->StartLoading(FALSE));
		}
#endif
}

int SpeedDialManager::GetSpeedDialPosition(BookmarkItem *speed_dial) const
{
	int i=0;
	for (; i < NUM_SPEEDDIALS; i++)
		if (m_speed_dials.Get(i)->GetStorage() == speed_dial)
			return i;
	return -1;
}

SpeedDial* SpeedDialManager::GetSpeedDial(int position) const
{
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSpeedDial))
		return NULL;

	OP_ASSERT(position > -1 && position < NUM_SPEEDDIALS);
	return 	m_speed_dials.Get(position);
}

#ifdef SUPPORT_DATA_SYNC
void SpeedDialManager::OnBookmarksSynced(OP_STATUS ret)
{
//	OP_ASSERT(!"Implement me!");
}
#endif // SUPPORT_DATA_SYNC

OP_STATUS SpeedDialManager::InitBookmarkStorage()
{	
	m_speed_dial_folder = g_bookmark_manager->GetSpeedDialFolder();

	if (!m_speed_dial_folder)
	{
		m_speed_dial_folder = OP_NEW(BookmarkItem, ());
		if (!m_speed_dial_folder)
			return OpStatus::ERR_NO_MEMORY;

		m_speed_dial_folder->SetFolderType(FOLDER_SPEED_DIAL_FOLDER);
		OP_STATUS res = g_bookmark_manager->AddBookmark(m_speed_dial_folder, g_bookmark_manager->GetRootFolder());
		if (OpStatus::IsError(res))
		{
			OP_DELETE(m_speed_dial_folder);
			m_speed_dial_folder = NULL;
			return res;
		}
	}

	BookmarkAttribute attr;
	attr.SetAttributeType(BOOKMARK_TITLE);
	attr.SetTextValue(UNI_L("Speed dial folder"));	// Need localized string here?
	m_speed_dial_folder->SetAttribute(BOOKMARK_TITLE, &attr);


	for (int i = 0; i < NUM_SPEEDDIALS; i++)
	{
		if (!m_speed_dials.Get(i)->GetStorage())
		{
			BookmarkItem* bm = OP_NEW(BookmarkItem, ());
			if (!bm)
				return OpStatus::ERR_NO_MEMORY;
			OP_STATUS res = g_bookmark_manager->AddBookmark(bm, m_speed_dial_folder);
			if (OpStatus::IsError(res))
			{
				OP_DELETE(bm);
				return res;
			}

			m_speed_dials.Get(i)->SetBookmarkStorage(bm);
		}
	}

	return OpStatus::OK;
}

#endif // CORE_SPEED_DIAL_SUPPORT
