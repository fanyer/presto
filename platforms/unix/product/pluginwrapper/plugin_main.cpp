/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/** @brief main() and associated functions for the plugin wrapper
  */

#include "core/pch.h"

#ifdef NO_CORE_COMPONENTS

#include "platforms/posix_ipc/posix_ipc_component_platform.h"
#include "platforms/unix/product/pluginwrapper/plugin_component_platform.h"
#include "platforms/unix/product/pluginwrapper/plugin_crashlog.h"
#include "modules/pi/OpThreadTools.h"

CoreComponent* g_opera = NULL;
__thread OpComponentManager* g_component_manager = NULL;
OpThreadTools* g_thread_tools = NULL;

/* For X11 debugging, from Xlib.h */
extern int _Xdebug;

int main(int argc, char** argv)
{
	// Debugging: define OPERA_PLUGINWRAPPER_SLEEP to a number of seconds to sleep in main
	const char* want_sleep = op_getenv("OPERA_PLUGINWRAPPER_SLEEP");
	if (want_sleep)
	{
		unsigned seconds = op_strtoul(want_sleep, NULL, 10);
		fprintf(stderr, "operapluginwrapper [%d]: sleeping for %u seconds\n", getpid(), seconds);
		sleep(seconds);
	}
	if (op_getenv("OPERA_PLUGINWRAPPER_XSYNC"))
	{
		/* Make all X11 requests synchronous (slows everything down but very
		 * useful for debugging X11 error such as BadDrawable, BadValue etc. */
		_Xdebug = 1;
	}

	if (argc < 5 || op_strcmp(argv[3], "-logfolder") != 0)
		return -1; // Need a folder spec and either multiproc or crashlog arguments to start going

	const char* logfolder = argv[4];

	// Check for a crashlog argument
	if (op_strcmp(argv[1], "-crashlog") == 0)
	{
		return OpStatus::IsSuccess(PluginCrashlog::HandleCrash(op_atoi(argv[2]), logfolder)) ? 0 : -1;
	}
	else if (!op_getenv("OPERA_PLUGINWRAPPER_NOCAT"))
	{
		PluginCrashlog::InstallHandler(argv[0], logfolder);
	}

	// Now return to the main case, a multiproc argument
	if (op_strcmp(argv[1], "-multiproc") != 0)
		return -1; // Didn't get multi-process token needed to start plugin

	// Start sub-process
	PosixIpcComponentPlatform* platform;
	if (OpStatus::IsError((PosixIpcComponentPlatform::Create(platform, argv[2]))))
	{
		if (op_getenv("OPERA_PLUGINWRAPPER_CRASH_ON_ERROR"))
		{
			int* x = NULL;
			fprintf(stderr, "operapluginwrapper [%d]: Crash %d\n", getpid(), *x);
		}

		fprintf(stderr, "operapluginwrapper [%d]: Error on creation of new process component\n", getpid());
		return -1;
	}

	RETURN_VALUE_IF_ERROR(OpThreadTools::Create(&g_thread_tools), -1);

	int retval = platform->Run();

	OP_DELETE(g_thread_tools);
	OP_DELETE(platform);

	return retval;
}

PosixIpcComponentPlatform* PosixIpcComponentPlatform::CreateComponent(OpComponentType type)
{
	if (type == COMPONENT_PLUGIN) // This is the only component we know how to create
		return OP_NEW(PluginComponentPlatform, ());

	return NULL;
}

#endif // NO_CORE_COMPONENTS
