/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESTHREAD_H
#define ES_UTILS_ESTHREAD_H

#include "modules/util/simset.h"
#include "modules/url/url2.h"
#include "modules/doc/domevent.h"
#include "modules/hardcore/keys/opkeys.h"

class ES_ThreadScheduler;
class ES_ThreadSchedulerImpl;
class ES_ForeignThreadBlock;
class ES_SharedThreadInfo;
class ES_Program;
class ES_Context;
class ES_Value;
class ES_Object;
class ES_TimeoutTimerEvent;
class ES_TimerManager;
class FramesDocument;
class DocumentManager;

/**
 * Thread types, returned by ES_Thread::Type().
 */
enum ES_ThreadType
{
	ES_THREAD_COMMON,
	/**< Class: ES_Thread.  An ordinary thread, nothing exciting. */

	ES_THREAD_EMPTY,
	/**< Class: ES_EmptyThread.  An empty thread, used as a placeholder when
	     something has to be blocked and there's nothing else to block. */

	ES_THREAD_TIMEOUT,
	/**< Class: ES_TimeoutThread.  A timeout or interval thread, normally
	     created by window.setTimeout() and window.setInterval() in JavaScript. */

	ES_THREAD_EVENT,
	/**< Classes: ES_EventThread.  An event thread. */

	ES_THREAD_TERMINATING,
	/**< Class: ES_TerminatingThread.  The pseudo thread that performs actions
	     that will lead to the termination of the scheduler. */

	ES_THREAD_INLINE_SCRIPT,
	/**< Class: ES_InlineScriptThread.  Inline scripts executed during document
	     loading. */

	ES_THREAD_JAVASCRIPT_URL,
	/**< Class: ES_JavascriptURLThread.  A script executed when loading a URL
	     with the javascript-protocol. */

	ES_THREAD_HISTORY_NAVIGATION,
	/**< Class: ES_HistoryNavigaionThread.  A script executed when navigating in history
	     within the document (e.g. hash change). */

	ES_THREAD_DEBUGGER_EVAL
	/**< Class: ES_DebuggerEvalThread.  A script executed via eval() internally,
	     ie. through the ecmascriptdebugger. */

};

/**
 * Thread block types (i.e., reasons for a thread to be blocked):
 */
enum ES_ThreadBlockType
{
	ES_BLOCK_NONE,
	/**< Not blocked. */

	ES_BLOCK_UNSPECIFIED,
	/**< Not specified, set if the ES engine returns ES_BLOCKED (normally
	     because the DOM code returns ES_BLOCKED), if the thread wasn't
	     explicitly blocked already.  Also the default argument to Block
	     and Unblock operations. */

	ES_BLOCK_DOCUMENT_WRITE,
	/**< Waiting for data written to the document by a document.write from an
	     inline script to be processed.  The thread is unblocked in
	     LogicalDocument::Load when all data has been processed. */

	ES_BLOCK_DOCUMENT_REPLACED,
	/**< Used when a thread in one window calls document.open to replace
	     the document in another window, while the replaced document's
	     scheduler is being terminated. */

	ES_BLOCK_USER_INTERACTION,
	/**< Block type used when a thread calls alert(), confirm() or prompt().
	     Blocking and unblocking is done by the class ES_RestartObject. */

	ES_BLOCK_TERMINATING_CHILDREN,
	/**< Used to block a terminating thread while the schedulers in subdocuments
	     in a frames structure are being terminated. */

	ES_BLOCK_JAVA_CALL,
	/**< Used when a Java function is called from JavaScript through
	     LiveConnect. */

	ES_BLOCK_RESTARTING,
	/**< Not used at all yet, may replace ES_BLOCK_USER_INTERACTION in a more
	     general suspend/restart mechanism. */

	ES_BLOCK_FOREIGN_THREAD,
	/**< Blocked by another thread.  Used when a thread is interrupted by a
	     thread in another scheduler.  Blocking and unblocking is done by the
	     class ES_ForeignThreadBlock in core/essched.cpp. */

	ES_BLOCK_WAITING_FOR_URL,
	/**< Used to block the terminating thread between the loading of an URL is
	     started by an ES_OpenURLAction terminating action and the URL is
	     partly loaded (after a MSG_HEADER_LOADED is sent). */

	ES_BLOCK_INLINE_SCRIPT,
	/**< Used when a thread causes an inline script to be executed (for instance
	     by modifying the 'src' attribute of a SCRIPT element). */

	ES_BLOCK_EXTERNAL_SCRIPT,
	/**< Used when an external script is being loaded but parsing is not blocked
	     to make sure that other SCRIPT elements are not executed until the
	     external script has been loaded. */

	ES_BLOCK_DEBUGGER
};

/**
 * Signals "sent" to thread with the function ES_Thread::Signal.
 *
 * Signals are used by the scheduler to tell threads and thread listeners
 * about things that happend to the scheduler or to the thread.
 *
 * Exactly one of ES_SIGNAL_CANCELLED, ES_SIGNAL_FINISHED and ES_SIGNAL_FAILED
 * will always be sent to a thread right before the thread is removed from
 * the scheduler and deleted.
 *
 * ES_SIGNAL_SCHEDULER_TERMINATED is only informational, a thread can ignore
 * it if it wants to.  A thread could for instance decide to stop doing what
 * it is doing and pretend it has completed after receiving this signal.
 * When the scheduler is terminated normal execution will continue, so all
 * currently runnable threads will be allowed to run to completion.
 */
