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
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/tempbuf.h"

#include "modules/search_engine/WordHighlighter.h"
#include "modules/search_engine/WordSegmenter.h"
#include "modules/search_engine/UniCompressor.h"

#include "modules/search_engine/VSIterator.h"
#include "modules/search_engine/VSUtil.h"

#include "modules/search_engine/ACTUtil.h"

#if defined(USE_OP_THREAD_TOOLS) && defined(VPS_WRAPPER)
#include "modules/pi/OpThreadTools.h"
#endif // USE_OP_THREAD_TOOLS && VPS_WRAPPER

#define MAX_DOC_WORDS 5000
#define STOPWORD_DOC_COUNT 500

// max size in MB if no size is specified
#define DEFAULT_MAX_SIZE 1024

// flush timing
#define DELAY_MSG_PREFLUSH    240000

#define PREFLUSH_LIMIT        100
#define DELAY_PREFLUSH        200
#define DELAY_PREFLUSH_FLUSH  30000

#define FLUSH_LIMIT           100
#define DELAY_FLUSH           150
#define DELAY_FLUSH_COMMIT    30000

#define DELAY_COMMIT_PREFLUSH 180000

#define NUM_RETRIES           5
#define ERROR_RETRY_MS         5000
#define BUSY_RETRY_MS          10000


/*****************************************************************************
 open/close */

VisitedSearch::VisitedSearch(void) : m_index(&RankIndex::CompareId, &PtrDescriptor<RankIndex>::Destruct),

		              m_cache(&FileWord::LessThan, &PtrDescriptor<FileWord>::Destruct),
		              m_preflush(&FileWord::LessThan, &PtrDescriptor<FileWord>::Destruct),

					  m_pending(),
					  m_meta(&PtrDescriptor<RecordHandleRec>::Destruct),
					  m_meta_pf(&PtrDescriptor<RecordHandleRec>::Destruct)
{
#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
		m_log = NULL;
#endif

	m_flags = 0;
	m_max_size = INVALID_FILE_LENGTH;
	m_subindex_size = INVALID_FILE_LENGTH;
	m_pos = 0;
	m_flush_errors = 0;
}

CHECK_RESULT(static OP_STATUS ScanDir(TVector<int> &subdirs, const uni_char *directory));
static OP_STATUS ScanDir(TVector<int> &subdirs, const uni_char *directory)
{
	OpFolderLister *lister;
	const uni_char *fname;
	uni_char *endptr;
	int num;
	OpString path;
	OP_BOOLEAN exists;
	OP_STATUS err;

	RETURN_IF_ERROR(exists = BlockStorage::FileExists(directory));
	if (exists == OpBoolean::IS_FALSE)
		return OpStatus::OK;
		
	RETURN_OOM_IF_NULL(path.Reserve((int)uni_strlen(directory) + 7 + (int)op_strlen(FNAME_WB)));
	RETURN_OOM_IF_NULL(lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), directory));

	while (lister->Next())
	{
		fname = lister->GetFileName();
		num = uni_strtoul(fname, &endptr, 10);

		RETURN_IF_ERROR(path.Set(directory));
		RETURN_IF_ERROR(path.AppendFormat(UNI_L("%c%.04i%c%s"), PATHSEPCHAR, num, PATHSEPCHAR, UNI_L(FNAME_WB)));

		RETURN_IF_ERROR(exists = BlockStorage::FileExists(path.CStr()));

		if (lister->IsFolder() && uni_strlen(fname) == 4 && endptr - fname == 4 && exists == OpBoolean::IS_TRUE)
		{
			if (OpStatus::IsError(err = subdirs.Insert(num)))
			{
				OP_DELETE(lister);
				return err;
			}
		}
	}

	OP_DELETE(lister);

	return OpStatus::OK;
}

OP_STATUS VisitedSearch::Open(const uni_char *directory)
{
	short i, pos;
	RankIndex *r;
	OP_STATUS err;
	TVector<int> subdirs;
	time_t min_time, modif_time;
	OpFileLength total_size;

//	if (m_max_size < MAX_VISITED_SEARCH_SUBINDEX_SIZE * 2097152)
//		return OpStatus::ERR;

	{
        // Do not listen to these messages when the wrapper is present, the wrapper will make sure
        // the messages are posted and read in a threadsafe manner.
#ifndef VPS_WRAPPER
		OpMessage msg[] = {MSG_VISITEDSEARCH_PREFLUSH, MSG_VISITEDSEARCH_FLUSH, MSG_VISITEDSEARCH_COMMIT};
		RETURN_IF_ERROR(g_main_message_handler->SetCallBackList(this, (MH_PARAM_1)this, msg, 3));
#endif // !VPS_WRAPPER
	}

	if (OpStatus::IsError(err = ScanDir(subdirs, directory)))
	{
#ifndef VPS_WRAPPER
		g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
		return err;
	}

	if (OpStatus::IsError(err = m_index.Reserve(subdirs.GetSize())))
	{
#ifndef VPS_WRAPPER
		g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
		return err;
	}

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	OpString journalname;

	RETURN_IF_ERROR(journalname.SetConcat(directory, UNI_L(PATHSEP), UNI_L("vps.log")));
	if (m_log != NULL)
		OP_DELETE(m_log);

	SearchEngineLog::ShiftLogFile(10, journalname);

	if ((m_log = SearchEngineLog::CreateLog(SearchEngineLog::File, journalname.CStr())) == NULL)
		return OpStatus::ERR;
#endif

	min_time = (time_t)-1;
	pos = 0;
	total_size = 0;

	for (i = subdirs.GetCount() - 1; i >= 0; --i)
	{
		if ((r = OP_NEW(RankIndex, ())) == NULL)
		{
#ifndef VPS_WRAPPER
			g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
			m_index.Clear();
			return OpStatus::ERR_NO_MEMORY;
		}

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
		if (OpStatus::IsError(RankIndex::LogSubDir(m_log, directory, subdirs[i])))
		{
#ifndef VPS_WRAPPER
			g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
			OP_DELETE(r);
			m_index.Clear();
			return err;
		}
#endif

		if (OpStatus::IsError(err = r->Open(directory, subdirs[i])))
		{
#ifndef VPS_WRAPPER
			g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
			OP_DELETE(r);
			m_index.Clear();
			return err;
		}

		modif_time = r->ModifTime();
		OP_ASSERT(modif_time != (time_t)-1);

		// this is the latest subindex
		if (modif_time != (time_t)-1 && (min_time == (time_t)-1 || min_time < modif_time))
		{
			min_time = modif_time;
			pos = 0;
		}

		if (!r->m_wordbag.IsNativeEndian())
		{
			OpStatus::Ignore(r->Clear());
			OP_DELETE(r);
		}
		else if (OpStatus::IsError(err = m_index.Insert(pos, r)))
		{
#ifndef VPS_WRAPPER
			g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
			OP_DELETE(r);
			m_index.Clear();
			return err;
		}
		else {
			++pos;
			if (m_max_size == INVALID_FILE_LENGTH)
				total_size += r->Size();
		}
	}

	if (m_index.GetCount() == 0)
	{
		if ((r = OP_NEW(RankIndex, ())) == NULL)
		{
#ifndef VPS_WRAPPER
			g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
			return OpStatus::ERR_NO_MEMORY;
		}

		if (OpStatus::IsError(err = r->Open(directory, 0)))
		{
#ifndef VPS_WRAPPER
			g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
			OP_DELETE(r);
			return err;
		}

		if (OpStatus::IsError(err = m_index.Add(r)))
		{
#ifndef VPS_WRAPPER
			g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
			OP_DELETE(r);
			return err;
		}
	}

	if (m_max_size == INVALID_FILE_LENGTH)
	{
		if (total_size > DEFAULT_MAX_SIZE * 1048576)
			m_max_size = total_size;
		else
			m_max_size = DEFAULT_MAX_SIZE * 1048576;

		m_subindex_size = m_max_size / 10;
	}

#if 0 // #ifdef _DEBUG
	TVector<RankId> wordlist(&(RankId::CompareId));
	SearchIterator<ACT::PrefixResult> *res;
	int j;
	int wb_block_size, meta_block_size;
	register OpFileLength block_pos, vector_pos;

	for (i = m_index.GetCount() - 1; i >= 0; --i)
	{
		res = m_index[i]->m_act.PrefixCaseSearch("");
		OP_ASSERT(res != NULL);

		// freshly created ACT
		if (res->End())
		{
			OP_DELETE(res);
			continue;
		}

		wb_block_size = m_index[i]->m_wordbag.GetBlockSize();
		meta_block_size = m_index[i]->m_metadata.GetBlockSize();

		do {
			vector_pos = res->Get().id * wb_block_size;
			wordlist.SetCount(m_index[i]->m_wordbag.DataLength(vector_pos) / sizeof(RankId));
			m_index[i]->m_wordbag.ReadApnd(wordlist.Ptr(), wordlist.GetCount() * sizeof(RankId), vector_pos);

			OP_ASSERT(wordlist.GetCount() > 0);
			for (j = wordlist.GetCount() - 1; j >= 0; --j)
			{
				block_pos = wordlist[j].data[1] * meta_block_size;
				OP_ASSERT(m_index[i]->m_metadata.IsStartBlock(block_pos));
			}
		} while (res->Next());
		OP_DELETE(res);
	}
#endif

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "Open", UNI_L("%s"), directory);
#endif

	return OpStatus::OK;
}

void VisitedSearch::SetMaxSize(OpFileLength max_size)
{
#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "SetMaxSize", "%f", (double)max_size);
#endif

	if (max_size == INVALID_FILE_LENGTH || (max_size >> 24) > 1048576)
		max_size = DEFAULT_MAX_SIZE;

	m_max_size = max_size * 1048576;
	m_subindex_size = m_max_size / 10;

	CheckMaxSize_RemoveOldIndexes();
}

void VisitedSearch::SetMaxItems(int max_items)
{
	if (max_items < 0)
		SetMaxSize(INVALID_FILE_LENGTH);
	else if (max_items < 20)
		SetMaxSize(0);
	else {
		max_items *= 25600;  // 1 URL took roughly 25KB when I measured
		SetMaxSize((max_items + 1048575) / 1048576);
	}
}

