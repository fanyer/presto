/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement support for memory guard functionality
 *
 */

#include "core/pch.h"

// For debugging the memory debugging itself:
// #define MEMORY_MEMDEBUG_DEBUGGING

#ifdef ENABLE_MEMORY_OOM_EMULATION
#include "modules/lea_malloc/lea_monoblock.h"
#endif

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_memguard.h"
#include "modules/memory/src/memory_events.h"
#include "modules/memory/src/memory_reporting.h"
#include "modules/memory/src/memory_state.h"
#include "modules/memory/src/memory_upperten.h"
#include "modules/memory/src/memory_mmap.h"
#include "modules/memory/src/memory_manager.h"
#include "modules/pi/system/OpMemory.h"

#ifdef MEMORY_USE_CODELOC
#  include "modules/memtools/memtools_codeloc.h"
#endif

#ifdef VALGRIND
#  include "modules/memory/src/memory_valgrind.h"
#  include "modules/memory/src/memory_valgrind_memcheck.h"
#endif

#ifdef MEMORY_USE_LOCKING
#  include "modules/pi/system/OpMemory.h"
#  define MUTEX_LOCK() OpMemory::MemdebugLock()
#  define MUTEX_UNLOCK() OpMemory::MemdebugUnlock()
#else
#  define MUTEX_LOCK() do {} while (0)
#  define MUTEX_UNLOCK() do {} while (0)
#endif

void* OpMemGuard::OpMalloc(size_t size, OpAllocInfo* ai)
{
	void* ptr;

	MUTEX_LOCK();

	if ( !random_oom || ((op_rand()>>16) & 0x7f) != 0 )
	{
		//
		// Don't fake a random OOM at this time
		//
#ifdef ENABLE_MEMORY_OOM_EMULATION
		//
		// But check if there is a limited heap that overflows
		//
		void* shadow = AllocateShadow(size, ai);

		if ( shadow == 0 && g_mem_limited_heap != 0 )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_OOM, ai, 0, 0, size);
			OpMemEventListener::DeliverMemoryEvent(&mev);
			ptr = 0;
		}
		else
		{
			ptr = Allocate(size, ai, shadow);
		}
#else
		ptr = Allocate(size, ai, 0);
#endif
	}
	else
	{
		//
		// Fake a random OOM
		//
		OpMemEvent mev(MEMORY_EVENTCLASS_OOM, ai, 0, 0, size);
		OpMemEventListener::DeliverMemoryEvent(&mev);
		ptr = 0;
	}

	MUTEX_UNLOCK();

	if ( ptr != 0 )
		InstallGuard((char*)ptr, size, MEMORY_NEW_PATTERN);

	return ptr;
}

#ifdef ENABLE_MEMORY_OOM_EMULATION

void *OpMemGuard::AllocateShadow(size_t size, OpAllocInfo* ai)
{
#ifdef USE_POOLING_MALLOC
	switch ( ai->GetFlags() & MEMORY_FLAG_ALLOCATED_MASK )
	{
	case MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD:
		return OpMemPoolManager::AllocSMO(size);
	case MEMORY_FLAG_ALLOCATED_OP_NEWSMOT:
		return OpMemPoolManager::AllocSMO(size, MEMPOOL_ALLOC_TEMPORARY);
	case MEMORY_FLAG_ALLOCATED_OP_NEWSMOP:
		return OpMemPoolManager::AllocSMO(size, MEMPOOL_ALLOC_PERSISTENT);
	}
#endif

	if ( g_mem_limited_heap != 0 )
		return g_mem_limited_heap->malloc(size);

	return 0;
}

#endif // ENABLE_MEMORY_OOM_EMULATION

void* OpMemGuard::Allocate(size_t size, OpAllocInfo* ai, void* ca)
{
	void* ptr;
	OpMemGuardInfo* mgi;

	// Handle OOM emulation (from e.g. selftests)
	if ( oom_countdown > 1 )
		oom_countdown--;
	else if ( oom_countdown == 1 )
		return 0;

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	OpSrcSite* site = OpSrcSite::GetSite(ai->GetSiteId());
	site->IncCallCount();
	if ( IsOOMFailEnabled() && !site->HasFailed() && size>GetOOMFailThreshold() || callnumber==site->GetCallCount() )
	{
		// The bigger the allocation the higher chance of failing.
		// It could be refined to yield more real life appearance once such refinement is found:
		if ( op_rand() % 100 <= DEFAULT_BASE_FAIL_CHANCE + (int)(size/1024) )
		{
			dbg_printf("OpMemGuard::Allocate() faking OOM at %s:%d size %d (call # %d) (threshold %d)\n", site->GetFile(), site->GetLine(), size, site->GetCallCount(), GetOOMFailThreshold());
			OpMemDebug_PrintCallstack();
			site->SetHasFailed();
			return 0;
		}
	}
	callnumber = 0;
#endif // MEMORY_FAIL_ALLOCATION_ONCE

	//
	// Decide if allocation is to be made using electric fence or not,
	// and allocate for the total 'area' needed (guard-bytes, info and
	// allocation itself).
	//
	BOOL use_efence = UseEfence(size, ai);
	size_t needed = GetTotalSize(size, use_efence);
	BOOL overflow = needed < size;
	void* area = 0;

#ifdef MEMORY_ELECTRIC_FENCE
	if ( !overflow && use_efence )
	{
		area = efence_malloc(needed);
		if ( area == 0 )
		{
			// We failed to use electric fence, try to revert back to an
			// ordinary allocation insted.
			use_efence = FALSE;
			needed = GetTotalSize(size, use_efence); // Get non-efence size
			overflow = needed < size;
			if ( !overflow )
				area = op_lowlevel_malloc(needed);
		}
	}
	else if ( !overflow )
#endif // MEMORY_ELECTRIC_FENCE
	{
		// We end up here when not using electic fence for allocation.
		area = op_lowlevel_malloc(needed);
	}

	if ( area != 0 )
	{
		mgi = SetupGuard(area, size, use_efence);

		mgi->object = ai->GetObject();

#if MEMORY_KEEP_ALLOCSITE
		mgi->allocsiteid = ai->GetSiteId();
#endif

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
		OpSrcSite* site = OpSrcSite::GetSite(ai->GetSiteId());
		site->IncMemUsed(size);
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
#ifdef MEMORY_FAIL_ALLOCATION_ONCE
		mgi->tag = currenttag;
#endif // MEMORY_FAIL_ALLOCATION_ONCE

#if MEMORY_KEEP_ALLOCSTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
		mgi->allocstack_new = ai->GetOpCallStack(MEMORY_KEEP_ALLOCSTACK);
#  else
		op_memcpy(mgi->allocstack, ai->GetStack(),
				  MEMORY_KEEP_ALLOCSTACK * sizeof(UINTPTR));
#  endif
#endif

#ifdef ENABLE_MEMORY_OOM_EMULATION
		mgi->constrained_allocation = ca;
#else
		(void)ca; // Remove warning
#endif

		mgi->bits.flags |= (ai->GetFlags() | initial_flags);
		if ( use_efence )
			mgi->bits.flags |= MEMORY_FLAG_EFENCE_ALLOCATION;

		mgi->InsertBefore(&live_allocations);

		ptr = InfoToAddr(mgi);

#ifdef MEMORY_MEMDEBUG_DEBUGGING
		dbg_printf("OpMemGuard::OpMalloc(%d) => %p[%p] %s:%d\n",
				   size, ptr, ca, file, line);
#endif
		if ( log_allocations )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_ALLOCATE, ai, mgi);
			OpMemEventListener::DeliverMemoryEvent(&mev);
		}
	}
	else // OOM or overflow
	{
		OpMemEvent mev(MEMORY_EVENTCLASS_OOM, ai, 0, 0, size);
		OpMemEventListener::DeliverMemoryEvent(&mev);
		ptr = 0;
	}

	return ptr;
}

