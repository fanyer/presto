/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/util/opguid.h"
#include "modules/util/tree.h"
#include "modules/util/simset.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpLocale.h"

#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/bookmark_storage_provider.h"
#ifdef SUPPORT_DATA_SYNC
#include "modules/bookmarks/bookmark_sync.h"
#endif // SUPPORT_DATA_SYNC
#ifdef OPERABOOKMARKS_URL
#include "modules/bookmarks/operabookmarks.h"
#endif // OPERABOOKMARKS_URL
#ifdef CORE_SPEED_DIAL_SUPPORT
#include "modules/bookmarks/speeddial_manager.h"
#endif // CORE_SPEED_DIAL_SUPPORT
#include "modules/bookmarks/bookmark_operation.h"

BookmarkManager::BookmarkManager() :
	m_root_folder(NULL),
	m_trash_folder(NULL),
#ifdef CORE_SPEED_DIAL_SUPPORT
	m_speed_dial_folder(NULL),
#endif // CORE_SPEED_DIAL_SUPPORT
	m_max_folder_depth(0),
	m_max_count_per_folder(0),
	m_max_count(0),
	m_count(0),
	m_operation_count(0),
	m_loaded_count(0),
	m_saved_count(0),
	m_save_policy(NO_AUTO_SAVE),
	m_save_delay(BOOKMARKS_DEFAULT_SAVE_TIMEOUT),
	m_first_change_after_save(TRUE),
	m_storage_provider(NULL),
	m_format(BOOKMARK_VERBOSE),
	m_current_bookmark(NULL),
	m_load_in_progress(FALSE),
	m_save_in_progress(FALSE),
	m_append_load(FALSE)
#ifdef SUPPORT_DATA_SYNC
	,m_sync(NULL)
#endif // SUPPORT_DATA_SYNC
#ifdef OPERABOOKMARKS_URL
	,m_opera_bookmarks_listener(NULL)
#endif // OPERABOOKMARKS_URL
{
	m_bookmark_url.SetAttributeType(BOOKMARK_URL);
	m_bookmark_title.SetAttributeType(BOOKMARK_TITLE);
	m_bookmark_desc.SetAttributeType(BOOKMARK_DESCRIPTION);
	m_bookmark_sn.SetAttributeType(BOOKMARK_SHORTNAME);
	m_bookmark_fav.SetAttributeType(BOOKMARK_FAVICON_FILE);
	m_bookmark_tf.SetAttributeType(BOOKMARK_THUMBNAIL_FILE);
	m_bookmark_created.SetAttributeType(BOOKMARK_CREATED);
	m_bookmark_visited.SetAttributeType(BOOKMARK_VISITED);

    // These should perhaps be prefs?
#ifdef PREFS_HAVE_BOOKMARK
	m_save_policy = static_cast<enum BookmarkSaveTimerType>(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksSavePolicy));
	m_save_delay = g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksSaveTimeout);
	m_max_count = g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksMaxCount);
	m_max_folder_depth = g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksMaxFolderDepth);
	m_max_count_per_folder = g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksMaxCountPerFolder);

	m_bookmark_url.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksUrlMaxLength));
	m_bookmark_title.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksTitleMaxLength));
	m_bookmark_desc.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksDescMaxLength));
	m_bookmark_sn.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksSnMaxLength));
	m_bookmark_fav.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksFaviconFileMaxLength));
	m_bookmark_tf.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksThumbnailFileMaxLength));
	m_bookmark_created.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksCreatedMaxLength));
	m_bookmark_visited.SetMaxLength(g_pccore->GetIntegerPref(PrefsCollectionCore::BookmarksVisitedMaxLength));
#else
	m_bookmark_url.SetMaxLength(128);
	m_bookmark_title.SetMaxLength(128);
	m_bookmark_desc.SetMaxLength(128);
	m_bookmark_sn.SetMaxLength(128);
	m_bookmark_fav.SetMaxLength(128);
	m_bookmark_tf.SetMaxLength(128);
	m_bookmark_created.SetMaxLength(32);
	m_bookmark_visited.SetMaxLength(32);

	m_max_count = 1000000;
	m_max_folder_depth = 1000000;
	m_max_count_per_folder = 1000000;
#endif // PREFS_HAVE_BOOKMARK

	m_bookmark_url.SetMaxLengthAction(ACTION_CUT);
	m_bookmark_title.SetMaxLengthAction(ACTION_CUT);
	m_bookmark_desc.SetMaxLengthAction(ACTION_CUT);
	m_bookmark_sn.SetMaxLengthAction(ACTION_CUT);
	m_bookmark_fav.SetMaxLengthAction(ACTION_CUT);
	m_bookmark_tf.SetMaxLengthAction(ACTION_CUT);
	m_bookmark_created.SetMaxLengthAction(ACTION_CUT);
	m_bookmark_visited.SetMaxLengthAction(ACTION_CUT);
}

BookmarkManager::~BookmarkManager()
{
	m_save_timer.Stop();

	RemoveAllBookmarks();
#ifdef OPERABOOKMARKS_URL
	OP_DELETE(m_opera_bookmarks_listener);
#endif // OPERABOOKMARKS_URL
}

OP_STATUS BookmarkManager::Init()
{
	m_save_timer.SetTimerListener(this);

	m_root_folder = OP_NEW(BookmarkItem, ());

	if (!m_root_folder)
		return OpStatus::ERR_NO_MEMORY;

	m_root_folder->SetFolderType(FOLDER_NORMAL_FOLDER);

	return m_root_folder->SetParentFolder(NULL);
}

OP_STATUS BookmarkManager::Destroy()
{
// Check if any save was in progress or scheduled to begin??
	
	// it's possible that a loading/saving operation is in progress. Stop that or we crash.
	// or should we finish the operations now?
	CancelPendingOperation();

	return OpStatus::OK;
}

OP_STATUS BookmarkManager::SetStorageProvider(BookmarkStorageProvider *storage_provider)
{
	if (m_save_in_progress || m_load_in_progress)
		return OpStatus::ERR;

	m_storage_provider = storage_provider;
	return OpStatus::OK;
}

BookmarkStorageProvider* BookmarkManager::GetStorageProvider()
{
	return m_storage_provider;
}

void BookmarkManager::SetStorageFormat(BookmarkFormatType format)
{
	m_format = format;
}

BookmarkFormatType BookmarkManager::GetStorageFormat()
{
	return m_format;
}

UINT32 BookmarkManager::GetMaxBookmarkCount()
{
	return m_max_count;
}

UINT32 BookmarkManager::GetMaxBookmarkCountPerFolder()
{
	return m_max_count_per_folder;
}

UINT32 BookmarkManager::GetMaxBookmarkFolderDepth()
{
	return m_max_folder_depth;
}

void BookmarkManager::SetMaxBookmarkCount(UINT32 max_count)
{
	m_max_count = max_count;
}

void BookmarkManager::SetMaxBookmarkCountPerFolder(UINT32 max_count_per_folder)
{
	m_max_count_per_folder = max_count_per_folder;
}

void BookmarkManager::SetMaxBookmarkFolderDepth(UINT32 max_folder_depth)
{
	m_max_folder_depth = max_folder_depth;
}

#ifdef SUPPORT_DATA_SYNC
void BookmarkManager::SetBookmarkSync(BookmarkSync *bookmark_sync)
{
	m_sync = bookmark_sync;
}

void BookmarkManager::NotifyBookmarkChanged(BookmarkItem* bookmark)
{
	m_sync->BookmarkChanged(bookmark);
}
#endif // SUPPORT_DATA_SYNC

