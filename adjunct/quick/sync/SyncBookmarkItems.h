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

#ifndef __SYNC_BOOKMARK_ITEMS_H__
#define __SYNC_BOOKMARK_ITEMS_H__

#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_dataitem.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

class SyncManager;
class BookmarkSync;

/*************************************************************************
 **
 ** SyncBookmarkItems - forward bookmark syncing notifications to core bookmark syncer
 **
 **
 **************************************************************************/
class SyncBookmarkItems : public OpSyncDataClient
						, public HotlistModelListener
{
public:
	SyncBookmarkItems();
	virtual ~SyncBookmarkItems();

	// Implementing HotlistModelListener interface 
	// Only handle OnHotlistItemChanged in Desktop, core will take care of adding/removing/moving
	void OnHotlistItemAdded(HotlistModelItem* item) {}
	void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child) {}
	void OnHotlistItemTrashed(HotlistModelItem* item ) {}
	void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
	void OnHotlistItemMovedFrom(HotlistModelItem* item) {}
	void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN);


	// OpSyncDataClient
	virtual OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);
	virtual OP_STATUS SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error);
	virtual OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);
	virtual OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);

private:
	BookmarkSync* m_core_bookmarks_syncer;
	BOOL m_is_receiving_items; // Avoid sending back change from sync server
};

#endif // __SYNC_BOOKMARK_ITEMS_H__
