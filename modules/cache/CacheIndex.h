// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef CACHEINDEX_H
#define CACHEINDEX_H

#include "modules/search_engine/Cursor.h"
#include "modules/search_engine/ACT.h"
#include "modules/search_engine/SingleBTree.h"

#include "modules/url/url2.h"
#include "modules/formats/url_dfr.h"

#include "modules/cache/BoolVector.h"

class URL_Store;

#ifdef _DEBUG
typedef OP_STATUS CONSISTENCY_STATUS;
class ConsistencyStatus : public OpStatus
{
public:
	enum ConsistencyErr
	{
		UNRELATED_VISITED =     USER_SUCCESS + 1,
		UNRELATED_URL =         USER_SUCCESS + 2,
		UNRELATED_URL_VISITED = USER_SUCCESS + 3,
		INVALID_METADATA =      USER_SUCCESS + 4,
		FILE_NOT_FOUND =        USER_SUCCESS + 5,
		TOO_MANY_FILES =        USER_SUCCESS + 6,
		ACT_CORRUPTED =         USER_SUCCESS + 7,
		BTREE_CORRUPTED =       USER_SUCCESS + 8
	};
};
#endif

/**
 * @brief Stores cache files on disk, cooperates with URL_Store, which holds used URL_Reps in memory.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * Class to index cache files and search in them. Multiple instances can be created in multiple folders.
 * Manages the maximum allowed size of the cache.
 * CacheIndex is supposed to be created from URL_Store.
 */
class CacheIndex
{
public:
	CacheIndex(URL_Store *hash);
	~CacheIndex(void);

	/**
	 * @param path other than default path, might be relative
	 * @param folder special folder for the path
	 * @return OpStatus::OK on success
	 */
	OP_STATUS Open(const uni_char *path = NULL, OpFileFolder folder = OPFILE_CACHE_FOLDER);

	/** @return TRUE if the index is opened */
	BOOL IsOpen(void) {return m_url.GetStorage()->IsOpen();}

	/**
	 * insert a new URL_Rep into index or modify an existing one
	 * @param url_rep URL_Rep with KCacheType of URL_CACHE_DISK
	 * @param max_size maximum cache size
	 * @return IS_TRUE if inserted, IS_FALSE in cache is full
	 */
	OP_BOOLEAN Insert(URL_Rep *url_rep, OpFileLength max_size = -1);

	/**
	 * delete urls not used recently
	 * @param new_size size of the cache to reach
	 * @return OpStatus::OK if new_size couldn't be reached, OpBoolean::IS_FALSE if operation reached a time limit, OpBoolean::IS_TRUE on success
	 */
	OP_BOOLEAN DeleteOld(OpFileLength new_size);


	/**
	 * create URL_Rep if the url exists in the cache
	 * @param url protocol://domain:port/path
	 * @return NULL on error or if not found
	 */
	URL_Rep *Search(const char *url);

	/**
	 * check if the url exists in the cache
	 */
	BOOL SearchUrl(const char *url);

#ifdef _DEBUG
	BOOL SearchFile(const uni_char *fname);  // do not use, it's slow!
#endif


	/**
	 * confirm the changes since the last Commit
	 */
	OP_STATUS Commit(BOOL ignore_errors = TRUE);

	/**
	 * reject the changes since the last Commit
	 */
	void Abort(void);

	/**
	 * @return current overall size of the cache, estimate
	 */
	OpFileLength Size(void) {return m_datasize + m_indexsize;}

	/**
	 * @return allways positive number for a file name
	 */
	INT32 GetFileNumber(BOOL temp_file)
	{
		if (temp_file && (m_file_number < m_last_valid_number + 16382 || m_number_bitmap[0]))  // session only files are not verified
		{                                                              // 4K limit for the bitmap
			m_number_bitmap.Reserve(m_file_number - m_last_valid_number + 1);
			m_number_bitmap.Set(m_file_number - m_last_valid_number, TRUE);
		}
		return m_file_number = (m_file_number + 1) & 0x7FFFFFFF;
	}

	/**
	 * @param file_number positive number returned by GetFileNumber
	 * @param max_cache_size maximum size of the cache, should not be <= 0
	 * @return directory number in range 0 .. 899
	 */
	static UINT16 GetFileFolder(INT32 file_number, OpFileLength max_cache_size)
	{
		// average size of a file in cache is 8917B, maximum number of subfolders should be around 900
		// the files go into the folders in packs of 64
		max_cache_size /= 8100000;  // 8917 * 900
		if (max_cache_size >= 899)
			return (UINT16)(file_number / 0x40 % 900);
		return (UINT16)(file_number / 0x40 % (((max_cache_size + 1) % 900) + 1));
	}

	void MakeFileTemporary(URL_Rep *url_rep);
#ifdef _DEBUG
	/**
	 * check if all the data are correct
	 * @param check_extra_files check that the cache doesn't contain anything more than it should, might fail after a crash
	 */
	CONSISTENCY_STATUS CheckConsistency(BOOL check_extra_files = FALSE);
#endif

#ifndef SELFTEST
protected:
#endif
	struct VisitedRec
	{
		VisitedRec(void) {}
		VisitedRec(time_t last_visited, BSCursor::RowID doc_id) {visited = (UINT32)last_visited; id = (INT32)doc_id;}
		UINT32 visited;
		INT32 id;

		BOOL operator<(const VisitedRec &right) const {return (visited == right.visited) ? (id < right.id) : (visited < right.visited);}
	};

#ifdef SELFTEST
	SearchIterator<VisitedRec> *SearchByTime(void) {return m_visited.SearchFirst();}
	URL_Rep *GetURL_Rep(const VisitedRec &vr);
protected:
#endif

	BlockStorage m_metadata;
	ACT m_url;
	SingleBTree<VisitedRec> m_visited;

	URL_Store *m_hash;

	OpFileLength m_datasize;
	OpFileLength m_indexsize;

	INT32 m_file_number;
	INT32 m_last_valid_number;
	BoolVector m_number_bitmap;

	uni_char *m_path;
	uni_char *m_path_end;
	OpFileFolder m_folder;

	static OP_STATUS GetTail(char **stored_value, ACT::WordID id, void *usr_val);

	/**
	 * @return IS_FALSE if object is in use and cannot be deleted
	 */
	OP_BOOLEAN Delete(OpFileLength id, BOOL brute_delete = FALSE);

	/**
	 * delete the record from the index while leaving the file untouched
	 */
	OP_STATUS Remove(const char *url);
};


#endif  // CACHEINDEX_H

