/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "platforms/windows_common/pi_impl/WindowsOpPluginLibrary.h"
#include "platforms/windows_common/pi_impl/WindowsOpPluginTranslator.h"
#include "platforms/windows_common/utils/fileinfo.h"
#include "platforms/windows_common/utils/hookapi.h"

/*static*/
OP_STATUS OpPluginLibrary::EnumerateLibraries(OtlList<LibraryPath>* out_library_paths, const UniString& suggested_paths)
{
	return WindowsOpPluginLibrary::EnumerateLibraries(out_library_paths, suggested_paths);
}

/*static*/
OP_STATUS OpPluginLibrary::Create(OpPluginLibrary** out_library, const UniString& path)
{
	return WindowsOpPluginLibrary::Create(out_library, path);
}

/*static*/
OP_STATUS WindowsOpPluginLibrary::Create(OpPluginLibrary** out_library, const UniString& path)
{
	OpAutoPtr<WindowsOpPluginLibrary> lib(OP_NEW(WindowsOpPluginLibrary, ()));
	RETURN_OOM_IF_NULL(lib.get());

	RETURN_IF_ERROR(lib->Init(path));

	*out_library = lib.release();

	return OpStatus::OK;
}

/*static*/
OP_STATUS WindowsOpPluginLibrary::EnumerateLibraries(OtlList<LibraryPath>* out_library_paths, const UniString& suggested_paths)
{
	OpAutoPtr<OtlCountedList<UniString>> dirpaths(suggested_paths.Split(';'));
	RETURN_OOM_IF_NULL(dirpaths.get());

	out_library_paths->Clear();

	/* Lookup QuickTime plugin. */
	static const uni_char quicktime_key[] = UNI_L("SOFTWARE\\Apple Computer, Inc.\\QuickTime");
	UniString quicktime_folder;

	if (OpStatus::IsSuccess(ReadRegValue(HKEY_LOCAL_MACHINE, quicktime_key, UNI_L("InstallDir"), quicktime_folder))
#ifdef SIXTY_FOUR_BIT
		|| OpStatus::IsSuccess(ReadRegValue(HKEY_LOCAL_MACHINE, quicktime_key, UNI_L("InstallDir"), quicktime_folder, KEY_WOW64_32KEY))
#endif //SIXTY_FOUR_BIT
		)
	{
		OP_STATUS s;
		if (quicktime_folder[quicktime_folder.Length() - 1] != PATHSEPCHAR)
			s = quicktime_folder.AppendCopyData(UNI_L(PATHSEP) UNI_L("Plugins"));
		else
			s = quicktime_folder.AppendCopyData(UNI_L("Plugins"));

		if (OpStatus::IsSuccess(s))
			OpStatus::Ignore(dirpaths->Append(quicktime_folder));
	}

	/* Lookup new Window Media Player plugin. */
	do
	{
		UniString path_string;
		bool exists = false;
		DWORD drives = GetLogicalDrives();

		if (drives == 0)
			break;

		uni_char drive_path[] = UNI_L("*:\\");
		for (DWORD drive = 0; drive < 26; drive++)
		{
			if (drives & (1 << drive))
			{
				drive_path[0] = 'A' + drive;
				if (GetDriveType(drive_path) == DRIVE_FIXED)
				{
					if (OpStatus::IsSuccess(path_string.SetCopyData(drive_path))
						&& OpStatus::IsSuccess(path_string.AppendCopyData(UNI_L("PFiles") UNI_L(PATHSEP) UNI_L("plugins") UNI_L(PATHSEP)))
						&& GetFileAttributes(path_string.Data(true)) != INVALID_FILE_ATTRIBUTES)
					{
						exists = true;
						break;
					}
				}
			}
		}

		if (exists)
			OpStatus::Ignore(dirpaths->Append(path_string));
	}
	while(false);

	/* Lookup plugin libraries in all known directory paths. */
	for (OtlCountedList<UniString>::ConstIterator it = dirpaths->Begin();
		 it != dirpaths->End(); ++it)
	{
		UniString path_with_slash(*it);
		if (path_with_slash[path_with_slash.Length() - 1] != PATHSEPCHAR)
			if (OpStatus::IsError(path_with_slash.Append(PATHSEPCHAR)))
				continue;

		UniString pattern(path_with_slash);
		if (OpStatus::IsError(pattern.AppendCopyData(UNI_L("np*.dll"))))
			continue;

		WIN32_FIND_DATA find_data;
		HANDLE find = FindFirstFile(pattern.Data(true), &find_data);
		if (find == INVALID_HANDLE_VALUE)
			continue;

		UniString plugin_path(path_with_slash);
		if (OpStatus::IsSuccess(plugin_path.AppendCopyData(find_data.cFileName)))
			OpStatus::Ignore(AddPluginToList(out_library_paths, plugin_path));

		while (FindNextFile(find, &find_data))
		{
			plugin_path = path_with_slash;
			if (OpStatus::IsSuccess(plugin_path.AppendCopyData(find_data.cFileName)))
				OpStatus::Ignore(AddPluginToList(out_library_paths, plugin_path));
		}

		FindClose(find);
	}

	/* Lookup Acrobat Reader plugin. */
	UniString result;
	static const uni_char acro_rd_key[] = UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AcroRd32.exe");
	static const uni_char alt_acro_rd_key[]  = UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Acrobat.exe");

	if (OpStatus::IsSuccess(ReadRegValue(HKEY_LOCAL_MACHINE, acro_rd_key, UNI_L("Path"), result)) ||
		OpStatus::IsSuccess(ReadRegValue(HKEY_LOCAL_MACHINE, alt_acro_rd_key, UNI_L("Path"), result)))
	{
		OP_STATUS s;
		if (result[result.Length() - 1] != PATHSEPCHAR)
			s = result.AppendCopyData(UNI_L(PATHSEP) UNI_L("Browser") UNI_L(PATHSEP) UNI_L("nppdf32.dll"));
		else
			s = result.AppendCopyData(UNI_L("Browser") UNI_L(PATHSEP) UNI_L("nppdf32.dll"));

		if (OpStatus::IsSuccess(s))
			OpStatus::Ignore(AddPluginToList(out_library_paths, result));
	}

#ifdef SIXTY_FOUR_BIT
	/* Lookup 32-bit Acrobat Reader plugin. */
	if (OpStatus::IsSuccess(ReadRegValue(HKEY_LOCAL_MACHINE, acro_rd_key, UNI_L("Path"), result, KEY_WOW64_32KEY)) ||
		OpStatus::IsSuccess(ReadRegValue(HKEY_LOCAL_MACHINE, alt_acro_rd_key, UNI_L("Path"), result, KEY_WOW64_32KEY)))
	{
		OP_STATUS s;
		if (result[result.Length() - 1] != PATHSEPCHAR)
			s = result.AppendCopyData(UNI_L(PATHSEP) UNI_L("Browser") UNI_L(PATHSEP) UNI_L("nppdf32.dll"));
		else
			s = result.AppendCopyData(UNI_L("Browser") UNI_L(PATHSEP) UNI_L("nppdf32.dll"));

		OpStatus::Ignore(AddPluginToList(out_library_paths, result));
	}
#endif //SIXTY_FOUR_BIT

	/* Lookup plugins registered in the registry. */
	AddMozillaPlugins(HKEY_LOCAL_MACHINE, out_library_paths);
	AddMozillaPlugins(HKEY_CURRENT_USER, out_library_paths);

#ifdef SIXTY_FOUR_BIT
	AddMozillaPlugins(HKEY_LOCAL_MACHINE, out_library_paths, KEY_WOW64_32KEY);
	AddMozillaPlugins(HKEY_CURRENT_USER, out_library_paths, KEY_WOW64_32KEY);
#endif //SIXTY_FOUR_BIT

	return OpStatus::OK;
}

