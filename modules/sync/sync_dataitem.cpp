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
#include "modules/sync/sync_dataitem.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_allocator.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_parser_myopera.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

OpSyncDataItem::OpSyncDataItem(OpSyncDataItem::DataItemType type)
	: m_children(NULL)
	, m_attributes(NULL)
	, m_parent(NULL)
	, packed1_init(0)
	, m_ref_count(0)
{
	SetType(type);
}

OpSyncDataItem::~OpSyncDataItem()
{
	OP_ASSERT(m_ref_count == 0);
	OP_ASSERT(!InList());
	OP_DELETE(m_children);
	m_children = NULL;
	OP_DELETE(m_attributes);
	m_attributes = NULL;
}

OpSyncDataItem* OpSyncDataItem::Copy() const
{
	OpSyncDataItem* new_item = OP_NEW(OpSyncDataItem, ());
	if (!new_item)
		return NULL;

	new_item->SetType(GetType());
	new_item->m_parent = m_parent;
	new_item->m_key.Set(m_key);
	new_item->m_data.Set(m_data);
	new_item->packed1_init = packed1_init;

	if (HasAttributes())
	{
		for (OpSyncDataItemIterator item(m_attributes->First()); *item; ++item)
		{
			OP_ASSERT(item->m_key.CStr());
			if (item->m_key.CStr() &&
				OpStatus::IsError(new_item->Add(item->m_key.CStr(), item->m_data.CStr(), DATAITEM_ATTRIBUTE, NULL)))
			{
				OP_DELETE(new_item);
				return 0;
			}
		}
	}

	if (HasChildren())
	{
		for (OpSyncDataItemIterator item(m_children->First()); *item; ++item)
		{
			OP_ASSERT(item->m_key.CStr());
			if (item->m_key.CStr() &&
				OpStatus::IsError(new_item->Add(item->m_key.CStr(), item->m_data.CStr(), DATAITEM_CHILD, NULL)))
			{
				OP_DELETE(new_item);
				return 0;
			}
		}
	}
	return new_item;
}

