/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT

#include "modules/util/opstring.h"

#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_dataitem.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/url/url_api.h"

#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_sync.h"
#include "modules/bookmarks/bookmark_operation.h"

#ifdef CORE_SPEED_DIAL_SUPPORT
#include "modules/bookmarks/speeddial_manager.h"
#endif // CORE_SPEED_DIAL_SUPPORT

BookmarkSync::BookmarkSync()
{
	OP_NEW_DBG("BookmarkSync::BookmarkSync","bookmarks.synchronization");

    m_manager = NULL;
    m_coordinator = NULL;
	m_dirty_sync = FALSE;
	m_waiting_dirty_sync = FALSE;
}

BookmarkSync::~BookmarkSync()
{
	OP_NEW_DBG("BookmarkSync::~BookmarkSync","bookmarks.synchronization");

	m_added.Clear();
	m_moved.Clear();
}

void BookmarkSync::SetManager(BookmarkManager *manager)
{
	OP_NEW_DBG("BookmarkSync::SetManager","bookmarks.synchronization");

    m_manager = manager;
}

void BookmarkSync::SetCoordinator(OpSyncCoordinator *coordinator)
{
	OP_NEW_DBG("BookmarkSync::SetCoordinator","bookmarks.synchronization");

    m_coordinator = coordinator;
}

static OP_STATUS bookmark_resolve_url(BookmarkAttribute *attr, OpString &fixed)
{
	OP_NEW_DBG("bookmark_resolve_url","bookmarks.synchronization");

	TRAPD(rc, g_url_api->ResolveUrlNameL(attr->GetTextValue(), fixed));
	return rc;
}

OP_STATUS BookmarkSync::NewItem(OpSyncItem *item)
{
	OP_NEW_DBG("BookmarkSync::NewItem","bookmarks.synchronization");

    OpString str;
    BookmarkItem *bookmark;
	BookmarkItem *prev_bookmark = NULL;
#ifdef CORE_SPEED_DIAL_SUPPORT
	SpeedDial *speed_dial = NULL;
#endif // CORE_SPEED_DIAL_SUPPORT
	BOOL deleting, modifying, adding;
	BOOL trash_folder = FALSE;
	uni_char *uid=NULL, *puid=NULL;
	ANCHOR_ARRAY(uni_char, uid);
	ANCHOR_ARRAY(uni_char, puid);

	deleting = modifying = adding = FALSE;
    if (item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED)
		adding = TRUE;
    else if (item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_MODIFIED)
		modifying = TRUE;
    else if (item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_DELETED)
		deleting = TRUE;

#ifdef CORE_SPEED_DIAL_SUPPORT
	if (item->GetType() == OpSyncDataItem::DATAITEM_SPEEDDIAL)
	{
		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_POSITION, str));
		if (!str.HasContent())
			return OpStatus::ERR;
		int position = uni_atoi(str.CStr());
		if (position > NUM_SPEEDDIALS || position < 1)
			return OpStatus::ERR;
		position--;  // Speeddial expects position 0-NUM_SPEEDDIALS-1 not 1-NUM_SPEEDDIALS
		speed_dial = g_speed_dial_manager->GetSpeedDial(position);
		if (!speed_dial)
			return OpStatus::ERR;

		bookmark = speed_dial->GetStorage();
		if (!bookmark)
		{
			RETURN_IF_ERROR(InitSpeedDialBookmarks());
			bookmark = speed_dial->GetStorage();
		}

		// Find puid and uid for the bookmark corresponding to the
		// speeddial at the given position.
		uid = OP_NEWA(uni_char, uni_strlen(bookmark->GetUniqueId()) + 1);
		puid = OP_NEWA(uni_char, uni_strlen(m_manager->GetSpeedDialFolder()->GetUniqueId()) + 1);
		if (!uid || !puid)
			return OpStatus::ERR_NO_MEMORY;
		uni_strcpy(uid, bookmark->GetUniqueId());
		uni_strcpy(puid, m_manager->GetSpeedDialFolder()->GetUniqueId());

		ANCHOR_ARRAY_RESET(uid);
		ANCHOR_ARRAY_RESET(puid);
	}
	else
