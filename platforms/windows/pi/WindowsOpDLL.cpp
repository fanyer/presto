/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpDLL.h"
#include "platforms/windows/pi/WindowsOpPluginWindow.h"

#ifdef STANDALONE_BUILD // don't use memoryManager
const INT TMP_BUF_2K_LEN = UNICODE_SIZE(4096);
char tmp_buf_2k[TMP_BUF_2K_LEN + UNICODE_SIZE(1)];
#endif

#ifdef USE_OPDLL
OP_STATUS OpDLL::Create(OpDLL** new_opdll)
{
	OP_ASSERT(new_opdll != NULL);
	*new_opdll = new WindowsOpDLL();
	if (*new_opdll == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}
#endif // USE_OPDLL

WindowsOpDLL::WindowsOpDLL() :
	loaded(FALSE),
	dll(NULL)
{
}

WindowsOpDLL::~WindowsOpDLL()
{
	if (loaded)
	{
		OpStatus::Ignore(Unload());
	}
}

OP_STATUS WindowsOpDLL::Load(const uni_char* dll_name)
{
	if (loaded)
		RETURN_IF_ERROR(Unload());

	// Make a copy of the dll name
	OpString dirname;
	RETURN_IF_ERROR(dirname.Set(dll_name));

	// Strip away the file name from dll_name
	uni_char* current_dir = NULL;
	DWORD curdir_len = 0;
	int pos = dirname.FindLastOf(UNI_L('\\'));

 	if (pos != -1)
    {
		dirname.CStr()[pos] = 0;
		// Get the current working directory
#ifdef STANDALONE_BUILD
		current_dir = (uni_char*)tmp_buf_2k;
		curdir_len = GetCurrentDirectory(UNICODE_DOWNSIZE(TMP_BUF_2K_LEN), current_dir);
#else 
		current_dir = (uni_char*)g_memory_manager->GetTempBuf2k();
		curdir_len = GetCurrentDirectory(UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()), current_dir);
#endif //STANDALONE_BUILD

		// Change to the directory found in dll_name
		if (!SetCurrentDirectory(dirname.CStr()))
			return OpStatus::ERR;
	}
	else
	{
		dll = WindowsUtils::SafeLoadLibrary(dll_name);
		if (dll)
			loaded = TRUE;
		return dll ? OpStatus::OK : OpStatus::ERR;
	}

#ifndef _DEBUG
	// Do not show error messages when loading libraries.
	DWORD old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif // _DEBUG
	// Load the dll
	dll = LoadLibrary(dll_name);
#ifndef _DEBUG
	SetErrorMode(old_error_mode);
#endif

#ifndef NS4P_COMPONENT_PLUGINS
	if (dll)
		WindowsOpPluginWindow::OnLibraryLoaded(dll, dll_name);
#endif // !NS4P_COMPONENT_PLUGINS

#ifdef _DEBUG
	if (!dll)
	{
		DWORD e = GetLastError();
		OP_ASSERT(!"LoadLibrary failed! See e for error code.");
		e = 0; // Avoid warning
	}
#endif
	// Change back to the previous working directory, if we successfully retrieved one previously
	if (curdir_len)
		SetCurrentDirectory(current_dir);

	// Check to see if the dll was successfully loaded
	RETURN_VALUE_IF_NULL(dll, OpStatus::ERR);

	loaded = TRUE;

	return OpStatus::OK;
}

BOOL WindowsOpDLL::IsLoaded() const
{
	return loaded;
}

OP_STATUS WindowsOpDLL::Unload()
{
	if (loaded)
	{
		loaded = FALSE;

		// Free the dll. 0 means an error occurred.
		if (FreeLibrary(dll) == 0)
		{
			return OpStatus::ERR;
		}
		dll = NULL;
	}

	return OpStatus::OK;
}

void* WindowsOpDLL::GetSymbolAddress(const char* symbol_name) const
{
	if (loaded)
		return GetProcAddress(dll, symbol_name);
	else
		return NULL;
}
