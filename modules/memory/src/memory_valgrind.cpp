/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * By Morten Rolland, mortenro@opera.com
 */

/** \file 
 *
 * \brief Implement gateway functions for interacting with valgrind
 *
 * In order to get the best possible results when Opera is run under
 * valgrind to help find e.g. memory errors, Opera can interact with
 * valgrind to make best possible use of it.
 *
 * \author Morten Rolland, mortenro@opera.com
 *
 * The gateway functions defined here uses the valgrind.h header file
 * to perform the interaction with valgrind through use of "magic
 * instruction sequences".
 *
 * The reason for using this wrapper layer is to try avoid including
 * valgrind.h file (named memory_valgrind.h and its companion
 * memory_valgrind_memcheck.h) directly outside of the memory module.
 * The memory module will include these files directly when accessing
 * functionality that shouldn't be needed outside of the memor module.
 */

#include "core/pch.h"

#ifdef VALGRIND

#include "modules/memory/src/memory_valgrind.h"
#include "modules/memory/src/memory_valgrind_memcheck.h"

int op_valgrind_used(void)
{
	// Return 0 when running natively, and nonzero when running under valgrind
	return RUNNING_ON_VALGRIND;
}

void op_valgrind_set_defined(void* addr, int bytes)
{
	if ( RUNNING_ON_VALGRIND )
		VALGRIND_MAKE_MEM_DEFINED(addr, bytes);
}

void op_valgrind_set_undefined(void* addr, int bytes)
{
	if ( RUNNING_ON_VALGRIND )
		VALGRIND_MAKE_MEM_UNDEFINED(addr, bytes);
}

#endif // VALGRIND
