/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Declare a global object for keeping memory state
 *
 * Declare the OpMemoryState object, which contains the memory
 * related state information needed by Opera.
 *
 * This object is kept separate from the global g_opera object, so it
 * can be initialized and memory functions be used before the opera
 * object is initialized.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_STATE_H
#define MEMORY_STATE_H

#undef MEMORY_STATE_NEEDED

#if defined(ENABLE_MEMORY_DEBUGGING) || defined(ENABLE_MEMORY_OOM_EMULATION)
/**
 * \brief Defined if the memory state object is needed
 *
 * Whenever the configuration is such that the memory state object
 * is needed, the \c MEMORY_STATE_NEEDED macro will be defined.
 * This is to simplify the conditional code dealing with the memory
 * state object.
 *
 * Remember to include \c memory_state.h when testing for this macro!
 */
#  define MEMORY_STATE_NEEDED
#endif

#if defined(HAVE_DL_MALLOC) && defined(LEA_MALLOC_API_GET_STATE_SIZE)
// lea_malloc module holds its state in the memory state object
#  undef  MEMORY_STATE_NEEDED
#  define MEMORY_STATE_NEEDED
#endif

#if defined(MEMTOOLS_ENABLE_CODELOC) || defined(MEMTOOLS_CALLSTACK_MANAGER)
// The OpCodeLocationManager and OpCallStackManager objects are anchored
// in the memory state object
#  undef MEMORY_STATE_NEEDED
#  define MEMORY_STATE_NEEDED
#endif

#if defined(USE_POOLING_MALLOC)
// The OpMemoryPoolManager is anchored in the memory state object
#  undef MEMORY_STATE_NEEDED
#  define MEMORY_STATE_NEEDED
#endif

#if defined(ENABLE_OPERA_MMAP)
// The OpMmapManager is needed when Opera provides 'mmap' like functionality
#  undef MEMORY_STATE_NEEDED
#  define MEMORY_STATE_NEEDED
#endif

#if defined(ENABLE_MEMORY_MANAGER)
// OpMemoryManager does memory accounting and memory policy decisions
#  undef MEMORY_STATE_NEEDED
#  define MEMORY_STATE_NEEDED
#endif

#ifdef MEMORY_STATE_NEEDED

#include "modules/util/simset.h"
#include "modules/memory/src/memory_opsrcsite.h"
#include "modules/pi/system/OpMemory.h"

#ifdef ENABLE_MEMORY_MANAGER
#include "modules/memory/src/memory_manager.h"
#endif

class OpMemDebug;
class OpMemGuard;
class SingleBlockHeap;
class OpCodeLocationManager;
class OpCallStackManager;
class OpMemPoolManager;
class OpHeapSegment;
class OpMmapSegment;

/**
 * \brief Opera memory state information structure
 *
 * This struct holds the opera memory state information.  This includes
 * (parts) of the state information of the memory module, and the lea_malloc
 * module (if it is enabled).
 *
 * The memory module also has state information in the MemoryModule
 * object, for pooling related data-structures.
 *
 * Note that the lea_malloc state information is located rigth
 * after this struct.  The lea malloc state details are local to
 * the lea_malloc/malloc.c file, and is only referenced through
 * an offset, which is the size of this struct.
 *
 * Not knowing the exact size of the lea_malloc state information
 * during compile time is a hassle, as the memory state will have
 * to be allocated "large enough" - with possible waste.
 * However, the size of the whole structure is typically below
 * 1K so there is little waste compared to overall memory usage.
 */
struct OpMemoryState
{
	/**
	 * \brief Constructor to initialize the OpMemoryState object
	 *
	 * The OpMemoryState object is initialized through construction, as
	 * this allows us to have embedded complex types like Head right
	 * inside the object.  This improves flexibility and performance.
	 */
	OpMemoryState(void);
	/**
	 * \brief Destructor. Destroys all structures created in the
	 *  constructor.
	 */
	~OpMemoryState();

	/**
	 * \brief Struct alignment guardian
	 *
	 * Help ensure the alignment of the "shadow" structure used to
	 * allocate the zeroed bytes that makes up the OpMemoryState
	 * object (and that of lea_malloc which follows closely).
	 *
	 * The "shadow" structure will typically look somethign like:
	 * <code>struct shadow {
	 *    double alignment;
	 *    char state[1024]; // ARRAY OK joehack 2008-02-24
	 * };</code>
	 * and be initialized to <code>{ 42.0, { 0, } }</code>.
	 */
	double mem_alignment_guard;

