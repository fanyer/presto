/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT
#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGDependencyGraph.h"

OP_STATUS SVGDependencyGraph::AddReference(OpPointerHashTable<HTML_Element, NodeSet>& table,
										   HTML_Element* node,
										   HTML_Element* target)
{
#ifdef SVG_DEBUG_DEPENDENCY_GRAPH
	OP_NEW_DBG("SVGDependencyGraph::AddReference", "svg_depgraph");
	if(node->GetNsType() != NS_SVG && target->GetNsType() != NS_SVG)
	{
		OP_DBG(("Non-svg dependency (type, ns): (%d %d) -> %s (%d %d)",
			node->Type(), node->GetNsType(), target->Type(), target->GetNsType()
		));
	}
	else if (target->GetNsType() != NS_SVG)
	{
		OP_DBG(("SVG -> non-svg dependency (type, ns): (%d %d) -> %s (%d %d)",
			node->Type(), node->GetNsType(), target->Type(), target->GetNsType()
		));
	}
	else if (node->GetNsType() != NS_SVG)
	{
		OP_DBG(("Non-svg -> SVG dependency (type, ns): (%d %d) -> %s (%d %d)",
			node->Type(), node->GetNsType(), target->Type(), target->GetNsType()
		));
	}
#endif // SVG_DEBUG_DEPENDENCY_GRAPH

	NodeSet* edges = NULL;
	if (OpStatus::IsError(table.GetData(node, &edges)))
	{
		edges = OP_NEW(NodeSet, ());
		if (!edges || OpStatus::IsError(table.Add(node, edges)))
		{
			OP_DELETE(edges);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	OP_ASSERT(edges);

	return edges->Add(target);
}

OP_STATUS SVGDependencyGraph::RemoveReference(OpPointerHashTable<HTML_Element, NodeSet>& table,
											  HTML_Element* node,
											  HTML_Element* target)
{
	NodeSet* edges = NULL;
	RETURN_IF_ERROR(table.GetData(node, &edges));

	// FIXME: Can this happen, considering the above?
	OP_ASSERT(edges);

	if (edges && OpStatus::IsSuccess(edges->Remove(target)))
	{
		OP_STATUS status = OpStatus::OK;
		// Remove node if it has no edges
		if (edges->IsEmpty())
		{
			status = table.Remove(node, &edges);
			OP_DELETE(edges);
		}
		return status;
	}
	return OpStatus::ERR;
}

OP_STATUS SVGDependencyGraph::AddUnresolvedDependency(HTML_Element* node)
{
	return m_has_unresolved_dependencies.Add(node);
}

void SVGDependencyGraph::ClearUnresolvedDependencies()
{
	m_has_unresolved_dependencies.RemoveAll();
}

void SVGDependencyGraph::DestroyDeps(NodeSet* edges, OpPointerHashTable<HTML_Element, NodeSet>& ref_table,
									 HTML_Element* ref_node)
{
	OP_ASSERT(edges); // Don't call this with NULL edges

	// Cleanup refs
	NodeSetIterator ei = edges->GetIterator();

	OP_STATUS status = ei.First();
	while (OpStatus::IsSuccess(status))
	{
		HTML_Element* e = ei.GetEdge();

		OpStatus::Ignore(RemoveReference(ref_table, e, ref_node));

		status = ei.Next();
	}

	OP_DELETE(edges);
}

OP_STATUS SVGDependencyGraph::RemoveSubTree(HTML_Element* subtree_root)
{
	// Remove subtree from the dependency graph
	HTML_Element* it = subtree_root;
	while (it)
	{
		OpStatus::Ignore(RemoveNode(it));
		it = it->Next();
	}
	
	return OpStatus::OK;
}

OP_STATUS SVGDependencyGraph::RemoveNode(HTML_Element* node)
{
	OpStatus::Ignore(m_has_unresolved_dependencies.Remove(node));
	OpStatus::Ignore(RemoveTargetNode(node));

	// Remove from dependents
	NodeSet* edges = NULL;
	if (OpStatus::IsSuccess(m_dependents.Remove(node, &edges)) &&
		edges)
		DestroyDeps(edges, m_targets, node);

	return OpStatus::OK;
}

OP_STATUS SVGDependencyGraph::RemoveTargetNode(HTML_Element* target_node)
{
	NodeSet* edges = NULL;
	RETURN_IF_MEMORY_ERROR(m_targets.Remove(target_node, &edges));

	if (edges)
		DestroyDeps(edges, m_dependents, target_node);

	// FIXME: Add some expensive consistency checks here and there
	return OpStatus::OK;
}

OP_STATUS SVGDependencyGraph::AddDependency(HTML_Element* node, HTML_Element* target)
{
	RETURN_IF_ERROR(AddReference(m_targets, target, node));
	OP_STATUS status = AddReference(m_dependents, node, target);
	if (OpStatus::IsMemoryError(status))
		OpStatus::Ignore(RemoveReference(m_targets, target, node));

	return status;
}

OP_STATUS SVGDependencyGraph::GetDependencies(HTML_Element* target, NodeSet** edgeset)
{
	OP_ASSERT(edgeset);

	NodeSet* edges = NULL;
	RETURN_IF_MEMORY_ERROR(m_targets.GetData(target, &edges));

	if (!edges)
		return OpStatus::ERR;

	*edgeset = edges;
	return OpStatus::OK;
}

void SVGDependencyGraph::Clear()
{
	m_targets.DeleteAll();
	m_dependents.DeleteAll();
	m_has_unresolved_dependencies.RemoveAll();
}

#ifdef SVG_DEBUG_DEPENDENCY_GRAPH
static BOOL IsInTree(HTML_Element* root, HTML_Element* element)
{
	while (root)
	{
		if (root == element)
			return TRUE;
		root = root->Next();
	}
	return FALSE;
}

void SVGDependencyGraph::CheckConsistency(HTML_Element* root)
{
	if (OpHashIterator* iter = m_targets.GetIterator())
	{
		for (OP_STATUS status = iter->First(); OpStatus::IsSuccess(status); status = iter->Next())
		{
			HTML_Element* elm = (HTML_Element*)iter->GetKey();

			// Verify node
			OP_ASSERT(IsInTree(root, elm));

			// Verify all edges
			DependencyInfo* depinfo = (DependencyInfo*)iter->GetData();
			for (unsigned int i = 0; i < depinfo->m_nodes.GetCount(); ++i)
				OP_ASSERT(IsInTree(root, depinfo->m_nodes.Get(i)));
		}
		OP_DELETE(iter);
	}

	if (OpHashIterator* iter = m_dependents.GetIterator())
	{
		for (OP_STATUS status = iter->First(); OpStatus::IsSuccess(status); status = iter->Next())
		{
			HTML_Element* elm = (HTML_Element*)iter->GetKey();

			// Verify node
			OP_ASSERT(IsInTree(root, elm));

			// Verify all edges
			DependencyInfo* depinfo = (DependencyInfo*)iter->GetData();
			for (unsigned int i = 0; i < depinfo->m_nodes.GetCount(); ++i)
				OP_ASSERT(IsInTree(root, depinfo->m_nodes.Get(i)));
		}
		OP_DELETE(iter);
	}
}
#endif // SVG_DEBUG_DEPENDENCY_GRAPH

OP_STATUS SVGGraphTraverser::AddRootPath(HTML_Element* parent)
{
	// Add parents to nodeset
	while (parent)
	{
		RETURN_IF_ERROR(m_nodestack.Push(parent));
		parent = parent->Parent();
	}
	return OpStatus::OK;
}

OP_STATUS SVGGraphTraverser::AddSubtree(HTML_Element* elm)
{
	HTML_Element* stop = static_cast<HTML_Element*>(elm->NextSibling());
	HTML_Element* it = elm;

	// Add all children to nodeset
	while (it != stop)
	{
		// Duplicates not possible here
		RETURN_IF_ERROR(m_nodestack.Push(it));
		it = it->Next();
	}
	return OpStatus::OK;
}

// Visit the neighbours of 'elm' (in the graph), aka 'the nodes that depend on elm'
void SVGGraphTraverser::VisitNeighbours(HTML_Element* elm, NodeStack& affected_nodes)
{
	NodeSet* edgeset = NULL;
	RETURN_VOID_IF_ERROR(m_graph->GetDependencies(elm, &edgeset));

	OP_ASSERT(edgeset);

	NodeSetIterator ei = edgeset->GetIterator();

	for (OP_STATUS status = ei.First();
		 OpStatus::IsSuccess(status);
		 status = ei.Next())
	{
		HTML_Element* depelm = ei.GetEdge();
		OP_ASSERT(depelm); // Don't expect this to happen
		if (!depelm)
			continue;

		// If this node has already been marked we can ignore it
		if (OpStatus::IsError(Mark(depelm)))
			continue;

		if (OpStatus::IsError(Visit(depelm)))
			continue;

		// This node (and possibly the path to it) has been "affected"
		if (OpStatus::IsError(affected_nodes.Push(depelm)))
			continue;

		if (m_add_indirect)
		{
			// Mark path to root
			HTML_Element* parent = depelm->Parent();
			while (parent)
			{
				if (m_visited.Contains(parent))
					break;
				RETURN_VOID_IF_ERROR(affected_nodes.Push(parent));
				parent = parent->Parent();
			}
		}
	}
}

OP_STATUS SVGGraphTraverser::Traverse()
{
	NodeStack affected_nodes;

	NodeStack* modified = &m_nodestack;
	NodeStack* affected = &affected_nodes;

	// Mark dependent nodes
	while (!modified->IsEmpty())
	{
		while (!modified->IsEmpty())
		{
			HTML_Element* elm = modified->Pop();

			// Get affected nodes (and mark them for repaint)
			// (nodes depending on modified nodes, and their parents)
			VisitNeighbours(elm, *affected);
		}

		NodeStack* tmp = affected;
		affected = modified;
		modified = tmp;

		OP_ASSERT(affected->IsEmpty());
	}

	return OpStatus::OK;
}

#endif // SVG_SUPPORT
