/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "platforms/posix_ipc/posix_ipc_component_platform.h"
#include "platforms/posix_ipc/posix_ipc_process_manager.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"

#include "platforms/unix/product/pluginwrapper/plugin_component_platform.h"

#ifdef SUPPORT_PROBETOOLS
#include "modules/probetools/probepoints.h"
#endif

#ifdef HAVE_DL_MALLOC
#include "modules/lea_malloc/lea_malloc.h"
#endif

#ifdef POSIX_GLOBAL_ASSERT_EAR
#include "platforms/posix/posix_logger.h"
#endif

#include "modules/pi/OpThreadTools.h"

#ifdef __GNUC__
extern "C" void __cxa_finalize(void *dso);
#endif

#ifdef CRASHLOG_CRASHLOGGER
#include "platforms/crashlog/crashlog.h"
#endif

extern int main_contentL(int argc, char *argv[]);

__thread OpComponentManager* g_component_manager = NULL;
__thread CleanupItem* g_cleanup_stack = NULL;
CoreComponent* g_opera = NULL;
OpThreadTools* g_thread_tools = NULL;

/* Parses a hexadecimal representation of a number with a '0x' prefix
 * as a pointer.  As a special case, "0" is accepted as NULL.
 *
 * Asserts and returns NULL if the string is not on the expected form.
 * (The assertions are here because ordinarily the input should come
 * from our crashlogging code.  And we really want to be sure it gets
 * it right.)
 */
#ifndef SELFTEST
static
#endif
void * parse_pointer(const char * s)
{
	/* SELFTESTS EXIST: In
	 * platforms/unix/selftests/simple_functions.ot, there are some
	 * automatic tests for this function.  The error tests are
	 * disabled by default, since they raise assertions (which is a
	 * Bad Thing for automatic tests).  But they are easy to enable if
	 * you want to test that too (see the source).  Run with:
	 *  --test="unix simple functions:parse_pointer*"
	 */
	/* Can't use strtol() as it will fail for pointers with the high
	 * bit set if sizeof(long) == sizeof(void*).
	 *
	 * strtoul() also has problems, since it silently converts
	 * negative numbers to positive, and is locale-dependent.  But at
	 * least it reports overflows.
	 */
	OP_ASSERT(sizeof(UINTPTR) <= sizeof(unsigned long));
	if (!s || !*s)
		return NULL;
	// special case: 0 does not need 0x prefix
	if (s[0] == '0' && s[1] == 0)
		return NULL;
	/* verify the parts of the format that strtoul() potentially does
	 * not check.
	 */
	if (s[0] != '0' || s[1] != 'x')
	{
		OP_ASSERT(!"Pointer strings should start with '0x'");
		return NULL;
	}
	for (const char * ch = s+2; *ch; ch++)
	{
		if ((*ch < '0' || *ch > '9') &&
			(*ch < 'a' || *ch > 'f') &&
			(*ch < 'A' || *ch > 'F'))
		{
			OP_ASSERT(!"Unexpected character in pointer string");
			return NULL;
		}
	}
	// So far, so good.  Convert.
	char * endp;
	errno = 0;
	UINTPTR p = strtoul(s, &endp, 16);
	OP_ASSERT(errno == 0);
	if (errno != 0)
		return NULL;
	OP_ASSERT(!*endp);
	if (*endp)
		return NULL;
	return reinterpret_cast<void*>(p);
}

