/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2009-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#if defined(POSIX_OK_MEMORY) || defined(POSIX_OK_MEMLOCK)
#include "modules/pi/system/OpMemory.h"
#endif

#ifdef POSIX_OK_MEMLOCK
#include "platforms/posix/posix_mutex.h"
/** @file posix_memory.cpp Implementation of OpMalloc porting interface.
 *
 * See modules/pi/system/OpMemory.h and the class OpMemory; note that all
 * methods of this class are static, so we do not define a derived class, we
 * simply implement the static methods.
 *
 * Note: globals.
 *
 * This implementation uses global memory, in the form of static variables.
 * There is no obvious way out of that, given that memory management has to work
 * well before g_opera->InitL() can be called, to let us set up the needed
 * globals as part of the Opera globals object.  Generally, however, systems
 * that can support POSIX can also cope with globals ...
 */

# ifdef OPMEMORY_MALLOC_LOCK
static PosixMutex s_malloc_lock(PosixMutex::MUTEX_NORMAL);
void OpMemory::MallocLock() { s_malloc_lock.Acquire(); }
void OpMemory::MallocUnlock() { s_malloc_lock.Release(); }
# endif // OPMEMORY_MALLOC_LOCK

# ifdef OPMEMORY_MEMDEBUG_LOCK
static PosixMutex s_memdbg_lock(PosixMutex::MUTEX_NORMAL);
void OpMemory::MemdebugLock() { s_memdbg_lock.Acquire(); }
void OpMemory::MemdebugUnlock() { s_memdbg_lock.Release(); }
# endif // OPMEMORY_MEMDEBUG_LOCK
#endif // POSIX_OK_MEMLOCK

#ifdef POSIX_OK_MEMORY
# if defined(OPMEMORY_VIRTUAL_MEMORY) || defined(OPMEMORY_EXECUTABLE_SEGMENT) || \
	defined(OPMEMORY_STACK_SEGMENT) || defined(OPMEMORY_ALIGNED_SEGMENT)
/* These variants need get_page_size(), mmap(), &c. */
#define POSMEM_SEGMENTED
#include <sys/mman.h> // mmap(), munmap() and MAP_* constants

// Define macros ALLOC_MMAP and ALLOC_MUNMAP for any uses of
// mmap/munmap that should be treated as memory allocated
// from the system for Opera's use.

#  ifdef CONSTRAIN_OPERA_MEMORY_USAGE
#   include "modules/lea_malloc/lea_malloc.h"
#   define ALLOC_MMAP constrained_external_mmap
#   define ALLOC_MUNMAP constrained_external_munmap
static void *constrained_external_mmap(void *addr, size_t size, int prot, int flags, int fd, off_t offset)
{
	if (!external_alloc(size))
		return MAP_FAILED;
	void *rc = mmap(addr, size, prot, flags, fd, offset);
	if (rc == MAP_FAILED)
		external_free(size);
	return rc;
}
static int constrained_external_munmap(void *addr, size_t size)
{
	int rc = munmap(addr, size);
	external_free(size);
	return rc;
}
#  else
#   define ALLOC_MMAP mmap
#   define ALLOC_MUNMAP munmap
#  endif // CONSTRAIN_OPERA_MEMORY_USAGE

#  ifdef DEBUG_ENABLE_OPASSERT
#   define CHECKED_MUNMAP(X) do { int rc = X; OP_ASSERT(rc == 0); } while (0)
#  else // DEBUG_ENABLE_OPASSERT
#   define CHECKED_MUNMAP(X) do { X; } while (0)
#  endif // DEBUG_ENABLE_OPASSERT
#  define CHECKED_ALLOC_MUNMAP(ptr,size) CHECKED_MUNMAP(ALLOC_MUNMAP(ptr,size))

# endif // OPMEMORY_VIRTUAL_MEMORY || OPMEMORY_{EXECUTABLE,STACK,ALIGNED}_SEGMENT

#if defined(OPMEMORY_STACK_SEGMENT) && !defined(POSIX_STACK_USE_MALLOC)
# ifdef POSIX_STACK_GUARD_PAGE
#  define POSIX_STACK_GUARD_SIZE (get_page_size())
# else // POSIX_STACK_GUARD_SIZE
#  define POSIX_STACK_GUARD_SIZE 0
# endif // POSIX_STACK_GUARD_PAGE
#endif // OPMEMORY_STACK_SEGMENT && !POSIX_STACK_USE_MALLOC

#ifdef VALGRIND
#include "modules/memory/src/memory_valgrind.h"
#endif

