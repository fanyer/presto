/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2001-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Stupid but reasonable memory allocator, to track memory use.
 * Lars T Hansen
 *
 *
 *   *************************************************************
 *   *                                                           *
 *   *         DO NOT USE THIS FOR PRODUCTION BUILDS!            *
 *   *                                                           * 
 *   *************************************************************
 *
 *
 * Platforms currently supported:
 *   QNX
 *   Unix/Linux
 *   Win32
 *   MacOS X (TARGET_RT_MAC_CFM)
 *
 * Normally the most one has to do to port the malloc is to:
 *   - Add a case in allocate_from_os() in this file
 *   - Add a case in GET_CALLER() in modules/memtools/happy-malloc.h if one wants
 *     to use the RECORD_ALLOC_SITE functionality
 *
 * Usage:
 *   Win32:   Add HAVE_LTH_MALLOC and RECORD_ALLOC_SITE to the proj settings
 *            Add dbug/malloc.cpp to the project sources
 *            Link with /force:multiple
 *            Note: The exit log is written to file, the file name never varies.
 *                  This may be a problem if you instrument several independent
 *                  parts of Opera during the same run.
 *
 *   MacOS X: Arrange to call summarize_stats_at_exit() from outside the file
 */

/* To do:
 *
 *  - synthesize OOM conditions based on call stack, not just call site
 *  - better blacklisting of call stack configurations for the simulation?
 *  - periodic coalescing and splitting when a lot of memory is on the 
 *    free list, to allow the program to run longer on small systems
 *  - separate the RNG for synthesized failure from the library RNG,
 *    since if Opera calls the RNG then we may get unreproducible
 *    sequences 
 *  - support for MacOS
 *  - support for EPOC
 *  - more reliable locking for multi-threaded environments?
 *  - more sophisticated protected allocation?
 */

#include "core/pch.h"

#ifdef HAVE_LTH_MALLOC

#include "modules/pi/OpSystemInfo.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modules/memtools/happy-malloc.h"
#ifdef SHOW_OOM_STACK_TRACE
#include "modules/memtools/stacktrace.h"
#define OOM_STACK_DEPTH 10
#endif
#ifdef ADDR2LINE_SUPPORT
#include "modules/memtools/addr2line.h"
#endif

#if defined(__linux__)
#  if defined(__i386__)
#    define LINUX_LIB_SPACE ((char*) 0x09000000) // Call addresses above this are from libraries
#  endif
#  define ADDR_IN_LIB(addr) ((addr) >= LINUX_LIB_SPACE)
#endif

#if defined(PARTIAL_LTH_MALLOC)
#  include <dlfcn.h>
#  if defined (USE_STDLIB_MALLOC_FOR_LIBS)
#    define USE_STDLIB_MALLOC() (in_non_oom_code() || ADDR_IN_LIB(site.location))
#  else
#    define USE_STDLIB_MALLOC() (in_non_oom_code())
#  endif
#endif

# if defined(PARTIAL_LTH_MALLOC) && !defined(_NO_THREADS_) && defined(_PTHREADS_)
pthread_key_t is_non_oom_code_key;
pthread_key_t silent_oom_key;
#endif

#if defined (__QNX__)
typedef void (*new_handler)(); // handler for operator new failures
new_handler set_new_handler(new_handler);
#else
# include <new>
using std::bad_alloc;
using std::nothrow_t;
using std::nothrow;
using std::new_handler;
using std::set_new_handler;
#endif

#ifndef MSWIN
# include <unistd.h>
#endif /* !MSWIN */

#if defined(HAVE_MPROTECT) && defined(HAVE_MMAP) || defined(QNX_PROTECTED_MALLOC)
# include <sys/mman.h>
#endif

#ifdef NEEDS_RISC_ALIGNMENT
# define ALIGNMENT           (sizeof(double))
#else
# define ALIGNMENT           (sizeof(void*))
#endif

#define BLOCK_SIZE          (256*1024)		/* Unit to get from OS */
#define SMALL_BLOCK         (16*1024)		/* Bigger than this and it gets its own block */
#define NLISTS				512				/* Number of quick lists */
#define NEW_PATTERN         0xcccccccc      /* Freshly allocated */
#define FREE_PATTERN        0xdeadbeef      /* On free list */

#ifdef SYSTEM_64BIT
# define MAGIC1              ((void*)0x0badbad00badbad0)  /* Tag allocated blocks */
# define MAGIC2              ((void*)0xabbaabbaabbaabba)  /* Tag allocated blocks */
# define PROTECTED_MAGIC     0xd01ef00dd01ef00d      /* Page allocated with protected_malloc */
# ifdef PARTIAL_LTH_MALLOC
#  define STDLIB_MAGIC1      0xdeadfeaddeadfead
#  define STDLIB_MAGIC2      0xfafafafafafafafa
# endif // PARTIAL_LTH_MALLOC
#else
# define MAGIC1              ((void*)0x0badbad0)  /* Tag allocated blocks */
# define MAGIC2              ((void*)0xabbaabba)  /* Tag allocated blocks */
# define PROTECTED_MAGIC     0xd01ef00d      /* Page allocated with protected_malloc */
# ifdef PARTIAL_LTH_MALLOC
#  define STDLIB_MAGIC1      0xdeadfead
#  define STDLIB_MAGIC2      0xfafafafa
# endif // PARTIAL_LTH_MALLOC
#endif

#include "modules/util/excepts.h"

#ifndef MAX
# define MAX( a, b )		((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN( a, b )		((a) < (b) ? (a) : (b))
#endif
#define ADDR(val)			((char*)val)

/* Fundamental allocation routines */
#define TAG_MALLOC			MALLOC_TAG_MALLOC
#define TAG_NEW				MALLOC_TAG_NEW
#define TAG_ANEW			MALLOC_TAG_ANEW

/* Mask off an ID to get allocation routine */
#define TAG_MASK			3

/* Identifies the routine that called the allocator, for some special ones */
/* Low two bits of all of these must be zero */
#define TAG_STRDUP			MALLOC_TAG_STRDUP
#define TAG_SETSTR			MALLOC_TAG_SETSTR

/* This is used for allocation sites, but if the debugging opstatus is
   enabled then it is also used for sites at which the OP_STATUS
   destructor is called without the variable having been checked.
*/
typedef struct {
	site_t    site;
	INT64     allocated;			/* Bytes allocated */
	INT64     freed;				/* Bytes freed */
	INT64     nallocated;			/* Number allocated */
	INT64     nfreed;             /* Number freed */
	INT64     peak;				/* Peak live */
	int       npeak;				/* Number live at peak */
#ifdef SYNTHESIZE_FAILURE
	int       failures;           /* Number of synthesize failures */
#endif
#ifdef USE_DEBUGGING_OP_STATUS
	int       opstatus_failures;  /* At this site */
#endif
#ifdef PROFILE_SENT_MESSAGES
	INT64     sent_messages[SENTMSG_NTYPES];	/* One counter per message type */
#endif
} site_stats_t;

typedef struct {
#ifdef DETECT_UNDERRUNS
	void         *magic1;			/* for detecting underruns */
#endif
	/* We can possibly save another word here by moving the tag into the site
	   stats and then split a bit field between the object size and a site
	   index (rather than pointer).  This works if we can keep the site index
	   to about 12 bits (4096 sites), which leaves room for objects up to
	   1MB in size.   (That's not enough for the current PNG decoder.)
	   4K sites may not be enough if we run with DEBUGGING_OP_STATUS
	   and/or PROFILE_SENT_MESSAGES, however.

	   Don't laugh :-)  These things matter.
	*/
	struct {
		unsigned int tag:4;			/* info about allocation method */
		unsigned int size:28;		/* size in bytes including header */
	} info;
#ifdef RECORD_ALLOC_SITE
	site_stats_t *site;			/* info about the site that allocated this object */
#endif
#ifdef DETECT_OVERRUNS
	void         **endmagic;      /* Make it easier to check the end pattern in gdb */
#endif
#ifdef DETECT_UNDERRUNS
	void         *magic2;			/* for detecting underruns */
#endif
} header_t;

typedef struct node {
	header_t     header;
	struct node *next;
} node_t;

typedef struct block {
	char         *free;
	char         *limit;
	struct block *next;
} block_t;

static struct {
	node_t     *freelist_head;
#if defined(DELAY_REUSE)
	node_t     *freelist_tail;
#endif
	block_t    *blocklist;
	int        accumulator;
	INT64      nbytes_peak;           /* Peak sum of alloc, new, anew */
	INT64      nbytes_alloc;          /* Currently allocated by {m,c,re}alloc */
	INT64      nbytes_alloc_total;    /* Total allocated by {m,c,re}alloc */
	INT64      nbytes_free_total;     /* Total freed by free */
#ifdef CPLUSPLUS
	INT64      nbytes_new;            /* Currently allocated by new */
	INT64      nbytes_anew;           /* Currently allocated by array new */
	INT64      nbytes_new_total;      /* Total allocated by new */
	INT64      nbytes_anew_total;     /* Total allocated by array new */
	INT64      nbytes_delete_total;   /* Total freed by delete */
	INT64      nbytes_adelete_total;  /* Total freed by array delete */
#endif
	INT64	   nobj_alloc_total;      /* Total #obj allocated by any routine */
	INT64      nobj_free_total;       /* Total #obj freed by any routine */
	INT64      nbytes_freelist_free;  /* Currently free on free list */
	INT64      nbytes_block;          /* Memory allocated to blocks, incl overhead*/
	INT64      nbytes_block_free;     /* Bytes free in blocks */
	INT64      nbytes_block_frag;     /* Bytes unusable in blocks */
	INT64      heapmax;          		/* Fail when heap grows above this. 0=disable */
#ifdef SYNTHESIZE_FAILURE
	INT64      synth_minalloc;        /* Minimum allocation before synthesizing failure */
	INT64      synth_minlive;         /* Minimum live before synthesizing failure. */
#endif
} stats; 

#ifdef QUICK_LISTS
static struct {
	node_t *head;
	node_t *tail;
} quick_lists[ NLISTS ];
#endif

#if defined(PARTIAL_LTH_MALLOC)

static struct {
	INT64 nbytes_peak;
	INT64 nbytes_in_use;
	INT64 nbytes_allocated_total;
} stdlib_stats;

