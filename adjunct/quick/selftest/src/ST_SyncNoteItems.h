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

#ifndef __ST_SYNC_NOTE_ITEMS_H__
#define __ST_SYNC_NOTE_ITEMS_H__

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/sync/SyncNoteItems.h"

/*************************************************************************
 **
 ** ST_SyncNoteItems
 **
 **
 **************************************************************************/

class ST_SyncNoteItems : 
	public SyncNoteItems
{
public:
	ST_SyncNoteItems() : 
	  SyncNoteItems(), 
	  m_received_notification(FALSE), 
	  m_sent_to_server(FALSE), 
	  m_note_listening(FALSE),
	  m_bookmark_listening(FALSE),
	  m_sent_add(0),
	  m_sent_delete(0),
	  m_sent_modified(0),
	  m_moved_from(0)
	  {}
	// virtual ~ST_SyncNoteItems();

	void EnableNoteListening(BOOL enable) 
	  {
	    m_note_listening = enable;
	    SyncNoteItems::EnableNoteListening(enable);
	  }

	BOOL ReceivedNotification() { return m_received_notification; }
	void ResetNotification() { m_received_notification = FALSE;}


	OP_STATUS ProcessSyncItem(OpSyncItem *item, BOOL& dirty) { return SyncNoteItems::ProcessSyncItem(item, dirty);}

	BOOL IsProcessingIncomingItems() { return m_is_receiving_items; }

	BOOL ItemsSentToServer() { return m_sent_to_server; }
	void ResetSentToServer() { m_sent_to_server = FALSE; }

	BOOL IsNoteListening(){ return m_note_listening; }

	// Implementing HotlistModelListener interface 
	void OnHotlistItemAdded(HotlistModelItem* item) 
	  { 
	    m_received_notification = TRUE; 
	    if (!IsProcessingIncomingItems())
		{
			m_sent_to_server = TRUE;
			m_sent_add++;
		}
	  }

	void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN) 
	  { 
	    m_received_notification = TRUE;
	    if (!IsProcessingIncomingItems() && !moved_as_child)
		{
			m_sent_modified++;
			m_sent_to_server = TRUE;
		}

	  }

	void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child) 
	  { 
	    m_received_notification = TRUE; 
	    if (!IsProcessingIncomingItems() && !removed_as_child)
		{
			m_sent_to_server = TRUE;
			m_sent_delete++;
		}
	  }

	void OnHotlistItemMovedFrom(HotlistModelItem* item)
	  { 
	    m_received_notification = TRUE; 
	    if (!IsProcessingIncomingItems())
		{
			m_sent_to_server = TRUE;
			m_moved_from++;
		}
	  }

	void ResetSentCounts()
	{
		m_sent_add = m_sent_delete = m_sent_modified = m_moved_from = 0;
	}

	INT32 GetAddCount() { return m_sent_add; }
	INT32 GetDeleteCount() { return m_sent_delete; }
	INT32 GetModifiedCount() { return m_sent_modified; }
	INT32 GetMovedFromCount() { return m_moved_from; }

	BOOL m_received_notification;
	BOOL m_sent_to_server;
	BOOL m_note_listening;
	BOOL m_bookmark_listening;

	INT32 m_sent_add;
	INT32 m_sent_delete;
	INT32 m_sent_modified;
	INT32 m_moved_from;
};

#endif // SUPPORT_DATA_SYNC
#endif // __ST_SYNC_SPEEDDIAL_ENTRIES_H__
