/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author{Morten Rolland <mortenro@opera.com>, Ole Jørgen Anfindsen <olejorgen@opera.com>}
 */

/*
OpProbeGraph:

OpProbeGraph contains the full runtime graph across threads and spaces.
Every probe relation is stored in the graph, and all recorded measurement data is
stored in the graph. Because of this, only a single OpProbeGraph is needed.
*/

#ifndef _PROBETOOLS_PROBEGRAPH_H
#define _PROBETOOLS_PROBEGRAPH_H

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probeidentifiers.h"
#include "modules/probetools/generated/probedefines.h"
#include "modules/probetools/src/probetimer.h"

/*
OpProbeEdge:

Representing an edge in the probe graph. Since a probes
measurement is the sum of its incoming edges, our probegraph only
contain edges and no probes. Each edge only represent a single
parent-child-probe relation, so post-calculation is needed.
*/
class OpProbeEdge {

	OP_ALLOC_POOLING
	OP_ALLOC_POOLING_SMO_PERSISTENT

private:
	OpProbeEdgeIdentifier edge_id;

	OpProbeTimestamp	 edge_self_time;
	OpProbeTimestamp	 edge_children_time;
	OpProbeTimestamp	 edge_overhead_time;
	unsigned int		 edge_total_count;		//including any recursive call
	OpProbeTimestamp	 edge_recursive_self_time;
	OpProbeTimestamp	 edge_recursion_initiated;
	unsigned int		 edge_recursive_count;
	OpProbeTimestamp	 edge_max_total_time;	//max sum of children and self

public:
	OpProbeEdge();
	OpProbeEdge(OpProbeEdgeIdentifier edge_id);
	void clear(void);
	OpProbeEdgeIdentifier& get_id(void);

	void count_edge(			OpProbeTimestamp& a_edge_total_time,
								OpProbeTimestamp& a_edge_self_time,
								OpProbeTimestamp& a_edge_recursion_initiated,
								OpProbeTimestamp& a_edge_recursive_child_compansation);//compensate child counting in recursive context
	void count_recursive_edge(	OpProbeTimestamp& a_edge_recursive_self_time);
	void add_overhead(			OpProbeTimestamp& a_edge_overhead);

	OpProbeTimestamp get_edge_total_time(){		return  get_edge_self_time() +  get_edge_children_time();}
	OpProbeTimestamp get_edge_self_time(){		return edge_self_time;}
	OpProbeTimestamp get_edge_children_time(){	return edge_children_time;}
	OpProbeTimestamp get_edge_overhead_time(){	return edge_overhead_time;}
	OpProbeTimestamp get_edge_max_total_time(){ return edge_max_total_time;}
	unsigned int get_edge_total_count(){		return edge_total_count;}
	OpProbeTimestamp get_edge_recursive_self_time(){ return  edge_recursive_self_time;}
	OpProbeTimestamp get_edge_recursion_initiated_time(){ return  edge_recursion_initiated;}
	unsigned int get_edge_recursive_count(){	return  edge_recursive_count;}
	
}; // OpProbeEdge

/*
OpProbeEdgeHash:

OpProbeEdgeHash stores probes in a fast accessible manner.
Currently this is solved by a hash, but ANY method may be
implemented as long as find_edge + find_next_edge is part
of the interface. Separated out in its one class since 
implementation is irrelevant for OpProbeGraph.

OpProbeEdgeHash may run out of slots, so space hash could
need to be bigger.
*/

//#define OP_PROBE_EDGE_HASH_DEBUG

class OpProbeEdgeHash {
private:
	OpProbeEdge** edgeArray;
	unsigned int edgeArraySize;
	unsigned int edgeArrayUsage;
	
	OpProbeEdge oom_edge;

#ifdef OP_PROBE_EDGE_HASH_DEBUG
		long debug_hitCount;
		long debug_hitTries;
		long debug_usage;
		long debug_maxTries;
		long debug_maxTriesCount;
		long debug_trigger_oom_after_a_while;//so that we can fake probe oom
#endif

public:
	OpProbeEdgeHash();
	~OpProbeEdgeHash();

	OpProbeEdge* find_edge(OpProbeEdgeIdentifier& edge_id);
	OpProbeEdge* find_next_edge(OpProbeEdge* edge = 0);//iterate trough model, will return next in ungiven order, return 0 = end, input 0 = first

	// If we have OOM, and can't expand our probe-graph so we will record everythin
	// on a special oom edge which was alocated early on.
	OpProbeEdge* get_oom_edge();

private:
	unsigned int hash_edge(OpProbeEdgeIdentifier& edge_id);

}; // OpProbeEdgeHash

/*
OpProbeGraph

The main class holding the full cross-thread representaiton of our measured graph.
*/
class OpProbeGraph {
private:
	OpProbeEdgeHash edgeHash;
	
public:
	struct { // This contains data related to each of the probe points.
		int		level;
		const char*	name;
		bool	parameterized;//does this probe expect parameters?
	} probe[OP_PROBE_COUNT];

	OpProbeGraph();
	~OpProbeGraph();

	OpProbeEdge* find_edge(OpProbeEdgeIdentifier& edge_id);
	OpProbeEdge* find_next_edge(OpProbeEdge* edge = 0);//iterate trough model

	// If we have OOM, and can't expand our probe-graph so we will record everythin
	// on a special oom edge which was alocated early on.
	OpProbeEdge* get_oom_edge();

}; // OpProbeGraph

#endif // SUPPORT_PROBETOOLS
#endif // _PROBETOOLS_PROBEGRAPH_H
