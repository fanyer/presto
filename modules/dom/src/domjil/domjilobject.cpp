/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"

/* static */ int
DOM_JILObject::CallException(DOM_Object::JILException exception_type, ES_Value* return_value, DOM_Runtime* runtime, const uni_char* message /* = NULL */)
{
	CALL_FAILED_IF_ERROR(CreateJILException(exception_type, return_value, runtime));
	if (message)
	{
		ES_Value message_value;
		DOM_Object::DOMSetString(&message_value, message);
		runtime->PutName(return_value->value.object, UNI_L("message"), message_value);
	}
	return ES_EXCEPTION;
}

/* static */ int
DOM_JILObject::HandleJILError(OP_STATUS error_code, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error_code)
	{
	case OpStatus::ERR_NO_MEMORY:
		return ES_NO_MEMORY;
	case OpStatus::ERR_NO_ACCESS:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime,  UNI_L("No access"));
	case OpStatus::ERR_NOT_SUPPORTED:
		return CallException(DOM_Object::JIL_UNSUPPORTED_ERR, return_value, origining_runtime);
	case OpStatus::ERR_OUT_OF_RANGE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Argument out of range"));
	case OpStatus::ERR_FILE_NOT_FOUND:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("File not found"));
	default:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime);
	}
}

ES_PutState
DOM_JILObject::PutCallback(ES_Value* value, ES_Object*& callback_storage)
{
	switch (value->type)
	{
		case VALUE_OBJECT:
			callback_storage = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
			callback_storage = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	}
}

OP_STATUS
DOM_JILObject::CallCallback(ES_Object* callback, ES_Value* argv, int argc)
{
	if (callback)
	{
		ES_AsyncInterface* async_iface = GetRuntime()->GetEnvironment()->GetAsyncInterface();
		OP_ASSERT(async_iface);
		return async_iface->CallFunction(callback, GetNativeObject(), argc, argv, NULL, NULL);
	}
	return OpStatus::OK;
}

ES_PutState
DOM_JILObject::PutDate(ES_Value* value, ES_Object*& storage)
{
	switch (value->type)
	{
	case VALUE_NULL:
	case VALUE_UNDEFINED:
		storage = NULL;
		return PUT_SUCCESS;
	case VALUE_OBJECT:
		{
			if (uni_str_eq(GetRuntime()->GetClass(value->value.object), UNI_L("Date")))
			{
				storage = value->value.object;
				return PUT_SUCCESS;
			}
			else
				return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	default:
		return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	}
}


#endif // DOM_JIL_API_SUPPORT
