/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Include file for OpMemSegment object
 * 
 * The OpMemSegment object represents a segment of memory allocated from
 * the operating system as one block.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_SEGMENT_H
#define MEMORY_SEGMENT_H

#ifdef USE_POOLING_MALLOC

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_pooling.h"

class OpCoreSegment
{
public:
	OpCoreSegment(const OpMemory::OpMemSegment* memseg);
	~OpCoreSegment(void);

	void PoolAllocation(OpMemPool* pool);
	void PoolFree(OpMemPool* pool);

	OpMemory::OpMemSegmentType GetType() const { return type; }
	unsigned int GetID() const { return (unsigned int)(UINTPTR)this; }

	void* operator new(size_t size) OP_NOTHROW { return op_lowlevel_malloc(size); }
	void operator delete(void* ptr) { op_lowlevel_free(ptr); }

private:
	const OpMemory::OpMemSegment* memory_segment;
	unsigned int mempools_total_count;
	unsigned int mempools_in_use_count;
	OpMemory::OpMemSegmentType type;

	void CreatePool(void* pool, char* addr, unsigned int size, OpMemPoolAlignment flag);
};

#endif // USE_POOLING_MALLOC

#endif // MEMORY_SEGMENT_H