/*static*/
OP_STATUS WindowsOpPluginLibrary::AddMozillaPlugins(HKEY root_key, OtlList<LibraryPath>* out_library_paths, REGSAM view_flags)
{
	OpAutoHKEY key;
	if (RegOpenKeyEx(root_key, UNI_L("SOFTWARE\\MozillaPlugins"), 0, KEY_READ | view_flags, &key) != ERROR_SUCCESS)
		return OpStatus::ERR;

	DWORD max_subkey_len;

	if (RegQueryInfoKey(key, NULL, NULL, NULL, NULL, &max_subkey_len, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
		return OpStatus::ERR;

	// Make room for terminating null.
	max_subkey_len++;
	OpAutoArray<uni_char> subkey_name(OP_NEWA(uni_char, max_subkey_len));
	RETURN_OOM_IF_NULL(subkey_name.get());
	DWORD index = 0;
	DWORD subkey_name_len = max_subkey_len;

	while (RegEnumKeyEx(key, index++, subkey_name.get(), &subkey_name_len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
	{
		subkey_name_len = max_subkey_len;
		UniString result;
		if (OpStatus::IsError(ReadRegValue(key, subkey_name.get(), UNI_L("Path"), result)))
			continue;

		// Don't break the loop on errors, need to go through all of them.
		OpStatus::Ignore(AddPluginToList(out_library_paths, result));
	}

	return OpStatus::OK;
}

/*static*/
OP_STATUS WindowsOpPluginLibrary::ReadRegValue(HKEY key, const uni_char* subkey_name, const uni_char* value_name, UniString &result, REGSAM view_flags)
{
	OpAutoHKEY subkey;
	if (RegOpenKeyEx(key, subkey_name, 0, KEY_READ | view_flags, &subkey) != ERROR_SUCCESS)
		return OpStatus::ERR;

	DWORD value_len;

	if (RegQueryValueEx(subkey, value_name, NULL, NULL, NULL, &value_len) != ERROR_SUCCESS)
		return OpStatus::ERR;

	OpAutoArray<uni_char> value(OP_NEWA(uni_char, value_len));
	RETURN_OOM_IF_NULL(value.get());

	if (RegQueryValueEx(subkey, value_name, NULL, NULL, (LPBYTE)value.get(), &value_len) != ERROR_SUCCESS)
		return OpStatus::ERR;

	RETURN_IF_ERROR(result.SetCopyData(value.get()));

	return OpStatus::OK;
}

/*static*/
WindowsOpPluginLibrary::PluginArch WindowsOpPluginLibrary::GetPluginArchitecture(const uni_char* plugin_path)
{
	if (!plugin_path || *plugin_path == 0)
		return ARCH_UNSUPPORTED;

	PluginArch arch = ARCH_UNSUPPORTED;

	do
	{
		OpAutoHANDLE fhandle(CreateFile(plugin_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0));
		if (fhandle.get() == INVALID_HANDLE_VALUE)
			break;

		DWORD fsize = GetFileSize(fhandle, NULL);
		if (fsize == INVALID_FILE_SIZE || fsize < sizeof(IMAGE_DOS_HEADER))
			break;

		OpAutoHANDLE mhandle(CreateFileMapping(fhandle, 0, PAGE_READONLY, 0, 0, 0));
		if (!mhandle.get())
			break;

		char *view = static_cast<char*>(MapViewOfFile(mhandle, FILE_MAP_READ, 0, 0, 0));
		if (!view)
			break;

		PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(view);
		LONG nth_offset = dos_header->e_lfanew;
		PIMAGE_NT_HEADERS nt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(view + nth_offset);

		if (nth_offset + sizeof(nt_header->FileHeader.Machine) > fsize)
		{
			UnmapViewOfFile(view);
			break;
		}

#ifdef SIXTY_FOUR_BIT
		if (nt_header->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
			arch = ARCH_64;
		else if (nt_header->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
			arch = ARCH_32;
#else
		// this is the only type allowed, see MiVerifyImageHeader in ntoskrnl.exe
		if (nt_header->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
			arch = ARCH_32;
#endif // !SIXTY_FOUR_BIT

		UnmapViewOfFile(view);
	}
	while (false);

	return arch;
}

/*static*/
OP_STATUS WindowsOpPluginLibrary::AddPluginToList(OtlList<LibraryPath>* out_library_paths, UniString& library_path)
{
	// Don't accept plugins with relative paths (avoid LoadLibrary security issue).
	if (library_path.Length() < 3 || !op_isalpha(library_path[0])
		|| library_path[1] != ':' || library_path[2] != PATHSEPCHAR)
		return OpStatus::ERR;

	PluginArch plugin_arch = GetPluginArchitecture(library_path.Data(true));
	if (plugin_arch == ARCH_UNSUPPORTED)
		return OpStatus::ERR;

	LibraryPath library;
	library.path = library_path;
#ifdef SIXTY_FOUR_BIT
	library.type = plugin_arch == ARCH_64 ? COMPONENT_PLUGIN : COMPONENT_PLUGIN_WIN32;
#else // !SIXTY_FOUR_BIT
	library.type = COMPONENT_PLUGIN;
#endif // !SIXTY_FOUR_BIT
	RETURN_IF_ERROR(out_library_paths->Append(library));

	return OpStatus::OK;
}

WindowsOpPluginLibrary::~WindowsOpPluginLibrary()
{
	FreeLibrary(m_dll);

	CoUninitialize();

	m_content_types.DeleteAll();
}

OP_STATUS WindowsOpPluginLibrary::GetContentTypes(OtlList<UniString>* out_content_types)
{
	out_content_types->Clear();

	OpAutoPtr<OpHashIterator> it(m_content_types.GetIterator());
	RETURN_OOM_IF_NULL(it.get());

	for (OP_STATUS stat = it->First(); stat == OpStatus::OK; stat = it->Next())
		RETURN_IF_ERROR(out_content_types->Append(static_cast<ContentType*>(it->GetData())->content_type));

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginLibrary::GetContentTypeDescription(UniString* out_content_type_description, const UniString& content_type)
{
	out_content_type_description->Clear();

	const uni_char* key = content_type.Data(true);
	RETURN_OOM_IF_NULL(key);

	ContentType* type;
	RETURN_IF_ERROR(m_content_types.GetData(key, &type));
	RETURN_IF_ERROR(out_content_type_description->Append(type->description));

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginLibrary::GetExtensions(OtlList<UniString>* out_extensions, const UniString& content_type)
{
	out_extensions->Clear();

	const uni_char* key = content_type.Data(true);
	RETURN_OOM_IF_NULL(key);

	ContentType* type;
	RETURN_IF_ERROR(m_content_types.GetData(key, &type));

	for (OtlList<UniString>::ConstIterator it = type->extensions.Begin();
		 it != type->extensions.End(); ++it)
		RETURN_IF_ERROR(out_extensions->Append(*it));

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginLibrary::Load()
{
	// Determine the length of the path of the current working directory.
	DWORD cur_working_dir_len = GetCurrentDirectory(0, NULL);

	OpAutoArray<uni_char> cur_working_dir(OP_NEWA(uni_char, cur_working_dir_len + 1));
	RETURN_OOM_IF_NULL(cur_working_dir.get());

	if (GetCurrentDirectory(cur_working_dir_len + 1, cur_working_dir.get()) == 0)
		return OpStatus::ERR;

	// Find position of the last backslash.
	size_t cur_slash_idx = 0;
	size_t last_slash_idx = OpDataNotFound;

	while ((cur_slash_idx = m_path.FindFirst(PATHSEPCHAR, cur_slash_idx)) != OpDataNotFound)
	{
		// Advance the pointer to the next character.
		cur_slash_idx++;
		// Store current pointer.
		last_slash_idx = cur_slash_idx;
	}

	// Did not find any backslash.
	if (last_slash_idx == OpDataNotFound)
		return OpStatus::ERR;

	// Strip away file name from the path.
	UniString new_working_dir(m_path, 0, last_slash_idx);

	// Set new working directory.
	if (!SetCurrentDirectory(new_working_dir.Data(true)))
		return OpStatus::ERR;

#ifndef _DEBUG
	// Do not show error messages when loading libraries.
	DWORD old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif // _DEBUG

	// Initialize the COM library. Some plugins forget to do this.
	CoInitialize(NULL);

	// Load the library.
	RETURN_OOM_IF_NULL(m_dll = LoadLibrary(m_path.Data(true)));

#ifndef _DEBUG
	SetErrorMode(old_error_mode);
#endif

	WindowsCommonUtils::HookAPI hook(m_dll);
	if (m_name.StartsWith(UNI_L("Shockwave Flash")))
	{
		/* On right clicking windowless Flash in opaque mode to open context menu,
		   it calls TrackPopupMenu with a handle to the browser window. That fails
		   because browser window is in a different thread (and process). We need to
		   create dummy window that we will pass handle to in hooked method when
		   thread of the plugin is different from the thread of the original handle.
		   Also we have to handle WM_ENTERMENULOOP message and related to be able to
		   reset InputManager state on the browser side or else mouse gestures will
		   trigger after dismissing the context menu. */
		hook.SetImportAddress("user32.dll", "TrackPopupMenu", (FARPROC)WindowsOpPluginTranslator::TrackPopupMenuHook);

		/* Workaround for Flash internal settings dialog not working
		   due to security checks that are made against client rect instead of
		   window rect. See DSK-327707. */
		hook.SetImportAddress("user32.dll", "GetWindowInfo", (FARPROC)WindowsOpPluginTranslator::GetWindowInfoHook);

		/* Faulty check in Flash makes it think that Opera is focused when both
		   GetFocus() and GetTopWindow() return NULL, causing security issue.
		   We'll force garbage value when plugin asks for the focus when no
		   instance has focus. DSK-351604.
		   If hooking fails due to plugin not importing the API, we get along
		   with that. We've done our best to minimize the issue. Blocking the
		   plugin in that case would be too harsh. */
		hook.SetImportAddress("user32.dll", "GetFocus", (FARPROC)WindowsOpPluginTranslator::GetFocusHook);
	}

	// Revert back to the previous working directory.
	SetCurrentDirectory(cur_working_dir.get());

	return OpStatus::OK;
}

NPError WindowsOpPluginLibrary::Initialize(PluginFunctions plugin_funcs, const BrowserFunctions browser_funcs)
{
	if ((m_functions.NP_GetEntryPoints = (NP_GetEntryPoints_p)GetProcAddress(m_dll, "NP_GetEntryPoints")) == NULL ||
		(m_functions.NP_Initialize = (NP_Initialize_p)GetProcAddress(m_dll, "NP_Initialize")) == NULL ||
		(m_functions.NP_Shutdown = (NP_Shutdown_p)GetProcAddress(m_dll, "NP_Shutdown")) == NULL)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	NPError err = m_functions.NP_GetEntryPoints(plugin_funcs);
	if (err != NPERR_NO_ERROR)
		return err;

	return m_functions.NP_Initialize(browser_funcs);
}

void WindowsOpPluginLibrary::Shutdown()
{
	m_functions.NP_Shutdown();
}

OP_STATUS WindowsOpPluginLibrary::Init(const UniString& path)
{
	m_path = path;

	WindowsCommonUtils::FileInformation fileinfo;
	RETURN_IF_ERROR(fileinfo.Construct(path.Data(true)));

	// We only want the American English version of the file informations.
	fileinfo.SetLanguageId(UNI_L("040904E4"));

	UniString extensions;
	UniString mime_types;
	UniString extension_descriptions;

	// First check. If no mime type, not a plugin.
	RETURN_IF_ERROR(fileinfo.GetInfoItem(UNI_L("MIMEType"), mime_types));

	fileinfo.GetInfoItem(UNI_L("ProductName"), m_name);
	fileinfo.GetInfoItem(UNI_L("FileDescription"), m_description);
	fileinfo.GetInfoItem(UNI_L("FileVersion"), m_version);
	fileinfo.GetInfoItem(UNI_L("FileExtents"), extensions);
	fileinfo.GetInfoItem(UNI_L("FileOpenName"), extension_descriptions);

	// Parse mime-types, extensions and description into a struct and store in the hash table.
	RETURN_IF_ERROR(ParseContentType(mime_types, extensions, extension_descriptions));

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginLibrary::ParseContentType(UniString& mime_types, UniString& extensions, UniString& extension_descriptions)
{
	OpAutoPtr<OtlCountedList<UniString>> split_mime_types(mime_types.Split('|'));
	RETURN_OOM_IF_NULL(split_mime_types.get());
	OpAutoPtr<OtlCountedList<UniString>> split_extensions(extensions.Split('|'));
	RETURN_OOM_IF_NULL(split_extensions.get());
	OpAutoPtr<OtlCountedList<UniString>> split_descriptions(extension_descriptions.Split('|'));
	RETURN_OOM_IF_NULL(split_descriptions.get());

	for (OtlCountedList<UniString>::ConstIterator it = split_mime_types->Begin();
		 it != split_mime_types->End(); ++it)
	{
		const UniString& elem = *it;

		// Simple validation of the mime type - must not be empty and contain exactly one slash.
		if (elem.IsEmpty() || elem.FindFirst('/') == OpDataNotFound
			|| elem.FindFirst('/') != elem.FindLast('/'))
		{
			// If skipping mime-type, skip other lists too to keep them aligned.
			if (split_extensions->Count())
				split_extensions->PopFirst();

			if (split_descriptions->Count())
				split_descriptions->PopFirst();

			continue;
		}

		OpAutoPtr<ContentType> content(OP_NEW(ContentType, ()));
		RETURN_OOM_IF_NULL(content.get());

		// Get mime-type.
		RETURN_IF_ERROR(content->content_type.Append(elem));

		// Get mime-type description.
		if (split_descriptions->Count())
			RETURN_IF_ERROR(content->description.Append(split_descriptions->PopFirst()));

		// Get mime-type extensions.
		if (split_extensions->Count())
		{
			/* String can contain either a single extension or a list of comma
			   separated extensions. */
			OpAutoPtr<OtlCountedList<UniString>> split_exts(split_extensions->PopFirst().Split(','));
			RETURN_OOM_IF_NULL(split_exts.get());

			for (OtlCountedList<UniString>::ConstIterator it = split_exts->Begin();
				 it != split_exts->End(); ++it)
				if (!it->IsEmpty())
					RETURN_IF_ERROR(content->extensions.Append(*it));
		}

		const uni_char* key = content->content_type.Data(true);
		RETURN_OOM_IF_NULL(key);

		// Bail out on memory error but not on a duplicate key.
		RETURN_IF_MEMORY_ERROR(m_content_types.Add(key, content.get()));
		content.release();
	}

	return OpStatus::OK;
}

#endif // NS4P_COMPONENT_PLUGINS
