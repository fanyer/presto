/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#define COMPILING_LEA_MALLOC
#include "core/pch_system_includes.h"

#ifdef HAVE_DL_MALLOC // all of file.
#include "modules/lea_malloc/malloc.h"
#include "modules/lea_malloc/lea_malloc.h"

#ifdef STACK_TRACE_SUPPORT
#include "modules/memtools/stacktrace.h"
#endif

#ifndef USE_DL_PREFIX // FEATURE_LEA_MALLOC_PREFIX
# ifndef LEA_MALLOC_FULL_MEMAPI // API_DLM_FULL_MEMAPI
#  ifdef __GNUC__
#warning "Third-party libraries linked to at run-time may exercise parts of the malloc API that Opera doesn't."
#warning "You are compiling without dl-prefix but not supplying those parts of the API."
#warning "Consequently, such third-party libraries shall mix this module's API with the system one."
#warning "This shall cause crashes.  Turn on API_DLM_FULL_MEMAPI or FEATURE_LEA_MALLOC_PREFIX,"
#warning "unless you are certain you use no third-party code (including plugins)."
#  elif defined(_MSC_VER)
#pragma message("Third-party libraries linked to at run-time may exercise parts of the malloc API that Opera doesn't.")
#pragma message("You are compiling without dl-prefix but not supplying those parts of the API.")
#pragma message("Consequently, such third-party libraries shall mix this module's API with the system one.")
#pragma message("This shall cause crashes.  Turn on API_DLM_FULL_MEMAPI or FEATURE_LEA_MALLOC_PREFIX,")
#pragma message("unless you are certain you use no third-party code (including plugins).")
#  else
#error "I don't know how to warn you about a problem your configuration leaves you with.  Please read source file !"
/* FEATURE_LEA_MALLOC_PREFIX sort-of-conflicts with API_DLM_FULL_MEMAPI but
 * there's no way to tell the API system to enforce that (and the feature system
 * doesn't know what APIs you're using).  You should not need the full memapi
 * when using the prefix: it is only provided for the sake of external libraries
 * and plugins we may link against at run-time.  If Opera code is calling a
 * function only supplied when the API is activated, you need to get that
 * function moved into the standard API.
 */
#  endif
# endif // full API
#endif // prefix

