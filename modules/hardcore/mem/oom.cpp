/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef __OPRBRIDGE_DLL__
# include "excepts.h"
# include "epocbridgedef.h"
# include "os.h"
#endif //__OPRBRIDGE_DLL__

/* Wizards only, emulator only.  Turn this on and follow the
   directions in modules/memtools/epoc-instrumentation.cpp to patch
   the EPOC memory allocation functions in a style similar to
   happy-malloc.  Contact lth@opera.com if you need help.
   */
//#define EPOC_EMULATOR_MALLOC_INSTRUMENTATION

#ifndef __OPRBRIDGE_DLL__

#ifdef EPOC_EMULATOR_MALLOC_INSTRUMENTATION
# include "modules/memtools/epoc-instrumentation.h"
#endif

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mem/oom.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/windowcommander/OpWindowCommanderManager.h"

#ifdef EPOC
#  include <e32hal.h>
#endif // EPOC

#ifdef CONSTRAIN_OPERA_MEMORY_USAGE
# include "modules/lea_malloc/lea_malloc.h"
# include "modules/lea_malloc/malloc.h"
#endif // CONSTRAIN_OPERA_MEMORY_USAGE

#ifdef OUT_OF_MEMORY_POLLING
#include "modules/url/url_man.h"
#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/img/image.h"
#endif // OUT_OF_MEMORY_POLLING

#ifdef OPERA_CONSOLE
#include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

#ifdef CONSTRAIN_OPERA_MEMORY_USAGE
static int bytes_left_to_heap_limit();
static int bytes_left_to_alloc_limit();
#endif // CONSTRAIN_OPERA_MEMORY_USAGE

/* OOM handling policy */
static const time_t    seconds_between_oom_flushes = 4;
//static const time_t seconds_visited_links_grace_time = 10*60;
//static const int    bytes_img_memory_to_keep = 0;
//static const int    bytes_doc_memory_to_keep = 0;
//static const int    bytes_ram_cache_to_keep = 0;
static const size_t    rainy_day_fund_size = 65536;				// We guess; static size may not be appropriate
static const size_t    rainy_day_min_block_size = 4096;			// no use to split it in smaller pieces than this
void ForceOutOfMemoryFlush( BOOL known_stack_content );

#endif //__OPRBRIDGE_DLL__

#ifndef __OPRBRIDGE_DLL__

#ifdef OUT_OF_MEMORY_POLLING
static void fill_rainyday_fund();
static void drain_rainyday_fund();
static void start_periodic_tasksL();
static void return_memory_to_os();
static void flush_memory( BOOL known_stack_content, BOOL stop_loading, BOOL clear_disk_cache );
static void protected_flush_memory( BOOL known_stack_content, BOOL stop_loading, BOOL clear_disk_cache );
static int  low_system_memory();
static int  low_opera_memory();
static void report_soft_out_of_memory_situation();
static void report_out_of_memory_situation();
static void report_out_of_disk_situation();
void SoftOutOfMemoryFlush( int severity );
#endif //OUT_OF_MEMORY_POLLING

#ifdef EPOC
extern void ReportError(TInt aError);
#endif

void InitializeOutOfMemoryHandling()
{
#ifdef OUT_OF_MEMORY_POLLING
# ifdef EPOC_EMULATOR_MALLOC_INSTRUMENTATION
	epoc_malloc_setup_instrumentation();
# endif
	fill_rainyday_fund();
#endif //OUT_OF_MEMORY_POLLING
}

void StartOutOfMemoryHandlingL()
{
#ifdef OUT_OF_MEMORY_POLLING
#ifdef CONSTRAIN_OPERA_MEMORY_USAGE
	fill_rainyday_fund();
#endif // CONSTRAIN_OPERA_MEMORY_USAGE

	start_periodic_tasksL();

	g_oom_stopping_loading = FALSE;
#endif //OUT_OF_MEMORY_POLLING
}

