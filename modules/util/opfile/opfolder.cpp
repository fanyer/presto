/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/opfile/opfolder.h"

// OpFolderManager

void
OpFolderManager::InitL()
{
	OP_ASSERT(sizeof(OpFileFolder) <= sizeof(unsigned int)); // Assumed by IsValid, q.v.

#ifdef DYNAMIC_FOLDER_SUPPORT
	for (int i=0; i < OPFILE_FOLDER_COUNT; ++i)
	{
		OpStackAutoPtr<OpString> s(OP_NEW_L(OpString, ()));
		LEAVE_IF_ERROR(m_paths.Add(s.get()));
		s.release();
	}
#endif // DYNAMIC_FOLDER_SUPPORT

#ifdef FOLDER_PARENT_SUPPORT
	for (int j=0; j < OPFILE_FOLDER_COUNT; ++j)
	{
# ifdef DYNAMIC_FOLDER_SUPPORT
		LEAVE_IF_ERROR(m_parents.Add(OPFILE_ABSOLUTE_FOLDER));
# else
		m_parents[j] = OPFILE_ABSOLUTE_FOLDER;
# endif
	}
#endif // FOLDER_PARENT_SUPPORT
}

OP_STATUS
OpFolderManager::SetFolderPath(OpFileFolder folder, const uni_char* path, OpFileFolder parent)
{
	if (!IsValid(folder))
		return OpStatus::ERR;
#ifdef FOLDER_PARENT_SUPPORT
	if (!IsValid(parent))
		return OpStatus::ERR;
#else
	((void)parent);
	OP_ASSERT(parent == OPFILE_ABSOLUTE_FOLDER || !"TWEAK_UTIL_FOLDER_PARENT not enabled.");
#endif // !FOLDER_PARENT_SUPPORT

	OpString old_path;
	OP_STATUS err = GetFolderPath(folder, old_path);
	BOOL same_path = FALSE;
	if (OpStatus::IsSuccess(err) && old_path.CStr())
	{
		same_path = uni_str_eq(path ? path : UNI_L(""), old_path.CStr());
	}
#ifdef FOLDER_PARENT_SUPPORT
	if (same_path && parent == GetParent(folder))
#else
	if (same_path)
#endif // FOLDER_PARENT_SUPPORT
		return OpStatus::OK;

	OpString* s = GetPath(folder);
	RETURN_IF_ERROR(s->Set(path));

	unsigned int len = s->Length();
	if (len && s->CStr()[len-1] != PATHSEPCHAR)
	{
		RETURN_IF_ERROR(s->Append(PATHSEP));
	}

#ifdef FOLDER_PARENT_SUPPORT
	RETURN_IF_ERROR(SetParent(folder, parent));
#endif // FOLDER_PARENT_SUPPORT

	return OpStatus::OK;
}

OP_STATUS
OpFolderManager::GetFolderPath(OpFileFolder folder, OpString & ret_path)
{
	if (!IsValid(folder))
		return OpStatus::ERR;

	ret_path.Empty();
#ifdef FOLDER_PARENT_SUPPORT
# ifdef DYNAMIC_FOLDER_SUPPORT
	OpFileFolder parent = (OpFileFolder)m_parents.Get(folder);
# else
	OpFileFolder parent = m_parents[folder];
# endif
	if (parent != OPFILE_ABSOLUTE_FOLDER)
		RETURN_IF_ERROR(GetFolderPath(parent, ret_path));
#endif // FOLDER_PARENT_SUPPORT

#ifdef DYNAMIC_FOLDER_SUPPORT
	RETURN_IF_ERROR(ret_path.Append(*m_paths.Get(folder)));
#else // DYNAMIC_FOLDER_SUPPORT
	RETURN_IF_ERROR(ret_path.Append(m_paths[folder]));
#endif // DYNAMIC_FOLDER_SUPPORT

	return OpStatus::OK;
}

const uni_char*
OpFolderManager::GetFolderPathIgnoreErrors(OpFileFolder folder, OpString & storage)
{
	//OP_ASSERT(!"You shouldn't use this method.  It is unsafe.");
	OP_STATUS err = GetFolderPath(folder, storage);
	if (OpStatus::IsError(err))
	{
		// This is what the old code would do on errors
		return 0;
	};
	return storage.CStr();
};

#ifdef DYNAMIC_FOLDER_SUPPORT
OP_STATUS
OpFolderManager::AddFolder(OpFileFolder parent, const uni_char* path, OpFileFolder* result)
{
	int const next_id = m_paths.GetCount(); // *before* we .Add() and change it
	if (!IsValid(parent))
		return OpStatus::ERR;

	OpStackAutoPtr<OpString> s(OP_NEW(OpString, ()));
	if (!s.get())
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_paths.Add(s.get()));
	s.release();

#ifdef FOLDER_PARENT_SUPPORT
	RETURN_IF_ERROR(m_parents.Add(parent));
#endif // FOLDER_PARENT_SUPPORT

	OpString p;
	RETURN_IF_ERROR(GetFolderPath(parent, p));
	OpString ns;
	RETURN_IF_ERROR(ns.SetConcat(p.CStr(), path));

	*result = (OpFileFolder)next_id;
	return SetFolderPath(*result, ns.CStr());
}
#endif // DYNAMIC_FOLDER_SUPPORT

#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

OP_STATUS OpFolderManager::AddLocaleFolder(OpFileFolder parent, const uni_char* path)
{
	OpFileFolder locale_folder_id;
	RETURN_IF_ERROR(AddFolder(parent, path, &locale_folder_id));
	RETURN_IF_ERROR(m_locale_folders.Add(static_cast<INT32>(locale_folder_id)));
	return OpStatus::OK;
}

#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

