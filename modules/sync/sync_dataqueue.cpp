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
#include "modules/sync/sync_dataqueue.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_util.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opsafefile.h"

#define SYNC_WRITE_DELAY	5000 // 5 seconds delay

#define SYNC_QUEUE_XML_HEADER "<?xml version=\"1.0\" encoding=\"utf-8\"?><link>"
#define SYNC_QUEUE_XML_FOOTER "</link>"

OpSyncDataQueue::OpSyncDataQueue(OpSyncFactory* factory, OpInternalSyncListener* listener)
	: m_factory(factory)
	, m_parser(NULL)
	, m_listener(listener)
	, m_max_items_to_send(SYNC_MAX_SEND_ITEMS)
	, m_dirty(FALSE)
	, m_use_disk_queue(FALSE)
	, m_write_timer_enabled(TRUE)
{
	OP_NEW_DBG("OpSyncDataQueue::OpSyncDataQueue()", "sync");
}

OpSyncDataQueue::~OpSyncDataQueue()
{
	OP_NEW_DBG("OpSyncDataQueue::~OpSyncDataQueue()", "sync");
}

void OpSyncDataQueue::Shutdown()
{
	OP_NEW_DBG("OpSyncDataQueue::Shutdown()", "sync");
#ifdef SYNC_QUEUE_ON_DISK
	StopTimeout();
	WriteQueue();
#endif // SYNC_QUEUE_ON_DISK

	ClearCollection(m_items_to_send);
	ClearCollection(m_item_queue);
	ClearCollection(m_received_items);
}

OP_STATUS OpSyncDataQueue::Init(BOOL use_disk_queue)
{
	OP_NEW_DBG("OpSyncDataQueue::Init()", "sync");
	OP_DBG(("disk-queue: %s", use_disk_queue?"yes":"no"));
	OpString filename;
	m_use_disk_queue = use_disk_queue;
	RETURN_IF_ERROR(m_factory->GetParser(&m_parser, NULL));

#ifdef SYNC_QUEUE_ON_DISK
	if (m_use_disk_queue)
	{
		// read both queues into OpSyncDataQueue::SYNCQUEUE_ACTIVE
		RETURN_IF_ERROR(ReadQueue(UNI_L("link_queue_out_myopera.dat")));
		RETURN_IF_ERROR(ReadQueue(UNI_L("link_queue_myopera.dat")));
	}
#endif // SYNC_QUEUE_ON_DISK

	return OpStatus::OK;
}

OP_STATUS OpSyncDataQueue::SetDirty()
{
	OP_NEW_DBG("OpSyncDataQueue::SetDirty()", "sync");
	m_dirty = TRUE;
#ifdef SYNC_QUEUE_ON_DISK
	RETURN_IF_ERROR(StartTimeout());
#endif // SYNC_QUEUE_ON_DISK
	return OpStatus::OK;
}

OP_STATUS OpSyncDataQueue::Add(OpSyncDataCollection& items_to_queue)
{
	OP_NEW_DBG("OpSyncDataQueue::Add()", "sync");
	for (OpSyncDataItemIterator item(items_to_queue.First()); *item; ++item)
	{
		OP_DBG(("item: %p", *item));
		m_item_queue.AddItem(*item);
	}
	// SetDirty() will start the timeout
	SetDirty();
	return OpStatus::OK;
}

OP_STATUS OpSyncDataQueue::Add(OpSyncDataItem* item_to_queue, BOOL dirty_item, BOOL keep_ordered)
{
	OP_NEW_DBG("OpSyncDataQueue::Add()", "sync");
	BOOL merged = FALSE;
	if (!dirty_item && keep_ordered)
	{
		OP_DBG(("keep ordered"));
		RETURN_IF_ERROR(AddItemOrdered(item_to_queue, merged));
	}
	else
	{
		OpSyncDataCollection* active_collection = GetSyncDataCollection(dirty_item ? SYNCQUEUE_DIRTY_ITEMS : SYNCQUEUE_ACTIVE);
		active_collection->AddItem(item_to_queue);
	}
	if (!merged)
		m_listener->OnSyncItemAdded(item_to_queue->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED);
	SetDirty();
	return OpStatus::OK;
}