OP_STATUS BookmarkManager::AddNewBookmark(BookmarkItem *bookmark, BookmarkItem *folder
#ifdef SUPPORT_DATA_SYNC
										  , BOOL sync
#endif // SUPPORT_DATA_SYNC
	)
{
#ifdef SUPPORT_DATA_SYNC
	bookmark->SetAdded(TRUE);
	bookmark->SetModified(FALSE);
#endif // SUPPORT_DATA_SYNC
#ifdef SUPPORT_DATA_SYNC
	RETURN_IF_ERROR(AddBookmark(bookmark, folder, sync));
#else
	RETURN_IF_ERROR(AddBookmark(bookmark, folder));
#endif // SUPPORT_DATA_SYNC
	return OpStatus::OK;
}

OP_STATUS BookmarkManager::CheckLimit(BookmarkAttribute *reference, BookmarkItem *test)
{
	if (reference->ViolatingMaxLength(test))
	{
		if (reference->GetMaxLengthAction() == ACTION_FAIL)
			return OpStatus::ERR_OUT_OF_RANGE;
		else if (reference->GetMaxLengthAction() == ACTION_CUT)
			test->Cut(reference->GetAttributeType(), reference->GetMaxLength());
	}
	return OpStatus::OK;
}

OP_STATUS BookmarkManager::AddBookmark(BookmarkItem *bookmark, BookmarkItem *folder
#ifdef SUPPORT_DATA_SYNC
									   , BOOL sync
#endif // SUPPORT_DATA_SYNC
									   , BOOL signal
	)
{
	uni_char *uid;

	if (!bookmark || !folder)
		return OpStatus::ERR_NULL_POINTER;

	if (bookmark == folder)
		return OpStatus::ERR_OUT_OF_RANGE;

#ifdef CORE_SPEED_DIAL_SUPPORT
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::EnableSpeedDial) && bookmark->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
		return OpStatus::ERR;

	unsigned max_count=0, compensate_speeddial = 0;
	if (GetSpeedDialFolder())
		max_count = NUM_SPEEDDIALS + 1; // Number of speeddials and one speeddial folder.
	else if (bookmark->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
		max_count = 1;

	if (folder == GetRootFolder())
		compensate_speeddial = 1;
	else if (folder == GetSpeedDialFolder() && m_max_count_per_folder < NUM_SPEEDDIALS)
		compensate_speeddial = NUM_SPEEDDIALS;

	if (m_count == m_max_count+max_count)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (folder->GetCount() == m_max_count_per_folder + compensate_speeddial || folder->GetCount() == folder->GetMaxCount())
#else
	if (m_count == m_max_count)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (folder->GetCount() == m_max_count_per_folder || folder->GetCount() == folder->GetMaxCount())
#endif // CORE_SPEED_DIAL_SUPPORT
		return OpStatus::ERR_OUT_OF_RANGE;
	if (folder->GetFolderDepth() == m_max_folder_depth)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (bookmark->GetFolderType() != FOLDER_NO_FOLDER && bookmark->GetFolderType() != FOLDER_SEPARATOR_FOLDER && !folder->SubFoldersAllowed())
		return OpStatus::ERR_OUT_OF_RANGE;
	if (bookmark->GetUniqueId() && FindId(bookmark->GetUniqueId()))
		return OpStatus::ERR_OUT_OF_RANGE;

#ifdef SUPPORT_DATA_SYNC
	BOOL sync_added = bookmark->IsAdded();
	BOOL sync_modified = bookmark->IsModified();
#endif // SUPPORT_DATA_SYNC

	BookmarkAttribute attr;
	OpString value;
	RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_CREATED, &attr));
	RETURN_IF_ERROR(attr.GetTextValue(value));
	if (!value.HasContent())
		RETURN_IF_ERROR(GenerateTime(bookmark, TRUE));

	RETURN_IF_ERROR(CheckLimit(&m_bookmark_url, bookmark));
	RETURN_IF_ERROR(CheckLimit(&m_bookmark_title, bookmark));
	RETURN_IF_ERROR(CheckLimit(&m_bookmark_desc, bookmark));
	RETURN_IF_ERROR(CheckLimit(&m_bookmark_sn, bookmark));
	RETURN_IF_ERROR(CheckLimit(&m_bookmark_fav, bookmark));
	RETURN_IF_ERROR(CheckLimit(&m_bookmark_tf, bookmark));
	RETURN_IF_ERROR(CheckLimit(&m_bookmark_created, bookmark));
	RETURN_IF_ERROR(CheckLimit(&m_bookmark_visited, bookmark));

	uid = bookmark->GetUniqueId();
	if (!uid)
	{
		RETURN_IF_ERROR(GenerateUniqueId(&uid));
		bookmark->SetUniqueId(uid);
	}

	RETURN_IF_ERROR(bookmark->SetParentFolder(folder));
#ifdef BOOKMARKS_UID_INDEX
	RETURN_IF_ERROR(m_uid_index.Add(bookmark->GetUniqueId(), bookmark));
#endif // BOOKMARKS_UID_INDEX
	m_count++;

#ifdef SUPPORT_DATA_SYNC
#ifdef CORE_SPEED_DIAL_SUPPORT
	if (bookmark->GetFolderType() != FOLDER_SPEED_DIAL_FOLDER && bookmark->GetParentFolder()->GetFolderType() != FOLDER_SPEED_DIAL_FOLDER)
#endif // CORE_SPEED_DIAL_SUPPORT
	{
		if (sync_added && !sync_modified)
			bookmark->SetModified(FALSE);

		if (sync && !m_load_in_progress)
		{
			// No need to sync old unchanged bookmarks if they already
			// have been synced.
			m_sync->BookmarkChanged(bookmark);
		}
	}
	bookmark->ClearSyncFlags();
#endif // SUPPORT_DATA_SYNC

	// Trashfolder previously undefined set the main trash folder to
	// this folder.
	if (bookmark->GetFolderType() == FOLDER_TRASH_FOLDER)
		NewTrashFolder(bookmark);
#ifdef CORE_SPEED_DIAL_SUPPORT
	if (bookmark->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER && !m_speed_dial_folder)
		m_speed_dial_folder = bookmark;
#endif // CORE_SPEED_DIAL_SUPPORT

	if (signal)
		SignalBookmarkAdded(bookmark);
	return SetSaveTimer();
}

OP_STATUS BookmarkManager::AddBookmark(BookmarkItem *bookmark, BookmarkItem *previous, BookmarkItem *folder
#ifdef SUPPORT_DATA_SYNC
									   , BOOL sync
#endif // SUPPORT_DATA_SYNC
	)
{
 	if (previous && previous->GetParentFolder() != folder)
		return OpStatus::ERR_OUT_OF_RANGE;
#ifdef SUPPORT_DATA_SYNC
	RETURN_IF_ERROR(AddBookmark(bookmark, folder, sync, FALSE));
	RETURN_IF_ERROR(MoveBookmarkAfter(bookmark, previous, sync, FALSE));
#else
	RETURN_IF_ERROR(AddBookmark(bookmark, folder, FALSE));
	RETURN_IF_ERROR(MoveBookmarkAfter(bookmark, previous, FALSE));
#endif // SUPPORT_DATA_SYNC

	SignalBookmarkAdded(bookmark);
	return OpStatus::OK;
}

OP_STATUS BookmarkManager::AddSeparator(BookmarkItem *separator, BookmarkItem *previous, BookmarkItem *folder
#ifdef SUPPORT_DATA_SYNC
										, BOOL sync
#endif // SUPPORT_DATA_SYNC
	)
{
	if (!folder->SeparatorsAllowed())
		return OpStatus::ERR_OUT_OF_RANGE;

	separator->SetFolderType(FOLDER_SEPARATOR_FOLDER);
	separator->SetMaxCount(0);
	separator->ClearAttributes();
#ifdef SUPPORT_DATA_SYNC
	return AddBookmark(separator, previous, folder, sync);
#else
	return AddBookmark(separator, previous, folder);
#endif // SUPPORT_DATA_SYNC
}