#endif // CORE_SPEED_DIAL_SUPPORT
	{
		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, str));
		uid = OP_NEWA(uni_char, str.Length() + 1);
		if (!uid)
			return OpStatus::ERR_NO_MEMORY;
		if (str.CStr())
			uni_strcpy(uid, str.CStr());
		else
			uid[0] = 0;

		ANCHOR_ARRAY_RESET(uid);

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PARENT, str));
		puid = OP_NEWA(uni_char, str.Length() + 1);
		if (!puid)
			return OpStatus::ERR_NO_MEMORY;
		if (str.CStr())
			uni_strcpy(puid, str.CStr());
		else
			puid[0] = 0;

		ANCHOR_ARRAY_RESET(puid);

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PREV, str));
		if (str.HasContent())
		{
			prev_bookmark = m_manager->FindId(str.CStr());
			if (!prev_bookmark)
				m_dirty_sync = TRUE;
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_TYPE, str));
		if (str.CompareI("trash") == 0)
			trash_folder = TRUE;
		str.Empty();

		// Check if bookmark already exist.
		bookmark = m_manager->FindId(uid);
	}

	if (bookmark)
    {
		if (deleting)
		{
#ifdef CORE_SPEED_DIAL_SUPPORT
			if (item->GetType() == OpSyncDataItem::DATAITEM_SPEEDDIAL)
			{
				speed_dial->Clear(TRUE);
				return OpStatus::OK;
			}
#endif // CORE_SPEED_DIAL_SUPPORT
			if (m_manager->IsBusy())
				return m_manager->PlaceInBusyQueue(bookmark, NULL, BookmarkOperation::OPERATION_DELETE, NULL, FALSE, TRUE);
			else
				return m_manager->DeleteBookmark(bookmark, TRUE, FALSE, TRUE);
		}

		// Bookmark already exists check if moved
		BookmarkItem *parent = bookmark->GetParentFolder();

		if ((!*puid && parent->GetUniqueId() || *puid && !parent->GetUniqueId() ||
			 *puid && parent->GetUniqueId() &&
			 uni_strcmp(puid, parent->GetUniqueId()) != 0
				)
#ifdef CORE_SPEED_DIAL_SUPPORT
			&& item->GetType() != OpSyncDataItem::DATAITEM_SPEEDDIAL
#endif // CORE_SPEED_DIAL_SUPPORT
			)
		{
			// Bookmark moved
			BookmarkItem *new_parent;
			if (*puid)
				new_parent = m_manager->FindId(puid);
			else
				new_parent = m_manager->GetRootFolder();				
			if (!new_parent && !m_manager->IsBusy() && bookmark != m_manager->GetTrashFolder())
			{
				// Bookmark moved to an unknown place. Maybe this is
				// something added later on. Put on hold.
				// Remove the bookmark until its folder is found.
				BookmarkItem *item = static_cast<BookmarkItem*>(bookmark->LastLeaf());
				while (item && item != bookmark)
				{
					BookmarkItem *tmp = static_cast<BookmarkItem*>(item->Prev());
					uni_char *puid_copy = OP_NEWA(uni_char, uni_strlen(item->GetParentFolder()->GetUniqueId()) + 1);
					if (!puid_copy)
						return OpStatus::ERR_NO_MEMORY;
					uni_strcpy(puid_copy, item->GetParentFolder()->GetUniqueId());
					item->SetParentUniqueId(puid_copy);
					m_manager->RemoveBookmark(item, FALSE);
					item->Into(&m_added);
					item = tmp;
				}
				
				m_manager->RemoveBookmark(bookmark, FALSE);
				bookmark->SetParentUniqueId(puid);
				ANCHOR_ARRAY_RELEASE(puid);
				bookmark->Into(&m_moved);
			}
			else if (new_parent)
			{
				unsigned int count = new_parent->GetCount();
				unsigned int max_count = new_parent->GetMaxCount();
				unsigned int max_count_global = m_manager->GetMaxBookmarkCountPerFolder();
				if (count == max_count || count == max_count_global)
				{
					m_dirty_sync = FALSE;
					return OpStatus::ERR_OUT_OF_RANGE;
				}

				if (m_manager->IsBusy())
					return m_manager->PlaceInBusyQueue(bookmark, new_parent, BookmarkOperation::OPERATION_MOVE, NULL, FALSE, TRUE);
				else
					RETURN_IF_ERROR(m_manager->MoveBookmark(bookmark, new_parent, FALSE, TRUE));
			}
		}

		if (!m_moved.HasLink(bookmark)
#ifdef CORE_SPEED_DIAL_SUPPORT
			&& item->GetType() != OpSyncDataItem::DATAITEM_SPEEDDIAL
#endif // CORE_SPEED_DIAL_SUPPORT
			)
		{
			// Check if bookmark has moved inside folder.
			BookmarkItem *prev_in_list = static_cast<BookmarkItem*>(bookmark->Pred());
			if (prev_in_list && !prev_bookmark)
				m_manager->MoveBookmarkAfter(bookmark, NULL, FALSE);
			else if (!prev_in_list && prev_bookmark)
				m_manager->MoveBookmarkAfter(bookmark, prev_bookmark, FALSE);
			else if (prev_in_list && prev_bookmark &&
					 uni_strcmp(prev_in_list->GetUniqueId(), prev_bookmark->GetUniqueId())
				)
				m_manager->MoveBookmarkAfter(bookmark, prev_bookmark, FALSE);
		}

		RETURN_IF_ERROR(ReadAttributes(bookmark, item));
    }
	else if (deleting)
		return OpStatus::OK; // Do nothing if a non-existing bookmark has been deleted.
    else
    {
		// Bookmark did not exist, then we create a new one.
		bookmark = OP_NEW(BookmarkItem, ());
		if (!bookmark)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<BookmarkItem> bookmark_ptr(bookmark);

		if (trash_folder == TRUE)
			bookmark->SetFolderType(FOLDER_TRASH_FOLDER);
		else if (item->GetType() == OpSyncDataItem::DATAITEM_BOOKMARK)
			bookmark->SetFolderType(FOLDER_NO_FOLDER);
		else if (item->GetType() == OpSyncDataItem::DATAITEM_BOOKMARK_FOLDER)
			bookmark->SetFolderType(FOLDER_NORMAL_FOLDER);
		else if (item->GetType() == OpSyncDataItem::DATAITEM_BOOKMARK_SEPARATOR)
			bookmark->SetFolderType(FOLDER_SEPARATOR_FOLDER);
		else
			OP_ASSERT(FALSE);

		bookmark->SetUniqueId(uid);
		ANCHOR_ARRAY_RELEASE(uid);

		RETURN_IF_ERROR(ReadAttributes(bookmark, item));

		if (*puid)
		{
			BookmarkItem *parent = m_manager->FindId(puid);
			if (!parent)
			{
				bookmark->SetParentUniqueId(puid);
				ANCHOR_ARRAY_RELEASE(puid);
				bookmark->Into(&m_added);
			}
			else
			{
				unsigned int count = parent->GetCount();
				unsigned int max_count = parent->GetMaxCount();
				unsigned int max_count_global = m_manager->GetMaxBookmarkCountPerFolder();
				if (count == max_count || count == max_count_global)
				{
					m_dirty_sync = FALSE;
					return OpStatus::ERR_OUT_OF_RANGE;
				}

				bookmark->ClearSyncFlags();
				if (bookmark->GetFolderType() == FOLDER_SEPARATOR_FOLDER)
					RETURN_IF_ERROR(m_manager->AddSeparator(bookmark, prev_bookmark, parent, FALSE));
				else
					RETURN_IF_ERROR(m_manager->AddBookmark(bookmark, parent, FALSE));
			}
		}
		else
		{
			bookmark->ClearSyncFlags();
			if (bookmark->GetFolderType() == FOLDER_SEPARATOR_FOLDER)
				RETURN_IF_ERROR(m_manager->AddSeparator(bookmark, prev_bookmark, m_manager->GetRootFolder(), FALSE));
			else
				RETURN_IF_ERROR(m_manager->AddBookmark(bookmark, m_manager->GetRootFolder(), FALSE));
		}

		if (!m_added.HasLink(bookmark))
		{
			// Check if bookmark has moved inside folder.
			BookmarkItem *prev_in_list = static_cast<BookmarkItem*>(bookmark->Pred());
			if (prev_in_list && !prev_bookmark)
				m_manager->MoveBookmarkAfter(bookmark, NULL, FALSE);
			else if (!prev_in_list && prev_bookmark)
				m_manager->MoveBookmarkAfter(bookmark, prev_bookmark, FALSE);
			else if (prev_in_list && prev_bookmark &&
					 uni_strcmp(prev_in_list->GetUniqueId(), prev_bookmark->GetUniqueId())
				)
				m_manager->MoveBookmarkAfter(bookmark, prev_bookmark, FALSE);
		}
		bookmark_ptr.release();
    }

	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_TARGET, str));
	if (str.HasContent())
	{
		BookmarkAttribute attr;
		attr.SetAttributeType(BOOKMARK_TARGET);
		RETURN_IF_ERROR(attr.SetTextValue(str));
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_TARGET, &attr));
	}	

	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_MOVE_IS_COPY, str));
	if (str.HasContent())
	{
		BOOL value = uni_atoi(str.CStr()) != 0;
		bookmark->SetMoveIsCopy(value);
	}
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_DELETABLE, str));
	if (str.HasContent())
	{
		BOOL value = uni_atoi(str.CStr()) != 0;
		bookmark->SetDeletable(value);
	}
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_SUB_FOLDERS_ALLOWED, str));
	if (str.HasContent())
	{
		BOOL value = uni_atoi(str.CStr()) != 0;
		bookmark->SetSubFoldersAllowed(value);
	}
 	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_MAX_ITEMS, str));
	if (str.HasContent())
	{
		unsigned int value = uni_atoi(str.CStr());
		if (value == 0) // 0 means unlimited
			value = ~0u;
		bookmark->SetMaxCount(value);
	}
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_SEPARATORS_ALLOWED, str));
	if (str.HasContent())
	{
		BOOL value = uni_atoi(str.CStr()) != 0;
		bookmark->SetSeparatorsAllowed(value);
	}

	if (m_added.HasLink(bookmark) || m_moved.HasLink(bookmark))
		return OpStatus::OK;

	BookmarkAttribute *attr;
	OpString fixed;
	attr = bookmark->GetAttribute(BOOKMARK_URL);
	if (attr)
	{
		RETURN_IF_ERROR(bookmark_resolve_url(attr, fixed));
		if (fixed.Compare(attr->GetTextValue()) != 0)
		{
			RETURN_IF_ERROR(attr->SetTextValue(fixed.CStr()));
			RETURN_IF_ERROR(BookmarkChanged(bookmark));
		}
	}

