/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef VISITED_PAGES_SEARCH

#include "modules/search_engine/RankIndex.h"
#include "modules/pi/system/OpLowLevelFile.h"

#define FNAME_ACT_OLD   "w.ax"
#define FNAME_URL_OLD   "url.ax"

OP_STATUS RankIndex::Open(const uni_char *path, unsigned short id)
{
	OpString fname, old_fname;
	int path_len;
	OP_STATUS err;
	IdTime last_doc;

	RETURN_OOM_IF_NULL(fname.Reserve((int)uni_strlen(path) + 16));
	RETURN_IF_ERROR(fname.Set(path));
	RETURN_IF_ERROR(fname.AppendFormat(UNI_L("%c%.04i%c"), PATHSEPCHAR, id, PATHSEPCHAR));
	path_len = fname.Length();

	RETURN_IF_ERROR(old_fname.Set(fname));
	RETURN_IF_ERROR(old_fname.Append(UNI_L(FNAME_ACT_OLD)));
	RETURN_IF_ERROR(fname.Append(UNI_L(FNAME_ACT)));

	if (BlockStorage::FileExists(fname.CStr()) == OpBoolean::IS_FALSE &&
		BlockStorage::FileExists(old_fname.CStr()) == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(BlockStorage::RenameStorage(old_fname.CStr(), fname.CStr()));		

	RETURN_IF_ERROR(m_act.Open(fname.CStr(), BlockStorage::OpenReadWrite));

	// What if one file is missing and not the others?

	fname.Delete(path_len);
	RETURN_IF_ERROR(fname.Append(UNI_L(FNAME_WB)));

	if (OpStatus::IsError(err = m_wordbag.Open(fname.CStr(), BlockStorage::OpenReadWrite, 512)))
	{
		OpStatus::Ignore(m_act.Close());
		return err;
	}

	fname.Delete(path_len);
	RETURN_IF_ERROR(fname.Append(UNI_L(FNAME_META)));

	if (OpStatus::IsError(err = m_metadata.Open(fname.CStr(), BlockStorage::OpenReadWrite, 8192)))
	{
		m_wordbag.Close();
		OpStatus::Ignore(m_act.Close());
		return err;
	}

	fname.Delete(path_len);
	RETURN_IF_ERROR(fname.Append(UNI_L(FNAME_BTREE)));

	if (OpStatus::IsError(err = m_alldoc.Open(fname.CStr(), BlockStorage::OpenReadWrite, 1024)))
	{
		m_metadata.Close();
		m_wordbag.Close();
		OpStatus::Ignore(m_act.Close());
		return err;
	}

	fname.Delete(path_len);
	RETURN_IF_ERROR(fname.Append(UNI_L(FNAME_URL)));
	old_fname.Delete(path_len);
	RETURN_IF_ERROR(old_fname.Append(UNI_L(FNAME_URL_OLD)));

	if (BlockStorage::FileExists(fname.CStr()) == OpBoolean::IS_FALSE &&
		BlockStorage::FileExists(old_fname.CStr()) == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(BlockStorage::RenameStorage(old_fname.CStr(), fname.CStr()));		

	if (OpStatus::IsError(err = m_url.Open(fname.CStr(), BlockStorage::OpenReadWrite, &RankIndex::GetTail, this)))
	{
		OpStatus::Ignore(m_alldoc.Close());
		m_metadata.Close();
		m_wordbag.Close();
		OpStatus::Ignore(m_act.Close());
		return err;
	}

	if (m_alldoc.GetFirst(last_doc) != OpBoolean::IS_TRUE)
		m_doc_count = 0;
	else
		m_doc_count = last_doc.data[IDTIME_ID];

	m_id = id;

	return OpStatus::OK;
}

void RankIndex::Close(void)
{
	if (m_metadata.InTransaction() || m_wordbag.InTransaction() ||
		m_alldoc.GetStorage()->InTransaction() || m_act.GetStorage()->InTransaction())
	{
		OpStatus::Ignore(m_alldoc.Flush());
		OpStatus::Ignore(m_act.Flush());
		OpStatus::Ignore(m_url.Flush());

		OpStatus::Ignore(m_alldoc.Commit());
		OpStatus::Ignore(m_metadata.Commit());
		OpStatus::Ignore(m_wordbag.Commit());
		OpStatus::Ignore(m_act.Commit());
		OpStatus::Ignore(m_url.Commit());
	}


	OpStatus::Ignore(m_alldoc.Close());
	m_metadata.Close();
	m_wordbag.Close();
	OpStatus::Ignore(m_act.Close());
	OpStatus::Ignore(m_url.Close());
}

OpFileLength RankIndex::Size()
{
	return m_act.GetStorage()->GetFileSize() +
		m_wordbag.GetFileSize() +
		m_metadata.GetFileSize() +
		m_alldoc.GetStorage()->GetFileSize() +
		m_url.GetStorage()->GetFileSize();
}

time_t RankIndex::ModifTime(void)
{
	OpFileInfo finfo;
	OpString dirname;
	OpLowLevelFile *dir;

	if (m_alldoc.GetStorage() == NULL || m_alldoc.GetStorage()->GetFullName() == NULL)
		return (time_t)-1;

	RETURN_VALUE_IF_ERROR(dirname.Set(m_alldoc.GetStorage()->GetFullName()), (time_t)-1);
	dirname.Delete(dirname.Length() - 1 - (int)op_strlen(FNAME_BTREE));

	RETURN_VALUE_IF_ERROR(OpLowLevelFile::Create(&dir, dirname.CStr()), (time_t)-1);

	finfo.flags = OpFileInfo::LAST_MODIFIED;

	if (OpStatus::IsError(dir->GetFileInfo(&finfo)))
	{
		OP_DELETE(dir);
		return (time_t)-1;
	}
	OP_DELETE(dir);

	return finfo.last_modified;
}

OP_STATUS RankIndex::Clear(void)
{
	OpString alldoc_path, metadata_path, act_path, wordbag_path, url_path, directory;

	RETURN_IF_ERROR(alldoc_path.Set(m_alldoc.GetStorage()->GetFullName()));
	RETURN_IF_ERROR(metadata_path.Set(m_metadata.GetFullName()));
	RETURN_IF_ERROR(act_path.Set(m_act.GetStorage()->GetFullName()));
	RETURN_IF_ERROR(wordbag_path.Set(m_wordbag.GetFullName()));
	RETURN_IF_ERROR(url_path.Set(m_url.GetStorage()->GetFullName()));
	RETURN_IF_ERROR(directory.Set(m_alldoc.GetStorage()->GetFullName()));
	directory.Delete(directory.Length() - 1 - (int)op_strlen(FNAME_BTREE));

	OpStatus::Ignore(Rollback());
	Close();

	OpStatus::Ignore(BlockStorage::DeleteFile(alldoc_path.CStr()));
	OpStatus::Ignore(BlockStorage::DeleteFile(metadata_path.CStr()));
	OpStatus::Ignore(BlockStorage::DeleteFile(act_path.CStr()));
	OpStatus::Ignore(BlockStorage::DeleteFile(wordbag_path.CStr()));
	OpStatus::Ignore(BlockStorage::DeleteFile(url_path.CStr()));
	OpStatus::Ignore(BlockStorage::DeleteFile(directory.CStr()));

	return OpStatus::OK;
}

OP_STATUS RankIndex::Rollback(void)
{
	RETURN_IF_ERROR(m_alldoc.Abort());
	RETURN_IF_ERROR(m_metadata.Rollback());
	RETURN_IF_ERROR(m_wordbag.Rollback());
	m_act.Abort();
	m_url.Abort();
	
	return OpStatus::OK;
}

OP_STATUS RankIndex::SetupCursor(BSCursor &cursor)
{
	RETURN_IF_ERROR(cursor.AddField("hash", 2));      // quick comparison of the documents
	RETURN_IF_ERROR(cursor.AddField("visited", 4));   // time of visiting the page
	RETURN_IF_ERROR(cursor.AddField("invalid", 1));   // do not include this one in results
	RETURN_IF_ERROR(cursor.AddField("prev_idx", 2));  // older index with the same URL
	RETURN_IF_ERROR(cursor.AddField("prev", 4));      // older record with the same URL
	RETURN_IF_ERROR(cursor.AddField("next_idx", 2));  // newer index with the same URL
	RETURN_IF_ERROR(cursor.AddField("next", 4));      // newer record with the same URL

	RETURN_IF_ERROR(cursor.AddField("url", 0));
	RETURN_IF_ERROR(cursor.AddField("title", 0));
	RETURN_IF_ERROR(cursor.AddField("filename", 0));
	RETURN_IF_ERROR(cursor.AddField("thumbnail", 0));
	return cursor.AddField("plaintext", 0);
}

OP_STATUS RankIndex::GetTail(char **stored_value, ACT::WordID id, void *usr_val)
{
	BSCursor cursor(&(((RankIndex *)usr_val)->m_metadata));

	RETURN_IF_ERROR(RankIndex::SetupCursor(cursor));

	RETURN_IF_ERROR(cursor.Goto(id));
	int size = cursor["url"].GetSize() + 1;
	RETURN_OOM_IF_NULL(*stored_value = OP_NEWA(char, size));
	cursor["url"].GetStringValue(*stored_value);

	return OpStatus::OK;
}

#if defined SEARCH_ENGINE_LOG && (SEARCH_ENGINE_LOG & SEARCH_ENGINE_LOG_VISITEDSEARCH)
OP_STATUS RankIndex::LogFile(OutputLogDevice *log, const uni_char *path, unsigned short id, const uni_char *fname, const uni_char *suffix)
{
	OpString fullname;
	OpString8 tag, s8;
	OP_BOOLEAN e;

	RETURN_IF_ERROR(fullname.AppendFormat(UNI_L("%s%c%.04i%c%s"), path, PATHSEPCHAR, id, PATHSEPCHAR, fname));

	RETURN_IF_ERROR(s8.Set(fname));
	RETURN_IF_ERROR(tag.AppendFormat("%04i.%s", id, s8.CStr()));

	if (suffix != NULL)
	{
		RETURN_IF_ERROR(s8.Set(suffix));
		RETURN_IF_ERROR(fullname.Append(suffix));
		RETURN_IF_ERROR(tag.Append(s8));
	}

	RETURN_IF_ERROR(e = BlockStorage::FileExists(fullname));
	if (e != OpBoolean::IS_TRUE)
		return OpStatus::OK;

	log->WriteFile(SearchEngineLog::Debug, tag, fullname);

	return OpStatus::OK;
}

OP_STATUS RankIndex::LogSubDir(OutputLogDevice *log, const uni_char *path, unsigned short id)
{
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_ACT)));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_ACT), UNI_L("-j")));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_ACT), UNI_L("-g")));

	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_WB)));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_WB), UNI_L("-j")));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_WB), UNI_L("-g")));

	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_META)));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_META), UNI_L("-j")));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_META), UNI_L("-g")));

	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_BTREE)));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_BTREE), UNI_L("-j")));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_BTREE), UNI_L("-g")));

	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_URL)));
	RETURN_IF_ERROR(RankIndex::LogFile(log, path, id, UNI_L(FNAME_URL), UNI_L("-j")));
	return RankIndex::LogFile(log, path, id, UNI_L(FNAME_URL), UNI_L("-g"));
}

#endif  // SEARCH_ENGINE_LOG

#endif  // VISITED_PAGES_SEARCH

