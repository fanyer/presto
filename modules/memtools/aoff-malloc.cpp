/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2001-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Address-ordered-first-fit malloc implementation.
 * Lars T Hansen
 *
 * A malloc engineered to mimic that of Symbian, to measure fragmentation
 * without having to run on the emulator or target.  NOTE, however, that even
 * the description we received was sketchy on how realloc behaves: realloc can
 * choose to operate in several ways.  I have marked the choice points that I
 * know about with POLICY in the code.
 *
 * Platforms currently supported:
 *   QNX
 *   Unix/Linux
 *   WIn32
 */

#include "core/pch.h"

#undef NDEBUG

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <new.h>

//#define LOG_OOM_EVENTS                          /* log all OOM events to ./oomlog.aoff */

#if defined(_PHOTON_)
# define QNX
#elif defined (MSWIN)
# include <unistd.h>
#else
# define UNIX
# define REENTRANT
#endif

#define CPLUSPLUS             					/* new, new[], delete, and delete[] too */
#define CLEVER_REALLOC							/* merge with adjoining blocks when possible */

#define DEFAULT_HEAPSIZE    (3*1024*1024)   	/* bytes */

#define ALIGNMENT           (sizeof(int))		/* Warning, code is not setup to tackle bigger alignments! */
#define MIN_OBJSIZE         (sizeof(node_t))	/* or the object cannot be represented on free list */

#include "core/unicode.h"

#define MAX( a, b )			((a) > (b) ? (a) : (b))
#define MIN( a, b )			((a) < (b) ? (a) : (b))

/* Object header for allocated objects */
typedef struct {
  unsigned int size;
} header_t;

/* Object header for free objects */
typedef struct node {
  header_t     header;
  struct node *next;
} node_t;

static struct {
  /* allocation data */
  node_t     *freelist;				/* Pointer to address-ordered free blocks */
#ifndef NDEBUG
  void       *membase;              /* Pointer to first byte (for debugging) */
  void       *memlimit;             /* Pointer past last byte (for debugging) */
#endif

  int        quiet;                 /* 0=verbose, 1=quiet, 2=silent */

  /* statistics data */
  INT64      heapmax;          		/* Requested max heap size */
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
  INT64      nbytes_freelist_free;  /* Currently free on free list */

  INT64      oom_events;			/* Number of times we were OOM */
  INT64      oom_free_sum;			/* Sum of storage free at OOM events */
  INT64      oom_free_minus_request_sum; /* Sum of difference of free and request at OOM events */
  INT64      oom_request_sum;		/* Sum of requests at OOM events */
} stats;

static INT64 stats_checksum, stats_checksum_tmp;

#ifndef NDEBUG
# define COMP_CHECKSUM()                 \
    do { INT64 *P=&stats.heapmax;        \
         int I=16;                       \
         stats_checksum_tmp=-1;          \
         while (I-- > 0)                 \
             stats_checksum_tmp ^= *P++; \
    } while(0)
# define SET_CHECKSUM()                                 \
    do { COMP_CHECKSUM();                               \
         stats_checksum=stats_checksum_tmp;             \
         if (malloc_initialized == 1) check_freelist(); \
    } while(0)
# define CHECK_CHECKSUM()                                                           \
    do { COMP_CHECKSUM();                                                           \
         assert( malloc_initialized != 1 || stats_checksum == stats_checksum_tmp ); \
         if (malloc_initialized == 1) check_freelist();                             \
    } while(0)
#else
# define SET_CHECKSUM()   ((void)0)
# define CHECK_CHECKSUM() ((void)0)
#endif

#define ALIGN(nbytes) ((nbytes + ALIGNMENT - 1) & ~(ALIGNMENT-1))
#define ALIGNP(p)     ((char*)(((unsigned long)(p) + ALIGNMENT - 1) & ~(ALIGNMENT-1)))
#define KB(x)         ((x)/1024)

#if defined(REENTRANT)
#  if defined(unix)
#    define MALLOC_LOCK() do { while (critical) sleep(1); critical=1; } while(0)
#  else
#    define MALLOC_LOCK() do { while (critical); critical=1; } while(0)
#  endif
#  define MALLOC_UNLOCK() critical=0
#else
#  define MALLOC_LOCK() ((void)0)
#  define MALLOC_UNLOCK() ((void)0)
#endif

