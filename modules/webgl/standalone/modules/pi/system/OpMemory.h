/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_SYSTEM_OPMEMORY_H
#define PI_SYSTEM_OPMEMORY_H

class OpMemory
{
public:
#ifdef OPMEMORY_EXECUTABLE_SEGMENT

	enum OpMemSegmentType
	{
		MEMORY_SEGMENT_EXECUTABLE,
		MEMORY_SEGMENT_UNUSED // Can be used by platform implementation
	};

	struct OpMemSegment
	{
		void* address; ///< Pointer to the memory segment
		size_t size; ///< Size of memory segment in bytes
		OpMemSegmentType type; ///< Actual type of the memory segment
	};

	static void DestroySegment(const OpMemSegment* mseg);
	static const OpMemSegment* CreateExecSegment(size_t minimum);
	static void* WriteExec(const OpMemSegment* mseg, void* ptr, size_t size);
	static OP_STATUS WriteExecDone(const OpMemSegment* mseg, void* ptr, size_t size);

#endif // OPMEMORY_EXECUTABLE_SEGMENT
};

#endif // PI_SYSTEM_OPMEMORY_H