OP_STATUS OpSyncDataQueue::AddAsReceivedItems(OpSyncCollection& items)
{
	OP_NEW_DBG("OpSyncDataQueue::AddAsReceivedItems()", "sync");
	OP_DBG(("add %d items to received items", items.GetCount()));
	m_received_items.AppendItems(&items);
	return OpStatus::OK;
}

OP_STATUS OpSyncDataQueue::ClearItemsToSend()
{
	OP_NEW_DBG("OpSyncDataQueue::ClearItemsToSend()", "sync");
	ClearCollection(m_items_to_send);
	m_dirty = TRUE;
#ifdef SYNC_QUEUE_ON_DISK
	return WriteQueue();
#else // !SYNC_QUEUE_ON_DISK
	return OpStatus::OK;
#endif // SYNC_QUEUE_ON_DISK
}

OP_STATUS OpSyncDataQueue::ClearReceivedItems()
{
	OP_NEW_DBG("OpSyncDataQueue::ClearReceivedItems()", "sync");
	ClearCollection(m_received_items);
	return OpStatus::OK;
}

void OpSyncDataQueue::ClearCollection(OpSyncDataCollection& collection)
{
	OP_NEW_DBG("OpSyncDataQueue::ClearCollection()", "sync");
	collection.Clear();
}

void OpSyncDataQueue::ClearCollection(OpSyncCollection& collection)
{
	OP_NEW_DBG("OpSyncDataQueue::ClearCollection()", "sync");
	for (OpSyncItemIterator item(collection.First()); *item; ++item)
	{
		collection.Remove(*item);
		OP_DELETE(*item);
	}
}

OpSyncDataCollection* OpSyncDataQueue::GetSyncDataCollection(SyncQueueType type)
{
	switch (type)
	{
	case SYNCQUEUE_ACTIVE:		return &m_item_queue;
	case SYNCQUEUE_OUTGOING:	return &m_items_to_send;
	case SYNCQUEUE_DIRTY_ITEMS:	return &m_dirty_items;
	default:
		OP_ASSERT(FALSE && "Unsupported SyncQueueType for an OpSyncDataCollection");
	}
	return NULL;
}

OpSyncCollection* OpSyncDataQueue::GetSyncCollection(SyncQueueType type)
{
	switch (type)
	{
	case SYNCQUEUE_RECEIVED_ITEMS:	return &m_received_items;
	default:
		OP_ASSERT(FALSE && "Unsupported SyncQueueType for an OpSyncCollection");
	}
	return NULL;
}

BOOL OpSyncDataQueue::HasQueuedItems(BOOL exclude_outgoing) const
{
	if (exclude_outgoing)
		return m_item_queue.HasItems();
	else
		return m_item_queue.HasItems() || m_items_to_send.HasItems();
}

bool OpSyncDataQueue::HasQueuedItems(SyncSupportsState supports) const
{
	for (OpSyncDataItemIterator item(m_item_queue.First()); *item; ++item)
		if (supports.HasSupports(OpSyncCoordinator::GetSupportsFromType(item->GetType())))
			return true;

	return false;
}