void UnInitializeOutOfMemoryHandling()
{
#ifdef OUT_OF_MEMORY_POLLING
#ifdef CONSTRAIN_OPERA_MEMORY_USAGE
    drain_rainyday_fund();
#endif // CONSTRAIN_OPERA_MEMORY_USAGE
    if (g_periodic_task_manager)
	    g_periodic_task_manager->UnregisterTask(g_PeriodicTask);
    OP_DELETE(g_PeriodicTask);
    g_PeriodicTask = NULL;
#endif //OUT_OF_MEMORY_POLLING
}

void OutOfMemoryHandlingMetaOperation( int n )
{
#ifdef OUT_OF_MEMORY_POLLING
	switch (n)
	{
	case 1:
# ifdef EPOC_EMULATOR_MALLOC_INSTRUMENTATION
		epoc_malloc_dump_allocation_sites();
# endif //EPOC_EMULATOR_MALLOC_INSTRUMENTATION
		break;
	}
#endif //OUT_OF_MEMORY_POLLING
}


static void report_soft_out_of_memory_situation()
{
#ifdef EPOC
    // note: this will not block the message loop.
	// we will still be dispatching messages (timers,
	// network events, paint events ...) to Opera,
	// and they may trigger new OOM-events.
    ReportError(KErrNoMemory);
#else
	g_windowCommanderManager->GetOomListener()->OnSoftOomError();
#endif
}

static void report_out_of_memory_situation()
{
#ifdef EPOC
    // note: this will not block the message loop.
	// we will still be dispatching messages (timers,
	// network events, paint events ...) to Opera,
	// and they may trigger new OOM-events.
    ReportError(KErrNoMemory);
#else
	g_windowCommanderManager->GetOomListener()->OnOomError();
#endif
}

static void report_out_of_disk_situation()
{
#ifdef EPOC
    // note: this will not block the message loop.
	// we will still be dispatching messages (timers,
	// network events, paint events ...) to Opera,
	// and they may trigger new OOM-events.
    ReportError(KErrDiskFull);
#else
	g_windowCommanderManager->GetOomListener()->OnOodError();
#endif
}

// HandleOutOfMemorySituation() and SoftOutOfMemorySituation() should be
// called *only* from a message handler in response to a raised OOM flag,
// never from random places in the code.

void HandleOutOfMemorySituation()
{
	if (op_time(0) - g_handle_oom_prevtime >= seconds_between_oom_flushes)
    {
#ifdef OUT_OF_MEMORY_POLLING
		drain_rainyday_fund();

		// This must happen before all alerts about OOM are shown
		flush_memory( TRUE, TRUE, FALSE );
#endif //OUT_OF_MEMORY_POLLING

		report_out_of_memory_situation();

#if defined HAVE_NEW_HANDLER && defined OUT_OF_MEMORY_POLLING
		// This must happen after all alerts about OOM have been dismissed
		g_main_message_handler->PostDelayedMessage( MSG_OOM_RAINYDAY_REFILL, 0, 0, 1000 );
#endif //HAVE_NEW_HANDLER && OUT_OF_MEMORY_POLLING
		g_handle_oom_prevtime = op_time(0);

		if (g_ecmaManager)
			g_ecmaManager->HandleOutOfMemory();
    }
}

void SoftOutOfMemorySituation()
{
	if (op_time(0) - g_handle_oom_prevtime >= seconds_between_oom_flushes)
	{
#ifdef OUT_OF_MEMORY_POLLING
		SoftOutOfMemoryFlush(1);
#endif //OUT_OF_MEMORY_POLLING
		report_soft_out_of_memory_situation();
		g_handle_oom_prevtime = op_time(0);

		if (g_ecmaManager)
			g_ecmaManager->HandleOutOfMemory();
	}
}

void HandleOutOfDiskSituation()
{
	if (op_time(0) - g_handle_ood_prevtime >= seconds_between_oom_flushes)
	{
#ifdef OUT_OF_MEMORY_POLLING
		if (urlManager)
		{
#if 0
			// Commented this out because: (a) only meaningful to do this when
			// the disk that is full is the disk that holds the disk cache and
			// (b) we are throwing away a lot of data here; since the user has
			// a function to empty the cache, he can do that and try to save
			// the data again.  lth / 2002-01-25
			urlManager->EmptyDCache();
#endif //0
		}
#endif //OUT_OF_MEMORY_POLLING

		report_out_of_disk_situation();
		g_handle_ood_prevtime = op_time(0);
	}
}

