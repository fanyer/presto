/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement callstack handeling functionality
 *
 * Implement functionality and API declared in \c memory_callstack.cpp
 * for \c OpCallStack and \c OpCallStackManager classes.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef MEMTOOLS_CALLSTACK_MANAGER

#include "modules/memtools/memtools_callstack.h"
#include "modules/memtools/memtools_codeloc.h"

OpCallStack::OpCallStack(const UINTPTR* stk, int size, int id) :
	next(0), size(size), id(id), status(0)
{
	// FIXME: OOM handeling for device-testing needed.
	stack = (UINTPTR*)op_debug_malloc(size * sizeof(UINTPTR));
	op_memcpy(stack, stk, sizeof(UINTPTR) * size);
}

void OpCallStack::AnnounceCallStack(void)
{
	if ( ! status )
	{
		char buffer[2048];	// ARRAY OK 2008-06-06 mortenro
		char tmp[64];		// ARRAY OK 2008-06-06 mortenro

		OP_ASSERT(size <= 80);  // Max 80 * 19 bytes == 1520 bytes in buffer

		if ( size > 0 )
		{
			op_sprintf(buffer, "%p", (void*)stack[0]);
#ifdef MEMTOOLS_ENABLE_CODELOC
			// This will prime the OpCodeLocation database
			OpCodeLocationManager::GetCodeLocation(stack[0]);
#endif
			for ( int k = 1; k < size; k++ )
			{
				op_sprintf(tmp, ",%p", (void*)stack[k]);
				op_strcat(buffer, tmp);
#ifdef MEMTOOLS_ENABLE_CODELOC
				// This will prime the OpCodeLocation database
				OpCodeLocationManager::GetCodeLocation(stack[k]);
#endif
			}
		}
		else
		{
			buffer[0] = 0;
		}

		log_printf("MEM: 0x%x call-stack %s\n", id, buffer);
		status = 1;
	}

#ifdef MEMTOOLS_ENABLE_CODELOC
	OpCodeLocationManager::AnnounceCodeLocation();
	OpCodeLocationManager::AnnounceCodeLocation();
	OpCodeLocationManager::AnnounceCodeLocation();
	OpCodeLocationManager::AnnounceCodeLocation();
	OpCodeLocationManager::AnnounceCodeLocation();
#endif
}

OpCallStackManager::OpCallStackManager(int hashtable_size) :
	hashtable_size(hashtable_size), next_id(1)
{
	// FIXME: OOM handeling for device testing
	hashtable = (OpCallStack**)
		op_debug_malloc(hashtable_size * sizeof(OpCallStack*));
	for ( int k = 0; k < hashtable_size; k++ )
		hashtable[k] = 0;
}

OpCallStack* OpCallStackManager::GetCallStack(const UINTPTR* stack,
											  int size)
{
	OpMemoryStateInit();  // Just to be sure

	OpCallStackManager* csm = g_memtools_callstackmgr;

	if ( csm == 0 )
		csm = g_memtools_callstackmgr = new OpCallStackManager(100003);

	// FIXME: Return a static "OOM" object instead on error
	if ( csm == 0 || csm->hashtable == 0 )
		return 0;

	// Compute a hash-key, and reduce the size of the call-stack if
	// it has empty slots (stop at first NULL address)
	UINTPTR hashkey = 0;
	for ( int k = 0; k < size; k++ )
	{
		if ( stack[k] == 0 )
		{
			if ( k == 0 )
				size = 1;
			else
				size = k;
			break;
		}

#ifdef SIXTY_FOUR_BIT
		hashkey = (hashkey << 17) ^ (hashkey >> 47) ^ stack[k];
#else
		hashkey = (hashkey << 13) ^ (hashkey >> 19) ^ stack[k];
#endif
	}

	int idx = hashkey % csm->hashtable_size;
	OpCallStack* cs = csm->hashtable[idx];
	OpCallStack* last = 0;

	while ( cs != 0 )
	{
		if ( cs->size == size && cs->stack[0] == stack[0]
			 && !op_memcmp(cs->stack, stack, sizeof(UINTPTR) * size) )
			return cs;
		last = cs;
		cs = cs->next;
	}

	int id = csm->next_id++;
	OpCallStack* create = new OpCallStack(stack, size, id);

	// FIXME: Return static OOM object if create is NULL

	if ( last != 0 )
		last->next = create;
	else
		csm->hashtable[idx] = create;

	return create;
}

#endif // MEMTOOLS_CALLSTACK_MANAGER
