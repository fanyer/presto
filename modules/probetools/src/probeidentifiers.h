/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** \author { Morten Rolland <mortenro@opera.com> , Ole Jørgen Anfindsen <olejorgen@opera.com> , Anders Oredsson <anderso@opera.com> }
*/

#ifndef _PROBETOOLS_PROBEIDENTIFIERS_H
#define _PROBETOOLS_PROBEIDENTIFIERS_H

#ifdef SUPPORT_PROBETOOLS

/*
OpProbeIdentifier:

Wrapper for probe identification,
currently identification contains location and index_parameter
*/
class OpProbeIdentifier{
private:
	unsigned int location;
	int			 index_parameter;
public:

	//setup
	OpProbeIdentifier(){location = 0; index_parameter = 0;}
	OpProbeIdentifier(unsigned int loc, int param){
		location = loc;
		index_parameter = param;
	}

	//equality
	inline bool equals(const OpProbeIdentifier& p) const {
		return p.location == location && p.index_parameter == index_parameter;
	}
	inline bool equals_location(const OpProbeIdentifier& p) const {
		return p.location == location;
	}

	//getters
	inline unsigned int get_location() const {return location;}
	inline int get_index_parameter() const {return index_parameter;}
};

/*
OpProbeIdentifier:

Wrapper for edge identification,
Child and parent probe identification can easely be extracted.
*/
class OpProbeEdgeIdentifier{
private:
	unsigned int edge_id;
	int			 parent_index_parameter;
	int			 child_index_parameter;
public:

	//setup
	OpProbeEdgeIdentifier(){edge_id=0;parent_index_parameter=0;child_index_parameter=0;}
	OpProbeEdgeIdentifier(unsigned int p_loc,unsigned int c_loc,int p_param, int c_param){
		edge_id = OpProbeEdgeIdentifier::parent_and_child_to_edge_id(p_loc, c_loc);
		parent_index_parameter = p_param;
		child_index_parameter = c_param;
	}
	OpProbeEdgeIdentifier(const OpProbeIdentifier& p, const OpProbeIdentifier& c){
		edge_id = OpProbeEdgeIdentifier::parent_and_child_to_edge_id(p.get_location(), c.get_location());
		parent_index_parameter = p.get_index_parameter();
		child_index_parameter = c.get_index_parameter();
	}

	//equality
	inline bool equals(const OpProbeEdgeIdentifier& e) const {
		return e.edge_id == edge_id && e.parent_index_parameter == parent_index_parameter && e.child_index_parameter == child_index_parameter;
	}
	inline bool equals_edge(const OpProbeEdgeIdentifier& e) const {
		return e.edge_id == edge_id;
	}
	inline bool equals_parent(const OpProbeIdentifier& p) const {
		return
			p.get_location() == get_parent_location()
			&& p.get_index_parameter() == get_parent_index_parameter();
	}
	inline bool equals_child(const OpProbeIdentifier& p) const {
		return
			p.get_location() == get_child_location()
			&& p.get_index_parameter() == get_child_index_parameter();
	}

	//getters
	inline unsigned int get_edge_location() const {return edge_id;}
	inline unsigned int get_parent_location() const {return OpProbeEdgeIdentifier::edge_id_to_parent_location(edge_id);}
	inline int			get_parent_index_parameter() const {return parent_index_parameter;}
	inline unsigned int get_child_location() const {return OpProbeEdgeIdentifier::edge_id_to_child_location(edge_id);}
	inline int			get_child_index_parameter() const {return child_index_parameter;}
	inline OpProbeIdentifier get_parent_id() const {return OpProbeIdentifier(get_parent_location(), get_parent_index_parameter());}
	inline OpProbeIdentifier get_child_id() const {return OpProbeIdentifier(get_child_location(), get_child_index_parameter());}

private:
	//idconversion
	inline static unsigned int parent_and_child_to_edge_id(unsigned int p ,unsigned int c){return (((p)<<16)|(c));}
	inline static unsigned int edge_id_to_parent_location(int e){return (((e)>>16)&0xffff);}
	inline static unsigned int edge_id_to_child_location(int e){return ((e)&0xffff);}
};

#endif // SUPPORT_PROBETOOLS
#endif // _PROBETOOLS_PROBEIDENTIFIERS_H
