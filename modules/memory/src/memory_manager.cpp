/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement a memory policy manager class
 *
 * The memory policy manager class is used to controll how much memory
 * is used for what, and whether an allocation can be allowed or not.
 * The decisions about allocation/no allocation is typically taken
 * right before a system call would request more memory from the
 * system (through e.g. the \c OpMemory allocation porting interface).
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_MANAGER

#include "modules/memory/src/memory_manager.h"

OpMemoryManager::OpMemoryManager(void)
{
	total = 0;
#ifdef SIXTY_FOUR_BIT
		// Assume size_t is 64-bit on 64-bit systems
		total_max = 0x7fffffffffffffff;
#else
		total_max = 0x7fffffff;
#endif

	for ( int k = 0; k < MEMCLS_LAST; k++ )
	{
		allocated[k] = 0;
#ifdef SIXTY_FOUR_BIT
		// Assume size_t is 64-bit on 64-bit systems
		max[k] = 0x7fffffffffffffff;
#else
		max[k] = 0x7fffffff;
#endif
	}
}

BOOL OpMemoryManager::Alloc(enum OpMemoryClass type, size_t size)
{
	if ( type != MEMCLS_UNACCOUNTED )
	{
		size_t new_type = allocated[type] + size;
		size_t new_total = total + size;

		if ( new_type > max[type] || new_total > total_max )
			return FALSE;

		allocated[type] = new_type;
		total = new_total;
	}

	return TRUE;
}

void OpMemoryManager::ForceAlloc(enum OpMemoryClass type, size_t size)
{
	if ( type != MEMCLS_UNACCOUNTED )
	{
		OP_ASSERT(type < MEMCLS_LAST);
		total += size;
		allocated[type] += size;
	}
}

BOOL OpMemoryManager::Transfer(enum OpMemoryClass src,
							   enum OpMemoryClass dst,
							   size_t size)
{
	if ( src != MEMCLS_UNACCOUNTED )
	{
		OP_ASSERT(src < MEMCLS_LAST);
		OP_ASSERT(total >= size);
		OP_ASSERT(allocated[src] >= size);

		allocated[src] -= size;
		total -= size;
	}

	if ( dst != MEMCLS_UNACCOUNTED )
	{
		size_t new_dst = allocated[dst] + size;
		size_t new_total = total + size;

		if ( new_dst > max[dst] || new_total > total_max )
		{
			if ( src != MEMCLS_UNACCOUNTED )
			{
				// Revert freeing from first if-clause in method
				allocated[src] += size;
				total += size;
			}

			return FALSE;
		}

		allocated[dst] = new_dst;
		total = new_total;
	}

	return TRUE;
}

void OpMemoryManager::ForceTransfer(enum OpMemoryClass src,
									enum OpMemoryClass dst,
									size_t size)
{
	if ( src != MEMCLS_UNACCOUNTED )
	{
		OP_ASSERT(src < MEMCLS_LAST);
		OP_ASSERT(total >= size);
		OP_ASSERT(allocated[src] >= size);

		total -= size;
		allocated[src] -= size;
	}

	if ( dst != MEMCLS_UNACCOUNTED )
	{
		OP_ASSERT(dst < MEMCLS_LAST);

		total += size;
		allocated[dst] += size;
	}
}

void OpMemoryManager::Free(enum OpMemoryClass type, size_t size)
{
	if ( type != MEMCLS_UNACCOUNTED )
	{
		OP_ASSERT(type < MEMCLS_LAST);
		OP_ASSERT(total >= size);
		OP_ASSERT(allocated[type] >= size);

		total -= size;
		allocated[type] -= size;
	}
}

#if 0
void OpMemoryManager::Dump(void)
{
	return;

	size_t check_total = 0;

	for ( int k = 1; k < MEMCLS_LAST; k++ )
	{
		if ( allocated[k] != 0 )
		{
			const char* str;
			switch ( k )
			{
			case MEMCLS_UNACCOUNTED:
				str = "MEMCLS_UNACCOUNTED";
				break;
			case MEMCLS_DEFAULT:
				str = "MEMCLS_DEFAULT    ";
				break;
			case MEMCLS_OP_SBRK:
				str = "MEMCLS_OP_SBRK    ";
				break;
			case MEMCLS_OP_MMAP:
				str = "MEMCLS_OP_MMAP    ";
				break;
			case MEMCLS_MMAP_HEADER:
				str = "MEMCLS_MMAP_HEADER";
				break;
			case MEMCLS_MMAP_UNUSED:
				str = "MEMCLS_MMAP_UNUSED";
				break;
			case MEMCLS_SELFTEST1:
				str = "MEMCLS_SELFTEST1  ";
				break;
			case MEMCLS_SELFTEST2:
				str = "MEMCLS_SELFTEST2  ";
				break;
			default:
				str = "Unknown           ";
				break;
			}

			dbg_printf("%s %z\n", str, allocated[k]);
		}
		check_total += allocated[k];
	}

	OP_ASSERT(check_total == total);

	dbg_printf("Total:             %z\n\n", check_total);
}
#endif // 0

#endif // ENABLE_MEMORY_MANAGER