static void *allocate_instrumented( size_t nbytes, INT64 *acct1, INT64 *acct2 );
static void free_instrumented( void *datum, INT64 *acct, INT64 *nact );
static void *allocate_from_os( size_t nbytes );
static void exit_stats();
#ifndef NDEBUG
static void check_freelist();
#endif
#ifdef CLEVER_REALLOC
static node_t *attach_adjoining( node_t *node, size_t requested );
static void release_trailing( node_t *obj, size_t requested );
#endif

static int malloc_initialized = 0;
#ifdef REENTRANT
static volatile int critical = 0;
#endif

static node_t*      get_node(void *data)    { return (node_t*)((char*)data - sizeof(header_t)); }
static void*        get_data(node_t *p)     { return (char*)p + sizeof(header_t); }
static unsigned int get_datasize(node_t *p) { return p->header.size - sizeof(header_t); }

static void init_malloc()
{
  if (!malloc_initialized) {
    malloc_initialized = 2;

	const char *val = getenv( "OPERA_DEBUG_MALLOC_HEAPMAX");
	if (val != NULL)
      stats.heapmax = atol(val);
    else
      stats.heapmax = DEFAULT_HEAPSIZE;

	val = getenv( "OPERA_DEBUG_MALLOC_QUIET");
	if (val != NULL)
	  stats.quiet = atoi(val);
	else
	  stats.quiet = 0;

	stats.freelist = (node_t*)allocate_from_os( stats.heapmax );
	if (stats.freelist == 0)
	  abort();
    stats.freelist->header.size = (unsigned int)stats.heapmax;
    stats.freelist->next = 0;
    stats.nbytes_freelist_free = stats.heapmax;
#ifndef NDEBUG
    stats.membase = stats.freelist;
    stats.memlimit = (char*)stats.membase + stats.heapmax;
#endif

    SET_CHECKSUM();
	malloc_initialized = 1;

    atexit( exit_stats );
  }
}

void memory_allocation_stats( FILE *output )
{
  if (output == 0) output = stderr;
#ifdef CPLUSPLUS
  INT64 sum = stats.nbytes_alloc + stats.nbytes_new + stats.nbytes_anew;
  fprintf( output,
	   "alloc use %6lldK, new use %6lldK, anew use %6lldK\n"
	   //"allocd   %6lldK, newd   %6lldK, anewd    %6lldK\n"
       //"freed    %6lldK, deld   %6lldK, adeld    %6lldK\n"
       "freelist  %6lldK, use     %6lldK, peak use %6lldK\n",
	   KB(stats.nbytes_alloc), KB(stats.nbytes_new), KB(stats.nbytes_anew),
	   //KB(stats.nbytes_alloc_total), KB(stats.nbytes_new_total), KB(stats.nbytes_anew_total),
	   //KB(stats.nbytes_free_total), KB(stats.nbytes_delete_total), KB(stats.nbytes_adelete_total),
	   KB(stats.nbytes_freelist_free), KB(sum), KB(stats.nbytes_peak) );
#else
  fprintf( output,
	   "allocd %6lldK, freed %6lldK, allocuse %6lldK, free %6lldK\n",
	   KB(stats.nbytes_alloc_total), KB(stats.nbytes_free_total),
	   KB(stats.nbytes_alloc), KB(stats.nbytes_freelist_free) );
#endif
  if (stats.oom_events > 0)
  {
	fprintf( output, "OOM events: %4lld  AVG free: %6lld  AVG request: %6lld  AVG diff: %6lld\n",
			 stats.oom_events, stats.oom_free_sum / stats.oom_events,
			 stats.oom_request_sum / stats.oom_events,
			 stats.oom_free_minus_request_sum / stats.oom_events );
  }
}

void *internal_malloc( size_t nbytes )
{
  CHECK_CHECKSUM();
  void *p = allocate_instrumented( nbytes, &stats.nbytes_alloc, &stats.nbytes_alloc_total );
  SET_CHECKSUM();
  return p;
}

#if defined (__QNX__) && defined (DINKUM)
_C_STD_BEGIN
#endif __QNX__ && DINKUM
void *malloc( size_t nbytes )
{
  return internal_malloc( nbytes );
}

void *calloc( size_t nobj, size_t objsize )
{
  void *p;
  size_t nbytes = nobj * ALIGN(objsize);

  p = internal_malloc( nbytes );
  if (p)
    memset( p, 0, nbytes );
  return p;
}

