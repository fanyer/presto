/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef OPMEMORY_EXECUTABLE_SEGMENT

#include "modules/pi/system/OpMemory.h"

#ifdef UNIX
# include <sys/mman.h>

/* static */ void
OpMemory::DestroySegment(const OpMemSegment* mseg)
{
	munmap(mseg->address, mseg->size);
	OP_DELETE(mseg);
}

/* static */ const OpMemory::OpMemSegment*
OpMemory::CreateExecSegment(size_t minimum)
{
	OpMemSegment *mseg = OP_NEW(OpMemSegment, ());

	if (mseg)
	{
		minimum = (minimum + 4095) & ~4095;

		mseg->address = mmap(NULL, minimum, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
		mseg->size = minimum;
		mseg->type = MEMORY_SEGMENT_EXECUTABLE;

		if (mseg->address == MAP_FAILED)
		{
			OP_DELETE(mseg);
			mseg = NULL;
		}
	}

	return mseg;
}

/* static */ void*
OpMemory::WriteExec(const OpMemSegment* mseg, void* ptr, size_t size)
{
	mprotect(mseg->address, mseg->size, PROT_READ | PROT_WRITE);
	return ptr;
}

/* static */ OP_STATUS
OpMemory::WriteExecDone(const OpMemSegment* mseg, void* ptr, size_t size)
{
	mprotect(mseg->address, mseg->size, PROT_READ | PROT_EXEC);

#ifdef ARCHITECTURE_ARM
	/* GCC apparently defines this function.  Solves mysterious problems. */
	__clear_cache(ptr, static_cast<char *>(ptr) + size);
#endif // ARCHITECTURE_ARM

	return OpStatus::OK;
}

#endif // UNIX

#ifdef MSWIN
/* wingogi and desktop uses a separate heap, so do the same, but keep old/other implementation
   based on VirtualAlloc() around should you need to switch. */
#define USE_SEP_EXEC_HEAP

# ifdef USE_SEP_EXEC_HEAP
HANDLE g_executableMemoryHeap;

const OpMemory::OpMemSegment* OpMemory::CreateExecSegment(size_t minimum)
{
	if (!g_executableMemoryHeap)
	{
		g_executableMemoryHeap = ::HeapCreate(HEAP_CREATE_ENABLE_EXECUTE | HEAP_NO_SERIALIZE, 0, 0);

		if (!g_executableMemoryHeap)
			return NULL;
	}

	OpMemSegment *mseg = OP_NEW(OpMemSegment, ());

	if (mseg)
	{
		SIZE_T size = static_cast<SIZE_T>(minimum);

		if (!(mseg->address = ::HeapAlloc(g_executableMemoryHeap, 0, size)))
		{
			OP_DELETE(mseg);
			mseg = NULL;
		}
		else
		{
			mseg->size = minimum;
			mseg->type = MEMORY_SEGMENT_EXECUTABLE;
		}
    }
	return mseg;
}

void* OpMemory::WriteExec(const OpMemSegment* mseg, void* ptr, size_t size)
{
	// Assert on illegal use
	OP_ASSERT(mseg != 0);
	OP_ASSERT(mseg->type == MEMORY_SEGMENT_EXECUTABLE);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));

	return ptr;
}

OP_STATUS OpMemory::WriteExecDone(const OpMemSegment* mseg, void* ptr, size_t size)
{
	// Assert on illegal use
	OP_ASSERT(mseg != 0);
	OP_ASSERT(mseg->type == MEMORY_SEGMENT_EXECUTABLE);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));

	return OpStatus::OK;
}

void OpMemory::DestroySegment(const OpMemory::OpMemSegment* seg)
{
	OpMemSegment* mseg = const_cast<OpMemSegment*>(seg);

	OP_ASSERT(mseg != 0);
	OP_ASSERT(mseg->type == MEMORY_SEGMENT_EXECUTABLE);

	::HeapFree(g_executableMemoryHeap, 0, mseg->address);
	OP_DELETE(mseg);
}

# else
/* static */ void
OpMemory::DestroySegment(const OpMemSegment* mseg)
{
	VirtualFree(mseg->address, mseg->size, MEM_RELEASE);
	OP_DELETE(mseg);
}

/* static */ const OpMemory::OpMemSegment*
OpMemory::CreateExecSegment(size_t minimum)
{
	OpMemSegment *mseg = OP_NEW(OpMemSegment, ());

	if (mseg)
	{
		minimum = (minimum + 4095) & ~4095;

		if (!(mseg->address = ::VirtualAlloc(NULL, minimum, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)))
		{
			OP_DELETE(mseg);
			mseg = NULL;
		}
		else
		{
			mseg->size = minimum;
			mseg->type = MEMORY_SEGMENT_EXECUTABLE;
		}
	}
	return mseg;
}

/* static */ void*
OpMemory::WriteExec(const OpMemSegment* mseg, void* ptr, size_t size)
{
	return ptr;
}

/* static */ OP_STATUS
OpMemory::WriteExecDone(const OpMemSegment* mseg, void* ptr, size_t size)
{
	return OpStatus::OK;
}
# endif // USE_SEP_EXEC_HEAP

#endif // MSWIN

#endif // OPMEMORY_EXECUTABLE_SEGMENT
