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

#include "platforms/windows/installer/ReplaceFileOperation.h"
#include "adjunct/desktop_util/transactions/CreateFileOperation.h"
#include "platforms/windows/installer/DeleteFileOperation.h"

ReplaceFileOperation::ReplaceFileOperation(const OpFile &file, OpFileInfo::Mode mode)
{
	m_init_successful = TRUE;

	if ((m_create_op = OP_NEW(CreateFileOperation,(file, mode))) == NULL)
		m_init_successful = FALSE;

	BOOL exists = FALSE;
	if (OpStatus::IsSuccess(file.Exists(exists)) && exists)
	{
		if ((m_delete_op = OP_NEW(DeleteFileOperation,(file))) == NULL)
			m_init_successful = FALSE;
	}
	else
		m_delete_op = NULL;
}

OP_STATUS ReplaceFileOperation::Do()
{
	if (!m_init_successful)
		return OpStatus::ERR_NO_MEMORY;

	if (m_delete_op)
		RETURN_IF_ERROR(m_delete_op->Do());

	OP_STATUS err = m_create_op->Do();
	if (OpStatus::IsError(err))
	{
		//The CreateFileOperation can fail if there is a file which has the same name as a component of the path to be created.
		//f.ex: We try to create C:\foo\bar\baz, bu a file named C:\foo exists, preventing the creation of a C:\foo directory
		//In that case, we want to remove that file and try again

		if (m_delete_op)
		{
			//In this case, we know there already was a file existing with the full path -> fail anyway
			m_delete_op->Undo();
			return err;
		}

		//For some reason, no path components were created -> fail
		if (m_create_op->GetPathComponents().Empty())
			return err;

		CreateFileOperation::PathComponent* component = m_create_op->GetPathComponents().Last();
		OpFile file;
		BOOL exists;
		OpFileInfo::Mode mode;
		RETURN_IF_ERROR(file.Construct(component->Get()));
		//If the last path component made by the CreateFileOperation doesn't point to an existing file, fail.
		if (OpStatus::IsError(file.Exists(exists)) || !exists || OpStatus::IsError(file.GetMode(mode)) || mode != OpFileInfo::FILE)
			return err;
		else
		{
			//Otherwise, try to delete said file
			RETURN_OOM_IF_NULL(m_delete_op = OP_NEW(DeleteFileOperation,(file)));
			RETURN_IF_ERROR(m_delete_op->Do());
			//And try again
			err = m_create_op->Do();
			if (OpStatus::IsError(err))
			{
				//If it still fails, we give up.
				m_delete_op->Undo();
				return err;
			}
		}
	}

	return OpStatus::OK;
}

void ReplaceFileOperation::Undo()
{
	m_create_op->Undo();
	if (m_delete_op)
		m_delete_op->Undo();
}

ReplaceFileOperation::~ReplaceFileOperation()
{
	OP_DELETE(m_create_op);
	OP_DELETE(m_delete_op);
}