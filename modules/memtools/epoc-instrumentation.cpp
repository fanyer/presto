/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Instrumentation for EPOC emulator memory allocation functions.
** Lars T Hansen, happy-malloc co-conspirators, and Adam Boardman
**
**
** Status: code that predates this is actively used with Opera 6, but the
** particular code in this file has probably not been tested for a while.
** It should work, but expect to spend a little time getting it working.
*/

/* Hey, it's just bits in RAM. 

 - The code below this point patches the library jump vector for most of the memory
   management functions, allowing us to do some memory accounting without replacing
   the Symbian libraries.  The patch code is a recent brainstorm; the rest has been
   ripped out of happy-malloc.


 - To enable this code, you must:
      Uncomment "#define EPOC_EMULATOR_MALLOC_INSTRUMENTATION" at the top of the file
        modules\hardcore\mem\oom.cpp
      Uncomment "#define EPOC_EMULATOR_MALLOC_INSTRUMENTATION", below
      Add this file to your makefile or project
      Add kernel32.lib to the libraries list in the OPRMODEL project
      Add kernel32.lib with full path to the LIBS line in 
        Build\*\Epoc\OperaEng\group\OPRMODEL.SUP.MAKE  (make sure you get the
        right LIBS line, there are two)
      NOTE: You dont need to add this if you remake the project...
      Recompile oom.cpp, compile this file, and relink.


 - When you are running the emulator, you can trigger a dump of the current state
   of the memory by typing in the following incantation in the URL field:

		javascript:opera.oomMetaOperation(1)

   The dump will be written to the end of the file ALLOCSITES in the emulator's C
   drive.  The dump does not contain function names, just allocation site addresses.


 - To see the allocation site addresses, you do ALT+8 to view the disassembly window
   in Visual Studio, then CTRL+G to get up the goto box, and then type in the code
   addresses in this box; you will be taken to the allocation site, and can find the
   function name from there.


 - Another useful thing to do is to set a breakpoint in alloc_accounting_after(),
   on the obvious line at the end of the function, and add 'instrumented_mm_stats'
   to your Visual Studio watch window; then for every 1000 allocations you can
   see how the state of things changes.


 - The memory log will also be appended to ALLOCSITES when you quit Opera.

*/

#include "core/pch.h"

// General enable -- you must enable this
//#define EPOC_EMULATOR_MALLOC_INSTRUMENTATION

// Enables a particular way of counting, only available for hurricane refui 640,
// code written by Adam Boardman and not maintained by Opera, may or may not work
// Requires a special build of e32/euser: \\homes\epoc\hurricane\memory_hooks_euser.zip
//#define MALLOC_INSTRUMENTATION_EUSER_HACK

#ifdef EPOC_EMULATOR_MALLOC_INSTRUMENTATION

#include "modules/memtools/epoc-instrumentation.h"

// Turn on different types of logging output
//#define MALLOC_INSTRUMENTATION_INCLUDE_NOT_SORTED
#define MALLOC_INSTRUMENTATION_INCLUDE_CODELOC_SORTED

// Use mempointers when you want to know the exact leak, don't use 
// when testing for between pages memory consumption
//#define MALLOC_INSTRUMENTATION_INCLUDE_MEMPOINTERS

#ifdef MALLOC_INSTRUMENTATION_EUSER_HACK
//#define MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
//#define MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
//#define MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
//#define MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
//#define MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
#endif // MALLOC_INSTRUMENTATION_EUSER_HACK

static void patch_memory_management_functions();
static void install_memory_tracker();

void epoc_malloc_setup_instrumentation()
{
#ifndef MALLOC_INSTRUMENTATION_EUSER_HACK
	patch_memory_management_functions();
#else
	install_memory_tracker();
#endif //MALLOC_INSTRUMENTATION_EUSER_HACK
}

#ifdef MALLOC_INSTRUMENTATION_EUSER_HACK
class CAllocCounter : public CBase, public MAllocCountCallback
{
public:
    static CAllocCounter* NewL();
    virtual void CountAlloc(TAny* aCell, TInt aSize);
    virtual void CountFree(TAny* aCell);
    ~CAllocCounter();
		
private:
    CAllocCounter();
    void ConstructL();

private:
    TInt iAllocTotal;
    TInt iAllocCountAlloc;
    TInt iAllocCountFree;
};

CAllocCounter* alloc_counter;
#endif //MALLOC_INSTRUMENTATION_EUSER_HACK

/* We need some stuff that isn't emulated, so get it from Windows */
extern "C" {
    #define HANDLE unsigned
    BOOL   __stdcall VirtualProtect( void *lpAddress, long dwSize, long flNewProtect, 
									 long* lpflOldProtect );
    HANDLE __stdcall HeapCreate( long flOptions, long dwInitialSize, long dwMaximumSize );
    void * __stdcall HeapAlloc( HANDLE hHeap, long dwFlags, long dwBytes );
    BOOL   __stdcall HeapFree( HANDLE hHeap, long dwFlags,void * lpMem );
};

static const int PAGE_EXECUTE_READWRITE = 0x40;
static const int HEAP_ZERO_MEMORY = 0x08;

/* We need more memory than the emulator provides, so hook into 
   Windows MM functions.
   */
static HANDLE private_heap;

#define ALLOCATE(n)  HeapAlloc( private_heap, HEAP_ZERO_MEMORY, n )
#define FREE(p)      HeapFree( private_heap, 0, p )

/* Patching the library calls:

   The procedures we want to patch are implemented in a jump table with a
   six-byte indirect jump instruction:

	  FF 25 __ __ __ __     jmp dword [loc]

   where the location 'loc' contains the address of the procedure to jump
   to.  By patching the instruction and altering loc, we redirect the jump
   to our own code.  (I suppose we could also just have changed the table;
   patching the code works just fine for now.) 
   */

static TAny *instrumented_user_adjust( TAny *aCell, TInt anOffset, TInt aDelta );
static TAny *instrumented_user_adjustL( TAny *aCell, TInt anOffset, TInt aDelta );
static TAny *instrumented_user_alloc( TInt n );
static TAny *instrumented_user_allocL( TInt n );
static TAny *instrumented_user_allocLC( TInt n );
static void instrumented_user_free( TAny *p );
static void instrumented_user_freeZ( TAny*& p );
static TAny *instrumented_user_realloc( TAny *p, TInt size );
static TAny *instrumented_user_reallocL( TAny *p, TInt size);
static void *instrumented_malloc( size_t n );
static void *instrumented_realloc( void *p, size_t n );
static void* instrumented_calloc(size_t nobjects, size_t objsize);
static void instrumented_free( void *p );