void* OpMemGuard::OpRealloc(void* ptr, size_t size, OpAllocInfo* ai)
{
	if ( ptr == 0 )
	{
		// Calling realloc with a NULL pointer is the same as calling malloc
		ai->SetFlags(MEMORY_FLAG_ALLOCATED_OP_MALLOC);
		return OpMalloc(size, ai);
	}

	if ( size == 0 )
	{
		// Calling realloc with a zero size is the same as calling free
		ai->SetFlags(MEMORY_FLAG_RELEASED_OP_FREE);
		OpFree(ptr, ai);
		return 0;
	}

	MUTEX_LOCK();

#ifdef MEMORY_MEMDEBUG_DEBUGGING
	dbg_printf("OpMemGuard::OpRealloc(%p, %d) ", ptr, size);
#endif

	OpMemGuardInfo* mgi = AddrToInfo(ptr);

	void* ptr2 = 0;

	if ( CheckIntegrity(mgi, ai) )
	{
		// mem_printf("OpMemGuard::OpRealloc flags=%x\n", info->flags);

		if ( mgi->bits.flags & MEMORY_FLAG_RELEASED_MASK )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_DOUBLEFREE, ai, mgi);
			OpMemEventListener::DeliverMemoryEvent(&mev);
			goto done;
		}

		switch ( mgi->bits.flags & MEMORY_FLAG_ALLOCATED_MASK )
		{
		case MEMORY_FLAG_ALLOCATED_OP_MALLOC:
		case MEMORY_FLAG_ALLOCATED_OP_CALLOC:
		case MEMORY_FLAG_ALLOCATED_OP_STRDUP:
		case MEMORY_FLAG_ALLOCATED_UNI_STRDUP:
			// These are all OK allocations to perform a realloc() on
			break;
 
		default:
			{
				// Oops, this allocation does not permit reallocation
				OpMemEvent mev(MEMORY_EVENTCLASS_MISMATCH, ai, mgi);
				OpMemEventListener::DeliverMemoryEvent(&mev);
				break;
			}
		}

#ifdef ENABLE_MEMORY_OOM_EMULATION
		//
		// With a constrained heap, reallocate with the correct sizes,
		// to decide if reallocation succeeded or not.
		//
		void* captr = mgi->constrained_allocation;
		void* ca = 0;

		if ( captr != 0 )
		{
			ca = g_mem_limited_heap->realloc(captr, size);

			if ( ca == 0 )
			{
				OpMemEvent mev(MEMORY_EVENTCLASS_OOM, ai, 0, mgi, size);
				mgi->bits.flags |= MEMORY_FLAG_REALLOCATED_FAILURE;
				OpMemEventListener::DeliverMemoryEvent(&mev);
				goto done;
			}
		}
#endif

		//
		// For reallocations, inherit the electric fence status from
		// original allocation, but also promote allocation into
		// electric fence status if 'UseEfence()' function says that
		// we should (e.g. based on where the realloc is located, or
		// the size for the realloc).
		//
		BOOL use_efence = FALSE;
		if ( (mgi->bits.flags & MEMORY_FLAG_EFENCE_ALLOCATION) ||
			 UseEfence(size, ai) )
			use_efence = TRUE;

		size_t needed = GetTotalSize(size, use_efence);
		BOOL overflow = needed < size;
		void* area = 0;

#ifdef MEMORY_ELECTRIC_FENCE
		if ( !overflow && use_efence )
		{
			area = efence_malloc(needed);
			if ( area == 0 )
			{
				// Efence failed, try ordinary
				use_efence = FALSE;
				needed = GetTotalSize(size, use_efence);
				overflow = needed < size;
				if ( !overflow )
					area = op_lowlevel_malloc(needed);
			}
		}
		else if ( !overflow )
#endif // MEMORY_ELECTRIC_FENCE
		{
			// Not using electric fence for reallocation
			area = op_lowlevel_malloc(needed);
		}

		if ( area == 0 ) // OOM or overflow
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_OOM, ai, 0, mgi, size);
			mgi->bits.flags |= MEMORY_FLAG_REALLOCATED_FAILURE;
			OpMemEventListener::DeliverMemoryEvent(&mev);
			goto done;
		}

		OpMemGuardInfo* mgi2 = SetupGuard(area, size, use_efence);
		ptr2 = InfoToAddr(mgi2);
		mgi2->bits.flags = mgi->bits.flags & ~(MEMORY_FLAG_MARKER1+MEMORY_FLAG_MARKER2+MEMORY_FLAG_EFENCE_ALLOCATION);
		if ( mgi2->bits.flags & MEMORY_FLAG_IS_REALLOCATED )
			mgi2->bits.flags |= MEMORY_FLAG_REALLOCATED_MULTIPLE;
		if ( use_efence )
			mgi2->bits.flags |= MEMORY_FLAG_EFENCE_ALLOCATION;

		mgi2->object = mgi->object;

#if MEMORY_KEEP_FREESITE
		// The memory vacated by the realloc operation is 'freed'
		mgi->freesiteid = ai->GetSiteId();
#endif

#if MEMORY_KEEP_FREESTACK > 0
		// Keep the call-stack for the 'freed' allocation
#  ifdef MEMORY_CALLSTACK_DATABASE
		mgi->freestack_new = ai->GetOpCallStack(MEMORY_KEEP_FREESTACK);
#  else
		op_memcpy(mgi->freestack, ai->GetStack(),
				  MEMORY_KEEP_FREESTACK * sizeof(UINTPTR));
#  endif
#endif

#if MEMORY_KEEP_ALLOCSITE
		// Keep the original allocation site accross reallocs
		mgi2->allocsiteid = mgi->allocsiteid;
#endif

#if MEMORY_KEEP_ALLOCSTACK > 0
		// Keep the original allocation call-stack accross reallocs
#  ifdef MEMORY_CALLSTACK_DATABASE
		mgi2->allocstack_new = mgi->allocstack_new;
#  else
		op_memcpy(mgi2->allocstack, mgi->allocstack,
				  MEMORY_KEEP_ALLOCSTACK * sizeof(UINTPTR));
#  endif
#endif

#if MEMORY_KEEP_REALLOCSITE
		// Log this reallocation as the last realloc of the allocation
		mgi2->reallocsiteid = ai->GetSiteId();
#endif

#if MEMORY_KEEP_REALLOCSTACK > 0
		// Keep the call-stack leading up to this reallocation
#  ifdef MEMORY_CALLSTACK_DATABASE
		mgi2->reallocstack_new = ai->GetOpCallStack(MEMORY_KEEP_REALLOCSTACK);
#  else
		op_memcpy(mgi2->reallocstack, ai->GetStack(),
				  MEMORY_KEEP_REALLOCSTACK * sizeof(UINTPTR));
#  endif
#endif

#ifdef ENABLE_MEMORY_OOM_EMULATION
		// The new allocation will carry the constrained allocation forward
		mgi2->constrained_allocation = ca;
#endif

		size_t copy_bytes;
		size_t fresh_bytes;

		if ( size > mgi->size )
		{
			mgi2->bits.flags |= MEMORY_FLAG_REALLOCATED_GROWN;
			copy_bytes = mgi->size;
			fresh_bytes = size - copy_bytes;
		}
		else
		{
			if ( size == mgi->size )
				mgi2->bits.flags |= MEMORY_FLAG_REALLOCATED_SAME;
			else
				mgi2->bits.flags |= MEMORY_FLAG_REALLOCATED_SHRUNK;
			copy_bytes = size;
			fresh_bytes = 0;
		}

		op_memcpy(ptr2, ptr, copy_bytes);

		if ( fresh_bytes > 0 )
		{
			char* zap = reinterpret_cast<char*>(ptr2);
			zap += copy_bytes;
			InstallGuard(zap, fresh_bytes, MEMORY_NEW_PATTERN);
		}

		// The new allocation is in place, and the old one is still not
		// smashed; deliver the realloc event at this time:
		if ( log_allocations )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_ALLOCATE, ai, mgi2, mgi);
			OpMemEventListener::DeliverMemoryEvent(&mev);
		}

		// Add the new allocation to the live list
		mgi2->InsertBefore(&live_allocations);

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
		OpSrcSite* site = OpSrcSite::GetSite(mgi->allocsiteid);
		site->DecMemUsed(mgi->size);
		site->IncMemUsed(size);
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE

		// Retire the old allocation, including zapping its contents
		mgi->bits.flags |= MEMORY_FLAG_RELEASED_REALLOCATED;
		Retire(mgi);

		// Free up some memory if the dead-list is growing large
		RecycleDeadAllocations();

#ifdef MEMORY_MEMDEBUG_DEBUGGING
		dbg_printf("=> %p\t%s:%d\n", ptr2, file, line);
#endif
	}

 done:

	MUTEX_UNLOCK();

	return ptr2;
}