#ifdef CORE_SPEED_DIAL_SUPPORT
	if (item->GetType() == OpSyncDataItem::DATAITEM_SPEEDDIAL)
		speed_dial->SpeedDialSynced();
#endif // CORE_SPEED_DIAL_SUPPORT

    return OpStatus::OK;
}

static BOOL AddFromList(BookmarkItem *first, BOOL root_folder, BookmarkManager *manager)
{
	OP_NEW_DBG("AddFromList","bookmarks.synchronization");

	BOOL ret = FALSE;
	OP_STATUS res;
	BookmarkItem *bookmark = first;
	BookmarkItem *folder = NULL;
	if (root_folder)
		folder = manager->GetRootFolder();
	while (bookmark)
	{
		BookmarkItem *next = static_cast<BookmarkItem*>(bookmark->Suc());
		if (!root_folder)
			folder = manager->FindId(bookmark->GetParentUniqueId());
		if (folder)
		{
			ret = TRUE;
			bookmark->Out();
			if (manager->IsBusy())
				res = manager->PlaceInBusyQueue(bookmark, folder, BookmarkOperation::OPERATION_ADD, NULL, FALSE, TRUE);
			else
				res = manager->AddBookmark(bookmark, folder, FALSE);
			OP_DELETEA(bookmark->GetParentUniqueId());
			bookmark->SetParentUniqueId(NULL);
			if (OpStatus::IsError(res))
				OP_DELETE(bookmark);
		}
		bookmark = next;
	}

	return ret;
}