enum ES_ThreadSignal
{
	ES_SIGNAL_SCHEDULER_TERMINATED,
	/**< Used to notify all runnable threads that a terminating thread has been
		 added to the scheduler. */

	ES_SIGNAL_CANCELLED,
	/**< Used to notify a thread that it has been cancelled, i.e., that it will
		 not execute any more and will be removed from the scheduler and deleted
		 immediately after the call to Signal has finished. */

	ES_SIGNAL_FINISHED,
	/**< Used to notify a thread and its listeners that the thread has finished
		 executing and will be removed. */

	ES_SIGNAL_FAILED
	/**< Used to notify a thread and its listeners that the thread has finished
		 executing with an error condition (usually some kind of runtime script
		 error, or OOM) and will be removed. */
};

/* --- ES_ThreadListener --------------------------------------- */

class ES_Thread;

/**
 * A general listener interface for listening to the signals sent to a thread
 * by the scheduler.
 *
 * A listener is owned by the thread it is listening to, and will be deleted
 * together with the thread, unless it has been removed again, using the
 * Remove() function.  After a listener has been removed, it will receive no
 * more signals and will not be deleted by the thread.
 *
 * Deleting a listener will also remove it from the thread.
 *
 * A listener can safely be removed from the thread from its Signal
 * function.
 */
class ES_ThreadListener
	: public Link
{
public:
	friend class ES_Thread;

	virtual ~ES_ThreadListener();
	/**< Destructor.  Removes the listener from its thread, if it belongs to
	     a thread. */

	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal) = 0;
	/**< Called from ES_Thread::Signal() for every signal the thread receives.
	     @param thread The thread this listener belongs to.
	     @param signal The signal the thread received.
	     @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
		*/

	void Remove();
	/**< To be used by subclasses to remove this listener from its thread. */
};

/* --- ES_RestartObject ---------------------------------------------- */

class ES_RestartObject
{
public:
	void Push(ES_Thread *thread, ES_ThreadBlockType block_type = ES_BLOCK_NONE);
	/**< Push this restart object onto the supplied thread's stack of restart
	     objects.  If the optional 'block_type' argument is not ES_BLOCK_NONE,
	     then the thread is also blocked. */

	static ES_RestartObject *Pop(ES_Thread *thread, OperaModuleId module, int tag);
	/**< Pop a restart object from the supplied thread's stack of restart
	     objects if it matches the supplied module ID and per-module tag.
	     If there is no restart object on the thread's stack, or if the
	     restart object at the top of the stack doesn't match the supplied
	     module ID and per-module tag, NULL is returned.  This allows the
	     caller to perform an "am I being restarted, or someone else?"
	     check. */

	void Discard(ES_Thread *thread);
	/**< Mark this restart object as being deletable by the supplied thread
	     at its next safe point for releasing restart objects. A discarded
	     restart object is guaranteed to live beyond the thread's current
	     execution timeslice, and can be used to pass string results from
	     a restarted operation back to the underlying ES engine - using
	     the restart object to hold the result.

	     Calling Discard() transfers ownership of the restart object to the
	     thread. */

	virtual ~ES_RestartObject() {}

	virtual void GCTrace(ES_Runtime *runtime) {}
	/**< Called if the garbage collector runs while the thread is suspended.
	     The restart object should call ES_Runtime::GCMark() on all garbage
	     collected object it holds references to. */

	virtual BOOL ThreadCancelled(ES_Thread *thread) { return TRUE; }
	/**< Called if the suspended thread is cancelled and thus will never resume
	     execution.  If this function returns TRUE, the restart object is
	     deleted immediately following the call, otherwise the thread simply
	     forgets about the restart object.

	     @param thread The thread that was cancelled.  This is guaranteed to be
	                   the same one passed to Push().
	     @return TRUE if the restart object should be deleted by the thread. */

protected:
	ES_RestartObject(OperaModuleId module, int tag)
		: next(NULL),
		  module(module),
		  tag(tag)
	{
	}

private:
	friend class ES_Thread;

	ES_RestartObject *next;
	OperaModuleId module;
	int tag;
};

/* --- ES_ThreadInfo ------------------------------------------------- */

/**
 * A simple data container class that describes a thread briefly.
 *
 * Used to track the origin of a thread, that is, a thread that was started from
 * outside the ECMAScript environment (usually an inline script in a document or
 * an event handler triggered by the document code) that resulted in the
 * execution of the thread whose origin is sought.
 *
 * Currently stores the thread type and, for event threads, the event type and,
 * for key events or mouse events, the keyCode and button event data,
 * respectively.  More thread specific data can be added as needed.
 */
struct ES_ThreadInfo
{
	ES_ThreadInfo(ES_ThreadType type, const ES_SharedThreadInfo *shared = NULL);
	/**< Constructor, initializes ES_ThreadInfo from type and
	     the thread info of its (shared) origin.

	     @param type The thread's type.
	     @param type The optional shared thread info to initialize from. */

	ES_ThreadType type:16;
	/**< The thread's type. */

	unsigned is_user_requested:1;
	unsigned open_in_new_window:1;
	unsigned open_in_background:1;
	unsigned has_opened_new_window:1;
	unsigned has_opened_url:1; //< Only the first location change can be user_initiated so here we keep track of scripts changing locations.
	unsigned has_opened_file_upload_dialog:1; //< Only the first opened file upload dialog can be user_initiated so here we keep track of scripts opening file dialogs.

	union
	{
		struct
		{
			DOM_EventType type:16;
			/**< For event threads (type==ES_THREAD_EVENT), the event type. */

