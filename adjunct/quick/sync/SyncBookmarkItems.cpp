/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    shuais
 *
 */

#include "core/pch.h"

#include "adjunct/quick/sync/SyncBookmarkItems.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/models/DesktopBookmark.h"

#include "modules/bookmarks/bookmark_sync.h"

SyncBookmarkItems::SyncBookmarkItems()
: m_core_bookmarks_syncer(0)
, m_is_receiving_items(FALSE)
{
	m_core_bookmarks_syncer = OP_NEW(BookmarkSync, ());
	m_core_bookmarks_syncer->SetManager(g_bookmark_manager);
	g_bookmark_manager->SetBookmarkSync(m_core_bookmarks_syncer);
	m_core_bookmarks_syncer->SetCoordinator(g_sync_coordinator);

	g_hotlist_manager->GetBookmarksModel()->AddModelListener(this);
}

SyncBookmarkItems::~SyncBookmarkItems()
{
	g_bookmark_manager->SetBookmarkSync(NULL);
	OP_DELETE(m_core_bookmarks_syncer);
	m_core_bookmarks_syncer = 0;

	g_hotlist_manager->GetBookmarksModel()->RemoveModelListener(this);
}

OP_STATUS SyncBookmarkItems::SyncDataInitialize(OpSyncDataItem::DataItemType type )
{
	if (type == OpSyncDataItem::DATAITEM_BOOKMARK)
	{
		OpFile bm_file;
		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, bm_file));
		g_sync_manager->BackupFile(bm_file);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS SyncBookmarkItems::SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
{
	if (type == OpSyncDataItem::DATAITEM_BOOKMARK)
	{
		m_core_bookmarks_syncer->WaitingForDirtySync(is_dirty);
		m_core_bookmarks_syncer->ResyncAllBookmarks();
		m_core_bookmarks_syncer->WaitingForDirtySync(FALSE);
	}
	return OpStatus::OK;
}

OP_STATUS SyncBookmarkItems::SyncDataAvailable(OpSyncCollection* new_items, OpSyncDataError& data_error)
{
    OpSyncItemIterator iter(new_items->First());
    for (; iter.GetDataItem(); iter.Next())
    {
		OpSyncItem *current = iter.GetDataItem();
		OpSyncDataItem::DataItemType type = current->GetType();

		if (type == OpSyncDataItem::DATAITEM_BOOKMARK || type == OpSyncDataItem::DATAITEM_BOOKMARK_FOLDER
		    || type == OpSyncDataItem::DATAITEM_BOOKMARK_SEPARATOR)
		{
			// Hack for the 2 trash folders situation.(DSK-290031) We don't want to 
			// show 2 trash folders so whenever items are added/moved to the ghost 
			// trash folder we make it looks like the target is the real trash.
			OpString puid;
			current->GetData(OpSyncItem::SYNC_KEY_PARENT, puid);
			if (puid.HasContent())
			{
				BookmarkItem* ghost_trash = g_bookmark_manager->FindId(puid.CStr());
				if (ghost_trash && ghost_trash->GetFolderType() == FOLDER_TRASH_FOLDER 
					&& g_bookmark_manager->GetTrashFolder() && ghost_trash != g_bookmark_manager->GetTrashFolder())
				{
					const uni_char* trash_uid = g_bookmark_manager->GetTrashFolder()->GetUniqueId();
					current->SetData(OpSyncItem::SYNC_KEY_PARENT, trash_uid);
				}
			}

			{
				QuickSyncLock lock(m_is_receiving_items, TRUE, FALSE);

				if (OpStatus::IsMemoryError(m_core_bookmarks_syncer->NewItem(current)))
					return OpStatus::ERR_NO_MEMORY;
			}
		}
    }
    
	if (OpStatus::IsMemoryError(m_core_bookmarks_syncer->CleanupItems()))
		return OpStatus::ERR_NO_MEMORY;

	if (m_core_bookmarks_syncer->DirtySync())
	{
		m_core_bookmarks_syncer->DoneSyncing();
		data_error = SYNC_DATAERROR_INCONSISTENCY;
	}
	return OpStatus::OK;
}

OP_STATUS SyncBookmarkItems::SyncDataSupportsChanged(OpSyncDataItem::DataItemType, BOOL)
{
	return OpStatus::OK;
}

void SyncBookmarkItems::OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag)
{
	// Check if user has been logged in to Link once. If not changes should not be sent to the sync module
	if (!g_sync_manager->HasUsedSync())
		return;

	if (!m_is_receiving_items)
	{
		// Always notify the change no matter sync is active for bookmark or not.
		// BookmarkSync::BookmarkChanged would take care of it
		CoreBookmarkWrapper* core_bookmark = BookmarkWrapper(item);
		OP_ASSERT(core_bookmark);

		if (core_bookmark)
			m_core_bookmarks_syncer->BookmarkChanged(core_bookmark->GetCoreItem());
	}
}
