/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probethreadlocalstorage.h"
#include "modules/probetools/src/probe.h"

/*==================================================
OpProbeCallStack
==================================================*/

void OpProbeCallStack::init_with_root_probe(OpProbe* root)
{
	root_probe = root;
	probe_stack_pointer = root_probe;
	pending_destructor_active = 0;
	pending_destructor_edge = 0;
	pending_destructor_parent = 0;
}

OpProbe* OpProbeCallStack::update_stack_pointer(OpProbe* new_stack_pointer) //return old stack pointer
{
	OpProbe* old_stack_pointer = probe_stack_pointer;
	probe_stack_pointer = new_stack_pointer;
	return old_stack_pointer;
}
OpProbeIdentifier& OpProbeCallStack::get_stack_pointer_id(void)
{
	return probe_stack_pointer->get_id();
}
BOOL OpProbeCallStack::verify_probe_stack_pointer(OpProbe* this_should_be_the_probe_stack_pointer)
{
	return probe_stack_pointer==this_should_be_the_probe_stack_pointer;
}
BOOL OpProbeCallStack::verify_probe_stack_pointer_is_root_probe(void)
{
	return probe_stack_pointer == root_probe;
}

void OpProbeCallStack::register_pending_destructor(OpProbeEdge* edge, OpProbe* parent, OpProbeTimestamp& postamble_start)
{
	pending_destructor_active = 1;
	pending_destructor_edge = edge;
	pending_destructor_parent = parent;
	pending_destructor_postamble_start = postamble_start;
}

void OpProbeCallStack::handle_pending_destructor()
{
	if( !pending_destructor_active )
		return;

	/*
	 * Update the pending destructor status, and calculate how much
	 * overhead was present in the last probe destructor.
	 */
	pending_destructor_active = 0;
	OpProbeTimestamp pending_destructor_postamble_stop = pending_destructor_postamble_stop_ts;
	OpProbeTimestamp postamble_time = pending_destructor_postamble_stop - pending_destructor_postamble_start;

	/*
	 * Accumulate the overhead of the previous destructor for the edge,
	 * and the parent.  Note: The parent may be dying, as this method
	 * is called from the destructor and constructor of OpProbe.
	 */
	pending_destructor_edge->add_overhead(postamble_time);
	pending_destructor_parent->add_child_overhead(postamble_time);
}

void OpProbeCallStack::destroy()
{
}

/*==================================================
OpProbeThreadLocalStorage
==================================================*/

void OpProbeThreadLocalStorage::init()
{
	root_probe.init_root_probe();
	callstack.init_with_root_probe(&root_probe);
}

OpProbe* OpProbeThreadLocalStorage::update_stack_pointer(OpProbe* new_stack_pointer) //return old stack pointer
{
	return callstack.update_stack_pointer(new_stack_pointer);
}

OpProbeIdentifier& OpProbeThreadLocalStorage::get_stack_pointer_id(void)
{
	return callstack.get_stack_pointer_id();
}

BOOL OpProbeThreadLocalStorage::verify_probe_stack_pointer(OpProbe* this_should_be_the_probe_stack_pointer)
{
	return callstack.verify_probe_stack_pointer(this_should_be_the_probe_stack_pointer);
}

BOOL OpProbeThreadLocalStorage::verify_probe_stack_pointer_is_root_probe(void)
{
	return callstack.verify_probe_stack_pointer_is_root_probe();
}

void OpProbeThreadLocalStorage::register_pending_destructor(OpProbeEdge* edge, OpProbe* parent, OpProbeTimestamp& postamble_start)
{
	callstack.register_pending_destructor(edge, parent, postamble_start);
}

void OpProbeThreadLocalStorage::handle_pending_destructor()
{
	callstack.handle_pending_destructor();
}

void OpProbeThreadLocalStorage::destroy()
{
	callstack.destroy();
}

#endif // SUPPORT_PROBETOOLS