			ShiftKeyState modifiers;
			unsigned is_window_event:1;

			union
			{
				int button:16;  /**< MouseEvent::button */
				unsigned keycode:16; /**< KeyboardEvent::keycode */
			} data;
		} event;
	} data;
};

/* --- ES_SharedThreadInfo ------------------------------------------- */

/**
 * This ref counted class contain the data that is shared between
 * inline scripts.
 */
class ES_SharedThreadInfo
{
public:
	/**< Created with ref_count = 1. Call DecRef to delete it. */
	ES_SharedThreadInfo()
		: is_user_requested(FALSE),
		  open_in_new_window(FALSE),
		  open_in_background(FALSE),
		  has_opened_new_window(FALSE),
		  has_opened_url(FALSE),
		  has_opened_file_upload_dialog(FALSE),
		  has_reported_recursion_error(FALSE),
		  inserted_by_parser_count(0),
		  doc_written_bytes(0),
		  refcount(1)
	{
	}

	void IncRef() { ++refcount; }
	void DecRef()
	{
		if (--refcount == 0)
			OP_DELETE(this);
	}

	unsigned int is_user_requested:1;
	unsigned int open_in_new_window:1;
	unsigned int open_in_background:1;
	unsigned int has_opened_new_window:1;
	unsigned int has_opened_url:1;
	unsigned int has_opened_file_upload_dialog:1;

	unsigned int has_reported_recursion_error:1;
	/**< Prevents the console from being flooded with
		 script recursion errors */

	unsigned int inserted_by_parser_count:16;
	/**< In case this thread has been started by an inline
		 script added by the parser, this count holds the
		 number of threads which have originated from
		 that single script element, due to doc.write().
		 This allows to detect recursive inclusion of the
		 same script in a page, like CORE-23286 */

	unsigned int doc_written_bytes;
	/** < Amount of data written by doc.write() for the set
		  of inline threads that share this object */

private:
	unsigned refcount;
};

/* --- ES_Thread ----------------------------------------------------- */

