/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef _DEBUG

#include <sys/timeb.h>


/**
 *	IsRunByDebugger
 *
 *	Returns TRUE if the app is run by a debugger
 *	This is the same as Win32 IsDebuggerPresent() but with added implementation
 *  for Win95.
*/

BOOL _DEBUG_IsRunByDebugger()
{
	typedef BOOL (WINAPI * ISDEBUGGERPRESENTPROC)(VOID);

    /* CONSTANTS */
    static const ISDEBUGGERPRESENTPROC fnIsDebuggerPresent =
        (ISDEBUGGERPRESENTPROC)GetProcAddress( GetModuleHandleA("KERNEL32"), "IsDebuggerPresent");

    BOOL fIsRunByDebugger = FALSE;


    if (fnIsDebuggerPresent != NULL)
        fIsRunByDebugger = fnIsDebuggerPresent();

	//	done
	return fIsRunByDebugger;
}

void
#ifdef DEBUG_ENABLE_OPASSERT // no suitable capability ...
Debug_OpAssert
#else // before anticimex_2_beta_4:
Debug::OpAssert
#endif
(const char* expression, const char* file, int line)
{
#if _MSC_VER < 1300
	_assert ((void*)expression, (void*)file, line);
#else
	OpString e;
	OpString f;

	e.Set(expression);
	f.Set(file);
	_wassert (e.CStr(), f.CStr(), line);
#endif
}

void Debug::GetTime(DebugTime& time)
{
	struct timeb tb;
	ftime(&tb);
	time.sec = tb.time;
	time.msec = tb.millitm;
}

void
#ifdef DEBUG_ENABLE_SYSTEM_OUTPUT // would be better to have a capability ...
dbg_systemoutput
#else
Debug::SystemDebugOutput
#endif
(const uni_char* str)
{
	OutputDebugString(str);
}

// (julienp)This implementation is probably not the best, but it doesn't matter a lot because it's debug only.
// It does respect the specification given in debug/src/debug_internal.h
void dbg_logfileoutput(const char* txt, int len)
{
	static FILE* dbg_logfile = NULL;
	if (!dbg_logfile)
	{
		if (len == 0)
			return;

		if ((dbg_logfile = fopen("debug_log.txt", "wbS")) == NULL)
			return;
	}

	fwrite(txt, len, 1, dbg_logfile);
}

// output debug messages to the visual studio output window
void Win32DebugLogging(const uni_char *x, ...)
{
	uni_char *tmp = new uni_char[500];
	int num = 0;
	va_list ap;

	va_start( ap, x );

	if(tmp)
	{
		num = uni_vsnprintf( tmp, 499, x, ap );
		if(num != -1)
		{
			tmp[499] = '\0';
			OutputDebugString(tmp);
		}
		delete [] tmp;
	}
	va_end(ap);
}

#endif