extern "C" { // hmm ... is this necessary ? malloc.c does its own ...

/* Include platform specific configuration header file if needed, which
   can configure lea_malloc as needed. See 'dummy_config.h' for more
   documentation. */
#undef assert
#ifdef LEA_MALLOC_CONFIG_HEADER
# include LEA_MALLOC_CONFIG_HEADER
#endif

/* Give assert() sensible default define if not given in config file. */
#ifndef assert
# define assert(x) OP_ASSERT(x)
#endif

/* Enable debugging and extensive testing if the api API_DLM_CHECK is
   enabled. The extensive testing is fairly intrusive in terms of added CPU
   usage. */
#ifdef LEA_MALLOC_EXTERN_CHECK
# define LEA_MALLOC_CHECKING
#endif

#undef DEBUG /* malloc.c gives it a different meaning */
#ifdef _DEBUG
# define DEBUG 1
#else
# define DEBUG 0
#endif

/* The lea_malloc state is in the Opera memory state object */
#ifdef get_malloc_state
# error "get_malloc_state should not be externally defined when OpMemoryState is used"
#endif
#define get_malloc_state() ((mstate)(((OpMemoryState*)g_opera_memory_state)+1))

/* TWEAK_MEMORY_USE_LOCKING (MEMORY_USE_LOCKING) is equivalent to
   TWEAK_LEA_MALLOC_LOCK (USE_MALLOC_LOCK) */
#ifdef MEMORY_USE_LOCKING
# ifndef USE_MALLOC_LOCK
#  define USE_MALLOC_LOCK
# endif // !USE_MALLOC_LOCK
#endif // MEMORY_USE_LOCKING

/* Set up macros for calling external locking functions */
#ifdef USE_MALLOC_LOCK
# include "modules/pi/system/OpMemory.h"
# define malloc_mutex_lock() ( OpMemory::MallocLock(), 1 )
# define malloc_mutex_unlock() OpMemory::MallocUnlock()
#endif // USE_MALLOC_LOCK

#ifdef USE_DL_PREFIX
#define MALLOC_FAILURE_ACTION /* suppress setting errno on failure */
#endif /* but, without prefix, system functions call our malloc and may rely on errno */

#ifdef CONSTRAIN_OPERA_MEMORY_USAGE
#define MORECORE(size) constrained_sbrk(size)
#define MMAP(addr, size, prot, flags) (constrained_mmap((addr), (size), (prot), (flags)|MAP_ANONYMOUS, -1, 0))
static void constrain_tag_allocation(void *m);
static void clear_constrain_tag(void* m);
static void *constrained_sbrk(intptr_t increment);
static void *constrained_mmap(void *addr, size_t size, int prot , int flags, int fd, off_t offset);
# ifndef CHAR_BIT
#define CHAR_BIT 8
# endif
#define CONSTRAIN_TAG (1 << ( sizeof(INTERNAL_SIZE_T) * CHAR_BIT - 1)) /* Highest bit */
#define chunk_is_constrained(p) ((p)->size & CONSTRAIN_TAG)
#endif /* CONSTRAIN_OPERA_MEMORY_USAGE */

/* Enabling TWEAK_LEA_MALLOC_USE_OPERA_SBRK will cause lea-malloc
   to call the op_sbrk() function implemented by Opera as a system
   emulation layer. */
#ifdef LEA_MALLOC_USE_OPERA_SBRK
# ifdef MORECORE
#  error "MORECORE defined, but it needs to be forced for TWEAK_LEA_MALLOC_USE_OPERA_SBRK"
# endif
# define MORECORE(size) op_sbrk(size)
# undef  MORECORE_CONTIGUOUS
# define MORECORE_CONTIGUOUS 1
#endif

/* Enabling TWEAK_LEA_MALLOC_USE_OPERA_MMAP will cause lea-malloc
   to call the op_mmap() set of functions implemented by Opera as
   a system emulation layer. */
#ifdef LEA_MALLOC_USE_OPERA_MMAP
# if defined(MMAP) || defined(MUNMAP)
#  error "MMAP/MUNMAP defined, but they need to be forced for TWEAK_LEA_MALLOC_USE_OPERA_MMAP"
# endif
# define MMAP(addr, size, prot, flags) op_mmap(addr, size, prot, flags)
# define MUNMAP(addr, size) op_munmap(addr, size)
# undef  HAVE_MMAP
# define HAVE_MMAP 1
# undef  MMAP_CLEARS
# define MMAP_CLEARS 0
# undef  HAVE_MREMAP
# define HAVE_MREMAP 0
/* Fake constants for MMAP to compile, since our op_mmap() just ignores them */
# undef  MAP_PRIVATE
# define MAP_PRIVATE 1
# undef  PROT_READ
# define PROT_READ 1
# undef  PROT_WRITE
# define PROT_WRITE 2
#endif

/* When any of op_mmap() or op_sbrk() are used, the expected return value
   for failure needs to be (void*)-1. This means that if this value does
   not correspond to the error return value of the platform (when you use
   e.g. op_sbrk() and native mmap() on platform), there is a conflict
   and a thin wrapper-function that provides the expected error return
   value should be written for the system functions to solve it.

   We also need to handle the malloc_getpagesize macro correctly, since it
   will be needed. */
#if defined(LEA_MALLOC_USE_OPERA_SBRK) || defined(LEA_MALLOC_USE_OPERA_MMAP)
# ifdef MORECORE_FAILURE
#  error "MORECORE_FAILURE defined, but it needs to be forced for one or both of LEA_MALLOC_USE_OPERA_SBRK and LEA_MALLOC_USE_OPERA_MMAP"
# endif
# define MORECORE_FAILURE ((void*)-1)
# undef  malloc_getpagesize
# define malloc_getpagesize op_get_mempagesize()
#endif

/* Configure whether MMAP() should be attempted as a backup for a
   failed MORECORE(). This may or may not be desirable, depending
   on your system, available memory, OOM behaviour etc.
   The default is to use MMAP() as a backup. */
#ifdef LEA_MALLOC_MMAP_AS_BACKUP
# define MMAP_AS_MORECORE_BACKUP 1
#else
# define MMAP_AS_MORECORE_BACKUP 0
#endif

/*
 * Include the main source code for lea-malloc into this C++ compile unit.
 * The configuration settings from this file will then be used correctly
 * by the lea-malloc source code, without too many changes to the
 * lea-malloc source code itself.
 */
# include "modules/lea_malloc/malloc.c"

#ifdef LEA_MALLOC_API_GET_STATE_SIZE
size_t get_malloc_state_size()
{
	return sizeof(struct malloc_state);
}
#endif // LEA_MALLOC_API_GET_STATE_SIZE

#ifdef LEA_MALLOC_EXTERN_CHECK
void lea_malloc_check_state()
{
	MALLOC_WRAPPED_ACTION(check_malloc_state());
}
#endif

#ifdef LEA_MALLOC_FULL_MEMAPI // see API_DLM_FULL_MEMAPI
# ifndef USE_DL_PREFIX // No prefix on this one
// See bug 249297 and POSIX specs.
int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	if (!memptr) // We don't like segfaulting.
		return EINVAL;

	// The spec says it should fail if alignment isn't a power of two multiple of sizeof(void*):
	if (alignment % sizeof(void *) || (alignment & (alignment - 1)) != 0)
		return EINVAL;

	/* I assumes we should not modify errno, since our return value is what we
	 * would have set it to - Eddy/2007/Jan/29. */
	const int saved = errno;
	void *ret = memalign(alignment, size);
	*memptr = ret; // ... which is NULL on failure

	if (errno != saved)
	{
		int was = errno;
		errno = saved;
		return was;
	}
	if (ret == NULL && size > 0)
	{
		OP_ASSERT(!"We should have set errno on failure !");
		return ENOMEM;
	}

	OP_ASSERT(0 == ((INTPTR)*memptr) % alignment);
	return 0;
}
# endif
#endif // LEA_MALLOC_FULL_MEMAPI