class ES_Thread
	: public Link
{
public:
	ES_Thread(ES_Context *context, ES_SharedThreadInfo *shared = NULL);
	/**< Constructor.
		 @param context The thread's context.  MAY be NULL, but then this class'
		                EvaluateThread() MUST NOT be used.
		 @param shared  The shared thread info that this thread should be
		                initialized with. */

	virtual ~ES_Thread();
	/**< Destructor.  The thread must not be started or it must also be finished.
	     Deletes the thread's context if there is one. */

	virtual OP_STATUS EvaluateThread();
	/**< Evaluate the thread.  Most thread types will want to override this
	     function to do pre- or postprocessing.  This implementation first sets
	     up the execution of a program set by SetProgram or the call of a
	     callable object set by SetCallable, if IsStarted() returns FALSE, then
	     it evaluates the thread's context and then updates the thread's state
	     (started/completed/failed) according to the result.

	     Any normal failures to evaluate a thread should be signalled through
	     ES_Thread::is_failed and ES_Thread::IsFailed(), not through this
	     function's return value.  Anything else than OpStatus::OK will be
	     treated as some fatal error by the scheduler and will make it abort
	     all current activities and remove all threads.

	     @return OpStatus::ERR_NO_MEMORY on out of memory, OpStatus::ERR on
	             other fatal errors; otherwise OpStatus::OK. */

	virtual ES_ThreadType Type();
	/**< The thread's type.  This class' type is ES_THREAD_COMMON.
	     @return The thread's type. */

	virtual ES_ThreadInfo GetInfo();
	/**< Return information about the thread, including at least the thread type.
	     @return A thread info object. */

	virtual ES_ThreadInfo GetOriginInfo();
	/**< Return information about the origin of this thread.  This class' version
	     returns the result of ES_Thread::GetInfo() called on this thread or, if
	     this thread has interrupted another thread, ES_Thread::GetOriginInfo()
	     called on the interrupted thread.

	     The origin of a thread can be used to decide whether the thread is
	     allowed to perform specific actions, such as opening a window.

	     @return A thread info object. */

	virtual const uni_char *GetInfoString();
	/**< Return a string describing the thread (for error reporting, mainly).
	     @return A string: "Unknown thread". */

	virtual void Block(ES_ThreadBlockType type = ES_BLOCK_UNSPECIFIED);
	/**< Block the thread.  The thread MUST NOT be blocked already.
	     @param type The type (reason) of blocking. */

	virtual OP_STATUS Unblock(ES_ThreadBlockType type = ES_BLOCK_UNSPECIFIED);
	/**< Unblock the thread.  The thread MUST be blocked, and it's block type
	     MUST be equal to the argumnet "type".
	     @param type The type used in the most recent call to Block on this thread.
	     @return OpStatus::ERR_NO_MEMORY or OpStatus::OK. */

	virtual void Pause();
	/**< Pause execution of this thread (forces the scheduler to return
	     control to the message loop before execution of this thread
	     continues.) */

	virtual void RequestTimeoutCheck();
	/**< Tell the thread's context to check if it should yield - to be called by
	     costly operations. */

	virtual void GCTrace(ES_Runtime *runtime);
	/**< Called by the scheduler that owns the thread when the garbage collector
	     runs on the heap associated with the thread's runtime.  Sub-classes
	     that override this method must call this implementation of the
	     function.
	     @param runtime The thread's runtime; should be used to mark referenced
	                    objects. */

	BOOL IsBlocked();
	/**< Return whether this thread is currently blocked or not, either explicitly
	     or by another thread.  Use GetBlockType() to check if this thread is
	     explicitly blocked.
	     @return TRUE if this thread is blocked, FALSE otherwise. */

	BOOL IsStarted();
	/**< Return whether this thread has been started.
	     @return TRUE if this thread is started, FALSE otherwise. */

	BOOL IsCompleted();
	/**< Return whether this thread has been completed.
	     @return TRUE if this thread is completed, FALSE otherwise. */

	BOOL IsFailed();
	/**< Return whether this thread has failed.
	     @return TRUE if this thread has failed, FALSE otherwise. */

	BOOL IsSuspended();
	/**< Return whether this thread has been suspended and reset the suspended
	     state of the thread (meaning this function will only return TRUE once
	     per suspension).
	     @return TRUE if this thread has been suspended, FALSE otherwise. */

	BOOL IsSignalled();
	/**< Return whether this thread has received any of the signals
	     ES_SIGNAL_FINISHED, ES_SIGNAL_FAILED or ES_SIGNAL_CANCELLED.
	     @return TRUE if this thread has been signalled, FALSE otherwise. */

	void AddListener(ES_ThreadListener *listener);
	/**< Add a thread listener to this thread.  The listener will receive all
	     signals this thread receives and will be deleted together with the
	     thread, if it hasn't been removed first using ES_ThreadListener::Remove().
	     @param listener The listener.  MUST NOT be NULL. */

	void SetProgram(ES_Program *program_, BOOL push_program_ = TRUE);
	/**< Set a program to run.  ES_Thread::Evaluate() will call
	     ES_Runtime::PushProgram() if called when is_started is FALSE.  The
	     thread will assume ownership of the program and will delete it when it
	     is deleted itself or when ES_Thread::SetProgram() is called with a
	     different program or when ES_Thread::SetCallable is called.
	     @param program_ A program.  MUST NOT be NULL.
	     @param push_program_ If TRUE, the program will be pushed onto the
	                          context when the thread is started. */

	OP_STATUS SetCallable(ES_Runtime *runtime, ES_Object *callable_, ES_Value* argv_ = NULL, int argc_ = 0);
	/**< Set an object to call as a function.  ES_Thread::Evaluate() will call
	     ES_Runtime::PushCall() if called when is_started is FALSE.
	     @param runtime A runtime.  MUST NOT be NULL.
	     @param callable_ An object.  MUST NOT be NULL.
	     @param argv_ Argument array.  If NULL, no arguments are used.
	     @param argc_ Number of arguments in the argument array.  MUST be zero
	                  if argv_ is NULL. */

	ES_ThreadBlockType GetBlockType();
	/**< Gets the current block type.
	     @return The current block type (ES_BLOCK_NONE if the thread is not
	             blocked). */

	BOOL ReturnedValue();
	/**< Return whether the thread's context returned a value or threw an
	     exception.  The thread MUST be completed.
	     @return TRUE if the context returned a value, FALSE otherwise. */

	OP_STATUS GetReturnedValue(ES_Value *value);
	/**< Get the value returned by the thread's context or the exception
	     thrown by it.  The thread MUST be finished.
	     @param value Receives the returned value, or undefined if no value was
	                  returned.  MUST NOT be NULL.
	     @return Possibly any OpStatus code. */

	BOOL WantStringResult();
	/**< Returned whether this thread should return a string.
	     @return TRUE if this thread's context should return a string.  Passed
	             to the ES engine by ES_Thread::EvaluateThread(). */

	void SetWantStringResult();
	/**< Make a note that this thread should return a string. */

	BOOL WantExceptions();
	/**< Returned whether this thread should propagate unhandled exceptions.
	     @return TRUE if this thread's context should propagate unhandled
	             exceptions.  Passed to the ES engine by
	             ES_Thread::EvaluateThread() if the engine supports it. */

	void SetWantExceptions();
	/**< Make a note that this thread should propagate unhandled exceptions. */

	BOOL AllowCrossOriginErrors();
	/**< Returns flag controlling whether or not full script errors are allowed
	     reported for this thread when executing.
	     @return TRUE if runtime error details are allowed reported to the toplevel
	             error handler, irrespective of origin of script and the runtime
	             executing it. Passed to the ES engine by ES_Thread::EvaluateThread(). */

	void SetAllowCrossOriginErrors();
	/**< Allow cross origin errors to be reported for this thread. */

	ES_ThreadScheduler *GetScheduler();
	/**< Gets the scheduler to which this thread belongs.
	     @return The scheduler or NULL, if this thread hasn't been added to a
	             scheduler yet. */

	ES_Thread *GetInterruptedThread();
	/**< Returns the thread that this thread interrupted (i.e, the argument
	     'interrupt_thread' to ES_ThreadScheduler::AddRunnable()), or NULL
	     if this thread didn't interrupt a thread, or hasn't been added to
	     a scheduler yet.
	     @return A thread or NULL. */

	ES_Thread *GetRunningRootThread();
	/**< Returns the one running thread that is the cause of this thread
	     through the interrupt chain, but only including threads that have
	     not yet finished (and been signalled). In rare
	     circumstances, a thread can be interrupted by another thread after the
	     first thread has already been signalled as finished.

	     The thread returned by this
	     method is the thread that is most suitable to listen to if the
	     the intent is to wait until a whole script has finished.

	     @return A thread, never NULL, if there are no other thread then
	             the thread itself will be returned. */

	BOOL IsOrHasInterrupted(ES_Thread *other_thread);
	/**< Returns TRUE if this thread has interrupted 'other_thread', or if this
	     thread and 'other_thread' is the same thread.
	     @return TRUE or FALSE. */

	ES_Context *GetContext();
	/**< Returns the thread's context or NULL if it hasn't got one (event
	     threads never have context, for instance, they spawm subthreads to
	     execute event handlers).
	     @return A context or NULL. */

	void IncBlockedByForeignThread();
	/**< INTERNAL.  Increment the blocked_by_foreign_thread counter. */

	OP_STATUS DecBlockedByForeignThread();
	/**< INTERNAL.  Decrement the blocked_by_foreign_thread counter. */

	void UseOriginInfoFrom(ES_Thread* origin_thread);
	/**< Only to be used internally in ecmascript_utils. Will make
	     sure this thread uses the same ES_SharedThreadInfo as
	     origin_thread. */

	void SetIsUserRequested() { if (shared) shared->is_user_requested = TRUE; }
	/**< Sets a flag signalling that this thread was directly requested by the
	     user (by using the mouse, keyboard or UI to trigger some action.) */

	BOOL IsUserRequested() { return shared && shared->is_user_requested; }
	/**< Returns TRUE if SetIsUserRequested() has been called.
	     @return TRUE or FALSE. */

	void SetHasOpenedNewWindow();
	/**< Sets a flag signalling that this thread has opened a new window. */

	BOOL HasOpenedNewWindow();
	/**< Returns whether this thread (or its origin thread, or any
	     other thread with the same origin thread) has opened a new
	     window. */

	void SetHasOpenedURL();
	/**< Sets a flag signalling that this thread has tried to open a
	     url. A thread normally only gets at most one chance to make a
	     "user initiated" document load and here we report that that
	     attempt has been consumed. */

	BOOL HasOpenedURL();
	/**< Returns whether this thread has consumed its one chance to make
	     a "user initiated" document load */

	void SetHasOpenedFileUploadDialog();
	/**< Sets a flag signalling that this thread has tried to open a
	     file upload dialog. A thread normally only gets at most one chance to
	     open a "user initiated" file upload dialog and here we report that the
	     attempt has been consumed. */

	BOOL HasOpenedFileUploadDialog();
	/**< Returns whether this thread has consumed its one chance to open a
	     "user initiated" file upload dialog. */

	void SetIsPluginThread() { is_plugin_thread = TRUE; }
	/**< Sets a flag indicating that this thread is initiating a script
	     operation from a plugin. The flag is used to guide the
	     interpretation of properties on the global object: (Flash)
	     plugin security checks relying on them (window.top) not
	     being overridable. */

	BOOL IsPluginThread();
	/**< Returns TRUE if this thread was plugin initiated. */

	void SetIsPluginActive(BOOL is_active) { is_plugin_active = is_active; }
	/**< Sets a flag indicating that this is a blocked thread executing a plugin
	     operation. The flag is reset once the operation completes.

	     It is used to prevent a scheduler from introducing thread deadlocks if
	     nested plugin operations end up trying to add new threads on the
	     is-plugin-active thread's scheduler and interrupt and block a nested
	     plugin thread while doing so. Progress of the nested (but now blocked)
	     thread is dependent on the new thread getting to run to have a chance
	     of becoming unblocked, hence it needs to be scheduled appropriately
	     (with respect to the is-plugin-active thread.) */

	BOOL IsPluginActive();
	/**< Returns a flag indicating if this thread is active in a plugin operation. */

	void SetIsSoftInterrupt() { is_soft_interrupt = TRUE; }
	/**< Sets a flag that indicates that even if this is interrupting
	     another ES_Thread, it only does so to ensure order between
	     threads, and there is no real logical connection, so the
	     other thread might not be aware that it has spawned "sub threads".

	     An example is a non-started timeout thread that happened to be first
	     in the runnable list when a plugin made a call to Opera. */

	BOOL IsSoftInterrupt() { return is_soft_interrupt; }
	/**< @see SetIsSoftInterrupt() */

	void SetOpenInNewWindow() { if (shared) shared->open_in_new_window = TRUE; }
	/**< Sets a flag signalling that this thread was started by the
	     user by clicking a link while holding down the shift key (or
	     otherwise requesting that a link be opened in a new
	     window.)
	     Use GetInfo() or GetOriginInfo to extract this information.*/

	void SetOpenInBackground() { if (shared) shared->open_in_background = TRUE; }
	/**< Sets a flag signalling that this thread was started by the
	     user by clicking a link while holding down the control key
	     (or otherwise requesting that a link be opened in the
	     background.)
	     Use GetInfo() or GetOriginInfo to extract this information.*/

	ES_SharedThreadInfo *GetSharedInfo() { return shared; }
	/**< Returns the thread's shared info.
	     @return A pointer to a ES_SharedThreadInfo object. */

	void SetIsSyncThread() { is_sync_thread = TRUE; }
	/**< Sets a flag indicating that this is a sync thread. That will
	     make sure to break out of the scheduler as soon as it's
	     completed rather than running until the timeslice has expired. */

	BOOL IsSyncThread() { return is_sync_thread; }
	/**< Returns a flag indicating that this is a sync
	     thread. A sync thread will break out of the scheduler as soon
	     as it is completed rather than letting the scheduler run
	     until the timeslice has expired, possibly executing other
	     unwanted and unexpected scripts. */

	BOOL IsExternalScript() const { return is_external_script; }
	/**< Returns a flag indicating that this thread represents
	     an external script. */

	void SetIsExternalScript(BOOL is_external) { is_external_script = is_external; }
	/**< Sets to the given value a flag indicating that this thread represents
	     an external script. */

#ifdef ECMASCRIPT_DEBUGGER
	BOOL HasDebugInfo();
	/**< Checks if the thread contains debug information, if not the
	     thread should not used by the debugger.
	     @return TRUE if the thread has debugging information, FALSE
	             otherwise. */
#endif // ECMASCRIPT_DEBUGGER

protected:
	friend class ES_ThreadSchedulerImpl;
	friend class ES_ForeignThreadBlock;
	friend class ES_RestartObject;

	virtual void Suspend();
	/**< Suspend the thread's context in the ES engine, if it has one.  This will
	     cause the thread to stop executing as soon as possible.  Normally only
	     called by the scheduler when ES_Thread::Block() is called. */

	virtual OP_STATUS Signal(ES_ThreadSignal signal);
	/**< Send a signal to the thread.  See ES_ThreadSignal for more information.
	     Calls ES_ThreadListener::Signal() on all registered listeners and updates
	     is_completed and is_failed appropriately, so subclasses should always
	     call this function.
	     @param signal The signal.
	     @return OpStatus::ERR_NO_MEMORY or OpStatus::OK. */

	void SetBlockType(ES_ThreadBlockType type);
	/**< Sets the current block type.  MUST NOT be used to block or unblock a
	     thread, for that, use Block() and Unblock().
	     @param type New block type. */

	void SetScheduler(ES_ThreadScheduler *scheduler_);
	/**< Set the scheduler to which this thread belongs.  All calls to a certain
	     thread MUST set the same scheduler.  If a scheduler is set before the
	     thread is added to a scheduler, it MUST NOT be added to a different
	     scheduler later.
	     @param scheduler_ The scheduler.  MUST NOT be NULL. */

	void MigrateTo(ES_ThreadScheduler *scheduler_);
	/**< Change the scheduler to which this thread belongs.
	     @param scheduler_ The scheduler.  MUST NOT be NULL. */

	void ForgetScheduler() { scheduler = NULL; }
	/**< Resets the scheduler to NULL. This is only used for timeout threads
	     that are reused over and over again. They still can't be moved
	     between schedulers but it keeps the internal state consistant. */

	void SetInterruptedThread(ES_Thread *thread_);
	/**< Set the thread that this thread interrupts.  Should only be called by
	     ES_ThreadScheduler::AddRunnable().
	     @param thread_ A thread.  Can be NULL, if this thread doesn't interrupt
	                    another thread. */

	unsigned GetInterruptedCount() { return interrupted_count; }
	unsigned GetRecursionDepth() { return recursion_depth; }

private:
	void ResetProgramAndCallable();
	virtual ES_Object *GetCallable();
	virtual ES_Value *GetCallableArgv();
	virtual int GetCallableArgc();
	virtual ES_Program *GetProgram();
	virtual BOOL GetPushProgram();

protected:
	ES_Runtime *runtime;
	/**< Runtime used to protect 'callable' and objects in 'callable_argv'.  If
	     SetCallable() has not been used, this variable is NULL! */

	ES_Context *context;
	ES_Program *program;
	ES_Object *callable;
	ES_Value *callable_argv;
	int callable_argc;
	ES_ThreadScheduler *scheduler;
	ES_ThreadBlockType block_type;
	ES_Thread *interrupted_thread;
	ES_RestartObject *restart_object_stack;
	ES_RestartObject *restart_object_discarded;
	unsigned interrupted_count;
	unsigned recursion_depth;
	Head listeners;
	ES_SharedThreadInfo *shared;

	unsigned is_started:1;
	unsigned is_completed:1;
	unsigned is_failed:1;
	unsigned is_suspended:1;
	unsigned is_signalled:1;
	unsigned returned_value:1;
	unsigned want_string_result:1;
	unsigned want_exceptions:1;
	unsigned allow_cross_origin_errors:1;
	unsigned push_program:1;
	unsigned is_sync_thread:1;
	unsigned is_external_script:1;
	unsigned is_plugin_thread:1;
	unsigned is_plugin_active:1;
	unsigned is_soft_interrupt:1; ///< TRUE means that this is an interrupt to ensure order with no real logical connection between threads. Will make CancelThread behave differently.

	unsigned blocked_by_foreign_thread; ///< counter

private:

#ifdef SCOPE_PROFILER
	unsigned thread_id; ///< ID used by profiler to track unique threads over time.

	void SetThreadId(unsigned id) { if (thread_id == 0) thread_id = id; }
	/**< Assign an ID to this thread. May be called multiple times, but has no
	     effect if the thread already has an ID assigned.

	     @param id The ID for the thread. */
#endif // SCOPE_PROFILER
	friend class ES_TimeoutTimerEvent;
};

