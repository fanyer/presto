// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#ifdef SEARCH_ENGINE_CACHE

#include "modules/cache/CacheIndex.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_stor.h"
#include "modules/cache/url_cs.h"

#define METADATA_BLOCK_SIZE 384

// copyed from url_manc.cpp
#ifndef URL_CACHE_FLUSH_THRESHOLD_LIMIT
#if !defined(SDK) && !defined(WIN_CE)
#define URL_CACHE_FLUSH_THRESHOLD_LIMIT 5
#else
#define URL_CACHE_FLUSH_THRESHOLD_LIMIT 1
#endif
#endif

#define CACHE_CUT_OFF_LIMIT 20  // / 20 == 5%
#define CACHE_LIMIT_LOW     17  // 6%
#define CACHE_LIMIT_HIGH    18  // from the CacheSizeLow limit
#define CACHE_LIMIT_SUB     8192

#define CacheSizeThreshold(max_size) (max_size - max_size / CACHE_CUT_OFF_LIMIT)
#define CacheSizeLow(max_size) (max_size - CACHE_LIMIT_SUB - max_size / CACHE_LIMIT_LOW)
#define CacheSizeHigh(cut_size) (cut_size + CACHE_LIMIT_SUB / 2 + cut_size / CACHE_LIMIT_HIGH)

#define MAX_CACHE_FNAME_LEN 15
#define CachePath(fname) (m_path == NULL ? fname : (uni_strncpy(m_path_end, fname, MAX_CACHE_FNAME_LEN), m_path))

CacheIndex::CacheIndex(URL_Store *hash)
{
	m_hash = hash;

	m_datasize = 0;
	m_indexsize = 0;

	m_file_number = -1;
	m_last_valid_number = -1;

	m_path = NULL;
	m_path_end = NULL;
	m_folder = OPFILE_CACHE_FOLDER;
//	m_fpd = 0;
}

CacheIndex::~CacheIndex(void)
{
	Commit();

	m_metadata.Close();
	m_url.Close();
	m_visited.Close();

	// this assert is much less critical than a crash, but indicates a problem with tracking all the cache files
//	OP_ASSERT(m_last_valid_number == m_file_number);

	if (m_path != NULL)
		OP_DELETE(m_path);
}

OP_STATUS CacheIndex::Open(const uni_char *path, OpFileFolder folder)
{
	int exist;
	OP_STATUS err;
	int path_len, has_path_sep;
	OpFolderLister *session_dir;

	if (path != NULL && *path != 0)
	{
		path_len = uni_strlen(path);
#if defined _WINDOWS || defined MSWIN
		has_path_sep = path[path_len - 1] == '/' || path[path_len - 1] == '\\';
#else
		has_path_sep = path[path_len - 1] == PATHSEPCHAR;
#endif
		if ((m_path = OP_NEWA(uni_char, path_len + MAX_CACHE_FNAME_LEN + 2 - has_path_sep)) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		m_path[path_len + MAX_CACHE_FNAME_LEN + 1 - has_path_sep] = 0;  // string will be always ended

		uni_strcpy(m_path, path);

		if (!has_path_sep)
		{
			m_path[path_len] = PATHSEPCHAR;
			m_path[path_len + 1] = 0;
		}

		m_path_end = m_path + (path_len + 1 - has_path_sep);
	}

	m_folder = folder;

	// "url.ax" has been changed to "url.axx" because ".ax" is a monitored file extension in Windows: "http://msdn.microsoft.com/en-us/library/aa378870.aspx"
	exist = (BlockStorage::FileExists(CachePath(UNI_L("index.dat")), m_folder) == OpBoolean::IS_TRUE) +
		(BlockStorage::FileExists(CachePath(UNI_L("url.axx")), m_folder) == OpBoolean::IS_TRUE) +
		(BlockStorage::FileExists(CachePath(UNI_L("visited.bx")), m_folder) == OpBoolean::IS_TRUE);

	if (exist < 3 && exist > 0)
	{
		BlockStorage::DeleteFile(CachePath(UNI_L("index.dat")), m_folder);
		BlockStorage::DeleteFile(CachePath(UNI_L("url.axx")), m_folder);
		BlockStorage::DeleteFile(CachePath(UNI_L("visited.bx")), m_folder);
	}

	if (exist < 3)
	{
		if (BlockStorage::FileExists(CachePath(UNI_L("dcache4.url")), m_folder) == OpBoolean::IS_TRUE)
		{
			OpFolderLister *c4l;

			if (m_path != NULL)
				*m_path_end = 0;

			if ((c4l = OpFile::GetFolderLister(m_folder, UNI_L("*"), m_path)) == NULL)
				return OpStatus::ERR_NO_MEMORY;

			while (c4l->Next())
				if (uni_strcmp(c4l->GetFileName(), UNI_L(".")) != 0 && uni_strcmp(c4l->GetFileName(), UNI_L("..")) != 0)
					BlockStorage::DeleteFile(c4l->GetFullPath());

			OP_DELETE(c4l);
		}
	}

	RETURN_IF_ERROR(m_metadata.Open(CachePath(UNI_L("index.dat")), BlockStorage::OpenReadWrite, METADATA_BLOCK_SIZE, 0, m_folder));
	if ((err = m_url.Open(CachePath(UNI_L("url.ax")), BlockStorage::OpenReadWrite, &GetTail, this, m_folder)) != OpStatus::OK)
	{
		m_metadata.Close();
		return err;
	}
	if ((err = m_visited.Open(CachePath(UNI_L("visited.bx")), BlockStorage::OpenReadWrite, 512, m_folder)) != OpStatus::OK)
	{
		m_url.Close();
		m_metadata.Close();
		return err;
	}

	m_indexsize = m_metadata.GetFileSize() + m_url.GetStorage()->GetFileSize() + m_visited.GetStorage()->GetFileSize();
	if (!m_metadata.ReadUserHeader(&m_datasize, sizeof(m_datasize), 8))
		m_datasize = 0;

	if (!m_metadata.ReadUserHeader(8, &m_file_number, 4, 4))
		return OpStatus::ERR_NO_ACCESS;
	m_last_valid_number = m_file_number;

	if ((session_dir = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), CachePath(UNI_L("sesn")))) != NULL)
	{
		while (session_dir->Next())
		{
			if (session_dir->IsFolder())
				continue;
			BlockStorage::DeleteFile(session_dir->GetFullPath());
		}
		OP_DELETE(session_dir);
	}

	return OpStatus::OK;
}