OP_STATUS VisitedSearch::Close(BOOL force_close)
{
	OP_STATUS err;

	if (m_index.GetCount() == 0)
	{
		m_cache.Clear();
		m_preflush.Clear();
#ifndef VPS_WRAPPER
		g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER
		return OpStatus::OK;
	}

	OpStatus::Ignore(Commit());

	if (m_meta.GetCount() > 0 || m_meta_pf.GetCount() > 0 || (m_flags & Flushed) != 0)
	{
		err = Commit();

		if (!force_close)
			RETURN_IF_ERROR(err);
	}
	else
		err = OpStatus::OK;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
	{
		m_log->Write(SearchEngineLog::Debug, "Close", "");
		OP_DELETE(m_log);
		m_log = NULL;
	}
#endif

	m_index.Clear();

#ifndef VPS_WRAPPER
	g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER

	return err;
}

OP_STATUS VisitedSearch::Clear(BOOL reopen)
{
	int i;
	OpString path;

	m_cache.Clear();
	m_preflush.Clear();

	m_meta.Clear();
	m_meta_pf.Clear();

	if (m_index.GetCount() == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(path.Set(m_index[0]->m_wordbag.GetFullName()));
	path.Delete(path.Length() - 6 - (int)op_strlen(FNAME_WB));  // the main directory

	for (i = m_index.GetCount() - 1; i >= 0; --i)
		RETURN_IF_ERROR(m_index[i]->Clear());

	m_index.Clear();

	for (i = 0; i < (int)m_pending.GetCount(); i++)
		m_pending[i]->Invalidate(); // The indexes are gone, invalidate all pending records

	m_flags = 0;
	m_pos = 0;
	m_flush_errors = 0;

	if (!reopen)
	{
		OpStatus::Ignore(BlockStorage::DeleteFile(path.CStr()));
		return OpStatus::OK;
	}

#ifndef VPS_WRAPPER
	g_main_message_handler->UnsetCallBacks(this);
#endif // !VPS_WRAPPER

	return Open(path.CStr());
}

/*****************************************************************************
 RecordHandle */

VisitedSearch::RecordHandle VisitedSearch::CreateRecord(const char *url, const uni_char *title)
{
	RecordHandleRec *cursor;
	OP_STATUS err;

	if (!IsOpen())
		return NULL;

	if ((cursor = OP_NEW(RecordHandleRec, (m_index[0]))) == NULL)
		return NULL;

	cursor->m_word_count = 0;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
	{
		m_log->Write(SearchEngineLog::Debug, "CreateRecord", "%X %s", (INTPTR)cursor, url);
		if (title != NULL)
			m_log->Write(SearchEngineLog::Debug, "record title", UNI_L("%X %s"), (INTPTR)cursor, title);
	}
#endif

	if (OpStatus::IsError(err = RankIndex::SetupCursor(*cursor)))
		goto visitedsearch_create_record;

	if (OpStatus::IsError(err = cursor->Create()))
		goto visitedsearch_create_record;

	if (OpStatus::IsError(err = cursor->GetField("visited").SetValue(op_time(NULL))))
		goto visitedsearch_create_record;

	if (OpStatus::IsError(err = cursor->GetField("invalid").SetValue(0)))
		goto visitedsearch_create_record;

	if (OpStatus::IsError(err = cursor->GetField("url").SetStringValue(url)))
		goto visitedsearch_create_record;

	if (title != NULL)
	{
		if (OpStatus::IsError(err = cursor->GetField("title").SetStringValue(title)))
			goto visitedsearch_create_record;

		if (OpStatus::IsError(err = IndexTextBlock(cursor, title, RANK_TITLE)))
			goto visitedsearch_create_record;
	}

	if (OpStatus::IsError(err = IndexURL(cursor, url, RANK_TITLE)))
		goto visitedsearch_create_record;

	if (OpStatus::IsError(err = m_pending.Add(cursor)))
		goto visitedsearch_create_record;

	return cursor;

visitedsearch_create_record:
#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Warning, "CreateRecord failed", "%X", (INTPTR)cursor);
#endif

	OP_DELETE(cursor);
	return NULL;
}

OP_STATUS VisitedSearch::AddTitle(RecordHandle handle, const uni_char *title)
{
	int old_word_count;

	if (title == NULL || *title == 0)
		return OpStatus::OK;

	if (handle->IsInvalid())
		return OpStatus::ERR;

	if (handle->GetField("title").GetSize() > 0)
		return OpStatus::OK;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "record title", UNI_L("%X %s"), (INTPTR)handle, title);
#endif

	RETURN_IF_ERROR(handle->GetField("title").SetStringValue(title));

	old_word_count = handle->m_word_count;

	RETURN_IF_ERROR(IndexTextBlock(handle, title, RANK_TITLE));

	handle->m_word_count = old_word_count;  // word count is not changed by title

	return OpStatus::OK;
}

OP_STATUS VisitedSearch::AddWord(VisitedSearch::RecordHandle handle, const uni_char *text, float ranking)
{
	WordRank w(text, ranking);
	OP_STATUS ret = handle->m_word_list.Add(w);
	if (OpStatus::IsError(ret))
	{
		w.Destroy();
		return ret;
	}
	return OpStatus::OK;
}

OP_STATUS VisitedSearch::IndexTextBlock(VisitedSearch::RecordHandle handle, const uni_char *text, float ranking, BOOL fine_segmenting)
{
	WordSegmenter msgtok(fine_segmenting ? WordSegmenter::FineSegmenting : 0);
	OpString word;
	OP_BOOLEAN e;
	register const uni_char *chk;
	UINT16 hash;

	if (!text || !*text)
		return OpStatus::OK;

	RETURN_IF_ERROR(msgtok.Set(text));

	RETURN_IF_ERROR(e = msgtok.GetNextToken(word));

	while (e == OpBoolean::IS_TRUE && handle->m_word_count <= MAX_DOC_WORDS)
	{
		word.MakeUpper();

		// it isn't expected to have invalid ACT characters here, but let's rather check anyway
		for (chk = word.CStr(); *chk != 0; ++chk)
			if ((UINT16)*chk <= FIRST_CHAR)
				break;

		if (*chk != 0)
		{
			RETURN_IF_ERROR(e = msgtok.GetNextToken(word));
			continue;
		}

		RETURN_IF_ERROR(AddWord(handle, word.CStr(), ranking));

		++(handle->m_word_count);

		RETURN_IF_ERROR(e = msgtok.GetNextToken(word));
	}

	handle->GetField("hash").GetValue(&hash);

	while (*text != 0)
	{
		hash *= 101;
		hash ^= *text;

		++text;
	}

	RETURN_IF_ERROR(handle->GetField("hash").SetValue(hash));

	return OpStatus::OK;
}

