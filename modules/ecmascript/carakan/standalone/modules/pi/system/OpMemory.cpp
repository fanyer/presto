/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/pi/system/OpMemory.h"

#ifdef UNIX
# include <sys/mman.h>

#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

#if defined(OPMEMORY_EXECUTABLE_SEGMENT) && defined(ARCHITECTURE_MIPS)
#include <sys/cachectl.h>
#endif

/* static */ void
OpMemory::DestroySegment(const OpMemSegment* seg)
{
	// The porting interface owns this object; un-const it for our use:
	OpMemSegment* mseg = const_cast<OpMemSegment*>(seg);

	OP_ASSERT(mseg != 0);

	switch ( mseg->type )
	{
#ifdef OPMEMORY_MEMORY_SEGMENT
	case MEMORY_SEGMENT_TEMPORARY:
	case MEMORY_SEGMENT_DOCUMENT:
	case MEMORY_SEGMENT_PERSISTENT:
		op_lowlevel_free(mseg->address);
		mseg->type = MEMORY_SEGMENT_UNUSED;
		break;
#endif

#ifdef OPMEMORY_VIRTUAL_MEMORY
	case MEMORY_SEGMENT_VIRTUAL:
	{
#ifdef DEBUG_ENABLE_OPASSERT
		int rc =
#endif
			munmap(mseg->address, mseg->size);

		OP_ASSERT(rc == 0);

		mseg->type = MEMORY_SEGMENT_UNUSED;
		break;
	}
#endif // OPMEMORY_VIRTUAL_MEMORY

#if defined OPMEMORY_EXECUTABLE_SEGMENT || defined OPMEMORY_ALIGNED_SEGMENT
#ifdef OPMEMORY_EXECUTABLE_SEGMENT
	case MEMORY_SEGMENT_EXECUTABLE:
#endif // OPMEMORY_EXECUTABLE_SEGMENT
#ifdef OPMEMORY_ALIGNED_SEGMENT
	case MEMORY_SEGMENT_ALIGNED:
#endif // OPMEMORY_ALIGNED_SEGMENT
	{
#ifdef DEBUG_ENABLE_OPASSERT
		int rc =
#endif
			munmap(mseg->address, mseg->size);

		OP_ASSERT(rc == 0);

		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_EXECUTABLE_SEGMENT || OPMEMORY_ALIGNED_SEGMENT

#ifdef OPMEMORY_STACK_SEGMENT
	case MEMORY_SEGMENT_STACK:
	{
#ifdef DEBUG_ENABLE_OPASSERT
		int rc =
#endif
			munmap(static_cast<char *>(mseg->address) - 4096, mseg->size + 4096);

		OP_ASSERT(rc == 0);

		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_STACK_SEGMENT

	default:
		OP_ASSERT(!"Illegal segment type - what's up?");
		break;
	}
}

#if defined OPMEMORY_EXECUTABLE_SEGMENT || defined OPMEMORY_ALIGNED_SEGMENT
static UINTPTR g_page_mask = 0;
#endif // OPMEMORY_EXECUTABLE_SEGMENT || OPMEMORY_ALIGNED_SEGMENT

#ifdef OPMEMORY_EXECUTABLE_SEGMENT

const OpMemory::OpMemSegment* OpMemory::CreateExecSegment(size_t minimum)
{
	if ( g_page_mask == 0 )
		g_page_mask = sysconf(_SC_PAGESIZE) - 1;

	OpMemSegment* mseg = OP_NEW(OpMemSegment, ());

	if ( mseg != 0 )
	{
		const size_t granularity = (1<<14); // 16K
		size_t size = (minimum + (granularity - 1)) & ~(granularity - 1);

#ifdef _DEBUG
		//
		// Do strong page access protection for debug builds. This
		// helps ensure honest use of the APIs.
		//
		const int prot = PROT_NONE;
#else
		//
		// In release builds, don't bother to change permissions for
		// performance reasons.
		//
		const int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
#endif

		void* ptr =
			mmap((void*)0, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if ( ptr == MAP_FAILED )
		{
			OP_DELETE(mseg);
			mseg = 0;
		}
		else
		{
			mseg->address = ptr;
			mseg->size = size;
			mseg->type = MEMORY_SEGMENT_EXECUTABLE;
		}
	}

	return mseg;
}

void* OpMemory::WriteExec(const OpMemSegment* mseg, void* ptr, size_t size)
{
	// Assert on illegal use
	OP_ASSERT(mseg->type == MEMORY_SEGMENT_EXECUTABLE);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));

#ifdef _DEBUG
	// Do strong page access protection for debug builds
	UINTPTR page_base = ((UINTPTR)ptr) & ~g_page_mask;
	UINTPTR top  = (UINTPTR)((char*)ptr + size);
	size_t page_size = (top - page_base + g_page_mask) & ~g_page_mask;

#ifdef DEBUG_ENABLE_OPASSERT
	int rc =
#endif
		mprotect((void*)page_base, page_size, PROT_READ | PROT_WRITE);

	OP_ASSERT(rc == 0);
#endif // _DEBUG

	return ptr;
}

OP_STATUS OpMemory::WriteExecDone(const OpMemSegment* mseg, void* ptr, size_t size)
{
	// Assert on illegal use
	OP_ASSERT(mseg->type == MEMORY_SEGMENT_EXECUTABLE);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));

