/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESSCHED_H
#define ES_UTILS_ESSCHED_H

#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript/ecmascript.h"

class ES_Runtime;
class Window;

class ES_ThreadScheduler
{
public:
	static ES_ThreadScheduler *Make(ES_Runtime *runtime, BOOL always_enabled = FALSE, BOOL is_dom_runtime = FALSE);
	/**< Create a scheduler running threads with the specified runtime.  The
	     created scheduler object is owned by the called and must be freed
	     using the delete operator.
	     @param runtime The runtime.  MUST NOT be NULL.
	     @param always_enabled If TRUE, this scheduler will always be
	                           enabled, regardless of preferences.
	     @param is_dom_runtime If TRUE, the scheduler's runtime is guaranteed to be
	                           a DOM runtime.
	     @return A thread scheduler or NULL on OOM. */

	virtual ~ES_ThreadScheduler();
	/**< Destructor.  When called, the scheduler MUST have no runnable tasks.
	     That should be achieved by terminating the scheduler by adding a
	     terminating action or by calling ES_ThreadScheduler::RemoveThreads(TRUE). */

#ifdef ESUTILS_SYNCIF_SUPPORT
	virtual OP_STATUS ExecuteThread(ES_Thread *thread, unsigned timeslice, BOOL leave_thread_alive) = 0;
	/**< Execute thread until it finishes, or fail if it hasn't
	     finished in the time allowed by the timeslice parameter.
	     If the thread becomes blocked (except if
	     blocked by an interrupting thread) it is cancelled and this call fails.
	     The thread to execute should not be added to the scheduler prior to
	     this call; it will be added implicitly first in the queue of runnable
	     threads by this function, and always removed before this call returns.

	     @param thread Thread to execute.
	     @param timeslice The maximum amount of time to execute scripts before
	                      returning in milliseconds. A script might execute
	                      slightly longer than this since a single
	                      uninterruptible operation might bring us over the
	                      limit.
	     @param leave_thread_alive If the thread didn't complete, it would normally
	            be terminated but if this TRUE then it will be left as is ready
	            to be run again/more.
	     @return OpStatus::OK, OpStatus::ERR if the thread was cancelled either
	             because it hadn't finished when the timeslice ended or because
	             it became blocked and leave_thread_alive was FALSE,
	             or OpStatus::ERR_NO_MEMORY on OOM. */
#endif // ESUTILS_SYNCIF_SUPPORT

	virtual OP_BOOLEAN AddRunnable(ES_Thread *new_thread, ES_Thread *interrupt_thread = NULL) = 0;
	/**< Add an immediately runnable thread to the scheduler.  A thread MUST
	     only be added to one scheduler, and would normally only be added once.

	     The scheduler assumes ownership of the thread and will delete it
	     immediately if it was not added to the scheduler (signalled by any
	     other return value than OpBoolean::IS_TRUE).

	     The interrupt thread, if not NULL, will be blocked during the execution
	     of the new thread.  If the interrupt thread belongs to this scheduler,
	     the new thread will be added immediately before it in the list of
	     runnable threads, otherwise the interrupt thread will be blocked with
	     the block type ES_BLOCK_FOREIGN_THREAD.

	     @param new_thread The new, runnable thread.  MUST NOT be NULL.
	     @param interrupt_thread Optional thread to interrupt.
	     @return OpStatus::ERR_NO_MEMORY, OpStatus::ERR (in case of invalid
	             arguments), OpBoolean::IS_TRUE if the thread was added
	             or else OpBoolean::IS_FALSE. */

	virtual OP_STATUS AddTerminatingAction(ES_TerminatingAction *action, ES_Thread *interrupt_thread = NULL, ES_Thread *blocking_thread = NULL) = 0;
	/**< Add a terminating action to this scheduler.  This will add a terminating
	     thread to the scheduler if this is the first terminating action being
	     added.  The scheduler will continue executing its current set of runnable
	     threads, then it will perform the terminating action(s).

	     The scheduler assumes ownership of the action and will delete it.

	     @param action A terminating action.  MUST NOT be NULL.
	     @param interrupt_thread Optional thread that the terminating thread will
	                             interrupt, if a new terminating thread is created
	                             and added to the scheduler.  The interrupt_thread
	                             MUST NOT be a thread in this scheduler.
	     @param blocking_thread Optional thread which will block the
	                             terminating thread until finished.
	     @return OpStatus::ERR_NO_MEMORY or OpStatus::OK. */

	virtual BOOL TestTerminatingAction(BOOL final, BOOL conditional) = 0;
	/**< Test if a terminating action with the given properties could be added
	     to the scheduler.  Returns FALSE if the scheduler is already being
	     terminated by a terminating action that wouldn't be overridden.

	     @param final The 'final' property of the imaginary action to test with.
	     @param conditional The 'conditional' property of the action.
	     @return TRUE if the imaginary action would be added by a call to
	             AddTerminatingAction. */

