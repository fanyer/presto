/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(_PLUGIN_SUPPORT_) && !defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/windows/WindowsPluginDetector.h"

#include "modules/pi/system/OpFolderLister.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/gen_str.h"

#include "platforms/windows/windows_ui/registry.h"
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows/win_handy.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "platforms/windows_common/utils/fileinfo.h"

#define MAX_LEN	1024

OP_STATUS WindowsPluginDetector::ReadPlugins(const OpStringC& suggested_plugin_paths)
{
	OP_PROFILE_METHOD("Read plugins");

	OpString paths;

	RETURN_IF_ERROR(paths.Set(suggested_plugin_paths.IsEmpty() ?
					g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath) : suggested_plugin_paths));

	int pos = 0;
	BOOL done = FALSE;
	OP_STATUS stat = OpStatus::OK;

	while (TRUE)
	{
		uni_char *dir = MyUniStrTok(const_cast<uni_char*>(paths.CStr()), UNI_L("\n\t;"), pos, done);
		if (done)
			break;
		if (!dir || !*dir)
			continue;

		OpFolderLister *folder_lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.dll"), dir);
		if (!folder_lister)
			continue;

		while (folder_lister->Next())
		{
			const uni_char* file_path = folder_lister->GetFullPath();
			if (file_path && !folder_lister->IsFolder() && !g_pcapp->IsPluginToBeIgnored(file_path))
				stat = InsertPluginPath(file_path);
       	}

		OP_DELETE(folder_lister);
	}

	// check registry for installed plugins
	if (!m_read_from_registry)
	{
		OP_STATUS stat2 = ReadFromRegistry();
		if (OpStatus::IsError(stat2))
			stat = stat2;

		RETURN_IF_ERROR(SubmitPlugins());
	}

	return stat;

}

