/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT

#include "modules/util/opfile/opfile.h"
#include "modules/device_api/WildcardCmp.h"
#include "modules/device_api/jil/JILFolderLister.h"

OP_STATUS
JILFolderLister::FillVirtNext(const OpString &path)
{
	RETURN_IF_ERROR(g_DAPI_jil_fs_mgr->GetAvailableVirtualDirs(&m_virt_nexts));
	for (UINT32 cnt = 0; cnt < m_virt_nexts.GetCount(); )
	{
		OP_ASSERT(m_virt_nexts.Get(cnt));
		// delete all not maching dirs
		if (uni_strncmp(m_virt_nexts.Get(cnt)->CStr(), path.CStr(), uni_strlen(path.CStr())) != 0)
		{
			m_virt_nexts.Delete(cnt);
			continue;
		}
		else
		{
			/* virt_next is filled with names of directories which are just right after path part in virtual root path e. g.
			 * when virtual roots are /virtual/music/, /virtual/photos/ and path is /virtual, "music" and "photos" will be put into virt_next.
			 * If path is "/" in the same example, "virtual" will be put into virt_next
			 */
			OpString *item = m_virt_nexts.Get(cnt);
			item->Delete(0, path.Length());
			int start_idx = 0;
			int end_idx = item->FindFirstOf(JILPATHSEPCHAR);
			if (end_idx == 0)
			{
				start_idx = 1;
				end_idx = item->Find(UNI_L(JILPATHSEP), start_idx);
			}

			if (end_idx != KNotFound) // Delete all beyond it:
				item->Delete(end_idx);

			if (start_idx > 0) // Delete all before it:
				item->Delete(0, start_idx);
		}
		++cnt;
	}

	return OpStatus::OK;
}

/* static */
int JILFolderLister::ComparePaths(const OpString **str1, const OpString **str2)
{
	return (*str1)->Compare((*str2)->CStr());
}

void
JILFolderLister::RemoveDuplicatesVirtNext()
{
	m_virt_nexts.QSort(ComparePaths);
	for (UINT32 cnt = 0; m_virt_nexts.GetCount() > 0 && cnt < m_virt_nexts.GetCount() - 1; )
	{
		OP_ASSERT(m_virt_nexts.Get(cnt));
		if (uni_strcmp(m_virt_nexts.Get(cnt)->CStr(), m_virt_nexts.Get(cnt + 1)->CStr()) == 0)
		{
			m_virt_nexts.Delete(cnt);
			continue;
		}
		++cnt;
	}
}

OP_STATUS
JILFolderLister::Construct(const OpString &path, const OpString &pattern, BOOL case_sensitive)
{
	OpString real_path;
	RETURN_IF_ERROR(m_pattern.Set(pattern.CStr()));
	if (OpStatus::IsError(g_DAPI_jil_fs_mgr->JILToSystemPath(path.CStr(), real_path)))
	{
		m_is_real = FALSE;
		RETURN_IF_ERROR(FillVirtNext(path));
		RemoveDuplicatesVirtNext();
		RETURN_IF_ERROR(m_virt_start.Set(path.CStr()));
		RETURN_IF_ERROR(m_virt_path.Set(m_virt_start));
		if (!m_virt_nexts.Get(m_virt_next_idx))
			return OpStatus::ERR;
		if (m_virt_path.CStr()[m_virt_path.Length() - 1] != JILPATHSEPCHAR)
			RETURN_IF_ERROR(m_virt_path.Append(UNI_L(JILPATHSEP)));
		RETURN_IF_ERROR(m_virt_path.Append(m_virt_nexts.Get(m_virt_next_idx)->CStr()));
	}
	else
	{
		OpFile file;
		RETURN_IF_ERROR(file.Construct(real_path.CStr()));
		/* HAPI 1.2.2:
		 * In addition to searching within the source directory specified by the matchFile,
		 * the function should also search within the whole hierarchy of any sub-directories of the specified search directory. any subdirectories.
		 *
		 * This is why whole tree is traversed and only filename is compared with wildcard
		 */
		const uni_char *real_lister_pattern = UNI_L("*");
		m_real_lister = file.GetFolderLister(OPFILE_ABSOLUTE_FOLDER, real_lister_pattern, real_path.CStr());
	}

	return OpStatus::OK;
}