OP_STATUS VisitedSearch::IndexURL(VisitedSearch::RecordHandle handle, const char *url, float ranking)
{
	OpString url16;
	uni_char *end, *dot, *domain;
	int words_indexed;

	unsigned offset = 0;
	if ((op_strnicmp(url, "http://", 7) == 0) && url[7] != 0)
		offset = 7;
	else if ((op_strnicmp(url, "https://", 8) == 0) && url[8] != 0)
		offset = 8;

	if (offset == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(url16.SetFromUTF8(url));

	domain = (uni_char *)url16.CStr() + offset;
	if ((end = uni_strchr(domain, '/')) == NULL)
		if ((end = uni_strchr(domain, '?')) == NULL)
			if ((end = uni_strchr(domain, '#')) == NULL)
				end = domain + uni_strlen(domain);
	// Domain is case-insensitive. For the purpose of fine-grained word-segmenting below, don't consider camelcasing here relevant
	for (uni_char *p = domain; p < end; p++)
		*p = uni_tolower(*p);

	RETURN_IF_ERROR(AddTextBlock(handle, url16.CStr(), ranking)); // Add whole url as is, also to plaintext field in case of phrase filtering
	handle->m_plaintext.Append(UNI_L("\r")); // Add "excerpt boundary" (repurposing CR) so that highlighted excerpts don't include url unnecessarily
	RETURN_IF_ERROR(IndexTextBlock(handle, url16.CStr(), ranking, TRUE)); // Add again using fine-grained word-segmenting (CORE-44206)

	// Remove path, replace "http://" with "site:"
	*end = 0;
	url16.Delete(0, offset);
	RETURN_IF_ERROR(url16.Insert(0, "site:"));
	url16.MakeUpper();

	words_indexed = 0;

	while ((dot = uni_strchr(url16.CStr() + 5, '.')) != NULL)
	{
		RETURN_IF_ERROR(AddWord(handle, url16.CStr(), ranking));

		// attempt to index "hp" from "www.hp.co.uk", "yahoo" from "yahoo.com.au",
		// "yahoo" from "au.yahoo.com" and "maps", "google" from "maps.google.com"
		*dot = 0;
		if (!uni_str_eq(url16.CStr() + 5, "WWW") && !uni_str_eq(url16.CStr() + 5, "COM") && 
				(words_indexed == 1 || uni_strlen(url16.CStr() + 5) > 3))
			RETURN_IF_ERROR(AddWord(handle, url16.CStr() + 5, ranking));
		++words_indexed;
		*dot = '.';

		url16.Delete(5, (int)(dot - url16.CStr() - 4));
	}

	if (url16[5] != 0 && !uni_str_eq(url16.CStr() + 5, "COM"))
	{
		RETURN_IF_ERROR(AddWord(handle, url16.CStr(), ranking));
	}

	return OpStatus::OK;
}

static OP_STATUS AppendWithoutCR(TempBuffer &buf, const uni_char *text)
{
	const uni_char *pos;
	while ((pos = uni_strchr(text, '\r')) != 0)
	{
		// Replacing CR ('\r') with space as CR has been repurposed for "excerpt boundary"
		RETURN_IF_ERROR(buf.Append(text, pos-text));
		RETURN_IF_ERROR(buf.Append(UNI_L(" ")));
		text = pos+1;
	}
	return buf.Append(text);
}

OP_STATUS VisitedSearch::AddTextBlock(VisitedSearch::RecordHandle handle, const uni_char *text, float ranking, BOOL is_continuation)
{
	if (handle->IsInvalid())
		return OpStatus::ERR;

	OP_ASSERT(handle->GetField("plaintext").GetSize() == 0);  // operation on a closed handle!
#ifdef _DEBUG                                                 // we are lucky now, but it can also crash
	if (handle->GetField("plaintext").GetSize() != 0)         // if there would be a PreFlush..Commit between CloseRecord and here
		return OpStatus::OK;                                  // so this cannot be tolerated
#endif

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
	{
		m_log->Write(SearchEngineLog::Debug, "AddTextBlock", "%X %f", (INTPTR)handle, (double)ranking);
		m_log->Write(SearchEngineLog::Debug, "text block", uni_strlen(text), text);
	}
#endif

	// only index the first MAX_DOC_WORDS from a document
	if (handle->m_word_count > MAX_DOC_WORDS)
		return OpStatus::OK;

	const uni_char* t = text;
	while (*text == '\r' || *text == '\n' || *text == ' ' || *text == '\t')
		++text;
	if (text != t)
		is_continuation = FALSE;

	if (!is_continuation && handle->m_plaintext.Length() != 0)
		OpStatus::Ignore(handle->m_plaintext.Append(UNI_L("\n")));
	OpStatus::Ignore(AppendWithoutCR(handle->m_plaintext, text));

	return IndexTextBlock(handle, text, ranking);
}

static void RemoveHandle(TVector<FileWord *> &cache, VisitedSearch::RecordHandle handle)
{
	int i, j;

	for (i = cache.GetCount() - 1; i >= 0; --i)
	{
		for (j = cache[i]->file_ids->GetCount() - 1; j >= 0; --j)
		{
			if (cache[i]->file_ids->Get(j).h == handle)
				cache[i]->file_ids->Delete(j);
		}

		if (cache[i]->file_ids->GetCount() == 0)
			cache.Delete(i);
	}
}

// Finds the record of the previous version of the document 
OP_STATUS VisitedSearch::FindLastIndex(UINT32 *prev, UINT16 *prev_idx, const char *url, RankIndex *current_index)
{
	int i, ipos, ipos_end;
	char *tail_url;

	ipos = m_index.Find(current_index);
	OP_ASSERT(ipos < (int)m_index.GetCount() && m_index[ipos] == current_index);
	ipos_end = ipos + (int)m_index.GetCount();

	for (i = ipos; i < ipos_end; ++i)
	{
		RankIndex *index = m_index[i % m_index.GetCount()];

		if ((*prev = index->m_url.CaseSearch(url)) != 0)
		{
			RETURN_IF_ERROR(RankIndex::GetTail(&tail_url, *prev, index));
			int words_equal = ACT::WordsEqual(tail_url, url);
			OP_DELETEA(tail_url);
			if (words_equal == -1)
			{
				*prev_idx = index->m_id;
				break;
			}
			else {
				*prev = 0;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS VisitedSearch::InsertHandle(VisitedSearch::RecordHandle handle)
{
	int i;

	for (i = handle->m_word_list.GetCount() - 1; i >= 0; --i)
	{
		if (handle->m_word_list[i].word != NULL)  // there was out of memory - ignore
			RETURN_IF_ERROR(Insert(m_cache, handle, handle->m_word_list[i].word, handle->m_word_list[i].rank));
	}

	handle->m_word_list.Clear();

	return OpStatus::OK;
}

OP_STATUS VisitedSearch::CompressText(VisitedSearch::RecordHandle handle)
{
	if (handle->m_plaintext.Length() != 0)
	{
		UniCompressor uc;
		unsigned char *ctext;
		int csize;
		OP_STATUS err;

		RETURN_IF_ERROR(uc.InitCompDict());

		size_t len = handle->m_plaintext.Length() * 3 + 6;
		RETURN_OOM_IF_NULL(ctext = OP_NEWA(unsigned char, len));

		csize = uc.Compress(ctext, handle->m_plaintext.GetStorage());

		err = handle->GetField("plaintext").SetValue(ctext, csize);
		
		OP_DELETEA(ctext);

		RETURN_IF_ERROR(err);

		handle->m_plaintext.Clear();
	}

	return OpStatus::OK;
}


OP_STATUS VisitedSearch::CloseRecord(VisitedSearch::RecordHandle handle)
{
	int i;
	UINT16 prev_idx = 0;
	UINT32 prev = 0;
	UINT16 hash, prev_hash;
	const char *url;
	OP_BOOLEAN err;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "CloseRecord", "%X", (INTPTR)handle);
#endif

	// ignore empty documents
	if (handle->m_word_count <= 0)
	{
		OP_ASSERT(m_pending.GetCount() > 0);
		m_pending.RemoveByItem(handle);

		OP_DELETE(handle);

		return OpStatus::OK;
	}

	if (!IsOpen() || handle->IsInvalid())
	{
		AbortRecord(handle);
		return OpStatus::ERR;
	}

	url = (const char *)handle->GetField("url").GetAddress();

	// finish preflushing if the URL being closed is being preflushed as well
	if ((m_flags & PreFlushing) != 0)
	{
		for (i = m_meta_pf.GetCount() - 1; i >= 0; --i)
		{
			if (ACT::WordsEqual(url, (const char *)m_meta_pf[i]->GetField("url").GetAddress()) == -1)
			{
				if ((err = PreFlush()) != OpBoolean::IS_TRUE)
				{
					AbortRecord(handle);
					return err;
				}

				break;
			}
		}
	}

	// find previous occurence of this URL
	if (OpStatus::IsError(err = FindLastIndex(&prev, &prev_idx, url, handle->m_index)))
	{
		OP_ASSERT(m_pending.GetCount() > 0);
		m_pending.RemoveByItem(handle);

		OP_DELETE(handle);

		return err;
	}

	// update chain of previous occurences
	if (prev != 0)
	{
		RETURN_IF_ERROR(handle->GetField("prev").SetValue(prev));
		RETURN_IF_ERROR(handle->GetField("prev_idx").SetValue(prev_idx));

		if (prev_idx == handle->m_index->m_id)
		{
			if (!handle->m_index->m_metadata.Read(&prev_hash, 2, ((OpFileLength)prev) * handle->m_index->m_metadata.GetBlockSize()))
			{
				AbortRecord(handle);
				return OpStatus::ERR;
			}
			handle->GetField("hash").GetValue(&hash);

			// ignore unchanged
			if (hash == prev_hash)
			{
				RemoveHandle(m_cache, handle);

				// nothing will get indexed except the time
				handle->m_word_count = 0;
				handle->m_word_list.Clear();
				handle->m_plaintext.Clear();
			}
		}
	}

	// insert words into caches
	if (OpStatus::IsError(err = InsertHandle(handle)))
	{
		AbortRecord(handle);
		return err;
	}

	// remove same URLs in cache
	for (i = m_meta.GetCount() - 1; i >= 0; --i)
	{
		RecordHandle meta = m_meta[i];
		if (ACT::WordsEqual(url, (const char *)meta->GetField("url").GetAddress()) == -1)
		{
			RemoveHandle(m_cache, meta);

			m_meta.Delete(i);
		}
	}

	// compress the plaintext
	OP_ASSERT(handle->GetField("plaintext").GetSize() == 0);
	if (OpStatus::IsError(err = CompressText(handle)))
	{
		AbortRecord(handle);
		return err;
	}

	if (OpStatus::IsError(m_meta.Add(handle)))
	{
		AbortRecord(handle);
		return OpStatus::ERR_NO_MEMORY;
	}

	if ((m_flags & (PreFlushing | PreFlushed | Flushed)) == 0 && m_meta.GetCount() == 1)
		PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, DELAY_MSG_PREFLUSH);

	OP_ASSERT(m_pending.GetCount() > 0);
	m_pending.RemoveByItem(handle);

	return OpStatus::OK;
}

void VisitedSearch::AbortRecord(VisitedSearch::RecordHandle handle)
{
	RemoveHandle(m_cache, handle);

	OP_ASSERT(m_pending.GetCount() > 0);
	m_pending.RemoveByItem(handle);

	OP_DELETE(handle);
}

OP_STATUS VisitedSearch::AssociateThumbnail(const char *url, const void *picture, int size)
{
	int i;
	ACT::WordID pos;
	char *tail_url;

	if (!IsOpen())
		return OpStatus::ERR;

	if ((m_flags & (PreFlushing | PreFlushed | Flushed)) != 0)
	{
		RETURN_IF_ERROR(Commit());
	}

	for (i = m_pending.GetCount() - 1; i >= 0; --i)
	{
		if (ACT::WordsEqual(url, (const char *)m_pending[i]->GetField("url").GetAddress()) == -1)
			return m_pending[i]->GetField("thumbnail").SetValue(picture, size);
	}

	for (i = m_meta.GetCount() - 1; i >= 0; --i)
	{
		if (ACT::WordsEqual(url, (const char *)m_meta[i]->GetField("url").GetAddress()) == -1)
			return m_meta[i]->GetField("thumbnail").SetValue(picture, size);
	}

	for (i = 0; (unsigned)i < m_index.GetCount(); ++i)
	{
		pos = m_index[i]->m_url.CaseSearch(url);

		RETURN_IF_ERROR(RankIndex::GetTail(&tail_url, pos, m_index[i]));
		if (ACT::WordsEqual(tail_url, url) != -1)
			pos = 0;
		OP_DELETEA(tail_url);

		if (pos != 0)
		{
			OP_STATUS err;
			BSCursor cursor(&m_index[i]->m_metadata);

			RETURN_IF_ERROR(RankIndex::SetupCursor(cursor));

			RETURN_IF_ERROR(cursor.Goto(pos));

			RETURN_IF_ERROR(cursor["thumbnail"].SetValue(picture, size));

			RETURN_IF_ERROR(m_index[i]->m_metadata.BeginTransaction());

			if (OpStatus::IsError(err = cursor.Flush()))
				OpStatus::Ignore(m_index[i]->m_metadata.Rollback());
			else if (OpStatus::IsError(err = m_index[i]->m_metadata.Commit()))
				OpStatus::Ignore(m_index[i]->m_metadata.Rollback());

			return err;
		}
	}

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS VisitedSearch::AssociateFileName(const char *url, const uni_char *fname)
{
	int i;
	ACT::WordID pos;
	char *tail_url;

	if (!IsOpen())
		return OpStatus::ERR;

	if ((m_flags & (PreFlushing | PreFlushed | Flushed)) != 0)
	{
		RETURN_IF_ERROR(Commit());
	}

	for (i = m_pending.GetCount() - 1; i >= 0; --i)
	{
		if (ACT::WordsEqual(url, (const char *)m_pending[i]->GetField("url").GetAddress()) == -1)
			return m_pending[i]->GetField("filename").SetStringValue(fname);
	}

	for (i = m_meta.GetCount() - 1; i >= 0; --i)
	{
		if (ACT::WordsEqual(url, (const char *)m_meta[i]->GetField("url").GetAddress()) == -1)
			return m_meta[i]->GetField("filename").SetStringValue(fname);
	}

	for (i = 0; (unsigned)i < m_index.GetCount(); ++i)
	{
		pos = m_index[i]->m_url.CaseSearch(url);

		RETURN_IF_ERROR(RankIndex::GetTail(&tail_url, pos, m_index[i]));
		if (ACT::WordsEqual(tail_url, url) != -1)
			pos = 0;
		OP_DELETEA(tail_url);

		if (pos != 0)
		{
			OP_STATUS err;
			BSCursor cursor(&m_index[i]->m_metadata);

			RETURN_IF_ERROR(RankIndex::SetupCursor(cursor));

			RETURN_IF_ERROR(cursor.Goto(pos));

			RETURN_IF_ERROR(cursor["filename"].SetStringValue(fname));

			RETURN_IF_ERROR(m_index[i]->m_metadata.BeginTransaction());

			if (OpStatus::IsError(err = cursor.Flush()))
				OpStatus::Ignore(m_index[i]->m_metadata.Rollback());
			else if (OpStatus::IsError(err = m_index[i]->m_metadata.Commit()))
				OpStatus::Ignore(m_index[i]->m_metadata.Rollback());

			return err;
		}
	}

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

/*****************************************************************************
 flush */

void VisitedSearch::CancelPreFlushItem(int i)
{
	RemoveHandle(m_preflush, m_meta_pf[i]);
	m_meta_pf.Delete(i);
}

OP_STATUS VisitedSearch::WriteMetadata(void)
{
	OP_BOOLEAN err;
	int i, j;
	BSCursor previous;
	UINT32 prev;
	UINT16 prev_idx;
	UINT32 last_visited;
#ifdef _DEBUG
	int prev_size;
#endif

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "WriteMetadata", "");
#endif

	RETURN_IF_ERROR(RankIndex::SetupCursor(previous));

	for (i = m_meta_pf.GetCount() - 1; i >= 0; --i)
	{
		RecordHandle meta = m_meta_pf[i];
		meta->GetField("prev").GetValue(&prev);
		if (prev != 0)
		{
			meta->GetField("prev_idx").GetValue(&prev_idx);
			j = 0;
			RankIndex *index = 0;

			while (j < (int)m_index.GetCount() && (index = m_index[j])->m_id != prev_idx)
				++j;

			if (j >= (int)m_index.GetCount() || index->m_metadata.GetFileSize() / index->m_metadata.GetBlockSize() <= prev )
			{
				OP_ASSERT(0);
				CancelPreFlushItem(i);
				continue;
			}

			previous.SetStorage(&(index->m_metadata));

#ifdef _DEBUG
			prev_size = index->m_metadata.DataLength(index->m_metadata.GetBlockSize() * ((OpFileLength)prev));
#endif
			err = previous.Goto(prev);

			if (err == OpStatus::ERR_PARSING_FAILED)
			{
				OP_ASSERT(0);
				CancelPreFlushItem(i);
				continue;
			}
			else
				RETURN_IF_ERROR(err);

			if (meta->m_word_count == 0)
			{
				// update visited time and alldoc
				previous["visited"].GetValue(&last_visited);
				err = index->m_alldoc.Delete(IdTime(last_visited, prev));
				if (err == OpBoolean::IS_FALSE && !index->m_alldoc.Empty())
					err = OpStatus::OK;
				OP_ASSERT(err != OpStatus::OK);
				if (err == OpBoolean::IS_FALSE)
					err = OpStatus::ERR_NO_SUCH_RESOURCE;
				if (err != OpBoolean::IS_TRUE)
					return err;

				meta->GetField("visited").GetValue(&last_visited);
				RETURN_IF_ERROR(previous["visited"].SetValue(last_visited));
				RETURN_IF_ERROR(previous.Flush());

#ifdef _DEBUG
				OP_ASSERT(previous.GetID() == prev);
				OP_ASSERT(index->m_metadata.DataLength(index->m_metadata.GetBlockSize() * ((OpFileLength)prev)) == prev_size);
#endif

				RETURN_IF_ERROR(meta->m_index->m_alldoc.Insert(IdTime(last_visited, prev)));

				CancelPreFlushItem(i);
				continue;
			}
			else {
				// setup prev/next
				if (previous["filename"].GetSize() > 0)
				{
//					sparc?
//					RETURN_IF_ERROR(meta->GetField("filename").SetStringValue((const uni_char *)previous["filename"].GetAddress()));
					RETURN_IF_ERROR(meta->GetField("filename").SetValue(previous["filename"].GetAddress(), previous["filename"].GetSize()));
				}
				RETURN_IF_ERROR(meta->GetField("invalid").SetValue(0));  // *(const unsigned char *)previous["invalid"].GetAddress()

				RETURN_IF_ERROR(meta->Flush());

				meta->GetField("visited").GetValue(&last_visited);
				OP_ASSERT((UINT32)meta->GetID() != 0);
				RETURN_IF_ERROR(meta->m_index->m_alldoc.Insert(IdTime(last_visited, (UINT32)meta->GetID())));

				RETURN_IF_ERROR(previous["next_idx"].SetValue(meta->m_index->m_id));
				RETURN_IF_ERROR(previous["next"].SetValue(meta->GetID()));

				RETURN_IF_ERROR(meta->m_index->m_url.AddCaseWord((const char *)meta->GetField("url").GetAddress(),
					(ACT::WordID)(meta->GetID())));
			}

			RETURN_IF_ERROR(previous.Flush());

#ifdef _DEBUG
			OP_ASSERT(previous.GetID() == prev);
			OP_ASSERT(index->m_metadata.DataLength(index->m_metadata.GetBlockSize() * ((OpFileLength)prev)) == prev_size);
#endif
		}
		else {
			RETURN_IF_ERROR(meta->Flush());

			meta->GetField("visited").GetValue(&last_visited);
			OP_ASSERT((UINT32)meta->GetID() != 0);
			RETURN_IF_ERROR(meta->m_index->m_alldoc.Insert(IdTime(last_visited, (UINT32)meta->GetID())));

			RETURN_IF_ERROR(meta->m_index->m_url.AddCaseWord((const char *)meta->GetField("url").GetAddress(),
				(ACT::WordID)(meta->GetID())));
		}
	}

	return OpStatus::OK;
}

OP_BOOLEAN VisitedSearch::PreFlush(int max_ms)
{
	int i;
	OP_STATUS err;
	UINT32 old_pos;
	double time_limit;
	unsigned wb_block_size;
	FileWord *preflush_item;

	if (!IsOpen())
		return OpStatus::ERR;

	if ((m_flags & PreFlushing) == 0 && m_meta.GetCount() == 0)
	{
		m_flags |= PreFlushed;
		m_flags &= ~PreFlushing;
		return OpBoolean::IS_TRUE;
	}

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "PreFlush", "(%i) %i", max_ms, m_pos);
#endif

	time_limit = g_op_time_info->GetWallClockMS() + max_ms;

	for (i = 0; i < (int)m_meta.GetCount(); ++i)
	{
		RankIndex *index = m_meta[i]->m_index;
		if (!index->m_metadata.InTransaction())
		{
			RETURN_IF_ERROR(index->m_wordbag.BeginTransaction());
			RETURN_IF_ERROR(index->m_metadata.BeginTransaction());

			BOOL flUseNUR = (m_flags & UseNUR) != 0;
			index->m_act.SetNURFlush(flUseNUR);
			index->m_alldoc.SetNURFlush(flUseNUR);
			index->m_url.SetNURFlush(flUseNUR);
		}
	}

	// prepare caches
	if ((m_flags & PreFlushing) == 0)
	{
		m_preflush.TakeOver(m_cache);

		m_meta_pf.TakeOver(m_meta);

		m_flags |= PreFlushing;

		// metadata and document list
		if (OpStatus::IsError(err = WriteMetadata()))
			return AbortPreFlush(err);

		m_pos = m_preflush.GetCount();
	}

	if (max_ms > 0 && g_op_time_info->GetWallClockMS() >= time_limit)
		return OpBoolean::IS_FALSE;

	wb_block_size = m_index[0]->m_wordbag.GetBlockSize();

	// insert
	while (--m_pos >= 0)
	{
		TVector<RankId> rank_vec(&RankId::CompareId);

		preflush_item = m_preflush[m_pos];

#ifdef _DEBUG
		for (unsigned debi = 0; debi < preflush_item->file_ids->GetCount(); ++debi)
		{
			const uni_char *word = preflush_item->word;
			const RecordHandleRec *handle = preflush_item->file_ids->Get(debi).h;
			uni_char *str, *title = NULL;
			OpString url;
			UniCompressor uc;

			unsigned len = uc.Length((const unsigned char *)handle->GetField("plaintext").GetAddress()) + 1;
			str = OP_NEWA(uni_char, len);
			if (str)
			{
				uc.Decompress(str, (const unsigned char *)handle->GetField("plaintext").GetAddress(), handle->GetField("plaintext").GetSize());
				uni_strupr(str);
			}

			uni_char *upper = handle->GetField("title").CopyStringValue(&title);
			if (upper)
				uni_strupr(upper);
			if (OpStatus::IsError(err = url.SetFromUTF8((const char *)handle->GetField("url").GetAddress())) || !str || !upper)
			{
				OP_DELETEA(title);
				OP_DELETEA(str);
				return AbortPreFlush(err);
			}
			url.MakeUpper();

			// (const uni_char *)(preflush_item->file_ids->Get(debi).h->GetField("plaintext").GetAddress()), preflush_item->word
			PhraseMatcher matcher;
			if (uni_strnicmp(word, "site:", 5) == 0)
			{
				if (OpStatus::IsSuccess(matcher.Init(word+5, PhraseMatcher::NoPhrases)))
					OP_ASSERT(matcher.Matches(url.CStr()));
			}
			else
			{
				if (OpStatus::IsSuccess(matcher.Init(word, PhraseMatcher::NoPhrases)))
					OP_ASSERT(matcher.Matches(str) || matcher.Matches(title) || matcher.Matches(url.CStr()));
			}

			OP_DELETEA(title);
			OP_DELETEA(str);
		}
#endif

		preflush_item->file_pos = preflush_item->index->m_act.Search(preflush_item->word);

		// stopword
		if (preflush_item->file_pos == (UINT32)-1)
			continue;

		OP_ASSERT(preflush_item->file_ids->GetCount() > 0);

		if (OpStatus::IsError(err = rank_vec.Reserve(preflush_item->file_ids->GetCount())))
			return AbortPreFlush(err);

		for (i = preflush_item->file_ids->GetCount() - 1; i >= 0; --i)
		{
			OP_ASSERT((UINT32)preflush_item->file_ids->Get(i).h->GetID() != 0);
#ifdef _DEBUG
			if (preflush_item->index->m_metadata.IsStartBlocksSupported())
			{
				OP_ASSERT(preflush_item->index->m_metadata.IsStartBlock(((OpFileLength)preflush_item->file_ids->Get(i).h->GetID()) * preflush_item->index->m_metadata.GetBlockSize()));
			}
#endif
			if (OpStatus::IsError(err = rank_vec.Add(RankId(preflush_item->file_ids->Get(i).rank, (UINT32)preflush_item->file_ids->Get(i).h->GetID()))))
				return AbortPreFlush(err);
		}

		old_pos = preflush_item->file_pos;

		if (preflush_item->index->m_doc_count > STOPWORD_DOC_COUNT && old_pos != 0 &&
			preflush_item->index->m_wordbag.DataLength(old_pos * wb_block_size) / sizeof(RankId) > preflush_item->index->m_doc_count / 2)
		{
			if (!preflush_item->index->m_wordbag.Delete(old_pos * wb_block_size))
				return AbortPreFlush(OpStatus::ERR_NO_DISK);

			preflush_item->file_pos = (UINT32)(-1);  // mark the stopword
			rank_vec.Clear();
		}
		// add the file ids to m_wordbag                                                     here
		else if ((preflush_item->file_pos = (UINT32)(preflush_item->index->m_wordbag.Append(rank_vec.Ptr(), rank_vec.GetCount() * sizeof(RankId),
			old_pos * wb_block_size) /	wb_block_size)) == 0)
			return AbortPreFlush(OpStatus::ERR_NO_DISK);

		OP_ASSERT(preflush_item->file_pos != 0);

		// update ACT for new words or if the position changed
		if (old_pos != preflush_item->file_pos)
		{
			err = preflush_item->index->m_act.AddWord(preflush_item->word, preflush_item->file_pos);
			switch (err)
			{
			case OpBoolean::IS_FALSE:
			case OpBoolean::IS_TRUE:
				break;
			case OpStatus::OK:  // empty/invalid word
				if (preflush_item->index->m_wordbag.Delete(preflush_item->file_pos))
					break;
				err = OpStatus::ERR_NO_DISK;
			default:
				return AbortPreFlush(err);
			}

			// update new data for this word in m_cache
			i = m_cache.Search(preflush_item);
			if (i < (int)m_cache.GetCount() && uni_strcmp(preflush_item->word, m_cache[i]->word) == 0)
				m_cache[i]->file_pos = preflush_item->file_pos;
		}

		if (max_ms > 0 && g_op_time_info->GetWallClockMS() >= time_limit)
			return OpBoolean::IS_FALSE;
	}

	for (i = 0; i < (int)m_index.GetCount(); ++i)
	{
		RankIndex *index = m_index[i];
		if (index->m_metadata.InTransaction())
		{
			if ((err = index->m_act.Flush(BSCache::JournalAll)) != OpBoolean::IS_TRUE)
				return AbortPreFlush(err);

			if ((err = index->m_alldoc.Flush(BSCache::JournalAll)) != OpBoolean::IS_TRUE)
				return AbortPreFlush(err);

			if ((err = index->m_url.Flush(BSCache::JournalAll)) != OpBoolean::IS_TRUE)
				return AbortPreFlush(err);
		}
	}

	m_flags |= PreFlushed;
	m_flags &= ~PreFlushing;

	return OpBoolean::IS_TRUE;
}

// discard the data being written
OP_STATUS VisitedSearch::AbortPreFlush(OP_STATUS err)
{
	int i;

	OP_ASSERT(0);  // this could normally happen only on out of memory or disk

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Warning, "AbortPreFlush", "(%i)", err);
#endif

	m_meta_pf.Clear();

	for (i = 0; i < (int)m_index.GetCount(); ++i)
	{
		RankIndex *index = m_index[i];
		if (index->m_metadata.InTransaction())
			OpStatus::Ignore(index->Rollback());
	}

	m_preflush.Clear();

	m_flags &= ~PreFlushing;

	return err;
}


OP_STATUS VisitedSearch::Flush(int max_ms)
{
	int i;
	OP_STATUS err;
	double time_limit;

	if (!IsOpen())
		return OpStatus::ERR;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "Flush", "(%i) %i", max_ms, m_index.GetCount());
#endif

	time_limit = g_op_time_info->GetWallClockMS() + max_ms;

	// call PreFlush if it wasn't called
	if ((m_flags & PreFlushed) == 0 || (m_flags & PreFlushing) != 0)
	{
		m_flags |= UseNUR;
		err = PreFlush(max_ms);
		m_flags ^= UseNUR;

		RETURN_IF_ERROR(err);

		if (err == OpBoolean::IS_FALSE)
			return err;
	}

	for (i = 0; i < (int)m_index.GetCount(); ++i)
	{
		RankIndex *index = m_index[i];
		if (index->m_metadata.InTransaction())
		{
			if (index->m_alldoc.GetItemCount() > 0)
			{
				switch (err = index->m_alldoc.Flush(BSCache::ReleaseAll, (int)(time_limit - g_op_time_info->GetWallClockMS())))
				{
				case OpBoolean::IS_FALSE:
					return OpBoolean::IS_FALSE;
				case OpBoolean::IS_TRUE:
					break;
				default:
					return AbortFlush(err);
				}
			}

			if (index->m_act.GetItemCount() > 0)
			{
				// This closes the journal files
				switch (err = index->m_act.Flush(BSCache::ReleaseAll, (int)(time_limit - g_op_time_info->GetWallClockMS())))
				{
				case OpBoolean::IS_FALSE:
					return OpBoolean::IS_FALSE;
				case OpBoolean::IS_TRUE:
					break;
				default:
					return AbortFlush(err);
				}

				if (OpStatus::IsError(err = index->m_act.SaveStatus()))
					return AbortFlush(err);
			}

			if (index->m_url.GetItemCount() > 0)
			{
				// This closes the journal files
				switch (err = index->m_url.Flush(BSCache::ReleaseAll, (int)(time_limit - g_op_time_info->GetWallClockMS())))
				{
				case OpBoolean::IS_FALSE:
					return OpBoolean::IS_FALSE;
				case OpBoolean::IS_TRUE:
					break;
				default:
					return AbortFlush(err);
				}
			}
		}
	}


	m_preflush.Clear();

	for (i = m_meta_pf.GetCount() - 1; i >= 0; --i)
		++(m_meta_pf[i]->m_index->m_doc_count);

	m_meta_pf.Clear();

	m_flags ^= PreFlushed;
	m_flags |= Flushed;

	return OpBoolean::IS_TRUE;
}

// discard the data being written
OP_STATUS VisitedSearch::AbortFlush(OP_STATUS err)
{
	int i;

	OP_ASSERT(0);  // this could normally happen only on out of memory or disk

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Warning, "AbortFlush", "(%i)", err);
#endif

	m_meta_pf.Clear();  // Clear m_meta_pf without deleting the items

	for (i = 0; i < (int)m_index.GetCount(); ++i)
	{
		RankIndex *index = m_index[i];
		if (index->m_metadata.InTransaction())
			OpStatus::Ignore(index->Rollback());
	}

	m_preflush.Clear();

	m_flags &= ~PreFlushed;

	return err;
}

