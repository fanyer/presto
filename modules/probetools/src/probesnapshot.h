/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** \author { Anders Oredsson <anderso@opera.com> }
*/

/*
OpSnaptshot:

When probe measurement is done, and you want to examine the probe-results,
for example print it to a file, you take a "snapshot" of the current probe
state, and then you work on that snapshot instead of working on the real
probes.
*/

#ifndef _PROBETOOLS_PROBESNAPSHOT_H
#define _PROBETOOLS_PROBESNAPSHOT_H

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probeidentifiers.h"

/*
OpProbeSnapshotMeasurement:

Class holding measurement either for prbe or edge
*/
class OpProbeSnapshotMeasurement{
private:
	
	double value_total_time;//self time + child time (not needed?)
	double value_self_time;//self time
	double value_children_time;//child time (adjusted for recursion)
	double value_overhead_time;//overhead
	double value_max_total_time;//max self time for singel probe
	unsigned int value_total_count;//total count (inkl recursive)

	double value_recursive_self_time;//self recursive time
	double value_recursion_initiated_time;//recursive time initiated by children
	unsigned int value_recursive_count;//total recursive count

	const char*  ref_name;

public:

	//setup
	OpProbeSnapshotMeasurement();
	~OpProbeSnapshotMeasurement();

	//data
	double get_total_time(){return value_total_time;};
	double get_self_time(){return value_self_time;};
	double get_children_time(){return value_children_time;};
	double get_overhead_time(){return value_overhead_time;};
	double get_max_total_time(){return value_max_total_time;};
	unsigned int get_total_count(){return value_total_count;};
	double get_recursive_self_time(){return value_recursive_self_time;};
	double get_recursion_initiated_time(){return value_recursion_initiated_time;};
	unsigned int get_recursive_count(){return value_recursive_count;};
	const char*  get_name(){return ref_name;};

	//population
	void zero();
	void add_total_time(double t){value_total_time += t;};
	void add_self_time(double t){value_self_time += t;};
	void add_children_time(double t){value_children_time += t;};
	void add_overhead_time(double t){value_overhead_time += t;};
	void set_max_total_time(double t){if(t > value_max_total_time){value_max_total_time = t;}};
	void add_total_count(unsigned int c){value_total_count += c;};

	void add_recursive_self_time(double t){value_recursive_self_time += t;};
	void add_recursion_initiated_time(double t){value_recursion_initiated_time += t;};
	void add_recursive_count(unsigned int c){value_recursive_count += c;};
	void set_name(const char* n){ref_name = n;};
};

/*
OpProbeSnapshotEdge:

Class representing a probe edge
*/
class OpProbeSnapshotEdge{
private:
	OpProbeEdgeIdentifier	   edge_id;
	int						   level;
	OpProbeSnapshotMeasurement measurement;
	OpProbeSnapshotEdge*	   next;		//for linked list
public:

	//setup
	OpProbeSnapshotEdge(OpProbeEdgeIdentifier& id);
	~OpProbeSnapshotEdge();
	void set_level(int a_level){level = a_level;};

	//data
	OpProbeEdgeIdentifier& get_id(){return edge_id;}
	int get_level(){return level;}
	OpProbeSnapshotMeasurement* get_measurement(){return &measurement;}

	//tree
	OpProbeSnapshotEdge* get_next(){return next;}

	//population
	OpProbeSnapshotEdge* add_next(OpProbeEdgeIdentifier& id);//adds new probe in chain (returns 0 on OOM)
};

/*
OpProbeSnapshotProbe:

Class representing a probe
*/
class OpProbeSnapshotProbe{
private:
	OpProbeIdentifier		   probe_id;
	int						   level;
	OpProbeSnapshotMeasurement measurement;
	OpProbeSnapshotEdge*	   incoming_edge;	//probes that call us
	OpProbeSnapshotEdge*	   outgoing_edge;	//probes that we call
	OpProbeSnapshotProbe*	   next;			//for linked list
public:
	
	//setup
	OpProbeSnapshotProbe(OpProbeIdentifier& id);
	~OpProbeSnapshotProbe();
	void set_level(int a_level);

	//data
	OpProbeIdentifier& get_id(){return probe_id;}
	int get_level(){return level;}
	OpProbeSnapshotMeasurement* get_measurement(){return &measurement;}
	
	//tree
	OpProbeSnapshotEdge* get_first_incoming_edge(){return incoming_edge;}
	OpProbeSnapshotEdge* get_first_outgoing_edge(){return outgoing_edge;}
	OpProbeSnapshotProbe* get_next(){return next;}
	int					 get_incoming_edge_count();
	int					 get_outgoing_edge_count();

	//population
	OpProbeSnapshotEdge* get_incoming_edge(OpProbeEdgeIdentifier& id);//creates new edge if needed (returns 0 on OOM)
	OpProbeSnapshotEdge* get_outgoing_edge(OpProbeEdgeIdentifier& id);//creates new edge if needed (returns 0 on OOM)
	OpProbeSnapshotProbe* add_next(OpProbeIdentifier& id);//adds new probe in chain (returns 0 on OOM)
};

/*
OpProbeSnapshot:

Class holding a full probe snapshot.
Future diff-functionality is expected so that
a certain periode of time can be measured.
*/
class OpProbeSnapshot{
private:
	OpProbeSnapshotProbe*		probes;
	OpProbeSnapshotMeasurement	oom_edge_measurement;
public:
	
	//setup
	OpProbeSnapshot();
	~OpProbeSnapshot();
	
	//tree
	OpProbeSnapshotProbe* get_first_probe(){return probes;}
	int					  get_probe_count();
	
	//oom
	OpProbeSnapshotMeasurement* get_oom_measurement(){return &oom_edge_measurement;};

	//population
	OpProbeSnapshotProbe* get_probe(OpProbeIdentifier& id);//creates new probe if needed (returns 0 on OOM)

	OpProbeSnapshotEdge* get_edge(OpProbeIdentifier& parent_id, OpProbeIdentifier& child_id);//creates new edge + probe if needed (returns 0 on OOM)
	OpProbeSnapshotEdge* get_edge(OpProbeEdgeIdentifier& id);//creates new edge + probe if needed (returns 0 on OOM)
	
};

/*
OpProbeSnapshotBuilder:

Builds the snapshot from a real running OpProbeGraph.
*/
class OpProbeGraph;
class OpProbeEdge;

class OpProbeSnapshotBuilder{
public:
	static OpProbeSnapshot* build_probetools_snapshot(OpProbeGraph* a_probe_graph);// Builds a snapshot from a probegraph (returns 0 on OOM)
private:
	static OP_STATUS populate_parent_probe(OpProbeSnapshotProbe* probe, OpProbeEdge* edge);// Returns OOM if OOM
	static OP_STATUS populate_child_probe(OpProbeSnapshotProbe* probe, OpProbeEdge* edge);// Returns OOM if OOM

	static void populate_measurement(OpProbeSnapshotMeasurement* m, OpProbeEdge* edge);
};

#endif //SUPPORT_PROBETOOLS
#endif //_PROBETOOLS_PROBESNAPSHOT_H
