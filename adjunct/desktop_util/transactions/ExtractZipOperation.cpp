/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/adt/finalizer.h"
#include "adjunct/desktop_util/transactions/CreateFileOperation.h"
#include "adjunct/desktop_util/transactions/ExtractZipOperation.h"
#include "modules/util/path.h"
#include "modules/util/zipload.h"


ExtractZipOperation::ExtractZipOperation(const OpZip& archive,
		const OpStringC& extract_to_path)
	: m_archive(&archive)
{
	const OP_STATUS status = m_extract_to_path.Set(extract_to_path);
	OP_ASSERT(OpStatus::IsSuccess(status));
}


OP_STATUS ExtractZipOperation::Do()
{
	OP_ASSERT(NULL != m_archive);

	// Protect ourselves against returning with partial results on error, and
	// thus fulfill the atomicity requirement imposed by OpTransaction.
	BOOL done = FALSE;
	Finalizer on_return(BindableFunction(&UndoUnless, &done, this));

	OpAutoVector<OpString> file_names;
	RETURN_IF_ERROR(const_cast<OpZip*>(m_archive)->GetFileNameList(file_names));

	for (UINT32 i = 0; i < file_names.GetCount(); ++i)
	{
		const OpString& src_file_path = *file_names.Get(i);
		OP_ASSERT(src_file_path.HasContent());
		if (!src_file_path.HasContent())
		{
			continue;
		}

		OpString dest_file_path;
		RETURN_IF_ERROR(OpPathDirFileCombine(dest_file_path, m_extract_to_path,
					src_file_path));
		OpFile dest_file;
		RETURN_IF_ERROR(dest_file.Construct(dest_file_path));

		const OpFileInfo::Mode file_mode =
				PATHSEPCHAR == src_file_path[src_file_path.Length() - 1]
						? OpFileInfo::DIRECTORY : OpFileInfo::FILE;
		OpAutoPtr<CreateFileOperation> create_file_operation(
				OP_NEW(CreateFileOperation, (dest_file, file_mode)));
		RETURN_OOM_IF_NULL(create_file_operation.get());

		RETURN_IF_ERROR(create_file_operation->Do());

		RETURN_IF_ERROR(m_create_file_operations.Add(
					create_file_operation.get()));
		create_file_operation.release();

		RETURN_IF_ERROR(const_cast<OpZip*>(m_archive)->Extract(
					const_cast<OpString*>(&src_file_path), &dest_file_path));
	}

	done = TRUE;
	return OpStatus::OK;
}


void ExtractZipOperation::Undo()
{
	for (UINT32 i = m_create_file_operations.GetCount(); i >= 1; --i)
	{
		m_create_file_operations.Get(i - 1)->Undo();
	}
}


void ExtractZipOperation::UndoUnless(const BOOL* condition,
		ExtractZipOperation* operation)
{
	OP_ASSERT(NULL != condition && NULL != operation);
	if (!*condition)
	{
		operation->Undo();
	}
}


const ExtractZipOperation::SubOperations&
		ExtractZipOperation::GetSubOperations() const
{
	return m_create_file_operations;
}