OP_STATUS OpSyncDataQueue::AddItemOrdered(OpSyncDataItem* item, BOOL& merged)
{
	OP_NEW_DBG("OpSyncDataQueue::AddItemOrdered()", "sync");
	OpSyncDataCollection* active_collection = GetSyncDataCollection(SYNCQUEUE_ACTIVE);
	const uni_char* primary_key_name = m_parser->GetPrimaryKeyName(item->GetType());

	// get the primary key item
	const uni_char* primary_key_value = item->FindData(primary_key_name);
	if (!primary_key_value)
		return OpStatus::ERR;

	// find if we have this item already
	OpSyncDataItem* dupe_item = active_collection->FindPrimaryKey(item->GetBaseType(), primary_key_name, primary_key_value);

	merged = FALSE;
	if (item != dupe_item)
	{
		if (dupe_item)
		{
			OP_DBG((UNI_L("Found item with '%s'='%s'"), primary_key_name, primary_key_value));
			OpSyncDataItem::MergeStatus merge_status;
			RETURN_IF_ERROR(dupe_item->Merge(item, merge_status));
			OP_DBG(("merge-status: ") << merge_status);
			switch (merge_status) {
			case OpSyncDataItem::MERGE_STATUS_DELETED:
				/* "add"+"delete": dupe_item may now be deleted and there is
				 * nothing else to do for item ... */
				merged = TRUE;
				return OpStatus::OK;

			case OpSyncDataItem::MERGE_STATUS_MERGED:
				/* item is now merged into dupe_item (where dupe_item contains
				 * all data and item may be an empty shell), so now continue to
				 * check for the order of the merged item: */
				merged = TRUE;
				item = dupe_item;
				break;

			case OpSyncDataItem::MERGE_STATUS_UNCHANGED:
				/* dupe_item already contained all data that was specified in
				 * item. Now continue to check the order of dupe_item (though
				 * the order shall be correct unless dupe_item was added
				 * without order): */
				item = dupe_item;
				break;
			}
		}
		else
			// new items just go at the end
			active_collection->AddItem(item);
	}

	/* we need to look if we have an item as previous or parent, and
	 * if this previous/parent is later in the collection */
	const uni_char* prev_id = 0;
	const uni_char* previous_key_name = m_parser->GetPreviousRecordKeyName(item->GetType());
	if (previous_key_name)
		// get the previous key for the current item
		prev_id = item->FindData(previous_key_name);
	if (!(prev_id && *prev_id))
	{
		// let's try with parent then
		previous_key_name = m_parser->GetParentRecordKeyName(item->GetType());
		if (previous_key_name)
			prev_id = item->FindData(previous_key_name);
	}
	if (prev_id && *prev_id)
	{
		/* find if any item has the current previous/parent key as primary in
		 * the active collection */
		OpSyncDataItem* previous_item = active_collection->FindPrimaryKey(item->GetBaseType(), primary_key_name, prev_id);
		OP_ASSERT(item != previous_item);
		if (previous_item && item != previous_item)
		{
			/* add the item immediately after item that is the previous,
			 * unless it precedes the previous */
			if (item->IsBeforeInList(previous_item))
				/* item to update precedes the previous item, move
				 * previous item instead */
				active_collection->AddBefore(previous_item, item);
			else
				active_collection->AddAfter(item, previous_item);
		}
	}
	return OpStatus::OK;
}

void OpSyncDataQueue::PopulateOutgoingItems(SyncSupportsState supports)
{
	OP_NEW_DBG("OpSyncDataQueue::PopulateOutgoingItems()", "sync");
	UINT32 count = m_items_to_send.GetCount();
	for (OpSyncDataItemIterator item(m_item_queue.First());
		 *item && count < m_max_items_to_send; ++item)
	{
		if (supports.HasSupports(OpSyncCoordinator::GetSupportsFromType(item->GetType())))
		{	// only add the item if the supports type is enabled
			m_items_to_send.AddItem(*item);
			OP_DBG(("") << count << ": add item " << *item << "; type: " << item->GetType() << "; status: " << item->GetStatus());
			count++;
		}
	}
}

void OpSyncDataQueue::RemoveQueuedItems(OpSyncDataItem::DataItemType type)
{
	OP_NEW_DBG("OpSyncDataQueue::RemoveQueuedItems()", "sync");
	OP_DBG(("type: ") << type);
	OpSyncDataItemIterator item(0);
	for (item.Reset(m_item_queue.First()); *item; ++item)
	{
		if (item->GetType() == type)
		{
			OP_DBG(("") << "remove queued item " << *item << "; status: " << item->GetStatus());
			m_item_queue.RemoveItem(*item);
		}
	}

	for (item.Reset(m_items_to_send.First()); *item; ++item)
	{
		if (item->GetType() == type)
		{
			OP_DBG(("") << "remove item to send " << *item << "; status: " << item->GetStatus());
			m_items_to_send.RemoveItem(*item);
		}
	}
}