// Jumps are rewritten to get the location from these addresses 
static unsigned instrumented_user_adjust_address = (unsigned)&instrumented_user_adjust;
static unsigned instrumented_user_adjustL_address = (unsigned)&instrumented_user_adjustL;
static unsigned instrumented_user_alloc_address = (unsigned)&instrumented_user_alloc;
static unsigned instrumented_user_allocL_address = (unsigned)&instrumented_user_allocL;
static unsigned instrumented_user_allocLC_address = (unsigned)&instrumented_user_allocLC;
static unsigned instrumented_user_free_address = (unsigned)&instrumented_user_free;
static unsigned instrumented_user_freeZ_address = (unsigned)&instrumented_user_freeZ;
static unsigned instrumented_user_realloc_address = (unsigned)&instrumented_user_realloc;
static unsigned instrumented_user_reallocL_address = (unsigned)&instrumented_user_reallocL;
static unsigned instrumented_malloc_address = (unsigned)&instrumented_malloc;
static unsigned instrumented_realloc_address = (unsigned)&instrumented_realloc;
static unsigned instrumented_calloc_address = (unsigned)&instrumented_calloc;
static unsigned instrumented_free_address = (unsigned)&instrumented_free;

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
const unsigned internal_user_free = 0x60001FD2; // only valid for hurricane refui 640
#endif
#if defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC)
const unsigned internal_user_alloc = 0x6000109B; // only valid for hurricane refui 640
const unsigned internal_user_allocL = 0x6000103C; // only valid for hurricane refui 640
const unsigned internal_user_allocLC = 0x60002513; // only valid for hurricane refui 640
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
const unsigned internal_user_realloc = 0x60002CA2; // only valid for hurricane refui 640
const unsigned internal_user_reallocL = 0x6000125D; // only valid for hurricane refui 640
#endif

// These are the addresses that were originally in the code 
static unsigned actual_user_adjust_address = 0;
static unsigned actual_user_adjustL_address = 0;
static unsigned actual_user_alloc_address = 0;
static unsigned actual_user_allocL_address = 0;
static unsigned actual_user_allocLC_address = 0;
static unsigned actual_user_free_address = 0;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
static unsigned actual_internal_user_free_address = 0;
#endif
#if defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC)
static unsigned actual_internal_user_alloc_address = 0;
static unsigned actual_internal_user_allocL_address = 0;
static unsigned actual_internal_user_allocLC_address = 0;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
static unsigned actual_internal_user_realloc_address = 0;
static unsigned actual_internal_user_reallocL_address = 0;
#endif
static unsigned actual_user_freeZ_address = 0;
static unsigned actual_user_realloc_address = 0;
static unsigned actual_user_reallocL_address = 0;
static unsigned actual_malloc_address = 0;
static unsigned actual_realloc_address = 0;
static unsigned actual_calloc_address = 0;
static unsigned actual_free_address = 0;

/* Doing the accounting: code to grab the return address from the
   stack frame.  Assuming standard (__cdecl) calling conventions
   with a base pointer.
   */
/* Our caller */
#define GET_CALLER( Site ) \
    do{ Site.tag = 1;
        _asm mov eax, [ebp+4] \
        _asm mov site.location, eax \
    } while (0)

/* Our caller's caller */
#define GET_CALLER2( Site ) \
    do{ Site.tag = 1; \
        _asm mov eax, [ebp] \
	    _asm mov eax, [eax+4] \
        _asm mov site.location, eax \
    } while (0)

#  define BACKTRACE_LEN 10		// 5 is not sufficient due to depth of constructor nesting

struct site_t 
{
    char *location;
    char *memadd;
#if !defined _NO_GLOBALS_
    unsigned backtrace[BACKTRACE_LEN];
#endif
    int tag;
};

typedef __int64 int64;

typedef struct 
{
    site_t    site;
    int64     allocated;			/* Bytes allocated */
    int64     freed;				/* Bytes freed */
    int64     nallocated;			/* Number allocated */
    int64     nfreed;				/* Number freed */
    int64     peak;				    /* Peak live */
    int64     npeak;				/* Number live at peak */
} site_stats_t;

static void alloc_accounting_after( size_t n, void *p, site_t site );
static void out_of_memory_event( size_t n, void *p, site_t site );
static void free_accounting_before( void *p );
static site_stats_t *lookup_allocation_site( site_t site );
static void add_object( site_stats_t *ss, void *p, size_t n );
static void remove_object( void *p, site_stats_t **ss, size_t *n );
static void do_summarize_allocation_sites( FILE *output, int nsites );