OP_STATUS BookmarkManager::RemoveFromTree(BookmarkItem *rem, BOOL with_delete, BOOL only_bookmark, BOOL signal_removal /* = TRUE */)
{
	BookmarkItem *tmp, *parent_folder;

	if (!rem)
		return OpStatus::OK;

	if (rem->GetFolderType() == FOLDER_NO_FOLDER || only_bookmark)
	{
		UINT32 count = rem->GetCount()+1;
		for (parent_folder = rem->GetParentFolder(); parent_folder; parent_folder = parent_folder->GetParentFolder())
			parent_folder->SetCount(parent_folder->GetCount()-count);

		rem->Out();
		OnBookmarkRemoved(rem, signal_removal);
		m_count--;
#ifdef BOOKMARKS_UID_INDEX
		RETURN_IF_ERROR(m_uid_index.Remove(rem->GetUniqueId(), &rem));
#endif // BOOKMARKS_UID_INDEX
		if (with_delete)
			OP_DELETE(rem);

		return OpStatus::OK;
	}

	UINT32 count = rem->GetCount()+1;
	rem->SetCount(0);
	for (parent_folder = rem->GetParentFolder(); parent_folder; parent_folder = parent_folder->GetParentFolder())
		parent_folder->SetCount(parent_folder->GetCount()-count);

	Head list;
	BookmarkItem *bookmark = static_cast<BookmarkItem*>(rem->LastLeaf());
	while (bookmark != rem)
	{
		tmp = static_cast<BookmarkItem*>(bookmark->Prev());
		bookmark->Out();
		OnBookmarkRemoved(bookmark, signal_removal);
		m_count--;
#ifdef BOOKMARKS_UID_INDEX
		RETURN_IF_ERROR(m_uid_index.Remove(bookmark->GetUniqueId(), &bookmark));
#endif // BOOKMARKS_UID_INDEX
		if (with_delete)
			bookmark->Into(&list);
		bookmark = tmp;
	}

	bookmark->Out();
	OnBookmarkRemoved(bookmark, signal_removal);
	m_count--;
#ifdef BOOKMARKS_UID_INDEX
	if (bookmark != GetRootFolder())
		RETURN_IF_ERROR(m_uid_index.Remove(bookmark->GetUniqueId(), &bookmark));
#endif // BOOKMARKS_UID_INDEX
	if (with_delete)
	{
		bookmark->Into(&list);
		list.Clear();
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkManager::RemoveBookmark(BookmarkItem *bookmark
#ifdef SUPPORT_DATA_SYNC
										  , BOOL sync
#endif // SUPPORT_DATA_SYNC
	)
{
	if (!bookmark)
		return OpStatus::ERR_NULL_POINTER;

	if (m_save_in_progress || m_load_in_progress)
		return OpStatus::ERR;

#ifdef SUPPORT_DATA_SYNC
	if (bookmark->GetFolderType() != FOLDER_NO_FOLDER)
		m_sync->CheckFolderSync(bookmark, sync);

	bookmark->ClearSyncFlags();
	bookmark->SetDeleted(TRUE);
	if (sync)
	{
		m_sync->CheckPreviousSync(bookmark);
		m_sync->BookmarkChanged(bookmark);
	}
	bookmark->ClearSyncFlags();
#endif // SUPPORT_DATA_SYNC
	RETURN_IF_ERROR(RemoveFromTree(bookmark, FALSE, FALSE));

	return SetSaveTimer();
}

OP_STATUS BookmarkManager::DeleteBookmark(BookmarkItem *bookmark, BOOL real_delete
#ifdef SUPPORT_DATA_SYNC
										  , BOOL sync
										  , BOOL from_sync
#endif // SUPPORT_DATA_SYNC
	)
{
#ifdef SUPPORT_DATA_SYNC
	// changes initiated from sync shouldn't be synced again
	OP_ASSERT(!(sync && from_sync));
#endif // SUPPORT_DATA_SYNC

	if (!bookmark)
		return OpStatus::ERR_NULL_POINTER;

	if (m_save_in_progress || m_load_in_progress)
		return OpStatus::ERR;

	if (!bookmark->Deletable()
#ifdef SUPPORT_DATA_SYNC
		// Delete trash folder if request is from sync and there are more than one of them
		&& !(from_sync && m_trash_folders.GetCount() > 1)
#endif // SUPPORT_DATA_SYNC
	)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (IsInTrashFolder(bookmark))
		real_delete = TRUE;

	if (real_delete || !m_trash_folder 
		// Avoid moving a trash into another trash, just delete it
		|| bookmark->GetFolderType() == FOLDER_TRASH_FOLDER)
	{
#ifdef SUPPORT_DATA_SYNC
		if (bookmark->GetFolderType() != FOLDER_NO_FOLDER)
			m_sync->CheckFolderSync(bookmark, sync);

		bookmark->ClearSyncFlags();
		bookmark->SetDeleted(TRUE);
		if (sync)
		{
			m_sync->CheckPreviousSync(bookmark);
			m_sync->BookmarkChanged(bookmark);
		}
		bookmark->ClearSyncFlags();
#endif // SUPPORT_DATA_SYNC
		RETURN_IF_ERROR(RemoveFromTree(bookmark, TRUE, FALSE));
	}
	else
	{
#ifdef SUPPORT_DATA_SYNC
		bookmark->SetModified(TRUE);
#endif // SUPPORT_DATA_SYNC
		RETURN_IF_ERROR(MoveBookmark(bookmark, m_trash_folder));
	}

	return SetSaveTimer();
}

UINT32 BookmarkManager::GetAttributeMaxLength(BookmarkAttributeType attribute)
{
	switch (attribute)
	{
	case BOOKMARK_URL:
		return m_bookmark_url.GetMaxLength();
	case BOOKMARK_TITLE:
		return m_bookmark_title.GetMaxLength();
	case BOOKMARK_DESCRIPTION:
		return m_bookmark_desc.GetMaxLength();
	case BOOKMARK_SHORTNAME:
		return m_bookmark_sn.GetMaxLength();
	case BOOKMARK_FAVICON_FILE:
		return m_bookmark_fav.GetMaxLength();
	case BOOKMARK_THUMBNAIL_FILE:
		return m_bookmark_tf.GetMaxLength();
	case BOOKMARK_CREATED:
		return m_bookmark_created.GetMaxLength();
	case BOOKMARK_VISITED:
		return m_bookmark_visited.GetMaxLength();
	}

	return 0;
}

BookmarkActionType BookmarkManager::GetAttributeMaxLengthAction(BookmarkAttributeType attribute)
{
	switch (attribute)
	{
	case BOOKMARK_URL:
		return m_bookmark_url.GetMaxLengthAction();
	case BOOKMARK_TITLE:
		return m_bookmark_title.GetMaxLengthAction();
	case BOOKMARK_DESCRIPTION:
		return m_bookmark_desc.GetMaxLengthAction();
	case BOOKMARK_SHORTNAME:
		return m_bookmark_sn.GetMaxLengthAction();
	case BOOKMARK_FAVICON_FILE:
		return m_bookmark_fav.GetMaxLengthAction();
	case BOOKMARK_THUMBNAIL_FILE:
		return m_bookmark_tf.GetMaxLengthAction();
	case BOOKMARK_CREATED:
		return m_bookmark_created.GetMaxLengthAction();
	case BOOKMARK_VISITED:
		return m_bookmark_visited.GetMaxLengthAction();
	}

	return ACTION_FAIL;
}

void BookmarkManager::SetAttributeMaxLength(BookmarkAttributeType attribute, UINT32 max_len, BookmarkActionType action)
{
	switch (attribute)
	{
	case BOOKMARK_URL:
		m_bookmark_url.SetMaxLengthAction(action);
		m_bookmark_url.SetMaxLength(max_len);
		break;
	case BOOKMARK_TITLE:
		m_bookmark_title.SetMaxLengthAction(action);
		m_bookmark_title.SetMaxLength(max_len);
		break;
	case BOOKMARK_DESCRIPTION:
		m_bookmark_desc.SetMaxLengthAction(action);
		m_bookmark_desc.SetMaxLength(max_len);
		break;
	case BOOKMARK_SHORTNAME:
		m_bookmark_sn.SetMaxLengthAction(action);
		m_bookmark_sn.SetMaxLength(max_len);
		break;
	case BOOKMARK_FAVICON_FILE:
		m_bookmark_fav.SetMaxLengthAction(action);
		m_bookmark_fav.SetMaxLength(max_len);
		break;
	case BOOKMARK_THUMBNAIL_FILE:
		m_bookmark_tf.SetMaxLengthAction(action);
		m_bookmark_tf.SetMaxLength(max_len);
		break;
	case BOOKMARK_CREATED:
		m_bookmark_created.SetMaxLengthAction(action);
		m_bookmark_created.SetMaxLength(max_len);
		break;
	case BOOKMARK_VISITED:
		m_bookmark_visited.SetMaxLengthAction(action);
		m_bookmark_visited.SetMaxLength(max_len);
		break;
	}
}

BookmarkItem* BookmarkManager::GetFirstByAttribute(BookmarkAttributeType attribute, BookmarkAttribute *value)
{
	BookmarkItem *bookmark = (BookmarkItem*) m_root_folder;
	for (; bookmark; bookmark = (BookmarkItem*) bookmark->Next())
	{
		BookmarkAttribute test_value;
		if (OpStatus::IsError(bookmark->GetAttribute(attribute, &test_value)))
			return NULL;
		if (test_value.Equal(value))
			return bookmark;
	}
	return NULL;
}

BookmarkItem* BookmarkManager::GetRootFolder() const
{
	return m_root_folder;
}

BookmarkItem* BookmarkManager::GetTrashFolder() const
{
	return m_trash_folder;
}

void BookmarkManager::NewTrashFolder(BookmarkItem *folder)
{
	OP_ASSERT(folder->GetFolderType() == FOLDER_TRASH_FOLDER);

	if (!m_trash_folder)
	{
		m_trash_folder = folder;
		m_trash_folders.Add(folder);
	}
	else if (m_trash_folders.Find(folder) == -1)
	{
		// Add new trash folder in alphabetic order and set the first one as real trash
		folder->SetDeletable(TRUE);
		m_trash_folder->SetDeletable(TRUE);

		UINT insert_pos = 0;
		while (insert_pos < m_trash_folders.GetCount() 
			&& uni_strcmp(folder->GetUniqueId(),m_trash_folders.Get(insert_pos)->GetUniqueId()) > 0)
			insert_pos ++;

		if (insert_pos < m_trash_folders.GetCount())
			OpStatus::Ignore(m_trash_folders.Insert(insert_pos, folder));
		else
			OpStatus::Ignore(m_trash_folders.Add(folder));
		m_trash_folder = m_trash_folders.GetCount() ? m_trash_folders.Get(0) : folder;
	}

	m_trash_folder->SetDeletable(FALSE);
}

#ifdef CORE_SPEED_DIAL_SUPPORT
BookmarkItem* BookmarkManager::GetSpeedDialFolder() const
{
	return m_speed_dial_folder;
}
#endif // CORE_SPEED_DIAL_SUPPORT


OP_STATUS BookmarkManager::MoveBookmark(BookmarkItem *&bookmark, BookmarkItem *destination_folder
#ifdef SUPPORT_DATA_SYNC
										, BOOL sync
										, BOOL no_copy
#endif // SUPPORT_DATA_SYNC
	)
{
#ifdef SUPPORT_DATA_SYNC
	// changes initiated from sync shouldn't be synced again
	OP_ASSERT(!(sync && no_copy));
#endif // SUPPORT_DATA_SYNC

	if (!bookmark || !destination_folder)
		return OpStatus::ERR_NULL_POINTER;

	if (m_save_in_progress || m_load_in_progress)
		return OpStatus::ERR;

	if (bookmark->GetFolderType() == FOLDER_TRASH_FOLDER && destination_folder != GetRootFolder())
		return OpStatus::ERR_OUT_OF_RANGE;

	if (destination_folder->GetFolderType() == FOLDER_NO_FOLDER)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (bookmark->GetParentFolder() == destination_folder)
	{
#ifdef SUPPORT_DATA_SYNC
		bookmark->SetModified(TRUE);
		if (sync)
			m_sync->BookmarkChanged(bookmark);
		bookmark->ClearSyncFlags();
#endif // SUPPORT_DATA_SYNC
		SetSaveTimer();
		return OpStatus::OK;
	}

	BOOL move_is_copy = FALSE;
	BookmarkItem *parent;
	for (parent = destination_folder; parent; parent = static_cast<BookmarkItem*>(parent->Parent()))
	{
		if (parent == bookmark)
			return OpStatus::ERR;

		if (parent->MoveIsCopy())
			move_is_copy = TRUE;
	}

	if (
#ifdef SUPPORT_DATA_SYNC
		!no_copy &&
#endif // SUPPORT_DATA_SYNC
		move_is_copy && destination_folder != bookmark->GetParentFolder())
	{
#ifdef SUPPORT_DATA_SYNC
		RETURN_IF_ERROR(CopyBookmark(bookmark, destination_folder, &bookmark, sync));
#else
		RETURN_IF_ERROR(CopyBookmark(bookmark, destination_folder, &bookmark));
#endif // SUPPORT_DATA_SYNC
		SetSaveTimer();
		return OpStatus::OK;
	}

#ifdef SUPPORT_DATA_SYNC
	if (sync)
		m_sync->CheckPreviousSync(bookmark);
#endif // SUPPORT_DATA_SYNC
	RETURN_IF_ERROR(RemoveFromTree(bookmark, FALSE, TRUE, FALSE));
#ifdef SUPPORT_DATA_SYNC
	RETURN_IF_ERROR(bookmark->SetParentFolder(destination_folder));
#ifdef BOOKMARKS_UID_INDEX
	RETURN_IF_ERROR(m_uid_index.Add(bookmark->GetUniqueId(), bookmark));
#endif // BOOKMARKS_UID_INDEX
	m_count++;

	if (sync)
		m_sync->BookmarkChanged(bookmark);
	bookmark->ClearSyncFlags();
#else
	RETURN_IF_ERROR(bookmark->SetParentFolder(destination_folder));
#ifdef BOOKMARKS_UID_INDEX
	RETURN_IF_ERROR(m_uid_index.Add(bookmark->GetUniqueId(), bookmark));
#endif // BOOKMARKS_UID_INDEX
	m_count++;
#endif // SUPPORT_DATA_SYNC
	SetSaveTimer();
	return OpStatus::OK;
}

OP_STATUS BookmarkManager::MoveBookmark(BookmarkItem *bookmark, BookmarkItem *previous, BookmarkItem *destination_folder
#ifdef SUPPORT_DATA_SYNC
					   , BOOL sync
					   , BOOL no_copy
#endif // SUPPORT_DATA_SYNC
	)
{
#ifdef SUPPORT_DATA_SYNC
	// changes initiated from sync shouldn't be synced again
	OP_ASSERT(!(sync && no_copy));
#endif // SUPPORT_DATA_SYNC

	if (previous && previous->GetParentFolder() != destination_folder)
		return OpStatus::ERR_OUT_OF_RANGE;
#ifdef SUPPORT_DATA_SYNC
	RETURN_IF_ERROR(MoveBookmark(bookmark, destination_folder, sync, no_copy));
	return MoveBookmarkAfter(bookmark, previous, sync);
#else
	RETURN_IF_ERROR(MoveBookmark(bookmark, destination_folder));
	return MoveBookmarkAfter(bookmark, previous);
#endif // SUPPORT_DATA_SYNC
}

OP_STATUS BookmarkManager::MoveBookmarkAfter(BookmarkItem *bookmark, BookmarkItem* after
#ifdef SUPPORT_DATA_SYNC
											 ,BOOL sync
#endif // SUPPORT_DATA_SYNC
											 , BOOL signal
	)
{
	OP_ASSERT(bookmark);

	if (!bookmark)
		return OpStatus::ERR_NULL_POINTER;

	if (bookmark == after || bookmark->Prev() == after)
		return OpStatus::OK;

	BookmarkItem *parent = static_cast<BookmarkItem*>(bookmark->Parent());
	if (!parent)
		return OpStatus::ERR;

#ifdef SUPPORT_DATA_SYNC
	BookmarkItem *next_in_list = static_cast<BookmarkItem*>(bookmark->Suc());
#endif // SUPPORT_DATA_SYNC

	if (after)
	{
		if (parent != after->Parent())
			return OpStatus::ERR;
		
		bookmark->Out();
		bookmark->Follow(after);
	}
	else
	{
		bookmark->Out();
		bookmark->IntoStart(parent);
	}

#ifdef SUPPORT_DATA_SYNC
	if (sync && !m_load_in_progress)
	{
		bookmark->SetModified(TRUE);
		m_sync->BookmarkChanged(bookmark);
		bookmark->ClearSyncFlags();
		if (next_in_list)
		{
			next_in_list->SetModified(TRUE);
			m_sync->BookmarkChanged(next_in_list);
			next_in_list->ClearSyncFlags();
		}
	}
#endif // SUPPORT_DATA_SYNC

	if (signal)
	{
		BookmarkItemListener *listener = bookmark->GetListener();
		if (listener)
			listener->OnBookmarkChanged(BOOKMARK_NONE, TRUE);
	}

	return SetSaveTimer();
}

OP_STATUS BookmarkManager::CopyBookmark(BookmarkItem *bookmark, BookmarkItem *destination, BookmarkItem ** new_item
#ifdef SUPPORT_DATA_SYNC
										, BOOL sync/*=TRUE*/
#endif // SUPPORT_DATA_SYNC
	)
{
	if (destination->GetFolderType() == FOLDER_NO_FOLDER || 
		destination->GetFolderType() == FOLDER_SEPARATOR_FOLDER)
		return OpStatus::ERR;

	BookmarkItem *copy = OP_NEW(BookmarkItem, ());
	if (!copy)
		return OpStatus::ERR_NO_MEMORY;

	copy->SetFolderType(bookmark->GetFolderType());
	RETURN_IF_ERROR(copy->SetAttribute(BOOKMARK_URL, bookmark->GetAttribute(BOOKMARK_URL)));
	RETURN_IF_ERROR(copy->SetAttribute(BOOKMARK_TITLE, bookmark->GetAttribute(BOOKMARK_TITLE)));
	RETURN_IF_ERROR(copy->SetAttribute(BOOKMARK_DESCRIPTION, bookmark->GetAttribute(BOOKMARK_DESCRIPTION)));
	RETURN_IF_ERROR(copy->SetAttribute(BOOKMARK_FAVICON_FILE, bookmark->GetAttribute(BOOKMARK_FAVICON_FILE)));
	RETURN_IF_ERROR(copy->SetAttribute(BOOKMARK_THUMBNAIL_FILE, bookmark->GetAttribute(BOOKMARK_THUMBNAIL_FILE)));

#ifdef SUPPORT_DATA_SYNC
	if (sync)
	{
		copy->SetModified(FALSE);
		copy->SetAdded(TRUE);
	}
#endif // SUPPORT_DATA_SYNC

	if (bookmark->GetFolderType() == FOLDER_SEPARATOR_FOLDER)
	{
#ifdef SUPPORT_DATA_SYNC
		RETURN_IF_ERROR(AddSeparator(copy, static_cast<BookmarkItem*>(destination->LastChild()), destination, TRUE));
#else
		RETURN_IF_ERROR(AddSeparator(copy, static_cast<BookmarkItem*>(destination->LastChild()), destination));
#endif // SUPPORT_DATA_SYNC
	}
	else
		RETURN_IF_ERROR(AddBookmark(copy, destination));

	BookmarkItem *children = bookmark->GetChildren();
	while (children)
	{
#ifdef SUPPORT_DATA_SYNC
		OP_STATUS ret = CopyBookmark(children, copy, NULL, sync);
#else
		OP_STATUS ret = CopyBookmark(children, copy, NULL);
#endif // SUPPORT_DATA_SYNC
		if (OpStatus::IsError(ret) && ret != OpStatus::ERR_OUT_OF_RANGE)
			return ret;

		children = children->GetNextItem();
	}

	if (new_item)
		*new_item = copy;

	return OpStatus::OK;
}

OP_STATUS BookmarkManager::SaveBookmarks(BookmarkFormatType format, BookmarkStorageProvider *storage_provider)
{
	BookmarkItem *bookmark;

	if (m_load_in_progress)
		return OpStatus::ERR;

	if (!m_count && !m_loaded_count && !m_saved_count)
	{
		OnBookmarksSaved(OpStatus::OK);
		return OpStatus::OK;
	}

	if (!RestoreSaveState(&bookmark))
	{
		m_operation_count = 0;
		bookmark = (BookmarkItem*) m_root_folder->First();

		if (storage_provider)
		{
			RETURN_IF_ERROR(SetStorageProvider(storage_provider));
			SetStorageFormat(format);
		}
		else if (!storage_provider && !m_storage_provider)
			return OpStatus::ERR;

		m_save_in_progress = TRUE;
		m_storage_provider->RegisterListener(this);
		g_main_message_handler->SetCallBack(this, MSG_BOOKMARK_SAVE_ASYNC, 0);

		if (!bookmark)
		{
			SaveSaveState(NULL);
			OP_STATUS ret = m_storage_provider->ClearStorage();
			if (OpStatus::IsError(ret))
			{
				m_save_in_progress = FALSE;
				m_storage_provider->Close();
				g_main_message_handler->UnsetCallBack(this, MSG_BOOKMARK_SAVE_ASYNC);
				return ret;
			}
			return OpStatus::OK;
		}
	}
	else
	{
		if (bookmark)
		{
			BookmarkItem* next = (BookmarkItem*) bookmark->Next();
			BookmarkItem* current_folder = bookmark->GetParentFolder();
			BookmarkItem* next_folder = next ? next->GetParentFolder() : GetRootFolder();

			if (bookmark->GetFolderType() != FOLDER_NO_FOLDER && bookmark->GetFolderType() != FOLDER_SEPARATOR_FOLDER)
			{
				m_storage_provider->FolderBegin(bookmark);
				current_folder = bookmark;
			}

			CloseFolders(current_folder, next_folder);

			bookmark = next;
		}
	}

	if (bookmark)
	{
		SaveSaveState(bookmark);
		OP_STATUS ret = m_storage_provider->SaveBookmark(bookmark);
		if (OpStatus::IsError(ret))
		{
			m_save_in_progress = FALSE;
			m_storage_provider->Close();
			g_main_message_handler->UnsetCallBack(this, MSG_BOOKMARK_SAVE_ASYNC);
			return ret;
		}
		return OpStatus::OK;
	}

	SaveSaveState(NULL);
	OnBookmarksSaved(OpStatus::OK);
	ClearBusyQueue();
	m_saved_count = m_count;
	return OpStatus::OK;
}

OP_STATUS BookmarkManager::SaveBookmarks()
{
	return SaveBookmarks(m_format, m_storage_provider);
}

OP_STATUS BookmarkManager::LoadBookmarks(BookmarkFormatType format, BookmarkStorageProvider *storage_provider)
{
	UINT number_of_bookmarks, index;
	OP_STATUS ret;
	BookmarkItem *bookmark;

	if (m_save_in_progress)
		return OpStatus::ERR;

	if (!RestoreLoadState(&index, &number_of_bookmarks))
	{
		m_operation_count = 0;
		if (!m_append_load)
		{
			RemoveAllBookmarks();
			RETURN_IF_ERROR(Init());
		}
		
		if (storage_provider)
		{
			RETURN_IF_ERROR(SetStorageProvider(storage_provider));
			SetStorageFormat(format);
		}
		else if (!storage_provider && !m_storage_provider)
			return OpStatus::ERR;

		m_storage_provider->RegisterListener(this);
		m_load_in_progress = TRUE;

		g_main_message_handler->SetCallBack(this, MSG_BOOKMARK_LOAD_ASYNC, 0);
	}

	if (m_storage_provider->MoreBookmarks())
	{
		bookmark = OP_NEW(BookmarkItem, ());
		if (!bookmark)
		{
			m_load_in_progress = FALSE;
			return OpStatus::ERR_NO_MEMORY;
		}
		
		SaveLoadState(index, number_of_bookmarks);
		ret = m_storage_provider->LoadBookmark(bookmark);
		
		if (OpStatus::IsError(ret))
		{
			OP_DELETE(bookmark);
			if (ret != OpStatus::ERR_OUT_OF_RANGE)
			{
				m_load_in_progress = FALSE;
				return ret;
			}
			else
				g_main_message_handler->PostMessage(MSG_BOOKMARK_LOAD_ASYNC, 0, 0);
		}
	}
	else
	{
		OnBookmarksLoaded(OpStatus::OK);
		ClearBusyQueue();
		m_loaded_count = m_count;
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkManager::LoadBookmarks()
{
	return LoadBookmarks(m_format, m_storage_provider);
}

void BookmarkManager::RemoveAllBookmarks()
{
	RemoveFromTree(m_root_folder, TRUE, FALSE);
	m_count = 0;
	m_root_folder = NULL;
	m_trash_folder = NULL;
#ifdef CORE_SPEED_DIAL_SUPPORT
	m_speed_dial_folder = NULL;
#endif // CORE_SPEED_DIAL_SUPPORT
	m_current_bookmark = NULL;
#ifdef BOOKMARKS_UID_INDEX
	m_uid_index.RemoveAll();
#endif // BOOKMARKS_UID_INDEX
	m_busy_queue.Clear();
}

OP_STATUS BookmarkManager::GenerateUniqueId(uni_char **uid)
{
	OpGuid guid;
	char str[37]; // ARRAY OK 2010-05-31 adame
	*uid = OP_NEWA(uni_char, 37);
	if (!*uid)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(g_opguidManager->GenerateGuid(guid));
	g_opguidManager->ToString(guid, str, 37);

	// Remove all dashes and make all letters uppercase.
	int offset=0, i;
	for (i=0; i<36; i++)
	{
		(*uid)[i-offset] = Unicode::ToUpper(str[i]);
		if (str[i] == '-')
			offset++;
	}

	(*uid)[32] = 0;
	return OpStatus::OK;
}

OP_STATUS BookmarkManager::GenerateTime(BookmarkItem *bookmark, BOOL created)
{
	BookmarkAttribute attr;
	uni_char date[21];
	*date = 0;

	time_t time = static_cast<time_t>(OpDate::GetCurrentUTCTime()/1000);
	struct tm* now = op_gmtime(&time);

	if(now)
	{
		g_oplocale->op_strftime(date, sizeof(date)/sizeof(uni_char), UNI_L("%Y-%m-%dT%H:%M:%SZ"), now);
	}
	
	RETURN_IF_ERROR(attr.SetTextValue(date));

	if (created)
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_CREATED, &attr));
	else
		RETURN_IF_ERROR(bookmark->SetAttribute(BOOKMARK_VISITED, &attr));

	return OpStatus::OK;
}

void BookmarkManager::OnTimeOut(OpTimer* timer)
{
	if (!(m_save_in_progress || m_load_in_progress))
	{
		OP_STATUS res = SaveBookmarks(m_format, m_storage_provider);
		if (OpStatus::IsError(res))
			OnBookmarksSaved(res);
			
	}

	m_first_change_after_save = TRUE;
}

void BookmarkManager::OnBookmarksLoaded(OP_STATUS res)
{
	m_load_in_progress = FALSE;
	m_append_load = FALSE;
	m_storage_provider->Close();
	g_main_message_handler->UnsetCallBack(this, MSG_BOOKMARK_LOAD_ASYNC);

	// Signal that all bookmarks has been loaded. 
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		BookmarkManagerListener* listener = m_listeners.GetNext(iterator);
		listener->OnBookmarksLoaded(res, m_operation_count);
	}
}

void BookmarkManager::OnBookmarksSaved(OP_STATUS res)
{
	if (m_storage_provider)
	{
		OP_STATUS err = m_storage_provider->Close();
		if (OpStatus::IsSuccess(res) && OpStatus::IsError(err))
			res = err;
	}
	m_save_in_progress = FALSE;
	g_main_message_handler->UnsetCallBack(this, MSG_BOOKMARK_SAVE_ASYNC);

	// Signal to all listeners that we are done.
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		BookmarkManagerListener* listener = m_listeners.GetNext(iterator);
		listener->OnBookmarksSaved(res, m_operation_count);
	}
}