OP_STATUS BookmarkSync::CleanupItems()
{
	OP_NEW_DBG("BookmarkSync::CleanupItems","bookmarks.synchronization");

	BOOL working = TRUE;
	while (working)
	{
		working = AddFromList(static_cast<BookmarkItem*>(m_added.First()), FALSE, m_manager);
		if (AddFromList(static_cast<BookmarkItem*>(m_moved.First()), FALSE, m_manager))
			working = TRUE;
	}

	if (m_added.First() || m_moved.First())
	{
		m_dirty_sync = TRUE;

		// Add or move bookmarks still not added to the root folder.
		AddFromList(static_cast<BookmarkItem*>(m_added.First()), TRUE, m_manager);
		AddFromList(static_cast<BookmarkItem*>(m_moved.First()), TRUE, m_manager);
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkSync::ReadAttributes(BookmarkItem *bookmark, OpSyncItem *item)
{
	OP_NEW_DBG("BookmarkSync::ReadAttributes","bookmarks.synchronization");

    OpString str;
    BookmarkAttribute attr;

	// Visited,Created and url attributes should not be synchronized for folders and separators.
	if (bookmark->GetFolderType() == FOLDER_NO_FOLDER)
	{
		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_VISITED, str));
		if (str.CStr())
		{
			RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_VISITED, &attr));
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_URI, str));
		if (str.CStr())
		{
			RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_URL, &attr));
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_CREATED, str));
		if (str.CStr())
		{
			RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_CREATED, &attr));
		}
	}

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_TITLE, str));
	if (str.CStr())
	{
		RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_TITLE, &attr));
	}

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_DESCRIPTION, str));
	if (str.CStr())
	{
		RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_DESCRIPTION, &attr));
	}

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ICON, str));
	if (str.CStr())
	{
		RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_FAVICON_FILE, &attr));
	}

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_THUMBNAIL, str));
	if (str.CStr())
	{
		RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_THUMBNAIL_FILE, &attr));
	}

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_NICKNAME, str));
	if (str.CStr())
	{
		RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SHORTNAME, &attr));
	}

    RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PARTNER_ID, str));
	if (str.CStr())
	{
		RETURN_IF_ERROR(attr.SetTextValue(str.CStr()));
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_PARTNER_ID, &attr));
	}

#ifdef BOOKMARKS_PERSONAL_BAR
#ifdef SYNC_HAVE_PERSONAL_BAR
	BOOL sync_personalbar = g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncPersonalbar);
	if (sync_personalbar)
	{
		attr.SetTextValue(UNI_L(""));

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_SHOW_IN_PERSONAL_BAR, str));
		if (str.CStr())
		{
			attr.SetIntValue(uni_atoi(str.CStr()));
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SHOW_IN_PERSONAL_BAR, &attr));
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PERSONAL_BAR_POS, str));
		if (str.CStr())
		{
			attr.SetIntValue(uni_atoi(str.CStr()));
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_PERSONALBAR_POS, &attr));
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_SHOW_IN_PANEL, str));
		if (str.CStr())
		{
			attr.SetIntValue(uni_atoi(str.CStr()));
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SHOW_IN_PANEL, &attr));
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PANEL_POS, str));
		if (str.CStr())
		{
			attr.SetIntValue(uni_atoi(str.CStr()));
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_PANEL_POS, &attr));
		}
	}
#endif
#endif

#ifdef CORE_SPEED_DIAL_SUPPORT
	if (bookmark->GetParentFolder() &&  bookmark->GetParentFolder()->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
	{
		int int_val;
		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_RELOAD_ENABLED, str));
		if (str.CStr())
		{
			int_val = uni_atoi(str.CStr());
			attr.SetIntValue(int_val);
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SD_RELOAD_ENABLED, &attr));
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_RELOAD_INTERVAL, str));
		if (str.CStr())
		{
			int_val = uni_atoi(str.CStr());
			attr.SetIntValue(int_val);
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SD_RELOAD_INTERVAL, &attr));
		}

		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_RELOAD_ONLY_IF_EXPIRED, str));
		if (str.CStr())
		{
			int_val = uni_atoi(str.CStr());
			attr.SetIntValue(int_val);
			RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SD_RELOAD_EXPIRED, &attr));
		}
	}
#endif // CORE_SPEED_DIAL_SUPPORT

	bookmark->ClearSyncFlags();
    return OpStatus::OK;
}

class BookmarkSyncItemWatch
{
	OpSyncItem *m_item;
	OpSyncCoordinator *m_coordinator;
public:
	BookmarkSyncItemWatch(OpSyncItem *item, OpSyncCoordinator *coordinator) : m_item(item), m_coordinator(coordinator) { }
	~BookmarkSyncItemWatch()
	{
		if (m_item)
			m_coordinator->ReleaseSyncItem(m_item);
	}

	void Reset() { m_item = NULL; }
};

OP_STATUS BookmarkSync::BookmarkChanged(BookmarkItem *bookmark, BOOL ordered /* = TRUE */)
{
	OP_NEW_DBG("BookmarkSync::BookmarkChanged","bookmarks.synchronization");

    OpString str;
    BookmarkAttribute attr;
    OpSyncItem *item;
    OpSyncDataItem::DataItemStatus status = OpSyncDataItem::DATAITEM_ACTION_NONE;
    OpSyncDataItem::DataItemType type;

	if (bookmark == m_manager->GetRootFolder())
		return OpStatus::OK;
#ifdef CORE_SPEED_DIAL_SUPPORT
	if (bookmark == m_manager->GetSpeedDialFolder())
		return OpStatus::OK;
#endif // CORE_SPEED_DIAL_SUPPORT

    BOOL sync = bookmark->ShouldSync();
	int sync_bookmark = 1;
#ifdef SYNC_HAVE_BOOKMARKS
	sync_bookmark = g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncBookmarks);