void *realloc( void *obj, size_t nbytes )
{
  node_t *p, *x;
  void *q;

  if (obj == 0)
    return internal_malloc( nbytes );

  p = get_node(obj);
  size_t oldsize = get_datasize(p);

  CHECK_CHECKSUM();

  if (oldsize >= nbytes)
  {
#ifdef CLEVER_REALLOC
    /* POLICY: Could choose not to do this, but to keep the entire object. */
    release_trailing( p, nbytes+sizeof(header_t) );
#endif
    SET_CHECKSUM();
    return obj;
  }

#ifdef CLEVER_REALLOC
  /* POLICY: Could choose not to do this, but always copy the block. */
  if ((x = attach_adjoining( p, nbytes+sizeof(header_t) )) != 0)
  {
    SET_CHECKSUM();
	return get_data(x);
  }
#endif

  if ((q = internal_malloc( nbytes )) == 0)
    return 0;

  memcpy( q, obj, MIN(oldsize, nbytes) );
  free( obj );
  return q;
}

void free( void *datum )
{
  CHECK_CHECKSUM();
  if (datum)
    free_instrumented( datum, &stats.nbytes_free_total, &stats.nbytes_alloc );
  SET_CHECKSUM();
}
#if defined (__QNX__) && defined (DINKUM)
_C_STD_END
#endif __QNX__ && DINKUM

#if defined(CPLUSPLUS)

void* operator new( size_t nbytes ) throw ()
{
  void *ptr = 0;

  CHECK_CHECKSUM();

  while (1)
  {
	ptr = allocate_instrumented( nbytes, &stats.nbytes_new, &stats.nbytes_new_total );
	if (ptr != 0)
	  break;

	new_handler global_new_handler = set_new_handler(0);
	set_new_handler(global_new_handler);

	if (global_new_handler == 0)
      break;
	(*global_new_handler)();
  }
  SET_CHECKSUM();
  return ptr;
}

void* operator new[]( size_t nbytes ) throw ()
{
  void *ptr = 0;

  CHECK_CHECKSUM();

  while (1)
  {
	ptr = allocate_instrumented( nbytes, &stats.nbytes_anew, &stats.nbytes_anew_total );
	if (ptr != 0)
      break;

	new_handler global_new_handler = set_new_handler(0);
	set_new_handler(global_new_handler);

	if (global_new_handler == 0)
      break;
	(*global_new_handler)();
  }
  SET_CHECKSUM();
  return ptr;
}

void operator delete( void* datum ) throw ()
{
  CHECK_CHECKSUM();
  if (datum)
    free_instrumented( datum, &stats.nbytes_delete_total, &stats.nbytes_new );
  SET_CHECKSUM();
}

void operator delete[]( void* datum ) throw ()
{
  CHECK_CHECKSUM();
  if (datum)
    free_instrumented( datum, &stats.nbytes_adelete_total, &stats.nbytes_anew );
  SET_CHECKSUM();
}

#endif // CPLUSPLUS

static void exit_stats()
{
  fprintf( stderr, "\nOpera memory allocation stats at program's end:\n" );
  memory_allocation_stats( stderr );
  fprintf( stderr, "\n" );
}

#ifndef NDEBUG
static void check_freelist()
{
/*
  size_t free = (size_t)stats.nbytes_freelist_free;
  size_t free_size=0;

  for ( node_t *p=stats.freelist; p != 0 ; p=p->next )
    free_size += p->header.size;

  assert (free_size == free);
*/
}
#endif

#ifdef LOG_OOM_EVENTS
#  include "dbug/oom-logger.c"
#endif

static void oom_event(size_t requested)
{
  /* Region of no storage allocation */
  size_t sum = (size_t)(stats.nbytes_alloc + stats.nbytes_new + stats.nbytes_anew);
  size_t free = (size_t)stats.nbytes_freelist_free;

#ifdef LOG_OOM_EVENTS
  log_oom_event( "oomlog.aoff", requested, stats.heapmax, stats.heapmax-free );
#endif

  stats.oom_events++;
  stats.oom_free_sum += free;
  stats.oom_free_minus_request_sum += free-requested;
  stats.oom_request_sum += requested;

  size_t free_size=0, free_count=0, free_maxsize=0;
  node_t *p;
  for ( p=stats.freelist; p != 0 ; p=p->next )
  {
	free_count++;
    free_size += p->header.size;
    free_maxsize = MAX(p->header.size, free_maxsize);
  }
  /* end region of no storage allocation */

  if (stats.quiet < 2)
    fprintf( stderr, "AOFF is OOM: used=%uK free=%uK requested=%u length=%d largest=%u\n",
	         KB(sum), KB(free), requested, free_count, free_maxsize );

  if (stats.quiet < 1)
  {
    memory_allocation_stats(stderr);
    fprintf( stderr, "\n" );
  }

  if (free_size != free && stats.quiet < 2)
	fprintf( stderr, "**** AOFF accounting error! free=%u, check=%d\n\n", free, free_size );
}

