/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
  
#include "core/pch.h" 
 
#ifdef M2_SUPPORT 

#include "adjunct/m2/src/engine/index/indexgroup.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"

/****************************************************************************
**							IndexGroup
****************************************************************************/

IndexGroup::IndexGroup(index_gid_t index_id, Index* index_to_use)
: m_index(index_to_use)
, m_index_id(0)
, m_base(NULL)
, m_base_id(0)
, m_indexer(NULL)
{
	m_indexer = MessageEngine::GetInstance()->GetIndexer();
	m_indexer->AddIndexerListener(this);

	if (!m_index)
	{
		if (!(m_index = new Index()))
			return;
		
		m_index->SetType(IndexTypes::FOLDER_INDEX);
		m_index->SetVisible(FALSE);
		m_index->SetId(index_id);
		m_indexer->NewIndex(m_index);
	}

	m_index_id = m_index->GetId();

	OP_ASSERT(m_index_id != 0);
}

IndexGroup::~IndexGroup()
{
	Empty();

	m_indexer->RemoveIndexerListener(this);

	if (m_index)
	{
		m_index->RemoveObserver(this);
	}

	if (m_base)
	{
		m_base->RemoveObserver(this);
	}
}

void IndexGroup::Empty()
{
	for (unsigned i = 0; i < m_indexes.GetCount(); i++)
	{
		Index* index = m_indexer->GetIndexById(m_indexes.GetByIndex(i));
		if (index)
			index->RemoveObserver(this);
	}
	m_indexes.Clear();
	
	if (m_index)
	{
		m_index->Empty();
	}
}

OP_STATUS IndexGroup::SetBase(UINT32 index_id)
{
	m_base_id = index_id;
	m_base = m_indexer->GetIndexById(index_id);
	
	if (m_base)
	{
		RETURN_IF_ERROR(m_base->PreFetch());

		for (INT32SetIterator it(m_base->GetIterator()); it; it++)
		{
			RETURN_IF_ERROR(m_index->NewMessage(it.GetData()));
		}
		return m_base->AddObserver(this);
	}
	return OpStatus::OK;
}

OP_STATUS IndexGroup::IndexAdded(Indexer *indexer, UINT32 index_id)
{
	if (m_base_id == index_id)
	{
		m_base = m_indexer->GetIndexById(index_id);
		return m_base->AddObserver(this);
	}
	else if (m_indexes.Contains(index_id))
	{
		Index* index = m_indexer->GetIndexById(index_id);
		return index->AddObserver(this);
	}

	return OpStatus::OK;
}

OP_STATUS IndexGroup::IndexRemoved(Indexer *indexer, UINT32 index_id)
{
	if (index_id == m_index_id)
	{
		m_index = NULL;
	}
	else if (m_base_id == index_id || m_indexes.Contains(index_id))
	{
		if (m_base_id == index_id)
		{
			m_base_id = 0;
			m_base = NULL;
		}

		// process messages that are now removed
		if (m_index)
		{
			m_indexes.Remove(index_id);

			Index* removed_index = m_indexer->GetIndexById(index_id);
			if (removed_index)
			{
				for (INT32SetIterator it(removed_index->GetIterator()); it; it++)
					MessageRemoved(removed_index, it.GetData(), FALSE);
			}
			m_index->ResetUnreadCount();
		}
	}

	return OpStatus::OK;
}

OP_STATUS IndexGroup::AddIndexWithoutAddingMessages(index_gid_t index_id)
{
	if (m_indexes.Contains(index_id))
		return OpStatus::OK;

	RETURN_IF_ERROR(m_indexes.Insert(index_id));

	Index* index = m_indexer->GetIndexById(index_id);
	if (index)
	{
		RETURN_IF_ERROR(index->AddObserver(this));
	}

	return OpStatus::OK;
}

OP_STATUS IndexGroup::SetBaseWithoutAddingMessages(index_gid_t index_id)
{
	m_base_id = index_id;
	m_base = m_indexer->GetIndexById(index_id);
	return m_base ? m_base->AddObserver(this) : OpStatus::OK;
}


/****************************************************************************
**							UnionIndexGroup
****************************************************************************/

OP_STATUS UnionIndexGroup::AddIndex(index_gid_t index_id)
{
	if (m_indexes.Contains(index_id))
		return OpStatus::OK;

	RETURN_IF_ERROR(m_indexes.Insert(index_id));

	Index* index = m_indexer->GetIndexById(index_id);

	if (index)
	{
		index->PreFetch();
		for (INT32SetIterator it(index->GetIterator()); it; it++)
		{
			RETURN_IF_ERROR(m_index->NewMessage(it.GetData()));
		}

		index->AddObserver(this);

		m_index->ResetUnreadCount();
	}

	return OpStatus::OK;
}