int
main(int argc, char* argv[])
{
#ifdef CRASHLOG_CRASHLOGGER
	const char* crashlog_location = NULL;
	char* crashlog_filename = NULL;

	if (argc >= 5 && op_strcmp(argv[3], "-logfolder") == 0)
		crashlog_location = argv[4];

	if (argc >= 3 && op_strcmp(argv[1], "-crashlog") == 0)
	{
		crashlog_filename = OP_NEWA(char, MAX_PATH);
		long pid = atol(argv[2]);

		GpuInfo * gpu_info = NULL;
		if (argc >= 7 && op_strcmp(argv[5], "-crashlog-gpuinfo") == 0)
			gpu_info = static_cast<GpuInfo*>(parse_pointer(argv[6]));

		WriteCrashlog(pid, gpu_info, crashlog_filename, MAX_PATH, crashlog_location);
		/* We need the filename in the UI for sending the report */
		argv[2] = crashlog_filename;
		/* In some cases we just want to write report to disk and exit */
		if (crashlog_location || (argc >= 5 && op_strcmp(argv[3], "-crashaction") == 0 && op_strcmp(argv[4], "exit") == 0))
		{
			return 0;
		}
	}
	else
	{
		/* We install the signal handlers after recovery to avoid multiple dialogs if
		 * we are in crashlog mode. See quick.cpp */
		InstallCrashSignalHandler();
	}
#endif

#ifdef POSIX_GLOBAL_ASSERT_EAR
	PosixAssertLogger grumble;
	PosixLogManager::SetAssert(&grumble);
#endif // First thing possible - *before* any OP_ASSERT !
	// OP_ASSERT(!"Waiting for debugger to attach ..."); // Uncomment, and unsetenv OPASSERT_CONTINUE

	int retval = 0;

	RETURN_VALUE_IF_ERROR(OpThreadTools::Create(&g_thread_tools), -1);

	if (argc >= 3 && op_strcmp(argv[1], "-multiproc") == 0)
	{
		// Debugging: define OPERA_COMPONENT_SLEEP to a number of seconds to sleep in main
		const char* want_sleep = op_getenv("OPERA_COMPONENT_SLEEP");
		if (want_sleep)
		{
			unsigned seconds = op_strtoul(want_sleep, NULL, 10);
			fprintf(stderr, "opera sub-process [%d]: sleeping for %u seconds\n", getpid(), seconds);
			sleep(seconds);
		}

		// Start sub-process
		PosixIpcComponentPlatform* platform;
		if (OpStatus::IsError((PosixIpcComponentPlatform::Create(platform, argv[2]))))
		{
			fprintf(stderr, "opera sub-process [%d]: Error on creation of new process component\n", getpid());
			return -1;
		}
		retval = platform->Run();
		OP_DELETE(platform);
	}
	else
	{
		// Create the main component manager
		OP_ASSERT(!g_component_manager);
		OpComponent* singleton;
		if (OpStatus::IsError(OpComponentManager::Create(&g_component_manager)) ||
			OpStatus::IsError(g_component_manager->CreateComponent(singleton, COMPONENT_SINGLETON)))
		{
			OP_DELETE(g_component_manager);
			fprintf(stderr, "opera [%d]: Out of memory on initialization\n", getpid());
			return -1;
		}
		g_opera = static_cast<CoreComponent*>(singleton);
	
		PosixIpcProcessManager process_manager;
		X11OpMessageLoop messageloop(process_manager);
		g_component_manager->SetPlatform(&messageloop);

#ifdef _DEBUG
		const char *dbgconf = op_getenv("OPERA_DEBUG_CONFIG");
		uni_char* err = Debug::InitSettings(dbgconf ? dbgconf : "dbug/debug.txt");
		if (err)
		{
			char* arr = uni_down_strdup(err);
			OP_DELETEA(err);
			if (arr) fprintf(stderr, "%s\n", arr);
			op_free(arr);
		}
#endif
		OP_STATUS error;
		TRAP(error, retval = main_contentL(argc, argv));

		if (OpStatus::IsError(error))
		{
			fprintf(stderr, "Uncaught exception. Code %d\n", error);
			retval = -1;
		}

		OP_DELETE(g_component_manager);
		g_component_manager = NULL;
		g_opera = NULL;
	}

	OP_DELETE(g_thread_tools);
	g_thread_tools = NULL;

#ifdef CRASHLOG_CRASHLOGGER
	OP_DELETEA(crashlog_filename);
#endif

	// MemLeaks leakdetect;
#ifdef ENABLE_MEMORY_DEBUGGING
# ifdef __GNUC__
	/* This is a glibc-specific hack to force cleanup.  A call to __cxa_finalize
	 * with NULL argument runs all static destructors, atexit handlers etc,
	 * including those for all shared libraries.  See rev 1.22 of this file for
	 * a different hack, in case this one doesn't work with some versions of
	 * glibc (that one doesn't ensure that the shared libraries are finalized). */
	__cxa_finalize(0);
	/* All globals and statics are now finalized, no more code should be added
	 * after this point! */
# endif
	OpMemDebug_ReportLeaks();
#endif

#ifdef POSIX_GLOBAL_ASSERT_EAR
	PosixLogManager::SetAssert(NULL);
#endif // Last thing possible - before ~grumble, but after all OP_ASSERT !
	return retval;
}

PosixIpcComponentPlatform* PosixIpcComponentPlatform::CreateComponent(OpComponentType type)
{
	return NULL;
}
