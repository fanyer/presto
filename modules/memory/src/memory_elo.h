/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Include file containing the ELOPOOL allocator.
 * 
 * This file contains the structures and associated with the
 * ELOPOOL and ELSPOOL allocator.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

/**
 * \brief Pool header for ELOPOOL allocator.
 *
 * These member fields are needed to keep track of allocation progress in
 * an ELOPOOL.
 */
struct OpMemPoolELO
{
	/**
	 * \brief Free memory offset
	 *
	 * This variable holds the offset into the pool for the first free byte.
	 * It starts out at 0, and ends up somewhere below or at MEMORY_BLOCKSIZE
	 * when the pool is filled up.
	 */
	unsigned short free_offset;

	/**
	 * \brief Number of allocations in this pool
	 *
	 * This variable will be increased on allocations, and decreased on
	 * when releasing memory (in this pool).  Only when this variable
	 * reaches zero can we reuse the pool for something else.
	 */
	unsigned short count;
};
