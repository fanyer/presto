/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement a global object for keeping memory state
 *
 * Implement the OpMemoryState object, which contains the memory
 * related state information needed by Opera.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef MEMORY_STATE_NEEDED

#include "modules/memory/src/memory_state.h"
#include "modules/memory/src/memory_memguard.h"
#include "modules/memory/src/memory_debug.h"

#if defined(HAVE_DL_MALLOC) && defined(MEMORY_EMBED_LEA_MALLOC_STATE)
#include "modules/lea_malloc/lea_malloc.h"
#endif

/**
 * \brief Get the required size of the memory state object
 *
 * This function will return the size of the memory state object
 * where Opera stores the memory state information for the memory
 * module and possibly lea_malloc.
 *
 * The allocated memory area *must* be at least as large as what
 * this function returns.  For dynamic allocations, this function
 * can be used to decide the exact size required.
 *
 * For statically allocated state information, it is recomended
 * that an assert() or similar code construct is performed to
 * make sure on runtime that the allocated block is large enough
 * to hold the data, e.g.:
 *
 * <code>assert(sizeof(memory_state) >= OpMemoryStateSize());</code>
 */
extern "C" size_t OpMemoryStateGetSize(void)
{
	return
#if defined(HAVE_DL_MALLOC) && defined(MEMORY_EMBED_LEA_MALLOC_STATE)
		get_malloc_state_size() +
#endif
		sizeof(OpMemoryState);
}

/**
 * \brief Construct parts of the memory state object
 *
 * The memory state object is initialized in several phases.
 * The memory debugging state is initialized explicitly on first
 * call to any of the debugging gateway functions, and the lea_malloc
 * state is initialized by lea_malloc itself on first use.
 *
 * This constructor should not be called explicitly, see
 * \c OpMemoryStateInit() instead.
 */
OpMemoryState::OpMemoryState(void)
	: mem_alignment_guard(42.0)
	, mem_state_initialized(1)
	, mem_state_initialized_ok(1)
#ifdef ENABLE_MEMORY_OOM_EMULATION
	, mem_limited_heap(0)
#endif
#ifdef ENABLE_MEMORY_DEBUGGING
	, mem_opsrcsite_hashtable(0)
	, mem_opsrcsite_id2site(0)
	, mem_opsrcsite_oom_site("OOM", 1, 1)
	, mem_opsrcsite_error_site("Internal-error", 1, 2)
	, mem_opsrcsite_unknown_site("<unknown>", 0, 3)
	, mem_opsrcsite_next_id(4)
#endif
#if defined(USE_POOLING_MALLOC)
	, mem_poolmgr(0)
#endif
#ifdef MEMTOOLS_ENABLE_CODELOC
	, mem_codelocmgr(0)
#endif
#ifdef MEMTOOLS_CALLSTACK_MANAGER
	, mem_callstackmgr(0)
#endif
{
#if MEMORY_USE_GLOBAL_VARIABLES
	if ( sizeof(OpMemoryStateStatic) < OpMemoryStateGetSize() )
	{
		// Oops, our OpMemoryStateStatic is not large enough!
		OP_ASSERT(!"OpMemoryStateStatic is not large enough!");
		mem_state_initialized_ok = 0;
	}
#endif // MEMORY_USE_GLOBAL_VARIABLES

#ifdef ENABLE_MEMORY_DEBUGGING

	mem_debug = new OpMemDebug;
	mem_guard = new OpMemGuard;

	if ( mem_debug == 0 || mem_guard == 0 )
	{
		OP_ASSERT(!"Failed to create OpMemDebug or OpMemGuard");
		mem_state_initialized_ok = 0;
	}

#endif // ENABLE_MEMORY_DEBUGGING

#if defined(USE_POOLING_MALLOC)
	mem_poolmgr = new OpMemPoolManager;
	if ( mem_poolmgr == 0 )
	{
		OP_ASSERT(!"Failed to create OpMemPoolManager.");
		mem_state_initialized_ok = 0;
	}
#ifdef ENABLE_MEMORY_DEBUGGING
	else
		mem_debug->Action(MEMORY_ACTION_SET_MARKER_M4_SINGLE, mem_poolmgr);
#endif // ENABLE_MEMORY_DEBUGGING
#endif // USE_POOLING_MALLOC
}

OpMemoryState::~OpMemoryState()
{
#ifdef ENABLE_MEMORY_DEBUGGING

	delete mem_debug;
	delete mem_guard;

	// normally this should not be needed, but since OpMemoryState uses
	// placement in static OpMemoryStateStatic storage, it might be good
	// to nullify these pointers, because they are still accessible.
	mem_debug = 0;
	mem_guard = 0;
#endif // ENABLE_MEMORY_DEBUGGING

#if defined(USE_POOLING_MALLOC)
	delete mem_poolmgr;
	// normally this should not be needed, but since OpMemoryState uses
	// placement in static OpMemoryStateStatic storage, it might be good
	// to nullify these pointers, because they are still accessible.
	mem_poolmgr = 0;
#endif // USE_POOLING_MALLOC
}


#if MEMORY_USE_GLOBAL_VARIABLES
/**
 * \brief The global variable OpMemoryStateStatic object
 *
 * The OpMemoryStateStatic object holds the OpMemoryState object,
 * and the lea_malloc state if enabled.
 *
 * The size of the \c OpMemoryStateStatic object should be large enough
 * to hold the \c OpMemoryState object and the lea_malloc state
 * if enabled.
 */
OpMemoryStateStatic g_memory_state_static = { 42.0, { 0, } };
OpMemoryState *g_memory_state = (OpMemoryState*)&g_memory_state_static;
#endif // MEMORY_USE_GLOBAL_VARIABLES

/**
 * \brief Initialize parts of the opera memory state object
 *
 * This initialization will currently be done automatically by
 * the memory debugging framework upon first activation, or, if
 * pooled allocations are used, when constructing the memory module.
 *
 * The OpMemoryState structure inside the \c g_opera_memory_state
 * might not hold any data (depending on configuration), but the
 * structure will still occupy (unused) space in the global memory
 * state object.
 *
 * The lea_malloc state is not initialized by this function; lea_malloc
 * will do its own initialization on first call, discovering that
 * the bytes are all zero (which is a requirement).
 */
extern "C" int OpMemoryStateInit(void)
{
	if ( !g_mem_state_initialized )
	{
		new ((void*)g_opera_memory_state) OpMemoryState;
		g_mem_state_initialized = 1;
	}

	return g_mem_state_initialized_ok;
}

/**
 * \brief Destroys the structures held by OpMemoryState
 *
 */
extern "C" void OpMemoryStateDestroy(void)
{
	if ( g_mem_state_initialized )
	{
		OpMemoryState* mem_state = ((OpMemoryState*)g_opera_memory_state);
		mem_state->~OpMemoryState();
		g_mem_state_initialized = 0;
	}
}

#endif // MEMORY_STATE_NEEDED
