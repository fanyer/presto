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
#include "platforms/windows/installer/MoveFileOperation.h"

MoveFileOperation::MoveFileOperation(const OpFile &from_file, const OpFile &to_file)
	: m_from_file(NULL)
	, m_to_file(NULL)
	, m_restore_access_info(NULL)
	, m_init_success(TRUE)
{
	m_from_file = OP_NEW(OpFile, ());
	if (m_from_file == NULL)
		m_init_success = FALSE;

	if (OpStatus::IsError(m_from_file->Copy(&from_file)))
		m_init_success = FALSE;

	m_to_file = OP_NEW(OpFile, ());
	if (m_to_file == NULL)
		m_init_success = FALSE;

	if (OpStatus::IsError(m_to_file->Copy(&to_file)))
		m_init_success = FALSE;
}

OP_STATUS MoveFileOperation::Do()
{
	if (m_init_success == FALSE)
		return OpStatus::ERR;

	BOOL exists;
	RETURN_IF_ERROR(m_from_file->Exists(exists));
	if (!exists)
		return OpStatus::ERR;

	OpString file_path;
	RETURN_IF_ERROR(file_path.Set(m_from_file->GetFullPath()));

	if (!WindowsUtils::CheckObjectAccess(file_path, SE_FILE_OBJECT, DELETE))
	{
		if (m_restore_access_info || !WindowsUtils::GiveObjectAccess(file_path, SE_FILE_OBJECT, DELETE, FALSE, m_restore_access_info))
			return OpStatus::ERR;
	}

	RETURN_IF_ERROR(DesktopOpFileUtils::Move(m_from_file, m_to_file));
	if (m_restore_access_info)
		RETURN_IF_ERROR(m_restore_access_info->object_name.Set(m_to_file->GetFullPath()));

	return OpStatus::OK;
}

void MoveFileOperation::Undo()
{
	if (OpStatus::IsSuccess(DesktopOpFileUtils::Move(m_to_file, m_from_file)) && m_restore_access_info)
		m_restore_access_info->object_name.Set(m_from_file->GetFullPath());
}

MoveFileOperation::~MoveFileOperation()
{
	if (m_restore_access_info)
		WindowsUtils::RestoreObjectAccess(m_restore_access_info);

	OP_DELETE(m_from_file);
	OP_DELETE(m_to_file);
}