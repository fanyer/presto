/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esevent.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/ecmascript_utils/estimermanager.h"
#include "modules/ecmascript_utils/estimerevent.h"
#include "modules/ecmascript_utils/esutils.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"

#include "modules/dochand/fdelm.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/stdlib/include/double_format.h"

#ifdef SCOPE_PROFILER
# include "modules/dom/domevents.h"
# include "modules/dom/domutils.h"
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_PROFILER

#ifdef SCOPE_PROFILER

/**
 * Custom probe for PROBE_EVENT_SCRIPT_THREAD_EVALUATION. We reduce noise in
 * the target function by splitting the activation into a separate class.
 */
class ES_ScriptThreadEvaluationProbe
	: public OpPropertyProbe
{
public:

	OP_STATUS Activate(OpProbeTimeline *timeline, ES_Thread *thread, unsigned thread_id);
	/**< Activate the ES_ScriptThreadEvaluationProbe.

	     @param timeline The timeline to report to. Can not be NULL.
	     @param thread The thread that is about to be evaluated.
	     @param thread_id The unique ID for the thread.

	     @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY. */
};

OP_STATUS
ES_ScriptThreadEvaluationProbe::Activate(OpProbeTimeline *timeline, ES_Thread *thread, unsigned thread_id)
{
	const void *key = reinterpret_cast<void*>(static_cast<UINTPTR>(thread_id));
	OpProbeEventType type = PROBE_EVENT_SCRIPT_THREAD_EVALUATION;

	OpPropertyProbe::Activator<2> act(*this);
	RETURN_IF_ERROR(act.Activate(timeline, type, key));

	ES_Thread *interrupted = thread->GetInterruptedThread();
	ES_Thread *target = thread;

	if (interrupted && interrupted->Type() == ES_THREAD_EVENT)
	{
		target = interrupted;

		const char *event_type = DOM_EventsAPI::GetEventTypeString(target->GetInfo().data.event.type);

		if (event_type)
			RETURN_IF_ERROR(act.AddString("event", event_type));
		else
			RETURN_IF_ERROR(act.AddString("event", DOM_Utils::GetCustomEventName(target)));
	}

	act.AddUnsigned("thread-type", static_cast<unsigned>(target->Type()));

	return OpStatus::OK;
}

# define SCRIPT_THREAD_EVALUATION_PROBE(TMP_PROBE, TMP_SCHEDULER, TMP_ID) \
	ES_ScriptThreadEvaluationProbe TMP_PROBE; \
	if (TMP_SCHEDULER->GetFramesDocument() && TMP_SCHEDULER->GetFramesDocument()->GetTimeline()) \
	{ \
		OpProbeTimeline *t = TMP_SCHEDULER->GetFramesDocument()->GetTimeline(); \
		RETURN_IF_ERROR(TMP_PROBE.Activate(t, this, TMP_ID)); \
	}

#else // SCOPE_PROFILER
# define SCRIPT_THREAD_EVALUATION_PROBE(TMP_PROBE, TMP_SCHEDULER, TMP_ID) ((void)0)
#endif // SCOPE_PROFILER

/* --- ES_ThreadListener --------------------------------------- */

/* virtual */
ES_ThreadListener::~ES_ThreadListener()
{
	Out();
}

void
ES_ThreadListener::Remove()
{
	Out();
}

/* --- ES_RestartObject ---------------------------------------------- */

void
ES_RestartObject::Push(ES_Thread *thread, ES_ThreadBlockType block_type)
{
	next = thread->restart_object_stack;
	thread->restart_object_stack = this;

	if (block_type != ES_BLOCK_NONE)
	{
		OP_ASSERT(thread->GetBlockType() == ES_BLOCK_NONE);
		thread->Block(block_type);
	}
}

ES_RestartObject *
ES_RestartObject::Pop(ES_Thread *thread, OperaModuleId module, int tag)
{
	ES_RestartObject *top = thread ? thread->restart_object_stack : NULL;
	if (top && top->module == module && top->tag == tag)
	{
		thread->restart_object_stack = top->next;
		top->next = NULL;
		return top;
	}
	else
		return NULL;
}

void
ES_RestartObject::Discard(ES_Thread *thread)
{
	OP_ASSERT(this->next == NULL);
	this->next = thread->restart_object_discarded;
	thread->restart_object_discarded = this;
}

/* --- ES_ThreadInfo ------------------------------------------------- */

ES_ThreadInfo::ES_ThreadInfo(ES_ThreadType type_, const ES_SharedThreadInfo *shared)
{
	if (shared)
	{
		is_user_requested = shared->is_user_requested;
		open_in_new_window = shared->open_in_new_window;
		open_in_background = shared->open_in_background;
		has_opened_new_window = shared->has_opened_new_window;
		has_opened_url = shared->has_opened_url;
		has_opened_file_upload_dialog = shared->has_opened_file_upload_dialog;
	}
	else
		op_memset(reinterpret_cast<unsigned char *>(this), 0, sizeof(ES_ThreadInfo));

	type = type_;

	/* The DOM code might support setting it, so we best set a default
	   value. */
	data.event.is_window_event = FALSE;
}

/* --- ES_Thread ----------------------------------------------------- */

ES_Thread::ES_Thread(ES_Context *context, ES_SharedThreadInfo *shared)
	: runtime(NULL)
	, context(context)
	, program(NULL)
	, callable(NULL)
	, callable_argv(NULL)
	, callable_argc(0)
	, scheduler(NULL)
	, block_type(ES_BLOCK_NONE)
	, interrupted_thread(NULL)
	, restart_object_stack(NULL)
	, restart_object_discarded(NULL)
	, interrupted_count(0)
	, recursion_depth(0)
	, is_started(FALSE)
	, is_completed(FALSE)
	, is_failed(FALSE)
	, is_suspended(FALSE)
	, is_signalled(FALSE)
	, returned_value(FALSE)
	, want_string_result(FALSE)
	, want_exceptions(FALSE)
	, allow_cross_origin_errors(FALSE)
	, push_program(TRUE)
	, is_sync_thread(FALSE)
	, is_external_script(FALSE)
	, is_plugin_thread(FALSE)
	, is_soft_interrupt(FALSE)
	, blocked_by_foreign_thread(0)
