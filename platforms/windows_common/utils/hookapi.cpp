/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows_common/utils/hookapi.h"

WindowsCommonUtils::HookAPI::HookAPI(HMODULE module)
{
	m_module_ptr = (const char*)module;
	IMAGE_DOS_HEADER* dosheader = (IMAGE_DOS_HEADER*)m_module_ptr;
	m_ntheader = (IMAGE_NT_HEADERS*)RVA2Ptr(dosheader->e_lfanew);
}

const void* WindowsCommonUtils::HookAPI::RVA2Ptr(SIZE_T rva)
{
	if (m_module_ptr && rva)
		return m_module_ptr + rva;
	else
		return NULL;
}

const void* WindowsCommonUtils::HookAPI::GetDirectory(int id)
{
	return RVA2Ptr(m_ntheader->OptionalHeader.DataDirectory[id].VirtualAddress);
}

IMAGE_IMPORT_DESCRIPTOR* WindowsCommonUtils::HookAPI::GetImportDescriptor(const char* dll_name)
{
	IMAGE_IMPORT_DESCRIPTOR* descriptor = (IMAGE_IMPORT_DESCRIPTOR*)GetDirectory(IMAGE_DIRECTORY_ENTRY_IMPORT);
	if (!descriptor)
		return NULL;

	while (descriptor->FirstThunk)
	{
		if (op_stricmp(dll_name, static_cast<const char*>(RVA2Ptr(descriptor->Name))) == 0)
			return descriptor;

		descriptor++;
	}

	return NULL;
}

const void* WindowsCommonUtils::HookAPI::GetFunctionPtr(IMAGE_IMPORT_DESCRIPTOR* import_desc, const char* proc_name)
{
	IMAGE_THUNK_DATA* thunk_orig = (IMAGE_THUNK_DATA*)RVA2Ptr(import_desc->OriginalFirstThunk);
	IMAGE_THUNK_DATA* thunk_curr  = (IMAGE_THUNK_DATA*)RVA2Ptr(import_desc->FirstThunk);
	if (!thunk_orig)
		return NULL;

	while(thunk_orig->u1.Function)
	{
		IMAGE_IMPORT_BY_NAME* thunk_fn = (IMAGE_IMPORT_BY_NAME*)RVA2Ptr(thunk_orig->u1.AddressOfData);
		if (op_stricmp(proc_name, reinterpret_cast<const char*>(thunk_fn->Name)) == 0)
			return thunk_curr;

		thunk_orig++;
		thunk_curr++;
	}

	return NULL;
}

FARPROC WindowsCommonUtils::HookAPI::SetImportAddress(const char* dll_name, const char* proc_name, FARPROC new_proc)
{
	IMAGE_IMPORT_DESCRIPTOR* import_desc = GetImportDescriptor(dll_name);

	if (!import_desc)
		return NULL;

	const void* pfn = GetFunctionPtr(import_desc, proc_name);

	if (!pfn)
		return NULL;

	// read the original function address
	FARPROC oldproc = (FARPROC)pfn;
	DWORD old_protect;
	SIZE_T written;

	if (!VirtualProtectEx(GetCurrentProcess(), const_cast<void*>(pfn), sizeof(FARPROC), PAGE_EXECUTE_READWRITE, &old_protect))
		return NULL;

	// overwrite with new function address
	WINBOOL success = WriteProcessMemory(GetCurrentProcess(), const_cast<void*>(pfn), &new_proc, sizeof(FARPROC), &written);

	VirtualProtectEx(GetCurrentProcess(), const_cast<void*>(pfn), sizeof(FARPROC), old_protect, &old_protect);

	return success ? oldproc : NULL;
}
