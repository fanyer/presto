/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/ecmascript_utils/estimerevent.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/estimermanager.h"
#include "modules/ecmascript/ecmascript.h"

/* --- ES_TimerEvent ------------------------------------------------- */

ES_TimerEvent::ES_TimerEvent(double delay, BOOL is_repeating)
	: timer_manager(NULL),
	  delay(delay),
	  timeout_time(0),
	  next_timeout_time(0),
	  previous_delay(0),
	  is_repeating(is_repeating),
	  id(0)
#ifdef SCOPE_PROFILER
	, scope_thread_id(0)
#endif // SCOPE_PROFILER
{
}

/* virtual */
ES_TimerEvent::~ES_TimerEvent()
{
}

BOOL
ES_TimerEvent::IsRepeating()
{
	return is_repeating;
}

double
ES_TimerEvent::GetDelay()
{
	return delay;
}

ES_TimerManager *
ES_TimerEvent::GetTimerManager()
{
	return timer_manager;
}

void
ES_TimerEvent::SetTimerManager(ES_TimerManager *timer_manager_)
{
	timer_manager = timer_manager_;
}

double
ES_TimerEvent::GetTotalRequestedDelay()
{
	return delay + previous_delay;
}

void
ES_TimerEvent::SetPreviousDelay(double new_previous_delay)
{
	previous_delay = new_previous_delay;
}

void
ES_TimerEvent::SetTimeout()
{
	if (is_repeating && next_timeout_time != 0)
	{
		timeout_time = next_timeout_time;
		next_timeout_time = 0;
	}
	else
		timeout_time = g_op_time_info->GetRuntimeMS() + delay;
}

double
ES_TimerEvent::GetTimeout()
{
	double now_time = g_op_time_info->GetRuntimeMS();

	if (now_time >= timeout_time)
		return 0;
	else
		return timeout_time - now_time;
}

void
ES_TimerEvent::SetNextTimeout()
{
	if (IsRepeating())
		next_timeout_time = g_op_time_info->GetRuntimeMS() + delay;
}

BOOL
ES_TimerEvent::TimeoutBefore(ES_TimerEvent *other)
{
	return timeout_time < other->timeout_time;
}

BOOL
ES_TimerEvent::TimeoutExpired()
{
	return GetTimeout() == 0;
}

void
ES_TimerEvent::StopRepeating()
{
	is_repeating = FALSE;
}

unsigned int
ES_TimerEvent::GetId()
{
	OP_ASSERT(id > 0);
	return id;
}

void
ES_TimerEvent::SetId(unsigned int id_)
{
	OP_ASSERT(id == 0);
	id = id_;
}

#ifdef SCOPE_PROFILER
void
ES_TimerEvent::SetScopeThreadId(unsigned int id)
{
	OP_ASSERT(scope_thread_id == 0);
	scope_thread_id = id;
}

unsigned
ES_TimerEvent::GetScopeThreadId()
{
	OP_ASSERT(scope_thread_id != 0);
	return scope_thread_id;
}
#endif // SCOPE_PROFILER

/* --- ES_TimeoutTimerEvent ------------------------------------------ */

ES_TimeoutTimerEvent::ES_TimeoutTimerEvent(ES_Runtime *runtime, double delay, BOOL is_repeating, ES_Thread *origin_thread)
	: ES_TimerEvent(delay, is_repeating),
	  runtime(runtime),
	  origin_info(origin_thread->GetOriginInfo()),
	  from_plugin_origin(origin_thread->IsPluginThread()),
	  program(NULL),
	  callable(NULL),
	  callable_argv(NULL),
	  callable_argc(0)
{
	shared = origin_thread->GetSharedInfo();
	if (shared)
		shared->IncRef();

	if (origin_thread->Type() == ES_THREAD_TIMEOUT)
		SetPreviousDelay(static_cast<ES_TimeoutThread *>(origin_thread)->GetTimerEvent()->GetTotalRequestedDelay());
}

/* virtual */
ES_TimeoutTimerEvent::~ES_TimeoutTimerEvent()
{
	if (shared)
		shared->DecRef();

	ResetProgramAndCallable();
}