#endif // SYNC_HAVE_BOOKMARKS
	BOOL allowed_to_sync = IsAllowedToSync(bookmark);
#ifdef CORE_SPEED_DIAL_SUPPORT
	int sync_speeddial = 1;
#ifdef SYNC_HAVE_SPEED_DIAL
	sync_speeddial = g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncSpeeddial);
#endif // SYNC_HAVE_SPEED_DIAL
	BOOL is_speeddial = FALSE;
	BookmarkItem *parent = bookmark->GetParentFolder();
	if (parent && parent->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
		is_speeddial = TRUE;

	if (is_speeddial && !(sync && sync_speeddial && allowed_to_sync))
		return OpStatus::OK;
    if (!(is_speeddial || sync && sync_bookmark && allowed_to_sync))
		return OpStatus::OK;
#else
	if (!(sync && sync_bookmark && allowed_to_sync))
		return OpStatus::OK;
#endif // CORE_SPEED_DIAL_SUPPORT

    if (bookmark->GetFolderType() == FOLDER_NO_FOLDER)
		type = OpSyncDataItem::DATAITEM_BOOKMARK;
	else if (bookmark->GetFolderType() == FOLDER_SEPARATOR_FOLDER)
		type = OpSyncDataItem::DATAITEM_BOOKMARK_SEPARATOR;
    else
		type = OpSyncDataItem::DATAITEM_BOOKMARK_FOLDER;
#ifdef CORE_SPEED_DIAL_SUPPORT
	if (is_speeddial)
		return SpeedDialChanged(bookmark);
#endif // CORE_SPEED_DIAL_SUPPORT

    if (bookmark->IsModified())
		status = OpSyncDataItem::DATAITEM_ACTION_MODIFIED;
    else if (bookmark->IsAdded())
		status = OpSyncDataItem::DATAITEM_ACTION_ADDED;
    else if (bookmark->IsDeleted())
		status = OpSyncDataItem::DATAITEM_ACTION_DELETED;

    RETURN_IF_ERROR(m_coordinator->GetSyncItem(&item, type, OpSyncItem::SYNC_KEY_ID, bookmark->GetUniqueId()));
	BookmarkSyncItemWatch item_watch(item, m_coordinator);

	if (bookmark->ShouldSyncAttribute(BOOKMARK_URL) && bookmark->GetFolderType() == FOLDER_NO_FOLDER)
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_URL, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_URI, str));
	}

	if (bookmark->ShouldSyncAttribute(BOOKMARK_TITLE))
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_TITLE, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_TITLE, str));
	}

	if (bookmark->GetFolderType() == FOLDER_TRASH_FOLDER)
	{
		RETURN_IF_ERROR(str.Set("trash"));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_TYPE, str));
	}

	// Check if any of the attributes needs change.
	if (bookmark->ShouldSyncAttribute(BOOKMARK_VISITED) && bookmark->GetFolderType() == FOLDER_NO_FOLDER)
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_VISITED, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_VISITED, str));
	}

	if (bookmark->ShouldSyncAttribute(BOOKMARK_CREATED) && bookmark->GetFolderType() == FOLDER_NO_FOLDER)
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_CREATED, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_CREATED, str));
	}

	if (bookmark->ShouldSyncAttribute(BOOKMARK_DESCRIPTION))
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_DESCRIPTION, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_DESCRIPTION, str));
	}

	if (bookmark->ShouldSyncAttribute(BOOKMARK_FAVICON_FILE))
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_FAVICON_FILE, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_ICON, str));
	}

	if (bookmark->ShouldSyncAttribute(BOOKMARK_THUMBNAIL_FILE))
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_THUMBNAIL_FILE, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_THUMBNAIL, str));
	}

	if (bookmark->ShouldSyncAttribute(BOOKMARK_SHORTNAME))
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_SHORTNAME, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_NICKNAME, str));
	}

	if (bookmark->ShouldSyncAttribute(BOOKMARK_PARTNER_ID))
	{
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_PARTNER_ID, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PARTNER_ID, str));
	}

#ifdef BOOKMARKS_PERSONAL_BAR
#ifdef SYNC_HAVE_PERSONAL_BAR
	BOOL sync_personalbar = g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncPersonalbar);
	if (sync_personalbar)
	{
		if (bookmark->ShouldSyncAttribute(BOOKMARK_PANEL_POS))
		{
			str.Empty();
			RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_PANEL_POS, &attr));
			RETURN_IF_ERROR(str.AppendFormat(UNI_L("%d"), attr.GetIntValue()));
			RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PANEL_POS, str));

			// sync SYNC_KEY_SHOW_IN_PANEL for backward compatibility
			str.Empty();
			RETURN_IF_ERROR(str.AppendFormat(UNI_L("%d"), attr.GetIntValue() >= 0 ));
			RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_SHOW_IN_PANEL, str));
		}

		if (bookmark->ShouldSyncAttribute(BOOKMARK_PERSONALBAR_POS))
		{
			OpString pos;
			RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_PERSONALBAR_POS, &attr));
			RETURN_IF_ERROR(pos.AppendFormat(UNI_L("%d"), attr.GetIntValue()));
			RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PERSONAL_BAR_POS, pos));

			// sync SYNC_KEY_SHOW_IN_PERSONAL_BAR for backward compatibility
			str.Empty();
			RETURN_IF_ERROR(str.AppendFormat(UNI_L("%d"), attr.GetIntValue() >= 0 ));
			RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_SHOW_IN_PERSONAL_BAR, str));
		}
	}
