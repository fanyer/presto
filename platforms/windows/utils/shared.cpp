/**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/desktop_starter/WindowsAUFileUtils.h"

#include <Psapi.h>

HMODULE WindowsUtils::SafeLoadLibrary(const wchar_t* dll_name)
{
#ifdef _DEBUG
	if (uni_strchr(dll_name, '\\'))
	{
		assert(!"Don't call SafeLoadLibrary with a path");
		return NULL;
	}
#endif

	HMODULE ret = NULL;
	uni_char* buf = new uni_char[_MAX_PATH+1];
	if (!buf)
		return NULL;

	// Always look in the Opera directory first (so we don't load a different version of the DLL
	// by accident if someone has installed it system wide). Then search the system directory, if wanted.
	do 
	{
		int sysdir_len = 0, path_len = _MAX_PATH - uni_strlen(dll_name) - 1;

		// Search Opera directory
		sysdir_len = GetModuleFileName(NULL, buf, path_len);
		if (sysdir_len == 0 || sysdir_len >= path_len)
			break;

		uni_char* pos = uni_strrchr(buf, '\\');
		if (!pos)
			break;

		uni_strcpy(pos+1, dll_name);

		ret = LoadLibrary(buf);
		if (ret)
			break;

		// Search System directory
		sysdir_len = GetSystemDirectory(buf, path_len);
		if (sysdir_len == 0 || sysdir_len >= path_len)
			break;

		buf[sysdir_len] = '\\';
		uni_strcpy(buf+sysdir_len+1, dll_name);

		ret = LoadLibrary(buf);
		if (ret)
			break;

	} while (false);

	delete[] buf;
	return ret;
}

void WindowsUtils::ShowError(char *message)
{
	DWORD last_err = GetLastError();
	if (!last_err)
		return;

	LPSTR lpMsgBuf;
	DWORD messSize = FormatMessageA( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		last_err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPSTR) &lpMsgBuf,
		0,
		NULL 
	);

	char* message_buf = new char[messSize+80];
	if (message_buf)
		sprintf(message_buf, message, lpMsgBuf);	// we don't have a language manager here

	MessageBoxA(NULL, message_buf ? message_buf : lpMsgBuf, "Opera Error", MB_OK | MB_ICONERROR);
	LocalFree(lpMsgBuf);
	delete [] message_buf;
}

#if !defined PLUGIN_WRAPPER && !defined OPERA_MAPI
DWORD FindProcess(const char *process_name, DWORD current_process);

DWORD WindowsUtils::GetOperaProcess(bool look_for_upgrader)
{
	// enumerate all processes to find the other opera process we're interested in
	// Get the list of process identifiers.

	DWORD current_process = GetCurrentProcessId();
	HANDLE current_process_handle = GetCurrentProcess();
	DWORD cbNeeded;
	char thisProcessName[MAX_PATH];
	HMODULE hMod;
	BOOL found_process_name = FALSE;

	// If we are looking for the OperaUpgrader.exe process, then we need to find its full path
	if (look_for_upgrader)
	{
		uni_char* wide_upgrader_name = NULL;

		AUFileUtils* file_utils = AUFileUtils::Create();
		if (file_utils && file_utils->GetUpdatePath(&wide_upgrader_name) == AUFileUtils::OK)
		{
			if (WideCharToMultiByte(CP_ACP, 0, wide_upgrader_name, -1, thisProcessName, MAX_PATH, NULL, NULL))
				found_process_name = TRUE;

			// Don't use OP_DELETE() on objects created with "new".
			delete [] wide_upgrader_name;
		}

		// Don't use OP_DELETE() on objects created with "new".
		delete file_utils;
	}
	else
	{
		if (EnumProcessModules(current_process_handle, &hMod, sizeof(hMod), &cbNeeded) &&
			GetModuleFileNameA(hMod, thisProcessName, sizeof(thisProcessName)))
		{
			found_process_name = TRUE;
		}
	}

	if (found_process_name)
		return FindProcess(thisProcessName, current_process);

	return 0;
}


DWORD FindProcess(const char *process_name, DWORD current_process)
{
	DWORD aProcesses[1024], cbNeeded, cProcesses, process = 0;
	char otherProcessName[MAX_PATH];
	HMODULE hMod;

	if (!EnumProcesses(aProcesses, 1024 * sizeof(DWORD), &cbNeeded ))
		return 0;
	// Calculate how many process identifiers were returned.
	cProcesses = cbNeeded / sizeof(DWORD);

	// Print the name and process identifier for each process.
	for (unsigned int i = 0; i < cProcesses; i++)
	{
		if (aProcesses[i] != 0 && aProcesses[i] != current_process)
		{
			HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);

			if (!hProcess)
				continue;

			// Get the process name.
			if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded) &&
				GetModuleFileNameExA(hProcess, hMod, otherProcessName, sizeof(otherProcessName)) &&
				!stricmp(process_name, otherProcessName))
			{
				// Yes, we are already running.
				process = aProcesses[i];
				CloseHandle(hProcess);
				break;
			}
			CloseHandle(hProcess);
		}
	}
	return process;
}

#endif //!defined PLUGIN_WRAPPER && !defined OPERA_MAPI