	/**
	 * \brief Variable to signal this structure has been initialized
	 *
	 * This flag variable is used to decide if initialization is
	 * needed or not in the \c OpMemoryStateInit() function.
	 * The debugging code needs to be able to decide if this
	 * structure has been initialized or not (as debug code needs
	 * to initialize on first use).
	 *
	 * Also, this flag will prevent multiple initialization from
	 * e.g. pooling initialization.
	 *
	 * A nonzero value indicates that the structure is initialized.
	 *
	 * For this initialized flag to work, it is important that the
	 * underlying memory used to hold this struct is initialized to
	 * all zeroes before the Opera allocation functions are called.
	 */
	int mem_state_initialized;

	/**
	 * \brief Variable signalling successful initialization
	 *
	 * During the construction of this structure, this variable
	 * will signal a successful initialization or not.
	 */
	int mem_state_initialized_ok;

#ifdef ENABLE_MEMORY_MANAGER
	/**
	 * \brief The central memory manager for memory policies
	 *
	 * The central OpMemoryManager is embedded into the memory
	 * state object so it does not need dynamic allocation,
	 * which would complicate things.
	 */
	OpMemoryManager mem_manager;
#endif

#ifdef ENABLE_MEMORY_OOM_EMULATION
	/**
	 * \brief A heap of a limited size to help emulate OOM
	 *
	 * This heap is allocated off the \c op_lowlevel_malloc() function,
	 * which is typically the system allocator.  This chunk of memory
	 * is initialized as a \c SingleBlockHeap object, and is used to
	 * emulate a limited heap, e.g. on a phone.
	 *
	 * This heap is used differently depending on whether memory
	 * debugging is enabled or not.
	 *
	 * If memory debugging is not enabled, the allocated objects are
	 * placed physically in this limited heap.  This pointer must also
	 * be initialized to NULL, or initialized properly by calling the
	 * \c OpMemoryInitLimitedHeap() function, before any allocations
	 * can happen.  Allocations done before calling
	 * \c OpMemoryInitLimitedHeap() will be done using
	 * \c op_lowlevel_malloc(), and will for this reason not be included
	 * in the accounting for the limited heap.
	 *
	 * If memory debugging is enabled, the allocated objects are
	 * placed outside this limited heap, where the overhead of
	 * guard bytes, call-stack, allocation information etc. does
	 * not influence the OOM behaviour.  In this case, the limited
	 * heap is only used to decide if an allocation succeeds or not,
	 * and the allocated bytes on the limited heap are not used.
	 */
	SingleBlockHeap* mem_limited_heap;
#endif // ENABLE_MEMORY_OOM_EMULATION

#ifdef ENABLE_MEMORY_DEBUGGING
	//
	// Memory debugging functionality.  Memory debugging is initialized
	// on first use by simple if-tests.  This can be afforded for
	// debugging code.  For this to work, we require that the memory
	// state is initialized to zeroes before first use.
	//
	OpMemDebug* mem_debug;
	OpMemGuard* mem_guard;

	OpSrcSite** mem_opsrcsite_hashtable;
	OpSrcSite** mem_opsrcsite_id2site;
	OpSrcSite mem_opsrcsite_oom_site;     // siteid == 1
	OpSrcSite mem_opsrcsite_error_site;   // siteid == 2
	OpSrcSite mem_opsrcsite_unknown_site; // siteid == 3
	UINT32 mem_opsrcsite_next_id;

	Head mem_event_listeners;

#endif // ENABLE_MEMORY_DEBUGGING

#if defined(USE_POOLING_MALLOC)
	OpMemPoolManager* mem_poolmgr;
#endif

#ifdef MEMTOOLS_ENABLE_CODELOC
	OpCodeLocationManager* mem_codelocmgr;
#endif

#ifdef MEMTOOLS_CALLSTACK_MANAGER
	OpCallStackManager* mem_callstackmgr;
#endif

#ifdef ENABLE_OPERA_MMAP
	const OpMemory::OpMemSegment* mem_mmapemu_mseg;
	OpMmapSegment* mem_mmapemu_mmap;
#endif

#ifdef ENABLE_OPERA_SBRK
	const OpMemory::OpMemSegment* mem_sbrk_mseg;
	OpHeapSegment* mem_sbrk_heap;
#endif
};

