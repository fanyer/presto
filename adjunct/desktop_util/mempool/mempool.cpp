/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/mempool/mempool.h"

/***********************************************************************************
 ** Structure of the pool:
 **
 ** Block:
 ** ------------------------------------------------------------------------------------------
 ** |   pointer to next block   |   Block Size   |    Padding    |    Block Size * Unit      |
 ** ------------------------------------------------------------------------------------------
 **
 ** The padding is only used on machines that need alignment.
 **
 ** Unit:
 ** ---------------------------------------------------------
 ** |   pointer to next unit    |       m_unit_size         |
 ** ---------------------------------------------------------
 **
 ** In the case of a free unit, the 'next unit' is the next free unit; in case
 ** of an occupied unit, its value is '1'.
 **
 ***********************************************************************************/



/***********************************************************************************
 ** Constructor
 **
 ** GenericMemPool::GenericMemPool
 ** @param unit_size Size of the units to be managed in bytes
 ** @param units_per_block How many units to create per block
 **
 ***********************************************************************************/
GenericMemPool::GenericMemPool(size_t unit_size, size_t units_per_block, size_t initial_units)
  : m_unit_size(unit_size)
  , m_units_per_block(units_per_block)
  , m_initial_units(initial_units)
  , m_blocks(0)
  , m_free_units(0)
{
#ifdef NEEDS_RISC_ALIGNMENT
	// Make sure unit size is divideable by 8 bytes
	if ((m_unit_size + sizeof(void*)) % 8)
		m_unit_size += 8 - ((m_unit_size + sizeof(void*)) % 8);
#else
	// Make sure unit size is divideable by sizeof(void*)
	if (m_unit_size % sizeof(void*))
		m_unit_size += sizeof(void*) - (m_unit_size % sizeof(void*));
#endif
}


/***********************************************************************************
 ** Mark all units as free and free all memory kept by the pool
 **
 ** GenericMemPool::DeleteAll
 **
 ***********************************************************************************/
void GenericMemPool::DeleteAll()
{
	while (m_blocks)
	{
		void* next_block = *m_blocks;

		op_free(m_blocks);
		m_blocks = (void**)next_block;
	}

	m_free_units = 0;
}


/***********************************************************************************
 ** Create a new block of units
 **
 ** GenericMemPool::NewBlock
 ** @param block_size Size of block to create (in units)
 **
 ***********************************************************************************/
void GenericMemPool::NewBlock(size_t block_size)
{
	size_t real_unit_size = sizeof(void*) + m_unit_size;
	size_t word_unit_size = real_unit_size / sizeof(void*);
	void** new_block      = (void**)op_malloc((HeaderWords * sizeof(void*)) + (real_unit_size * block_size));
	if (!new_block)
		return;

	// Save block size
	*(new_block + 1) = (void*)block_size;

	// Initialize units
	void** unit = new_block + HeaderWords;

	for (size_t i = 0; i < block_size - 1; i++, unit += word_unit_size)
	{
		*unit = unit + word_unit_size;
	}
	*unit        = m_free_units;

	// Maintain lists
	*new_block   = m_blocks;
	m_blocks     = new_block;
	m_free_units = new_block + HeaderWords;
}
