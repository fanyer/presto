/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johannes Hoff
**
*/

#ifndef SCOPE_DOM_INSPECTOR_H
#define SCOPE_DOM_INSPECTOR_H

#if defined(SCOPE_ECMASCRIPT_DEBUGGER) && defined(DOM_INSPECT_NODE_SUPPORT)

#include "modules/dom/domutils.h"
#include "modules/xmlutils/xmlfragment.h"

class ES_ScopeDebugFrontend;

typedef ES_ScopeDebugFrontend::NodeInfo::Attribute& AttributeType;
typedef OpProtobufMessageVector<ES_ScopeDebugFrontend::NodeInfo::Attribute>& AttributeListType;
typedef OpProtobufMessageVector<ES_ScopeDebugFrontend::NodeInfo>& NodeListType;
typedef OpAutoVector<OpString> EventNameSet;

// Compatibility with old code in core-2.3 and lower

enum TraversalType {
	TRAVERSAL_NODE,
	TRAVERSAL_SUBTREE,
	TRAVERSAL_CHILDREN,
	TRAVERSAL_PARENT_NODE_CHAIN_WITH_CHILDREN
};

OP_STATUS GetDOMNodes(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object *node, TraversalType traversal);

OP_STATUS GetDOMView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object *node, int assumed_depth = -1);
OP_STATUS GetNodeView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object* node);
OP_STATUS GetChildrenView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object* node, DOM_Object** stack = 0);
OP_STATUS GetParentNodeChainView(ES_ScopeDebugFrontend *debugger, NodeListType nodes, DOM_Object* node);

/**
 * Inspects the specified node or object and optionally all its children for event listeners.
 * The name of the event for each listener is then recorded in the set @a names, it will
 * not contain duplicates.
 *
 * @param[out] names Unique event-names will be placed here.
 * @param node The root node or object to start the inspection on, must be non-NULL.
 * @param include_children If @c TRUE then any children of @a node will also be inspected, otherwise only @a node is inspected.
 * @return OpStatus::OK on success, or OpStatus::ERR on failure.
 */
OP_STATUS GetEventNames(EventNameSet &names, DOM_Object *node, BOOL include_children=TRUE);

OP_STATUS SearchDOMNodes(ES_ScopeDebugFrontend *debugger, NodeListType nodes, const DOM_Utils::SearchOptions &options);

#endif // SCOPE_ECMASCRIPT_DEBUGGER

#endif // SCOPE_DOM_INSPECTOR_H