static void loa_event(size_t requested)
{
	// Just a hook for a breakpoint.
}

static void take_from_freelist( node_t *node, node_t *prev, size_t actual_bytes )
{
  if (node->header.size - actual_bytes < MIN_OBJSIZE)
  {
    /* too small to split */
    if (prev)
      prev->next = node->next;
    else
      stats.freelist = node->next;
  }
  else
  {
    /* split it */
    node_t *leftover;

	leftover = (node_t*)((char*)node+actual_bytes);
	/* The ordering of these two assignments matters when realloc grows a node by one word. */
	leftover->next = node->next;
	leftover->header.size = node->header.size - actual_bytes;

    if (prev)
  	  prev->next = leftover;
	else
	  stats.freelist = leftover;
    node->header.size = actual_bytes;
  }
  stats.nbytes_freelist_free -= node->header.size;
}

#ifdef CLEVER_REALLOC
/* merge obj with its neighbors on the free list if possible, and
   return a pointer to a new node if so.  Prefer to merge with the
   succeeding block if it is large enough.

   Return 0 if merge cannot be accomplished.

   POLICY: One can be less biased in merging by choosing the block
   that appears to be most advantageous.  For example, if the preceding
   block can be absorbed completely then perhaps that is a better choice
   than the succeeding block if the latter must be split.
   */
static node_t *attach_adjoining( node_t *obj, size_t requested )
{
  node_t *node, *prev, *prevprev;
  int has_successor, has_predecessor;

  if (requested < MIN_OBJSIZE) requested = MIN_OBJSIZE;
  requested = ALIGN(requested);

  MALLOC_LOCK();

  for ( node=stats.freelist, prev=0, prevprev=0 ; node && node < obj ; prevprev=prev, prev=node, node=node->next )
    ;

  assert( node == 0 || node > obj );
  assert( prev == 0 || prev < obj );
  assert( prevprev == 0 || prevprev < prev );

  has_successor = node && (char*)node == (char*)obj+obj->header.size;
  has_predecessor = prev && (char*)prev+prev->header.size == (char*)obj;

  if (has_successor && obj->header.size + node->header.size >= requested)
  {
	/* merge with successor */
    take_from_freelist( node, prev, requested-obj->header.size );
    stats.nbytes_alloc += node->header.size;
    stats.nbytes_alloc_total += node->header.size;
    obj->header.size += node->header.size;
    MALLOC_UNLOCK();
    return obj;
  }
  else
  {
    if (has_predecessor && prev->header.size + obj->header.size >= requested)
    {
      /* merge with predecessor */
      take_from_freelist( prev, prevprev, prev->header.size );
      stats.nbytes_alloc += prev->header.size;
      stats.nbytes_alloc_total += prev->header.size;
      prev->header.size += obj->header.size;
    }
    else if (has_predecessor && has_successor && prev->header.size + obj->header.size + node->header.size >= requested)
    {
      /* merge with both */
      take_from_freelist( node, prev, node->header.size );
      take_from_freelist( prev, prevprev, prev->header.size );
      stats.nbytes_alloc += prev->header.size + node->header.size;
      stats.nbytes_alloc_total += prev->header.size + node->header.size;
      prev->header.size += obj->header.size + node->header.size;
    }
    else
    {
      MALLOC_UNLOCK();
      return 0;
    }

    /* copy data, free any excess */
    MALLOC_UNLOCK();
    memmove( get_data(prev), get_data(obj), get_datasize(obj) );
    if (prev->header.size - requested >= MIN_OBJSIZE)
    {
      node_t *tmp = (node_t*)((char*)prev + requested);
      tmp->header.size = prev->header.size - requested;
      prev->header.size = requested;
      free_instrumented( get_data(tmp),  &stats.nbytes_free_total, &stats.nbytes_alloc );

    }
    return prev;
  }
}