// Called by storage provider to signal that a bookmark has been
// loaded from storage.
void BookmarkManager::BookmarkIsLoaded(BookmarkItem *bookmark, OP_STATUS res)
{
	if (OpStatus::IsError(res))
	{
		OP_DELETE(bookmark);

		if (res == OpStatus::ERR_OUT_OF_RANGE)
		{
			g_main_message_handler->PostMessage(MSG_BOOKMARK_LOAD_ASYNC, 0, 0);
			return;
		}
		OnBookmarksLoaded(res);
	}
	else
	{
		if (bookmark && bookmark->GetFolderType() == FOLDER_NO_FOLDER)
			m_operation_count++;

		// Trashfolder previously undefined set the main trash folder to
		// this folder.
		if (bookmark && bookmark->GetFolderType() == FOLDER_TRASH_FOLDER)
			NewTrashFolder(bookmark);
#ifdef CORE_SPEED_DIAL_SUPPORT
		if (bookmark && bookmark->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER && !m_speed_dial_folder)
			m_speed_dial_folder = bookmark;
#endif // CORE_SPEED_DIAL_SUPPORT

		g_main_message_handler->PostMessage(MSG_BOOKMARK_LOAD_ASYNC, 0, 0);
	}
}

// Called by storage provider to signal that a bookmark has been saved
// to storage.
void BookmarkManager::BookmarkIsSaved(BookmarkItem *bookmark, OP_STATUS res)
{
	if (OpStatus::IsError(res))
		OnBookmarksSaved(res);
	else
	{
		if (bookmark && bookmark->GetFolderType() == FOLDER_NO_FOLDER)
			m_operation_count++;

		g_main_message_handler->PostMessage(MSG_BOOKMARK_SAVE_ASYNC, 0, 0);
	}
}