static void patch_memory_management_functions()
{
	long oldprot;
#ifdef MALLOC_INSTRUMENTATION_EUSER_HACK
	_asm int 3; // shouldnt use in conjuction with each other
#endif
    if (VirtualProtect( &(User::Adjust), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &(User::AdjustL), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &(User::Alloc), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &(User::AllocL), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &(User::AllocLC), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &(User::Free), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
        VirtualProtect( (void*)internal_user_free, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 && //User:Free - internal
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC || MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL || MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
        VirtualProtect( (void*)internal_user_alloc, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 && //User:Free - internal
        VirtualProtect( (void*)internal_user_allocL, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 && //User:Free - internal
        VirtualProtect( (void*)internal_user_allocLC, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 && //User:Free - internal
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
        VirtualProtect( (void*)internal_user_realloc, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 && //User:Free - internal
        VirtualProtect( (void*)internal_user_reallocL, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 && //User:Free - internal
#endif
        VirtualProtect( &(User::FreeZ), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &(User::ReAlloc), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &(User::ReAllocL), 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &malloc, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &calloc, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &realloc, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0 &&
        VirtualProtect( &free, 100, PAGE_EXECUTE_READWRITE, &oldprot ) != 0)
	{
		actual_user_adjust_address = *(unsigned*)((char*)&(User::Adjust)+2);
		*(unsigned*)((char*)&(User::Adjust)+2) = (unsigned)&instrumented_user_adjust_address;

		actual_user_adjustL_address = *(unsigned*)((char*)&(User::AdjustL)+2);
		*(unsigned*)((char*)&(User::AdjustL)+2) = (unsigned)&instrumented_user_adjustL_address;

		actual_user_alloc_address = *(unsigned*)((char*)&(User::Alloc)+2);
		*(unsigned*)((char*)&(User::Alloc)+2) = (unsigned)&instrumented_user_alloc_address;

		actual_user_allocL_address = *(unsigned*)((char*)&(User::AllocL)+2);
		*(unsigned*)((char*)&(User::AllocL)+2) = (unsigned)&instrumented_user_allocL_address;

		actual_user_allocLC_address = *(unsigned*)((char*)&(User::AllocLC)+2);
		*(unsigned*)((char*)&(User::AllocLC)+2) = (unsigned)&instrumented_user_allocLC_address;

		actual_user_free_address = *(unsigned*)((char*)&(User::Free)+2);
		*(unsigned*)((char*)&(User::Free)+2) = (unsigned)&instrumented_user_free_address;

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
		actual_internal_user_free_address = *(unsigned*)((char*)internal_user_free+1);
		unsigned tmp = (unsigned)instrumented_user_free_address-(internal_user_free+5);
		*(unsigned*)((char*)internal_user_free+1) = tmp;
#endif

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
		actual_internal_user_alloc_address = *(unsigned*)((char*)internal_user_alloc+1);
		*(unsigned*)((char*)internal_user_alloc+1) = (unsigned)instrumented_user_alloc_address-(internal_user_alloc+5);
#endif

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
		actual_internal_user_allocL_address = *(unsigned*)((char*)internal_user_allocL+1);
		unsigned tmp1 = (unsigned)instrumented_user_allocL_address-(internal_user_allocL+5);
		*(unsigned*)((char*)internal_user_allocL+1) = tmp1;
#endif

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
		actual_internal_user_allocLC_address = *(unsigned*)((char*)internal_user_allocLC+1);
		*(unsigned*)((char*)internal_user_allocLC+1) = (unsigned)instrumented_user_allocLC_address-(internal_user_allocLC+5);
#endif

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
		actual_internal_user_realloc_address = *(unsigned*)((char*)internal_user_realloc+1);
		*(unsigned*)((char*)internal_user_realloc+1) = (unsigned)instrumented_user_realloc_address-(internal_user_realloc+5);

		actual_internal_user_reallocL_address = *(unsigned*)((char*)internal_user_reallocL+1);
		*(unsigned*)((char*)internal_user_reallocL+1) = (unsigned)instrumented_user_reallocL_address-(internal_user_reallocL+5);
#endif

		actual_user_freeZ_address = *(unsigned*)((char*)&(User::FreeZ)+2);
		*(unsigned*)((char*)&(User::FreeZ)+2) = (unsigned)&instrumented_user_freeZ_address;

		actual_user_realloc_address = *(unsigned*)((char*)&(User::ReAlloc)+2);
		*(unsigned*)((char*)&(User::ReAlloc)+2) = (unsigned)&instrumented_user_realloc_address;

		actual_user_reallocL_address = *(unsigned*)((char*)&(User::ReAllocL)+2);
		*(unsigned*)((char*)&(User::ReAllocL)+2) = (unsigned)&instrumented_user_reallocL_address;

		actual_malloc_address = *(unsigned*)((char*)&malloc+2);
		*(unsigned*)((char*)&malloc+2) = (unsigned)&instrumented_malloc_address;

		actual_realloc_address = *(unsigned*)((char*)&realloc+2);
		*(unsigned*)((char*)&realloc+2) = (unsigned)&instrumented_realloc_address;

		actual_calloc_address = *(unsigned*)((char*)&calloc+2);
		*(unsigned*)((char*)&calloc+2) = (unsigned)&instrumented_calloc_address;

		actual_free_address = *(unsigned*)((char*)&free+2);
		*(unsigned*)((char*)&free+2) = (unsigned)&instrumented_free_address;

		private_heap = HeapCreate( 0, 40*1024*1024, 0);

		//atexit( dump_allocation_sites ); //AB - Causes a leak in clocalfilesystem
	}
}

static struct
{
	int		n_alloc;	// Number of objects allocated
	int		n_free;		// Number of objects freed
	int		n_foreign;	// Number of objects not found in object table
	int     b_alloc;	// Number of bytes allocated
	int     b_free;     // Number of bytes free
	int     b_live;     // Number of bytes live now
} instrumented_mm_stats;

static TAny *instrumented_user_adjust( TAny *aCell, TInt anOffset, TInt aDelta )
{
	void *p;
	site_t site;
	int oldsize;

	GET_CALLER( site );

	*(unsigned*)((char*)&(User::Adjust)+2) = actual_user_adjust_address;
	free_accounting_before( aCell );
	oldsize = aCell ? User::AllocLen( aCell ) : 0;
	p = User::Adjust( aCell, anOffset, aDelta );
	if (p == NULL)
		alloc_accounting_after( oldsize+aDelta, p, site );
	else
		alloc_accounting_after( oldsize, aCell, site );
	*(unsigned*)((char*)&(User::Adjust)+2) = (unsigned)&instrumented_user_adjust_address;
	return p;
}

static TAny *instrumented_user_adjustL( TAny *aCell, TInt anOffset, TInt aDelta )
{
	void *p;
	site_t site;
	int oldsize;
	int r;

	GET_CALLER( site );

	*(unsigned*)((char*)&(User::Adjust)+2) = actual_user_adjust_address;
	free_accounting_before( aCell );
	oldsize = aCell ? User::AllocLen( aCell ) : 0;
	p = NULL;
	TRAP( r, (p = User::Adjust( aCell, anOffset, aDelta )) );
	if (p == NULL)
		alloc_accounting_after( oldsize+aDelta, p, site );
	else
		alloc_accounting_after( oldsize, aCell, site );
	*(unsigned*)((char*)&(User::Adjust)+2) = (unsigned)&instrumented_user_adjust_address;
	if (r < 0)
		LEAVE(r);
	return p;
}

static TAny *instrumented_user_alloc( TInt n )
{
	void *p;
	site_t site;

	GET_CALLER2( site );	// User::Alloc() is rarely called directly

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
	*(unsigned*)((char*)internal_user_alloc+1) = actual_internal_user_alloc_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
	*(unsigned*)((char*)internal_user_allocL+1) = actual_internal_user_allocL_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)internal_user_allocLC+1) = actual_internal_user_allocLC_address;
#endif
#if defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC)
	*(unsigned*)((char*)&(User::AllocL)+2) = actual_user_allocL_address;
	*(unsigned*)((char*)&(User::AllocLC)+2) = actual_user_allocLC_address;
#endif
	*(unsigned*)((char*)&(User::Alloc)+2) = actual_user_alloc_address;
	p = User::Alloc( n );
	alloc_accounting_after( n, p, site );
	*(unsigned*)((char*)&(User::Alloc)+2) = (unsigned)&instrumented_user_alloc_address;
#if defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC)
	*(unsigned*)((char*)&(User::AllocLC)+2) = (unsigned)&instrumented_user_allocLC_address;
	*(unsigned*)((char*)&(User::AllocL)+2) = (unsigned)&instrumented_user_allocL_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
	*(unsigned*)((char*)internal_user_alloc+1) = (unsigned)instrumented_user_alloc_address-(internal_user_alloc+5);
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
	*(unsigned*)((char*)internal_user_allocL+1) = (unsigned)instrumented_user_allocL_address-(internal_user_allocL+5);
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)internal_user_allocLC+1) = (unsigned)instrumented_user_allocLC_address-(internal_user_allocLC+5);
#endif
	return p;
}

static TAny *instrumented_user_allocL( TInt n )
{
	int r;
	void *p;
	site_t site;

	GET_CALLER2( site );	// User::AllocL() is rarely called directly

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
	*(unsigned*)((char*)internal_user_alloc+1) = actual_internal_user_alloc_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
	*(unsigned*)((char*)internal_user_allocL+1) = actual_internal_user_allocL_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)internal_user_allocLC+1) = actual_internal_user_allocLC_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC || MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL || MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)&(User::AllocL)+2) = actual_user_allocL_address;
	*(unsigned*)((char*)&(User::AllocLC)+2) = actual_user_allocLC_address;
#endif
	*(unsigned*)((char*)&(User::AllocL)+2) = actual_user_allocL_address;
	p = NULL;
	TRAP( r, (p = User::AllocL( n )) );
	alloc_accounting_after( n, p, site );
	*(unsigned*)((char*)&(User::AllocL)+2) = (unsigned)&instrumented_user_allocL_address;
#if defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC)
	*(unsigned*)((char*)&(User::AllocLC)+2) = (unsigned)&instrumented_user_allocLC_address;
	*(unsigned*)((char*)&(User::AllocL)+2) = (unsigned)&instrumented_user_allocL_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
	*(unsigned*)((char*)internal_user_alloc+1) = (unsigned)instrumented_user_alloc_address-(internal_user_alloc+5);
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
	*(unsigned*)((char*)internal_user_allocL+1) = (unsigned)instrumented_user_allocL_address-(internal_user_allocL+5);
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)internal_user_allocLC+1) = (unsigned)instrumented_user_allocLC_address-(internal_user_allocLC+5);
#endif
	if (r < 0)
		LEAVE(r);
	return p;
}

