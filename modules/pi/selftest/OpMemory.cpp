/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#if defined(SELFTEST) && defined(OPMEMORY_EXECUTABLE_SEGMENT)

//
// This simple function is used to test executable memory. It is
// assumed that it can be copied into an executable memory segment,
// and still execute the same function (smallest fibonacci number
// larger or equal to the argument).
//
extern "C" int OpMemory_Test_ExecutableMemory(int arg)
{
	int n1 = 0;
	int n2 = 1;

	for (;;)
	{
		int sum = n1 + n2;
		if ( sum >= arg )
			return sum;
		n1 = n2;
		n2 = sum;
	}
}

extern "C" int OpMemory_Test_ExecutableMemory2(int arg)
{
	return arg + 100;
}

#endif // SELFTEST && OPMEMORY_EXECUTABLE_SEGMENT
