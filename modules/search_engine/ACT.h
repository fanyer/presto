/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef ACT_H
#define ACT_H

#include "modules/search_engine/BlockStorage.h"
#include "modules/search_engine/BSCache.h"
#include "modules/search_engine/ResultBase.h"

// skip non-printable characters
#define FIRST_CHAR ' '

#define RANDOM_STATUS_SIZE 17

class TrieBranch;

/**
 * @brief Array Compacted Trie implementation for searching/indexing utf-8 text
 * @author Pavel Studeny <pavels@opera.com>
 *
 * Trie is an array of pointers indexed by appropriate character from a word.
 * Each pointer points to array for next character.
 @verbatim Example:
   "word"
  
   [a|b|c|....|w|...]
               \-> [a|b|c|.....|o|....]
                                \-> [.....]
 @endverbatim
 * These tries are very sparse, so the pointer in ACTs can point to a free position in the current array
 * instead of pointing to next array.
 * @see http://www.n3labs.com/pdf/fast-and-space-efficient.pdf for more information about ACTs
 */
class ACT : public BSCache
{
public:
	typedef UINT32 WordID;

	struct PrefixResult : public NonCopyable
	{
		PrefixResult(void) : id(0), utf8_word(NULL) {}
		~PrefixResult(void) {OP_DELETEA(utf8_word);}
		WordID id;
		char *utf8_word;
	};

	/**
	 * @param stored_value should be in uppercase if you use case insensitive insertions; must be allocated by new [] and will be deleted by caller
	 */
	typedef CHECK_RESULT(OP_STATUS (* TailCallback)(char **stored_value, WordID id, void *usr_val));

	ACT(void);

	/**
	 * ACT must be opened before you call any other method
	 * @param path file storing the data; file is always created if it doesn't exist
	 * @param mode Read/ReadWrite mode
	 * @param folder might be one of predefind folders
	 * @param tc tail compression callback if tail compression is required
	 * @param callback_val user parameter to TailCallback
	 */
	CHECK_RESULT(OP_STATUS Open(const uni_char* path, BlockStorage::OpenMode mode,
		TailCallback tc = NULL, void *callback_val = NULL, OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER));

	/**
	 * flush all unsaved data, commit any pending transaction and close the file
	 */
	CHECK_RESULT(OP_STATUS Close(void));

	/**
	 * erase all data
	 */
	CHECK_RESULT(OP_STATUS Clear(void));

	/**
	 * index a new word; it will have the given ID if it doesn't exist in the index already
	 * @param word a word to index
	 * @param id ID for a newly created word, shouldn't be 0
	 * @param overwrite_existing overwrite ID of the word if it was already present in the database
	 * @return OpBoolean::IS_TRUE if the word has been created, OpBoolean::IS_FALSE if the word has been already indexed, OpStatus::OK on empty word or word without any valid character
	 */
	CHECK_RESULT(OP_BOOLEAN AddWord(const uni_char *word, WordID id, BOOL overwrite_existing = TRUE));
	CHECK_RESULT(OP_BOOLEAN AddWord(const char *utf8_word, WordID id, BOOL overwrite_existing = TRUE));
	/** case-sensitive */
	CHECK_RESULT(OP_BOOLEAN AddCaseWord(const uni_char *word, WordID id, BOOL overwrite_existing = TRUE));
	CHECK_RESULT(OP_BOOLEAN AddCaseWord(const char *utf8_word, WordID id, BOOL overwrite_existing = TRUE));

	/**
	 * delete a word from index, file might be truncated;
	 * be carefull to delete only previously added words if you use tail compression
	 */
	CHECK_RESULT(OP_STATUS DeleteWord(const uni_char *word));
	CHECK_RESULT(OP_STATUS DeleteWord(const char *utf8_word));
	/** case-sensitive */
	CHECK_RESULT(OP_STATUS DeleteCaseWord(const uni_char *word));
	CHECK_RESULT(OP_STATUS DeleteCaseWord(const char *utf8_word));

	/**
	 * abort all write operations since the first AddWord or DeleteWord
	 */
	void Abort(void);

	/**
	 * flushes all data and ends any pending transaction
	 */
	CHECK_RESULT(OP_STATUS Commit(void));

	/**
	 * write all unsaved data to disk
	 */
//	OP_STATUS Flush(ReleaseSeverity severity = ReleaseNo) {return BSCache::Flush(severity);}

	/**
	 * search for a word
	 * @return ID of the word, 0 on error or if not found
	 */
	WordID Search(const uni_char *word);
	WordID Search(const char *utf8_word);
	/** case-sensitive */
	WordID CaseSearch(const uni_char *word);
	WordID CaseSearch(const char *utf8_word);

