/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEMTOOLS_ENABLE_STACKGUARD

#include "modules/memtools/stacktraceguard.h"

/* static */
BOOL OpStackTraceGuard::StackAddressIsOK(void* addr)
{
	return g_current_top_of_stack == 0 ||
		(char*)g_current_top_of_stack > (char*)addr;
}

//
// This global variable needs to go, but we keep it until
// memtools gets its own proper MemtoolsModule in
// the global opera object.
//
OpStackTraceGuard* OpStackTraceGuard::g_current_top_of_stack = 0;

#endif // MEMTOOLS_ENABLE_STACKGUARD