const size_t stdlib_allocation_extra_size = 2 * sizeof(void*) + sizeof(size_t);
#if defined(_NO_THREADS_) || !defined(_PTHREADS_)
static BOOL is_non_oom_code = FALSE;
static BOOL silent_oom = FALSE;
#endif
static BOOL initializing_stdlib_malloc = FALSE;
static BOOL stdlib_malloc_initialized = FALSE;

void* (*stdlib_malloc)(size_t size) = NULL;
void  (*stdlib_free)(void *ptr) = NULL;
void* (*stdlib_realloc) (void *ptr, size_t size) = NULL;
char* (*stdlib_strdup) (const char *s) = NULL;

#endif // PARTIAL_LTH_MALLOC


#if defined(SELFTEST)
unsigned int illegal_free_count = 0;
#endif

#if defined(SYNTHESIZE_FAILURE)
int blacklisted_site = 0;				/* Manipulate with GDB, see comments below */
int synthesized_failures_disabled = 0;	/* Manipulate with GDB */
#endif

#if defined(HAVE_MPROTECT) && defined(HAVE_MMAP) || defined(QNX_PROTECTED_MALLOC)
static BOOL g_malloc_protect_all_allocations = FALSE;
#endif

#define ALIGN(nbytes) ((nbytes + ALIGNMENT - 1) & ~(ALIGNMENT-1))
#define ALIGNP(p)     ((char*)(((unsigned long)(p) + ALIGNMENT - 1) & ~(ALIGNMENT-1)))
/* Cast to double to support fprintf with poor support for long long (Codewarrior on Mac) */
#define KB(x)         ((double)((x)/1024.0))

#if defined(REENTRANT)
# if defined(UNIX)
#  include "platforms/unix/base/common/mt.h"
Mutex mutex;
#  define MALLOC_LOCK() do { mutex.Acquire(); } while(0)
#  define MALLOC_UNLOCK() do { mutex.Release(); } while(0)
# else
#  define MALLOC_LOCK() do { while (critical); critical=1; } while(0)
#  define MALLOC_UNLOCK() critical=0
# endif
#else
# define MALLOC_LOCK() ((void)0)
# define MALLOC_UNLOCK() ((void)0)
#endif

static void *operator_new_implementation( size_t nbytes, site_t site );
static void *operator_anew_implementation( size_t nbytes, site_t site );
static void *allocate_instrumented( size_t nbytes, INT64 *acct1, INT64 *acct2, unsigned int tag, site_t site );
static void free_instrumented( void *datum, INT64 *acct, INT64 *nact, unsigned int tag );
static void *allocate_from_os( size_t nbytes );
static void *allocate_dedicated_block( size_t nbytes );
static int  allocate_block();
static site_stats_t *lookup_allocation_site( site_t site );
#ifdef RECORD_ALLOC_SITE_STACK
#include "stackset.h"
#define NSTACK_ADDRS_MAX 400000
#define NSTACK_ADDRS_START 25000
static StackSet stackset;
BOOL stackset_initialized = FALSE;

static site_t lookup_stack(void *stack[]);
static BOOL is_stack_site(site_t site);
static void do_summarize_allocation_stack_sites( FILE *output, int nsites );
void summarize_allocation_stack_sites( FILE *output );
#endif
static void do_summarize_allocation_sites( FILE *output, int nsites );
static int getenv_value(const char *name, INT64 *value);
static void initialize_size_stats();
#ifdef PARTIAL_LTH_MALLOC
static void init_stdlib_malloc();
#endif

void summarize_allocation_sites( FILE *output );
void dump_allocation_sites();

#if TARGET_RT_MAC_CFM || defined MSWIN
void    show(FILE *ignoredStream, const char *format, ...);
#else
#define show  fprintf
#endif

static int malloc_initialized = 0;
#ifdef REENTRANT
static volatile int critical = 0;
#endif

#if defined(__linux__)
# define local_qsort happy_qsort  // Linux qsort sometimes allocates memory. It can cause problems in happy malloc.

static char *happy_partition(void *base, size_t size, int(*compar)(const void *, const void *),
					  void *first, void *last)
{
    char pivot[size]; /* ARRAY OK 2009-06-18 johanh */
	char *bottom = (char *) first - size;
	char *top = (char *) last;

	op_memcpy(pivot, last, size);

    while ( TRUE )
	{
        do
		{
            bottom += size;

			if ( bottom == top )
				goto done;

		}
		while ( compar(bottom, pivot) <= 0 );

		op_memcpy(top, bottom, size);

		do
		{
			top -= size;

            if ( top == bottom )
				goto done;

		} while ( compar(top, pivot) > 0 );

		op_memcpy(bottom, top, size);
	}

 done:
    op_memcpy(top, pivot, size);

    return top;
}

static void happy_qsort2(void *base, size_t size, int(*compar)(const void *, const void *),
				  void *first, void *last)
{
	if ( first < last )
	{
        char *split = happy_partition(base, size, compar, first, last);
		happy_qsort2(base, size, compar, first, split - size);
		happy_qsort2(base, size, compar, split + size, last);
	}
}

static void happy_qsort(void *base, size_t nmemb, size_t size, int(*compar)(const void *, const void *))
{
	happy_qsort2(base, size, compar, base, ((char *) base) + (nmemb - 1)*size);
}
#else
#define local_qsort op_qsort
#endif // __linux__

/**
 * Return address of node_t associated with this data pointer.
 */
static node_t*
get_node(void *data)
{
	return (node_t*)((char*)data - sizeof(header_t));
}

/**
 * Return address of data associated with this node pointer.
 */
static void*
get_data(node_t *p)
{
	return (char*)p + sizeof(header_t);
}

/**
 * Return the size of the data associated with this node pointer.
 */
static unsigned int
get_datasize(node_t *p)
{
#ifdef DETECT_OVERRUNS
	return p->header.info.size - sizeof(header_t) - sizeof(void*); /* header_t::magic2 */
#else
	return p->header.info.size - sizeof(header_t);
#endif
}

static void init_malloc()
{
	if (!malloc_initialized) {
		malloc_initialized = 2;

		// Codewarrior (MacOS X??) does not clear globals that do not
		// have an initializer!!  So make sure to clear all globals.
		initialize_size_stats();

#if defined (PARTIAL_LTH_MALLOC) && !defined(_NO_THREADS_) && defined(_PTHREADS_)
		int res = pthread_key_create(&is_non_oom_code_key, NULL);
		OP_ASSERT( res == 0 );
		res = pthread_key_create(&silent_oom_key, NULL);
		OP_ASSERT( res == 0 );

		init_stdlib_malloc();
#endif

#ifdef QUICK_LISTS
		int i;
		for (i=0 ; i < NLISTS ; i++ )
			quick_lists[i].head = quick_lists[i].tail = NULL;
#endif

		op_memset( &stats, 0, sizeof(stats) );
		// End clear globals cruft

		/* Do this last in case atexit() calls malloc(). */
#if defined(ACCOUNTING) && !defined(SILENT_LTH_MALLOC)
		atexit( summarize_stats_at_exit );
#endif

		getenv_value("OPERA_DEBUG_MALLOC_HEAPMAX", &stats.heapmax);
#ifdef SYNTHESIZE_FAILURE
		INT64 seed;
		if (getenv_value("OPERA_DEBUG_MALLOC_SYNTH_MINALLOC", &stats.synth_minalloc) == -1)
			stats.synth_minalloc = 2048*1024;
		if (getenv_value("OPERA_DEBUG_MALLOC_SYNTH_MINLIVE", &stats.synth_minlive) == -1)
			stats.synth_minlive = 1024*1024;
		if (getenv_value("OPERA_DEBUG_MALLOC_SYNTH_SEED", &seed ) == -1)
			seed = 0;
		srand48((long)seed);
#endif

		malloc_initialized = 1;
	}
}

size_t object_size_lth( void* p )
{
	return get_datasize(get_node(p));
}

void memory_allocation_stats( FILE *output )
{
	if (output == 0) output = stderr; 
#ifdef CPLUSPLUS
	INT64 sum = stats.nbytes_alloc + stats.nbytes_new + stats.nbytes_anew
		+ stats.nbytes_freelist_free + stats.nbytes_block_free
		+ stats.nbytes_block_frag;
	show( output,
		  "allocuse %9.1fK, newuse %9.1fK, anewuse %9.1fK, peak %9.1fK\n"
		  "freelist %9.1fK, bfree  %9.1fK, bfrag   %9.1fK\n"
		  "allocd   %9.1fK, newd   %9.1fK, anewd   %9.1fK\n"
		  "freed    %9.1fK, deld   %9.1fK, adeld   %9.1fK\n"
		  "use+free %9.1fK, block  %9.1fK\n",
		  KB(stats.nbytes_alloc), KB(stats.nbytes_new), KB(stats.nbytes_anew), KB(stats.nbytes_peak),
		  KB(stats.nbytes_freelist_free), KB(stats.nbytes_block_free), KB(stats.nbytes_block_frag),
		  KB(stats.nbytes_alloc_total), KB(stats.nbytes_new_total), KB(stats.nbytes_anew_total), 
		  KB(stats.nbytes_free_total), KB(stats.nbytes_delete_total), KB(stats.nbytes_adelete_total),
		  KB(sum), KB(stats.nbytes_block) );
#else
	show( output,
		  "allocd %6gK, freed %6gK, allocuse %6gK, free %6gK\n",
		  KB(stats.nbytes_alloc_total), KB(stats.nbytes_free_total),
		  KB(stats.nbytes_alloc), KB(stats.nbytes_freelist_free) );
#endif
#ifdef PARTIAL_LTH_MALLOC
	show( output,
		  "---------- Memory allocated with stdlib functions ----------\n"
		  "in use:  %6gK, peak:  %6gK, total:  %6gK\n",
		  KB(stdlib_stats.nbytes_in_use), KB(stdlib_stats.nbytes_peak), KB(stdlib_stats.nbytes_allocated_total) );
#endif // PARTIAL_LTH_MALLOC
}

#if defined(PARTIAL_LTH_MALLOC)

void handle_dlsym_error()
{
	char *error = dlerror();
	if (error)
	{
		fprintf(stderr, "dlerror: \"%s\"\n", error);
	 	OP_ASSERT(FALSE);
	}
}

