/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_STYLESHEET_H
#define DOM_STYLESHEET_H

#include "modules/dom/src/domobj.h"

class DOM_Node;

class DOM_StyleSheet
	: public DOM_Object
{
protected:
	DOM_StyleSheet(DOM_Node *owner_node);

	DOM_Node *owner_node;

public:
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_STYLESHEET || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_Node *GetOwnerNode() { return owner_node; }
};

#endif // DOM_STYLESHEET_H
