/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARK_MANAGER_H
#define BOOKMARK_MANAGER_H

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/timer/optimer.h"

#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_storage_provider.h"
#include "modules/bookmarks/bookmark_operation.h"

#include "modules/util/adt/oplisteners.h"

#include "modules/prefs/prefsmanager/opprefscollection.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"

#ifdef SUPPORT_DATA_SYNC
class BookmarkSync;
#endif // SUPPORT_DATA_SYNC
#ifdef OPERABOOKMARKS_URL
class OperaBookmarksListener;
#endif // OPERABOOKMARKS_URL

/**
 *
 */
class BookmarkManagerListener
{
public:
	virtual ~BookmarkManagerListener() { }
	virtual void OnBookmarksSaved(OP_STATUS ret, UINT32 count) = 0;
	virtual void OnBookmarksLoaded(OP_STATUS ret, UINT32 count) = 0;
#ifdef SUPPORT_DATA_SYNC
	virtual void OnBookmarksSynced(OP_STATUS ret) = 0;
#endif // SUPPORT_DATA_SYNC

	virtual void OnBookmarkAdded(BookmarkItem *bookmark) = 0;
};

/**
 * BookmarkManager provides functions to store, retrieve and delete
 * bookmarks both from memory and from a storage, given a storage
 * provider.
 */
class BookmarkManager : public OpTimerListener, public MessageObject, public BookmarkStorageListener, OpPrefsListener
{
public:
	BookmarkManager();
	~BookmarkManager();

	OP_STATUS Init();
	OP_STATUS Destroy();

	/**
	 * In which situations the bookmarks will be saved on storage.
	 *
	 * SAVE_IMMEDIATELY save bookmarks after every change,
	 * DELAY_AFTER_FIRST_CHANGE restart timer on first change after a save,
	 * DELAY_AFTER_LAST_CHANGE restart timer on every change.
	 * NO_AUTO_SAVE do not save bookmarks automatically.
	 */
	enum BookmarkSaveTimerType
	{
		NO_AUTO_SAVE,
		SAVE_IMMEDIATELY,
		DELAY_AFTER_FIRST_CHANGE,
		DELAY_AFTER_LAST_CHANGE
	};

	/**
	 * Add a bookmark.
	 *
	 * @param bookmark The bookmark to add.
	 * @param bookmark_folder The bookmark folder to add the bookmark to.
	 *
	 * @return OpStatus::ERR_OUT_OF_RANGE if 'bookmark' contain
	 * attributes that are too wide or if too many bookmarks are
	 * already added to 'bookmark_folder' or if too large folder
	 * depth.
	 * OpStatus::ERR_OUT_OF_RANGE if there are too many bookmarks added.
	 * OpStatus::ERR_OUT_OF_RANGE if bookmark with the same uid already exists.
	 * OpStatus::ERR_NO_MEMORY if OOM.
	 * OpStatus::OK otherwise.
	 */
	OP_STATUS AddNewBookmark(BookmarkItem *bookmark, BookmarkItem *bookmark_folder
#ifdef SUPPORT_DATA_SYNC
							 , BOOL sync=TRUE
#endif // SUPPORT_DATA_SYNC
		);

	OP_STATUS AddBookmark(BookmarkItem *bookmark, BookmarkItem *bookmark_folder
#ifdef SUPPORT_DATA_SYNC
						  , BOOL sync=TRUE
#endif // SUPPORT_DATA_SYNC
						  , BOOL signal = TRUE
		);
	/**
	 * Add a bookmark.
	 *
	 * @param bookmark The bookmark to add.
	 * @param previous place the added bookmark after this
	 * bookmark. If NULL the added bookmark is placed first in its
	 * folder. The previous bookmark must be in the bookmark_folder.
	 * @param bookmark_folder The bookmark folder to add the bookmark to.
	 *
	 * @return OpStatus::ERR_OUT_OF_RANGE if 'bookmark' contain
	 * attributes that are too wide or if too many bookmarks are
	 * already added to 'bookmark_folder' or if too large folder
	 * depth.
	 * OpStatus::ERR if previous is not in bookmark_folder or if
	 * bookmark with the same uid already exists.
	 * OpStatus::ERR_NO_MEMORY if OOM.
	 * OpStatus::OK otherwise.
	 */
	OP_STATUS AddBookmark(BookmarkItem *bookmark, BookmarkItem *previous, BookmarkItem *bookmark_folder
#ifdef SUPPORT_DATA_SYNC
						  , BOOL sync=TRUE
#endif // SUPPORT_DATA_SYNC
		);

