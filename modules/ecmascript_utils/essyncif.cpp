/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ESUTILS_SYNCIF_SUPPORT

#include "modules/ecmascript_utils/essyncif.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"

#ifndef MESSAGELOOP_RUNSLICE_SUPPORT
# error "MESSAGELOOP_RUNSLICE_SUPPORT is required for ES_SyncInterface support."
#endif // MESSAGELOOP_RUNSLICE_SUPPORT

class ES_SyncAsyncCallback
	: public ES_AsyncCallback
{
protected:
	BOOL finished, terminated;
	ES_SyncInterface::Callback *callback;
	OP_STATUS status;

public:
	ES_SyncAsyncCallback(ES_SyncInterface::Callback *callback)
		: finished(FALSE),
		  terminated(FALSE),
		  callback(callback),
		  status(OpStatus::OK)
	{
		OpStatus::Ignore(status);
	}

	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus async_status, const ES_Value &result)
	{
		finished = TRUE;

		if (terminated)
		{
			ES_SyncAsyncCallback *cb = this;
			OP_DELETE(cb);
		}
		else if (callback)
		{
			ES_SyncInterface::Callback::Status sync_status = ES_SyncInterface::Callback::ESSYNC_STATUS_SUCCESS;

			switch (async_status)
			{
			case ES_ASYNC_FAILURE:   sync_status = ES_SyncInterface::Callback::ESSYNC_STATUS_FAILURE; break;
			case ES_ASYNC_EXCEPTION: sync_status = ES_SyncInterface::Callback::ESSYNC_STATUS_EXCEPTION; break;
			case ES_ASYNC_NO_MEMORY: sync_status = ES_SyncInterface::Callback::ESSYNC_STATUS_NO_MEMORY; break;
			case ES_ASYNC_CANCELLED: sync_status = ES_SyncInterface::Callback::ESSYNC_STATUS_CANCELLED; break;
			}

			status = callback->HandleCallback(sync_status, result);
		}
		else switch (async_status)
		{
		case ES_ASYNC_FAILURE:
		case ES_ASYNC_EXCEPTION:
		case ES_ASYNC_CANCELLED: status = OpStatus::ERR; break;
		case ES_ASYNC_NO_MEMORY: status = OpStatus::ERR_NO_MEMORY; break;
		}

		return OpStatus::OK;
	}

	BOOL IsFinished() { return finished; }
	void Terminate()
	{
		terminated = TRUE;
		if (finished)
		{
			ES_SyncAsyncCallback *cb = this;
			OP_DELETE(cb);
		}
	}
	OP_STATUS GetStatus() { return status; }
};

#ifdef ESUTILS_ES_ENVIRONMENT_SUPPORT
# include "modules/ecmascript_utils/esenvironment.h"

ES_SyncInterface::ES_SyncInterface(ES_Environment *environment)
	: runtime(environment->GetRuntime()),
	  asyncif(environment->GetAsyncInterface())
{
}

#endif // ESUTILS_ES_ENVIRONMENT_SUPPORT

ES_SyncInterface::ES_SyncInterface(ES_Runtime *runtime, ES_AsyncInterface *asyncif)
	: runtime(runtime),
	  asyncif(asyncif)
{
}

class ES_SyncRunInProgress
	: public ES_ThreadListener
{
public:
	ES_SyncRunInProgress(ES_Thread *thread)
		: previous(g_opera->ecmascript_utils_module.sync_run_in_progress),
		  thread(thread)
	{
		g_opera->ecmascript_utils_module.sync_run_in_progress = this;
		thread->AddListener(this);
	}
	~ES_SyncRunInProgress()
	{
		g_opera->ecmascript_utils_module.sync_run_in_progress = previous;
	}

	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		switch (signal)
		{
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
		case ES_SIGNAL_CANCELLED:
			ES_ThreadListener::Remove();
			thread = NULL;
		}
		return OpStatus::OK;
	}

	static ES_Thread *GetInterruptThread()
	{
		ES_SyncRunInProgress *in_progress = g_opera->ecmascript_utils_module.sync_run_in_progress;
		while (in_progress && !in_progress->thread)
			in_progress = in_progress->previous;
		return in_progress ? in_progress->thread : NULL;
	}

