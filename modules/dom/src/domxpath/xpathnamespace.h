/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_XPATHNAMESPACE
#define DOM_XPATHNAMESPACE

#ifdef DOM3_XPATH

#include "modules/dom/src/domcore/node.h"

class DOM_Element;

class DOM_XPathNamespace
	: public DOM_Node
{
protected:
	DOM_XPathNamespace(DOM_Element *owner_element);

	DOM_Element *owner_element;
	uni_char *prefix;
	uni_char *uri;

	DOM_XPathNamespace *next_ns;

public:
	static OP_STATUS Make(DOM_XPathNamespace *&nsnode, DOM_Element *owner_element, const uni_char *prefix, const uni_char *uri);

	virtual ~DOM_XPathNamespace();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XPATHNAMESPACE || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	BOOL HasName(const uni_char *prefix, const uni_char *uri);

	const uni_char *GetNsPrefix() { return prefix; }
	const uni_char *GetNsUri() { return uri; }
	DOM_Element *GetOwnerElement() { return owner_element; }

	DOM_XPathNamespace *GetNextNS() { return next_ns; }
	void SetNextNS(DOM_XPathNamespace *next) { next_ns = next; }

	/** Initialize the global variable "XPathNamespace". */
	static void ConstructXPathNamespaceObjectL(ES_Object *object, DOM_Runtime *runtime);
};

#endif // DOM3_XPATH
#endif // DOM_XPATHNAMESPACE
