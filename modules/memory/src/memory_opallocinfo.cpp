/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the OpAllocInfo object
 * 
 * This file implemnts the OpAllocInfo object, which is created and
 * populated at the various functions used to debug memory allocations.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_opallocinfo.h"
#include "modules/memory/src/memory_memguard.h"

OpAllocInfo::OpAllocInfo(const char* file, int line, const char* obj,
						 UINT32 type)
{
#ifdef MEMORY_USE_LOCKING
	OpMemory::MemdebugLock();
#endif

	OpSrcSite* site = OpSrcSite::GetSite(file, line);

	array_count = 0;
	array_elm_size = 0;
	flags = type;
	allocsite_id = site->GetId();
	object = (obj == 0 ? "unknown" : obj);

#if MEMORY_ALLOCINFO_STACK_DEPTH > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
	callstack = 0;
#  endif
#else
	zero_addr = 0;
#endif

#ifdef MEMORY_USE_LOCKING
	OpMemory::MemdebugUnlock();
#endif
}

OpAllocInfo::OpAllocInfo(OpMemGuardInfo* mgi)
{
	array_count = 0;
	array_elm_size = 0;
	flags = mgi->bits.flags;

#if MEMORY_ALLOCINFO_STACK_DEPTH > 0
#  ifdef MEMORY_CALLSTACK_DATABASE

	if ( mgi->bits.flags & MEMORY_FLAG_IS_REALLOCATED )
	{
		// Has been reallocated
		allocsite_id = mgi->reallocsiteid;
		callstack = mgi->reallocstack_new;
		op_memset(stack, 0, MEMORY_ALLOCINFO_STACK_DEPTH * sizeof(UINTPTR));
		op_memcpy(stack, mgi->reallocstack_new->GetStack(),
				  mgi->reallocstack_new->GetSize() * sizeof(UINTPTR));
	}
	else
	{
		// Is a plain allocation
		allocsite_id = mgi->allocsiteid;
		callstack = mgi->allocstack_new;
		op_memset(stack, 0, MEMORY_ALLOCINFO_STACK_DEPTH * sizeof(UINTPTR));
		op_memcpy(stack, mgi->allocstack_new->GetStack(),
				  mgi->allocstack_new->GetSize() * sizeof(UINTPTR));
	}

#  else

	// Copy callstack from mgi

	if ( mgi->bits.flags & MEMORY_FLAG_IS_REALLOCATED )
	{
		// Has been reallocated
		op_memset(stack, 0, MEMORY_ALLOCINFO_STACK_DEPTH * sizeof(UINTPTR));
		op_memcpy(stack, mgi->reallocstack,
				  MEMORY_KEEP_REALLOCSTACK * sizeof(UINTPTR));
	}
	else
	{
		// Is a plain allocation
		op_memset(stack, 0, MEMORY_ALLOCINFO_STACK_DEPTH * sizeof(UINTPTR));
		op_memcpy(stack, mgi->allocstack,
				  MEMORY_KEEP_ALLOCSTACK * sizeof(UINTPTR));
	}
#  endif
#else
	zero_addr = 0;
#endif
}

#if defined(MEMORY_CALLSTACK_DATABASE) && MEMORY_ALLOCINFO_STACK_DEPTH > 0

OpCallStack* OpAllocInfo::GetOpCallStack(int size)
{
	if ( callstack == 0 )
		callstack = OpCallStackManager::GetCallStack(stack, size);

	return callstack;
}

#endif // MEMORY_CALLSTACK_DATABASE && MEMORY_ALLOCINFO_STACK_DEPTH > 0

#endif // ENABLE_MEMORY_DEBUGGING
