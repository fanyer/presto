/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement a layer on top of class OpMemory porting interface
 *
 * This thin layer is used for a few things: Implement memory accounting
 * and policies, a debugging layer and instrumentation for selftests.
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#include "modules/memory/src/memory_pilayer.h"

#ifdef OPMEMORY_VIRTUAL_MEMORY

#ifdef MEMORY_USE_LOCKING
const OpMemory::OpMemSegment* OpMemory_CreateVirtualSegment(size_t minimum,
															BOOL memlock)
#else
const OpMemory::OpMemSegment* OpMemory_CreateVirtualSegment(size_t minimum)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	const OpMemory::OpMemSegment* mseg;
	mseg = OpMemory::CreateVirtualSegment(minimum);

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif

	return mseg;
}

BOOL OpMemory_VirtualAlloc(const OpMemory::OpMemSegment* mseg, void* ptr,
						   size_t size, OpMemoryClass type)
{
	BOOL rc = FALSE;
	(void)type;

#ifdef ENABLE_MEMORY_MANAGER
	OpMemoryManager* mm = g_mem_manager;
	if ( mm->Alloc(type, size) )
#endif
	{
		rc = OpMemory::VirtualAlloc(mseg, ptr, size);

#ifdef ENABLE_MEMORY_MANAGER
		if ( ! rc )
			mm->Free(type, size);
#endif
	}

	return rc;
}

void OpMemory_VirtualFree(const OpMemory::OpMemSegment* mseg, void* ptr,
						  size_t size, OpMemoryClass type)
{
	(void)type;

#ifdef ENABLE_MEMORY_MANAGER
	g_mem_manager->Free(type, size);
#endif

	OpMemory::VirtualFree(mseg, ptr, size);
}

#endif // OPMEMORY_VIRTUAL_MEMORY

#if defined(OPMEMORY_VIRTUAL_MEMORY) || defined(OPMEMORY_MEMORY_SEGMENT)

#ifdef MEMORY_USE_LOCKING
void OpMemory_DestroySegment(const OpMemory::OpMemSegment* mseg, BOOL memlock)
#else
void OpMemory_DestroySegment(const OpMemory::OpMemSegment* mseg)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	OpMemory::DestroySegment(mseg);

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif
}

#endif // OPMEMORY_VIRTUAL_MEMORY || OPMEMORY_MEMORY_SEGMENT
