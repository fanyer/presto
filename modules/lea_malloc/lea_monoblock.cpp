/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef LEA_MALLOC_MONOBLOCK
#include "modules/lea_malloc/lea_monoblock.h"

#define PUBLIC(type,  name) type SingleBlockHeap::name
#define PRIVATE(type, name) type SingleBlockHeap::name
#define PREDECLARE(decl)
#define IN_MONOBLOCK

// Set up appropriate #defines to make malloc.c behave desirably
#define MALLOC_FAILURE_ACTION m_oom = true
#undef assert
#define assert(x) OP_ASSERT(x)
#define MORECORE(size) MORECORE_FAILURE
#define MORECORE_CANNOT_TRIM
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#define get_malloc_state() m_state
#define malloc_getpagesize (4096)
#define LACKS_UNISTD_H
#undef USE_DL_PREFIX
#undef LEA_MALLOC_FULL_MEMAPI
#undef NEED_WIN32_SBRK
#undef DL_MALLOC_NAME
#define DL_MALLOC_NAME(nom) nom

// Turn on checking only if requested externally
#ifdef LEA_MALLOC_EXTERN_CHECK
#define LEA_MALLOC_CHECKING
#endif // #if-ery must match declarations of do_check_* in .h

// Set up macros for calling external locking functions
#ifdef USE_MALLOC_LOCK
# ifdef OPMEMORY_MALLOC_LOCK /* Use API_PI_OPMEMORY_MALLOC_LOCK */
#  include "modules/pi/system/OpMemory.h"
#  define malloc_mutex_lock() ( OpMemory::MallocLock(), 1 )
#  define malloc_mutex_unlock() OpMemory::MallocUnlock()
# elif defined(malloc_mutex_lock) && defined(malloc_mutex_unlock)
/* malloc_mutex_lock() and malloc_mutex_unlock() are properly #defined by
 * platform. Use them as is.
 */
# else /* No external locking functions found */
#  error "Failed to find external locking functions. You must either export API_PI_OPMEMORY_MALLOC_LOCK, or #define malloc_mutex_(un)lock yourself."
# endif // OPMEMORY_MALLOC_LOCK
#endif // USE_MALLOC_LOCK

#include "modules/lea_malloc/malloc.c"

static char *fixAlign(char *base, size_t off, size_t &len)
{
	INTERNAL_SIZE_T misalign = (INTERNAL_SIZE_T)chunk2mem(base + off) & MALLOC_ALIGN_MASK;
	if (misalign)
		off += MALLOC_ALIGNMENT - misalign;
	OP_ASSERT(len > off); // else you're doomed to crash.
	len -= off;
	return base + off;
}

void * SingleBlockHeap::operator new (size_t size, void *chunk)
{
	// Ensure both chunk and size are properly aligned
	OP_ASSERT(aligned_OK(chunk));
	OP_ASSERT(aligned_OK(size));
	return chunk;
}

SingleBlockHeap::SingleBlockHeap(size_t size)
	: m_state((struct malloc_state *)fixAlign((char*)this, sizeof(*this), size))
	, m_block((void*)fixAlign((char *)m_state, sizeof(struct malloc_state), size))
	, m_block_size(size)
	, m_oom(false)
{
	op_memset(m_state, 0, sizeof(struct malloc_state));
	malloc_init_state(m_state);
	/* Now fake up the effect of having done a sYSMALLOc that called
	 * MORECORE(len) and got m_block back as answer, but didn't actually use up any
	 * of that allocation on new memory.  We can exploit prior knowledge that
	 * everything is zero.  First, sanitize base and len: */
	m_state->top = (mchunkptr)m_block;
	m_state->free_size = m_state->max_total_mem = m_state->max_sbrked_mem = m_state->sbrked_mem = size;
	set_head(m_state->top, size | PREV_INUSE);
	// ... and we're ready to roll :-)
}

SingleBlockHeap::~SingleBlockHeap()
{
	OP_ASSERT(m_state->free_size == m_state->max_total_mem); // you should have free()d everything first !
}

size_t SingleBlockHeap::GetOverhead()
{
	char *base = (char *)  0;
	size_t len = (size_t) -1; // Start with an arbitrary large size
	base = fixAlign(base, sizeof(SingleBlockHeap), len); // Subtract (aligned) size of SingleBlockHeap
	base = fixAlign(base, sizeof(struct malloc_state), len); // Subtract (aligned) size of malloc_state
	len = (size_t) -1 - len; // Calculate how much len was altered
	OP_ASSERT(len == (size_t)(base - (char *)0)); // Ensure base was moved by the same amount as len
	return len;
}

size_t SingleBlockHeap::GetCapacity() const
{
	return m_block_size;
}

size_t SingleBlockHeap::GetFreeMemory() const
{
	return m_state->free_size;
}

bool SingleBlockHeap::Contains(void *ptr) const
{
	const char *const max_address = (char *)(m_state->top) + chunksize(m_state->top);
	const char *const min_address = max_address - m_state->sbrked_mem;
	return (const char *)ptr >= min_address && (const char *)ptr < max_address;
}

# ifdef LEA_MALLOC_EXTERN_CHECK
void SingleBlockHeap::CheckState()
{
	do_check_malloc_state();
}
# endif // LEA_MALLOC_EXTERN_CHECK
#endif // LEA_MALLOC_MONOBLOCK
