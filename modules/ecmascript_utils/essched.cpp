/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/estimermanager.h"
#include "modules/ecmascript_utils/estimerevent.h"
#include "modules/ecmascript_utils/esutils.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

#ifdef ECMASCRIPT_DEBUGGER
# include "modules/ecmascript_utils/esenginedebugger.h"
#endif // ECMASCRIPT_DEBUGGER

#ifdef DEBUG_THREAD_DEADLOCKS
class ES_ThreadDependency
	: public Link
{
public:
	// Means 'to' must be finished before 'from' can be finished.
	ES_Thread *from, *to;
};

Head g_dependencies;
BOOL g_deadlock;

static BOOL
IsDependent(ES_Thread *from, ES_Thread *to)
{
	ES_ThreadDependency *dep = (ES_ThreadDependency *) g_dependencies.First();
	while (dep)
		if (dep->from == from && dep->to == to)
			return TRUE;
		else
			dep = (ES_ThreadDependency *) dep->Suc();
	return FALSE;
}

static void
AddDependency(ES_Thread *from, ES_Thread *to)
{
	if (IsDependent(from, to))
		// Already added.
		return;

	// Circular!  Will deadlock.  Very bad.
	if (IsDependent(to, from))
		g_deadlock = TRUE;

	ES_ThreadDependency *dep = (ES_ThreadDependency *) g_dependencies.First();
	while (dep)
	{
		if (dep->from == to)
			AddDependency(from, dep->to);
		dep = (ES_ThreadDependency *) dep->Suc();
	}

	while (to)
	{
		ES_ThreadDependency *dep = OP_NEW(ES_ThreadDependency, ());

		if (dep)
		{
			dep->from = from;
			dep->to = to;
			dep->Into(&g_dependencies);
		}
		else
		{
			// From here on, all results are bogus.
			OP_ASSERT(FALSE);
			return;
		}

		if (to->GetInterruptedThread())
			AddDependency(to->GetInterruptedThread(), to);

		if (to->Type() == ES_THREAD_TERMINATING)
			((ES_TerminatingThread *) to)->AddDependencies(from);

		ES_Thread *next = (ES_Thread *) to->Pred();

		if (next)
			AddDependency(to, next);

		to = next;
	}
}

static void
CheckDeadlocks(ES_Thread *thread)
{
	if (ES_Thread *pred = (ES_Thread *) thread->Pred())
		AddDependency(thread, pred);

	if (ES_Thread *interrupted = thread->GetInterruptedThread())
	{
		AddDependency(interrupted, thread);
		CheckDeadlocks(interrupted);
	}

	if (thread->Type() == ES_THREAD_TERMINATING)
		((ES_TerminatingThread *) thread)->AddDependencies(NULL);
}

class ES_SchedulerElement
	: public Link
{
public:
	ES_ThreadScheduler *scheduler;
};

class ES_ThreadElement
	: public Link
{
public:
	ES_Thread *thread;
};

#include <stdio.h>

static void
PrintDependencies()
{
	FILE *f = fopen("C:\\Temp\\dependencies.txt", "w");
	Head schedulers;
	ES_SchedulerElement *scheduler;

	ES_ThreadDependency *dep = (ES_ThreadDependency *) g_dependencies.First();
	while (dep)
	{
		ES_ThreadScheduler *from_scheduler = dep->from->GetScheduler();
		scheduler = (ES_SchedulerElement *) schedulers.First();
		while (scheduler)
			if (scheduler->scheduler == from_scheduler)
			{
				from_scheduler = NULL;
				break;
			}
			else
				scheduler = (ES_SchedulerElement *) scheduler->Suc();
		if (from_scheduler)
		{
			scheduler = OP_NEW(ES_SchedulerElement, ());
			scheduler->scheduler = from_scheduler;
			scheduler->Into(&schedulers);
		}

		ES_ThreadScheduler *to_scheduler = dep->to->GetScheduler();
		if (to_scheduler != dep->from->GetScheduler())
		{
			scheduler = (ES_SchedulerElement *) schedulers.First();
			while (scheduler)
				if (scheduler->scheduler == to_scheduler)
				{
					to_scheduler = NULL;
					break;
				}
				else
					scheduler = (ES_SchedulerElement *) scheduler->Suc();
			if (to_scheduler)
			{
				scheduler = OP_NEW(ES_SchedulerElement, ());
				scheduler->scheduler = to_scheduler;
				scheduler->Into(&schedulers);
			}
		}

		dep = (ES_ThreadDependency *) dep->Suc();
	}

	unsigned index;

	fprintf(f, "Schedulers:\n");

	scheduler = (ES_SchedulerElement *) schedulers.First();
	index = 0;
	while (scheduler)
	{
		fprintf(f, "%u: 0x%08x: %s\n", index++, (unsigned) scheduler->scheduler, scheduler->scheduler->GetFramesDocument()->GetURL().Name(PASSWORD_SHOW));
		scheduler = (ES_SchedulerElement *) scheduler->Suc();
	}

	fprintf(f, "\n");

	scheduler = (ES_SchedulerElement *) schedulers.First();
	index = 0;
	while (scheduler)
	{
		Head threads;
		ES_ThreadElement *thread;

		fprintf(f, "Scheduler 0x%08x:\n", (unsigned) scheduler->scheduler);

		dep = (ES_ThreadDependency *) g_dependencies.First();
		while (dep)
		{
			ES_Thread *from_thread = dep->from;
			if (from_thread->GetScheduler() == scheduler->scheduler)
			{
				thread = (ES_ThreadElement *) threads.First();
				while (thread)
					if (thread->thread == from_thread)
					{
						from_thread = NULL;
						break;
					}
					else
						thread = (ES_ThreadElement *) thread->Suc();
				if (from_thread)
				{
					thread = OP_NEW(ES_ThreadElement, ());
					thread->thread = from_thread;
					thread->Into(&threads);

					uni_char buffer[1024]; /* ARRAY OK jl 2008-02-07 */
					uni_strcpy(buffer, from_thread->GetInfoString());
					for (unsigned i = 0, length = uni_strlen(buffer) + 1; i < length; ++i)
						((char *) buffer)[i] = (char) buffer[i];

					fprintf(f, "  %u: 0x%08x (0x%08x|0x%08x): %s\n", index++, (unsigned) from_thread, (unsigned) from_thread->Suc(), (unsigned) from_thread->GetInterruptedThread(), (const char *) buffer);
				}
			}
			ES_Thread *to_thread = dep->from;
			if (to_thread->GetScheduler() == scheduler->scheduler)
			{
				thread = (ES_ThreadElement *) threads.First();
				while (thread)
					if (thread->thread == to_thread)
					{
						to_thread = NULL;
						break;
					}
					else
						thread = (ES_ThreadElement *) thread->Suc();
				if (to_thread)
				{
					thread = OP_NEW(ES_ThreadElement, ());
					thread->thread = to_thread;
					thread->Into(&threads);

					uni_char buffer[1024]; /* ARRAY OK jl 2008-02-07 */
					uni_strcpy(buffer, to_thread->GetInfoString());
					for (unsigned i = 0, length = uni_strlen(buffer) + 1; i < length; ++i)
						((char *) buffer)[i] = (char) buffer[i];

					fprintf(f, "  %u: 0x%08x (0x%08x|0x%08x): %s\n", index++, (unsigned) to_thread, (unsigned) to_thread->Suc(), (unsigned) to_thread->GetInterruptedThread(), (const char *) buffer);
				}
			}

			dep = (ES_ThreadDependency *) dep->Suc();
		}

		scheduler = (ES_SchedulerElement *) scheduler->Suc();
		threads.Clear();
	}

	fprintf(f, "\nDependencies:\n");

	dep = (ES_ThreadDependency *) g_dependencies.First();
	while (dep)
	{
		fprintf(f, "  0x%08x => 0x%08x\n", (unsigned) dep->from, (unsigned) dep->to);
		dep = (ES_ThreadDependency *) dep->Suc();
	}

	schedulers.Clear();

	fclose(f);
}

