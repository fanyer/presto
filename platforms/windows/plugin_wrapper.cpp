/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef PLUGIN_WRAPPER

#include "platforms/crashlog/crashlog.h"
#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"
#include "platforms/windows_common/pi_impl/WindowsOpPluginTranslator.h"
#include "platforms/windows/pluginwrapperhooks.h"
#include "platforms/windows/pi/WindowsOpThreadTools.h"

#include "modules/opdata/UniString.h"
#include "modules/pi/OpThreadTools.h"

CoreComponent* g_opera = NULL;
__declspec(thread) OpComponentManager* g_component_manager = NULL;
OpThreadTools* g_thread_tools = NULL;
HWND g_main_hwnd = NULL; //It has to be declarated, uses in registry.obj

HINSTANCE hInst;

UINT simple_atol(char *&str)
{
	UINT retval = 0, digit;
	while ( (digit = *str - '0') <= 9)
	{
		retval = retval*10 + digit;
		str++;
	}
	return retval;
}

char* GetNextWord(char* str)
{
	while (*str && *str != ' ')
		++str;

	while (*str && *str == ' ')
		++str;

	return str;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
//Uncomment to set up a console for the process (useful for printf debugging when no debugger is hooked up or when the debugger's output window is inadequate
/*
	AllocConsole();
	freopen("CONOUT$", "wb", stdout);
*/

	hInst = hInstance;

	g_CmdLine = lpCmdLine;
	char *crashlog_args = str_begins_with(lpCmdLine, "-write_crashlog");
	if (crashlog_args)
	{
		char filename[MAX_PATH];
		char special_char = *crashlog_args;
		if (special_char)
			crashlog_args++;

		UINT process_id = simple_atol(crashlog_args);
		WriteCrashlog(process_id, NULL, (*crashlog_args) ? GetNextWord(++crashlog_args) : NULL, filename);

		// quit this instance, which will automatically terminate the crashed instance as well
		return 0;
	}

	SetUnhandledExceptionFilter(ExceptionFilter);


	LPWSTR command_line = GetCommandLineW();
	LPWSTR* wide_argv = NULL;
	int commandline_argument_number;
	wide_argv = CommandLineToArgvW(command_line, &commandline_argument_number);
	OP_ASSERT (commandline_argument_number == __argc);


	if (commandline_argument_number < 5 || uni_strcmp(wide_argv[1], UNI_L("-newprocess")) != 0 || uni_strcmp(wide_argv[3], UNI_L("-logfolder")) != 0)
		return -1;

	g_crashlog_folder = __argv[4];

	WindowsOpComponentPlatform* platform;

	if (OpStatus::IsError(WindowsOpComponentPlatform::RegisterWNDCLASS()))
		return 0;

	UniString command_line_string;
	command_line_string.SetCopyData(wide_argv[2]);
	LocalFree(wide_argv);

	if (OpStatus::IsSuccess(WindowsOpComponentPlatform::ProcessCommandLineRequest(platform, command_line_string)))
	{
		WindowsOpPluginTranslator::RegisterWrapperWndProcClass();
		InstallPluginWrapperHooks();

		platform->Init();

		if (!MainThreadSender::Construct())
			return -1;
		RETURN_VALUE_IF_ERROR(OpThreadTools::Create(&g_thread_tools), -1);
		platform->Run();

		OP_DELETE(g_component_manager);
		OP_DELETE(platform);

		RemovePluginWrapperHooks();
		WindowsOpPluginTranslator::RemoveDummyPopupActivationWindow();
		WindowsOpPluginTranslator::UnregisterWrapperWndProcClass();
		WindowsOpComponentPlatform::UnregisterWNDCLASS();

		OP_DELETE(g_thread_tools);
		MainThreadSender::Destruct();

		if (g_crash_thread_id != 0)
		{
			// another thread crashed - suspend current thread, otherwise CRT might
			// terminate current process before crash logging subprocess attaches
			// itself as a debugger (DSK-362561)
			SuspendThread(GetCurrentThread());
		}
		return 0;
	}

	return -1;
}

#endif //PLUGIN_WRAPPER