#endif
#endif

	if (bookmark->GetParentFolder()->GetUniqueId())
	{
		RETURN_IF_ERROR(str.Set(bookmark->GetParentFolder()->GetUniqueId()));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PARENT, str));
	}
	else
	{
		RETURN_IF_ERROR(str.Set(""));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PARENT, str));
	}

	BookmarkItem *prev = static_cast<BookmarkItem*>(bookmark->Pred());
	if (prev)
	{
#ifdef CORE_SPEED_DIAL_SUPPORT
		// Exclude the speeddial folder when synchronizing.
		if (prev->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
		{
			prev = static_cast<BookmarkItem*>(prev->Pred());
			if (prev)
				RETURN_IF_ERROR(str.Set(prev->GetUniqueId()));
			else
				str.Set("");
		}
		else
#endif // CORE_SPEED_DIAL_SUPPORT
			RETURN_IF_ERROR(str.Set(prev->GetUniqueId()));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PREV, str));
	}
	else
	{
		str.Set("");
		item->SetData(OpSyncItem::SYNC_KEY_PREV, str);
	}

	item->SetStatus(status);
	if (m_waiting_dirty_sync)
		RETURN_IF_ERROR(item->CommitItem(TRUE, FALSE));
	else
		RETURN_IF_ERROR(item->CommitItem(FALSE, ordered));
	item_watch.Reset();
	m_coordinator->ReleaseSyncItem(item);
	bookmark->ClearSyncFlags();

	return OpStatus::OK;
}

#ifdef CORE_SPEED_DIAL_SUPPORT
OP_STATUS BookmarkSync::SpeedDialChanged(BookmarkItem *speed_dial)
{
	OP_NEW_DBG("BookmarkSync::SpeedDialChanged","bookmarks.synchronization");

	OpString str;
	uni_char uid[37]; /* ARRAY OK 2008-06-17 adame */
    BookmarkAttribute attr;
    OpSyncItem *item;
    OpSyncDataItem::DataItemStatus status;
    OpSyncDataItem::DataItemType type;

	type = OpSyncDataItem::DATAITEM_SPEEDDIAL;

	status = OpSyncDataItem::DATAITEM_ACTION_NONE;
    if (speed_dial->IsModified())
		status = OpSyncDataItem::DATAITEM_ACTION_MODIFIED;
    else if (speed_dial->IsAdded())
		status = OpSyncDataItem::DATAITEM_ACTION_ADDED;
    else if (speed_dial->IsDeleted())
		status = OpSyncDataItem::DATAITEM_ACTION_DELETED;

	int position = g_speed_dial_manager->GetSpeedDialPosition(speed_dial);
	if (position < 0)
		return OpStatus::ERR;
	position++; // Sync expects position 1-NUM_SPEEDDIALS not 0-NUM_SPEEDDIALS-1

	uni_snprintf(uid, 37, UNI_L("%d"), position);
	RETURN_IF_ERROR(m_coordinator->GetSyncItem(&item, type, OpSyncItem::SYNC_KEY_POSITION, uid));
	BookmarkSyncItemWatch item_watch(item, m_coordinator);

	if (speed_dial->ShouldSyncAttribute(BOOKMARK_URL))
	{
		RETURN_IF_ERROR(speed_dial->GetAttribute(BOOKMARK_URL, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_URI, str));
	}

	if (speed_dial->ShouldSyncAttribute(BOOKMARK_TITLE))
	{
		RETURN_IF_ERROR(speed_dial->GetAttribute(BOOKMARK_TITLE, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(str));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_TITLE, str));
	}

	int int_val;
	if (speed_dial->ShouldSyncAttribute(BOOKMARK_SD_RELOAD_ENABLED))
	{
		str.Empty();
		RETURN_IF_ERROR(speed_dial->GetAttribute(BOOKMARK_SD_RELOAD_ENABLED, &attr));
		int_val = attr.GetIntValue();
		RETURN_IF_ERROR(str.AppendFormat(UNI_L("%d"), int_val));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_RELOAD_ENABLED, str));
	}

	if (speed_dial->ShouldSyncAttribute(BOOKMARK_SD_RELOAD_INTERVAL))
	{
		str.Empty();
		RETURN_IF_ERROR(speed_dial->GetAttribute(BOOKMARK_SD_RELOAD_INTERVAL, &attr));
		int_val = attr.GetIntValue();
		RETURN_IF_ERROR(str.AppendFormat(UNI_L("%d"), int_val));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_RELOAD_INTERVAL, str));
	}

	if (speed_dial->ShouldSyncAttribute(BOOKMARK_SD_RELOAD_EXPIRED))
	{
		str.Empty();
		RETURN_IF_ERROR(speed_dial->GetAttribute(BOOKMARK_SD_RELOAD_EXPIRED, &attr));
		int_val = attr.GetIntValue();
		RETURN_IF_ERROR(str.AppendFormat(UNI_L("%d"), int_val));
		RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_RELOAD_ONLY_IF_EXPIRED, str));
	}

	item->SetStatus(status);
	if (m_waiting_dirty_sync)
		RETURN_IF_ERROR(item->CommitItem(TRUE, FALSE));
	else
		RETURN_IF_ERROR(item->CommitItem());
	item_watch.Reset();
	m_coordinator->ReleaseSyncItem(item);
	return OpStatus::OK;
}
#endif // CORE_SPEED_DIAL_SUPPORT

void BookmarkSync::DoneSyncing()
{
	OP_NEW_DBG("BookmarkSync::DoneSyncing","bookmarks.synchronization");

	m_manager->DoneSyncing();
	m_dirty_sync = FALSE;
}