	/**
	 * @deprecated Use the iterator methods instead
	 */
	int PrefixWords(uni_char **result, const uni_char *prefix, int max_results);
	int PrefixWords(char **result, const char *utf8_prefix, int max_results);
	int PrefixCaseWords(uni_char **result, const uni_char *prefix, int max_results);
	int PrefixCaseWords(char **result, const char *utf8_prefix, int max_results);

	/**
	 * @deprecated Use the iterator methods instead
	 */
	int PrefixSearch(WordID *result, const uni_char *prefix, int max_results);
	int PrefixSearch(WordID *result, const char *utf8_prefix, int max_results);
	int PrefixCaseSearch(WordID *result, const uni_char *prefix, int max_results);
	int PrefixCaseSearch(WordID *result, const char *utf8_prefix, int max_results);

	/**
	 * search for all words with given prefix
	 * @param prefix word prefix
	 * @param single_word if TRUE, the returned iterator will only search the given prefix as a word (and not do prefix search after all)
	 * @return Iterator containing the first result or empty. Must be deleted by caller. NULL on error.
	 */
	SearchIterator<PrefixResult> *PrefixSearch(const uni_char *prefix, BOOL single_word = FALSE);
	SearchIterator<PrefixResult> *PrefixSearch(const char *utf8_prefix, BOOL single_word = FALSE);
	SearchIterator<PrefixResult> *PrefixCaseSearch(const uni_char *prefix, BOOL single_word = FALSE);
	SearchIterator<PrefixResult> *PrefixCaseSearch(const char *utf8_prefix, BOOL single_word = FALSE);

	/**
	 * Used internally by Prefix(Case)Search.
	 * Find the first word with the given prefix, ordered by unicode values, case sensitive.
	 */
	CHECK_RESULT(OP_BOOLEAN FindFirst(PrefixResult &res, const char *utf8_prefix));

	/**
	 * Used internally by Prefix(Case)Search.
	 * Find the next word with the given prefix, ordered by unicode values, case sensitive.
	 */
	CHECK_RESULT(OP_BOOLEAN FindNext(PrefixResult &res, const char *utf8_prefix));

	/**
	 * pseudo-random number generator, RANROT B algorithm
	 */
	int Random(void);

	/**
	 * save status of the random number generator
	 */
	CHECK_RESULT(OP_STATUS SaveStatus(void));

	/**
	 * restore status of the random number generator
	 */
	void RestoreStatus(void);

	/**
	 * case-sensitive word comparison skipping the invalid characters
	 * @return the number of valid common chracters or -1 on match
	 */
	static int WordsEqual(const char *w1, const char *w2, int max = -1);

	/**
	 * @return an estimate of the memory used by this data structure
	 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	virtual size_t EstimateMemoryUsed() const;
#endif

	friend class NodePointer;
	friend class TrieBranch;

protected:
	CHECK_RESULT(OP_BOOLEAN AddCaseWord(const char *utf8_word, WordID id, int new_len, BOOL overwrite_existing));

	virtual Item *NewMemoryItem(int id, Item *rbranch, int rnode, unsigned short nur);
	virtual Item *NewDiskItem(OpFileLength id, unsigned short nur);

	TailCallback m_TailCallback;
	void *m_callback_val;

private:
	void InitRandom(void);
	UINT32 random_status[RANDOM_STATUS_SIZE + 2];

#ifdef _DEBUG
	// statistics
public:
	// branch_type: 0 ~ all, 1 ~ parents, 2 ~ children
	int GetFillFactor(int *f_average, int *f_min, int *f_max, int *empty, int branch_type);
	int GetFillFactor(int *f_average, int *f_min, int *f_max, int branch_type) {return GetFillFactor(f_average, f_min, f_max, NULL, branch_type);}
	int GetFillDistribution(int *levels, int *counts, int max_level, int *total = NULL, OpFileLength disk_id = 2);
	int GetFillDistribution(int *levels, int max_level, int *total = NULL, OpFileLength disk_id = 2) {return GetFillDistribution(levels, NULL, max_level, total, disk_id);}
	int collision_count;
#endif
public:
	CHECK_RESULT(OP_BOOLEAN CheckConsistency(void));

	static void SkipNonPrintableChars(const char* &s) {
#if FIRST_CHAR >= 0
		while ((unsigned char)*s <= FIRST_CHAR && *s != 0)
			++s;
#endif
	}
	static void SkipNonPrintableChars(const uni_char* &s) {
#if FIRST_CHAR >= 0
		while ((unsigned)*s <= FIRST_CHAR && *s != 0)
			++s;
#endif
	}
};

#endif // ACT_H