OP_STATUS WindowsPluginDetector::CheckReplacePlugin(PluginType type, BOOL& ignore, const OpStringC& checkedFileDescription)
{
	OP_STATUS stat = OpStatus::OK;
	ignore = FALSE;
	OpString newfname;
	uni_char szRegPath[_MAX_PATH]; /* ARRAY OK 2007-01-26 peter */
	DWORD dwSize = ARRAY_SIZE(szRegPath);

	switch (type)
	{
	case ACROBAT:
	{
		m_acrobat_installed = TRUE;
		//	Use the registry path and get the installed Adobe plugin path
		static const uni_char szKeyName[] = UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe");
		static const uni_char alt_szKeyName[]  = UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe");

		if (OpRegReadStrValue(HKEY_LOCAL_MACHINE, szKeyName, UNI_L("Path"), szRegPath, &dwSize) == ERROR_SUCCESS ||
			OpRegReadStrValue(HKEY_LOCAL_MACHINE, alt_szKeyName, UNI_L("Path"), szRegPath, &dwSize) == ERROR_SUCCESS)
		{
			RETURN_IF_ERROR(StrAppendSubDir(szRegPath, UNI_L("Browser\\nppdf32.dll"), newfname));
			RETURN_IF_ERROR(InsertPluginPath(newfname, &ignore, checkedFileDescription, TRUE));
		}
		else
		{ // ignore all Adobe plugins if AcroRd32.exe or Acrobat.exe is not in registry!
			ignore = TRUE;
		}
		break;
	}
	case SVG:
	{
		m_svg_installed = TRUE;
		HKEY hkey;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,UNI_L("SOFTWARE\\Adobe\\Adobe SVG Viewer"), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			const DWORD key_name_size = 32;
			uni_char key_name[key_name_size]; /* ARRAY OK 2007-01-26 peter */
			DWORD key_name_len;
			LONG ret = ERROR_SUCCESS;
			DWORD index = 0;

			while (OpStatus::IsSuccess(stat) && ret != ERROR_NO_MORE_ITEMS)
			{
				key_name_len = key_name_size;
				ret = RegEnumKeyEx(hkey, index++, key_name, &key_name_len, NULL, NULL, NULL, NULL);
				if (ret == ERROR_SUCCESS)
				{
					if ((OpRegReadStrValue(hkey, key_name, UNI_L("Path"), szRegPath, &dwSize) == ERROR_SUCCESS))
						stat = newfname.Set(szRegPath);
				}
			}
			RegCloseKey(hkey);
			if (OpStatus::IsSuccess(stat) && newfname.HasContent())
				RETURN_IF_ERROR(InsertPluginPath(newfname, &ignore, checkedFileDescription, TRUE));
		}
		else
		{ // ignore any Adobe SVG plugins if Adobe SVG is not in registry!
			ignore = TRUE;
		}
		break;
	}
	case QT:
	{
		// QuickTime plugin files in Opera's plugin path are ignored
		// Use the registry path and get the installed QuickTime plugin path, if any, and expand Opera's plugin path
		m_qt_installed = TRUE;
		static const uni_char szKeyName[] = UNI_L("SOFTWARE\\Apple Computer, Inc.\\QuickTime");
		if (OpRegReadStrValue(HKEY_LOCAL_MACHINE, szKeyName, UNI_L("InstallDir"), szRegPath, &dwSize) == ERROR_SUCCESS)
		{
			RETURN_IF_ERROR(StrAppendSubDir(szRegPath, UNI_L("Plugins"), newfname));
			RETURN_IF_ERROR(ReadPlugins(newfname.CStr())); // plugin path is expanded with QuickTime plugin path
		}
		else
		{
			ignore = TRUE;
		}
		break;
	}
	case WMP:
	{
		m_wmp_detection_done = TRUE;
		OpFile path;
		OpString path_string;
		BOOL exists = FALSE;

		if(IsSystemWin2000orXP())
		{
			DWORD drives, drive;

			if ((drives = GetLogicalDrives()) == 0)
			{
				ignore = TRUE;
				break;
			}

			uni_char drive_path[4] = UNI_L("*:\\");
			for (drive = 0; drive < 26; drive++)
			{
				if (drives & (1 << drive))
				{
					drive_path[0] = 'A' + drive;
					if(GetDriveType(drive_path) == DRIVE_FIXED)
					{
						path_string.Set(drive_path);
						path_string.Append(UNI_L("PFiles\\plugins\\"));
						RETURN_IF_ERROR(path.Construct(path_string.CStr()));
						if (OpStatus::IsSuccess(path.Exists(exists)) && exists)
							break;
					}
				}
			}

			if(exists)
			{
				RETURN_IF_ERROR(ReadPlugins(path_string.CStr()));
			}
			else
			{
				ignore = TRUE;
			}
		}
		else
		{
			ignore = TRUE;
		}
		break;
	}

	default:
		ignore = TRUE;
		break;
	}
	return stat;
}

