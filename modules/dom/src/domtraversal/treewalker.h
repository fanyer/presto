/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_TREEWALKER_H
#define DOM_TREEWALKER_H

#ifdef DOM2_TRAVERSAL

#include "modules/dom/src/domtraversal/traversal.h"
#include "modules/dom/src/domobj.h"

class DOM_TreeWalker_State;

class DOM_TreeWalker
	: public DOM_Object,
	  public DOM_TraversalObject
{
protected:
	DOM_TreeWalker();

	DOM_Node *current_node;
	DOM_Node *current_candidate;
	DOM_Node *best_candidate;
	DOM_TreeWalker_State *state;

public:
	enum Operation
	{
		PARENT_NODE,
		FIRST_CHILD,
		LAST_CHILD,
		PREVIOUS_SIBLING,
		NEXT_SIBLING,
		PREVIOUS_NODE,
		NEXT_NODE
	};

	static OP_STATUS Make(DOM_TreeWalker *&node_iterator, DOM_EnvironmentImpl *environment, DOM_Node *root,
	                      unsigned what_to_show, ES_Object *node_filter, BOOL entity_reference_expansion);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TREEWALKER || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION_WITH_DATA(move); // parentNode, {first,last}Child, {next,previous}{Sibling,Node}
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 8 };
};

#endif // DOM2_TRAVERSAL
#endif // DOM_TREEWALKER_H