void
CheckDeadlocksStart(ES_Thread *thread)
{
	g_deadlock = FALSE;

	CheckDeadlocks(thread);

	if (g_deadlock)
	{
		OP_ASSERT(FALSE);
		PrintDependencies();
	}

	g_dependencies.Clear();
}

void
ES_TerminatingThread::AddDependencies(ES_Thread *other)
{
	Child *child = (Child *) all_children.First();
	while (child)
	{
		AddDependency(this, child->thread);

		if (other)
			AddDependency(other, child->thread);

		child = (Child *) child->Suc();
	}
}
#endif // DEBUG_THREAD_DEADLOCKS

BOOL ES_TimesliceExpired(void *);

class ES_ThreadSchedulerImpl : public ES_ThreadScheduler, public MessageObject
{
public:
	ES_ThreadSchedulerImpl(ES_Runtime *runtime, BOOL always_enabled, BOOL is_dom_runtime);
	~ES_ThreadSchedulerImpl();

	/* Functions from ES_ThreadScheduler. */

	OP_STATUS ExecuteThread(ES_Thread *thread, unsigned timeslice, BOOL leave_thread_alive);
	OP_STATUS AddRunnable(ES_Thread *new_thread, ES_Thread *interrupt_thread = NULL);
	OP_STATUS AddTerminatingAction(ES_TerminatingAction *action, ES_Thread *interrupt_thread, ES_Thread *blocking_thread = NULL);
	BOOL TestTerminatingAction(BOOL final, BOOL conditional);

	void RemoveThreads(BOOL terminating = FALSE, BOOL final = FALSE);

	OP_STATUS MigrateThread(ES_Thread *thread);
	void ThreadMigrated(ES_Thread *thread);

	OP_STATUS CancelThread(ES_Thread *thread);
	OP_STATUS CancelThreads(ES_ThreadType type);
	OP_STATUS CancelTimeout(unsigned int id);
	OP_STATUS CancelAllTimeouts(BOOL cancel_runnable, BOOL cancel_waiting);

	OP_STATUS Activate();
	void Reset();

	void Block(ES_Thread *thread, ES_ThreadBlockType type = ES_BLOCK_UNSPECIFIED);
	OP_STATUS Unblock(ES_Thread *thread, ES_ThreadBlockType type = ES_BLOCK_UNSPECIFIED);

	BOOL IsActive();
	BOOL IsBlocked();
	BOOL IsDraining();
	BOOL HasRunnableTasks();
	BOOL HasTerminatingThread();
	ES_TerminatingThread *GetTerminatingThread();

	ES_Runtime *GetRuntime();
	ES_TimerManager *GetTimerManager();
	FramesDocument *GetFramesDocument();
	ES_Thread *GetCurrentThread();
	ES_Thread *SetOverrideCurrentThread(ES_Thread *thread);
	ES_Thread *GetErrorHandlerInterruptThread(ES_Runtime::ErrorHandler::ErrorType type);
	void SetErrorHandlerInterruptThread(ES_Thread *thread);
	void ResetErrorHandlerInterruptThread();
	const uni_char *GetThreadInfoString();
	OP_STATUS GetLastError();
	void ResumeIfNeeded();
	void PushNestedActivation();
	void PopNestedActivation();
	BOOL IsPresentOnTheStack();
	OP_STATUS SerializeThreads(ES_Thread *execute_first, ES_Thread *execute_last);
	void GCTrace();

	/*  Function from MessageObject. */
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	Head runnable;
	ES_Thread *current_thread;
	ES_Thread *override_current_thread;
	ES_Thread *error_handler_interrupt_thread;

	BOOL is_active;
	BOOL is_draining;
	BOOL is_terminated;
	BOOL is_final;
	BOOL is_removing_threads;
	BOOL has_posted_run;
	BOOL has_set_callbacks;
	BOOL always_enabled;
	BOOL remove_all_threads;
	BOOL is_dom_runtime;

	int id;

	int nested_count; ///< The number of times this appear in lower nested message loops.

	unsigned int timeout_id;
#ifdef SCOPE_PROFILER
	unsigned int thread_id;
#endif // SCOPE_PROFILER

	double timeslice_start;
	unsigned long timeslice_ms;
	unsigned long timeslice_max_len;

	ES_Runtime *runtime;
	FramesDocument *frames_doc;
	MessageHandler *msg_handler;
	Window *window;

	OP_STATUS last_error;

	ES_TimerManager timer_manager;

#ifdef _DEBUG
	ES_ThreadSchedulerImpl* next;
#endif

	/** Keeps a local 'is-active'-state for all esschedulars. Used to detect if opera is idle, important for testing */
	OpActivity activity_script;

    BOOL IsEnabled();
	BOOL IsPaused();

	void SuspendActiveTask();
	void Remove(ES_Thread *thread);

	OP_STATUS RunNow();
	OP_STATUS TimeoutNow();
	void HandleError();

	OP_STATUS PostRunMessage();
	OP_STATUS PostTimeoutMessage();
	OP_STATUS SetCallbacks();
	void UnsetCallbacks();

	OP_STATUS Terminate(ES_TerminatingThread *thread);

	friend BOOL ES_TimesliceExpired(void *);

	void TimesliceStart(unsigned timeslice_length);
	BOOL TimesliceExpired();

	ES_Thread *GetLocalInterruptThread(ES_Thread *interrupt_thread, BOOL check_script_elms = TRUE);
};

/* --- ES_ForeignThreadBlock ----------------------------------------- */

/**
 * Used to block a thread in one scheduler during the execution of a
 * thread in another scheduler, to achieve the same effect as when a
 * thread interrupts another thread in the same scheduler.
 *
 * Implemented using two thread listeners, one on each thread.
 */
