/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _CRASHLOG_H_
#define _CRASHLOG_H_

struct GpuInfo;

extern "C"
{
#ifdef MSWIN
	extern LPSTR g_CmdLine;
	extern LPSTR g_crashlog_folder;
	extern GpuInfo g_gpu_info;
	extern volatile LONG g_crash_thread_id;

	UINT __stdcall WriteCrashlog(DWORD process_id, GpuInfo* gpu_info, char* log_folder, char* log_file, const char* location = NULL);
	LONG WINAPI ExceptionFilter(struct _EXCEPTION_POINTERS *);
	char * __stdcall str_begins_with(char *str1, char *str2);

	BOOL __stdcall InstallMemGuardHooks(HINSTANCE opera_dll);
	void * __stdcall InitializeMemGuard(char *mg_ini);
	void DestroyMemGuard();
#else
	extern 	UINT g_wait_for_debugger;
	extern char** g_crash_recovery_parameters;

	void WriteCrashlog(pid_t pid, GpuInfo* gpu_info, char* log_filename, int log_filename_size, const char* location = NULL);
	void InstallCrashSignalHandler();
	void RemoveCrashSignalHandler();
#endif
}

#endif // _CRASHLOG_H_