void OpMemGuard::OpFree(void* ptr, OpAllocInfo* ai)
{
#ifdef MEMORY_MEMDEBUG_DEBUGGING
	dbg_printf("OpMemGuard::OpFree(%p)\t%s:%d\n", ptr, file, line);
#endif

	if ( ptr == 0 )
		return;

	MUTEX_LOCK();

	OpMemGuardInfo* mgi = AddrToInfo(ptr);
	if ( CheckIntegrity(mgi, ai) )
	{
		if ( mgi->bits.flags & MEMORY_FLAG_RELEASED_MASK )
		{
			// Ooops, allready released it seems...
			OpMemEvent mev(MEMORY_EVENTCLASS_DOUBLEFREE, ai, mgi);
			OpMemEventListener::DeliverMemoryEvent(&mev);
		}
		else
		{
			mgi->bits.flags |= ai->GetFlags();

#if MEMORY_KEEP_FREESITE
			// Overwrite previously undefined, OOM, error and unknown sites
			if ( mgi->freesiteid <= 3 )
				mgi->freesiteid = ai->GetSiteId();

			if ( tracked_delete_ptr != 0 )
			{
				char* start = (char*)ptr;
				char* end = start + mgi->size;
				char* track = (char*)tracked_delete_ptr;

				if ( mgi->freesiteid <= 3 && start <= track && track < end )
				{
					OpSrcSite* site;
					site = OpSrcSite::GetSite(tracked_delete_file,
											  tracked_delete_line);
					mgi->freesiteid = site->GetId();
				}

				tracked_delete_ptr = 0;
			}
#endif // MEMORY_KEEP_FREESITE

#if MEMORY_KEEP_FREESTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
			mgi->freestack_new = ai->GetOpCallStack(MEMORY_KEEP_FREESTACK);
#  else
			op_memcpy(mgi->freestack, ai->GetStack(),
					  MEMORY_KEEP_FREESTACK * sizeof(UINTPTR));
#  endif
#endif

			if ( ! IsMatchingFree(mgi->bits.flags) )
			{
				OpMemEvent mev(MEMORY_EVENTCLASS_MISMATCH, ai, mgi);
				OpMemEventListener::DeliverMemoryEvent(&mev);
			}

			mgi->bits.flags |= ai->GetFlags();

			if ( log_allocations )
			{
				OpMemEvent mev(MEMORY_EVENTCLASS_FREE, ai, mgi);
				OpMemEventListener::DeliverMemoryEvent(&mev);
			}

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
			OpSrcSite* site = OpSrcSite::GetSite(mgi->allocsiteid);
			site->DecMemUsed(mgi->size);
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE

#ifdef ENABLE_MEMORY_OOM_EMULATION
			if ( mgi->constrained_allocation )
			{
				FreeShadow(mgi->constrained_allocation, ai);
				mgi->constrained_allocation = 0;
			}
#endif // ENABLE_MEMORY_OOM_EMULATION

			// Retire allocation to deadlist, or inaccessible memory
			Retire(mgi);

			// Free up some memory if the dead-list is growing large
			RecycleDeadAllocations();
		}
	}

	MUTEX_UNLOCK();
}

void OpMemGuard::Retire(OpMemGuardInfo* mgi)
{
	mgi->Unlink();

#ifdef MEMORY_ELECTRIC_FENCE
	if ( mgi->bits.flags & MEMORY_FLAG_EFENCE_ALLOCATION )
	{
		void* area = InfoToArea(mgi);
		efence_free(area);
		// TODO: Add this efence allocation to a recycle queue,
		// so we can recycle efence memory too (if we want to),
		// or so we can identify what was previously in efence
		// memory.
	}
	else
#endif
	{
		// Move this allocation to the dead list
		void* ptr = InfoToAddr(mgi);
		InstallGuard((char*)ptr, mgi->size, MEMORY_FREE_PATTERN);
		mgi->InsertBefore(&dead_allocations);
		count_dead_allocations++;
		bytes_dead_allocations += mgi->size;
	}
}

#ifdef ENABLE_MEMORY_OOM_EMULATION

void OpMemGuard::FreeShadow(void* ptr, OpAllocInfo* ai)
{
#ifdef USE_POOLING_MALLOC
	if ( (ai->GetFlags() & MEMORY_FLAG_RELEASED_MASK) 
		 == MEMORY_FLAG_RELEASED_POOLED_DELETE )
	{
		g_mem_poolmgr->Free(ptr);
		return;
	}
#endif // USE_POOLING_MALLOC

	g_mem_limited_heap->free(ptr);
}

#endif // ENABLE_MEMORY_OOM_EMULATION

#if MEMORY_KEEP_FREESITE

void OpMemGuard::TrackDelete(const void* ptr, const char* file, int line)
{
	if ( ptr != 0 )
	{
		MUTEX_LOCK();
		tracked_delete_ptr = ptr;
		tracked_delete_file = file;
		tracked_delete_line = line;
		MUTEX_UNLOCK();
	}
}

void OpMemGuard::TrackDeleteA(const void* ptr, const char* file, int line)
{
	TrackDelete(ptr, file, line);  // Same for the time being
}

#endif // MEMORY_KEEP_FREESITE


void* OpMemGuard::ShadowAlloc(void* ptr, size_t size, OpAllocInfo* info)
{
	if ( ptr == 0 )
		return 0;

	MUTEX_LOCK();
	void* ret = Allocate(size, info, ptr);
	MUTEX_UNLOCK();

	return ret;
}

BOOL OpMemGuard::IsMatchingFree(UINT32 flags)
{
	UINT32 mask = MEMORY_FLAG_ALLOCATED_MASK | MEMORY_FLAG_RELEASED_MASK;
	switch ( flags & mask )
	{
	case MEMORY_FLAG_ALLOCATED_OP_MALLOC + MEMORY_FLAG_RELEASED_OP_FREE:
	case MEMORY_FLAG_ALLOCATED_OP_CALLOC + MEMORY_FLAG_RELEASED_OP_FREE:
	case MEMORY_FLAG_ALLOCATED_OP_STRDUP + MEMORY_FLAG_RELEASED_OP_FREE:
	case MEMORY_FLAG_ALLOCATED_UNI_STRDUP + MEMORY_FLAG_RELEASED_OP_FREE:
		// These are all OK regular malloc/free allocations+deallocations
		break;

	case MEMORY_FLAG_ALLOCATED_OP_NEW + MEMORY_FLAG_RELEASED_DELETE:
	case MEMORY_FLAG_ALLOCATED_OP_NEWA + MEMORY_FLAG_RELEASED_DELETEA:
	case MEMORY_FLAG_ALLOCATED_GLOBAL_NEW + MEMORY_FLAG_RELEASED_DELETE:
	case MEMORY_FLAG_ALLOCATED_GLOBAL_NEWA + MEMORY_FLAG_RELEASED_DELETEA:
		// Regular new/delete and new[]/delete[] operations
		break;

	case MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD+MEMORY_FLAG_RELEASED_POOLED_DELETE:
	case MEMORY_FLAG_ALLOCATED_OP_NEWSMOP+MEMORY_FLAG_RELEASED_POOLED_DELETE:
	case MEMORY_FLAG_ALLOCATED_OP_NEWSMOT+MEMORY_FLAG_RELEASED_POOLED_DELETE:
		// Pooling new/delete
		break;

	default:
		// Ooops, this allocation+deallocation is invalid
		return FALSE;
	}

	return TRUE;
}


void* OpMemGuard::Real2Shadow(void* ptr)
{
#ifdef ENABLE_MEMORY_OOM_EMULATION

	if ( ptr != 0 )
	{
		OpMemGuardInfo* mgi = AddrToInfo(ptr);

		if ( CheckIntegrity(mgi) )
		{
			// OK, memory looks healthy, look at the shadow allocation
			if ( mgi->constrained_allocation != 0 )
				return mgi->constrained_allocation;
		}
	}
#endif

	return ptr;
}

OpMemGuard::OpMemGuard(void) :
	count_dead_allocations(0),
	bytes_dead_allocations(0),
	random_oom(FALSE),
	log_allocations(FALSE),
	oom_countdown(0),
	initial_flags(0),
	tracked_delete_ptr(0)
#ifdef MEMORY_ELECTRIC_FENCE
	, mmap_current(-1)
#endif
#ifdef MEMORY_FAIL_ALLOCATION_ONCE
	, failenabled(false)
	, oomfailthreshold(DEFAULT_OOM_FAIL_THRESHOLD)
	, currenttag(0)
	, callnumber(0)
#endif // MEMORY_FAIL_ALLOCATION_ONCE
{
	// Set up the anchors as a circular double linked list
	live_allocations.prev = &live_allocations;
	live_allocations.next = &live_allocations;
	dead_allocations.prev = &dead_allocations;
	dead_allocations.next = &dead_allocations;

	// This object is leaked for the time being, since this will allow
	// the reporting object to keep reporting until the very end.
	reporting = new OpMemReporting;

#ifdef MEMORY_ELECTRIC_FENCE
	memory_page_size = OpMemory::GetPageSize();
	memory_page_mask = memory_page_size - 1;
#endif

	OP_ASSERT((MEMORY_EXTERNAL_GUARD_SIZE & (MEMORY_ALIGNMENT-1)) == 0);
	OP_ASSERT((MEMORY_FRONT_GUARD_SIZE & (MEMORY_ALIGNMENT-1)) == 0);
	OP_ASSERT((MEMORY_REAR_GUARD_SIZE & 3) == 0);
}

