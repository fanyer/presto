/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ST_HISTORY_DIRECT_HISTORY_H
#define ST_HISTORY_DIRECT_HISTORY_H

#ifdef DIRECT_HISTORY_SUPPORT

#include "modules/history/direct_history.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/sync/sync_dataitem.h"

#ifdef SYNC_TYPED_HISTORY

class ST_HistoryOpSyncItem : public OpSyncItem
{
public:

	static OpString& GetProvider()
	{
		static OpString str;
		return str;
	}

	ST_HistoryOpSyncItem() :
		OpSyncItem()
	{
		m_type = OpSyncDataItem::DATAITEM_TYPED_HISTORY;
	}

	virtual OP_STATUS SetData(Key key, const OpStringC& data) { return OpStatus::ERR; }
	virtual OP_STATUS GetData(Key key, OpString& data) const { return OpStatus::ERR; }
	virtual OP_STATUS SetStatus(OpSyncDataItem::DataItemStatus status) { return OpStatus::ERR; }
	virtual OpSyncDataItem::DataItemStatus GetStatus() { return OpSyncDataItem::DATAITEM_ACTION_NONE; }
	virtual OP_STATUS CommitItem(BOOL dirty = FALSE, BOOL ordered = TRUE) { return OpStatus::ERR; }
	virtual void SetDataSyncItem(OpSyncDataItem *item) {}
	virtual OpSyncDataItem* GetDataSyncItem() { return NULL; }
	virtual const OpSyncDataItem* GetDataSyncItem() const { return NULL; }
	virtual OP_STATUS SetPrimaryKey(Key primary_key, const uni_char* key_data) { return OpStatus::ERR; }
	virtual OpSyncItem::Key GetPrimaryKey() const { return OpSyncItem::SYNC_KEY_NONE; }
};

#endif // SYNC_TYPED_HISTORY

class ST_DirectHistory :
	public DirectHistory
{
public:

	ST_DirectHistory() :
		DirectHistory(g_pccore->GetIntegerPref(PrefsCollectionCore::MaxDirectHistory)),
		m_item(NULL),
		m_sync_item(NULL)
	{}

	~ST_DirectHistory()
	{
		delete m_sync_item;
	}

	// -------------------
	// GIVE ACCESS TO
	// -------------------
	unsigned int GetCount() { return m_items.GetCount(); }

	// -------------------
	// MAKE PUBLIC
	// -------------------
	virtual void DeleteAllItems(BOOL save) { DirectHistory::DeleteAllItems(save); }
	OP_STATUS Read() { return DirectHistory::Read(); }

#ifdef SYNC_TYPED_HISTORY
	virtual OP_STATUS TypedHistoryItem_to_OpSyncItem(TypedHistoryItem* item,
													 OpSyncItem*& sync_item,
													 OpSyncDataItem::DataItemStatus sync_status)
	{
		m_item = item;

		if(!m_sync_item)
			m_sync_item = new ST_HistoryOpSyncItem();

		if(!m_sync_item)
			return OpStatus::ERR_NO_MEMORY;

		sync_item = m_sync_item;

		return OpStatus::OK;
	}

	virtual OP_STATUS OpSyncItem_to_TypedHistoryItem(OpSyncItem* sync_item,
													 TypedHistoryItem*& item)
	{
		if(m_sync_item == sync_item)
		{
			item = m_item;
			return OpStatus::OK;
		}

		return OpStatus::ERR;
	}
#endif // SYNC_TYPED_HISTORY

	// -------------------
	// MOCK OUT
	// -------------------
#ifdef SYNC_TYPED_HISTORY
	virtual OP_STATUS SyncItem(TypedHistoryItem* item,
							   OpSyncDataItem::DataItemStatus sync_status)
		{ return OpStatus::OK; }
#endif // SYNC_TYPED_HISTORY

private:

	TypedHistoryItem* m_item;
	OpSyncItem* m_sync_item;
};

#endif // DIRECT_HISTORY_SUPPORT
#endif // ST_HISTORY_DIRECT_HISTORY_H