OP_STATUS WindowsPluginDetector::InsertPluginPath(const OpStringC& plugin_path, BOOL* was_different, const OpStringC& checkedFileDescription, BOOL is_replacement)
{
	// check if this is a plugin for my particular CPU architecture (Win32 or x64)
	if(!IsPluginForArchitecture(plugin_path))
		return OpStatus::OK;

	WindowsCommonUtils::FileInformation fileinfo;

	// We want to ignore errors. That will ignore just this plugin and not all of them.
	if (OpStatus::IsError(fileinfo.Construct(plugin_path)))
		return OpStatus::OK;

	// Do this the American english translation be default.
	// Keep track of the string length for easy updating.
	// 040904E4 represents the language ID and the four
	// least significant digits represent the codepage for
	// which the data is formatted.  The language ID is
	// composed of two parts: the low ten bits represent
	// the major language and the high six bits represent
	// the sub language.
	fileinfo.SetLanguageId(UNI_L("040904E4"));

	UniString uni_productName;
	UniString uni_fileExtents;
	UniString uni_mimeType;
	UniString uni_fileOpenName;
	UniString uni_fileDescription;
	UniString uni_version;

	// First check. If no mime type, not a plugin.
	if (OpStatus::IsError(fileinfo.GetInfoItem(UNI_L("MIMEType"), uni_mimeType)))
		return OpStatus::OK;

	fileinfo.GetInfoItem(UNI_L("ProductName"), uni_productName);
	fileinfo.GetInfoItem(UNI_L("FileExtents"), uni_fileExtents);
	fileinfo.GetInfoItem(UNI_L("FileOpenName"), uni_fileOpenName);
	fileinfo.GetInfoItem(UNI_L("FileDescription"), uni_fileDescription);
	fileinfo.GetInfoItem(UNI_L("FileVersion"), uni_version);

	// Stupid hacks to get OpStrings from UniStrings. Should be gone once DSK-342766 is integrated
	const uni_char* ptr;
	OpString productName;
	RETURN_IF_ERROR(uni_productName.CreatePtr(&ptr, TRUE));
	RETURN_IF_ERROR(productName.Set(ptr));

	OpString fileExtents;
	RETURN_IF_ERROR(uni_fileExtents.CreatePtr(&ptr, TRUE));
	RETURN_IF_ERROR(fileExtents.Set(ptr));

	OpString mimeType;
	RETURN_IF_ERROR(uni_mimeType.CreatePtr(&ptr, TRUE));
	RETURN_IF_ERROR(mimeType.Set(ptr));

	OpString fileOpenName;
	RETURN_IF_ERROR(uni_fileOpenName.CreatePtr(&ptr, TRUE));
	RETURN_IF_ERROR(fileOpenName.Set(ptr));

	OpString fileDescription;
	RETURN_IF_ERROR(uni_fileDescription.CreatePtr(&ptr, TRUE));
	RETURN_IF_ERROR(fileDescription.Set(ptr));

	OpString version;
	RETURN_IF_ERROR(uni_version.CreatePtr(&ptr, TRUE));
	RETURN_IF_ERROR(version.Set(ptr));

	// Filter out commas from productName
	const uni_char *delChars = UNI_L(",");
	StrFilterOutChars(productName.CStr(), delChars);

	if (is_replacement)
	{
		if (checkedFileDescription.IsEmpty() || fileDescription.Compare(checkedFileDescription))
		{ // file versions differ
			RETURN_IF_ERROR(InsertPlugin(productName, mimeType, fileExtents, fileOpenName, fileDescription, plugin_path, version));
			if (was_different)
				*was_different = TRUE;
		}
	}
	else
	{
		// Check if plugin should be used, replaced and ignored, or just ignored
		BOOL ignore = FALSE;
		if (productName.HasContent() && fileDescription.HasContent())
		{
			if (!productName.Compare("Adobe Acrobat", MAXSTRLEN("Adobe Acrobat")))
			{
				if (m_acrobat_installed)
					ignore = TRUE;
				else
					RETURN_IF_ERROR(CheckReplacePlugin(ACROBAT, ignore, fileDescription));
			}
			else if (productName.Find("Java(TM)") == 0)
			{
				if (IsVersionNewer(m_latest_seen_java_version, version))
				{
					if (m_latest_seen_java_name.HasContent())
						// Remove old version.
						OpStatus::Ignore(RemovePlugin(m_latest_seen_java_name));

					OpStatus::Ignore(m_latest_seen_java_name.Set(productName));
					OpStatus::Ignore(m_latest_seen_java_version.Set(version));
				}
				else
					// Ignore this version.
					ignore = TRUE;
			}
			else if (!productName.Compare("Adobe SVG Viewer Plugin", MAXSTRLEN("Adobe SVG Viewer Plugin")))
			{
				if (m_svg_installed)
					ignore = TRUE;
				else
					RETURN_IF_ERROR(CheckReplacePlugin(SVG, ignore, fileDescription));
			}
			else if (!productName.Compare("QuickTime Plug-in", MAXSTRLEN("QuickTime Plug-in")))
			{
				if (!m_qt_installed)
					ignore = TRUE; // local installed QuickTime plug-in is ignored
			}
			// this is the old plugin, not suitable for Windows Vista/Windows 7
			else if(!productName.Compare("Windows Media Player Plug-in Dynamic Link Library", MAXSTRLEN("Windows Media Player Plug-in Dynamic Link Library")))
			{
				if(m_wmp_installed)
				{
					// we have a newer installed already
					ignore = TRUE;

					// remove this plugin from the list if the new one is already installed
					RETURN_IF_ERROR(RemovePlugin(productName));
					RETURN_IF_ERROR(RemovePlugin(UNI_L("Microsoft® DRM")));
				}
			}
			else if(!productName.Compare("Microsoft® Windows Media Player Firefox Plugin", MAXSTRLEN("Microsoft® Windows Media Player Firefox Plugin")))
			{
				if(GetWinType() < WIN2K)
				{
					ignore = TRUE;
				}
				else
				{
					m_wmp_installed = TRUE;

					// remove the old plugin from the list if the new is already installed
					RETURN_IF_ERROR(RemovePlugin(UNI_L("Windows Media Player Plug-in Dynamic Link Library")));
					RETURN_IF_ERROR(RemovePlugin(UNI_L("Microsoft® DRM")));
				}
			}
		}
		if (!ignore)
			return InsertPlugin(productName, mimeType, fileExtents, fileOpenName, fileDescription, plugin_path, version);
	}
	return OpStatus::OK;
}

