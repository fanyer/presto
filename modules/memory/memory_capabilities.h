/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/** \file
 *
 * \brief Capability defines for the memory module.
 *
 * Macros that can be tested for to adapt other code to deal with
 * different versions of the memory module.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_CAPABILITIES_H
#define MEMORY_CAPABILITIES_H

/**
 * \brief The memory module is present
 *
 * This macro is defined to signal the presense of the memory module.
 * This macro may be used to test whether its include files should be
 * included or not, for instance.
 *
 * When this macro is defined, it is the responsibility of the memory
 * module to define all memory allocation operations, including:
 *
 *   \c op_malloc() <br>
 *   \c op_free() <br>
 *   \c op_realloc() <br>
 *   \c op_calloc() <br>
 *   \c op_strdup() <br>
 *   \c uni_strdup() <br>
 *   <code>operator new</code> <br>
 *   <code>operator new[]</code> <br>
 *   <code>operator delete</code> <br>
 *   <code>operator delete[]</code> <br>
 *
 * This list may not be exhaustive.
 */
#define MEM_CAP_DECLARES_MEMORY_API

/**
 * \brief The memory module uses OpMemoryState for memory state information 
 *
 * When this macro is defined, the memory state for the memory module
 * is stored in the OpMemoryState object.  Also, the lea_malloc module
 * can make use of this memory state "infrastructure":
 *
 * The lea_malloc module may place its state information at the following
 * designated address:
 *
 * <code>(void*)(((OpMemoryState*)g_opera_memory_state)+1)</code>
 *
 * I.e. the first byte after the OpMemoryState object (which is located
 * at \c g_opera_memory_state).
 *
 * This capability can also be used to determine if the memory module
 * (has the ability to) provide a global placement new operator.
 * With the introduction of this capability and onwards, it must be
 * assumed that the placement new operator is generally available, and
 * it must not be declared locally where needed.
 */
#define MEM_CAP_MEMORY_STATE

/**
 * \brief The memory module provides simple leakchecking for selftests
 *
 * When this macro is defined, the \c MEMORY_ACTION_SET_MARKER_M3,
 * \c MEMORY_ACTION_NEW_SINCE_M3 and \c MEMORY_ACTION_MARK_SINGLE_M3
 * actions will be available through the \c OpMemDebug::Action() method.
 *
 * This cabability is versioned for future expansion.
 */
#define MEM_CAP_SELFTEST_LEAKCHECK 1

/**
 * \brief The memory module can enable memory logging dynamically
 *
 * When this macro is defined, the \c MEMORY_ACTION_ENABLE_LOGGING action
 * is available to let the platform enable full memory logging dynamically
 * at startup (based on e.g. command line options).
 */
#define MEM_CAP_DYNAMICALLY_ENABLE_MEMLOG

/**
 * \brief The memory module supports pooling declarations macros
 *
 * When this macro is defined, the following macros are defined for
 * use:
 *
 *   OP_ALLOC_ACCOUNTED_POOLING
 *   OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
 */
#define MEM_CAP_POOLING_MACROS_1

/**
 * \brief The memory module provides MEMORY_ACTION_SET_MARKER_M4_SINGLE
 *
 * When this macro is defined, the \c
 * MEMORY_ACTION_SET_MARKER_M4_SINGLE can be used to mark a single
 * memory allocation as exempt from leak reporting.
 */
#define MEM_CAP_SET_MARKER_M4_SINGLE

#endif // MEMORY_CAPABILITIES_H
