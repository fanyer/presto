/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_NODEITERATOR_H
#define DOM_NODEITERATOR_H

#ifdef DOM2_TRAVERSAL

#include "modules/dom/src/domtraversal/traversal.h"
#include "modules/dom/src/domobj.h"

class DOM_NodeIterator_State;
class DOM_NodeIteratorMutationListener;
class DOM_Document;

class DOM_NodeIterator
	: public DOM_Object,
	  public DOM_TraversalObject
{
protected:
	DOM_NodeIterator();
	~DOM_NodeIterator();

	DOM_Node *reference;
	DOM_Node *current_candidate;
	DOM_NodeIterator_State *state;
	DOM_NodeIteratorMutationListener *listener;
	BOOL forward;

	BOOL reference_node_changed_by_filter;

	OP_STATUS NextFrom(HTML_Element* element, BOOL forward, DOM_Document* owner_document, DOM_Node *&node);

public:
	static OP_STATUS Make(DOM_NodeIterator *&node_iterator, DOM_EnvironmentImpl *environment, DOM_Node *root,
	                      unsigned what_to_show, ES_Object *node_filter, BOOL entity_reference_expansion);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_NODEITERATOR || DOM_Object::IsA(type); }
	virtual void GCTrace();

	void Detach();

	OP_STATUS ElementRemoved(HTML_Element* elm);
	OP_STATUS MoveMonitor();

	DOM_DECLARE_FUNCTION(detach);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

	DOM_DECLARE_FUNCTION_WITH_DATA(move); // nextNode, previousNode
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };
};

#endif // DOM2_TRAVERSAL
#endif // DOM_NODEITERATOR_H