OP_STATUS WindowsPluginDetector::ReadFromRegistry()
{
	BOOL dummy = FALSE;
	if (!m_acrobat_installed)
		RETURN_IF_ERROR(CheckReplacePlugin(ACROBAT, dummy));
	if (!m_svg_installed)
		RETURN_IF_ERROR(CheckReplacePlugin(SVG, dummy));
	if (!m_qt_installed)
		RETURN_IF_ERROR(CheckReplacePlugin(QT, dummy));
	if (!m_wmp_detection_done)
		RETURN_IF_ERROR(CheckReplacePlugin(WMP, dummy));

	if (!m_read_from_registry)
	{
		m_read_from_registry = TRUE;
		RETURN_IF_ERROR(ReadEntries(HKEY_LOCAL_MACHINE)); // FF installed
		RETURN_IF_ERROR(ReadEntries(HKEY_CURRENT_USER)); // FF not installed
	}

	return OpStatus::OK;
}

OP_STATUS WindowsPluginDetector::ReadEntries(const HKEY hk)
{
	OP_STATUS stat = OpStatus::OK;
	HKEY hkey_accounts = NULL;
	DWORD res = RegOpenKeyEx(hk, UNI_L("SOFTWARE\\MozillaPlugins"), 0, KEY_READ, &hkey_accounts);
	if (res == ERROR_SUCCESS)
	{
		DWORD index = 0;
		DWORD size = MAX_LEN - 1;
		uni_char key[MAX_LEN] = {0};

		while (RegEnumKeyEx(hkey_accounts, index++, key, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			HKEY hkey = NULL;
			size = MAX_LEN - 1;
			res = RegOpenKeyEx(hkey_accounts, key, 0, KEY_READ, &hkey);
			if (res == ERROR_SUCCESS)
			{
				size = MAX_LEN - 1;
				uni_char value[MAX_LEN] = {'\0'};
				if (RegQueryValueEx(hkey, UNI_L("Path"), NULL, NULL, (LPBYTE)value, &size) == ERROR_SUCCESS)
				{
					OpString newfname;
					stat = newfname.Set(value);
					if (OpStatus::IsSuccess(stat) && !g_pcapp->IsPluginToBeIgnored(newfname.CStr()))
						// Ignoring return value as failure while adding plugin should not stop enumeration
						OpStatus::Ignore(InsertPluginPath(newfname));
				}
			}
			RegCloseKey(hkey);
			// Update max size of key - RegEnumKeyEx will use it for getting next key
			size = MAX_LEN - 1;
		}
	}
	RegCloseKey(hkey_accounts);
	return stat;
}

BOOL WindowsPluginDetector::IsVersionNewer(const OpStringC& existing, const OpStringC& added)
{
	if (existing.IsEmpty())
		return added.HasContent();

	const uni_char *old_version_pos = existing.CStr();
	const uni_char *new_version_pos = added.CStr();
	const uni_char *next_dot_old, *next_dot_new;

	OpString number;

	while ((next_dot_old = uni_strpbrk(old_version_pos, UNI_L(".,"))) != 0)
	{
		if ((next_dot_new = uni_strpbrk(new_version_pos, UNI_L(".,"))) == 0)		//New version number is shorter -> older
			return FALSE;

		number.Set(old_version_pos, next_dot_old-old_version_pos);
		int old_ver = uni_atoi(number);
		number.Set(new_version_pos, next_dot_new-new_version_pos);
		int new_ver = uni_atoi(number);

		if (old_ver > new_ver)
			return FALSE;
		else if (old_ver < new_ver)
			return TRUE;

		old_version_pos = next_dot_old + 1;
		new_version_pos = next_dot_new + 1;
	}

	if ((next_dot_new = uni_strpbrk(new_version_pos, UNI_L(".,"))) != 0)		//New version number is longer -> newer
		return TRUE;

	return uni_atoi(old_version_pos) < uni_atoi(new_version_pos);
}

OP_STATUS WindowsPluginDetector::RemovePlugin(const OpStringC& productName)
{
	PluginInfo *existing_plugin;
	const uni_char *hashkey;

	hashkey = productName.CStr();

	if (OpStatus::IsSuccess(m_plugin_list.GetData(hashkey, &existing_plugin)))
	{
		m_plugin_list.Remove(hashkey, &existing_plugin);
		OP_DELETE(existing_plugin);
	}
	return OpStatus::OK;
}

OP_STATUS WindowsPluginDetector::InsertPlugin(const OpStringC& productName, const OpStringC& mimeType, const OpStringC& fileExtensions, const OpStringC& fileOpenName, const OpStringC& fileDescription, const OpStringC& plugin, const OpStringC& version)
{
	PluginInfo *existing_plugin;
	const uni_char *hashkey;
	BOOL hash_mimetype = FALSE;

	hashkey = productName.CStr() ? productName.CStr() : fileDescription.CStr() ? fileDescription.CStr() : mimeType.CStr();

	if (OpStatus::IsSuccess(m_plugin_list.GetData(hashkey, &existing_plugin)))
	{
		if (IsVersionNewer(existing_plugin->version, version))
		{
			m_plugin_list.Remove(hashkey, &existing_plugin);
			OP_DELETE(existing_plugin);
		}
		else
		{
			// allow same version, but different mime types (stupid QuickTime, see bug #335179)
			if (!existing_plugin->version.Compare(version) && existing_plugin->mime_type.Compare(mimeType))
				hash_mimetype = TRUE;
			else
				return OpStatus::OK;
		}
	}

	PluginInfo *new_plugin = OP_NEW(PluginInfo, ());

	if (!new_plugin)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status;
	if (OpStatus::IsError(status = new_plugin->product_name.Set(hashkey))
	 || OpStatus::IsError(status = new_plugin->mime_type.Set(mimeType))
	 || OpStatus::IsError(status = new_plugin->extensions.Set(fileExtensions))
	 || OpStatus::IsError(status = new_plugin->file_open_name.Set(fileOpenName))
	 || OpStatus::IsError(status = new_plugin->description.Set(fileDescription))
	 || OpStatus::IsError(status = new_plugin->plugin.Set(plugin))
	 || OpStatus::IsError(status = new_plugin->version.Set(version))
	 || OpStatus::IsError(status = m_plugin_list.Add(hash_mimetype ? new_plugin->mime_type.CStr() : new_plugin->product_name.CStr(), new_plugin)))
	{
		OP_DELETE(new_plugin);
	}
	return status;
}

OP_STATUS WindowsPluginDetector::SubmitPlugins()
{
	OP_PROFILE_METHOD("Submitted plugins to core");

	OpHashIterator* it = m_plugin_list.GetIterator();
	if (!it)
	{
		OP_ASSERT(!"SubmitPlugins(): Could not get iterator");
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = OpStatus::OK;

	for (OP_STATUS for_status = it->First(); OpStatus::IsSuccess(for_status); for_status = it->Next())
	{
		PluginInfo* plugin = (PluginInfo*)it->GetData();

		void *token = NULL;
		if (plugin->mime_type.IsEmpty())
		{
			OP_ASSERT(!"SubmitPlugins(): mimetype is empty");
			status = OpStatus::ERR;
			break;
		}
		// OnPrepareNewPlugin() return ERR if the plug-in path already exists for another plug-in already registered. We should just ignore it probably and
		// skip to the next plugin. This is perferctly normal since SubmitPlugins() is called multiple times when refreshing plugins.
		if (OpStatus::IsError(m_listener->OnPrepareNewPlugin(plugin->plugin, plugin->product_name, plugin->description, plugin->version, COMPONENT_PLUGIN, TRUE, token)))
			continue;

		const uni_char* next_mime_type_ptr = plugin->mime_type.CStr();
		const uni_char* next_mime_type_end_ptr = NULL;
		const uni_char* next_ext_ptr = plugin->extensions.CStr();
		const uni_char* next_ext_end_ptr = NULL;
		const uni_char* next_desc_ptr = plugin->file_open_name.CStr();
		const uni_char* next_desc_end_ptr = NULL;
		OpString next_mime_type;
		OpString next_ext;
		OpString next_desc;

		if (!next_ext.Reserve(5))
			return OpStatus::ERR_NO_MEMORY;

		if (!next_desc.Reserve(30))
			return OpStatus::ERR_NO_MEMORY;

		while (1)
		{
			next_mime_type_end_ptr = uni_strchr(next_mime_type_ptr, '|');
			next_mime_type.Set(next_mime_type_ptr, next_mime_type_end_ptr?next_mime_type_end_ptr-next_mime_type_ptr:KAll);
			if (next_ext_ptr)
			{
				next_ext_end_ptr = uni_strchr(next_ext_ptr, '|');
				int length = next_ext_end_ptr ? next_ext_end_ptr-next_ext_ptr : KAll;
				RETURN_IF_ERROR(next_ext.Set(next_ext_ptr, length));
			}
			else
				next_ext[0] = 0;

			if (next_desc_ptr)
			{
				next_desc_end_ptr = uni_strchr(next_desc_ptr, '|');
				int length = next_desc_end_ptr ? next_desc_end_ptr - next_desc_ptr : KAll;
				RETURN_IF_ERROR(next_desc.Set(next_desc_ptr, length));
			}
			else
				next_desc[0] = 0;

			if (next_mime_type.HasContent() && OpStatus::IsError(status = m_listener->OnAddContentType(token, next_mime_type)))
				break;

			if(next_ext.HasContent() && OpStatus::IsError(status = m_listener->OnAddExtensions(token, next_mime_type, next_ext, next_desc)))
				break;

			if (!next_mime_type_end_ptr)
				break;

			next_mime_type_ptr = next_mime_type_end_ptr + 1;
			next_ext_ptr = next_ext_end_ptr ? next_ext_end_ptr+1 : 0;
			next_desc_ptr = next_desc_end_ptr ? next_desc_end_ptr+1 : 0;
		}

		if (OpStatus::IsError(status) || OpStatus::IsError(status = m_listener->OnCommitPreparedPlugin(token)))
		{
			OP_ASSERT(!"SumbitPlugins(): Cancelling prepared plugin due to prevous errors or could not commit");
			OpStatus::Ignore(m_listener->OnCancelPreparedPlugin(token));
			break;
		}
	}
	OP_DELETE(it);

	return status;
}

#define PLUGIN_DLL_READ_BUFFER_SIZE	1024

BOOL WindowsPluginDetector::IsPluginForArchitecture(const OpStringC& plugin_path)
{
	if(plugin_path.IsEmpty())
		return FALSE;

	BOOL is_supported;
	{
		OpAutoHANDLE fhandle(CreateFile(plugin_path.CStr(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0));
		if (fhandle.get() == INVALID_HANDLE_VALUE)
			return FALSE;

		DWORD fsize = GetFileSize(fhandle, NULL);
		if (fsize == 0xFFFFFFFF || fsize < sizeof(IMAGE_DOS_HEADER))
			return FALSE;

		{
			OpAutoHANDLE mhandle(CreateFileMapping(fhandle, 0, PAGE_READONLY, 0, 0, 0));
			if (!mhandle.get())
				return FALSE;

			char *view = (char *)MapViewOfFile(mhandle, FILE_MAP_READ, 0, 0, 0);
			if (!view)
				return FALSE;

			PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(view);
			LONG nth_offset = dos_header->e_lfanew;
			PIMAGE_NT_HEADERS nt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(view + nth_offset);

			if (nth_offset + sizeof(nt_header->FileHeader.Machine) > fsize)
			{
				UnmapViewOfFile(view);
				return FALSE;
			}

			// check if we support this one
#ifdef _WIN64
			is_supported = (nt_header->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);
#else
			// this is the only type allowed, see MiVerifyImageHeader in ntoskrnl.exe
			is_supported = (nt_header->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);
#endif // _WIN64

			UnmapViewOfFile(view);
		}
	}
	return is_supported;
}

#endif // _PLUGIN_SUPPORT_ && !NS4P_COMPONENT_PLUGINS
