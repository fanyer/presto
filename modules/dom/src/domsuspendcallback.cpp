/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domsuspendcallback.h"

DOM_SuspendCallbackBase::DOM_SuspendCallbackBase()
	: m_was_called(FALSE)
	, m_is_async(FALSE)
	, m_error_code(OpStatus::OK)
	, m_thread(NULL)
	{}

void DOM_SuspendCallbackBase::SuspendESThread(DOM_Runtime* origining_runtime)
{
	OP_ASSERT(origining_runtime);
	SetAsync();
	m_thread = DOM_Object::GetCurrentThread(origining_runtime);
	OP_ASSERT(m_thread);
	m_thread->AddListener(this);
	m_thread->Block();
}

void DOM_SuspendCallbackBase::OnSuccess()
{
	m_error_code = OpStatus::OK;
	DOM_SuspendCallbackBase::CallFinished();
}
void DOM_SuspendCallbackBase::OnFailed(OP_STATUS error)
{
	m_error_code = error;
	DOM_SuspendCallbackBase::CallFinished();
}

void DOM_SuspendCallbackBase::CallFinished()
{
	OP_ASSERT(!m_was_called);
	m_was_called = TRUE;
	if (IsAsync())
	{
		if (m_thread)
			m_thread->Unblock();
		else
		{
			// if the thread has already been terminated then unblocking it is pointless
			// lets just destroy this object to avoid memory leaks
			OP_DELETE(this);
		}
	}
}

/* virtual */
OP_STATUS DOM_SuspendCallbackBase::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	OP_ASSERT(m_thread == thread);
	switch (signal)
	{
	case ES_SIGNAL_SCHEDULER_TERMINATED:
		break; // Doesn't matter to us
	case ES_SIGNAL_CANCELLED:
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
		m_thread = NULL;
		Remove();	// Prevent deletion of this object
		break;
	default:
		OP_ASSERT(FALSE);
	}
	return OpStatus::OK;
}

void DOM_SuspendingCall::CallFunctionSuspending(OpFunctionObjectBase* function, DOM_SuspendCallbackBase*& callback)
{
	OP_ASSERT(m_restart_value);
	DOM_CallState* call_state = DOM_CallState::FromReturnValue(m_restart_value);
	OP_ASSERT(function);
	if (!call_state)
	{
		m_error_code = DOM_CallState::Make(call_state, m_origining_runtime, m_argv, m_argc);
		RETURN_VOID_IF_ERROR(m_error_code);
		DOM_Object::DOMSetObject(m_restart_value, call_state);	// Pretend there is restart state even in synchronous calls.
	}
	if (call_state->GetPhase() < m_phase)
	{
		if (!callback)
		{
			m_error_code = OpStatus::ERR_NO_MEMORY; // we got oom in PREPARE_SUSPENDING_CALLBACK.
			return;
		}
		m_error_code = callback->Construct();
		RETURN_VOID_IF_ERROR(m_error_code);
		m_callback = callback;

		function->Call();
		call_state->SetPhase(m_phase);
		call_state->SetUserData(m_callback);
		if (!callback->WasCalled())
		{
			m_error_code = call_state->PrepareForSuspend();
			RETURN_VOID_IF_ERROR(m_error_code); // as SuspendESThread wasn't called the thread wasn't set then then the async callback is finished
												// it will just delete itself and ignore results(as if the calling ES_thread was cancelled).
			DOM_Object::DOMSetObject(m_return_value, call_state);
			callback->SuspendESThread(m_origining_runtime);
			m_error_code = OpStatus::ERR_YIELD;
			return;
		}
	}
	else if (call_state->GetPhase() == m_phase)
	{
		callback = reinterpret_cast<DOM_SuspendCallbackBase*>(call_state->GetUserData());
		m_callback = callback;
	}
}

DOM_SuspendingCall::~DOM_SuspendingCall()
{
	if (m_callback && m_callback->WasCalled())
		OP_DELETE(m_callback);
}

