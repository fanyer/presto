/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Declare a memory policy manager class
 *
 * The memory policy manager class is used to controll how much memory
 * is used for what, and whether an allocation can be allowed or not.
 * The decisions about allocation/no allocation is typically taken
 * right before a system call would request more memory from the
 * system (through e.g. the \c OpMemory allocation porting interface).
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

/**
 * \brief All allocation classes of interest
 *
 * This enumeration is used to identify allocation types, and tie
 * allocations into the accounting done in the \c OpMemoryManager class.
 *
 * Accounting may be important to certain projects, and for dynamically
 * keeping track of memory usage.
 *
 * The information for the classes MEMCLS_OP_SBRK and MEMCLS_OP_MMAP
 * will only be valid when the Opera emulation layer is used for
 * these functions, or the platform takes care to update these
 * values accordingly to what happens in the platform layer.
 *
 * The selftest accounts are only used by the selftests, and should
 * not be used for anything else.
 */
enum OpMemoryClass
{
	MEMCLS_UNACCOUNTED = 0, ///< This class is not counted nor policied

	MEMCLS_DEFAULT,	 ///< If nothing else is better
	MEMCLS_OP_SBRK,	 ///< Allocations by op_sbrk() or by similar platform code
	MEMCLS_OP_MMAP,	 ///< Allocations by op_mmap() or by similar platform code
	MEMCLS_ECMASCRIPT_EXEC, ///< Ecmascript memory for native execution
	MEMCLS_REGEXP_EXEC, ///< Regexp memory for native execution

#ifdef ENABLE_OPERA_MMAP_SEGMENT
	MEMCLS_MMAP_HEADER,	///< Default memory class for OpMmapSegment headers
	MEMCLS_MMAP_UNUSED,	///< Memory held by OpMmapSegment for possible reuse
#endif

#ifdef SELFTEST
	MEMCLS_SELFTEST1,  ///< Used during selftests to verify correct accounting
	MEMCLS_SELFTEST2,  ///< Used during selftests to verify correct accounting
	MEMCLS_SELFTEST3,  ///< Used during selftests to verify correct accounting
#endif

	MEMCLS_LAST		   ///< End marker to determine number of entries
};

#ifdef ENABLE_MEMORY_MANAGER

class OpMemoryManager
{
public:

	OpMemoryManager(void);

	BOOL Alloc(enum OpMemoryClass type, size_t size);
	void ForceAlloc(enum OpMemoryClass type, size_t);

	BOOL Transfer(enum OpMemoryClass src, enum OpMemoryClass dst, size_t size);
	void ForceTransfer(enum OpMemoryClass src, enum OpMemoryClass dst,
					   size_t size);

	void Free(enum OpMemoryClass type, size_t);

	size_t GetTotal(void) { return total; }
	size_t GetTotal(enum OpMemoryClass type) { return allocated[type]; }
	void SetMax(size_t val) { total_max = val; }
	void SetMax(enum OpMemoryClass type, size_t val) { max[type] = val; }
	size_t GetMax(enum OpMemoryClass type) { return max[type]; }

	void Dump(void);

private:

	size_t total;
	size_t total_max;
	size_t max[MEMCLS_LAST];
	size_t allocated[MEMCLS_LAST];
};

#endif // ENABLE_MEMORY_MANAGER

#endif // MEMORY_MANAGER_H