class ES_ForeignThreadBlock
	: public ES_ThreadListener
{
private:
	/**
	 * Helper method because SetInterruptedThread is protected and the mac compiler
	 * got confused by nested classes and friend declarations. Or maybe we abused some
	 * part of C++ that just happened to work in gcc and Visual Studio.
	 */
	static void SetInterruptedThreadToNULL(ES_Thread* thread) { thread->SetInterruptedThread(NULL); }

	class ES_Blocking
		: public ES_ThreadListener
	{
	private:
		ES_Thread *blocking;
		ES_ThreadScheduler *blocking_scheduler;
		BOOL is_active;
		BOOL foreign_interrupt_thread;

	public:
		ES_Blocking(ES_Thread *blocking, ES_Thread *blocked, BOOL foreign_interrupt_thread)
			: blocking(blocking),
			  blocking_scheduler(blocking->GetScheduler()),
			  is_active(TRUE),
			  foreign_interrupt_thread(foreign_interrupt_thread),
			  blocked(blocked) {}

		void Init()
		{
			blocking->AddListener(this);
		}

		OP_STATUS
		Signal(ES_Thread *thread, ES_ThreadSignal signal)
		{
			switch (signal)
			{
			case ES_SIGNAL_CANCELLED:
			case ES_SIGNAL_FINISHED:
			case ES_SIGNAL_FAILED:
				Remove();
				is_active = FALSE;
				if (foreign_interrupt_thread)
				{
					OP_ASSERT(thread->GetInterruptedThread() == blocked);
					SetInterruptedThreadToNULL(thread); // Same as thread->SetInterruptedThread(NULL);
				}
				return blocked->DecBlockedByForeignThread();

			default:
				return OpStatus::OK;
			}
		}

		BOOL
		IsActive()
		{
			return is_active;
		}

		void
		Cancel()
		{
			if (foreign_interrupt_thread)
			{
				OP_ASSERT(blocking->GetInterruptedThread() == blocked);
				SetInterruptedThreadToNULL(blocking); // Same as blocking->SetInterruptedThread(NULL);
			}
			Remove();

			/* Propagate cancelling, unless the blocking thread has migrated to
			   a new scheduler since blocked us or if the blocked thread has
			   been removed. */
			if (blocking->GetScheduler() == blocking_scheduler && foreign_interrupt_thread)
				blocking_scheduler->CancelThread(blocking);
		}

		ES_Thread *blocked;
	} blocking_listener;

	friend class ES_Blocking;

public:
	ES_ForeignThreadBlock(ES_Thread *blocking, ES_Thread *blocked, BOOL foreign_interrupt_thread = TRUE)
		: blocking_listener(blocking, blocked, foreign_interrupt_thread) {}

	void Init()
	{
		blocking_listener.Init();
		blocking_listener.blocked->AddListener(this);
		blocking_listener.blocked->IncBlockedByForeignThread();
	}

	OP_STATUS
	Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		if (blocking_listener.IsActive())
			switch (signal)
			{
			case ES_SIGNAL_FINISHED:
			case ES_SIGNAL_FAILED:
				/* The blocked thread is supposed to be blocked while this
				   listener is active, and should neither finish nor fail. */
				OP_ASSERT(FALSE);
				/* fall through */

			case ES_SIGNAL_CANCELLED:
				/* It might be cancelled, though. */
				blocking_listener.Cancel();
			}

		return OpStatus::OK;
	}
};

/* --- ES_ThreadScheduler -------------------------------------------- */

/* static */
ES_ThreadScheduler *
ES_ThreadScheduler::Make(ES_Runtime *runtime, BOOL always_enabled, BOOL is_dom_runtime)
{
	return OP_NEW(ES_ThreadSchedulerImpl, (runtime, always_enabled, is_dom_runtime));
}

ES_ThreadScheduler::~ES_ThreadScheduler()
{
}

/* --- ES_ThreadSchedulerImpl ---------------------------------------- */

ES_ThreadSchedulerImpl::ES_ThreadSchedulerImpl(ES_Runtime *runtime, BOOL always_enabled, BOOL is_dom_runtime)
	: current_thread(NULL),
	  override_current_thread(NULL),
	  error_handler_interrupt_thread(NULL),
	  is_active(FALSE),
	  is_draining(FALSE),
	  is_terminated(FALSE),
	  is_final(FALSE),
	  is_removing_threads(FALSE),
	  has_posted_run(FALSE),
	  has_set_callbacks(FALSE),
	  always_enabled(always_enabled),
	  remove_all_threads(FALSE),
	  is_dom_runtime(is_dom_runtime),
	  nested_count(0),
	  timeout_id(0),
#ifdef SCOPE_PROFILER
	  thread_id(0),
#endif // SCOPE_PROFILER
	  runtime(runtime),
	  frames_doc(runtime->GetFramesDocument()),
	  window(NULL),
	  last_error(OpStatus::OK),
	  activity_script(ACTIVITY_ECMASCRIPT)
{
	id = ++g_opera->ecmascript_utils_module.scheduler_id;

	if (frames_doc && frames_doc->GetWindow())
	{
		window = frames_doc->GetWindow();
		msg_handler = frames_doc->GetMessageHandler();
	}
	else
		msg_handler = g_main_message_handler;

	timer_manager.SetMessageHandler(msg_handler);
}

/* virtual */
ES_ThreadSchedulerImpl::~ES_ThreadSchedulerImpl()
{
	/* The scheduler must not have any runnable tasks when it is removed.
	   If it has, this means that it wasn't terminated correctly. */
	OP_ASSERT(!HasRunnableTasks());

	UnsetCallbacks();
}

#ifdef ESUTILS_SYNCIF_SUPPORT

class ES_MainThreadListener : public ES_ThreadListener
{
private:
	BOOL* main_thread_was_killed;
public:
	ES_MainThreadListener(BOOL* b) : main_thread_was_killed(b) {}
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_CANCELLED ||
			signal == ES_SIGNAL_FINISHED ||
			signal == ES_SIGNAL_FAILED)
		{
			*main_thread_was_killed = TRUE;
			Remove();
		}

		return OpStatus::OK;
	}
};

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::ExecuteThread(ES_Thread *thread, unsigned timeslice, BOOL leave_thread_alive)
{
	OP_NEW_DBG("ES_ThreadSchedulerImpl::ExecuteThread", "es_utils");
	if (current_thread == thread)
		current_thread = NULL;

	OP_ASSERT(!thread->InList() || runnable.HasLink(thread));
	thread->Out();

	BOOL previous_is_active = is_active;
	ES_Thread *previous_current_thread = current_thread;

	thread->IntoStart(&runnable);

	BOOL main_thread_was_killed = FALSE;
	ES_MainThreadListener listener(&main_thread_was_killed);
	thread->AddListener(&listener);

	is_active = TRUE;
	current_thread = thread;

	OP_STATUS ret = OpStatus::OK;

	TimesliceStart(timeslice);

	while (!main_thread_was_killed && !remove_all_threads && HasRunnableTasks() && !IsBlocked() && !TimesliceExpired() && OpStatus::IsSuccess(ret))
	{
		BOOL is_executing_main_thread = current_thread == thread;

		if (!current_thread->IsCompleted())
			ret = current_thread->EvaluateThread();

		// current_thread can be changed by the call to EvaluateThread():
		// coverity[check_after_deref: FALSE]
		if (current_thread)
		{
			if (current_thread->IsCompleted() || OpStatus::IsError(ret))
			{
				/* When the current thread is cancelled, it is signalled but left in
				   the list of runnable threads.  In such case, the thread shouldn't
				   be signalled again. */
				if (!current_thread->IsSignalled())
				{
					BOOL failed = current_thread->IsFailed() || OpStatus::IsError(ret);
					ES_ThreadSignal signal = failed ? ES_SIGNAL_FAILED : ES_SIGNAL_FINISHED;
					last_error = ret;

					if (current_thread->Signal(signal) == OpStatus::ERR_NO_MEMORY)
						ret = OpStatus::ERR_NO_MEMORY;
					if (is_executing_main_thread)
						main_thread_was_killed = FALSE; // Not killed, possibly peacefully finished.
				}

				if (!current_thread->Pred())
				{
					/* Don't remove the thread if it has been interrupted by a
					   listener that was just signalled. */
					Remove(current_thread);

					if (is_executing_main_thread)
					{
						thread = NULL;
						break;
					}
				}
			}
		}
		else if (is_executing_main_thread)
		{
			thread = NULL;
			break;
		}

		current_thread = (ES_Thread *) runnable.First();
	}

#ifdef _DEBUG
	if (!thread)
		OP_DBG(("Finished all threads back to the original state"));
	else if (main_thread_was_killed)
		OP_DBG(("The thread we wanted to execute was killed by something external"));
	else if (!HasRunnableTasks())
		OP_DBG(("Ran out of runnable threads while executing"));
	else if (IsBlocked())
		OP_DBG(("Couldn't execute thread to end because it got blocked"));
	else if (TimesliceExpired())
		OP_DBG(("Couldn't execute thread to end because the timeslice expired"));
	else if (OpStatus::IsError(ret))
		OP_DBG(("Couldn't execute thread to end because Evaluate returned an error: %d", static_cast<int>(ret)));
	else
		OP_DBG(("Execute finished in an unexpected way"));
#endif // _DEBUG

	// Decide return value and kill thread if we are to kill it
	if (thread && (main_thread_was_killed || !leave_thread_alive))
	{
		ret = OpStatus::ERR;
		if (!main_thread_was_killed && !leave_thread_alive)
		{
			current_thread = NULL; // To make sure no threads are protected from removal.
			if (OpStatus::IsMemoryError(CancelThread(thread)))
				ret = OpStatus::ERR_NO_MEMORY;
		}
		thread = NULL;
	}

	is_active = previous_is_active;

	if (!thread)
		if (previous_current_thread)
			current_thread = previous_current_thread;
		else
			current_thread = static_cast<ES_Thread *>(runnable.First());

	if (remove_all_threads)
	{
		remove_all_threads = FALSE;
		RemoveThreads(TRUE);
	}

	return ret;
}

