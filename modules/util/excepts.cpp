/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(MEMTEST) || defined(ADDR2LINE_SUPPORT)
#include "modules/memtools/mem_util.h"
#endif
#ifdef ADDR2LINE_SUPPORT
#include "modules/memtools/addr2line.h"
#endif // ADDR2LINE_SUPPORT
#if defined USE_OP_THREAD_TOOLS && defined _DEBUG
#include "modules/pi/OpSystemInfo.h"
#endif // USE_OP_THREAD_TOOLS && _DEBUG
#if !defined(USE_CXX_EXCEPTIONS) && !defined(EPOC)

CleanupItem::CleanupItem()
{
	next = UTIL_CLEANUP_STACK;
	UTIL_CLEANUP_STACK = this;
}

/**
 * Called during normal program stack unwinding (return, goto).
 */
CleanupItem::~CleanupItem()
{
	UTIL_CLEANUP_STACK = next;
}

/**
 * Called during exception stack unwinding (leave/throw).
 */
void
CleanupItem::Cleanup(int)
{
	UTIL_CLEANUP_STACK = next;
}

void
CleanupItem::CleanupAll(int error)
{
	while (UTIL_CLEANUP_STACK)
		UTIL_CLEANUP_STACK->Cleanup(error);
}

void
CleanupCatcher::Cleanup(int _error)
{
	// Take this trap handler out of the cleanup stack, to avoid endless loops
	// on retrow.
	CleanupItem::Cleanup(error);

	error = _error;
	op_longjmp(catcher, 1);
}

#ifdef USE_DEBUGGING_OP_STATUS
void
User::Leave(OP_STATUS error)
{
	Leave(error.GetValue());
}
#endif // USE_DEBUGGING_OP_STATUS

void
User::Leave(int error)
{
#ifdef MEMTEST
	oom_report_and_remove_unanchored_objects();
#endif // MEMTEST
	// Is this legal?
	if (0 == error)
	{
		//assert(!"Trying to leave with no error set (error=0)!");
		LEAVE(OpStatus::ERR);
	}

	CleanupItem::CleanupAll(error);

	// No need trying to LEAVE() here.  It would only lead to endless
	// recursion. [pere 2001-05-21]
	OP_ASSERT(!"Trying to leave outside a try block!");
}

void
User::LeaveNoMemory()
{
	LEAVE(OpStatus::ERR_NO_MEMORY);
}

int
User::LeaveIfError(int error)
{
	if (OpStatus::IsError(error))
		Leave(error);
	return error;
}

#endif // not USE_CXX_EXCEPTIONS and not EPOC

#ifdef _DEBUG
void leave_trapped()
{
	// put a breakpoint here to catch leave-events
	int i = 9;
	(void)i;
}
#endif
