/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_NODEMAP_H
#define DOM_NODEMAP_H

#include "modules/dom/src/domobj.h"

class DOM_Node;

class DOM_NamedNodeMapImpl
{
public:
	virtual ~DOM_NamedNodeMapImpl();
	virtual void GCTrace();

	virtual int Length() = 0;
	virtual int Item(int index, ES_Value *return_value, ES_Runtime *origining_runtime) = 0;

	virtual int GetNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime) = 0;
	virtual int RemoveNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime) = 0;
	virtual int SetNamedItem(DOM_Node *node, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime) = 0;
};

class DOM_NamedNodeMap
	: public DOM_Object
{
protected:
	DOM_NamedNodeMap(DOM_NamedNodeMapImpl *implementation);

	DOM_NamedNodeMapImpl *implementation;

public:
	static OP_STATUS Make(DOM_NamedNodeMap *&map, DOM_Node *reference, DOM_NamedNodeMapImpl *implementation);

	virtual ~DOM_NamedNodeMap();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_NAMEDNODEMAP || DOM_Object::IsA(type); }
	virtual void GCTrace() { if (implementation) implementation->GCTrace(); }

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION_WITH_DATA(accessItem); // {get,set,remove}NamedItem{,NS} and item
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 8 };
};

#endif // DOM_NODEMAP_H