#ifdef SCOPE_PROFILER
	, thread_id(0)
#endif // SCOPE_PROFILER
{
	if (shared)
	{
		shared->IncRef();
		this->shared = shared;
	}
	else
	{
		// FIXME: Can't allocate memory in constructors since we can't
		// signal OOM
		this->shared = OP_NEW(ES_SharedThreadInfo, ());
	}
}

/* virtual */
ES_Thread::~ES_Thread()
{
	/* Don't delete executing threads. */
	OP_ASSERT(!is_started || is_completed || is_failed);

	/* Make sure threads that were once successfully added to a scheduler
	   are also sent one of the signals FINISHED, FAILED or CANCELLED. */
	OP_ASSERT(!scheduler || is_signalled);

	if (context)
		ES_Runtime::DeleteContext(context);

	/* Delete program or unprotect callable. */
	ResetProgramAndCallable();

	while (ES_ThreadListener *listener = (ES_ThreadListener *) listeners.First())
		OP_DELETE(listener);

	if (interrupted_thread)
		--interrupted_thread->interrupted_count;

	if (shared)
		shared->DecRef();

	while (ES_RestartObject *top = restart_object_discarded)
	{
		restart_object_discarded = top->next;
		OP_DELETE(top);
	}
}

extern BOOL ES_TimesliceExpired(void *);

/* virtual */
OP_STATUS
ES_Thread::EvaluateThread()
{
	OP_NEW_DBG("ES_Thread::EvaluateThread", "es_utils");
	OP_DBG(("thread=%p, ES_Context=%p", this, context));

	SCRIPT_THREAD_EVALUATION_PROBE(probe, scheduler, thread_id);

	ES_Eval_Status eval_ret;
	OP_STATUS ret = OpStatus::OK;
	OpStatus::Ignore(ret);

	if (!context)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	if (!is_started)
	{
		if (GetProgram() && GetPushProgram())
			RETURN_IF_ERROR(scheduler->GetRuntime()->PushProgram(context, GetProgram()));
		else if (GetCallable())
			RETURN_IF_ERROR(ES_Runtime::PushCall(context, GetCallable(), GetCallableArgc(), GetCallableArgv()));

		is_started = TRUE;
	}

	is_suspended = FALSE;

	eval_ret = ES_Runtime::ExecuteContext(context, WantStringResult(), WantExceptions(), AllowCrossOriginErrors(), &ES_TimesliceExpired, scheduler);

	switch (eval_ret)
	{
	case ES_SUSPENDED:
		is_suspended = TRUE;
		break;

	case ES_NORMAL:
		is_completed = TRUE;
		break;

	case ES_NORMAL_AFTER_VALUE:
		is_completed = returned_value = TRUE;
		break;

	case ES_ERROR:
		is_completed = is_failed = TRUE;
		break;

	case ES_ERROR_NO_MEMORY:
		ret = OpStatus::ERR_NO_MEMORY;
		break;

	case ES_BLOCKED:
		if (!IsBlocked())
			is_suspended = TRUE;
		break;

	case ES_THREW_EXCEPTION:
		OP_ASSERT(WantExceptions());
		is_completed = is_failed = returned_value = TRUE;
		break;

	default:
		ret = OpStatus::ERR;
		break;
	}

	if (OpStatus::IsError(ret))
	{
		is_completed = TRUE;
		is_failed = TRUE;
	}

	while (ES_RestartObject *top = restart_object_discarded)
	{
		restart_object_discarded = top->next;
		OP_DELETE(top);
	}

	return ret;
}

/* virtual */
ES_ThreadType
ES_Thread::Type()
{
	return ES_THREAD_COMMON;
}

/* virtual */
ES_ThreadInfo
ES_Thread::GetInfo()
{
	return ES_ThreadInfo(Type(), shared);
}

/* virtual */
ES_ThreadInfo
ES_Thread::GetOriginInfo()
{
	if (interrupted_thread)
		return interrupted_thread->GetOriginInfo();
	else
		return GetInfo();
}

/* virtual */
const uni_char *
ES_Thread::GetInfoString()
{
	if (interrupted_thread && interrupted_thread->Type() == ES_THREAD_EVENT)
		return interrupted_thread->GetInfoString();
	else
		return UNI_L("Unknown thread");
}

/* virtual */
void
ES_Thread::Block(ES_ThreadBlockType type)
{
	if (block_type == ES_BLOCK_FOREIGN_THREAD)
		SetBlockType(type);
	else
	{
		OP_ASSERT(GetBlockType() == ES_BLOCK_NONE);

		if (scheduler)
			scheduler->Block(this, type);
		else
			SetBlockType(type);
	}
}

/* virtual */
OP_STATUS
ES_Thread::Unblock(ES_ThreadBlockType type)
{
	OP_ASSERT(GetBlockType() == type);

	if (blocked_by_foreign_thread != 0)
		SetBlockType(ES_BLOCK_FOREIGN_THREAD);
	else if (scheduler)
		return scheduler->Unblock(this, type);
	else
		SetBlockType(ES_BLOCK_NONE);

	return OpStatus::OK;
}

/* virtual */
void
ES_Thread::Pause()
{
	Suspend();
	is_suspended = TRUE;
}

/* virtual */
void
ES_Thread::RequestTimeoutCheck()
{
	if (context)
		ES_Runtime::RequestTimeoutCheck(context);
}

/* virtual */
void
ES_Thread::GCTrace(ES_Runtime *runtime)
{
	ES_RestartObject *restart_object = restart_object_stack;

	while (restart_object)
	{
		restart_object->GCTrace(runtime);
		restart_object = restart_object->next;
	}
}

/* virtual */
void
ES_Thread::Suspend()
{
	if (context)
		ES_Runtime::SuspendContext(context);
}

