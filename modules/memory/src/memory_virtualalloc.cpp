/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement block allocations using VirtualAlloc for MSWIN
 * 
 * Implement the OpMemPools methods that relies on the OS to perform
 * allocations and deallocations of the pools.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef WINGOGI
//
// Example implementation of the VirtualAlloc code, used to allocate 4K blocks
// dynamically.
//

#ifdef MEMORY_POOLING_ALLOCATORS

void* OpMemPools_VirtualCreate(unsigned int bytes);
void* OpMemPools_VirtualAlloc(void* ptr, unsigned int bytes);

#define MAX_SIZE_MB		25

unsigned int OpMemPools::AddrToBlocknum(void* ptr)
{
	OP_ASSERT(mmap_addr != 0);

	char* a = reinterpret_cast<char*>(ptr);
	if ( a < mmap_addr || a >= (mmap_addr + mmap_size) )
		return 0;

	return ((a - mmap_addr) >> MEMORY_BLOCKSIZE_BITS) + 1;
}

void* OpMemPools::BlocknumToAddr(unsigned int blocknum)
{
	if ( blocknum == 0 )
		return 0;

	char* a = mmap_addr + ((blocknum - 1) << MEMORY_BLOCKSIZE_BITS);

	return reinterpret_cast<void*>(a);
}

void OpMemPools::PlatformInit(void)
{
	OP_ASSERT(MEMORY_BLOCKSIZE == (1<<MEMORY_BLOCKSIZE_BITS));

    mmap_addr = (char*)OpMemPools_VirtualCreate(MAX_SIZE_MB*1024*1024);

	mmap_size = 0;

	OP_ASSERT(mmap_addr != 0);
}

unsigned int OpMemPools::AllocBlock(void)
{
	void* a = OpMemPools_VirtualAlloc(mmap_addr + mmap_size, MEMORY_BLOCKSIZE);

	if ( a != 0 )
		mmap_size += MEMORY_BLOCKSIZE;

	return AddrToBlocknum(a);
}

#endif // MEMORY_POOLING_ALLOCATORS

#endif // WINGOGI