void* OpMemGuard::operator new(size_t size) OP_NOTHROW
{
	return op_lowlevel_malloc(size);
}

void OpMemGuard::Destroy(void)
{
}

void* OpMemGuard::InfoToAddr(const OpMemGuardInfo* info) const
{
	char* ptr = reinterpret_cast<char*>(const_cast<OpMemGuardInfo*>(info));
	ptr += sizeof(OpMemGuardInfo);
	ptr += MEMORY_FRONT_GUARD_SIZE;
	return reinterpret_cast<void*>(ptr);
}

void* OpMemGuard::InfoToArea(OpMemGuardInfo* info)
{
	char* ptr = reinterpret_cast<char*>(info);
	ptr -= MEMORY_EXTERNAL_GUARD_SIZE;
	return reinterpret_cast<void*>(ptr);
}

OpMemGuardInfo* OpMemGuard::AddrToInfo(const void* ptr)
{
	char* info = reinterpret_cast<char*>(const_cast<void*>(ptr));
	info -= sizeof(OpMemGuardInfo);
	info -= MEMORY_FRONT_GUARD_SIZE;
	return reinterpret_cast<OpMemGuardInfo*>(info);
}

//
// Rear guard has variable size, depending on padding and electric fence
//
size_t OpMemGuard::GetRearGuardSize(size_t size, BOOL use_efence)
{
	if ( use_efence )
	{
		//
		// With Efence enabled for an allocation, the very first byte past
		// the allocation is on an inaccessible memory page.
		//
		// This means that all allocations will be done with an alignment
		// equal to the size of the allocation. This means that e.g.
		// OP_NEWA(char, 1) will be an odd valued pointer.
		//
		// This assumes that allocation of e.g. a structure that requires
		// 8-byte alignment is also 8 bytes in size. This would typically
		// hold true for all ordinary objects allocated, but a hand-crafted
		// allocation of e.g. a 'double' and a string might get unsuitable
		// alignment for the double, e.g.:
		//
		// double* ptr = op_malloc(sizeof(double) + op_strlen(name) + 1);
		// ptr[0] = 42.0;
		// op_strcpy(&ptr[1], name);
		//
		// Would have an allocation size, and therefore also alignment,
		// determined by the length of 'name'. This is unfortunate but
		// probably not a problem since most Efence use will be targetted
		// towards specific objects (with size suitable for alignment).
		//

		// TODO: Efence alignment must be configurable
		return 0;
	}
	else
	{
		//
		// Without electric fence, the rear-guard bytes has only 4-byte
		// alignment requirements
		//
		size_t mask = (~(size_t)0) - 3;
		return ((size + 3) & mask) - size + MEMORY_REAR_GUARD_SIZE;
	}
}

OpMemGuardInfo* OpMemGuard::SetupGuard(void* area, size_t size, BOOL use_efence)
{
	char* p = reinterpret_cast<char*>(area);

	if ( MEMORY_EXTERNAL_GUARD_SIZE > 0 )
	{
		InstallGuard(p, MEMORY_EXTERNAL_GUARD_SIZE, MEMORY_EXTERNAL_PATTERN);
		p += MEMORY_EXTERNAL_GUARD_SIZE;
	}

	OpMemGuardInfo* info = reinterpret_cast<OpMemGuardInfo*>(p);
	info->bits.magic = MEMORY_MEMGUARD_HEADER_MAGIC;
	info->bits.flags = 0;
	info->prev = 0;
	info->next = 0;
#if MEMORY_KEEP_ALLOCSITE
	info->allocsiteid = 0;
#endif
#if MEMORY_KEEP_REALLOCSITE
	info->reallocsiteid = 0;
#endif
#if MEMORY_KEEP_FREESITE
	info->freesiteid = 0;
#endif
	info->size = size;

	p += sizeof(OpMemGuardInfo);

	if ( MEMORY_FRONT_GUARD_SIZE > 0 )
	{
		InstallGuard(p, MEMORY_FRONT_GUARD_SIZE, MEMORY_FRONT_PATTERN);
		p += MEMORY_FRONT_GUARD_SIZE;
	}

	size_t rgs = GetRearGuardSize(size, use_efence);

	if ( rgs > 0 )
	{
		p += size;
		InstallGuard(p, rgs, MEMORY_REAR_PATTERN);
	}

	return info;
}

size_t OpMemGuard::GetTotalSize(size_t size, BOOL use_efence)
{
	//
	// This method does NOT detect overflow. The caller is responsible
	// for checking if overflow occured, typically by checking if the
	// returned size is smaller than the given size.
	//
	size_t total = MEMORY_EXTERNAL_GUARD_SIZE
		+ MEMORY_FRONT_GUARD_SIZE +
		+ sizeof(OpMemGuardInfo);

	total += size;
	total += GetRearGuardSize(size, use_efence);

	return total;
}

void OpMemGuard::RecycleDeadAllocation(void)
{
	OpMemGuardInfo* anchor = &dead_allocations;
	OpMemGuardInfo* mgi = anchor->next;

	OP_ASSERT(mgi != anchor);
	OP_ASSERT(count_dead_allocations > 0);
	OP_ASSERT(bytes_dead_allocations >= mgi->size);

	BOOL rc = CheckIntegrity(mgi);

	//
	// Try to unlink the allocation, even if it is currupted (so we don't
	// get stuck on this object later)
	//
	mgi->Unlink();
	count_dead_allocations--;
	
	if ( rc )
	{
		//
		// Everything looks fine, update counters and relase memory
		// to system heap for reuse.
		//
		bytes_dead_allocations -= mgi->size;
		void* ptr = InfoToArea(mgi);

#ifdef VALGRIND
		//
		// Before the allocation is recycled back into the underlying
		// heap, the memory-access status in valgrind must be updated,
		// so that the allocator itself or the next user of the memory
		// does not stumble upon memory-access protection status that
		// triggers valgrind incorrectly.
		//
		// The contents of the allocation can be set to undefined,
		// since neither the allocator, nor the next user of the
		// memory, should depend on what was previously in memory.
		//

		//
		// Get the full size of the allocation including guards and
		// info etc., and make it undefined. This can't be an
		// Electric fence allocation; they don't end up on the
		// dead_allocations list.
		//
		if ( RUNNING_ON_VALGRIND )
		{
			size_t total = GetTotalSize(mgi->size, FALSE);
			OP_ASSERT(total >= mgi->size);
			VALGRIND_MAKE_MEM_UNDEFINED(ptr, total);
		}
#endif

		op_lowlevel_free(ptr);
	}
}

void OpMemGuard::RecycleDeadAllocations(void)
{
	if ( count_dead_allocations > MEMORY_DEAD_OBJECT_QUEUE )
	{
		// Keep number of dead allocations below this.  Make sure to free
		// many blocks at once, as this will be less demanding on lea_malloc
		// in terms of CPU usage (i.e. do_check_inuse_chunk, do_check_chunk
		// and do_check_free_chunk).

		int count = MEMORY_DEAD_OBJECT_RECYCLE;
		for ( int k = 0; k < count; k++ )
			RecycleDeadAllocation();
	}

	if ( bytes_dead_allocations > MEMORY_DEAD_BYTES_QUEUE )
	{
		// The ammount of bytes in dead allocations is larger than the
		// limit, prune the dead-queue until it is below the hysteresis
		// threshold.

		unsigned int bytes_threshold = MEMORY_DEAD_BYTES_QUEUE -
			MEMORY_DEAD_BYTES_RECYCLE;
		while ( bytes_dead_allocations > bytes_threshold )
			RecycleDeadAllocation();
	}
}

