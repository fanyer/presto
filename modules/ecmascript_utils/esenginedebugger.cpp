/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; coding: iso-8859-1 -*-
 *
 * Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#ifdef ECMASCRIPT_DEBUGGER

#include "modules/ecmascript_utils/esenginedebugger.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript/carakan/src/ecmascript_manager.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/fdelm.h"
#include "modules/dom/domevents.h"
#include "modules/dom/domutils.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/tempbuf.h"

class ES_DebugThreadListener
	: public ES_ThreadListener
{
protected:
	ES_EngineDebugBackend *backend;
	ES_DebugThread *dbg_thread;

public:
	ES_DebugThreadListener(ES_EngineDebugBackend *backend, ES_DebugThread *dbg_thread)
		: backend(backend),
		  dbg_thread(dbg_thread)
	{
	}
	virtual ~ES_DebugThreadListener();
	/**< Destructor.  Removes the listener from its debug thread, if it belongs to
	     a debug thread. */

	void DetachDebugThread() { dbg_thread = NULL; }

	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);
};

class ES_DebugThread
	: public Link
{
public:
	ES_DebugRuntime *dbg_runtime;
	ES_Thread *thread;
	ES_DebugThreadListener *listener;
	ES_Context *context;
	BOOL started;
	ES_DebugPosition last_position;
	ES_DebugReturnValue *dbg_return_value;
	ES_DebugPosition last_return_position;
	/**< The last remembered position when we were about to call a function
	     or about to return from one, whichever came last. */
	BOOL delete_return_values;
	/**< Flag indicating that return values should be cleared
	     as soon as next debugger event is being handled. */
	unsigned id, parent_id, current_depth, target_depth;
	ES_DebugFrontend::Mode mode;
	BOOL        allow_event_break;
	/**<
	 * If TRUE the event thread can trigger any event breakpoints.
	 * Used to avoid triggering event breakpoints multiple times in the same thread.
	 */

	ES_DebugThread(ES_DebugRuntime *dbg_runtime, ES_Context *context, unsigned id);
	~ES_DebugThread();

	OP_STATUS GetEventStr(OpString &str);
	/**< Store the event name in 'str'.

	     @param str [out] The location to store the event name.
	     @return OpStatus::OK on success, OpStatus::ERR if the thread has no
	             event, or OpStatus::ERR_NO_MEMORY. */

	const uni_char *GetNamespace();
	/**< Get the namespace URI for this event.

	     @return A string containing the namespace URI, or NULL if the event has
	             no namespace. Also NULL if feature DOM3_EVENTS is disabled. */

	BOOL IsEventThread(const uni_char *ev, const uni_char *ns);
	/**< Check whether this thread is an event thread with the specified name
	     and namespace.

	     @param ev The event name (e.g. "DOMContentLoaded"). Can be NULL, which
	               means "any event".
	     @param ns The event namespace (e.g. "http://www.w3.org/2001/xml-events").
	               Can be NULL, which means "any namespace".
	     @return TRUE if this thread does match the criteria, FALSE otherwise. */
private:
	void GetEventNames(const char *&event_name, const uni_char *&custom_name);
	/**< Get the known event name (x)or custom event name for this thread.

	     @param event_name [out] This pointer is set to the known event name,
	                             or NULL if there is no known event name.
	     @param custom_name [out] Set to the custom event name, or NULL if
	                              there is no custom event name. */
};

class ES_DebugRuntime
	: public Link
{
public:
	ES_Runtime *runtime;
	ES_ThreadScheduler *GetScheduler() { return runtime->GetESScheduler(); }
	ES_DebugThread *blocked_dbg_thread;
	BOOL debug_window, information_sent;
	Head dbg_threads;
	OpINT32Set m_scripts_to_stop;	// id of scripts that will be stopped when execution starts
	unsigned id;

	ES_DebugRuntime(ES_Runtime *runtime, BOOL debug_window, unsigned id);
	~ES_DebugRuntime();

	void ThreadMigrated(ES_DebugThread *dbg_thread, ES_DebugRuntime *from);
	/**< Migrate a thread from a specified runtime to this runtime.

	     The specified thread ('dbg_thread') must be owned by the specified
	     runtime ('from') when this function is called.

	     @param dbg_thread The thread to migrate.
	     @param from The runtime to migrate from. */

	ES_DebugThread *GetDebugThread(ES_Thread *thread) const;
	/**< Find the ES_DebugThread for a ES_Thread.

	     @param thread The ES_Thread to find the ES_DebugThread for.
	     @return A ES_DebugThread, or NULL if this runtime has no
	             ES_DebugThread for the specified ES_Thread. */

};

class ES_DebugWindow
	: public Link
{
public:
	Window *window;
	unsigned Id() { return window->Id(); };
};

class ES_DebugBreakpoint
	: public Link
{
public:
	enum { TYPE_INVALID, TYPE_POSITION, TYPE_FUNCTION, TYPE_EVENT };

	unsigned id;
	unsigned type;
	union
	{
		struct
		{
			unsigned scriptid;
			unsigned linenr;
		} position;
		struct
		{
			uni_char *namespace_uri;
			uni_char *event_type;
		} event;
	} data;
	ES_ObjectReference function;
	unsigned window_id;

	ES_DebugBreakpoint(unsigned id)
		: id(id),
		  type(TYPE_INVALID),
		  window_id(0)
	{
	}

	~ES_DebugBreakpoint()
	{
		if (type == TYPE_EVENT)
		{
			OP_DELETEA(data.event.namespace_uri);
			OP_DELETEA(data.event.event_type);
		}
	}

	OP_STATUS Init(const ES_DebugBreakpointData &data0, ES_Runtime *runtime)
	{
		if (data0.type == ES_DebugBreakpointData::TYPE_POSITION)
		{
			data.position.scriptid = data0.data.position.scriptid;
			data.position.linenr = data0.data.position.linenr;
			type = TYPE_POSITION;
		}
		else if (data0.type == ES_DebugBreakpointData::TYPE_FUNCTION)
		{
			RETURN_IF_ERROR(function.Protect(runtime, data0.data.function));
			type = TYPE_FUNCTION;
		}
		else if (data0.type == ES_DebugBreakpointData::TYPE_EVENT)
		{
			data.event.event_type = NULL;
			data.event.namespace_uri = NULL;

			if (data0.data.event.event_type)
			{
				data.event.event_type = UniSetNewStr(data0.data.event.event_type);
				RETURN_OOM_IF_NULL(data.event.event_type);
			}

			if (data0.data.event.namespace_uri)
			{
				data.event.namespace_uri = UniSetNewStr(data0.data.event.namespace_uri);
				RETURN_OOM_IF_NULL(data.event.namespace_uri);
			}

			window_id = data0.window_id;
			type = TYPE_EVENT;
		}
		return OpStatus::OK;
	}
};

/* ===== ES_DebugThreadListener ====================================== */

/* virtual */
ES_DebugThreadListener::~ES_DebugThreadListener()
{
	if (dbg_thread)
		dbg_thread->listener = NULL;
}

/* virtual */ OP_STATUS
ES_DebugThreadListener::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	if (!dbg_thread)
		return OpStatus::OK;
	backend->ThreadFinished(dbg_thread, signal);
	if (dbg_thread)
		dbg_thread->started = FALSE;
	return OpStatus::OK;
}

ES_DebugThread::ES_DebugThread(ES_DebugRuntime *dbg_runtime, ES_Context *context, unsigned id)
	: dbg_runtime(dbg_runtime),
	  thread(NULL),
	  listener(NULL),
	  context(context),
	  started(FALSE),
	  dbg_return_value(NULL),
	  delete_return_values(FALSE),
	  id(id),
	  parent_id(0),
	  current_depth(1),
	  target_depth(0),
	  mode(ES_DebugFrontend::RUN),
	  allow_event_break(TRUE)
{
}

ES_DebugThread::~ES_DebugThread()
{
	if (dbg_runtime->blocked_dbg_thread == this)
		dbg_runtime->blocked_dbg_thread = 0;
	OP_DELETE(listener);
	OP_DELETE(dbg_return_value);
}

OP_STATUS
ES_DebugThread::GetEventStr(OpString &str)
{
	const char* event_name;
	const uni_char *custom_name;

	GetEventNames(event_name, custom_name);

	if (event_name)
		return str.Set(event_name);
	else if(custom_name)
		return str.Set(custom_name);

	OP_ASSERT(!"Missing event name");
	return OpStatus::ERR;
}

const uni_char *
ES_DebugThread::GetNamespace()
{
	return DOM_Utils::GetNamespaceURI(thread);
}

BOOL
ES_DebugThread::IsEventThread(const uni_char *ev, const uni_char *ns)
{
	BOOL known_event_eq = TRUE;
	BOOL custom_event_eq = FALSE;
	BOOL namespace_eq = TRUE;

	if (ev)
	{
		const char* event_name;
		const uni_char *custom_name;

		GetEventNames(event_name, custom_name);

		known_event_eq = (event_name && uni_str_eq(ev, event_name));
		custom_event_eq = (custom_name && uni_str_eq(ev, custom_name));
	}

	if (ns)
	{
		const uni_char *namespace_uri = GetNamespace();
		namespace_eq = (namespace_uri && uni_str_eq(ns, namespace_uri));
	}

	return ((known_event_eq || custom_event_eq) && namespace_eq);
}

void
ES_DebugThread::GetEventNames(const char *&event_name, const uni_char *&custom_name)
{
	ES_Thread *interrupted = thread->GetInterruptedThread();
	ES_Thread *event_thread = interrupted ? interrupted : thread;

	event_name = DOM_EventsAPI::GetEventTypeString(event_thread->GetInfo().data.event.type);
	custom_name = NULL;

	if (!event_name)
		custom_name = DOM_Utils::GetCustomEventName(event_thread);
}

ES_DebugRuntime::ES_DebugRuntime(ES_Runtime *runtime, BOOL debug_window, unsigned id)
	: runtime(runtime),
	  blocked_dbg_thread(NULL),
	  debug_window(debug_window),
	  information_sent(FALSE),
	  id(id)
{
}

ES_DebugRuntime::~ES_DebugRuntime()
{
	dbg_threads.Clear();
}

void
ES_DebugRuntime::ThreadMigrated(ES_DebugThread *dbg_thread, ES_DebugRuntime *from)
{
	OP_ASSERT(dbg_thread->dbg_runtime == from);

	if (dbg_thread->dbg_runtime == from)
	{
		dbg_thread->Out();
		dbg_thread->dbg_runtime = this;
		dbg_thread->Into(&dbg_threads);
	}
}

ES_DebugThread *
ES_DebugRuntime::GetDebugThread(ES_Thread *thread) const
{
	ES_DebugThread *dbg_thread = static_cast<ES_DebugThread *>(dbg_threads.First());

	while (dbg_thread)
	{
		if (dbg_thread->thread == thread)
			return dbg_thread;

		dbg_thread = static_cast<ES_DebugThread *>(dbg_thread->Suc());
	}

	return NULL;
}