static TAny *instrumented_user_allocLC( TInt n )
{
	int r;
	void *p;
	site_t site;

	GET_CALLER( site );

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
	*(unsigned*)((char*)internal_user_alloc+1) = actual_internal_user_alloc_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
	*(unsigned*)((char*)internal_user_allocL+1) = actual_internal_user_allocL_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)internal_user_allocLC+1) = actual_internal_user_allocLC_address;
#endif
#if defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC)
	*(unsigned*)((char*)&(User::AllocL)+2) = actual_user_allocL_address;
	*(unsigned*)((char*)&(User::AllocLC)+2) = actual_user_allocLC_address;
#endif
	*(unsigned*)((char*)&(User::AllocLC)+2) = actual_user_allocLC_address;
	p = NULL;
	TRAP( r, p = User::AllocLC( n ); CleanupStack::Pop(); );
	alloc_accounting_after( n, p, site );
	*(unsigned*)((char*)&(User::AllocLC)+2) = (unsigned)&instrumented_user_allocLC_address;
#if defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL) || defined(MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC)
	*(unsigned*)((char*)&(User::AllocLC)+2) = (unsigned)&instrumented_user_allocLC_address;
	*(unsigned*)((char*)&(User::AllocL)+2) = (unsigned)&instrumented_user_allocL_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
	*(unsigned*)((char*)internal_user_alloc+1) = (unsigned)instrumented_user_alloc_address-(internal_user_alloc+5);
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
	*(unsigned*)((char*)internal_user_allocL+1) = (unsigned)instrumented_user_allocL_address-(internal_user_allocL+5);
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)internal_user_allocLC+1) = (unsigned)instrumented_user_allocLC_address-(internal_user_allocLC+5);
#endif
	CleanupStack::PushL(p);
	if (r < 0)
		LEAVE(r);
	return p;
}

static void instrumented_user_free( TAny *p )
{
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
	*(unsigned*)((char*)internal_user_free+1) = actual_internal_user_free_address;
#endif
	*(unsigned*)((char*)&(User::Free)+2) = actual_user_free_address;
	*(unsigned*)((char*)&free+2) = actual_free_address;
	free_accounting_before( p );
	User::Free( p );
	*(unsigned*)((char*)&free+2) = (unsigned)&instrumented_free_address;
	*(unsigned*)((char*)&(User::Free)+2) = (unsigned)&instrumented_user_free_address;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
	*(unsigned*)((char*)internal_user_free+1) = (unsigned)instrumented_user_free_address-(internal_user_free+5);
#endif
}

static void instrumented_user_freeZ( TAny* &p )
{
	*(unsigned*)((char*)&(User::FreeZ)+2) = actual_user_freeZ_address;
	free_accounting_before( p );
	User::FreeZ( p );
	// Note: p is now dead
	*(unsigned*)((char*)&(User::FreeZ)+2) = (unsigned)&instrumented_user_freeZ_address;
}

static TAny *instrumented_user_realloc( TAny *p, TInt n )
{
	void *newp;
	site_t site;
	int oldsize;

	GET_CALLER( site );

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
	*(unsigned*)((char*)internal_user_realloc+1) = actual_internal_user_realloc_address;
	*(unsigned*)((char*)internal_user_reallocL+1) = actual_internal_user_reallocL_address;
#endif
	*(unsigned*)((char*)&(User::ReAlloc)+2) = actual_user_realloc_address;
	free_accounting_before( p );
	oldsize = p ? User::AllocLen( p ) : 0;
	newp = User::ReAlloc( p, n );
	if (newp == NULL)
		alloc_accounting_after( oldsize, p, site );
	else
		alloc_accounting_after( n, newp, site );
	*(unsigned*)((char*)&(User::ReAlloc)+2) = (unsigned)&instrumented_user_realloc_address;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
	*(unsigned*)((char*)internal_user_realloc+1) = (unsigned)instrumented_user_realloc_address-(internal_user_realloc+5);
	*(unsigned*)((char*)internal_user_reallocL+1) = (unsigned)instrumented_user_reallocL_address-(internal_user_reallocL+5);
#endif
	return newp;
}

static TAny *instrumented_user_reallocL( TAny *p, TInt n )
{
	void *newp;
	site_t site;
	int r;
	int oldsize;

	GET_CALLER( site );

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
	*(unsigned*)((char*)internal_user_realloc+1) = actual_internal_user_realloc_address;
	*(unsigned*)((char*)internal_user_reallocL+1) = actual_internal_user_reallocL_address;
#endif
	*(unsigned*)((char*)&(User::ReAllocL)+2) = actual_user_reallocL_address;
	free_accounting_before( p );
	oldsize = p ? User::AllocLen( p ) : 0;
	newp = NULL;
	TRAP( r, (newp = User::ReAllocL( p, n )));
	if (newp == NULL)
		alloc_accounting_after( oldsize, p, site );
	else
		alloc_accounting_after( n, newp, site );
	*(unsigned*)((char*)&(User::ReAllocL)+2) = (unsigned)&instrumented_user_reallocL_address;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
	*(unsigned*)((char*)internal_user_realloc+1) = (unsigned)instrumented_user_realloc_address-(internal_user_realloc+5);
	*(unsigned*)((char*)internal_user_reallocL+1) = (unsigned)instrumented_user_reallocL_address-(internal_user_reallocL+5);
#endif
	if (r < 0)
		LEAVE( r );
	return newp;
}

static void *instrumented_malloc( size_t n )
{
	void *p;
	site_t site;

	GET_CALLER( site );

	*(unsigned*)((char*)&malloc+2) = actual_malloc_address;
	p = malloc( n );
	alloc_accounting_after( n, p, site );
	*(unsigned*)((char*)&malloc+2) = (unsigned)&instrumented_malloc_address;
	return p;
}

static void *instrumented_realloc( void *p, size_t n )
{
	void *newp;
	site_t site;
	int oldsize;

	GET_CALLER( site );

	*(unsigned*)((char*)&realloc+2) = actual_realloc_address;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
	*(unsigned*)((char*)internal_user_realloc+1) = actual_internal_user_realloc_address;
	*(unsigned*)((char*)internal_user_reallocL+1) = actual_internal_user_reallocL_address;
#endif
	free_accounting_before( p );
	oldsize = p ? User::AllocLen(p) : 0;
	newp = realloc( p, n );
	if (newp == NULL)
		alloc_accounting_after( oldsize, p, site );
	else
		alloc_accounting_after( n, newp, site );
	*(unsigned*)((char*)&realloc+2) = (unsigned)&instrumented_realloc_address;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
	*(unsigned*)((char*)internal_user_realloc+1) = (unsigned)instrumented_user_realloc_address-(internal_user_realloc+5);
	*(unsigned*)((char*)internal_user_reallocL+1) = (unsigned)instrumented_user_reallocL_address-(internal_user_reallocL+5);
#endif
	return newp;
}

