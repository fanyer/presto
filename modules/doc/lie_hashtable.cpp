/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/doc/lie_hashtable.h"
#include "modules/util/excepts.h"

LoadInlineElmHashTable::LoadInlineElmHashTable()
	: OpHashTable(NULL, TRUE)
{
}

OP_STATUS LoadInlineElmHashTable::Add(LoadInlineElm* lie)
{
	URL_ID url_id = lie->GetRedirectedUrl()->Id();
	void* hash_entry;
	LoadInlineElmHashEntry* entry;
	void* hash_key = reinterpret_cast<void*>(url_id);
	if (OpStatus::IsSuccess(OpHashTable::GetData(hash_key, &hash_entry)))
	{
		entry = static_cast<LoadInlineElmHashEntry*>(hash_entry);
		if (entry->m_list.HasLink(lie))
			return OpStatus::ERR;
		entry->Out(); // Move entry last, for insertion ordering
	}
	else
	{
		if ((entry = OP_NEW(LoadInlineElmHashEntry, (hash_key))) == NULL)
			return OpStatus::ERR_NO_MEMORY;
		OP_STATUS status = OpHashTable::Add(hash_key, entry);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(entry);
			return status;
		}
	}
	entry->Into(&m_insertion_ordered_entries);
	lie->Into(&entry->m_list);
	return OpStatus::OK;
}

OP_STATUS LoadInlineElmHashTable::Remove(LoadInlineElm* lie)
{
	URL_ID url_id = lie->GetRedirectedUrl()->Id();
	void* hash_entry;
	LoadInlineElmHashEntry* entry;
	if (OpStatus::IsSuccess(OpHashTable::GetData((void*)url_id, &hash_entry)))
	{
		entry = static_cast<LoadInlineElmHashEntry*>(hash_entry);
		if (entry->m_list.HasLink(lie))
		{
			lie->Out();
			if (entry->m_list.Empty())
			{
				entry->Out();
				void* dummy;
				OP_STATUS status = OpHashTable::Remove((void*)url_id, &dummy);
				OP_ASSERT(dummy == hash_entry);
				OP_DELETE(entry);
				return status;
			}
			return OpStatus::OK;
		}
	}

	/* Remove is NOT called on elements that should not exist in the hash,
	 * so something is currently amiss. However, in most situations
	 * "delete lie;" is called shortly after this!
	 * When we get here, the lie->GetRedirectedUrl()->Id() has probably changed
	 * due to a bug somewhere else. Assertions elsewhere should prevent this
	 * in debug code, but in production, we must make an extra effort to
	 * aviod a guaranteed crash. */
	OpHashIterator* it = OpHashTable::GetIterator();
	if (it && OpStatus::IsSuccess(it->First()))
		do
		{
			entry = (LoadInlineElmHashEntry*)it->GetData();
			if (entry->m_list.HasLink(lie))
			{
				OP_ASSERT(0); // It is this situation that is really wrong!
				OP_DELETE(it);
				lie->Out();
				if (entry->m_list.Empty())
				{
					entry->Out();
					void* dummy;
					OP_STATUS status = OpHashTable::Remove(entry->m_hash_key, &dummy);
					OP_ASSERT(dummy == entry);
					OP_DELETE(entry);
					return status;
				}
				return OpStatus::OK;
			}
		}
		while (OpStatus::IsSuccess(it->Next()));

	OP_DELETE(it);
	return OpStatus::ERR;
}

OP_STATUS LoadInlineElmHashTable::GetData(URL_ID url_id, Head** list) const
{
	void* hash_entry;
	RETURN_IF_ERROR(OpHashTable::GetData((void*)url_id, &hash_entry));
	LoadInlineElmHashEntry* entry = static_cast<LoadInlineElmHashEntry*>(hash_entry);
	*list = &entry->m_list;
	return OpStatus::OK;
}

BOOL LoadInlineElmHashTable::Contains(LoadInlineElm* lie) const
{
	Head* list;
	return OpStatus::IsSuccess(GetData(lie->GetRedirectedUrl()->Id(), &list)) && list->HasLink(lie);
}