class ES_DebugEvalCallback
	: public ES_AsyncCallback
{
protected:
	ES_EngineDebugBackend *backend;
	ES_DebugFrontend *frontend;
	ES_DebugRuntime *dbg_runtime;
	ES_Thread *thread;
	unsigned tag;

public:
	ES_DebugEvalCallback(ES_EngineDebugBackend *backend, ES_DebugFrontend *frontend, ES_DebugRuntime *dbg_runtime, unsigned tag)
		: backend(backend),
		  frontend(frontend),
		  dbg_runtime(dbg_runtime),
		  thread(NULL),
		  tag(tag)
	{
	}

	virtual ~ES_DebugEvalCallback()
	{
		if (backend)
			backend->DetachEvalCallback(this);
	}

	void DetachBackend(ES_EngineDebugBackend *b) { backend = NULL; }

	void SetRunningThread(ES_Thread *new_thread) { thread = new_thread; }
	/**< Sets the thread which will eventually result in this callback being
	     called.  This thread will be cancelled if the ES_EngineDebugBackend is
	     destroyed before the callback is called. */

	ES_Thread *GetRunningThread() const { return thread; }
	/**< Returns the thread which will eventually result in this callback being
	     called. */

	ES_Runtime *GetRuntime() const { return (dbg_runtime ? dbg_runtime->runtime : NULL); }
	/**< @return the runtime associated with this Eval, or NULL. */

	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
	{
		thread = NULL;

		if (backend == NULL)
		{
			ES_DebugEvalCallback *cb = this;
			OP_DELETE(cb);
			return OpStatus::OK;
		}

		ES_DebugValue value;

		ES_DebugFrontend::EvalStatus evalstatus;
		switch (status)
		{
		case ES_ASYNC_SUCCESS:
			evalstatus = ES_DebugFrontend::EVAL_STATUS_FINISHED;
			backend->ExportValue(dbg_runtime->runtime, result, value);
			break;

		case ES_ASYNC_FAILURE:
			evalstatus = ES_DebugFrontend::EVAL_STATUS_ABORTED;
			break;

		case ES_ASYNC_EXCEPTION:
			evalstatus = ES_DebugFrontend::EVAL_STATUS_EXCEPTION;
			backend->ExportValue(dbg_runtime->runtime, result, value);
			break;

		default:
			evalstatus = ES_DebugFrontend::EVAL_STATUS_CANCELLED;
		}

		OP_STATUS op_status = frontend->EvalReply(tag, evalstatus, value);
		OP_DELETE(this);
		return op_status;
	}

	virtual OP_STATUS HandleError(const uni_char *message, unsigned line, unsigned offset)
	{
		if (backend == NULL)
			return OpStatus::OK;

		return frontend->EvalError(tag, message, line, offset);
	}
};

class ES_EvalCallbackLink
	: public Link
{
public:
	ES_EvalCallbackLink(ES_DebugEvalCallback *callback) : callback(callback) {}
	ES_DebugEvalCallback *callback;
};

ES_EngineDebugBackend::ES_EngineDebugBackend()
	: current_dbg_thread(NULL),
	  accepter(NULL),
	  common_runtime(NULL),
	  stop_at_script(TRUE),
	  stop_at_exception(FALSE),
	  stop_at_error(FALSE),
	  stop_at_handled_error(FALSE),
	  stop_at_abort(FALSE),
	  include_objectinfo(TRUE),
	  next_dbg_runtime_id(1),
	  next_dbg_thread_id(1),
	  break_dbg_runtime_id(0),
	  break_dbg_thread_id(0)
{
	/* Two engine backends currently doesn't work. */
	OP_ASSERT(!g_opera->ecmascript_utils_module.engine_debug_backend);
	g_opera->ecmascript_utils_module.engine_debug_backend = this;
}

ES_EngineDebugBackend::~ES_EngineDebugBackend()
{
	OP_ASSERT(g_opera->ecmascript_utils_module.engine_debug_backend == this);
	g_opera->ecmascript_utils_module.engine_debug_backend = NULL;

	CancelEvalThreads();

	m_function_filter.DeleteAll();

	dbg_breakpoints.Clear();
	dbg_runtimes.Clear();
	dbg_windows.Clear();

	g_ecmaManager->SetDebugListener(NULL);

	common_runtime->Detach();
}

OP_STATUS
ES_EngineDebugBackend::Construct(ES_DebugFrontend *new_frontend, ES_DebugWindowAccepter* new_accepter)
{
	OpAutoPtr<ES_Runtime> crt_ptr(OP_NEW(ES_Runtime, ()));
	RETURN_OOM_IF_NULL(crt_ptr.get());
	RETURN_IF_ERROR(crt_ptr->Construct());
	common_runtime = crt_ptr.release();

	accepter = new_accepter;

	frontend = new_frontend;
	g_ecmaManager->SetDebugListener(this);
	g_ecmaManager->SetWantReformatScript(false);
	g_ecmaManager->SetReformatConditionFunction(NULL);

	return OpStatus::OK;
}

void
ES_EngineDebugBackend::ImportValue(ES_Runtime *runtime, const ES_DebugValue &external, ES_Value &internal)
{
	internal.type = VALUE_UNDEFINED;

	switch (external.type)
	{
	case VALUE_UNDEFINED:
	case VALUE_NULL:
		break;

	case VALUE_BOOLEAN:
		internal.value.boolean = external.value.boolean;
		break;

	case VALUE_NUMBER:
		internal.value.number = external.value.number;
		break;

	case VALUE_STRING:
		if (external.is_8bit)
		{
			OpString temporary;
			uni_char *string = NULL;
			if (OpStatus::IsError(temporary.SetFromUTF8(external.value.string8.value, external.value.string8.length)) ||
			    OpStatus::IsError(UniSetStr(string, temporary.CStr())))
				return;
			internal.value.string = string;
		}
		else
		{
			uni_char *string = NULL;
			if (OpStatus::IsError(UniSetStrN(string, external.value.string16.value, external.value.string16.length)))
				return;
			internal.value.string = string;
		}
		break;

	case VALUE_OBJECT:
		internal.value.object = objman.GetObject(runtime, external.value.object.id);
		if (!internal.value.object)
			return;
	}

	internal.type = external.type;
}

void
ES_EngineDebugBackend::ThreadFinished(ES_DebugThread *dbg_thread, ES_ThreadSignal signal)
{
	if (dbg_thread->listener)
		dbg_thread->listener->DetachDebugThread();
	dbg_thread->listener = NULL;

	ES_DebugFrontend::ThreadStatus status;

	if (signal == ES_SIGNAL_FINISHED)
	{
		status = ES_DebugFrontend::THREAD_STATUS_FINISHED;

		ES_DebugReturnValue *dbg_return_value = OP_NEW(ES_DebugReturnValue, ());
		if (dbg_return_value)
		{
			dbg_return_value->function.id = 0;

			ES_Value internal_value;
			if (!dbg_thread->context || OpStatus::IsError(ES_Runtime::GetReturnValue(dbg_thread->context, &internal_value)))
				internal_value.type = VALUE_UNDEFINED;
			ExportValue(dbg_thread->dbg_runtime->runtime, internal_value, dbg_return_value->value);

			dbg_return_value->next = dbg_thread->dbg_return_value;
			dbg_thread->dbg_return_value = dbg_return_value;
		}
	}
	else if (signal == ES_SIGNAL_FAILED)
	{
		// FIXME: find out what happened and retrieve the exception, if possible.
		status = ES_DebugFrontend::THREAD_STATUS_ABORTED;
	}
	else
		status = ES_DebugFrontend::THREAD_STATUS_CANCELLED;

	OpStatus::Ignore(frontend->ThreadFinished(dbg_thread->dbg_runtime->id, dbg_thread->id, status, dbg_thread->dbg_return_value));
	dbg_thread->started = FALSE;
	dbg_thread->dbg_return_value = NULL;
}

OP_STATUS
ES_EngineDebugBackend::ThreadMigrated(ES_Thread *thread, ES_Runtime *from, ES_Runtime *to)
{
	ES_DebugRuntime *dbg_from, *dbg_to;

	OpStatus::Ignore(GetDebugRuntime(dbg_from, from, FALSE));

	if (dbg_from)
	{
		RETURN_IF_ERROR(GetDebugRuntime(dbg_to, to, TRUE));

		if (dbg_to)
		{
			ES_DebugThread *dbg_thread = dbg_from->GetDebugThread(thread);

			if (dbg_thread)
			{
				dbg_to->ThreadMigrated(dbg_thread, dbg_from);
				OpStatus::Ignore(frontend->ThreadMigrated(dbg_thread->id, dbg_from->id, dbg_to->id));
			}
		}
	}

	return OpStatus::OK;
}

void
ES_EngineDebugBackend::AddRuntime(ES_DebugRuntime *dbg_runtime)
{
	dbg_runtime->Into(&dbg_runtimes);

	TempBuffer buffer;
	ES_DebugRuntimeInformation info;
	if (OpStatus::IsSuccess(GetRuntimeInformation(dbg_runtime, info, buffer)))
		frontend->RuntimeStarted(dbg_runtime->id, &info);
}

unsigned
ES_EngineDebugBackend::GetWindowId(ES_Runtime* runtime)
{
	FramesDocument *frm_doc = runtime->GetFramesDocument();
	return frm_doc ? frm_doc->GetWindow()->Id() : 0;
}

BOOL
ES_EngineDebugBackend::AcceptRuntime(ES_Runtime *runtime)
{
	if (!accepter)
		return FALSE;

	OpVector<Window> windows;
	RETURN_VALUE_IF_ERROR(runtime->GetWindows(windows), FALSE);

	unsigned count = windows.GetCount();

	for (unsigned i = 0; i < count; ++i)
		if (accepter->AcceptWindow(windows.Get(i)))
			return TRUE;

	return FALSE;
}

/* virtual */ void
ES_EngineDebugBackend::Prune()
{
	ES_DebugRuntime *dbg_runtime, *next;

	for (dbg_runtime = (ES_DebugRuntime *) dbg_runtimes.First(); dbg_runtime; )
	{
		next = (ES_DebugRuntime *) dbg_runtime->Suc();

		if (!AcceptRuntime(dbg_runtime->runtime))
		{
			if (OpStatus::IsSuccess(Detach(dbg_runtime)))
			{
				dbg_runtime->Out();
				OP_DELETE(dbg_runtime);
			}
		}

		dbg_runtime = next;
	}
}

/* virtual */ void
ES_EngineDebugBackend::FreeObject(unsigned object_id)
{
	objman.Release(object_id);
}