class PosixMemSegment : public OpMemory::OpMemSegment
{
public:
	PosixMemSegment()
	{
		address = 0;
		size = 0;
		type = OpMemory::MEMORY_SEGMENT_UNUSED;
	}
#ifdef VALGRIND
	int vg_stack_id;
#endif
};

# ifdef OPMEMORY_VIRTUAL_MEMORY

/** Array of non-heap virtual memory segment descriptors.
 *
 * Since virtual segments may be used in the implementation of malloc, their
 * descriptors cannot be obtained from malloc; so a fixed-size array of them is
 * the best we can do.  See TWEAK_POSIX_MEMSEG_COUNT for the size.
 *
 * Please resist any urge you may feel to unify virtual memory segment
 * allocation with that of other types of memory segment; the others sensibly
 * can use descriptors allocated on the heap, so need not eat into this limited
 * buffer, nor need we complicate the estimation of this buffer's size with
 * estimates of how many non-virtual segments shall be needed.
 */
static PosixMemSegment handles[POSIX_MAX_MEMORY_SEGMENTS];

# endif // OPMEMORY_VIRTUAL_MEMORY

# if 0 // def OPMEMORY_MEMORY_SEGMENT
// 2009/12/14: MortenRo says this isn't useful, at least for now.
// static
const OpMemory::OpMemSegment* OpMemory::CreateSegment(OpMemory::OpMemSegmentType type)
{
	OP_ASSERT(type == MEMORY_SEGMENT_TEMPORARY ||
			  type == MEMORY_SEGMENT_DOCUMENT ||
			  type == MEMORY_SEGMENT_PERSISTENT);
	// desirable size: >= approx 6 to 8 times MEMORY_BLOCKSIZE, may vary with type
	// falling back to a smaller size on OOM is allowed
	// when debug: fill memory with drivel *not* zeros
	return NULL;
}
# endif // OPMEMORY_MEMORY_SEGMENT

# ifdef POSMEM_SEGMENTED
static size_t get_page_size()
{
	static size_t s_page_size = 0;
	if (s_page_size == 0)
	{
		// Guarantee consistency of return value, required by GetPageSize()'s spec.
		errno = 0;
		s_page_size = sysconf(_SC_PAGESIZE);
		OP_ASSERT(errno == 0 && s_page_size > 0); // i.e. it's not -1 or 0
		// Check this agrees with the almost-synonym - NB: extra underscore !
		OP_ASSERT((long)s_page_size == sysconf(_SC_PAGE_SIZE));
		// Assorted code below assumes it's a power of two:
		OP_ASSERT(s_page_size ^ (s_page_size - 1) ^ 1 == (s_page_size -1) << 1);
	}
	return s_page_size;
}
// NB: sysconf(_SC_MAPPED_FILES) is an upper bound on number of open mmap() files.

# endif // POSMEM_SEGMENTED

# ifdef OPMEMORY_SEGMENT_BASIC
// static
void OpMemory::DestroySegment(const OpMemory::OpMemSegment* memseg)
{
	if (memseg == 0) return; // no-op
#ifdef OPMEMORY_VIRTUAL_MEMORY
	OP_ASSERT((memseg >= handles && memseg < handles + POSIX_MAX_MEMORY_SEGMENTS) ==
			  (memseg->type == MEMORY_SEGMENT_VIRTUAL));
#endif // Only an array element if of the type that uses the array.

	PosixMemSegment* const mseg = static_cast<PosixMemSegment*>(const_cast<OpMemSegment*>(memseg));
	OP_ASSERT(mseg); // Casting constness away from non-NULL should still yield non-NULL.
	switch (mseg->type)
	{
#ifdef OPMEMORY_MEMORY_SEGMENT
	case MEMORY_SEGMENT_TEMPORARY:
	case MEMORY_SEGMENT_DOCUMENT:
	case MEMORY_SEGMENT_PERSISTENT:
		OP_ASSERT(!"I don't support creating these types !");
		break;
#endif
#if defined(OPMEMORY_EXECUTABLE_SEGMENT) || defined(OPMEMORY_ALIGNED_SEGMENT)
#ifdef OPMEMORY_EXECUTABLE_SEGMENT
	case MEMORY_SEGMENT_EXECUTABLE:
#endif // OPMEMORY_EXECUTABLE_SEGMENT
#ifdef OPMEMORY_ALIGNED_SEGMENT
	case MEMORY_SEGMENT_ALIGNED:
#endif // OPMEMORY_ALIGNED_SEGMENT
		CHECKED_ALLOC_MUNMAP(mseg->address, mseg->size);
		OP_DELETE(mseg);
		break;
#endif // OPMEMORY_EXECUTABLE_SEGMENT || OPMEMORY_ALIGNED_SEGMENT
#ifdef OPMEMORY_VIRTUAL_MEMORY
	case MEMORY_SEGMENT_VIRTUAL:
		CHECKED_ALLOC_MUNMAP(mseg->address, mseg->size);
		mseg->type = MEMORY_SEGMENT_UNUSED;
		break;
#endif
#ifdef OPMEMORY_STACK_SEGMENT
	case MEMORY_SEGMENT_STACK: {
#ifdef VALGRIND
		VALGRIND_STACK_DEREGISTER(mseg->vg_stack_id);
#endif

#ifdef POSIX_STACK_USE_MALLOC
		op_free(mseg->address);
#else // POSIX_STACK_USE_MALLOC
		CHECKED_ALLOC_MUNMAP(static_cast<char *>(mseg->address) -
							 POSIX_STACK_GUARD_SIZE,
							 mseg->size + POSIX_STACK_GUARD_SIZE);
#endif // POSIX_STACK_USE_MALLOC
		OP_DELETE(mseg);
		break;
	}
#endif // OPMEMORY_STACK_SEGMENT

	default: OP_ASSERT(!"Bogus segment type; possible memory trample"); break;
	}
}
# endif // OPMEMORY_SEGMENT_BASIC

