/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Include file containing the SMOPOOL allocator.
 * 
 * This file contains the structures and code associated with the
 * SMOPOOL allocator.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

/**
 * \brief Pool header for SMOPOOL allocator.
 *
 * These member fields are needed to keep track of all allocations in
 * an SMOPOOL.  The \c freelist initially contains 0, i.e. it is
 * empty, and the number of allocated objects identified by \c count
 * is 0.  The free_offset is initialized to 0 to indicate that the
 * first allocation should happen from the beginning of the pool.
 *
 * When \c freelist is non-zero, it is the offset into the pool \e
 * pluss \e one, of the first byte of the free block (e.g. \c freelist=1
 * indicates the object at offset 0 in the pool).
 */
struct OpMemPoolSMO
{
	/**
	 * \brief The size of individual objects in this pool
	 *
	 * All objects in an \c SMOPOOL are of the same size, this
	 * variable identifies this size.  The size is preconditioned
	 * before actual allocation, so this size is always compliant
	 * with alignment restrictions.
	 */
	unsigned short size;

	/**
	 * \brief The offset into the pool for the first free object pluss one
	 *
	 * When the \c freelist field is 0, this means there are no available
	 * objects on the freelist.  When it is 1, this means there is an
	 * object available on pool-offset 0, a value of 17 means an object is
	 * available on offset 16 etc.
	 */
	unsigned short freelist;

	/**
	 * \brief Number of objects currently allocated in pool
	 *
	 * This field starts at 0, and is incremented for every allocation,
	 * and decremented for every deallocation.  When it reaches zero, the
	 * pool is empty and can potentially be reallocated for new uses.
	 */
	unsigned short count;

	/**
	 * \brief Offset into pool of first unallocated object
	 *
	 * When a new pool is created, the freelist is initially empty, and
	 * the \c free_offset is set to 0 to signal that the next allocation
	 * is from address <code>pooladdr+free_offset</code> provided this
	 * will not overflow the pool.
	 *
	 * The advantage of this strategy is that we don't have to set up the
	 * complete freelist during pool creation. This avoids a bit of
	 * cache polution.
	 */
	unsigned short free_offset;
};