/* virtual */
OP_STATUS
ES_Thread::Signal(ES_ThreadSignal signal)
{
	/* OOM strategy: if at least one of the listeners returns ERR_NO_MEMORY,
	   return ERR_NO_MEMORY, but always call all listeners.  Chanses are
	   things get really stuck if a listener isn't told when a thread
	   finishes, successfully or not. */

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (signal == ES_SIGNAL_FAILED || signal == ES_SIGNAL_CANCELLED)
		while (ES_RestartObject *top = restart_object_stack)
		{
			restart_object_stack = top->next;

			if (top->ThreadCancelled(this))
				OP_DELETE(top);
		}
	else
		OP_ASSERT(signal == ES_SIGNAL_SCHEDULER_TERMINATED || !restart_object_stack);

	if (signal == ES_SIGNAL_CANCELLED || signal == ES_SIGNAL_FAILED)
		is_completed = is_failed = TRUE;

	Head temporary1, temporary2;

	temporary1.Append(&listeners);
	while (ES_ThreadListener *listener = (ES_ThreadListener *) temporary1.First())
	{
		listener->Out();
		listener->Into(&temporary2);

		if (listener->Signal(this, signal) == OpStatus::ERR_NO_MEMORY)
			status = OpStatus::ERR_NO_MEMORY;
	}

	switch (signal)
	{
	case ES_SIGNAL_CANCELLED:
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_FINISHED:
		is_signalled = TRUE;
		temporary2.Clear();
		break;

	default:
		listeners.Append(&temporary2);
	}

	return status;
}

void
ES_Thread::AddListener(ES_ThreadListener *listener)
{
	OP_ASSERT(!listener->InList());
	OP_ASSERT(!is_signalled);
	OP_ASSERT(Type() == ES_THREAD_TIMEOUT || Type() == ES_THREAD_JAVASCRIPT_URL || !is_completed); // Or the listener will never be triggered
	listener->Into(&listeners);
}

/* virtual */ ES_Object *
ES_Thread::GetCallable()
{
	return callable;
}

/* virtual */ ES_Value *
ES_Thread::GetCallableArgv()
{
	return callable_argv;
}

/* virtual */ int
ES_Thread::GetCallableArgc()
{
	return callable_argc;
}

/* virtual */ ES_Program *
ES_Thread::GetProgram()
{
	return program;
}

/* virtual */ BOOL
ES_Thread::GetPushProgram()
{
	return push_program;
}

void
ES_Thread::SetProgram(ES_Program *program_, BOOL push_program_)
{
	OP_ASSERT(program_);
	OP_ASSERT(!GetProgram());
	OP_ASSERT(!GetCallable());

	program = program_;
	push_program = push_program_;
}

