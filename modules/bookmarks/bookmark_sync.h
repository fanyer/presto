/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARK_SYNC_H
#define BOOKMARK_SYNC_H

#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT

#include "modules/bookmarks/bookmark_item.h"

class BookmarkManager;
class OpSyncCoordinator;
class OpSyncItem;

/**
 * Class for syncing bookmarks using the sync module.
 */
class BookmarkSync
{
public:
	BookmarkSync();
	~BookmarkSync();

	void SetManager(BookmarkManager *manager);
	void SetCoordinator(OpSyncCoordinator *coordinator);

	/** Check which bookmarks are changed since last sync. */
	OP_STATUS BookmarkChanged(BookmarkItem *bookmark, BOOL ordered = TRUE);

	/** Called when we have got a bookmark from the outside to add. */
	OP_STATUS NewItem(OpSyncItem *item);

	/***/
	OP_STATUS CleanupItems();

	/**
	 * Called after sync from the server to indicate that sync has
	 * finished.
	 */
	void DoneSyncing();

	/**
	 * Check if full resync is needed from the server.
	 */
	BOOL DirtySync() const;

	/** @param val TRUE a dirty sync is pending. FALSE if no dirty
	 * sync is pending.
	 */
	void WaitingForDirtySync(BOOL val);

	/**
	 * Go through all bookmarks and add them for sync. Called when
	 * sync signals that all data needs to be resent to the server.
	 */
	OP_STATUS ResyncAllBookmarks();
#ifdef CORE_SPEED_DIAL_SUPPORT
	OP_STATUS ResyncAllSpeeddials();
#endif // CORE_SPEED_DIAL_SUPPORT

	/**
	 * Called when deleting a bookmark folder to check if the folder
	 * was added but not yet synchronized, in which case all
	 * subfolders and bookmarks contained in the folder are
	 * synchronized as deleted as well. Folders which have been
	 * synchronized as added before are synchronized as added but not
	 * their contents.
	 */
	OP_STATUS CheckFolderSync(BookmarkItem *folder, BOOL sync);

	/**
	 * Called when moving or deleting a bookmark to update the next
	 * bookmark elements previous value. If the next bookmark is in
	 * the sync queue.
	 */
	OP_STATUS CheckPreviousSync(BookmarkItem *bookmark);
private:
	OP_STATUS ReadAttributes(BookmarkItem *bookmark, OpSyncItem *item);
	OP_STATUS SetFlag(BookmarkItem *bookmark, BookmarkAttributeType attr_type);
#ifdef CORE_SPEED_DIAL_SUPPORT
	OP_STATUS SpeedDialChanged(BookmarkItem *speed_dial);
	OP_STATUS InitSpeedDialBookmarks();
#endif // CORE_SPEED_DIAL_SUPPORT

	BOOL IsAllowedToSync(BookmarkItem *bookmark);

	BookmarkManager *m_manager;
	OpSyncCoordinator *m_coordinator;

	BOOL m_dirty_sync; ///< Need to start a dirty sync since missing item from server.
	BOOL m_waiting_dirty_sync; ///< Wating for a pending dirty sync from the server.

	Head m_added;
	Head m_moved;
};

#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT

#endif // BOOKMARK_SYNC_H
