/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef VSUTIL_H
#define VSUTIL_H

#include "modules/search_engine/VisitedSearch.h"

struct WordRank
{
	uni_char *word;
	float rank;

	WordRank(void) {word = NULL; rank = 0.0F;}
	WordRank(const uni_char *w, float r)
	{
		word = uni_strdup(w);
		rank = r;
	}

	BOOL operator<(const WordRank &r) const
	{
		return this->word < r.word;  // anything is fine here
	}

	static void Destruct(void *wr)
	{
		if (((WordRank *)wr)->word != NULL)
			op_free(((WordRank *)wr)->word);
	}

	void Destroy()
	{
		op_free(word);
	}
};

class RecordHandleRec : public BSCursor
{
public:
	RecordHandleRec(RankIndex *index) : BSCursor(&(index->m_metadata)), m_word_list(&DefDescriptor<WordRank>::Compare, &WordRank::Destruct)
	{
		m_index = index;
		m_word_count = 0;
		m_plaintext.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
		m_plaintext.SetCachedLengthPolicy(TempBuffer::TRUSTED);
	}

	~RecordHandleRec(void)
	{
		m_word_list.Clear();
	}

friend class VisitedSearch;
friend struct FileWord;
protected:
	void Invalidate() { m_index = NULL; table = NULL; }
	BOOL IsInvalid() { return m_index == NULL; }

	RankIndex *m_index;
	int m_word_count;
	TempBuffer m_plaintext;
	TVector<WordRank> m_word_list;
};

struct FileWord : public NonCopyable
{
	struct RankRec
	{
		VisitedSearch::RecordHandle h;
		float rank;

		RankRec(void)
		{
			h = NULL;
			rank = 0;
		}
		RankRec(float rank, VisitedSearch::RecordHandle h)
		{
			this->h = h;
			this->rank = rank;
		}
		BOOL operator<(const RankRec &right) const {return h < right.h;}
	};

	uni_char *word;             // the caches are sorted by the word
	TVector<RankRec> *file_ids; // in-memory list of id/rank
	UINT32 file_pos;            // position where to update file_ids
	RankIndex *index;           // one of the N indexes to save the data into

	FileWord(void)
	{
		word = NULL;
		file_ids = NULL;
		file_pos = 0;
		index = NULL;
	}

	~FileWord(void)
	{
		if (word != NULL)
			OP_DELETEA(word);  // op_free(word);
		if (file_ids != NULL)
			OP_DELETE(file_ids);
	}

	CHECK_RESULT(OP_STATUS Add(float rank, VisitedSearch::RecordHandle h));

	static FileWord *Create(const uni_char *word, float rank, VisitedSearch::RecordHandle h);

	static BOOL LessThan(const void *left, const void *right);
};

class CacheIterator : public SearchIterator<VisitedSearch::Result>
{
public:
	CacheIterator(TypeDescriptor::ComparePtr sort);

	CHECK_RESULT(OP_STATUS Init(const TVector<FileWord *> &cache, const TVector<uni_char *> *words, BOOL prefix_search = FALSE));

	CHECK_RESULT(OP_STATUS Handle2Result(VisitedSearch::Result &result, VisitedSearch::RecordHandle handle));

	virtual BOOL Next(void) {return (UINT32)++m_pos < m_results.GetCount();}

	virtual BOOL Prev(void) {return --m_pos >= 0;}

	virtual const VisitedSearch::Result &Get(void) {return m_results.Get(m_pos);}

	CHECK_RESULT(virtual OP_STATUS Error(void) const) {return OpStatus::OK;}

	virtual int Count(void) const {return m_results.GetCount();}

	virtual BOOL End(void) const {return (UINT32)m_pos >= m_results.GetCount();}

	virtual BOOL Beginning(void) const {return m_pos < 0;}

protected:
	TVector<VisitedSearch::Result> m_results;
	int m_pos;
};

#endif  // VSUTIL_H