static void init_stdlib_malloc()
{
	if (stdlib_malloc_initialized)
		return;

	if (initializing_stdlib_malloc)
		return;

	initializing_stdlib_malloc = TRUE;

	memset(&stdlib_stats, 0, sizeof(stdlib_stats));
	
	stdlib_malloc = (void* (*)(size_t)) dlsym(RTLD_NEXT, "malloc");
	handle_dlsym_error();

	stdlib_realloc = (void* (*)(void*, size_t)) dlsym(RTLD_NEXT, "realloc");
	handle_dlsym_error();
	
	stdlib_free = (void (*)(void*)) dlsym(RTLD_NEXT, "free");
	handle_dlsym_error();

	stdlib_strdup = (char* (*)(const char*))dlsym(RTLD_NEXT, "strdup");
	handle_dlsym_error();

	stdlib_malloc_initialized = TRUE;
	initializing_stdlib_malloc = FALSE;
}

# if !defined(_NO_THREADS_) && defined(_PTHREADS_)

#  ifdef DEBUG_PARTIAL_LTH_MALLOC
void dbg_reset_malloc_state(const void *old_oom_state)
{
	if (old_oom_state) {
		enter_non_oom_code();
		pthread_setspecific(is_non_oom_code_key, old_oom_state);
	}
	else
		exit_non_oom_code();
}

void dbg_enter_non_oom_code(char *file, int line)
{
	init_malloc();
	pthread_setspecific(is_non_oom_code_key, (void*) __builtin_return_address(0));
}

void dbg_exit_non_oom_code(char *file, int line)
{
	init_malloc();
	pthread_setspecific(is_non_oom_code_key, (void*) FALSE);
}
#  else

void enter_non_oom_code()
{
	init_malloc();
	pthread_setspecific(is_non_oom_code_key, (void*) TRUE);
}

void exit_non_oom_code()
{
	init_malloc();
	pthread_setspecific(is_non_oom_code_key, (void*) FALSE);
}

void set_silent_oom()
{
	init_malloc();
	pthread_setspecific(silent_oom_key, (void*) TRUE);
}

void unset_silent_oom()
{
	init_malloc();
	pthread_setspecific(silent_oom_key, (void*) TRUE);
}

#  endif

BOOL in_non_oom_code()
{
	init_malloc();
	return (BOOL) pthread_getspecific(is_non_oom_code_key);
}

BOOL use_silent_oom()
{
	if (!silent_oom_key)
		return FALSE;
	return (BOOL) pthread_getspecific(silent_oom_key);
}

# else // no worry about threads
void enter_non_oom_code()
{
	is_non_oom_code = TRUE;
}

void exit_non_oom_code()
{
	is_non_oom_code = FALSE;
}

BOOL in_non_oom_code()
{
	return is_non_oom_code;
}

void set_silent_oom()
{
	silent_oom = TRUE;
}

void unset_silent_oom()
{
	silent_oom = FALSE;
}

BOOL use_silent_oom()
{
	return silent_oom;
}
# endif // Threads

void* malloc_lth(size_t size)
{
	LTH_MALLOC_SITE(site,TAG_MALLOC);
	
	return internal_malloc(size, site);
}

void free_lth(void *ptr)
{
	BOOL was_in_non_oom_code = in_non_oom_code();
	exit_non_oom_code();
	free(ptr);
	if (was_in_non_oom_code)
	{
		enter_non_oom_code();
	}
}

# ifdef DEBUG_PARTIAL_LTH_MALLOC
void debug_partial_lth_malloc(site_t site, char *ptr, size_t size)
{
	if ( !ADDR_IN_LIB(site.location) )
	{
#  ifdef ADDR2LINE_SUPPORT
		const char *s = Addr2Line::TranslateAddress(site.location);
		if (!strstr(s, "platforms")
			&& !strstr(s, "qt-x11-free-3.1.1")
			&& !strstr(s, "dbug")
			&& !strstr(s, "opstring.cpp")
			&& !strstr(s, "uni_up_strdup"))
		{
			fprintf(stderr, "Stdlib allocation %p of size %d at %s\n", ptr, size, s);
			fprintf(stderr, "Non oom code entered at %s\n", Addr2Line::TranslateAddress((void *)in_non_oom_code()));
			show_stack_trace_from(10, site.location);
		}
#  else
		fprintf(stderr, "Stdlib allocation %p of size %d at %p\n", ptr, size, site.location);
#  endif
	}
}
# endif

void set_stdlib_allocation_data(char *ptr, size_t size)
{
	size_t *size_info = (size_t*) ptr;
	int *magic1 = (int*) (ptr + sizeof(size_t));
	int *magic2 = (int*) (ptr + sizeof(size_t) + sizeof(int) + size);
	*size_info = size;
	*magic1 = STDLIB_MAGIC1;
	*magic2 = STDLIB_MAGIC2;
}

BOOL check_stdlib_allocation(char *ptr)
{
	int magic1 = *((int*) (ptr - sizeof(void*)));
	if (magic1 != STDLIB_MAGIC1)
	{
		return FALSE;
	}
	size_t size = *((size_t*) (ptr - sizeof(void*) - sizeof(size_t)));
	int magic2 = *((int*) (ptr + size));
	if (magic2 != STDLIB_MAGIC2)
	{
		return FALSE;
	}

	return TRUE;
}

void* get_stdlib_header_offset(char *ptr)
{
	return ptr - sizeof(size_t) - sizeof(void*);
}

void* get_stdlib_memory_offset(char *ptr)
{
	return ptr + sizeof(size_t) + sizeof(void*);
}

void stdlib_stats_add(size_t size)
{
	stdlib_stats.nbytes_in_use += size;
	stdlib_stats.nbytes_allocated_total += size;
	stdlib_stats.nbytes_peak = max(stdlib_stats.nbytes_peak, stdlib_stats.nbytes_in_use);
}
void stdlib_stats_sub(size_t size)
{
	stdlib_stats.nbytes_in_use -= size;
}
#endif // PARTIAL_LTH_MALLOC

void *internal_malloc( size_t nbytes, site_t site )
{
	if (nbytes < 4) nbytes = 4;

#ifdef QNX
	/* HACK: For debugging leaks in vpi.so on Photon only */
	if (site.location >= ADDR(0xb049a000) && site.location < ADDR(0xb04a0000))
	{
		int stophere = 0;
	}
#endif
#if defined(_MSC_VER)
	// MSVC brokenness -- does not call operator new[] and delete[], so do not
	// distinguish these.
	if ((site.tag & TAG_MASK) == TAG_ANEW)
		site.tag = (site.tag & ~TAG_MASK) | TAG_NEW;
#endif
	/* The switch is necessary because internal_malloc() isn't internal at
	   all -- it is called directly from instrumented libraries in Opera!
	*/
	switch (site.tag & TAG_MASK)
	{
	case TAG_MALLOC:
		return allocate_instrumented( nbytes, 
									  &stats.nbytes_alloc, 
									  &stats.nbytes_alloc_total,
									  site.tag, site );
	case TAG_NEW:
		return allocate_instrumented( nbytes, 
									  &stats.nbytes_new, 
									  &stats.nbytes_new_total,
									  site.tag, site );
	case TAG_ANEW:
		return allocate_instrumented( nbytes, 
									  &stats.nbytes_anew, 
									  &stats.nbytes_anew_total,
									  site.tag, site );
	}
	return 0;
}

char *strdup( const char *s )
{
	LTH_MALLOC_SITE(site,TAG_MALLOC | TAG_STRDUP);
	int nbytes = op_strlen(s)+1;
#ifdef PARTIAL_LTH_MALLOC
	if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
	{
		init_stdlib_malloc();

		char *ptr = (char*) stdlib_malloc(nbytes + stdlib_allocation_extra_size);
		if (!ptr)
			return NULL;
		stdlib_stats_add(nbytes);
		set_stdlib_allocation_data(ptr, nbytes);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
		debug_partial_lth_malloc(site, ptr, nbytes);
#endif
		ptr = (char*) get_stdlib_memory_offset(ptr);
		op_memcpy(ptr, s, nbytes);
		return ptr;
	}
	else
#endif // PARTIAL_LTH_MALLOC
	{
		char *p = (char*)internal_malloc( nbytes, site );
		
		if (p)
			op_memcpy( p, s, nbytes );
		return p;
	}
}

#if defined (__QNX__) && defined (DINKUM)
_C_STD_BEGIN
#endif /* __QNX__ && DINKUM */