void RefillOutOfMemoryRainyDayFund()
{
#ifdef OUT_OF_MEMORY_POLLING
	fill_rainyday_fund();
#endif //OUT_OF_MEMORY_POLLING
}

void ForceOutOfMemoryFlush( BOOL known_stack_content )
{
#ifdef OUT_OF_MEMORY_POLLING
	flush_memory( known_stack_content, TRUE, FALSE );
#endif //OUT_OF_MEMORY_POLLING
}

void NondestructiveOutOfMemoryFlush()
{
#ifdef OUT_OF_MEMORY_POLLING
	flush_memory( TRUE, FALSE, FALSE );
#endif //OUT_OF_MEMORY_POLLING
}

void SoftOutOfMemoryFlush( int severity )
{
#ifdef OUT_OF_MEMORY_POLLING
	switch (severity)
	{
	case 0:
		break;
	case 1:
		protected_flush_memory( TRUE, FALSE, FALSE );
		break;
	default:
		protected_flush_memory( TRUE, FALSE, TRUE );
		break;
	}
	return_memory_to_os();
#endif //OUT_OF_MEMORY_POLLING
}
#endif //__OPRBRIDGE_DLL__

// Internals from this point on

#ifdef OUT_OF_MEMORY_POLLING
#ifndef __OPRBRIDGE_DLL__

void OOMPeriodicTask::Run()
{
#ifdef DEBUG
	DebugTimer timer(DebugTimer::OOM);
#endif // DEBUG

	BOOL try_to_return_memory = FALSE;
	int  have_flushed_memory = 0;

	/* The idea is to monitor various quantities in the system and try to
	   ensure that nobody is starved for memory.  It's a bit heuristic,
	   but it probably contributes quite a bit to the useability of both
	   the browser and the machine.

	   Every tick: check if opera memory is low and flush some memory if it is.
	   Every tick: check if system memory is low and flush some memory if it is,
	               then try to compress the heap
	   Every five ticks: try to compress the heap.
	   */
	ticks = (ticks + 1) % 5;
	if (ticks == 0)
		try_to_return_memory = TRUE;

	switch (low_opera_memory())
	{
	case 0:
		break;
	case 1:
		have_flushed_memory = 1;
		SoftOutOfMemoryFlush(1);
		report_soft_out_of_memory_situation();
		break;
	default:
		have_flushed_memory = 2;
		SoftOutOfMemoryFlush(2);
		report_soft_out_of_memory_situation();
		break;
	}

	switch (low_system_memory())
	{
	case 0:
		break;
	case 1:
		try_to_return_memory = TRUE;
		break;
	case 2:
		if (have_flushed_memory < 1)
		{
			SoftOutOfMemoryFlush(1);
			report_soft_out_of_memory_situation();
		}
		try_to_return_memory = TRUE;
	default:
		if (have_flushed_memory < 2)
		{
			SoftOutOfMemoryFlush(2);
			report_soft_out_of_memory_situation();
		}
		try_to_return_memory = TRUE;
	}

	if (try_to_return_memory)
		return_memory_to_os();
}

static void start_periodic_tasksL()
{
	g_PeriodicTask = OP_NEW_L(OOMPeriodicTask,());
	LEAVE_IF_ERROR(g_periodic_task_manager->RegisterTask(g_PeriodicTask, 1000));
}

