/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef VSITERATOR_H
#define VSITERATOR_H

#include "modules/search_engine/VisitedSearch.h"

/**
 * Returns all documents sorted by time.
 */
class AllDocIterator : public SearchIterator<VisitedSearch::Result>
{
public:
	AllDocIterator(RankIndex *index) : m_result(&VisitedSearch::Result::DeleteResult)
	{
		m_index = index;
		m_result_pos = -1;
		m_status = OpStatus::ERR;
	}

	CHECK_RESULT(OP_STATUS Init(void));

	virtual BOOL Next(void);
	virtual BOOL Prev(void);

	virtual const VisitedSearch::Result &Get(void)
	{
		return m_result.Get(m_result_pos);
	}

	CHECK_RESULT(virtual OP_STATUS Error(void) const) {return m_status;}

	virtual int Count(void) const {return End() ? (int)m_result.GetCount() : -1;}

	virtual BOOL End(void) const
	{
		return (unsigned)m_result_pos >= m_result.GetCount();
	}

	virtual BOOL Beginning(void) const
	{
		return m_result_pos < 0 && m_result.GetCount() > 0;
	}

protected:
	RankIndex *m_index;
	TVector<VisitedSearch::Result> m_result;
	int m_result_pos;
	OP_STATUS m_status;
};

class QueryIteratorBase : public AllDocIterator
{
public:
	QueryIteratorBase(RankIndex *index) : AllDocIterator(index) {}

	CHECK_RESULT(virtual OP_STATUS AddWord(UINT32 pos)) = 0;
};

/**
 * Returns documents sorted by ranking.
 */
class RankIterator : public QueryIteratorBase
{
public:
	RankIterator(RankIndex *index) : QueryIteratorBase(index),
		m_rank_vec(PtrDescriptor< TVector<RankId> >()), m_id_vec(PtrDescriptor< TVector<RankId> >())
	{
		m_line = 0;
		m_max_line = (unsigned)-1;
	}

	CHECK_RESULT(virtual OP_STATUS AddWord(UINT32 pos));

	virtual BOOL Next(void);

	virtual int Count(void) const
	{
		if (m_rank_vec.GetCount() == 0)
			return AllDocIterator::Count();

		if (End())
			return (int)m_result.GetCount();

		return m_max_line;
	}

	virtual BOOL End(void) const
	{
		if (m_rank_vec.GetCount() == 0)
			return AllDocIterator::End();

		return m_line >= m_max_line && (unsigned)m_result_pos >= m_result.GetCount();
	}

protected:
	TVector<TVector<RankId> *> m_rank_vec;
	TVector<TVector<RankId> *> m_id_vec;
	unsigned m_line;
	unsigned m_max_line;
};

/**
 * Returns documents sorted by time of being seen.
 */
class TimeIterator : public QueryIteratorBase
{
public:
	TimeIterator(RankIndex *index) : QueryIteratorBase(index), m_id_vec(PtrDescriptor< TVector<RankId> >()) {}

	CHECK_RESULT(virtual OP_STATUS AddWord(UINT32 pos));
	virtual BOOL Next(void);
protected:
	TVector<TVector<RankId> *> m_id_vec;
};


/**
 * Join results from all subindexes.
 * I would have to think to tell if calling Prev() after Next() will lead to the same position if multiple items had the same ranking.
 */
class MultiOrIterator : public SearchIterator<VisitedSearch::Result>
{
public:

	MultiOrIterator(void) :
	  subindex(&PtrDescriptor<SearchIterator<VisitedSearch::Result> >::Destruct), Compare(NULL)
	{}

	MultiOrIterator(TVector<SearchIterator<VisitedSearch::Result> *> &feeder, TypeDescriptor::ComparePtr compare) :
	  subindex(&PtrDescriptor<SearchIterator<VisitedSearch::Result> >::Destruct)
	{
		Set(feeder, compare);
	}

	void Set(TVector<SearchIterator<VisitedSearch::Result> *> &feeder, TypeDescriptor::ComparePtr compare)
	{
		subindex.TakeOver(feeder);

		Compare = compare;
	}

	virtual BOOL Next(void);

	virtual BOOL Prev(void);

	virtual const VisitedSearch::Result &Get(void);

	CHECK_RESULT(virtual OP_STATUS Error(void) const);

	virtual int Count(void) const;

	virtual BOOL End(void) const;

	virtual BOOL Beginning(void) const;

protected:
	TVector<SearchIterator<VisitedSearch::Result> *> subindex;
	TypeDescriptor::ComparePtr Compare;
};

/**
 * ranking is not taken into account for performance reasons
 */
class FastPrefixIterator : public SearchIterator<VisitedSearch::Result>
{
public:
	FastPrefixIterator(RankIndex *index) :
	  m_full_words(PtrDescriptor< TVector<RankId> >()),
	  m_result(TypeDescriptor(sizeof(VisitedSearch::Result),
	    &VisitedSearch::Result::Assign,
		&VisitedSearch::Result::CompareId,
		&VisitedSearch::Result::DeleteResult
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		, &VisitedSearch::Result::EstimateMemoryUsed
#endif		
		)),
	  m_current_prefix(&RankId::CompareId)
	{
		m_index = index;
		m_prefixes = NULL;
		m_prefix_data_size = -1;
		m_result_pos = -1;
		m_status = OpStatus::ERR;
	}

	virtual ~FastPrefixIterator(void);

	CHECK_RESULT(OP_STATUS AddWord(UINT32 pos));
	void SetPrefix(SearchIterator<ACT::PrefixResult> *prefix_iterator);

	CHECK_RESULT(OP_STATUS Init(void)) {Next(); return Error();}

	virtual BOOL Next(void);
	virtual BOOL Prev(void);

	CHECK_RESULT(virtual OP_STATUS Error(void) const);

	virtual int Count(void) const;

	virtual BOOL End(void) const;
	virtual BOOL Beginning(void) const;

	virtual const VisitedSearch::Result &Get(void);
protected:
	RankIndex *m_index;
	TVector<TVector<RankId> *> m_full_words;
	TVector<VisitedSearch::Result> m_result;
	TVector<UINT32> m_rejects;
	SearchIterator<ACT::PrefixResult> *m_prefixes;
	TVector<RankId> m_current_prefix;
	int m_prefix_data_size;
	int m_result_pos;
	OP_STATUS m_status;
};

class ChainIterator : public  SearchIterator<VisitedSearch::Result>
{
public:
	ChainIterator(void) :
	  m_words(&PtrDescriptor<uni_char>::DestructArray), m_chain(&PtrDescriptor<FastPrefixIterator>::Destruct)
	{
		m_current_iterator = 0;
	}

	CHECK_RESULT(OP_STATUS Init(TVector<uni_char *> &words, const TVector<RankIndex *> &indexes, SearchIterator<VisitedSearch::Result> *cache_iterator, BOOL prefix_search));

	virtual BOOL Next(void);
	virtual BOOL Prev(void);

	CHECK_RESULT(virtual OP_STATUS Error(void) const);

	virtual int Count(void) const;

	virtual BOOL End(void) const;
	virtual BOOL Beginning(void) const;

	virtual const VisitedSearch::Result &Get(void);

protected:
	TVector<uni_char *> m_words;
	TVector<SearchIterator<VisitedSearch::Result> *> m_chain;
	int m_current_iterator;
};

#endif  // VSITERATOR_H

