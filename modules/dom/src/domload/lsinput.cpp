/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domload/lsinput.h"

#include "modules/util/tempbuf.h"

/* static */ OP_STATUS
DOM_LSInput::Make(ES_Object *&input, DOM_EnvironmentImpl *environment, const uni_char *stringData, const uni_char *systemId)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	ES_Value value;

	DOM_Object *dom_input;
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(dom_input = OP_NEW(DOM_Object, ()), runtime, runtime->GetObjectPrototype(), "LSInput"));
	input = *dom_input;

	DOM_Object::DOMSetNull(&value);
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("characterStream"), value));
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("byteStream"), value));

	if (stringData)
		DOM_Object::DOMSetString(&value, stringData);
	else
		DOM_Object::DOMSetNull(&value);
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("stringData"), value));

	if (systemId)
		DOM_Object::DOMSetString(&value, systemId);
	else
		DOM_Object::DOMSetNull(&value);
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("systemId"), value));

	DOM_Object::DOMSetNull(&value);
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("publicId"), value));
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("baseURI"), value));
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("encoding"), value));

	DOM_Object::DOMSetBoolean(&value, FALSE);
	RETURN_IF_ERROR(runtime->PutName(input, UNI_L("certifiedText"), value));

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_LSInput::GetByteStream(ES_Object *&byteStream, ES_Object *input, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	OP_BOOLEAN result;
	ES_Value value;

	byteStream = NULL;

	RETURN_IF_ERROR(result = runtime->GetName(input, UNI_L("characterStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type != VALUE_NULL && value.type != VALUE_UNDEFINED && !(value.type == VALUE_STRING && *value.value.string))
		return OpStatus::OK;

	RETURN_IF_ERROR(result = runtime->GetName(input, UNI_L("byteStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		byteStream = value.value.object;

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_LSInput::GetStringData(const uni_char *&stringData, ES_Object *input, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	OP_BOOLEAN result;
	ES_Value value;

	stringData = NULL;

	RETURN_IF_ERROR(result = runtime->GetName(input, UNI_L("characterStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type != VALUE_NULL && value.type != VALUE_UNDEFINED && !(value.type == VALUE_STRING && *value.value.string))
		return OpStatus::OK;

	RETURN_IF_ERROR(result = runtime->GetName(input, UNI_L("byteStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type != VALUE_NULL && value.type != VALUE_UNDEFINED && !(value.type == VALUE_STRING && *value.value.string))
		return OpStatus::OK;

	RETURN_IF_ERROR(result = runtime->GetName(input, UNI_L("stringData"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING && *value.value.string)
	{
		TempBuffer *buffer = environment->GetWindow()->GetEmptyTempBuf();
		RETURN_IF_ERROR(buffer->Append(value.value.string));
		stringData = buffer->GetStorage();
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_LSInput::GetSystemId(const uni_char *&systemId, ES_Object *input, DOM_EnvironmentImpl *environment)
{
	OP_BOOLEAN result;
	ES_Value value;

	systemId = NULL;

	const uni_char *stringData;

	RETURN_IF_ERROR(GetStringData(stringData, input, environment));
	if (stringData)
		return OpStatus::OK;

	RETURN_IF_ERROR(result =  environment->GetDOMRuntime()->GetName(input, UNI_L("systemId"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING && *value.value.string)
	{
		TempBuffer *buffer = environment->GetWindow()->GetEmptyTempBuf();
		RETURN_IF_ERROR(buffer->Append(value.value.string));
		systemId = buffer->GetStorage();
	}

	return OpStatus::OK;
}

#endif // DOM3_LOAD