private:
	ES_SyncRunInProgress *previous;
	ES_Thread *thread;
};


static OP_STATUS
ES_SyncRun(ES_SyncAsyncCallback &callback, BOOL allow_nested_message_loop, unsigned timeslice, ES_Thread *thread)
{
	OP_NEW_DBG("ES_SyncRun", "es_utils");
	ES_SyncRunInProgress in_progress(thread);

	if (timeslice == 0)
		timeslice = ESUTILS_SYNC_TIMESLICE_LENGTH;

	ES_ThreadScheduler* scheduler = thread->GetScheduler();
	BOOL leave_thread_alive = allow_nested_message_loop;
	OP_STATUS status = scheduler->ExecuteThread(thread, timeslice, leave_thread_alive);
	if (OpStatus::IsError(status))
	{
		OP_DBG(("ExecuteThread failed so syncrun terminated"));
		callback.Terminate();
		return status;
	}

	if (!callback.IsFinished())
	{
		OP_ASSERT(allow_nested_message_loop);
		OP_DBG(("Thread couldn't complete so entering nested message loop"));

		OP_STATUS status;
		thread->SetIsSyncThread();

		// Make the scheduler (should really be all schedulers) think it's
		// not active in case it's active

		BOOL was_active = scheduler->IsActive();
		if (was_active)
			scheduler->PushNestedActivation();
		do
			if (OpStatus::IsError(status = g_opera->RequestRunSlice()))
			{
				callback.Terminate();
				break;
			}
		while (!callback.IsFinished());

		thread = NULL; // Is probably deleted
		if (was_active)
			scheduler->PopNestedActivation();

		return status;
	}
	else
	{
		if (allow_nested_message_loop)
			OP_DBG(("Able to complete thread without entering nested message loop"));
		else
			OP_DBG(("Thread could complete directly"));
	}

	return OpStatus::OK;
}

ES_SyncInterface::EvalData::EvalData()
	: program(NULL),
	  program_array(NULL),
	  program_array_length(0),
	  scope_chain(NULL),
	  scope_chain_length(0),
	  this_object(NULL),
	  interrupt_thread(NULL),
	  want_exceptions(FALSE),
	  want_string_result(FALSE),
	  allow_nested_message_loop(TRUE),
	  max_timeslice(0)
{
}

OP_STATUS
ES_SyncInterface::Eval(const EvalData &data, Callback *callback)
{
	OP_ASSERT(data.program || data.program_array);

	ES_SyncAsyncCallback *async_callback = OP_NEW(ES_SyncAsyncCallback, (callback));
	if (!async_callback)
		return OpStatus::ERR_NO_MEMORY;

	ES_ProgramText *program_array, program_array_single;
	int program_array_length;

	if (data.program)
	{
		program_array_single.program_text = data.program;
		program_array_single.program_text_length = uni_strlen(data.program);
		program_array = &program_array_single;
		program_array_length = 1;
	}
	else
	{
		program_array = data.program_array;
		program_array_length = data.program_array_length;
	}

	if (data.want_exceptions)
		asyncif->SetWantExceptions();

	if (data.want_string_result)
		asyncif->SetWantStringResult();

	ES_Thread *interrupt_thread = data.interrupt_thread;
	if (!interrupt_thread)
		interrupt_thread = ES_SyncRunInProgress::GetInterruptThread();

	OP_STATUS status = asyncif->Eval(program_array, program_array_length, data.scope_chain, data.scope_chain_length, async_callback, interrupt_thread, data.this_object);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(async_callback);
		return status;
	}
	asyncif->GetLastStartedThread()->SetIsSoftInterrupt();
	RETURN_IF_ERROR(ES_SyncRun(*async_callback, data.allow_nested_message_loop, data.max_timeslice, asyncif->GetLastStartedThread()));

	status = async_callback->GetStatus();

	OP_ASSERT(async_callback->IsFinished());
	OP_DELETE(async_callback);

	return status;
}