#endif // ESUTILS_SYNCIF_SUPPORT

/* virtual */
OP_BOOLEAN
ES_ThreadSchedulerImpl::AddRunnable(ES_Thread *new_thread, ES_Thread *interrupt_thread)
{
	OpAutoPtr<ES_Thread> anchor(new_thread);

#ifdef DEBUG_THREAD_DEADLOCKS
	if (interrupt_thread)
		CheckDeadlocksStart(interrupt_thread);
#endif // DEBUG_THREAD_DEADLOCKS

	/* Don't allow new threads when the scheduler is terminated or when
	   ECMAScript is disabled. */
	if (is_terminated || !IsEnabled())
		return OpBoolean::IS_FALSE;

	/* Don't allow adding new threads in a document that is generating (and
	   being replaced by) another document. */
	if (frames_doc && !frames_doc->IsCurrentDoc() && !interrupt_thread)
		return OpBoolean::IS_FALSE;

	/* Don't allow new threads to be added when the scheduler is shutting down,
	   unless the new thread is interrupting another thread. */
	if (is_draining && !interrupt_thread)
	{
#ifndef ES_SCHEDULER_REMOVE_UGLY_HACKS
		/* Ugly hack!  Ugly hack!
		   If we're draining and the terminating thread is blocked waiting for
		   an URL to be loaded, allow new event threads.  This is to make it
		   possible to for instance click on a new link while waiting for
		   another link to load. */
		if (new_thread->Type() == ES_THREAD_EVENT &&
		    runnable.Last() && ((ES_Thread *) runnable.Last())->GetBlockType() == ES_BLOCK_WAITING_FOR_URL)
			/* Since the last thread is blocked, we need to interrupt that
			   thread, or the new thread will wait for it to be unblocked. */
			interrupt_thread = (ES_Thread *) runnable.Last();
		else
#endif
			return OpBoolean::IS_FALSE;
	}

	if (new_thread->GetScheduler() != NULL && new_thread->GetScheduler() != this)
	{
		/* A thread can only be added to one scheduler and can't migrate to another. */
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	if (interrupt_thread)
	{
		if (interrupt_thread->GetRecursionDepth() == ESUTILS_MAXIMUM_THREAD_RECURSION)
		{
#ifdef OPERA_CONSOLE
			if (!new_thread->GetSharedInfo()->has_reported_recursion_error)
			{
				new_thread->GetSharedInfo()->has_reported_recursion_error = TRUE;
				ES_ErrorInfo error(NULL);
				uni_strlcpy(error.error_text, UNI_L("Maximum script thread recursion depth exceeded"), ARRAY_SIZE(error.error_text));
				OpStatus::Ignore(ES_Utils::PostError(frames_doc, error, UNI_L("Script thread creation"), frames_doc ? frames_doc->GetURL() : URL()));
			}
#endif // OPERA_CONSOLE

			return OpStatus::ERR;
		}

		new_thread->UseOriginInfoFrom(interrupt_thread);

		if (interrupt_thread->GetScheduler() == this)
		{
			if (IsActive() && interrupt_thread == current_thread)
				SuspendActiveTask();

			new_thread->Precede(interrupt_thread);
		}
		else
		{
			if (current_thread && current_thread->IsBlocked() && current_thread->IsPluginActive())
			{
				ES_Thread *root_interrupt_thread = interrupt_thread->GetRunningRootThread();
				if (!root_interrupt_thread->IsOrHasInterrupted(current_thread) && root_interrupt_thread->IsPluginThread())
				{
					/* Inject an extra block between the (root) plugin thread
					   and the plugin operation that instigated it all. By doing so,
					   'new_thread' will be appropriately scheduled to run before the
					   (blocked) plugin thread. Which it needs to do to avoid
					   a deadlock. See ES_Thread::SetIsPluginActive() comment. */
					ES_ForeignThreadBlock *inter_plugin_block = OP_NEW(ES_ForeignThreadBlock, (root_interrupt_thread, current_thread));
					if (!inter_plugin_block)
						return OpStatus::ERR_NO_MEMORY;

					inter_plugin_block->Init();
					root_interrupt_thread->SetInterruptedThread(current_thread);
				}
			}

			ES_Thread *local_interrupt_thread = GetLocalInterruptThread(interrupt_thread);

#ifdef DEBUG_THREAD_DEADLOCKS
			if (local_interrupt_thread)
				CheckDeadlocksStart(local_interrupt_thread);
#endif // DEBUG_THREAD_DEADLOCKS

			ES_ForeignThreadBlock *foreign_thread_block = OP_NEW(ES_ForeignThreadBlock, (new_thread, interrupt_thread));
			if (!foreign_thread_block)
				return OpStatus::ERR_NO_MEMORY;

			anchor.release();

			OP_BOOLEAN result = AddRunnable(new_thread, local_interrupt_thread);

			if (result == OpBoolean::IS_TRUE)
			{
				foreign_thread_block->Init();
				new_thread->SetInterruptedThread(interrupt_thread);
#ifdef DEBUG_THREAD_DEADLOCKS
				CheckDeadlocksStart(new_thread);
#endif // DEBUG_THREAD_DEADLOCKS
			}
			else
				OP_DELETE(foreign_thread_block);

			return result;
		}

		new_thread->SetInterruptedThread(interrupt_thread);
	}
	else
		new_thread->Into(&runnable);

	OP_ASSERT(new_thread->Type() != ES_THREAD_TERMINATING || new_thread == GetTerminatingThread());
	OP_ASSERT(!is_draining || HasTerminatingThread());

	/* The thread is now owned by the scheduler. */
	anchor.release();

	new_thread->SetScheduler(this);
#ifdef SCOPE_PROFILER
	new_thread->SetThreadId(++thread_id);
#endif // SCOPE_PROFILER

	if (new_thread->Type() == ES_THREAD_TERMINATING)
		RETURN_IF_ERROR(Terminate((ES_TerminatingThread *) new_thread));

	if (!IsActive())
	{
		current_thread = (ES_Thread *) runnable.First();
		RETURN_IF_ERROR(PostRunMessage());
	}

#ifdef DEBUG_THREAD_DEADLOCKS
	CheckDeadlocksStart(new_thread);
#endif // DEBUG_THREAD_DEADLOCKS

	activity_script.Begin();

	return OpBoolean::IS_TRUE;
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::AddTerminatingAction(ES_TerminatingAction *action, ES_Thread *interrupt_thread, ES_Thread *blocking_thread)
{
	OpAutoPtr<ES_TerminatingAction> anchor(action);

	if (is_terminated)
		/* Already terminated. */
		return OpStatus::OK;

	/* Interrupt threads for terminating threads are a bit special: the
	   terminating thread should always be the last one in its scheduler, since
	   its whole purpose is to execute once the scheduler is empty.  Thus
	   interrupting (that is, inserting before) another thread in the same
	   scheduler makes little sense. */
	OP_ASSERT(!interrupt_thread || interrupt_thread->GetScheduler() != this);

	/* But even if the interrupt thread is in a different scheduler, we must
	   make sure no thread in this scheduler in turn has been interrupted by it,
	   since otherwise we would get a deadlock.  GetLocalInterruptThread() is
	   the function AddRunnable() uses to calculate which local thread to
	   interrupt for a foreign interrupt thread.  We use it to disqualify an
	   interrupt thread instead.  (If we didn't, AddRunnable() would interrupt a
	   local thread, thus creating the situation the above assertion is supposed
	   to catch.) */
	if (interrupt_thread && GetLocalInterruptThread(interrupt_thread))
		interrupt_thread = NULL;

	ES_TerminatingThread *terminating = NULL;

	if (is_draining)
	{
		terminating = GetTerminatingThread();

		/* We expect there to be a terminating thread in the list if the
		   'is_draining' flag is set. */
		OP_ASSERT(terminating);

		if (terminating)
			if (!terminating->TestAction(action))
				return OpStatus::OK;
			else if (interrupt_thread && terminating->GetBlockType() == ES_BLOCK_FOREIGN_THREAD ||
			         blocking_thread && terminating->GetInterruptedThread())
				if (!terminating->IsStarted())
				{
					CancelThread(terminating);
					terminating = NULL;
				}
	}
#ifdef DEBUG_ENABLE_OPASSERT
	else
		/* We don't expect to find a terminating thread in the list if the
		   'is_draining' flag is not set. */
		OP_ASSERT(!GetTerminatingThread());
#endif // DEBUG_ENABLE_OPASSERT

	if (!terminating)
	{
		OP_ASSERT(!is_draining);

		terminating = OP_NEW(ES_TerminatingThread, ());

		if (!terminating)
			return OpStatus::ERR_NO_MEMORY;

		OP_BOOLEAN result = AddRunnable(terminating, interrupt_thread);

		if (OpStatus::IsError(result))
			return result;
		else if (result != OpBoolean::IS_TRUE)
		{
			OP_ASSERT(!"Failed to add a terminating action, which might prevent loading of a new document");
			return OpStatus::OK;
		}
	}

	if (blocking_thread)
		RETURN_IF_ERROR(SerializeThreads(blocking_thread, terminating));

	anchor.release();

#ifdef DEBUG_THREAD_DEADLOCKS
	CheckDeadlocksStart(terminating);
#endif // DEBUG_THREAD_DEADLOCKS

	terminating->AddAction(action);
	return OpStatus::OK;
}

/* virtual */
BOOL
ES_ThreadSchedulerImpl::TestTerminatingAction(BOOL final, BOOL conditional)
{
	if (ES_TerminatingThread *thread = GetTerminatingThread())
		return thread->TestAction(final, conditional);
	else
		return TRUE;
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::Activate()
{
	Reset();

	is_terminated = FALSE;

	/* Restart timeout threads. */
	timer_manager.Activate();

	return OpStatus::OK;
}

/* virtual */
void
ES_ThreadSchedulerImpl::Reset()
{
	is_draining = FALSE;
}

void
ES_ThreadSchedulerImpl::Remove(ES_Thread *thread)
{
	/* Can only be used to remove the current thread. */
	OP_ASSERT(thread->InList());
	OP_ASSERT(thread == current_thread);

	/* Check so that a thread that has been interrupted isn't removed,
	   since the thread that interrupted it has a reference to it. */
	for (ES_Thread *pred = (ES_Thread *) thread->Pred(); pred; pred = (ES_Thread *) thread->Pred())
		if (pred->GetInterruptedThread() != thread)
		{
			OP_ASSERT(FALSE);
			return;
		}

	thread->Out();
	OP_DELETE(thread);
	current_thread = NULL;

	if (runnable.Empty() && frames_doc)
		frames_doc->SignalESResting();
}

/* virtual */
void
ES_ThreadSchedulerImpl::RemoveThreads(BOOL terminating, BOOL final)
{
	ES_Thread *thread;

	/* Avoid reentering here when executing as this will lead to a usage of a deleted object */
	if (is_removing_threads)
		return;

	is_removing_threads = TRUE;

	if (terminating)
	{
		is_terminated = TRUE;

		if (IsPresentOnTheStack() && current_thread)
		{
			/* Remove all threads when we get back to the RunNow()/ExecuteNow() call that
			   is on the stack. */
			remove_all_threads = TRUE;
			/* And make sure we get back as soon as possible. */
			SuspendActiveTask();
		}
		else
		{
			thread = (ES_Thread *) runnable.First();
			while (thread)
			{
				OpStatus::Ignore(thread->Signal(ES_SIGNAL_CANCELLED));

				thread->Out();
				OP_DELETE(thread);

				thread = (ES_Thread *) runnable.First();
			}
			current_thread = NULL;

			/* This will make us ignore any MSG_ES_RUN that me have posted. */
			has_posted_run = FALSE;
		}
	}
	else
	{
		/* This should never happend. */
		OP_ASSERT(!HasRunnableTasks());
	}

	if (final)
	{
		is_final = TRUE;
		timer_manager.RemoveAllEvents();
	}

	timer_manager.Deactivate();

	UnsetCallbacks();

	if (runnable.Empty())
		activity_script.Cancel();

	is_removing_threads = FALSE;
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::CancelThreads(ES_ThreadType type)
{
	OP_STATUS result = OpStatus::OK;
	ES_Thread *thread = static_cast<ES_Thread*>(runnable.First());
	while (thread)
	{
		ES_Thread *next = static_cast<ES_Thread*>(thread->Suc());
		if (thread->Type() == type)
		{
			if (OpStatus::IsMemoryError(CancelThread(thread)))
				result = OpStatus::ERR_NO_MEMORY; // Remember the failure but go on
		}

		thread = next;
	}

	return result;
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::MigrateThread(ES_Thread *thread)
{
	ES_ThreadSchedulerImpl *source_scheduler = (ES_ThreadSchedulerImpl *) thread->GetScheduler();

	source_scheduler->ThreadMigrated(thread);

#ifdef ECMASCRIPT_DEBUGGER
	ES_EngineDebugBackend *engine_debug_backend = g_opera->ecmascript_utils_module.engine_debug_backend;
#endif // ECMASCRIPT_DEBUGGER

	while (thread)
	{
		if (thread->GetScheduler() == source_scheduler)
		{
			thread->MigrateTo(this);
			thread->Out();

			if (ES_Thread *interrupted = thread->GetInterruptedThread())
			again:
				for (ES_Thread *to_cancel = static_cast<ES_Thread *>(source_scheduler->runnable.First()); to_cancel; to_cancel = static_cast<ES_Thread *>(to_cancel->Suc()))
					if (to_cancel->GetInterruptedThread() == interrupted)
					{
						source_scheduler->CancelThread(to_cancel);
						goto again;
					}

			thread->Into(&runnable);

#ifdef ECMASCRIPT_DEBUGGER
			if (engine_debug_backend)
				engine_debug_backend->ThreadMigrated(thread, source_scheduler->GetRuntime(), runtime);
#endif // ECMASCRIPT_DEBUGGER
		}

		thread = thread->GetInterruptedThread();
	}

	// Migrate the terminating thread too if there is one.
	if (ES_Thread *terminating_thread = source_scheduler->GetTerminatingThread())
		RETURN_IF_MEMORY_ERROR(MigrateThread(terminating_thread));

	return PostRunMessage();
}

void
ES_ThreadSchedulerImpl::ThreadMigrated(ES_Thread *thread)
{
	if (thread == current_thread)
	{
		current_thread->Suspend();
		current_thread = NULL;
	}
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::CancelThread(ES_Thread *thread)
{
	OP_STATUS status = OpStatus::OK;

	if (is_removing_threads)
		return status;

	/* First signal the thread.  This may operation may trigger the
	   addition of new threads that interrupt this thread. */
	if (OpStatus::IsMemoryError(thread->Signal(ES_SIGNAL_CANCELLED)))
		status = OpStatus::ERR_NO_MEMORY;

	/* Cancel all threads that has interrupted 'thread'. */
	ES_Thread *interrupting_thread = (ES_Thread *) thread->Pred();
	while (interrupting_thread && thread->GetInterruptedCount() != 0)
	{
		if (interrupting_thread->GetInterruptedThread() == thread)
			if (!interrupting_thread->IsSignalled())
			{
				if (interrupting_thread->IsSoftInterrupt())
				{
					interrupting_thread->interrupted_thread = NULL;
					thread->interrupted_count--;
				}
				else if (OpStatus::IsMemoryError(CancelThread(interrupting_thread)))
					status = OpStatus::ERR_NO_MEMORY;

				interrupting_thread = (ES_Thread *) thread->Pred();
				continue;
			}
			else
				interrupting_thread->SetInterruptedThread(NULL);

		interrupting_thread = (ES_Thread *) interrupting_thread->Pred();
	}

	if (IsActive() && current_thread == thread)
		/* If the scheduler is currently executing this thread it can't be
		   deleted now.  ES_Thread::Signal will set the flags completed and
		   failed, so the thread will be removed and deleted once it stops
		   executing. */
		SuspendActiveTask();
	else
	{
		thread->Out();
		if (current_thread == thread)
		{
			current_thread = (ES_Thread *) runnable.First();
			if (OpStatus::IsMemoryError(PostRunMessage()))
				status = OpStatus::ERR_NO_MEMORY;
		}
		OP_DELETE(thread);
	}

	if (runnable.Empty() && frames_doc)
		frames_doc->SignalESResting();

	return status;
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::CancelAllTimeouts(BOOL cancel_runnable, BOOL cancel_waiting)
{
	OP_STATUS status = OpStatus::OK;
	ES_Thread *thread;
	ES_TimeoutThread *timeout_thread;

	if (cancel_runnable)
	{
		thread = static_cast<ES_Thread *> (runnable.First());
		while (thread)
		{
			if (thread->Type() == ES_THREAD_TIMEOUT)
			{
				ES_Thread *next = static_cast<ES_Thread *> (thread->Suc());
				timeout_thread = static_cast<ES_TimeoutThread *> (thread);

				if (timeout_thread->IsStarted())
					/* If the thread has already started, let it run to completion. */
					timeout_thread->GetTimerEvent()->StopRepeating();
				else
					/* If it hasn't, cancel it. */
					status = CancelThread(timeout_thread);

				if (!OpStatus::IsSuccess(status))
					return status;

				thread = next;
			}
			else
				thread = static_cast<ES_Thread *> (thread->Suc());
		}
	}

	if (cancel_waiting)
		/* The scheduler will never be reactivated, so we can remove all pending
		   timeout events as well. */
		timer_manager.RemoveAllEvents();

	return status;
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::CancelTimeout(unsigned id)
{
	ES_TimerManager *timeout_manager = GetTimerManager();
	ES_Thread *thread = static_cast<ES_Thread *> (runnable.First());
	while (thread)
	{
		if (thread->Type() == ES_THREAD_TIMEOUT)
		{
			ES_TimeoutThread *timeout_thread = static_cast<ES_TimeoutThread *> (thread);

			if (timeout_thread->GetId() == id)
			{
				ES_TimerEvent *timer_event = timeout_thread->GetTimerEvent();
				if (timeout_thread->IsStarted())
					/* If the thread has already started, let it run to completion. */
					timer_event->StopRepeating();
				else
				{
					timeout_manager->RemoveExpiredEvent(timer_event);
					RETURN_IF_ERROR(CancelThread(timeout_thread));
				}

				return OpStatus::OK;
			}
		}

		thread = static_cast<ES_Thread *> (thread->Suc());
	}

	RETURN_IF_ERROR(timeout_manager->RemoveEvent(id));
	return OpStatus::OK;
}

/* virtual */
void
ES_ThreadSchedulerImpl::Block(ES_Thread *thread, ES_ThreadBlockType type)
{
	/* Should not happend.  If it becomes necessary sometime to block a thread
	   multiple times, it will also be necessary to count blocks and unblocks
	   properly. */
	OP_ASSERT(thread->GetBlockType() == ES_BLOCK_NONE);

	thread->SetBlockType(type);

	if (IsActive() && thread == (ES_Thread *) runnable.First())
		SuspendActiveTask();
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::Unblock(ES_Thread *thread, ES_ThreadBlockType type)
{
	if (thread->GetBlockType() == ES_BLOCK_NONE)
	{
		/* This is not good at all, unblocking a thread that wasn't blocked. */
		OP_ASSERT(0);
		return OpStatus::ERR;
	}

	/* Symptom of unblocking the wrong thread, or the right thread for the wrong
	   reason.  To unconditionally unblock a blocked thread, one can always use

		 thread->Unblock(thread->GetBlockType());

	   but that should hardly ever be necessary. */
	OP_ASSERT(thread->GetBlockType() == type);

	thread->SetBlockType(ES_BLOCK_NONE);
	if (thread == (ES_Thread *) runnable.First())
		return PostRunMessage();

	return OpStatus::OK;
}

/* virtual */
BOOL
ES_ThreadSchedulerImpl::IsActive()
{
	return is_active;
}

/* virtual */
BOOL
ES_ThreadSchedulerImpl::IsBlocked()
{
	ES_Thread *thread = (ES_Thread *) runnable.First();
	return thread && thread->IsBlocked();
}

/* virtual */
BOOL
ES_ThreadSchedulerImpl::IsDraining()
{
	return is_draining;
}

/* virtual */
BOOL
ES_ThreadSchedulerImpl::HasRunnableTasks()
{
	return !runnable.Empty();
}

BOOL
ES_ThreadSchedulerImpl::HasTerminatingThread()
{
	return GetTerminatingThread() != NULL;
}

ES_TerminatingThread *
ES_ThreadSchedulerImpl::GetTerminatingThread()
{
	ES_Thread *thread = static_cast<ES_Thread *>(runnable.Last());

	while (thread && thread->Type() != ES_THREAD_TERMINATING)
		thread = static_cast<ES_Thread *>(thread->Pred());

	return static_cast<ES_TerminatingThread *>(thread);
}

/* virtual */
ES_Runtime *
ES_ThreadSchedulerImpl::GetRuntime()
{
	return runtime;
}

/* virtual */
ES_TimerManager *
ES_ThreadSchedulerImpl::GetTimerManager()
{
	return &timer_manager;
}

/* virtual */
FramesDocument *
ES_ThreadSchedulerImpl::GetFramesDocument()
{
	return frames_doc;
}

/* virtual */
ES_Thread *
ES_ThreadSchedulerImpl::GetCurrentThread()
{
	if (override_current_thread)
		return override_current_thread;

	if (!current_thread && !runnable.Empty())
		current_thread = (ES_Thread *) runnable.First();

	return current_thread;
}

/* virtual */
ES_Thread *
ES_ThreadSchedulerImpl::SetOverrideCurrentThread(ES_Thread *thread)
{
	ES_Thread *previous = override_current_thread;
	override_current_thread = thread;
	return previous;
}

/* virtual */
ES_Thread *
ES_ThreadSchedulerImpl::GetErrorHandlerInterruptThread(ES_Runtime::ErrorHandler::ErrorType type)
{
	if (type == ES_Runtime::ErrorHandler::COMPILATION_ERROR)
		return error_handler_interrupt_thread;
	else if (type == ES_Runtime::ErrorHandler::RUNTIME_ERROR)
	{
		OP_ASSERT(IsActive());
		return GetCurrentThread();
	}
	else
		return NULL;
}

/* virtual */
void
ES_ThreadSchedulerImpl::SetErrorHandlerInterruptThread(ES_Thread *thread)
{
	OP_ASSERT(!error_handler_interrupt_thread);
	OP_ASSERT(thread);

	error_handler_interrupt_thread = thread;
}

/* virtual */
void
ES_ThreadSchedulerImpl::ResetErrorHandlerInterruptThread()
{
	OP_ASSERT(error_handler_interrupt_thread);

	error_handler_interrupt_thread = NULL;
}

/* virtual */
const uni_char *
ES_ThreadSchedulerImpl::GetThreadInfoString()
{
	if (!IsActive())
		return UNI_L("Unknown context");
	else
		return GetCurrentThread()->GetInfoString();
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::GetLastError()
{
	return last_error;
}

/* virtual */
void
ES_ThreadSchedulerImpl::ResumeIfNeeded()
{
	/* PostRunMessage does all the necessary checks anyway, so we just call it
	   from here.  Timeout messages aren't paused, so we don't need to care
	   about them. */
	PostRunMessage();
}

/* virtual */
void
ES_ThreadSchedulerImpl::PushNestedActivation()
{
	OP_ASSERT(is_active || !"Or this isn't needed and PopNestedActivation will set is_active to the wrong value.");
	nested_count++;
	is_active = FALSE;

	OP_ASSERT(nested_count > 0);
}

/* virtual */
void
ES_ThreadSchedulerImpl::PopNestedActivation()
{
	nested_count--;
	is_active = TRUE;

	OP_ASSERT(nested_count >= 0);
}

/* virtual */
BOOL
ES_ThreadSchedulerImpl::IsPresentOnTheStack()
{
	return is_active || nested_count > 0;
}

/* virtual */
OP_STATUS
ES_ThreadSchedulerImpl::SerializeThreads(ES_Thread *execute_first, ES_Thread *execute_last)
{
	if (execute_first->GetScheduler() != execute_last->GetScheduler())
	{
		ES_ForeignThreadBlock *foreign_thread_block = OP_NEW(ES_ForeignThreadBlock, (execute_first, execute_last, FALSE));
		if (!foreign_thread_block)
			return OpStatus::ERR_NO_MEMORY;
		foreign_thread_block->Init();
	}
	return OpStatus::OK;
}

/* virtual */
void
ES_ThreadSchedulerImpl::GCTrace()
{
	ES_Thread *thread;

	for (thread = static_cast<ES_Thread *>(runnable.First()); thread; thread = static_cast<ES_Thread *>(thread->Suc()))
		thread->GCTrace(runtime);
}

/* virtual */
void
ES_ThreadSchedulerImpl::HandleCallback(OpMessage msg, MH_PARAM_1, MH_PARAM_2)
{
#ifdef MINI
	MINI_SET_ACTIVE_WINDOW(window);
#endif // MINI

	OP_STATUS ret = OpStatus::OK;
	OpStatus::Ignore(ret);

	switch (msg)
	{
	case MSG_ES_RUN:
		if (has_posted_run)
		{
			has_posted_run = FALSE;
			ret = RunNow();
		}
		break;

	case MSG_ES_TIMEOUT:
		OP_ASSERT(!"Should not happen.");
		break;
	}

	if (OpStatus::IsMemoryError(ret))
	{
		if (window)
			window->RaiseCondition(OpStatus::ERR_SOFT_NO_MEMORY);
		else
			g_memory_manager->RaiseCondition(ret);
	}
	else if (OpStatus::IsError(ret))
		OP_ASSERT(!"Unhandled error.");
}

BOOL
ES_ThreadSchedulerImpl::IsEnabled()
{
	if (always_enabled)
		return TRUE;
	else if (frames_doc)
		if (frames_doc->GetDOMEnvironment())
			return frames_doc->GetDOMEnvironment()->IsEnabled();
		else
			/* The scheduler is owned by its FramesDocument's DOM environment,
			   so it is really strange if the FramesDocument doesn't have one.
			   Except in one situation: while the DOM environment is being
			   created.  And since it wouldn't be created if it wasn't enabled,
			   we can safely return TRUE here. */
			return TRUE;
	else if (runtime && is_dom_runtime && !DOM_Utils::IsRuntimeEnabled(DOM_Utils::GetDOM_Runtime(runtime)))
		return FALSE;
	else
		return g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled);
}

BOOL
ES_ThreadSchedulerImpl::IsPaused()
{
	if (frames_doc && frames_doc->GetWindow()->IsEcmaScriptPaused())
		return TRUE;

	return FALSE;
}

void
ES_ThreadSchedulerImpl::SuspendActiveTask()
{
	OP_ASSERT(IsPresentOnTheStack());

	if (current_thread)
		current_thread->Suspend();
}

OP_STATUS
ES_ThreadSchedulerImpl::RunNow()
{
	OP_STATUS ret = OpStatus::OK;
	BOOL suspend = FALSE;

	/* It is very bad if this function is called (indirectly) recursively. */
	if (is_active)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	/* Check if the runtime has been disabled and kill all tasks if it has.
	   Should probably allow event threads to perform their default actions. */
	if (!IsEnabled())
	{
		RemoveThreads(TRUE);
		Activate(); // So that we're not half dead in case someone later on enables script again
		return OpStatus::OK;
	}

	/* Check if we've been paused.  If so just return, without calling
	   PostRunMessage.  A new message will be posted when we're unpaused. */
	if (IsPaused())
		return OpStatus::OK;

	/* If our document isn't the current document don't execute threads in it. */
	if (frames_doc && !frames_doc->IsCurrentDoc())
	{
		RemoveThreads(TRUE);
		return OpStatus::OK;
	}

	TimesliceStart(ESUTILS_TIMESLICE_LENGTH);
	current_thread = (ES_Thread *) runnable.First();

	is_active = TRUE;
	while (!suspend && !remove_all_threads && HasRunnableTasks() && !IsBlocked() && !TimesliceExpired() && OpStatus::IsSuccess(ret))
	{
		if (!current_thread->IsCompleted())
			ret = current_thread->EvaluateThread();

		// current_thread can be changed by the call to EvaluateThread():
		// coverity[check_after_deref: FALSE]
		if (current_thread)
		{
			/* This will break out of the run loop, post a message and restart
			   the thread when that message is processed.  This means that any
			   other messages that have been posted while the thread executed
			   will be processed before the thread continues.  This is not
			   exactly beautiful perhaps, but sufficient in some cases. */
			suspend = current_thread->IsSuspended();

			if (current_thread->IsCompleted() || OpStatus::IsError(ret))
			{
				/* When the current thread is cancelled, it is signalled but left in
				   the list of runnable threads.  In such case, the thread shouldn't
				   be signalled again. */
				if (!current_thread->IsSignalled())
				{
					BOOL failed = current_thread->IsFailed() || OpStatus::IsError(ret);
					ES_ThreadSignal signal = failed ? ES_SIGNAL_FAILED : ES_SIGNAL_FINISHED;
					last_error = ret;

					if (current_thread->Signal(signal) == OpStatus::ERR_NO_MEMORY)
						ret = OpStatus::ERR_NO_MEMORY;
				}

				if (current_thread->IsSyncThread())
				{
					/* This was all the caller wanted to run, exit from the scheduler
					   so that we can unnest the message loop before something
					   unexpected is run. */
					suspend = TRUE;
				}

				if (!current_thread->Pred())
				{
					/* Don't remove the thread if it has been interrupted by a
					   listener that was just signalled. */
					Remove(current_thread);
				}
			}
		}

		current_thread = (ES_Thread *) runnable.First();
	}
	is_active = FALSE;

	if (remove_all_threads)
	{
		remove_all_threads = FALSE;
		RemoveThreads(TRUE);
	}

	if (!OpStatus::IsError(ret))
	{
		if (HasRunnableTasks())
			return PostRunMessage();
		else if (!timer_manager.HasWaiting())
			activity_script.Cancel();

		return OpStatus::OK;
	}

	HandleError();
	return ret;
}

void
ES_ThreadSchedulerImpl::HandleError()
{
	if (!IsActive())
	{
		/* Something went wrong, nuke all threads and reset everything to a good state.
		   This may be overly drastic. */

		while (ES_Thread *thread = (ES_Thread *) runnable.First())
		{
			if (thread->IsSignalled())
			{
				thread->Out();
				OP_DELETE(thread);
			}
			else
				OpStatus::Ignore(thread->Signal(ES_SIGNAL_CANCELLED));
		}

		timer_manager.RemoveAllEvents();

		is_active = has_posted_run = FALSE;
		current_thread = NULL;
	}
}

OP_STATUS
ES_ThreadSchedulerImpl::PostRunMessage()
{
	if (is_terminated || has_posted_run || !HasRunnableTasks() || IsBlocked() || IsActive() || IsPaused())
		return OpStatus::OK;

	RETURN_IF_ERROR(SetCallbacks());

	has_posted_run = TRUE;

	if (!msg_handler->PostMessage(MSG_ES_RUN, id, 0))
	{
		has_posted_run = FALSE;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ThreadSchedulerImpl::SetCallbacks()
{
	if (!has_set_callbacks)
	{
		if (OpStatus::IsMemoryError(msg_handler->SetCallBack(this, MSG_ES_RUN, id)))
		{
			msg_handler->UnsetCallBacks(this);
			return OpStatus::ERR_NO_MEMORY;
		}

		has_set_callbacks = TRUE;
	}

	return OpStatus::OK;
}

void
ES_ThreadSchedulerImpl::UnsetCallbacks()
{
	if (has_set_callbacks)
		msg_handler->UnsetCallBacks(this);

	has_set_callbacks = has_posted_run = FALSE;
}

OP_STATUS
ES_ThreadSchedulerImpl::Terminate(ES_TerminatingThread *thread)
{
	ES_Thread *t = (ES_Thread *) runnable.First();
	OP_STATUS ret = OpStatus::OK;

	while (t && OpStatus::IsSuccess(ret))
	{
		ret = t->Signal(ES_SIGNAL_SCHEDULER_TERMINATED);
		t = (ES_Thread *) t->Suc();
	}

	if (OpStatus::IsError(ret))
		HandleError();

	if (HasRunnableTasks())
	{
		OP_ASSERT(HasTerminatingThread());
		is_draining = TRUE;
	}

	return ret;
}

BOOL ES_TimesliceExpired(void *data)
{
	return static_cast<ES_ThreadSchedulerImpl *>(data)->TimesliceExpired();
}

void
ES_ThreadSchedulerImpl::TimesliceStart(unsigned timeslice_length)
{
	timeslice_start = g_op_time_info->GetRuntimeMS();
	timeslice_ms = 0;
	timeslice_max_len = timeslice_length;
}

BOOL
ES_ThreadSchedulerImpl::TimesliceExpired()
{
	timeslice_ms = (unsigned long) (g_op_time_info->GetRuntimeMS() - timeslice_start);
	return timeslice_ms >= timeslice_max_len;
}

/* Calculate the first thread in this scheduler that must be interrupted by a
   thread added to this scheduler but interrupts a thread in another scheduler. */
ES_Thread *
ES_ThreadSchedulerImpl::GetLocalInterruptThread(ES_Thread *interrupt_thread, BOOL check_script_elms)
{
	OP_ASSERT(interrupt_thread->GetScheduler() != this);

	ES_Thread *thread = interrupt_thread, *candidate = NULL;
	while (thread)
	{
		if (ES_Thread *t = thread->GetInterruptedThread())
			if (t->GetScheduler() != thread->GetScheduler())
			{
				if (t->GetScheduler() != this)
					t = GetLocalInterruptThread(t);

				if (t && (!candidate || t->Precedes(candidate)))
					candidate = t;
			}

		thread = (ES_Thread *) thread->Suc();
	}

	if (check_script_elms && frames_doc)
	{
		ES_LoadManager::ScriptElm *interrupt_elm = frames_doc->GetHLDocProfile()->GetESLoadManager()->FindScriptElm(interrupt_thread, FALSE, TRUE);

		if (interrupt_elm)
		{
			ES_LoadManager::ScriptElm *script_elm = static_cast<ES_LoadManager::ScriptElm *>(interrupt_elm->Suc());

			while (script_elm)
			{
				if (script_elm->thread && script_elm->thread->GetScheduler())
				{
					ES_Thread *local = script_elm->thread->GetScheduler() == this ? script_elm->thread : GetLocalInterruptThread(script_elm->thread, FALSE);

					if (local)
					{
						if (!candidate || local->Precedes(candidate))
							candidate = local;

						/* The runnable list in this scheduler and script
						   element list in the corresponding load manager will
						   have matching order, so the first relevant thread we
						   find will be the only one we need to consider. */
						break;
					}
				}

				script_elm = static_cast<ES_LoadManager::ScriptElm *>(script_elm->Suc());
			}
		}
	}

	return candidate;
}

