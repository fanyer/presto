/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/store/duplicatetable.h"


const DuplicateMessage* DuplicateTable::GetDuplicates(message_gid_t master_id)
{
	return GetById(master_id);
}

OP_STATUS DuplicateTable::AddDuplicate(message_gid_t master_id, message_gid_t duplicate_id)
{
	DuplicateMessage* item = GetById(master_id);

	if (!item)
	{
		// add new master
		DuplicateMessage* master = new (&m_mempool) DuplicateMessage(master_id);
		if (!master)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<DuplicateMessage> master_holder(master);
		
		DuplicateMessage* dupe = new (&m_mempool) DuplicateMessage(duplicate_id);
		if (!dupe)
			return OpStatus::ERR_NO_MEMORY;
		
		master->next = dupe;

		if (OpStatus::IsError(m_duplicate_table.Add(master_id, master)))
		{
			OP_DELETE(dupe);
			return OpStatus::ERR_NO_MEMORY;
		}
		master_holder.release();
		return OpStatus::OK;
	}
	else
	{
		// search for this dupe
		while (item->next != NULL && item->m2_id != duplicate_id)
		{
			item = item->next;
		}

		if (item->m2_id == duplicate_id)
			return OpStatus::OK;

		// add new dupe at the end
		DuplicateMessage* dupe = new (&m_mempool) DuplicateMessage(duplicate_id);
		if (!dupe)
			return OpStatus::ERR_NO_MEMORY;
		
		item->next = dupe;
		return OpStatus::OK;
	}

	
}

OP_STATUS DuplicateTable::RemoveDuplicate(message_gid_t master_id, message_gid_t id_to_remove, message_gid_t &new_master_id)
{
	DuplicateMessage *prev_item = NULL;
	DuplicateMessage *item = GetById(master_id);

	if (!item || !item->next || !item->next->next)
	{
		// clear entire list of dupes and remove from hashtable
		RETURN_IF_ERROR(m_duplicate_table.Remove(master_id, &item));
		new_master_id = 0;
		while (item)
		{
			DuplicateMessage* to_delete = item;
			item = item->next;
			m_mempool.Delete(to_delete);
		}
		return OpStatus::OK;
	}

	while (item && item->m2_id != id_to_remove)
	{
		prev_item = item;
		item = item->next;
	}

	if (prev_item)
	{
		prev_item->next = item->next;
		new_master_id = master_id;
	}
	else
	{
		// we wanted to remove the master_id, so remove it from the hashtable as well
		RETURN_IF_ERROR(m_duplicate_table.Remove(id_to_remove, &item));

		// update the new_master_id and insert the new list of duplicates with the new key
		new_master_id = item->next->m2_id;
		if (OpStatus::IsError(m_duplicate_table.Add(new_master_id, item->next)))
		{
			// if we can't add the new master to the hash table we might as well delete the whole list of dupes
			// since they won't be linked to anymore and will create a memleak until exit
			while (item)
			{
				prev_item = item;
				item = item->next;
				m_mempool.Delete(prev_item);
			}
			new_master_id = 0;
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	m_mempool.Delete(item);
	
	return OpStatus::OK;
}

void DuplicateTable::Clear()
{
	m_duplicate_table.RemoveAll();
	m_mempool.DeleteAll();
}

DuplicateMessage* DuplicateTable::GetById(message_gid_t m2_id)
{
	DuplicateMessage* item;
	if (OpStatus::IsSuccess(m_duplicate_table.GetData(m2_id, &item)))
		return item;
	return 0;
}

#endif // M2_SUPPORT
