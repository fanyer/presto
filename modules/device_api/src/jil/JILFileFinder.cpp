/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT

#include "modules/device_api/jil/JILFileFinder.h"
#include "modules/device_api/jil/JILFolderLister.h"
#include "modules/util/opfile/opfile.h"

#define RETURN_VOID_IF_ERROR_WITH_CALLBACK(err, cb)				\
do																\
{																\
	if (OpStatus::IsError(err))									\
	{															\
		cb(err);												\
		return;													\
	}															\
} while (0)

#define RETURN_VOID_IF_NULL_WITH_CALLBACK(ptr, cb, err)			\
do																\
{																\
	if (!ptr)													\
	{															\
		cb(err);												\
		return;													\
	}															\
} while (0)

#define RETURN_ENUMERATION_ERROR_IF_ERROR(err)					\
do																\
{																\
	if (OpStatus::IsError(err))									\
	{															\
		if (OpStatus::IsMemoryError(err))						\
			return FolderEntry::ENUMERATION_NO_MEMORY_ERROR;	\
		else													\
			return FolderEntry::ENUMERATION_ERROR;				\
	}															\
}																\
while (0)

#define RETURN_ENUMERATION_NO_MEMORY_ERROR_IF_NULL(ptr)			\
do																\
{																\
	if (!ptr)													\
		return FolderEntry::ENUMERATION_NO_MEMORY_ERROR;		\
}																\
while (0)


class JILFileFolderEntry : public FolderEntry
{
public:
	virtual FolderEntryType IsA() { return FILE; }
	virtual OP_STATUS Enter(FolderEntry *from)
	{
		m_parent = from;
		return OpStatus::OK;
	}
	virtual EnumerationState Enumerate(OpVector<OpString> *result, const OpString &pattern, BOOL case_sensitive, FolderEntry *&curr, UINT32 max_files, BOOL r, OpFileInfo *info = NULL)
	{
		return ENUMERATION_CONTINUE;
	}
};

class JILFolderEntry : public FolderEntry
{
private:
	JILFolderLister *m_lister;

public:

	JILFolderEntry() : m_lister(NULL) {}
	virtual ~JILFolderEntry() { OP_DELETE(m_lister); }
	virtual FolderEntryType IsA() { return DIR; }
	virtual EnumerationState Enumerate(OpVector<OpString> *result, const OpString &pattern, BOOL case_sensitive, FolderEntry *&curr, UINT32 max_files, BOOL recursive, OpFileInfo *info = NULL);
};

FolderEntry::EnumerationState
JILFolderEntry::Enumerate(OpVector<OpString> *result, const OpString &pattern, BOOL case_sensitive, FolderEntry *&curr, UINT32 max_files, BOOL recursive, OpFileInfo *info /* = NULL */)
{
	EnumerationState state = ENUMERATION_FINISHED;
	if (!m_lister)
	{
		m_lister = OP_NEW(JILFolderLister, ());
		RETURN_ENUMERATION_NO_MEMORY_ERROR_IF_NULL(m_lister);
		RETURN_ENUMERATION_ERROR_IF_ERROR(m_lister->Construct(m_path, pattern, case_sensitive));
	}

	BOOL has_next = FALSE;
	UINT32 iter = 0;
	while (iter < max_files && (has_next = m_lister->Next()))
	{
		FolderEntry *new_entry;
		BOOL is_folder = FALSE;
		if (m_lister->IsFolder())
		{
			if (uni_strcmp(m_lister->GetFileName(), UNI_L(".")) == 0 ||
				uni_strcmp(m_lister->GetFileName(), UNI_L("..")) == 0)
				continue;

			new_entry = OP_NEW(JILFolderEntry, ());
			is_folder = TRUE;
		}
		else
		{
			new_entry = OP_NEW(JILFileFolderEntry, ());
		}

		++iter;

		RETURN_ENUMERATION_NO_MEMORY_ERROR_IF_NULL(new_entry);
		new_entry->Into(m_entries);
		curr = this;
		RETURN_ENUMERATION_ERROR_IF_ERROR(new_entry->Construct(m_lister->GetFullPath()));

		if (m_lister->Matches(info))
		{
			OpString *found = OP_NEW(OpString, ());
			RETURN_ENUMERATION_NO_MEMORY_ERROR_IF_NULL(found);
			OpAutoPtr<OpString> ap_found(found);
			const uni_char *full_path = m_lister->GetFullPath();
			if (!full_path)
				return ENUMERATION_ERROR;
			RETURN_ENUMERATION_ERROR_IF_ERROR(found->Set(full_path));
			RETURN_ENUMERATION_ERROR_IF_ERROR(result->Add(found));
			ap_found.release();
		}

		if (!is_folder || recursive)
		{
			RETURN_ENUMERATION_ERROR_IF_ERROR(new_entry->Enter(this));
			state = new_entry->Enumerate(result, pattern, case_sensitive, curr, max_files - iter, recursive, info);
		}

		if (state == ENUMERATION_YIELD || state == ENUMERATION_ERROR || state == ENUMERATION_NO_MEMORY_ERROR)
		{
			return state;
		}
	}

	if (!has_next)
	{
		curr = GetParent();
		if (!curr || !curr->GetParent() || !recursive)
			state = ENUMERATION_FINISHED;
		else
			state = ENUMERATION_CONTINUE;
	}
	else
		state = ENUMERATION_YIELD;

	return state;
}