#if TARGET_RT_MAC_CFM
/* Ensure std:: namespace since metrowerks C library depends on it */
namespace std {
extern "C" {
#endif

	void *malloc( size_t nbytes )
	{
		LTH_MALLOC_SITE(site,TAG_MALLOC);
#if defined(PARTIAL_LTH_MALLOC)
		if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
		{
			init_stdlib_malloc();

			char *ptr = (char*) stdlib_malloc(nbytes + stdlib_allocation_extra_size);
			if (!ptr)
				return NULL;
			stdlib_stats_add(nbytes);
			set_stdlib_allocation_data(ptr, nbytes);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
			debug_partial_lth_malloc(site, ptr, nbytes);
#endif
			return get_stdlib_memory_offset(ptr);
		}
		else
#endif // PARTIAL_LTH_MALLOC
			return internal_malloc( nbytes, site );
	}

	void *calloc( size_t nobj, size_t objsize )
	{
		void *p;
		size_t nbytes = nobj * ALIGN(objsize);
		LTH_MALLOC_SITE(site,TAG_MALLOC);
#if defined(PARTIAL_LTH_MALLOC)
		if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
		{
			init_stdlib_malloc();
			size_t size = nobj * objsize;
			char *ptr = (char*) stdlib_malloc(size + stdlib_allocation_extra_size);
			if (!ptr)
				return NULL;
			stdlib_stats_add(size);
			set_stdlib_allocation_data(ptr, size);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
			debug_partial_lth_malloc(site, ptr, size);
#endif
			ptr = (char*) get_stdlib_memory_offset(ptr);
			memset(ptr, 0, size);
			return ptr;
		}
		else
#endif // PARTIAL_LTH_MALLOC
		{
			p = internal_malloc( nbytes, site );
			if (p)
				op_memset( p, 0, nbytes );
			return p;
		}
	}

	void *realloc( void *obj, size_t nbytes )
	{
		node_t *p;
		void *q;
		LTH_MALLOC_SITE(site,TAG_MALLOC);
#if defined(PARTIAL_LTH_MALLOC)
		BOOL run_lth_realloc = TRUE;
		if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
		{
			run_lth_realloc = FALSE;
			init_stdlib_malloc();
			if (nbytes == 0)
			{
				free(obj);
				return NULL;
			}
			if (obj == 0)
			{
				char *ptr = (char*) stdlib_malloc(nbytes + stdlib_allocation_extra_size);
				if (!ptr)
					return NULL;
				stdlib_stats_add(nbytes);
				set_stdlib_allocation_data(ptr, nbytes);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
				debug_partial_lth_malloc(site, ptr, nbytes);
#endif
				return get_stdlib_memory_offset(ptr);
			}
			if (check_stdlib_allocation((char*) obj))
			{
				char *ptr = (char*) get_stdlib_header_offset((char*) obj);
				
				size_t old_size = *((size_t*) ptr);
				if (nbytes == old_size)
				{
					return obj;
				}
				ptr = (char*) stdlib_realloc(ptr, nbytes + stdlib_allocation_extra_size);
				if (!ptr)
					return NULL;
				stdlib_stats_add(nbytes);
				stdlib_stats_sub(old_size);
				if (nbytes > old_size)
				{
					int *magic2 = (int*) (ptr + sizeof(size_t) + sizeof(int) + old_size);
					*magic2 = 0;
				}
				set_stdlib_allocation_data(ptr, nbytes);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
				debug_partial_lth_malloc(site, ptr, nbytes);
#endif
				return get_stdlib_memory_offset(ptr);
			}
			else
			{
				run_lth_realloc = TRUE;
			}
		}
		if (run_lth_realloc)
#endif // PARTIAL_LTH_MALLOC
		{
			if (obj == 0)
				return internal_malloc( nbytes, site );

#if defined(PARTIAL_LTH_MALLOC)
			OP_ASSERT(!check_stdlib_allocation((char*) obj));
#endif

			p = get_node(obj);
			size_t oldsize = get_datasize(p);
			if (oldsize >= nbytes) 
				return obj;

			if ((q = internal_malloc( nbytes, site )) == 0)
				return 0;

			op_memcpy( q, obj, MIN(oldsize, nbytes) );
			free( obj );
			return q;
		}

		return NULL; //FIXME: When does this happen?
	}

	void free( void *datum )
	{
		// FIXME: Check if memory exists in LTH-pool, otherwise use stdlib_free
		if (datum == 0) return;
#ifdef PARTIAL_LTH_MALLOC
		if (check_stdlib_allocation((char*)datum))
		{
			char* ptr = (char*) get_stdlib_header_offset((char*) datum);
			size_t size = *((size_t*) ptr);
			stdlib_stats_sub(size);
			stdlib_free(ptr);
		}
		else
#endif // PARTIAL_LTH_MALLOC
			free_instrumented( datum, &stats.nbytes_free_total, &stats.nbytes_alloc, TAG_MALLOC );
	}

#if TARGET_RT_MAC_CFM
}} /* end of namespace and extern "C" */
#endif

#if defined (__QNX__) && defined (DINKUM)
_C_STD_END
#endif /* __QNX__ && DINKUM */

#if defined(HAVE_MPROTECT) && defined(HAVE_MMAP) || defined(QNX_PROTECTED_MALLOC)
/* Experimental interface that allows allocation on segregated pages that
   can be protected against access.  Typically this is used during memory 
   debugging: some objects are specially allocated on individual pages, and
   after the objects are freed, VM protection triggers an exception if the
   pages are accessed.

   This implementation is the simplest possible: a separate page or set of
   pages for each object, with no reuse ever.  This is expensive.  It is 
   possible to be smarter, by grouping objects onto pages and then handling
   the access violation and letting an access complete if it is to an object
   that has not been freed.  Usually that involves either getting the
   fault address from a signal structure (if we're lucky) or decoding the
   faultuing instruction to compute the accessed address (if we're not).
*/
void malloc_protect_all_allocations(BOOL f)
{
	g_malloc_protect_all_allocations = f;
}

void *malloc_protected( size_t nbytes )
{
	void *addr;

	nbytes += 2*sizeof(size_t);
	if ((addr = mmap( NULL, nbytes, PROT_READ|PROT_WRITE, MAP_ANON, NOFD, 0 )) == MAP_FAILED)
		return 0;

	size_t *obj = (size_t*)addr;
	obj[0] = PROTECTED_MAGIC;
	obj[1] = nbytes;
	return &obj[2];
}

/* We'd like this to return -1 for 'yes, free', 0 for 'no', and 1 for 'yes, live' */
/* Right now it doesn't handle the first case */
int is_protected_object( void *addr )
{
	size_t *obj = (size_t*)addr - 2;
	return obj[0] == PROTECTED_MAGIC;		// Fails if object has been freed
}

static void free_protected( void *addr )
{
	size_t *obj = (size_t*)addr - 2;
	OP_ASSERT( obj[0] == PROTECTED_MAGIC );

	size_t nbytes = obj[1]; 
	mprotect( obj, nbytes, PROT_NONE );
}
#endif // HAVE_MPROTECT && HAVE_MMAP

#if defined(CPLUSPLUS)

void* operator new( size_t nbytes )
	throw ()
{
	LTH_MALLOC_SITE(site,TAG_NEW);
#ifdef PARTIAL_LTH_MALLOC
	if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
	{
		init_stdlib_malloc();

		char *ptr = (char*) stdlib_malloc(nbytes + stdlib_allocation_extra_size);
		if (!ptr)
			return NULL;
		stdlib_stats_add(nbytes);
		set_stdlib_allocation_data(ptr, nbytes);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
		debug_partial_lth_malloc(site, ptr, nbytes);
#endif
		return get_stdlib_memory_offset(ptr);
	}
	else
#endif // PARTIAL_LTH_MALLOC
		return operator_new_implementation( nbytes, site );
}

/* A global version of new(TLeave) */

void *operator new(size_t size, TLeave cookie)
{
	LTH_MALLOC_SITE(site,TAG_NEW);
#ifdef PARTIAL_LTH_MALLOC
	if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
	{
		init_stdlib_malloc();

		char *ptr = (char*) stdlib_malloc(size + stdlib_allocation_extra_size);
#if defined(USE_DEBUGGING_LEAVE)
		char *caller;
		GET_CALLER(caller);
		oom_report_and_remove_unanchored_objects(caller);
#endif // USE_DEBUGGING_LEAVE
		if (!ptr)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		stdlib_stats_add(size);
		set_stdlib_allocation_data(ptr, size);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
		debug_partial_lth_malloc(site, ptr, size);
#endif
		return get_stdlib_memory_offset(ptr);
	}
	else
#endif // PARTIAL_LTH_MALLOC
	{
		void *p = operator_new_implementation( size, site );
#if defined(USE_DEBUGGING_LEAVE)
		char *caller;
		GET_CALLER(caller);
		oom_report_and_remove_unanchored_objects(caller);
#endif // USE_DEBUGGING_LEAVE
			if (p == 0)
				LEAVE(OpStatus::ERR_NO_MEMORY);
		return p;
	}
}

void operator delete(void *ptr, TLeave cookie)
{
	operator delete(ptr);
}

void operator delete( void* datum )
	throw ()
{
	if (!datum)
		return;
#ifdef PARTIAL_LTH_MALLOC
	if (check_stdlib_allocation((char*)datum))
	{
		char* ptr = (char*) get_stdlib_header_offset((char*) datum);
		size_t size = *((size_t*) ptr);
		stdlib_stats_sub(size);
		stdlib_free(ptr);
	}
	else
#endif // PARTIAL_LTH_MALLOC
		free_instrumented( datum, &stats.nbytes_delete_total, &stats.nbytes_new, TAG_NEW );
}

// Yes, this is defined even with MSVC, even though new[] is not.
void operator delete[]( void* datum )
	throw ()
{
	if (!datum)
		return;
#ifdef PARTIAL_LTH_MALLOC
	if (check_stdlib_allocation((char*)datum))
	{
		char* ptr = (char*) get_stdlib_header_offset((char*) datum);
		size_t size = *((size_t*) ptr);
		stdlib_stats_sub(size);
		stdlib_free(ptr);
	}
	else
#endif // PARTIAL_LTH_MALLOC
		free_instrumented( datum, &stats.nbytes_adelete_total, &stats.nbytes_anew, TAG_ANEW );
}

#if !(defined(_MSC_VER) && _MSC_VER==1200)
/* MSVC 6 does not use the array versions. MSVC _MSC_VER 5.0=1100, 6.0=1200, 7.0=1300, 7.1=1310 */
void* operator new[]( size_t nbytes )
	throw ()
{
	LTH_MALLOC_SITE(site,TAG_ANEW);
#ifdef PARTIAL_LTH_MALLOC
	if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
	{
		init_stdlib_malloc();

		char *ptr = (char*) stdlib_malloc(nbytes + stdlib_allocation_extra_size);
		if (!ptr)
			return NULL;
		stdlib_stats_add(nbytes);
		set_stdlib_allocation_data(ptr, nbytes);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
		debug_partial_lth_malloc(site, ptr, nbytes);
#endif
		return get_stdlib_memory_offset(ptr);
	}
	else
#endif // PARTIAL_LTH_MALLOC
		return operator_anew_implementation( nbytes, site );
}

void *operator new[](size_t size, TLeave cookie)
{
	LTH_MALLOC_SITE(site,TAG_ANEW);

#ifdef PARTIAL_LTH_MALLOC
	if ( !initializing_stdlib_malloc && USE_STDLIB_MALLOC() )
	{
		init_stdlib_malloc();

		char *ptr = (char*) stdlib_malloc(size + stdlib_allocation_extra_size);
#if defined(USE_DEBUGGING_LEAVE)
		char *caller;
		GET_CALLER(caller);
		oom_report_and_remove_unanchored_objects(caller);
#endif // USE_DEBUGGING_LEAVE
		if (!ptr)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		stdlib_stats_add(size);
		set_stdlib_allocation_data(ptr, size);
#ifdef DEBUG_PARTIAL_LTH_MALLOC
		debug_partial_lth_malloc(site, ptr, size);
#endif
		return get_stdlib_memory_offset(ptr);
	}
	else
#endif // PARTIAL_LTH_MALLOC
	{
		void *p = operator_anew_implementation( size, site );
#if defined(USE_DEBUGGING_LEAVE)
		char *caller;
		GET_CALLER(caller);
		oom_report_and_remove_unanchored_objects(caller);
#endif // USE_DEBUGGING_LEAVE
		if (p == 0)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		return p;
	}
}