BookmarkItem* BookmarkManager::FindId(const uni_char *uid)
{
	if (!m_root_folder || !uid)
		return NULL;

#ifdef BOOKMARKS_UID_INDEX
	BookmarkItem *ret = NULL;
	m_uid_index.GetData(uid, &ret);
	return ret;
#else
	BookmarkItem *bookmark = (BookmarkItem*) m_root_folder->First();
	for (; bookmark; bookmark = (BookmarkItem*) bookmark->Next())
	{
		if (uni_strcmp(bookmark->GetUniqueId(), uid) == 0)
			return bookmark;
	}
	return NULL;
#endif // BOOKMARKS_UID_INDEX
}

// Sorting also?
OP_STATUS BookmarkManager::GetList(Head *bookmark_list)
{
	if (!bookmark_list)
		return OpStatus::ERR_NULL_POINTER;
	if (!m_root_folder)
		return OpStatus::OK;

	BookmarkListElm *tmp;
	BookmarkItem *bookmark = (BookmarkItem*) m_root_folder->First();
	for (; bookmark; bookmark = (BookmarkItem*) bookmark->Next())
	{
		tmp = OP_NEW(BookmarkListElm, ());
		if (!tmp)
		{
			bookmark_list->Clear();
			return OpStatus::ERR_NO_MEMORY;
		}
		
		tmp->SetBookmark(bookmark);
		tmp->Into(bookmark_list);
	}
	return OpStatus::OK;
}

