/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef STRINGTABLE_H
#define STRINGTABLE_H

#include "modules/util/adt/opvector.h"
#include "modules/search_engine/ACT.h"
#include "modules/search_engine/BTree.h"
#include "modules/search_engine/Vector.h"
#include "modules/search_engine/PhraseSearch.h"

#include "modules/probetools/probepoints.h"

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
#include "modules/search_engine/log/Log.h"
#endif

/**
 * @brief Holds relationship between words and file IDs.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * There are two files on disk: \<table name\>.act holds words and pointers to the second file,
 * \<table name\>.lex (the "wordbag") holds file IDs for appropriate words. There aren't duplicate file IDs for one word.
 */
class StringTable : public SearchGroupable
{
public:
	/** flags passed to Open */
	enum OpenFlags
	{
		OverwriteCorrupted =  1,  /**< Clean the files if they contain wrong data */
		CaseSensitive =       2,  /**< Case sensitive words */
		ReadOnly =            4,  /**< The files can be opened from multiple threads for a read-only access */

		OpenFlagMask =     4095,  /**< Used internally, mask for the non-internal flags */
		PreFlushed =       4096,  /**< Used internally, Flush had been called before Commit */
		CachesSorted =     8192,  /**< Used internally, SortCaches finished successfully */
		CachesMerged =    16384,  /**< Used internally, MergeCaches finished successfully */
		PreFlushing =     32768,  /**< Used internally, time limit occured during PreFlush */
		UseNUR =          65536   /**< Used internally, PreFlush hadn't been called before Flush */
	};

	StringTable(void)
	{
#ifdef SEARCH_ENGINE_PHRASESEARCH
		m_document_source = NULL;
		m_phrase_search_cutoff = 0;
#endif
		m_act.GroupWith(m_wordbag);
#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
		m_log = NULL;
#endif
	}

	/**
	 * open the data files
	 * @param path file directory
	 * @param table_name table name to cunstruct the file names
	 * @param flags optional flags, see OpenFlags
	 * @return error code / IS_TRUE if the tables existed / IS_FALSE if the table was newly created
	 */
	CHECK_RESULT(OP_BOOLEAN Open(const uni_char *path, const uni_char *table_name, int flags = OverwriteCorrupted));

	/**
	 * Flush all cached data and closes all resources
	 * @param force_close close all resources even if all operations cannot be completed e.g. because out of disk space
	 * @return if force_close was set, returns error anyway, but the resources are released
	 */
	CHECK_RESULT(OP_STATUS Close(BOOL force_close = TRUE));

	/**
	 * erase all data
	 */
	CHECK_RESULT(OP_STATUS Clear());

	/**
	 * begin transaction and prepare data from the cache to be written to disk;
	 * all Inserts or Deletes until Commit will not be included in this transaction;
	 * an error cancels the whole transaction and the data are returned back to cache
	 *
	 * @param max_ms maximum time to spend by PreFlush in miliseconds, 0 means unlimited
	 * @return OpBoolean::IS_TRUE if finished successfully, OpBoolean::IS_FALSE if time limit was reached (call PreFlush again)
	 * @see BlockStorage for more information about the transaction modes
	 */
	CHECK_RESULT(OP_BOOLEAN PreFlush(int max_ms = 0));

	/**
	 * write the data prepared by PreFlush;
	 * calling PreFlush before Flush is optional, but a delay (roughly 30s) between PreFlush and Flush
	 * reduces the time spent in operating system calls;
	 * an error cancels the whole transaction and the data are returned back to cache
	 *
	 * @param max_ms maximum time to spend by PreFlush in miliseconds, 0 means unlimited
	 * @return OpBoolean::IS_TRUE if finished successfully, OpBoolean::IS_FALSE if time limit was reached (call Flush again)
	 */
	CHECK_RESULT(OP_BOOLEAN Flush(int max_ms = 0));

	/**
	 * finish the transaction begun by PreFlush;
	 * calling Flush before Commit is optional, but a delay (roughly 30s) between Flush and Commit
	 * reduces the time spent in operating system calls
	 * @return not supposed to fail under normal circumstances if Flush was called and finished successfully
	 */
	CHECK_RESULT(OP_STATUS Commit(void));

	/**
	 * if TRUE, there is no data to be flushed; this doesn't include any data being written by current transaction (PreFlush .. Commit) at the moment
	 */
	BOOL CacheEmpty(void);

	/**
	 * insert a word with its file ID
	 */
	CHECK_RESULT(OP_STATUS Insert(const uni_char *word, INT32 file_id))
	{
		OP_PROBE4(OP_PROBE_SEARCH_ENGINE_STRINGTABLE_INSERT);
		// highest 3 bits are flags
		if ((file_id & 0xE0000000) != 0)
			RETURN_IF_ERROR(Insert(m_word_cache, word, file_id & 0x1FFFFFFF));
		return Insert(m_word_cache, word, file_id);
	}