OP_STATUS VisitedSearch::Commit(void)
{
	int i;
	OpFileLength subsize;
	TVector<int> subdirs;

	if (!IsOpen())
		return OpStatus::ERR;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "Commit", "");
#endif

	if ((m_flags & Flushed) == 0)
	{
		RETURN_IF_ERROR(Flush());
	}

	for (i = 0; i < (int)m_index.GetCount(); ++i)
	{
		RankIndex *index = m_index[i];
		if (index->m_metadata.InTransaction())
		{
			RETURN_IF_ERROR(index->m_act.Commit());
			RETURN_IF_ERROR(index->m_wordbag.Commit());
			RETURN_IF_ERROR(index->m_alldoc.Commit());
			RETURN_IF_ERROR(index->m_metadata.Commit());
			RETURN_IF_ERROR(index->m_url.Commit());
		}
	}

	m_flags ^= Flushed;

	subsize = m_index[0]->Size();

	if (subsize >= m_subindex_size)
	{
		OpString path;
		RankIndex *r;

		if (OpStatus::IsError(path.Set(m_index[0]->m_wordbag.GetFullName())))
			return OpStatus::OK;  // prefer to report a successfull data commit

		path.Delete(path.Length() - 6 - (int)op_strlen(FNAME_WB));  // the main directory

		RETURN_VALUE_IF_ERROR(subdirs.Reserve(m_index.GetCount()), OpStatus::OK);

		for (i = m_index.GetCount() - 1; i >= 0; --i)
			RETURN_VALUE_IF_ERROR(subdirs.Add(m_index[i]->m_id), OpStatus::OK);

		RETURN_VALUE_IF_ERROR(subdirs.Sort(), OpStatus::OK);

		if (subdirs[0] != 0)
			i = 0;
		else {
			i = 1;
			while (i < (int)subdirs.GetCount() && subdirs[i - 1] + 1 == subdirs[i])
				++i;
		}

		if ((r = OP_NEW(RankIndex, ())) == NULL)
			return OpStatus::OK;

		if (OpStatus::IsError(r->Open(path.CStr(), i)))
			OP_DELETE(r);
		else if (OpStatus::IsError(m_index.Insert(0, r)))
		{
			r->Close();
			OP_DELETE(r);
		}
	}

	CheckMaxSize_RemoveOldIndexes();

	return OpStatus::OK;
}