static void* instrumented_calloc(size_t nobjects, size_t objsize)
{
	void *p;
	site_t site;

	GET_CALLER( site );

	*(unsigned*)((char*)&calloc+2) = actual_calloc_address;
	p = calloc( nobjects, objsize );
	alloc_accounting_after( nobjects*objsize, p, site );
	*(unsigned*)((char*)&calloc+2) = (unsigned)&instrumented_calloc_address;
	return p;
}

static void instrumented_free( void *p )
{
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
	*(unsigned*)((char*)internal_user_free+1) = actual_internal_user_free_address;
#endif
	*(unsigned*)((char*)&(User::Free)+2) = actual_user_free_address;
	*(unsigned*)((char*)&free+2) = actual_free_address;
	free_accounting_before( p );
	free( p );
	*(unsigned*)((char*)&free+2) = (unsigned)&instrumented_free_address;
	*(unsigned*)((char*)&(User::Free)+2) = (unsigned)&instrumented_user_free_address;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
	*(unsigned*)((char*)internal_user_free+1) = (unsigned)instrumented_user_free_address-(internal_user_free+5);
#endif
}

#ifdef MALLOC_INSTRUMENTATION_EUSER_HACK
CAllocCounter::CAllocCounter()
	{
	iAllocTotal = 0;
	iAllocCountAlloc = 0;
	iAllocCountFree = 0;
	User::SetAllocCountCallback(this);
	}

CAllocCounter::~CAllocCounter()
	{
	User::SetAllocCountCallback(NULL);
	}

/**
** Static constructer for a new line breaker
*/
CAllocCounter* CAllocCounter::NewL()
	{
	CAllocCounter* self = new (ELeave) CAllocCounter();
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(); //self
	return self;
	}

/**
** Constructs a new character converter for the given charset
*/
void CAllocCounter::ConstructL()
	{
	}

void CAllocCounter::CountAlloc(TAny* aCell, TInt aSize)
	{
	iAllocTotal += aSize;
	iAllocCountAlloc++;
	site_t site;
	GET_CALLER( site );
	alloc_accounting_after( aSize, aCell, site );
	}

void CAllocCounter::CountFree(TAny* aCell)
	{
	free_accounting_before( aCell );
	iAllocCountFree++;
	}
#endif

static void out_of_memory_event( size_t n, void *p, site_t site )
{
	// Just a hook
}

static void alloc_accounting_after( size_t n, void *p, site_t site )
{
	if (p == NULL) 
	{
		out_of_memory_event( n, p, site );
		return;
	}

	instrumented_mm_stats.n_alloc++;
	instrumented_mm_stats.b_alloc += n;
	instrumented_mm_stats.b_live += n;

	unsigned *fp;
	_asm mov fp, ebp;
	for ( int i=0 ; i < BACKTRACE_LEN ; i++ )
	{
		if (fp) 
		{
			site.backtrace[i] = fp[1];
			fp = (unsigned*)fp[0];
		}
	}
	site.memadd = (char*)p;

	site_stats_t *ss = lookup_allocation_site(site);

	ss->allocated += n;
	ss->nallocated++;
	ss->npeak = MAX( ss->npeak, (ss->nallocated - ss->nfreed));
	ss->peak = MAX( ss->peak, (ss->allocated - ss->freed));
	add_object( ss, p, n );

	if (instrumented_mm_stats.n_alloc % 1000 == 0)
	{
		int stop_here = 0;
	}
}

/* static size table to make things easy, attempt to compact if it overflows or assert */

#define NSITES 10000

static site_stats_t *alloc_pool = 0;
static site_stats_t **alloc_sites = 0;
static site_stats_t **alloc_sites_copy = 0;
static int sites_in_use = 0;
static int sites_pool_max = 0;
// Must store object size and site_stats reference externally, so we have a
// map from object pointer to those data.  This map can be large and can change
// very rapidly, so a hash table is most appropriate.
//
// Hash table code stolen from plugins memory management and barely modified.

#define NODES_PER_BLOCK		2000

struct OOMMemNode {
	void*			datum;
	size_t			n;
	site_stats_t	*ss;
	OOMMemNode*		next;
};

class OOMMemBlock {
public:
	OOMMemBlock() : next_block(0), next_node(0) {}
	OOMMemBlock*	next_block;
	int				next_node;
	OOMMemNode		nodes[NODES_PER_BLOCK];
};

class OOMMemoryHandler
{
public:
	OOMMemNode**	table;
	int				size;
private:
	OOMMemBlock*	blocks;
	OOMMemNode*		free_nodes;
	int				population;
	int				Hash( void *p )
					{
						return (int)((long)p % size); 
					}
	void			Insert( OOMMemNode* node ) 
					{
						int h = Hash(node->datum);
						node->next = table[h];
						table[h] = node;
					}
	BOOL			Grow();
	OOMMemNode*		NewNode();
	void			DisposeNode( OOMMemNode* );

public:
	OOMMemoryHandler()
		: blocks(0),
		  table(0),
		  free_nodes(0),
		  size(0),
		  population(0) {}
    ~OOMMemoryHandler();

    void			Malloc(void *p, site_stats_t *ss, size_t n);
    void			Free(void *p, site_stats_t **ss, size_t *n);
};

static OOMMemoryHandler *object_manager = new OOMMemoryHandler();

enum TLogPrintTypes 
	{
	ELogPrintNotDef,
	ELogPrintAlloc,
	ELogPrintFree
	};

static void printlog(site_stats_t *ss=NULL, OOMMemNode *node=NULL,int type=ELogPrintNotDef)
{
	FILE *fp;
	fp = fopen( "ALLOCLOG", "a" );
	if (!fp) return;
	switch (type)
	{
	case ELogPrintNotDef:
		fprintf( fp, "\n" );
		break;
	case ELogPrintAlloc:
		fprintf( fp, "Alloc\n" );
		break;
	case ELogPrintFree:
		fprintf( fp, "Free\n" );
		break;
	}
	if (ss)
		fprintf( fp, "SS L:0x%08lx M:0x%08lx\n", (long)ss->site.location,(long)ss->site.memadd );
	if (node)
	{
		fprintf( fp, "Node D: 0x%08lx N:0x%08lx\n", (long)node->datum, (long)node->n );
		if (node->ss)
			fprintf( fp, "SS L:0x%08lx M:0x%08lx\n", (long)node->ss->site.location,(long)node->ss->site.memadd );
	}
	fclose( fp );
}