OP_STATUS BookmarkManager::Swap(BookmarkItem *a, BookmarkItem *b)
{
	OP_ASSERT(a && b);
	if (!(a && b))
		return OpStatus::ERR_NULL_POINTER;

	BookmarkItem *parent = static_cast<BookmarkItem*>(a->Parent());

	if (parent != b->Parent())
		return OpStatus::ERR;

	BookmarkItem *a_suc = static_cast<BookmarkItem*>(a->Suc());
	BookmarkItem *a_pred = static_cast<BookmarkItem*>(a->Pred());
	BookmarkItem *b_suc = static_cast<BookmarkItem*>(b->Suc());
	BookmarkItem *b_pred = static_cast<BookmarkItem*>(b->Pred());

	a->Out();
	b->Out();

	if (a_suc == b && b_pred == a)
	{
		if (a_pred)
		{
			b->Follow(a_pred);
			a->Follow(b);
		}
		else if (b_suc)
		{
			a->Precede(b_suc);
			b->Precede(a);
		}
		else
		{
			a->Into(parent);
			b->Precede(a);
		}
	}
	else if (a_pred == b && b_suc == a)
	{
		if (b_pred)
		{
			a->Follow(b_pred);
			b->Follow(a);
		}
		else if (a_suc)
		{
			b->Precede(a_suc);
			a->Precede(b);
		}
		else
		{
			b->Into(parent);
			a->Precede(b);
		}
	}
	else
	{
		if (a_suc)
			b->Precede(a_suc);
		else if (a_pred)
			b->Follow(a_pred);

		if (b_suc)
			a->Precede(b_suc);
		else if (b_pred)
			a->Follow(b_pred);
	}

	BookmarkItemListener *listener = a->GetListener();
	if (listener)
		listener->OnBookmarkChanged(BOOKMARK_NONE, TRUE);
	listener = b->GetListener();
	if (listener)
		listener->OnBookmarkChanged(BOOKMARK_NONE, TRUE);

	return OpStatus::OK;
}

