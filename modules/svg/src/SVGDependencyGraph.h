/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _SVG_DEPENDENCY_GRAPH_
#define _SVG_DEPENDENCY_GRAPH_

#ifdef SVG_SUPPORT

#include "modules/logdoc/htm_elm.h"

#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

class NodeSet;

class NodeSetIterator
{
public:
	NodeSetIterator(const NodeSet* edges) : m_set(edges), m_iterator_pos(0) {}

	OP_STATUS First();
	OP_STATUS Next();

	HTML_Element* GetEdge() const;

private:
	const NodeSet* m_set;
	unsigned int m_iterator_pos;
};

class NodeSet
{
	friend class NodeSetIterator;
public:
	NodeSet() : m_nodes(2) {} // grow by two elements at a time

	OP_STATUS Add(HTML_Element* e)
	{
		if (m_nodes.Find(e) != -1)
			return OpStatus::OK;
		return m_nodes.Add(e);
	}

	OP_STATUS Remove(HTML_Element* e)
	{
		return m_nodes.RemoveByItem(e);
	}

	BOOL Contains(HTML_Element* e)
	{
		return m_nodes.Find(e) != -1;
	}

	unsigned int GetCount() { return m_nodes.GetCount(); }

	BOOL IsEmpty() const { return m_nodes.GetCount() == 0; }

	NodeSetIterator GetIterator() { return NodeSetIterator(this); }

private:
	OpVector<HTML_Element> m_nodes;
};

inline OP_STATUS NodeSetIterator::First()
{
	m_iterator_pos = 0;
	return m_iterator_pos < m_set->m_nodes.GetCount() ? OpStatus::OK : OpStatus::ERR;
}

inline OP_STATUS NodeSetIterator::Next()
{
	m_iterator_pos++;
	return m_iterator_pos < m_set->m_nodes.GetCount() ? OpStatus::OK : OpStatus::ERR;
}

inline HTML_Element* NodeSetIterator::GetEdge() const
{
	return m_set->m_nodes.Get(m_iterator_pos);
}

class SVGDependencyGraph
{
public:
	SVGDependencyGraph() {}
	~SVGDependencyGraph()
	{
		OP_ASSERT((m_targets.GetCount() != 0 && m_dependents.GetCount() != 0) ||
				  (m_targets.GetCount() == 0 && m_dependents.GetCount() == 0));
		Clear();
	}

	OP_STATUS RemoveSubTree(HTML_Element* subtree_root);

	OP_STATUS RemoveTargetNode(HTML_Element* target_node);

	OP_STATUS RemoveNode(HTML_Element* node);

	OP_STATUS AddDependency(HTML_Element* node, HTML_Element* target);

	OP_STATUS GetDependencies(HTML_Element* target, NodeSet** edgelist);

	OP_STATUS AddUnresolvedDependency(HTML_Element* node);
	void ClearUnresolvedDependencies();
	OpHashIterator* GetUnresolvedDependenciesIterator() { return m_has_unresolved_dependencies.GetIterator(); }

	void Clear();

#ifdef _DEBUG
	BOOL IsDependent(HTML_Element* node) { return m_dependents.Contains(node); }
#endif // _DEBUG

#ifdef SVG_DEBUG_DEPENDENCY_GRAPH
	void CheckConsistency(HTML_Element* root);
#endif // SVG_DEBUG_DEPENDENCY_GRAPH

private:
	static OP_STATUS AddReference(OpPointerHashTable<HTML_Element, NodeSet>& table, HTML_Element* node,
								  HTML_Element* target);
	static OP_STATUS RemoveReference(OpPointerHashTable<HTML_Element, NodeSet>& table, HTML_Element* node,
									 HTML_Element* target);
	static void DestroyDeps(NodeSet* edges, OpPointerHashTable<HTML_Element, NodeSet>& ref_table,
							HTML_Element* ref_node);

	// Hash from <things used by someone else> to DependencyInfo (owned)
	OpPointerHashTable<HTML_Element, NodeSet> m_targets;
	// Hash from <elements using other things> to DependencyInfo (owned)
	OpPointerHashTable<HTML_Element, NodeSet> m_dependents;

	OpPointerSet<HTML_Element> m_has_unresolved_dependencies;
};

/**
 * Helper class used to traverse the dependency graph
 */
class SVGGraphTraverser
{
public:
	SVGGraphTraverser(SVGDependencyGraph* dgraph, BOOL add_indirect) :
		m_graph(dgraph), m_add_indirect(add_indirect) {}
	virtual ~SVGGraphTraverser() {}

	OP_STATUS Mark(HTML_Element* elm) { return m_visited.Add(elm); }

	OP_STATUS AddRootPath(HTML_Element* parent);
	OP_STATUS AddSubtree(HTML_Element* elm);

	OP_STATUS Traverse();

	virtual OP_STATUS Visit(HTML_Element* node) = 0;

protected:
	class NodeStack : private OpVector<HTML_Element>
	{
	public:
		OP_STATUS Push(HTML_Element* elm) { return Add(elm); }
		HTML_Element* Pop() { return Remove(GetCount() - 1); }
		BOOL IsEmpty() { return GetCount() == 0; }
	};

	void VisitNeighbours(HTML_Element* elm, NodeStack& affected_nodes);

	OpPointerSet<HTML_Element> m_visited;
	NodeStack m_nodestack;

	SVGDependencyGraph* m_graph;

	BOOL m_add_indirect;
};

#endif // SVG_SUPPORT
#endif // _SVG_DEPENDENCY_GRAPH_