# ifdef OPMEMORY_VIRTUAL_MEMORY
/* Allegedly MAP_NORESERVE can be handled via posix_madvise, see
 * http://www.freebsd.org/cgi/man.cgi?query=posix_madvise
 * Could use MADV_FREE in place of POSIX_MADV_DONTNEED, at least on FreeBSD, but
 * it's not in POSIX (or Linux).
 */
/* Linux has MAP_ANONYMOUS with MAP_ANON as an alias; FreeBSD only has it as
 * MAP_ANON.  So use MAP_ANON.
 */
/* FIXME: Solaris ? */

// static
const OpMemory::OpMemSegment* OpMemory::CreateVirtualSegment(size_t minimum)
{
	for ( int k = 0; k < POSIX_MAX_MEMORY_SEGMENTS; k++ )
		if (handles[k].type == MEMORY_SEGMENT_UNUSED)
		{
			const size_t mask = (1<<POSIX_MEMSEG_VIRTUAL_UNIT_BITS) - 1;
			size_t size = (minimum + mask) & ~mask;

			void* ptr = ALLOC_MMAP((void*)0, size, PROT_NONE,
								   MAP_ANON | MAP_NORESERVE | // not strict POSIX
								   MAP_PRIVATE,
								   -1, 0);
			if (ptr == MAP_FAILED)
				break;

			// synonymous with MAP_NORESERVE but officially specified by POSIX:
			if (0 == posix_madvise(ptr, size, POSIX_MADV_DONTNEED))
			{
				OpMemSegment* mseg = &handles[k];
				mseg->address = ptr;
				mseg->size = size;
				mseg->type = MEMORY_SEGMENT_VIRTUAL;
				return mseg;
			}

			CHECKED_ALLOC_MUNMAP(ptr, size); // won't be using it
			break;
		}

	return NULL;
}

// static
size_t OpMemory::GetPageSize(void)
{
	return get_page_size(); // must be the same ever time we're called.
}

// static
BOOL OpMemory::VirtualAlloc(const OpMemory::OpMemSegment* mseg, void* ptr, size_t size)
{
	OP_ASSERT(mseg->type == MEMORY_SEGMENT_VIRTUAL);
	OP_ASSERT((((UINTPTR)ptr) & (GetPageSize()-1)) == 0);
	OP_ASSERT((size & (GetPageSize()-1)) == 0);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));

	// Note: Doesn't allocate new memory, just remaps existing memory,
	// thus this isn't affected by CONSTRAIN_OPERA_MEMORY_USAGE.
	void* rc = mmap(ptr, size, PROT_READ | PROT_WRITE,
					MAP_ANON | // not strict POSIX
					MAP_PRIVATE | MAP_FIXED, -1, 0);

	if (rc == MAP_FAILED)
		return FALSE;

	OP_ASSERT(rc == ptr);
	return TRUE;
}

