/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "adjunct/desktop_util/file_utils/folder_recurse.h"

FolderRecursor::FolderRecursor(INT32 max_depth)
	: max_stack_depth(max_depth)
{
}

FolderRecursor::~FolderRecursor()
{
}

OP_STATUS FolderRecursor::SetRootFolder(const OpString& root)
{
	OpFolderLister * lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), root.CStr());
	if (lister)
	{
		return lister_stack.Add(lister);
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS FolderRecursor::GetNextFile(OpFile*& file)
{
	const OpStringC THIS_FOLDER(UNI_L("."));
	const OpStringC PARENT_FOLDER(UNI_L(".."));

	file = NULL;

	while (lister_stack.GetCount() > 0)
	{
		OpFolderLister* lister = lister_stack.Get(lister_stack.GetCount() - 1);
		while (lister->Next())
		{
			if (lister->IsFolder())
			{
				if (0 <= max_stack_depth
						&& UINT32(max_stack_depth + 1) == lister_stack.GetCount())
				{
					continue;
				}

				if (THIS_FOLDER == OpStringC(lister->GetFileName())
						|| PARENT_FOLDER == OpStringC(lister->GetFileName()))
				{
					continue;
				}

				lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER,
						UNI_L("*"), lister->GetFullPath());
				if (NULL != lister)
				{
					RETURN_IF_ERROR(lister_stack.Add(lister));
					continue;
				}
				else
				{
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			else
			{
				OpAutoPtr<OpFile> file_holder(OP_NEW(OpFile, ()));
				if (NULL != file_holder.get())
				{
					RETURN_IF_ERROR(
							file_holder->Construct(lister->GetFullPath()));
					file = file_holder.release();
					return OpStatus::OK;
				}
				else
				{
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}

		lister_stack.Delete(lister_stack.GetCount() - 1);
	}

	return OpStatus::OK;
}