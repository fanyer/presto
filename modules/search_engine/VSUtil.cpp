/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef VISITED_PAGES_SEARCH

#include "modules/search_engine/VSUtil.h"
#include "modules/search_engine/UniCompressor.h"

OP_STATUS FileWord::Add(float rank, VisitedSearch::RecordHandle h)
{
	unsigned idx;
	RankRec item(rank, h);

	idx = file_ids->Search(item);

	if (idx < file_ids->GetCount() && file_ids->Get(idx).h == item.h)
	{
		item.rank = rank * file_ids->Get(idx).rank;
		return file_ids->Replace(idx, item);
	}
	else
		return file_ids->Insert(idx, item);
}

FileWord *FileWord::Create(const uni_char *word, float rank, VisitedSearch::RecordHandle h)
{
	FileWord *fw;
	int wlen;

	if ((fw = OP_NEW(FileWord, ())) == NULL)
		return NULL;

	wlen = (int)uni_strlen(word) + 1;
	if ((fw->word = OP_NEWA(uni_char, wlen)) == NULL)
	{
		OP_DELETE(fw);
		return NULL;
	}
	op_memcpy(fw->word, word, wlen * 2);

	if ((fw->file_ids = OP_NEW(TVector<RankRec>, ())) == NULL)
	{
		OP_DELETE(fw);
		return NULL;
	}

	if (OpStatus::IsError(fw->file_ids->Add(RankRec(rank, h))))
	{
		OP_DELETE(fw);
		return NULL;
	}

	fw->index = h->m_index;

	return fw;
}

BOOL FileWord::LessThan(const void *left, const void *right)
{
	// inlining uni_strcmp saves around 20% of this function
	register const uni_char *s1 = (*(FileWord **)left)->word;
	register const uni_char *s2 = (*(FileWord **)right)->word;

	while (*s1 && *s1 == *s2)
		++s1, ++s2;
	return *s1 < *s2;
}


/*****************************************************************************
 CacheIterator */

CacheIterator::CacheIterator(TypeDescriptor::ComparePtr sort) : m_results(TypeDescriptor(sizeof(VisitedSearch::Result),
											   &VisitedSearch::Result::Assign,
											   sort,
											   &VisitedSearch::Result::DeleteResult
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
											   , &VisitedSearch::Result::EstimateMemoryUsed
#endif
											   ))
										, m_pos(0)
{
}

