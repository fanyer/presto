/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author { Morten Rolland <mortenro@opera.com>, Ole Jørgen Anfindsen <olejorgen@opera.com> }
*/

#ifndef _PROBETOOLS_PROBETHREADLOCALSTORAGE_H
#define _PROBETOOLS_PROBETHREADLOCALSTORAGE_H

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probegraph.h"
#include "modules/probetools/src/probe.h"

/*
OpProbeCallStack:

Contains the current probe callstack, and "pending destructor".
The callstack itself is linked between probes, but OpProbeCallStack
knows about top probe and root probe.

The "pending destructor" works so that a probe does not measure it
own destruction, but the next probe measures it for it. This is important
so that overhead measurements get more precise.
*/
class OpProbeCallStack {
private:
	
	// Each thread has its own chain of probepoints on the stack
	OpProbe* root_probe;			// Parent of objects of this thread
	OpProbe* probe_stack_pointer;	// The active probe instance

	// These are used to include last destructor at a better time
	int pending_destructor_active;
	OpProbeEdge* pending_destructor_edge;
	OpProbe* pending_destructor_parent;
	OpProbeTimestamp pending_destructor_postamble_start;
	OpProbeTimestamp pending_destructor_postamble_stop_ts;

public:
	void init_with_root_probe(OpProbe* root);//for testing!

	OpProbe* update_stack_pointer(OpProbe* new_stack_pointer); //return old stack pointer //TODO: push/pop
	OpProbeIdentifier& get_stack_pointer_id(void);
	BOOL verify_probe_stack_pointer(OpProbe* this_should_be_the_probe_stack_pointer);
	BOOL verify_probe_stack_pointer_is_root_probe(void);

	void register_pending_destructor(OpProbeEdge* edge, OpProbe* parent, OpProbeTimestamp& postamble_start);
	inline void register_pending_destructor_final_timestamp()
	{
		pending_destructor_postamble_stop_ts.timestamp_now();
	}
	void set_pending_destructor_final_timestamp(OpProbeTimestamp& ts)//For testing
	{
		pending_destructor_postamble_stop_ts = ts;
	}

	void handle_pending_destructor();

	void destroy();

}; // OpProbeCallStack

/*
OpProbeThreadLocalStorage:

Every thread / probespace in probetools need its own data.
Currently this data is only a callstack, but when probetools
is made multithreaded, some kind of thread/process identifier
is expected.
*/
class OpProbeThreadLocalStorage {

private:
	
	//Each thread has its own callstack, we will improve this later
	//to be able to have different "spaces" within a thread
	OpProbeCallStack callstack;
	OpProbe			 root_probe;

public:
	void init();

	OpProbe* update_stack_pointer(OpProbe* new_stack_pointer); //return old stack pointer
	OpProbeIdentifier& get_stack_pointer_id(void);
	BOOL verify_probe_stack_pointer(OpProbe* this_should_be_the_probe_stack_pointer);
	BOOL verify_probe_stack_pointer_is_root_probe(void);

	void register_pending_destructor(OpProbeEdge* edge, OpProbe* parent, OpProbeTimestamp& postamble_start);
	inline void register_pending_destructor_final_timestamp()
	{
		callstack.register_pending_destructor_final_timestamp();
	}
	void set_pending_destructor_final_timestamp(OpProbeTimestamp& ts)//For testing
	{
		callstack.set_pending_destructor_final_timestamp(ts);
	}

	void handle_pending_destructor();

	void destroy();

}; // OpProbeThreadLocalStorage

#endif // SUPPORT_PROBETOOLS
#endif // _PROBETOOLS_PROBETHREADLOCALSTORAGE_H