/*****************************************************************************
 insert utility */

// cache is m_word_cache and m_backup_cache if Flush is in progress (PreFlushing is set)
// word must already be in uppercase
OP_STATUS VisitedSearch::Insert(TVector<FileWord *> &cache, RecordHandle h, const uni_char *word, float rank)
{
	int i;
	FileWord *w, fw;
	const uni_char **srch = &word;;

	fw.word = const_cast<uni_char*>(word);
	i = cache.Search(&fw);
	fw.word = NULL; // Avoid OP_DELETE
	if (i >= (int)cache.GetCount() || FileWord::LessThan(&srch, &(cache[i])))  // not found
	{
		RETURN_OOM_IF_NULL(w = FileWord::Create(word, rank, h));

		OP_STATUS err;
		if (OpStatus::IsError(err = cache.Insert(i, w)))
		{
			OP_DELETE(w);
			return err;
		}
	}
	else {
		RETURN_IF_ERROR(cache[i]->Add(rank, h));
	}

	return OpStatus::OK;
}


/*****************************************************************************
 search */

VisitedSearch::Result::Result(void)
{
	url = NULL;
	title = NULL;
	thumbnail = NULL;
	thumbnail_size = 0;
	filename = NULL;
	compressed_plaintext = NULL;
	compressed_plaintext_size = 0;
	plaintext = NULL;
	visited = 0;
	ranking = 0;

	id = (UINT32)(-1);
	invalid = FALSE;
	prev_idx = 0;
	prev = 0;
	next_idx = 0;
	next = 0;
}

