/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probegraph.h"

/*==================================================
OpProbeEdge
==================================================*/

OpProbeEdge::OpProbeEdge()
{
	//try to not have an id
	clear();
}
OpProbeEdge::OpProbeEdge(OpProbeEdgeIdentifier id) : edge_id(id)
{
	clear();
}

OpProbeEdgeIdentifier& OpProbeEdge::get_id(void)
{
	return edge_id;
}

void OpProbeEdge::clear(void)
{
	edge_self_time.zero();
	edge_children_time.zero();
	edge_overhead_time.zero();
	edge_total_count = 0;
	edge_recursive_self_time.zero();
	edge_recursion_initiated.zero();
	edge_recursive_count = 0;
	edge_max_total_time.zero();	//max (children + self)
}

void OpProbeEdge::count_edge(OpProbeTimestamp& a_edge_total_time, OpProbeTimestamp& a_edge_self_time, OpProbeTimestamp& a_edge_recursion_initiated, OpProbeTimestamp& a_edge_recursive_child_compansation)
{
	edge_self_time		+= a_edge_self_time;
	edge_children_time	+= a_edge_total_time - a_edge_self_time - a_edge_recursive_child_compansation;
	edge_total_count	+= 1;
	if(a_edge_total_time > edge_max_total_time)
		edge_max_total_time = a_edge_total_time;
	edge_recursion_initiated += a_edge_recursion_initiated;
}

void OpProbeEdge::count_recursive_edge(OpProbeTimestamp& a_edge_recursive_self_time)
{
	edge_recursive_self_time += a_edge_recursive_self_time;
	edge_recursive_count += 1;
}

void OpProbeEdge::add_overhead(OpProbeTimestamp& a_edge_overhead)
{
	edge_overhead_time += a_edge_overhead;
}

/*==================================================
OpProbeEdgeHash
==================================================*/

OpProbeEdgeHash::OpProbeEdgeHash()
{
	edgeArraySize = 1427;
	edgeArrayUsage = 0;
	edgeArray = OP_NEWA(OpProbeEdge*,edgeArraySize);
	if(edgeArray != 0){
		for(unsigned int i=0;i<edgeArraySize;i++){
			edgeArray[i] = 0;
		}
	}

	#ifdef OP_PROBE_EDGE_HASH_DEBUG
		debug_hitCount = 0;
		debug_hitTries = 0;
		debug_usage = 0;
		debug_maxTries = 0;
		debug_maxTriesCount = 0;
		debug_trigger_oom_after_a_while = -1;//no triggering
		debug_trigger_oom_after_a_while = 50;//trigger after 50
	#endif
}
OpProbeEdgeHash::~OpProbeEdgeHash()
{
	if(edgeArray != 0){
		for(unsigned int i=0;i<edgeArraySize;i++){
			if(edgeArray[i])
				OP_DELETE(edgeArray[i]);
		}
		OP_DELETEA(edgeArray);
	}

	#ifdef OP_PROBE_EDGE_HASH_DEBUG
		OP_ASSERT(false);
		//Just a simple restingplace to check out debug numbers!
	#endif
}

OpProbeEdge* OpProbeEdgeHash::find_edge(OpProbeEdgeIdentifier& edge_id){

	#ifdef OP_PROBE_EDGE_HASH_DEBUG
		if(debug_trigger_oom_after_a_while == 0){
			return &oom_edge;
		} else if(debug_trigger_oom_after_a_while > 0){
			debug_trigger_oom_after_a_while -= 1;
		}
	#endif

	//handle OOM!
	if(!edgeArray)
		return &oom_edge;

	unsigned int hash = hash_edge(edge_id);

	if(edgeArrayUsage >= edgeArraySize / 2){
		OP_ASSERT(!"Resizable hash not implemented!");
	}

	#ifdef OP_PROBE_EDGE_HASH_DEBUG
		debug_hitCount += 1;
		unsigned int tryCount = 0;
	#endif

	//look for our edge
	while(edgeArray[hash] != 0){

		#ifdef OP_PROBE_EDGE_HASH_DEBUG
			tryCount += 1;
			debug_hitTries += 1;
		#endif

		if(
			edgeArray[hash]->get_id().equals(edge_id)
		){
			#ifdef OP_PROBE_EDGE_HASH_DEBUG
				if(tryCount > debug_maxTries){
					debug_maxTries = tryCount;
					debug_maxTriesCount = 0;
				}
				if(tryCount == debug_maxTries){
					debug_maxTriesCount += 1;
				}
			#endif

			return edgeArray[hash];
		}
		hash = (hash+1) % edgeArraySize;
	}
	
	//we found an empty slot, this must be first encounter
	edgeArray[hash] = OP_NEW(OpProbeEdge,(edge_id));

	if(edgeArray[hash] != 0){
		edgeArrayUsage += 1;
		
		#ifdef OP_PROBE_EDGE_HASH_DEBUG
			debug_usage += 1;
		#endif

		return edgeArray[hash];
	} else {
		return &oom_edge;
	}
}
OpProbeEdge* OpProbeEdgeHash::find_next_edge(OpProbeEdge* edge)
{//iterate trough model, will return next in ungiven order, return 0 = end, input 0 = first

	unsigned int index = 0;

	//find edge
	if(edge != 0){
		index = hash_edge( edge->get_id() );
		while(!edgeArray[index]->get_id().equals(edge->get_id())){
			index = (index+1) % edgeArraySize;
		}
		//found it, now start at next!
		index++;
	}

	//iterate to next
	while(index<edgeArraySize){
		if(edgeArray[index] != 0){
			return edgeArray[index];
		}
		index++;
	}

	//did not find anything, so we must be at end
	return 0;

}

// If we have OOM, and can't expand our probe-graph so we will record everythin
// on a special oom edge which was alocated early on.
OpProbeEdge* OpProbeEdgeHash::get_oom_edge(){
	return &oom_edge;
}

unsigned int OpProbeEdgeHash::hash_edge(OpProbeEdgeIdentifier& edge_id){
	return
		op_abs(
			(
				(
					(edge_id.get_edge_location() * 379)
					+	edge_id.get_parent_index_parameter()
					+	97
				) * 77
				+ edge_id.get_child_index_parameter()
				+ 13
			) * 7
		) % edgeArraySize;
}

/*==================================================
OpProbeGraph
==================================================*/

OpProbeGraph::OpProbeGraph(void)
{
	for (unsigned int i=0; i<OP_PROBE_COUNT; i++ ) {
		probe[i].level = -1;
		probe[i].name = 0;
		probe[i].parameterized = false;
	}
	probe[1].level = -1;
	probe[1].name  = "OP_PROBE_ROOT";
}

OpProbeGraph::~OpProbeGraph(void)
{
}
OpProbeEdge* OpProbeGraph::find_edge(OpProbeEdgeIdentifier& edge_id){
	return edgeHash.find_edge(edge_id);
}
OpProbeEdge* OpProbeGraph::find_next_edge(OpProbeEdge* edge){//iterate trough model
	return edgeHash.find_next_edge(edge);
}
OpProbeEdge* OpProbeGraph::get_oom_edge(){
	return edgeHash.get_oom_edge();
}

#endif // SUPPORT_PROBETOOLS