OP_STATUS OpSyncDataItem::Merge(OpSyncDataItem* other, enum MergeStatus& merge_status)
{
	merge_status = MERGE_STATUS_UNCHANGED;
	if (!other)
		return OpStatus::OK;

	OP_NEW_DBG("OpSyncDataItem::Merge()", "sync");
	if (m_key != other->m_key || m_data != other->m_data)
	{
		OP_DBG(("Error: this item should have the same id as the specified item"));
		OP_DBG(("Error:") << "this item: " << m_key.CStr() << "=" << m_data.CStr());
		OP_DBG(("Error:") << "other item: " << other->m_key.CStr() << "=" << other->m_data.CStr());
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	OP_DBG(("status: ") << "this: " << GetStatus() << " -> other: " << other->GetStatus());
	if (DATAITEM_ACTION_DELETED == other->GetStatus())
	{	// the other item was deleted:
		switch (GetStatus()) {
		case DATAITEM_ACTION_DELETED: // delete+delete: keep this unchanged
			break;

		case DATAITEM_ACTION_MODIFIED: // modify+delete:
			/* remove all children and attributes from this item and set the
			 * status of this item to "deleted": */
			SetStatus(DATAITEM_ACTION_DELETED);
			merge_status = MERGE_STATUS_MERGED;
			RemoveAllChildren();
			RemoveAllAttributes();
			/* Re-add the primary key as attribute after removing all other
			 * attributes: */
			return Add(m_key.CStr(), m_data.CStr(), OpSyncDataItem::DATAITEM_ATTRIBUTE, NULL);

		case DATAITEM_ACTION_ADDED: // add+delete
		default:
			/* this item shall be deleted, because there is no need to send any
			 * item if we first "add" and then "delete" the same item: */
			if (GetList())
				GetList()->RemoveItem(this);
			// note: this may now be deleted ...
			merge_status = MERGE_STATUS_DELETED;
		}
		return OpStatus::OK;

	}
	else if (DATAITEM_ACTION_DELETED == GetStatus())
	{	/* delete+modify/add: This item should not have any attributes
		 * or children, so move all attributes and children from the
		 * other item into this: */
		SetStatus(DATAITEM_ACTION_MODIFIED);
		merge_status = MERGE_STATUS_MERGED;
		RemoveAllChildren();
		if (other->HasChildren())
		{
			while (other->m_children->First())
				RETURN_IF_ERROR(AddChild(other->m_children->First()));
		}
		if (other->HasAttributes())
		{
			RemoveAllAttributes();
			while (other->m_attributes->First())
				RETURN_IF_ERROR(AddAttribute(other->m_attributes->First()));
		}
		return OpStatus::OK;

	}
	else
	{	/* modify/add + modify/add: use the existing status, i.e.
		 * modify+modify -> modify
		 * modify+add -> modify (though this should never happen)
		 * add+modify -> add
		 * add+add -> add */

		/* Merge all attributes from the other item into this: */
		for (OpSyncDataItemIterator attr(other->GetFirstAttribute()); *attr; ++attr)
		{
			OpSyncDataItem* item = FindAttributeById(attr->m_key.CStr());
			if (item)
			{
				if (attr->m_data != item->m_data)
				{
					// data is different
					item->m_data.TakeOver(attr->m_data);
					merge_status = MERGE_STATUS_MERGED;
				}
			}
			else
			{
				// data didn't exist in this item, move it here
				AddAttribute(*attr);
				merge_status = MERGE_STATUS_MERGED;
			}
		}

		/* Merge all children from the other item into this: */
		for (OpSyncDataItemIterator child(other->GetFirstChild()); *child; ++child)
		{
			OpSyncDataItem* item = FindChildById(child->m_key.CStr());
			if (item)
			{
				if (child->m_data != item->m_data)
				{
					// data is different
					item->m_data.TakeOver(child->m_data);
					merge_status = MERGE_STATUS_MERGED;
				}
			}
			else
			{
				// data didn't exist in the old item, add it here
				AddChild(*child);
				merge_status = MERGE_STATUS_MERGED;
			}
		}
		return OpStatus::OK;
	}
}

OP_STATUS OpSyncDataItem::Add(const uni_char* key, const uni_char* data, DataItemType type, OpSyncDataItem** sync_child_new)
{
	OP_ASSERT(key && "The key must not be 0. It is used to create a hash-value.");
	if (!key)
		return OpStatus::ERR_NULL_POINTER;

	// Create a new DataItem
	OpSyncDataItem* sync_child = OP_NEW(OpSyncDataItem, ());
	RETURN_OOM_IF_NULL(sync_child);

	// Set to an attribute
	sync_child->SetType(type);
	sync_child->m_parent = this;

	// Set the key and data
	OP_STATUS status = sync_child->m_key.Set(key);
	if (OpStatus::IsSuccess(status))
		status = sync_child->m_data.Set(data);

	if (OpStatus::IsSuccess(status))
	{	// Add this as the type of DataItem it is
		switch (type)
		{
		case DATAITEM_CHILD:	status = AddChild(sync_child); break;
		case DATAITEM_ATTRIBUTE:status = AddAttribute(sync_child); break;
		default:
			OP_ASSERT(FALSE);
			status = OpStatus::ERR;
		}
	}

	if (OpStatus::IsError(status))
		OP_DELETE(sync_child);
	else if (sync_child_new)
		// If you want a pointer to the created object assign it now
		*sync_child_new = sync_child;

	return status;
}

OP_STATUS OpSyncDataItem::AddChild(OpSyncDataItem* child)
{
	if (!m_children)
		m_children = OP_NEW(OpSyncDataCollection, ());
	RETURN_OOM_IF_NULL(m_children);
	child->m_parent = this;
	m_children->AddItem(child);
	return OpStatus::OK;
}

OP_STATUS OpSyncDataItem::AddChild(const uni_char* key, const uni_char* data, OpSyncDataItem** sync_child_new)
{
	return Add(key, data, DATAITEM_CHILD, sync_child_new);
}

void OpSyncDataItem::RemoveChild(const uni_char* key)
{
	OP_ASSERT(key);
	OpSyncDataItem* item = FindChildById(key);
	if (item)
		m_children->RemoveItem(item);
}

void OpSyncDataItem::RemoveAllChildren()
{
	if (m_children)
		m_children->Clear();
}

BOOL OpSyncDataItem::HasChildren() const
{
	return m_children && m_children->HasItems();
}

OpSyncDataItem* OpSyncDataItem::GetFirstChild()
{
	if (HasChildren())
		return m_children->First();
	else
		return 0;
}

OpSyncDataItem* OpSyncDataItem::FindChildById(const uni_char* id)
{
	for (OpSyncDataItemIterator item(GetFirstChild()); *item; ++item)
		if (item->m_key == id)
			return *item;
	return 0;
}

OP_STATUS OpSyncDataItem::AddAttribute(OpSyncDataItem* attr)
{
	if (!m_attributes)
		m_attributes = OP_NEW(OpSyncDataCollection, ());
	RETURN_OOM_IF_NULL(m_attributes);
	attr->m_parent = this;
	m_attributes->AddItem(attr);
	return OpStatus::OK;
}

OP_STATUS OpSyncDataItem::AddAttribute(const uni_char* key, const uni_char* data, OpSyncDataItem** sync_child_new)
{
	return Add(key, data, DATAITEM_ATTRIBUTE, sync_child_new);
}

void OpSyncDataItem::RemoveAttribute(const uni_char* key)
{
	OP_ASSERT(key);
	OpSyncDataItem* item = FindAttributeById(key);
	if (item)
		m_attributes->RemoveItem(item);
}

void OpSyncDataItem::RemoveAllAttributes()
{
	if (m_attributes)
		m_attributes->Clear();
}

BOOL OpSyncDataItem::HasAttributes() const
{
	return m_attributes && m_attributes->HasItems();
}

OpSyncDataItem* OpSyncDataItem::GetFirstAttribute()
{
	if (HasAttributes())
		return m_attributes->First();
	else
		return 0;
}

OpSyncDataItem* OpSyncDataItem::FindAttributeById(const uni_char* id)
{
	for (OpSyncDataItemIterator item(GetFirstAttribute()); *item; ++item)
		if (item->m_key == id)
			return *item;
	return 0;
}

OpSyncDataItem* OpSyncDataItem::FindItemById(const uni_char* key)
{
	OpSyncDataItem* item = FindAttributeById(key);
	if (item)
		return item;
	else
		return FindChildById(key);
}

const uni_char* OpSyncDataItem::FindData(const OpStringC& id)
{
	OpSyncDataItem* item = FindItemById(id.CStr());
	if (item)
		return item->m_data.CStr();
	else
		return 0;
}

// ======================================== OpSyncItem

OP_STATUS OpSyncItem::SetStatus(OpSyncDataItem::DataItemStatus status)
{
	if (!m_data_item)
		return OpStatus::ERR_NO_MEMORY;

	switch (status)
	{
	case OpSyncDataItem::DATAITEM_ACTION_ADDED:
	case OpSyncDataItem::DATAITEM_ACTION_DELETED:
	case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
		m_data_item->SetStatus(status);
		break;

	default:
		OP_ASSERT(FALSE);
		break;
	}
	return OpStatus::OK;
}

const MyOperaItemTable* FindMyOperaItem(OpSyncItem::Key key)
{
	for (unsigned int i = 0; g_sync_myopera_item_table[i].name != NULL; i++)
	{
		if (key == g_sync_myopera_item_table[i].key)
			return &(g_sync_myopera_item_table[i]);
	}
	return 0;
}

const uni_char* GetNamedKeyFromKey(OpSyncItem::Key key)
{
	const MyOperaItemTable* item_declaration = FindMyOperaItem(key);
	return item_declaration ? item_declaration->name : 0;
}

OP_STATUS OpSyncItem::SetPrimaryKey(Key primary_key, const uni_char* key_data)
{
	m_primary_key = primary_key;
	RETURN_IF_ERROR(m_key_data.Set(key_data));
	m_primary_key_name = GetNamedKeyFromKey(m_primary_key);

	OP_ASSERT(m_key_data.HasContent() && "the key data (unique id) is empty!");
	if (!m_key_data.HasContent())
		return OpStatus::ERR_OUT_OF_RANGE;

	if (OpSyncItem::SYNC_KEY_NONE != m_primary_key)
		RETURN_IF_ERROR(m_data_item->Add(m_primary_key_name, m_key_data.CStr(), OpSyncDataItem::DATAITEM_ATTRIBUTE, NULL));
	return OpStatus::OK;
}

void StripIllegalControlCharacters(OpString& data)
{
	if (data.IsEmpty())
		return;

	uni_char* ptr;
	uni_char* data_ptr;
	ptr = data_ptr = data.CStr();
	while (*ptr)
	{
		/* 0x20 = space, anything below is a control char, but we'll
		 * accept 0x0A (NL), 0x0D (CR) and 0x09 (TAB) */
		if (*ptr < 0x20 && *ptr != 0x09 && *ptr != 0x0A && *ptr != 0x0D)
			ptr++;
		else
			*data_ptr++ = *ptr++;
	}
	*data_ptr = '\0';
}

OP_STATUS OpSyncItem::SetData(Key key, const OpStringC& data)
{
	if (!m_key_data.HasContent())
	{
		OP_ASSERT(!"SetPrimaryKey() was not called with valid data");
		return OpStatus::ERR_NULL_POINTER;
	}
	if (!m_data_item)
		return OpStatus::ERR_NO_MEMORY;

	OpString legal_data;
	RETURN_IF_ERROR(legal_data.Set(data));
	StripIllegalControlCharacters(legal_data);

	if (OpSyncItem::SYNC_KEY_NONE == key)
	{
		/* If the key is SYNC_KEY_NONE, we store the data as child with an empty
		 * key (i.e. key == ""). The data is then handled as text content of
		 * this element. This is used e.g. for the encryption-key: */
		OpSyncDataItem* item = m_data_item->FindChildById(UNI_L(""));
		if (item)
			RETURN_IF_ERROR(item->m_data.TakeOver(legal_data));
		else
			RETURN_IF_ERROR(m_data_item->AddChild(UNI_L(""), legal_data.CStr()));
	}
	else
	{
		const MyOperaItemTable* item_declaration = FindMyOperaItem(key);
		if (item_declaration)
		{
			// only care about the existing data on primary keys
			OpSyncDataItem* item = m_data_item->FindItemById(item_declaration->name);
			if (item)
			{
				RETURN_IF_ERROR(item->m_key.Set(item_declaration->name));
				RETURN_IF_ERROR(item->m_data.TakeOver(legal_data));
			}
			else
				RETURN_IF_ERROR(m_data_item->Add(item_declaration->name, legal_data.CStr(), item_declaration->type, NULL));
		}
		else
			OP_ASSERT(!"Specified key not found in g_sync_myopera_item_table");
	}

	return OpStatus::OK;
}

OP_STATUS OpSyncItem::GetData(Key key, OpString& data) const
{
	if (!m_data_item)
		return OpStatus::ERR_NO_MEMORY;

	const uni_char* key_name;
	if (OpSyncItem::SYNC_KEY_NONE == key)
		key_name = UNI_L("");
	else
		key_name = GetNamedKeyFromKey(key);
	if (key_name)
	{
		const OpSyncDataItem* item = m_data_item->FindItemById(key_name);
		if (item)
		{
			if (item->m_data.HasContent())
				RETURN_IF_ERROR(data.Set(item->m_data));
			else
				data.Set("");
			return OpStatus::OK;
		}
	}
	data.Empty();
	return OpStatus::OK;
}

OP_STATUS OpSyncItem::CommitItem(BOOL dirty, BOOL ordered)
{
	OP_NEW_DBG("OpSyncItem::CommitItem", "sync");
	if (!m_data_item)
		return OpStatus::ERR_NO_MEMORY;

	OP_DBG((UNI_L("id: '%s'%s%s"), m_key_data.CStr(), dirty?UNI_L("; dirty"):UNI_L(""), ordered?UNI_L("; ordered"):UNI_L("")));
	OP_ASSERT(!(dirty && ordered) && "OpSyncItem::CommitItem(): both ordered and dirty flags can't be set at the same time as dirty implies pre-ordering");

	RETURN_IF_ERROR(m_data_item->m_key.Set(m_primary_key_name));
	RETURN_IF_ERROR(m_data_item->m_data.Set(m_key_data));

	if (ordered && !dirty)
		// merging is done in the ordering code
		RETURN_IF_ERROR(m_data_queue->Add(m_data_item, dirty, ordered));

	else
	{
		OpSyncDataCollection* collection = m_data_queue->GetSyncDataCollection(OpSyncDataQueue::SYNCQUEUE_ACTIVE);
		OpSyncDataItem* target_item = collection->FindPrimaryKey(m_data_item->GetBaseType(), m_primary_key_name, m_key_data);
		if (target_item)
		{
			OP_DBG((UNI_L("Found target item with '%s'='%s'"), m_primary_key_name, m_key_data.CStr()));
			OpSyncDataItem::MergeStatus merge_status;
			RETURN_IF_ERROR(target_item->Merge(m_data_item, merge_status));
			OP_DBG(("merge-status: ") << merge_status);
			if (merge_status == OpSyncDataItem::MERGE_STATUS_MERGED ||
				merge_status == OpSyncDataItem::MERGE_STATUS_DELETED)
				m_data_queue->SetDirty();
		}
		else
			// no merging, add it normally
			RETURN_IF_ERROR(m_data_queue->Add(m_data_item, dirty, ordered));
	}

	SetDataSyncItem(NULL);
	return OpStatus::OK;
}

#endif // SUPPORT_DATA_SYNC