/* --- ES_EmptyThread ------------------------------------------------ */

class ES_EmptyThread
	: public ES_Thread
{
public:
	ES_EmptyThread();
	/** Constructor. */

	virtual OP_STATUS EvaluateThread();
	/**< Does nothing except signalling that this thread is done.
		 @return OpStatus::OK.
		*/

	ES_ThreadType Type();
	/**< The thread's type.  This class' type is ES_THREAD_EMPTY.
		 @return The thread's type.
		*/
};

/* --- ES_TimeoutThread ---------------------------------------------- */

class ES_TimeoutThread
	: public ES_Thread
{
public:
	ES_TimeoutThread(ES_Context *context, ES_SharedThreadInfo *shared);
	/**< Constructor.
		 @param context The thread's context.  MAY be NULL, but then this class'
		                EvaluateThread() MUST NOT be used.
		 @param shared  The shared thread info that this thread should be
		                initialized with. */

	virtual ~ES_TimeoutThread();

	virtual OP_STATUS EvaluateThread();
	/**< Evaluates this thread by calling ES_Thread::EvaluateThread.  Resets and
		 restarts the thread if it is a repeating (interval) thread. */

	ES_ThreadType Type();
	/**< The thread's type.  This class' type is ES_THREAD_TIMEOUT.
		 @return The thread's type.
		*/

	virtual ES_ThreadInfo GetOriginInfo();
	/**< Return information about the origin of this thread.  This class' version
		 returns information about the thread that called setTimeout or
		 setInterval to start this thread (as set by SetOriginInfo()).  If no
		 information has been set, this thread returns information about itself.
		 @return A thread info object.
		*/

	void SetOriginInfo(const ES_ThreadInfo &info);
	/**< Set the origin information of this thread.  Normally called to set the
		 information about the thread that called setTimeout or setInterval to
		 start this thread.
		 @return A thread info object.
		*/

	const uni_char *GetInfoString();
	/**< Return a string describing the thread (for error reporting, mainly).
		 @return A string: "Timeout thread: delay <delay> ms".
		*/

	unsigned int GetId();
	/**< Get this thread's id.  The id is returned from the javascript functions
		 setTimeout and setInterval and can be used to cancel this thread.
		 @return An id unique to this thread's scheduler.
		*/

	void StopRepeating();
	/**< INTERNAL.  Make this thread stop repeating if it is a repeating thread. */

	ES_TimeoutTimerEvent *GetTimerEvent();
	/**< INTERNAL.  Get the timer event that initiated this thread.
	     @return A timer event.
         */

	void SetTimerEvent(ES_TimeoutTimerEvent *event);
	/**< INTERNAL.  Set the timer event that initiated this thread.
	     @param event The event.
	     */

private:
	ES_ThreadInfo origin_info;
	ES_TimeoutTimerEvent *timer_event;
	unsigned int id;

	virtual ES_Object *GetCallable();
	virtual ES_Value *GetCallableArgv();
	virtual int GetCallableArgc();
	virtual ES_Program *GetProgram();
	virtual BOOL GetPushProgram();
};