static void fill_rainyday_fund()
{
	// try to refill the rainyday_fund again
	// we will allow splitting it into several smaller blocks
	// but if we don't find blocks larger than rainy_day_min_block_size
	// we will bail out
	void *p;

	if (g_oom_rainyday_bytes >= rainy_day_fund_size)
		return;

	size_t nbytes = rainy_day_fund_size - g_oom_rainyday_bytes;

	do {
		p = 0;
		while (p == 0 && nbytes >= sizeof(rainyday_t))
		{
			p = op_malloc(nbytes);	// NOT!! 'new'
			if (p == 0)
				nbytes /= 2;
		}
		if (p != 0)
		{
			rainyday_t *q = (rainyday_t*)p;
			q->size = nbytes;
			q->next = g_oom_rainyday_fund;
			g_oom_rainyday_fund = q;
			g_oom_rainyday_bytes += nbytes;
		}
	}
	while (p != 0 && g_oom_rainyday_bytes < rainy_day_fund_size && nbytes >= rainy_day_min_block_size);

#ifdef HAVE_NEW_HANDLER
	if (g_oom_rainyday_bytes > 0)
		set_new_handler( drain_rainyday_fund );
#endif //HAVE_NEW_HANDLER
}

static void drain_rainyday_fund()
{
	rainyday_t *q;

	while (g_oom_rainyday_fund != 0)
	{
		q = g_oom_rainyday_fund;
		g_oom_rainyday_fund = g_oom_rainyday_fund->next;
		g_oom_rainyday_bytes -= q->size;
		op_free( q );
	}

#ifdef HAVE_NEW_HANDLER
	set_new_handler( 0 );
#endif //HAVE_NEW_HANDLER
}

static void protected_flush_memory(BOOL known_stack_content, BOOL stop_loading, BOOL clear_disk_cache)
{
	if (op_time(0) - g_flush_oom_prevtime >= seconds_between_oom_flushes)
	{
		flush_memory(known_stack_content, stop_loading, clear_disk_cache);
	}
}


/* If known_stack_content is FALSE then the only things that can
   be done are those that can be done regardless of where this
   procedure is called from.  Right now the only thing that can be
   done is to drain the rainy day fund.

   It's a little unclear how we can get to the point where there
   are other actions that are safe to perform when we don't know
   where we are -- possibly, we can do this with careful locking
   on the data structures that we are trying to flush.
   */
static void flush_memory( BOOL known_stack_content, BOOL stop_loading, BOOL clear_disk_cache )
{
	unsigned long mdiff = 0;

	if (known_stack_content && g_memory_manager)
	{
		mdiff += g_memory_manager->DocMemoryUsed();
		g_memory_manager->FreeDocMemory(ULONG_MAX, TRUE);	// FIXME: use policy number
		mdiff -= g_memory_manager->DocMemoryUsed();
	}

	if (known_stack_content && g_memory_manager && imgManager)
	{
		unsigned long m = g_memory_manager->GetMaxImgMemory();
		//g_memory_manager->SetMaxImgMemory(bytes_img_memory_to_keep);
		mdiff += imgManager->GetUsedCacheMem();
		g_memory_manager->SetMaxImgMemory(0);
		imgManager->FreeMemory();
		g_memory_manager->SetMaxImgMemory(m);
		mdiff -= imgManager->GetUsedCacheMem();
	}

	if (known_stack_content && urlManager)
	{
		//urlManager->CheckRamCacheSize(bytes_ram_cache_to_keep);
		mdiff += (unsigned long) urlManager->GetRamCacheSize();
		urlManager->CheckRamCacheSize(0);

		urlManager->FreeUnusedResources();
//FIXME-OOM does not exist any more	urlManager->PurgeVisitedLinks(seconds_visited_links_grace_time);
		mdiff -= (unsigned long) urlManager->GetRamCacheSize();
	}

	// Reclaim any space freed by previous actions.
	if (known_stack_content && g_ecmaManager)
	{
		mdiff += g_memory_manager->DocMemoryUsed();
		g_ecmaManager->GarbageCollect();
		mdiff -= g_memory_manager->DocMemoryUsed();
	}

	if (g_windowManager)
	{
		Window *w = g_windowManager->FirstWindow();
		while (w != NULL)
		{
			if (w->IsLoading())
				w->SetOOMOccurred(TRUE);
			w = w->Suc();
		}
	}


	// Really must not stop loading in softOOM situations.
	if (known_stack_content && stop_loading  && mdiff < MEM_STOP_LOADING_THRESHOLD)  
	{
		if (g_oom_stopping_loading)
			return;

		void* tst = op_malloc(MEM_STOP_LOADING_THRESHOLD);

		if (tst)
		{
			op_free(tst);
		}
		else
		{
			g_oom_stopping_loading = TRUE;
			if (g_windowManager)
			{
				Window *w = g_windowManager->FirstWindow();
				while (w != NULL)
				{
					if (w->IsLoading())
						w->CancelLoad(TRUE);
					w = w->Suc();
				}
			}
			g_oom_stopping_loading = FALSE;
		}
	}

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	if (known_stack_content)
		g_table_manager->Flush();
#endif

#ifdef OPERA_CONSOLE
	if (known_stack_content)
		g_console->Clear();
#endif // OPERA_CONSOLE

	// This may use a bit of memory, so do it last, to decrease
	// the chances that it will fail.

	// Note, this is quite disruptive!!  By emptying the disk cache we
	// throw out 20 *minutes* worth of downloaded (cached) data in
	// a 1MB cache at 10kbps.  So clear_disk_cache should be TRUE only
	// when you really mean it.

	if (known_stack_content && clear_disk_cache && urlManager)
		urlManager->EmptyDCache();

	for(int i=0; i < OPERA_MODULE_COUNT; ++i)
	{
		g_opera->modules[i]->FreeCachedData(TRUE);
	}

#ifdef HAVE_NEW_HANDLER
	if (known_stack_content)
		fill_rainyday_fund();
#endif

	return_memory_to_os();

	g_flush_oom_prevtime = op_time(0);
}

