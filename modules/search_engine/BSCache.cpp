/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE

#include "modules/search_engine/BSCache.h"
#include "modules/pi/OpSystemInfo.h"

BSCache::BSCache(int max_cache) : m_storage()
{
	m_head = NULL;
	m_memory_id = 1;
	m_branch_count = 0;
	m_cache_count = 0;
	m_journal_count = 0;
	m_flush_mode = ReleaseNUR;
	m_max_cache = max_cache;

	m_NUR_mark = 0;
	m_journal_flushed = FALSE;

#ifdef _DEBUG
	branches_read = 0;
	branches_created = 0;
	branches_cached = 0;
	branches_written = 0;
	flush_count = 0;
#endif
}

BSCache::~BSCache(void)
{
	Item *b;

	OP_ASSERT(m_head == NULL);
	while (m_head != NULL)
	{
		b = m_head->previous;
		OP_DELETE(m_head);
		m_head = b;
	}
}

OP_STATUS BSCache::Flush(ReleaseSeverity severity, int max_ms)
{
	Item *b, *next, *tmp;
	BSCache::Item::DiskId new_id;
	double time_limit;
	int counter;

#ifdef _DEBUG
	++flush_count;
#endif

	time_limit = g_op_time_info->GetWallClockMS() + max_ms;

	m_journal_count = 0;

	if (m_storage.InTransaction() && !m_journal_flushed)
	{
		counter = 0;
		b = m_head;
		while (b != NULL)
		{
			++m_journal_count;

			if (!b->journalled)
			{
				if (b->modified && b->disk_id > 0)
				{
					RETURN_IF_ERROR(m_storage.PreJournal(((OpFileLength)b->disk_id) * m_storage.GetBlockSize()));
					b->journalled = TRUE;
				}
				else if (b->disk_id == 0 && b->deleted_id > 0)
				{
					RETURN_IF_ERROR(m_storage.PreJournal(((OpFileLength)b->deleted_id) * m_storage.GetBlockSize()));
					b->journalled = TRUE;
				}
				else if (b->modified && b->disk_id < 0 && severity == JournalAll)
				{
					new_id = (BSCache::Item::DiskId)(m_storage.Reserve() / m_storage.GetBlockSize());

					if (new_id == 0)
						return OpStatus::ERR_NO_DISK;

					b->id_reserved = TRUE;
					b->modified = TRUE;

					b->OnIdChange(new_id, b->disk_id);

					b->disk_id = new_id;

					b->journalled = TRUE;
				}

				if (counter < 9)
					++counter;
				else
				{
					counter = 0;
					if (max_ms > 0 && g_op_time_info->GetWallClockMS() >= time_limit)
						return OpBoolean::IS_FALSE;
				}
			}

			b = b->previous;
		}

		if (severity == JournalOnly || severity == JournalAll)
			return OpBoolean::IS_TRUE;

		RETURN_IF_ERROR(m_storage.FlushJournal());
		m_journal_flushed = TRUE;
	}

	counter = 0;
	next = NULL;
	b = m_head;
	while (b != NULL)
	{
		if (b->modified)
		{
			RETURN_IF_ERROR(b->Flush(&m_storage));

			if (b->journalled)
			{
				b->journalled = FALSE;
				--m_journal_count;
			}
#ifdef _DEBUG
			++branches_written;
#endif
		}
		if (b->disk_id == 0 && b->deleted_id > 0)
		{
			if (b->id_reserved)
			{
				if (!m_storage.Write(b, 0, (OpFileLength)(((OpFileLength)b->deleted_id) * m_storage.GetBlockSize())))
					return OpStatus::ERR_NO_DISK;
				b->id_reserved = FALSE;
			}

			if (b->journalled)
			{
				b->journalled = FALSE;
				--m_journal_count;
			}

			if (!m_storage.Delete(((OpFileLength)b->deleted_id) * m_storage.GetBlockSize()))
				return OpStatus::ERR_NO_DISK;
			b->deleted_id = 0;
		}

		tmp = b->previous;

		if (b->reference_count == 0 &&
			(b->disk_id == 0 || severity == ReleaseAll ||
			(severity == ReleaseNUR && b->NUR_mark != m_NUR_mark && b->NUR_mark != ((m_NUR_mark + NUR_MAX) & NUR_MASK))))
		{
			if (next == NULL)
				m_head = b->previous;
			else
				next->previous = b->previous;

			--m_cache_count;
			--m_branch_count;

			OP_DELETE(b);
		}
		else
			next = b;

		if (counter < 9)
			++counter;
		else
		{
			counter = 0;
			if (max_ms > 0 && g_op_time_info->GetWallClockMS() >= time_limit)
				return OpBoolean::IS_FALSE;
		}

		b = tmp;
	}

	m_journal_count = 0;

	m_memory_id = 1;

	m_journal_flushed = FALSE;

	return OpBoolean::IS_TRUE;
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t BSCache::EstimateMemoryUsed(void) const
{
	size_t sum = 0;
	Item *b;

	for (b=m_head; b; b=b->previous)
		sum += b->EstimateMemoryUsed() + 2*sizeof(size_t);
	
	return sum +
		sizeof(m_head) +
		sizeof(m_branch_count) +
		sizeof(m_cache_count) +
		sizeof(m_journal_count) +
		sizeof(m_max_cache) +
		m_storage.EstimateMemoryUsed() +
		sizeof(m_memory_id) +
		sizeof(m_NUR_mark) +
		sizeof(m_flush_mode) +
		sizeof(m_journal_flushed);
}

size_t BSCache::Item::EstimateMemoryUsed() const
{
	return
		sizeof(disk_id) +
		sizeof(previous) +
		sizeof(modified) +
		sizeof(in_list) +
		sizeof(journalled) +
		sizeof(deleted_id) +
		sizeof(reference_count) +
		sizeof(NUR_mark) +
		sizeof(id_reserved);
}
#endif

void BSCache::ClearCache(void)
{
	Item *b;

	while (m_head != NULL)
	{
		OP_ASSERT(m_head->reference_count == 0);
		b = m_head->previous;
		OP_DELETE(m_head);
		m_head = b;
	}

	m_branch_count = 0;
	m_cache_count = 0;
	m_journal_count = 0;
	m_memory_id = 1;
}

OP_STATUS BSCache::Create(Item **t, Item *reference, int ref_node)
{
	int journal_dec;

	journal_dec = m_flush_mode == JournalOnly ? m_journal_count : 0;

	if (m_cache_count - journal_dec > m_max_cache || (m_branch_count - journal_dec > m_max_cache * 2 && m_cache_count > 0))
	{
		RETURN_IF_ERROR(Flush(m_flush_mode));

		if (m_cache_count > m_max_cache || (m_branch_count > m_max_cache * 2 && m_cache_count > 0))
		{
			m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;
		}
	}

	if (reference != NULL && !reference->in_list)
	{
		reference->previous = m_head;
		m_head = reference;
		reference->in_list = TRUE;
	}

	if ((*t = NewMemoryItem((int)m_memory_id, reference, ref_node, m_NUR_mark)) == NULL)
	{
		if (m_cache_count > 0)
		{
			OP_STATUS err;

			if (OpStatus::IsError(err = Flush(ReleaseAll)))
			{
				OP_DELETE(*t);
				return err;
			}

			if ((*t = NewMemoryItem((int)m_memory_id, reference, ref_node, m_NUR_mark)) == NULL)
				return OpStatus::ERR_NO_MEMORY;
		}
		else return OpStatus::ERR_NO_MEMORY;
	}

	(*t)->previous = m_head;
	m_head = *t;
	(*t)->in_list = TRUE;
	++m_memory_id;
	(*t)->reference_count = 1;

#ifdef _DEBUG
	++branches_created;
#endif

	++m_branch_count;

	return OpStatus::OK;
}

void BSCache::Unlink(Item *t)
{
	t->modified = FALSE;  // will be deleted on the next Flush()
	t->deleted_id = t->disk_id;
	t->disk_id = 0;
}

OP_STATUS BSCache::Load(Item **t, OpFileLength id)
{
	Item *tmp;
	OP_STATUS err;
	int journal_dec;

	journal_dec = m_flush_mode == JournalOnly ? m_journal_count : 0;

	for (tmp = m_head; tmp != NULL; tmp = tmp->previous)
		if (tmp->disk_id == (BSCache::Item::DiskId)id)
		{
#ifdef _DEBUG
			++branches_cached;
#endif
			if (tmp->reference_count == 0)
				--m_cache_count;
			++(tmp->reference_count);
			tmp->NUR_mark = m_NUR_mark;
			*t = tmp;
			return OpStatus::OK;
		}

	if (m_cache_count - journal_dec > m_max_cache || (m_branch_count - journal_dec > m_max_cache * 2 && m_cache_count > 0))
	{
		RETURN_IF_ERROR(Flush(m_flush_mode));

		if (m_cache_count > m_max_cache || (m_branch_count > m_max_cache * 2 && m_cache_count > 0))
		{
			m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;
		}
	}

	if ((*t = NewDiskItem(id, m_NUR_mark)) == NULL)
	{
		if (m_cache_count > 0)
		{
			if (OpStatus::IsError(err = Flush(ReleaseAll)))
			{
				OP_DELETE(*t);
				return err;
			}

			if ((*t = NewDiskItem(id, m_NUR_mark)) == NULL)
				return OpStatus::ERR_NO_MEMORY;
		}
		else return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsError(err = (*t)->Read(&m_storage)))
	{
		OP_DELETE(*t);
		*t = NULL;
		return err;
	}

	(*t)->previous = m_head;
	m_head = *t;
	(*t)->in_list = TRUE;
	++m_memory_id;
	(*t)->reference_count = 1;

#ifdef _DEBUG
	++branches_read;
#endif

	++m_branch_count;

	return OpStatus::OK;
}

void BSCache::Load(Item **t, Item *b)
{
	*t = b;

#ifdef _DEBUG
	++branches_cached;
#endif

	OP_ASSERT((*t)->disk_id != 0);  // deleted branch

	if ((*t)->reference_count == 0)
		--m_cache_count;

	++((*t)->reference_count);
	(*t)->NUR_mark = m_NUR_mark;
}

void BSCache::Release(Item *t)
{
	OP_ASSERT(t->reference_count > 0);

	if (--(t->reference_count) > 0)
		return;

	if (t->reference_count < 0)
	{
		// just to fix what must never happen
		t->reference_count = 0;
	}

	++m_cache_count;

	if (!t->in_list)
	{
		t->previous = m_head;
		t->in_list = TRUE;
		m_head = t;
	}
}

#endif  // SEARCH_ENGINE

