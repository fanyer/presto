/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H

/**
 * @brief A memory pool
 * @author Arjan van Leeuwen
 *
 * Use this if you expect to allocate many items of the same type. Can be used like this:
 * (where T is the type you're allocating)
 *
 * Declaration: MemPool<T> mempool
 * Allocation:  T* new_item = new (&mempool) T();
 * Deletion:    mempool.Delete(new_item);
 *
 */

class GenericMemPool
{
protected:

#if defined NEEDS_RISC_ALIGNMENT && !defined SIXTY_FOUR_BIT
	static const size_t HeaderWords = 3;
#else
	static const size_t HeaderWords = 2;
#endif

//  -------------------------
//  Protected member functions:
//  -------------------------

	GenericMemPool(size_t unit_size, size_t units_per_block, size_t initial_units);

	virtual ~GenericMemPool() {}

	void* New()
		{
			if (!m_free_units)
				NewBlock(!m_blocks && m_initial_units ? m_initial_units : m_units_per_block);

			if (m_free_units)
			{
				void** new_unit_start = m_free_units;

				m_free_units = (void**)*m_free_units;
				*new_unit_start = (void*)0x1;

				return new_unit_start + 1;
			}

			return NULL;
		}

	void  Delete(void* to_delete)
		{
			void** unit = (void**)to_delete - 1;
			*unit = m_free_units;
			m_free_units = unit;
		}

	void  DeleteAll();

	void  NewBlock(size_t block_size);

//  -------------------------
//  Protected member variables:
//  -------------------------

	size_t m_unit_size;
	size_t m_units_per_block;
	size_t m_initial_units;
	void** m_blocks; // points at allocated block
	void** m_free_units; // points at unit
};

/***********************************************************************************
**
** MemPool
**
***********************************************************************************/

template<class T>
class MemPool
	: protected GenericMemPool
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 * Constructor
	 * @param units_per_block How many units to create per block
	 * @param initial_units Size of initial block
	 */
	MemPool(size_t units_per_block = 256,
			size_t initial_units = 0) :
		GenericMemPool(sizeof(T),
					   units_per_block,
					   initial_units)
		{}

	/**
	 * Destructor
	 */
	~MemPool()
		{ DeleteAll(); }

	/**
	 * Change how many units should be created in the initial block
	 * @param initial_units Size of initial block
	 */
	void  SetInitialUnits(size_t initial_units) { m_initial_units = initial_units; }

	/**
	 * Get a new unit and mark it as used
	 * @return Address of a free unit
	 */
	void* New()
		{ return GenericMemPool::New(); }

	/**
	 * Mark a unit as free
	 * @param to_delete The unit to mark
	 */
	void  Delete(T* to_delete)
		{ to_delete->~T(); GenericMemPool::Delete(to_delete); }

	/**
	 * Mark all units as free and free all memory kept by the pool
	 */
	void  DeleteAll()
		{
			void** block = m_blocks;

			while (block)
			{
				void** unit_start = block + HeaderWords;
				INTPTR block_size = (INTPTR)(*(block + 1));
				size_t unit_size  = (sizeof(void*) + m_unit_size) / sizeof(void*);

				for (INTPTR i = 0; i < block_size; i++, unit_start += unit_size)
				{
					if ((INTPTR)(*unit_start) == 0x1)
					{
						((T*)(unit_start + 1))->~T();
					}
				}

				block = (void**)*block;
			}

			GenericMemPool::DeleteAll();
		}
};

template <class T>
void* operator new(size_t sz, MemPool<T>* mp)
	{ OP_ASSERT(sz == sizeof(T)); return mp->New(); }

#endif // MEMPOOL_H
