/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT

#include "modules/device_api/jil/JILFSMgr.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"

#include "modules/url/protocols/comm.h"

#if defined OPERA_CONSOLE
#include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

#if PATHSEPCHAR == '/'
#define INVALIDPATHSEPCHAR '\\'
#else // PATHSEPCHAR != '/'
#define INVALIDPATHSEPCHAR '/'
#endif // PATHSEPCHAR

JILFSMgr::JILFSMgr()
: m_drive_monitor(NULL)
, m_fs_mapping_manager()
{
}

OP_STATUS
JILFSMgr::Construct()
{
	OpStatus::Ignore(ParseMappingFile()); // Mapping file parsing failed - ignore the error as mapping might be added later with proper API

	RETURN_IF_ERROR(OpStorageMonitor::Create(&m_drive_monitor, this));

	m_drive_monitor->StartMonitoring();

	return OpStatus::OK;
}

/* virtual */
JILFSMgr::~JILFSMgr()
{
	if (m_drive_monitor)
		m_drive_monitor->StopMonitoring();
	OP_DELETE(m_drive_monitor);
}

OP_STATUS
JILFSMgr::OnStorageMounted(const StorageInfo &info)
{
	if ((uni_strlen(info.mountpoint) == 1) && (info.mountpoint[0] == PATHSEPCHAR))
		// Special handling for root of the filesystem
		return m_fs_mapping_manager.SetMounted(UNI_L(""), TRUE);
	else
		return m_fs_mapping_manager.SetMounted(info.mountpoint, TRUE);
}

OP_STATUS
JILFSMgr::OnStorageUnmounted(const StorageInfo &info)
{
	if ((uni_strlen(info.mountpoint) == 1) && (info.mountpoint[0] == PATHSEPCHAR))
		// Special handling for root of the filesystem
		return m_fs_mapping_manager.SetMounted(UNI_L(""), FALSE);
	else
		return m_fs_mapping_manager.SetMounted(info.mountpoint, FALSE);
}

OP_STATUS
JILFSMgr::FSMappingMgr::PopulateDataFromFile()
{
	DeleteAll();

	const OpFile *mapping_file = g_pcfiles->GetFile(PrefsCollectionFiles::DeviceSettingsFile);

	RETURN_VALUE_IF_NULL(mapping_file, OpStatus::ERR);

	PrefsFile drive_mapping(PREFS_INI, 0 ,0);
	RETURN_IF_LEAVE(drive_mapping.ConstructL());
	RETURN_IF_LEAVE(drive_mapping.SetFileL(mapping_file));

	RETURN_IF_LEAVE(drive_mapping.LoadAllL());

	OpString section;
	section.Set("Mappings Info");
	PrefsSection *prefs_section = NULL;
	RETURN_IF_LEAVE(prefs_section = drive_mapping.ReadSectionL(section));
	RETURN_VALUE_IF_NULL(prefs_section, OpStatus::ERR);
	OpAutoPtr<PrefsSection> ap_prefs_section(prefs_section);
	const PrefsEntry *prefs_entry = prefs_section->FindEntry(UNI_L("Count")); //get mapping count
	RETURN_VALUE_IF_NULL(prefs_entry, OpStatus::ERR);
	int count = uni_atoi(prefs_entry->Value());

	for (int cnt = 1; cnt < count + 1; ++cnt) // mappings are numerated starting from 1
	{
		section.Empty();
		RETURN_IF_ERROR(section.AppendFormat(UNI_L("Mapping %d"), cnt));

		RETURN_IF_LEAVE(prefs_section = drive_mapping.ReadSectionL(section));
		RETURN_VALUE_IF_NULL(prefs_section, OpStatus::ERR);
		ap_prefs_section.reset(prefs_section);
		const PrefsEntry *mnt_prefs_entry = prefs_section->FindEntry(UNI_L("Mount Point"));
		RETURN_VALUE_IF_NULL(mnt_prefs_entry, OpStatus::ERR);
		const uni_char *mount_point = mnt_prefs_entry->Value();
		RETURN_VALUE_IF_NULL(mount_point, OpStatus::ERR);
		prefs_entry = prefs_section->Entries();
		RETURN_VALUE_IF_NULL(prefs_entry, OpStatus::ERR);

		while (prefs_entry)
		{
			if (prefs_entry != mnt_prefs_entry) // For entries other than Mount Point entry...
				RETURN_IF_ERROR(AddData(mount_point, prefs_entry->Key(), prefs_entry->Value()));
			prefs_entry = prefs_entry->Suc();
		}
	}

	return OpStatus::OK;
}