static char *op_ndup(const char *s)
{
	char *dup;
	size_t len = op_strlen(s) + 1;

	if ((dup = OP_NEWA(char, len)) == NULL)
		return NULL;

	op_memcpy(dup, s, len);

	return dup;
}

static uni_char *uni_ndup(const uni_char *s)
{
	uni_char *dup;
	size_t len = uni_strlen(s) + 1;

	if ((dup = OP_NEWA(uni_char, len)) == NULL)
		return NULL;

	op_memcpy(dup, s, len << 1);

	return dup;
}

OP_STATUS VisitedSearch::Result::Assign(void *dst_, const void *src_)
{
	VisitedSearch::Result *dst = static_cast<VisitedSearch::Result *>(dst_);
	const VisitedSearch::Result *src = static_cast<const VisitedSearch::Result *>(src_);

	dst->url = NULL;
	if (src->url != NULL)
		RETURN_OOM_IF_NULL(dst->url = op_ndup(src->url));

	dst->title = NULL;
	if (src->title != NULL &&
		(dst->title = uni_ndup(src->title)) == NULL)
	{
		DeleteResult(dst);
		return OpStatus::ERR_NO_MEMORY;
	}


	dst->thumbnail = NULL;
	dst->thumbnail_size = src->thumbnail_size;
	if (src->thumbnail != NULL)
	{
		if ((dst->thumbnail = OP_NEWA(unsigned char, src->thumbnail_size)) == NULL)
		{
			DeleteResult(dst);
			return OpStatus::ERR_NO_MEMORY;
		}
	 	op_memcpy(dst->thumbnail, src->thumbnail, src->thumbnail_size);
	}

	dst->filename = NULL;
	if (src->filename != NULL &&
		(dst->filename = uni_ndup(src->filename)) == NULL)
	{
		DeleteResult(dst);
		return OpStatus::ERR_NO_MEMORY;
	}

	dst->compressed_plaintext = NULL;
	dst->compressed_plaintext_size = src->compressed_plaintext_size;
	if (src->compressed_plaintext != NULL)
	{
		if ((dst->compressed_plaintext = OP_NEWA(unsigned char, src->compressed_plaintext_size)) == NULL)
		{
			DeleteResult(dst);
			return OpStatus::ERR_NO_MEMORY;
		}
	 	op_memcpy(dst->compressed_plaintext, src->compressed_plaintext, src->compressed_plaintext_size);
	}

	dst->plaintext = NULL;
	if (src->plaintext != NULL &&
		(dst->plaintext = uni_ndup(src->plaintext)) == NULL)
	{
		DeleteResult(dst);
		return OpStatus::ERR_NO_MEMORY;
	}

	dst->visited = src->visited;
	dst->ranking = src->ranking;

	dst->id = src->id;
	dst->invalid = src->invalid;
	dst->prev_idx = src->prev_idx;
	dst->prev = src->prev;
	dst->next_idx = src->next_idx;
	dst->next = src->next;

	return OpStatus::OK;
}

OP_STATUS VisitedSearch::Result::SetCompressedPlaintext(const unsigned char *buf, int size)
{
	compressed_plaintext = NULL;
	compressed_plaintext_size = size;
	if (size > 0)
	{
		RETURN_OOM_IF_NULL(compressed_plaintext = OP_NEWA(unsigned char, size));
	 	op_memcpy(compressed_plaintext, buf, size);
	}
	return OpStatus::OK;
}

uni_char *VisitedSearch::Result::GetPlaintext() const
{
	if (plaintext || !compressed_plaintext)
		return plaintext;

	UniCompressor uc;

	unsigned len = uc.Length(compressed_plaintext) + 1;
	plaintext = OP_NEWA(uni_char, len);
	if (!plaintext)
		return NULL;

	if (uc.Decompress(plaintext, compressed_plaintext, compressed_plaintext_size) == 0)
	{
		OP_ASSERT(0);  // error in compressed data?
		OP_DELETEA(plaintext);
		plaintext = NULL;
	}

	OP_DELETEA(compressed_plaintext);
	compressed_plaintext = NULL;
	compressed_plaintext_size = 0;

	return plaintext;
}

OP_STATUS VisitedSearch::Result::ReadResult(VisitedSearch::Result &res, BlockStorage *metadata)
{
	BSCursor c(metadata);

	RETURN_IF_ERROR(RankIndex::SetupCursor(c));

	RETURN_IF_ERROR(c.Goto(res.id));

	if (c["url"].CopyStringValue(&(res.url)) == NULL)
	{
		OP_ASSERT(c["url"].GetSize() > 0);
		return OpStatus::ERR_NO_MEMORY;
	}

	res.title = NULL;
	if (c["title"].GetSize() > 0)
		RETURN_OOM_IF_NULL(c["title"].CopyStringValue(&(res.title)));

	res.filename = NULL;
	if (c["filename"].GetSize() > 0)
		RETURN_OOM_IF_NULL(c["filename"].CopyStringValue(&(res.filename)));

	res.compressed_plaintext = NULL;
	res.compressed_plaintext_size = 0;
	res.plaintext = NULL;
	if (c["plaintext"].GetSize() > 0)
	{
		RETURN_IF_ERROR(res.SetCompressedPlaintext((const unsigned char *)c["plaintext"].GetAddress(), c["plaintext"].GetSize()));
	}

	res.thumbnail = NULL;
	res.thumbnail_size = 0;
	if (c["thumbnail"].GetSize() > 0)
	{
		res.thumbnail_size = c["thumbnail"].GetSize();
		RETURN_OOM_IF_NULL(res.thumbnail = OP_NEWA(unsigned char, res.thumbnail_size));
		c["thumbnail"].GetValue(res.thumbnail, res.thumbnail_size);
	}

	c["visited"].GetValue(&(res.visited));
	c["invalid"].GetValue(&(res.invalid));

	c["prev_idx"].GetValue(&(res.prev_idx));
	c["prev"].GetValue(&(res.prev));
	c["next_idx"].GetValue(&(res.next_idx));
	c["next"].GetValue(&(res.next));

	return OpStatus::OK;
}

BOOL VisitedSearch::Result::Later(const void *left_, const void *right_)
{
	const VisitedSearch::Result *left = static_cast<const VisitedSearch::Result *>(left_);
	const VisitedSearch::Result *right = static_cast<const VisitedSearch::Result *>(right_);

	if (left->visited == right->visited)
	{
		if (left->id == (UINT32)-1 ||
			right->id == (UINT32)-1)
			return op_strcmp(left->url, right->url) != 0;
		return left->id > right->id;
	}

	return left->visited > right->visited;
}

BOOL VisitedSearch::Result::CompareId(const void *left_, const void *right_)
{
	const VisitedSearch::Result *left = static_cast<const VisitedSearch::Result *>(left_);
	const VisitedSearch::Result *right = static_cast<const VisitedSearch::Result *>(right_);

	return left->id > right->id;
}