// static
void OpMemory::VirtualFree(const OpMemory::OpMemSegment* mseg, void* ptr, size_t size)
{
	OP_ASSERT(mseg->type == MEMORY_SEGMENT_VIRTUAL);
	OP_ASSERT((((UINTPTR)ptr) & (GetPageSize()-1)) == 0);
	OP_ASSERT((size & (GetPageSize()-1)) == 0);
	OP_ASSERT((char*)mseg->address <= (char*)ptr);
	OP_ASSERT(((char*)mseg->address + mseg->size) >= ((char*)ptr + size));

	// Note: Doesn't allocate new memory, just remaps existing memory,
	// thus this isn't affected by CONSTRAIN_OPERA_MEMORY_USAGE.
#ifdef DEBUG_ENABLE_OPASSERT
	void* rc =
#endif
		mmap(ptr, size, PROT_NONE,
			 MAP_ANON | MAP_NORESERVE | // not strict POSIX
			 MAP_PRIVATE | MAP_FIXED, -1, 0);

#ifdef DEBUG_ENABLE_OPASSERT
	int ok =
#endif
		posix_madvise(ptr, size, POSIX_MADV_DONTNEED);

	OP_ASSERT(rc == ptr && 0 == ok);
}
# endif // OPMEMORY_VIRTUAL_MEMORY

# ifdef OPMEMORY_EXECUTABLE_SEGMENT // CORE-26219's clean-up of the API
#  ifdef MEMORY_SMALL_EXEC_SEGMENTS
#error "This implementation does not purport to support small memory segments; " \
	"please turn off TWEAK_MEMORY_SMALL_EXEC_SEGMENTS"
#  endif
/* TODO ? implement debug-only checking that the Write-mode limitations are
 * honoured, via some checksum of each non-write segment having not changed.
 */
const OpMemory::OpMemSegment * OpMemory::CreateExecSegment(size_t size)
{
	PosixMemSegment * block = OP_NEW(PosixMemSegment, ());
	if (!block)
		return NULL;

	const size_t mask = get_page_size() - 1;
	size = (size + mask) & ~mask;

	void *rc = ALLOC_MMAP(NULL, size, PROT_EXEC | PROT_READ | PROT_WRITE,
						  MAP_PRIVATE | MAP_ANON, -1, 0);

	if (rc == MAP_FAILED)
	{
		OP_DELETE(block);
		return NULL;
	}

	block->address = rc;
	block->size = size;
	block->type = MEMORY_SEGMENT_EXECUTABLE;
	return block;
}

void* OpMemory::WriteExec(const OpMemory::OpMemSegment* mseg, void* ptr, size_t size)
{
	if (mseg->type != MEMORY_SEGMENT_EXECUTABLE)
	{
		OP_ASSERT(!"Wrong memory segment type");
		return NULL;
	}
	// TODO: record ptr, size for later checking.
	return ptr;
}

OP_STATUS OpMemory::WriteExecDone(const OpMemory::OpMemSegment* mseg, void* ptr, size_t size)
{
	// TODO: check ptr, size match mseg's last WriteExec() call.

#ifdef POSIX_CLEARCACHE
	/* Flush instruction cache, see TWEAK_POSIX_CLEARCACHE description */
	posix_clearcache(ptr, size);
#endif

	return OpStatus::OK;
}
# endif // OPMEMORY_EXECUTABLE_SEGMENT

# ifdef OPMEMORY_STACK_SEGMENT
const OpMemory::OpMemSegment* OpMemory::CreateStackSegment(size_t size)
{
	PosixMemSegment * block = OP_NEW(PosixMemSegment, ());
	if (!block)
		return 0;

#ifdef POSIX_STACK_USE_MALLOC
	void *rc = op_malloc(size);
	if (!rc)
	{
		OP_DELETE(block);
		return NULL;
	}
#else // POSIX_STACK_USE_MALLOC
	const size_t mask = get_page_size() - 1;
	size = (size + mask) & ~mask;

	/* Allocate requested size plus one page. */
	void *rc = ALLOC_MMAP(NULL, size + POSIX_STACK_GUARD_SIZE, PROT_READ | PROT_WRITE,
						  MAP_PRIVATE | MAP_ANON, -1, 0);

	if (rc == MAP_FAILED)
	{
		OP_DELETE(block);
		return NULL;
	}

# ifdef POSIX_STACK_GUARD_PAGE
	mprotect(rc, POSIX_STACK_GUARD_SIZE, PROT_NONE);
	rc = static_cast<char *>(rc) + POSIX_STACK_GUARD_SIZE;
# endif // POSIX_STACK_GUARD_PAGE
#endif // POSIX_STACK_USE_MALLOC

	block->address = rc;
	block->size = size;
	block->type = MEMORY_SEGMENT_STACK;
#ifdef VALGRIND
	block->vg_stack_id = VALGRIND_STACK_REGISTER(rc, static_cast<char*>(rc) + size);
#endif

	return block;
}