BOOL OpMemGuard::CheckIntegrity(OpMemGuardInfo* info, OpAllocInfo* ai)
{
	char* p = reinterpret_cast<char*>(info);
	BOOL ret = TRUE;
	BOOL use_efence = FALSE;

	if ( info->bits.magic != MEMORY_MEMGUARD_HEADER_MAGIC )
	{
		OpMemEvent mev(MEMORY_EVENTCLASS_CORRUPT_EXTERN, ai, info);
		OpMemEventListener::DeliverMemoryEvent(&mev);
		ret = FALSE;
	}

	if ( MEMORY_EXTERNAL_GUARD_SIZE > 0 )
	{
		if ( ! CheckGuard(p - MEMORY_EXTERNAL_GUARD_SIZE,
						  MEMORY_EXTERNAL_GUARD_SIZE,
						  MEMORY_EXTERNAL_PATTERN) )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_CORRUPT_EXTERN, ai, info);
			OpMemEventListener::DeliverMemoryEvent(&mev);
			ret = FALSE;
		}
	}

	p += sizeof(OpMemGuardInfo);

	if ( MEMORY_FRONT_GUARD_SIZE > 0 )
	{
		if ( ! CheckGuard(p, MEMORY_FRONT_GUARD_SIZE,
						  MEMORY_FRONT_PATTERN) )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_CORRUPT_BELOW, ai, info);
			OpMemEventListener::DeliverMemoryEvent(&mev);
			ret = FALSE;
		}

		p += MEMORY_FRONT_GUARD_SIZE;
	}

	if ( info->bits.flags & MEMORY_FLAG_EFENCE_ALLOCATION )
		use_efence = TRUE;

	size_t rgs = GetRearGuardSize(info->size, use_efence);

	if ( rgs > 0 )
	{
		p += info->size;
		if ( ! CheckGuard(p, rgs, MEMORY_REAR_PATTERN) )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_CORRUPT_ABOVE, ai, info);
			OpMemEventListener::DeliverMemoryEvent(&mev);
			ret = FALSE;
		}
	}

	if ( info->bits.flags & MEMORY_FLAG_RELEASED_MASK )
	{
		// Allocation is released, check if dead pattern is intact
		char* ptr = reinterpret_cast<char*>(InfoToAddr(info));
		if ( ! CheckGuard(ptr, info->size, MEMORY_FREE_PATTERN) )
		{
			OpMemEvent mev(MEMORY_EVENTCLASS_LATEWRITE, ai, info);
			OpMemEventListener::DeliverMemoryEvent(&mev);
			ret = FALSE;
		}
	}

	return ret;
}

//
// Use a linear congruential generator to create (parts) of the guard
// bytes. The random number generator is initialized with a seed based
// on the address of the first byte in the guard block.
//
// The guard words has a format of 0xPwPxPyPz where 'P' is the pattern
// value (fixed value nibbles), and w,x,y,z are the random nibbles.
//
// The high order bits of the LCG is used to create the random nibbles:
//
// w: bits 30-27
// y: bits 26-23 XOR bits 14-11
// x: bits 22-19
// z: bits 18-15 XOR bits 3-6
//
#define STEP_LCG(x) do { x = x * 1103515245 + 12345; } while (0)
#define MAKE_PATTERN(x, pattern) \
	(((((x) >> 3) ^ ((x) > 15)) & 0x0f0f0f0f) ^ pattern)

void OpMemGuard::InstallGuard(char* mem, size_t size, UINT32 pattern)
{
#ifdef VALGRIND
//
// If we are running on valgrind, initialize the memory-access state
// in valgrind depending on what guard pattern the memory debugger was
// prescribing for the particular guard.
//
// The memory debugger guard pattern is not used by valgrind, since
// valgrind does not depend on the guard values for its operation, but
// rather on the memory-access status of individual bytes. So we save
// cycles by not writing the guard patterns themselves when running on
// valgrind, but rather set a memory-access state that will cause
// valgrind to detect illegal memory accesses using its own detection
// logic (which includes catching illegal reads).
//
	if ( RUNNING_ON_VALGRIND )
	{
		switch ( pattern )
		{
		case MEMORY_NEW_PATTERN:
			// Fresh allocation; make it uninitialized
			VALGRIND_MAKE_MEM_UNDEFINED(mem, size);
			break;

		case MEMORY_EXTERNAL_PATTERN:
		case MEMORY_FRONT_PATTERN:
		case MEMORY_REAR_PATTERN:
		case MEMORY_FREE_PATTERN:
			// It is illegal to access any of these (freed memory can't be
			// written to since this will cause a corruption), and reading
			// the freed memory should at least give undefined reads.
			VALGRIND_MAKE_MEM_NOACCESS(mem, size);
			break;

		default:
			OP_ASSERT(!"Don't know how to handle pattern in valgrind");
			break;
		}

		return;
	}
#endif

	UINTPTR intptr = reinterpret_cast<UINTPTR>(mem);
	unsigned int unaligned = intptr & (sizeof(UINT32) - 1);

	union {
		UINT32 pattern;
		unsigned char bytes[sizeof(UINT32)]; // ARRAY OK 2007-11-21 mortenro
	} value;

	UINT32 seed = (UINT32)intptr;

	if ( unaligned )
	{
		// Unaligned start address
		STEP_LCG(seed);
		value.pattern = MAKE_PATTERN(seed, pattern);
		unsigned int remainder = sizeof(UINT32) - unaligned;
		if ( remainder > size )
			remainder = size;
		op_memcpy(mem, &value.bytes[unaligned], remainder);
		mem += remainder;
		size -= remainder;
	}

	UINT32* wordfill = reinterpret_cast<UINT32*>(mem);
	while ( size >= 4 )
	{
		STEP_LCG(seed);
		*wordfill++ = MAKE_PATTERN(seed, pattern);
		size -= 4;
	}

	if ( size > 0 )
	{
		STEP_LCG(seed);
		value.pattern = MAKE_PATTERN(seed, pattern);
		op_memcpy(wordfill, &value.bytes[0], size);
	}
}

BOOL OpMemGuard::CheckGuard(char* mem, size_t size, UINT32 pattern)
{
#ifdef VALGRIND
//
// When running on valgrind, the guard bytes are not written into the
// guard areas, so there is nothing to check. So just return when
// running on valgrind.
//
	if ( RUNNING_ON_VALGRIND )
		return TRUE;
#endif

	UINTPTR intptr = reinterpret_cast<UINTPTR>(mem);
	unsigned int unaligned = intptr & (sizeof(UINT32) - 1);

	union {
		UINT32 pattern;
		unsigned char bytes[sizeof(UINT32)]; // ARRAY OK 2007-11-21 mortenro
	} value;

	UINT32 seed = (UINT32)intptr;

	if ( unaligned )
	{
		// Unaligned start address
		STEP_LCG(seed);
		value.pattern = MAKE_PATTERN(seed, pattern);
		unsigned int remainder = sizeof(UINT32) - unaligned;
		if ( remainder > size )
			remainder = size;
		if ( op_memcmp(mem, &value.bytes[unaligned], remainder) )
			return FALSE;
		mem += remainder;
		size -= remainder;
	}

	UINT32* wordcheck = reinterpret_cast<UINT32*>(mem);
	while ( size >= 4 )
	{
		STEP_LCG(seed);
		UINT32 val = MAKE_PATTERN(seed, pattern);
		if ( *wordcheck++ != val )
			return FALSE;
		size -= 4;
	}

	if ( size > 0 )
	{
		STEP_LCG(seed);
		value.pattern = MAKE_PATTERN(seed, pattern);
		if ( op_memcmp(wordcheck, &value.bytes[0], size) )
			return FALSE;
	}

	return TRUE;
}

#ifdef MEMORY_ELECTRIC_FENCE
class OpMemEfenceClassname : public ListElement<OpMemEfenceClassname>
{
public:
	OpMemEfenceClassname(char* name) : name(name) {}
	char* name;
};
#endif // MEMORY_ELECTRIC_FENCE

BOOL OpMemGuard::UseEfence(size_t size, OpAllocInfo* ai)
{
#ifdef MEMORY_ELECTRIC_FENCE
	//
	// Check for misc characteristics of the allocations, and return
	// TRUE for those allocations that should be protected by electric
	// fence. Not too many objects should receive this honour, though...
	//
	//
	// object: The name of the object allocated, or "unknown"
	// size:   The size (in bytes) of the original allocation (not rounded)
	// file:   The filename of the source code where allocation happened
	// line:   The line in the sourcecode where allocation happened
	//
	// Note: Fetching the filename is currently a bit expensive.
	//

	// Examples:
	//
	// const char* object = ai->GetObject();
	// OpSrcSite* site = OpSrcSite::GetSite(ai->GetSiteId());
	// const char* file = site->GetFile();
	// int line = site->GetLine();
	//
	// if ( ! op_strcmp(object, "ES_MarkSweepHeap") )
	//     return TRUE;
	// if ( ! op_strcmp(object, "Plugin") )
	//	   return TRUE;
	// if ( size == 1234 )
	//	   return TRUE;

	const char* object = ai->GetObject();

	for ( OpMemEfenceClassname* classname = mem_efence_class_names.First(); classname; classname = classname->Suc() )
		if ( ! op_strcmp(object, classname->name) )
			return TRUE;

#endif // MEMORY_ELECTRIC_FENCE

	return FALSE;
}