ES_SyncInterface::CallData::CallData()
	: this_object(NULL),
	  function(NULL),
	  method(NULL),
	  arguments(NULL),
	  arguments_count(0),
	  interrupt_thread(NULL),
	  want_exceptions(FALSE),
	  want_string_result(FALSE),
	  allow_nested_message_loop(TRUE),
	  max_timeslice(0)
{
}

OP_STATUS
ES_SyncInterface::Call(const CallData &data, Callback *callback)
{
	OP_ASSERT(data.this_object && data.method || data.function);

	ES_SyncAsyncCallback *async_callback = OP_NEW(ES_SyncAsyncCallback, (callback));
	if (!async_callback)
		return OpStatus::ERR_NO_MEMORY;

	if (data.want_exceptions)
		asyncif->SetWantExceptions();

	if (data.want_string_result)
		asyncif->SetWantStringResult();

	ES_Thread *interrupt_thread = data.interrupt_thread;
	if (!interrupt_thread)
		interrupt_thread = ES_SyncRunInProgress::GetInterruptThread();

	OP_STATUS status;
	if (data.function)
		status = asyncif->CallFunction(data.function, data.this_object, data.arguments_count, data.arguments, async_callback, interrupt_thread);
	else
		status = asyncif->CallMethod(data.this_object, data.method, data.arguments_count, data.arguments, async_callback, interrupt_thread);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(async_callback);
		return status;
	}
	RETURN_IF_ERROR(ES_SyncRun(*async_callback, data.allow_nested_message_loop, data.max_timeslice, asyncif->GetLastStartedThread()));

	status = async_callback->GetStatus();

	OP_ASSERT(async_callback->IsFinished());
	OP_DELETE(async_callback);

	return status;
}

#ifdef ESUTILS_SYNCIF_PROPERTIES_SUPPORT

ES_SyncInterface::SlotData::SlotData()
	: object(NULL),
	  name(NULL),
	  index(0),
	  interrupt_thread(NULL),
	  want_exceptions(FALSE),
	  want_string_result(FALSE),
	  allow_nested_message_loop(TRUE),
	  max_timeslice(0)
{
}

OP_STATUS
ES_SyncInterface::GetSlot(const SlotData &data, Callback *callback)
{
	ES_SyncAsyncCallback *async_callback = OP_NEW(ES_SyncAsyncCallback, (callback));
	if (!async_callback)
		return OpStatus::ERR_NO_MEMORY;

	ES_Object *object = data.object;

	if (!object)
		object = (ES_Object *) runtime->GetGlobalObject();

	if (data.want_exceptions)
		asyncif->SetWantExceptions();

	if (data.want_string_result)
		asyncif->SetWantStringResult();

	ES_Thread *interrupt_thread = data.interrupt_thread;
	if (!interrupt_thread)
		interrupt_thread = ES_SyncRunInProgress::GetInterruptThread();

	OP_STATUS status;
	if (data.name)
		status = asyncif->GetSlot(object, data.name, async_callback, interrupt_thread);
	else
		status = asyncif->GetSlot(object, data.index, async_callback, interrupt_thread);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(async_callback);
		return status;
	}
	RETURN_IF_ERROR(ES_SyncRun(*async_callback, data.allow_nested_message_loop, data.max_timeslice, asyncif->GetLastStartedThread()));

	status = async_callback->GetStatus();

	OP_ASSERT(async_callback->IsFinished());
	OP_DELETE(async_callback);

	return status;
}