BOOL LoadInlineElmHashTable::Empty() const
{
	return OpHashTable::GetCount() == 0;
}

OP_STATUS LoadInlineElmHashTable::UrlMoved(URL_ID old_url_id, URL_ID new_url_id)
{
	OP_STATUS return_value = OpStatus::OK;
	void* hash_entry;
	if (OpStatus::IsSuccess(OpHashTable::Remove((void*)old_url_id, &hash_entry)))
	{
		LoadInlineElmHashEntry* entry = static_cast<LoadInlineElmHashEntry*>(hash_entry);
		// We can't just OpHashTable::Add(new_url_id,entry); because
		// there may exist elements mapped to new_url_id in the hash
		// already. Instead, we add them one by one.
		for (LoadInlineElm *lie = static_cast<LoadInlineElm*>(entry->m_list.First()), *next; lie; lie = next)
		{
			next = static_cast<LoadInlineElm*>(lie->Suc()); // After "Add", lie will be linked into another list
			OP_ASSERT(lie->GetRedirectedUrl()->Id() == new_url_id);
			lie->Out();
			OP_STATUS status = Add(lie);
			if (OpStatus::IsError(status))
			{
				// Not much we can do without memory. Let's forget about this online. :-(
				// It might be reloaded later if we get enough memory
				OP_DELETE(lie);
				return_value = status;
			}
			else
			{
				OP_ASSERT(lie->InList());
			}
		}
		OP_ASSERT(!entry->m_list.First() || !"All LoadInlineElm objects should have been moved");
		entry->Out();
		OP_DELETE(entry);
	}
	return return_value;
}

OP_STATUS LoadInlineElmHashTable::MoveLast(LoadInlineElm* lie)
{
	// Note! If more than one LoadInlineElm has the same URL_ID, the whole list is moved,
	// and the given LoadInlineElm is placed last in the list.
	OP_ASSERT(Contains(lie));
	void* hash_entry;
	RETURN_IF_ERROR(OpHashTable::GetData((void*)lie->GetRedirectedUrl()->Id(), &hash_entry));

	LoadInlineElmHashEntry* entry = static_cast<LoadInlineElmHashEntry*>(hash_entry);

	OP_ASSERT(entry->m_list.HasLink(lie));
	lie->Out();
	lie->Into(&entry->m_list);
	entry->Out();
	entry->Into(&m_insertion_ordered_entries);
	return OpStatus::OK;
}

void LoadInlineElmHashTable::DeleteAll()
{
	OpHashTable::DeleteAll();
}

void LoadInlineElmHashTable::Delete(void* data)
{
	LoadInlineElmHashEntry* entry = (LoadInlineElmHashEntry*)data;
	entry->m_list.Clear();
	entry->Out();
	OP_DELETE(entry);
}


LoadInlineElmHashIterator::LoadInlineElmHashIterator(const LoadInlineElmHashTable& hash_table)
	: m_entries(&hash_table.m_insertion_ordered_entries), m_next_entry(NULL), m_next(NULL)
{
}

LoadInlineElm* LoadInlineElmHashIterator::First()
{
	LoadInlineElmHashEntry* current_entry = (LoadInlineElmHashEntry*)m_entries->First();
	if (!current_entry)
		return NULL;

	LoadInlineElm* current = (LoadInlineElm*)current_entry->m_list.First();

	// Saving "next" makes the iterator work even if "current" is removed
	m_next_entry = (LoadInlineElmHashEntry*)current_entry->Suc();
	m_next = current ? (LoadInlineElm*)current->Suc() : NULL;

	return current;
}

LoadInlineElm* LoadInlineElmHashIterator::Next()
{
	LoadInlineElm* current = m_next;
	if (!current && m_next_entry)
	{
		current = (LoadInlineElm*)m_next_entry->m_list.First();
		m_next_entry = (LoadInlineElmHashEntry*)m_next_entry->Suc();
	}
	m_next = current ? (LoadInlineElm*)current->Suc() : NULL;
	return current;
}
