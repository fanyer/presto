/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Declare a layer on top of class OpMemory porting interface
 *
 * This thin layer is used for a few things: Implement memory accounting
 * and policies, a debugging layer and instrumentation for selftests.
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_PILAYER_H
#define MEMORY_PILAYER_H

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_manager.h"

#ifdef OPMEMORY_VIRTUAL_MEMORY

#ifdef MEMORY_USE_LOCKING
/**
 * \brief Create a virtual memory segment
 *
 * This function is the preferred way of creating a virtual memory
 * segment. It is a thin layer on top of the OpMemory porting
 * interface abstraction class, and it allows a point of accounting,
 * debugging/logging and selftest monitoring/injection.
 *
 * This function and the associated \c OpMemory_VirtualAlloc(), \c
 * OpMemory_VirtualFree() and \c OpMemory_DestroySegment() functions
 * basically does the exact same thing as the corresponding \c
 * OpMemory porting interface, with the addition of the \c
 * OpMemoryClass type parameter for the allocation functions, which is
 * used for accounting and policy reasons.
 *
 * Additional hooks for selftests and debugging may also be present,
 * so these functions should not be circumvented by calling the
 * porting interface directly.
 *
 * \b Note: The porting interface in \c OpMemory requires the
 * "malloc-lock" to be held when called. The default TRUE value of the
 * \c memlock parameter causes the \c OpMemory::MallocLock()/Unlock()
 * methods to be called to properly protect the porting interface from
 * being called from multiple threads simultaneously.
 *
 * If the "malloc-lock" is already held, like would be the case
 * if the call-chain includes lea_malloc which also uses this lock,
 * specifying a \c memlock parameter value of FALSE would prevent
 * grabbing the lock a second time.
 *
 * \b Note: The \c memlock parameter must be FALSE \b only when
 * the "malloc-lock" is already held.
 *
 * The \c memlock parameter must not be supplied unless \c
 * MEMORY_USE_LOCKING is defined (to prevent compilation errors).
 *
 * \param minimum Number of bytes (minimum) of the new segment
 * \param memlock Call OpMemory::MallocLock()/Unlock() when set to TRUE
 * \return A memory segment handle or NULL
 */
const OpMemory::OpMemSegment*
OpMemory_CreateVirtualSegment(size_t minimum, BOOL memlock = TRUE);
#else
const OpMemory::OpMemSegment*
OpMemory_CreateVirtualSegment(size_t minimum);
#endif

/**
 * \brief Make an allocation in a virtual memory segment
 *
 * This function is the preferred way of allocating virtual memory.
 * It is a thin layer on top of the OpMemory porting interface
 * abstraction class, like the \c OpMemory_CreateVirtualSegment()
 * method.
 *
 * \b Note: The \c ptr and \c size arguments should both be multiples
 * of the memory page size.
 *
 * \b Note: The porting interface does not allow allocating the same
 * memory twise. Care should thus be taken to only allocate regions of
 * memory that has no memory allocated.
 *
 * \b Note: The "malloc-lock" \b must be held when this method is
 * called (as long as \c MEMORY_USE_LOCKING is defined and signals
 * that locking is needed).
 *
 * \param mseg The memory segment that owns the memory region
 * \param ptr A pointer to the first byte to be allocated
 * \param size Number of bytes to be allocated
 * \param type The memory class type for accounting purposes
 * \return TRUE when the allocation succeeded
 */
BOOL OpMemory_VirtualAlloc(const OpMemory::OpMemSegment* mseg, void* ptr,
						   size_t size, OpMemoryClass type);

/**
 * \brief Release memory from a virtual memory segment
 *
 * This function is the preferred way of releasing virtual memory.
 * It is a thin layer on top of the OpMemory porting interface
 * abstraction class, like the \c OpMemory_CreateVirtualSegment()
 * method.
 *
 * \b Note: The \c ptr and \c size arguments should both be multiples
 * of the memory page size.
 *
 * \b Note: The porting interface does not allow releasing memory that
 * was not previously allocated. Care should thus be taken to only
 * release regions of memory that is fully allocated.
 *
 * \b Note: The "malloc-lock" \b must be held when this method is
 * called (as long as \c MEMORY_USE_LOCKING is defined and signals
 * that locking is needed).
 *
 * \param mseg The memory segment that owns the memory region
 * \param ptr A pointer to the first byte to be released
 * \param size Number of bytes to be released
 * \param type The memory class type for accounting purposes
 */
void OpMemory_VirtualFree(const OpMemory::OpMemSegment* mseg, void* ptr,
						  size_t size, OpMemoryClass type);

#endif // OPMEMORY_VIRTUAL_MEMORY

#if defined(OPMEMORY_VIRTUAL_MEMORY) || defined(OPMEMORY_MEMORY_SEGMENT)

#ifdef MEMORY_USE_LOCKING
/**
 * \brief Destroy a memory segment
 *
 * This function is the preferred way of destroying a memory
 * segment. It is a thin layer on top of the OpMemory porting
 * interface abstraction class, like the \c
 * OpMemory_CreateVirtualSegment() method.
 *
 * \b Note: The semantics for the \c memlock parameter is the same as
 * for the \c OpMemory_CreateVirtualSegment() function.
 *
 * \param mseg The memory segment to be destroyed
 * \param memlock Call OpMemory::MallocLock()/Unlock() when set to TRUE
 */
void OpMemory_DestroySegment(const OpMemory::OpMemSegment* mseg,
							 BOOL memlock = TRUE);
#else
void OpMemory_DestroySegment(const OpMemory::OpMemSegment* mseg);
#endif // MEMORY_USE_LOCKING

#endif // OPMEMORY_VIRTUAL_MEMORY || OPMEMORY_MEMORY_SEGMENT

#endif // MEMORY_PILAYER_H