static void free_accounting_before( void *p )
{
	if (p == 0) return;

	site_stats_t *ss;
	size_t n;
	
	remove_object( p, &ss, &n );
	if (n == 0)
	{
		instrumented_mm_stats.n_foreign++;
/*
		//search entire table to see if its hash problem?
		for (int j=0 ; j < object_manager->size ; j++ ) 
		{
			OOMMemNode *prev, *next;
			for ( prev=object_manager->table[j] ; prev ; prev=next ) 
			{
				next = prev->next;
				if (prev->datum == p)
				{
					int break_here=0;
					printlog(ss, prev);
				}
				if (prev->n >= 4)
				{
					if ((prev->datum >= (void*)((int)p-4)) && (prev->datum <= (void*)((int)p+4)))
					{
						int breek=1; // close pointer...
						printlog(ss, prev);
					}
				}
			}
		}
		for (int i=0 ; i < sites_in_use; i++)
		{
			if ((alloc_sites[i]->nallocated - alloc_sites[i]->nfreed) != 0)
				if (alloc_sites[i]->site.memadd == p)
				{
					int breakhere=0;
					printlog(ss);
				}
		}
*/
	}
	else
	{
		instrumented_mm_stats.n_free++;
		instrumented_mm_stats.b_free += n;
		instrumented_mm_stats.b_live -= n;
		ss->freed += n;
		ss->nfreed++;
		long b_inuse = (long)(ss->allocated - ss->freed);
		if ((b_inuse <= 0) && (ss->site.tag != 0))
		{
			sites_in_use--;
			ss->site.tag = 0;
		}
	}
}

static int compare_size_stats( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_MEMPOINTERS
	bool canretzero = TRUE;
	if ((*pa)->site.location != (*pb)->site.location) canretzero = FALSE;
	//AB - ignore memory address here if necessasary
	if ((*pa)->site.memadd != (*pb)->site.memadd) canretzero = FALSE;
	int ret = ((*pa)->site.location - (*pb)->site.location) + ((*pa)->site.memadd - (*pb)->site.memadd);
#ifndef _NO_GLOBALS_
	if (ret == 0)
		for (int j=0; j<BACKTRACE_LEN; j++)
		{
			ret += ((*pa)->site.backtrace[j] - (*pb)->site.backtrace[j]);
			if ((*pa)->site.backtrace[j] != (*pb)->site.backtrace[j]) canretzero = FALSE;
		}
#endif
	if ((ret == 0) && !canretzero)
		return 1;
	else
		return ret;
#else
	return (*pa)->site.location - (*pb)->site.location;
#endif
}

static int compare_peak_stats_a( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;
	if (!*pb || !*pa)
		return -1;
	
	int64 diff = (*pb)->peak - (*pa)->peak;
	if (diff != 0)
		return (int)diff;
	else
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_MEMPOINTERS
		return ((*pa)->site.location - (*pb)->site.location) + ((*pa)->site.memadd - (*pb)->site.memadd);
#else
		return (*pb)->site.location - (*pa)->site.location;
#endif
}

static int compare_size_stats_a( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;
	if (!*pb || !*pa)
		return -1;
	int64 diff = ((*pb)->allocated - (*pb)->freed) - ((*pa)->allocated - (*pa)->freed);
	if (diff != 0)
		return (int)diff;
	else
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_MEMPOINTERS
		return ((*pa)->site.location - (*pb)->site.location) + ((*pa)->site.memadd - (*pb)->site.memadd);
#else
		return (*pb)->site.location - (*pa)->site.location;
#endif
}

static int compare_code_stats_a( const void *a, const void *b )
{
	const site_stats_t * const *pa = (const site_stats_t* const *)a;
	const site_stats_t * const *pb = (const site_stats_t* const *)b;
	if (!*pb || !*pa)
		return -1;
	int64 diff = ((*pb)->site.location - (*pa)->site.location);
	if (diff != 0)
		return (int)diff;
	else
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_MEMPOINTERS
		return (((*pa)->site.location - (*pb)->site.location)) + (((*pa)->site.memadd - (*pb)->site.memadd));
#else
		return (*pb)->site.location - (*pa)->site.location;
#endif
}

static void compact_allocation_sites()
{
	//dump_allocation_sites();
	memcpy( alloc_sites_copy, alloc_sites, sizeof(site_stats_t*)*sites_in_use );
	qsort( alloc_sites_copy, sites_in_use, sizeof(site_stats_t*), compare_size_stats_a );
	int nsites = sites_in_use;
	sites_in_use = 0;
	for (int i=0 ; i < nsites ; i++ )
		if (alloc_sites_copy[i]->site.tag != 0)
		{
			long b_inuse = (long)(alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed);
			if (b_inuse > 0)
				sites_in_use++;
			else
				alloc_sites_copy[i]->site.tag = 0;
		}
		else
			_asm int 3;
	memcpy( alloc_sites, alloc_sites_copy, sizeof(site_stats_t*)*sites_in_use );
}

#ifdef MALLOC_INSTRUMENTATION_EUSER_HACK

static void install_memory_tracker()
	{
	private_heap = HeapCreate( 0, 40*1024*1024, 0);// 40*1024*1024 );
	if (alloc_pool == NULL)
		{
		alloc_pool = (site_stats_t*)ALLOCATE( sizeof(site_stats_t)*NSITES );
		alloc_sites = (site_stats_t**)ALLOCATE( sizeof(site_stats_t*)*NSITES );
		alloc_sites_copy = (site_stats_t**)ALLOCATE( sizeof(site_stats_t*)*NSITES );
		assert( alloc_pool != 0 && alloc_sites != 0 && alloc_sites_copy != 0 );
		}
	alloc_counter = CAllocCounter::NewL();
	}

#endif //MALLOC_INSTRUMENTATION_EUSER_HACK

static site_stats_t *lookup_allocation_site( site_t site )
{
	if (alloc_pool == NULL)
	{
		alloc_pool = (site_stats_t*)ALLOCATE( sizeof(site_stats_t)*NSITES );
		alloc_sites = (site_stats_t**)ALLOCATE( sizeof(site_stats_t*)*NSITES );
		alloc_sites_copy = (site_stats_t**)ALLOCATE( sizeof(site_stats_t*)*NSITES );
		assert( alloc_pool != 0 && alloc_sites != 0 && alloc_sites_copy != 0 );
	}

	site_stats_t ss;
	site_stats_t *ass = &ss;
	void *found;
	int i;
	
	memset( &ss, 0, sizeof(ss) );
	ss.site = site;

	// something screwed with the binary search???
	//if ((found = bsearch( &ass, alloc_sites, sites_in_use, sizeof(site_stats_t*), compare_size_stats )) != 0) 
	//	return *(site_stats_t**)found;
	//else
	//{
	for ( i=0 ; i < sites_in_use; i++)
	{
		if (!alloc_sites[i])
			compact_allocation_sites();
		if (compare_size_stats( &alloc_sites[i], &ass) == 0)
			return alloc_sites[i];
	}
	//}

	if (sites_in_use >= NSITES-1)
		compact_allocation_sites();	
	
	for ( i=0 ; (i < sites_in_use) && (compare_size_stats( &alloc_sites[i], &ass ) != 0); i++ )
		;
	memmove( &alloc_sites[i+1], &alloc_sites[i], sizeof(site_stats_t*)*(sites_in_use-i) );
	int poolelement = 0;
	for (int j=0; j < sites_pool_max; j++)
	{
		if (alloc_pool[j].site.tag == 0)
			poolelement = j;
	}
	if (poolelement)
	{
		alloc_sites[i] = &alloc_pool[poolelement];
		alloc_pool[poolelement] = ss;
	}
	else
	{
		alloc_sites[i] = &alloc_pool[sites_pool_max];
		alloc_pool[sites_pool_max] = ss;
		sites_pool_max++;
		if ( sites_pool_max >= NSITES)
			_asm int 3;
	}
	sites_in_use++;
	return alloc_sites[i];
}