	/**
	 * parse and insert a block of plaintext words
	 */
	CHECK_RESULT(OP_STATUS InsertBlock(const uni_char *words, INT32 file_id))
	{
		return InsertBlock(m_word_cache, words, file_id);
	}

	/**
	 * deletes a file ID from the list of file IDs of the word
	 */
	CHECK_RESULT(OP_STATUS Delete(const uni_char *word, INT32 file_id))
	{
		OP_PROBE4(OP_PROBE_SEARCH_ENGINE_STRINGTABLE_DELETE);
		// highest 3 bits are flags
		if ((file_id & 0xE0000000) != 0)
			RETURN_IF_ERROR(Insert(m_deleted_cache, word, file_id & 0x1FFFFFFF));
		return Insert(m_deleted_cache, word, file_id);
	}

	/**
	 * parse and delete a block of plaintext words
	 */
	CHECK_RESULT(OP_STATUS DeleteBlock(const uni_char *words, INT32 file_id))
	{
		return InsertBlock(m_deleted_cache, words, file_id);
	}

	/**
	 * deletes a number of file IDs from a specific word
	 * @param word
	 * @param file_ids must be sorted
	 */
	CHECK_RESULT(OP_STATUS Delete(const uni_char *word, const OpINT32Vector &file_ids));

	/**
	 * deletes a number of file IDs from all words associated with these IDs
	 * @param file_ids must be sorted
	 */
	CHECK_RESULT(OP_STATUS Delete(const OpINT32Vector &file_ids));

	/**
	 * deletes a word and all file IDs associated with it
	 * @param word
	 */
	CHECK_RESULT(OP_STATUS Delete(const uni_char *word));

	/**
	 * find all the file IDs belonging to the word, Flushes all cached data before the search
	 * @param word word (or a word prefix) to search
	 * @param result resulting (sorted) file IDs, mustn't be NULL, is cleared before the search
	 * @param prefix_search if not 0, search all the words with given prefix up to the given number
	 */
	CHECK_RESULT(OP_STATUS Search(const uni_char *word, TVector<INT32> *result, int prefix_search = 0));
	CHECK_RESULT(OP_STATUS Search(const uni_char *word, OpINT32Vector *result, int prefix_search = 0))
	{
		TVector<INT32> tvres;
		unsigned i;
		result->Clear();
		RETURN_IF_ERROR(Search(word, &tvres, prefix_search));
		for (i = 0; i < tvres.GetCount(); ++i)
			RETURN_IF_ERROR(result->Add(tvres[i]));
		return OpStatus::OK;
	}

	/**
	 * find the indexed words, Flushes all cached data before the search
	 * @param word word (or a word prefix) to search
	 * @param result resulting words, mustn't be NULL, the fields must be freed by caller
	 * @param result_size maximum number of results on input, number of results on output
	 */
	CHECK_RESULT(OP_STATUS WordSearch(const uni_char *word, uni_char **result, int *result_size));

	/**
	 * find all the file IDs belonging to at least one of the words, Flushes all cached data before the search
	 * @param words phrase to search for
	 * @param result resulting (sorted) file IDs, mustn't be NULL
	 * @param match_any if FALSE, document must contain all the words, if TRUE, document must contain at least one word
	 * @param prefix_search if not 0, apply a prefix search for the last word and search for max. prefix_search prefixes
	 * @param phrase_flags flags built up from PhraseMatcher::PhraseFlags, controlling what kind of phrase search is performed
	 */
	CHECK_RESULT(OP_STATUS MultiSearch(const uni_char *words, TVector<INT32> *result, BOOL match_any, int prefix_search = 0, int phrase_flags = 0));
	CHECK_RESULT(OP_STATUS MultiSearch(const uni_char *words, OpINT32Vector *result, BOOL match_any, int prefix_search = 0, int phrase_flags = 0))
	{
		TVector<INT32> tvres;
		unsigned i;
		result->Clear();
		RETURN_IF_ERROR(MultiSearch(words, &tvres, match_any, prefix_search, phrase_flags));
		for (i = 0; i < tvres.GetCount(); ++i)
			RETURN_IF_ERROR(result->Add(tvres[i]));
		return OpStatus::OK;
	}

#ifdef SEARCH_ENGINE_PHRASESEARCH
	/**
	 * Configure phrase search (when using nonzero phrase_flags in MultiSearch).
	 * @param document_source A document source for post-processing to filter out results that don't contain the phrase.
	 * @param phrase_search_cutoff Cutoff number to limit the number of preliminary results that are used in
	 *     phrase search. If there are too many results for the words alone, post-processing might take extremely long.
	 *     Setting 0 disables the cutoff limit.
	 */
	void ConfigurePhraseSearch(DocumentSource<INT32>* document_source, UINT32 phrase_search_cutoff)
		{ m_document_source = document_source; m_phrase_search_cutoff = phrase_search_cutoff; }
#endif

	/**
	 * search for words separated by non-word characters
	 * @param words phrase to search for
	 * @param prefix_search if not 0, apply a prefix search for the last word and search for max. prefix_search prefixes
	 * @param phrase_flags flags built up from PhraseMatcher::PhraseFlags, controlling what kind of phrase search is performed
	 * @return iterator which must be deleted by a caller or NULL on error
	 */
	SearchIterator<INT32> *PhraseSearch(const uni_char *words, int prefix_search = 1024, int phrase_flags = 0);

