/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef USER_JAVASCRIPT

#include "modules/dom/src/userjs/userjsmagic.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/util/str.h"

class DOM_UserJSMagicCallback
	: public DOM_Object,
	  public ES_AsyncCallback
{
protected:
	ES_AsyncStatus status;
	ES_Value value;

	DOM_UserJSMagicVariable *variable;
	BOOL reading;

	DOM_UserJSMagicFunction *function;

public:
	DOM_UserJSMagicCallback(DOM_UserJSMagicFunction *function)
		: variable(NULL),
		  function(function)
	{
	}

	DOM_UserJSMagicCallback(DOM_UserJSMagicVariable *variable, BOOL reading)
		: variable(variable),
		  reading(reading),
		  function(NULL)
	{
	}

	virtual ~DOM_UserJSMagicCallback();

	virtual void GCTrace();
	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

	ES_AsyncStatus GetStatus() { return status; }
	const ES_Value &GetValue() { return value; }
};

/* virtual */
DOM_UserJSMagicCallback::~DOM_UserJSMagicCallback()
{
	DOMFreeValue(value);
}

/* virtual */ void
DOM_UserJSMagicCallback::GCTrace()
{
	GCMark(value);
}

/* virtual */ OP_STATUS
DOM_UserJSMagicCallback::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	GetRuntime()->Unprotect(*this);

	this->status = status;

	if (variable)
		variable->SetNonBusy(reading);
	if (function)
		function->SetNonBusy();

	if (status == ES_ASYNC_SUCCESS || status == ES_ASYNC_EXCEPTION)
		return DOMCopyValue(value, result);
	else
		return OpStatus::OK;
}

DOM_UserJSMagicFunction::DOM_UserJSMagicFunction(ES_Object *impl, DOM_Object *data)
	: name(NULL),
	  impl(impl),
	  real(NULL),
	  data(data),
	  busy(FALSE)
{
}

DOM_UserJSMagicFunction::~DOM_UserJSMagicFunction()
{
	OP_DELETEA(name);
}

/* static */ OP_STATUS
DOM_UserJSMagicFunction::Make(DOM_UserJSMagicFunction *&function, DOM_EnvironmentImpl *environment, const uni_char *name, ES_Object *impl)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	DOM_Object *data;

	RETURN_IF_ERROR(DOMSetObjectRuntime(data = OP_NEW(DOM_Object, ()), runtime));
	RETURN_IF_ERROR(DOMSetFunctionRuntime(function = OP_NEW(DOM_UserJSMagicFunction, (impl, data)), runtime));

	return UniSetStr(function->name, name);
}

/* virtual */ int
DOM_UserJSMagicFunction::Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	if (argc >= 0)
	{
		if (busy)
			return CallInternalException(RESOURCE_BUSY_ERR, return_value);
		else
		{
			DOM_Runtime *runtime = (DOM_Runtime *) origining_runtime;
			DOM_UserJSMagicCallback *callback;

			CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(callback = OP_NEW(DOM_UserJSMagicCallback, (this)), runtime));

			ES_Value *innerargv = OP_NEWA(ES_Value, 2 + argc);

			if (!innerargv)
				return ES_NO_MEMORY;

			DOMSetObject(&innerargv[0], real);
			DOMSetObject(&innerargv[1], this_object);

			op_memcpy(innerargv + 2, argv, argc * sizeof argv[0]);

			if (runtime->Protect(*callback))
			{
				ES_AsyncInterface *asyncif = runtime->GetEnvironment()->GetAsyncInterface();

				asyncif->SetWantExceptions();

				OP_STATUS status = asyncif->CallFunction(impl, *data, argc + 2, innerargv, callback, GetCurrentThread(runtime));

				OP_DELETEA(innerargv);

				if (OpStatus::IsError(status))
				{
					runtime->Unprotect(*callback);
					return OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_FAILED;
				}

				busy = TRUE;

				DOMSetObject(return_value, callback);
				return ES_SUSPEND | ES_RESTART;
			}
			else
				return ES_NO_MEMORY;
		}
	}
	else
	{
		DOM_UserJSMagicCallback *callback = DOM_VALUE2OBJECT(*return_value, DOM_UserJSMagicCallback);

		switch (callback->GetStatus())
		{
		case ES_ASYNC_SUCCESS:
			*return_value = callback->GetValue();
			return ES_VALUE;

		case ES_ASYNC_FAILURE:
		case ES_ASYNC_CANCELLED:
			return ES_FAILED;

		case ES_ASYNC_EXCEPTION:
			*return_value = callback->GetValue();
			return ES_EXCEPTION;

		default:
			return ES_NO_MEMORY;
		}
	}
}

/* virtual */ void
DOM_UserJSMagicFunction::GCTrace()
{
	GCMark(impl);
	GCMark(real);
	GCMark(data);
}

DOM_UserJSMagicVariable::DOM_UserJSMagicVariable(ES_Object *getter, ES_Object *setter)
	: name(NULL),
	  getter(getter),
	  setter(setter),
	  busy(0)
{
}

