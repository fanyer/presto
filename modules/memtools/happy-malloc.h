/* -*- mode: c++; tab-width: 4 -*-
 *
 * Copyright 2001 Opera Software AS.  All rights reserved.
 *
 * Debug malloc library (aka happy-malloc aka lth-malloc)
 * Lars T Hansen
 *
 * $Id$
 *
 *   *** DO NOT USE THIS LIBRARY IN PRODUCTION BUILDS ***
 *
 */

#if !defined(_DEBUG_MALLOC_H_) && defined(HAVE_LTH_MALLOC)
#define _DEBUG_MALLOC_H_

#include <stdio.h>

#if defined(_PHOTON_)
# define QNX
#elif defined (MSWIN)
/* Nothing to define */
#else
# define REENTRANT
#endif

#define DELAY_REUSE			  			/* catches more errors by reusing objects FIFO */
#define CPLUSPLUS             			/* new, new[], delete, and delete[] too */
#define ACCOUNTING            			/* more expensive bookkeeping */
#define RECORD_ALLOC_SITE     			/* is needed for many other functions too */
#define QUICK_LISTS			  			/* speeds up allocation, especially with DELAY_REUSE */
#define CHECK_FREELIST_CORRUPTION       /* extra-extra sanity check -- requires DETECT_UNDERRUNS! */
//#define CHECK_FREELIST_ALWAYS         /* does what you think; may be expensive.  NOT YET IMPLEMENTED. */
#define DETECT_OVERRUNS                 /* detect object overruns (costs two words) */
#define DETECT_UNDERRUNS                /* detect object underruns (costs two words) */
//#define SYNTHESIZE_OCCASIONAL_FAILURE	/* synthesize failures pseudorandomly */
//#define SYNTHESIZE_SYSTEMATIC_FAILURE /* Synthesize failures based on call stack info */
//#define NEVER_FREE_MEMORY             /* Use with some care */

#ifdef _PHOTON_
# define EXCLUDE_PHOTON_LIBRARIES    	/* From synthesized failures */
# define PHOTON_CUTOFF 2048             /*   but only if smaller than this */
#endif

#ifdef CHECK_FREELIST_CORRUPTION
# ifndef DETECT_UNDERRUNS
#  error "You must enable DETECT_UNDERRUNS to use CHECK_FREELIST_CORRUPTION."
# endif
#endif

//#define HAVE_MALLOC_H       			/* If <malloc.h> declare mallinfo() */

#if defined(SYNTHESIZE_SYSTEMATIC_FAILURE) || defined(SYNTHESIZE_OCCASIONAL_FAILURE)
# define SYNTHESIZE_FAILURE 
#endif

/* GET_CALLER is a macro that accepts a char* and stores the return address of the
   function executing GET_CALLER into it.
   GCC has a primitive for this; other compilers must use inline assembler. */

#if defined(RECORD_ALLOC_SITE) || defined(PROFILE_SENT_MESSAGES)
# ifdef MSWIN
#   define GET_CALLER( location ) \
               do{ _asm mov eax, [ebp+4] _asm mov location, eax } while (0)		// x86, msvc syntax
# elif defined _MACINTOSH_
#   define GET_CALLER( location ) \
               do { __asm { mflr r0 ; stw r0, location } } while(0)				// ppc
# elif __GNUC__ && intel_x86  // Warning: GCC specific
#   define GET_CALLER( location ) \
               asm ( "movl 4(%%ebp), %0" : "=g" (location) )						// x86, gcc syntax
# elif __GNUC__ // not intel_x86
#   define GET_CALLER( location ) \
               do{ location = (char*)__builtin_return_address(0); } while (0)		// generic
# else
#   error "You need to undefine RECORD_ALLOC_SITE or define GET_CALLER() for this platform."
# endif // MSWIN
#endif // RECORD_ALLOC_SITE

/* LTH_MALLOC_SITE declares a variable of type site_t, initializes it with the tag,
   and stores the return address in it if appropriate.
   */
#ifdef RECORD_ALLOC_SITE
# define LTH_MALLOC_SITE(var,thetag) \
	site_t var; \
	var.tag = thetag; \
	GET_CALLER(var.location)
# else
# define LTH_MALLOC_SITE(var,thetag) \
	site_t var; \
	var.tag = thetag
#endif

#if defined(PROFILE_SENT_MESSAGES)
enum SentMessageType {
    SENTMSG_UNUSED,
	SENTMSG_SEND_MESSAGE,
	SENTMSG_POST_MESSAGE,
	SENTMSG_POST_DELAYED_MESSAGE,
    SENTMSG_LOCAL_BROADCAST,
	SENTMSG_NTYPES						// always last!
};
#endif	// PROFILE_SENT_MESSAGES