static OP_STATUS SetupFields(BSCursor &cursor)
{
	RETURN_IF_ERROR(cursor.AddField("filesize", 8));
	RETURN_IF_ERROR(cursor.AddField("modified", 4));
	RETURN_IF_ERROR(cursor.AddField("visited", 4));
	RETURN_IF_ERROR(cursor.AddField("filename", 0));
	RETURN_IF_ERROR(cursor.AddField("url", 0));
	return cursor.AddField("metadata", 0);
}

OP_BOOLEAN CacheIndex::Insert(URL_Rep *url_rep, OpFileLength max_size)
{
	int pos;
	const uni_char *tmp_name;
	const char *end_name;
	char *end_parse;
	INT32 file_no;
	BSCursor cursor(&m_metadata);
	DataFile_Record dfr(TAG_CACHE_FILE_ENTRY);
	DataRecord_Spec spec;
	OpFile cached_file;
	time_t last_visited_time, lru_time, modif_time;
	OpString8 fname, url_str;
	OpFileLength fsize, old_size;
	BOOL update;
	OP_BOOLEAN err;

	tmp_name = url_rep->GetAttribute(URL::KFileName);
	if (tmp_name != NULL && m_path != NULL)
	{
		RETURN_IF_LEAVE(fname.SetUTF8FromUTF16L(tmp_name + (m_path_end - m_path)));
	}
	else {
		RETURN_IF_LEAVE(fname.SetUTF8FromUTF16L(tmp_name));
	}

	end_name = fname.CStr() + fname.Length();
	if (end_name - fname.CStr() < 8)
		file_no = -1;
	else {
		file_no = op_strtol(end_name - 8, &end_parse, 16);
		if (end_parse != end_name)
			file_no = -1;
	}
	OP_ASSERT(file_no != -1);
	if (file_no >= m_last_valid_number + 16383 && !m_number_bitmap[0])
		return OpBoolean::IS_FALSE;

	RETURN_IF_ERROR(url_str.Set(url_rep->LinkId())); //url_rep->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI);

	if (url_str.IsEmpty() || fname.IsEmpty())
		return OpBoolean::IS_TRUE;

	RETURN_IF_ERROR(SetupFields(cursor));

	RETURN_IF_ERROR(cached_file.Construct(url_rep->GetAttribute(URL::KFileName), m_folder));

	switch (err = cached_file.GetFileLength(fsize))
	{
	case OpStatus::OK:
		break;
	case OpStatus::ERR_FILE_NOT_FOUND:
		OP_ASSERT(0);  // inserting not existing file?
		fsize = 0;
		break;
	default:
		return err;
	}

	switch (err = cached_file.GetLastModified(modif_time))
	{
	case OpStatus::OK:
		break;
	case OpStatus::ERR_FILE_NOT_FOUND:
		modif_time = 0;
		break;
	default:
		return err;
	}

	lru_time = 0;
	url_rep->GetAttribute(URL::KVLocalTimeLoaded, &lru_time);
	if (lru_time == 0)
	{
		OP_ASSERT(0);  // URL_DataStorage::local_time_loaded should be always set
		url_rep->GetAttribute(URL::KVLocalTimeVisited, &lru_time);
		if (lru_time == 0)
			lru_time = g_timecache->CurrentTime();
	}

	update = FALSE;
	old_size = 0;

	if ((pos = m_url.CaseSearch(url_str)) != 0)
	{
		RETURN_IF_ERROR(cursor.Goto(pos));

		update = (ACT::WordsEqual(url_str, (const char *)cursor["url"].GetAddress()) == -1);

		cursor["filesize"].GetValue(&old_size);
	}

	if (!update)
	{
		// if the cache is more than 95% full, we might need to remove something
		if (max_size != FILE_LENGTH_NONE && Size() > CacheSizeThreshold(max_size))
		{
			// we don't know exactly how much it would take, so lets make some estimation with 8K + 6% fallback
			err = DeleteOld(CacheSizeLow(max_size) - fsize - m_metadata.GetBlockSize() - 10 - 12);

			if (err == OpBoolean::IS_FALSE)  // time limit reached
			{
				if (Size() + fsize > CacheSizeThreshold(max_size))
					return OpBoolean::IS_FALSE;
			}
			else if (err != OpBoolean::IS_TRUE)
				return err == OpStatus::OK ? OpBoolean::IS_FALSE : err;  // not enough space or error
		}
	}
	else {
		cursor["visited"].GetValue(&last_visited_time);

		if (last_visited_time == lru_time &&
			cursor["metadata"].GetSize() == (int)dfr.GetLength() &&
			op_memcmp(cursor["metadata"].GetAddress(), dfr.GetDirectPayload(), dfr.GetLength()) == 0)

			return OpBoolean::IS_TRUE;  // no change since last indexing

		if (max_size != FILE_LENGTH_NONE && Size() > CacheSizeThreshold(max_size))
		{
			RETURN_IF_ERROR(DeleteOld(CacheSizeLow(max_size) -
				fsize + old_size - m_metadata.GetBlockSize() - 10 - 12));
		}
	}

	spec.idtag_len = 1;
	spec.length_len = 2;

	dfr.SetNeedDirectAccess(TRUE);
	dfr.SetRecordSpec(&spec);

	RETURN_IF_LEAVE(url_rep->WriteCacheDataL(&dfr));

	if (update)
	{
		if (op_strcmp(fname, (char *)cursor["filename"].GetAddress()) != 0)
		{
			// you shouldn't get into this condition, it's prohibited to change the file name for the URL
			OpString oldname;

			RETURN_IF_ERROR(oldname.SetFromUTF8((char *)cursor["filename"].GetAddress()));

			OP_ASSERT(BlockStorage::FileExists(url_rep->GetAttribute(URL::KFileName), m_folder) == OpBoolean::IS_TRUE);
			
			// trying to update an existing record to a non-existent file, this is a completely invalid request
			if (BlockStorage::FileExists(url_rep->GetAttribute(URL::KFileName), m_folder) != OpBoolean::IS_TRUE)
				// MessageHandler::HandleMessage wouldn't solve it anyway, so returning an error would be worse
				return OpBoolean::IS_TRUE;

//			OP_ASSERT(BlockStorage::FileExists(oldname, m_folder) == OpBoolean::IS_FALSE);
			BlockStorage::DeleteFile(oldname, m_folder);
		}
	}

	if (!m_metadata.InTransaction())
	{
		RETURN_IF_ERROR(m_metadata.BeginTransaction());
	}

	if (!update)
	{
		RETURN_IF_ERROR(cursor.Create());
		cursor.Reserve(128 + 4 + 16 + 4 + dfr.GetLength());  // average url length == 79, visited, fname, metadata
	}

	RETURN_IF_ERROR(cursor["filesize"].SetValue(fsize));
	RETURN_IF_ERROR(cursor["modified"].SetValue(modif_time));
	RETURN_IF_ERROR(cursor["visited"].SetValue(lru_time));
	RETURN_IF_ERROR(cursor["filename"].SetStringValue(fname));
	RETURN_IF_ERROR(cursor["url"].SetStringValue(url_str));
	RETURN_IF_ERROR(cursor["metadata"].SetValue(dfr.GetDirectPayload(), dfr.GetLength()));
	RETURN_IF_ERROR(cursor.Flush());
	if (update)
	{
		if (last_visited_time != lru_time)
		{
			RETURN_IF_ERROR(m_visited.Delete(VisitedRec(last_visited_time, cursor.GetID())));
			RETURN_IF_ERROR(m_visited.Insert(VisitedRec(lru_time, cursor.GetID())));
		}

		m_datasize += fsize - old_size;
	}
	else {
		RETURN_IF_ERROR(m_url.AddCaseWord(url_str, (ACT::WordID)cursor.GetID()));
		RETURN_IF_ERROR(m_visited.Insert(VisitedRec(lru_time, cursor.GetID())));

		m_datasize += fsize;

		// estimate of size change for indexes (metadata, ACT (full from 40%), BTree (full from 50%))
		m_indexsize += ((cursor.Size() + m_metadata.GetBlockSize() - 1) / m_metadata.GetBlockSize()) * m_metadata.GetBlockSize()
			+ 25 + 24;
	}

	if (file_no != -1 && file_no > m_last_valid_number)
	{
		m_number_bitmap.Reserve(file_no - m_last_valid_number);
		m_number_bitmap.Set(file_no - 1 - m_last_valid_number, TRUE);
	}

	return OpBoolean::IS_TRUE;
}