BOOL BookmarkSync::IsAllowedToSync(BookmarkItem *bookmark)
{
	OP_NEW_DBG("BookmarkSync::IsAllowedToSync","bookmarks.synchronization");

	if (!bookmark->IsAllowedToSync())
		return FALSE;

	BookmarkItem *parent = bookmark->GetParentFolder();
	for (; parent; parent = parent->GetParentFolder())
	{
		if (!parent->IsAllowedToSync())
			return FALSE;
	}

	return TRUE;
}

BOOL BookmarkSync::DirtySync() const
{
	OP_NEW_DBG("BookmarkSync::DirtySync","bookmarks.synchronization");

	return m_dirty_sync;
}

void BookmarkSync::WaitingForDirtySync(BOOL val)
{
	OP_NEW_DBG("BookmarkSync::WaitingForDirtySync","bookmarks.synchronization");

	m_waiting_dirty_sync = val;
}

OP_STATUS BookmarkSync::SetFlag(BookmarkItem *bookmark, BookmarkAttributeType attr_type)
{
	OP_NEW_DBG("BookmarkSync::SetFlag","bookmarks.synchronization");

    OpString str;
    BookmarkAttribute attr;

    RETURN_IF_ERROR(bookmark->GetAttribute(attr_type, &attr));
    RETURN_IF_ERROR(attr.GetTextValue(str));

	if (str.HasContent())
		bookmark->SetSyncAttribute(attr_type, TRUE);
	return OpStatus::OK;
}

OP_STATUS BookmarkSync::ResyncAllBookmarks()
{
	OP_NEW_DBG("BookmarkSync::ResyncAllBookmarks","bookmarks.synchronization");

	BookmarkItem *bookmark = static_cast<BookmarkItem*>(m_manager->GetRootFolder()->First());
#ifdef CORE_SPEED_DIAL_SUPPORT
	BookmarkItem *speeddial_root = m_manager->GetSpeedDialFolder();
#endif // CORE_SPEED_DIAL_SUPPORT
	while (bookmark)
	{
#ifdef CORE_SPEED_DIAL_SUPPORT
		if (bookmark == speeddial_root)
		{
			bookmark = static_cast<BookmarkItem*>(bookmark->Suc());
		}
		else
#endif // CORE_SPEED_DIAL_SUPPORT
		{
#ifdef _DEBUG
			{
				BookmarkAttribute* attr = bookmark->GetAttribute(BOOKMARK_TITLE);
				if (attr && attr->GetTextValue())
					OP_DBG((UNI_L("bookmark: %p; title: '%s'"), bookmark, attr->GetTextValue()));
				attr = bookmark->GetAttribute(BOOKMARK_URL);
				if (attr  && attr->GetTextValue())
					OP_DBG((UNI_L("bookmark: %p; url: '%s'"), bookmark, attr->GetTextValue()));
			}
#endif // _DEBUG

			bookmark->ClearSyncFlags();
			bookmark->SetAdded(TRUE);

			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_URL));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_TITLE));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_DESCRIPTION));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_VISITED));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_CREATED));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_FAVICON_FILE));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_THUMBNAIL_FILE));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_SHORTNAME));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_PARTNER_ID));
#ifdef BOOKMARKS_PERSONAL_BAR
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_PANEL_POS));
			RETURN_IF_ERROR(SetFlag(bookmark, BOOKMARK_PERSONALBAR_POS));
#endif
			bookmark->SetSyncParent(TRUE);

			RETURN_IF_ERROR(BookmarkChanged(bookmark, FALSE));
			bookmark = static_cast<BookmarkItem*>(bookmark->Next());
		}
	}

	m_dirty_sync = FALSE;
	return OpStatus::OK;
}

#ifdef CORE_SPEED_DIAL_SUPPORT
OP_STATUS BookmarkSync::ResyncAllSpeeddials()
{
	OP_NEW_DBG("BookmarkSync::ResyncAllSpeeddials","bookmarks.synchronization");

	BookmarkItem *root = m_manager->GetSpeedDialFolder();
	if (!root)
		return OpStatus::ERR;
	BookmarkItem *speeddial = static_cast<BookmarkItem*>(root->LastLeaf());
	while (speeddial != root)
	{
		BookmarkAttribute *attr = speeddial->GetAttribute(BOOKMARK_URL);
		if (!speeddial->ShouldSync() && attr && attr->GetTextValue())
		{
			OP_DBG((UNI_L("speeddial: %p; url: '%s'"), speeddial, speeddial->GetAttribute(BOOKMARK_URL)->GetTextValue()));
			speeddial->ClearSyncFlags();
			speeddial->SetAdded(TRUE);

			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_URL));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_TITLE));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_DESCRIPTION));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_VISITED));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_CREATED));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_FAVICON_FILE));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_THUMBNAIL_FILE));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_SHORTNAME));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_PARTNER_ID));
#ifdef BOOKMARKS_PERSONAL_BAR
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_PANEL_POS));
			RETURN_IF_ERROR(SetFlag(speeddial, BOOKMARK_PERSONALBAR_POS));	
#endif
			speeddial->SetSyncParent(TRUE);

			speeddial->SetSyncAttribute(BOOKMARK_SD_RELOAD_ENABLED, TRUE);
			speeddial->SetSyncAttribute(BOOKMARK_SD_RELOAD_INTERVAL, TRUE);
			speeddial->SetSyncAttribute(BOOKMARK_SD_RELOAD_EXPIRED, TRUE);
		}

		RETURN_IF_ERROR(BookmarkChanged(speeddial));
		speeddial = static_cast<BookmarkItem*>(speeddial->Prev());
	}

	m_dirty_sync = FALSE;
	return OpStatus::OK;
}