/* virtual */ void
ES_EngineDebugBackend::FreeObjects()
{
	objman.ReleaseAll();
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Detach()
{
	OP_STATUS ret_val = OpStatus::OK;

	for (ES_DebugRuntime *dbg_runtime = (ES_DebugRuntime *) dbg_runtimes.First();
	     dbg_runtime;
	     dbg_runtime = (ES_DebugRuntime *) dbg_runtime->Suc())
	{
		OP_STATUS status = Detach(dbg_runtime);
		if (OpStatus::IsError(status))
			ret_val = status;
	}

	return ret_val;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Detach(ES_DebugRuntime *dbg_runtime)
{
	OP_STATUS status = OpStatus::OK;

	CancelEvalThreads(dbg_runtime->runtime);

	for (ES_DebugThread *dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();
	     dbg_thread;
	     dbg_thread = (ES_DebugThread *) dbg_thread->Suc())
		if (dbg_thread->thread && dbg_thread->thread->GetBlockType() == ES_BLOCK_DEBUGGER)
			if (OpStatus::IsMemoryError(dbg_thread->thread->Unblock(ES_BLOCK_DEBUGGER)))
			{
				OpStatus::Ignore(status);
				status = OpStatus::ERR_NO_MEMORY;
			}

	objman.Release(dbg_runtime->runtime);

	return status;
}

/**
	Add a debug runtime to the a list of debug runtime information.
	@param runtimes the list to add to
	@param debug_runtime the runtime to add
	@param buffer a temporary buffer
 */
OP_STATUS
ES_EngineDebugBackend::AddDebugRuntime(OpAutoVector<ES_DebugRuntimeInformation>& runtimes, ES_DebugRuntime* debug_runtime, TempBuffer& buffer)
{
	if (debug_runtime != NULL)
	{
		ES_DebugRuntimeInformation *info = OP_NEW(ES_DebugRuntimeInformation, ());

		buffer.Clear();
		GetRuntimeInformation(debug_runtime, *info, buffer);
		runtimes.Add(info);
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Runtimes(unsigned tag, OpUINTPTRVector &runtime_ids, BOOL force_create_all)
{
	OpAutoVector<ES_DebugRuntimeInformation> runtimes;

	// Enumerate specific runtimes, or all runtimes.

	TempBuffer buffer;

	if (runtime_ids.GetCount() > 0)
	{
		for (unsigned int i = 0; i < runtime_ids.GetCount(); ++i)
			AddDebugRuntime(runtimes, GetDebugRuntimeById(runtime_ids.Get(i)), buffer);
	}
	else
	{
		OpPointerSet<ES_Runtime> included_runtimes;

		for ( Window* w = g_windowManager->FirstWindow() ; w != NULL ; w = w->Suc() )
		{
			if (!(accepter && !accepter->AcceptWindow(w)))
			{
				DocumentTreeIterator dociter(w);
				dociter.SetIncludeThis();
				DocumentManager* docman = NULL;
				while (dociter.Next())
				{
					OP_ASSERT(docman != dociter.GetDocumentManager());
					docman = dociter.GetDocumentManager();
					if (docman != NULL)
					{
						FramesDocument *frm_doc = docman->GetCurrentDoc();
						if (frm_doc != NULL)
						{
							// Create the runtime if none exists, DOM/CSS inspectors
							// needs this for non-scripted pages to work.
							if (force_create_all && !frm_doc->GetESRuntime())
								RETURN_IF_MEMORY_ERROR(frm_doc->ConstructDOMEnvironment());

							OpVector<ES_Runtime> rts;
							RETURN_IF_ERROR(frm_doc->GetESRuntimes(rts));

							for (unsigned i = 0; i < rts.GetCount(); ++i)
							{
								ES_Runtime* rt = rts.Get(i);
								OP_ASSERT(rt);

								// Do not add the same runtime more than once.
								if (included_runtimes.Contains(rt))
									continue;

								ES_DebugRuntime* drt;
								if (OpStatus::IsSuccess(GetDebugRuntime(drt, rt, TRUE)) && drt)
								{
									RETURN_IF_ERROR(AddDebugRuntime(runtimes, drt, buffer));
									RETURN_IF_ERROR(included_runtimes.Add(rt));
								}
							}
						}
					}
				}
			}
		}
	}

	return frontend->RuntimesReply(tag, runtimes);
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Continue(unsigned dbg_runtime_id, ES_DebugFrontend::Mode new_mode)
{
	ES_DebugRuntime *dbg_runtime = GetDebugRuntimeById(dbg_runtime_id);

	if (dbg_runtime)
		if (ES_DebugThread *blocked_dbg_thread = dbg_runtime->blocked_dbg_thread)
		{
			// all threads must inherit the new mode
			for (ES_DebugThread *dbg_thread = static_cast<ES_DebugThread *>(dbg_runtime->dbg_threads.First()); dbg_thread; dbg_thread = static_cast<ES_DebugThread *>(dbg_thread->Suc()))
				dbg_thread->mode = new_mode;

			if (blocked_dbg_thread->mode == ES_DebugFrontend::STEPOVER
				|| blocked_dbg_thread->mode == ES_DebugFrontend::STEPINTO)
			{
				blocked_dbg_thread->target_depth = blocked_dbg_thread->current_depth;
				ES_DebugThread *parent_thread = static_cast<ES_DebugThread *>(blocked_dbg_thread->Pred());
				if (parent_thread)
					// We set the parent thread's target depth since we want it to stop when we return to it.
					parent_thread->target_depth = parent_thread->current_depth;
			}
			else if (blocked_dbg_thread->mode == ES_DebugFrontend::FINISH)
				blocked_dbg_thread->target_depth = blocked_dbg_thread->current_depth - 1;

			dbg_runtime->blocked_dbg_thread = NULL;
			return blocked_dbg_thread->thread->Unblock(ES_BLOCK_DEBUGGER);
		}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Backtrace(ES_Runtime *runtime, ES_Context *context, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames)
{
	OP_ASSERT(runtime);
	OP_ASSERT(context);

	ES_DebugStackElement *stack;

	g_ecmaManager->GetStack(context, max_frames == 0 ? (unsigned) ES_DebugControl::ALL_FRAMES : max_frames, &length, &stack);

	frames = OP_NEWA(ES_DebugStackFrame, length);
	if (!frames)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned index = 0; index < length; ++index)
	{
		ES_DebugStackElement &element = stack[index];
		ES_DebugStackFrame &frame = frames[index];

		ExportObject(runtime, element.function, frame.function);
		ExportObject(runtime, element.arguments, frame.arguments);
		ExportObject(runtime, element.this_obj, frame.this_obj);
		frame.variables = GetObjectId(runtime, element.variables);

		frame.script_guid = element.script_guid;
		frame.line_no = element.line_no;

		ES_Object **scope_chain;
		frame.scope_chain_length = length;
		g_ecmaManager->GetScope(context, length - (index + 1), &(frame.scope_chain_length), &scope_chain);

		frame.scope_chain = OP_NEWA(unsigned, frame.scope_chain_length);
		RETURN_OOM_IF_NULL(frame.scope_chain);

		for (unsigned j = 0; j < frame.scope_chain_length; ++j)
			frame.scope_chain[j] = GetObjectId(runtime, scope_chain[j]);
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Backtrace(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames)
{
	ES_DebugRuntime *dbg_runtime = GetDebugRuntimeById(dbg_runtime_id);

	if (dbg_runtime)
	{
		ES_Runtime *rt = dbg_runtime->runtime;
		ES_DebugThread *dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();

		while (dbg_thread && dbg_thread->id != dbg_thread_id)
			dbg_thread = (ES_DebugThread *) dbg_thread->Suc();

		if (dbg_thread && dbg_thread->context)
			return Backtrace(rt, dbg_thread->context, max_frames, length, frames);
	}

	length = 0;
	frames = NULL;

	return OpStatus::ERR;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Eval(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned frame_index, const uni_char *string, unsigned string_length, ES_DebugVariable *variables, unsigned variables_count, BOOL want_debugging)
{
	ES_DebugRuntime *dbg_runtime = GetDebugRuntimeById(dbg_runtime_id);

	if (dbg_runtime)
	{
		ES_DebugThread *dbg_thread;

		if (dbg_thread_id != 0)
		{
			for (dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();
			     dbg_thread && dbg_thread->id != dbg_thread_id;
			     dbg_thread = (ES_DebugThread *) dbg_thread->Suc()) {}

			if (!dbg_thread)
				return OpStatus::ERR;
		}
		else
			dbg_thread = NULL;

		unsigned length;
		ES_Object **scope, **allocated_scope = NULL, *this_object = NULL;
		ES_Thread *interrupt_thread;

		if (dbg_runtime->blocked_dbg_thread)
			interrupt_thread = dbg_runtime->blocked_dbg_thread->thread;
		else
			interrupt_thread = NULL;

		if (dbg_thread)
		{
#if 0
			if (frame_index == 0)
				frame_index = ES_DebugControl::OUTERMOST_FRAME;
			else
			{
				unsigned length;
				ES_DebugStackElement *stack;

				g_ecmaManager->GetStack(dbg_thread->context, ES_DebugControl::ALL_FRAMES, &length, &stack);

				frame_index = length - frame_index - 1;
			}
#else
			{
 				ES_DebugStackElement *stack;

				g_ecmaManager->GetStack(dbg_thread->context, ES_DebugControl::ALL_FRAMES, &length, &stack);

				if (frame_index > length)
					return OpStatus::ERR_OUT_OF_RANGE;

				this_object = stack[frame_index].this_obj;
				frame_index = length - frame_index - 1;
			}
#endif

			g_ecmaManager->GetScope(dbg_thread->context, frame_index, &length, &scope);
		}
		else
		{
			scope = NULL;
			length = 0;
		}

		ES_DebugEvalCallback *callback = OP_NEW(ES_DebugEvalCallback, (this, frontend, dbg_runtime, tag));
		if (callback == NULL)
			return OpStatus::ERR_NO_MEMORY;
		ES_EvalCallbackLink *callback_link = OP_NEW(ES_EvalCallbackLink, (callback));
		if (callback_link == NULL)
		{
			OP_DELETE(callback);
			return OpStatus::ERR_NO_MEMORY;
		}
		callback_link->Into(&eval_callbacks);
		ES_AsyncInterface *asyncif = dbg_runtime->runtime->GetESAsyncInterface();

		ES_ProgramText program[1];
		program[0].program_text = string ? string : UNI_L("");
		program[0].program_text_length = string_length;

		if (variables_count != 0)
		{
			EcmaScript_Object *extrascope = OP_NEW(EcmaScript_Object, ());
			if (OpStatus::IsError(extrascope->SetObjectRuntime(dbg_runtime->runtime, dbg_runtime->runtime->GetObjectPrototype(), "Object")))
				OP_DELETE(variables);
			else
			{
				unsigned index;

				for (index = 0; index < variables_count; ++index)
				{
					ES_Value value;
					ImportValue(dbg_runtime->runtime, variables[index].value, value);

					OP_STATUS status = extrascope->Put(variables[index].name, value, TRUE);

					if (value.type == VALUE_STRING)
					{
						uni_char *todelete = const_cast<uni_char *>(value.value.string);
						OP_DELETEA(todelete);
					}

					if (OpStatus::IsError(status))
						break;
				}

				if (index == variables_count)
				{
					allocated_scope = OP_NEWA(ES_Object *, length + 1);
					op_memcpy(allocated_scope, scope, length * sizeof scope[0]);
					allocated_scope[length] = *extrascope;
					scope = allocated_scope;
					++length;
				}
			}
		}

		if (want_debugging)
		{
			asyncif->SetWantDebugging();
			/* Only when debugging that particular eval call is it
			   interesting to see formatted source. Otherwise script won't
			   even be reported to the debugger. */
			if (g_ecmaManager->GetWantReformatScript(dbg_runtime->runtime))
				asyncif->SetWantReformatSource();
		}

		asyncif->SetWantExceptions();
		OP_STATUS status = asyncif->Eval(program, 1, scope, length, callback, interrupt_thread, this_object);
		if (OpStatus::IsError(status))
		{
			callback_link->Out();
			OP_DELETE(callback_link);
			OP_DELETE(callback);
		}
		else
			callback->SetRunningThread(asyncif->GetLastStartedThread());
		OP_DELETEA(allocated_scope);
		return status;
	}
	else
		return OpStatus::ERR;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Examine(unsigned dbg_runtime_id, unsigned objects_count, unsigned *object_ids, BOOL examine_prototypes, BOOL skip_non_enum, ES_DebugPropertyFilters *filters, /* out */ ES_DebugObjectChain *&chains, unsigned int async_tag)
{
	OP_ASSERT(objects_count > 0);
	OP_ASSERT(object_ids);

	if (objects_count <= 0 || !object_ids)
		return OpStatus::ERR;

	ES_DebugRuntime *dbg_runtime = GetDebugRuntimeById(dbg_runtime_id);

	if (!dbg_runtime)
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::OK;

	chains = OP_NEWA(ES_DebugObjectChain, objects_count);
	if (!chains)
		return OpStatus::ERR_NO_MEMORY;

	// Use context of blocked thread if any, otherwise create a new one.
	ES_Context *context = NULL;
	BOOL destroy_context = FALSE;

	if (dbg_runtime->blocked_dbg_thread)
		context = dbg_runtime->blocked_dbg_thread->context;
	else
	{
		context = dbg_runtime->runtime->CreateContext(NULL, TRUE);
		if (!context)
		{
			OP_DELETEA(chains);
			return OpStatus::ERR_NO_MEMORY;
		}
		destroy_context = TRUE;
	}

	ES_Thread *interrupted_thread = dbg_runtime->blocked_dbg_thread ? dbg_runtime->blocked_dbg_thread->thread : NULL;

	ES_EngineDebugBackendGetPropertyListener* property_handler = OP_NEW(ES_EngineDebugBackendGetPropertyListener, (this, frontend, dbg_runtime_id, async_tag, interrupted_thread));
	status = property_handler ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(status))
		status = current_propertylisteners.Add(property_handler);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(property_handler);
		OP_DELETEA(chains);
		if (destroy_context)
			dbg_runtime->runtime->DeleteContext(context);
		return status;
	}

	property_handler->OnDebugObjectChainCreated(chains, objects_count);

	property_handler->SetSynchronousMode();
	// Examine the chain of each (root) object.
	for (unsigned i = 0; i < objects_count; ++i)
	{
		ES_Object *object = objman.GetObject(dbg_runtime->runtime, object_ids[i]);

		if (!object)
		{
			status = OpStatus::ERR;
			break;
		}

		status = ExamineChain(dbg_runtime->runtime, context, object, skip_non_enum, examine_prototypes, chains[i], filters, property_handler, async_tag);

		if (OpStatus::IsError(status))
			break;
	}
	property_handler->SetAsynchronousMode();

	unsigned unreported_objects = property_handler->GetNumberOfUnreportedObjects();
	while (unreported_objects--)
		property_handler->ObjectPropertiesDone();	// report objects without properties

	if (context && destroy_context)
		dbg_runtime->runtime->DeleteContext(context);

	return status;
}

/* virtual */ BOOL
ES_EngineDebugBackend::HasBreakpoint(unsigned id)
{
	for (ES_DebugBreakpoint *dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoints.First();
	     dbg_breakpoint;
	     dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoint->Suc())
		if (dbg_breakpoint->id == id)
			return TRUE;

	return FALSE;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::AddBreakpoint(unsigned id, const ES_DebugBreakpointData &data)
{
	ES_DebugBreakpoint *dbg_breakpoint = OP_NEW(ES_DebugBreakpoint, (id));
	if (!dbg_breakpoint || OpStatus::IsMemoryError(dbg_breakpoint->Init(data, NULL)))
	{
		OP_DELETE(dbg_breakpoint);
		return OpStatus::ERR_NO_MEMORY;
	}

	dbg_breakpoint->Into(&dbg_breakpoints);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::RemoveBreakpoint(unsigned id)
{
	for (ES_DebugBreakpoint *dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoints.First();
	     dbg_breakpoint;
	     dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoint->Suc())
		if (dbg_breakpoint->id == id)
		{
			dbg_breakpoint->Out();
			OP_DELETE(dbg_breakpoint);
			return OpStatus::OK;
		}

	return OpStatus::ERR;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::SetStopAt(StopType stop_type, BOOL value)
{
	switch (stop_type)
	{
	case STOP_TYPE_SCRIPT:
		stop_at_script = value;
		break;
	case STOP_TYPE_EXCEPTION:
		stop_at_exception = value;
		break;
	case STOP_TYPE_ERROR:
		stop_at_error = value;
		break;
	case STOP_TYPE_HANDLED_ERROR:
		stop_at_handled_error = value;
		break;
	case STOP_TYPE_ABORT:
		stop_at_abort = value;
		break;
	case STOP_TYPE_DEBUGGER_STATEMENT:
		// FIXME: Not implemented
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::GetStopAt(StopType stop_type, BOOL &return_value)
{
	switch (stop_type)
	{
	case STOP_TYPE_SCRIPT:
		return_value = stop_at_script;
		break;
	case STOP_TYPE_EXCEPTION:
		return_value = stop_at_exception;
		break;
	case STOP_TYPE_ERROR:
		return_value = stop_at_error;
		break;
	case STOP_TYPE_HANDLED_ERROR:
		return_value = stop_at_handled_error;
		break;
	case STOP_TYPE_ABORT:
		return_value = stop_at_abort;
		break;
	case STOP_TYPE_DEBUGGER_STATEMENT:
		// FIXME: Not implemented
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::Break(unsigned dbg_runtime_id, unsigned dbg_thread_id)
{
	break_dbg_runtime_id = dbg_runtime_id;
	break_dbg_thread_id = dbg_thread_id;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_EngineDebugBackend::GetFunctionPosition(unsigned dbg_runtime_id, unsigned function_id, unsigned &script_guid, unsigned &line_no)
{
	ES_DebugRuntime *dbg_runtime = GetDebugRuntimeById(dbg_runtime_id);

	if (dbg_runtime)
	{
		ES_Object *object = objman.GetObject(dbg_runtime->runtime, function_id);
		if (!object)
			return OpStatus::ERR_NO_SUCH_RESOURCE;

		return g_ecmaManager->GetFunctionPosition(object, script_guid, line_no);
	}
	return OpStatus::ERR;
}

/* virtual */ ES_Object *
ES_EngineDebugBackend::GetObjectById(ES_Runtime *runtime, unsigned object_id)
{
	return objman.GetObject(runtime, object_id);
}

/* virtual */ ES_Object *
ES_EngineDebugBackend::GetObjectById(unsigned object_id)
{
	return objman.GetObject(object_id);
}

/* virtual */ ES_Runtime *
ES_EngineDebugBackend::GetRuntimeById(unsigned runtime_id)
{
	ES_DebugRuntime* dbg_runtime = GetDebugRuntimeById(runtime_id);

	return dbg_runtime ? dbg_runtime->runtime : 0;
}

/* virtual */ unsigned
ES_EngineDebugBackend::GetObjectId(ES_Runtime *runtime, ES_Object *object)
{
	object = object ? ES_Runtime::Identity(object) : NULL;

	if (!object)
		return 0;

	unsigned id;
	OpStatus::Ignore(objman.GetId(runtime, object, id));
	return id;
}

/* virtual */ unsigned
ES_EngineDebugBackend::GetRuntimeId(ES_Runtime *runtime)
{
	ES_DebugRuntime *dbg_runtime;
	if (runtime && OpStatus::IsSuccess(GetDebugRuntime(dbg_runtime, runtime, TRUE)) && dbg_runtime)
		return dbg_runtime->id;
	else
		return 0;
}

/* virtual */ unsigned
ES_EngineDebugBackend::GetThreadId(ES_Thread *thread)
{
	ES_DebugThread *dbg_thread;
	if (OpStatus::IsSuccess(GetDebugThread(dbg_thread, thread, TRUE)) && dbg_thread)
		return dbg_thread->id;
	else
		return 0;
}

/* virtual */ ES_DebugReturnValue *
ES_EngineDebugBackend::GetReturnValue(unsigned dbg_runtime_id, unsigned dbg_thread_id)
{
	ES_DebugRuntime *dbg_runtime = GetDebugRuntimeById(dbg_runtime_id);

	if (dbg_runtime)
	{
		ES_DebugThread *dbg_thread = static_cast<ES_DebugThread *>(dbg_runtime->dbg_threads.First());

		while (dbg_thread && dbg_thread->id != dbg_thread_id)
			dbg_thread = static_cast<ES_DebugThread *>(dbg_thread->Suc());

		if (dbg_thread)
			return dbg_thread->dbg_return_value;
	}

	return NULL;
}

/* virtual */ BOOL
ES_EngineDebugBackend::EnableDebugging(ES_Runtime *runtime)
{
	return AcceptRuntime(runtime);
}

/* virtual */ void
ES_EngineDebugBackend::ExportObject(ES_Runtime *rt, ES_Object *internal, ES_DebugObject &external, BOOL chain_info)
{
	OP_NEW_DBG("ES_EngineDebugBackend::ExportObject", "esdb");
	OP_DBG(("ExportObject(internal=%p, external=%p)", internal, &external));

	external.id = GetObjectId(rt, internal);

	OP_DBG(("external.id ==> %d", external.id));

	if (include_objectinfo && internal)
	{
		external.info = OP_NEW(ES_DebugObjectInfo, ());
		if (external.info)
		{
			ES_DebugObjectElement attr;
			g_ecmaManager->GetObjectAttributes(rt, internal, &attr);

			external.info->flags.packed.is_function = (attr.type == OBJTYPE_NATIVE_FUNCTION || attr.type == OBJTYPE_HOST_FUNCTION);
			external.info->flags.packed.is_callable = (external.info->flags.packed.is_function || attr.type == OBJTYPE_HOST_CALLABLE_OBJECT);

			if (chain_info)
				ExportObject(rt, attr.prototype, external.info->prototype);
			else
				external.info->prototype.id = GetObjectId(rt, attr.prototype);

			external.info->class_name = const_cast<char*>(attr.classname);

			if (external.info->flags.packed.is_function)
			{
				external.info->function_name = const_cast<uni_char*>(attr.u.function.name);

				if (!external.info->function_name || uni_strlen(external.info->function_name) == 0 && attr.u.function.alias)
					external.info->function_name = const_cast<uni_char*>(attr.u.function.alias);
#ifdef _DEBUG
				char* f = uni_down_strdup(attr.u.function.name);
				if ( f != 0 )
					OP_DBG(("Function: %s", f));
				op_free(f);
#endif
			}
		}
	}
}

/* virtual */ void
ES_EngineDebugBackend::ExportValue(ES_Runtime *rt, const ES_Value &internal, ES_DebugValue &external, BOOL chain_info)
{
	external.type = internal.type;

	switch (internal.type)
	{
	case VALUE_UNDEFINED:
	case VALUE_NULL:
		break;

	case VALUE_BOOLEAN:
		external.value.boolean = internal.value.boolean;
		break;

	case VALUE_NUMBER:
		external.value.number = internal.value.number;
		break;

	case VALUE_STRING:
	{
		size_t len = internal.GetStringLength();
		external.value.string16.value = OP_NEWA(uni_char, len + 1);
		if (!external.value.string16.value)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			external.type = VALUE_UNDEFINED;
			break;
		}

		op_memcpy(external.value.string16.value, internal.value.string, (len + 1) * sizeof(uni_char));
		external.value.string16.length = len;
		external.owns_value = TRUE;
		break;
	}

	case VALUE_OBJECT:
		ExportObject(rt, internal.value.object, external.value.object, chain_info);
		external.owns_value = TRUE;
	}
}

/* virtual */ void
ES_EngineDebugBackend::NewScript(ES_Runtime *runtime, ES_DebugScriptData *data)
{
	ES_DebugRuntime *dbg_runtime;

	if (OpStatus::IsSuccess(GetDebugRuntime(dbg_runtime, runtime, TRUE)) && dbg_runtime)
	{
		ES_DebugRuntimeInformation info, *infop = NULL;
		TempBuffer buffer;

		if (!dbg_runtime->information_sent)
		{
			if (OpStatus::IsSuccess(GetRuntimeInformation(dbg_runtime, info, buffer)))
			{
				infop = &info;
				dbg_runtime->information_sent = TRUE;
			}
		}

		if (stop_at_script)
			switch (data->type)
			{
			case SCRIPT_TYPE_LINKED:
			case SCRIPT_TYPE_INLINE:
			case SCRIPT_TYPE_GENERATED:
			case SCRIPT_TYPE_EVENT_HANDLER:
			case SCRIPT_TYPE_EVAL:
			case SCRIPT_TYPE_TIMEOUT:
			case SCRIPT_TYPE_JAVASCRIPT_URL:
			case SCRIPT_TYPE_USER_JAVASCRIPT:
			case SCRIPT_TYPE_USER_JAVASCRIPT_GREASEMONKEY:
			case SCRIPT_TYPE_BROWSER_JAVASCRIPT:
			case SCRIPT_TYPE_EXTENSION_JAVASCRIPT:
				OpStatus::Ignore(dbg_runtime->m_scripts_to_stop.Add(data->script_guid));

			case SCRIPT_TYPE_DEBUGGER:
				break;

			default:
				OP_ASSERT(!"Decide on handling new script type!");
			}

		OpStatus::Ignore(frontend->NewScript(dbg_runtime->id, data, infop));
	}
}

/* virtual */ void
ES_EngineDebugBackend::ParseError(ES_Runtime *runtime, ES_ErrorInfo *err)
{
	ES_DebugRuntime *dbg_runtime;

	if (OpStatus::IsSuccess(GetDebugRuntime(dbg_runtime, runtime, TRUE)) && dbg_runtime && AcceptRuntime(dbg_runtime->runtime))
	{
		OpStatus::Ignore(frontend->ParseError(dbg_runtime->id, err));
	}
}

/* virtual */ void
ES_EngineDebugBackend::NewContext(ES_Runtime *runtime, ES_Context *context)
{
	ES_DebugRuntime *dbg_runtime;

	if (OpStatus::IsSuccess(GetDebugRuntime(dbg_runtime, runtime, TRUE)) && dbg_runtime)
	{
		ES_DebugThread *dbg_thread = OP_NEW(ES_DebugThread, (dbg_runtime, context, next_dbg_thread_id++));
		if (dbg_thread)
			dbg_thread->Into(&dbg_runtime->dbg_threads);
	}
}

/* virtual */ void
ES_EngineDebugBackend::EnterContext(ES_Runtime *runtime, ES_Context *context)
{
	ES_DebugRuntime *dbg_runtime;

	if (OpStatus::IsSuccess(GetDebugRuntime(dbg_runtime, runtime, FALSE)) && dbg_runtime)
		for (ES_DebugThread *dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();
		     dbg_thread;
		     dbg_thread = (ES_DebugThread *) dbg_thread->Suc())
			if (dbg_thread->context == context)
			{
				if (!dbg_thread->started)
				{
					dbg_thread->started = TRUE;
					dbg_thread->thread = dbg_runtime->GetScheduler()->GetCurrentThread();

					OpStatus::Ignore(SignalNewThread(dbg_thread));

					ES_DebugThread *parent_thread = GetDebugThreadById(dbg_runtime, dbg_thread->parent_id);

					if (parent_thread)
						dbg_thread->mode = parent_thread->mode;
				}

				OP_ASSERT(dbg_thread->thread == dbg_runtime->GetScheduler()->GetCurrentThread());

				current_dbg_thread = dbg_thread;
				return;
			}
}

/* virtual */ void
ES_EngineDebugBackend::LeaveContext(ES_Runtime *runtime, ES_Context *context)
{
	if (current_dbg_thread)
	{
		OP_ASSERT(current_dbg_thread->dbg_runtime->runtime == runtime);
		OP_ASSERT(current_dbg_thread->context == context);
		current_dbg_thread = NULL;
	}
}

/* virtual */ void
ES_EngineDebugBackend::DestroyContext(ES_Runtime *runtime, ES_Context *context)
{
	ES_DebugRuntime *dbg_runtime;

	if (OpStatus::IsSuccess(GetDebugRuntime(dbg_runtime, runtime, FALSE)) && dbg_runtime)
		for (ES_DebugThread *dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();
		     dbg_thread;
		     dbg_thread = (ES_DebugThread *) dbg_thread->Suc())
			if (dbg_thread->context == context)
			{
				dbg_thread->Out();
				OP_DELETE(dbg_thread);
				return;
			}
}

/* virtual */ void
ES_EngineDebugBackend::DestroyRuntime(ES_Runtime *runtime)
{
	ES_DebugRuntime *dbg_runtime;

	if (OpStatus::IsSuccess(GetDebugRuntime(dbg_runtime, runtime, FALSE)) && dbg_runtime)
	{
		Detach(dbg_runtime);

		dbg_runtime->Out();
		frontend->RuntimeStopped(dbg_runtime->id, 0);
		OP_DELETE(dbg_runtime);
	}
}

/* virtual */ void
ES_EngineDebugBackend::DestroyObject(ES_Object * /*object*/)
{
	OP_ASSERT(!"No objects should be 'tracked by debugger'.");
}

/* virtual */ ES_DebugListener::EventAction
ES_EngineDebugBackend::HandleEvent(EventType type, unsigned int script_guid, unsigned int line_no)
{
	if (current_dbg_thread)
	{
		ES_Runtime *current_runtime = current_dbg_thread->dbg_runtime->runtime;
		ES_DebugPosition position(script_guid, line_no);

		if (current_dbg_thread->delete_return_values)
		{
			OP_DELETE(current_dbg_thread->dbg_return_value);
			current_dbg_thread->dbg_return_value = NULL;
			current_dbg_thread->delete_return_values = FALSE;
		}

		if (type == ESEV_NEWSCRIPT)
		{
			if (OpStatus::IsSuccess(current_dbg_thread->dbg_runtime->m_scripts_to_stop.Remove(script_guid)))
				return Stop(STOP_REASON_NEW_SCRIPT, position);
		}
		else if (type == ESEV_STATEMENT)
		{
			if (current_dbg_thread->mode == ES_DebugFrontend::STEPINTO || current_dbg_thread->mode == ES_DebugFrontend::STEPOVER && current_dbg_thread->current_depth <= current_dbg_thread->target_depth)
				return Stop(STOP_REASON_STEP, position);

			for (ES_DebugBreakpoint *dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoints.First();
			     dbg_breakpoint;
			     dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoint->Suc())
				if (dbg_breakpoint->type == ES_DebugBreakpoint::TYPE_POSITION && dbg_breakpoint->data.position.scriptid == script_guid && dbg_breakpoint->data.position.linenr == line_no)
					return StopBreakpoint(position, dbg_breakpoint->id);

			// Check if we have stop at script enabled, and only stop once for each script.
			if (break_dbg_runtime_id == current_dbg_thread->dbg_runtime->id && break_dbg_thread_id == current_dbg_thread->id)
			{
				break_dbg_runtime_id = break_dbg_thread_id = 0;
				return Stop(STOP_REASON_BREAK, position);
			}
			else if (current_dbg_thread->mode == ES_DebugFrontend::RUN || current_dbg_thread->mode == ES_DebugFrontend::FINISH)
				return ESACT_CONTINUE;
		}
		else if (type == ESEV_CALLSTARTING || type == ESEV_ENTERFN)
		{
			ES_Object *function;
			if (type == ESEV_CALLSTARTING)
			{
				current_dbg_thread->last_return_position.linenr = 0;
				function = g_ecmaManager->GetCallee(current_dbg_thread->context);

				if (function)
				{
					ES_DebugObjectElement attr;
					g_ecmaManager->GetObjectAttributes(current_dbg_thread->dbg_runtime->runtime, function, &attr);

					if (attr.type == OBJTYPE_NATIVE_FUNCTION)
						/* Will be handled by the following ESEV_ENTERFN instead. */
						function = NULL;
					else if (current_dbg_thread->mode == ES_DebugFrontend::STEPINTO)
						current_dbg_thread->target_depth = current_dbg_thread->current_depth;
				}

				ES_Object *callee;		// current calling object
				ES_Object *thisobject;	// current this object
				ES_Value *func_argv;	// arguments
				int func_argc;			// number of arguments

				g_ecmaManager->GetCallDetails(current_dbg_thread->context, &thisobject, &callee, &func_argc, &func_argv);
				if (thisobject)
				{
					ES_DebugObjectElement attr;
					g_ecmaManager->GetObjectAttributes(current_dbg_thread->dbg_runtime->runtime, thisobject, &attr);

					if (m_function_filter.GetCount() > 0 && m_function_filter.Contains(attr.classname))
					{
						if (OpStatus::IsMemoryError(frontend->FunctionCallStarting(current_dbg_thread->dbg_runtime->id, current_dbg_thread->id, func_argv, func_argc, thisobject, callee, position)))
						{
							g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
							return ESACT_CONTINUE;
						}
					}
				}
			}
			else
				function = GetCurrentFunction();

			unsigned breakpoint_id = 0;
			if (function)
			{
				for (ES_DebugBreakpoint *dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoints.First();
				     dbg_breakpoint;
				     dbg_breakpoint = (ES_DebugBreakpoint *) dbg_breakpoint->Suc())
				{
					if (dbg_breakpoint->type == ES_DebugBreakpoint::TYPE_FUNCTION && dbg_breakpoint->function.GetObject() == function)
					{
						breakpoint_id = dbg_breakpoint->id;
						break;
					}
					else if (type == ESEV_ENTERFN && dbg_breakpoint->type == ES_DebugBreakpoint::TYPE_EVENT)
					{
						// Ensure event breakpoint is only triggered once for the same thread
						if (!current_dbg_thread->allow_event_break)
							continue;

						ES_Thread *interrupted_thread = current_dbg_thread->thread->GetInterruptedThread();

						if (interrupted_thread && interrupted_thread->Type() == ES_THREAD_EVENT)
						{
							BOOL event_eq = current_dbg_thread->IsEventThread(dbg_breakpoint->data.event.event_type, dbg_breakpoint->data.event.namespace_uri);
							BOOL window_eq = FALSE;
							if (dbg_breakpoint->window_id == 0)
								window_eq = TRUE;
							else
							{
								OP_ASSERT(current_dbg_thread->dbg_runtime && current_dbg_thread->dbg_runtime->runtime);
								if (current_dbg_thread->dbg_runtime && current_dbg_thread->dbg_runtime->runtime)
									window_eq = GetWindowId(current_dbg_thread->dbg_runtime->runtime) == dbg_breakpoint->window_id;
							}

							if (event_eq && window_eq)
							{
								breakpoint_id = dbg_breakpoint->id;
								break;
							}
						}
					}
				}

				if (current_dbg_thread->allow_event_break && current_dbg_thread->thread->GetInterruptedThread() && current_dbg_thread->thread->GetInterruptedThread()->Type() == ES_THREAD_EVENT)
					current_dbg_thread->allow_event_break = FALSE;
			}

			BOOL stop = breakpoint_id != 0;

			if (type == ESEV_CALLSTARTING)
				++current_dbg_thread->current_depth;

			if (stop)
				return StopBreakpoint(position, breakpoint_id);
		}
		else if (type == ESEV_RETURN)
			current_dbg_thread->last_return_position = position;
		else if (type == ESEV_LEAVEFN)
		{
			// Invalid position can happen when stepping in an attribute
			// handler where we wrap attribute's handler code inside
			// a function that is not visible on the outside. Don't stop.
			if (!position.Valid())
				return ESACT_CONTINUE;

			// This allows us enter empty functions when single stepping.
			if (current_dbg_thread->mode == ES_DebugFrontend::STEPINTO
				|| current_dbg_thread->mode == ES_DebugFrontend::STEPOVER && current_dbg_thread->current_depth == current_dbg_thread->target_depth)
			{
				return Stop(STOP_REASON_STEP, position);
			}
		}
		else if (type == ESEV_CALLCOMPLETED)
		{
			--current_dbg_thread->current_depth;

			if ((current_dbg_thread->mode == ES_DebugFrontend::STEPINTO || current_dbg_thread->mode == ES_DebugFrontend::STEPOVER)
				&& current_dbg_thread->target_depth > current_dbg_thread->current_depth)
				current_dbg_thread->target_depth = current_dbg_thread->current_depth;

			if (current_dbg_thread->target_depth == current_dbg_thread->current_depth
				&& (current_dbg_thread->mode == ES_DebugFrontend::STEPOVER || current_dbg_thread->mode == ES_DebugFrontend::STEPINTO || current_dbg_thread->mode == ES_DebugFrontend::FINISH))
			{
				if (ES_DebugReturnValue *dbg_return_value = OP_NEW(ES_DebugReturnValue, ()))
				{
					ES_Object *function = g_ecmaManager->GetCallee(current_dbg_thread->context);
					ExportObject(current_runtime, function, dbg_return_value->function);

					ES_Value internal_value;
					g_ecmaManager->GetReturnValue(current_dbg_thread->context, &internal_value);
					ExportValue(current_runtime, internal_value, dbg_return_value->value);

					dbg_return_value->from = current_dbg_thread->last_return_position.Valid() ? current_dbg_thread->last_return_position : position;
					dbg_return_value->to = position;

					dbg_return_value->next = current_dbg_thread->dbg_return_value;
					current_dbg_thread->dbg_return_value = dbg_return_value;
				}
			}

			BOOL stop = FALSE;

			if (current_dbg_thread->mode == ES_DebugFrontend::FINISH && current_dbg_thread->current_depth == current_dbg_thread->target_depth)
			{
				stop = TRUE;
				current_dbg_thread->mode = ES_DebugFrontend::STEPOVER;
			}
			else if (current_dbg_thread->mode == ES_DebugFrontend::STEPOVER && current_dbg_thread->current_depth < current_dbg_thread->target_depth)
				stop = TRUE;

			ES_Value returnvalue;
			g_ecmaManager->GetReturnValue(current_dbg_thread->context, &returnvalue);

			if(!ReportFunctionCallComplete(returnvalue, FALSE))
				return ESACT_CONTINUE;

			if (stop)
				return Stop(STOP_REASON_STEP, position);
		}
		else if (type == ESEV_EXCEPTION && stop_at_exception
				 || type == ESEV_ERROR && stop_at_error
				 || type == ESEV_HANDLED_ERROR && stop_at_error && stop_at_handled_error)
		{
			ES_Value exception;
			g_ecmaManager->GetExceptionValue(current_dbg_thread->context, &exception);

			ES_DebugValue dbg_exception;
			ExportValue(current_runtime, exception, dbg_exception);

			ES_DebugStopReason reason = (type == ESEV_EXCEPTION) ? STOP_REASON_EXCEPTION : (type == ESEV_ERROR) ? STOP_REASON_ERROR : STOP_REASON_HANDLED_ERROR;

			return StopException(reason, position, dbg_exception);
		}
		else if (type == ESEV_ABORT && stop_at_abort)
			return Stop(STOP_REASON_ABORT, position);
		else if (type == ESEV_BREAKHERE)
			return Stop(STOP_REASON_DEBUGGER_STATEMENT, position);
	}

	return ESACT_CONTINUE;
}

BOOL
ES_EngineDebugBackend::ReportFunctionCallComplete(ES_Value& returnvalue, BOOL isexception)
{
	ES_Object *callee;		// current calling object
	ES_Object* thisobject;	// the current 'this' object
	ES_Value* func_argv;	// arguments being passed to 'function'
	int func_argc;			// number of arguments being passed to 'function'

	g_ecmaManager->GetCallDetails(current_dbg_thread->context, &thisobject, &callee, &func_argc, &func_argv);
	if(thisobject)
	{
		ES_DebugObjectElement attr;
		g_ecmaManager->GetObjectAttributes(current_dbg_thread->dbg_runtime->runtime, thisobject, &attr);
		if (m_function_filter.GetCount() > 0 && m_function_filter.Contains(attr.classname))
		{
			if(OpStatus::IsMemoryError(frontend->FunctionCallCompleted(current_dbg_thread->dbg_runtime->id, current_dbg_thread->id, returnvalue, isexception)))
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return FALSE;
			}
		}
	}
	return TRUE;
}

/* virtual */ void
ES_EngineDebugBackend::RuntimeCreated(ES_Runtime *runtime)
{
	ES_DebugRuntime* drt;
	GetDebugRuntime(drt, runtime, TRUE);
}

ES_DebugListener::EventAction
ES_EngineDebugBackend::Stop(const ES_DebugStoppedAtData &data)
{
	/* Queue return values deletion upon stopping. Should not hold onto them
	   for more than just one stop following their return. */
	current_dbg_thread->delete_return_values = TRUE;

	// Don't stop on invalid positions, unless the stop reason is
	// STOP_REASON_NEW_SCRIPT.
	if (!data.position.Valid() && data.reason != STOP_REASON_NEW_SCRIPT)
		return ESACT_CONTINUE;

	current_dbg_thread->last_position = data.position;

	ES_DebugRuntime *dbg_runtime = current_dbg_thread->dbg_runtime;

	OP_STATUS status = frontend->StoppedAt(dbg_runtime->id, current_dbg_thread->id, data);

	if (OpStatus::IsSuccess(status))
	{
		current_dbg_thread->thread->Block(ES_BLOCK_DEBUGGER);
		current_dbg_thread->dbg_runtime->blocked_dbg_thread = current_dbg_thread;
		return ESACT_SUSPEND;
	}

	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

	return ESACT_CONTINUE;
}

ES_DebugListener::EventAction
ES_EngineDebugBackend::Stop(ES_DebugStopReason reason, const ES_DebugPosition &position)
{
	ES_DebugStoppedAtData data;
	data.reason = reason;
	data.position = position;

	return Stop(data);
}

ES_DebugListener::EventAction
ES_EngineDebugBackend::StopBreakpoint(const ES_DebugPosition &position, unsigned breakpoint_id)
{
	ES_DebugStoppedAtData data;
	data.reason = STOP_REASON_BREAKPOINT;
	data.position = position;
	data.u.breakpoint_id = breakpoint_id;

	return Stop(data);
}

ES_DebugListener::EventAction
ES_EngineDebugBackend::StopException(ES_DebugStopReason reason, const ES_DebugPosition &position, ES_DebugValue &exception)
{
	ES_DebugStoppedAtData data;
	data.reason = reason;
	data.position = position;
	data.u.exception = &exception;

	return Stop(data);
}

OP_STATUS
ES_EngineDebugBackend::GetDebugRuntime(ES_DebugRuntime *&dbg_runtime, ES_Runtime *runtime, BOOL create)
{
	for (dbg_runtime = (ES_DebugRuntime *) dbg_runtimes.First();
	     dbg_runtime;
	     dbg_runtime = (ES_DebugRuntime *) dbg_runtime->Suc())
		if (dbg_runtime->runtime == runtime)
			return OpStatus::OK;

	if (create && AcceptRuntime(runtime))
	{
		RETURN_IF_ERROR(common_runtime->MergeHeapWith(runtime));
		if (!(dbg_runtime = OP_NEW(ES_DebugRuntime, (runtime, TRUE, next_dbg_runtime_id++))))
			return OpStatus::ERR_NO_MEMORY;
		AddRuntime(dbg_runtime);
	}
	else
		dbg_runtime = 0;

	return OpStatus::OK;
}

ES_DebugRuntime *
ES_EngineDebugBackend::GetDebugRuntimeById(unsigned id)
{
	for (ES_DebugRuntime *dbg_runtime = (ES_DebugRuntime *) dbg_runtimes.First();
	     dbg_runtime;
	     dbg_runtime = (ES_DebugRuntime *) dbg_runtime->Suc())
		if (dbg_runtime->id == id)
			return dbg_runtime;

	return NULL;
}

ES_DebugThread *
ES_EngineDebugBackend::GetDebugThreadById(ES_DebugRuntime *dbg_runtime, unsigned id)
{
	if (id == 0)
		return NULL;

	for (ES_DebugThread *dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();
		 dbg_thread;
		 dbg_thread = (ES_DebugThread *) dbg_thread->Suc())
		if (dbg_thread->id == id)
			return dbg_thread;

	return NULL;
}

OP_STATUS
ES_EngineDebugBackend::GetDebugThread(ES_DebugThread *&dbg_thread, ES_Thread *thread, BOOL create)
{
	ES_DebugRuntime *dbg_runtime;

	RETURN_IF_ERROR(GetDebugRuntime(dbg_runtime, thread->GetScheduler()->GetRuntime(), create));

	if (dbg_runtime)
	{
		for (dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();
		     dbg_thread;
		     dbg_thread = (ES_DebugThread *) dbg_thread->Suc())
		{
			if (dbg_thread->thread == thread)
				return OpStatus::OK;
			if (dbg_thread->thread == 0 && dbg_thread->context == thread->GetContext())
			{
				// Debug thread was created in NewContext and should be used now
				dbg_thread->thread = thread;
				if (dbg_thread->started == FALSE)
				{
					dbg_thread->started = TRUE;
					return SignalNewThread(dbg_thread);
				}
				return OpStatus::OK;
			}
		}

		if (create)
		{
			if (!(dbg_thread = OP_NEW(ES_DebugThread, (dbg_runtime, NULL, next_dbg_thread_id++))))
				return OpStatus::ERR_NO_MEMORY;

			dbg_thread->Into(&dbg_runtime->dbg_threads);
			dbg_thread->thread = thread;
			dbg_thread->started = TRUE;

			return SignalNewThread(dbg_thread);
		}
	}
	else
		dbg_thread = NULL;

	return OpStatus::OK;
}

OP_STATUS
ES_EngineDebugBackend::SignalNewThread(ES_DebugThread *dbg_thread)
{
	ES_Thread *thread = dbg_thread->thread;

	if (ES_Thread *interrupted_thread = thread->GetInterruptedThread())
	{
		ES_DebugThread *dbg_parent_thread = NULL;
		// Some interrupted threads are not real user/author threads and most be ignored
		// and instead we try the next interrupted thread in the parent chain until we
		// have a proper thread, or we end up with no parent thread and use 0 as parent id.
		// Basically we try to avoid internal threads without debugging info, already started
		// threads and onload events.
		//
		// Some known internal threads are:
		// * MethodCall, GetSlot, SetSlot, RemoveSlot, HasSlot, they all use code looking like "x[y]"
		// * NS4Plugin uses eval JS code to perform certain operations
		for (;
			interrupted_thread;
			interrupted_thread = interrupted_thread->GetInterruptedThread())
		{
			// Do not use thread if there is no debug info
			if (!interrupted_thread->HasDebugInfo())
				continue;
			// Do not use thread if it is not running or is an event thread.
			if (!interrupted_thread->IsStarted() || interrupted_thread->IsCompleted())
				continue;

			RETURN_IF_ERROR(GetDebugThread(dbg_parent_thread, interrupted_thread, FALSE));
			if (dbg_parent_thread)
				break;
			// Uncomment next assert to catch problems with parent dbg_threads
			//OP_ASSERT(!"Debug thread for interrupted_thread not found, creating it");
			RETURN_IF_ERROR(GetDebugThread(dbg_parent_thread, interrupted_thread, TRUE));
			if (dbg_parent_thread)
				break;
		}
		if (dbg_parent_thread)
		{
			dbg_parent_thread->last_position = ES_DebugPosition();
			dbg_thread->parent_id = dbg_parent_thread->id;
		}
	}

	ES_DebugFrontend::ThreadType type;
	switch (thread->Type())
	{
	case ES_THREAD_TIMEOUT:
		type = ES_DebugFrontend::THREAD_TYPE_TIMEOUT;
		break;

	case ES_THREAD_INLINE_SCRIPT:
		type = ES_DebugFrontend::THREAD_TYPE_INLINE;
		break;

	case ES_THREAD_JAVASCRIPT_URL:
		type = ES_DebugFrontend::THREAD_TYPE_URL;
		break;

	case ES_THREAD_DEBUGGER_EVAL:
		type = ES_DebugFrontend::THREAD_TYPE_DEBUGGER_EVAL;
		break;

	case ES_THREAD_COMMON:
		if (ES_Thread *interrupted_thread = thread->GetInterruptedThread())
			if (interrupted_thread->Type() == ES_THREAD_EVENT)
			{
				thread = interrupted_thread;
				type = ES_DebugFrontend::THREAD_TYPE_EVENT;
				break;
			}
	default:
		type = ES_DebugFrontend::THREAD_TYPE_OTHER;
	}

	ES_DebugRuntimeInformation info, *infop = NULL;
	TempBuffer buffer;

	if (!dbg_thread->dbg_runtime->information_sent)
	{
		if (OpStatus::IsSuccess(GetRuntimeInformation(dbg_thread->dbg_runtime, info, buffer)))
		{
			infop = &info;
			dbg_thread->dbg_runtime->information_sent = TRUE;
		}
	}

	OpString event_name16;

	if (type == ES_DebugFrontend::THREAD_TYPE_EVENT)
		RETURN_IF_ERROR(dbg_thread->GetEventStr(event_name16));

	RETURN_IF_ERROR(frontend->ThreadStarted(dbg_thread->dbg_runtime->id, dbg_thread->id, dbg_thread->parent_id, type, dbg_thread->GetNamespace(), event_name16.CStr(), infop));

	if ((dbg_thread->listener = OP_NEW(ES_DebugThreadListener, (this, dbg_thread))) != NULL)
	{
		if (thread->IsSignalled()) // If the thread has already fired the signal we need to do this ourselves
		{
			ES_DebugThreadListener *listener = dbg_thread->listener;
			listener->Signal(thread, thread->IsFailed() ? ES_SIGNAL_FAILED : ES_SIGNAL_FINISHED);
			OP_DELETE(listener);
			dbg_thread->listener = NULL;
		}
		else
			dbg_thread->thread->AddListener(dbg_thread->listener);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

ES_Object *
ES_EngineDebugBackend::GetCurrentFunction()
{
	if (current_dbg_thread)
	{
		unsigned length;
		ES_DebugStackElement *stack;

		g_ecmaManager->GetStack(current_dbg_thread->context, 1, &length, &stack);

		if (length >= 1)
			return stack[0].function;
	}
	return NULL;
}

static OP_STATUS
ES_DebugAppendFramePath(TempBuffer &buffer, FramesDocument *doc)
{
	if (FramesDocument *parent_doc = doc->GetParentDoc())
	{
		RETURN_IF_ERROR(ES_DebugAppendFramePath(buffer, parent_doc));

		if (FramesDocElm *fde = doc->GetDocManager()->GetFrame())
		{
			const uni_char *name = fde->GetName();
			unsigned name_length = name ? uni_strlen(name) : 0;

			// Enough unless unsigned is more than 32 bits and the frame has at least 10 billion siblings.
			RETURN_IF_ERROR(buffer.Expand(15 + name_length));

			OpStatus::Ignore(buffer.Append("/"));

			if (name)
				OpStatus::Ignore(buffer.Append(name));

			unsigned index = 1;
			while ((fde = fde->Pred()) != NULL)
				++index;

			OpStatus::Ignore(buffer.Append("["));
			OpStatus::Ignore(buffer.AppendUnsignedLong(index));
			OpStatus::Ignore(buffer.Append("]"));
		}

		return OpStatus::OK;
	}
	else
		return buffer.Append("_top");
}

OP_STATUS
ES_EngineDebugBackend::GetRuntimeInformation(ES_DebugRuntime *dbg_runtime, ES_DebugRuntimeInformation &info, TempBuffer &buffer)
{
	OP_NEW_DBG("ES_EngineDebugBackend::GetRuntimeInformation", "esdb");

	ES_Runtime *runtime = dbg_runtime->runtime;

	info.dbg_runtime_id = dbg_runtime->id;

	OP_DBG(("info.dbg_runtime_id=%d", info.dbg_runtime_id));

	RETURN_IF_ERROR(runtime->GetWindows(info.windows));

	if (FramesDocument *doc = runtime->GetFramesDocument())
	{
		RETURN_IF_ERROR(ES_DebugAppendFramePath(buffer, doc));
		info.framepath = uni_strdup(buffer.GetStorage());
	}
	else
		info.framepath = uni_strdup(UNI_L(""));

	OpString uri;
	RETURN_IF_ERROR(runtime->GetURLDisplayName(uri));
	info.documenturi = uni_strdup(uri.CStr());

#ifdef EXTENSION_SUPPORT
	OpString extension_name;
	if (OpStatus::IsSuccess(runtime->GetExtensionName(extension_name)) && extension_name.HasContent())
		info.extension_name = uni_strdup(extension_name.CStr());
#endif // EXTENSION_SUPPORT

	info.dbg_globalobject_id = GetObjectId(runtime, runtime->GetGlobalObjectAsPlainObject());
	info.description = runtime->GetDescription();

	return OpStatus::OK;
}

OP_STATUS
ES_EngineDebugBackend::GetWindowId(Window *window, unsigned &id)
{
	ES_DebugWindow *dbg_window;

	for (dbg_window = (ES_DebugWindow *) dbg_windows.First();
	     dbg_window;
	     dbg_window = (ES_DebugWindow *) dbg_window->Suc())
		if (dbg_window->window == window)
		{
			id = dbg_window->Id();
			return OpStatus::OK;
		}

	if (!(dbg_window = OP_NEW(ES_DebugWindow, ())))
		return OpStatus::ERR_NO_MEMORY;

	dbg_window->window = window;
	dbg_window->Into(&dbg_windows);

	id = dbg_window->Id();

	return OpStatus::OK;
}

OP_STATUS
ES_EngineDebugBackend::GetScope(ES_DebugRuntime *dbg_runtime, unsigned dbg_thread_id, unsigned frame_index, unsigned * length, ES_Object ***scope)
{
	ES_DebugThread *dbg_thread;

	if (dbg_thread_id != 0)
	{
		for (dbg_thread = (ES_DebugThread *) dbg_runtime->dbg_threads.First();
			 dbg_thread && dbg_thread->id != dbg_thread_id;
			 dbg_thread = (ES_DebugThread *) dbg_thread->Suc()) {}

		if (!dbg_thread)
			return OpStatus::ERR;
	}
	else
		return OpStatus::ERR;

	if (frame_index == 0)
		frame_index = ES_DebugControl::OUTERMOST_FRAME;
	else
	{
		unsigned length;
		ES_DebugStackElement *stack;

		g_ecmaManager->GetStack(dbg_thread->context, ES_DebugControl::ALL_FRAMES, &length, &stack);

		frame_index = length - frame_index - 1;
	}

	g_ecmaManager->GetScope(dbg_thread->context, frame_index, length, scope);

	return OpStatus::OK;
}

OP_STATUS
ES_EngineDebugBackend::ExamineChain(ES_Runtime *rt, ES_Context *context, ES_Object *root, BOOL skip_non_enum, BOOL examine_prototypes, ES_DebugObjectChain &chain, ES_DebugPropertyFilters *filters, ES_EngineDebugBackendGetPropertyListener *listener, unsigned int async_tag)
{
	ExportObject(rt, root, chain.prop.object);

	ES_PropertyFilter *filter = NULL;

	if (filters && chain.prop.object.info)
		filter = filters->GetPropertyFilter(chain.prop.object.info->class_name);

	RETURN_IF_ERROR(ExamineObject(rt, context, root, skip_non_enum, FALSE, chain.prop, filter, listener, async_tag));

	if (examine_prototypes)
	{
		ES_DebugObjectElement attr;
		g_ecmaManager->GetObjectAttributes(rt, root, &attr);

		if (attr.prototype)
		{
			chain.prototype = OP_NEW(ES_DebugObjectChain, ());
			RETURN_OOM_IF_NULL(chain.prototype);

			OP_STATUS status = ExamineChain(rt, context, attr.prototype, skip_non_enum, examine_prototypes, *chain.prototype, filters, listener, async_tag);

			if (OpStatus::IsError(status))
			{
				OP_DELETE(chain.prototype);
				chain.prototype = NULL;
				return status;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS
ES_EngineDebugBackend::ExamineObject(ES_Runtime *rt, ES_Context *context, ES_Object *object, BOOL skip_non_enum, BOOL chain_info, ES_DebugObjectProperties &properties, ES_PropertyFilter *filter, ES_EngineDebugBackendGetPropertyListener* listener, unsigned int async_tag)
{
	uni_char **names;
	ES_Value *values;
	GetNativeStatus *getstatuses;

	g_ecmaManager->GetObjectProperties(context, object, filter, skip_non_enum, &names, &values, &getstatuses);

	if (!names || !values || !getstatuses)
		return OpStatus::ERR_NO_MEMORY;

	uni_char **name = names;
	unsigned count = 0;

	while (*name)
	{
		++count;
		++name;
	}

	name = names;
	GetNativeStatus *getstatus = getstatuses;
	ES_Value *value = values;

	OP_STATUS status = listener->AddObjectPropertyListener(object, count, &properties, chain_info);
	for (unsigned index = 0; index < count; index++)
	{
		if(OpStatus::IsSuccess(status))
		{
			OP_STATUS setpropstatus = OpStatus::OK;
			if (*getstatus == GETNATIVE_SUCCESS)
			{
				BOOL exclude = (filter && filter->Exclude(*name, *value));
				setpropstatus = listener->OnPropertyValue(&properties, *name, *value, index, exclude, *getstatus);
			}
			else if (*getstatus == GETNATIVE_NEEDS_CALLSTACK || *getstatus == GETNATIVE_SCRIPT_GETTER)
				setpropstatus = listener->OnPropertyValue(&properties, *name, *value, index, FALSE, *getstatus);
			else
			{
				ES_AsyncInterface *asyncif = rt->GetESAsyncInterface();
				if (!asyncif)
					setpropstatus = OpStatus::ERR_NO_MEMORY;
				else
				{
					uni_char *aname = *name;
					ES_GetSlotHandler *getslothandler;
					OP_STATUS getslothandlerstat = ES_GetSlotHandler::Make(getslothandler, object, listener, filter, index, aname, &properties);
					if (OpStatus::IsError(getslothandlerstat) || OpStatus::IsError(asyncif->GetSlot(object, aname, getslothandler, listener->GetBlockedThread())))
					{
						OP_DELETE(getslothandler);
						setpropstatus = OpStatus::ERR_NO_MEMORY;
					}
				}
			}
			if (OpStatus::IsError(setpropstatus))
				listener->OnError();
		}
		op_free(*name);

		value++;
		name++;
		getstatus++;
	}

	OP_DELETEA(names);
	OP_DELETEA(values);
	OP_DELETEA(getstatuses);
	return status;
}

ES_AsyncInterface *
ES_EngineDebugBackend::GetAsyncInterface(ES_DebugRuntime *dbg_runtime)
{
	return (dbg_runtime && dbg_runtime->runtime) ? dbg_runtime->runtime->GetESAsyncInterface() : NULL;
}

ES_Thread *
ES_EngineDebugBackend::GetCurrentThread(ES_DebugRuntime *dbg_runtime)
{
	if (dbg_runtime && dbg_runtime->runtime && dbg_runtime->runtime->GetESScheduler())
		return dbg_runtime->runtime->GetESScheduler()->GetCurrentThread();
	else
		return NULL;
}

void
ES_EngineDebugBackend::DetachEvalCallback(ES_DebugEvalCallback *callback)
{
	for (ES_EvalCallbackLink *cl = static_cast<ES_EvalCallbackLink *>(eval_callbacks.First()); cl; cl = static_cast<ES_EvalCallbackLink *>(cl->Suc()))
		if (cl->callback == callback)
		{
			cl->Out();
			OP_DELETE(cl);
			return;
		}
}

void
ES_EngineDebugBackend::CancelEvalThreads(ES_Runtime *runtime)
{
	ES_EvalCallbackLink *cl = static_cast<ES_EvalCallbackLink *>(eval_callbacks.First());

	while (cl)
	{
		ES_DebugEvalCallback *callback = cl->callback;
		ES_EvalCallbackLink *next = static_cast<ES_EvalCallbackLink *>(cl->Suc());

		if (runtime == NULL || callback->GetRuntime() == runtime)
		{
			callback->DetachBackend(this);

			// Cancel the running thread. This will cause thread to be signalled
			// with ES_SIGNAL_CANCELLED, and the ES_DebugEvalCallback will delete itself.
			if (ES_Thread *thread = callback->GetRunningThread())
				thread->GetScheduler()->CancelThread(thread);

			cl->Out();
			OP_DELETE(cl);
		}

		cl = next;
	}
}

OP_STATUS
ES_EngineDebugBackend::AddFunctionClassToFilter(const char* classname)
{
	const char *name_container = SetNewStr(classname);
	RETURN_OOM_IF_NULL(name_container);

	OP_STATUS status = m_function_filter.Add(name_container, (void*)name_container);

	if (status == OpStatus::ERR)	// allready in the hash
	{
		OP_DELETEA(name_container);
		status = OpStatus::OK;	// no worries
	}
	return status;
}

void
ES_EngineDebugBackend::SetReformatFlag(BOOL enable)
{
	g_ecmaManager->SetWantReformatScript(enable);
}

OP_STATUS
ES_EngineDebugBackend::SetReformatConditionFunction(const uni_char *source)
{
	return g_ecmaManager->SetReformatConditionFunction(source);
}

ES_EngineDebugBackendGetPropertyListener::ES_EngineDebugBackendGetPropertyListener(ES_EngineDebugBackend *backend, ES_DebugFrontend *frontend, unsigned runtimeid, unsigned asynctag, ES_Thread *blocked_thread)
	: m_remaining_objects(0),
	  m_tag(asynctag),
	  m_runtime_id(runtimeid),
	  m_objects(NULL),
	  m_proto_object_properties(NULL),
	  m_frontend(frontend),
	  m_export_chain_info(FALSE),
	  m_sync_mode(FALSE),
	  m_blocked_thread(blocked_thread),
	  m_unreported_objects(0),
	  m_backend(backend)
{ }

ES_EngineDebugBackendGetPropertyListener::~ES_EngineDebugBackendGetPropertyListener()
{
	for (unsigned i = 0; i < m_getslotshandlers.GetCount(); i++)
	{
		ES_GetSlotHandler* handler = m_getslotshandlers.Get(i);
		handler->OnListenerDied();
	}

	ES_ObjectPropertiesListener* listener = static_cast<ES_ObjectPropertiesListener *>(m_object_properties.First());
	while (listener)
	{
		ES_ObjectPropertiesListener* currlistener = listener;
		listener = listener->Suc();
		OP_DELETE(currlistener);
	}

	OP_DELETEA(m_objects);
}

OP_STATUS
ES_EngineDebugBackendGetPropertyListener::ES_ObjectPropertiesListener::OnPropertyValue(const ES_Value &getslotresult, unsigned property_index, const uni_char* propertyname, BOOL chain_info, BOOL exclude, GetNativeStatus getstatus)
{
	unsigned runtime_id = m_owner_listener->m_runtime_id;

	OP_ASSERT(g_ecmaManager->GetDebugListener());	// no debugger attached?

	ES_Runtime* runtime = m_owner_listener->m_backend->GetRuntimeById(runtime_id);
	if (!runtime)
		return OpStatus::ERR;

	// make sure the object isn't GCed , see bugs CORE-15451 & CORE-15694
	if (getslotresult.type == VALUE_OBJECT)
		m_owner_listener->m_backend->GetObjectId(runtime, getslotresult.value.object);

	property_index -= m_property_index_alignment;

	if (!exclude)
	{
		m_properties->properties[property_index].value_status = getstatus;
		if (getstatus == GETNATIVE_SUCCESS)
			m_owner_listener->m_backend->ExportValue(runtime, getslotresult, m_properties->properties[property_index].value, chain_info);

		m_properties->properties[property_index].name = uni_strdup(propertyname);

		m_properties->properties_count++;
	}
	else
	{
		//we need to align the property for filtered slots
		m_property_index_alignment++;
	}

	OP_ASSERT(m_remaining_properties);
	if (!(--m_remaining_properties))
		m_owner_listener->ObjectPropertiesDone();

	return OpStatus::OK;
}

void
ES_EngineDebugBackendGetPropertyListener::Done()
{
	m_frontend->ExamineObjectsReply(m_tag, m_root_object_count, m_objects);
	m_backend->RemoveObjectListener(this);
	OP_DELETE(this);
}

void
ES_EngineDebugBackendGetPropertyListener::ObjectPropertiesDone()
{
	if (m_sync_mode)
		m_unreported_objects++;
	else if (!--m_remaining_objects)
		Done();
}

void
ES_EngineDebugBackendGetPropertyListener::OnDebugObjectChainCreated(ES_DebugObjectChain *targetstruct, unsigned count)
{
	OP_ASSERT(!m_objects);	// this should only be called once
	m_objects = targetstruct;
	m_root_object_count = count;
}

OP_STATUS
ES_EngineDebugBackendGetPropertyListener::AddObjectPropertyListener(ES_Object *owner, unsigned property_count, ES_DebugObjectProperties *properties, BOOL chain_info)
{
	m_export_chain_info = chain_info;
	m_proto_object_properties = properties;

	m_proto_object_properties->properties_count = 0;	// none are filled in yet, incremented in OnPropertyValue when filled
	m_proto_object_properties->properties = OP_NEWA(ES_DebugObjectProperties::Property, (property_count));
	RETURN_OOM_IF_NULL(m_proto_object_properties->properties);

	ES_ObjectPropertiesListener *newlistener = OP_NEW(ES_ObjectPropertiesListener, (this, owner, m_proto_object_properties, property_count));
	if (!newlistener)
	{
		OP_DELETEA(m_proto_object_properties->properties);
		m_proto_object_properties->properties = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	newlistener->Into(&m_object_properties);
	m_remaining_objects++;

	if (!property_count)
		m_unreported_objects++;	// need to "manually" report these propertyless objects

	return OpStatus::OK;
}

#endif // ECMASCRIPT_DEBUGGER
