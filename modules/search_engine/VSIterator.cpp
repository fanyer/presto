/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef VISITED_PAGES_SEARCH

#include "modules/search_engine/VisitedSearch.h"
#include "modules/search_engine/VSIterator.h"
#include "modules/search_engine/VSUtil.h"
#include "modules/search_engine/UniCompressor.h"
#include "modules/hardcore/opera/opera.h"


/*****************************************************************************
 AllDocIterator */

OP_STATUS AllDocIterator::Init(void)
{
	Next();

	if (m_result.GetCount() > 0)
		m_result_pos = 0;

	return m_status;
}

BOOL AllDocIterator::Next(void)
{
	SearchIterator<IdTime> *it;
	VisitedSearch::Result row;

	m_status = OpStatus::OK;

	if ((unsigned)(m_result_pos + 1) < m_result.GetCount())
	{
		++m_result_pos;
		return TRUE;
	}

	if (m_result.GetCount() == 0)
		it = m_index->m_alldoc.SearchFirst();
	else
		it = m_index->m_alldoc.Search(IdTime(m_result[m_result.GetCount() - 1].visited, m_result[m_result.GetCount() - 1].id), operatorGT);

	if (it == NULL)
	{
		OP_ASSERT(0);  // this was more probably a search error than OOM
		m_status = OpStatus::ERR_NO_MEMORY;
		return FALSE;
	}

	if (it->End())
	{
		if ((unsigned)m_result_pos < m_result.GetCount())
			++m_result_pos;
		OP_DELETE(it);
		return FALSE;
	}

	if (m_result.GetSize() == m_result.GetCount())
	{
		if (OpStatus::IsError(m_result.Reserve(m_result.GetSize() + 20)))
		{
			OP_DELETE(it);
			return FALSE;
		}
	}

	do {
		row.id = it->Get().data[IDTIME_ID];
		row.ranking = 1.0;

		OP_ASSERT(row.id != 0);
		
		if (OpStatus::IsError(m_status = VisitedSearch::Result::ReadResult(row, &(m_index->m_metadata))))
		{
			OP_DELETE(it);
			return FALSE;
		}

		if (row.invalid || row.next != 0)
			VisitedSearch::Result::DeleteResult(&row);
		else
			if (OpStatus::IsError(m_status = m_result.Add(row)))
			{
				OP_DELETE(it);
				return FALSE;
			}
	} while (m_result.GetSize() > m_result.GetCount() && it->Next());

	if (OpStatus::IsError(m_status = it->Error()))
	{
		OP_DELETE(it);
		return FALSE;
	}

	OP_DELETE(it);

	++m_result_pos;

	if (m_result_pos == 0)
	{
		if (m_result.GetCount() <= 1)
			return FALSE;

		++m_result_pos;
	}

	return m_result_pos < (int)m_result.GetCount();
}

BOOL AllDocIterator::Prev(void)
{
	if (m_result_pos <= 0)
	{
		if (m_result_pos == 0)
			--m_result_pos;
		return FALSE;
	}

	--m_result_pos;
	return TRUE;
}


/*****************************************************************************
 RankIterator */

