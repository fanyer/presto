/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMSUSPENDCALLLBACK_H
#define DOM_DOMSUSPENDCALLLBACK_H

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcallstate.h"
#include "modules/ecmascript_utils/esthread.h"

class OpFunctionObjectBase;

class DOM_SuspendCallbackBase : public ES_ThreadListener
{
public:
	DOM_SuspendCallbackBase();
	virtual ~DOM_SuspendCallbackBase() {}

	virtual OP_STATUS Construct() { return OpStatus::OK; }

	BOOL WasCalled(){ return m_was_called; }
	BOOL IsAsync()	{ return m_is_async; }
	BOOL HasFailed() { return OpStatus::IsError(m_error_code); }
	virtual OP_STATUS GetErrorCode() { return m_error_code; }

	template<class ImplClassName>
	static ImplClassName* FromCallState(DOM_CallState* call_state)
	{
		return static_cast<ImplClassName*>(reinterpret_cast<DOM_SuspendCallbackBase*>(call_state->GetUserData()));
	}

	void SuspendESThread(DOM_Runtime* origining_runtime);

	//from ES_ThreadListener
	virtual OP_STATUS Signal(ES_Thread* thread, ES_ThreadSignal signal);
public: // The methods below should be protected, but it doesnt seem to work in brew compiler
	void SetAsync()	{ m_is_async = TRUE;}
	// Must be called by derived class implementation when everything finished successfully
	// This can delete the current object so no operation on members should be done after calling it.
	void OnSuccess();
	// Must be called by derived class implementation when something fails
	// This can delete the current object so no operation on members should be done after calling it.
	void OnFailed(OP_STATUS status);
private:
	void CallFinished();
	BOOL m_was_called;
	BOOL m_is_async;
	OP_STATUS m_error_code;
	ES_Thread* m_thread;
};

template <class CallbackInterfaceType>
class DOM_SuspendCallback : public DOM_SuspendCallbackBase, public CallbackInterfaceType
{
public:
	typedef CallbackInterfaceType CallbackInterface;
};

#define NEW_SUSPENDING_CALLBACK(type, var, callstate_val, call, constructor_params) \
	type* var = NULL;																	\
	if (DOM_CallState::GetPhaseFromESValue(callstate_val) < call.GetPhase())			\
		var = OP_NEW(type, constructor_params);											\

class DOM_SuspendingCall // name - maybe DOM_SuspendingCaller or sth?
{
public:
	DOM_SuspendingCall(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Value* restart_value, DOM_Runtime* origining_runtime, DOM_CallState::CallPhase phase)
		: m_this_object(this_object)
		, m_argv(argv)
		, m_argc(argc)
		, m_return_value(return_value)
		, m_restart_value(restart_value)
		, m_origining_runtime(origining_runtime)
		, m_callback(NULL)
		, m_phase(phase)
		, m_error_code(OpStatus::OK)
	{
	}

	virtual ~DOM_SuspendingCall();

	void CallFunctionSuspending(OpFunctionObjectBase* function, DOM_SuspendCallbackBase*& callback);

	OP_STATUS GetCallStatus() { return m_error_code; }
	DOM_CallState::CallPhase GetPhase(){ return m_phase;}

public:
	/**
	 *
	 */
	class ES_ErrorCodeConverter
	{
	public:
		enum Errors
		{
			ECC_NO_ERROR = 0,
			ECC_SUSPEND = 1,
			ECC_NO_MEMORY = 2,
			ECC_SECURITY_VIOLATION = 3,
			ECC_EXCEPTION = 4
		};

		ES_ErrorCodeConverter(Errors error): m_error(error){}

		BOOL IsError() { return m_error != ECC_NO_ERROR; }

		operator int()
		{
			switch (m_error)
			{
			case ECC_SUSPEND:
				return ES_SUSPEND | ES_RESTART;
			case ECC_SECURITY_VIOLATION:
				return ES_EXCEPT_SECURITY;
			case ECC_NO_MEMORY:
				return ES_NO_MEMORY;
			case ECC_EXCEPTION:
				return ES_EXCEPTION;
			default:
				OP_ASSERT(FALSE);
				return ES_FAILED;
			}
		}

		operator ES_GetState()
		{
			switch (m_error)
			{
			case ECC_SUSPEND:
				return GET_SUSPEND;
			case ECC_SECURITY_VIOLATION:
				return GET_SECURITY_VIOLATION;
			case ECC_NO_MEMORY:
				return GET_NO_MEMORY;
			case ECC_EXCEPTION:
				return GET_EXCEPTION;
			default:
				OP_ASSERT(FALSE);
				return GET_FAILED;
			}
		}

		operator ES_PutState()
		{
			switch (m_error)
			{
			case ECC_SUSPEND:
				return PUT_SUSPEND;
			case ECC_SECURITY_VIOLATION:
				return PUT_SECURITY_VIOLATION;
			case ECC_NO_MEMORY:
				return PUT_NO_MEMORY;
			case ECC_EXCEPTION:
				return PUT_EXCEPTION;
			default:
				OP_ASSERT(FALSE);
				return PUT_FAILED;
			}
		}
	private:
		Errors m_error;
	};
public:
	DOM_Object*		m_this_object;
	ES_Value*		m_argv;
	int				m_argc;
	ES_Value*		m_return_value;
	ES_Value*		m_restart_value;
	DOM_Runtime*	m_origining_runtime;
	DOM_SuspendCallbackBase* m_callback;
	DOM_CallState::CallPhase m_phase;
	OP_STATUS m_error_code;
};

#define DOM_SUSPENDING_CALL(call, function_obj, CallbackType, callback)												\
do {																												\
	DOM_CHECK_OR_RESTORE_GUARD;																						\
	/* casting to base class */																								\
	DOM_SuspendCallbackBase* callback_base = callback;																\
	/* modifies callback_base when we are restarting */																				\
	call.CallFunctionSuspending(&function_obj, callback_base);														\
	switch (call.GetCallStatus())																					\
	{																												\
	case OpStatus::OK:																								\
		break;																										\
	case OpStatus::ERR_YIELD:																						\
		return DOM_SuspendingCall::ES_ErrorCodeConverter(DOM_SuspendingCall::ES_ErrorCodeConverter::ECC_SUSPEND);	\
	case OpStatus::ERR_NO_MEMORY:																					\
		return DOM_SuspendingCall::ES_ErrorCodeConverter(DOM_SuspendingCall::ES_ErrorCodeConverter::ECC_NO_MEMORY);	\
	default:																										\
		break;		/*other errors can't be handled in a generic way*/																\
	}																												\
	/* ... and here we downcast back the callback which was restored from the CallState	*/														\
	callback = static_cast<CallbackType*>(callback_base);															\
} while (0)

class OpFunctionObjectBase
{
public:
	virtual void Call() = 0;
};

// Autogenerated templates for concrete function objects
#include "modules/dom/src/domsuspendcallback.inc"
#endif // DOM_DOMSUSPENDCALLLBACK_H