/* Return some free heap memory to the operating system, if possible. */
static void return_memory_to_os()
{
#ifdef EPOC
    // Reduce the heap size if any available at the 'end'.
	RHeap& heap = User::Heap();
	//TInt sizeBefore = heap.Size();
    heap.Compress();
	//TInt sizeAfter = heap.Size();
    //Log("Heap compressed by:"); LogSp(sizeBefore-sizeAfter); LogSp("bytes"); LogNl();
#elif defined(CONSTRAIN_OPERA_MEMORY_USAGE)
	// Shrink the heap as much as possible.

    // NOTE: We might want to consider using a value other than 0 here
    // NOTE: to make things a bit more efficient.  Since we don't
    // NOTE: leave any extra space we will probably get a potentially
    // NOTE: time consuming sbrk call quite soon.  If we trim with a
    // NOTE: negative value instead, some extra space is left at the
    // NOTE: end to service future expected allocations.  Perhaps even
    // NOTE: better, we could let the amount to trim depend on how
    // NOTE: much memory is available on the system.
	malloc_trim(0);
#endif
}


// return the amount of free system memory or INT_MAX if it can't be determined.
static int free_system_memory()
{
#ifdef EPOC
    TMemoryInfoV1Buf memoryInfo;
    UserHal::MemoryInfo(memoryInfo);
	TInt sysmem = memoryInfo().iFreeRamInBytes;
    return MAX(0,sysmem);
#elif defined(CONSTRAIN_OPERA_MEMORY_USAGE)
	// On Linux we do not want to grow to fill the system memory.  If
	// we do, the system as a whole must have a reasonable OOM
	// handling, which is something it probably doesn't have.
	// Therefore constrained malloc may use a heap limit as an
	// indication of OOM. If the heap limit is reached it is treated
	// as if we are running out of system memory (in a way we are,
	// since we are not allowed to use more system memory for the
	// heap).
	int heap_free = bytes_left_to_heap_limit();
	return MAX(0,heap_free);
#else
	return INT_MAX;
#endif
}


