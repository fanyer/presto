/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** \author { Anders Oredsson <anderso@opera.com> }
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probesnapshot.h"
#include "modules/probetools/src/probehelper.h" //because of the probe_loc/edge_id recalc.. should be outsourced

/*==================================================
OpProbeSnapshotMeasurement
==================================================*/

OpProbeSnapshotMeasurement::OpProbeSnapshotMeasurement()
{
	zero();
}
OpProbeSnapshotMeasurement::~OpProbeSnapshotMeasurement()
{
	
}

//population
void OpProbeSnapshotMeasurement::zero()
{
	value_total_time = 0;
	value_self_time = 0;
	value_children_time = 0;
	value_overhead_time = 0;
	value_max_total_time = 0;
	value_total_count = 0;

	value_recursive_self_time = 0;
	value_recursion_initiated_time = 0;
	value_recursive_count = 0;
}

/*==================================================
OpProbeSnapshotEdge
==================================================*/

OpProbeSnapshotEdge::OpProbeSnapshotEdge(OpProbeEdgeIdentifier& id)
{
	edge_id = id;
	measurement.zero();
	next = 0;
}
OpProbeSnapshotEdge::~OpProbeSnapshotEdge()
{
	if(next)
		OP_DELETE(next);
}

//population

//adds new probe in chain (returns 0 on OOM)
OpProbeSnapshotEdge* OpProbeSnapshotEdge::add_next(OpProbeEdgeIdentifier& new_edge_id)
{
	OpProbeSnapshotEdge* new_edge = OP_NEW(OpProbeSnapshotEdge,(new_edge_id));
	if(new_edge){
		new_edge->next = next;
		next = new_edge;
		return new_edge;
	} else {
		return 0; // OOM
	}
}

/*==================================================
OpProbeSnapshotProbe
==================================================*/

OpProbeSnapshotProbe::OpProbeSnapshotProbe(OpProbeIdentifier& id)
{
	probe_id = id;
	measurement.zero();
	incoming_edge = 0;
	outgoing_edge = 0;
	next = 0;
}
OpProbeSnapshotProbe::~OpProbeSnapshotProbe()
{
	if(incoming_edge)
		OP_DELETE(incoming_edge);
	if(outgoing_edge)
		OP_DELETE(outgoing_edge);
	if(next)
		OP_DELETE(next);
}
void OpProbeSnapshotProbe::set_level(int a_level)
{
	level = a_level;
}

//tree
int OpProbeSnapshotProbe::get_incoming_edge_count()
{
	int c = 0;
	OpProbeSnapshotEdge* e = incoming_edge;
	while(e)
	{
		c++; //yay!
		e = e->get_next();
	}
	return c;
}

int	OpProbeSnapshotProbe::get_outgoing_edge_count()
{
	int c = 0;
	OpProbeSnapshotEdge* e = outgoing_edge;
	while(e)
	{
		c++; //yay!
		e = e->get_next();
	}
	return c;
}

//population

//creates new edge if needed (returns 0 on OOM)
OpProbeSnapshotEdge* OpProbeSnapshotProbe::get_incoming_edge(OpProbeEdgeIdentifier& id)
{
	if(incoming_edge){
		OpProbeSnapshotEdge* e = incoming_edge;
		while(!e->get_id().equals(id) && e->get_next() != 0)
			e = e->get_next();
		if(e->get_id().equals(id))
			return e;
		else {
			OP_ASSERT(e->get_next() == 0);
			return e->add_next(id); // returns 0 if OOM
		}
	} else {
		incoming_edge = OP_NEW(OpProbeSnapshotEdge,(id));
		if(incoming_edge){
			return incoming_edge;
		} else { // (else not needed, but clearer)
			return 0; // OOM
		}
	}
}

