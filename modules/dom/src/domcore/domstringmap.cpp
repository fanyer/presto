/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/domstringmap.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/logdoc/htm_elm.h"

/**
 * Returns OpStatus:ERR if there is no legal mapping.
 */
static OP_STATUS MapPropertyToAttributeName(const uni_char* prop, TempBuffer& attr)
{
	RETURN_IF_ERROR(attr.Append("data-"));
	while (*prop)
	{
		if (*prop == '-' && prop[1] >= 'a' && prop[1] <= 'z')
			return OpStatus::ERR;

		if (*prop >= 'A' && *prop <= 'Z')
		{
			RETURN_IF_ERROR(attr.Append('-'));
			RETURN_IF_ERROR(attr.Append(*prop - 'A' + 'a'));
		}
		else
			RETURN_IF_ERROR(attr.Append(*prop));

		prop++;
	}
	return OpStatus::OK;
}

/**
 * Returns OpStatus:ERR if there is no legal mapping.
 */
static OP_STATUS MapAttributeToPropertyName(const uni_char* attr, TempBuffer& prop)
{
	if (uni_strncmp(attr, "data-", 5) != 0)
		return OpStatus::ERR;

	attr += 5;
	const uni_char* p = attr;
	while (*p)
	{
		if (*p >= 'A' && *p <= 'Z')
			return OpStatus::ERR;
		p++;
	}

	while (*attr)
	{
		if (*attr == '-' && attr[1] >= 'a' && attr[1] <= 'z')
		{
			RETURN_IF_ERROR(prop.Append(attr[1] - 'a' + 'A'));
			attr++;
		}
		else
			RETURN_IF_ERROR(prop.Append(*attr));
		attr++;
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_DOMStringMap::Make(DOM_DOMStringMap *&stringmap, DOM_Element *owner)
{
	DOM_Runtime *runtime = owner->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(stringmap = OP_NEW(DOM_DOMStringMap, (owner)), runtime, runtime->GetPrototype(DOM_Runtime::DOMSTRINGMAP_PROTOTYPE), "DOMStringMap"));
	stringmap->SetHasVolatilePropertySet();
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_DOMStringMap::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	TempBuffer attribute_name;
	OP_STATUS status = MapPropertyToAttributeName(property_name, attribute_name);
	if (OpStatus::IsMemoryError(status))
		return GET_NO_MEMORY;
	if (OpStatus::IsSuccess(status))
	{
		OP_ASSERT(attribute_name.GetStorage());
		DOM_EnvironmentImpl *environment = GetEnvironment();
		int index;
		BOOL case_sensitive = owner->HasCaseSensitiveNames();
		HTML_Element *element = owner->GetThisElement();
		if (element->DOMHasAttribute(environment, ATTR_XML, attribute_name.GetStorage(), NS_IDX_ANY_NAMESPACE, case_sensitive, index))
		{
			if (value)
				DOMSetString(value, element->DOMGetAttribute(environment, ATTR_XML, attribute_name.GetStorage(), NS_IDX_ANY_NAMESPACE, case_sensitive, FALSE, index));
			return GET_SUCCESS;
		}
	}

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_DOMStringMap::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	TempBuffer attribute_name;
	OP_STATUS status = MapPropertyToAttributeName(property_name, attribute_name);
	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			return PUT_NO_MEMORY;
		return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);
	}

	if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;
	OP_ASSERT(attribute_name.GetStorage());
	PUT_FAILED_IF_ERROR(owner->SetAttribute(ATTR_XML, attribute_name.GetStorage(), NS_IDX_DEFAULT, value->value.string, value->GetStringLength(), owner->HasCaseSensitiveNames(), origining_runtime));
	return PUT_SUCCESS;
}

/* virtual */ ES_DeleteStatus
DOM_DOMStringMap::DeleteName(const uni_char* property_name, ES_Runtime *origining_runtime)
{
	TempBuffer attribute_name;
	OP_STATUS status = MapPropertyToAttributeName(property_name, attribute_name);
	if (OpStatus::IsMemoryError(status))
		return DELETE_NO_MEMORY;
	if (OpStatus::IsSuccess(status))
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();
		int index;
		BOOL case_sensitive = owner->HasCaseSensitiveNames();
		HTML_Element *element = owner->GetThisElement();
		if (element->DOMHasAttribute(environment, ATTR_XML, attribute_name.GetStorage(), NS_IDX_ANY_NAMESPACE, case_sensitive, index))
			DELETE_FAILED_IF_MEMORY_ERROR(element->DOMRemoveAttribute(environment, attribute_name.GetStorage(), NS_IDX_ANY_NAMESPACE, case_sensitive));
	}

	return DELETE_OK;
}

/* virtual */ ES_GetState
DOM_DOMStringMap::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	TempBuffer property_name;
	HTML_AttrIterator it(owner->GetThisElement());
	const uni_char* attribute_name;
	const uni_char* dummy_value;
	while (it.GetNext(attribute_name, dummy_value))
	{
		property_name.Clear();
		OP_STATUS status = MapAttributeToPropertyName(attribute_name, property_name);
		if (OpStatus::IsMemoryError(status))
			LEAVE(status);
		else if (OpStatus::IsSuccess(status))
			enumerator->AddPropertyL(property_name.GetStorage());
	}
	return GET_SUCCESS;
}

/* virtual */ void
DOM_DOMStringMap::GCTrace()
{
	GCMark(owner);
}