DOM_UserJSMagicVariable::~DOM_UserJSMagicVariable()
{
	OP_DELETEA(name);
	DOMFreeValue(last);
}

/* static */ OP_STATUS
DOM_UserJSMagicVariable::Make(DOM_UserJSMagicVariable *&variable, DOM_EnvironmentImpl *environment, const uni_char *name, ES_Object *getter, ES_Object *setter)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(variable = OP_NEW(DOM_UserJSMagicVariable, (getter, setter)), runtime, runtime->GetObjectPrototype(), "Object"));

	return UniSetStr(variable->name, name);
}

/* virtual */ void
DOM_UserJSMagicVariable::GCTrace()
{
	GCMark(getter);
	GCMark(setter);
	GCMark(last);
}

ES_GetState
DOM_UserJSMagicVariable::GetValue(ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (!restart_object)
	{
		if ((busy & BUSY_READING) == BUSY_READING)
			return ConvertCallToGetName(CallInternalException(RESOURCE_BUSY_ERR, value), value);
		else if (getter)
		{
			DOM_Runtime *runtime = (DOM_Runtime *) origining_runtime;
			DOM_UserJSMagicCallback *callback;

			GET_FAILED_IF_ERROR(DOMSetObjectRuntime(callback = OP_NEW(DOM_UserJSMagicCallback, (this, TRUE)), runtime));

			if (runtime->Protect(*callback))
			{
				ES_AsyncInterface *asyncif = runtime->GetEnvironment()->GetAsyncInterface();

				asyncif->SetWantExceptions();

				OP_STATUS status = runtime->GetEnvironment()->GetAsyncInterface()->CallFunction(getter, *this, 1, &last, callback, GetCurrentThread(runtime));

				if (OpStatus::IsError(status))
				{
					runtime->Unprotect(*callback);
					return OpStatus::IsMemoryError(status) ? GET_NO_MEMORY : GET_FAILED;
				}

				busy |= BUSY_READING;

				DOMSetObject(value, callback);
				return GET_SUSPEND;
			}
			else
				return GET_NO_MEMORY;
		}
		else
		{
			*value = last;
			return GET_SUCCESS;
		}
	}
	else
	{
		DOM_UserJSMagicCallback *callback = DOM_HOSTOBJECT(restart_object, DOM_UserJSMagicCallback);

		switch (callback->GetStatus())
		{
		case ES_ASYNC_SUCCESS:
			DOMFreeValue(last);

			GET_FAILED_IF_ERROR(DOMCopyValue(last, callback->GetValue()));

			*value = last;
			return GET_SUCCESS;

		case ES_ASYNC_FAILURE:
		case ES_ASYNC_CANCELLED:
			value->type = VALUE_UNDEFINED;
			return GET_SUCCESS;

		case ES_ASYNC_EXCEPTION:
			*value = callback->GetValue();
			return GET_EXCEPTION;

		default:
			return GET_NO_MEMORY;
		}
	}
}

ES_PutState
DOM_UserJSMagicVariable::SetValue(ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (!restart_object)
	{
		if ((busy & BUSY_WRITING) == BUSY_WRITING)
			return ConvertCallToPutName(CallInternalException(RESOURCE_BUSY_ERR, value), value);
		else
		{
			DOMFreeValue(last);

			PUT_FAILED_IF_ERROR(DOMCopyValue(last, *value));

			if (setter && (busy & BUSY_WRITING) == 0)
			{
				DOM_Runtime *runtime = (DOM_Runtime *) origining_runtime;
				DOM_UserJSMagicCallback *callback;

				PUT_FAILED_IF_ERROR(DOMSetObjectRuntime(callback = OP_NEW(DOM_UserJSMagicCallback, (this, FALSE)), runtime));

				if (runtime->Protect(*callback))
				{
					ES_AsyncInterface *asyncif = runtime->GetEnvironment()->GetAsyncInterface();

					asyncif->SetWantExceptions();

					OP_STATUS status = runtime->GetEnvironment()->GetAsyncInterface()->CallFunction(setter, *this, 1, &last, callback, GetCurrentThread(runtime));

					if (OpStatus::IsError(status))
					{
						runtime->Unprotect(*callback);
						return OpStatus::IsMemoryError(status) ? PUT_NO_MEMORY : PUT_FAILED;
					}

					busy |= BUSY_WRITING;

					DOMSetObject(value, callback);
					return PUT_SUSPEND;
				}
				else
					return PUT_NO_MEMORY;
			}
			else
				return PUT_SUCCESS;
		}
	}
	else
	{
		DOM_UserJSMagicCallback *callback = DOM_HOSTOBJECT(restart_object, DOM_UserJSMagicCallback);

		switch (callback->GetStatus())
		{
		case ES_ASYNC_SUCCESS:
		case ES_ASYNC_FAILURE:
		case ES_ASYNC_CANCELLED:
			return PUT_SUCCESS;

		case ES_ASYNC_EXCEPTION:
			*value = callback->GetValue();
			return PUT_EXCEPTION;

		default:
			return PUT_NO_MEMORY;
		}
	}
}

#endif // USER_JAVASCRIPT