// Return the amount of free memory for Opera within the heap
//  + plus system memory above, according to comments above low_opera_memory (chrille)
static int free_opera_memory()
{
#ifdef EPOC
	// On Symbian heap memory is allocated in steps and RHeap::Available() call returns 
	// free memory for the current allocation, which is not what Opera wants.
	// Available memory is (maximum - current heap size + available currently)
	RHeap& curHeap = User::Heap();
	TInt maxLength = curHeap.MaxLength();
	TInt biggestBlock = 0;
	TInt availCurrent = curHeap.Available(biggestBlock);
	TInt curSize = curHeap.Size();
	TInt opmem = maxLength - curSize + availCurrent;
	return MAX(0,opmem);
#elif defined(CONSTRAIN_OPERA_MEMORY_USAGE)
	// First calculate how much memory is available within the heap.
	// We pretend we can use it without growing the heap.
	struct mallinfo info = mallinfo();
	int free_mem = info.fordblks;
	OP_ASSERT( free_mem >= 0 ); // Might wrap around if we have alot of memory...

	// Adjust the free memory according to how much we are allowed to
	// grow the heap.
	int heap_free = bytes_left_to_heap_limit();

	// If heap_free max is INT_MAX, don't try to do maths with it
	if (heap_free == INT_MAX)
		free_mem = INT_MAX;
	else
	{
		// If heap_free is < 0, that means somebody else (Qt for
		// example) has allocated the heap beyond our allowance. The
		// OOM system will not be shy about using this memory however,
		// so there's no sense in reporting those situations as low
		// memory. Thus, only add heap_free if it is > 0
		if (heap_free > 0)
			free_mem += heap_free;
	}

	// Adjust the free memory according to how much memory Opera is
	// allowed to allocate on the heap. It might very well be that
	// there is much more free memory on the heap than is set aside
	// for Opera.
	int alloc_free = bytes_left_to_alloc_limit();
	free_mem = MIN(free_mem, alloc_free);
	return MAX(0,free_mem);
#else
	return INT_MAX;
#endif
}

/* Return nonzero if the free system memory is estimated to be below
   some safe threshold (set by TWEAK_HC_SYSTEM_MEMORY_THRESHOLD).
   Higher values indicate higher severity.
   */
static int low_system_memory()
{
	int sysmem = free_system_memory();

	int severity = 0;
	int threshold = FREE_SYSTEM_MEMORY_THRESHOLD;

	OP_ASSERT( sysmem >= 0 );

	while (sysmem < threshold)
	{
		threshold /= 2;
		severity++;
	}

	return severity;
}

/* Return nonzero if the free memory for Opera is estimated to be below
   some safe threshold (set by TWEAK_HC_OPERA_MEMORY_THRESHOLD).
   The memory for Opera is free memory in Opera's heap plus free system
   memory above.  Higher return values indicate higher severity.
   */
static int low_opera_memory()
{
	int opmem =  free_opera_memory();
	if ( opmem == INT_MAX )
		return 0;

	int severity = 0;
	int freemem = MIN( opmem, INT_MAX - opmem );
	int threshold = FREE_OPERA_MEMORY_THRESHOLD;

	OP_ASSERT( freemem >= 0 );

	while (freemem < threshold)
	{
		threshold /= 2;
		severity++;
	}

	return severity;
}

#ifdef CONSTRAIN_OPERA_MEMORY_USAGE

// How much more opera may allocate before the allocation limit is reached.
static int bytes_left_to_alloc_limit()
{
	int alloc_limit = get_alloc_limit();
	if ( alloc_limit == INT_MAX )
		return INT_MAX;
	return alloc_limit-get_constrain_allocated_mem();
}


// How much more the heap may grow before the heap size limit is
// reached (a negative number if the heap is to large already...)
static int bytes_left_to_heap_limit()
{
	int heap_limit = get_heap_limit();
	if ( heap_limit == INT_MAX )
		return INT_MAX;

	struct mallinfo info = mallinfo();

	OP_ASSERT( info.arena >= 0 );
	OP_ASSERT( info.hblkhd >= 0 );

    // Takes into account both the size of the arena and the mmapped
    // memory that malloc uses for large allocations.
	return heap_limit - info.arena - info.hblkhd;
}

#endif // CONSTRAIN_OPERA_MEMORY_USAGE

#endif //__OPRBRIDGE_DLL__

#endif // OUT_OF_MEMORY_POLLING