OP_STATUS BookmarkManager::SetSaveTimer()
{
	if (m_load_in_progress || m_save_in_progress)
		return OpStatus::OK;

	switch (m_save_policy)
	{
	case SAVE_IMMEDIATELY:
		{
			OP_STATUS res = SaveBookmarks(m_format, m_storage_provider);
			if (OpStatus::IsError(res))
			{
				OnBookmarksSaved(res);
				return res;
			}
		}
		break;
	case DELAY_AFTER_FIRST_CHANGE:
		if (m_first_change_after_save)
		{
			m_first_change_after_save = FALSE;
			m_save_timer.Stop();
			m_save_timer.Start(m_save_delay);
		}
		break;
	case DELAY_AFTER_LAST_CHANGE:
		m_save_timer.Stop();
		m_save_timer.Start(m_save_delay);
		break;
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkManager::SaveImmediately()
{
	BookmarkItem *bookmark;
	BookmarkItem *current_folder = GetRootFolder();

	if (m_load_in_progress || !m_storage_provider)
		return OpStatus::ERR;

	m_save_timer.Stop();
	m_first_change_after_save = TRUE;

	if (!m_count && !m_loaded_count && !m_saved_count)
	{
		m_storage_provider->Close();
		return OpStatus::OK;
	}

	g_main_message_handler->UnsetCallBack(this, MSG_BOOKMARK_SAVE_ASYNC);
	// synchronous operation doesn't rely on the callback
	m_storage_provider->UnRegisterListener(this);

	if (!RestoreSaveState(&bookmark))
	{
		m_save_in_progress = TRUE;
		m_operation_count = 0;
		bookmark = (BookmarkItem*) m_root_folder->First();

		if (!bookmark)
		{
			SaveSaveState(NULL);
			OP_STATUS ret = m_storage_provider->ClearStorage();
			if (OpStatus::IsError(ret))
			{
				m_save_in_progress = FALSE;
				m_storage_provider->Close();
				return ret;
			}
			return OpStatus::OK;
		}
	}
	else
	{
		SaveSaveState(NULL);
		if (bookmark)
		{
			if (bookmark->GetFolderType() != FOLDER_NO_FOLDER && bookmark->GetFolderType() != FOLDER_SEPARATOR_FOLDER)
			{
				m_storage_provider->FolderBegin(bookmark);
				current_folder = bookmark;
			}
			else
				current_folder = bookmark->GetParentFolder();
			bookmark = (BookmarkItem*) bookmark->Next();
		}
	}

	while (bookmark)
	{
		CloseFolders(current_folder, bookmark->GetParentFolder());
		current_folder = bookmark->GetParentFolder();
		OP_STATUS ret = m_storage_provider->SaveBookmark(bookmark);
		if (OpStatus::IsError(ret))
		{
			m_save_in_progress = FALSE;
			m_storage_provider->Close();
			return ret;
		}

		if (bookmark->GetFolderType() != FOLDER_NO_FOLDER && bookmark->GetFolderType() != FOLDER_SEPARATOR_FOLDER)
		{
			m_storage_provider->FolderBegin(bookmark);
			current_folder = bookmark;
		}

		bookmark = (BookmarkItem*) bookmark->Next();
	}

	CloseFolders(current_folder, m_root_folder);

	m_save_in_progress = FALSE;
	RETURN_IF_ERROR(m_storage_provider->Close());
	ClearBusyQueue();
	m_saved_count = m_count;
	return OpStatus::OK;
}

void BookmarkManager::CloseFolders(BookmarkItem *current, BookmarkItem *next)
{
	BookmarkItem *current_folder = current;
	while (current_folder != next)
	{
		m_storage_provider->FolderEnd(current_folder);
		current_folder = current_folder->GetParentFolder();
	}
}

void BookmarkManager::SetSaveBookmarksTimeout(BookmarkSaveTimerType save_timer_type, UINT32 delay)
{
	m_save_policy = save_timer_type;
	m_save_delay = delay;
}

void BookmarkManager::SaveLoadState(UINT current_index, UINT number_of_bookmarks)
{
}

BOOL BookmarkManager::RestoreLoadState(UINT *current_index, UINT *number_of_bookmarks)
{
	if (!m_load_in_progress)
		return FALSE;

	return TRUE;
}

void BookmarkManager::SaveSaveState(BookmarkItem *current_bookmark)
{
	m_current_bookmark = current_bookmark;
}

BOOL BookmarkManager::RestoreSaveState(BookmarkItem **current_bookmark)
{
	if (!m_save_in_progress)
		return FALSE;

	*current_bookmark = m_current_bookmark;
	return TRUE;
}

void BookmarkManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_BOOKMARK_LOAD_ASYNC && m_load_in_progress)
	{
		OP_STATUS ret = LoadBookmarks(m_format, m_storage_provider);
		if (OpStatus::IsError(ret))
			OnBookmarksLoaded(ret);
	}
	else if (msg == MSG_BOOKMARK_SAVE_ASYNC && m_save_in_progress)
	{
		OP_STATUS ret = SaveBookmarks(m_format, m_storage_provider);
		if (OpStatus::IsError(ret))
			OnBookmarksSaved(ret);
	}
}