#ifdef MEMORY_ELECTRIC_FENCE
void OpMemGuard::AddEfenceClassname(const char *classname)
{
	char* copy = op_strdup(classname);
	if (copy)
	{
		// Protect name copy from end of session leak check.
		Action(MEMORY_ACTION_SET_MARKER_M4_SINGLE, copy);

		OpMemEfenceClassname* element = OP_NEW(OpMemEfenceClassname, (copy));
		if (element)
		{
			element->Into(&mem_efence_class_names);

			// Protect list element from leak check.
			Action(MEMORY_ACTION_SET_MARKER_M4_SINGLE, element);
		}
	}
}

void* OpMemGuard::try_efence_malloc(OpMmapSegment* mm, size_t size)
{
	size += sizeof(int); // Store size of allocation first as 'int'

	size_t page_aligned_size = (size + memory_page_mask) & ~memory_page_mask;
	size_t skip_bytes = page_aligned_size - size + sizeof(int);
	size_t request_size = page_aligned_size + memory_page_size;

	// Allocate one memory page more than needed
#ifdef MEMORY_USE_LOCKING
	void* ptr = mm->mmap(request_size, MEMCLS_DEFAULT, FALSE);
#else
	void* ptr = mm->mmap(request_size, MEMCLS_DEFAULT);
#endif

	if ( ptr != 0 )
	{
		char* cptr = reinterpret_cast<char*>(ptr);

		// Make the last memory page inaccessibly through freeing it
		OpMemory::VirtualFree(mm->GetOpMemSegment(),
							  (void*)(cptr + page_aligned_size),
							  memory_page_size);

		int* alloc = (int*)(cptr + skip_bytes);
		alloc[-1] = page_aligned_size;

		return (void*)alloc;
	}

	return 0;
}

void* OpMemGuard::efence_malloc(size_t size)
{
	void* ptr = 0;

	if ( mmap_current >= 0 )
		ptr = try_efence_malloc(mmap[mmap_current], size);

	if ( ptr == 0 && mmap_current < EFENCE_MMAP_SEGMENT_LIMIT - 1)
	{
		// Allocate a new segment
		mmap_current++;
		mseg[mmap_current] = OpMemory::CreateVirtualSegment(EFENCE_MMAP_SEGMENT_SIZE);
		if ( mseg[mmap_current] == 0 )
		{
			mmap_current--;
			return 0;
		}

		mmap[mmap_current] =
			OpMmapSegment::Create(mseg[mmap_current],
								  MEMCLS_MMAP_HEADER,
								  MEMCLS_MMAP_UNUSED);
		if ( mmap[mmap_current] == 0 )
		{
			OpMemory::DestroySegment(mseg[mmap_current]);
			mmap_current--;
			return 0;
		}

		ptr = try_efence_malloc(mmap[mmap_current], size);
	}

	return ptr;
}

void OpMemGuard::efence_free(void* ptr)
{
	for ( int k = 0; k <= mmap_current; k++ )
	{
		if ( mmap[k]->Contains(ptr) )
		{
			// Found the memory segment with this allocation

			int* alloc = (int*)ptr;
			size_t size = alloc[-1];
			void* addr = (void*)((UINTPTR)ptr & ~memory_page_mask);

			OpMemory::VirtualFree(mmap[k]->GetOpMemSegment(), addr, size);

			return;
		}
	}

	OP_ASSERT(!"E-fence deallocation from non-existing memory segment\n");
}

#endif // MEMORY_ELECTRIC_FENCE

#ifdef MEMORY_CALLSTACK_DATABASE

void OpMemGuard::ShowCallStack(OpCallStack* stack)
{
	int size = stack->GetSize();
	UINTPTR ptr;
	int k;

#ifdef MEMORY_USE_CODELOC
	for ( k = 0; k < size; k++ )
	{
		ptr = stack->GetAddr(k);
		OpCodeLocationManager::GetCodeLocation(ptr);
	}
#endif

	for ( k = 0; k < size; k++ )
	{
		ptr = stack->GetAddr(k);
#ifdef MEMORY_USE_CODELOC
		OpCodeLocation* loc = OpCodeLocationManager::GetCodeLocation(ptr);
		if (loc)
		{
#ifndef WIN32
			dbg_printf("MEM:      %p %s %s\n",
					   ptr, loc->GetFunction(), loc->GetFileLine());
#else
			dbg_printf("%s : %s (MEM: %p)\n",
					   loc->GetFileLine(), loc->GetFunction(),ptr);
#endif
#else
			dbg_printf("MEM:      %p\n", ptr);
#endif
		}
	}
}

#else // !MEMORY_CALLSTACK_DATABASE

void OpMemGuard::ShowCallStack(const UINTPTR* stack, int size)
{
	int max = size - 1;
	while ( max > 0 && stack[max] == 0 )
		max--;

#ifdef MEMORY_USE_CODELOC
	// Make it clear what code-locations we are interested in first,
	// to allow faster async lookup
	for ( int k = 0; k <= max; k++ )
		OpCodeLocationManager::GetCodeLocation(stack[k]);
#endif

	for ( int k = 0; k <= max; k++ )
	{
#ifdef MEMORY_USE_CODELOC
		OpCodeLocation* loc = OpCodeLocationManager::GetCodeLocation(stack[k]);
		dbg_printf("MEM:      %p %s %s\n",
				   stack[k], loc->GetFunction(), loc->GetFileLine());
#else
		dbg_printf("MEM:      %p\n", stack[k]);
#endif
	}
}

#endif // !MEMORY_CALLSTACK_DATABASE

void OpMemGuard::ReportLeaks(void)
{
	unsigned int leaked_bytes = 0;
	unsigned int leaked_allocations = 0;
	unsigned int exempted_bytes = 0;
	unsigned int exempted_allocations = 0;
	OpMemGuardInfo* anchor;
	OpMemGuardInfo* i;

	MUTEX_LOCK();

#if defined(MEMORY_LOG_ALLOCATIONS) && defined(MEMORY_USE_CODELOC)
	// Flush the OpCodeLocation database
	OpCodeLocationManager::Flush();
#endif

	// Check all the allocations first for corruptions

	anchor =  &dead_allocations;
	for ( i = anchor->next; i != anchor; i = i->next )
		CheckIntegrity(i);

	anchor = &live_allocations;
	for ( i = anchor->next; i != anchor; i = i->next )
	{
		if ( CheckIntegrity(i) )
		{
			if ( i->bits.flags & MEMORY_FLAG_MARKER4 )
			{
				// This is an allocation exempted from leak-checking
				exempted_bytes += i->size;
				exempted_allocations++;
			}
			else
			{
				// This is a regular allocation that should be leak-checked
				OpMemEvent mev(MEMORY_EVENTCLASS_LEAKED, 0, i);
				OpMemEventListener::DeliverMemoryEvent(&mev);
				leaked_bytes += i->size;
				leaked_allocations++;
			}
		}
	}

	if ( leaked_allocations == 0 )
	{
		if ( exempted_allocations == 0 )
			dbg_printf("MEM: No live allocations\n");
		else
			dbg_printf("MEM: No live allocations, "
					   "but %u bytes in %u exempted allocations\n",
					   exempted_bytes, exempted_allocations);
	}
	else
	{
		dbg_printf("MEM: Total of %u leaked bytes in %u allocations\n",
				   leaked_bytes, leaked_allocations);
		if ( exempted_allocations > 0 )
			dbg_printf("MEM: Total of %u bytes in %u exempted allocations\n",
					   exempted_bytes, exempted_allocations);
	}

	MUTEX_UNLOCK();

	reporting->PrintStatistics();
	dbg_printf("\n");

#ifndef MEMORY_ASSERT_ON_ERROR
	// This assertion is redundant with TWEAK_MEMORY_ASSERT_ON_ERROR
	OP_ASSERT(leaked_bytes == 0);
#endif // MEMORY_ASSERT_ON_ERROR
}

void OpMemGuard::SetMarker(UINT32 setflags)
{
	MUTEX_LOCK();

	OpMemGuardInfo* anchor = &live_allocations;
	OpMemGuardInfo* mgi = anchor->next;

	while ( mgi != anchor )
	{
		mgi->bits.flags |= setflags;
		mgi = mgi->next;
	}

	MUTEX_UNLOCK();
}

int OpMemGuard::Action(enum OpMemDebugActionInteger action, int value)
{
	switch ( action )
	{
	case MEMORY_ACTION_SET_OOM_COUNT:
		oom_countdown = value;
		break;

	case MEMORY_ACTION_SET_MEMORY_MAX_ERROR_COUNT:
		reporting->SetMemoryMaxErrorCount(value);
		break;

	default:
		OP_ASSERT(!"Illegal memory-action with integer argument");
		break;
	}

	return 0;
}