#ifdef CONSTRAIN_OPERA_MEMORY_USAGE

#include <stdlib.h>
#include <limits.h>

/* Restricts the heap size. Constrained allocations will fail rather than grow
 * the heap beyond this size. */
int g_opera_heap_limit = -1;

/* Restricts the total amount constrain allocated memory.  Constrained
 * allocations will fail rather than grow the amount of constrain-allocated
 * memory past this limit. */
int g_opera_alloc_limit = -1;

/* The total number of bytes that is currently constrain allocated */
static size_t g_constrain_allocated_mem;

/* Set to 1 when a constrained allocation is ongoing, 0 otherwise */
static int g_constrained_allocation;

/* The total number of bytes allocated externally (i.e. reported with
 * external_alloc(). Such memory counts towards both the heap and
 * allocation limits. It is not reflected in mallinfo() or
 * get_malloc_state(), so this has to be manually added onto all
 * counts of total memory (see get_heap_size()). */
static size_t g_external_allocated_mem;

#if DEBUG
#define LOG_CONSTRAINED_ALLOCATIONS
#endif

/* Keep this defined for a while. We want this in release builds so that we can test the constrained memory system */
#define SHOW_MEM_INFO

#  ifdef SHOW_MEM_INFO
static void show_mem_info(int heap_size);
static int g_show_mem_info = -1;
#    ifdef STACK_TRACE_SUPPORT
static int g_show_mem_info_stack = -1;
#    endif /* STACK_TRACE_SUPPORT */
static int g_max_constrain_allocated_mem;
#  endif /* SHOW_MEM_INFO */

/*************************************************
 * Functions used by constrained lea malloc.
 ************************************************/

/** tags the allocated memory and adjusts the allocation count */
static void constrain_tag_allocation(void *m)
{
	if ( m == NULL )
		return;

	mchunkptr p = mem2chunk(m);

	assert(!(p->size & CONSTRAIN_TAG));
	p->size |= CONSTRAIN_TAG;

	g_constrain_allocated_mem += chunksize(p);

#ifdef SHOW_MEM_INFO
	if ( g_show_mem_info && (g_constrain_allocated_mem > g_max_constrain_allocated_mem) )
	{
		show_mem_info(0);
		g_max_constrain_allocated_mem = g_constrain_allocated_mem;
	}
#endif

}

