/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * \author { Morten Rolland <mortenro@opera.com>, Ole Jørgen Anfindsen <olejorgen@opera.com>, Anders Oredsson <anderso@opera.com> }
 */
#ifndef _PROBETOOLS_PROBE_H
#define _PROBETOOLS_PROBE_H

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/probepoint_enabling.h"
#include "modules/probetools/src/probeidentifiers.h"
#include "modules/probetools/src/probetimer.h"
class OpProbeThreadLocalStorage;
class OpProbeGraph;

#if ENABLED_OP_PROBE_LOG
# define PROBETOOLS_LOG_PROBES
#endif

/*
OpProbe:

The main OpProbe class. Instances are initiated on the stack around in code, and
constructor/destructor is used to register pre and post times + overhead.

OpProbe has a protected part, so that its possible to test probes without touching the
real probegraph.
*/
class OpProbe {
public:
	OpProbe(int level, unsigned int location, const char* location_name, const OpProbeTimestamp& preamble);
	OpProbe(void);
	~OpProbe(void);
	
	//parameter
	void set_probe_index_parameter(int index_parameter);

	//root probe
	void init_root_probe(void);	///< Special initialization for root probe
	
	//access
	OpProbeIdentifier& get_id(){ return probe_id; }
	void add_child_overhead(OpProbeTimestamp &time){ accumulated_children_overhead_registered_from_children += time; }
	
	//timers
	OpProbeTimestamp preamble_start_ts;		///< When the probe object is created (assigned in ctor)
	OpProbeTimestamp preamble_stop_ts;		///< When the function-body is started (assigned in ctor macro)
	OpProbeTimestamp postamble_start_ts;	///< When probe object cleanup started (assigned in dtor)

protected://timing logic must be testable!

	//logic for startup of probe
	void startup_probe(OpProbeThreadLocalStorage* tls);

	//logic for shutdown of probe
	void shutdown_probe(OpProbeThreadLocalStorage* tls, OpProbeGraph* probegraph);
		
	//set location from testing
	void set_id(OpProbeIdentifier& id){ probe_id = id; }

private:
	
	OpProbe*				callstack_parent;// Last "pushed" OpProbe instance
	OpProbeIdentifier		probe_id;
	OpProbeEdgeIdentifier	edge_id;
	int						level;			// The level we are profiling at right now

	unsigned int			active_child;	// Zero, or the currently active child (FOR DEBUG?)

	//Data from child probes
	OpProbeTimestamp accumulated_children_total_time_registered_from_children;
	OpProbeTimestamp accumulated_children_overhead_registered_from_children;
	OpProbeTimestamp accumulated_children_recursive_self;//from recursive children
	OpProbeTimestamp accumulated_recursive_child_compansation;//from recursive children

	OpProbe* find_recursive_immediate_parent(OpProbe*);	// find parent with identical pid/location
};

#endif // SUPPORT_PROBETOOLS
#endif // _PROBETOOLS_PROBE_H