static void release_trailing( node_t *obj, size_t requested )
{
  if (requested < MIN_OBJSIZE) requested = MIN_OBJSIZE;
  requested = ALIGN(requested);

  if (obj->header.size - requested < MIN_OBJSIZE)
    return;

  node_t *tmp = (node_t*)((char*)obj + requested);
  tmp->header.size = obj->header.size - requested;
  obj->header.size = requested;
  free_instrumented( get_data(tmp), &stats.nbytes_free_total, &stats.nbytes_alloc );
}
#endif // CLEVER_REALLOC

static node_t *find_on_freelist( size_t actual_bytes )
{
  node_t *node, *prev;

  for ( node=stats.freelist, prev=0 ; node && node->header.size < actual_bytes ; prev=node, node=node->next )
    ;
  if (!node)
    return 0;

  take_from_freelist( node, prev, actual_bytes );
  return node;
}


/* Hack to allow allocation while malloc is being initialized; this is
   needed at least on QNX (because _smalloc() calls malloc...)
   */
static char startup_fund[ 8192 ];
static char *startup_fund_p = startup_fund;

static void *alloc_from_startup_fund( size_t nbytes )
{
  char *p = startup_fund_p;
  startup_fund_p += nbytes;
  assert( startup_fund_p < startup_fund + sizeof(startup_fund) );
  return p;
}

static size_t large_alloc_cutoff;

static void *allocate_instrumented( size_t nbytes, INT64 *acct1, INT64 *acct2 )
{
  node_t *node;
  size_t actual_bytes;

  if (malloc_initialized == 2)
    return alloc_from_startup_fund( nbytes );

  init_malloc();	// Outside locked region

  MALLOC_LOCK();

  nbytes += sizeof(header_t);
  if (nbytes < MIN_OBJSIZE) nbytes = MIN_OBJSIZE;
  actual_bytes = ALIGN(nbytes);

  if (large_alloc_cutoff > 0 && actual_bytes >= large_alloc_cutoff)
	loa_event(actual_bytes);

  node = find_on_freelist( actual_bytes );
  if (!node)
  {
    oom_event(actual_bytes);
    MALLOC_UNLOCK();
    return 0;
  }

  assert( node && node->header.size >= actual_bytes );

  if (acct1) *acct1 += node->header.size;
  if (acct2) *acct2 += node->header.size;

  stats.nbytes_peak = MAX( stats.nbytes_peak, stats.nbytes_alloc+stats.nbytes_new+stats.nbytes_anew );

  MALLOC_UNLOCK();
  return get_data(node);
}

static void free_instrumented( void *datum, INT64 *acct, INT64 *nact )
{
  node_t *x, *p, *q;
  INT64 free_before = stats.nbytes_freelist_free;
  size_t obj_size;

  MALLOC_LOCK();

  assert( malloc_initialized );

  if (datum >= startup_fund && datum < startup_fund+sizeof(startup_fund))
    return;

  assert( stats.membase <= datum && datum < stats.memlimit );

  x = get_node(datum);
  assert( x->header.size >= MIN_OBJSIZE );
  x->next = 0;
  obj_size = x->header.size;

  if (acct) *acct += x->header.size;
  if (nact) *nact -= x->header.size;
  stats.nbytes_freelist_free += x->header.size;

  /* find spot in free list */
  for ( p=stats.freelist, q=0 ; p != 0 && p < x ; q=p, p=p->next )
    ;
  assert( p == 0 || p > x );
  assert( q == 0 || q < x );

  /* insert block, merging with predecessor and/or successor if possible */
  if (q && p && (char*)q+q->header.size == (char*)x && (char*)x+x->header.size == (char*)p)
  {
    q->header.size = q->header.size + x->header.size + p->header.size;
    q->next = p->next;
  }
  else if (q && (char*)q+q->header.size == (char*)x)
  {
    q->header.size = q->header.size + x->header.size;
  }
  else if (p && (char*)x+x->header.size == (char*)p)
  {
    x->header.size = x->header.size + p->header.size;
    x->next = p->next;
    if (q)
      q->next = x;
    else
      stats.freelist = x;
  }
  else
  {
    if (q)
      q->next = x;
    else
      stats.freelist = x;
    x->next = p;
  }

  MALLOC_UNLOCK();
  check_freelist();
}

static void *allocate_from_os( size_t nbytes )
{
#if defined(QNX)
  return _smalloc( nbytes );
#elif defined(UNIX)
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
      return 0;
  }
  return HeapAlloc(heap_handle, 0, nbytes);
#else
# error "must implement allocate_from_os"
#endif
}