OP_STATUS
ES_Thread::SetCallable(ES_Runtime *runtime_, ES_Object *callable_, ES_Value* argv_, int argc_)
{
	OP_ASSERT(runtime_);
	OP_ASSERT(callable_);
	OP_ASSERT(!GetProgram());
	OP_ASSERT(!GetCallable());

	runtime = runtime_;
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
ES_Thread::SetBlockType(ES_ThreadBlockType type)
{
	block_type = type;
}

ES_ThreadBlockType
ES_Thread::GetBlockType()
{
	return block_type;
}

BOOL
ES_Thread::IsBlocked()
{
	return block_type != ES_BLOCK_NONE || Pred();
}

BOOL
ES_Thread::IsStarted()
{
	return is_started;
}

BOOL
ES_Thread::IsCompleted()
{
	return is_completed;
}

BOOL
ES_Thread::IsFailed()
{
	return is_failed;
}

BOOL
ES_Thread::IsSuspended()
{
	BOOL was_suspended = is_suspended;
	is_suspended = FALSE;
	return was_suspended;
}

BOOL
ES_Thread::IsSignalled()
{
	return is_signalled;
}

BOOL
ES_Thread::ReturnedValue()
{
	return returned_value;
}

OP_STATUS
ES_Thread::GetReturnedValue(ES_Value *value)
{
	if (returned_value)
		return ES_Runtime::GetReturnValue(context, value);
	else
	{
		value->type = VALUE_UNDEFINED;
		return OpStatus::OK;
	}
}

BOOL
ES_Thread::WantStringResult()
{
	return want_string_result;
}

void
ES_Thread::SetWantStringResult()
{
	want_string_result = TRUE;
}

BOOL
ES_Thread::WantExceptions()
{
	return want_exceptions;
}

void
ES_Thread::SetWantExceptions()
{
	want_exceptions = TRUE;
}

BOOL
ES_Thread::AllowCrossOriginErrors()
{
	return allow_cross_origin_errors;
}

void
ES_Thread::SetAllowCrossOriginErrors()
{
	allow_cross_origin_errors = TRUE;
}

void
ES_Thread::SetScheduler(ES_ThreadScheduler *scheduler_)
{
	OP_ASSERT(!scheduler || scheduler == scheduler_);
	scheduler = scheduler_;
}

void
ES_Thread::MigrateTo(ES_ThreadScheduler *scheduler_)
{
	OP_ASSERT(scheduler && scheduler != scheduler_);
	scheduler = scheduler_;

	if (context)
		scheduler->GetRuntime()->MigrateContext(context);
}

ES_ThreadScheduler *
ES_Thread::GetScheduler()
{
	return scheduler;
}

void
ES_Thread::SetInterruptedThread(ES_Thread *thread)
{
	if (interrupted_thread != thread)
	{
		if (interrupted_thread)
			--interrupted_thread->interrupted_count;

		interrupted_thread = thread;

		if (interrupted_thread)
		{
			recursion_depth = thread->recursion_depth + 1;
			++interrupted_thread->interrupted_count;
		}
	}
}

ES_Thread *
ES_Thread::GetInterruptedThread()
{
	return interrupted_thread;
}

ES_Thread *
ES_Thread::GetRunningRootThread()
{
	OP_ASSERT(!IsSignalled());

	ES_Thread *best_candidate = this;
	ES_Thread *t = this;
	while ((t = t->interrupted_thread) != NULL)
		if (!t->IsSignalled())
			best_candidate = t;

	return best_candidate;
}

BOOL
ES_Thread::IsOrHasInterrupted(ES_Thread *other_thread)
{
	ES_Thread *this_thread = this;

	while (this_thread && this_thread != other_thread)
		this_thread = this_thread->interrupted_thread;

	return this_thread && this_thread == other_thread;
}

ES_Context *
ES_Thread::GetContext()
{
	return context;
}

void
ES_Thread::IncBlockedByForeignThread()
{
	++blocked_by_foreign_thread;

	if (block_type == ES_BLOCK_NONE)
		Block(ES_BLOCK_FOREIGN_THREAD);
}

OP_STATUS
ES_Thread::DecBlockedByForeignThread()
{
	if (--blocked_by_foreign_thread == 0)
	{
		if (block_type == ES_BLOCK_FOREIGN_THREAD)
			return Unblock(ES_BLOCK_FOREIGN_THREAD);
	}

	return OpStatus::OK;
}


void
ES_Thread::UseOriginInfoFrom(ES_Thread* origin_thread)
{
	if (origin_thread->shared && origin_thread->shared != shared)
	{
		origin_thread->shared->IncRef();
		if (shared)
			shared->DecRef();
		shared = origin_thread->shared;
	}
}


BOOL
ES_Thread::HasOpenedNewWindow()
{
	return GetOriginInfo().has_opened_new_window;
}

void
ES_Thread::SetHasOpenedNewWindow()
{
	ES_Thread* thread = this;
	while (thread->interrupted_thread)
		thread = thread->interrupted_thread;

	if (thread->shared)
		thread->shared->has_opened_new_window = TRUE;
}

BOOL
ES_Thread::HasOpenedURL()
{
	return GetOriginInfo().has_opened_url;
}

void
ES_Thread::SetHasOpenedURL()
{
	ES_Thread* thread = this;
	while (thread->interrupted_thread)
		thread = thread->interrupted_thread;

	if (thread->shared)
		thread->shared->has_opened_url = TRUE;
}

BOOL
ES_Thread::HasOpenedFileUploadDialog()
{
	return GetOriginInfo().has_opened_file_upload_dialog;
}

void
ES_Thread::SetHasOpenedFileUploadDialog()
{
	ES_Thread* thread = this;
	while (thread->interrupted_thread)
		thread = thread->interrupted_thread;

	if (thread->shared)
		thread->shared->has_opened_file_upload_dialog = TRUE;
}

BOOL
ES_Thread::IsPluginThread()
{
	ES_Thread *thread = this;
	while (thread)
	{
		if (thread->is_plugin_thread)
			return TRUE;
		thread = thread->interrupted_thread;
	}
	return FALSE;
}

BOOL
ES_Thread::IsPluginActive()
{
	ES_Thread *thread = this;
	while (thread)
	{
		if (thread->is_plugin_active)
			return TRUE;
		thread = thread->interrupted_thread;
	}
	return FALSE;
}

void
ES_Thread::ResetProgramAndCallable()
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

#ifdef ECMASCRIPT_DEBUGGER

BOOL
ES_Thread::HasDebugInfo()
{
	return (context != NULL && ES_Runtime::IsContextDebugged(context)) ||
		   (program != NULL && ES_Runtime::HasDebugInfo(program));
}

#endif // ECMASCRIPT_DEBUGGER

/* --- ES_EmptyThread ------------------------------------------------ */

ES_EmptyThread::ES_EmptyThread()
	: ES_Thread(NULL)
{
}

/* virtual */
OP_STATUS
ES_EmptyThread::EvaluateThread()
{
	is_started = is_completed = TRUE;
	return OpStatus::OK;
}

ES_ThreadType
ES_EmptyThread::Type()
{
	return ES_THREAD_EMPTY;
}

/* --- ES_TimeoutThread ---------------------------------------------- */

ES_TimeoutThread::ES_TimeoutThread(ES_Context *context, ES_SharedThreadInfo *shared)
	: ES_Thread(context, shared),
	  origin_info(GetInfo()),
	  timer_event(NULL)
{
}

/* virtual */
ES_TimeoutThread::~ES_TimeoutThread()
{
}

/* virtual */
OP_STATUS
ES_TimeoutThread::EvaluateThread()
{
#ifdef DOCUMENT_EDIT_SUPPORT
	if (!IsCompleted() && scheduler->GetFramesDocument() && scheduler->GetFramesDocument()->GetDesignMode())
	{
		// We do not run timeouts or interval threads in designmode documents
		// but we need to keep track of them in case designmode is disabled
		is_completed = is_failed = TRUE;
	}
#endif // DOCUMENT_EDIT_SUPPORT

	if (!IsCompleted() && !IsFailed())
	{
		if (!is_started)
			timer_event->SetNextTimeout();

		OP_STATUS ret = ES_Thread::EvaluateThread();

		if (OpStatus::IsError(ret))
			return ret;
	}

	return OpStatus::OK;
}

/* virtual */
ES_ThreadType
ES_TimeoutThread::Type()
{
	return ES_THREAD_TIMEOUT;
}

/* virtual */
ES_ThreadInfo
ES_TimeoutThread::GetOriginInfo()
{
	ES_ThreadInfo ret = origin_info;
	if (shared)
	{
		ret.has_opened_new_window = shared->has_opened_new_window;
		ret.has_opened_url = shared->has_opened_url;
		ret.is_user_requested = shared->is_user_requested;
		ret.open_in_new_window = shared->open_in_new_window;
		ret.open_in_background = shared->open_in_background;
	}
	return ret;
}

void
ES_TimeoutThread::SetOriginInfo(const ES_ThreadInfo &info)
{
	origin_info = info;
	if (shared)
	{
		if (info.has_opened_new_window)
			shared->has_opened_new_window = TRUE;
		if (info.has_opened_url)
			shared->has_opened_url = TRUE;
		shared->is_user_requested = info.is_user_requested;
		shared->open_in_new_window = info.open_in_new_window;
		shared->open_in_background = info.open_in_background;
	}
}

/* virtual */
const uni_char *
ES_TimeoutThread::GetInfoString()
{
	char delay_string[32]; // ARRAY OK 2011-11-07 sof
	OpDoubleFormat::ToString(delay_string, GetTimerEvent()->GetDelay());

	uni_char *buffer = (uni_char *) g_memory_manager->GetTempBuf();

	uni_strcpy(buffer, UNI_L("Timeout thread: delay "));
	make_doublebyte_in_buffer(delay_string, op_strlen(delay_string), buffer + uni_strlen(buffer), op_strlen(delay_string) + 1);
	uni_strcat(buffer, UNI_L(" ms"));

	return buffer;
}

void
ES_TimeoutThread::StopRepeating()
{
	timer_event->StopRepeating();
}

unsigned int
ES_TimeoutThread::GetId()
{
	return timer_event->GetId();
}

ES_TimeoutTimerEvent *
ES_TimeoutThread::GetTimerEvent()
{
	return timer_event;
}

void
ES_TimeoutThread::SetTimerEvent(ES_TimeoutTimerEvent *event)
{
	timer_event = event;
}

/* virtual */ ES_Object *
ES_TimeoutThread::GetCallable()
{
	return timer_event->callable;
}

/* virtual */ ES_Value *
ES_TimeoutThread::GetCallableArgv()
{
	return timer_event->callable_argv;
}

/* virtual */ int
ES_TimeoutThread::GetCallableArgc()
{
	return timer_event->callable_argc;
}

/* virtual */ ES_Program *
ES_TimeoutThread::GetProgram()
{
	return timer_event->program;
}

/* virtual */ BOOL
ES_TimeoutThread::GetPushProgram()
{
	return timer_event->push_program;
}

/* --- ES_InlineScriptThread ----------------------------------------- */

ES_InlineScriptThread::ES_InlineScriptThread(ES_Context *context, ES_Program *program,
                                             ES_SharedThreadInfo *shared)
	: ES_Thread(context, shared)
{
	OP_ASSERT(program);
	SetProgram(program);
}

/* virtual */
ES_InlineScriptThread::~ES_InlineScriptThread()
{
	if (shared && shared->inserted_by_parser_count > 0 && !shared->has_reported_recursion_error)
		shared->inserted_by_parser_count--;
}

/* virtual */
ES_ThreadType
ES_InlineScriptThread::Type()
{
	return ES_THREAD_INLINE_SCRIPT;
}

/* virtual */
const uni_char *
ES_InlineScriptThread::GetInfoString()
{
	return UNI_L("Inline script thread");
}

/* --- ES_DebuggerEvalThread ----------------------------------------- */

ES_DebuggerEvalThread::ES_DebuggerEvalThread(ES_Context *context, ES_SharedThreadInfo *shared)
	: ES_Thread(context, shared)
{
}

/* virtual */
ES_ThreadType
ES_DebuggerEvalThread::Type()
{
	return ES_THREAD_DEBUGGER_EVAL;
}

/* virtual */
const uni_char *
ES_DebuggerEvalThread::GetInfoString()
{
	return UNI_L("Debugger eval thread");
}


/* --- ES_JavascriptURLThread ---------------------------------------- */

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define DH_ERRSTR(p,x) Str::##p##_##x
#else
#define DH_ERRSTR(p,x) x
#endif

ES_JavascriptURLThread::ES_JavascriptURLThread(ES_Context *context, uni_char *source, const URL &url,
											   BOOL write_result_to_document, BOOL write_result_to_url, BOOL is_reload,
											   BOOL want_utf8, ES_SharedThreadInfo* shared)
	: ES_Thread(context, shared),
	  source(source),
	  url(url),
	  write_result_to_document(write_result_to_document),
	  write_result_to_url(write_result_to_url),
	  is_reload(is_reload),
	  want_utf8(want_utf8)
#ifdef USER_JAVASCRIPT
	  , result(NULL)
	  , has_result(FALSE)
#endif // USER_JAVASCRIPT
	  , eval_state(STATE_INITIAL)
{
	SetWantStringResult();
}

ES_JavascriptURLThread::~ES_JavascriptURLThread()
{
	OP_DELETEA(source);
#ifdef USER_JAVASCRIPT
	OP_DELETEA(result);
#endif // USER_JAVASCRIPT
}

/* virtual */
OP_STATUS
ES_JavascriptURLThread::EvaluateThread()
{
	OP_STATUS ret = OpStatus::OK;

	switch (eval_state)
	{
	case STATE_INITIAL:
		{
			eval_state = STATE_SET_PROGRAM;

#ifdef USER_JAVASCRIPT
			DOM_Environment *environment = scheduler->GetFramesDocument()->GetDOMEnvironment();

			RETURN_IF_ERROR(environment->HandleJavascriptURL(this));

			if (IsBlocked())
				return OpStatus::OK;
#endif // USER_JAVASCRIPT
		}
		// fall through

	case STATE_SET_PROGRAM:
		if (source)
		{
			ES_ProgramText program_text;

			program_text.program_text = source;
			program_text.program_text_length = uni_strlen(source);

			ES_Runtime *runtime = scheduler->GetRuntime();

			ES_Program *program;
			OP_STATUS status;

			ES_Runtime::CompileProgramOptions options;
			options.generate_result = TRUE;
			options.global_scope = FALSE;
			options.script_type = SCRIPT_TYPE_JAVASCRIPT_URL;
			options.when = UNI_L("while loading");
			options.script_url = &url;
#ifdef ECMASCRIPT_DEBUGGER
			options.reformat_source = g_ecmaManager->GetWantReformatScript(runtime);
#endif // ECMASCRIPT_DEBUGGER

			if (OpStatus::IsSuccess(status = runtime->CompileProgram(&program_text, 1, &program, options)))
				if (program)
					SetProgram(program);
				else
					status = OpStatus::ERR;

			if (OpStatus::IsMemoryError(status))
			{
				is_completed = is_failed = TRUE;
				return status;
			}
			else if (OpStatus::IsError(status))
			{
				is_completed = is_failed = TRUE;
				return OpStatus::OK;
			}
		}
		eval_state = STATE_EVALUATE;
		// fall through

	case STATE_EVALUATE:
		{
			ret = ES_Thread::EvaluateThread();
			if (OpStatus::IsError(ret))
			{
				eval_state = STATE_HANDLE_RESULT;
				is_completed = TRUE;
				break;
			}
			else if (IsCompleted())
			{
				if (IsFailed())
				{
					eval_state = STATE_DONE;
					break;
				}
				else
#ifdef USER_JAVASCRIPT
					eval_state = STATE_SEND_USER_JS_AFTER;
#else // USER_JAVASCRIPT
					eval_state = STATE_HANDLE_RESULT;
#endif // USER_JAVASCRIPT
			}
			else
				break;
		}
		// fall through

#ifdef USER_JAVASCRIPT
	case STATE_SEND_USER_JS_AFTER:
		{
			DOM_Environment *environment = scheduler->GetFramesDocument()->GetDOMEnvironment();

			RETURN_IF_ERROR(environment->HandleJavascriptURLFinished(this));

			eval_state = STATE_HANDLE_RESULT;

			if (IsBlocked())
			{
				is_completed = FALSE;
				break;
			}
		}
		// fall through
#endif // USER_JAVASCRIPT

	case STATE_HANDLE_RESULT:
		{
			is_completed = TRUE;
			const uni_char *use_result = NULL;

#ifdef USER_JAVASCRIPT
			if (has_result)
				use_result = result;
			else
#endif // USER_JAVASCRIPT
				if (ReturnedValue())
				{
					ES_Value return_value;

					RETURN_IF_ERROR(GetReturnedValue(&return_value));

					if (return_value.type == VALUE_STRING)
						use_result = return_value.value.string;
				}

			FramesDocument *frames_doc = GetFramesDocument();
			OP_ASSERT(frames_doc); // Since we're executing we must be in a document
			if (use_result)
			{
				if (write_result_to_document)
				{
					// The HTML5 spec says that we should load this data exactly as if it had come
					// from an HTTP connection with content type text/html and status 200. This
					// is a very bad approximation of that.
					BOOL is_busy = scheduler->IsDraining() || !frames_doc->IsCurrentDoc();
					write_result_to_document = FALSE; // Since it's enough to do it once
					frames_doc->SetWaitForJavascriptURL(FALSE);

					if (!is_busy)
						if (HLDocProfile *hld_profile = frames_doc->GetHLDocProfile())
							is_busy = hld_profile->GetESLoadManager()->GetScriptGeneratingDoc();

					if (!is_busy)
					{
						if (GetOriginInfo().open_in_new_window)
							RETURN_IF_ERROR(DOM_Environment::OpenWindowWithData(use_result, frames_doc, this, GetOriginInfo().is_user_requested));
						else
						{
							// The ESOpen()/ESClose() calls are just done in order to create an
							// empty document in The Right Way(tm) in order to parse some data
							// into it as if it had been loaded from a URL, and not to set up
							// a document.write call like they are usually used for.
							ESDocException doc_exception; // Ignored
							RETURN_IF_ERROR(frames_doc->ESOpen(scheduler->GetRuntime(), &url, is_reload, NULL, NULL, &doc_exception));

							FramesDocument *new_frames_doc = frames_doc->GetDocManager()->GetCurrentDoc();
							RETURN_IF_ERROR(new_frames_doc->ESClose(scheduler->GetRuntime()));

							SetBlockType(ES_BLOCK_NONE);

							if (frames_doc != new_frames_doc)
								RETURN_IF_ERROR(new_frames_doc->GetLogicalDocument()->ParseHtmlFromString(use_result, uni_strlen(use_result), FALSE, FALSE, FALSE));
							else
								OP_ASSERT(!"ESOpen might have failed, but if it didn't then we're stuck with a hung thread in a document and nothing will work");

							new_frames_doc->ESStoppedGeneratingDocument();
						}
					}
				}

				if (write_result_to_url)
				{
					if (want_utf8)
					{
						UTF16toUTF8Converter converter;

						unsigned length = uni_strlen(use_result) * sizeof source[0], needed = converter.BytesNeeded(use_result, length);
						converter.Reset();

						char *data;
						if (needed < g_memory_manager->GetTempBufLen())
							data = (char *) g_memory_manager->GetTempBuf();
						else
							data = OP_NEWA(char, needed);

						if (!data)
						{
							ret = OpStatus::ERR_NO_MEMORY;
							break;
						}
						else
						{
							int read, written = converter.Convert(use_result, length, data, needed, &read);

							url.WriteDocumentData(URL::KNormal, data, written);
							url.WriteDocumentDataFinished();

							if (data != g_memory_manager->GetTempBuf())
								OP_DELETEA(data);
						}
					}
					else
					{
						url.WriteDocumentData(URL::KNormal, use_result, uni_strlen(use_result));
						url.WriteDocumentDataFinished();
					}
				}
			}
			else if (write_result_to_document)
			{
				// The HTML5 spec says this should be handled as a HTTP status 204, NO_CONTENT
				frames_doc->GetMessageHandler()->PostMessage(MSG_URL_LOADING_FAILED, url.Id(), DH_ERRSTR(SI,ERR_HTTP_NO_CONTENT));
				frames_doc->SetWaitForJavascriptURL(FALSE);
				write_result_to_document = FALSE; // Since it's done and we don't want to do it again.
			}

			eval_state = STATE_DONE;
		}
		// fall through

	case STATE_DONE:
		break;
	}

	if (write_result_to_url)
		RETURN_IF_ERROR(PostURLMessages(OpStatus::IsError(ret), FALSE));

	return ret;
}

/* virtual */
OP_STATUS
ES_JavascriptURLThread::Signal(ES_ThreadSignal signal)
{
	OP_STATUS status = ES_Thread::Signal(signal);
	BOOL failed = signal == ES_SIGNAL_FAILED;
	BOOL cancelled = signal == ES_SIGNAL_CANCELLED;

	if (failed || cancelled)
		if (OpStatus::IsMemoryError(PostURLMessages(failed, cancelled)))
		{
			OpStatus::Ignore(status);
			status = OpStatus::ERR_NO_MEMORY;
		}

	if (failed || cancelled || signal == ES_SIGNAL_FINISHED)
		if (write_result_to_document)
		{
			if (FramesDocument *frames_doc = GetFramesDocument())
			{
				frames_doc->GetMessageHandler()->PostMessage(MSG_URL_LOADING_FAILED, url.Id(), DH_ERRSTR(SI,ERR_HTTP_NO_CONTENT));
				frames_doc->SetWaitForJavascriptURL(FALSE);
			}
			write_result_to_document = FALSE; // Since it's done and we don't want to do it again.
		}

	return status;
}

/* virtual */
ES_ThreadType
ES_JavascriptURLThread::Type()
{
	return ES_THREAD_JAVASCRIPT_URL;
}

/* virtual */
const uni_char *
ES_JavascriptURLThread::GetInfoString()
{
	uni_char *buffer = (uni_char *) g_memory_manager->GetTempBuf();
	buffer[0] = 0;

	const uni_char *url_name = url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr();

	uni_strcat(buffer, UNI_L("Javascript URL thread: \""));
	if (uni_strlen(url_name) > (126 - uni_strlen(buffer)))
	{
		uni_strncat(buffer, url_name, (123 - uni_strlen(buffer)));
		uni_strcat(buffer, UNI_L("...\""));
		OP_ASSERT(uni_strlen(buffer) == 127);
	}
	else
	{
		uni_strcat(buffer, url_name);
		uni_strcat(buffer, UNI_L("\""));
	}

	return buffer;
}

#ifdef USER_JAVASCRIPT

OP_STATUS
ES_JavascriptURLThread::SetSource(const uni_char *new_source)
{
	return UniSetStr(source, new_source);
}

OP_STATUS
ES_JavascriptURLThread::SetResult(const uni_char *new_result)
{
	if (new_result)
		RETURN_IF_ERROR(UniSetStr(result, new_result));
	else
		result = NULL;

	has_result = TRUE;

	return OpStatus::OK;
}

#endif // USER_JAVASCRIPT

/* static */ OP_STATUS
ES_JavascriptURLThread::Load(ES_ThreadScheduler *scheduler, const uni_char *source, const URL &url, BOOL write_result_to_document, BOOL write_result_to_url, BOOL is_reload, BOOL want_utf8, ES_Thread *origin_thread, BOOL is_user_requested, BOOL open_in_new_window, BOOL open_in_background)
{
	ES_Runtime *runtime = scheduler->GetRuntime();
	ES_Context *context;

	if ((context = runtime->CreateContext(NULL)) != NULL)
	{
		uni_char *source_copy = UniSetNewStr(source);

		if (source_copy)
		{
			ES_JavascriptURLThread *thread;

			ES_SharedThreadInfo* shared = origin_thread ? origin_thread->GetSharedInfo() : NULL;
			if ((thread = OP_NEW(ES_JavascriptURLThread, (context, source_copy, url, write_result_to_document, write_result_to_url, is_reload, want_utf8, shared))) != NULL)
			{
				if (is_user_requested)
				{
					thread->SetIsUserRequested();
					if (open_in_new_window)
						thread->SetOpenInNewWindow();

					if (open_in_background)
						thread->SetOpenInBackground();
				}

				OP_STATUS status = scheduler->AddRunnable(thread);

				if (origin_thread && status == OpBoolean::IS_TRUE)
					status = scheduler->SerializeThreads(origin_thread, thread);

				return OpStatus::IsSuccess(status) ? OpStatus::OK : status;
			}

			OP_DELETEA(source_copy);
		}

		ES_Runtime::DeleteContext(context);
	}

	return OpStatus::ERR_NO_MEMORY;
}

FramesDocument*
ES_JavascriptURLThread::GetFramesDocument()
{
	OP_ASSERT(scheduler);
	return scheduler->GetFramesDocument();
}

OP_STATUS
ES_JavascriptURLThread::PostURLMessages(BOOL failed, BOOL cancelled)
{
	BOOL oom = FALSE;

	if (write_result_to_url)
	{
		FramesDocument *frames_doc = GetFramesDocument();
		MessageHandler *mh = frames_doc->GetMessageHandler();

		if (failed || cancelled)
		{
			url.ForceStatus(failed ? URL_LOADING_FAILURE : URL_LOADING_ABORTED);

			oom = oom || !mh->PostMessage(MSG_URL_LOADING_FAILED, url.Id(), 0);
		}
		else
		{
			url.ForceStatus(URL_LOADED);

			oom = !mh->PostMessage(MSG_HEADER_LOADED, url.Id(), 0);
			oom = oom || !mh->PostMessage(MSG_URL_DATA_LOADED, url.Id(), 0);
		}
	}

	return oom ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

/* --- ES_HistoryNavigationThread ------------------------------------ */

/* virtual */
OP_STATUS
ES_HistoryNavigationThread::EvaluateThread()
{
	Window *window = document_manager->GetWindow();
	int max_position = window->GetHistoryMax();
	int min_position = window->GetHistoryMin();
	int current = window->GetCurrentHistoryNumber();
	int position = current;

	position += delta;
	position = MAX(min_position, position);
	position = MIN(max_position, position);

	OP_ASSERT(position != current);
	window->SetCurrentHistoryPos(position, TRUE, shared && shared->is_user_requested);
	document_manager->AddPendingHistoryDelta(-delta);
	is_started = is_completed = TRUE;
	return OpStatus::OK;
}

ES_ThreadType
ES_HistoryNavigationThread::Type()
{
	return ES_THREAD_HISTORY_NAVIGATION;
}

const uni_char *
ES_HistoryNavigationThread::GetInfoString()
{
	return UNI_L("History navigation thread");
}

/* virtual */
OP_STATUS
ES_HistoryNavigationThread::Signal(ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED)
		document_manager->AddPendingHistoryDelta(-delta);

	return ES_Thread::Signal(signal);
}

/* --- ES_TerminatingAction ------------------------------------------ */

ES_TerminatingAction::ES_TerminatingAction(BOOL final, BOOL conditional, BOOL send_onunload, ES_TerminatingActionType type, BOOL terminated_by_parent)
	: final(final),
	  conditional(conditional),
	  send_onunload(send_onunload),
	  terminated_by_parent(terminated_by_parent),
	  type(type)
{
}

/* virtual */
ES_TerminatingAction::~ES_TerminatingAction()
{
}

/* virtual */
OP_STATUS
ES_TerminatingAction::PerformBeforeUnload(ES_TerminatingThread *thread, BOOL sending_onunload, BOOL &cancel_onunload)
{
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
ES_TerminatingAction::PerformAfterUnload(ES_TerminatingThread *thread)
{
	return OpStatus::OK;
}

BOOL
ES_TerminatingAction::IsLast()
{
	return Suc() == NULL;
}

BOOL
ES_TerminatingAction::IsFinal()
{
	return final;
}

BOOL
ES_TerminatingAction::IsConditional()
{
	return conditional;
}

BOOL
ES_TerminatingAction::SendOnUnload()
{
	return send_onunload;
}

ES_TerminatingActionType
ES_TerminatingAction::Type()
{
	return type;
}

/* --- ES_TerminatingThread ------------------------------------------ */

ES_TerminatingThread::ES_TerminatingThread()
	: ES_Thread(NULL),
	  children(0),
	  delayed_unload(FALSE),
	  children_terminated(FALSE)
{
}

/* virtual */
ES_TerminatingThread::~ES_TerminatingThread()
{
	while (ES_TerminatingAction *action = (ES_TerminatingAction *) actions.First())
	{
		action->Out();
		OP_DELETE(action);
	}
}

/* virtual */
OP_STATUS
ES_TerminatingThread::EvaluateThread()
{
	ES_TerminatingAction *action;

	if (!IsStarted())
	{
		FramesDocument *frames_doc = scheduler->GetFramesDocument();
		is_started = TRUE;

		action = (ES_TerminatingAction *) actions.Last();
		BOOL send_unload = frames_doc && action && action->SendOnUnload();
		if (send_unload)
		{
			DOM_Environment *environment = frames_doc->GetDOMEnvironment();
			if (!environment->HasWindowEventHandler(ONUNLOAD))
				send_unload = FALSE;
		}

		BOOL cancel_unload = FALSE;

		for (action = (ES_TerminatingAction *) actions.First();
		     action;
		     action = (ES_TerminatingAction *) action->Suc())
			RETURN_IF_ERROR(action->PerformBeforeUnload(this, send_unload, cancel_unload));

		if (send_unload)
		{
			if (IsBlocked())
				delayed_unload = TRUE;
			else if (!cancel_unload)
				return SendUnload();
		}

		if (IsBlocked())
			return OpStatus::OK;
	}

	if (delayed_unload)
	{
		delayed_unload = FALSE;
		return SendUnload();
	}

	for (action = (ES_TerminatingAction *) actions.First();
	     action;
	     action = (ES_TerminatingAction *) action->Suc())
		RETURN_IF_ERROR(action->PerformAfterUnload(this));

	is_completed = TRUE;
	scheduler->Reset();
	return OpStatus::OK;
}

/* virtual */
ES_ThreadType
ES_TerminatingThread::Type()
{
	return ES_THREAD_TERMINATING;
}

void
ES_TerminatingThread::AddAction(ES_TerminatingAction *action)
{
	ES_TerminatingAction *last_action = static_cast<ES_TerminatingAction *>(actions.Last());
	BOOL is_busy = scheduler->IsActive() && scheduler->GetCurrentThread() == this;

	if (last_action)
	{
		if (last_action->IsFinal())
		{
			if (!last_action->IsConditional() || action->IsConditional())
				OP_DELETE(action);
			else if (action->IsFinal())
			{
				if (!is_busy)
				{
					last_action->Out();
					OP_DELETE(last_action);
				}

				action->Into(&actions);
				goto end;
			}
			else
			{
				action->Precede(last_action);
				goto end;
			}

			return;
		}
		else if (last_action->IsConditional() && !is_busy)
		{
			last_action->Out();
			OP_DELETE(last_action);
		}
	}

	action->Into(&actions);

end:
	if (action->Type() == ES_TERMINATED_BY_PARENT_ACTION)
		static_cast<ES_TerminatedByParentAction *>(action)->TellParent(this);
}

void
ES_TerminatingThread::RemoveAction(ES_TerminatingActionType type)
{
	BOOL is_busy = scheduler->IsActive() && scheduler->GetCurrentThread() == this;
	if (!is_busy)
	{
		ES_TerminatingAction *action = static_cast<ES_TerminatingAction *>(actions.Last());

		while (action && action->Type() != type)
			action = static_cast<ES_TerminatingAction *>(action->Pred());

		if (action)
		{
			action->Out();
			OP_DELETE(action);
		}
	}
}

BOOL
ES_TerminatingThread::TestAction(BOOL final, BOOL conditional)
{
	ES_TerminatingAction *last_action = static_cast<ES_TerminatingAction *>(actions.Last());

	if (last_action && last_action->IsFinal() && (!last_action->IsConditional() || conditional))
		return FALSE;
	else
		return TRUE;
}

BOOL
ES_TerminatingThread::TestAction(ES_TerminatingAction *action)
{
	return TestAction(action->IsFinal(), action->IsConditional());
}

void
ES_TerminatingThread::CancelUnload()
{
	delayed_unload = FALSE;
}

void
ES_TerminatingThread::ChildNoticed(ES_Thread *thread)
{
	if (children == 0)
		Block(ES_BLOCK_TERMINATING_CHILDREN);

	++children;

#ifdef DEBUG_THREAD_DEADLOCKS
	Child *child = OP_NEW(Child, ());
	if (child)
	{
		child->thread = thread;
		child->Into(&all_children);

		extern void CheckDeadlocksStart(ES_Thread *);
		CheckDeadlocksStart(this);
	}
#endif // DEBUG_THREAD_DEADLOCKS
}

OP_STATUS
ES_TerminatingThread::ChildTerminated(ES_Thread *thread)
{
#ifdef DEBUG_THREAD_DEADLOCKS
	Child *child = (Child *) all_children.First();
	while (child)
		if (child->thread == thread)
		{
			child->Out();
			OP_DELETE(child);
			break;
		}
		else
			child = (Child *) child->Suc();
#endif // DEBUG_THREAD_DEADLOCKS

	--children;

	if (children == 0)
		return Unblock(ES_BLOCK_TERMINATING_CHILDREN);
	else
		return OpStatus::OK;
}

/* virtual */
OP_STATUS
ES_TerminatingThread::Signal(ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED)
		scheduler->Reset();

	return ES_Thread::Signal(signal);
}

OP_STATUS
ES_TerminatingThread::SendUnload()
{
	if (FramesDocument *frames_doc = scheduler->GetFramesDocument())
	{
		if (!children_terminated)
		{
			children_terminated = TRUE;
			RETURN_IF_ERROR(frames_doc->ESTerminate(this));
		}

		if (IsBlocked())
			delayed_unload = TRUE;
		else
			return scheduler->GetFramesDocument()->HandleWindowEvent(ONUNLOAD, this);
	}

	return OpStatus::OK;
}

