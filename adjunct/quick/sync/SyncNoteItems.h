/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Adam Minchinton (adamm)
 *
 */

#ifndef __SYNC_NOTE_ITEMS_H__
#define __SYNC_NOTE_ITEMS_H__

#include "modules/sync/sync_coordinator.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "modules/sync/sync_dataitem.h"

class SyncManager;

/*************************************************************************
 **
 ** SyncNoteItems
 **
 **
 **************************************************************************/


class SyncNoteItems : public HotlistModelListener
						  , public MessageObject
						  , public OpSyncDataClient
{
public:
	SyncNoteItems();
	virtual ~SyncNoteItems();

public:
	// Implementing HotlistModelListener interface 
	void OnHotlistItemAdded(HotlistModelItem* item); 
	void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN);
	void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child);
	void OnHotlistItemTrashed(HotlistModelItem* item ) {}
	void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
	void OnHotlistItemMovedFrom(HotlistModelItem* item);

	// OpSyncDataClient
	virtual OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);
	virtual OP_STATUS SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error);
	virtual OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);
	virtual OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);

	OP_STATUS SendModifiedNotes();

protected:

	/// override function from MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	OP_STATUS StartTimeout();

	// Turn off /on sending items to the server
	BOOL m_is_receiving_items;

	OP_STATUS ProcessSyncItem(OpSyncItem *item, BOOL& dirty);

	virtual void EnableNoteListening(BOOL enable);
	BOOL IsProcessingIncomingItems(){ return m_is_receiving_items; }
	
private:
	OP_STATUS SendAllItems(BOOL dirty, INT32 model_type);

	OP_STATUS BuildNotesItem(OpSyncItem* item,
							 HotlistManager::ItemData& item_data,
							 double& created_date,
							 OpString& previousGuid,
							 OpString& parentGuid);

	// Functions to create new items 
	OP_STATUS CreateItemOrdered(HotlistGenericModel* model,
								OpSyncDataItem::DataItemType type, 
								const HotlistManager::ItemData& item_data, 
								HotlistModelItem* previous );

	OP_STATUS CreateItemFirst(HotlistGenericModel* model,
							OpSyncDataItem::DataItemType type, 
							const HotlistManager::ItemData& item_data, 
							HotlistModelItem* parent);

	// Internal method to handle notifications from the hotlist
	OP_STATUS HotlistItemChanged(HotlistModelItem* item, 
								 OpSyncDataItem::DataItemStatus status, 
								 BOOL dirty, 
								 UINT32 changed_flag, 
								 BOOL delayed_triggered = FALSE);

	// Utility functions
	OpSyncDataItem::DataItemType GetSyncTypeFromItemType(HotlistModelItem* item );
	OP_STATUS SetType(HotlistModelItem* item, OpSyncDataItem* sync_item, INT32 model_type);
	OP_STATUS SetParent(HotlistModelItem* item, OpSyncItem* sync_item);
	OP_STATUS SetPrevious(HotlistModelItem* item, OpSyncItem* sync_item);
	OP_STATUS SetCreated(HotlistModelItem* item, OpSyncItem* sync_item);

	// TRUE if a delayed timer for notes changes are already running
	BOOL m_has_delayed_timer;
};

#endif // __SYNC_NOTE_ITEMS_H__
