/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_SAVE

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domsave/lsoutput.h"

#include "modules/util/tempbuf.h"

/* static */ OP_STATUS
DOM_LSOutput::Make(ES_Object *&output, DOM_EnvironmentImpl *environment, const uni_char *systemId)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	ES_Value value;

	DOM_Object *dom_output;
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(dom_output = OP_NEW(DOM_Object, ()), runtime, runtime->GetObjectPrototype(), "LSOutput"));
	output = *dom_output;

	DOM_Object::DOMSetNull(&value);
	RETURN_IF_ERROR(runtime->PutName(output, UNI_L("characterStream"), value));
	RETURN_IF_ERROR(runtime->PutName(output, UNI_L("byteStream"), value));

	if (systemId)
		DOM_Object::DOMSetString(&value, systemId);
	else
		DOM_Object::DOMSetNull(&value);
	RETURN_IF_ERROR(runtime->PutName(output, UNI_L("systemId"), value));

	DOM_Object::DOMSetNull(&value);
	RETURN_IF_ERROR(runtime->PutName(output, UNI_L("encoding"), value));

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_LSOutput::GetCharacterStream(ES_Object *&object, ES_Object *output, DOM_EnvironmentImpl *environment)
{
	OP_BOOLEAN result;
	ES_Value value;

	object = NULL;

	RETURN_IF_ERROR(result = environment->GetRuntime()->GetName(output, UNI_L("characterStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		object = value.value.object;

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_LSOutput::GetByteStream(ES_Object *&object, ES_Object *output, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	OP_BOOLEAN result;
	ES_Value value;

	object = NULL;

	RETURN_IF_ERROR(result = runtime->GetName(output, UNI_L("characterStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type != VALUE_NULL && value.type != VALUE_UNDEFINED && !(value.type == VALUE_STRING && *value.value.string))
		return OpStatus::OK;

	RETURN_IF_ERROR(result = runtime->GetName(output, UNI_L("byteStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		object = value.value.object;

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_LSOutput::GetSystemId(const uni_char *&systemId, ES_Object *output, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	OP_BOOLEAN result;
	ES_Value value;

	systemId = NULL;

	RETURN_IF_ERROR(result = runtime->GetName(output, UNI_L("characterStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type != VALUE_NULL && value.type != VALUE_UNDEFINED && !(value.type == VALUE_STRING && *value.value.string))
		return OpStatus::OK;

	RETURN_IF_ERROR(result = runtime->GetName(output, UNI_L("byteStream"), &value));
	if (result == OpBoolean::IS_TRUE && value.type != VALUE_NULL && value.type != VALUE_UNDEFINED && !(value.type == VALUE_STRING && *value.value.string))
		return OpStatus::OK;

	RETURN_IF_ERROR(result = runtime->GetName(output, UNI_L("systemId"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING && *value.value.string)
	{
		TempBuffer *buffer = environment->GetWindow()->GetEmptyTempBuf();
		RETURN_IF_ERROR(buffer->Append(value.value.string));
		systemId = buffer->GetStorage();
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_LSOutput::GetEncoding(const uni_char *&encoding, ES_Object *output, DOM_EnvironmentImpl *environment)
{
	OP_BOOLEAN result;
	ES_Value value;

	encoding = NULL;

	RETURN_IF_ERROR(result = environment->GetRuntime()->GetName(output, UNI_L("encoding"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING && *value.value.string)
	{
		TempBuffer *buffer = environment->GetWindow()->GetEmptyTempBuf();
		RETURN_IF_ERROR(buffer->Append(value.value.string));
		encoding = buffer->GetStorage();
	}

	return OpStatus::OK;
}

#endif // DOM3_SAVE
