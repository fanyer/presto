/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "WindowsOpFolderLister.h"

OP_STATUS OpFolderLister::Create(OpFolderLister** new_lister)
{
	OP_ASSERT(new_lister != NULL);
	*new_lister = OP_NEW(WindowsOpFolderLister, ());
	if (*new_lister == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

WindowsOpFolderLister::WindowsOpFolderLister() :
	m_find_handle(INVALID_HANDLE_VALUE),
	m_path(NULL),
	m_path_length(0),
	m_drives(0),
	m_drive(0)
{
}

WindowsOpFolderLister::~WindowsOpFolderLister()
{
	if (m_find_handle != INVALID_HANDLE_VALUE)
	{
		FindClose(m_find_handle);
	}

	OP_DELETEA(m_path);
}

OP_STATUS
WindowsOpFolderLister::Construct(const uni_char* path, const uni_char* pattern)
{
	m_find_handle = INVALID_HANDLE_VALUE;

	RETURN_IF_ERROR(m_path_pattern.Set(path));
	if (!m_path_pattern.Length() || m_path_pattern[m_path_pattern.Length() - 1] != '\\')
	{
		RETURN_IF_ERROR(m_path_pattern.Append("\\"));
	}

	m_path = OP_NEWA(uni_char, m_path_pattern.Length() + MAX_PATH + 1);
	if (m_path == NULL) return OpStatus::ERR_NO_MEMORY;
	m_path_length = m_path_pattern.Length();
	uni_strcpy(m_path, m_path_pattern.CStr());
	RETURN_IF_ERROR(m_path_pattern.Append(pattern));

	if (!m_path_pattern.Compare(UNI_L("\\*")))
	{
		if ((m_drives = GetLogicalDrives()) == 0)
			return OpStatus::ERR;
	}
	else if (m_path_pattern[0] == '\\' && m_path_pattern[1] != '\\') // remove \ before drive name if present but not from network paths
	{
		m_path_pattern.Delete(0, 1);
	}

	return OpStatus::OK;

}

BOOL
WindowsOpFolderLister::Next()
{
	BOOL success;
	if (m_drives)
	{
		success = FALSE;
		while (m_drive < 26)
		{
			if (m_drives & (1 << m_drive++))
			{
				success = TRUE;
				break;
			}
		}

		if (success)
		{
			*(m_path + m_path_length) = 'A' + (m_drive-1);
			*(m_path + m_path_length+1) = ':';
			*(m_path + m_path_length+2) = 0;
		}

		return success;
	}

	if (m_find_handle == INVALID_HANDLE_VALUE)
	{
		success = SearchForFirst(m_find_handle, m_path_pattern, m_find_data);
	}
	else
	{
		success = SearchForNext(m_find_handle, m_find_data);
	}
	if (success)
	{
		UpdateFullPath();
	}

	return success;
}

BOOL WindowsOpFolderLister::SearchForFirst(HANDLE &handle, const OpString &path_pattern, WIN32_FIND_DATA &data)
{
	handle = FindFirstFile(path_pattern, &data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		return FALSE; // Nothing was found, bad search pattern or something.
	}
	else
	{
		return TRUE;
	}
}

BOOL WindowsOpFolderLister::SearchForNext(const HANDLE &handle, WIN32_FIND_DATA &data)
{
	return FindNextFile(handle, &data);
}

const uni_char*
WindowsOpFolderLister::GetFileName() const
{
	return m_drives ? m_path : m_find_data.cFileName;
}

const uni_char*
WindowsOpFolderLister::GetFullPath() const
{
	return m_path;
}

OpFileLength
WindowsOpFolderLister::GetFileLength() const
{
	return m_drives ? 0 : (m_find_data.nFileSizeHigh * MAXDWORD) + m_find_data.nFileSizeLow;
}

BOOL
WindowsOpFolderLister::IsFolder() const
{
	return m_drives ? TRUE : m_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

void
WindowsOpFolderLister::UpdateFullPath()
{
	uni_strcpy(m_path + m_path_length, m_find_data.cFileName);
}