void VisitedSearch::Result::DeleteResult(void *item_)  // fields only get deleted when Result is destructed from the internal Vector
{
	VisitedSearch::Result *item = static_cast<VisitedSearch::Result *>(item_);
	if (item->url != NULL)
	{
		OP_DELETEA(item->url);
		item->url = NULL;
	}
	if (item->title != NULL)
	{
		OP_DELETEA(item->title);
		item->title = NULL;
	}
	if (item->thumbnail != NULL)
	{
		OP_DELETEA(item->thumbnail);
		item->thumbnail = NULL;
	}
	if (item->filename != NULL)
	{
		OP_DELETEA(item->filename);
		item->filename = NULL;
	}
	if (item->plaintext != NULL)
	{
		OP_DELETEA(item->plaintext);
		item->plaintext = NULL;
	}
	if (item->compressed_plaintext != NULL)
	{
		OP_DELETEA(item->compressed_plaintext);
		item->compressed_plaintext = NULL;
	}
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t VisitedSearch::Result::EstimateMemoryUsed(const void *item_)
{
	const VisitedSearch::Result *item = reinterpret_cast<const VisitedSearch::Result *>(item_);
	size_t sum = 0;
	if (item->url)
		sum += (op_strlen(item->url)+1)*sizeof(char) + 2*sizeof(size_t);
	if (item->title)
		sum += (uni_strlen(item->title)+1)*sizeof(uni_char) + 2*sizeof(size_t);
	if (item->thumbnail)
		sum += item->thumbnail_size*sizeof(unsigned char) + 2*sizeof(size_t);
	if (item->filename != NULL)
		sum += (uni_strlen(item->filename)+1)*sizeof(uni_char) + 2*sizeof(size_t);
	if (item->compressed_plaintext != NULL)
		sum += item->compressed_plaintext_size*sizeof(unsigned char) + 2*sizeof(size_t);
	if (item->plaintext != NULL)
		sum += (uni_strlen(item->plaintext)+1)*sizeof(uni_char) + 2*sizeof(size_t);

	return sum +
		sizeof(item->url) +
		sizeof(item->title) +
		sizeof(item->thumbnail) +
		sizeof(item->thumbnail_size) +
		sizeof(item->filename) +
		sizeof(item->compressed_plaintext) +
		sizeof(item->compressed_plaintext_size) +
		sizeof(item->plaintext) +
		sizeof(item->visited) +
		sizeof(item->ranking) +
		sizeof(item->id) +
		sizeof(item->invalid) +
		sizeof(item->prev_idx) +
		sizeof(item->prev) +
		sizeof(item->next_idx) +
		sizeof(item->next);
}
#endif

SearchIterator<VisitedSearch::Result> *VisitedSearch::Search(const uni_char *text, Sort sort_by, int phrase_flags)
{
	QueryIteratorBase *tmp_it;
	AllDocIterator *ad_it;
	TVector<SearchIterator<VisitedSearch::Result> *> sub_results(&PtrDescriptor<SearchIterator<VisitedSearch::Result> >::Destruct);
	MultiOrIterator *mo;
	SearchIterator<VisitedSearch::Result> *result = NULL, *tmp_result;
	WordSegmenter msgtok;
	const uni_char *s_tmp;
	TVector<uni_char *> *words;
	int i, j;
	UINT32 idx;
	CacheIterator *cit;

	if (!IsOpen())
		return NULL;

	if ((m_flags & (PreFlushing /*| PreFlushed | Flushed*/)) != 0)
	{
		switch (Commit())
		{
		case OpStatus::OK:
			break;
		case OpStatus::ERR_NO_DISK:
			AbortFlush();
			break;
		default:
			OP_ASSERT(0);  // this is not supposed to happen; may be out of memory?
			return NULL;
		}
	}

	RETURN_VALUE_IF_ERROR(sub_results.Reserve(m_index.GetCount() + (m_cache.GetCount() > 0)), NULL);

	if (uni_strni_eq_upper(text, "SITE:", 5) && op_isalpha(text[5]))
		s_tmp = text;
	else if ((s_tmp = uni_stristr(text, " SITE:")) != NULL && op_isalpha(s_tmp[6]))
		++s_tmp;
	else
		s_tmp = NULL;

	OpString querytext;
	RETURN_VALUE_IF_ERROR(querytext.Set(text), NULL);

	if (s_tmp != NULL)
	{
		OpString sitetext;
		const uni_char *sitestart, *siteend;
		uni_char *sitedup;

		sitestart = querytext.CStr() + (s_tmp - text);

		do {
			siteend = sitestart + 5;
			while (op_isalpha(*siteend) || *siteend == '.')
				++siteend;

			if ((s_tmp = uni_stristr(siteend, " SITE:")) != NULL && op_isalpha(s_tmp[6]))
			{
				++s_tmp;

				querytext.Delete((int)(sitestart - querytext.CStr()), (int)(siteend - sitestart));
				sitestart = uni_stristr(querytext.CStr(), " SITE:") + 1;
			}
			else
				s_tmp = NULL;
		} while (s_tmp != NULL);

		RETURN_VALUE_IF_ERROR(sitetext.Set(sitestart, (int)(siteend - sitestart)), NULL);
		querytext.Delete((int)(sitestart - querytext.CStr()), (int)(siteend - sitestart));

		RETURN_VALUE_IF_ERROR(msgtok.Set(querytext), NULL);

		if ((words = msgtok.Parse()) == NULL)
			return NULL;

		if ((sitedup = uni_ndup(sitetext.CStr())) == NULL)
		{
			OP_DELETE(words);
			return NULL;
		}

		if (OpStatus::IsError(words->Add(sitedup)))
		{
			OP_DELETEA(sitedup);
			OP_DELETE(words);
			return NULL;
		}
	}
	else {
		RETURN_VALUE_IF_ERROR(msgtok.Set(text), NULL);

		if ((words = msgtok.Parse()) == NULL)
			return NULL;
	}

	if (sort_by == Autocomplete)
	{
		if (words->GetCount() == 0)
		{
			OP_DELETE(words);
			return OP_NEW(EmptyIterator<VisitedSearch::Result>, ());
		}

		sort_by = RankSort;
	}

	// return all documents from all indexes
	if (words->GetCount() == 0)
	{
		for (i = m_index.GetCount() - 1; i >= 0; --i)
		{
			if ((ad_it = OP_NEW(AllDocIterator, (m_index[i]))) == NULL)
			{
				OP_DELETE(words);
				return NULL;
			}

			if (OpStatus::IsError(ad_it->Init()) ||
				OpStatus::IsError(sub_results.Add(ad_it)))
			{
				OP_DELETE(ad_it);
				OP_DELETE(words);
				return NULL;
			}
		}
	}
	else {
		for (i = m_index.GetCount() - 1; i >= 0; --i)
		{
			RankIndex *index = m_index[i];
			if (sort_by == RankSort)
				tmp_it = OP_NEW(RankIterator, (index));
			else
				tmp_it = OP_NEW(TimeIterator, (index));

			if (tmp_it == NULL)
			{
				OP_DELETE(words);
				return NULL;
			}

			for (j = words->GetCount() - 1; j >= 0; --j)
			{
				idx = index->m_act.Search(words->Get(j));

				// not found
				if (idx == 0)
					break;

				if (idx == (UINT32)-1)
					continue;

				if (OpStatus::IsError(tmp_it->AddWord(idx)))
				{
					OP_DELETE(tmp_it);
					OP_DELETE(words);
					return NULL;
				}
			}

			if (j >= 0)
			{
				OP_DELETE(tmp_it);
				continue;
			}

			if (OpStatus::IsError(tmp_it->Init()) ||
				OpStatus::IsError(sub_results.Add(tmp_it)))
			{
				OP_DELETE(tmp_it);
				OP_DELETE(words);
				return NULL;
			}
		}
	}

	if (m_cache.GetCount() > 0)
	{
		if ((cit = OP_NEW(CacheIterator, (sort_by == RankSort && words->GetCount() > 0 ? &DefDescriptor<VisitedSearch::Result>::Compare : &VisitedSearch::Result::Later))) == NULL)
		{
			OP_DELETE(words);
			return NULL;
		}

		for (i = (int)words->GetCount() - 1; i >= 0; --i)
			uni_strupr(words->Get(i));

		if (OpStatus::IsError(cit->Init(m_cache, words)))
		{
			OP_DELETE(words);
			OP_DELETE(cit);
			return NULL;
		}
	}
	else
		cit = NULL;

	OP_DELETE(words);

	// nothing found
	if (sub_results.GetCount() == 0)
	{
		if (cit == NULL)
			return OP_NEW(EmptyIterator<VisitedSearch::Result>, ());

		tmp_result = cit;
	}
	else
	{
		if (sub_results.GetCount() == 1 && cit == NULL)
		{
			tmp_result = sub_results.Remove(0);
		}
		else
		{
			if (cit != NULL)
				RETURN_VALUE_IF_ERROR(sub_results.Add(cit), NULL);

			if ((mo = OP_NEW(MultiOrIterator, ())) == NULL)
				return NULL;

			if (sort_by == RankSort)
				mo->Set(sub_results, &DefDescriptor<VisitedSearch::Result>::Compare);
			else
				mo->Set(sub_results, &VisitedSearch::Result::Later);
			tmp_result = mo;
		}
	}

#ifdef SEARCH_ENGINE_PHRASESEARCH
	if ((phrase_flags & PhraseMatcher::AllPhrases) != 0)
	{
		PhraseFilter<VisitedSearch::Result> *phrase_filter = OP_NEW(PhraseFilter<VisitedSearch::Result>, (querytext.CStr(), m_doc_source, phrase_flags));
		if(phrase_filter)
		{
			result = OP_NEW(FilterIterator<VisitedSearch::Result>, (tmp_result, phrase_filter));
			if (result == NULL)
			{
				OP_DELETE(phrase_filter);
				OP_DELETE(tmp_result);
			}
		}
	}
	else
		result = tmp_result;
#else
	result = tmp_result;
#endif

	return result;
}

SearchIterator<VisitedSearch::Result> *VisitedSearch::FastPrefixSearch(const uni_char *text, int phrase_flags)
{
	WordSegmenter ws;
	TVector<uni_char *> *parsed_text;
	BOOL last_is_prefix, prefix_search = TRUE;
	ChainIterator *it;
	CacheIterator *cit = NULL;
	SearchIterator<VisitedSearch::Result> *result = NULL;
	int i;

	RETURN_VALUE_IF_ERROR(ws.Set(text), NULL);
	if ((parsed_text = ws.Parse(&last_is_prefix)) == NULL)
		return NULL;
	if (!last_is_prefix)
		prefix_search = FALSE;

	for (i = (int)parsed_text->GetCount() - 1; i >= 0; --i)
		uni_strupr(parsed_text->Get(i));

	if (m_cache.GetCount() > 0 && parsed_text->GetCount() > 0)
	{
		if ((cit = OP_NEW(CacheIterator, (&VisitedSearch::Result::Later))) == NULL)
		{
			OP_DELETE(parsed_text);
			return NULL;
		}

		if (OpStatus::IsError(cit->Init(m_cache, parsed_text, prefix_search)))
		{
			OP_DELETE(parsed_text);
			OP_DELETE(cit);
			return NULL;
		}

		if (cit->End())
		{
			OP_DELETE(cit);
			cit = NULL;
		}
	}

	if ((it = OP_NEW(ChainIterator, ())) == NULL)
	{
		OP_DELETE(parsed_text);
		return NULL;
	}

	if (OpStatus::IsError(it->Init(*parsed_text, m_index, cit, prefix_search)))
	{
		OP_DELETE(parsed_text);
		OP_DELETE(it);
		return NULL;
	}

	OP_DELETE(parsed_text);

#ifdef SEARCH_ENGINE_PHRASESEARCH
	if ((phrase_flags & PhraseMatcher::AllPhrases) != 0)
	{
		PhraseFilter<VisitedSearch::Result> *phrase_filter = OP_NEW(PhraseFilter<VisitedSearch::Result>, (text, m_doc_source, phrase_flags | PhraseMatcher::PrefixSearch));
		if(phrase_filter)
		{
			result = OP_NEW(FilterIterator<VisitedSearch::Result>, (it, phrase_filter));
			if (result == NULL)
			{
				OP_DELETE(phrase_filter);
				OP_DELETE(it);
			}
		}
	}
	else
		result = it;
#else
	result = it;
#endif

	return result;
}

OP_STATUS VisitedSearch::InvalidateResult(const Result &row)
{
	unsigned i;
	OP_BOOLEAN found;
	IdTime tst;

	if (!IsOpen())
		return OpStatus::ERR;

	i = 0;

	while (i < m_index.GetCount())
	{
		tst.data[IDTIME_ID] = row.id;
		tst.data[IDTIME_VISITED] = (UINT32)row.visited;

		RETURN_IF_ERROR(found = m_index[i]->m_alldoc.Search(tst));
		if (found == OpBoolean::IS_TRUE)
			break;

		++i;
	}

	if (i >= m_index.GetCount())
		return OpStatus::OK;

	// transaction is not necessary, but progressing transaction is not broken
//	return m_index[i]->m_metadata.Update(&c_true, 6, 1, (OpFileLength)(row.id) * m_index[i]->m_metadata.GetBlockSize()) ? OpStatus::OK : OpStatus::ERR_NO_DISK;
	return WipeData(row.id, i);
}

OP_STATUS VisitedSearch::InvalidateUrl(const uni_char *url)
{
	OpString8 utf8_str;

	RETURN_IF_ERROR(utf8_str.SetUTF8FromUTF16(url));
	return InvalidateUrl(utf8_str.CStr());
}

OP_STATUS VisitedSearch::InvalidateUrl(const char *url)
{
	int i;
	ACT::WordID pos;
	char *tail_url;

	if (!IsOpen())
		return OpStatus::ERR;

	if ((m_flags & (PreFlushing | PreFlushed | Flushed)) != 0)
	{
		RETURN_IF_ERROR(Commit());
	}

	for (i = 0; (unsigned)i < m_index.GetCount(); ++i)
	{
		RankIndex *index = m_index[i];
		pos = index->m_url.CaseSearch(url);

		RETURN_IF_ERROR(RankIndex::GetTail(&tail_url, pos, index));
		if (ACT::WordsEqual(tail_url, url) != -1)
			pos = 0;
		OP_DELETEA(tail_url);

		if (pos != 0)
			RETURN_IF_ERROR(WipeData(pos, i));
	}

	return OpStatus::OK;
}

CHECK_RESULT(static OP_STATUS WipeCursor(BSCursor &cursor));
static OP_STATUS WipeCursor(BSCursor &cursor)
{
	int uc_len;
	UniCompressor uc;
	unsigned char nothing[8];  /* ARRAY OK 2010-09-24 roarl */

	uc_len = (int)uc.Compress(nothing, UNI_L(" "));
	OP_ASSERT(uc_len < (int)sizeof(nothing));

	RETURN_IF_ERROR(cursor["invalid"].SetValue(1));
	RETURN_IF_ERROR(cursor["hash"].SetValue(0));
	RETURN_IF_ERROR(cursor["next"].SetValue(0));
	RETURN_IF_ERROR(cursor["next_idx"].SetValue(0));
	RETURN_IF_ERROR(cursor["prev"].SetValue(0));
	RETURN_IF_ERROR(cursor["prev_idx"].SetValue(0));

	op_memset((char *)cursor["title"].GetAddress(), 0, cursor["title"].GetSize());
	op_memset((char *)cursor["plaintext"].GetAddress(), 0, cursor["plaintext"].GetSize());
	op_memset((char *)cursor["url"].GetAddress(), 0, cursor["url"].GetSize());

	if (cursor["plaintext"].GetSize() > uc_len)
		op_memcpy((unsigned char *)cursor["plaintext"].GetAddress(), nothing, uc_len);

	return cursor.Flush();
}

OP_STATUS VisitedSearch::WipeData(BSCursor::RowID id, unsigned short r_index)
{
	BSCursor cursor(&m_index[r_index]->m_metadata);
	BSCursor::RowID next, prev;
	unsigned short next_idx, prev_idx;

	RETURN_IF_ERROR(RankIndex::SetupCursor(cursor));

	RETURN_IF_ERROR(cursor.Goto(id));

	cursor["next"].GetValue(&next);
	cursor["next_idx"].GetValue(&next_idx);
	cursor["prev"].GetValue(&prev);
	cursor["prev_idx"].GetValue(&prev_idx);

	RETURN_IF_ERROR(WipeCursor(cursor));

	while (prev != 0 && prev_idx == r_index)
	{
		BSCursor bc(&m_index[r_index]->m_metadata);
		RETURN_IF_ERROR(RankIndex::SetupCursor(bc));
		RETURN_IF_ERROR(bc.Goto(prev));

		bc["prev"].GetValue(&prev);
		bc["prev_idx"].GetValue(&prev_idx);

		RETURN_IF_ERROR(WipeCursor(bc));
	}

	while (next != 0 && next_idx == r_index)
	{
		BSCursor bc(&m_index[r_index]->m_metadata);
		RETURN_IF_ERROR(RankIndex::SetupCursor(bc));
		RETURN_IF_ERROR(bc.Goto(next));

		bc["next"].GetValue(&next);
		bc["next_idx"].GetValue(&next_idx);

		RETURN_IF_ERROR(WipeCursor(bc));
	}

	return OpStatus::OK;
}


void VisitedSearch::CheckMaxSize_RemoveOldIndexes()
{
	int i, j;
	OpFileLength total_size = 0;

	for (i = m_index.GetCount() - 1; i >= 0; --i)
		total_size += m_index[i]->Size();

	i = m_index.GetCount() - 1;
	while (total_size > m_max_size && (i > 0 || (m_max_size == 0 && i == 0)))
	{
		RankIndex *r = m_index[i];
		if (r->m_metadata.InTransaction())
			return;

		// Check if the subindex is referenced
		// - ignore m_pending records... may be left behind by NSL documents, see CORE-29064
		for (j = m_meta.GetCount() - 1; j >= 0; --j)
			if (m_meta[j]->m_index == r)
				return;
		for (j = m_meta_pf.GetCount() - 1; j >= 0; --j)
			if (m_meta_pf[j]->m_index == r)
				return;

		// Remove it
		total_size -= r->Size();
		if (OpStatus::IsError(r->Clear()))
			return;
		for (j = m_pending.GetCount() - 1; j >= 0; --j)
			if (m_pending[j]->m_index == r)
				m_pending[j]->Invalidate(); // This index is gone

		m_index.Delete(i);
		--i;
	}
}

OP_STATUS VisitedSearch::WordSearch(const uni_char *prefix, uni_char **result, int *result_size)
{
	unsigned i;
	int max_result;

	if (!IsOpen())
		return OpStatus::ERR;

	max_result = *result_size;
	*result_size = 0;

	for (i = 0; i < m_index.GetCount() && max_result > *result_size; ++i)
		*result_size += m_index[i]->m_act.PrefixWords(result + *result_size, prefix, max_result - *result_size);

	return OpStatus::OK;
}


void VisitedSearch::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (par1 != (MH_PARAM_1)this)
		return;

	if (!IsOpen())
		return;

	switch (msg)
	{
	case MSG_VISITEDSEARCH_PREFLUSH:
		if ((m_flags & Flushed) != 0)
		{
			if (m_meta.GetCount() > 0)
				PostMessage(MSG_VISITEDSEARCH_COMMIT, (MH_PARAM_1)this, 0, 0);
			break;
		}

		if ((m_flags & PreFlushed) != 0)
		{
			PostMessage(MSG_VISITEDSEARCH_FLUSH, (MH_PARAM_1)this, 0, DELAY_PREFLUSH);
			break;
		}

		switch (PreFlush(PREFLUSH_LIMIT))
		{
		case OpBoolean::IS_TRUE:
			PostMessage(MSG_VISITEDSEARCH_FLUSH, (MH_PARAM_1)this, 0, DELAY_PREFLUSH_FLUSH);
			m_flush_errors = 0;
			break;

		case OpBoolean::IS_FALSE:
			PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, DELAY_PREFLUSH);
			m_flush_errors = 0;
			break;

		case OpStatus::ERR_NO_DISK:
			AbortFlush();
			break;

		case OpStatus::ERR_NO_ACCESS:
			PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, BUSY_RETRY_MS);
			break;

		default:  // error - try again
			if (++m_flush_errors > NUM_RETRIES)
				AbortFlush();
			else
				PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, ERROR_RETRY_MS);
		}
		break;

	case MSG_VISITEDSEARCH_FLUSH:
		if ((m_flags & PreFlushing) != 0)
		{
			PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, 0);
			break;
		}

		if ((m_flags & Flushed) != 0)
		{
			PostMessage(MSG_VISITEDSEARCH_COMMIT, (MH_PARAM_1)this, 0, DELAY_FLUSH);
			break;
		}

		switch (Flush(FLUSH_LIMIT))
		{
		case OpBoolean::IS_TRUE:
			PostMessage(MSG_VISITEDSEARCH_COMMIT, (MH_PARAM_1)this, 0, DELAY_FLUSH_COMMIT);
			m_flush_errors = 0;
			break;

		case OpBoolean::IS_FALSE:
			PostMessage(MSG_VISITEDSEARCH_FLUSH, (MH_PARAM_1)this, 0, DELAY_FLUSH);
			m_flush_errors = 0;
			break;

		case OpStatus::ERR_NO_DISK:
			AbortFlush();
			break;

		default:  // error - try again
			if (++m_flush_errors > NUM_RETRIES)
				AbortFlush();
			else
			{
				PostMessage(MSG_VISITEDSEARCH_FLUSH, (MH_PARAM_1)this, 0, ERROR_RETRY_MS);
			}
		}
		break;

	case MSG_VISITEDSEARCH_COMMIT:
		if ((m_flags & Flushed) == 0)
		{
			if (m_meta.GetCount() > 0)
				PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, 0);
			break;
		}

		if (OpStatus::IsError(Commit()))
		{
			if (++m_flush_errors > NUM_RETRIES)
				AbortFlush();
			else
				PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, ERROR_RETRY_MS);
		}
		else {
			m_flush_errors = 0;

			if (m_meta.GetCount() > 0)
				PostMessage(MSG_VISITEDSEARCH_PREFLUSH, (MH_PARAM_1)this, 0, DELAY_COMMIT_PREFLUSH);
		}

		break;
	}
}

void VisitedSearch::PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay)
{
#if defined(USE_OP_THREAD_TOOLS) && defined(VPS_WRAPPER)
    g_thread_tools->PostMessageToMainThread(msg, par1, par2, delay);
#else
    g_main_message_handler->PostMessage(msg, par1, par2, delay);
#endif // USE_OP_THREAD_TOOLS && VPS_WRAPPER
}

void VisitedSearch::AbortFlush(void)
{
	int i;

	m_meta_pf.Clear();

	for (i = 0; i < (int)m_index.GetCount(); ++i)
	{
		RankIndex *index = m_index[i];
		if (index->m_metadata.InTransaction())
			OpStatus::Ignore(index->Rollback());
	}

	m_preflush.Clear();

	m_flags = 0;
}

OP_STATUS VisitedSearch::SearchResult::CopyFrom(const VisitedSearch::Result & result,
												VisitedSearch::SearchSpec * search_spec)
{
    RETURN_IF_ERROR(url.Set(result.url));
    RETURN_IF_ERROR(title.Set(result.title));

    thumbnail_size = result.thumbnail_size;
    OP_DELETEA(thumbnail);
    RETURN_OOM_IF_NULL(thumbnail = OP_NEWA(unsigned char, thumbnail_size));
    op_memcpy(thumbnail, result.thumbnail, thumbnail_size);

    RETURN_IF_ERROR(filename.Set(result.filename));
    visited = result.visited;
    ranking = result.ranking;

    WordHighlighter word_highlighter;
    RETURN_IF_ERROR(word_highlighter.Init(search_spec->query.CStr()));

    RETURN_IF_ERROR(word_highlighter.AppendHighlight(excerpt,
													 result.GetPlaintext(),
													 search_spec->max_chars,
													 search_spec->start_tag,
													 search_spec->end_tag,
													 search_spec->prefix_ratio
													 ));

    return OpStatus::OK;
}

#endif  // VISITED_PAGES_SEARCH
