/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include <ctype.h>
#include <signal.h>
#include <sys/time.h>

#include "modules/debug/debug.h"

#if defined(_DEBUG) && defined(DBG_CAP_IMPL_IN_PLATFORM)

void
#ifdef DEBUG_ENABLE_OPASSERT // no suitable capability ...
Debug_OpAssert
#else // before anticimex_2_beta_4:
Debug::OpAssert
#endif
(const char* expression, const char* file, int line)
{
	fprintf(stderr,
			"### Assertion failed for expression: %s\n"
			"### \tin %s:%d\n", expression, file, line);

	fflush(stderr);
	Debugger();
}

void Debug::GetTime(DebugTime& time)
{
//#warning "port me"
// FIXME: port me (Debug::GetTime)
	time.sec = time.msec = 0;
}

void
#ifdef DEBUG_ENABLE_SYSTEM_OUTPUT // would be better to have a capability ...
dbg_systemoutput
#else
Debug::SystemDebugOutput
#endif
(const uni_char* str)
{
	// This fuction is deliberately rather self-contained, to allow OP_ASSERT without a full core.
	if (!str)
		return;

	const uni_char* s=str;
	int length = 0;
	while (*s++)
		++length;
	s = str;
	char *str8 = (char*)malloc(length+1),*t=str8;
	if (!t)
		return;
	for (int i=0; i<length; i++)
		*t++ = (char) *s++;
	*t = '\0';
	fprintf(stderr, "%s", str8);
	free(str8);
}

#ifdef DEBUG_ENABLE_SYSTEM_OUTPUT
void dbg_logfileoutput(const char* txt, int len)
{
#warning "implement me"

}
#endif // DEBUG_ENABLE_SYSTEM_OUTPUT


#endif // DBG_CAP_IMPL_IN_PLATFORM