	/**
	 * @return TRUE if endians on disk are the same like in memory
	 */
	BOOL IsNativeEndian(void)
	{
		return m_wordbag.GetStorage()->IsNativeEndian();
	}

	static BOOL CompareID(const void *left, const void *right) {return (*(INT32 *)left & 0x1FFFFFFF) < (*(INT32 *)right & 0x1FFFFFFF);}

	/**
	 * check if all the data in files are correct
	 * @param thorough If TRUE, the BTree will be checked in depth for recursive sorting errors
	 * @return IS_TRUE if data are OK, IS_FALSE if data are corrupted, other value on error during the check
	 */
	CHECK_RESULT(OP_BOOLEAN CheckConsistency(BOOL thorough = TRUE));

	/**
	 * recovers what it can from a corrupted StringTable. the stringtable has to be closed when calling this function.
	 * @param path file directory
	 * @param table_name table name to cunstruct the file names
	 * @return OpStatus::OK if everything went fine
	 */
	CHECK_RESULT(OP_STATUS Recover(const uni_char* path, const uni_char* tablename));

	/**
	 * @return an estimate of the memory used by this data structure
	 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	size_t EstimateMemoryUsed() const;
#endif
	
	/**
	 * Get SearchGroupable group member
	 */
	virtual BlockStorage &GetGroupMember() { return m_act.GetGroupMember(); }

	friend class STPrefixIterator;

#ifdef SELFTEST
	ACT *GetACT() {return &m_act;}
	TPool<INT32> *GetBT() {return &m_wordbag;}
#endif

	struct FileWord : public NonCopyable
	{
		uni_char *word;
		TBTree<INT32> *btree;
		TVector<INT32> *file_ids;
		BOOL is_new_word;

		FileWord(void)
		{
			word = NULL;
			btree = NULL;
			file_ids = NULL;
			is_new_word = FALSE;
		}

		~FileWord(void)
		{
			if (btree != NULL)
				OP_DELETE(btree);
			if (word != NULL)
				op_free(word);
			if (file_ids != NULL)
				OP_DELETE(file_ids);
		}
		INT32 GetLastID(void)
		{
			return file_ids->Get(file_ids->GetCount() - 1);
		}

		CHECK_RESULT(OP_STATUS Add(INT32 file_id))
		{
			if (GetLastID() == file_id)
				return OpStatus::OK;
			return file_ids->Add(file_id);
		}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		size_t EstimateMemoryUsed() const;
#endif

		static FileWord *Create(const uni_char *word, INT32 file_id);
	};

protected:

	class WordCache : private OpGenericVector
	{
	public:
		int BinarySearch(const uni_char *key);
		FileWord *Get(int index) {return (FileWord *)OpGenericVector::Get(index);}
		FileWord *operator[](int index) {return (FileWord *)OpGenericVector::Get(index);}
		CHECK_RESULT(OP_STATUS Insert(int index, FileWord *value)) {return OpGenericVector::Insert(index, value);}
		int GetCount(void) {return OpGenericVector::GetCount();}
		FileWord *Remove(int index) {return (FileWord *)OpGenericVector::Remove(index);}
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		size_t EstimateMemoryUsed() const;
#endif

		void Clear();
		CHECK_RESULT(OP_STATUS DuplicateOf(const WordCache& vec)) { return OpGenericVector::DuplicateOf(vec); }
		void MoveFrom(WordCache &src);
		CHECK_RESULT(OP_STATUS CopyFrom(WordCache &src));
	};

	CHECK_RESULT(OP_STATUS Insert(WordCache &cache, const uni_char *word, INT32 file_id));
	CHECK_RESULT(OP_STATUS InsertBlock(WordCache &cache, const uni_char *words, INT32 file_id));

	CHECK_RESULT(OP_STATUS SearchForBTree(FileWord *fw, BOOL must_exist = FALSE));

	CHECK_RESULT(OP_STATUS SortCaches(void));
	void MergeCaches(void);

	ACT m_act;
	TPool<INT32> m_wordbag;

	WordCache m_word_cache;
	WordCache m_deleted_cache;
	WordCache m_word_preflush;
	WordCache m_deleted_preflush;
	WordCache m_word_backup;
	WordCache m_deleted_backup;
	int m_word_pos;
	int m_deleted_pos;

	int m_flags;

#ifdef SEARCH_ENGINE_PHRASESEARCH
	DocumentSource<INT32>* m_document_source;
	UINT32 m_phrase_search_cutoff;
#endif

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_STRINGTABLE)
	OutputLogDevice *m_log;
#endif

private:
	CHECK_RESULT(OP_STATUS AbortPreFlush(OP_STATUS err));
	CHECK_RESULT(OP_STATUS AbortFlush(OP_STATUS err));
};

#endif  // STRINGTABLE_H