//creates new edge if needed (returns 0 on OOM)
OpProbeSnapshotEdge* OpProbeSnapshotProbe::get_outgoing_edge(OpProbeEdgeIdentifier& id)
{
	if(outgoing_edge){
		OpProbeSnapshotEdge* e = outgoing_edge;
		while(!e->get_id().equals(id) && e->get_next() != 0)
			e = e->get_next();
		if(e->get_id().equals(id))
			return e;
		else {
			OP_ASSERT(e->get_next() == 0);
			return e->add_next(id); // returns 0 if OOM
		}
	} else {
		outgoing_edge = OP_NEW(OpProbeSnapshotEdge,(id));
		if(outgoing_edge){
			return outgoing_edge;
		} else { // (else not needed, but clearer)
			return 0; // OOM
		}
	}
}

//adds new probe in chain (returns 0 on OOM)
OpProbeSnapshotProbe* OpProbeSnapshotProbe::add_next(OpProbeIdentifier& id)
{
	OpProbeSnapshotProbe* new_probe = OP_NEW(OpProbeSnapshotProbe,(id));
	if(new_probe){
		new_probe->next = next;
		next = new_probe;
		return new_probe;
	} else {
		return 0;// OOM
	}
}

/*==================================================
OpProbeSnapshot
==================================================*/

OpProbeSnapshot::OpProbeSnapshot()
{
	probes = 0;
}
OpProbeSnapshot::~OpProbeSnapshot()
{
	if(probes)
		OP_DELETE(probes);
}

//tree
int	OpProbeSnapshot::get_probe_count()
{
	int c = 0;
	OpProbeSnapshotProbe* p = probes;
	while(p)
	{
		c++;//yay!
		p = p->get_next();
	}
	return c;
}

//population

//creates new probe if needed (returns 0 on OOM)
OpProbeSnapshotProbe* OpProbeSnapshot::get_probe(OpProbeIdentifier& id)
{
	if(probes){
		OpProbeSnapshotProbe* p = probes;
		while(!p->get_id().equals(id) && p->get_next() != 0)
			p = p->get_next();
		if(p->get_id().equals(id))
			return p;
		else {
			OP_ASSERT(p->get_next() == 0);
			return p->add_next(id); // returns 0 if OOM
		}
	} else {
		probes = OP_NEW(OpProbeSnapshotProbe,(id));
		if(probes){
			return probes;
		} else { // (else not needed, but clearer)
			return 0; // OOM
		}
	}
}

OpProbeSnapshotEdge* OpProbeSnapshot::get_edge(OpProbeIdentifier& parent_id, OpProbeIdentifier& child_id)//creates new edge + probe if needed (returns 0 on OOM)
{
	OpProbeEdgeIdentifier edge_id(parent_id, child_id);
	return get_edge(edge_id); // returns 0 if OOM
}

OpProbeSnapshotEdge* OpProbeSnapshot::get_edge(OpProbeEdgeIdentifier& id)//creates new edge + probe if needed (returns 0 on OOM)
{
	OpProbeIdentifier child_id = id.get_child_id();
	OpProbeSnapshotProbe* child_probe = get_probe(child_id);
	return child_probe->get_incoming_edge(id); // returns 0 if OOM
}

/*==================================================
OpProbeSnapshotBuilder
==================================================*/