extern "C" int OpMemoryStateInit(void);
extern "C" void OpMemoryStateDestroy(void);
extern "C" size_t OpMemoryStateGetSize(void);

#if MEMORY_USE_GLOBAL_VARIABLES
/**
 * \brief The object used to allocate the memory state in global variables
 *
 * This structure is used to statically allocate the \c g_opera_memory_state
 * in global variables.  The reason for the static sizes and missing
 * OpMemoryState object (which is actually stored inside this struct), is
 * those of not knowing the size at compile time (lea_malloc state), and
 * because we don't want to be dependent on static initializers for the
 * OpMemoryState object, as this may not work everywhere, fire too late etc.
 */
struct OpMemoryStateStatic
{
	double alignment_guard;

#if defined(HAVE_DL_MALLOC) && defined(LEA_MALLOC_API_GET_STATE_SIZE)
#  ifdef SIXTY_FOUR_BIT
	char state[2560];  // ARRAY OK 2008-02-24 mortenro
#  else
	char state[1280];  // ARRAY OK 2008-02-24 mortenro
#  endif

#else // No lea_malloc state in memory state object
	char state[sizeof(OpMemoryState)];	// ARRAY OK 2009-06-03 mortenro
#endif
};

extern OpMemoryState* g_memory_state;
#define g_opera_memory_state (g_memory_state)

#endif // MEMORY_USE_GLOBAL_VARIABLES

#define g_mem_state_initialized \
	g_opera_memory_state->mem_state_initialized
#define g_mem_state_initialized_ok \
	g_opera_memory_state->mem_state_initialized_ok

#ifdef ENABLE_MEMORY_OOM_EMULATION
#define g_mem_limited_heap \
	g_opera_memory_state->mem_limited_heap
#endif // ENABLE_MEMORY_OOM_EMULATION

#ifdef ENABLE_MEMORY_DEBUGGING
#define g_mem_debug \
	g_opera_memory_state->mem_debug
#define g_mem_guard \
	g_opera_memory_state->mem_guard
#define g_mem_opsrcsite_hashtable \
	g_opera_memory_state->mem_opsrcsite_hashtable
#define g_mem_opsrcsite_id2site \
	g_opera_memory_state->mem_opsrcsite_id2site
#define g_mem_opsrcsite_oom_site \
	g_opera_memory_state->mem_opsrcsite_oom_site
#define g_mem_opsrcsite_error_site \
	g_opera_memory_state->mem_opsrcsite_error_site
#define g_mem_opsrcsite_unknown_site \
	g_opera_memory_state->mem_opsrcsite_unknown_site
#define g_mem_opsrcsite_next_id \
	g_opera_memory_state->mem_opsrcsite_next_id
#define g_mem_event_listeners \
	g_opera_memory_state->mem_event_listeners
#endif // ENABLE_MEMORY_DEBUGGING

#if defined(USE_POOLING_MALLOC)
#define g_mem_poolmgr \
	g_opera_memory_state->mem_poolmgr
#endif

#ifdef MEMTOOLS_ENABLE_CODELOC
#define g_memtools_codelocmgr \
	g_opera_memory_state->mem_codelocmgr
#endif

#ifdef MEMTOOLS_CALLSTACK_MANAGER
#define g_memtools_callstackmgr \
	g_opera_memory_state->mem_callstackmgr
#endif

#ifdef ENABLE_OPERA_MMAP
#define g_mem_mmapemu_mseg \
	g_opera_memory_state->mem_mmapemu_mseg
#define g_mem_mmapemu_mmap \
	g_opera_memory_state->mem_mmapemu_mmap
#endif

#ifdef ENABLE_OPERA_SBRK
#define g_mem_sbrk_mseg \
	g_opera_memory_state->mem_sbrk_mseg
#define g_mem_sbrk_heap \
	g_opera_memory_state->mem_sbrk_heap
#endif

#ifdef ENABLE_MEMORY_MANAGER
#define g_mem_manager \
	(&(g_opera_memory_state->mem_manager))
#endif

#endif // MEMORY_STATE_NEEDED

#endif // MEMORY_STATE_H