void operator delete[](void *ptr, TLeave cookie)
{
	operator delete[](ptr);
}
#endif // !(defined(_MSC_VER) && _MSC_VER==1200)

static void *operator_new_implementation( size_t nbytes, site_t site )
{
	if (nbytes < 4) nbytes = 4;

	while (true)
    {
		void *ptr = allocate_instrumented( nbytes, 
										   &stats.nbytes_new, 
										   &stats.nbytes_new_total,
										   TAG_NEW, site );
		if (NULL != ptr)
			return ptr;

		new_handler global_new_handler = set_new_handler(0);
		set_new_handler(global_new_handler);

		if (0 != global_new_handler)
			(*global_new_handler)();
		else
			return ptr; /* returning 0 trigger gcc warning */
	}
}

static void *operator_anew_implementation( size_t nbytes, site_t site )
{
	if (nbytes < 4) nbytes = 4;

	while (true)
    {
		void *ptr = allocate_instrumented( nbytes, 
										   &stats.nbytes_anew, 
										   &stats.nbytes_anew_total,
										   TAG_ANEW, site );
		if (NULL != ptr)
			return ptr;

		new_handler global_new_handler = set_new_handler(0);
		set_new_handler(global_new_handler);

		if (0 != global_new_handler)
			(*global_new_handler)();
		else
			return ptr; /* returning 0 trigger gcc warning */
    }
}


#endif

/**
 * Get numeric value from environment variable name.
 *
 * @param name of environment variable.
 * @param value where to store the value on success.
 * @return 0 if variable was numeric, -1 otherwise.
 */
static int
getenv_value(const char *name, INT64 *value)
{
	const char *str = getenv(name);
	if (NULL != str) {
		char *endptr;
		int i = op_strtol(str, &endptr, 10);
		if ('\0' == *endptr) {
			*value = i;
			return 0;
		}
	}
	return -1;
}

void summarize_stats_at_exit()
{
	show( stderr, "\nOpera memory allocation stats at program's end:\n" );
	summarize_allocation_sites( stderr );
	memory_allocation_stats( stderr );
#if defined(USE_DEBUGGING_OP_STATUS)
	summarize_opstatus_failures( stderr );
#endif 
#if defined(PROFILE_SENT_MESSAGES)
	summarize_sent_messages( stderr );
#endif
#ifdef RECORD_ALLOC_SITE_STACK
//	summarize_allocation_stack_sites( stderr ); // This is very verbose and kind of annoying most of the time... ;)
#endif
	show( stderr, "\n" );
}

static void
outofmemory(int used, int requested, site_t site)
{
#if !defined(SILENT_LTH_MALLOC)
# ifdef PARTIAL_LTH_MALLOC
	if (!use_silent_oom())
# endif
	{
		show(stderr,
			 "warning: Out of memory (used=%d, requested=%d, call address: %p): heap limit reached!\n",
			 used, requested, site.location);
#  if defined(SHOW_OOM_STACK_TRACE)
		show_stack_trace_from(OOM_STACK_DEPTH, site.location);
#  endif
	}
#endif //SILENT_LTH_MALLOC
}

/**
 * Report when illegal free occured.
 */
static void
illegal_free(node_t *p, const char *msg)
{
#if defined(SELFTEST)
	illegal_free_count++;
#endif
	show(stderr, "error:illegal_free(): %s\n", msg);
	if (NULL == getenv("OPERA_DEBUG_FREE"))
	{
		show(stderr, "Set OPERA_DEBUG_FREE=1 to ignore such errors.\n");
		abort();
	}
}

static void
free_list_corrupted()
{
	static int corruptions = 0;

	corruptions++;
	show(stderr, "error: free list corrupted!\n" );
	if (NULL == getenv("OPERA_DEBUG_FREE"))
	{
		show(stderr, "Set OPERA_DEBUG_FREE=1 to ignore such errors.\n");
		abort();
	}
	else if (corruptions > 100)
	{
		show(stderr, "Too many corruptions.");
		abort();
	}
}

#ifdef SYNTHESIZE_FAILURE
static int synthesized_failures = 0;

static int synthesizing_oom()
{
	synthesized_failures++;

	int breakpoint_here = 0;
}

static int maybe_synthesize_failure( size_t nbytes, site_t site )
{
	if (malloc_initialized != 1)	/* Note, can have several values */
		return 0;

	if (synthesized_failures_disabled)
		return 0;

	INT64 allocated = stats.nbytes_alloc_total + stats.nbytes_new_total + stats.nbytes_anew_total;
	if (stats.synth_minalloc > allocated)
		return 0;

	int live = stats.nbytes_alloc + stats.nbytes_new + stats.nbytes_anew;
	if (stats.synth_minlive > live)
		return 0;

# ifdef EXCLUDE_PHOTON_LIBRARIES
	if ((unsigned long)site.location >= 0xb0000000 && nbytes < PHOTON_CUTOFF)
		return 0;
# endif
	/* Primitive support for non-contextual blacklisting
	 *
	 * Use blacklisted_site together with GDB: set a breakpoint on the line that
	 * contains allocations to be blacklisted.  For the commands of that breakpoint
	 * use
	 *  silent
	 *  set blacklisted_site 1
	 *  cont
	 *  end 
	 * so that when the allocator is hit, blacklisting works.
	 */
	if (blacklisted_site) {
		blacklisted_site = 0;
		return 0;
	}

	site_stats_t *ss = lookup_allocation_site( site );
#ifdef SYNTHESIZE_SYSTEMATIC_FAILURE
	if (ss->failures == 0) {
		ss->failures = 1;
		synthesizing_oom();
		return 1;
	}
#endif
#ifdef SYNTHESIZE_OCCASIONAL_FAILURE
	if (nbytes < 1024 && drand48() < 0.001 ||		/* One out of 1000 */
		nbytes >= 1024 && drand48() < 0.01) {  	/* One out of 100 */
		ss->failures++;
		synthesizing_oom();
		return 1;
	}
#endif
	return 0;
}
#endif

static int override_heaplimit( site_t site, size_t nbytes )
{
#ifdef EXCLUDE_PHOTON_LIBRARIES
	return ((unsigned long)site.location >= 0xb0000000) && nbytes < PHOTON_CUTOFF;
#endif
	return 0;
}

static void signal_large_allocation( size_t nbytes )
{
	// Just a hook.
}

static void *allocate_instrumented( size_t nbytes, INT64 *acct1, INT64 *acct2, unsigned int tag, site_t site )
{
	node_t *p, *q;
	unsigned int actual_bytes;
	int list;

	MALLOC_LOCK();

	init_malloc();

#if defined(HAVE_MPROTECT) && defined(HAVE_MMAP) || defined(QNX_PROTECTED_MALLOC)
	if (g_malloc_protect_all_allocations)
	{
		void *p = malloc_protected( nbytes );
		MALLOC_UNLOCK();
		return p;
	}
#endif

#ifdef SYNTHESIZE_FAILURE
	if (maybe_synthesize_failure(nbytes,site)) {
		MALLOC_UNLOCK();
		return 0;
	}
#endif

	nbytes += sizeof(header_t);
#ifdef DETECT_OVERRUNS
	nbytes += sizeof(void*); /* header_t::magic2 // end magic value */
#endif
	actual_bytes = ALIGN(nbytes);
  
	if (actual_bytes >= 4200)
	{
		signal_large_allocation(actual_bytes);
	}

	/* Allow maximum heap limit */
	int heapused = stats.nbytes_alloc + stats.nbytes_new + stats.nbytes_anew;
	if (stats.heapmax != 0 && stats.heapmax <= heapused+actual_bytes && !override_heaplimit( site, actual_bytes )) {
		outofmemory(heapused, actual_bytes, site);
		MALLOC_UNLOCK();
		return 0;
	}

 retry_allocation:
#ifdef NEVER_FREE_MEMORY
	p = 0;
#else
#ifdef QUICK_LISTS
	p = 0;
	list = actual_bytes / ALIGNMENT;
	if (list < NLISTS) {
		if (quick_lists[list].head) {
			p = quick_lists[list].head;
			if (p == quick_lists[list].tail)
				quick_lists[list].head = quick_lists[list].tail = 0;
			else
				quick_lists[list].head = p->next;
			stats.nbytes_freelist_free -= p->header.info.size;
			if (p->header.info.size != actual_bytes)
			{
				free_list_corrupted();
				p->header.info.size = actual_bytes;
			}
		}
	}
	else  /* Do a general search if the quick lists fail */
#endif
	{
		for ( p=stats.freelist_head, q=0 ; p && p->header.info.size != actual_bytes ; q=p, p=p->next )
			;
		if (p) {
			/* Found on free list */
#if defined(DELAY_REUSE)
			if (q == 0) {
				/* First elt on freelist */
				stats.freelist_head = p->next;
				if (stats.freelist_head == NULL) 
					stats.freelist_tail = NULL;
			}
			else { 
				q->next = p->next;
				if (stats.freelist_tail == p) /* The last one in the list */
					stats.freelist_tail = q;
			}
#else
			if (q)
				q->next = p->next;
			else
				stats.freelist_head = p->next;
#endif
			stats.nbytes_freelist_free -= p->header.info.size;
		}
	}
#endif	// NEVER_FREE_MEMORY

	if (p) {
#if defined(CHECK_FREELIST_CORRUPTION) && defined(DETECT_UNDERRUNS)
		if ((INTPTR)p->header.magic1 != ((INTPTR)MAGIC1 ^ p->header.info.size) || (INTPTR)p->header.magic2 != ((INTPTR)MAGIC2 ^ p->header.info.size))
		{
			free_list_corrupted();
			goto retry_allocation;
		}
#endif
	}
	else if (nbytes > SMALL_BLOCK) {
		/* Block of its own */
		p = (node_t*)allocate_dedicated_block( actual_bytes );
		if (!p) { 
			MALLOC_UNLOCK();
			return 0;
		}
		p->header.info.size = actual_bytes;
	}
	else {
		if (!stats.blocklist) {
			/* No blocks yet, allocate more space from OS */
			if (!allocate_block()) {
				MALLOC_UNLOCK();
				return 0;
			}
		}
		else if (stats.blocklist->limit - stats.blocklist->free < (int)actual_bytes) {
			/* No space in block; drop on the floor and allocate more space from OS */
			stats.nbytes_block_frag += 
				(stats.blocklist->limit - stats.blocklist->free);
			if (!allocate_block()) {
				MALLOC_UNLOCK();
				return 0;
			}
		}
    
		/* First block has space for object */

		p = (node_t*)stats.blocklist->free;
		stats.blocklist->free += actual_bytes;
		stats.nbytes_block_free -= actual_bytes;
		p->header.info.size = actual_bytes;
	}
	p->header.info.tag = tag;

	{ /* ... fill the chunk with garbage (though preserve the header) */
		int datasize = get_datasize(p);
		int *datastart = (int*)get_data(p);
		int *dataend = datastart + (datasize / sizeof(int));
		for (; datastart < dataend; datastart++)
			*datastart = NEW_PATTERN;
	}
#ifdef RECORD_ALLOC_SITE
	p->header.site = lookup_allocation_site(site);
	p->header.site->allocated += p->header.info.size;
	p->header.site->nallocated += 1;
	p->header.site->peak = MAX( p->header.site->peak, p->header.site->allocated-p->header.site->freed );
	p->header.site->npeak = MAX( p->header.site->npeak, p->header.site->nallocated-p->header.site->nfreed );
#  ifdef RECORD_ALLOC_SITE_STACK
	void *stack[ALLOC_SITE_STACK_SIZE];
	store_stack_trace_from(stack, ALLOC_SITE_STACK_SIZE, site.location);
	OP_ASSERT(site.location == stack[0]);
	site_t st = lookup_stack(stack);
	site_stats_t *ss = lookup_allocation_site(st);
	ss->allocated += p->header.info.size;
	ss->nallocated += 1;
	ss->peak = MAX( ss->peak, ss->allocated-ss->freed );
	ss->npeak = MAX( ss->npeak, ss->nallocated-ss->nfreed );
	OP_ASSERT(p->header.site->site.location == ss->site.stack[0]);
	p->header.site = ss; // Should be this way when using RECORD_ALLOC_SITE_STACK
#  endif
#endif

#ifdef DETECT_UNDERRUNS
	p->header.magic1 = MAGIC1; /* Tag struct */
	p->header.magic2 = MAGIC2; /* Detect buffer underrun */
#endif
#ifdef DETECT_OVERRUNS
	/* Calculate address of end pattern */
	p->header.endmagic =
		(void**)((char*)p + p->header.info.size - sizeof(void*));	/* sizeof(header_t::endmagic) */
	*(p->header.endmagic) = MAGIC2; /* Detect buffer overrun */
#endif

	if (acct1) *acct1 += p->header.info.size;
	if (acct2) *acct2 += p->header.info.size;
	stats.nbytes_peak = MAX( stats.nbytes_peak, stats.nbytes_alloc+stats.nbytes_new+stats.nbytes_anew );
	stats.nobj_alloc_total++;

	MALLOC_UNLOCK();
	return get_data(p);
}