/** clears the constrain tag and adjusts the allocation count */
static void clear_constrain_tag(void* m)
{
	if ( m == NULL )
		return;

	mchunkptr p = mem2chunk(m);

	if ( p->size & CONSTRAIN_TAG )
	{
		p->size &= ~CONSTRAIN_TAG;
		g_constrain_allocated_mem -= chunksize(p);
	}
}

static void *constrained_sbrk(intptr_t increment)
{
	/* increment == 0 is used to find the current break, so we must always let
	 * it through, and it should ofcourse always be possible to decrease the
	 * heap size...
	 */
	int heap_increase_allowed = !g_constrained_allocation || increment <= 0 || (get_heap_limit() >= (get_heap_size() + increment));

	if ( heap_increase_allowed )
	{
#ifdef SHOW_MEM_INFO
		if ( g_show_mem_info )
		{
			mstate av = get_malloc_state();
			int new_total = av->mmapped_mem + av->sbrked_mem + increment;
			if ( av->max_total_mem < new_total )
				show_mem_info(new_total);
		}
#endif
		return sbrk(increment);
	}
	return (void*) MORECORE_FAILURE;
}

static void  *constrained_mmap(void *addr, size_t size, int prot , int flags, int fd, off_t offset)
{
	int heap_increase_allowed = !g_constrained_allocation || (get_heap_limit() >= (get_heap_size() + size));

	if ( heap_increase_allowed )
	{
#ifdef SHOW_MEM_INFO
		if ( g_show_mem_info )
		{
			mstate av = get_malloc_state();
			int new_total = av->mmapped_mem + av->sbrked_mem + size;
			if ( av->max_total_mem < new_total )
				show_mem_info(new_total);
		}
#endif
		return mmap(addr, size, prot, flags, fd, offset);
	}

	return (void*) MORECORE_FAILURE;
}

/* opera_malloc() is an alias to malloc in this file.  If we for some reason are
 * linked against another malloc opera_malloc() will still point to our malloc.
 * We use that fact to check if we are using our own allocator or not.  It is
 * very important that we do not mix allocators.  It will mess up the internal
 * data structures.  When we are not using the constrained allocator we normaly
 * end up using either system allocator or our lea malloc (depending on how
 * things are linked).  One of them always replace all the malloc functions of
 * the other.  With constrained malloc things are a bit different.  The
 * constrained_* functions does not call malloc(), but rather the internal
 * mALLOc() function.  This is done so that we can reuse the malloc lock to
 * guard accesses to g_constrained_allocation.  Unfortunately, the system
 * allocator does not provide the constrained_* functions, so we always use lea
 * malloc for constrained allocations.  It is bad, because if we have linked
 * against the system allocator we end up using two allocators at the same time.
 * Both are accessing the heap, and most likely conflicting with each other.
 * Even worse is that free() from the system allocator will get constrain
 * allocated memory (lea malloc allocated).  That is almost certain to crash the
 * system allocator.  Especially since the memory is constrain tagged.
 */
void *opera_malloc(size_t nbytes) __attribute__ ((weak, alias ("malloc")));

/* If using_opera_allocator() isn't true the constrained_*() functions fallback
 * to use regular malloc() instead.  Ofcourse that means that Opera won't
 * enforce the memory limits, but atleast we won't crash.
 */
#define using_opera_allocator() (opera_malloc == malloc)


/*********************************************************
 * Debug functions
 *********************************************************/

#ifdef SHOW_MEM_INFO
/* We need the heap_size argument because the heap size is not up to date when
 * we call this function from constrained_sbrk() or constrained_mmap().
 */