#ifdef DEBUG_ENABLE_OPASSERT
static BOOL is_current_stack(const OpMemory::OpMemSegment* segment)
{
	int placeholder;

	/* Accurate enough for how we're about to use it. */
	const char *const stack_pointer = reinterpret_cast<const char *>(&placeholder);

	/* Segment boundaries. */
	const char *const segment_start = static_cast<const char *>(segment->address);
	const char *const segment_end = segment_start + segment->size;

	return segment_start < stack_pointer && stack_pointer < segment_end;
}
#endif // DEBUG_ENABLE_OPASSERT

void OpMemory::SignalStackSwitch(StackSwitchOperation operation,
								 const OpMemSegment* current_stack,
								 const OpMemSegment* new_stack)
{
	/* Nothing needs to be done. */

#ifdef DEBUG_ENABLE_OPASSERT
	/* Check that the arguments are sane. */

	switch (operation)
	{
	case OpMemory::STACK_SWITCH_SYSTEM_TO_CUSTOM:
		OP_ASSERT(current_stack == NULL);
		OP_ASSERT(new_stack != NULL && !is_current_stack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_BEFORE_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && is_current_stack(current_stack));
		OP_ASSERT(new_stack != NULL && !is_current_stack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_AFTER_CUSTOM_TO_CUSTOM:
		OP_ASSERT(current_stack != NULL && !is_current_stack(current_stack));
		OP_ASSERT(new_stack != NULL && is_current_stack(new_stack));
		break;

	case OpMemory::STACK_SWITCH_CUSTOM_TO_SYSTEM:
		OP_ASSERT(current_stack != NULL && !is_current_stack(current_stack));
		OP_ASSERT(new_stack == NULL);
		break;
	}
#endif // DEBUG_ENABLE_OPASSERT
}
# endif // OPMEMORY_STACK_SEGMENT

# ifdef OPMEMORY_ALIGNED_SEGMENT
const OpMemory::OpMemSegment* OpMemory::CreateAlignedSegment(size_t size, size_t alignment)
{
	PosixMemSegment * block = OP_NEW(PosixMemSegment, ());
	if (!block)
		return 0;

	const size_t mask = get_page_size() - 1;
	size = (size + mask) & ~mask;

	/* First attempt: just mmap() exactly the requested size.  Not guaranteed to
	   give a correctly aligned block, but testing has shown that it is quite
	   likely to do so. */

	void* ptr = ALLOC_MMAP(NULL, size, PROT_READ | PROT_WRITE,
						   MAP_PRIVATE | MAP_ANON, -1, 0);

	if (ptr == MAP_FAILED)
	{
		OP_DELETE(block);
		return NULL;
	}

	if ((reinterpret_cast<UINTPTR>(ptr) & (alignment - 1)) != 0)
	{
		/* Block allocated in first attempt was not properly aligned.  Free it,
		   and try again. */
		CHECKED_ALLOC_MUNMAP(ptr, size);

		/* Second attempt: allocate a block that is that is sufficiently many
		   pages larger than the requested size that we can find an
		   appropriately aligned block within the allocated block regardless of
		   the alignment of the allocated block.

		   The extra padding needed to ensure the existance of a properly
		   aligned sub-block is one page less than the alignment requirement.

		   The alignment requirement argument is guaranteed (in the API) to be a
		   power of two, and must be larger than the page size, or the
		   first-attempt allocation would have been properly aligned, so if we
		   reach this point the alignment requirement is known to be a multiple
		   of the page size, and larger than it.  The extra padding added by the
		   calculation below is thus at least one page. */

		size_t total_size = size + alignment - get_page_size();

		ptr = ALLOC_MMAP(NULL, total_size, PROT_READ | PROT_WRITE,
				 	 	 MAP_PRIVATE | MAP_ANON, -1, 0);

		if (ptr == MAP_FAILED)
		{
			OP_DELETE(block);
			return NULL;
		}

		/* Trimming: find the appropriately aligned sub-block and then munmap()
		   the extra allocated pages before and/or after it. */

		char* base = static_cast<char *>(ptr);
		char* aligned = reinterpret_cast<char *>(
			(reinterpret_cast<UINTPTR>(base) + alignment - 1) &
			~static_cast<UINTPTR>(alignment - 1));

		if (base != aligned)
		{
			CHECKED_ALLOC_MUNMAP(base, aligned - base);
		}

		if ((aligned - base) + size < total_size)
		{
			CHECKED_ALLOC_MUNMAP(aligned + size, total_size - ((aligned - base) + size));
		}

		ptr = aligned;
	}

	block->address = ptr;
	block->size = size;
	block->type = MEMORY_SEGMENT_ALIGNED;

	return block;
}
# endif // OPMEMORY_ALIGNED_SEGMENT
#endif // POSIX_OK_MEMORY
