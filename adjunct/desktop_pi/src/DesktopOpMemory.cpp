/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Alexey Feldgendler (alexeyf)
 */
#include "core/pch.h"

#include "modules/pi/system/OpMemory.h"
#include "adjunct/desktop_pi/DesktopThreads.h"

#ifdef OPMEMORY_MALLOC_LOCK
static OpMutexId g_malloc_mutex_id;

void OpMemory::MallocLock()
{
	static BOOL initialized = FALSE;
	if (!initialized) {
		OP_STATUS err = DesktopThreads::CreateMutex(g_malloc_mutex_id);
		OP_ASSERT(OpStatus::IsSuccess(err));
		OpStatus::Ignore(err);
		initialized = TRUE;
	}
	OP_STATUS err = DesktopThreads::LockMutex(g_malloc_mutex_id);
	OP_ASSERT(OpStatus::IsSuccess(err));
	OpStatus::Ignore(err);
}

void OpMemory::MallocUnlock()
{
	OP_STATUS err = DesktopThreads::UnlockMutex(g_malloc_mutex_id);
	OP_ASSERT(OpStatus::IsSuccess(err));
	OpStatus::Ignore(err);
}
#endif // OPMEMORY_MALLOC_LOCK

#ifdef OPMEMORY_MEMDEBUG_LOCK
static OpMutexId g_memdebug_mutex_id;

void OpMemory::MemdebugLock()
{
	static BOOL initialized = FALSE;
	if (!initialized) {
		OP_STATUS err = DesktopThreads::CreateMutex(g_memdebug_mutex_id);
		OP_ASSERT(OpStatus::IsSuccess(err));
		OpStatus::Ignore(err);
		initialized = TRUE;
	}
	OP_STATUS err = DesktopThreads::LockMutex(g_memdebug_mutex_id);
	OP_ASSERT(OpStatus::IsSuccess(err));
	OpStatus::Ignore(err);
}

void OpMemory::MemdebugUnlock()
{
	OP_STATUS err = DesktopThreads::UnlockMutex(g_memdebug_mutex_id);
	OP_ASSERT(OpStatus::IsSuccess(err));
	OpStatus::Ignore(err);
}
#endif // OPMEMORY_MEMDEBUG_LOCK