void *malloc_stop_datum = 0;

static void free_instrumented( void *datum, INT64 *acct, INT64 *nact, unsigned int tag )
{
	node_t *p;
	int list;

	if (malloc_stop_datum != 0)
	{
		OP_ASSERT(malloc_stop_datum != datum);
	}

	MALLOC_LOCK();

	OP_ASSERT( malloc_initialized );

#if defined(HAVE_MPROTECT) && defined(HAVE_MMAP) || defined(QNX_PROTECTED_MALLOC)
	size_t *obj = (size_t*)datum - 2;
	if (obj[0] == PROTECTED_MAGIC)
	{
		free_protected(datum);
		return;
	}
#endif

	p = get_node(datum);

#ifdef DETECT_UNDERRUNS
	if (MAGIC1 != p->header.magic1)
	{
		illegal_free(0,"Trying to release non-allocated memory or large buffer underrun!");
		MALLOC_UNLOCK();
		return; /* Do not dare to trust the header, lose the block! */
	}
	if (MAGIC2 != p->header.magic2)
	{
		illegal_free(0,"Object written before beginning!");
		MALLOC_UNLOCK();
		return; /* Do not dare to trust the header, lose the block! */
	}
#endif
#ifdef DETECT_OVERRUNS
	if (MAGIC2 != *(p->header.endmagic))
		illegal_free(p,"Object written past end!");
#endif

	if ((p->header.info.tag & TAG_MASK) != tag)
	{
		switch (p->header.info.tag & TAG_MASK)
		{
		case 0:
			illegal_free(p,"Double free()/delete/delete[] on pointer!");
			break;
		case TAG_MALLOC:
			if (p->header.info.tag & TAG_STRDUP)
				illegal_free(p,"Not using free() on memory allocated from strdup()!");
			else
				illegal_free(p,"Not using free() on memory allocated from malloc()!");
			break;
		case TAG_NEW:
			illegal_free(p,"Not using delete on memory allocated from new!");
			break;
		case TAG_ANEW:
			illegal_free(p,"Not using delete[] (array delete) on memory allocated from new[]!");
			break;
		}
	}
	if (0 == p->header.info.tag)
	{
		MALLOC_UNLOCK();
		/* Double free -- already reported via illegal_free, just return */
		return;
	}

	stats.nobj_free_total++;

	p->header.info.tag = 0; /* Make sure the next free fails. */
	if (acct) *acct += p->header.info.size;
//  OP_ASSERT(p->header.info.size <= *nact);
	if (nact) *nact -= p->header.info.size;
#ifdef RECORD_ALLOC_SITE
	p->header.site->freed += p->header.info.size;
	p->header.site->nfreed += 1;
	OP_ASSERT(p->header.site->freed <= p->header.site->allocated);
	OP_ASSERT(p->header.site->nfreed <= p->header.site->nallocated);
#  ifdef RECORD_ALLOC_SITE_STACK
	site_t s;
	s.location = (char *) p->header.site->site.stack[0];
	s.tag = 0; // doesn't matter, lookup doesn't depend on it.
	site_stats_t *ss = lookup_allocation_site(s);
	ss->freed += p->header.info.size;
	ss->nfreed += 1;
	OP_ASSERT(ss->freed <= ss->allocated);
	OP_ASSERT(ss->nfreed <= ss->nallocated);
#  endif
#endif
#if defined(DELAY_REUSE)
	{ /* ... fill the chunk with garbage (though preserve the header) */
		int datasize = get_datasize(p);
		int *datastart = (int*)get_data(p);
		int *dataend = datastart + (datasize / sizeof(int));
		for (; datastart < dataend; datastart++)
			*datastart = FREE_PATTERN;
	}
#endif
#if defined(CHECK_FREELIST_CORRUPTION) && defined(DETECT_UNDERRUNS)
	p->header.magic1 = (void*)((INTPTR)MAGIC1 ^ p->header.info.size);
	p->header.magic2 = (void*)((INTPTR)MAGIC2 ^ p->header.info.size);
#endif
	stats.nbytes_freelist_free += p->header.info.size;
	/* p->next is Stored inside data segment, need to initialize it
	   after the free pattern is filled into the data area. */
	p->next = NULL;
#ifdef QUICK_LISTS
	/* Always in DELAY_REUSE mode */
	list = p->header.info.size / ALIGNMENT;
	if (list < NLISTS) {
		if (quick_lists[list].tail) {
			quick_lists[list].tail->next = p;
			quick_lists[list].tail = p;
		}
		else {
			quick_lists[list].tail = quick_lists[list].head = p;
		}
		MALLOC_UNLOCK();
		return;
	}
#endif
#if defined(DELAY_REUSE)
	if (NULL != stats.freelist_tail)
		stats.freelist_tail->next = p;
	stats.freelist_tail = p;
	if (NULL == stats.freelist_head)
		stats.freelist_head = p;
#else
	p->next = stats.freelist_head;
	stats.freelist_head = p;
#endif
	MALLOC_UNLOCK();
}

/* Place a block first on the block list with block->free pointing to free
   memory of proper alignment 
*/
static int allocate_block()
{
	block_t *block;

	block = (block_t*)allocate_from_os( BLOCK_SIZE + sizeof(block_t) + ALIGNMENT );
	if (block == 0)
		return 0;

	block->free = ALIGNP((char*)block + sizeof(block_t));
	block->limit = block->free + BLOCK_SIZE;
	block->next = stats.blocklist;
	stats.blocklist = block;
	stats.nbytes_block += BLOCK_SIZE + sizeof(block_t) + ALIGNMENT;
	stats.nbytes_block_free = BLOCK_SIZE;
	return 1;
}

static void *allocate_dedicated_block( size_t nbytes ) /* request includes hdr */
{
	node_t *p = (node_t*)allocate_from_os( nbytes + ALIGNMENT );
	if (p == 0)
		return 0;
	stats.nbytes_block += nbytes + ALIGNMENT;
	return (node_t*)ALIGNP(p);
}

/* a site is location + tag
   static size table to make things easy, die if it overflows */

#if defined(USE_DEBUGGING_OP_STATUS)
#  define NSITES 10000
#else
#  define NSITES 40000
#endif

static site_stats_t alloc_pool[ NSITES ];
static site_stats_t *alloc_sites[ NSITES ];
static site_stats_t *alloc_sites_copy[ NSITES ];
static int sites_in_use = 0;

#ifdef RECORD_ALLOC_SITE_STACK
static BOOL is_stack_site(site_t site)
{
	if ( !stackset_initialized )
	{
		stackset.Init(NSTACK_ADDRS_START, NSTACK_ADDRS_MAX, ALLOC_SITE_STACK_SIZE);
		stackset_initialized = TRUE;
	}

	return stackset.InStackStore(site.stack);
}
#endif

static void initialize_size_stats()
{
	// Some systems (Mac) do not initialize globals properly!
	op_memset( alloc_pool, 0, sizeof(alloc_pool) );
	op_memset( alloc_sites, 0, sizeof(alloc_sites) );
	op_memset( alloc_sites_copy, 0, sizeof(alloc_sites_copy) );
}

