/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domstylesheets/stylesheetlist.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domhtml/htmlcoll.h"

DOM_StyleSheetList::DOM_StyleSheetList(DOM_Document *document)
	: document(document),
	  collection(NULL)
{
}

/* static */ OP_STATUS
DOM_StyleSheetList::Make(DOM_StyleSheetList *&stylesheetlist, DOM_Document *document)
{
	DOM_Runtime *runtime = document->GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(stylesheetlist = OP_NEW(DOM_StyleSheetList, (document)), runtime, runtime->GetPrototype(DOM_Runtime::STYLESHEETLIST_PROTOTYPE), "StyleSheetList"));

	DOM_SimpleCollectionFilter filter(STYLESHEETS);
	DOM_Collection *collection;

	RETURN_IF_ERROR(DOM_Collection::Make(collection, document->GetEnvironment(), "Object", document, FALSE, FALSE, filter));

	stylesheetlist->collection = collection;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_StyleSheetList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return collection->GetName(property_name, value, origining_runtime);
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_StyleSheetList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_StyleSheetList::GCTrace()
{
	GCMark(collection);
}

/* virtual */ ES_GetState
DOM_StyleSheetList::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	DOMSetNull(value);

	int length = collection->Length();

	if (property_index >= 0 && property_index < length)
	{
		HTML_Element *element = collection->Item(property_index);
		DOM_Node *node;

		GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, element, document));
		ES_GetState state = node->GetStyleSheet(value, NULL, (DOM_Runtime *) origining_runtime);

		/* This would mean DOM_Node::GetStyleSheet didn't think this node
		   should have a StyleSheet object.  But since our collection is
		   supposed to contain specifically those nodes that are supposed
		   to have a StyleSheet object, this shouldn't happen. */
		OP_ASSERT(state != GET_FAILED);

		return state;
	}
	else
		return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_StyleSheetList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = collection->Length();
	return GET_SUCCESS;
}

/* static */ int
DOM_StyleSheetList::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(stylesheetlist, DOM_TYPE_STYLESHEETLIST, DOM_StyleSheetList);
	DOM_CHECK_ARGUMENTS("n");

	if (!stylesheetlist->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOMSetNull(return_value);

	int index = (int) argv[0].value.number;

	if (argv[0].value.number == (double) index)
		RETURN_GETNAME_AS_CALL(stylesheetlist->GetIndex(index, return_value, origining_runtime));
	else
		return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_StyleSheetList)
	DOM_FUNCTIONS_FUNCTION(DOM_StyleSheetList, DOM_StyleSheetList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_StyleSheetList)