	virtual void RemoveThreads(BOOL terminating = FALSE, BOOL final = FALSE) = 0;
	/**< Remove threads from the scheduler.  If terminating is TRUE, it removes all
	     runnable threads.  If final is TRUE is also removes all waiting threads.
	     If neither are TRUE, no threads are removed, but the scheduler stops
	     executing somewhat.

	     This function MUST NOT be called while the scheduler is active, except if
	     it is currently executing its terminating thread.

	     @param terminating If TRUE, remove runnable threads.  Use this only when
	                        there is no time to use a terminating action (e.g. when the
	                        user closes a window to which this scheduler belongs).
	     @param final If TRUE, also remove waiting threads.  Use this when the
	                  scheduler will never be used again (that is, when its document
	                  is being deleted rather than just being made inactive). */

	virtual OP_STATUS MigrateThread(ES_Thread *thread) = 0;
	/**< Migrate a running thread from its current scheduler to this one.  Will
	     also migrate all threads that the thread has interrupted, while
	     cancelling any other threads that have interrupted those threads.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS CancelThreads(ES_ThreadType type) = 0;
	/**< Cancel (by calling CancelThread()) all threads of given type.
	     @see CancelThread()
	     @param type A type of the thread to cancel. @see ES_ThreadType.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. Note: the threads are
	             cancelled and deleted regardless of the return value, only the
	             signalling to the threads can fail. */

	virtual OP_STATUS CancelThread(ES_Thread *thread) = 0;
	/**< Unconditionally cancel a thread in the scheduler.  If the thread has been
	     interrupted, the interrupting thread(s) will be cancelled too, before
	     this thread.

	     @param thread A thread to cancel.  MUST NOT be NULL, MUST be a thread added
	                   to this scheduler (either runnable or waiting).
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.  Note: the thread is
	             cancelled and deleted regardless of the return value, only the
	             signalling to the thread can fail. */

	virtual OP_STATUS CancelAllTimeouts(BOOL cancel_runnable, BOOL cancel_waiting) = 0;
	/**< Cancels all timeout or interval threads.

	     @param cancel_runnable Whether to cancel timeout threads from runnable queue.
	     @param cancel_waiting Whether to cancel waiting threads.
		 @return OpStatus::OK or OpStatus::ERR_NO_MEMORY (signals OOM only if
		         posting a delayed message fails.) */

	virtual OP_STATUS CancelTimeout(unsigned id) = 0;
	/**< Cancels the timeout or interval thread scheduled for execution.
	     The thread is identified by the id.

	     @param id The id of the timeout/interval thread to cancel.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY in case of OOM */

	virtual OP_STATUS Activate() = 0;
	/**< (Re)activate the scheduler.  Currently, this includes reactivating
	     timeout and interval threads that were active when the scheduler was
	     terminated and calling Reset().
	     @return OpStatus::ERR_NO_MEMORY or OpStatus::OK. */

	virtual void Reset() = 0;
	/**< Reset this scheduler to the state it was in when it was constructed.
		 This is necessary when a scheduler is being reused after being terminated
		 as it would otherwise refuse new threads. */

	virtual void Block(ES_Thread *thread, ES_ThreadBlockType type = ES_BLOCK_UNSPECIFIED) = 0;
	/**< Equivalent to "thread->Block(type)".  See ES_Thread::Block().
		 @param thread The thread to block.
		 @param type The type (reason) of blocking. */

	virtual OP_STATUS Unblock(ES_Thread *thread, ES_ThreadBlockType type = ES_BLOCK_UNSPECIFIED) = 0;
	/**< Equivalent to "thread->Unblock(type)".  See ES_Thread::Unblock().
	     @param thread The thread to unblock.
	     @param type The type (reason) of blocking.
	     @return OpStatus::ERR_NO_MEMORY, OpStatus::ERR (if type does not match
	             the argument to the last call to Block() for this thread) or
	             OpStatus::OK. */

	virtual BOOL IsActive() = 0;
	/**< Check if this scheduler is active (currently executing).
	     @return TRUE if this scheduler is currently executing a thread. */

	virtual BOOL IsBlocked() = 0;
	/**< Check if the current thread of this scheduler is blocked, i.e., if
	     this entire scheduler is blocked.
	     @return TRUE if this scheduler is blocked. */

	virtual BOOL IsDraining() = 0;
	/**< Check if this scheduler is being terminated, i.e., if it is currently
	     draining its list of runnable threads.
	     @return TRUE if this scheduler is being terminated. */

	virtual BOOL HasRunnableTasks() = 0;
	/**< Check if the scheduler has runnable threads.  Note that having runnable
	     threads does not mean the scheduler can run right now.  Runnable threads
	     are simply threads in the list of runnable threads, they can still be
	     blocked.
	     @return TRUE if the scheduler has runnable threads. */

	virtual BOOL HasTerminatingThread() = 0;
	/**< Check if scheduler has a terminating thread, i.e., if it is currently
	     being terminated.
	     @return TRUE if the scheduler has a terminating thread. */

	virtual ES_TerminatingThread *GetTerminatingThread() = 0;
	/**< Get the scheduler's terminating thread, if it has one.
	     @return A terminating thread or NULL. */

