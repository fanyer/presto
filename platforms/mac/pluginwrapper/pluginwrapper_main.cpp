/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 ** Dummy definitions of requisite plug-in wrapper symbols.
 */

#include "core/pch.h"
#include "platforms/posix_ipc/posix_ipc_component_platform.h"
#include "platforms/crashlog/crashlog.h"
#include "platforms/mac/pluginwrapper/plugin_crashlog.h"
#include "platforms/mac/pluginwrapper/PluginWrapperNSApplicationDelegate.h"
#include "platforms/mac/pluginwrapper/PluginWrapperNSCursorSwizzle.h"
#include "modules/pi/OpThreadTools.h"

#ifdef NO_CORE_COMPONENTS
CoreComponent* g_opera;
OpThreadTools* g_thread_tools = NULL;

extern "C"

int main(int argc, char** argv)
{
	int retval = 0;
	
	// DO NOT place any initialisation code here. If you want to do something
	// place it in the MacComponentPlatform::Run() function in the place
	// shown

	
	const char* logfolder = argv[4];

#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "pluginwrapper [crashlog debug %d]: in main\n", time(NULL));
#endif
	
	// Check for a crashlog argument
	if (op_strcmp(argv[1], "-write_crashlog") == 0)
	{
#ifdef CRASHLOG_DEBUG
	fprintf(stderr, "pluginwrapper [crashlog debug %d]: handling crash: %s\n", time(NULL), logfolder);
#endif
		return OpStatus::IsSuccess(PluginCrashlog::HandleCrash(op_atoi(argv[2]), logfolder)) ? 0 : -1;
	}
	else
	{
		PluginCrashlog::InstallHandler(logfolder);
	}

	// Run the main pluginwrapper
	if (argc >= 3 && !strncmp(argv[1], "-multiproc", 10))
	{
		// Initialize TLS
		if (MacTLS::Init() != 0)
			return -1;

		// Start sub-process
		PosixIpcComponentPlatform* platform;
		if (OpStatus::IsError((PosixIpcComponentPlatform::Create(platform, argv[2]))))
			return -1;
		PluginApplicationDelegate *plugin_application_delegate = OP_NEW(PluginApplicationDelegate, ());
		HijackNSCursor();
		retval = platform->Run();
		OP_DELETE(plugin_application_delegate);
		OP_DELETE(platform);
	}

	// Nothing will be called after the "Run" call above, since we are using 
	// "terminate" to shutdown the PluginWrapper inside of MacComponentPlatform::Exit(), 
	// so DO NOT place any code below it and expect it to be called EVER!

	return retval;
}

#endif // NO_CORE_COMPONENTS