int OpMemGuard::Action(enum OpMemDebugAction action, void* misc)
{
	UINT32 matchpattern;
	UINT32 matchmask;
	const char* desc;
	int total_bytes = 0;
	int total_count = 0;

	switch ( action )
	{
	case MEMORY_ACTION_SET_MARKER_T1:
		SetMarker(MEMORY_FLAG_MARKER1 | MEMORY_FLAG_MARKER2);
		return 0;

	case MEMORY_ACTION_SET_MARKER_T2:
		SetMarker(MEMORY_FLAG_MARKER2);
		return 0;

	case MEMORY_ACTION_SET_MARKER_M3_ALL:
		SetMarker(MEMORY_FLAG_MARKER3);
		return 0;

	case MEMORY_ACTION_SET_MARKER_M3_SINGLE:
		if ( misc != 0 )
		{
			OpMemGuardInfo* mgi = AddrToInfo(misc);
			if ( CheckIntegrity(mgi) )
				mgi->bits.flags |= MEMORY_FLAG_MARKER3;
		}
		return 0;

	case MEMORY_ACTION_SET_MARKER_M3_AUTO:
		initial_flags |= MEMORY_FLAG_MARKER3;
		return 0;

	case MEMORY_ACTION_SET_MARKER_M3_NOAUTO:
		initial_flags &= ~MEMORY_FLAG_MARKER3;
		return 0;

	case MEMORY_ACTION_SET_MARKER_M4_ALL:
		SetMarker(MEMORY_FLAG_MARKER4);
		return 0;

	case MEMORY_ACTION_SET_MARKER_M4_SINGLE:
		if ( misc != 0 )
		{
			OpMemGuardInfo* mgi = AddrToInfo(misc);
			if ( CheckIntegrity(mgi) )
				mgi->bits.flags |= MEMORY_FLAG_MARKER4;
		}
		return 0;

	case MEMORY_ACTION_SHOW_ALL:
		matchpattern = 0;
		matchmask = 0;
		desc = "All currently live allocations";
		break;

	case MEMORY_ACTION_SHOW_BEFORE_T1:
		matchpattern = MEMORY_FLAG_MARKER1;
		matchmask = MEMORY_FLAG_MARKER1;
		desc = "Live allocations from before T1";
		break;

	case MEMORY_ACTION_SHOW_BETWEEN_T1T2:
		matchpattern = MEMORY_FLAG_MARKER2;
		matchmask = MEMORY_FLAG_MARKER1 + MEMORY_FLAG_MARKER2;
		desc = "Live allocations from between T1 and T2";
		break;

	case MEMORY_ACTION_SHOW_AFTER_T2:
		matchpattern = 0;
		matchmask = MEMORY_FLAG_MARKER1 + MEMORY_FLAG_MARKER2;
		desc = "Live allocations from after T2";
		break;

	case MEMORY_ACTION_COUNT_NOT_M3:
		return CountNotM3();

	case MEMORY_ACTION_FORKED:
		//
		// When the process forks, we shouldn't do any more logging
		// since the new process starts living independently from the
		// main process.
		//
		log_allocations = FALSE;
#ifdef MEMTOOLS_ENABLE_CODELOC
		// The codelocation manager needs to be aware of this also
		OpCodeLocationManager::Forked();
#endif
		return 0;

	case MEMORY_ACTION_ENABLE_LOGGING:
	{
		MUTEX_LOCK();

		if ( ! log_allocations )
		{
			log_allocations = TRUE;
			dbg_printf("MEM: Memory logging enabled\n");

			OpMemGuardInfo* anchor = &live_allocations;
			OpMemGuardInfo* i;

			for ( i = anchor->next; i != anchor; i = i->next )
			{
				OpAllocInfo ai(i);
				OpMemEvent mev(MEMORY_EVENTCLASS_ALLOCATE, &ai, i);
				OpMemEventListener::DeliverMemoryEvent(&mev);
			}

			log_printf("MEM: Backlog of memory operations completed\n");
		}

		MUTEX_UNLOCK();
		return 0;
	}

	case MEMORY_ACTION_MEMORY_DUMP:
	{
		MUTEX_LOCK();

		dbg_printf("MEM: Begin memory dump\n");
		int count = 0;
		int bytes = 0;
		OpMemGuardInfo* anchor = &live_allocations;
		OpMemGuardInfo* mgi;

		for ( mgi = anchor->next; mgi != anchor; mgi = mgi->next )
		{
			if ( CheckIntegrity(mgi) )
			{
				void* ptr = InfoToAddr(mgi);
				int size = mgi->size;

				if ( size >= 0 && size < 100*1024*1024 )
				{
#ifdef ENABLE_MEMORY_OOM_EMULATION
					if ( mgi->constrained_allocation != 0 )
						log_printf("MEM: Dump %p(%p) '%B'\n", ptr,
								   mgi->constrained_allocation, ptr, size);
					else
#endif
						log_printf("MEM: Dump %p '%B'\n", ptr, ptr, size);

					count++;
					bytes += size;
				}
				else
				{
					dbg_printf("MEM: Skipped dump %p size=%d\n", ptr, size);
				}
			}
		}

		dbg_printf("MEM: Dumped %d objects of %d bytes total\n", count, bytes);

		MUTEX_UNLOCK();

		return 0;
	}

	default:
		OP_ASSERT(!"Illegal OpMemDebugAction");
		return 0;
	}

	MUTEX_LOCK();

	OpMemUpperTen ranking(16);

	OpMemGuardInfo* anchor = &live_allocations;
	OpMemGuardInfo* mgi = anchor->next;
	OpMemGuardInfo* mgi2;

	while ( mgi != anchor )
	{
		mgi->bits.flags &= ~MEMORY_FLAG_STATUS_TMP1;
		mgi = mgi->next;
	}

	mgi = anchor->next;

	while ( mgi != anchor )
	{
		if ( (mgi->bits.flags & (matchmask +	MEMORY_FLAG_STATUS_TMP1)) ==
			 matchpattern )
		{
			// This is the pattern we want, and it has not processed
			// earlier either (MEMORY_FLAG_STATUS_TMP1 == 0).
#if MEMORY_KEEP_ALLOCSTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
			UINTPTR key = mgi->allocstack_new->GetID();
#  else
			UINTPTR key = mgi->allocstack[0];
#  endif
#else
			UINTPTR key = mgi->size;
#endif
			size_t size = 0;
			int count = 0;
			for ( mgi2 = mgi; mgi2 != anchor; mgi2 = mgi2->next )
			{
				if (
#if MEMORY_KEEP_ALLOCSTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
					 mgi2->allocstack_new->GetID() == key &&
#  else
					 mgi2->allocstack[0] == key &&
#  endif
#else
					 mgi2->size == key &&
#endif
					 (mgi2->bits.flags & (matchmask + MEMORY_FLAG_STATUS_TMP1)) ==
					 matchpattern )
				{
					size += mgi2->size;
					count++;
					mgi2->bits.flags |= MEMORY_FLAG_STATUS_TMP1;
				}
			}
			ranking.Add(key, size, count);
		}

		total_bytes += mgi->size;
		total_count++;
		mgi = mgi->next;
	}

	dbg_printf("\n%s\n", desc);
#if MEMORY_KEEP_ALLOCSTACK > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
	ranking.Show("Top allocation call-stacks:", "%p   %d bytes in %d allocs");
#  else
	ranking.Show("Top allocation sites:", "%p   %d bytes in %d allocs");
#  endif
#else
	ranking.Show("Top allocation sizes:", "%p   %d bytes in %d allocs");
#endif
	dbg_printf("All allocations: %d bytes in %d allocations\n\n",
			   total_bytes, total_count);

	MUTEX_UNLOCK();

	return 0;
}

int OpMemGuard::CountNotM3(void)
{
	int count = 0;

	MUTEX_LOCK();

	OpMemGuardInfo* anchor = &live_allocations;
	OpMemGuardInfo* i;

	for ( i = anchor->next; i != anchor; i = i->next )
		if ( CheckIntegrity(i) && (i->bits.flags & MEMORY_FLAG_MARKER3) == 0 )
			count++;

	MUTEX_UNLOCK();

	return count;
}

void OpMemGuardInfo::InsertBefore(OpMemGuardInfo* target)
{
	next = target;
	prev = target->prev;
	prev->next = this;
	next->prev = this;
}

void OpMemGuardInfo::Unlink(void)
{
	next->prev = prev;
	prev->next = next;
	prev = next = 0;
}

UINTPTR OpMemGuardInfo::GetAddr(void) const
{
#ifdef ENABLE_MEMORY_OOM_EMULATION
	if ( constrained_allocation != 0 )
		return (UINTPTR)constrained_allocation;
#endif
	return (UINTPTR)g_mem_guard->InfoToAddr(this);
}