#ifdef SYNC_QUEUE_ON_DISK
OP_STATUS OpSyncDataQueue::StartTimeout()
{
	OP_NEW_DBG("OpSyncDataQueue::StartTimeout()", "sync");
	if (!m_write_timer_enabled)
		return OpStatus::OK;

	if (!g_main_message_handler->HasCallBack(this, MSG_SYNC_WRITE_DELAY, 0))
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SYNC_WRITE_DELAY, 0));
	}
	g_main_message_handler->RemoveDelayedMessage(MSG_SYNC_WRITE_DELAY, 0, 0);
	g_main_message_handler->PostDelayedMessage(MSG_SYNC_WRITE_DELAY, 0, 0, SYNC_WRITE_DELAY);

	return OpStatus::OK;
}

void OpSyncDataQueue::StopTimeout()
{
	OP_NEW_DBG("OpSyncDataQueue::StopTimeout()", "sync");
	if (g_main_message_handler->HasCallBack(this, MSG_SYNC_WRITE_DELAY, 0))
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_SYNC_WRITE_DELAY, 0, 0);
		g_main_message_handler->UnsetCallBack(this, MSG_SYNC_WRITE_DELAY);
	}
}

void OpSyncDataQueue::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(MSG_SYNC_WRITE_DELAY == msg);
	// TODO: Fix some error handling
	if (m_use_disk_queue)
	{
		OP_NEW_DBG("OpSyncDataQueue::HandleCallback()", "sync");
		OpStatus::Ignore(WriteQueue());
	}
}

OP_STATUS OpSyncDataQueue::ReadQueue(const OpStringC& filename)
{
	if (!m_use_disk_queue)
		return OpStatus::OK;

	OP_NEW_DBG("OpSyncDataQueue::ReadQueue()", "sync");

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename, OPFILE_USERPREFS_FOLDER));
	BOOL exists;
	RETURN_IF_ERROR(file.Exists(exists));
	if (!exists)
		return OpStatus::OK;

	RETURN_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT));

	/* Make sure the adds from reading do not result in a write, by disabling
	 * the setting of the timer for the period in which we are reading from
	 * file: */
	m_write_timer_enabled = FALSE;
	SyncResetter<BOOL> write_resetter(m_write_timer_enabled, TRUE);

	XMLFragment root;
	root.SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);
	RETURN_IF_ERROR(root.Parse(static_cast<OpFileDescriptor*>(&file)));

	if (root.EnterElement(UNI_L("link")))
	{
		OpSyncDataCollection* active_collection = GetSyncDataCollection(SYNCQUEUE_ACTIVE);

		BOOL valid_element = FALSE;

		const uni_char* primary_key = NULL;
		while (TRUE)
		{
			OpSyncDataItem* current_sync_item = NULL;

			if (root.EnterAnyElement())
			{
				BOOL extra_level = FALSE;
				BOOL match_for_subtype;
				SYNCACCEPTEDFORMAT* format = FindAcceptedFormat(root.GetElementName().GetLocalPart(), 0, match_for_subtype);
				if (format)
				{
					if (format->sub_element_tag && !match_for_subtype)
					{
						if (!root.EnterAnyElement())
						{
							continue;
						}
						extra_level = TRUE;
					}
					if (!current_sync_item)
					{
						current_sync_item = OP_NEW(OpSyncDataItem, ());
						if (!current_sync_item)
							return OpStatus::ERR_NO_MEMORY;
					}
					current_sync_item->SetType(format->item_type);
					primary_key = m_parser->GetPrimaryKeyName(format->item_type);
					valid_element = TRUE;
				}
				else
					valid_element = FALSE;

				if (valid_element)
				{
					OpString key;
					RETURN_IF_ERROR(key.Set(m_parser->GetRegularName(root.GetElementName().GetLocalPart())));

					const uni_char* value_char;
					XMLCompleteName name;
					// lets get the attributes for this element
					while (root.GetNextAttribute(name, value_char))
					{
						OpString value;
						// add all attributes without validating, validation will happen later
						RETURN_IF_ERROR(key.Set(m_parser->GetRegularName(name.GetLocalPart())));
						RETURN_IF_ERROR(value.Set(value_char));

						if (primary_key && key.HasContent() && !uni_strcmp(key.CStr(), primary_key))
						{
							OP_ASSERT(value.HasContent() && "the data is missing for the primary key");

							RETURN_IF_ERROR(current_sync_item->m_key.Set(key));
							RETURN_IF_ERROR(current_sync_item->m_data.Set(value));
						}

						OpSyncDataItem::DataItemStatus action = OpSyncDataItem::DATAITEM_ACTION_NONE;
						if (key == "status")
						{
							if (value == "added")
								action = OpSyncDataItem::DATAITEM_ACTION_ADDED;
							else if (value == "modified")
								action = OpSyncDataItem::DATAITEM_ACTION_MODIFIED;
							else if (value == "deleted")
								action = OpSyncDataItem::DATAITEM_ACTION_DELETED;
						}
						if (action == OpSyncDataItem::DATAITEM_ACTION_NONE)
							RETURN_IF_ERROR(current_sync_item->AddAttribute(key.CStr(), value.CStr()));
						else
							current_sync_item->SetStatus(action);
					}

					// handle all child elements
					while (TRUE)
					{
						if (root.EnterAnyElement())
						{
							RETURN_IF_ERROR(current_sync_item->AddChild(m_parser->GetRegularName(root.GetElementName().GetLocalPart()), root.GetText()));
							root.LeaveElement();
						}
						else
							break;
					}

					active_collection->AddItem(current_sync_item);
					SetDirty();
#ifdef _DEBUG
//					TempBuffer xml;
//					current_sync_item->ToXML(xml);
#endif
				}
				if (extra_level)
				{
					root.LeaveElement();
				}
				root.LeaveElement();
			}
			else
				break;
		}
		root.LeaveElement();
	}
	return OpStatus::OK;
}