OP_STATUS CacheIndex::Remove(const char *url)
{
	VisitedRec vis;
	OpFileLength row;
	BSCursor c;
	OpFileLength fsize;
	int csize;
	unsigned tmp_vis;
	BSCursor cursor(&m_metadata);
	BOOL do_commit = FALSE;
	OP_STATUS status;

	if (url == NULL)
		return OpStatus::ERR_NULL_POINTER;

	if ((row = m_url.CaseSearch(url)) == 0)
		return OpStatus::OK;


	RETURN_IF_ERROR(SetupFields(cursor));

	RETURN_IF_ERROR(cursor.Goto(row));

	if (!m_metadata.InTransaction())
	{
		RETURN_IF_ERROR(m_metadata.BeginTransaction());
		do_commit = TRUE;
	}

	cursor["filesize"].GetValue(&fsize);

	if (OpStatus::IsError(status = m_url.DeleteCaseWord((const char *)cursor["url"].GetAddress())))
	{
		if (do_commit)
			Abort();
		return status;
	}
	if (OpStatus::IsError(status = m_visited.Delete(VisitedRec(cursor["visited"].GetValue(&tmp_vis), row))))
	{
		if (do_commit)
			Abort();
		return status;
	}
	csize = cursor.Size();
	if (OpStatus::IsError(status = cursor.Delete()))
	{
		if (do_commit)
			Abort();
		return status;
	}

	m_datasize -= fsize;
	m_indexsize -= ((csize + m_metadata.GetBlockSize() - 1) / m_metadata.GetBlockSize()) * m_metadata.GetBlockSize()
		+ 25 + 24;

	if (do_commit)
	{
		if (OpStatus::IsError(status = Commit()))
		{
			Abort();
			return status;
		}
	}

	return OpStatus::OK;
}