OOMMemoryHandler::~OOMMemoryHandler()
{
	OOMMemBlock *b, *next;

	for ( b=blocks ; b ; b=next ) {
		next = b->next_block;
		FREE(b);
	}
	FREE(table);
}

OOMMemNode* OOMMemoryHandler::NewNode()
{
	OOMMemNode *node;

	if (free_nodes != 0) {
		node = free_nodes;
		free_nodes = node->next;
	}
	else {
		if (blocks == 0 || blocks->next_node == NODES_PER_BLOCK) {
			OOMMemBlock *b = (OOMMemBlock*)ALLOCATE(sizeof(OOMMemBlock));
			if (b == 0)	return 0;
			b->next_block = blocks;
			blocks = b;
		}
		node = &blocks->nodes[ blocks->next_node++ ];
	}

	node->next = 0;
	node->datum = 0;
	return node;
}

void OOMMemoryHandler::DisposeNode( OOMMemNode *node )
{
	node->next = free_nodes;
	free_nodes = node;
}

BOOL OOMMemoryHandler::Grow()
{
	int size, i, old_size;
	OOMMemNode **t, **old_table;

	if (table != 0)
		size = (int)(this->size*1.20);
	else
		size = (int)(NODES_PER_BLOCK/0.75);

	t = (OOMMemNode**)ALLOCATE( sizeof(OOMMemNode*)*size );
	if (t == 0) return FALSE;

	for ( i=0 ; i < size ; i++ )
		t[i] = 0;
	old_table = table;
	old_size = this->size;
	table = t;
	this->size = size;

	if (old_table != 0) {
		for ( i=0 ; i < old_size ; i++ ) {
			OOMMemNode *p, *next;
			for ( p=old_table[i] ; p ; p=next ) {
				next = p->next;
				Insert( p );
			}
		}
		FREE(old_table);
	}
	return TRUE;
}

void OOMMemoryHandler::Malloc(void *p, site_stats_t *ss, size_t n)
{
	OOMMemNode *node;

	node = NewNode();
	if (node == 0)
		return;
/*
	OOMMemNode *snode, *sprev;
	if (size)
	{
		int h = Hash(p);
		for ( sprev=0, snode=table[h]; snode != 0 && snode->datum != p ; sprev=snode, snode=snode->next )
			;
		if (snode != 0)
		{
			for (int i=0 ; i < sites_in_use; i++)
			{
				if ((alloc_sites[i]->nallocated - alloc_sites[i]->nfreed) != 0)
					if (alloc_sites[i]->site.memadd == p)
					{
						printlog(alloc_sites[i]);
						//ok so then someone deleted our stuff! might as well unrefit
						//dont know the size though...
						if (alloc_sites[i]->nallocated > alloc_sites[i]->nfreed)
						{
							alloc_sites[i]->nfreed++;
							if (alloc_sites[i]->nallocated == alloc_sites[i]->nfreed)
								alloc_sites[i]->freed = alloc_sites[i]->allocated;
							printlog(alloc_sites[i]);
							int breakhere=0;
						}
					}
			}
			int break_here = 1; //memory not previously unreferenced...
		}
	}
*/
	node->datum = p;
	node->n = n;
	node->ss = ss;

	if (++population >= size * 0.75)
		if (!Grow()) 
		{
			DisposeNode( node );
			return;
		}

	//printlog(ss,node,ELogPrintAlloc);
	Insert( node );
}

void OOMMemoryHandler::Free(void *p, site_stats_t **ss, size_t *n )
{
	OOMMemNode *node, *prev;
	int h;

	*n = 0;
	*ss = 0;
	if (size == 0) 
		return;
	h = Hash(p);
	for ( prev=0, node=table[h]; node != 0 && node->datum != p ; prev=node, node=node->next )
		;
	if (node == 0) return;
	if (prev == 0)
		table[h] = node->next;
	else
		prev->next = node->next;
	*n = node->n;
	*ss = node->ss;
	//printlog(*ss,node,ELogPrintFree);
	DisposeNode( node );
	--population;
}

#ifndef __OPRBRIDGE_DLL__
#ifdef EPOC_EMULATOR_MALLOC_INSTRUMENTATION
void UnInitializeOutOfMemoryHandling()
{
}
#endif //EPOC_EMULATOR_MALLOC_INSTRUMENTATION
#endif //__OPRBRIDGE_DLL__

static void add_object( site_stats_t *ss, void *p, size_t n )
{
	if (!object_manager)
	{
		object_manager = new OOMMemoryHandler();
	}
	object_manager->Malloc( p, ss, n );
}

static void remove_object( void *p, site_stats_t **ss, size_t *n )
{
	object_manager->Free( p, ss, n );
}

EXPORT_C void remove_callback() 
{
#ifndef MALLOC_INSTRUMENTATION_EUSER_HACK
	*(unsigned*)((char*)&(User::Adjust)+2) = actual_user_adjust_address;
	*(unsigned*)((char*)&(User::AdjustL)+2) = actual_user_adjustL_address;
	*(unsigned*)((char*)&(User::Alloc)+2) = actual_user_alloc_address;
	*(unsigned*)((char*)&(User::AllocL)+2) = actual_user_allocL_address;
	*(unsigned*)((char*)&(User::AllocLC)+2) = actual_user_allocLC_address;
	*(unsigned*)((char*)&(User::Free)+2) = actual_user_free_address;
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_FREE
	*(unsigned*)((char*)internal_user_free+1) = actual_internal_user_free_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOC
	*(unsigned*)((char*)internal_user_alloc+1) = actual_internal_user_alloc_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCL
	*(unsigned*)((char*)internal_user_allocL+1) = actual_internal_user_allocL_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_ALLOCLC
	*(unsigned*)((char*)internal_user_allocLC+1) = actual_internal_user_allocLC_address;
#endif
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_INTERNAL_REALLOC
	*(unsigned*)((char*)internal_user_realloc+1) = actual_internal_user_realloc_address;
	*(unsigned*)((char*)internal_user_reallocL+1) = actual_internal_user_reallocL_address;
#endif
	*(unsigned*)((char*)&(User::FreeZ)+2) = actual_user_freeZ_address;
	*(unsigned*)((char*)&(User::ReAlloc)+2) = actual_user_realloc_address;
	*(unsigned*)((char*)&(User::ReAllocL)+2) = actual_user_reallocL_address;
	*(unsigned*)((char*)&malloc+2) = actual_malloc_address;
	*(unsigned*)((char*)&realloc+2) = actual_realloc_address;
	*(unsigned*)((char*)&calloc+2) = actual_calloc_address;
	*(unsigned*)((char*)&free+2) = actual_free_address;
#else //MALLOC_INSTRUMENTATION_EUSER_HACK
	delete alloc_counter;
#endif //MALLOC_INSTRUMENTATION_EUSER_HACK
}

