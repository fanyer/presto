/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#ifndef __ST_SYNC_SEARCH_ENTRIES_H__
#define __ST_SYNC_SEARCH_ENTRIES_H__

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/sync/SyncSearchEntries.h"

/*************************************************************************
 **
 ** ST_SyncSearchEntries
 **
 **
 **************************************************************************/

class ST_SyncSearchEntries : 
public SyncSearchEntries
{
public:
	ST_SyncSearchEntries() : 
		SyncSearchEntries(),
		m_received_notification(FALSE),
		m_sent_to_server(FALSE),
		m_listening(FALSE)	{}
	// virtual ~ST_SyncSearchEntries();

	OP_STATUS ProcessSyncItem(OpSyncItem *item, SearchTemplate** search = 0) { return SyncSearchEntries::ProcessSyncItem(item, search);}
	OP_STATUS SearchTemplateToOpSyncItem(SearchTemplate* item, 
										 OpSyncItem*& sync_item,
										 OpSyncDataItem::DataItemStatus status,
										 UINT32 flag)
	  { return SyncSearchEntries::SearchTemplateToOpSyncItem(item, sync_item, status, flag); }

	OP_STATUS OpSyncItemToSearchTemplate(OpSyncItem* sync_item, 
					     SearchTemplate*& item)
	  { return SyncSearchEntries::OpSyncItemToSearchTemplate(sync_item, item); }

	BOOL ItemsSentToServer() { return m_sent_to_server; }
	void ResetSentToServer() { m_sent_to_server = FALSE; }
	BOOL ReceivedNotification() { return m_received_notification; }
	void ResetNotification() { m_received_notification = FALSE;}

	void EnableSearchesListening(BOOL enable)
	{
		m_listening = TRUE;
		SyncSearchEntries::EnableSearchesListening(enable);
	}

	// SearchEngineManager::Listener
	virtual	void OnSearchEngineItemAdded(SearchTemplate* item)
	{
		m_received_notification = TRUE;
		SyncSearchEntries::OnSearchEngineItemAdded(item);
	    if (!IsProcessingIncomingItems())
	      m_sent_to_server = TRUE;

	}

	virtual	void OnSearchEngineItemChanged(SearchTemplate* item, UINT32 flag_changed)
	{
		m_received_notification = TRUE;
		SyncSearchEntries::OnSearchEngineItemChanged(item, flag_changed);
	    if (!IsProcessingIncomingItems())
	      m_sent_to_server = TRUE;
	}


	virtual	void OnSearchEngineItemRemoved(SearchTemplate* item)
	{
		m_received_notification = TRUE;
		SyncSearchEntries::OnSearchEngineItemRemoved(item);
	    if (!IsProcessingIncomingItems())
	      m_sent_to_server = TRUE;

	}

	virtual void OnSearchEnginesChanged() {}

	BOOL IsListening() { return m_listening; }


private:

	BOOL m_received_notification;
	BOOL m_sent_to_server;
	BOOL m_listening;
		
	
};

#endif // SUPPORT_DATA_SYNC
#endif // __ST_SYNC_SEARCH_ENTRIES_H__
