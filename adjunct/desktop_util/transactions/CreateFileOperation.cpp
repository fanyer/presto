/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/desktop_util/transactions/CreateFileOperation.h"


CreateFileOperation::PathComponent::PathComponent(OpString& component)
{
	m_component.TakeOver(component);
}

const OpStringC& CreateFileOperation::PathComponent::Get() const
{
	return m_component;
}


CreateFileOperation::CreateFileOperation(const OpFile& file,
		OpFileInfo::Mode file_mode)
	: m_file_mode(file_mode)
{
	const OP_STATUS copied = m_file.Copy(&file);
	OP_ASSERT(OpStatus::IsSuccess(copied) || !"Could not create OpFile copy");
}


OP_STATUS CreateFileOperation::Do()
{
	m_components.Clear();

	// Determine the chances that this operation will succeed.

	BOOL exists = FALSE;
	RETURN_IF_ERROR(m_file.Exists(exists));
	if (exists)
	{
		OpFileInfo::Mode mode = OpFileInfo::NOT_REGULAR;
		RETURN_IF_ERROR(m_file.GetMode(mode));
		if (mode != m_file_mode)
		{
			return OpStatus::ERR;
		}
		if (!m_file.IsWritable())
		{
			return OpStatus::ERR;
		}
	}

	// Find out what path components (directories) will be created if this
	// operation succeeds.

	OpFile dir;
	RETURN_IF_ERROR(dir.Copy(&m_file));

	BOOL dir_exists = FALSE;
	while (!dir_exists)
	{
		OpString path;
		RETURN_IF_ERROR(dir.GetDirectory(path));

		OP_ASSERT(path.HasContent()
				|| !"This loop needs another stop condition");

		RETURN_IF_ERROR(dir.Construct(path));
		RETURN_IF_ERROR(dir.Exists(dir_exists));
		if (dir_exists)
		{
			// Bail out if the current path component names an existing file.
			OpFileInfo::Mode mode = OpFileInfo::NOT_REGULAR;
			RETURN_IF_ERROR(dir.GetMode(mode));
			if (OpFileInfo::DIRECTORY != mode)
			{
				return OpStatus::ERR;
			}
		}
		else
		{
			// OpFile::GetDirectory() doesn't work as advertised on UNIX.  Need to 
			// remove any trailing path separators.
			if (path.HasContent() && PATHSEPCHAR == path[path.Length() - 1])
			{
				path.Delete(path.Length() - 1);
			}
			RETURN_IF_ERROR(dir.Construct(path));

			PathComponent* component = OP_NEW(PathComponent, (path));
			RETURN_OOM_IF_NULL(component);
			component->Into(&m_components);
		}
	}

	return OpStatus::OK;
}


void CreateFileOperation::Undo()
{
	OpStatus::Ignore(m_file.Close());
	if (OpFileInfo::DIRECTORY == m_file_mode)
	{
		OpStatus::Ignore(DeleteDirIfEmpty(m_file.GetFullPath()));
	}
	else
	{
		OpStatus::Ignore(m_file.Delete(FALSE));
	}

	for (PathComponent* component = m_components.First(); NULL != component;
			component = component->Suc())
	{
		OpStatus::Ignore(DeleteDirIfEmpty(component->Get()));
	}
}


OP_STATUS CreateFileOperation::DeleteDirIfEmpty(const OpStringC& path)
{
	BOOL dir_empty = FALSE;
	const OP_STATUS status = DesktopOpFileUtils::IsFolderEmpty(path, dir_empty);
	if (OpStatus::ERR_FILE_NOT_FOUND == status)
	{
		// This can happen when the original file path had a trailing separator.
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(status);

	if (dir_empty)
	{
		OpFile dir;
		RETURN_IF_ERROR(dir.Construct(path));
		RETURN_IF_ERROR(dir.Delete(FALSE));
	}
	return OpStatus::OK;
}


const OpFile& CreateFileOperation::GetFile() const
{
	return m_file;
}

OpFileInfo::Mode CreateFileOperation::GetFileMode() const
{
	return m_file_mode;
}

const CreateFileOperation::PathComponentList&
		CreateFileOperation::GetPathComponents() const
{
	return m_components;
}