OP_BOOLEAN CacheIndex::DeleteOld(OpFileLength new_size)
{
	VisitedRec vis;
	OP_STATUS rv;
	int deleted_count;

	// MSB is set and new_size is unsigned - probably a try to set size < 0
	// WARNING: (OpFileLength)-1 changed to FILE_LENGTH_NONE
	if (FILE_LENGTH_NONE > 0 && (new_size & ((FILE_LENGTH_NONE) >> 1)) != new_size && Size() < ((FILE_LENGTH_NONE) >> 2))
		new_size = 0;

	rv = m_visited.GetFirst(vis);
	if (rv == OpBoolean::IS_FALSE)
	{
		OP_ASSERT(m_datasize == 0);
		return OpBoolean::IS_TRUE;  // already empty
	}
	if (rv != OpBoolean::IS_TRUE)
		return rv;

	deleted_count = 0;
	while (Size() > new_size && deleted_count < 50)
	{
		RETURN_IF_ERROR((rv = Delete(vis.id, Size() > CacheSizeHigh(new_size))));

		if (rv == OpBoolean::IS_TRUE)
			++deleted_count;

		++vis.id;
		rv = m_visited.Search(vis);

		if (rv != OpStatus::OK && rv != OpBoolean::IS_TRUE)  // no more records or error
		{
			if (rv == OpBoolean::IS_FALSE)
			{
				// cache is already empty or cannot reach the new_size
				if (m_visited.Empty())
					break;

				return OpStatus::OK;
			}

			return rv;  // error
		}
	}

	if (new_size <= 0 && m_datasize == 0)
	{
		OpFolderLister *fl;

		if (m_path != NULL)
			*m_path_end = 0;

		if ((fl = OpFile::GetFolderLister(m_folder, UNI_L("*"), m_path)) != NULL)
		{
			while (fl->Next())
			{
				if (fl->IsFolder()
					&& uni_strcmp(fl->GetFileName(), UNI_L(".")) != 0  && uni_strcmp(fl->GetFileName(), UNI_L("..")) != 0)

					if (BlockStorage::DirectoryEmpty(fl->GetFullPath()) == OpBoolean::IS_TRUE)
						BlockStorage::DeleteFile(fl->GetFullPath());
			}
			OP_DELETE(fl);
		}
	}

	// size may be bigger than required because of the index files, but if we are empty, let's return TRUE
	return Size() > new_size && !m_visited.Empty() ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
}

OP_BOOLEAN CacheIndex::Delete(OpFileLength id, BOOL brute_delete)
{
	OpFile f;
	OpString fname;
	BOOL fexist;
	OpFileLength fsize;
	int csize;
	unsigned tmp_vis;
	URL_Rep *url_rep;
	BSCursor cursor(&m_metadata);

	RETURN_IF_ERROR(SetupFields(cursor));

	RETURN_IF_ERROR(cursor.Goto(id));

	if ((url_rep = (URL_Rep *)m_hash->GetLinkObject((const char *)cursor["url"].GetAddress())) != NULL)
	{
		if (url_rep->GetUsedCount() <= 0)
			url_rep->Unload();
		else
			return OpBoolean::IS_FALSE;
	}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	else {
		URL_Name_Components_st name_components;
		OpString uni_url;

		RETURN_IF_ERROR(uni_url.SetFromUTF8((const char *)cursor["url"].GetAddress()));
		RETURN_IF_ERROR(urlManager->GetResolvedNameRep(name_components, NULL, uni_url));

		if (name_components.server_name->TestAndSetNeverFlushTrust() == NeverFlush_Trusted && !brute_delete)
			return OpBoolean::IS_FALSE;
	}
#endif

	if (!m_metadata.InTransaction())
	{
		RETURN_IF_ERROR(m_metadata.BeginTransaction());
	}

	RETURN_IF_ERROR(fname.SetFromUTF8((const char *)cursor["filename"].GetAddress()));
	if (m_path != NULL)
	{
		*m_path_end = 0;
		RETURN_IF_ERROR(fname.Insert(0, m_path));
	}

	cursor["filesize"].GetValue(&fsize);

	RETURN_IF_ERROR(f.Construct(fname, m_folder));
	RETURN_IF_ERROR(f.Exists(fexist));
	if (fexist)
	{
		RETURN_IF_ERROR(f.Delete());
	}

	RETURN_IF_ERROR(m_url.DeleteCaseWord((const char *)cursor["url"].GetAddress()));
	RETURN_IF_ERROR(m_visited.Delete(VisitedRec(cursor["visited"].GetValue(&tmp_vis), id)));
	csize = cursor.Size();
	RETURN_IF_ERROR(cursor.Delete());

	m_datasize -= fsize;
	m_indexsize -= ((csize + m_metadata.GetBlockSize() - 1) / m_metadata.GetBlockSize()) * m_metadata.GetBlockSize()
		+ 25 + 24;

	return OpBoolean::IS_TRUE;
}