static int compare_size_stats( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;

	int res = (*pa)->site.location - (*pb)->site.location;
#ifdef RECORD_ALLOC_SITE_STACK
	if (is_stack_site((*pa)->site) && is_stack_site((*pb)->site) && res == 0)
		res = (*pa)->site.size - (*pb)->site.size;
#endif
	return res;
}

static int compare_peak_stats_a( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;

	if (NULL == *pa || NULL == *pb)
		return 0;

	INT64 peaka = (*pa)->peak;
	INT64 peakb = (*pb)->peak;
	if (peaka > peakb)
		return -1;
	else if (peaka < peakb)
		return 1;
	else if ((*pa)->site.location < (*pb)->site.location)
		return -1;
	else if ((*pa)->site.location > (*pb)->site.location)
		return 1;
	else
		return 0; 
}

static int compare_size_stats_a( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;

	if (NULL == *pa || NULL == *pb)
		return 0;

	INT64 diffa = (*pa)->allocated - (*pa)->freed;
	INT64 diffb = (*pb)->allocated - (*pb)->freed;
	if (diffa > diffb)
		return -1;
	else if (diffa < diffb)
		return 1;
	else if ((*pa)->site.location < (*pb)->site.location)
		return -1;
	else if ((*pa)->site.location > (*pb)->site.location)
		return 1;
	else
		return 0; 
}

static site_stats_t *lookup_allocation_site( site_t site )
{
	site_stats_t ss;
	site_stats_t *ass = &ss;
	void *found;
	int i;

	op_memset( &ss, 0, sizeof(ss) );
	ss.site = site;

	if ((found = op_bsearch( &ass, alloc_sites, sites_in_use, sizeof(site_stats_t*),
							 compare_size_stats )) != 0)
	{
		return *(site_stats_t**)found;
	}

	OP_ASSERT( sites_in_use < NSITES );

	for ( i=0 ; i < sites_in_use && compare_size_stats( &alloc_sites[i], &ass ) < 0 ; i++ )
		;
	if (sites_in_use > i)
		op_memmove( &alloc_sites[i+1], &alloc_sites[i], sizeof(site_stats_t*)*(sites_in_use-i) );
	alloc_sites[i] = &alloc_pool[sites_in_use];
	alloc_pool[sites_in_use] = ss;
	sites_in_use++;
	return alloc_sites[i];
}

#ifdef RECORD_ALLOC_SITE_STACK
static site_t lookup_stack(void *stack[])
{
	site_t st;
	st.stack = NULL;
	st.size = ALLOC_SITE_STACK_SIZE; // Not really usefull, maybe we can use this for something else?

	if ( !stackset_initialized )
	{
		stackset.Init(NSTACK_ADDRS_START, NSTACK_ADDRS_MAX, ALLOC_SITE_STACK_SIZE);
		stackset_initialized = TRUE;
	}

	stackset.Add(stack, &st.stack);
	return st;
}
#endif

void dump_allocation_sites()
{
	FILE *fp;

	fp = fopen( "ALLOCSITES", "a" );
	if (!fp) return;
	do_summarize_allocation_sites( fp, sites_in_use );
	show( fp, "------------------------------------------------------------\n\n" );
	fclose( fp );
}

void summarize_allocation_sites( FILE *output )
{
	INT64 limit_sites = 20;
	if (0 == getenv_value("OPERA_DEBUG_MALLOC_NSITES", &limit_sites)) {
		if (-1 == limit_sites) /* Print all sites */
			limit_sites = sites_in_use;
	}
	do_summarize_allocation_sites(output, MIN(limit_sites, sites_in_use));
}

#ifdef RECORD_ALLOC_SITE_STACK
void summarize_allocation_stack_sites( FILE *output )
{
	INT64 limit_sites = 20;
	if (0 == getenv_value("OPERA_DEBUG_MALLOC_NSITES", &limit_sites)) {
		if (-1 == limit_sites) /* Print all sites */
			limit_sites = sites_in_use;
	}
	do_summarize_allocation_stack_sites(output, MIN(limit_sites, sites_in_use));
}
#endif

static const char *tagcode[] =
{
	"-",
	"M",	// malloc
	"N",	// new
	"A",	// new[]
	"-",
	"D",	// strdup uses malloc
	"D",	// except when it uses new (MSVC)
	"D",	// or array new
	"-",	
	"-",
	"S",	// setstr uses new (MSVC)
	"S",	// setstr really uses new[]
};

#ifdef PROFILE_SENT_MESSAGES
static const char *send_tagcode[] =
{
	"-",
	"S",
	"P",
	"D",
	"B"
};
#endif


static void show_site_stats(FILE *output, site_stats_t *site_stats)
{
	double current_use;
	double peak;
	char *current_use_type = "";
	char *peak_type = "";

    current_use = site_stats->allocated - site_stats->freed;
	if (current_use >= 1024)
	{
		current_use = KB(current_use);
		current_use_type = "K";
	}
	
	peak = site_stats->peak;
	if (peak >= 1024)
	{
		peak = KB(peak);
		peak_type = "K";
	}

#ifdef RECORD_ALLOC_SITE_STACK
	if (is_stack_site(site_stats->site))
	{
		show(output,
			 "   Current use and peak is %.1f%s (%d) and %.1f%s (%d) for the data allocated from: \n",
			 current_use, current_use_type,
			 (int) (site_stats->nallocated - site_stats->nfreed),
			 peak, peak_type,
			 (int) site_stats->npeak);
		
		write_stored_stack_trace(output, site_stats->site.stack, site_stats->site.size);
	}
	else
#endif
	{
		const char *addr_line = "";

#ifdef ADDR2LINE_SUPPORT
		addr_line = Addr2Line::TranslateAddress(site_stats->site.location);
#endif

		show(output,
			 "%s %7.1f%1s %7.1f%1s 0x%08lx %s\n", 
			 tagcode[site_stats->site.tag],
			 current_use, current_use_type,
			 peak, peak_type,
			 (long)(site_stats->site.location),
			 addr_line);
	}
}


static void do_summarize_allocation_sites( FILE *output, int nsites )
{
	int i, live_sites_in_use;
	INT64 inuse = 0;
	int nskipped;
	int sites_in_use_copy = sites_in_use;

	if (output == 0) output = stderr;

	op_memcpy( alloc_sites_copy, alloc_sites, sizeof(alloc_sites_copy) );
	nskipped = sites_in_use_copy;
	for ( i=live_sites_in_use=0 ; i < sites_in_use_copy ; i++ )
	{
#ifdef RECORD_ALLOC_SITE_STACK
		if (!is_stack_site(alloc_sites_copy[i]->site))
#endif
		{
			inuse += alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed; 
			if (alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed > 0)
				live_sites_in_use++;
			nskipped--;
		}
	}
	show( output, "In use: %.1fK allocated from %d sites (%d live)\n", KB(inuse), sites_in_use_copy - nskipped, live_sites_in_use );

	local_qsort( alloc_sites_copy, sites_in_use_copy, sizeof(site_stats_t*), compare_size_stats_a );
	show( output, "\nCurrent use and peak, sorted by use\n" );

	for ( i=0, nskipped=0; i < nsites + nskipped && i < sites_in_use_copy; i++ )
	{
#ifdef RECORD_ALLOC_SITE_STACK
		if (is_stack_site(alloc_sites_copy[i]->site))
		{
			nskipped++;
		}
		else
#endif
		if (alloc_sites_copy[i]->site.tag != 0 && alloc_sites_copy[i]->allocated)
			show_site_stats(output, alloc_sites_copy[i]);
	}

	show( output, "\nCurrent use and peak, sorted by peak\n" );
	local_qsort( alloc_sites_copy, sites_in_use_copy, sizeof(site_stats_t*), compare_peak_stats_a );
	for ( i=0, nskipped=0; i < nsites + nskipped && i < sites_in_use_copy; i++ )
	{
#ifdef RECORD_ALLOC_SITE_STACK
		if (is_stack_site(alloc_sites_copy[i]->site))
		{
			nskipped++;
		}
		else
#endif
		if (alloc_sites_copy[i]->site.tag != 0 && alloc_sites_copy[i]->peak > 102)
			show_site_stats(output, alloc_sites_copy[i]);
	}
}


#ifdef RECORD_ALLOC_SITE_STACK

static void do_summarize_allocation_stack_sites( FILE *output, int nsites )
{
	int i, live_sites_in_use;
	INT64 inuse = 0;
	int nskipped;
	int sites_in_use_copy = sites_in_use;

	if (output == 0) output = stderr;

	op_memcpy( alloc_sites_copy, alloc_sites, sizeof(alloc_sites) );
	nskipped = sites_in_use_copy;
	for ( i=live_sites_in_use=0 ; i < sites_in_use_copy ; i++ )
	{
		if (!is_stack_site(alloc_sites_copy[i]->site))
		{
			inuse += alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed; 
			if (alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed > 0)
				live_sites_in_use++;
			nskipped--;
		}
	}
	show( output, "In use: %.1fK allocated from %d sites (%d live)\n", KB(inuse), sites_in_use_copy - nskipped, live_sites_in_use );

	local_qsort( alloc_sites_copy, sites_in_use_copy, sizeof(site_stats_t*), compare_size_stats_a );
	show( output, "\nCurrent use and peak, sorted by use\n" );

	for ( i=0, nskipped=0; i < nsites + nskipped && i < sites_in_use_copy; i++ )
	{
		if (is_stack_site(alloc_sites_copy[i]->site) || (alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed) == 0)
		{
			nskipped++;
		}
		else if (alloc_sites_copy[i]->site.tag != 0 && alloc_sites_copy[i]->allocated)
		{
			show_site_stats(output, alloc_sites_copy[i]);
			show(output, "\n");
			for (int j = 0; j < sites_in_use_copy; j++)
			{
				if (is_stack_site(alloc_sites_copy[j]->site)
					&& alloc_sites_copy[j]->site.stack[0] == alloc_sites_copy[i]->site.location
					&& (alloc_sites_copy[j]->allocated - alloc_sites_copy[j]->freed))
				{
					show_site_stats(output, alloc_sites_copy[j]);
					show(output, "\n");
				}
			}
		}
	}

	show( output, "\nCurrent use and peak, sorted by peak\n" );
	local_qsort( alloc_sites_copy, sites_in_use_copy, sizeof(site_stats_t*), compare_peak_stats_a );
	for ( i=0, nskipped=0; i < nsites + nskipped && i < sites_in_use_copy; i++ )
	{
		if (is_stack_site(alloc_sites_copy[i]->site) || (alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed) == 0)
		{
			nskipped++;
		}
		else if (alloc_sites_copy[i]->site.tag != 0 && alloc_sites_copy[i]->peak > 102)
		{
			show_site_stats(output, alloc_sites_copy[i]);
			show(output, "\n");
			for (int j = 0; j < sites_in_use_copy; j++)
			{
				if (is_stack_site(alloc_sites_copy[j]->site)
					&& alloc_sites_copy[j]->site.stack[0] == alloc_sites_copy[i]->site.location
					&& alloc_sites_copy[j]->peak > 102
					&& (alloc_sites_copy[j]->allocated - alloc_sites_copy[j]->freed))
				{
					show_site_stats(output, alloc_sites_copy[j]);
					show(output, "\n");
				}
			}
		}
	}
}

