/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if !defined(USE_CXX_EXCEPTIONS) && !defined(EPOC)

CleanupItem *CleanupItem::cleanupstack = 0;

CleanupItem::CleanupItem()
{
	next = GetLastStackItem();
	SetLastStackItem(this);
}

/**
 * Called during normal program stack unwinding (return, goto).
 */
CleanupItem::~CleanupItem()
{
	SetLastStackItem(next);
}

/**
 * Called during exception stack unwinding (leave/throw).
 */
void
CleanupItem::Cleanup(int error)
{
	error = 0; // Keep compiler quiet
	SetLastStackItem(next);
}

CleanupCatcher::CleanupCatcher()
	: CleanupItem(),
	  error(0)
{
}

CleanupCatcher::~CleanupCatcher()
{
}

void
CleanupCatcher::Cleanup(int _error)
{
	// Take this trap handler out of the cleanup stack, to avoid endless loops
	// on retrow.
	CleanupItem::Cleanup(error);

	error = _error;
	longjmp(catcher, 1);
}


void
User::Leave(int error)
{
	// Is this legal?
	if (0 == error)
	{
		//assert(!"Trying to leave with no error set (error=0)!");
		LEAVE(OpStatus::ERR);
	}

	while (CleanupItem::GetLastStackItem())
		CleanupItem::GetLastStackItem()->Cleanup(error);

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