#define RETURN_NULL_IF_ERROR(f) if (OpStatus::IsError(f)) return NULL

URL_Rep *CacheIndex::Search(const char *url)
{
	BSCursor cursor(&m_metadata);
	OpFileLength row;
	DataFile_Record dfr;
	DataRecord_Spec spec;
	URL_Rep *rep;
	OpFile cached_file;
	OpString fname;
	OpFileLength fsize, chksize;
	time_t modif_time;
	OP_STATUS err;
	time_t tmp_modif;
#ifdef _DEBUG
	UINT32 file_number;
#endif

	if ((row = m_url.CaseSearch(url)) == 0)
		return NULL;

	RETURN_NULL_IF_ERROR(SetupFields(cursor));

	RETURN_NULL_IF_ERROR(cursor.Goto(row));

	err = ACT::WordsEqual(url, (const char *)cursor["url"].GetAddress());

	if (err != -1)
		return NULL;

	RETURN_NULL_IF_ERROR(fname.SetFromUTF8((const char *)cursor["filename"].GetAddress()));

	if (m_path != NULL)
	{
		*m_path_end = 0;
		RETURN_NULL_IF_ERROR(fname.Insert(0, m_path));
	}

	RETURN_NULL_IF_ERROR(cached_file.Construct(fname, m_folder));

	switch (cached_file.GetFileLength(fsize))
	{
	case OpStatus::OK:
		break;
	case OpStatus::ERR_FILE_NOT_FOUND:
		fsize = 0;
		break;
	default:
		return NULL;
	}

	switch (cached_file.GetLastModified(modif_time))
	{
	case OpStatus::OK:
		break;
	case OpStatus::ERR_FILE_NOT_FOUND:
		modif_time = 0;
		break;
	default:
		return NULL;
	}

#ifdef _DEBUG
	file_number = op_strtoul((const char *)cursor["filename"].GetAddress() + op_strlen((const char *)cursor["filename"].GetAddress()) - 8, NULL, 10);
	OP_ASSERT(file_number <= (UINT32)m_last_valid_number);
#endif

	cursor["filesize"].GetValue(&chksize);

	if (fsize != chksize || modif_time != cursor["modified"].GetValue(&tmp_modif))
		return NULL;

	spec.idtag_len = 1;
	spec.length_len = 2;
	dfr.SetRecordSpec(&spec);

	TRAP(err, dfr.AddContentL((const byte *)cursor["metadata"].GetAddress(), cursor["metadata"].GetSize()));
	RETURN_NULL_IF_ERROR(err);

	// rep->SetAttribute(URL::KFileName, tmpstr) doesn't work!
	if (m_path != NULL)
	{
		OpString8 utffname;

		*m_path_end = 0;
		RETURN_NULL_IF_ERROR(utffname.SetUTF8FromUTF16(m_path));
		RETURN_NULL_IF_ERROR(utffname.Append((const char *)cursor["filename"].GetAddress()));
		TRAP(err, dfr.AddRecordL(TAG_CACHE_FILENAME, utffname));
	}
	else {
		TRAP(err, dfr.AddRecordL(TAG_CACHE_FILENAME, (const char *)cursor["filename"].GetAddress()));
	}
	RETURN_NULL_IF_ERROR(err);

	TRAP(err, dfr.AddRecordL(TAG_URL_NAME, (const char *)cursor["url"].GetAddress()));
	RETURN_NULL_IF_ERROR(err);

	TRAP(err, dfr.IndexRecordsL());
	RETURN_NULL_IF_ERROR(err);

	dfr.SetTag(TAG_CACHE_FILE_ENTRY);

	TRAP(err, URL_Rep::CreateL(&rep, &dfr, m_folder, 0));

	RETURN_NULL_IF_ERROR(err);

	return rep;
}

#undef RETURN_NULL_IF_ERROR

BOOL CacheIndex::SearchUrl(const char *url)
{
	BSCursor cursor(&m_metadata);
	OpFileLength row;
	BOOL rv;

	if ((row = m_url.CaseSearch(url)) == 0)
		return FALSE;

	if (SetupFields(cursor) != OpStatus::OK)
		return FALSE;

	if (cursor.Goto(row) != OpStatus::OK)
		return FALSE;

	rv = ACT::WordsEqual(url, (const char *)cursor["url"].GetAddress()) == -1;

	return rv;
}