OP_STATUS BookmarkSync::InitSpeedDialBookmarks()
{
	OP_NEW_DBG("BookmarkSync::InitSpeedDialBookmarks","bookmarks.synchronization");

	OP_STATUS ret;
	BookmarkItem *folder = m_manager->GetSpeedDialFolder();
	if (!folder)
	{
		folder = OP_NEW(BookmarkItem, ());
		if (!folder)
			return OpStatus::ERR_NO_MEMORY;

		folder->SetFolderType(FOLDER_SPEED_DIAL_FOLDER);
		ret = m_manager->AddBookmark(folder, m_manager->GetRootFolder());
		if (OpStatus::IsError(ret))
		{
			delete folder;
			return ret;
		}

		BookmarkAttribute attr;
		attr.SetAttributeType(BOOKMARK_TITLE);
		RETURN_IF_ERROR(attr.SetTextValue(UNI_L("Speed dial folder")));	// Need localized string here?
		RETURN_IF_ERROR(folder->SetAttribute(BOOKMARK_TITLE, &attr));
	}

	int i = 0;
	BookmarkItem *bookmark;
	SpeedDial *speed_dial;
	for (; i<NUM_SPEEDDIALS; i++)
	{
		speed_dial = g_speed_dial_manager->GetSpeedDial(i);
		bookmark = speed_dial->GetStorage();

		if (!bookmark)
		{
			bookmark = OP_NEW(BookmarkItem, ());
			if (!bookmark)
				return OpStatus::ERR_NO_MEMORY;

			ret = m_manager->AddBookmark(bookmark, m_manager->GetSpeedDialFolder());
			if (OpStatus::IsError(ret))
			{
				delete bookmark;
				return ret;
			}
			speed_dial->SetBookmarkStorage(bookmark);
		}
	}

	return OpStatus::OK;
}
#endif // CORE_SPEED_DIAL_SUPPORT

OP_STATUS BookmarkSync::CheckFolderSync(BookmarkItem *folder, BOOL sync)
{
	OP_NEW_DBG("BookmarkSync::CheckFolderSync","bookmarks.synchronization");

	OpSyncItem *item;
	RETURN_IF_ERROR(m_coordinator->GetExistingSyncItem(&item, OpSyncDataItem::DATAITEM_BOOKMARK, OpSyncItem::SYNC_KEY_ID, folder->GetUniqueId()));
	if (!(item && item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED))
	{
		m_coordinator->ReleaseSyncItem(item);
		return OpStatus::OK;
	}
	m_coordinator->ReleaseSyncItem(item);

	BookmarkItem *new_folder = folder;
	BookmarkItem *bookmark = static_cast<BookmarkItem*>(new_folder->First());
	while (new_folder)
	{
		for (; bookmark; bookmark = static_cast<BookmarkItem*>(bookmark->Suc()))
		{
			if (bookmark->GetFolderType() != FOLDER_NO_FOLDER)
			{
				RETURN_IF_ERROR(m_coordinator->GetExistingSyncItem(&item, OpSyncDataItem::DATAITEM_BOOKMARK, OpSyncItem::SYNC_KEY_ID, bookmark->GetUniqueId()));
				if (item && item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED)
				{
					m_coordinator->ReleaseSyncItem(item);
					new_folder = bookmark;

					bookmark->ClearSyncFlags();
					bookmark->SetDeleted(TRUE);
					if (sync)
						BookmarkChanged(bookmark);
					bookmark->ClearSyncFlags();
					bookmark = static_cast<BookmarkItem*>(new_folder->First());
					break;
				}
				m_coordinator->ReleaseSyncItem(item);
			}

			bookmark->ClearSyncFlags();
			bookmark->SetDeleted(TRUE);
			if (sync)
				BookmarkChanged(bookmark);
			bookmark->ClearSyncFlags();
		}

		if (!bookmark)
		{
			BookmarkItem *parent = new_folder;
			for (; parent && parent != folder; parent = parent->GetParentFolder())
				if (parent->Suc())
					break;
			if (parent && parent != folder)
			{
				bookmark = static_cast<BookmarkItem*>(parent->Suc());
				new_folder = bookmark->GetParentFolder();
			}
			else
				new_folder = NULL;
		}
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkSync::CheckPreviousSync(BookmarkItem *bookmark)
{
	OP_NEW_DBG("BookmarkSync::CheckPreviousSync","bookmarks.synchronization");

	BookmarkItem *next = static_cast<BookmarkItem*>(bookmark->Suc());
	if (next)
	{
		OpSyncItem *item;
		RETURN_IF_ERROR(m_coordinator->GetExistingSyncItem(&item, OpSyncDataItem::DATAITEM_BOOKMARK, OpSyncItem::SYNC_KEY_ID, next->GetUniqueId()));
		if (item)
		{
			OpString str;
			BookmarkItem *prev = static_cast<BookmarkItem*>(bookmark->Pred());
			if (prev)
			{
#ifdef CORE_SPEED_DIAL_SUPPORT
				// Exclude the speeddial folder when synchronizing.
				if (prev->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
				{
					prev = static_cast<BookmarkItem*>(prev->Pred());
					if (prev)
						str.Set(prev->GetUniqueId());
				}
				else
#endif // CORE_SPEED_DIAL_SUPPORT
					str.Set(prev->GetUniqueId());
			}
			item->SetData(OpSyncItem::SYNC_KEY_PREV, str);
			m_coordinator->ReleaseSyncItem(item);
		}
	}

	return OpStatus::OK;
}

#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT
