/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#if defined SEARCH_ENGINE && (defined SEARCH_ENGINE_FOR_MAIL || defined SELFTEST)

#include "modules/search_engine/StringTable.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/search_engine/ACTUtil.h"
#include "modules/search_engine/WordSegmenter.h"
#include "modules/util/opautoptr.h"

#if !defined ADVANCED_OPINT32VECTOR && defined ADVANCED_OPVECTOR
#define Subtract Substract
#endif

// number of items in cache which trigger PreFlush/Flush to write all available data immediately
#define IGNORE_TIMEOUT_LIMIT 15000

#define ACT_EXTENSION ".axx"
#define ACT_EXTENSION_OLD ".ax"
#define LEX_EXTENSION ".bx"

CHECK_RESULT(static OP_STATUS ConcatPaths(OpString& dest, const uni_char *directory, const uni_char *filename));
static OP_STATUS ConcatPaths(OpString& dest, const uni_char *directory, const uni_char *filename)
{
	RETURN_OOM_IF_NULL(dest.Reserve((int)(uni_strlen(directory) + 1 + uni_strlen(filename) + 1)));
	RETURN_IF_ERROR(dest.Set(directory));

	if (dest.HasContent())
		dest.Append(PATHSEP);

	return dest.Append(filename);
}

static int act_strlen(const uni_char *w)
{
	int len = 0;

	ACT::SkipNonPrintableChars(w);

	while (*w != 0)
	{
		++len;
		++w;

		ACT::SkipNonPrintableChars(w);
	}

	return len;
}

static int act_strcmp(const uni_char *w1, const uni_char *w2)
{
	ACT::SkipNonPrintableChars(w1);
	ACT::SkipNonPrintableChars(w2);

	while (*w1 == *w2 && *w2 != 0)
	{
		++w1;
		++w2;

		ACT::SkipNonPrintableChars(w1);
		ACT::SkipNonPrintableChars(w2);
	}

	return (int)*w1 - (int)*w2;
}

static int act_strncmp(const uni_char *w1, const uni_char *w2, int max_len)
{
	if (max_len <= 0)
		return 0;

	ACT::SkipNonPrintableChars(w1);
	ACT::SkipNonPrintableChars(w2);

	while (--max_len > 0 && *w1 == *w2 && *w2 != 0)
	{
		++w1;
		++w2;

		ACT::SkipNonPrintableChars(w1);
		ACT::SkipNonPrintableChars(w2);
	}

	return (int)*w1 - (int)*w2;
}

static int SwitchINT32Endian(void *data, int size, void *user_arg)
{
	BlockStorage::SwitchEndian(data, 4);
	return 4;
}

OP_BOOLEAN StringTable::Open(const uni_char *path, const uni_char *table_name, int flags)
{
	OpString act_path, act_old_path, lex_path;
	OpString filename;
	OP_STATUS err;
	BOOL exist;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	OpString journalname;

	RETURN_IF_ERROR(filename.SetConcat(table_name, UNI_L(".log")));
	RETURN_IF_ERROR(ConcatPaths(journalname, OpStringC(path), filename));
	if (m_log != NULL)
		OP_DELETE(m_log);

	SearchEngineLog::ShiftLogFile(10, journalname);

	if ((m_log = SearchEngineLog::CreateLog(SearchEngineLog::File, journalname.CStr())) == NULL)
		return OpStatus::ERR;
#endif

	RETURN_IF_ERROR(filename.SetConcat(table_name, UNI_L(ACT_EXTENSION)));
	RETURN_IF_ERROR(ConcatPaths(act_path, path, filename.CStr()));

	RETURN_IF_ERROR(filename.SetConcat(table_name, UNI_L(ACT_EXTENSION_OLD)));
	RETURN_IF_ERROR(ConcatPaths(act_old_path, path, filename.CStr()));
	
	if (BlockStorage::FileExists(act_path.CStr()) == OpBoolean::IS_FALSE &&
		BlockStorage::FileExists(act_old_path.CStr()) == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(BlockStorage::RenameStorage(act_old_path.CStr(), act_path.CStr()));

	RETURN_IF_ERROR(filename.SetConcat(table_name, UNI_L(LEX_EXTENSION)));
	RETURN_IF_ERROR(ConcatPaths(lex_path, path, filename.CStr()));

	exist = BlockStorage::FileExists(lex_path.CStr()) == OpBoolean::IS_TRUE && BlockStorage::FileExists(act_path.CStr()) == OpBoolean::IS_TRUE;
	// We can't have one without the other:
	if (!exist && BlockStorage::FileExists(lex_path.CStr()) == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(BlockStorage::DeleteFile(lex_path.CStr()));
	if (!exist && BlockStorage::FileExists(act_path.CStr()) == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(BlockStorage::DeleteFile(act_path.CStr()));

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (BlockStorage::FileExists(act_path) == OpBoolean::IS_TRUE)
	{
		m_log->WriteFile(SearchEngineLog::Debug, "ax", act_path);

		RETURN_IF_ERROR(journalname.SetConcat(act_path, UNI_L("-j")));

		if (BlockStorage::FileExists(journalname) == OpBoolean::IS_TRUE)
			m_log->WriteFile(SearchEngineLog::Debug, "ax-j", journalname);

		RETURN_IF_ERROR(journalname.SetConcat(act_path, UNI_L("-g")));

		if (BlockStorage::FileExists(journalname) == OpBoolean::IS_TRUE)
			m_log->WriteFile(SearchEngineLog::Debug, "ax-g", journalname);
	}

	if (BlockStorage::FileExists(lex_path) == OpBoolean::IS_TRUE)
	{
		m_log->WriteFile(SearchEngineLog::Debug, "bx", lex_path);

		RETURN_IF_ERROR(journalname.SetConcat(lex_path, UNI_L("-j")));

		if (BlockStorage::FileExists(journalname) == OpBoolean::IS_TRUE)
			m_log->WriteFile(SearchEngineLog::Debug, "bx-j", journalname);

		RETURN_IF_ERROR(journalname.SetConcat(lex_path, UNI_L("-g")));

		if (BlockStorage::FileExists(journalname) == OpBoolean::IS_TRUE)
			m_log->WriteFile(SearchEngineLog::Debug, "bx-g", journalname);
	}

	m_log->Write(SearchEngineLog::Debug, "Open", UNI_L("%i %s %s"), flags, table_name, path);
#endif

	if ((err = m_act.Open(act_path.CStr(),
		(flags & StringTable::ReadOnly) != 0 ? BlockStorage::OpenRead : BlockStorage::OpenReadWrite)) == OpStatus::ERR_PARSING_FAILED &&
		(flags & StringTable::OverwriteCorrupted) != 0)
	{
		exist = FALSE;
		RETURN_IF_ERROR(BlockStorage::DeleteFile(act_path.CStr()));
		RETURN_IF_ERROR(BlockStorage::DeleteFile(lex_path.CStr()));
		err = m_act.Open(act_path.CStr(), (flags & StringTable::ReadOnly) != 0 ? BlockStorage::OpenRead : BlockStorage::OpenReadWrite);
	}

	RETURN_IF_ERROR(err);

	if ((err = m_wordbag.Open(lex_path.CStr(),
		(flags & StringTable::ReadOnly) != 0 ? BlockStorage::OpenRead : BlockStorage::OpenReadWrite)) == OpStatus::ERR_PARSING_FAILED &&
		(flags & StringTable::OverwriteCorrupted) != 0)
	{
		exist = FALSE;
		OpStatus::Ignore(m_act.Close());
		RETURN_IF_ERROR(BlockStorage::DeleteFile(act_path.CStr()));
		RETURN_IF_ERROR(BlockStorage::DeleteFile(lex_path.CStr()));

		RETURN_IF_ERROR(m_act.Open(act_path.CStr(), (flags & StringTable::ReadOnly) != 0 ? BlockStorage::OpenRead : BlockStorage::OpenReadWrite));

		if (OpStatus::IsError(err = m_wordbag.Open(lex_path.CStr(),
			(flags & StringTable::ReadOnly) != 0 ? BlockStorage::OpenRead : BlockStorage::OpenReadWrite)))
			err = m_act.Clear();
	}

	if (OpStatus::IsError(err))
	{
		OpStatus::Ignore(m_act.Close());
		return err;
	}

	if (!m_wordbag.GetStorage()->IsNativeEndian())
		m_wordbag.GetStorage()->SetOnTheFlyCnvFunc(&SwitchINT32Endian);

	m_flags = (flags & OpenFlagMask) | CachesSorted | CachesMerged;  // empty caches are sorted and merged

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (exist && (err = CheckConsistency()) != OpBoolean::IS_TRUE)
		m_log->Write(SearchEngineLog::Emerg, "inconsistent index", "%i", err);
#endif

//#if defined(SELFTEST) || defined(_DEBUG)
//	if (exist)
//	{
//		OP_ASSERT(CheckConsistency() == OpBoolean::IS_TRUE);
//	}
//#endif

	return exist ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_STATUS StringTable::Close(BOOL force_close)
{
	OP_STATUS err;
	OpString wb_name;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
	{
		m_log->Write(SearchEngineLog::Debug, "Close", "");
		OP_DELETE(m_log);
		m_log = NULL;
	}
#endif

	if (!m_wordbag.GetStorage()->IsOpen())
		return OpStatus::ERR_BAD_FILE_NUMBER;

	if (OpStatus::IsError(err = Commit()) ||
		(!CacheEmpty() && OpStatus::IsError(err = Commit())))
	{
		if (!force_close)
			return err;
		m_act.Abort();
		OpStatus::Ignore(m_act.Close());

		OpStatus::Ignore(m_wordbag.Abort());
		OpStatus::Ignore(m_wordbag.Close());

		m_word_cache.Clear();
		m_deleted_cache.Clear();

		return err;
	}

	if (OpStatus::IsError(err = wb_name.Set(m_wordbag.GetStorage()->GetFullName())))
	{
		if (!force_close)
			return err;
	}

	if (OpStatus::IsError(err = m_wordbag.Close()))
	{
		if (!force_close)
			return err;
		m_act.Abort();
		OpStatus::Ignore(m_act.Close());

		OpStatus::Ignore(m_wordbag.Abort());
		OpStatus::Ignore(m_wordbag.Close());
	}

	if (OpStatus::IsError(err = m_act.Close()))
	{
		if (!force_close)
		{
			OpStatus::Ignore(m_wordbag.Open(wb_name.CStr(), BlockStorage::OpenReadWrite));
			return err;
		}
		m_act.Abort();
		OpStatus::Ignore(m_act.Close());
	}

	return err;
}

OP_STATUS StringTable::Clear()
{
	int i;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "Clear", "");
#endif

	for (i = m_word_cache.GetCount() - 1; i >= 0; i--)
	{
		FileWord* word = m_word_cache.Remove(i);
		OP_DELETE(word);
	}

	for (i = m_deleted_cache.GetCount() - 1; i >= 0; i--)
	{
		FileWord* word = m_deleted_cache.Remove(i);
		OP_DELETE(word);
	}

	m_flags |= CachesSorted | CachesMerged;

	m_act.Abort();
	RETURN_IF_ERROR(m_act.Clear());
	RETURN_IF_ERROR(m_wordbag.Abort());
	OpStatus::Ignore(m_wordbag.Flush(BSCache::ReleaseAll));
	return m_wordbag.GetStorage()->Clear();
}

class TVectorIterator : public SearchIterator<INT32>
{
public:
	TVectorIterator(TVector<INT32> *vector, BOOL take_ownership = FALSE)
		: m_vector(vector)
		, m_pos(0)
		, m_take_ownership(take_ownership) {}
	~TVectorIterator() { if (m_take_ownership) OP_DELETE(m_vector); }

	virtual BOOL Next(void) {return ++m_pos < (int)m_vector->GetCount();}
	virtual BOOL Prev(void) {return --m_pos >= 0;}

	virtual const INT32 &Get(void) {return m_vector->Get(m_pos);}

	virtual OP_STATUS Error(void) const {return OpStatus::OK;}
	virtual int Count(void) const {return m_vector->GetCount();}
	virtual BOOL End(void) const {return (unsigned)m_pos >= m_vector->GetCount();}
	virtual BOOL Beginning(void) const {return m_pos < 0;}

protected:
	TVector<INT32> *m_vector;
	int m_pos;
	BOOL m_take_ownership;
};


OP_BOOLEAN StringTable::PreFlush(int max_ms)
{
	int i, j;
	OP_STATUS err;
	double time_limit;
	FileWord *preflush, *del_preflush, *cache_word;
	OP_PROBE4(OP_PROBE_SEARCH_ENGINE_STRINGTABLE_PREFLUSH);

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "PreFlush", "%i", max_ms);
#endif

	// ignore timeout if there is too many items in cache
	if (m_word_cache.GetCount() + m_deleted_cache.GetCount() > IGNORE_TIMEOUT_LIMIT)
		max_ms = 0;

	if ((m_flags & PreFlushing) == 0 && CacheEmpty())
		return OpBoolean::IS_TRUE;

	time_limit = g_op_time_info->GetWallClockMS() + max_ms;

	// prepare caches
	if ((m_flags & PreFlushing) == 0)
	{
		RETURN_IF_ERROR(SortCaches());
		MergeCaches();

		RETURN_IF_ERROR(m_word_backup.CopyFrom(m_word_cache));
		if (OpStatus::IsError(err = m_deleted_backup.CopyFrom(m_deleted_cache)))
		{
			m_word_backup.Clear();
			return err;
		}

		m_word_preflush.MoveFrom(m_word_cache);
		m_deleted_preflush.MoveFrom(m_deleted_cache);

		m_word_pos = m_word_preflush.GetCount();
		m_deleted_pos = m_deleted_preflush.GetCount();

		m_flags |= PreFlushing;

	}
	m_act.SetNURFlush((m_flags & UseNUR) != 0);
	m_wordbag.SetNURFlush((m_flags & UseNUR) != 0);

	// insert (and delete in those affected by the insertion)
	while (--m_word_pos >= 0)
	{
		preflush = m_word_preflush[m_word_pos];
		TVectorIterator it(preflush->file_ids);
		OP_ASSERT(preflush->file_ids->GetCount() > 0);

		j = m_deleted_preflush.BinarySearch(preflush->word);
		if (j >= m_deleted_preflush.GetCount() || act_strcmp(m_deleted_preflush[j]->word, preflush->word) != 0)
			j = -1;

		if (preflush->btree == NULL && OpStatus::IsError(err = SearchForBTree(preflush)))
			return AbortPreFlush(err);

		if (OpStatus::IsError(err = preflush->btree->Insert(&it)))  // add the file ids
			return AbortPreFlush(err);

		if (j >= 0)  // delete the file ids
		{
			del_preflush = m_deleted_preflush[j];
			TVectorIterator jt(del_preflush->file_ids);

			 // the b-tree here is the same as preflush->btree, no need to allocate a new one
			if (OpStatus::IsError(err = preflush->btree->Delete(&jt)))
				return AbortPreFlush(err);

			if (preflush->btree->Empty())
			{  // this can happen only after out of memory in MergeCaches
				if (OpStatus::IsError(err = m_act.DeleteCaseWord(preflush->word)))
					if (err != OpStatus::ERR_OUT_OF_RANGE)
						return AbortPreFlush(err);
			}

			FileWord* word = m_deleted_preflush.Remove(j);
			OP_DELETE(word);
			m_deleted_pos = m_deleted_preflush.GetCount();
		}

		if (max_ms > 0 && g_op_time_info->GetWallClockMS() >= time_limit)
			return OpBoolean::IS_FALSE;
	}

	// delete
	while (--m_deleted_pos >= 0)
	{
		del_preflush = m_deleted_preflush[m_deleted_pos];
		TVectorIterator jt(del_preflush->file_ids);
		OP_ASSERT(del_preflush->file_ids->GetCount() > 0);

		if (OpStatus::IsError(err = SearchForBTree(del_preflush, TRUE)))
			return AbortPreFlush(err);

		if (del_preflush->btree == NULL || del_preflush->btree->Empty())
			continue;  // you are probably trying to delete something that hasn't been indexed

		if (OpStatus::IsError(err = del_preflush->btree->Delete(&jt)))
			return AbortPreFlush(err);

		if (del_preflush->btree->Empty())
		{
			if (OpStatus::IsError(err = m_act.DeleteCaseWord(del_preflush->word)))
				if (err != OpStatus::ERR_OUT_OF_RANGE)
					return AbortPreFlush(err);
		}

		if (max_ms > 0 && g_op_time_info->GetWallClockMS() >= time_limit)
			return OpBoolean::IS_FALSE;
	}

	switch (err = m_wordbag.Flush((m_flags & UseNUR) != 0 ? BSCache::ReleaseNo : BSCache::JournalAll, max_ms))
	{
		case OpBoolean::IS_TRUE:
			break;
		case OpBoolean::IS_FALSE:
			return OpBoolean::IS_FALSE;
		default:
			return AbortPreFlush(err);
	}

	for (i = m_word_preflush.GetCount() - 1; i >= 0; --i)
	{
		preflush = m_word_preflush[i];

		if (preflush->is_new_word)
		{
			OP_ASSERT(preflush->btree->GetId() > 0);
			if ((err = m_act.AddCaseWord(preflush->word, (ACT::WordID)preflush->btree->GetId(), FALSE)) != OpBoolean::IS_TRUE)
			{
				if (err == OpBoolean::IS_FALSE)
					err = OpStatus::ERR_PARSING_FAILED;

				if (err != OpStatus::OK)
					return AbortPreFlush(err);
				if (OpStatus::IsError(err = preflush->btree->Clear()))
					return AbortPreFlush(err);
			}
			preflush->is_new_word = FALSE;

			j = m_word_cache.BinarySearch(preflush->word);
			if (j < m_word_cache.GetCount())
			{
				cache_word = m_word_cache[j];
				if (cache_word->btree == NULL && act_strcmp(preflush->word, cache_word->word) == 0)
				{
					cache_word->btree = m_wordbag.GetTree(preflush->btree->GetId());
					cache_word->is_new_word = FALSE;
				}
			}
		}
	}

	if (max_ms > 0 && g_op_time_info->GetWallClockMS() >= time_limit)
		return OpBoolean::IS_FALSE;

	switch (err = m_act.Flush((m_flags & UseNUR) != 0 ? BSCache::ReleaseNo : BSCache::JournalAll, max_ms))
	{
		case OpBoolean::IS_TRUE:
			break;
		case OpBoolean::IS_FALSE:
			return OpBoolean::IS_FALSE;
		default:
			return AbortPreFlush(err);
	} 

	m_flags |= PreFlushed;
	m_flags &= ~PreFlushing;


	return OpBoolean::IS_TRUE;
}

OP_STATUS StringTable::AbortPreFlush(OP_STATUS err)
{
	int i;
	FileWord *preflush;

	OP_ASSERT(0);  // this could normally happen only on out of memory or disk

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Warning, "AbortPreFlush", "%i", err);
#endif

	m_act.Abort();
	OpStatus::Ignore(m_wordbag.Abort());

	for (i = m_word_preflush.GetCount() - 1; i >= 0; --i)
	{
		preflush = m_word_preflush[i];
		if (preflush->is_new_word)
			preflush->btree->Renew();
	}

	m_word_preflush.Clear();
	m_deleted_preflush.Clear();
	m_word_cache.MoveFrom(m_word_backup);
	m_deleted_cache.MoveFrom(m_deleted_backup);

	m_flags &= ~PreFlushing;

	return err;
}


OP_STATUS StringTable::Flush(int max_ms)
{
	OP_STATUS err;
	OP_PROBE4(OP_PROBE_SEARCH_ENGINE_STRINGTABLE_FLUSH);

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "Flush", "%i", max_ms);
#endif

	// ignore timeout if there is too many items in cache
	if (m_word_cache.GetCount() + m_deleted_cache.GetCount() > IGNORE_TIMEOUT_LIMIT)
		max_ms = 0;

	if ((m_flags & PreFlushed) == 0 || (m_flags & PreFlushing) != 0)
	{
		m_flags |= UseNUR;
		err = PreFlush();
		m_flags ^= UseNUR;

		RETURN_IF_ERROR(err);
	}

	if (m_wordbag.GetItemCount() > 0)
	{
		switch (err = m_wordbag.Flush(BSCache::ReleaseAll, max_ms))
		{
		case OpBoolean::IS_FALSE:
			return OpBoolean::IS_FALSE;
		case OpBoolean::IS_TRUE:
			break;
		default:
			return AbortFlush(err);
		}
	}

	switch (err = m_act.Flush(BSCache::ReleaseAll, max_ms))
	{
	case OpBoolean::IS_FALSE:
		return OpBoolean::IS_FALSE;
	case OpBoolean::IS_TRUE:
		break;
	default:
		return AbortFlush(err);
	}

	RETURN_IF_ERROR(m_act.SaveStatus());

	m_word_backup.Clear();
	m_deleted_backup.Clear();

	m_deleted_preflush.Clear();
	m_word_preflush.Clear();

	m_flags ^= PreFlushed;

	return OpBoolean::IS_TRUE;
}

OP_STATUS StringTable::AbortFlush(OP_STATUS err)
{
	OP_ASSERT(0);  // this could normally happen only on out of memory or disk

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Warning, "AbortFlush", "%i", err);
#endif

	m_act.Abort();
	OpStatus::Ignore(m_wordbag.Abort());

	m_word_preflush.Clear();
	m_deleted_preflush.Clear();
	m_word_cache.MoveFrom(m_word_backup);
	m_deleted_cache.MoveFrom(m_deleted_backup);

	m_flags &= ~PreFlushed;

	return err;
}


OP_STATUS StringTable::Commit(void)
{
	OP_PROBE4(OP_PROBE_SEARCH_ENGINE_STRINGTABLE_COMMIT);

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "Commit", "");
#endif

	if ((!CacheEmpty() &&
		!m_wordbag.GetStorage()->InTransaction()) ||
		(m_word_preflush.GetCount() > 0 || m_deleted_preflush.GetCount() > 0))
		RETURN_IF_ERROR(Flush());

	RETURN_IF_ERROR(m_wordbag.Commit());
	return m_act.Commit();

//#if defined(_DEBUG) || defined(SELFTEST)
//	OP_ASSERT(m_wordbag.CheckConsistency() == OpBoolean::IS_TRUE);
//#endif
}

BOOL StringTable::CacheEmpty(void)
{
	return m_word_cache.GetCount() == 0 && m_deleted_cache.GetCount() == 0;
}

OP_STATUS StringTable::InsertBlock(WordCache &cache, const uni_char *words, INT32 file_id)
{
	WordSegmenter msgtok;
	OpString word;
	OP_BOOLEAN e;
	register const uni_char *chk;

	RETURN_IF_ERROR(msgtok.Set(words));

	RETURN_IF_ERROR((e = msgtok.GetNextToken(word)));

	while (e == OpBoolean::IS_TRUE)
	{
		for (chk = word.CStr(); *chk != 0; ++chk)
			if ((UINT16)*chk <= FIRST_CHAR)
				break;

		if (*chk != 0)
		{
			RETURN_IF_ERROR((e = msgtok.GetNextToken(word)));
			continue;
		}
		// highest 3 bits are flags
		if ((file_id & 0xE0000000) != 0)
			RETURN_IF_ERROR(Insert(cache, word.CStr(), file_id & 0x1FFFFFFF));
		RETURN_IF_ERROR(Insert(cache, word.CStr(), file_id));

		RETURN_IF_ERROR((e = msgtok.GetNextToken(word)));
	}

	return OpStatus::OK;
}

OP_STATUS StringTable::Insert(WordCache &cache, const uni_char *word, INT32 file_id)
{
	int i;
	uni_char *up_word;
	OpString temp_upper;
	register FileWord *w;
	OP_STATUS err;
	WordCache *backup_cache;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, &cache == &m_word_cache ? "Insert" : "Delete", UNI_L("%.08X %s"), file_id, word);
#endif

	m_flags &= ~(CachesSorted | CachesMerged);

	if ((m_flags & CaseSensitive) == 0)
	{
		RETURN_IF_ERROR(temp_upper.Set(word));
		temp_upper.MakeUpper();
		up_word = (uni_char *)temp_upper.CStr();
	}
	else up_word = (uni_char *)word;

	backup_cache = &cache == &m_word_cache ? &m_word_backup : &m_deleted_backup;

	i = cache.BinarySearch(up_word);
	if (i >= cache.GetCount() || act_strcmp(up_word, cache[i]->word) != 0)
	{
		RETURN_OOM_IF_NULL(w = FileWord::Create(up_word, file_id));

		if (OpStatus::IsError(err = cache.Insert(i, w)))
		{
			OP_DELETE(w);
			return err;
		}
	}
	else
		RETURN_IF_ERROR(cache[i]->Add(file_id));

	if ((m_flags & PreFlushing) != 0)
	{
		i = backup_cache->BinarySearch(up_word);
		if (i >= backup_cache->GetCount() || act_strcmp(up_word, (*backup_cache)[i]->word) != 0)
		{
			RETURN_OOM_IF_NULL(w = FileWord::Create(up_word, file_id));

			if (OpStatus::IsError(err = backup_cache->Insert(i, w)))
			{
				OP_DELETE(w);
				return err;
			}
		}
		else
			RETURN_IF_ERROR((*backup_cache)[i]->Add(file_id));
	}

	return OpStatus::OK;
}

OP_STATUS StringTable::Delete(const uni_char *word, const OpINT32Vector &file_ids)
{
	OpINT32Vector found_file_ids;
	RETURN_IF_ERROR(Search(word, &found_file_ids));
	RETURN_IF_ERROR(found_file_ids.Intersect(file_ids));

	for (UINT32 i = 0; i < found_file_ids.GetCount(); i++)
		RETURN_IF_ERROR(Delete(word, found_file_ids.Get(i)));

	return OpStatus::OK;
}

OP_STATUS StringTable::Delete(const OpINT32Vector &file_ids)
{
	// search for all words
	OpAutoPtr<SearchIterator<ACT::PrefixResult> > search_iterator(m_act.PrefixSearch(""));
	if (!search_iterator.get())
		return OpStatus::ERR_NO_MEMORY;

	if (!search_iterator->Empty())
	{
		OpString word16;
		do
		{
			RETURN_IF_ERROR(word16.Set(search_iterator->Get().utf8_word));
			RETURN_IF_ERROR(Delete(word16.CStr(), file_ids));
		}
		while (search_iterator->Next());
	}

	// Also words in the cache
	WordCache &search_cache = (m_flags & PreFlushing) == 0 ? m_word_cache : m_word_backup;

	// search_cache can shrink as we go, so make copy of the words
	OpVector<uni_char> tmp_copy(search_cache.GetCount());
	int i;
	for (i = 0; i < search_cache.GetCount(); i++)
		RETURN_IF_ERROR(tmp_copy.Add(UniSetNewStr(search_cache.Get(i)->word)));

	for (i = 0; i < (int)tmp_copy.GetCount(); i++)
	{
		RETURN_IF_ERROR(Delete(tmp_copy.Get(i), file_ids));
		OP_DELETEA(tmp_copy.Get(i));
	}

	return OpStatus::OK;
}

OP_STATUS StringTable::Delete(const uni_char *word)
{
	TVector<INT32> ids;
	RETURN_IF_ERROR(Search(word, &ids));
	for (UINT32 i=0; i<ids.GetCount(); i++)
		RETURN_IF_ERROR(Delete(word, ids.Get(i)));

	return OpStatus::OK;
}

OP_STATUS StringTable::SearchForBTree(FileWord *fw, BOOL must_exist)
{
	ACT::WordID btree_id;

	btree_id = m_act.CaseSearch(fw->word);

	if (btree_id == 0)
	{
		if (!must_exist)
			RETURN_OOM_IF_NULL(fw->btree = m_wordbag.CreateTree());
	}
	else {
		RETURN_OOM_IF_NULL(fw->btree = m_wordbag.GetTree(btree_id));
	}

	fw->is_new_word = btree_id == 0;

	return OpStatus::OK;
}

OP_STATUS StringTable::Search(const uni_char *word, TVector<INT32> *result, int prefix_search)
{
	int i, j;
	uni_char *up_word;
	OpString temp_upper;
	ACT::WordID wbpos;
	ACT::WordID *act_result;
	OpAutoArray<ACT::WordID> prefix_anchor;
	int result_count;
	TBTree<INT32> *btree;
	SearchIterator<INT32> *it;
	WordCache *search_cache;
	FileWord *cache_word;
	OP_PROBE4(OP_PROBE_SEARCH_ENGINE_STRINGTABLE_SEARCH);

	if (word == NULL || *word == 0)
		return OpStatus::OK;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "Search", UNI_L("%i %s"), prefix_search, word);
#endif

	RETURN_IF_ERROR(SortCaches());
	MergeCaches();

	result->Clear();

	if ((m_flags & CaseSensitive) == 0)
	{
		RETURN_IF_ERROR(temp_upper.Set(word));
		temp_upper.MakeUpper();
		up_word = (uni_char *)temp_upper.CStr();
	}
	else up_word = (uni_char *)word;

	if (prefix_search > 0)
	{
		RETURN_OOM_IF_NULL(act_result = OP_NEWA(ACT::WordID, prefix_search));
		prefix_anchor.reset(act_result);

		result_count = m_act.PrefixCaseSearch(act_result, up_word, prefix_search);
	}
	else {
		act_result = &wbpos;

		wbpos = m_act.CaseSearch(up_word);
		result_count = wbpos == 0 ? 0 : 1;
	}

	for (i = 0; i < result_count; ++i)
	{
		RETURN_OOM_IF_NULL(btree = m_wordbag.GetTree(act_result[i]));

		if ((it = btree->SearchFirst()) == NULL)
		{
			OP_DELETE(btree);
			return OpStatus::ERR_NO_MEMORY;
		}

		j = 0;
		LoopDetector<INT32> loopDetector;
		if (it->Count() != 0)
		do {
			if (OpStatus::IsError(loopDetector.CheckNext(it->Get())) ||
				OpStatus::IsError(result->Add(it->Get())))
			{
				OP_DELETE(it);
				OP_DELETE(btree);
				return OpStatus::ERR_NO_MEMORY;
			}
		} while (it->Next());

		OP_DELETE(it);
		OP_DELETE(btree);

		if (prefix_search > 0 && (int)result->GetCount() > 2 * prefix_search)
		{
			RETURN_IF_ERROR(result->Sort());
			if ((int)result->GetCount() >= prefix_search)
				break;
		}
	}

	if (prefix_search)
		RETURN_IF_ERROR(result->Sort());

	search_cache = (m_flags & PreFlushing) == 0 ? &m_word_cache : &m_word_backup;

	if (search_cache->GetCount() > 0)
	{
		i = search_cache->BinarySearch(up_word);
		if (!prefix_search)
		{
			if (i < search_cache->GetCount())
			{
				cache_word = (*search_cache)[i];
				if (act_strcmp(cache_word->word, up_word) == 0)
					RETURN_IF_ERROR(result->Unite(*(cache_word->file_ids)));
			}
		}
		else {
			j = act_strlen(up_word);
			while (i < search_cache->GetCount())
			{
				cache_word = (*search_cache)[i];
				if (act_strncmp(cache_word->word, up_word, j) != 0)
					break;
				RETURN_IF_ERROR(result->Unite(*(cache_word->file_ids)));
				++i;
			}
		}
	}

	search_cache = (m_flags & PreFlushing) == 0 ? &m_deleted_cache : &m_deleted_backup;

	if (search_cache->GetCount() > 0)
	{
		i = search_cache->BinarySearch(up_word);
		if (!prefix_search)
		{
			if (i < search_cache->GetCount())
			{
				cache_word = (*search_cache)[i];
				if (act_strcmp(cache_word->word, up_word) == 0)
					result->Differ(*(cache_word->file_ids));
			}
		}
		else {
			j = act_strlen(up_word);
			while (i < search_cache->GetCount())
			{
				cache_word = (*search_cache)[i];
				if (act_strncmp(cache_word->word, up_word, j) != 0)
					break;
				result->Differ(*(cache_word->file_ids));
				++i;
			}
		}
	}

	if (prefix_search > 0 && (int)result->GetCount() > prefix_search)
		result->Remove(prefix_search);

	return OpStatus::OK;
}

OP_STATUS StringTable::WordSearch(const uni_char *word, uni_char **result, int *result_size)
{

	if (result_size == NULL || *result_size <= 0)
		return OpStatus::OK;

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	if (m_log != NULL)
		m_log->Write(SearchEngineLog::Debug, "WordSearch", UNI_L("%i %s"), result_size, word);
#endif

	if ((m_flags & CaseSensitive) == 0)
		*result_size = m_act.PrefixWords(result, word, *result_size);
	else *result_size = m_act.PrefixCaseWords(result, word, *result_size);

	return OpStatus::OK;
}

OP_STATUS StringTable::MultiSearch(const uni_char *words, TVector<INT32> *result, BOOL match_any, int prefix_search, int phrase_flags)
{
	TVector<INT32> tmp_result;
	OpAutoPtr< TVector<uni_char *> > tokens;
	int i;

	if (words == NULL || *words == 0)
		return OpStatus::OK;

	{
		WordSegmenter msgtok;
		BOOL last_is_prefix;

		RETURN_IF_ERROR(msgtok.Set(words));

		tokens = msgtok.Parse(&last_is_prefix);

		RETURN_OOM_IF_NULL(tokens.get());

		if (!last_is_prefix)
			prefix_search = 0;
	}


	for (i = 0; i < (int)tokens->GetCount(); ++i)
	{
		if (i == 0 || (result->GetCount() == 0 && match_any))
		{
			RETURN_IF_ERROR(Search(tokens->Get(i), result, i == (int)tokens->GetCount() - 1 ? prefix_search * 16 : 0));
		}
		else {
			RETURN_IF_ERROR(Search(tokens->Get(i), &tmp_result, i == (int)tokens->GetCount() - 1 ? prefix_search * 16 : 0));
			if (match_any)
				RETURN_IF_ERROR(result->Unite(tmp_result));
			else {
				RETURN_IF_ERROR(result->Intersect(tmp_result));
				if (result->GetCount() == 0)
					break;  // doesn't make sense to search further
			}
		}
	}

	tokens.reset();

#ifdef SEARCH_ENGINE_PHRASESEARCH
	if ((phrase_flags & PhraseMatcher::AllPhrases) != 0 && m_document_source != NULL && result->GetCount() != 0 &&
		(m_phrase_search_cutoff == 0 || result->GetCount() <= m_phrase_search_cutoff))
	{
		if (prefix_search)
			phrase_flags |= PhraseMatcher::PrefixSearch;
		PhraseFilter<INT32> filter(words, *m_document_source, phrase_flags);
		if (!filter.Empty())
			result->Filter(filter);
	}
#endif

//	if (prefix_search > 0 && (int)result->GetCount() > prefix_search)
//		result->Remove(prefix_search, result->GetCount() - prefix_search);

	return OpStatus::OK;
}

SearchIterator<INT32> *StringTable::PhraseSearch(const uni_char *words, int prefix_search, int phrase_flags)
{
	TVectorIterator *it;
	SearchIterator<INT32> *result;
	TVector<INT32> *unfiltered_result;

	unfiltered_result = OP_NEW(TVector<INT32>, ());
	if (unfiltered_result == NULL)
		return NULL;
	if (OpStatus::IsError(MultiSearch(words, unfiltered_result, FALSE, prefix_search, 0/*no phrases yet*/)) ||
		(it = OP_NEW(TVectorIterator, (unfiltered_result, TRUE))) == NULL)
	{
		OP_DELETE(unfiltered_result);
		return NULL;
	}

#ifdef SEARCH_ENGINE_PHRASESEARCH
	if ((phrase_flags & PhraseMatcher::AllPhrases) != 0 && m_document_source != NULL && unfiltered_result->GetCount() != 0 &&
		(m_phrase_search_cutoff == 0 || unfiltered_result->GetCount() <= m_phrase_search_cutoff))
	{
		PhraseFilter<INT32>* filter = OP_NEW(PhraseFilter<INT32>, (words, *m_document_source, phrase_flags));
		if (prefix_search)
			phrase_flags |= PhraseMatcher::PrefixSearch;
		result = OP_NEW(FilterIterator<INT32>, (it, filter));
		if (result == NULL)
			OP_DELETE(it);
	}
	else
		result = it;
#else
	result = it;
#endif

	return result;
}


#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t StringTable::EstimateMemoryUsed() const
{
	return
		m_act.EstimateMemoryUsed() +
		m_wordbag.EstimateMemoryUsed() +
		m_word_cache.EstimateMemoryUsed() +
		m_deleted_cache.EstimateMemoryUsed() +
		m_word_preflush.EstimateMemoryUsed() +
		m_deleted_preflush.EstimateMemoryUsed() +
		m_word_backup.EstimateMemoryUsed() +
		m_deleted_backup.EstimateMemoryUsed() +
		sizeof(m_word_pos) +
		sizeof(m_deleted_pos) +
		sizeof(m_flags);
}
#endif


StringTable::FileWord *StringTable::FileWord::Create(const uni_char *word, INT32 file_id)
{
	FileWord *fw;

	if ((fw = OP_NEW(FileWord, ())) == NULL)
		return NULL;

	if ((fw->word = uni_strdup(word)) == NULL)
	{
		OP_DELETE(fw);
		return NULL;
	}

	if ((fw->file_ids = OP_NEW(TVector<INT32>, ())) == NULL)
	{
		OP_DELETE(fw);
		return NULL;
	}

	if (OpStatus::IsError(fw->file_ids->Add(file_id)))
	{
		OP_DELETE(fw);
		return NULL;
	}

	return fw;
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t StringTable::FileWord::EstimateMemoryUsed() const
{
	size_t sum = 0;

	if (word)
		sum += uni_strlen(word) + 2*sizeof(size_t);

	if (file_ids)
		sum += file_ids->EstimateMemoryUsed() + 2*sizeof(size_t);

	if (btree)
		sum += btree->EstimateMemoryUsed() + 2*sizeof(size_t);

	return sum +
		sizeof(word) +
		sizeof(btree) +
		sizeof(file_ids) +
		sizeof(is_new_word);
}
#endif

int StringTable::WordCache::BinarySearch(const uni_char *key)
{
	int bstart, bend, n2;

	bstart = 0;
	bend = GetCount();

	while (bend > bstart)
	{
		n2 = (bend - bstart) / 2;
		if (act_strcmp(Get(bstart + n2)->word, key) < 0)
			bstart = bstart + n2 + 1;
		else
			bend = bstart + n2;
	}

	return bstart;
}

OP_STATUS StringTable::SortCaches(void)
{
	int i;

	if ((m_flags & CachesSorted) != 0)
		return OpStatus::OK;

	for (i = m_word_cache.GetCount() - 1; i >= 0; --i)
		RETURN_IF_ERROR(m_word_cache[i]->file_ids->Sort());
	for (i = m_deleted_cache.GetCount() - 1; i >= 0; --i)
		RETURN_IF_ERROR(m_deleted_cache[i]->file_ids->Sort());

	m_flags |= CachesSorted;
	return OpStatus::OK;
}

void StringTable::MergeCaches(void)
{
	int i, j;
	int k;
	unsigned int l;
	TVector<INT32> tmp_vec;
	FileWord *cache_word, *del_cache;

	if ((m_flags & CachesMerged) != 0)
		return;

	for (i = m_word_cache.GetCount() - 1; i >= 0; --i)
	{
		cache_word = m_word_cache[i];

		j = m_deleted_cache.BinarySearch(cache_word->word);
		if (j >= m_deleted_cache.GetCount())
			continue;
		
		del_cache = m_deleted_cache[j];
		if (act_strcmp(del_cache->word, cache_word->word) != 0)
			continue;

		if (OpStatus::IsSuccess(VectorBase::Intersect(tmp_vec, *(cache_word->file_ids), *(del_cache->file_ids))))
		{
			cache_word->file_ids->Differ(tmp_vec);
			del_cache->file_ids->Differ(tmp_vec);
			tmp_vec.Clear();
		}
		else {  // on ERR_NO_MEMORY
			for (k = cache_word->file_ids->GetCount() - 1; k >= 0; --k)
			{
				l = del_cache->file_ids->Search(cache_word->file_ids->Get(k));
				if (l < del_cache->file_ids->GetCount() &&
					cache_word->file_ids->Get(k) == del_cache->file_ids->Get(l))
				{
					cache_word->file_ids->Remove(k);
					del_cache->file_ids->Remove(l);
				}
			}
		}

		if (del_cache->file_ids->GetCount() == 0)
		{
			FileWord* word = m_deleted_cache.Remove(j);
			OP_DELETE(word);
		}

		if (cache_word->file_ids->GetCount() == 0)
		{
			FileWord* word = m_word_cache.Remove(i);
			OP_DELETE(word);
		}
	}

	m_flags |= CachesMerged;
}

OP_BOOLEAN StringTable::CheckConsistency(BOOL thorough)
{
	OP_BOOLEAN a, b;

	RETURN_IF_ERROR((a = m_act.CheckConsistency()));
	RETURN_IF_ERROR((b = m_wordbag.CheckConsistency(0, thorough)));

	if (a == OpBoolean::IS_FALSE || b == OpBoolean::IS_FALSE)
		return OpBoolean::IS_FALSE;

	if (!m_wordbag.GetStorage()->IsStartBlocksSupported())
		return OpBoolean::IS_TRUE;

	// search for all words
	OpAutoPtr<SearchIterator<ACT::PrefixResult> > search_iterator(m_act.PrefixSearch(""));
	if (!search_iterator.get())
		return OpStatus::ERR_NO_MEMORY;

	// Make sure all words have a btree
	if (!search_iterator->Empty())
		do
			if (!m_wordbag.GetStorage()->IsStartBlock(search_iterator->Get().id * m_wordbag.GetStorage()->GetBlockSize()))
				return OpBoolean::IS_FALSE;
		while (search_iterator->Next());

	return OpBoolean::IS_TRUE;
}

OP_STATUS StringTable::Recover(const uni_char* path, const uni_char* tablename)
{

	StringTable indexer_table, temp_indexer_table;
	OpString temp_tablename;
	// make a temporary table where we will insert all values that we can find
	RETURN_IF_ERROR(temp_tablename.AppendFormat(UNI_L("%s.temp"),tablename));
	
	// if the temporary table was already created, we should empty it
	if (temp_indexer_table.Open(path,temp_tablename.CStr()) != OpBoolean::IS_FALSE)
		RETURN_IF_ERROR(temp_indexer_table.Clear());

	// Open the string table
	if (Open(path, tablename) == OpBoolean::IS_TRUE)
	{
		// search for all words
		OpAutoPtr<SearchIterator<ACT::PrefixResult> > search_iterator(m_act.PrefixSearch(""));
		if (!search_iterator.get())
			return OpStatus::ERR_NO_MEMORY;

		if (!search_iterator->Empty())
		{
			do
			{
				OpString word16;
				RETURN_IF_ERROR(word16.Set(search_iterator->Get().utf8_word));
	
				OpINT32Vector temp_result;
				// Search for all file ids belonging to that word
				RETURN_IF_ERROR(Search(word16.CStr(), &temp_result));
				
				// Insert all file ids with that word
				for (UINT32 i= 0; i < temp_result.GetCount(); i++)
				{
					RETURN_IF_ERROR(temp_indexer_table.Insert(word16.CStr(),temp_result.Get(i)));
				}
			}
			while (search_iterator->Next());
		}
		// open iterators need to be closed before closing the tree
		search_iterator.reset();

		// recovery is finished, kill the old file and replace with the new (temp) one
		RETURN_IF_ERROR(Close());
		RETURN_IF_ERROR(temp_indexer_table.Close());

		OpString act_filename, lex_filename, temp_act_filename, temp_lex_filename;
		RETURN_IF_ERROR(act_filename.AppendFormat(UNI_L("%s%c%s%s"), path, PATHSEPCHAR, tablename, UNI_L(ACT_EXTENSION)));
		RETURN_IF_ERROR(lex_filename.AppendFormat(UNI_L("%s%c%s%s"), path, PATHSEPCHAR, tablename, UNI_L(LEX_EXTENSION)));
		RETURN_IF_ERROR(temp_act_filename.AppendFormat(UNI_L("%s%c%s%s"), path, PATHSEPCHAR, temp_tablename.CStr(), UNI_L(ACT_EXTENSION)));
		RETURN_IF_ERROR(temp_lex_filename.AppendFormat(UNI_L("%s%c%s%s"), path, PATHSEPCHAR, temp_tablename.CStr(), UNI_L(LEX_EXTENSION)));

		RETURN_IF_ERROR(BlockStorage::DeleteFile(act_filename.CStr()));
		RETURN_IF_ERROR(BlockStorage::DeleteFile(lex_filename.CStr()));
		RETURN_IF_ERROR(BlockStorage::RenameFile(temp_act_filename.CStr(), act_filename.CStr()));
		RETURN_IF_ERROR(BlockStorage::RenameFile(temp_lex_filename.CStr(), lex_filename.CStr()));
	}

	return OpStatus::OK;
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t StringTable::WordCache::EstimateMemoryUsed() const
{
	size_t sum = 0;
	unsigned i;

	for (i = 0; i < m_count; ++i)
		sum += ((FileWord *)(m_items[i]))->EstimateMemoryUsed();

	return sum + (m_size*sizeof(void*) + 2*sizeof(size_t)) +
		sizeof(m_size) +
		sizeof(m_items) +
		sizeof(m_count) +
		sizeof(m_step) +
		sizeof(m_min_step);
}
#endif

void StringTable::WordCache::Clear()
{
	unsigned i;

	for (i = 0; i < m_count; ++i)
		OP_DELETE((FileWord *)(m_items[i]));

	OpGenericVector::Clear();
}

void StringTable::WordCache::MoveFrom(WordCache &src)
{
	Clear();
	m_size = src.m_size;
	m_items = src.m_items;
	m_count = src.m_count;
	m_step = src.m_step;
	src.m_items = NULL;
	src.m_count = 0;
	src.m_size = 0;
}

OP_STATUS StringTable::WordCache::CopyFrom(WordCache &src)
{
	unsigned i;
	FileWord **new_items = OP_NEWA(FileWord*, src.m_count);
	FileWord *dst_item, *src_item;

	RETURN_OOM_IF_NULL(new_items);

	for (i = 0; i < src.m_count; ++i)
	{
		src_item = (FileWord *)src.m_items[i];

		if ((dst_item = new_items[i] = OP_NEW(FileWord, ())) == NULL
			|| (dst_item->word = uni_strdup(src_item->word)) == NULL
			/*|| (dst_item->btree = new TBTree<INT32>(*src_item->btree)) == NULL*/
			|| (dst_item->file_ids = OP_NEW(TVector<INT32>, ())) == NULL
			|| OpStatus::IsError(dst_item->file_ids->DuplicateOf(*src_item->file_ids)))
		{
			do 	{
				OP_DELETE(new_items[i]);
			} while (i-- > 0);
			OP_DELETEA(new_items);

			return OpStatus::ERR_NO_MEMORY;
		}
	}

	Clear();

	m_items = (void **)new_items;
	m_count = src.m_count;
	m_size = m_count;
	m_step = src.m_step;

	return OpStatus::OK;
}


#endif // defined SEARCH_ENGINE && (defined SEARCH_ENGINE_FOR_MAIL || defined SELFTEST)