BOOL
JILFolderLister::Matches(OpFileInfo *info /* = NULL */)
{
	if (info)
	{
		OpString sys_path;
		OP_STATUS result = g_DAPI_jil_fs_mgr->JILToSystemPath(GetFullPath(), sys_path);
		if (OpStatus::IsError(result))
		{
			if ((info->mode != OpFileInfo::DIRECTORY && info->mode != OpFileInfo::NOT_REGULAR) ||
				info->length != 0 || info->creation_time != 0 || info->last_modified != 0)
				return FALSE;
		}
		else
		{
			OpFile file;
			RETURN_VALUE_IF_ERROR(file.Construct(sys_path), FALSE);
			OpFileInfo i;
			i.flags = OpFileInfo::LENGTH | OpFileInfo::LAST_MODIFIED | OpFileInfo::CREATION_TIME | OpFileInfo::MODE;
			file.GetFileInfo(&i);
			if (!((info->length == 0 || info->length == i.length) &&
			(info->creation_time == 0 || info->creation_time == i.creation_time) &&
			(info->last_modified == 0 || info->last_modified == i.last_modified) &&
			(info->mode == OpFileInfo::NOT_REGULAR || info->mode == i.mode)))
				return FALSE;
		}
	}

	/* HAPI 1.2.2:
	 * In addition to searching within the source directory specified by the matchFile,
	 * the function should also search within the whole hierarchy of any sub-directories of the specified search directory. any subdirectories.
	 *
	 * This is why whole tree is traversed and only filename is compared with wildcard
	 */
	if (WildcardCmp(m_pattern, GetFileName(), m_case_sensitive) == OpBoolean::IS_TRUE)
		return TRUE;

	return FALSE;
}

BOOL
JILFolderLister::Next()
{
	if (m_is_real)
	{
		return m_real_lister->Next();
	}
	else
	{
		if (m_virt_next_idx < m_virt_nexts.GetCount())
		{
			RETURN_VALUE_IF_ERROR(m_virt_path.Set(m_virt_start), FALSE);
			OP_ASSERT(m_virt_nexts.Get(m_virt_next_idx));
			if (m_virt_path.CStr()[m_virt_path.Length() - 1] != JILPATHSEPCHAR)
				RETURN_VALUE_IF_ERROR(m_virt_path.Append(UNI_L(JILPATHSEP)), FALSE);
			RETURN_VALUE_IF_ERROR(m_virt_path.Append(m_virt_nexts.Get(m_virt_next_idx)->CStr()), FALSE);
			m_virt_next_idx++;
			return TRUE;
		}
	}

	return FALSE;
}

const uni_char *JILFolderLister::GetFileName()
{
	m_virt_name.Empty();
	if (m_is_real)
		RETURN_VALUE_IF_ERROR(m_virt_name.Set(m_real_lister->GetFileName()), NULL);
	else
	{
		OpString vp;
		RETURN_VALUE_IF_ERROR(vp.Set(GetFullPath()), NULL);
		GetFileName(vp, &m_virt_name);
	}

	return m_virt_name.CStr();
}

const uni_char *JILFolderLister::GetDirName()
{
	RETURN_VALUE_IF_ERROR(GetDirName(m_virt_path, &m_virt_dir_name, &m_virt_name), NULL);
	return m_virt_dir_name.CStr();
}

/* static */
OP_STATUS
JILFolderLister::GetFileName(const OpString &path, OpString *filename)
{
	RETURN_VALUE_IF_NULL(filename, OpStatus::ERR_NULL_POINTER);
	filename->Empty();

	int pos = path.FindLastOf(JILPATHSEPCHAR);
	OpString virt_path_substr;
	if (pos == path.Length() - 1)
	{
		RETURN_IF_ERROR(virt_path_substr.Set(path.CStr(), pos));
		pos = virt_path_substr.FindLastOf(JILPATHSEPCHAR);
	}
	else
		RETURN_IF_ERROR(virt_path_substr.Set(path));

	if (pos != KNotFound)
		RETURN_IF_ERROR(filename->Set(virt_path_substr.CStr() + pos + 1));

	return OpStatus::OK;
}

/* static */
OP_STATUS
JILFolderLister::GetDirName(const OpString &path, OpString *dirname, const OpString *filename /* = NULL */)
{
	RETURN_VALUE_IF_NULL(dirname, OpStatus::ERR_NULL_POINTER);
	dirname->Empty();

	OpString file_name;
	if (filename)
		RETURN_IF_ERROR(file_name.Set(filename->CStr()));
	else
		RETURN_IF_ERROR(GetFileName(path, &file_name));

	RETURN_IF_ERROR(dirname->Set(path.CStr(), path.Find(file_name)));
	return OpStatus::OK;
}
#endif // DAPI_JIL_FILESYSTEM_SUPPORT
