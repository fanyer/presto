/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/domstringlist.h"

DOM_DOMStringList::DOM_DOMStringList(DOM_Object *base, Generator *generator)
	: base(base),
	  generator(generator)
{
}

/* static */ OP_STATUS
DOM_DOMStringList::Make(DOM_DOMStringList *&stringlist, DOM_Object *base, Generator *generator, DOM_Runtime *runtime)
{
	return DOMSetObjectRuntime(stringlist = OP_NEW(DOM_DOMStringList, (base, generator)), runtime, runtime->GetPrototype(DOM_Runtime::DOMSTRINGLIST_PROTOTYPE), "DOMStringList");
}

/* virtual */ ES_GetState
DOM_DOMStringList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, generator ? generator->StringList_length() : 0);
		return GET_SUCCESS;
	}
	else
		return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_DOMStringList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0 && generator && property_index < static_cast<int>(generator->StringList_length()))
	{
		if (value)
		{
			const uni_char *name;
			GET_FAILED_IF_ERROR(generator->StringList_item(property_index, name));
			DOMSetString(value, name);
		}

		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_DOMStringList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_DOMStringList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_READ_ONLY;
}

/* virtual */ void
DOM_DOMStringList::GCTrace()
{
	GCMark(base);
}

/* virtual */ ES_GetState
DOM_DOMStringList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = generator ? generator->StringList_length() : 0;
	return GET_SUCCESS;
}

/* static */ int
DOM_DOMStringList::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(stringlist, DOM_TYPE_DOMSTRINGLIST, DOM_DOMStringList);
	DOM_CHECK_ARGUMENTS("n");

	unsigned index = TruncateDoubleToUInt(argv[0].value.number);

	if (stringlist->generator && index < stringlist->generator->StringList_length())
	{
		const uni_char *name;
		CALL_FAILED_IF_ERROR(stringlist->generator->StringList_item(index, name));
		DOMSetString(return_value, name);
	}
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
DOM_DOMStringList::contains(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(stringlist, DOM_TYPE_DOMSTRINGLIST, DOM_DOMStringList);
	DOM_CHECK_ARGUMENTS("s");

	DOMSetBoolean(return_value, stringlist->generator && stringlist->generator->StringList_contains(argv[0].value.string));
	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_DOMStringList)
	DOM_FUNCTIONS_FUNCTION(DOM_DOMStringList, DOM_DOMStringList::item, "item", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_DOMStringList, DOM_DOMStringList::contains, "contains", "s-")
DOM_FUNCTIONS_END(DOM_DOMStringList)