enum MallocTags 
{
	// Low two bits record the allocation method.  DO NOT CHANGE THESE!
	MALLOC_TAG_MALLOC = 0x01,
	MALLOC_TAG_NEW    = 0x02,
	MALLOC_TAG_ANEW   = 0x03,

	// Next two bits record the allocation routine if it is a standard
	// wrapper around allocation: strdup, SetStr, etc.  DO NOT CHANGE THESE!
	MALLOC_TAG_NOWRAPPER = 0x00 << 2,
	MALLOC_TAG_STRDUP    = 0x01 << 2,
	MALLOC_TAG_SETSTR    = 0x02 << 2
};

#define ALLOC_SITE_STACK_SIZE 10

struct site_t {
/* Anonymous structs like this are not accepted by gcc (at least not in my
 * version). Therefore, the ifdef for RECORD_ALLOC_SITE_STACK has to be here,
 * and, furthermore, if RECORD_ALLOC_SITE_STACK is defined, it will not compile
 * with all compilers.
 */
#ifdef RECORD_ALLOC_SITE_STACK
	union {
		struct {
#endif
			char *location;
			int tag;
#ifdef RECORD_ALLOC_SITE_STACK
		};
		struct {
			void **stack;
			int size;
		};
	};
#endif
};

#if defined(PARTIAL_LTH_MALLOC)
#  ifdef DEBUG_PARTIAL_LTH_MALLOC
#    define enter_non_oom_code() dbg_enter_non_oom_code(__FILE__, __LINE__)
#    define exit_non_oom_code() dbg_exit_non_oom_code(__FILE__, __LINE__)
     extern void dbg_enter_non_oom_code(char *file, int line);
     extern void dbg_exit_non_oom_code(char *file, int line);
     extern void dbg_reset_malloc_state(const void *old_oom_state);

#define SAVE_MALLOC_STATE() \
    const void *_old_oom_state = (const void *) in_non_oom_code()

#define RESET_MALLOC_STATE() \
    dbg_reset_malloc_state(_old_oom_state)

#  else
void set_silent_oom();
void unset_silent_oom();
void enter_non_oom_code();
void exit_non_oom_code();
#  endif
BOOL in_non_oom_code();
BOOL use_silent_oom();
void* malloc_lth(size_t size);
void free_lth(void *ptr);

#ifndef SAVE_MALLOC_STATE
#define SAVE_MALLOC_STATE() \
    BOOL _old_oom_state = in_non_oom_code()
#endif

#ifndef RESET_MALLOC_STATE
#define RESET_MALLOC_STATE() \
    do { \
        if (_old_oom_state) \
            enter_non_oom_code(); \
        else \
            exit_non_oom_code(); \
    } while (0)
#endif

#define OOM_RETURN_IF_ERROR( expr )    \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr; \
        if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
   {	\
       RESET_MALLOC_STATE(); \
	   return RETURN_IF_ERROR_TMP; \
   }	\
   } while(0)

#endif // PARTIAL_LTH_MALLOC

/* Given an object allocated by happy-malloc, return its (user) size.  
   The user size may be slightly larger than the requested size at
   allocation, since it may have been rounded up or be subject to some
   minimum size.
   */
size_t object_size_lth( void* p );

void operator delete(void* datum);
void operator delete[](void* datum);

#if defined(HAVE_MPROTECT) && defined(HAVE_MMAP) || defined(QNX_PROTECTED_MALLOC)
extern void *malloc_protected( size_t nbytes );
extern void malloc_protect_all_allocations(BOOL f);
extern int  is_protected_object( void *addr );
#endif

extern void *internal_malloc( size_t nbytes, site_t site );
extern void summarize_allocation_sites( FILE *output );
extern void memory_allocation_stats( FILE *output );
extern void summarize_stats_at_exit();		   	// Normally called by atexit

#if defined(USE_DEBUGGING_OP_STATUS)
extern int  malloc_record_static_opstatus_failure( site_t site );
extern void summarize_opstatus_failures( FILE *output );
#endif
#if defined(PROFILE_SENT_MESSAGES)
extern void malloc_record_sent_message( site_t site, SentMessageType type );
extern void summarize_sent_messages( FILE *output );
#endif


/* These are debugging functions that can be called to find out when memory
 * corruption occurs.
 *
 * malloc_check_integrity will make as many integrity checks as possible on
 * the malloc meta-data structures.
 *
 * malloc_check_pointer_integrity will make as many integrity checks as
 * possible on the allocated pointer "datum".
 *
 * Both functions return FALSE if any errors are found and TRUE if no errors
 * are found.
 *
 * If the parameter printerrors is TRUE, then these functions will print
 * diagnostic messages (using fprintf to stderr...) for each error it finds.
 */
extern BOOL malloc_check_integrity(BOOL printerrors=TRUE);
extern BOOL malloc_check_pointer_integrity(void *datum, BOOL printerrors=TRUE);

#endif // _DEBUG_MALLOC_H_