static void show_mem_info(int heap_size)
{
	if ( g_show_mem_info == 0 )
		return;

	if ( g_show_mem_info == -1)
	{
		g_show_mem_info = getenv("OPERA_SHOW_MEM_INFO") ? 1 : 0;
		if ( g_show_mem_info == 0 )
			return;
	}

	mstate av = get_malloc_state();
	if ( heap_size == 0 )
	{
		heap_size = av->mmapped_mem + av->sbrked_mem;
	}

	fprintf(stderr, "Heap size is %d\n",  heap_size);
	fprintf(stderr, "Opera allocated memory is %d\n", g_constrain_allocated_mem);

#ifdef STACK_TRACE_SUPPORT

	if ( g_show_mem_info_stack == 0 )
		return;

	if (g_show_mem_info_stack == -1)
	{
		g_show_mem_info_stack = getenv("OPERA_SHOW_MEM_INFO_STACK") ? 1 : 0;
		if ( g_show_mem_info_stack == 0 )
			return;
	}

	if ( heap_size > get_heap_limit() )
	{
		fprintf(stderr,"Exceeded heap limit at:\n");
		show_stack_trace(15,0);
	}
	if ( heap_size == 0 && g_constrain_allocated_mem > get_alloc_limit() )
	{
		fprintf(stderr,"Exceeded alloc limit at:\n");
		show_stack_trace(15,0);
	}
#endif
}
#endif // SHOW_MEM_INFO

#ifdef LOG_CONSTRAINED_ALLOCATIONS
static int g_log_oom = -1;
static int g_log_alloc_counter=0;
static void log_alloc(void* addr)
{
	if ( addr == NULL )
	{
#ifdef STACK_TRACE_SUPPORT
		if ( g_log_oom == 0 )
			return;

		if (g_log_oom == -1)
		{
			g_log_oom = getenv("OPERA_LOG_OOM") ? 1 : 0;
			if ( g_log_oom == 0 )
				return;
		}

		fprintf(stderr,"OOM at:\n");
		show_stack_trace(15,0);
#endif // STACK_TRACE_SUPPORT
		g_log_alloc_counter++; /* A place to put a breakpoint. Do something so the code isn't optimized away. */

	}
}
#define LOG_ALLOC(addr) log_alloc(addr)
#else
#define LOG_ALLOC(addr)
#endif

/*********************************************************************
 * Constrained version of the allocation functions.
 *
 * The normal allocation functions are still available and used by
 * default - they will "never" OOM.  Some parts of Opera are changed
 * to use the constrained allocation functions - they will OOM.
 *
 ********************************************************************/
#define alloc_within_limits(bytes) (g_constrain_allocated_mem + request2size((bytes)) <= get_alloc_limit())

#ifndef MALLOC_WRAPPED_ACTION
#define MALLOC_WRAPPED_ACTION(code) code
#endif

/* malloc() replacment functions are found below:
 * operator new() and operator new[]() are defined in malloc_extra.cpp
 * because this code is inside extern "C" { ... }
 */
void *constrained_malloc( size_t nbytes )
{
	if ( !using_opera_allocator() )
		return malloc(nbytes);

	void *res = NULL;

	MALLOC_WRAPPED_ACTION (
		if ( alloc_within_limits(nbytes) )
	    {
		    g_constrained_allocation = 1;
			res = mALLOc(nbytes);
			g_constrained_allocation = 0;
			constrain_tag_allocation(res);
		}
	);
	LOG_ALLOC(res);
	return res;
}

void *constrained_calloc( size_t nobj, size_t objsize )
{
	if ( !using_opera_allocator() )
		return calloc(nobj, objsize);

	void *res = NULL;

	MALLOC_WRAPPED_ACTION(
		if ( alloc_within_limits(nobj*objsize) )
		{
			g_constrained_allocation = 1;
			res = cALLOc(nobj, objsize);
			g_constrained_allocation = 0;
			constrain_tag_allocation(res);
		}
	);
	LOG_ALLOC(res);
	return res;
}

void *constrained_realloc( void *obj, size_t nbytes )
{
	if ( !using_opera_allocator() )
		return realloc(obj, nbytes);

	void *res = NULL;

	MALLOC_WRAPPED_ACTION(
		clear_constrain_tag(obj); /* So that we check against the correct size */
		if ( alloc_within_limits(nbytes) )
		{
			g_constrained_allocation = 1;
			res = rEALLOc(obj, nbytes);
			g_constrained_allocation = 0;
		}
		constrain_tag_allocation(res ? res : obj); /* retag original memory on failure */
	);
	LOG_ALLOC(res);
	return res;
}