/* --- ES_InlineScriptThread ----------------------------------------- */

class ES_InlineScriptThread
	: public ES_Thread
{
public:
	ES_InlineScriptThread(ES_Context *context, ES_Program *program,
	                      ES_SharedThreadInfo *shared = NULL);

	virtual ~ES_InlineScriptThread();

	ES_ThreadType Type();
	/**< The thread's type.  This class' type is ES_THREAD_INLINE_SCRIPT.
		 @return The thread's type.
		*/

	const uni_char *GetInfoString();
	/**< Return a string describing the thread (for error reporting, mainly).
		 @return A string: "Inline script thread".
		*/
};

/* --- ES_DebuggerEvalThread ----------------------------------------- */

class ES_DebuggerEvalThread
	: public ES_Thread
{
public:
	ES_DebuggerEvalThread(ES_Context *context, ES_SharedThreadInfo *shared = NULL);

	ES_ThreadType Type();
	/**< The thread's type.  This class' type is ES_THREAD_DEBUGGER_EVAL.
		 @return The thread's type.
		*/

	const uni_char *GetInfoString();
	/**< Return a string describing the thread (for error reporting, mainly).
		 @return A string: "Debugger eval thread".
		*/
};

/* --- ES_JavascriptURLThread ---------------------------------------- */

