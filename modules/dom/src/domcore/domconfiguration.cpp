/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domcore/domconfiguration.h"

#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

class DOM_DOMConfiguration_Parameter
	: public Link
{
public:
	DOM_DOMConfiguration_Parameter(DOM_DOMConfiguration::CheckParameterValue *check)
		: name(NULL), check(check)
	{
	}

	~DOM_DOMConfiguration_Parameter()
	{
		OP_DELETEA(name);
	}

	char *name;
	DOM_DOMConfiguration::CheckParameterValue *check;
};

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_STATIC_PARAMETERS_START() void DOM_configurationParameters_Init(DOM_GlobalData *global_data) { DOM_DOMConfiguration_StaticParameter *parameters = global_data->configurationParameters;
# define DOM_STATIC_PARAMETERS_ITEM(name_, initial_, check_) parameters->name = name_; parameters->initial = initial_; parameters->check = check_; ++parameters;
# define DOM_STATIC_PARAMETERS_END() parameters->name = NULL; }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_STATIC_PARAMETERS_START() const DOM_DOMConfiguration_StaticParameter g_DOM_configurationParameters[] = {
# define DOM_STATIC_PARAMETERS_ITEM(name_, initial_, check_) { name_, initial_, check_ },
# define DOM_STATIC_PARAMETERS_END() { 0, 0, 0 } };
#endif // DOM_NO_COMPLEX_GLOBALS

DOM_STATIC_PARAMETERS_START()
	DOM_STATIC_PARAMETERS_ITEM("canonical-form", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("cdata-sections", "true", DOM_DOMConfiguration::acceptTrue)
	DOM_STATIC_PARAMETERS_ITEM("check-character-normalization", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("comments", "true", DOM_DOMConfiguration::acceptTrue)
	DOM_STATIC_PARAMETERS_ITEM("datatype-normalization", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("entities", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("error-handler", "null", DOM_DOMConfiguration::acceptObject)
	DOM_STATIC_PARAMETERS_ITEM("infoset", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("namespaces", "true", DOM_DOMConfiguration::acceptTrue)
	DOM_STATIC_PARAMETERS_ITEM("namespace-declarations", "true", DOM_DOMConfiguration::acceptTrue)
	DOM_STATIC_PARAMETERS_ITEM("normalize-characters", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("schema-location", "null", DOM_DOMConfiguration::acceptNothing)
	DOM_STATIC_PARAMETERS_ITEM("schema-type", "null", DOM_DOMConfiguration::acceptNothing)
	DOM_STATIC_PARAMETERS_ITEM("split-cdata-sections", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("validate", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("validate-if-schema", "false", DOM_DOMConfiguration::acceptFalse)
	DOM_STATIC_PARAMETERS_ITEM("well-formed", "true", DOM_DOMConfiguration::acceptTrue)
	DOM_STATIC_PARAMETERS_ITEM("element-content-whitespace", "true", DOM_DOMConfiguration::acceptTrue)
DOM_STATIC_PARAMETERS_END()

/* static */ OP_STATUS
DOM_DOMConfiguration::Make(DOM_DOMConfiguration *&configuration, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(configuration = OP_NEW(DOM_DOMConfiguration, ()), runtime, runtime->GetPrototype(DOM_Runtime::DOMCONFIGURATION_PROTOTYPE), "DOMConfiguration"));
	RETURN_IF_ERROR(DOMSetObjectRuntime(configuration->parameter_values = OP_NEW(DOM_Object, ()), runtime));

	const DOM_DOMConfiguration_StaticParameter *parameters = g_DOM_configurationParameters;

	while (parameters->name)
	{
		const char *initial = parameters->initial;
		ES_Value value;

		if (op_strcmp("true", initial) == 0)
			DOMSetBoolean(&value, TRUE);
		else if (op_strcmp("false", initial) == 0)
			DOMSetBoolean(&value, FALSE);
		else if (op_strcmp("null", initial) == 0)
			DOMSetNull(&value);

		RETURN_IF_ERROR(configuration->AddParameter(parameters->name, &value, parameters->check));

		++parameters;
	}

	return OpStatus::OK;
}

DOM_DOMConfiguration::~DOM_DOMConfiguration()
{
	parameters.Clear();
}

OP_STATUS
DOM_DOMConfiguration::AddParameter(const char *name, ES_Value *value, DOM_DOMConfiguration::CheckParameterValue *check)
{
	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Append(name));

	DOM_DOMConfiguration_Parameter *parameter = OP_NEW(DOM_DOMConfiguration_Parameter, (check));
	if (!parameter)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsMemoryError(SetStr(parameter->name, name)) || value->type != VALUE_NULL && OpStatus::IsMemoryError(parameter_values->Put(buffer.GetStorage(), *value)))
	{
		OP_DELETE(parameter);
		return OpStatus::ERR_NO_MEMORY;
	}

	parameter->Into(&parameters);
	return OpStatus::OK;
}

OP_STATUS
DOM_DOMConfiguration::GetParameter(const uni_char *name, ES_Value *value)
{
	OP_BOOLEAN result = parameter_values->Get(name, value);

	if (OpStatus::IsError(result))
		return result;
	else if (result != OpBoolean::IS_TRUE)
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_DOMConfiguration::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_parameterNames)
	{
		ES_GetState state = DOMSetPrivate(value, DOM_PRIVATE_parameterNames);
		if (state == GET_FAILED)
		{
			DOM_DOMStringList *stringlist;

			GET_FAILED_IF_ERROR(DOM_DOMStringList::Make(stringlist, this, this, static_cast<DOM_Runtime *>(origining_runtime)));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_parameterNames, *stringlist));

			DOMSetObject(value, stringlist);
			return GET_SUCCESS;
		}
		return state;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_DOMConfiguration::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_parameterNames)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_DOMConfiguration::GCTrace()
{
	GCMark(parameter_values);
}

/* virtual */ unsigned
DOM_DOMConfiguration::StringList_length()
{
	return parameters.Cardinal();
}

/* virtual */ OP_STATUS
DOM_DOMConfiguration::StringList_item(int index, const uni_char *&name)
{
	for (DOM_DOMConfiguration_Parameter *parameter = (DOM_DOMConfiguration_Parameter *) parameters.First();
	     parameter;
	     parameter = (DOM_DOMConfiguration_Parameter *) parameter->Suc())
		if (index == 0)
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			RETURN_IF_ERROR(buffer->Append(parameter->name));
			name = buffer->GetStorage();
			return OpStatus::OK;
		}
		else
			--index;

	return OpStatus::ERR;
}

/* virtual */ BOOL
DOM_DOMConfiguration::StringList_contains(const uni_char *string)
{
	for (DOM_DOMConfiguration_Parameter *parameter = (DOM_DOMConfiguration_Parameter *) parameters.First();
	     parameter;
	     parameter = (DOM_DOMConfiguration_Parameter *) parameter->Suc())
		if (uni_str_eq(string, parameter->name))
			return TRUE;

	return FALSE;
}

/* static */ int
DOM_DOMConfiguration::accessParameter(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime, int data)
{
	DOM_THIS_OBJECT(configuration, DOM_TYPE_DOMCONFIGURATION, DOM_DOMConfiguration);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char *name = argv[0].value.string;
	DOM_DOMConfiguration_Parameter *parameter;

	for (parameter = (DOM_DOMConfiguration_Parameter *) configuration->parameters.First();
	     parameter && !uni_str_eq(name, parameter->name);
	     parameter = (DOM_DOMConfiguration_Parameter *) parameter->Suc())
	{
		// Loop until we find the right parameter
	}

	if (data == 0)
	{
		if (parameter)
		{
			OP_STATUS result = configuration->GetParameter(name, return_value);

			if (result == OpStatus::ERR_NO_MEMORY)
				return ES_NO_MEMORY;
			else if (result != OpStatus::OK)
				DOMSetNull(return_value);

			return ES_VALUE;
		}
		else
			return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);
	}
	else
	{
		DOMException code = parameter ? parameter->check(parameter->name, &argv[1]) : NOT_FOUND_ERR;

		if (data == 1)
		{
			if (code == DOMEXCEPTION_NO_ERR)
			{
				OP_STATUS status;

				if (argv[1].type == VALUE_NULL || argv[1].type == VALUE_UNDEFINED)
					status = configuration->parameter_values->Delete(name);
				else
					status = configuration->parameter_values->Put(name, argv[1]);

				CALL_FAILED_IF_ERROR(status);
				return ES_FAILED;
			}
			else
				return configuration->CallDOMException(code, return_value);
		}
		else
		{
			DOMSetBoolean(return_value, code == DOMEXCEPTION_NO_ERR);
			return ES_VALUE;
		}
	}
}

DOM_Object::DOMException DOM_DOMConfiguration::acceptTrue(const char *, ES_Value *value)
{
	if (value->type == VALUE_BOOLEAN)
		if (value->value.boolean == TRUE)
			return DOM_Object::DOMEXCEPTION_NO_ERR;
		else
			return DOM_Object::NOT_SUPPORTED_ERR;
	else
		return DOM_Object::TYPE_MISMATCH_ERR;
}

DOM_Object::DOMException DOM_DOMConfiguration::acceptFalse(const char *, ES_Value *value)
{
	if (value->type == VALUE_BOOLEAN)
		if (value->value.boolean == FALSE)
			return DOM_Object::DOMEXCEPTION_NO_ERR;
		else
			return DOM_Object::NOT_SUPPORTED_ERR;
	else
		return DOM_Object::TYPE_MISMATCH_ERR;
}

DOM_Object::DOMException DOM_DOMConfiguration::acceptBoolean(const char *, ES_Value *value)
{
	if (value->type == VALUE_BOOLEAN)
		return DOM_Object::DOMEXCEPTION_NO_ERR;
	else
		return DOM_Object::TYPE_MISMATCH_ERR;
}

DOM_Object::DOMException DOM_DOMConfiguration::acceptNull(const char *, ES_Value *value)
{
	if (value->type == VALUE_NULL)
		return DOM_Object::DOMEXCEPTION_NO_ERR;
	else if (value->type == VALUE_OBJECT)
		return DOM_Object::NOT_SUPPORTED_ERR;
	else
		return DOM_Object::TYPE_MISMATCH_ERR;
}

DOM_Object::DOMException DOM_DOMConfiguration::acceptObject(const char *, ES_Value *value)
{
	if (value->type == VALUE_NULL || value->type == VALUE_OBJECT)
		return DOM_Object::DOMEXCEPTION_NO_ERR;
	else
		return DOM_Object::TYPE_MISMATCH_ERR;
}

DOM_Object::DOMException DOM_DOMConfiguration::acceptNothing(const char *, ES_Value *value)
{
	if (value->type == VALUE_NULL)
		return DOM_Object::DOMEXCEPTION_NO_ERR;
	else
		return DOM_Object::NOT_SUPPORTED_ERR;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_DOMConfiguration)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DOMConfiguration, DOM_DOMConfiguration::accessParameter, 0, "getParameter", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DOMConfiguration, DOM_DOMConfiguration::accessParameter, 1, "setParameter", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DOMConfiguration, DOM_DOMConfiguration::accessParameter, 2, "canSetParameter", "s-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_DOMConfiguration)