OP_STATUS RankIterator::AddWord(UINT32 pos)
{
	OpFileLength file_pos;
	int size;
	TVector<RankId> *word_rank;
	TVector<RankId> *word_id;
	OP_STATUS err;

	if (pos == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_rank_vec.Reserve(m_rank_vec.GetCount() + 1));
	RETURN_IF_ERROR(m_id_vec.Reserve(m_id_vec.GetCount() + 1));

	if ((word_rank = OP_NEW(TVector<RankId>, (&RankId::CompareRank))) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if ((word_id = OP_NEW(TVector<RankId>, (&RankId::CompareId))) == NULL)
	{
		OP_DELETE(word_rank);
		return OpStatus::ERR_NO_MEMORY;
	}

	file_pos = ((OpFileLength)pos) * m_index->m_wordbag.GetBlockSize();
	size = m_index->m_wordbag.DataLength(file_pos) / sizeof(RankId);

	if (size == 0)
	{
		if (file_pos >= m_index->m_wordbag.GetFileSize())
			err = OpStatus::ERR_OUT_OF_RANGE;
		else {
			OP_ASSERT(0);  // empty vector for this word? impossible! really.
			err = OpStatus::ERR;
		}
		goto cleanup;
	}

	if (OpStatus::IsError(err = word_rank->SetCount(size)))
		goto cleanup;

	if (OpStatus::IsError(err = word_id->Reserve(size)))
		goto cleanup;

	if (!m_index->m_wordbag.ReadApnd(word_rank->Ptr(), size * sizeof(RankId), file_pos))
	{  // read error from disk?
		err = OpStatus::ERR;
		goto cleanup;
	}

	if (OpStatus::IsError(err = word_id->DuplicateOf(*word_rank)))
		goto cleanup;

	if (OpStatus::IsError(err = word_rank->Sort()))
		goto cleanup;
	if (OpStatus::IsError(err = word_id->Sort()))
		goto cleanup;

	if (OpStatus::IsError(err = m_rank_vec.Add(word_rank)))
		goto cleanup;
	if (OpStatus::IsError(err = m_id_vec.Add(word_id)))
	{
		m_rank_vec.RemoveByItem(word_rank);
		goto cleanup;
	}

	if (m_max_line > (unsigned)size)
		m_max_line = size;

	return OpStatus::OK;

cleanup:
	OP_DELETE(word_id);
	OP_DELETE(word_rank);
	return err;
}

BOOL RankIterator::Next(void)
{
	int i, j;
	unsigned idx;
	float rank;
	VisitedSearch::Result res_val;

	m_status = OpStatus::OK;

	// no words
	if (m_rank_vec.GetCount() == 0)
		return AllDocIterator::Next();

	if (m_line >= m_max_line)
	{
		++m_result_pos;
		return (unsigned)m_result_pos < m_result.GetCount();
	}

	if ((unsigned)(m_result_pos + 1) < m_result.GetCount())
	{
		rank = 0.0;
		for (j = m_rank_vec.GetCount() - 1; j >= 0 ; --j)
			rank += m_rank_vec[j]->Get(m_line).rank;

		rank /= m_rank_vec.GetCount();

		if (rank > m_result[m_result_pos + 1].ranking)  // cannot get better results
		{
			++m_result_pos;
			return TRUE;
		}
	}

	idx = m_result.GetCount() + 100 + m_rank_vec.GetCount();
	RETURN_VALUE_IF_ERROR(m_status = m_result.Reserve(idx <= m_max_line * m_rank_vec.GetCount() ? idx : m_max_line * m_rank_vec.GetCount()), FALSE);

	while (m_line < m_max_line)
	{
		for (i = m_rank_vec.GetCount() - 1; i >= 0; --i)
		{
			rank = m_rank_vec[i]->Get(m_line).rank;
			for (j = m_id_vec.GetCount() - 1; j >= 0 ; --j)
			{
				if (j == i)
					continue;

				idx = m_id_vec[j]->Search(m_rank_vec[i]->Get(m_line));
				if (idx >= m_id_vec[j]->GetCount() || m_id_vec[j]->Get(idx).data[RANKID_ID] != m_rank_vec[i]->Get(m_line).data[RANKID_ID])
					break;

				rank += m_id_vec[j]->Get(idx).rank;
			}

			if (j >= 0)
				continue;

			rank /= m_rank_vec.GetCount();

			res_val.id = m_rank_vec[i]->Get(m_line).data[RANKID_ID];
			res_val.ranking = rank;

			idx = m_result.Search(res_val);
			if (idx < m_result.GetCount() &&
				(m_result[idx].id == m_rank_vec[i]->Get(m_line).data[RANKID_ID] ||
				(idx > 0 && m_result[idx - 1].id == m_rank_vec[i]->Get(m_line).data[RANKID_ID]) ||  // can be one off due to rounding of the float number
				(idx < m_result.GetCount() - 1 && m_result[idx + 1].id == m_rank_vec[i]->Get(m_line).data[RANKID_ID])))
				continue;

			RETURN_VALUE_IF_ERROR(m_status = VisitedSearch::Result::ReadResult(res_val, &(m_index->m_metadata)), FALSE);

			if (res_val.invalid || res_val.next != 0)
				VisitedSearch::Result::DeleteResult(&res_val);
			else
				RETURN_VALUE_IF_ERROR(m_result.Insert(idx, res_val), FALSE);
		}

		++m_line;

		if (m_result.GetCount() > (unsigned)(m_result_pos + 1) && m_line < m_max_line)
		{
			rank = 0.0;
			for (j = m_rank_vec.GetCount() - 1; j >= 0 ; --j)
				rank += m_rank_vec[j]->Get(m_line).rank;

			rank /= m_rank_vec.GetCount();

			if (rank > m_result[m_result_pos + 1].ranking)  // cannot get better results
				break;
		}

		if (m_result.GetCount() >= m_result.GetSize() - m_rank_vec.GetCount())  // allways keep enough place for a complete line
		{
			idx = m_result.GetCount() + 100 + m_rank_vec.GetCount();
			RETURN_VALUE_IF_ERROR(m_status = m_result.Reserve(idx <= m_max_line * m_rank_vec.GetCount() ? idx : m_max_line * m_rank_vec.GetCount()), FALSE);
		}
	}

	++m_result_pos;

	return m_line < m_max_line || (unsigned)m_result_pos < m_result.GetCount();
}

/*****************************************************************************
 TimeIterator */

OP_STATUS TimeIterator::AddWord(UINT32 pos)
{
	OpFileLength file_pos;
	int size;
	TVector<RankId> *word_id;
	OP_STATUS err;

	if (pos == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_id_vec.Reserve(m_id_vec.GetCount() + 1));

	if ((word_id = OP_NEW(TVector<RankId>, (&RankId::CompareId))) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	file_pos = ((OpFileLength)pos) * m_index->m_wordbag.GetBlockSize();
	size = m_index->m_wordbag.DataLength(file_pos) / sizeof(RankId);

	if (size == 0)
	{
		OP_DELETE(word_id);
		if (file_pos >= m_index->m_wordbag.GetFileSize())
			return OpStatus::ERR_OUT_OF_RANGE;
		else {
			OP_ASSERT(0);  // empty vector for this word? impossible! really.
			return OpStatus::ERR;
		}
	}

	if (OpStatus::IsError(err = word_id->SetCount(size)))
	{
		OP_DELETE(word_id);
		return err;
	}

	if (!m_index->m_wordbag.ReadApnd(word_id->Ptr(), size * sizeof(RankId), file_pos))
	{  // read error from disk?
		OP_DELETE(word_id);
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(err = word_id->Sort()) ||
		OpStatus::IsError(err = m_id_vec.Add(word_id)))
	{
		OP_DELETE(word_id);
		return err;
	}

	return OpStatus::OK;
}

BOOL TimeIterator::Next(void)
{
	int j;
	unsigned idx;
	float rank;
	SearchIterator<IdTime> *it;
	VisitedSearch::Result row;
	RankId tmp_srch;

	m_status = OpStatus::OK;

	// no words
	if (m_id_vec.GetCount() == 0)
		return AllDocIterator::Next();

	if ((unsigned)(m_result_pos + 1) < m_result.GetCount())
	{
		++m_result_pos;
		return TRUE;
	}

	if (m_result_pos != -1 && (unsigned)m_result_pos >= m_result.GetCount())  // end already reached
		return FALSE;

	if (m_result.GetCount() == 0)
		it = m_index->m_alldoc.SearchFirst();
	else
		it = m_index->m_alldoc.Search(IdTime(m_result[m_result.GetCount() - 1].visited, m_result[m_result.GetCount() - 1].id), operatorGT);

	if (it == NULL)
	{
		OP_ASSERT(0);  // more probably a search error
		m_status = OpStatus::ERR_NO_MEMORY;
		return FALSE;
	}

	if (it->End())
	{
		if ((unsigned)m_result_pos < m_result.GetCount())
			++m_result_pos;
		OP_DELETE(it);
		return FALSE;
	}

	if (m_result.GetSize() == m_result.GetCount())
	{
		if (OpStatus::IsError(m_result.Reserve(m_result.GetSize() + 20)))
		{
			OP_DELETE(it);
			return FALSE;
		}
	}

	do {
		rank = 0.0;
		tmp_srch.data[RANKID_ID] = it->Get().data[RANKID_ID];

		for (j = m_id_vec.GetCount() - 1; j >= 0 ; --j)
		{
			idx = m_id_vec[j]->Search(tmp_srch);
			if (idx >= m_id_vec[j]->GetCount() || m_id_vec[j]->Get(idx).data[RANKID_ID] != tmp_srch.data[RANKID_ID])
				break;

			rank += m_id_vec[j]->Get(idx).rank;
		}

		if (j >= 0)
			continue;

		row.id = it->Get().data[IDTIME_ID];
		row.ranking = rank / m_id_vec.GetCount();

		if (OpStatus::IsError(m_status = VisitedSearch::Result::ReadResult(row, &(m_index->m_metadata))))
		{
			OP_DELETE(it);
			return FALSE;
		}

		if (row.invalid || row.next != 0)
			VisitedSearch::Result::DeleteResult(&row);
		else if (OpStatus::IsError(m_status = m_result.Add(row)))
		{
			VisitedSearch::Result::DeleteResult(&row);
			OP_DELETE(it);
			return FALSE;
		}
	} while (m_result.GetSize() > m_result.GetCount() && it->Next());

	if (OpStatus::IsError(m_status = it->Error()))
	{
		OP_DELETE(it);
		return FALSE;
	}

	OP_DELETE(it);

	++m_result_pos;

	if (m_result_pos == 0)
	{
		if (m_result.GetCount() <= 1)
			return FALSE;

		++m_result_pos;
	}

	return m_result_pos < (int)m_result.GetCount();
}


/*****************************************************************************
 MultiOrIterator */

BOOL MultiOrIterator::Next(void)
{
	int i, j, m;
	BOOL end;

	i = 0;
	while (i < (int)subindex.GetCount() && subindex[i]->End())
	{
		RETURN_VALUE_IF_ERROR(subindex[i]->Error(), FALSE);
		++i;
	}

	if (i >= (int)subindex.GetCount() || OpStatus::IsError(subindex[i]->Error()))
		return FALSE;

	end = FALSE;
	m = i;

	for (j = subindex.GetCount() - 1; j > m; --j)
	{
		RETURN_VALUE_IF_ERROR(subindex[j]->Error(), FALSE);

		if (!subindex[j]->End())
		{
			end = TRUE;
			if (Compare(&subindex[j]->Get(), &subindex[i]->Get()))
				i = j;
		}
	}

	return subindex[i]->Next() || end;
}

BOOL MultiOrIterator::Prev(void)
{
	int i, j, m;
	BOOL beginning;

	i = 0;
	while (i < (int)subindex.GetCount() && subindex[i]->End())
	{
		RETURN_VALUE_IF_ERROR(subindex[i]->Error(), FALSE);
		++i;
	}

	if (i >= (int)subindex.GetCount() || OpStatus::IsError(subindex[i]->Error()))
		return FALSE;

	beginning = FALSE;
	m = i;

	for (j = subindex.GetCount() - 1; j > m; --j)
	{
		RETURN_VALUE_IF_ERROR(subindex[j]->Error(), FALSE);

		if (!subindex[j]->End())
		{
			beginning = TRUE;
			if (Compare(&subindex[i]->Get(), &subindex[j]->Get()))
				i = j;
		}
	}

	return subindex[i]->Prev() || beginning;
}

const VisitedSearch::Result &MultiOrIterator::Get(void)
{
	int i, j, m;

	i = 0;
	while (i < (int)subindex.GetCount() && subindex[i]->End())
	{
		OP_ASSERT(OpStatus::IsSuccess(subindex[i]->Error()));
		++i;
	}

	if (i >= (int)subindex.GetCount()) {
		OP_ASSERT(0);
		return *(VisitedSearch::Result*)g_opera->search_engine_module.empty_visited_search_result;
	}

	OP_ASSERT(OpStatus::IsSuccess(subindex[i]->Error()));

	m = i;

	for (j = subindex.GetCount() - 1; j > m; --j)
	{
		OP_ASSERT(OpStatus::IsSuccess(subindex[j]->Error()));

		if (!subindex[j]->End())
			if (Compare(&subindex[j]->Get(), &subindex[i]->Get()))
				i = j;
	}

	return subindex[i]->Get();
}

OP_STATUS MultiOrIterator::Error(void) const
{
	register int i;

	for (i = subindex.GetCount() - 1; i >= 0; --i)
	{
		RETURN_IF_ERROR(subindex[i]->Error());
	}

	return OpStatus::OK;
}

int MultiOrIterator::Count(void) const
{
	int count, c, i;

	count = 0;
	for (i = subindex.GetCount() - 1; i >= 0; --i)
	{
		c = subindex[i]->Count();
		if (c == -1)
			return c;

		count += c;
	}

	return count;
}

BOOL MultiOrIterator::End(void) const
{
	int i;

	for (i = subindex.GetCount() - 1; i >= 0; --i)
		if (!subindex[i]->End())
			return FALSE;

	return TRUE;
}

BOOL MultiOrIterator::Beginning(void) const
{
	int i;

	for (i = subindex.GetCount() - 1; i >= 0; --i)
		if (!subindex[i]->Beginning())
			return FALSE;

	return TRUE;
}


/*****************************************************************************
 FastPrefixIterator */

FastPrefixIterator::~FastPrefixIterator(void)
{
	OP_DELETE(m_prefixes);
}

OP_STATUS FastPrefixIterator::AddWord(UINT32 u32_pos)
{
	OpAutoPtr< TVector<RankId> > wv;
	OpFileLength pos;

	pos = ((OpFileLength)u32_pos) * m_index->m_wordbag.GetBlockSize();

	wv.reset(OP_NEW(TVector<RankId>, (&RankId::CompareId)));
	if (wv.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(wv->Reserve(m_index->m_wordbag.DataLength(pos) / sizeof(RankId)));
	RETURN_IF_ERROR(wv->SetCount(wv->GetSize()));

	if (!m_index->m_wordbag.ReadApnd(wv->Ptr(), wv->GetCount() * sizeof(RankId), pos))
		return OpStatus::ERR;

	RETURN_IF_ERROR(wv->Sort());

	return m_full_words.Add(wv.release());
}

void FastPrefixIterator::SetPrefix(SearchIterator<ACT::PrefixResult> *prefix_iterator)
{
	OP_ASSERT(m_prefixes == NULL);
	m_prefixes = prefix_iterator;
	m_status = OpStatus::OK;
}

BOOL FastPrefixIterator::Next(void)
{
	int i, j, pos, rjct_pos;
	unsigned read_count;

	if (m_prefix_data_size < 0)
	{
		if (OpStatus::IsError(m_prefixes->Error()) || m_prefixes->End())
		{
			if (m_result_pos < (int)m_result.GetCount())
				m_result_pos = m_result.GetCount();
			return FALSE;
		}
	}

	++m_result_pos;
	read_count = m_current_prefix.GetCount();

	while (m_result_pos >= (int)m_result.GetCount())
	{
		if ((int)m_current_prefix.GetCount() >= m_prefix_data_size)
		{
			if (m_prefix_data_size >= 0 && !m_prefixes->Next())
				return FALSE;

			if ((m_prefix_data_size = m_index->m_wordbag.DataLength(((OpFileLength)m_prefixes->Get().id) * m_index->m_wordbag.GetBlockSize()) / sizeof(RankId)) == 0)
			{
				OP_ASSERT(0);  // no data for this word id?
				m_current_prefix.Clear();
				continue;
			}

			RETURN_VALUE_IF_ERROR(m_status = m_current_prefix.Reserve(16), FALSE);

			read_count = m_prefix_data_size > 16 ? 16 : m_prefix_data_size;
			RETURN_VALUE_IF_ERROR(m_status = m_current_prefix.SetCount(read_count), FALSE);
			if (!m_index->m_wordbag.ReadApnd(m_current_prefix.Ptr(), m_current_prefix.GetCount() * sizeof(RankId),
				((OpFileLength)m_prefixes->Get().id) * m_index->m_wordbag.GetBlockSize()))
			{
				m_status = OpStatus::ERR;
				return FALSE;
			}

		}
		else {
			read_count = m_prefix_data_size > (int)(read_count * 2) ? read_count * 2 : m_prefix_data_size;
			RETURN_VALUE_IF_ERROR(m_status = m_current_prefix.Reserve(read_count), FALSE);

			RETURN_VALUE_IF_ERROR(m_status = m_current_prefix.SetCount(read_count), FALSE);
			if (!m_index->m_wordbag.ReadApnd(m_current_prefix.Ptr(), m_current_prefix.GetCount() * sizeof(RankId),
				((OpFileLength)m_prefixes->Get().id) * m_index->m_wordbag.GetBlockSize()))
			{
				m_status = OpStatus::ERR;
				return FALSE;
			}

		}

		if ((int)m_current_prefix.GetCount() == m_prefix_data_size)
		{
			RETURN_VALUE_IF_ERROR(m_status = m_current_prefix.Sort(), FALSE);
			// count can change if there was a PreFlush while a document was indexed
			m_prefix_data_size = m_current_prefix.GetCount();
		}
		else
			RETURN_VALUE_IF_ERROR(m_status = m_current_prefix.Sort(), FALSE);

		RETURN_VALUE_IF_ERROR(m_status = m_rejects.Reserve(m_rejects.GetCount() + m_current_prefix.GetCount()), FALSE);

		for (i = 0; i < (int)m_current_prefix.GetCount(); ++i)
		{
			VisitedSearch::Result res;

			for (j = m_full_words.GetCount() - 1; j >= 0; --j)
			{
				if ((pos = m_full_words[j]->Search(m_current_prefix[i])) >= (int)m_full_words[j]->GetCount())
					break;
				if (m_full_words[j]->Get(pos).data[RANKID_ID] != m_current_prefix[i].data[RANKID_ID])
					break;
			}
			if (j >= 0)
				continue;

			res.id = m_current_prefix[i].data[RANKID_ID];

			rjct_pos = m_rejects.Search(res.id);
			if (rjct_pos < (int)m_rejects.GetCount() && m_rejects[rjct_pos] == res.id)
				continue;

			if (m_result.Find(res) != -1)
				continue;

			RETURN_VALUE_IF_ERROR(m_status = VisitedSearch::Result::ReadResult(res, &(m_index->m_metadata)), FALSE);

			if (res.invalid || res.next != 0)
			{
				if (m_rejects.GetCount() < m_rejects.GetSize())
					m_status = m_rejects.Insert(rjct_pos, res.id);

				VisitedSearch::Result::DeleteResult(&res);

				RETURN_VALUE_IF_ERROR(m_status, FALSE);
				continue;
			}

			m_status = m_result.Add(res);

			VisitedSearch::Result::DeleteResult(&res);

			RETURN_VALUE_IF_ERROR(m_status, FALSE);
		}
	}

	return TRUE;
}

BOOL FastPrefixIterator::Prev(void)
{
	if (m_result_pos <= 0)
		return FALSE;
	--m_result_pos;
	return TRUE;
}

OP_STATUS FastPrefixIterator::Error(void) const
{
	RETURN_IF_ERROR(m_prefixes->Error());
	return m_status;
}

int FastPrefixIterator::Count(void) const
{
	if (m_prefixes->End() && m_result.GetSize() == 0)
		return 0;

	return -1;
}

BOOL FastPrefixIterator::End(void) const
{
	return m_prefixes->End() && (int)m_current_prefix.GetCount() >= m_prefix_data_size && m_result_pos >= (int)m_result.GetCount();
}

BOOL FastPrefixIterator::Beginning(void) const
{
	return m_result_pos == 0;
}

const VisitedSearch::Result &FastPrefixIterator::Get(void)
{
	return m_result.Get(m_result_pos);
}

/*****************************************************************************
 ChainIterator */

// @param cache_iterator NULL or non-empty iterator
OP_STATUS ChainIterator::Init(TVector<uni_char *> &words, const TVector<RankIndex *> &indexes, SearchIterator<VisitedSearch::Result> *cache_iterator, BOOL prefix_search)
{
	int i, j;
	FastPrefixIterator *it;
	SearchIterator<ACT::PrefixResult> *prefix_it;
	ACT::WordID wid;

	m_words.TakeOver(words);

	if (cache_iterator != NULL)
		RETURN_IF_ERROR(m_chain.Add(cache_iterator));

	for (i = indexes.GetCount() - 1; i >= 0; --i)
	{
		if ((it = OP_NEW(FastPrefixIterator, (indexes[i]))) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(m_chain.Add(it)))
		{
			OP_DELETE(it);
			return OpStatus::ERR_NO_MEMORY;
		}

		for (j = 0; j < (int)m_words.GetCount() - 1; ++j)
		{
			if ((wid = indexes[i]->m_act.Search(m_words[j])) == 0)
				break;
			RETURN_IF_ERROR(it->AddWord(wid));
		}

		if (j != (int)m_words.GetCount() - 1)
		{
			m_chain.Delete(m_chain.GetCount() - 1);
			continue;
		}

		if ((prefix_it = indexes[i]->m_act.PrefixSearch(m_words[j], !prefix_search)) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		it->SetPrefix(prefix_it);
	}

	if (cache_iterator == NULL)
	{
		m_current_iterator = -1;
		do {
			if (++m_current_iterator >= (int)m_chain.GetCount())
				break;
			RETURN_IF_ERROR(((FastPrefixIterator *)m_chain[m_current_iterator])->Init());
		} while (m_chain[m_current_iterator]->End());
	}

	return OpStatus::OK;
}

BOOL ChainIterator::Next(void)
{
	while (m_current_iterator < (int)m_chain.GetCount() && !m_chain[m_current_iterator]->Next())
		++m_current_iterator;

	return m_current_iterator < (int)m_chain.GetCount();
}

BOOL ChainIterator::Prev(void)
{
	while (m_current_iterator >= 0 && !m_chain[m_current_iterator]->Prev())
		--m_current_iterator;

	return m_current_iterator >= 0;
}

OP_STATUS ChainIterator::Error(void) const
{
	if (m_current_iterator < (int)m_chain.GetCount())
		return m_chain[m_current_iterator]->Error();

	return OpStatus::OK;
}

int ChainIterator::Count(void) const
{
	int i, empty;

	if (m_chain.GetCount() == 0)
		return 0;

	empty = 0;
	for (i = m_chain.GetCount() - 1; i >= 0; --i)
		empty += !m_chain[i]->Empty();

	if (empty == 0)
		return FALSE;

	return -1;
}

BOOL ChainIterator::End(void) const
{
	return m_current_iterator >= (int)m_chain.GetCount();
}

BOOL ChainIterator::Beginning(void) const
{
	return m_current_iterator < 0;
}

const VisitedSearch::Result &ChainIterator::Get(void)
{
	return m_chain[m_current_iterator]->Get();
}


#endif  // VISITED_PAGES_SEARCH