OP_STATUS
JILFSMgr::ParseMappingFile()
{
	return m_fs_mapping_manager.PopulateDataFromFile();
}

OP_STATUS
JILFSMgr::FSMappingMgr::CheckVirtualRootClash(const uni_char *virt)
{
	OP_ASSERT(virt);

	OpAutoVector<OpString> virt_roots;
	GetData(NULL, &virt_roots, FALSE);
	for (UINT32 i = 0; i < virt_roots.GetCount(); ++i)
	{
		size_t target_len = MIN(uni_strlen(virt), static_cast<size_t>(virt_roots.Get(i)->Length()));
		if (uni_strncmp(virt, virt_roots.Get(i)->CStr(), target_len) == 0)
		{
#ifdef OPERA_CONSOLE
			if (g_console->IsLogging())
			{
				OpConsoleEngine::Message message(OpConsoleEngine::Gadget, OpConsoleEngine::Error);
				OpStatus::Ignore(message.message.Set(UNI_L("JIL filesystem: Invalid virtual roots! None of virtual roots may be subset of any other.")));
				TRAPD(result, g_console->PostMessageL(&message));
			}
#endif // OPERA_CONSOLE
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

JILFSMgr::FSMappingMgr::MountPointInfo *
JILFSMgr::FSMappingMgr::GetOrCreateMountPointInfo(const uni_char* mount_point_path)
{
	OP_ASSERT(mount_point_path);

	MountPointInfo *mount_point_info;

	// Cast avoids compiler warnings and mount points will rather not have length in GBs.
	int length = static_cast<int>(uni_strlen(mount_point_path));
	if (length > 0 && mount_point_path[length - 1] == PATHSEPCHAR)
		--length;

	OpString normalized_mount_point_path;
	RETURN_VALUE_IF_ERROR(normalized_mount_point_path.Set(mount_point_path, length), NULL);

	OP_STATUS status = m_mount_points.GetData(normalized_mount_point_path.CStr(), &mount_point_info);
	if (OpStatus::IsError(status))
	{
		mount_point_info = OP_NEW(MountPointInfo, ());
		OpAutoPtr<MountPointInfo> ap_mount_point_info(mount_point_info);
		if (!mount_point_info)
			return NULL;

		RETURN_VALUE_IF_ERROR(mount_point_info->mount_point.Set(normalized_mount_point_path.CStr()), NULL);
		RETURN_VALUE_IF_ERROR(m_mount_points.Add(mount_point_info->mount_point.CStr(), mount_point_info), NULL);
		ap_mount_point_info.release();
	}
	return mount_point_info;
}

OP_STATUS
JILFSMgr::FSMappingMgr::AddData(const uni_char *mount_point, const uni_char *real, const uni_char *virt, BOOL mounted /* = FALSE */)
{
	OP_ASSERT(mount_point);
	OP_ASSERT(real);
	OP_ASSERT(virt);

	RETURN_IF_ERROR(CheckVirtualRootClash(virt));

	OpString real_dir;
	RETURN_IF_ERROR(real_dir.Set(real));
	JILFSMgr::ToSystemPathSeparator(real_dir.DataPtr());

	MountPointInfo *mount_point_info = GetOrCreateMountPointInfo(mount_point);
	RETURN_OOM_IF_NULL(mount_point_info);

	MappingType *fs_mapping = OP_NEW(MappingType, ());
	RETURN_OOM_IF_NULL(fs_mapping);
	OP_STATUS construct_status = fs_mapping->Construct(mount_point_info, real_dir.CStr(), virt);
	if (OpStatus::IsError(construct_status))
	{
		OP_DELETE(fs_mapping);
		return construct_status;
	}

	fs_mapping->Into(&(mount_point_info->mappings));

	if (mounted)
		mount_point_info->is_mounted = TRUE;

	return OpStatus::OK;
}

OP_STATUS
JILFSMgr::FSMappingMgr::RemoveData(const uni_char *mount_point, const uni_char *real, const uni_char *virt)
{
	OP_ASSERT(mount_point);
	OP_ASSERT((real && virt) || (!virt && !real));

	OpString converted_real_dir;
	const uni_char *real_dir = real;
	if (real_dir)
	{
		RETURN_IF_ERROR(converted_real_dir.Set(real_dir));
		JILFSMgr::ToSystemPathSeparator(converted_real_dir.DataPtr());
		real_dir = converted_real_dir.CStr();
	}

	OpString mount_point_stripped;
	const uni_char *normalized_mount_point = mount_point;
	int length = static_cast<int>(uni_strlen(mount_point));
	if (uni_strrchr(mount_point, PATHSEPCHAR) == mount_point + length - 1)
	{
		RETURN_IF_ERROR(mount_point_stripped.Set(mount_point, length - 1));
		normalized_mount_point = mount_point_stripped.CStr();
	}

	MountPointInfo *mount_point_info;
	OP_STATUS status = m_mount_points.GetData(normalized_mount_point, &mount_point_info);
	if (OpStatus::IsError(status))
		return OpStatus::ERR_FILE_NOT_FOUND;

	if (!real_dir && !virt)
	{
		mount_point_info->mappings.Clear();
		return OpStatus::OK;
	}

	MappingType *mapping = mount_point_info->mappings.First();
	while (mapping)
	{
		if (mapping->real.Compare(real_dir) == 0 && mapping->virt.Compare(virt) == 0)
		{
			MappingType *mapping_suc = mapping->Suc();
			mapping->Out();
			OP_DELETE(mapping);
			mapping = mapping_suc;
		}
		else
			mapping = mapping->Suc();
	}

	return OpStatus::OK;
}

/* static */
void
JILFSMgr::ToJILPathSeparator(uni_char* path)
{
#if PATHSEPCHAR != JILPATHSEPCHAR
	uni_char* p = path;
	while (*p != 0)
	{
		if (*p == PATHSEPCHAR)
			*p = JILPATHSEPCHAR;
		++p;
	}
#endif
}

/* static */
void
JILFSMgr::ToSystemPathSeparator(uni_char* path)
{
	uni_char* p = path;
	while (*p != 0)
	{
		if (*p == INVALIDPATHSEPCHAR)
			*p = PATHSEPCHAR;
		++p;
	}
}

/* static */ OP_STATUS
JILFSMgr::SafeJoinPath(OpString *result, const uni_char* path1, const uni_char* path2, uni_char path_separator)
{
	OP_ASSERT(result);
	OP_ASSERT(path1);
	OP_ASSERT(path2);
	OP_ASSERT((path1[uni_strlen(path1) - 1] != PATHSEPCHAR) && (path1[uni_strlen(path1) - 1] != JILPATHSEPCHAR));
	result->Empty();

	// One can never be sure of the input here...
	if ((path2[0] != PATHSEPCHAR) && (path2[0] != JILPATHSEPCHAR))
		RETURN_IF_ERROR(result->AppendFormat(UNI_L("%s%c%s"), path1, path_separator, path2));
	else
		RETURN_IF_ERROR(result->AppendFormat(UNI_L("%s%s"), path1, path2));

	return OpStatus::OK;
}

OP_STATUS
JILFSMgr::MapRealToVirtual(const uni_char *real_path, OpString &virt_path)
{
	RETURN_VALUE_IF_NULL(real_path, OpStatus::ERR);

	OpString found_virt;
	OP_STATUS find_status = m_fs_mapping_manager.FindVirtualFromReal(real_path, uni_strcmp, found_virt); // Check exact matching
	if (find_status == OpStatus::ERR_FILE_NOT_FOUND)
	{
		// Check partial matching if exact has not been found
		find_status = m_fs_mapping_manager.FindVirtualFromReal(real_path, CmpIfPathFromRoot, found_virt);
	}
	RETURN_IF_ERROR(find_status);

	OpString found_real;
	find_status = m_fs_mapping_manager.FindRealFromVirtual(found_virt, uni_strcmp, found_real);
	OP_ASSERT(find_status != OpStatus::ERR_FILE_NOT_FOUND);
	RETURN_IF_ERROR(find_status);

	RETURN_IF_ERROR(SafeJoinPath(&virt_path, found_virt.CStr(), real_path + found_real.Length(), JILPATHSEPCHAR));

	return OpStatus::OK;
}

OP_STATUS
JILFSMgr::MapVirtualToReal(const uni_char *virt_path, OpString &real_path)
{
	RETURN_VALUE_IF_NULL(virt_path, OpStatus::ERR);

	OpString found_real;
	OP_STATUS find_status = m_fs_mapping_manager.FindRealFromVirtual(virt_path, uni_strcmp, found_real); // Check exact matching
	if (find_status == OpStatus::ERR_FILE_NOT_FOUND)
	{
		// Check partial matching if exact has not been found
		find_status = m_fs_mapping_manager.FindRealFromVirtual(virt_path, CmpIfPathFromRoot, found_real);
	}
	RETURN_IF_ERROR(find_status);

	OpString found_virtual;
	find_status = m_fs_mapping_manager.FindVirtualFromReal(found_real, uni_strcmp, found_virtual);
	OP_ASSERT(find_status != OpStatus::ERR_FILE_NOT_FOUND);
	RETURN_IF_ERROR(find_status);

	RETURN_IF_ERROR(SafeJoinPath(&real_path, found_real.CStr(), virt_path + found_virtual.Length(), PATHSEPCHAR));

	return OpStatus::OK;
}

BOOL
JILFSMgr::FSMappingMgr::FSMapping::IsActive()
{
	if (mount_point_info->is_mounted)
	{
		OpString path;
		if (OpStatus::IsSuccess(SafeJoinPath(&path, mount_point_info->mount_point.CStr(),
											 real, PATHSEPCHAR)))
		{
			OpFile file;
			if (OpStatus::IsSuccess(file.Construct(path)))
			{
				BOOL exists = FALSE;
				file.Exists(exists);
				return exists;
			}
		}
	}
	return FALSE;
}

OP_STATUS
JILFSMgr::FSMappingMgr::SetMounted(const uni_char *mountpoint, BOOL mounted)
{
	MountPointInfo *mount_point_info = GetOrCreateMountPointInfo(mountpoint);
	RETURN_OOM_IF_NULL(mount_point_info);
	mount_point_info->is_mounted = mounted;

	return OpStatus::OK;
}

OP_STATUS
JILFSMgr::FSMappingMgr::FSMapping::Construct(MountPointInfo *mount_point_info_arg, const uni_char *real_path, const uni_char *virt_path)
{
	OP_ASSERT(mount_point_info_arg);
	OP_ASSERT(real_path);
	OP_ASSERT(virt_path);

	mount_point_info = mount_point_info_arg;

	if (real_path[0] != PATHSEPCHAR)
		return OpStatus::ERR_PARSING_FAILED;

	int length = static_cast<int>(uni_strlen(real_path));
	if (length > 0 && real_path[length - 1] == PATHSEPCHAR)
		--length;
	RETURN_IF_ERROR(real.Set(real_path, length));

	length = static_cast<int>(uni_strlen(virt_path));
	if (length > 1 && virt_path[length - 1] == JILPATHSEPCHAR)
		--length;
	return virt.Set(virt_path, length);
}

OP_STATUS
JILFSMgr::FSMappingMgr::GetData(OpVector<OpString> *real, OpVector<OpString> *virt, BOOL active_only /* = TRUE */)
{
	return m_data_grabber.CollectData(real, virt, active_only);
}

OP_STATUS
JILFSMgr::FSMappingMgr::FSMappingDataGrabber::CollectData(OpVector<OpString> *real_path, OpVector<OpString> *virt_path, BOOL active_only)
{
	if (real_path == NULL && virt_path == NULL)
		return OpStatus::ERR_NULL_POINTER;

	OpHashIterator *iter = m_mount_points->GetIterator();
	if (!iter)
		return OpStatus::ERR;

	OpAutoPtr<OpHashIterator> ap_iter(iter);

	OP_STATUS status = OpStatus::OK;

	MountPointInfo *mount_point_info = NULL;

	status = iter->First();
	while (OpStatus::IsSuccess(status) &&
		 (mount_point_info = static_cast<MountPointInfo *>(iter->GetData())))
	{
		MappingType *mapping = mount_point_info->mappings.First();
		while (mapping)
		{
			if ((active_only && mapping->IsActive()) || !active_only)
			{
				if (real_path)
				{
					OpString *path = OP_NEW(OpString, ());
					RETURN_OOM_IF_NULL(path);
					OpAutoPtr<OpString> ap_path(path);

					RETURN_IF_ERROR(ap_path.get()->Set(mount_point_info->mount_point));
					RETURN_IF_ERROR(ap_path.get()->Append(mapping->real));
					RETURN_IF_ERROR(real_path->Add(ap_path.get()));
					ap_path.release();
				}

				if (virt_path)
				{
					OpString *path = OP_NEW(OpString, ());
					RETURN_OOM_IF_NULL(path);
					OpAutoPtr<OpString> ap_path(path);

					RETURN_IF_ERROR(ap_path.get()->Set(mapping->virt));
					RETURN_IF_ERROR(virt_path->Add(ap_path.get()));
					ap_path.release();
				}
			}

			mapping = mapping->Suc();
		}

		status = iter->Next();
	}

	return OpStatus::OK;
}

OP_STATUS
JILFSMgr::FSMappingMgr::FSMappingDataGrabber::Find(int find_params, const uni_char *ref, int (*cmp)(const uni_char *, const uni_char *), OpString &result, BOOL active_only /* = TRUE */)
{
	if (!ref || !cmp)
		return OpStatus::ERR_NULL_POINTER;

	OpHashIterator *iter = m_mount_points->GetIterator();

	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<OpHashIterator> ap_iter(iter);

	OP_STATUS status = iter->First();

	result.Empty();
	MountPointInfo *mount_point_info = NULL;
	while (OpStatus::IsSuccess(status) && (mount_point_info = static_cast<MountPointInfo *>(iter->GetData())))
	{
		MappingType *mapping = mount_point_info->mappings.First();
		while (mapping)
		{
			OpString compare;
			OpString ret;
			// Fill proper return values and compare patterns depending on type of find operation specified by find_params
			if (find_params & REAL)
			{
				RETURN_IF_ERROR(compare.AppendFormat(UNI_L("%s%s"), mount_point_info->mount_point.CStr(), mapping->real.CStr()));
				if ((find_params & MOUNT_POINT))
					RETURN_IF_ERROR(ret.Set(mount_point_info->mount_point.CStr()));
				else
					RETURN_IF_ERROR(ret.Set(mapping->virt));
			}
			else
			{
				RETURN_IF_ERROR(compare.Set(mapping->virt));
				RETURN_IF_ERROR(ret.Set(mount_point_info->mount_point));
				if (!(find_params & MOUNT_POINT))
					RETURN_IF_ERROR(ret.Append(mapping->real));
			}

			if (cmp(compare, ref) == 0 && (mapping->IsActive() || !active_only))
			{
				result.Set(ret);
				return OpStatus::OK;
			}

			mapping = mapping->Suc();
		}

		status = iter->Next();
	}

	return OpStatus::ERR_FILE_NOT_FOUND;
}

OP_STATUS
JILFSMgr::FSMappingMgr::FindMountPointFromVirtual(const uni_char *virt, int (*cmp_func)(const uni_char *, const uni_char *), OpString &result, BOOL active_only /* = TRUE */)
{
	return m_data_grabber.Find(FSMappingDataGrabber::VIRTUAL | FSMappingDataGrabber::MOUNT_POINT, virt, cmp_func, result, active_only);
}

OP_STATUS
JILFSMgr::FSMappingMgr::FindVirtualFromReal(const uni_char *real, int (*cmp_func)(const uni_char *, const uni_char *), OpString &result, BOOL active_only /* = TRUE */)
{
	return m_data_grabber.Find(FSMappingDataGrabber::REAL, real, cmp_func, result, active_only);
}

OP_STATUS
JILFSMgr::FSMappingMgr::FindRealFromVirtual(const uni_char *virt, int (*cmp_func)(const uni_char *, const uni_char *), OpString &result, BOOL active_only /* = TRUE */)
{
	return m_data_grabber.Find(FSMappingDataGrabber::VIRTUAL, virt, cmp_func, result, active_only);
}

OP_STATUS
JILFSMgr::GetFileSystemSize(const uni_char *fs, UINT64 *size)
{
	if (!fs || !size)
		return OpStatus::ERR_NULL_POINTER;

	OpString mount_point;
	RETURN_IF_ERROR(m_fs_mapping_manager.FindMountPointFromVirtual(fs, uni_strcmp, mount_point));

	StorageInfo info;
	info.mountpoint = mount_point;

	*size = m_drive_monitor->GetTotalSize(&info);

	return OpStatus::OK;
}

BOOL
JILFSMgr::IsVirtualRoot(const uni_char *vr, BOOL part /* = FALSE */)
{
	OpAutoVector<OpString> virt_dirs;
	RETURN_VALUE_IF_ERROR(m_fs_mapping_manager.GetData(NULL, &virt_dirs), FALSE);
	for (UINT32 cnt = 0; cnt < virt_dirs.GetCount(); ++cnt)
	{
		OP_ASSERT(virt_dirs.Get(cnt));
		OpString *virt_dir = virt_dirs.Get(cnt);
		if (part)
		{
			if (virt_dir && (uni_strncmp(vr, virt_dir->CStr(), uni_strlen(vr)) == 0))
				return TRUE;
		}
		else
		{
			if (virt_dir &&
				((uni_strcmp(vr, virt_dir->CStr()) == 0) ||
				((uni_strlen(vr) > 1) && ((vr[uni_strlen(vr) - 1] == JILPATHSEPCHAR && (uni_strncmp(vr, virt_dir->CStr(), uni_strlen(vr) - 1) == 0)) ||
				(vr[uni_strlen(vr) - 1] != JILPATHSEPCHAR && (uni_strncmp(vr, virt_dir->CStr(), uni_strlen(virt_dir->CStr()) - 1) == 0))))
				)) // ignore trailing '/'
				return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS
JILFSMgr::JILToSystemPath(const uni_char *jil_path, OpString &system_path)
{
	OP_ASSERT(jil_path);

	if (!jil_path)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(MapVirtualToReal(jil_path, system_path));

	ToSystemPathSeparator(system_path.DataPtr());

	return OpStatus::OK;
}

OP_STATUS
JILFSMgr::SystemToJILPath(const uni_char *system_path, OpString &jil_path)
{
	OP_ASSERT(system_path);

	if (!system_path)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(MapRealToVirtual(system_path, jil_path));

	ToJILPathSeparator(jil_path.DataPtr());

	return OpStatus::OK;
}

BOOL
JILFSMgr::IsFileAccessAllowed(URL url)
{
	OP_ASSERT(url.Type() == URL_FILE);
	OpString sys_path;
	OpStringC path = url.GetAttribute(URL::KUniPath);
	return OpStatus::IsSuccess(JILToSystemPath(path, sys_path));
}

#ifdef SELFTEST
OP_STATUS
JILFSMgr::SetActive(const uni_char *virt, BOOL active)
{
	OpString mount_point;
	RETURN_IF_ERROR(m_fs_mapping_manager.FindMountPointFromVirtual(virt, uni_strcmp, mount_point, FALSE));
	RETURN_IF_ERROR(m_fs_mapping_manager.SetMounted(mount_point.CStr(), active));
	return OpStatus::OK;
}

OP_BOOLEAN
JILFSMgr::FSMappingMgr::IsMounted(const uni_char *mount_point)
{
	MountPointInfo *mount_point_info;
	RETURN_IF_ERROR(m_mount_points.GetData(mount_point, &mount_point_info));
	return mount_point_info->is_mounted ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}
#endif // SELFTEST
#endif // DAPI_JIL_FILESYSTEM_SUPPORT