#ifdef _DEBUG
	// Do strong page access protection for debug builds
	UINTPTR page_base = ((UINTPTR)ptr) & ~g_page_mask;
	UINTPTR top  = (UINTPTR)((char*)ptr + size);
	size_t page_size = (top - page_base + g_page_mask) & ~g_page_mask;

#ifdef DEBUG_ENABLE_OPASSERT
	int rc =
#endif
		mprotect((void*)page_base, page_size, PROT_READ | PROT_EXEC);

	OP_ASSERT(rc == 0);
#endif // _DEBUG

#ifdef ARCHITECTURE_ARM
	/* GCC apparently defines this function.  Solves mysterious problems. */
	__clear_cache(ptr, static_cast<char *>(ptr) + size);
#endif // ARCHITECTURE_ARM

#ifdef ARCHITECTURE_MIPS
	// Flush instruction cache.
	cacheflush(ptr, size, ICACHE);
#endif

	return OpStatus::OK;
}

#endif // OPMEMORY_EXECUTABLE_SEGMENT

#ifdef OPMEMORY_STACK_SEGMENT

const OpMemory::OpMemSegment* OpMemory::CreateStackSegment(size_t size)
{
	/* API requirement: size must be power of two. */
	OP_ASSERT(((size - 1) & size) == 0);

	/* API requirement: size must be at least 4096. */
	OP_ASSERT(size >= 4096);

	OpMemSegment* mseg = OP_NEW(OpMemSegment, ());

	if ( mseg != 0 )
	{
		/* Allocate requested size plus one page. */
		void* ptr =
			mmap((void*)0, size + 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if ( ptr == MAP_FAILED )
		{
			OP_DELETE(mseg);
			mseg = 0;
		}
		else
		{
			/* Make the extra page allocated completely inaccessible
			   so that we crash instantly on stack overflows. */
			mprotect(mseg->address, 4096, PROT_NONE);

			mseg->address = static_cast<char *>(ptr) + 4096;
			mseg->size = size;
			mseg->type = MEMORY_SEGMENT_STACK;
		}
	}

	return mseg;
}

#ifdef DEBUG_ENABLE_OPASSERT

static BOOL IsSegmentCurrentStack(const OpMemory::OpMemSegment* segment)
{
	int placeholder;

	/* More or less accurate. */
	char *stack_pointer = reinterpret_cast<char *>(&placeholder);

	/* Segment boundaries. */
	char *segment_start = static_cast<char *>(segment->address);
	char *segment_end = segment_start + segment->size;

	return segment_start < stack_pointer && stack_pointer < segment_end;
}

#endif // DEBUG_ENABLE_OPASSERT

void OpMemory::SignalStackSwitch(StackSwitchOperation operation, const OpMemSegment* current_stack, const OpMemSegment* new_stack)
{
	/* We needn't do anything here.  Just double-check that the
	   arguments are sane. */

	switch (operation)
	{
	case OpMemory::STACK_SWITCH_SYSTEM_TO_CUSTOM:
		OP_ASSERT(current_stack == NULL);
		OP_ASSERT(new_stack != NULL && !IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_BEFORE_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack != NULL && !IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_AFTER_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && !IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack != NULL && IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_CUSTOM_TO_SYSTEM:
		OP_ASSERT(current_stack != NULL && !IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack == NULL);
		break;
	}
}

#endif // OPMEMORY_STACK_SEGMENT

#ifdef OPMEMORY_ALIGNED_SEGMENT

const OpMemory::OpMemSegment* OpMemory::CreateAlignedSegment(size_t size, size_t alignment)
{
	if ( g_page_mask == 0 )
		g_page_mask = sysconf(_SC_PAGESIZE) - 1;

	OpMemSegment* mseg = OP_NEW(OpMemSegment, ());

	if ( mseg != 0 )
	{
		int flags = MAP_PRIVATE | MAP_ANONYMOUS;

#ifdef MAP_UNINITIALIZED
		flags |= MAP_UNINITIALIZED;
#endif // MAP_UNINITIALIZED

		/* First attempt: just mmap() exactly the requested size.  Not
		   guaranteed to give a correctly aligned block, but testing has shown
		   that it is quite likely to do so. */

		void* ptr =
			mmap((void*)0, size, PROT_READ | PROT_WRITE, flags, -1, 0);

		if ( ptr == MAP_FAILED )
		{
			OP_DELETE(mseg);
			return NULL;
		}

		if ( (reinterpret_cast<UINTPTR>(ptr) & (alignment - 1)) != 0 )
		{
			/* Block allocated in first attempt was not properly aligned.  Free
			   it, and try again. */
#ifdef DEBUG_ENABLE_OPASSERT
			int rc =
#endif
				munmap(ptr, size);

			OP_ASSERT(rc == 0);

			/* Second attempt: allocate a block that is that is sufficiently
			   many pages larger than the requested size that we can find an
			   appropriately aligned block within the allocated block regardless
			   of the alignment of the allocated block.

			   The extra padding needed is one page less than the alignment
			   requirement (rounded up to the nearest page size boundary.) */

			size_t total_size = (size + alignment - 1) & ~g_page_mask;

			ptr = mmap((void*)0, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);

			if ( ptr == MAP_FAILED )
			{
				OP_DELETE(mseg);
				return NULL;
			}
			else
			{
				/* Trimming: find the appropriately aligned sub-block and then
				   munmap() the extra allocated pages before and/or after it. */

				char* base = static_cast<char*>(ptr);
				char* aligned = reinterpret_cast<char*>((reinterpret_cast<UINTPTR>(base) + alignment - 1) & ~static_cast<UINTPTR>(alignment - 1));

				if ( base != aligned )
				{
#ifdef DEBUG_ENABLE_OPASSERT
					int rc =
#endif
						munmap(base, aligned - base);

					OP_ASSERT(rc == 0);
				}

				if ( (aligned - base) + size < total_size )
				{
#ifdef DEBUG_ENABLE_OPASSERT
					int rc =
#endif
						munmap(aligned + size, total_size - ((aligned - base) + size));

					OP_ASSERT(rc == 0);
				}

				ptr = aligned;
			}
		}

		mseg->address = ptr;
		mseg->size = size;
		mseg->type = MEMORY_SEGMENT_ALIGNED;
	}

	return mseg;
}

#endif // OPMEMORY_ALIGNED_SEGMENT

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

#ifdef OPMEMORY_ALIGNED_SEGMENT
struct WingogiAlignedMemSegment : public OpMemory::OpMemSegment
{
	void* virtual_alloc_address;
};
#endif // OPMEMORY_ALIGNED_SEGMENT

void OpMemory::DestroySegment(const OpMemory::OpMemSegment* seg)
{
	OpMemSegment* mseg = const_cast<OpMemSegment*>(seg);

	OP_ASSERT(mseg != 0);

	switch ( mseg->type )
	{
#ifdef OPMEMORY_EXECUTABLE_SEGMENT
	case MEMORY_SEGMENT_EXECUTABLE:
	{
		HeapFree(g_executableMemoryHeap, 0, mseg->address);
		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_EXECUTABLE_SEGMENT

#ifdef OPMEMORY_VIRTUAL_MEMORY
	case MEMORY_SEGMENT_VIRTUAL:
	{
		::VirtualFree(mseg->address, 0, MEM_RELEASE);
		mseg->type = MEMORY_SEGMENT_UNUSED;
		break;
	}
#endif // OPMEMORY_VIRTUAL_MEMORY

#ifdef OPMEMORY_STACK_SEGMENT
	case MEMORY_SEGMENT_STACK:
	{
#ifdef DEBUG_ENABLE_OPASSERT
		int rc =
#endif
			::VirtualFree(static_cast<char *>(mseg->address) - 4096, 0, MEM_RELEASE);

		OP_ASSERT(rc);

		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_STACK_SEGMENT

#ifdef OPMEMORY_ALIGNED_SEGMENT
	case MEMORY_SEGMENT_ALIGNED:
	{
#ifdef DEBUG_ENABLE_OPASSERT
		BOOL res =
#endif // DEBUG_ENABLE_OPASSERT
			::VirtualFree(static_cast<WingogiAlignedMemSegment*>(mseg)->virtual_alloc_address, 0, MEM_RELEASE);
		OP_ASSERT(res);
		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_ALIGNED_SEGMENT

	default:
		OP_ASSERT(!"Unsupported OpMemSegment type for DestroySegment");
	}
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

#ifdef OPMEMORY_STACK_SEGMENT

const OpMemory::OpMemSegment* OpMemory::CreateStackSegment(size_t size)
{
	/* API requirement: size must be power of two. */
	OP_ASSERT(((size - 1) & size) == 0);

	/* API requirement: size must be at least 4096. */
	OP_ASSERT(size >= 4096);

	OpMemSegment* mseg = OP_NEW(OpMemSegment, ());

	if ( mseg != 0 )
	{
		/* Allocate requested size plus one page. */
		void* ptr =
			::VirtualAlloc(NULL, size + 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if ( ptr == 0 )
		{
			OP_DELETE(mseg);
			mseg = 0;
		}
		else
		{
			/* Make the extra page allocated completely inaccessible
			   so that we crash instantly on stack overflows. */
			unsigned long op;
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL rc =
#endif
				::VirtualProtect(ptr, 1, PAGE_NOACCESS, &op);

			OP_ASSERT(rc);

			mseg->address = static_cast<char *>(ptr) + 4096;
			mseg->size = size;
			mseg->type = MEMORY_SEGMENT_STACK;
		}
	}

	return mseg;
}

#ifdef DEBUG_ENABLE_OPASSERT

static BOOL IsSegmentCurrentStack(const OpMemory::OpMemSegment* segment)
{
	int placeholder;

	/* More or less accurate. */
	char *stack_pointer = reinterpret_cast<char *>(&placeholder);

	/* Segment boundaries. */
	char *segment_start = static_cast<char *>(segment->address);
	char *segment_end = segment_start + segment->size;

	return segment_start < stack_pointer && stack_pointer < segment_end;
}

#endif // DEBUG_ENABLE_OPASSERT

void OpMemory::SignalStackSwitch(StackSwitchOperation operation, const OpMemSegment* current_stack, const OpMemSegment* new_stack)
{
	/* We needn't do anything here.  Just double-check that the
	   arguments are sane. */

	switch (operation)
	{
	case OpMemory::STACK_SWITCH_SYSTEM_TO_CUSTOM:
		OP_ASSERT(current_stack == NULL);
		OP_ASSERT(new_stack != NULL && !IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_BEFORE_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack != NULL && !IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_AFTER_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && !IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack != NULL && IsSegmentCurrentStack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_CUSTOM_TO_SYSTEM:
		OP_ASSERT(current_stack != NULL && !IsSegmentCurrentStack(current_stack));
		OP_ASSERT(new_stack == NULL);
		break;
	}
}

#endif // OPMEMORY_STACK_SEGMENT

#ifdef OPMEMORY_ALIGNED_SEGMENT

const OpMemory::OpMemSegment* OpMemory::CreateAlignedSegment(size_t size, size_t alignment)
{
	WingogiAlignedMemSegment* mseg = OP_NEW(WingogiAlignedMemSegment, ());
	if (mseg)
	{
		// Our strategy to allocate an aligned block is divided into three phases identified
		// by attempt == 0, attempt == MAXATTEMPT and finally everything in between.
		// attempt = 0: Make an exact VirtualAlloc and hope for some luck. That luck is quite common
		//              because Windows normally return blocks with very high alignment.
		// attempt 1 - MAXATTEMPT: Reserve a too big block, find an aligned segment, free and then make
		//                         a targetted assignment at that address. Extremely likely to work.
		// attempt = MAXATTEMPT: Reserve and commit a big enough block and return an aligned sub-section of it.
		UINTPTR alignment_mask = ~static_cast<UINTPTR>(alignment-1);
		const int MAXATTEMPT = 10;
		for (int attempt = 0; attempt <= MAXATTEMPT; attempt++)
		{
			// First try exact size to really give the system a chance at minimizing fragmentation.
			size_t alloc_size = attempt == 0 ? size : size + alignment;
			char* ptr = static_cast<char*>(::VirtualAlloc(NULL, alloc_size, MEM_RESERVE, PAGE_READWRITE));
			if (!ptr)
			{
				OP_DELETE(mseg);
				mseg = NULL;
				break;
			}
			else
			{
				UINTPTR p = reinterpret_cast<UINTPTR>(ptr);
				char* aligned = reinterpret_cast<char*>((p + alignment - 1) & alignment_mask);

				mseg->address = aligned;
				mseg->size = size;
				mseg->type = MEMORY_SEGMENT_ALIGNED;
				mseg->virtual_alloc_address = ptr;
				if (attempt == 0 || attempt == MAXATTEMPT)
				{
					if (attempt == 0 && aligned != ptr)
					{
#ifdef DEBUG_ENABLE_OPASSERT
						BOOL res =
#endif // DEBUG_ENABLE_OPASSERT
							::VirtualFree(ptr, 0, MEM_RELEASE);
						OP_ASSERT(res);
						continue;
					}
					// Either use the perfectly aligned block (if attempt == 0) or
					// give up and accept some slack (if attempt == MAXATTEMPT)
					void* res = ::VirtualAlloc(aligned, size, MEM_COMMIT, PAGE_READWRITE);
					if (!res)
					{
						DestroySegment(mseg);
						mseg = NULL;
					}
					break;
				}

#ifdef DEBUG_ENABLE_OPASSERT
				BOOL res =
#endif // DEBUG_ENABLE_OPASSERT
					::VirtualFree(ptr, 0, MEM_RELEASE);
				OP_ASSERT(res);
				ptr = static_cast<char*>(::VirtualAlloc(aligned, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
				if (ptr)
				{
					OP_ASSERT(ptr == aligned);
					mseg->virtual_alloc_address = ptr;
					break;
				}
			}
		}
	}

	return mseg;
}

#endif // OPMEMORY_ALIGNED_SEGMENT

#endif // MSWIN