	virtual ES_Runtime *GetRuntime() = 0;
	/**< Get the scheduler's runtime.
	     @return A runtime.  Can not NULL. */

	virtual FramesDocument *GetFramesDocument() = 0;
	/**< Get the scheduler's runtime's frames document, if there is one.
	     @return A frames document or NULL if the scheduler's runtime is not
	             associated with a document. */

	virtual ES_TimerManager *GetTimerManager() = 0;
	/**< Get the scheduler's timer manager.
	     @return A timer manager.  Can not NULL. */

	virtual ES_Thread *GetCurrentThread() = 0;
	/**< Get the scheduler's current thread.  When the scheduler is active,
	     this is the thread currently being executed.  When the scheduler is
	     inactive, this is the thread that would be executed if the scheduler
	     became active right now, i.e., the first thread in the list of runnable
	     threads.
	     @return The current thread, or NULL if the list of runnable threads is
	             empty. */

	virtual ES_Thread *SetOverrideCurrentThread(ES_Thread *thread) = 0;
	/**< Temporarily override the return value of GetCurrentThread().  Typically
	     used when this scheduler (or rather its associated ES_Runtime) must
	     seem to be the active one, but actually isn't.  To unset, another call
	     should be made with the return value of the call that set the override.

	     @return The previous override current thread, or NULL if there was no
	             override set before.  Never returns the actual current thread
	             of this scheduler. */

	virtual ES_Thread *GetErrorHandlerInterruptThread(ES_Runtime::ErrorHandler::ErrorType type) = 0;
	/**< Get the thread that should be interrupted if the window.onerror error
	     handler is called, either for a compilation error or uncaught runtime
	     error.  For runtime errors, the scheduler simply returns the currently
	     executing thread.

		 For compilation errors, callers of ES_Runtime::CompileProgram() needs
	     to use SetErrorHandlerInterruptThread() to set the appropriate thread
	     (if there is one) and ResetErrorHandlerInterruptThread() after the
	     call.

	     @param type Type of error being handled.
	     @return Thread to interrupt or NULL. */

	virtual void SetErrorHandlerInterruptThread(ES_Thread *thread) = 0;
	/**< Temporarily set thread to interrupt when calling the window.onerror
	     error handler.  Caller must make sure to reset it again.  Must be reset
	     before the thread could conceivably finish and die. */

	virtual void ResetErrorHandlerInterruptThread() = 0;
	/**< Reset the error handler interrupt thread. */

	virtual const uni_char *GetThreadInfoString() = 0;
	/**< Return a string describing the currently executing thread (used by
	     the JavaScript console when reporting errors).  The returned string is
	     constant or stored in the buffer returned by g_memory_manager->GetTempBuf.
	     @return A informational string. */

	virtual OP_STATUS GetLastError() = 0;
	/**< Return the last error that was raised during the execution of a thread.
	     Only meant to be used from a thread listener that wants to know the
	     reason for a ES_SIGNAL_FAILED, its value in any other situation is
	     undefined.
		 @return An error code. */

	virtual void ResumeIfNeeded() = 0;
	/**< Something has changed that might affect whether this scheduler should
	     be executing scripts right now.  Reevaluate, and if needed, resume
	     script execution (that is, post a message about it.) */

	virtual void PushNestedActivation() = 0;
	/**< Used by nested message loops to hide the fact that the scheduler is
	     already running in lower levels of nesting. That means that it's
	     possible that the scheduler is active when IsActive() is FALSE, but
	     in a lower level message loop. If that case is interesting then check
	     IsPresentOnTheStack().
	     Only to be used if IsActive() returns TRUE. */

	virtual void PopNestedActivation() = 0;
	/**< Used to balance calls to PushNestedActivation when nested message
	     loops unwind.
	     Since PushNestedActivation() is only called when IsActive() is TRUE, this
	     will result in IsActive() being TRUE. */

	virtual BOOL IsPresentOnTheStack() = 0;
	/**< Normally IsActive would be a good enough indication of whether the
	     scheduler is on the stack, but when we run nested message
	     loops this is one of the few objects
	     that might be present on the stack of the hidden message loops.
	     Use this method to see if that is the case and then act accordingly.
	     For instance deleting the object or running thread would be wrong. */

	virtual OP_STATUS SerializeThreads(ES_Thread *execute_first, ES_Thread *execute_last) = 0;
	/**< Block the thread 'execute_last' until the thread 'execute_first' has
	     finished if the threads are in different schedulers. If 'execute_last'
	     is canceled 'execute_first' will also be canceled unless it has been
	     migrated to a different scheduler. If 'execute_first' is canceled it is
	     no longer blocking 'execute_last'.
	     @return OpStatus::ERR_NO_MEMORY on OOM otherwise OpStatus::OK. */

	virtual void GCTrace() = 0;
	/**< Should be called by the owner of scheduler when the garbage collector
	     runs on the heap associated with the scheduler's runtime.  The
	     scheduler propagates the call to its threads. */
};

#endif /* ES_UTILS_ESSCHED_H */