void
ES_TimeoutTimerEvent::SetProgram(ES_Program *program_, BOOL push_program_)
{
	OP_ASSERT(program_);
	OP_ASSERT(!callable);
	OP_ASSERT(!program);

	program = program_;
	push_program = push_program_;
}

OP_STATUS
ES_TimeoutTimerEvent::SetCallable(ES_Object *callable_, ES_Value* argv_, int argc_)
{
	OP_ASSERT(callable_);
	OP_ASSERT(!callable);
	OP_ASSERT(!program);

	callable = callable_;

	if (!runtime->Protect(callable))
	{
		callable = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	if ((callable_argc = argc_) > 0)
	{
		if (!(callable_argv = OP_NEWA(ES_Value, argc_)))
		{
			callable_argc = 0;
			return OpStatus::ERR_NO_MEMORY;
		}

		for ( int i=0 ; i < argc_ ; i++ )
		{
			callable_argv[i] = argv_[i];

			if (callable_argv[i].type == VALUE_OBJECT && !runtime->Protect(callable_argv[i].value.object) ||
			    callable_argv[i].type == VALUE_STRING && !(callable_argv[i].value.string = SetNewStr(callable_argv[i].value.string)))
			{
				callable_argv[i].type = VALUE_UNDEFINED;
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	return OpStatus::OK;
}

void
ES_TimeoutTimerEvent::ResetProgramAndCallable()
{
	if (program)
	{
		ES_Runtime::DeleteProgram(program);
		program = NULL;
	}
	else if (callable)
	{
		runtime->Unprotect(callable);
		callable = NULL;

		for (int index = 0; index < callable_argc ; ++index)
		{
			if (callable_argv[index].type == VALUE_OBJECT)
				runtime->Unprotect(callable_argv[index].value.object);
			else if (callable_argv[index].type == VALUE_STRING)
			{
				uni_char *todelete = const_cast<uni_char *>(callable_argv[index].value.string);
				OP_DELETEA(todelete);
			}
		}

		OP_DELETEA(callable_argv);
		callable_argv = NULL;
		callable_argc = 0;
	}
}

/* virtual */ OP_BOOLEAN
ES_TimeoutTimerEvent::Expire()
{
	ES_Context *context = runtime->CreateContext(NULL);
	if (!context)
		return OpStatus::ERR_NO_MEMORY;

	ES_TimeoutThread *thread = OP_NEW(ES_TimeoutThread, (context, shared));
	if (!thread)
	{
		ES_Runtime::DeleteContext(context);
		return OpStatus::ERR_NO_MEMORY;
	}

	thread->SetOriginInfo(origin_info);
	thread->SetTimerEvent(this);
#ifdef SCOPE_PROFILER
	thread->SetThreadId(GetScopeThreadId());
#endif // SCOPE_PROFILER

	if (from_plugin_origin)
		thread->SetIsPluginThread();

	OP_BOOLEAN stat = runtime->GetESScheduler()->AddRunnable(thread);
	if (stat == OpBoolean::IS_TRUE)
		/* Thread added to scheduler. Add listener after AddRunnable() since it
		   deletes the thread and all its listeners if scheduler is draining. */
		thread->AddListener(this);
	else if (stat == OpBoolean::IS_FALSE)
		/* Thread not added. Make it repeat here. */
		if (IsRepeating())
		{
			SetNextTimeout();
			SetPreviousDelay(GetTotalRequestedDelay());
			if (OpStatus::IsMemoryError(GetTimerManager()->RepeatEvent(this)))
				return OpBoolean::ERR_NO_MEMORY;
			return OpBoolean::IS_TRUE;
		}

	return stat;
}

/* virtual */ OP_STATUS
ES_TimeoutTimerEvent::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_CANCELLED:
		ES_ThreadListener::Remove();

		if (IsRepeating())
		{
			SetPreviousDelay(GetTotalRequestedDelay());
			return GetTimerManager()->RepeatEvent(this);
		}
		else
			GetTimerManager()->RemoveExpiredEvent(this);
		break;

	default:
		break;
	}

	return OpStatus::OK;
}