UINTPTR OpMemGuardInfo::GetPhysAddr(void) const
{
	return (UINTPTR)g_mem_guard->InfoToAddr(this);
}

BOOL OpMemGuardInfo::IsArray(void) const
{
	switch ( bits.flags & MEMORY_FLAG_ALLOCATED_MASK )
	{
	case MEMORY_FLAG_ALLOCATED_OP_NEWA:
	case MEMORY_FLAG_ALLOCATED_GLOBAL_NEWA:
	case MEMORY_FLAG_ALLOCATED_OP_NEWELSA:
	case MEMORY_FLAG_ALLOCATED_OP_STRDUP:
	case MEMORY_FLAG_ALLOCATED_UNI_STRDUP:
		return TRUE;

	default:
		return FALSE;
	}
}

#endif // ENABLE_MEMORY_DEBUGGING

#ifdef ENABLE_MEMORY_OOM_EMULATION
extern "C" int OpMemoryInitLimitedHeap(size_t size)
{
	OpMemoryStateInit();

	if ( size > 0 && g_mem_limited_heap == 0 )
	{
		size += 4096;

		void* memory = op_lowlevel_malloc(size);
		if ( memory == 0 )
			return 0;

		g_mem_limited_heap = new (memory) SingleBlockHeap(size);

#ifdef ENABLE_MEMORY_DEBUGGING
		dbg_printf("MEM: Limited heap at %p size %z bytes\n",
				   g_mem_limited_heap, size);

		// Move all allocations onto the limited heap
		g_mem_guard->PopulateLimitedHeap();
#endif // ENABLE_MEMORY_DEBUGGING
	}
	return 1;
}

#ifdef ENABLE_MEMORY_DEBUGGING

void OpMemGuard::PopulateLimitedHeap(void)
{
	// Move the allocations done so far onto the limited heap

	MUTEX_LOCK();

	size_t bytes = 0;
	size_t count = 0;
	OpMemGuardInfo* anchor = &live_allocations;
	OpMemGuardInfo* i;

	OP_ASSERT(log_allocations == FALSE);  // Enable logging must happen later

	for ( i = anchor->next; i != anchor; i = i->next )
	{
		if ( CheckIntegrity(i) && i->constrained_allocation == 0 )
		{
			i->constrained_allocation = g_mem_limited_heap->malloc(i->size);
			if ( i->constrained_allocation == 0 )
			{
				dbg_printf("MEM: No room for %d bytes on limited heap\n",
						   i->size);
			}
			else
			{
				bytes += i->size;
				count++;
			}
		}
	}

	if ( count > 0 )
		dbg_printf("MEM: Moved %z bytes in %z allocs to the limited heap\n",
				   bytes, count);

	MUTEX_UNLOCK();
}

#if 0

// This may come in handy together with -memlog for debug builds when
// tracking down a problem.

void barf(void)
{
}

void OpMemGuard::CheckAlloc(void* ptr)
{
	OpMemGuardInfo* mgi = AddrToInfo(ptr);
	if ( mgi->bits.flags & MEMORY_FLAG_RELEASED_MASK )
		*(char*)ptr = 0;
	CheckIntegrity(mgi);
}

void OpMemDebug_CheckAlloc(void* ptr)
{
	OPALLOCINFO(info, 0, 0, 0, 0);

	OpCallStack* stack = info.GetOpCallStack(MEMORY_ALLOCINFO_STACK_DEPTH);
	if ( stack != 0 )
	{
		int id = stack->GetID();
		dbg_printf("Callstack for %p is: 0x%x\n", ptr, id);
		stack->AnnounceCallStack();
	}

	// g_mem_guard->CheckAlloc(ptr);
}
#endif // 0

#endif // ENABLE_MEMORY_DEBUGGING

#endif // ENABLE_MEMORY_OOM_EMULATION

extern "C" void OpMemoryInit(size_t limited_heap_size, BOOL memory_logging, const char* efence_classnames)
{
#ifdef ENABLE_MEMORY_OOM_EMULATION
	OpMemoryInitLimitedHeap(limited_heap_size);
#endif // ENABLE_MEMORY_OOM_EMULATION

#ifdef MEMORY_ELECTRIC_FENCE
	/*
	 * Electric fence class name specifiers are expected to be provided as
	 * one string of comma-separated class names. Split them and feed them
	 * to the starving beast that is the memory guard singleton.
	 */
	if (efence_classnames)
	{
		char* efence_class_list = op_strdup(efence_classnames);
		if (efence_class_list)
		{
			const char* classname = efence_class_list;
			char* delim;

			do
			{
				delim = op_strchr(classname, ',');

				if (delim)
					*delim = 0;

				g_mem_guard->AddEfenceClassname(classname);
				classname = delim + 1;
			}
			while (delim);

			op_free(efence_class_list);
		}
	}
#endif // MEMORY_ELECTRIC_FENCE

#ifdef MEMORY_LOG_ALLOCATIONS
	if (memory_logging)
		g_mem_debug->Action(MEMORY_ACTION_ENABLE_LOGGING);
#endif // MEMORY_LOG_ALLOCATIONS
}

#ifdef ENABLE_MEMORY_DEBUGGING
void OpMemDebug_Disregard(void* ptr)
{
	g_mem_guard->Action(MEMORY_ACTION_SET_MARKER_M4_SINGLE, ptr);
}

#ifdef MEMTOOLS_CALLSTACK_MANAGER
void OpMemDebug_PrintCallstack()
{
	OPALLOCINFO(info, 0, 0, 0, 0);

#if MEMORY_ALLOCINFO_STACK_DEPTH > 0
#  ifdef MEMORY_CALLSTACK_DATABASE
	g_mem_guard->ShowCallStack(info.GetOpCallStack(MEMORY_ALLOCINFO_STACK_DEPTH));
#  else
	g_mem_guard->ShowCallStack(info.GetStack(),
							   MEMORY_ALLOCINFO_STACK_DEPTH);
#  endif
#endif
}

void OpMemDebug_PrintThisCallstack(OpCallStack* s)
{
	if ( s )
		g_mem_guard->ShowCallStack(s);
}

#if defined(MEMORY_CALLSTACK_DATABASE) && MEMORY_ALLOCINFO_STACK_DEPTH > 0
OpCallStack* OpMemDebug_GetCallstack()
{
	OPALLOCINFO(info, 0, 0, 0, 0);

	return info.GetOpCallStack(MEMORY_ALLOCINFO_STACK_DEPTH);
}
#endif // MEMORY_CALLSTACK_DATABASE && MEMORY_ALLOCINFO_STACK_DEPTH > 0

BOOL OpMemDebug_IsCallstacksEqual(OpCallStack* s1, OpCallStack* s2, int start, int length)
{
	for( int i = start; i < length; i++ )
	{
		if ( s1->GetAddr(i) != s2->GetAddr(i) )
			return FALSE;
	}
	return TRUE;
}
#endif // MEMTOOLS_CALLSTACK_MANAGER

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
void OpMemDebug_PrintLiveAllocations(BOOL full)
{
	OpSrcSite::PrintLiveAllocations();
	if ( full )
		g_mem_debug->Action(MEMORY_ACTION_SHOW_ALL);
}
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE

UINT32 OpMemGuard::GetAllocSiteID(void* ptr)
{
	OpMemGuardInfo* mgi = AddrToInfo(ptr);
	if ( mgi )
		return mgi->allocsiteid;
	OP_ASSERT(!"Null pointer or invalid pointer passed as argument");
	return 0;
}

UINT32 OpMemGuard::GetAllocatedAmount(void* ptr)
{
	OpMemGuardInfo* mgi = AddrToInfo(ptr);
	if ( mgi )
		return mgi->size;
	OP_ASSERT(!"Null pointer or invalid pointer passed as argument");
	return 0;
}

void OpMemGuard::PrintSiteInfo(UINT32 id)
{
	OpSrcSite* site = OpSrcSite::GetSite(id);
	const char* file = site->GetFile();
	int line = site->GetLine();
	dbg_printf("%s (%d)", file, line);
}

#ifdef MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
int OpMemDebug_GetTotalMemUsed()
{
	return g_mem_guard->GetTotalMemUsed();
}
#endif // MEMORY_LOG_USAGE_PER_ALLOCATION_SITE


#ifdef MEMORY_FAIL_ALLOCATION_ONCE
void
OpMemGuard::SetCurrentTag(int tag)
{
	currenttag = tag;
}

void
OpMemGuard::ClearCallCounts()
{
	OpSrcSite::ClearCallCounts();
}
#endif // MEMORY_FAIL_ALLOCATION_ONCE

#endif // ENABLE_MEMORY_DEBUGGING