void* constrained_memalign(size_t alignment, size_t n)
{
	if ( !using_opera_allocator() )
		return memalign(alignment, n);

	void *res = NULL;

	MALLOC_WRAPPED_ACTION(
		if ( alloc_within_limits(n) )
		{
			g_constrained_allocation = 1;
			res = mEMALIGn(alignment, n);
			g_constrained_allocation = 0;
			constrain_tag_allocation(res);
		}
	);
	LOG_ALLOC(res);
	return res;
}

#include <string.h>
char *constrained_strdup( const char *s )
{
	if ( !using_opera_allocator() )
		return strdup(s);

	if ( s == NULL )
		return NULL;

	char *res = NULL;
	int len = strlen(s) + 1;

	MALLOC_WRAPPED_ACTION(
		if ( alloc_within_limits(len) )
		{
			g_constrained_allocation = 1;
			res = (char*) mALLOc(len);
			g_constrained_allocation = 0;
			constrain_tag_allocation(res);
		}
	);

	if ( res != NULL )
		strcpy(res, s);
	LOG_ALLOC(res);
	return res;
}

/*********************************************************************
 * Additional API functions.
 ********************************************************************/
int is_using_opera_allocator()
{
	return using_opera_allocator();
}

int get_alloc_limit()
{
	if ( g_opera_alloc_limit == -1 )
	{
		const char *val = getenv("OPERA_ALLOC_LIMIT");
		if ( val == NULL )
			val = getenv("OPERA_RESERVED_MEM"); // Fallback
		if ( val == NULL )
			g_opera_alloc_limit = INT_MAX;
		else
			g_opera_alloc_limit = atoi(val);
	}
	return g_opera_alloc_limit;
}

void set_alloc_limit(int limit)
{
	g_opera_alloc_limit = limit;
}

int get_heap_limit()
{
	if ( g_opera_heap_limit == -1 )
	{
		const char *val = getenv("OPERA_HEAP_LIMIT");
		if ( val == NULL )
			val = getenv("OPERA_RESERVED_MEM"); // Old name.
		if ( val == NULL )
			g_opera_heap_limit = INT_MAX;
		else
			g_opera_heap_limit = atoi(val);
	}
	return g_opera_heap_limit;
}

void set_heap_limit(int limit)
{
	g_opera_heap_limit = limit;
}

int get_constrain_allocated_mem()
{
	return g_constrain_allocated_mem;
}

int get_heap_size()
{
	mstate av = get_malloc_state();
	return av->sbrked_mem + av->mmapped_mem + g_external_allocated_mem;
}