OP_STATUS OpSyncDataQueue::WriteQueue()
{
	if (!m_use_disk_queue)
		return OpStatus::OK;
	if (!m_dirty)
		return OpStatus::OK;

	OP_NEW_DBG("OpSyncDataQueue::WriteQueue()", "sync");
	RETURN_IF_ERROR(WriteQueue(UNI_L("link_queue_myopera.dat"), OpSyncDataQueue::SYNCQUEUE_ACTIVE));
	RETURN_IF_ERROR(WriteQueue(UNI_L("link_queue_out_myopera.dat"), OpSyncDataQueue::SYNCQUEUE_OUTGOING));

	m_dirty = FALSE;
	return OpStatus::OK;
}

OP_STATUS OpSyncDataQueue::WriteQueue(const OpStringC& filename, SyncQueueType dest_queue_type)
{
	OP_NEW_DBG("OpSyncDataQueue::WriteQueue()", "sync");

	OpSyncDataCollection* collection = GetSyncDataCollection(dest_queue_type);

	TempBuffer xml;
	xml.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	xml.SetCachedLengthPolicy(TempBuffer::TRUSTED);

	XMLFragment xmlfragment;
	RETURN_IF_ERROR(xmlfragment.OpenElement(UNI_L("link")));
	m_parser->ToXML(*collection, xmlfragment, TRUE, FALSE);
	xmlfragment.CloseElement();	// </link>

	RETURN_IF_ERROR(xmlfragment.GetXML(xml, TRUE, "utf-8", FALSE));

	OpSafeFile file;
	RETURN_IF_ERROR(file.Construct(filename, OPFILE_USERPREFS_FOLDER));
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE | OPFILE_TEXT));

	OpString8 tmp;
	RETURN_IF_ERROR(tmp.SetUTF8FromUTF16(xml.GetStorage(), xml.Length()));
	RETURN_IF_ERROR(file.Write(tmp.CStr(), tmp.Length()));
	RETURN_IF_ERROR(file.SafeClose());

	return OpStatus::OK;
}
#endif // SYNC_QUEUE_ON_DISK

#endif // SUPPORT_DATA_SYNC