JILFileFinder::JILFileFinder()
	: m_find_files_info(NULL)
	, m_listener(NULL)
	, m_files_found(NULL)
	, m_start_entry(NULL)
	, m_curr_entry(NULL)
	, m_recursive(FALSE)
	, m_case_sensitive(FALSE)
	, m_max_files(FILE_FINDER_MAX_FILES_WITHOUT_YIELDING)
	, m_yield_time(FILE_FINDER_YIELD_TIME)
{
}

void
JILFileFinder::Find(const uni_char *path, const uni_char *pattern, FilesFoundListener *listener, BOOL recursive, OpFileInfo *find_files_info /* = NULL */)
{
	RETURN_VOID_IF_NULL_WITH_CALLBACK(listener, OnError, OpStatus::ERR_NULL_POINTER);

	m_listener = listener;
	Reset();

	if (!pattern)
	{
		RETURN_VOID_IF_ERROR_WITH_CALLBACK(m_pattern.Set(UNI_L("*")), OnError);
	}
	else
	{
		RETURN_VOID_IF_ERROR_WITH_CALLBACK(m_pattern.Set(pattern), OnError);
	}

	if (!path)
	{
		RETURN_VOID_IF_ERROR_WITH_CALLBACK(m_path.Set(UNI_L(JILPATHSEP)), OnError);
	}
	else
	{
		RETURN_VOID_IF_ERROR_WITH_CALLBACK(m_path.Set(path), OnError);
	}
	m_recursive = recursive;
	m_find_files_info = find_files_info;
	m_files_found = OP_NEW(OpVector<OpString>, ());
	RETURN_VOID_IF_NULL_WITH_CALLBACK(m_files_found, OnError, OpStatus::ERR_NO_MEMORY);

	m_timer.SetTimerListener(this);
	Continue();
}

void
JILFileFinder::Continue()
{
	FolderEntry::EnumerationState state = FolderEntry::ENUMERATION_CONTINUE;

	if (!m_start_entry)
	{
		m_start_entry = OP_NEW(JILFolderEntry, ());
		RETURN_VOID_IF_NULL_WITH_CALLBACK(m_start_entry, OnError, OpStatus::ERR_NO_MEMORY);
		RETURN_VOID_IF_ERROR_WITH_CALLBACK(m_start_entry->Construct(m_path), OnError);
		RETURN_VOID_IF_ERROR_WITH_CALLBACK(m_start_entry->Enter(NULL), OnError);
		m_curr_entry = m_start_entry;
	}

	while (m_curr_entry && state == FolderEntry::ENUMERATION_CONTINUE)
	{
		state = m_curr_entry->Enumerate(m_files_found, m_pattern, m_case_sensitive, m_curr_entry, m_max_files, m_recursive, m_find_files_info);
	}

	if (state == FolderEntry::ENUMERATION_FINISHED)
	{
		OnFinished();
	}
	else if (state == FolderEntry::ENUMERATION_NO_MEMORY_ERROR)
	{
		OnError(OpStatus::ERR_NO_MEMORY);
	}
	else if (state == FolderEntry::ENUMERATION_ERROR)
	{
		OnError(OpStatus::ERR_FILE_NOT_FOUND);
	}
	else
	{
		OP_ASSERT(state == FolderEntry::ENUMERATION_YIELD);
		m_timer.Start(m_yield_time);
	}
}
#endif // DAPI_JIL_FILESYSTEM_SUPPORT