static void dbg_allocsite(void *site_addr)
{
	int sites_in_use_copy = sites_in_use;

	op_memcpy( alloc_sites_copy, alloc_sites, sizeof(alloc_sites) );
	
	local_qsort( alloc_sites_copy, sites_in_use_copy, sizeof(site_stats_t*), compare_size_stats_a );
	show( stderr, "\n*** Current use and peak, sorted by use\n" );

	for (int i = 0; i < sites_in_use_copy; i++ )
	{
		if (alloc_sites_copy[i]->site.location == site_addr)
		{
			show_site_stats(stderr, alloc_sites_copy[i]);
			show(stderr, "\n");
			for (int j = 0; j < sites_in_use_copy; j++)
			{
				if (is_stack_site(alloc_sites_copy[j]->site)
					&& alloc_sites_copy[j]->site.stack[0] == alloc_sites_copy[i]->site.location
					&& alloc_sites_copy[i]->allocated)
				{
					show_site_stats(stderr, alloc_sites_copy[j]);
					show(stderr, "\n");
				}
			}
			break;
		}
	}
}

#endif

#if defined(USE_DEBUGGING_OP_STATUS)
int malloc_record_static_opstatus_failure( site_t site )
{
	site.tag = 0;
	site_stats_t *ss = lookup_allocation_site( site );
	ss->opstatus_failures++;
	return (ss->opstatus_failures > 1);
}

void summarize_opstatus_failures( FILE *output )
{
	int i,n;

	for ( i=0, n=0 ; i < sites_in_use ; i++ )
		if (alloc_sites[i]->opstatus_failures > 0)
			n++;
	show( output ? output : stderr, "\nStatic failures to check OP_STATUS: %d\n", n );
}
#endif

#if defined(PROFILE_SENT_MESSAGES)
static long long sent_messages = 0;

void malloc_record_sent_message( site_t site, SentMessageType type )
{
	sent_messages++;
	site.tag = 0;
	site_stats_t *ss = lookup_allocation_site( site );
	ss->sent_messages[(int)type]++;
}

static int compare_send_stats_a( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;
	long long suma = 0;
	long long sumb = 0;
	for ( int i=0 ; i < SENTMSG_NTYPES ; i++ ) {
		suma += (*pa)->sent_messages[i];
		sumb += (*pb)->sent_messages[i];
	}
	if (suma > sumb)
		return -1;
	else if (suma < sumb)
		return 1;
	else if ((*pa)->site.location < (*pb)->site.location)
		return -1;
	else if ((*pa)->site.location > (*pb)->site.location)
		return 1;
	else
		return 0; 
}

void do_summarize_sent_messages( FILE *output, int nsites )
{
	int i, j, tag;
	int sites_in_use_copy = sites_in_use;

	if (output == NULL) output = stderr;

	op_memcpy( alloc_sites_copy, alloc_sites, sizeof(alloc_sites) );
	local_qsort( alloc_sites_copy, sites_in_use_copy, sizeof(site_stats_t*), compare_send_stats_a );
  
	show( output, "\nSent messages by site, sorted by volume\n" );
	for ( i=0 ; i < nsites ; i++ ) {
		long long msgs = 0;
		for ( j=0 ; j < SENTMSG_NTYPES ; j++ )
			if (alloc_sites_copy[i]->sent_messages[j]) {
				msgs = alloc_sites_copy[i]->sent_messages[j];
				tag = j;
			}
		show( output, "%s %15g 0x%08x\n",
			  send_tagcode[tag], (double)msgs, (long)(alloc_sites_copy[i]->site.location) );
	}
}

void summarize_sent_messages( FILE *output )
{
	if (sent_messages == 0) {
		show( output ? output : stderr, "No message sends were recorded.\n" );
		return;
	}
	do_summarize_sent_messages( output, 20 );
}

#endif

#ifdef MSWIN
static HANDLE heap_handle = 0;
#endif

static void *allocate_from_os( size_t nbytes )
{
#if defined(QNX)
	return _smalloc( nbytes );
#elif defined(unix)
	void *p = sbrk( nbytes );
	if (p == (void*)-1)
		return 0;
	else
		return p;
#elif defined(MSWIN)
	static HANDLE heap_handle = 0;

	if (heap_handle == 0)
	{
		heap_handle = HeapCreate(0, 20 * 1024 * 1024, 0);
		if (heap_handle == 0)
			return NULL;
	}
	return HeapAlloc(heap_handle, 0, nbytes);
#elif TARGET_RT_MAC_CFM
	return NewPtr(nbytes);
#else
# error "must implement allocate_from_os"
#endif
}

/* Support function. Returns number of errors */
int malloc_check_free_node_integrity(node_t * p, BOOL printerrors)
{
	int errors=0;
#ifdef DETECT_UNDERRUNS
	if ((size_t)p->header.magic1 != ((size_t)MAGIC1 ^ p->header.info.size) || (size_t)p->header.magic2 != ((size_t)MAGIC2 ^ p->header.info.size))
	{
		errors++;
		if (printerrors)
		{
			show(stderr, "free node header corrupt (hadrr=%p, size=%d)\n", &(p->header), p->header.info.size);
		};
	};
#endif
	return errors;
};

/* Support function. Returns number of errors */
int malloc_check_alloced_node_integrity(node_t *p, BOOL printerrors)
{
	int errors=0;
#ifdef DETECT_UNDERRUNS
	if (MAGIC1 != p->header.magic1)
	{
		errors++;
		if (printerrors)
			show(stderr, "Wild pointer (node=%p), large buffer underrun, overrun from previous or other error... (magic1: %p -> %p)\n", p, MAGIC1, p->header.magic1);
	};
	if (MAGIC2 != p->header.magic2)
	{
		errors++;
		if (printerrors)
			show(stderr, "buffer underrun (node=%p):  magic2: %p -> %p\n", p, MAGIC2, p->header.magic2);
	};
#endif
#ifdef DETECT_OVERRUNS
	if (MAGIC2 != *(p->header.endmagic))
	{
		errors++;
		if (printerrors)
			show(stderr, "buffer overrun (node=%p):  endmagic: %p -> %p\n", p, MAGIC2, *(p->header.endmagic));
	};
#endif
	return errors;
};

BOOL malloc_check_pointer_integrity(void *datum, BOOL printerrors)
{
	return malloc_check_alloced_node_integrity(get_node(datum), printerrors) == 0;
};

BOOL malloc_check_integrity(BOOL printerrors)
{
	int errors=0;
#ifdef QUICK_LISTS
	for (int l=0; l<NLISTS; l++)
	{
		node_t *p = quick_lists[l].head;
		int expected_size = (int)(l * ALIGNMENT);

		do
		{
			if (p != 0)
			{
				if (p->header.info.size != expected_size)
				{
					errors++;
					if (printerrors)
						show(stderr, "quick_list[%d] has element with size==%d rather than %zd\n", l, p->header.info.size, expected_size);
				};
				errors+=malloc_check_free_node_integrity(p, printerrors);
				p=p->next;
			};
		} while (p != 0  &&  p != quick_lists[l].tail);

	};
#endif

	for (node_t * p=stats.freelist_head; p!=0; p=p->next)
	{
		errors+=malloc_check_free_node_integrity(p, printerrors);
	};

	if (errors>0 && printerrors)
		show(stderr, "Total number of malloc structure errors: %d\n", errors);

	return errors==0;
};

#if TARGET_RT_MAC_CFM
void show(FILE *ignoredStream, const char *format, ...)
{
 	char o[1024]; /* ARRAY OK 2009-06-18 johanh */
 	FSSpec fss = { -102, 15730, "\pMemoryDebug.log"};
 
 	short refNum;
 	if(FSpOpenDF(&fss, fsWrPerm, &refNum) == noErr)
 	{
 		SetFPos(refNum, fsFromLEOF, 0L);
 	
 		va_list ap;
 		va_start(ap, format);
 		vsnprintf(o, 1024, format, ap);
 		va_end(ap);
 		
 		long l;
 		l = strlen(o);
 		OSErr err = FSWrite(refNum, &l, o);
 		
 		FSClose(refNum);
 	} else {
 		if(FSpCreate(&fss, 'OPRA', 'TEXT', smRoman) == noErr)
 		{
 			FSClose(refNum);
 		}
 	}
}
#endif // TARGET_RT_MAC_CFM

#if MSWIN
void show(FILE *ignoredStream, const char *format, ...)
{
	FILE *fp = fopen( "c:\\klient\\memdump.txt", "a" );
	if (fp == NULL)
		return;

 	va_list ap;
 	va_start(ap, format);
	vfprintf( fp, format, ap );
	va_end( ap );

	fclose( fp );
}
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>

/**
 * Naive implementation returning only the few values I used in my
 * statistics. [pere 2001-03-28]
 */
struct mallinfo
mallinfo(void)
{
	struct mallinfo mi;

	mi.smblks   = 0;
	mi.ordblks  = 0;
	mi.fordblks = stats.nbytes_freelist_free;
	mi.uordblks = stats.nbytes_alloc+stats.nbytes_new+stats.nbytes_anew;
	mi.arena    = mi.uordblks + mi.fordblks;
	mi.hblks    = 0;
	mi.hblkhd   = 0;
	mi.fsmblks  = 0;
	mi.keepcost = 0;
	mi.usmblks  = 0;

	return mi;
}
#endif /* HAVE_MALLOC_H */

#endif //HAVE_LTH_MALLOC