static int internal_try_alloc(size_t bytes)
{
	mstate av = get_malloc_state();

	INTERNAL_SIZE_T nb;               /* normalized request size */
	unsigned int    idx;              /* associated bin index */
	mbinptr         bin;              /* associated bin */
	mfastbinptr*    fb;               /* associated fastbin */

	mchunkptr       victim;           /* inspected/selected chunk */
	INTERNAL_SIZE_T size;             /* its size */
	int             victim_index;     /* its bin index */

	mchunkptr       remainder;        /* remainder from a split */
	CHUNK_SIZE_T    remainder_size;   /* its size */

	unsigned int    block;            /* bit map traverser */
	unsigned int    bit;              /* bit map traverser */
	unsigned int    map;              /* current word of binmap */

	mchunkptr       fwd;              /* misc temp for linking */
	mchunkptr       bck;              /* misc temp for linking */


	if ( g_constrained_allocation && !alloc_within_limits(bytes) )
		return 0;

	/*
	  Convert request size to internal form by adding SIZE_SZ bytes overhead
	  plus possibly more to obtain necessary alignment and/or to obtain a size
	  of at least MINSIZE, the smallest allocatable size. Also,
	  checked_request2size traps (returning 0) request sizes that are so large
	  that they wrap around zero when padded and aligned.
	*/

	checked_request2size(bytes, nb);


	/* FIXME: We should do the things in sYSMALLOc() too, to get everything exact, but we cheat a bit... */

	if ( (get_heap_limit() >= (get_heap_size() + nb + MINSIZE)) )
		return 1;


	/* Bypass search if no frees yet */
	if (!have_anychunks(av)) {
		if (av->max_fast == 0) /* initialization check */
			malloc_consolidate(av);
		goto use_top;
	}

  /*
    If the size qualifies as a fastbin, first check corresponding bin.
  */

  if ((CHUNK_SIZE_T)(nb) <= (CHUNK_SIZE_T)(av->max_fast)) {
    fb = &(av->fastbins[(fastbin_index(nb))]);
    if ( (victim = *fb) != 0) {
		// There is a free fastbin.
		return 1;
    }
  }

  /*
    If a small request, check regular bin.  Since these "smallbins" hold one
    size each, no searching within bins is necessary.  (For a large request, we
    need to wait until unsorted chunks are processed to find best fit. But for
    small ones, fits are exact anyway, so we can check now, which is faster.)
  */

  if (in_smallbin_range(nb)) {
    idx = smallbin_index(nb);
    bin = bin_at(av,idx);

    if ( (victim = last(bin)) != bin) {
		// There is a room in a smallbin.
		return 1;
    }
  }

  /*
     If this is a large request, consolidate fastbins before continuing.  While
     it might look excessive to kill all fastbins before even seeing if there is
     space available, this avoids fragmentation problems normally associated
     with fastbins.  Also, in practice, programs tend to have runs of either
     small or large requests, but less often mixtures, so consolidation is not
     invoked all that often in most programs. And the programs that it is called
     frequently in otherwise tend to fragment.
  */

  else {
    idx = largebin_index(nb);
    if (have_fastchunks(av))
      malloc_consolidate(av);
  }

  /*
    Process recently freed or remaindered chunks, taking one only if it is exact
    fit, or, if this a small request, the chunk is remainder from the most
    recent non-exact fit.  Place other traversed chunks in bins.  Note that this
    step is the only place in any routine where chunks are placed in bins.
  */

  while ( (victim = unsorted_chunks(av)->bk) != unsorted_chunks(av)) {
    bck = victim->bk;
    size = chunksize(victim);

    /*
       If a small request, try to use last remainder if it is the only chunk in
       unsorted bin.  This helps promote locality for runs of consecutive small
       requests. This is the only exception to best-fit, and applies only when
       there is no exact fit for a small chunk.
    */

    if (in_smallbin_range(nb) &&
        bck == unsorted_chunks(av) &&
        victim == av->last_remainder &&
        (CHUNK_SIZE_T)(size) > (CHUNK_SIZE_T)(nb + MINSIZE)) {

		/* There is room in the remainder */
      return 1;
    }

    /* Take now instead of binning if exact fit */

    if (size == nb) {
		/* Found an exact fit */
      return 1;
    }

    /* remove from unsorted list */
    unsorted_chunks(av)->bk = bck;
    bck->fd = unsorted_chunks(av);

	/* place chunk in bin */

    if (in_smallbin_range(size)) {
      victim_index = smallbin_index(size);
      bck = bin_at(av, victim_index);
      fwd = bck->fd;
    }
    else {
      victim_index = largebin_index(size);
      bck = bin_at(av, victim_index);
      fwd = bck->fd;

      if (fwd != bck) {
        /* if smaller than smallest, place first */
        if ((CHUNK_SIZE_T)(size) < (CHUNK_SIZE_T)(bck->bk->size)) {
          fwd = bck;
          bck = bck->bk;
        }
        else if ((CHUNK_SIZE_T)(size) >=
                 (CHUNK_SIZE_T)(FIRST_SORTED_BIN_SIZE)) {

          /* maintain large bins in sorted order */
          size |= PREV_INUSE; /* Or with inuse bit to speed comparisons */
          while ((CHUNK_SIZE_T)(size) < (CHUNK_SIZE_T)(fwd->size))
            fwd = fwd->fd;
          bck = fwd->bk;
        }
      }
    }

    mark_bin(av, victim_index);
    victim->bk = bck;
    victim->fd = fwd;
    fwd->bk = victim;
    bck->fd = victim;
  }

  /*
    If a large request, scan through the chunks of current bin to find one that
    fits.  (This will be the smallest that fits unless FIRST_SORTED_BIN_SIZE has
    been changed from default.)  This is the only step where an unbounded number
    of chunks might be scanned without doing anything useful with them. However
    the lists tend to be short.
  */

  if (!in_smallbin_range(nb)) {
    bin = bin_at(av, idx);

    for (victim = last(bin); victim != bin; victim = victim->bk) {
      size = chunksize(victim);

      if ((CHUNK_SIZE_T)(size) >= (CHUNK_SIZE_T)(nb)) {
          return 1;
      }
    }
  }

  /*
    Search for a chunk by scanning bins, starting with next largest bin. This
    search is strictly by best-fit; i.e., the smallest (with ties going to
    approximately the least recently used) chunk that fits is selected.

    The bitmap avoids needing to check that most blocks are nonempty.
  */

  ++idx;
  bin = bin_at(av,idx);
  block = idx2block(idx);
  map = av->binmap[block];
  bit = idx2bit(idx);

  for (;;) {

    /* Skip rest of block if there are no more set bits in this block.  */
    if (bit > map || bit == 0) {
      do {
        if (++block >= BINMAPSIZE)  /* out of bins */
          goto use_top;
      } while ( (map = av->binmap[block]) == 0);

      bin = bin_at(av, (block << BINMAPSHIFT));
      bit = 1;
    }

    /* Advance to bin with set bit. There must be one. */
    while ((bit & map) == 0) {
      bin = next_bin(bin);
      bit <<= 1;
      assert(bit != 0);
    }

    /* Inspect the bin. It is likely to be non-empty */
    victim = last(bin);

    /*  If a false alarm (empty bin), clear the bit. */
    if (victim == bin) {
      av->binmap[block] = map &= ~bit; /* Write through */
      bin = next_bin(bin);
      bit <<= 1;
    }
    else {
		return 1;
    }
  }

  use_top:
  /*
    If large enough, split off the chunk bordering the end of memory (held in
    av->top). Note that this is in accord with the best-fit search rule.  In
    effect, av->top is treated as larger (and thus less well fitting) than any
    other available chunk since it can be extended to be as large as necessary
    (up to system limitations).

    We require that av->top always exists (i.e., has size >= MINSIZE) after
    initialization, so if it would otherwise be exhuasted by current request, it
    is replenished. (The main reason for ensuring it exists is that we may need
    MINSIZE space to put in fenceposts in sysmalloc.)
  */

  victim = av->top;
  size = chunksize(victim);

  if ((CHUNK_SIZE_T)(size) >= (CHUNK_SIZE_T)(nb + MINSIZE)) {
    return 1;
  }

  /* The allocation wouldn't fit inside the heap */
  return 0;
}