/*static*/
OpProbeSnapshot* OpProbeSnapshotBuilder::build_probetools_snapshot(OpProbeGraph* a_probe_graph)
{
	//METHOD THAT POPULATES A PROBE-SNAPSHOT FROM A PROBEGRAPH
	//RETURN 0 AND CLEAN UP IF OOM
	
	//create new snapshot
	OpProbeSnapshot* snapshot = OP_NEW(OpProbeSnapshot,());
	if(!snapshot)
		return 0; // OOM

	BOOL OOM_flag = FALSE;
	
	//iterate trough all probes in graph
	OpProbeEdge* iter_edge = a_probe_graph->find_next_edge();
	while(iter_edge != 0){
		
		//each edge will affect two probes
		OpProbeEdgeIdentifier edge_id = iter_edge->get_id();

		//parent probe
		OpProbeIdentifier parent_id = edge_id.get_parent_id();
		OpProbeSnapshotProbe* parent_probe = snapshot->get_probe(parent_id);
		if(!parent_probe || !OpStatus::IsSuccess(populate_parent_probe(parent_probe, iter_edge))){
			OOM_flag = TRUE;
			break;
		}
		
		//child probe
		OpProbeIdentifier child_id = edge_id.get_child_id();
		OpProbeSnapshotProbe* child_probe  = snapshot->get_probe(child_id);
		if(!child_probe || !OpStatus::IsSuccess(populate_child_probe(child_probe, iter_edge))){
			OOM_flag = TRUE;
			break;
		}
		
		iter_edge = a_probe_graph->find_next_edge(iter_edge);
	}

	//If we got OOM, just clean up and exit
	if(OOM_flag){
		//everything created should be 'connected' to our snapshot.
		OP_DELETE(snapshot);
		return 0;
	}

	//populate level values and names
	OpProbeSnapshotProbe* iter_probe = snapshot->get_first_probe();
	while(iter_probe){
		iter_probe->set_level(a_probe_graph->probe[iter_probe->get_id().get_location()].level);
		iter_probe->get_measurement()->set_name(a_probe_graph->probe[iter_probe->get_id().get_location()].name);

		//set name + level on incoming edges
		OpProbeSnapshotEdge* iter_edge = iter_probe->get_first_incoming_edge();
		while(iter_edge){
			unsigned int edge_parent_probe_location = iter_edge->get_id().get_parent_location();
			iter_edge->get_measurement()->set_name(a_probe_graph->probe[edge_parent_probe_location].name);
			iter_edge->set_level(a_probe_graph->probe[edge_parent_probe_location].level);
			iter_edge = iter_edge->get_next();
		}

		//set name + level on outgoing edges
		iter_edge = iter_probe->get_first_outgoing_edge();
		while(iter_edge){
			unsigned int edge_child_probe_location = iter_edge->get_id().get_child_location();
			iter_edge->get_measurement()->set_name(a_probe_graph->probe[edge_child_probe_location].name);
			iter_edge->set_level(a_probe_graph->probe[edge_child_probe_location].level);
			iter_edge = iter_edge->get_next();
		}

		iter_probe = iter_probe->get_next();
	}

	//populate oom_measurement
	populate_measurement(snapshot->get_oom_measurement(), a_probe_graph->get_oom_edge());

	//return our populated snapshot
	return snapshot;
}

/*static*/
OP_STATUS OpProbeSnapshotBuilder::populate_parent_probe(OpProbeSnapshotProbe* probe, OpProbeEdge* edge)
{
	//add outgoing edge
	OpProbeSnapshotEdge* outgoing_edge = probe->get_outgoing_edge(edge->get_id());
	if(!outgoing_edge)
		return OpStatus::ERR_NO_MEMORY;// OOM
	populate_measurement(outgoing_edge->get_measurement(), edge);

	return OpStatus::OK;
}

/*static*/
OP_STATUS OpProbeSnapshotBuilder::populate_child_probe(OpProbeSnapshotProbe* probe, OpProbeEdge* edge)
{
	//add incoming edge
	OpProbeSnapshotEdge* incoming_edge = probe->get_incoming_edge(edge->get_id());//TODO OOM
	if(!incoming_edge)
		return OpStatus::ERR_NO_MEMORY;// OOM
	populate_measurement(incoming_edge->get_measurement(), edge);

	//add data to our probe (summarizing all edges)
	populate_measurement(probe->get_measurement(), edge);
	return OpStatus::OK;
}

/*static*/
void OpProbeSnapshotBuilder::populate_measurement(OpProbeSnapshotMeasurement* m, OpProbeEdge* edge)
{
	m->add_total_time(			edge->get_edge_total_time().get_time()		);
	m->add_self_time(			edge->get_edge_self_time().get_time()		);
	m->add_children_time(		edge->get_edge_children_time().get_time()	);
	m->add_overhead_time(		edge->get_edge_overhead_time().get_time()	);
	m->set_max_total_time(		edge->get_edge_max_total_time().get_time()	);
	m->add_total_count(			edge->get_edge_total_count()				);
	m->add_recursive_self_time(	edge->get_edge_recursive_self_time().get_time());
	m->add_recursion_initiated_time(edge->get_edge_recursion_initiated_time().get_time());
	m->add_recursive_count(		edge->get_edge_recursive_count()			);
}

#endif
