/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_XPATH

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"

#include "modules/util/str.h"
#include "modules/xpath/xpath.h"

DOM_XPathNamespace::DOM_XPathNamespace(DOM_Element *owner_element)
	: DOM_Node(XPATH_NAMESPACE_NODE),
	  owner_element(owner_element),
	  prefix(NULL),
	  uri(NULL),
	  next_ns(NULL)
{
	SetOwnerDocument(owner_element->GetOwnerDocument());
}

/* static */ OP_STATUS
DOM_XPathNamespace::Make(DOM_XPathNamespace *&nsnode, DOM_Element *owner_element, const uni_char *prefix, const uni_char *uri)
{
	DOM_Runtime *runtime = owner_element->GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(nsnode = OP_NEW(DOM_XPathNamespace, (owner_element)), runtime, runtime->GetPrototype(DOM_Runtime::NODE_PROTOTYPE), "XPathNamespace"));
	if (prefix)
		RETURN_IF_ERROR(UniSetStr(nsnode->prefix, prefix));
	RETURN_IF_ERROR(UniSetStr(nsnode->uri, uri));

	return OpStatus::OK;
}

DOM_XPathNamespace::~DOM_XPathNamespace()
{
	OP_DELETEA(prefix);
	OP_DELETEA(uri);
}

/* virtual */ ES_GetState
DOM_XPathNamespace::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_localName:
	case OP_ATOM_prefix:
		DOMSetString(value, prefix);
		return GET_SUCCESS;

	case OP_ATOM_namespaceURI:
	case OP_ATOM_nodeValue:
		DOMSetString(value, uri);
		return GET_SUCCESS;

	case OP_ATOM_nodeName:
		DOMSetString(value, UNI_L("#namespace"));
		return GET_SUCCESS;

	case OP_ATOM_ownerElement:
		DOMSetObject(value, owner_element);
		return GET_SUCCESS;
	}

	return DOM_Node::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_XPathNamespace::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetName(property_name, NULL, origining_runtime) == GET_SUCCESS)
		return PUT_READ_ONLY;
	else
		return DOM_Node::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_XPathNamespace::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);

	if (!via_tree)
		GCMark(owner_element);
}

BOOL
DOM_XPathNamespace::HasName(const uni_char *prefix0, const uni_char *uri0)
{
	return (prefix == prefix0 || prefix && prefix0 && uni_str_eq(prefix, prefix0)) && uni_str_eq(uri, uri0);
}

/* static */ void
DOM_XPathNamespace::ConstructXPathNamespaceObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	ES_Value val;

	DOMSetNumber(&val, XPATH_NAMESPACE_NODE);
	DOM_Object::PutL(object, "XPATH_NAMESPACE_NODE", val, runtime);
}

#endif // DOM3_XPATH