int try_alloc(size_t nbytes)
{
  if ( !using_opera_allocator() )
	  return 1;

  int res;

  MALLOC_WRAPPED_ACTION(res = internal_try_alloc(nbytes));

  return res;
}

int fake_alloc(size_t nbytes)
{
  if ( !using_opera_allocator() )
	  return 1;

  int res;

  MALLOC_WRAPPED_ACTION(
	  g_constrained_allocation = 1;
	  res = internal_try_alloc(nbytes);
	  g_constrained_allocation = 0;
	  if ( res )
	  {
		  g_constrain_allocated_mem += request2size(nbytes);
	  }
  );
  // LOG_ALLOC(res); /* This logs "address" 1 on success and 0 on failure... */
  return res;
}

void fake_free(size_t nbytes)
{
  if ( !using_opera_allocator() )
	  return;

  MALLOC_WRAPPED_ACTION(g_constrain_allocated_mem -= request2size(nbytes));
}

int external_alloc(size_t nbytes)
{
	if ( !using_opera_allocator() )
		return 1;

	int res = 0;

	MALLOC_WRAPPED_ACTION(
		if (get_heap_limit() >= (get_heap_size() + nbytes) &&
			get_alloc_limit() >= (g_constrain_allocated_mem + nbytes))
		{
			res = 1;
			g_external_allocated_mem += nbytes;
			g_constrain_allocated_mem += nbytes;
		}
	);
	// LOG_ALLOC(res); /* This logs "address" 1 on success and 0 on failure... */
	return res;
}

void external_free(size_t nbytes)
{
	if ( !using_opera_allocator() )
		return;

	MALLOC_WRAPPED_ACTION(
		g_external_allocated_mem -= nbytes;
		g_constrain_allocated_mem -= nbytes;
	);
}
#endif // CONSTRAIN_OPERA_MEMORY_USAGE

} // extern "C"
#endif // HAVE_DL_MALLOC
