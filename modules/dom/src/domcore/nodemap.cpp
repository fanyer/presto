/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/nodemap.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"

DOM_NamedNodeMapImpl::~DOM_NamedNodeMapImpl()
{
}

void
DOM_NamedNodeMapImpl::GCTrace()
{
}

DOM_NamedNodeMap::DOM_NamedNodeMap(DOM_NamedNodeMapImpl *implementation)
	: implementation(implementation)
{
}

/* static */ OP_STATUS
DOM_NamedNodeMap::Make(DOM_NamedNodeMap *&map, DOM_Node *reference, DOM_NamedNodeMapImpl *implementation)
{
	DOM_Runtime *runtime = reference->GetRuntime();

	if (!(map = OP_NEW(DOM_NamedNodeMap, (implementation))))
	{
		OP_DELETE(implementation);
		return OpStatus::ERR_NO_MEMORY;
	}
	else
		return DOMSetObjectRuntime(map, runtime, runtime->GetPrototype(DOM_Runtime::NAMEDNODEMAP_PROTOTYPE), "NamedNodeMap");
}

/* virtual */
DOM_NamedNodeMap::~DOM_NamedNodeMap()
{
	OP_DELETE(implementation);
}

/* virtual */ ES_GetState
DOM_NamedNodeMap::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, implementation ? implementation->Length() : 0);
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_NamedNodeMap::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	ES_Value return_value;
	int result;

	// Don't let named elements shadow default properties
	// so first look for known properties, then check the
	// collection implementation
	ES_GetState get_state = DOM_Object::GetName(property_name, property_code, value, origining_runtime);

	if (get_state != GET_FAILED)
		return get_state;

	if (implementation)
		result = implementation->GetNamedItem(NULL, property_name, FALSE, &return_value, (DOM_Runtime *) origining_runtime);
	else
		result = ES_FAILED;

	if (result == ES_VALUE && return_value.type == VALUE_OBJECT)
	{
		if (value)
			*value = return_value;

		return GET_SUCCESS;
	}
	else if (result == ES_NO_MEMORY)
		return GET_NO_MEMORY;

	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_NamedNodeMap::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	ES_Value return_value;
	int result;

	if (implementation)
		result = implementation->Item(property_index, &return_value, origining_runtime);
	else
		result = ES_FAILED;

	if (result == ES_VALUE && return_value.type == VALUE_OBJECT)
	{
		if (value)
			*value = return_value;

		return GET_SUCCESS;
	}
	else if (result == ES_NO_MEMORY)
		return GET_NO_MEMORY;
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_NamedNodeMap::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_NamedNodeMap::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = implementation ? implementation->Length() : 0;
	return GET_SUCCESS;
}

/* static */ int
DOM_NamedNodeMap::accessItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(map, DOM_TYPE_NAMEDNODEMAP, DOM_NamedNodeMap);

	if (!map->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	const uni_char *ns_uri, *name;

	DOM_NamedNodeMapImpl *impl = map->implementation;

	if (!impl)
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	if (data < 2)
	{
		DOM_CHECK_ARGUMENTS("s");
		ns_uri = NULL;
		name = argv[0].value.string;
	}
	else if (data < 4)
	{
		DOM_CHECK_ARGUMENTS("Ss");
		name = argv[1].value.string;
		ns_uri = argv[0].type == VALUE_STRING ? argv[0].value.string : NULL;

		if (ns_uri && !*ns_uri)
			ns_uri = NULL;
	}
	else if (data < 6)
	{
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);

		return impl->SetNamedItem(node, data == 5, return_value, origining_runtime);
	}
	else
	{
		DOM_CHECK_ARGUMENTS("n");

		return impl->Item((int) argv[0].value.number, return_value, origining_runtime);
	}

	if ((data & 1) == 0)
		return impl->GetNamedItem(ns_uri, name, data == 2, return_value, origining_runtime);
	else
		return impl->RemoveNamedItem(ns_uri, name, data == 3, return_value, origining_runtime);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_NamedNodeMap)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NamedNodeMap, DOM_NamedNodeMap::accessItem, 0, "getNamedItem", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NamedNodeMap, DOM_NamedNodeMap::accessItem, 1, "removeNamedItem", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NamedNodeMap, DOM_NamedNodeMap::accessItem, 2, "getNamedItemNS", "Ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NamedNodeMap, DOM_NamedNodeMap::accessItem, 3, "removeNamedItemNS", "Ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NamedNodeMap, DOM_NamedNodeMap::accessItem, 4, "setNamedItem", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NamedNodeMap, DOM_NamedNodeMap::accessItem, 5, "setNamedItemNS", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NamedNodeMap, DOM_NamedNodeMap::accessItem, 6, "item", "n-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_NamedNodeMap)
