/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/probepoints.h"

#include "modules/probetools/src/probe.h"
#include "modules/probetools/src/probehelper.h"

OpProbe::OpProbe(void)
{
}

//Probe constructor without parameter
OpProbe::OpProbe(int lev, unsigned int loc, const char* loc_name, const OpProbeTimestamp &preamble_start) :
	preamble_start_ts(preamble_start),
	level(lev)
{
	// Stamp start of constructor is done by OP_PROBE macro and assigned to preamble_start_ts

	//probe_id + edge_id is updated if param is set.
	probe_id = OpProbeIdentifier(loc,0);

	// If probe is called to early, we ignore it.
	OpProbeHelper* probehelper = g_probetools_helper;
	if ( !probehelper ) {
		probe_id = OpProbeIdentifier(0,0);
		callstack_parent = 0;
		return;
	}

#ifdef PROBETOOLS_LOG_PROBES
	probehelper->add_invocation(preamble_start, loc_name, TRUE);
#endif

	// Get the interesting stuff from the helper
	OpProbeThreadLocalStorage*	tls = probehelper->tls();
	OpProbeGraph* probegraph = probehelper->get_probe_graph();
	if ( !probegraph ) {
		probe_id = OpProbeIdentifier(0,0);
		callstack_parent = 0;
		return;
	}
	probegraph->probe[probe_id.get_location()].level = lev;
	probegraph->probe[probe_id.get_location()].name = loc_name;

	// Run startup logic for probe
	startup_probe(tls);
	
	// Stamp end of constructor is done by OP_PROBE macro
}

//logic for startup of probe
void OpProbe::startup_probe(OpProbeThreadLocalStorage* tls)
{

	// Handle pending destructor
	tls->handle_pending_destructor();

	// Update probe stackpointer
	callstack_parent = tls->update_stack_pointer(this);

	// Find edge id
	if ( callstack_parent != 0 )
		edge_id = OpProbeEdgeIdentifier(callstack_parent->get_id(), get_id());
	else
		edge_id = OpProbeEdgeIdentifier();//root edge
	//probe_id + edge_id is updated if param is set.

	//initiate child activity as zero and register our presense at our parent
	active_child = 0;
	if ( callstack_parent != 0 )
		callstack_parent->active_child = probe_id.get_location();
	
	// Zero values populated by children
	accumulated_children_total_time_registered_from_children.zero();
	accumulated_children_overhead_registered_from_children.zero();
	accumulated_children_recursive_self.zero();
	accumulated_recursive_child_compansation.zero();
}

OpProbe::~OpProbe(void)
{

	// Stamp start of destructor
	postamble_start_ts.timestamp_now();

	// If probe was called to early, we ignore it //IMPORTANT FOR TESTING STRATEGY
	// We also ignore root probes!				  //IMPORTANT FOR TESTING STRATEGY
	OpProbeHelper* probehelper = g_probetools_helper;
	if ( !(probehelper && probe_id.get_location() && probe_id.get_location() != OP_PROBE_ROOT) )
		return;

	// Debug
		if ( !probehelper->tls()->verify_probe_stack_pointer( this ) ) {
			/*
			fprintf(stderr, "ERROR: Messed up profiling stack!!\n");
			fprintf(stderr, "       Suddenly destroying probe 0x%04x\n", probe_id.get_location());
			fflush(stderr);
			fprintf(stderr, "       Should have destroyed probe 0x%04x\n",
				probehelper->tls()->get_stack_pointer_id().get_location());
			fflush(stderr);
			*/
			OP_ASSERT(!"ERROR: Messed up profiling stack!!");
		}

	// Get the interesting stuff from the helper
	OpProbeThreadLocalStorage*	tls = probehelper->tls();
	OpProbeGraph*				probegraph = probehelper->get_probe_graph();

	if (!probegraph)
		return;

#ifdef PROBETOOLS_LOG_PROBES
	probehelper->add_invocation(postamble_start_ts, probegraph->probe[probe_id.get_location()].name, FALSE);
#endif

	// Call shutdown probe code
	shutdown_probe(tls, probegraph);
	
	// Signal for debugging; this instance is dead!
	probe_id = OpProbeIdentifier(0xdead, probe_id.get_index_parameter());

	// Stamp end of destructor
	tls->register_pending_destructor_final_timestamp();
}

//logic for shutdown of probe
void OpProbe::shutdown_probe(OpProbeThreadLocalStorage* tls, OpProbeGraph* probegraph)
{
	// Handle pending destructor
	tls->handle_pending_destructor();

	// Find edge
	OpProbeEdge* edge = probegraph->find_edge(edge_id);

	// Count initital overhead
	OpProbeTimestamp preamble_overhead	= preamble_stop_ts - preamble_start_ts;
	edge->add_overhead(preamble_overhead);

	// Count the probe times
	OpProbeTimestamp edge_total_time	= postamble_start_ts
										- preamble_stop_ts
										- accumulated_children_overhead_registered_from_children;
	OpProbeTimestamp edge_self_time		= edge_total_time
										- accumulated_children_total_time_registered_from_children;
	
	// If we are a recursive probe, count us!
	OpProbe* recursive_parent_probe = find_recursive_immediate_parent(this);
	if(recursive_parent_probe)
	{
		edge->count_recursive_edge(edge_self_time);

		//if recursive, pass along our accumulated_children_recursive_self
		recursive_parent_probe->accumulated_children_recursive_self += edge_self_time + accumulated_children_recursive_self;

		//Clear accumulated_children_recursive_self since we are recursive ourself and have passed it to our r_parent
		accumulated_children_recursive_self.zero();

		//Just pass all our time to our recursive parent, so that its not counted twice
		recursive_parent_probe->accumulated_recursive_child_compansation += edge_total_time;
	}	

	// Count the normal part of us!
	edge->count_edge(edge_total_time, edge_self_time, accumulated_children_recursive_self, accumulated_recursive_child_compansation);

	// Pass stuff to parent
	if(callstack_parent){
		callstack_parent->accumulated_children_total_time_registered_from_children += edge_total_time;
		callstack_parent->accumulated_children_overhead_registered_from_children	 += preamble_overhead + accumulated_children_overhead_registered_from_children; //postamble overhead will be added later
	}

	//Register pending destructor so that overhead can be calculated without any overhead
	tls->register_pending_destructor(edge, callstack_parent, postamble_start_ts);
	
	//Teardown stack
	if ( callstack_parent != 0 )
		callstack_parent->active_child = 0;	// Closed nicely; we're no longer a child
	tls->update_stack_pointer(callstack_parent);
}

//parameter
void OpProbe::set_probe_index_parameter(int index_parameter){
	probe_id = OpProbeIdentifier(probe_id.get_location(), index_parameter);
	edge_id = OpProbeEdgeIdentifier(callstack_parent->get_id(), get_id());
}

void OpProbe::init_root_probe(void)
{
	level = 1;
	callstack_parent = 0;
	probe_id = OpProbeIdentifier(OP_PROBE_ROOT, 0);
	edge_id = OpProbeEdgeIdentifier(0,OP_PROBE_ROOT,0,0);
	active_child = 0;
	
	accumulated_children_total_time_registered_from_children.zero();
	accumulated_children_overhead_registered_from_children.zero();
}

OpProbe* OpProbe::find_recursive_immediate_parent(OpProbe* child)
{
	OpProbe* candidate = child->callstack_parent;
	while ( candidate ) {
		if ( candidate->get_id().equals( child->get_id() ))
			return candidate;
		candidate = candidate->callstack_parent;
	}
	return 0;
}

#endif // SUPPORT_PROBETOOLS