OP_STATUS UnionIndexGroup::MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword)
{
	if (m_indexes.Contains(index->GetId()))
	{
		return m_index->NewMessage(message);
	}

	return OpStatus::OK;
}

OP_STATUS UnionIndexGroup::MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword)
{
	for (unsigned i = 0; i < m_indexes.GetCount(); i++)
	{
		Index* index = m_indexer->GetIndexById(m_indexes.GetByIndex(i));
		if (index && index->Contains(message))
		{
			return OpStatus::OK;
		}
	}
	return m_index->RemoveMessage(message);
}

/****************************************************************************
**							ComplementIndexGroup
****************************************************************************/

OP_STATUS ComplementIndexGroup::AddIndex(index_gid_t index_id)
{
	if (m_indexes.Contains(index_id))
		return OpStatus::OK;

	Index* index = m_indexer->GetIndexById(index_id);
	if (index)
	{
		RETURN_IF_ERROR(index->PreFetch());
		
		OpINT32Vector all_messages;
		RETURN_IF_ERROR(m_index->GetAllMessages(all_messages));
		for (UINT32 i = 0; i < all_messages.GetCount(); i++)
		{
			if (index->Contains(all_messages.Get(i)))
			{
				RETURN_IF_ERROR(m_index->RemoveMessage(all_messages.Get(i)));
			}
		}
		
		RETURN_IF_ERROR(m_indexes.Insert(index_id));
		index->AddObserver(this);
	}

	return OpStatus::OK;
}

OP_STATUS ComplementIndexGroup::MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword)
{
	if (m_index->Contains(message) && m_indexes.Contains(index->GetId()))
	{
		return m_index->RemoveMessage(message);
	}
	else if (index == m_base)
	{
		for (unsigned i = 0; i < m_indexes.GetCount(); i++)
		{
			Index * index = m_indexer->GetIndexById(m_indexes.GetByIndex(i));
			if (index && index->Contains(message))
				return m_index->RemoveMessage(message);
		}
		return m_index->NewMessage(message);
	}

	return OpStatus::OK;
}

OP_STATUS ComplementIndexGroup::MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword)
{
	if (index == m_base)
	{
		return m_index->RemoveMessage(message);
	}
	else if (m_base && m_base->Contains(message))
	{
		for (unsigned i = 0; i < m_indexes.GetCount(); i++)
		{
			Index * index = m_indexer->GetIndexById(m_indexes.GetByIndex(i));
			if (index && index->Contains(message))
				return m_index->RemoveMessage(message);
		}
		return m_index->NewMessage(message);
	}

	return OpStatus::OK;
}

/****************************************************************************
**							IntersectionIndexGroup
****************************************************************************/

OP_STATUS IntersectionIndexGroup::AddIndex(index_gid_t index_id)
{
	if (m_indexes.Contains(index_id))
		return OpStatus::OK;

	Index* index = m_indexer->GetIndexById(index_id);

	if (index)
	{
		RETURN_IF_ERROR(index->PreFetch());
		
		OpINT32Vector all_messages;
		RETURN_IF_ERROR(m_index->GetAllMessages(all_messages));
		for (UINT32 i = 0; i < all_messages.GetCount(); i++)
		{
			if (!index->Contains(all_messages.Get(i)))
			{
				RETURN_IF_ERROR(m_index->RemoveMessage(all_messages.Get(i)));
			}
		}

		RETURN_IF_ERROR(m_indexes.Insert(index_id));
		RETURN_IF_ERROR(index->AddObserver(this));
	}

	return OpStatus::OK;
}

OP_STATUS IntersectionIndexGroup::MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword)
{
	if (index == m_base || m_indexes.Contains(index->GetId()))
	{
		if (m_base->Contains(message))
		{
			for (unsigned i = 0; i < m_indexes.GetCount(); i++)
			{
				Index * index = m_indexer->GetIndexById(m_indexes.GetByIndex(i));
				if (index && !index->Contains(message))
					return OpStatus::OK;
			}
			return m_index->NewMessage(message);
		}
	}
	return OpStatus::OK;
}

OP_STATUS IntersectionIndexGroup::MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword)
{
	if (index == m_base || m_indexes.Contains(index->GetId()))
		return m_index->RemoveMessage(message);
	else
		return OpStatus::OK;
}

/****************************************************************************
**							IndexGroupRange
****************************************************************************/