	/**
	 * Add a separator.
	 *
	 * @param separator The bookmark item to add as a separator.
	 * @param previous place the separator after this bookmark
	 * item. If NULL the separator is placed first in its folder. The
	 * previous bookmark must be in the bookmark folder folder.
	 * @param folder the folder that the separator will be added to.
	 *
	 * @return OpStatus::ERR_OUT_OF_RANGE if 'folder' does not allow
	 * separators to be added. Otherwise the same return values as the
	 * BookmarkManager::AddBookmark function.
	 */
	OP_STATUS AddSeparator(BookmarkItem *separator, BookmarkItem *previous, BookmarkItem *folder
#ifdef SUPPORT_DATA_SYNC
						   , BOOL sync = TRUE
#endif // SUPPORT_DATA_SYNC
		);

	/**
	 * Delete a bookmark. If it is a folder everything in the folder
	 * will be deleted. If a trash folder is present the bookmark or
	 * the folder with all content will be moved to the trash folder.
	 *
	 * @param bookmark the bookmark to delete
	 * @param real_delete if set to TRUE the bookmark will be deleted
	 * even if a trash folder is present and the bookmark is not
	 * contained in it.
	 * @param sync should this change be synced
	 * @param from_sync is this operation requested by sync server, in
	 * which case we delete the trash folder which is normally undeletable
	 *
	 * @return OpStatus::ERR_NULL_POINTER if bookmark is NULL,
	 * OpStatus::ERR_NO_MEMORY if OOM
	 * OpStatus::OK otherwise
	 */
	OP_STATUS DeleteBookmark(BookmarkItem *bookmark, BOOL real_delete = FALSE
#ifdef SUPPORT_DATA_SYNC
							 , BOOL sync=TRUE
							 , BOOL from_sync=FALSE
#endif // SUPPORT_DATA_SYNC
		);

	OP_STATUS RemoveBookmark(BookmarkItem *bookmark
#ifdef SUPPORT_DATA_SYNC
							 , BOOL sync=TRUE
#endif // SUPPORT_DATA_SYNC
		);

	/**
	 *
	 */
	UINT32 GetAttributeMaxLength(BookmarkAttributeType attribute);
	BookmarkActionType GetAttributeMaxLengthAction(BookmarkAttributeType attribute);
	void SetAttributeMaxLength(BookmarkAttributeType attribute, UINT32 max_length, BookmarkActionType action);

	UINT32 GetMaxBookmarkCount();
	UINT32 GetMaxBookmarkCountPerFolder();
	UINT32 GetMaxBookmarkFolderDepth();
	void SetMaxBookmarkCount(UINT32 max_count);
	void SetMaxBookmarkCountPerFolder(UINT32 max_count_per_folder);
	void SetMaxBookmarkFolderDepth(UINT32 max_folder_depth);
	UINT32 GetCount() const { return m_count; }

	BookmarkItem* GetFirstByAttribute(BookmarkAttributeType attribute, BookmarkAttribute* value); //returns first match

	OP_STATUS SetStorageProvider(BookmarkStorageProvider *storage_provider);
	BookmarkStorageProvider* GetStorageProvider();
	void SetStorageFormat(BookmarkFormatType format);
	BookmarkFormatType GetStorageFormat();
#ifdef SUPPORT_DATA_SYNC
	void SetBookmarkSync(BookmarkSync *bookmark_sync);
	void NotifyBookmarkChanged(BookmarkItem* bookmark);
#endif // SUPPORT_DATA_SYNC

	BOOL IsBusy () { return (m_save_in_progress || m_load_in_progress); }

	BookmarkItem* GetRootFolder() const;

	BookmarkItem* GetTrashFolder() const;

	// Called when seeing a new trash folder, when there are multiple trash
	// folders only one of them is chosen as real trash
	void NewTrashFolder(BookmarkItem *folder);

#ifdef CORE_SPEED_DIAL_SUPPORT
	BookmarkItem* GetSpeedDialFolder() const;
#endif // CORE_SPEED_DIAL_SUPPORT

	/**
	 * Move a bookmark to another folder. If bookmark is a folder then
	 * all bookmarks and subfolders are moved along. If
	 * destination_folder is a subfolder to bookmark then
	 * OpStatus::ERR_OUT_OF_RANGE is returned. If bookmark already is in the folder
	 * destination_folder attribute changes are synced and saved.
	 *
	 * @param bookmark the bookmark item to move, if the move turns out
	 * to be a copy operation the variable points to the new bookmark
	 * when the function returns.
	 */
	OP_STATUS MoveBookmark(BookmarkItem *&bookmark, BookmarkItem *destination_folder
#ifdef SUPPORT_DATA_SYNC
						   , BOOL sync=TRUE
						   , BOOL no_copy=FALSE
#endif // SUPPORT_DATA_SYNC
		);

