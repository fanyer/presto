// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "core/pch.h"

#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "platforms/windows/installer/DeleteFileOperation.h"
#include "adjunct/quick/quick-version.h"

DeleteFileOperation::DeleteFileOperation(const OpFile &file, BOOL remove_empty_folders)
	: m_remove_empty_folders(remove_empty_folders)
	, m_file(NULL)
	, m_restore_access_info(NULL)
	, m_init_success(TRUE)
{
	BOOL exists = FALSE;
	if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
	{
		m_file = OP_NEW(OpFile, ());
		if (m_file == NULL)
			m_init_success = FALSE;

		if (OpStatus::IsError(m_file->Copy(&file)))
			m_init_success = FALSE;

		if (OpStatus::IsError(m_file_name.Set(m_file->GetFullPath())))
			m_init_success = FALSE;
	}

	m_temp_file = NULL;
}

OP_STATUS DeleteFileOperation::Do()
{
	if (m_init_success == FALSE)
		return OpStatus::ERR;

	//The file doesn't exist, so nothing to do.
	if (!m_file)
		return OpStatus::OK;

	OpString new_path;
	RETURN_OOM_IF_NULL(m_temp_file = OP_NEW(OpFile, ()));
	RETURN_IF_ERROR(new_path.Set(m_file->GetFullPath()));
	RETURN_IF_ERROR(new_path.Append( UNI_L(".") VER_NUM_STR_UNI UNI_L(".bak")));
	RETURN_IF_ERROR(m_temp_file->Construct(new_path));

	// In case the file has it's read-only attribute set, we have to unset it to be able to delete it on XP.
	SetFileAttributes(new_path.CStr(), FILE_ATTRIBUTE_NORMAL);

	RETURN_IF_ERROR(m_temp_file->Delete(FALSE));

	OpFile from_file;
	RETURN_IF_ERROR(from_file.Copy(m_file));

	if (!WindowsUtils::CheckObjectAccess(m_file_name, SE_FILE_OBJECT, DELETE | FILE_GENERIC_WRITE))
	{
		if (m_restore_access_info || !WindowsUtils::GiveObjectAccess(m_file_name, SE_FILE_OBJECT, DELETE | FILE_GENERIC_WRITE, TRUE, m_restore_access_info))
			return OpStatus::ERR;
	}

	return DesktopOpFileUtils::Move(&from_file, m_temp_file);
}

void DeleteFileOperation::Undo()
{
	if (m_file && m_temp_file)
		DesktopOpFileUtils::Move(m_temp_file, m_file);

	OP_DELETE(m_temp_file);
	m_temp_file = NULL;
}

DeleteFileOperation::~DeleteFileOperation()
{
	if (m_temp_file)
	{
		m_temp_file->Delete(TRUE);

		if (m_remove_empty_folders)
		{
			OpString folder;
			OpFile parent;
			parent.Copy(m_temp_file);
			do
			{
				if (OpStatus::IsError(parent.GetDirectory(folder)))
					break;
				if (OpStatus::IsError(parent.Construct(folder)))
					break;
			}
			while (folder.HasContent() && RemoveDirectory(folder.CStr()) != FALSE);
		}
		if (m_restore_access_info)
			OP_DELETE(m_restore_access_info);
	}
	else
	{
		if (m_restore_access_info)
			WindowsUtils::RestoreObjectAccess(m_restore_access_info);
	}

	OP_DELETE(m_file);
	OP_DELETE(m_temp_file);
}