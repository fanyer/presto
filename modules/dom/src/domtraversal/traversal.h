/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_TRAVERSAL_H
#define DOM_TRAVERSAL_H

#ifdef DOM2_TRAVERSAL

class DOM_TraversalObject_State;
class DOM_Node;
class DOM_Runtime;
class ES_Object;
class ES_Value;

class DOM_TraversalObject
{
public:
	void GCTrace();

protected:
	DOM_TraversalObject();

	DOM_Node *root;
	unsigned what_to_show;
	ES_Object *filter;
	BOOL entity_reference_expansion;

	DOM_TraversalObject_State *state;

	int AcceptNode(DOM_Node *node, DOM_Runtime *origining_runtime, ES_Value &exception);
};

#endif // DOM2_TRAVERSAL
#endif // DOM_NODEITERATOR_H
