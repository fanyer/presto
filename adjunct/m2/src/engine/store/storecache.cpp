/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/store/duplicatetable.h"
#include "adjunct/m2/src/engine/store/storecache.h"
#include "adjunct/m2/src/engine/store/storemessage.h"

OP_STATUS StoreCache::GetChildrenIds(message_gid_t message_id, OpINTSet& children_ids)
{
	for (StoreItem* item = GetFirstChild(message_id); item; item = GetById(item->next_sibling_id))
	{
		RETURN_IF_ERROR(children_ids.Insert(item->m2_id));
	}

	return OpStatus::OK;
}

OP_STATUS StoreCache::GetThreadIds(message_gid_t message_id, OpINTSet& thread_ids)
{
	OpINT32Vector tohandle;

	// Follow the chain of parents until the root of the thread is found
	StoreItem* item = GetById(message_id);
	if (!item)
		return OpStatus::ERR;
	
	RETURN_IF_ERROR(thread_ids.Insert(item->thread_root_id));
	RETURN_IF_ERROR(tohandle.Add(item->thread_root_id));

	// Now process all children of the root
	for (unsigned i = 0; i < tohandle.GetCount(); i++)
	{
		if (item && item->master_id != 0 && m_duplicate_table)
		{
				RETURN_IF_ERROR(tohandle.Add(item->master_id));
				RETURN_IF_ERROR(thread_ids.Insert(item->m2_id));
				const DuplicateMessage* dupe = m_duplicate_table->GetDuplicates(item->master_id);
				while (dupe)
				{
					RETURN_IF_ERROR(tohandle.Add(dupe->m2_id));
					RETURN_IF_ERROR(thread_ids.Insert(dupe->m2_id));
					dupe = dupe->next;
				}
		}
		for (item = GetFirstChild(tohandle.Get(i)); item; item = GetById(item->next_sibling_id))
		{
			if (!thread_ids.Contains(item->m2_id))
			{
				RETURN_IF_ERROR(thread_ids.Insert(item->m2_id));
				RETURN_IF_ERROR(tohandle.Add(item->m2_id));
			}
		}
	}

	return OpStatus::OK;
}

const StoreItem& StoreCache::GetFromCache(message_gid_t id) const
{
	StoreItem* found_item = GetById(id);

	if (found_item)
		return *found_item;

	return m_null_item;
}

OP_STATUS StoreCache::UpdateItem(const StoreItem& item)
{
	StoreItem* cache_item = GetById(item.m2_id);
	bool update_sent = !cache_item;
	bool update_parent = !cache_item;

	if (cache_item)
	{
		if (cache_item->parent_id != item.parent_id)
		{
			RemoveFromCacheByParent(cache_item);
			update_parent = true;
		}
		update_sent = update_parent || cache_item->child_sent_date != item.child_sent_date;
		message_gid_t next = cache_item->next_sibling_id;
		*cache_item = item;
		cache_item->next_sibling_id = next;
	}
	else
	{
		// If not found, create a new item
		cache_item = new (&m_mempool) StoreItem(item);
		if (!cache_item)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(m_cache_by_id.Add(cache_item->m2_id, cache_item)))
		{
			m_mempool.Delete(cache_item);
			return OpStatus::ERR;
		}
		
		SetThreadRootIdOnChildren(cache_item, 0);
	}

	if (cache_item->parent_id)
	{
		if (update_parent)
			RETURN_IF_ERROR(AddToCacheByParent(cache_item));
		if (update_sent && !(cache_item->flags & (1 << StoreMessage::IS_OUTGOING)))
			UpdateThreadSent(cache_item);
	}

	return OpStatus::OK;
}

OP_STATUS StoreCache::RemoveFromCacheByParent(StoreItem* item)
{
	if (!item->parent_id)
		return OpStatus::OK;

	StoreItem* head_item = 0;
	if (OpStatus::IsError(m_cache_by_parent.Remove(item->parent_id, &head_item)))
		return OpStatus::OK;

	StoreItem* prev_item = 0;
	for (StoreItem* found_item = head_item; found_item; found_item = GetById(found_item->next_sibling_id))
	{
		if (found_item == item)
		{
			if (prev_item)
			{
				prev_item->next_sibling_id = found_item->next_sibling_id;
			}
			else
			{
				head_item = GetById(found_item->next_sibling_id);
			}
			break;
		}
		prev_item = found_item;
	}

	if (head_item)
		return m_cache_by_parent.Add(head_item->parent_id, head_item);

	return OpStatus::OK;
}

OP_STATUS StoreCache::AddToCacheByParent(StoreItem* item)
{
	item->next_sibling_id = 0;

	StoreItem* predecessor = GetFirstChild(item->parent_id);
	if (!predecessor)
	{
		RETURN_IF_ERROR(m_cache_by_parent.Add(item->parent_id, item));
	}
	else
	{
		while (predecessor && predecessor->next_sibling_id && predecessor->m2_id != item->m2_id)
			predecessor = GetById(predecessor->next_sibling_id);

		if (predecessor && predecessor->m2_id != item->m2_id)
			predecessor->next_sibling_id = item->m2_id;
	}

	return OpStatus::OK;
}

void StoreCache::UpdateThreadSent(StoreItem* item)
{
	StoreItem *child_item = item;
	for (StoreItem* parent_item = GetById(child_item->parent_id); parent_item; parent_item = GetById(child_item->parent_id))
	{
		if (parent_item->child_sent_date == 0 || parent_item->child_sent_date >= child_item->child_sent_date)
			break;

		parent_item->child_sent_date = child_item->child_sent_date;
		child_item = parent_item;
	}
}

void StoreCache::SetThreadRootIdOnChildren(StoreItem* item, message_gid_t thread_root_id)
{
	if (thread_root_id == 0)
	{
		if (item->parent_id == 0)
			thread_root_id = item->master_id != 0 ? item->master_id : item->m2_id;
		else
		{
			StoreItem* parent = GetById(item->parent_id);
			if (parent)
				thread_root_id = parent->thread_root_id;
			else
				return;
		}
	}
	item->thread_root_id = thread_root_id;
	StoreItem* child = GetFirstChild(item->m2_id);

	while (child)
	{
		SetThreadRootIdOnChildren(child, thread_root_id);
		child = GetById(child->next_sibling_id);
	}
}

void StoreCache::RemoveItem(message_gid_t id)
{
	// Check if we can find this item in the list and delete it
	StoreItem* item_to_delete;
	if (OpStatus::IsSuccess(m_cache_by_id.Remove(id, &item_to_delete)))
	{
		RemoveFromCacheByParent(item_to_delete);
		m_mempool.Delete(item_to_delete);
	}
}

void StoreCache::Clear()
{
	m_cache_by_id.RemoveAll();
	m_cache_by_parent.RemoveAll();
	m_mempool.DeleteAll();
}

StoreItem* StoreCache::GetById(message_gid_t id) const
{
	StoreItem* item;
	if (OpStatus::IsSuccess(m_cache_by_id.GetData(id, &item)))
		return item;
	return 0;
}

StoreItem* StoreCache::GetFirstChild(message_gid_t parent_id) const
{
	StoreItem* item;
	if (OpStatus::IsSuccess(m_cache_by_parent.GetData(parent_id, &item)))
		return item;
	return 0;
}

#endif // M2_SUPPORT
