/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/sync/sync_datacollection.h"
#include "modules/sync/sync_parser.h"

OpSyncDataCollection::~OpSyncDataCollection()
{
	Clear();
}

OpSyncDataItem* OpSyncDataCollection::FindPrimaryKey(OpSyncDataItem::DataItemType type, const OpStringC& key, const OpStringC& value)
{
	for (OpSyncDataItemIterator item(First()); *item; ++item)
	{
		if (item->GetBaseType() == type && key == item->m_key && value == item->m_data)
			return *item;
	}
	return NULL;
}

/**
 * A wrapper-class to store an OpSyncDataItem into an OpStringHashTable.
 *
 * This wrapper-class stores the hash-key and the pointer to the OpSyncDataItem
 * instance. The constructor increases the item's reference counter and the
 * destructor decreases it.
 */
class OpSyncDataItemHashed
{
private:
	/**
	 * The hash-key for the associated OpSyncDataItem. The OpStringHashTable
	 * uses a pointer to the key's memory, so after inserting this wrapper
	 * instance into the OpStringHashTable, the key must not be changed.
	 */
	OpString m_key;
	OpSyncDataItem* m_item;

public:
	/**
	 * @param item is the OpSyncDataItem to store in the OpStringHashTable.
	 * @param key is the hash-key. This instance takes over the key's data. The
	 *  hash-key can be created by OpSyncDataHashedCollection::GetHashKey().
	 */
	OpSyncDataItemHashed(OpSyncDataItem* item, OpString& key)
		: m_item(item)
		{
			if (m_item) m_item->IncRefCount();
			m_key.TakeOver(key);
		}
	~OpSyncDataItemHashed() { if (m_item) m_item->DecRefCount(); }
	OpSyncDataItem* Item() { return m_item; }
	const uni_char* Key() const { return m_key.CStr(); }
};

/* virtual */
OpSyncDataHashedCollection::~OpSyncDataHashedCollection()
{
	ClearHashTable();
}

/* virtual */
OpSyncDataItem* OpSyncDataHashedCollection::FindPrimaryKey(OpSyncDataItem::DataItemType type, const OpStringC& key, const OpStringC& value)
{
	OpString hash_key;
	if (m_hash_table && OpStatus::IsSuccess(GetHashKey(type, key, value, hash_key)))
	{
		OpSyncDataItemHashed* hash_item = 0;
		if (OpStatus::IsSuccess(m_hash_table->GetData(hash_key, &hash_item)))
			return hash_item->Item();
		else
			return 0;
	}
	else
		return OpSyncDataCollection::FindPrimaryKey(type, key, value);
}

/* virtual */
void OpSyncDataHashedCollection::OnItemAdded(OpSyncDataItem* item)
{
	if (!m_oom)
	{
		if (!m_hash_table)
			m_hash_table = OP_NEW(OpStringHashTable<OpSyncDataItemHashed>, ());
		OpString hash_key;
		OpSyncDataItemHashed* hash_item = 0;
		if (!m_hash_table ||
			OpStatus::IsError(GetHashKey(item, hash_key)) ||
			(NULL == (hash_item = OP_NEW(OpSyncDataItemHashed, (item, hash_key)))) ||
			OpStatus::IsError(m_hash_table->Add(hash_item->Key(), hash_item)))
		{
			OP_DELETE(hash_item);
			SetOOM();
		}
	}
}

/* virtual */
void OpSyncDataHashedCollection::OnItemRemoved(OpSyncDataItem* item)
{
	if (m_hash_table)
	{
		OpString hash_key;
		if (OpStatus::IsSuccess(GetHashKey(item, hash_key)))
		{
			OpSyncDataItemHashed* hash_item = 0;
			if (OpStatus::IsSuccess(m_hash_table->Remove(hash_key, &hash_item)))
			{
				OP_ASSERT(item == hash_item->Item());
				OP_DELETE(hash_item);
			}
		}
		else
			SetOOM();
	}
}

OP_STATUS OpSyncDataHashedCollection::GetHashKey(const OpSyncDataItem* item, OpString& hash_key) const
{
	return GetHashKey(item->GetType(), item->m_key, item->m_data, hash_key);
}

OP_STATUS OpSyncDataHashedCollection::GetHashKey(OpSyncDataItem::DataItemType type, const OpStringC& key, const OpStringC& value, OpString& hash_key) const
{
	hash_key.Empty();
	return hash_key.AppendFormat("%02x:%s:%s", OpSyncDataItem::BaseTypeOf(type), key.CStr(), value.CStr());
}

void OpSyncDataHashedCollection::ClearHashTable()
{
	OP_DELETE(m_hash_table);
	m_hash_table = 0;
}

void OpSyncDataHashedCollection::SetOOM()
{
	OP_NEW_DBG("OpSyncDataHashedCollection::SetOOM()", "sync");
	ClearHashTable();
	m_oom = true;
}

#endif // SUPPORT_DATA_SYNC