IndexGroupRange::IndexGroupRange(index_gid_t index_id, index_gid_t range_start, index_gid_t range_end)
	: UnionIndexGroup(index_id)
	, m_or_range_start(range_start)
	, m_or_range_end(range_end)
{
	// Or all indexes currently in the range
	INT32 it = -1;
	Index* index;
	while ((index = m_indexer->GetRange(range_start, range_end, it)) != NULL)
	{
		AddIndex(index->GetId());
	}
}

OP_STATUS IndexGroupRange::IndexAdded(Indexer *indexer, UINT32 index_id)
{
	if (m_or_range_start <= index_id && index_id < m_or_range_end)
	{
		return AddIndex(index_id);
	}
	
	return OpStatus::OK;
}


/****************************************************************************
**							IndexGroupWatched
****************************************************************************/

IndexGroupWatched::IndexGroupWatched(index_gid_t index_id, index_gid_t range_start, index_gid_t range_end)
	: UnionIndexGroup(index_id)
	, m_or_range_start(range_start)
	, m_or_range_end(range_end)
{
	// Or all indexes currently in the range that are being watched
	INT32 it = -1;
	Index* index = m_indexer->GetRange(range_start, range_end, it);

	while (index)
	{
		if (index->IsWatched())
		{
			AddIndex(index->GetId());
		}
		index = m_indexer->GetRange(range_start, range_end, it);
	}
}

OP_STATUS IndexGroupWatched::IndexAdded(Indexer *indexer, UINT32 index_id)
{
	if (m_or_range_start <= index_id && index_id < m_or_range_end)
	{
		Index* index = indexer->GetIndexById(index_id);

		if (index && index->IsWatched())
		{
			return AddIndex(index_id);
		}
	}

	return OpStatus::OK;
}

OP_STATUS IndexGroupWatched::IndexVisibleChanged(Indexer *indexer, UINT32 index_id)
{
	if (m_or_range_start <= index_id && index_id < m_or_range_end)
	{
		Index* index = indexer->GetIndexById(index_id);
		if (index)
		{
			if (index->IsWatched())
			{
				return AddIndex(index_id);
			}
			else if (m_indexes.Contains(index_id))
				return IndexRemoved(indexer, index_id);
		}
	}
	
	return OpStatus::OK;
}

/****************************************************************************
**							IndexGroupMailingLists
****************************************************************************/

IndexGroupMailingLists::IndexGroupMailingLists(index_gid_t index_id)
	: UnionIndexGroup(index_id)
{
	// Check all indexes that might be mailing lists
	INT32 it = -1;
	Index* index = m_indexer->GetRange(IndexTypes::FIRST_CONTACT, IndexTypes::LAST_CONTACT, it);

	while (index)
	{
		IndexSearch *search = index->GetSearch();
		if (search && search->GetSearchText().FindFirstOf('@') == KNotFound &&
			search->GetSearchText().FindFirstOf('.') != KNotFound)
		{
			AddIndex(index->GetId());
		}
		index = m_indexer->GetRange(IndexTypes::FIRST_CONTACT, IndexTypes::LAST_CONTACT, it);
	}
}

OP_STATUS IndexGroupMailingLists::IndexAdded(Indexer *indexer, UINT32 index_id)
{
	if (IndexTypes::FIRST_CONTACT <= index_id && index_id < IndexTypes::LAST_CONTACT)
	{
		Index* index = indexer->GetIndexById(index_id);
		if (index)
		{
			IndexSearch *search = index->GetSearch();
			if (search &&
				search->GetSearchText().FindFirstOf('@') == KNotFound &&
				search->GetSearchText().FindFirstOf('.') != KNotFound)
			{
				return AddIndex(index_id);
			}
		}
	}
	
	return OpStatus::OK;
}

/****************************************************************************
**							FolderIndexGroup
****************************************************************************/

FolderIndexGroup::FolderIndexGroup(index_gid_t index_id) 
: UnionIndexGroup(index_id)
{
	m_index->SetType(IndexTypes::UNIONGROUP_INDEX);

	OpINT32Vector children;
	m_indexer->GetChildren(index_id, children);
	for (UINT32 i = 0; i < children.GetCount(); i++)
	{
		AddIndex(children.Get(i));
	}
}

OP_STATUS FolderIndexGroup::IndexParentIdChanged(Indexer *indexer, UINT32 index_id, UINT32 old_parent_id, UINT32 new_parent_id)
{
	if (new_parent_id == m_index_id)
	{
		return AddIndex(index_id);
	}
	else if (old_parent_id == m_index_id)
	{
		return IndexRemoved(indexer, index_id);
	}
	return OpStatus::OK;
}

#endif // M2_SUPPORT
