/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/opfile/opfile.h"
#include "modules/pi/system/OpFolderLister.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/desktop_pi/file/DesktopOpLowLevelFile.h"

/* static */
OP_STATUS DesktopOpFileUtils::Move(OpFile *old_location, const OpFile *new_location)
{
	if (!old_location || !new_location)
		return OpStatus::ERR_NULL_POINTER;

	BOOL exists;
	OP_STATUS rc = new_location->Exists(exists);
	if (OpStatus::IsError(rc))
		return rc;
	else if (exists)
		return OpStatus::ERR;

	return ((DesktopOpLowLevelFile *) old_location->m_file)->Move((DesktopOpLowLevelFile *) new_location->m_file);
}

OP_STATUS DesktopOpFileUtils::Stat(const OpFile *file, struct stat *buffer)
{
	if (!file)
		return OpStatus::ERR_NULL_POINTER;

	return ((DesktopOpLowLevelFile *) file->m_file)->Stat(buffer);
}

OP_STATUS DesktopOpFileUtils::Exists(const OpStringC& filename, BOOL& exists)
{
	exists = FALSE;

	OP_STATUS ret;
	OpFile tmp_file;
	if ((ret=tmp_file.Construct(filename.CStr())) != OpStatus::OK)
		return ret;

	return tmp_file.Exists(exists);
}

BOOL DesktopOpFileUtils::MightExist(const OpStringC& filename)
{
	BOOL exists = TRUE;
	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(filename), exists);
	RETURN_VALUE_IF_ERROR(file.Exists(exists), exists);
	return exists;
}

OP_STATUS DesktopOpFileUtils::ReadLine(char *string, int max_length, const OpFile *file)
{
	if (!file)
		return OpStatus::ERR_NULL_POINTER;

	return ((DesktopOpLowLevelFile *) file->m_file)->ReadLine(string, max_length);
}

OP_STATUS DesktopOpFileUtils::Copy(OpFile& target, OpFile& src, BOOL recursive, BOOL fail_if_exists)
{
	BOOL exists;
	RETURN_IF_ERROR(target.Exists(exists));
	if (exists && fail_if_exists)
		return OpStatus::ERR;
	
	OpFileInfo::Mode mode;
	RETURN_IF_ERROR(src.GetMode(mode));
	
	if (mode == OpFileInfo::DIRECTORY && recursive)
	{
#ifdef DIRECTORY_SEARCH_SUPPORT
		OpFolderLister* lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), src.GetFullPath());
		if (!lister)
			return OpStatus::ERR_NO_MEMORY;
		
		OP_STATUS err = OpStatus::OK;
		while (lister->Next())
		{
			const uni_char* name = lister->GetFileName();
			if (uni_str_eq(name, ".") || uni_str_eq(name, ".."))
				continue;
			
			const uni_char* path = lister->GetFullPath();
			OpFile src_file;
			if (OpStatus::IsError(err = src_file.Construct(path)))
				break;

			OpString target_path;
			if (OpStatus::IsError(target_path.Set(target.GetFullPath())))
				break;
			if (OpStatus::IsError(target_path.Append(PATHSEP)))
				break;
			if (OpStatus::IsError(target_path.Append(name)))
				break;

			OpFile target_file;
			if (OpStatus::IsError(err = target_file.Construct(target_path.CStr())))
				break;
			
			if (OpStatus::IsError(err = Copy(target_file, src_file, recursive, fail_if_exists)))
				break;
		}
		OP_DELETE(lister);
		
#else // DIRECTORY_SEARCH_SUPPORT
		OP_STATUS err = OpStatus::ERR_NOT_SUPPORTED;
#endif // DIRECTORY_SEARCH_SUPPORT
		return err;
	}

	return target.CopyContents(&src, fail_if_exists);
}



OP_STATUS DesktopOpFileUtils::IsFolderEmpty(const OpStringC& path, BOOL& empty)
{
	OpFile folder;
	RETURN_IF_ERROR(folder.Construct(path));

	BOOL exists = FALSE;
	RETURN_IF_ERROR(folder.Exists(exists));
	if (!exists)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	OpFileInfo::Mode mode;
	RETURN_IF_ERROR(folder.GetMode(mode));
	if (OpFileInfo::DIRECTORY != mode)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

#ifdef DIRECTORY_SEARCH_SUPPORT
	OpAutoPtr<OpFolderLister> lister(OpFile::GetFolderLister(
		OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), path.CStr()));
	if (NULL == lister.get())
	{
		return OpStatus::ERR;
	}

	// The lister might be unable to list the folder contents due to
	// insufficient permissions.  A "working" listener, on the other hand,
	// should always return at least two entries: "." and "..".
	INT32 count = 0;
	while (lister->Next() && count++ < 2) {}

	if (count < 2)
	{
		return OpStatus::ERR_NO_ACCESS;
	}

	empty = 2 == count;

	return OpStatus::OK;

#else // DIRECTORY_SEARCH_SUPPORT
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // DIRECTORY_SEARCH_SUPPORT
}