EXPORT_C void epoc_malloc_dump_allocation_sites()
{
	FILE *fp;
	
	fp = fopen( "ALLOCSITES", "a" );
	if (!fp) return;
	do_summarize_allocation_sites( fp, sites_in_use );
	fprintf( fp, "------------------------------------------------------------\n\n" );
	fclose( fp );
}

// Symbian libraries do not include longlong division support...
#define KB(x) ((int64)((double)(x)/1024.0))

static void do_summarize_allocation_sites( FILE *output, int nsites )
{
	int i, live_sites_in_use;
	int old_sites_in_use = sites_in_use;
	int64 inuse = 0;
	
	if (output == 0) return;
	
	memcpy( alloc_sites_copy, alloc_sites, sizeof(site_stats_t*)*NSITES );
	for ( i=live_sites_in_use=0 ; i < NSITES ; i++ )
	{
		if (old_sites_in_use < 0)  //something screwed up??
			if (alloc_sites_copy[i])
				sites_in_use = i; //keep the largest i which has a site
		if (alloc_sites_copy[i] && (alloc_sites_copy[i]->site.tag != 0))
		{
			inuse += alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed; 
			if (alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed > 0)
				live_sites_in_use++;
		}
	}
	TInt bytes_inuse;
	TInt bytes_free;
	TInt bytes_largest_block;
	User::AllocSize(bytes_inuse);
	bytes_free = User::Available(bytes_largest_block);
	fprintf( output, "In use: %ldb allocated from %d sites (%d live); OS reports %d live, %d free\n", 
			(long)inuse, sites_in_use, live_sites_in_use,
			(int)bytes_inuse, (int)bytes_free);
	fprintf( output, "Printing nontrivial entries only.\n" );
	
	if (nsites < 0)
		nsites = sites_in_use;

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_NOT_SORTED
	fprintf( output, "\nCurrent use and peak, unsorted\n" );
	for ( i=0 ; i < sites_pool_max ; i++ )
	{
		if (alloc_sites_copy[i]->site.tag != 0)
		{
			long kb_inuse = (long)(alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed);
			long kb_peak = (long)(alloc_sites_copy[i]->peak);
			if (kb_inuse > 0)
			{
				fprintf( output, "%6ldb %6ldb 0x%08lx M:0x%08lx, SS:0x%08lx\n", kb_inuse, kb_peak,
						 (long)(alloc_sites_copy[i]->site.location),
						 (long)(alloc_sites_copy[i]->site.memadd), alloc_sites_copy[i]);
#ifndef _NO_GLOBALS_
				for (int j=0; j<BACKTRACE_LEN; j++)
				{
					fprintf( output, "0x%08lx, ", (long)(alloc_sites_copy[i]->site.backtrace[j]));
				}
				fprintf( output, "\n");
#endif
			}
		}
	}
#endif

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_CODELOC_SORTED
	qsort( alloc_sites_copy, sites_pool_max, sizeof(site_stats_t*), compare_code_stats_a );
	fprintf( output, "\n-----<>-----\nCurrent use and peak, sorted by use\n" );
	for ( i=0 ; i < sites_pool_max ; i++ )
		if (alloc_sites_copy[i]->site.tag != 0)
		{
			long kb_inuse = (long)(alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed);
			long kb_peak = (long)(alloc_sites_copy[i]->peak);
			if (kb_inuse > 0)
			{
				fprintf( output, "%6ldb %6ldb 0x%08lx M:0x%08lx, SS:0x%08lx\n", kb_inuse, kb_peak,
						 (long)(alloc_sites_copy[i]->site.location),
						 (long)(alloc_sites_copy[i]->site.memadd), alloc_sites_copy[i]);
#ifndef _NO_GLOBALS_
				fprintf( output, "0x%08lx, ", (*alloc_sites_copy[i]->site.memadd));
				for (int j=0; j<BACKTRACE_LEN; j++)
				{
					fprintf( output, "0x%08lx, ", (long)(alloc_sites_copy[i]->site.backtrace[j]));
				}
				fprintf( output, "\n");
#endif
			}
		}
#endif

#ifdef MALLOC_INSTRUMENTATION_INCLUDE_USEAGE_SORTED
	qsort( alloc_sites_copy, sites_pool_max, sizeof(site_stats_t*), compare_size_stats_a );
	fprintf( output, "\n-----<>-----\nCurrent use and peak, sorted by use\n" );
	for ( i=0 ; i < sites_pool_max ; i++ )
		if (alloc_sites_copy[i]->site.tag != 0)
		{
			long kb_inuse = (long)(alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed);
			long kb_peak = (long)(alloc_sites_copy[i]->peak);
			if (kb_inuse > 0)
			{
				fprintf( output, "%6ldb %6ldb 0x%08lx M:0x%08lx, SS:0x%08lx\n", kb_inuse, kb_peak,
						 (long)(alloc_sites_copy[i]->site.location),(long)(alloc_sites_copy[i]->site.memadd), alloc_sites_copy[i]);
#ifndef _NO_GLOBALS_
				for (int j=0; j<BACKTRACE_LEN; j++)
				{
					fprintf( output, "0x%08lx, ", (long)(alloc_sites_copy[i]->site.backtrace[j]));
				}
				fprintf( output, "\n");
#endif
			}
		}
#endif
	
#ifdef MALLOC_INSTRUMENTATION_INCLUDE_ALL
	fprintf( output, "\n-----<>-----\nCurrent use and peak, sorted by peak\n" );
	qsort( alloc_sites_copy, sites_pool_max, sizeof(site_stats_t*), compare_peak_stats_a );
	for ( i=0 ; i < sites_pool_max ; i++ )
		if (alloc_sites_copy[i]->site.tag != 0)
		{
			long kb_inuse = (long)KB(alloc_sites_copy[i]->allocated - alloc_sites_copy[i]->freed);
			long kb_peak = (long)KB(alloc_sites_copy[i]->peak);
			if (kb_inuse > 0 || kb_peak > 0)
			{
				fprintf( output, "%6ldK %6ldK 0x%08lx M:0x%08lx\n", kb_inuse, kb_peak,
						 (long)(alloc_sites_copy[i]->site.location),
						 (long)(alloc_sites_copy[i]->site.memadd) );
#ifndef _NO_GLOBALS_
				for (int j=0; j<BACKTRACE_LEN; j++)
				{
					fprintf( output, "0x%08lx, ", (long)(alloc_sites_copy[i]->site.backtrace[j]));
				}
				fprintf( output, "\n");
#endif
			}
		}
#endif
}

#endif // EPOC_EMULATOR_MALLOC_INSTRUMENTATION