OP_STATUS CacheIterator::Init(const TVector<FileWord *> &cache, const TVector<uni_char *> *words, BOOL prefix_search)
{
	TVector<FileWord::RankRec> handles, prefixes;
	VisitedSearch::Result result;
	int i, j;
	uni_char *w;
	uni_char **p_w = &w;
	FileWord fw;
	int vmin, k, dpos, dlen, prefix_len;

	m_pos = 0;

	if (cache.GetCount() == 0)
		return OpStatus::OK;

	if (words->GetCount() == 0)
	{  // include all documents
		RETURN_IF_ERROR(handles.DuplicateOf(*(cache[0]->file_ids)));
		for (i = 1; i < (int)cache.GetCount(); ++i)
		{
			RETURN_IF_ERROR(handles.Unite(*(cache[i]->file_ids)));
		}
	}
	else if (prefix_search && words->GetCount() == 1)
	{
		w = words->Get(0);
		prefix_len = (int)uni_strlen(w);
		fw.word = w;
		i = cache.Search(&fw);
		fw.word = NULL; // Avoid OP_DELETE
		while (i < (int)cache.GetCount() && uni_strncmp(w, cache[i]->word, prefix_len) == 0)
	    {
			RETURN_IF_ERROR(handles.Unite(*(cache[i]->file_ids)));
			++i;
	    }
	}
	else {

		w = words->Get(0);
		fw.word = w;
		i = cache.Search(&fw);
		fw.word = NULL; // Avoid OP_DELETE
		if (i >= (int)cache.GetCount() || FileWord::LessThan(&p_w, &(cache[i])))  // not found
			return OpStatus::OK;

		RETURN_IF_ERROR(handles.DuplicateOf(*(cache[i]->file_ids)));

		for (j = 1; (UINT32)j < words->GetCount() - (UINT32)prefix_search && handles.GetCount() > 0; ++j)
		{
			w = words->Get(j);
			fw.word = w;
			i = cache.Search(&fw);
			fw.word = NULL; // Avoid OP_DELETE
			if (i >= (int)cache.GetCount() || FileWord::LessThan(&p_w, &(cache[i])))  // not found
				return OpStatus::OK;

			// VectorBase::Intersect + modifies the ranking
			vmin = 0;
			dpos = 0;
			dlen = 0;

			do {
				k = cache[i]->file_ids->Search(handles.Get(dpos + dlen), vmin, cache[i]->file_ids->GetCount());
				if (k < (int)cache[i]->file_ids->GetCount() && !(handles.Get(dpos + dlen) < cache[i]->file_ids->Get(k)))
				{
					handles[dpos + dlen].rank *= cache[i]->file_ids->Get(k).rank;

					vmin = k;

					if (dlen > 0)
						handles.Delete(dpos, dlen);
					else
						++dpos;

					dlen = 0;
				}
				else
					++dlen;

			} while (dpos + dlen < (int)handles.GetCount());

			if (dlen > 0)
				handles.Delete(dpos, dlen);
		}

		if (handles.GetCount() == 0)
			return OpStatus::OK;

		if (prefix_search)
		{
			w = words->Get(words->GetCount() - 1);
			prefix_len = (int)uni_strlen(w);
			fw.word = w;
			i = cache.Search(&fw);
			fw.word = NULL; // Avoid OP_DELETE
			if (i >= (int)cache.GetCount() || uni_strncmp(w, cache[i]->word, prefix_len) != 0)  // not found
				return OpStatus::OK;

			do {
				RETURN_IF_ERROR(prefixes.Unite(*(cache[i]->file_ids)));
				++i;
			} while (i < (int)cache.GetCount() && uni_strncmp(w, cache[i]->word, prefix_len) == 0);

			// VectorBase::Intersect + modifies the ranking
			vmin = 0;
			dpos = 0;
			dlen = 0;

			do {
				k = prefixes.Search(handles.Get(dpos + dlen), vmin, prefixes.GetCount());
				if (k < (int)prefixes.GetCount() && !(handles.Get(dpos + dlen) < prefixes.Get(k)))
				{
					handles[dpos + dlen].rank *= prefixes.Get(k).rank;

					vmin = k;

					if (dlen > 0)
						handles.Delete(dpos, dlen);
					else
						++dpos;

					dlen = 0;
				}
				else
					++dlen;

			} while (dpos + dlen < (int)handles.GetCount());

			if (dlen > 0)
				handles.Delete(dpos, dlen);
		}
	}

	RETURN_IF_ERROR(m_results.Reserve(handles.GetCount()));
	for (i = handles.GetCount() - 1; i >= 0; --i)
	{
		RETURN_IF_ERROR(Handle2Result(result, handles[i].h));
		result.ranking = handles[i].rank;
		RETURN_IF_ERROR(m_results.Add(result));
		VisitedSearch::Result::DeleteResult(&result);
	}

	return m_results.Sort();
}

OP_STATUS CacheIterator::Handle2Result(VisitedSearch::Result &result, VisitedSearch::RecordHandle handle)
{
	UniCompressor uc;

	if (handle->GetField("url").CopyStringValue(&result.url) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (handle->GetField("title").CopyStringValue(&result.title) == NULL)
	{
		VisitedSearch::Result::DeleteResult(&result);
		return OpStatus::ERR_NO_MEMORY;
	}

	if ((result.thumbnail_size = handle->GetField("thumbnail").GetSize()) > 0)
	{
		if ((result.thumbnail = OP_NEWA(unsigned char, result.thumbnail_size)) == NULL)
		{
			VisitedSearch::Result::DeleteResult(&result);
			return OpStatus::ERR_NO_MEMORY;
		}
		handle->GetField("thumbnail").GetValue(result.thumbnail, result.thumbnail_size);
	}

	OP_STATUS status = result.SetCompressedPlaintext((const unsigned char *)handle->GetField("plaintext").GetAddress(), handle->GetField("plaintext").GetSize());
	if (OpStatus::IsError(status))
	{
		VisitedSearch::Result::DeleteResult(&result);
		return status;
	}

	handle->GetField("visited").GetValue(&result.visited);

	result.ranking = 0.0F;

	return OpStatus::OK;
}

#endif  // VISITED_PAGES_SEARCH