#ifdef _DEBUG
BOOL CacheIndex::SearchFile(const uni_char *fname)
{
	SearchIterator<VisitedRec> *it;
	BSCursor cursor(&m_metadata);
	OpString8 filename;
	BOOL rv;

	filename.SetUTF8FromUTF16(fname + (m_path_end - m_path));

	RETURN_IF_ERROR(SetupFields(cursor));

	if ((it = m_visited.SearchFirst()) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (it->Empty())
	{
		OP_DELETE(it);
		return FALSE;
	}

	rv = FALSE;
	do {
		if (it->Error() != OpStatus::OK)
			break;

		if (cursor.Goto(it->Get().id) != OpStatus::OK)
			break;

		if (op_strcmp(filename, (const char *)cursor["filename"].GetAddress()) == 0)
		{
			rv = TRUE;
			break;
		}
	} while (it->Next());

	OP_DELETE(it);

	return rv;
}
#endif


#ifdef SELFTEST
URL_Rep *CacheIndex::GetURL_Rep(const VisitedRec &vr)
{
	char *url_str;
	URL_Rep *ur;

	GetTail(&url_str, vr.id, this);
	ur = Search(url_str);
	OP_DELETEA(url_str);
	return ur;
}
#endif

OP_STATUS CacheIndex::Commit(BOOL ignore_errors)
{
	int ones;

	if (!m_metadata.InTransaction())
		return OpStatus::OK;

	ones = m_number_bitmap.FindFirst(0);
	if (ones > m_file_number - m_last_valid_number)
	{
		m_last_valid_number = m_file_number;
		m_number_bitmap.Clear();
	}
	else {
		m_last_valid_number += ones;
		m_number_bitmap.Delete(ones);
	}

	OP_ASSERT(m_last_valid_number <= m_file_number);

	m_metadata.WriteUserHeader(&m_datasize, sizeof(m_datasize), 8);
	if (m_last_valid_number != -1)
		m_metadata.WriteUserHeader(8, &m_last_valid_number, 4, 4);

	if (ignore_errors)
	{
		m_metadata.Commit();
		m_url.Commit();
		m_visited.Commit();
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_metadata.Commit());
	RETURN_IF_ERROR(m_url.Commit());
	RETURN_IF_ERROR(m_visited.Commit());

	m_indexsize = m_metadata.GetFileSize() + m_url.GetStorage()->GetFileSize() + m_visited.GetStorage()->GetFileSize();

	return OpStatus::OK;
}

void CacheIndex::Abort(void)
{
	m_metadata.Rollback();
	m_url.Abort();
	m_visited.Abort();

	if (!m_metadata.ReadUserHeader(&m_datasize, sizeof(m_datasize), 8))
		m_datasize = 0;

	m_metadata.ReadUserHeader(8, &m_file_number, 4, 4);
	m_last_valid_number = m_file_number;
}

void CacheIndex::MakeFileTemporary(URL_Rep *url_rep)
{
	int length;
	uni_char *end_parse = NULL;
	INT32 file_no = -1;
	OpFile src, dst;
	uni_char *name;

	if (url_rep->GetAttribute(URL::KCacheType) != URL_CACHE_DISK || url_rep->GetDataStorage() == NULL || url_rep->GetDataStorage()->GetCacheStorage() == NULL)
	{
		OP_ASSERT(0);
		return;
	}

	name = (uni_char *)(((File_Storage *)(url_rep->GetDataStorage()->GetCacheStorage()))->GetFileName());

	length = uni_strlen(name);

	OP_ASSERT(length == 13);  // 1234/12345678

	if (length <= 0)
		return;

	if (length >= 8)
	{
		file_no = uni_strtol(name + length - 8, &end_parse, 16);
		if (end_parse != name + length)
			file_no = -1;
	}

	if (file_no != -1 && file_no > m_last_valid_number)
	{
		m_number_bitmap.Reserve(file_no - m_last_valid_number);
		m_number_bitmap.Set(file_no - 1 - m_last_valid_number, TRUE);
	}

	if (length < 13)
	{
		if (OpStatus::IsError(src.Construct(name, m_folder)))
			return;  // unable to rename, keeping the old name is the best thing to do
		src.Delete();
		name[0] = 0;
		return;
	}

	if (OpStatus::IsError(src.Construct(name, m_folder)))
		return;  // unable to rename, keeping the old name is the best thing to do

	Remove(url_rep->LinkId());  // return status ignored

	op_memcpy(name + length - 13, UNI_L("sesn"), 4 * sizeof(uni_char));

	if (OpStatus::IsError(dst.Construct(name, m_folder)))
	{
		src.Delete();
		return;
	}

	if (OpStatus::IsError(dst.SafeReplace(&src)))
		src.Delete();
}

#ifdef _DEBUG
/*class RecursiveFolderLister : public OpFolderLister
{
public:
	RecursiveFolderLister(void) {m_path_buf[_MAX_PATH - 1] = 0;}

	virtual ~RecursiveFolderLister() {}

	OP_STATUS Construct(OpFileFolder folder, const uni_char* path=NULL);
	virtual OP_STATUS Construct(const uni_char* path, const uni_char* pattern)
	{
		if (pattern[0] != '*' || pattern[1] != 0)
		{
			OP_ASSERT(0);
			return OpStatus::ERR_NOT_SUPPORTED;
		}

		return Construct(OPFILE_ABSOLUTE_FOLDER, path);
	}

	virtual BOOL Next();

	virtual const uni_char* GetFileName() const;

	virtual const uni_char* GetFullPath() const;

	virtual OpFileLength GetFileLength() const;

	virtual BOOL IsFolder() const {return FALSE;}

protected:
	OpAutoVector<OpFolderLister> m_dir;
	OpString m_path;
	uni_char m_path_buf[_MAX_PATH];  // ARRAY OK 2009-04-23 lucav
};

OP_STATUS RecursiveFolderLister::Construct(OpFileFolder folder, const uni_char* path)
{
	OpFolderLister *fl;
	OP_STATUS err;

	if (folder != OPFILE_ABSOLUTE_FOLDER && folder != OPFILE_SERIALIZED_FOLDER)
	{
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, m_path));
	}

	RETURN_IF_ERROR(m_path.Append(path));

	RETURN_IF_ERROR(OpFolderLister::Create(&fl));

	RETURN_IF_ERROR(fl->Construct(m_path, UNI_L("*")));

	if ((err = m_dir.Add(fl)) != OpStatus::OK)
	{
		delete fl;
		return err;
	}

	return OpStatus::OK;
}

BOOL RecursiveFolderLister::Next()
{
	BOOL rv;
	OpFolderLister *fl;

	if (m_dir.GetCount() <= 0)
		return FALSE;

	do {  // go up until Next()
		while (rv = m_dir.Get(m_dir.GetCount() - 1)->Next())
		{
			if (uni_strcmp(m_dir.Get(m_dir.GetCount() - 1)->GetFileName(), UNI_L(".")) != 0 &&
				uni_strcmp(m_dir.Get(m_dir.GetCount() - 1)->GetFileName(), UNI_L("..")) != 0)
				break;
		}

		if (rv)
			break;

		fl = m_dir.Remove(m_dir.GetCount() - 1);
		delete fl;
	} while (m_dir.GetCount() > 0);

	if (!rv)
		return FALSE;

	while (m_dir.Get(m_dir.GetCount() - 1)->IsFolder())
	{
		if (uni_strcmp(m_dir.Get(m_dir.GetCount() - 1)->GetFileName(), UNI_L(".")) == 0 ||
			uni_strcmp(m_dir.Get(m_dir.GetCount() - 1)->GetFileName(), UNI_L("..")) == 0)
		{
			while (!m_dir.Get(m_dir.GetCount() - 1)->Next())  // empty directory
			{
				fl = m_dir.Remove(m_dir.GetCount() - 1);
				delete fl;

				if (m_dir.GetCount() <= 0)
					return FALSE;
			}

			continue;
		}

		// go down to the directory
		if (OpFolderLister::Create(&fl) != OpStatus::OK)
			return FALSE;
		if (fl->Construct(m_dir.Get(m_dir.GetCount() - 1)->GetFullPath(), UNI_L("*")) != OpStatus::OK)
		{
			delete fl;
			return FALSE;
		}
		if (m_dir.Add(fl) != OpStatus::OK)
		{
			delete fl;
			return FALSE;
		}

		while (!m_dir.Get(m_dir.GetCount() - 1)->Next())  // unlikely to happen
		{
			fl = m_dir.Remove(m_dir.GetCount() - 1);
			delete fl;

			if (m_dir.GetCount() <= 0)
				return FALSE;
		}
	}

	return TRUE;
}

const uni_char* RecursiveFolderLister::GetFileName() const
{
	int i;
	uni_char *end;

	if (m_dir.GetCount() <= 0)
		return NULL;

	uni_strncpy((uni_char *)m_path_buf, m_dir.Get(0)->GetFileName(), _MAX_PATH - 1);
	end = (uni_char *)m_path_buf + uni_strlen(m_path_buf);

	for (i = 1; i < (int)m_dir.GetCount() && end - m_path_buf < _MAX_PATH - 1; ++i)
	{
		*(end++) = UNI_L(PATHSEPCHAR);
		uni_strncpy(end, m_dir.Get(i)->GetFileName(), _MAX_PATH - 1 - (end - m_path_buf));
	}

	return m_path_buf;
}

const uni_char* RecursiveFolderLister::GetFullPath() const
{
	if (m_dir.GetCount() <= 0)
		return NULL;

	return m_dir.Get(m_dir.GetCount() - 1)->GetFullPath();
}

OpFileLength RecursiveFolderLister::GetFileLength() const
{
	if (m_dir.GetCount() <= 0)
		return NULL;

	return m_dir.Get(m_dir.GetCount() - 1)->GetFileLength();
}
*/

CONSISTENCY_STATUS CacheIndex::CheckConsistency(BOOL check_extra_files)
{
	SearchIterator<VisitedRec> *it;
	BSCursor cursor(&m_metadata);
	OP_STATUS err;
	OpString fname;
	int record_count, url_count;
	ACT::WordID *all_words;
//	RecursiveFolderLister *cache_files;
	unsigned tmp_vis;

#if defined(_DEBUG) || defined(SELFTEST)
	RETURN_IF_ERROR((err = m_url.CheckConsistency()));
	if (err != OpBoolean::IS_TRUE)
		return ConsistencyStatus::ACT_CORRUPTED;
	RETURN_IF_ERROR((err = m_visited.CheckConsistency()));
	if (err != OpBoolean::IS_TRUE)
		return ConsistencyStatus::BTREE_CORRUPTED;
#endif

	RETURN_IF_ERROR(SetupFields(cursor));

	if ((it = m_visited.SearchFirst()) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (it->Empty())
	{
		OP_DELETE(it);

		return m_metadata.GetFileSize() == (OpFileLength)m_metadata.GetBlockSize() || m_metadata.InTransaction() ?
			OpStatus::OK : CONSISTENCY_STATUS(ConsistencyStatus::UNRELATED_URL);
	}

	record_count = 0;
	do {
		if ((err = it->Error()) != OpStatus::OK)
			goto checkconsistency_err;

		if (it->Get().id * m_metadata.GetBlockSize() < 1 || it->Get().id * (OpFileLength)m_metadata.GetBlockSize() > m_metadata.GetFileSize())
		{
			err = ConsistencyStatus::UNRELATED_VISITED;
			goto checkconsistency_err;
		}

		if ((err = cursor.Goto(it->Get().id)) != OpStatus::OK)
			goto checkconsistency_err;

		if (cursor.Size() < 16 ||
			cursor["url"].GetSize() == 0 || cursor["filename"].GetSize() == 0 || cursor["metadata"].GetSize() == 0 ||
			cursor["url"].GetSize() + cursor["filename"].GetSize() + cursor["metadata"].GetSize() + 28 !=
				m_metadata.DataLength(it->Get().id * m_metadata.GetBlockSize()) ||
			cursor["visited"].GetValue(&tmp_vis) < 1000000000)
		{
			err = ConsistencyStatus::INVALID_METADATA;
			goto checkconsistency_err;
		}

		if (op_strlen((const char *)cursor["filename"].GetAddress()) != 8U &&
			op_strlen((const char *)cursor["filename"].GetAddress()) != 13U)
		{
			err = ConsistencyStatus::INVALID_METADATA;
			goto checkconsistency_err;
		}

		if (cursor["visited"].GetValue(&tmp_vis) != it->Get().visited)
		{
			err = ConsistencyStatus::UNRELATED_VISITED;
			goto checkconsistency_err;
		}

		if (m_url.CaseSearch((char *)cursor["url"].GetAddress()) != cursor.GetID())
		{
			err = ConsistencyStatus::UNRELATED_URL;
			goto checkconsistency_err;
		}

		if ((err = fname.SetFromUTF8((char *)cursor["filename"].GetAddress())) != OpStatus::OK)
			goto checkconsistency_err;

		if (m_path != NULL)
		{
			*m_path_end = 0;
			if ((err = fname.Insert(0, m_path)) != OpStatus::OK)
				goto checkconsistency_err;
		}

/*		if ((err = BlockStorage::FileExists(fname, m_folder)) != OpBoolean::IS_TRUE)
		{
			if (err == OpBoolean::IS_FALSE)
				err = ConsistencyStatus::FILE_NOT_FOUND;
			goto checkconsistency_err;
		}*/
		++record_count;
	} while (it->Next());

	OP_DELETE(it);

	if ((all_words = OP_NEWA(ACT::WordID, record_count + 1)) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	url_count = m_url.PrefixCaseSearch(all_words, "", record_count);

	OP_DELETEA(all_words);

	if (url_count != record_count)
		return ConsistencyStatus::UNRELATED_URL_VISITED;

/*	if (check_extra_files)
	{
		if (m_path != NULL)
			*m_path_end = 0;

		if ((cache_files = new RecursiveFolderLister) == NULL)
			return OpStatus::ERR_NO_MEMORY;
		if ((err = cache_files->Construct(m_folder, m_path)) != OpStatus::OK)
			return err;

		record_count = 0;

		while (cache_files->Next())
		{
			if (uni_strcmp(cache_files->GetFileName(), UNI_L("dcache4.url")) != 0 &&
				uni_strcmp(cache_files->GetFileName(), UNI_L("index.dat")) != 0 &&
				uni_strcmp(cache_files->GetFileName(), UNI_L("url.ax")) != 0 &&
				uni_strcmp(cache_files->GetFileName(), UNI_L("visited.bx")) != 0)
				++record_count;
		}

		delete cache_files;

		if (record_count != url_count)
		{
			if (url_count == 0)
				return ConsistencyStatus::TOO_MANY_FILES;

			if (m_path != NULL)
				*m_path_end = 0;

			if ((cache_files = new RecursiveFolderLister) == NULL)
				return OpStatus::ERR_NO_MEMORY;
			if ((err = cache_files->Construct(m_folder, m_path)) != OpStatus::OK)
				return err;

			while (cache_files->Next())
			{
				if (cache_files->IsFolder() ||
					uni_strcmp(cache_files->GetFileName(), UNI_L("dcache4.url")) == 0 ||
					uni_strcmp(cache_files->GetFileName(), UNI_L("index.dat")) == 0 ||
					uni_strcmp(cache_files->GetFileName(), UNI_L("url.ax")) == 0 ||
					uni_strcmp(cache_files->GetFileName(), UNI_L("visited.bx")) == 0)
					continue;

				if ((it = m_visited.SearchFirst()) == NULL)
				{
					delete cache_files;

					return OpStatus::ERR_NO_MEMORY;
				}

				do {
					cursor.Goto(it->Get().id);

					fname.SetFromUTF8((char *)cursor["filename"].GetAddress());
					if (uni_strcmp(fname, cache_files->GetFileName()) == 0)
						break;
				} while (it->Next());

				OP_ASSERT(!it->End());
				if (it->End())
					fname.Set(cache_files->GetFileName());

				if (!it->End())
				{
					while (it->Next())
					{
						cursor.Goto(it->Get().id);

						fname.SetFromUTF8((char *)cursor["filename"].GetAddress());
						if (uni_strcmp(fname, cache_files->GetFileName()) == 0)
							break;
					}

					OP_ASSERT(it->End());
				}

				delete it;
			}

			delete cache_files;

			return ConsistencyStatus::TOO_MANY_FILES;
		}
	}*/

	return OpStatus::OK;

checkconsistency_err:
	OP_DELETE(it);
	return err;
}
#endif

OP_STATUS CacheIndex::GetTail(char **stored_value, ACT::WordID id, void *usr_val)
{
	BSCursor cursor(&(((CacheIndex *)usr_val)->m_metadata));

	RETURN_IF_ERROR(SetupFields(cursor));

	cursor.Goto(id);
	if ((*stored_value = OP_NEWA(char, cursor["url"].GetSize())) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	cursor["url"].GetStringValue(*stored_value);

	return OpStatus::OK;
}

#endif  // SEARCH_ENGINE_CACHE