OP_STATUS
ES_SyncInterface::SetSlot(const SlotData &data, const ES_Value &value, Callback *callback)
{
	ES_SyncAsyncCallback *async_callback = OP_NEW(ES_SyncAsyncCallback, (callback));
	if (!async_callback)
		return OpStatus::ERR_NO_MEMORY;

	ES_Object *object = data.object;

	if (!object)
		object = (ES_Object *) runtime->GetGlobalObject();

	if (data.want_exceptions)
		asyncif->SetWantExceptions();

	if (data.want_string_result)
		asyncif->SetWantStringResult();

	ES_Thread *interrupt_thread = data.interrupt_thread;
	if (!interrupt_thread)
		interrupt_thread = ES_SyncRunInProgress::GetInterruptThread();

	OP_STATUS status;
	if (data.name)
		status = asyncif->SetSlot(object, data.name, value, async_callback, interrupt_thread);
	else
		status = asyncif->SetSlot(object, data.index, value, async_callback, interrupt_thread);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(async_callback);
		return status;
	}
	RETURN_IF_ERROR(ES_SyncRun(*async_callback, data.allow_nested_message_loop, data.max_timeslice, asyncif->GetLastStartedThread()));

	status = async_callback->GetStatus();

	OP_ASSERT(async_callback->IsFinished());
	OP_DELETE(async_callback);

	return status;
}

OP_STATUS
ES_SyncInterface::RemoveSlot(const SlotData &data, Callback *callback)
{
	ES_SyncAsyncCallback *async_callback = OP_NEW(ES_SyncAsyncCallback, (callback));
	if (!async_callback)
		return OpStatus::ERR_NO_MEMORY;

	ES_Object *object = data.object;

	if (!object)
		object = (ES_Object *) runtime->GetGlobalObject();

	if (data.want_exceptions)
		asyncif->SetWantExceptions();

	if (data.want_string_result)
		asyncif->SetWantStringResult();

	ES_Thread *interrupt_thread = data.interrupt_thread;
	if (!interrupt_thread)
		interrupt_thread = ES_SyncRunInProgress::GetInterruptThread();

	OP_STATUS status;
	if (data.name)
		status = asyncif->RemoveSlot(object, data.name, async_callback, interrupt_thread);
	else
		status = asyncif->RemoveSlot(object, data.index, async_callback, interrupt_thread);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(async_callback);
		return status;
	}
	RETURN_IF_ERROR(ES_SyncRun(*async_callback, data.allow_nested_message_loop, data.max_timeslice, asyncif->GetLastStartedThread()));

	status = async_callback->GetStatus();

	OP_ASSERT(async_callback->IsFinished());
	OP_DELETE(async_callback);

	return status;
}

OP_STATUS
ES_SyncInterface::HasSlot(const SlotData &data, Callback *callback)
{
	ES_SyncAsyncCallback *async_callback = OP_NEW(ES_SyncAsyncCallback, (callback));
	if (!async_callback)
		return OpStatus::ERR_NO_MEMORY;

	ES_Object *object = data.object;

	if (!object)
		object = (ES_Object *) runtime->GetGlobalObject();

	if (data.want_exceptions)
		asyncif->SetWantExceptions();

	if (data.want_string_result)
		asyncif->SetWantStringResult();

	ES_Thread *interrupt_thread = data.interrupt_thread;
	if (!interrupt_thread)
		interrupt_thread = ES_SyncRunInProgress::GetInterruptThread();

	OP_STATUS status;
	if (data.name)
		status = asyncif->HasSlot(object, data.name, async_callback, interrupt_thread);
	else
		status = asyncif->HasSlot(object, data.index, async_callback, interrupt_thread);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(async_callback);
		return status;
	}
	RETURN_IF_ERROR(ES_SyncRun(*async_callback, data.allow_nested_message_loop, data.max_timeslice, asyncif->GetLastStartedThread()));

	status = async_callback->GetStatus();

	OP_ASSERT(async_callback->IsFinished());
	OP_DELETE(async_callback);

	return status;
}

#endif // ESUTILS_SYNCIF_PROPERTIES_SUPPORT
#endif // ESUTILS_SYNCIF_SUPPORT
