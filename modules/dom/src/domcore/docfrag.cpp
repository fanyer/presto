/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domselectors/domselector.h"

#include "modules/logdoc/htm_elm.h"

DOM_DocumentFragment::DOM_DocumentFragment()
	: DOM_Node(DOCUMENT_FRAGMENT_NODE),
	  placeholder(NULL)
{
}

/* static */ OP_STATUS
DOM_DocumentFragment::Make(DOM_DocumentFragment *&document_fragment, DOM_Node *reference, HTML_Element *placeholder)
{
	DOM_EnvironmentImpl *environment = reference->GetEnvironment();

	if (!placeholder)
		RETURN_IF_ERROR(HTML_Element::DOMCreateNullElement(placeholder, environment));

	DOM_Runtime *runtime = environment->GetDOMRuntime();

	OP_STATUS status = DOMSetObjectRuntime(document_fragment = OP_NEW(DOM_DocumentFragment, ()), runtime, runtime->GetPrototype(DOM_Runtime::DOCUMENTFRAGMENT_PROTOTYPE), "DocumentFragment");
	if (OpStatus::IsSuccess(status))
	{
		document_fragment->SetPlaceholderElement(placeholder);
		document_fragment->SetOwnerDocument(reference->IsA(DOM_TYPE_DOCUMENT) ? static_cast<DOM_Document *>(reference) : reference->GetOwnerDocument());
	}
	else
		HTML_Element::DOMFreeElement(placeholder, environment);

	return status;
}

DOM_DocumentFragment::~DOM_DocumentFragment()
{
	if (placeholder)
		FreeElementTree(placeholder);
}

/* virtual */ ES_GetState
DOM_DocumentFragment::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeName:
		DOMSetString(value, UNI_L("#document-fragment"));
		return GET_SUCCESS;

	case OP_ATOM_firstChild:
		return DOMSetElement(value, placeholder->FirstChild());

	case OP_ATOM_lastChild:
		return DOMSetElement(value, placeholder->LastChild());

	case OP_ATOM_childNodes:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_childNodes);
			if (result != GET_FAILED)
				return result;
			else
			{
				DOM_SimpleCollectionFilter filter(CHILDNODES);
				DOM_Collection *collection;

				GET_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, GetEnvironment(), this, FALSE, FALSE, filter));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_childNodes, *collection));

				DOMSetObject(value, collection);
			}
		}
		return GET_SUCCESS;

	default:
		return DOM_Node::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_DocumentFragment::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);

	if (!via_tree)
		TraceElementTree(placeholder);
}

/* virtual */ void
DOM_DocumentFragment::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_Node::DOMChangeRuntime(new_runtime);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_childNodes);
}

void
DOM_DocumentFragment::SetPlaceholderElement(HTML_Element *new_placeholder)
{
	if (placeholder)
	{
		/* Not supported except on empty document fragments. */
		OP_ASSERT(placeholder->FirstChild() == NULL);

		placeholder->SetESElement(NULL);
		HTML_Element::DOMFreeElement(placeholder, GetEnvironment());
	}

	placeholder = new_placeholder;
	placeholder->SetESElement(this);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_DocumentFragment)
#ifdef DOM_SELECTORS_API
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DocumentFragment, DOM_Selector::querySelector, DOM_Selector::QUERY_DOCFRAG, "querySelector", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DocumentFragment, DOM_Selector::querySelector, DOM_Selector::QUERY_DOCFRAG | DOM_Selector::QUERY_ALL, "querySelectorAll", "s-")
#endif // DOM_SELECTORS_API
DOM_FUNCTIONS_WITH_DATA_END(DOM_DocumentFragment)