BOOL BookmarkManager::BookmarkManagerListenerRegistered(BookmarkManagerListener *listener)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		if (m_listeners.GetNext(iterator) == listener)
			return TRUE;
	}
	return FALSE;
}

#ifdef OPERABOOKMARKS_URL
OP_STATUS BookmarkManager::GetOperaBookmarksListener(OperaBookmarksListener **listener)
{
	if (!m_opera_bookmarks_listener)
	{
		m_opera_bookmarks_listener = OP_NEW(OperaBookmarksListener, ());
		if (!m_opera_bookmarks_listener)
			return OpStatus::ERR_NO_MEMORY;
	}

	*listener = m_opera_bookmarks_listener;
	return OpStatus::OK;
}
#endif // OPERABOOKMARKS_URL

#ifdef SUPPORT_DATA_SYNC
void BookmarkManager::DoneSyncing()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		BookmarkManagerListener* listener = m_listeners.GetNext(iterator);
		listener->OnBookmarksSynced(OpStatus::OK);
	}

	SetSaveTimer();
}
#endif // SUPPORT_DATA_SYNC

OP_STATUS BookmarkManager::ClearBusyQueue()
{
	BookmarkOperation *operation = static_cast<BookmarkOperation*>(m_busy_queue.First());
	BookmarkOperation *next_operation;
	BookmarkItem *operand1, *operand2;
	OP_STATUS res = OpStatus::OK;
	while (operation && !IsBusy())
	{
		next_operation = static_cast<BookmarkOperation*>(operation->Suc());
		operation->Out();

		operand1 = operation->GetFirstOperand();
		operand2 = operation->GetSecondOperand();

		switch (operation->GetType())
		{
		case BookmarkOperation::OPERATION_ADD:
#ifdef SUPPORT_DATA_SYNC
			res = AddBookmark(operand1, operand2, operation->ShouldSync());
#else
			res = AddBookmark(operand1, operand2);
#endif // SUPPORT_DATA_SYNC
			break;
		case BookmarkOperation::OPERATION_MOVE:
#ifdef SUPPORT_DATA_SYNC
			res = MoveBookmark(operand1, operand2, operation->ShouldSync(), operation->FromSync());
#else
			res = MoveBookmark(operand1, operand2);
#endif // SUPPORT_DATA_SYNC
			break;
		case BookmarkOperation::OPERATION_DELETE:
#ifdef SUPPORT_DATA_SYNC
			res = DeleteBookmark(operand1, TRUE, operation->ShouldSync(), operation->FromSync());
#else
			res = DeleteBookmark(operand1, TRUE);
#endif // SUPPORT_DATA_SYNC
			break;
		case BookmarkOperation::OPERATION_NONE:
			break;
		default:
			OP_ASSERT(FALSE);
		}

		if (operation->GetListener())
			operation->GetListener()->OnOperationCompleted(operation->GetFirstOperand(), operation->GetType(), res);
		OP_DELETE(operation);
		RETURN_IF_ERROR(res);

		operation = next_operation;
	}

	return OpStatus::OK;
}

OP_STATUS BookmarkManager::PlaceInBusyQueue(BookmarkItem *operand1, BookmarkItem *operand2, BookmarkOperation::BookmarkOperationType operation_type, BookmarkOperationListener *listener
#ifdef SUPPORT_DATA_SYNC
						   , BOOL should_sync
						   , BOOL from_sync
#endif // SUPPORT_DATA_SYNC
	)
{
	BookmarkOperation *operation = OP_NEW(BookmarkOperation, ());
	if (!operation)
		return OpStatus::ERR_NO_MEMORY;

	operation->SetFirstOperand(operand1);
	operation->SetSecondOperand(operand2);
	operation->SetType(operation_type);
	operation->SetListener(listener);
#ifdef SUPPORT_DATA_SYNC
	operation->SetSync(should_sync);
	operation->SetFromSync(from_sync);
#endif // SUPPORT_DATA_SYNC

	operation->Into(&m_busy_queue);

	return OpStatus::OK;
}

BOOL BookmarkManager::IsInTrashFolder(BookmarkItem *bookmark)
{
	BookmarkItem *parent = static_cast<BookmarkItem*>(bookmark->Parent());
	for (; parent; parent = static_cast<BookmarkItem*>(parent->Parent()))
	{
		if (parent == m_trash_folder)
			return TRUE;
	}
	return FALSE;
}

void BookmarkManager::SignalBookmarkAdded(BookmarkItem *bookmark)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		BookmarkManagerListener* listener = m_listeners.GetNext(iterator);
		listener->OnBookmarkAdded(bookmark);
	}
}

void BookmarkManager::SignalBookmarkRemoved(BookmarkItem *bookmark)
{
	BookmarkItemListener *listener = bookmark->GetListener();
	if (listener)
		listener->OnBookmarkRemoved();
}

void BookmarkManager::CancelPendingOperation()
{
	m_load_in_progress = FALSE;

	m_save_in_progress = FALSE;
	m_current_bookmark = NULL;

	if(g_main_message_handler->HasCallBack(this, MSG_BOOKMARK_LOAD_ASYNC, 0))
		g_main_message_handler->RemoveDelayedMessage(MSG_BOOKMARK_LOAD_ASYNC, 0, 0);

	if(g_main_message_handler->HasCallBack(this, MSG_BOOKMARK_SAVE_ASYNC, 0))
		g_main_message_handler->RemoveDelayedMessage(MSG_BOOKMARK_SAVE_ASYNC, 0, 0);

	g_main_message_handler->UnsetCallBack(this, MSG_BOOKMARK_LOAD_ASYNC);
	g_main_message_handler->UnsetCallBack(this, MSG_BOOKMARK_SAVE_ASYNC);
}

void BookmarkManager::OnBookmarkRemoved(BookmarkItem *bookmark, BOOL signal)
{
	OpStatus::Ignore(m_trash_folders.RemoveByItem(bookmark));
	if (m_trash_folder == bookmark)
	{
		m_trash_folder = m_trash_folders.GetCount() ? m_trash_folders.Get(0) : NULL;
		if (m_trash_folder)
			m_trash_folder->SetDeletable(FALSE);
	}

	if (signal)
		SignalBookmarkRemoved(bookmark);
}

void BookmarkManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id != OpPrefsCollection::Core)
		return;

	bool restart = false;

	// This sorts of replicates SetSaveTimer...

	switch (static_cast<PrefsCollectionCore::integerpref>(pref))
	{
		case PrefsCollectionCore::BookmarksSavePolicy:
		{
			BookmarkSaveTimerType policy = static_cast<BookmarkSaveTimerType>(newvalue);

			switch (policy)
			{
				case NO_AUTO_SAVE:
				case SAVE_IMMEDIATELY:
				case DELAY_AFTER_FIRST_CHANGE:
				case DELAY_AFTER_LAST_CHANGE:
					restart = m_save_policy != policy;
					break;

				default:
					policy = DELAY_AFTER_FIRST_CHANGE;
					restart = m_save_policy != policy;
					break;
			}

			m_save_policy = policy;
			break;
		}

		case PrefsCollectionCore::BookmarksSaveTimeout:
		{
			if (newvalue < 0)
				newvalue = BOOKMARKS_DEFAULT_SAVE_TIMEOUT;

			restart = static_cast<UINT32>(newvalue) != m_save_delay;
			m_save_delay = newvalue;
			break;
		}
	}

	if (restart)
		m_save_timer.Stop();
}

#endif // CORE_BOOKMARKS_SUPPORT
