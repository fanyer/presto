/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef ENABLE_MEMORY_DEBUGGING
/* This function is needed to implement the OPGETCALLSTACK and OPCOPYCALLSTACK
 * macros used to capture call-stack information.  See ../../system.h
 */
extern "C" void OpCopyCallStack(UINTPTR* dst, void* pc, void* fp, int size)
{
	// Store the return address first.  This one should always be correct
	dst[0] = reinterpret_cast<UINTPTR>(pc);
	int k = 1;

#ifdef _DEBUG
	// Check if the frame pointer looks correct:
	int a_stack_variable;
	char* var_addr = (char*)&a_stack_variable;
	char* frame_pointer = (char*)fp;

	// I don't know why, but QRegion::QRegion (a constructor in Qt) sometimes
	// causes fp to be 0x7fffffffffff, crashing this whole thing. Testing for 0x7fffffffffff causes
	// it not to crash.
	//
	// This might be a really freaky bug in Qt, but
	// since I prefer just continuing my work to hunting weird stack-bugs in Qt,
	// just test for 0x7fffffffffff. (arjanl 20080716)
	if ( fp != (void*)((unsigned long)-1 >> 17) && var_addr < frame_pointer && frame_pointer < var_addr + 20480 )
	{
		// Frame pointer looks ok - Walk the stack frames to get
		// the return addresses
		while ( k < size )
		{
			void* new_fp = *(void**)fp;
			char* ofp = (char*)fp;
			char* nfp = (char*)new_fp;
			if ( ofp > nfp || ofp + 20480 < nfp )
				break;
			fp = new_fp;
			dst[k++] = (UINTPTR)*(((void**)fp) + 1);
		}
	}
#else
	// Debugging is not available
#endif

	while ( k < size )
		dst[k++] = 0;
}
#endif // ENABLE_MEMORY_DEBUGGING
