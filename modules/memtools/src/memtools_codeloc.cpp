/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 * \brief Implement OpCodeLocation and OpCodeLocationManager
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef MEMTOOLS_ENABLE_CODELOC

#include "modules/memtools/memtools_codeloc.h"
#include "modules/memory/src/memory_state.h"

static OpCodeLocationManager* get_manager(void)
{
	OpMemoryStateInit();  // Just to be sure

	OpCodeLocationManager* clm = g_memtools_codelocmgr;

	if ( clm == 0 )
		clm = g_memtools_codelocmgr = OpCodeLocationManager::create();

	return clm;
}

const char* OpCodeLocation::GetFileLine(void)
{
	if ( file == 0 )
	{
		g_memtools_codelocmgr->Sync();
		if ( file == 0 )
			return "internal-error:1";
	}

	return file;
}

const char* OpCodeLocation::GetFunction(void)
{
	if ( func == 0 )
	{
		g_memtools_codelocmgr->Sync();
		if ( func == 0 )
			return "internal-error(void)";
	}

	return func;
}

OpCodeLocationManager::OpCodeLocationManager(void) :
	init_ok(FALSE),
	delay_init(FALSE),
	lazy_init(FALSE),
	initialized_list(0),
	initialized_list_last(0),
	uninitialized_list(0),
	uninitialized_list_last(0)
{
	for ( int k = 0; k < CODE_LOCATION_HASHTABLE_SIZE; k++ )
		hashtable[k] = 0;
}

OpCodeLocationManager::~OpCodeLocationManager(void)
{
	// FIXME: Should clean up here
}

OpCodeLocation* OpCodeLocationManager::GetCodeLocation(UINTPTR addr)
{
	OpCodeLocationManager* clm = get_manager();

	// FIXME: Return a static "OOM" object instead
	if ( clm == 0 )
		return 0;

	UINT32 idx = addr % CODE_LOCATION_HASHTABLE_SIZE;
	OpCodeLocation* loc_search;
	OpCodeLocation* loc_new;

	loc_search = clm->hashtable[idx];

	// See if we can find the address in the hashtable
	if ( loc_search != 0 )
	{
		for (;;)
		{
			if ( loc_search->addr == addr )
				return loc_search;
			if ( loc_search->hash_next == 0 )
				break;
			loc_search = loc_search->hash_next;
		}
	}

	// Not found, create a new entry at the end of the hash-list
	loc_new = clm->create_location(addr);
	if ( loc_search != 0 )
		loc_search->hash_next = loc_new;
	else
		clm->hashtable[idx] = loc_new;
	loc_new->hash_next = 0;
	loc_new->internal_next = 0;

	// Add the new object into the uninitialized list at the end
	if ( clm->uninitialized_list == 0 )
	{
		clm->uninitialized_list = loc_new;
		clm->uninitialized_list_last = loc_new;
	}
	else
	{
		clm->uninitialized_list_last->internal_next = loc_new;
		clm->uninitialized_list_last = loc_new;
	}

	if ( clm->init_ok )
		clm->start_lookup(loc_new);

	return loc_new;
}

void OpCodeLocationManager::Sync(void)
{
	if ( lazy_init )
	{
		// We have to assume that this worked...
		OpStatus::Ignore(internal_initialize());
		lazy_init = FALSE;
	}

	if ( init_ok )
		do_sync();
}

void OpCodeLocationManager::forked(void)
{
	// Do nothing unless implemented by platform code
}

void OpCodeLocationManager::start_lookup(OpCodeLocation*)
{
	// Do nothing unless implemented by platform code
}

int OpCodeLocationManager::poll(void)
{
	// Do nothing unless implemented by platform code
	return 0;
}

void OpCodeLocationManager::do_sync(void)
{
	// Do nothing unless implemented by platform code
}

OpCodeLocation* OpCodeLocationManager::GetNextInitialized(void)
{
	OpCodeLocation* ret = 0;
	OpCodeLocationManager* clm = g_memtools_codelocmgr;

	if ( clm != 0 )
	{
		clm->poll();
		ret = clm->initialized_list;
		if ( ret != 0 )
			clm->initialized_list = ret->GetInternalNext();
	}

	return ret;
}

OP_STATUS OpCodeLocationManager::Init(void)
{
	OpCodeLocationManager* clm = get_manager();

	if ( clm == 0 )
		return OpStatus::ERR;

	if (clm->init_ok)
		return OpStatus::OK;

	if ( clm->delay_init )
	{
		// If delayed initialization has been requested, signal that lazy
		// initialization should be used when needed.

		clm->lazy_init = TRUE;
		return OpStatus::OK;
	}

	OpStatus::Ignore(internal_initialize());
	return OpStatus::OK;
}

OP_STATUS OpCodeLocationManager::InitAndSync()
{
	OpCodeLocationManager* clm = get_manager();
	if ( clm == 0 )
		return OpStatus::ERR;

	RETURN_IF_ERROR(Init());
	clm->Sync();
	return OpStatus::OK;
}

OP_STATUS OpCodeLocationManager::internal_initialize(void)
{
	OpCodeLocationManager* clm = g_memtools_codelocmgr;

	OP_ASSERT(clm != 0);

	if ( OpStatus::IsSuccess(clm->initialize()) )
	{
		g_memtools_codelocmgr->init_ok = TRUE;

		if ( clm->uninitialized_list != 0 )
		{
			// Start lookup of all previous symbols registered, one at a time

			OpCodeLocation* tmp_list = clm->uninitialized_list;

			// Start lookup for first symbol (which we know is there)
			clm->uninitialized_list_last = tmp_list;
			tmp_list = tmp_list->internal_next;
			clm->uninitialized_list_last->internal_next = 0;
			clm->start_lookup(clm->uninitialized_list_last);

			// Add remaining symbols from tmp_list and start lookup on them
			while ( tmp_list != 0 )
			{
				OpCodeLocation* nxt = tmp_list;
				tmp_list = tmp_list->internal_next;

				clm->uninitialized_list_last->internal_next = nxt;
				nxt->internal_next = 0;
				clm->uninitialized_list_last = nxt;

				clm->start_lookup(nxt);
			}				
		}

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

BOOL OpCodeLocationManager::AnnounceCodeLocation(void)
{
	OpCodeLocation* loc = OpCodeLocationManager::GetNextInitialized();

	if ( loc != 0 )
	{
		log_printf("MEM: %p loc %s@%s\n", (void*)loc->GetAddr(),
				   loc->GetFunction(), loc->GetFileLine());
		return TRUE;
	}

	return FALSE;
}

void OpCodeLocationManager::Flush(void)
{
	if (!g_memtools_codelocmgr)
		return;

	g_memtools_codelocmgr->Sync();
	while ( OpCodeLocationManager::AnnounceCodeLocation() )
		;
}

void OpCodeLocationManager::Forked(void)
{
	g_memtools_codelocmgr->forked();
}

void OpCodeLocationManager::SetDelayedSymbolLookup(BOOL delay_lookup)
{
	OpCodeLocationManager* clm = get_manager();

	OP_ASSERT(clm != 0);
	OP_ASSERT(!clm->init_ok);
	clm->delay_init = delay_lookup;
}

#endif // MEMTOOLS_ENABLE_CODELOC