	/**
	 * Move a bookmark to another folder and place it after a certain
	 * bookmark in that folder.
	 *
	 * @param bookmark the bookmark item to move.
	 *
	 * @param previous a bookmark in destination_folder which the
	 * bookmark moved will be placed directly after. If NULL the
	 * bookmark will be placed first in destination_folder.
	 *
	 * @param destination_folder the folder that the bookmark item
	 * will be moved to.
	 *
	 * @param sync a flag that should be TRUE if this move should be
	 * synchronized.
	 *
	 * @param no_copy the move is initiated by sync, in this case move
	 * is a move disregarding the move_is_copy attribute
	 *
	 * @returns OpStatus::ERR_NO_MEMORY if out of memory,
	 * OpStatus::ERR_OUT_OF_RANGE if previous does not belong to
	 * destination_folder, OpStatus::OK otherwise.
	 */
	OP_STATUS MoveBookmark(BookmarkItem *bookmark, BookmarkItem *previous, BookmarkItem *destination_folder
#ifdef SUPPORT_DATA_SYNC
						   , BOOL sync=TRUE
						   , BOOL no_copy=FALSE
#endif // SUPPORT_DATA_SYNC
		);

	/**
	 * Move a bookmark after another BookmarkItem in the tree. If
	 * after is NULL, then bookmark is put first in its folder. If
	 * both items exist, they must be in the same folder, otherwise
	 * OpStatus::ERR is returned.
	 */
	OP_STATUS MoveBookmarkAfter(BookmarkItem *bookmark, BookmarkItem* after
#ifdef SUPPORT_DATA_SYNC
								,BOOL sync=TRUE
#endif // SUPPORT_DATA_SYNC
								,BOOL signal = TRUE
		);

	OP_STATUS LoadBookmarks(BookmarkFormatType format, BookmarkStorageProvider* storage_provider); //format will be verbose (XML?), compact (binary, compressed?) or platform-specific. storage_provider is platform-implementation for loading (and converting to BookmarkItem, in case of platform-specific format)
	OP_STATUS SaveBookmarks(BookmarkFormatType format, BookmarkStorageProvider* storage_provider);
	OP_STATUS LoadBookmarks();
	OP_STATUS SaveBookmarks();

	void SetSaveBookmarksTimeout(BookmarkSaveTimerType save_timer_type, UINT32 delay); //How long to wait after a change, until bookmarks are saved

	void SaveLoadState(UINT current_index, UINT number_of_bookmarks);
	BOOL RestoreLoadState(UINT *current_index, UINT *number_of_bookmarks);
	void SaveSaveState(BookmarkItem *current_bookmark);
	BOOL RestoreSaveState(BookmarkItem **current_bookmark);

	void RemoveAllBookmarks();

	/**
	 * Generate a unique identification for this bookmark.
	 */
	OP_STATUS GenerateUniqueId(uni_char **uid); //will have to go through porting interface

	/**
	 * Save bookmarks to storage according to BookmarkSaveTimeType.
	 */
	virtual void OnTimeOut(OpTimer *timer);

	/**
	 * Called by storage provider to signal that the bookmark has been
	 * loaded from storage.
	 */
	void BookmarkIsLoaded(BookmarkItem *bookmark, OP_STATUS res);

	/**
	 * Called by storage provider to signal that the bookmark has been
	 * saved to storage.
	 */
	void BookmarkIsSaved(BookmarkItem *bookmark, OP_STATUS res);

	/**
	 * If IsBusy() is true then certain operations such as delete and
	 * move can not be completed on a bookmark. By calling this
	 * functions the bookmark is placed in a queue and the operations
	 * are completed when saving or loading has completed.
	 *
	 * To know when a operation has been completed a callback could be
	 * provided which is called with return value for the operation
	 * when it has completed.
	 *
	 * @param bookmark which bookmark to apply operation on.
	 * @param operation the type of operation
	 * @param listener a callback to be able to know when operation
	 * has completed and return value from operation. Can be NULL.
	 * @return OpStatus::ERR_NO_MEMORY if operation could not placed
	 * in queue. Otherwise OpStatus::OK.
	 */
	OP_STATUS PlaceInBusyQueue(BookmarkItem *operand1, BookmarkItem *operand2, BookmarkOperation::BookmarkOperationType operation_type, BookmarkOperationListener *listener
#ifdef SUPPORT_DATA_SYNC
							   , BOOL should_sync
							   , BOOL from_sync
#endif // SUPPORT_DATA_SYNC
		);

	BookmarkItem* FindId(const uni_char *uid);
	OP_STATUS GetList(Head *bookmark_list);