class ES_JavascriptURLThread
	: public ES_Thread
{
public:
	ES_JavascriptURLThread(ES_Context *context, uni_char *source, const URL &url,
	                       BOOL write_result_to_document, BOOL write_result_to_url, BOOL is_reload,
						   BOOL want_utf8, ES_SharedThreadInfo* shared);
	~ES_JavascriptURLThread();

	virtual OP_STATUS EvaluateThread();
	ES_ThreadType Type();

	const uni_char *GetInfoString();
	/**< Return a string describing the thread (for error reporting, mainly).
		 @return A string: "Javascript URL thread: \"url\"".
		*/

	URL GetURL() { return url; }
	/**< Return the javascript: URL this thread is executing.
	     @return A URL.
		*/

	const uni_char *GetSource() { return source; }
	/**< Returns the source of the javascript: URL. */

#if defined USER_JAVASCRIPT
	OP_STATUS SetSource(const uni_char *source);
	/**< Sets the source of the javascript: URL.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS SetResult(const uni_char *result);
	/**< Sets the result of the javascript: URL.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
#endif // USER_JAVASCRIPT

	static OP_STATUS Load(ES_ThreadScheduler *scheduler, const uni_char *source, const URL &url, BOOL write_result_to_document, BOOL write_result_to_url, BOOL is_reload, BOOL want_utf8, ES_Thread *origin_thread, BOOL is_user_requested = FALSE, BOOL open_in_new_window = FALSE, BOOL open_in_background = FALSE);

protected:
	OP_STATUS Signal(ES_ThreadSignal signal);

private:
	OP_STATUS PostURLMessages(BOOL failed, BOOL cancelled);
	FramesDocument* GetFramesDocument();

	enum State
	{
		STATE_INITIAL,
		STATE_SET_PROGRAM,
		STATE_EVALUATE,
		STATE_SEND_USER_JS_AFTER,
		STATE_HANDLE_RESULT,
		STATE_DONE
	};

	uni_char *source;
	URL url;
	BOOL write_result_to_document;
	BOOL write_result_to_url;
	BOOL is_reload;
	BOOL want_utf8;
#ifdef USER_JAVASCRIPT
	uni_char *result;
	BOOL has_result;
#endif // USER_JAVASCRIPT
	State eval_state;
};

/* --- ES_HistoryNavigationThread ------------------------------------ */

class ES_HistoryNavigationThread
	: public ES_Thread
{
public:
	ES_HistoryNavigationThread(ES_SharedThreadInfo* shared, DocumentManager *doc_man, int delta)
		: ES_Thread(NULL, shared)
		, document_manager(doc_man)
		, delta(delta)
	{
	}

	virtual OP_STATUS EvaluateThread();
	ES_ThreadType Type();
	/**< The thread's type.  This class' type is ES_THREAD_HISTORY_NAVIGATION.
		 @return The thread's type.
		*/

	const uni_char *GetInfoString();
	/**< Return a string describing the thread (for error reporting, mainly).
		 @return A string: "History navigation thread".
		*/

protected:
	OP_STATUS Signal(ES_ThreadSignal signal);

private:
	DocumentManager *document_manager;
	int delta;
};

/* --- ES_TerminatingAction ------------------------------------------ */

/**
 * Termination action types, returned by ES_TerminationAction::Type().
 */
enum ES_TerminatingActionType
{
	ES_GENERIC_ACTION,
	ES_TERMINATED_BY_PARENT_ACTION,
	ES_OPEN_URL_ACTION,
	ES_HISTORY_WALK_ACTION,
	ES_WINDOW_CLOSE_ACTION,
	ES_REPLACE_DOCUMENT_ACTION,
};


class ES_TerminatingThread;

class ES_TerminatingAction
	: public Link
{
public:
	ES_TerminatingAction(BOOL final, BOOL conditional, BOOL send_onunload, ES_TerminatingActionType type, BOOL terminated_by_parent = FALSE);
	virtual ~ES_TerminatingAction();

	virtual OP_STATUS PerformBeforeUnload(ES_TerminatingThread *thread, BOOL sending_onunload, BOOL &cancel_onunload);
	/**< Called by ES_TerminatingThread when this action should be performed.
	     This function is called once, before the unload event is processed,
	     regardless of an unload event is actually sent.
		 @param thread The terminating thread.
		 @param sending_onunload TRUE if an unload event will be sent.
	     @return OpStatus::OK if the action is completed normally or
	             OpStatus::ERR* to signal errors.
		*/

	virtual OP_STATUS PerformAfterUnload(ES_TerminatingThread *thread);
	/**< Called by ES_TerminatingThread when this action should be performed.
	     This function is called once, after the unload event is processed,
	     regardless of an unload event is actually sent.
	     @return OpStatus::OK if the action is completed normally or
	             OpStatus::ERR* to signal errors.
		*/

	BOOL IsLast();
	/**< Check if this is the last terminating action of a terminating thread.
	     @return TRUE or FALSE.
		*/

	BOOL IsFinal();
	/**< Check if this action is final.  A final action must be the last
	     action of a terminating thread.  Non-final (unconditional) actions
	     added later will be added before it, and another final action will
	     replace it.
	     @return TRUE or FALSE.
		*/

	BOOL IsConditional();
	/**< Check if this action is conditional.  A conditional action will not be
	     performed unless it is the last action of a terminating thread.  If it
	     is added to a terminating thread that already has a final action or if
	     an action is added after it later, it will be removed without being
	     performed.
	     @return TRUE or FALSE.
		*/

	BOOL SendOnUnload();
	/**< Check if an onunload event should be fired.  The last action of a
	     terminating thread decides this, and the event thread will be evaluated
	     before any terminating actions are performed.
	     @return TRUE or FALSE
		*/

	ES_TerminatingActionType Type();
	/**< Returns action's type.
	     @return ES_TerminatingActionType
	     @see ES_TerminatingActionType
		*/

protected:
	BOOL final;
	BOOL conditional;
	BOOL send_onunload;
	BOOL terminated_by_parent;
	ES_TerminatingActionType type;
};

/* --- ES_TerminatingThread ------------------------------------------ */

class ES_TerminatingThread
	: public ES_Thread
{
public:
	ES_TerminatingThread();
	~ES_TerminatingThread();

	virtual OP_STATUS EvaluateThread();
	ES_ThreadType Type();

	void AddAction(ES_TerminatingAction *action);
	void RemoveAction(ES_TerminatingActionType type); // Remove by type
	BOOL TestAction(BOOL final, BOOL conditional);
	BOOL TestAction(ES_TerminatingAction *action);
	void CancelUnload();

	void ChildNoticed(ES_Thread *thread);
	OP_STATUS ChildTerminated(ES_Thread *thread);

#ifdef DEBUG_THREAD_DEADLOCKS
	void AddDependencies(ES_Thread *other);
#endif // DEBUG_THREAD_DEADLOCKS

protected:
	OP_STATUS Signal(ES_ThreadSignal signal);

	OP_STATUS SendUnload();

private:
	Head actions;
	int children;
	BOOL delayed_unload;
	BOOL children_terminated;

#ifdef DEBUG_THREAD_DEADLOCKS
	class Child
		: public Link
	{
	public:
		ES_Thread *thread;
	};
	Head all_children;
#endif // DEBUG_THREAD_DEADLOCKS
};

#endif /* ES_UTILS_ESTHREAD_H */
