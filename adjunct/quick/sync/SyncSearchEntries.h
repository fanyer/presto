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

#ifndef __SYNC_SEARCH_ENTRIES_H__
#define __SYNC_SEARCH_ENTRIES_H__

#ifdef SUPPORT_DATA_SYNC

#include "modules/sync/sync_coordinator.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"

/*************************************************************************
 **
 ** SyncSearchEntries
 **
 **
 **************************************************************************/

class SyncSearchEntries : 
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
	public OpSyncDataClient,
#else
	public OpSyncDataListener,
#endif
	public SearchEngineManager::Listener
{
public:
	SyncSearchEntries();
	virtual ~SyncSearchEntries();

	// SearchEngineManager::Listener
	virtual	void OnSearchEngineItemAdded(SearchTemplate* item);
	virtual	void OnSearchEngineItemChanged(SearchTemplate* item, UINT32 flag_changed);
	virtual	void OnSearchEngineItemRemoved(SearchTemplate* item);
	virtual void OnSearchEnginesChanged() {}

#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
	// OpSyncDataListener
	OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);
	OP_STATUS SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error);
	OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);
	OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);
#else
	void OnSyncDataInitialize(OpSyncDataItem::DataItemType type);
	void OnSyncDataAvailable(OpSyncCollection *new_items);
	void OnSyncDataFlush(OpSyncDataItem::DataItemType type);
	void OnSyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);
#endif

	BOOL IsProcessingIncomingItems() { return m_is_receiving_items; }

protected:

#ifdef SY_CAP_SYNCLISTENER_CHANGED
	OP_STATUS ProcessSyncItem(OpSyncItem *item, SearchTemplate** search = 0);
#endif


	OP_STATUS SearchTemplateToOpSyncItem(SearchTemplate* item, 
										 OpSyncItem*& sync_item,
										 OpSyncDataItem::DataItemStatus status,
										 UINT32 changed_flag);

	OP_STATUS OpSyncItemToSearchTemplate(OpSyncItem* sync_item, 
										 SearchTemplate*& item);


	virtual void EnableSearchesListening(BOOL enable);		
private:


#ifndef SY_CAP_SYNCLISTENER_CHANGED
	OP_STATUS ProcessSyncItem(OpSyncItem *item, SearchTemplate** search = 0);
#endif

	OP_STATUS SendAllItems(BOOL dirty);

	OP_STATUS SearchItemChanged(SearchTemplate* item,
								OpSyncDataItem::DataItemStatus status,
								BOOL dirty,
								UINT32 changed_flag);
	
	// Turn off /on sending items to the server
	BOOL m_is_receiving_items;
};

#endif // SUPPORT_DATA_SYNC
#endif // __SYNC_SEARCH_ENTRIES_H__