	/**
	 * Swap position between two bookmark items in the same folder.
	 *
	 * @returns OpStatus::ERR if 'a' and 'b' belong to different folders.
	 * OpStatus::ERR_NULL_POINTER if either 'a' or 'b' is NULL.
	 * else OpStatus::OK is returned.
	 */
	OP_STATUS Swap(BookmarkItem *a, BookmarkItem *b);

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	OP_STATUS RegisterBookmarkManagerListener(BookmarkManagerListener *listener)
	{ return m_listeners.Add(listener); }
	OP_STATUS UnregisterBookmarkManagerListener(BookmarkManagerListener *listener)
	{ return m_listeners.Remove(listener); }
	BOOL BookmarkManagerListenerRegistered(BookmarkManagerListener *listener);
#ifdef OPERABOOKMARKS_URL
	OP_STATUS GetOperaBookmarksListener(OperaBookmarksListener **listener);
#endif // OPERABOOKMARKS_URL
#ifdef SUPPORT_DATA_SYNC
	void DoneSyncing();
#endif // SUPPORT_DATA_SYNC
	void SignalBookmarkAdded(BookmarkItem *bookmark);
	void SignalBookmarkRemoved(BookmarkItem *bookmark);
	
	void SetAppendLoad(BOOL val) { if (!m_load_in_progress) m_append_load = val; }
	OP_STATUS SetSaveTimer();
	OP_STATUS SaveImmediately();

	/**
	 * @returns TRUE if bookmark is in trashfolder, else FALSE.
	 */
	BOOL IsInTrashFolder(BookmarkItem *bookmark);

	/**
	* Cancel pending operation, e.g. delayed Loading/Saving messages.
	* so we don't receive messages after resources are destructed.
	*/
	void	CancelPendingOperation();

	// From OpPrefsListener

	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

private:
	void OnBookmarksLoaded(OP_STATUS res);
	void OnBookmarksSaved(OP_STATUS res);

	OP_STATUS RemoveFromTree(BookmarkItem *bookmark, BOOL with_delete, BOOL only_bookmark, BOOL signal_removal = TRUE);
	OP_STATUS CheckLimit(BookmarkAttribute *reference, BookmarkItem *test);
	OP_STATUS GenerateTime(BookmarkItem *bookmark, BOOL created);
	OP_STATUS CopyBookmark(BookmarkItem *item, BookmarkItem *destination, BookmarkItem ** new_item = NULL
#ifdef SUPPORT_DATA_SYNC
						   ,BOOL sync=TRUE
#endif // SUPPORT_DATA_SYNC
		);

	void OnBookmarkRemoved(BookmarkItem *bookmark, BOOL signal);

	OP_STATUS ClearBusyQueue();
	void CloseFolders(BookmarkItem *current, BookmarkItem *next);

	BookmarkItem *m_root_folder;
	BookmarkItem *m_trash_folder;
#ifdef CORE_SPEED_DIAL_SUPPORT
	BookmarkItem *m_speed_dial_folder;
#endif // CORE_SPEED_DIAL_SUPPORT

	UINT32 m_max_folder_depth;
	UINT32 m_max_count_per_folder;
	UINT32 m_max_count;
	UINT32 m_count;
	UINT32 m_operation_count;
	int m_loaded_count; // Number of bookmarks loaded on last load.
	int m_saved_count; // Number of bookmarks saved on last save.
	
	BookmarkAttribute m_bookmark_url;
	BookmarkAttribute m_bookmark_title;
	BookmarkAttribute m_bookmark_desc;
	BookmarkAttribute m_bookmark_sn;
	BookmarkAttribute m_bookmark_fav;
	BookmarkAttribute m_bookmark_tf;
	BookmarkAttribute m_bookmark_created;
	BookmarkAttribute m_bookmark_visited;
	
	OpTimer m_save_timer;
	BookmarkSaveTimerType m_save_policy;
	UINT32 m_save_delay;
	BOOL m_first_change_after_save;
	
	BookmarkStorageProvider *m_storage_provider;
	BookmarkFormatType m_format;
	
 	BookmarkItem *m_current_bookmark;
	BOOL m_load_in_progress;
	BOOL m_save_in_progress;
	OpListeners<BookmarkManagerListener> m_listeners;
	BOOL m_append_load;	// don't remove all bookmarks when loading

#ifdef BOOKMARKS_UID_INDEX
	OpStringHashTable<BookmarkItem> m_uid_index;
#endif // BOOKMARKS_UID_INDEX
#ifdef SUPPORT_DATA_SYNC
	BookmarkSync *m_sync;
#endif // SUPPORT_DATA_SYNC
#ifdef OPERABOOKMARKS_URL
	OperaBookmarksListener *m_opera_bookmarks_listener;
#endif // OPERABOOKMARKS_URL

	// all the trash folders, including m_trash_folder
	OpVector<BookmarkItem> m_trash_folders;

	Head m_busy_queue;
};

#define g_bookmark_manager g_opera->bookmarks_module.m_bookmark_manager

#endif // CORE_BOOKMARKS_SUPPORT

#endif // BOOKMARK_MANAGER_H
