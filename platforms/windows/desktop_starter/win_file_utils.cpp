// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core\pch.h"

#include "platforms/windows_common/utils/OpAutoHandle.h"
#include "win_file_utils.h"

// See http://msdn.microsoft.com/msdnmag/issues/02/02/PE/default.aspx
class PEImage
{
public:
	explicit PEImage(HMODULE module) : m_module(module) {}
	explicit PEImage(const void* module) 
	{
		m_module = reinterpret_cast<HMODULE>(const_cast<void*>(module));
	}
	// Returns the IMAGE_NT_HEADER for this PE.
	PIMAGE_NT_HEADERS GetImageNTHeaders()
	{
		PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(m_module);

		return reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(dos_header) + dos_header->e_lfanew);
	}

private:
	HMODULE m_module;
};

DWORD  WinFileUtils::Read(uni_char *file, char** buffer, size_t& buf_size)
{
	HANDLE handle = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (handle == INVALID_HANDLE_VALUE)
		return GetLastError();

	DWORD size = GetFileSize(handle, NULL);
	if (size == 0xFFFFFFFF)
		return GetLastError();

	*buffer = new char[size];
	if (!*buffer)
	{
		CloseHandle(handle);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	if (!ReadFile(handle, *buffer, size, (DWORD *)&buf_size, NULL))
	{
		CloseHandle(handle);
		return GetLastError();
	}

	CloseHandle(handle);

	return 0;

}

BOOL WinFileUtils::PreLoadDll(const uni_char* dll_file, DWORD max_read)
{
	// Default to 1MB sequential reads.
	DWORD step_size = 1024*1024;
	OSVERSIONINFOA osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	BOOL version = GetVersionExA(&osvi);
	if (!version || osvi.dwMajorVersion > 5)
	{
		// On Vista+, a forced read through the dll slows warm starts. 
		// Instead, read the file contents sequentially up to a certain point 
		// forcing the OS to keep it in the file cache, significantly reducing
		// seeks when it's accessed with LoadLibrary() and page faults.
		OpAutoHANDLE handle(CreateFile(dll_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0));
		if (handle.get() == INVALID_HANDLE_VALUE)
		{
			return FALSE;
		}
		LPVOID buffer = VirtualAlloc(NULL, step_size, MEM_COMMIT, PAGE_READWRITE);
		if (buffer == NULL)
			return FALSE;

		DWORD len;
		DWORD total_read = 0;
		while(max_read ? total_read < max_read : TRUE)
		{
			if(!ReadFile(handle.get(), buffer, step_size, &len, NULL))
				break;

			if(!len)
				break;

			total_read += len;
		}
		VirtualFree(buffer, 0, MEM_RELEASE);
	}
	else if(osvi.dwMajorVersion == 5)
	{
		// XP code path where reading the dll from disk doesn't put it in the disk cache,
		// so we need to load it and touch each code page.  We're not actually executing it, 
		// so we don't need to use SafeLoadLibrary(). Worst case is that we might preload 
		// the wrong dll, with no other ill effects that slower startup.
		// DONT_RESOLVE_DLL_REFERENCES is obsolete on newer OSes, but not on XP. 
		HMODULE module = LoadLibraryExW(dll_file, NULL, DONT_RESOLVE_DLL_REFERENCES);
		if (!module)
		{
			return FALSE;
		}
		DWORD page_buffered_size = 4 * 1024;	// a page
		PEImage image(module);
		PIMAGE_NT_HEADERS nt_headers = image.GetImageNTHeaders();
		DWORD actual_max_read = max_read ? MAX(max_read, nt_headers->OptionalHeader.SizeOfImage) : nt_headers->OptionalHeader.SizeOfImage;

		// make sure the optimizer doesn't do anything funky
		volatile UINT8* touch = reinterpret_cast<UINT8 *>(module);
		volatile UINT8 unused;
		size_t offset = 0;
		while (offset < actual_max_read) 
		{
			unused = *(touch + offset);
			offset += page_buffered_size;
		}
		FreeLibrary(module);
	}
	return TRUE;
}
