/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_STATICNODELIST_H
#define DOM_STATICNODELIST_H

#include "modules/dom/src/domcore/node.h"
#include "modules/util/adt/opvector.h"

class DOM_EnvironmentImpl;
class HTML_Element;

class DOM_StaticNodeList
	: public DOM_Object,
	  public ListElement<DOM_StaticNodeList>
{
public:
	static OP_STATUS Make(DOM_StaticNodeList *&list, OpVector<HTML_Element>& elements, DOM_Document *document);

	virtual ~DOM_StaticNodeList();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_STATIC_NODE_LIST || DOM_Object::IsA(type); }
	virtual void GCTrace();
	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(getItem);

protected:
	DOM_StaticNodeList() {}
	DOM_StaticNodeList(const DOM_StaticNodeList&) : DOM_Object() {}

	OpVector<DOM_Object> m_dom_nodes;
};

#endif // DOM_STATICNODELIST_H
