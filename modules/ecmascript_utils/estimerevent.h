/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESTIMEREVENT_H
#define ES_UTILS_ESTIMEREVENT_H

#include "modules/util/simset.h"
#include "modules/ecmascript_utils/esthread.h"

class ES_Object;
class ES_Program;
class ES_Runtime;
class ES_SharedThreadInfo;
class ES_Thread;
class ES_TimeoutThread;
class ES_Value;

/**
 * A timer event can be a one-time event or a repeated event and is added to a
 * ES_TimerManager. To use it an implementation must implement Expire() that will be
 * called once the timer event expires.
 *
 * If the event is a repeating one then it is itself responsible for updating
 * the new timeout time and adding itself to the ES_TimerManager again after it has
 * expired. Once added it is owned by the ES_TimerManager and is deleted automatically
 * when the ES_TimerManager is shut down, unless it is expired. If expired it will only
 * be deleted by the ES_TimerManager if Expire() returns OpBoolean::IS_FALSE, else it
 * must be deleted by an explicit call to ES_TimerManager::RemoveExpiredEvent().
 */
class ES_TimerEvent
	: public ListElement<ES_TimerEvent>
{
public:
	ES_TimerEvent(double delay, BOOL is_repeating);
	/**< Constructor. The delay is the time from now until the thread's execution time.
	     @param delay Delay in milliseconds.
	     @param is_repeating If TRUE, this event will fire repeatedly, every
		                     'delay' milliseconds. */

	virtual ~ES_TimerEvent();

	double GetDelay();
	/**< Get this event's delay.
	     @return The event's delay in milliseconds. */

	unsigned int GetId();
	/**< Get this event's id, which can be used to later cancel this event.
	     @return An id unique to this event. */

	void StopRepeating();
	/**< Make this event stop repeating if it is a repeating event. */

	ES_TimerManager *GetTimerManager();
	/**< Gets the the timer manager the event belongs to. */

	double GetTotalRequestedDelay();
	/**< Returns the total requested delay from when the initial timeout was started. This
	     is used to recognise long timeouts (or many chained short timeouts) and prevent
	     them from opening popups. */

protected:
	BOOL IsRepeating();
	/**< Return whether this event is repeating.
	     @return TRUE if this event is repeating. */

	void SetPreviousDelay(double new_previous_delay);
	/**< Sets the total requested delay before this timeout even started. */

	void SetNextTimeout();
	/**< Initialize the event's next timeout time as delay + current time. */

#ifdef SCOPE_PROFILER
	unsigned GetScopeThreadId();
	/**< Get the scope thread id.
	     @return The id. */
#endif // SCOPE_PROFILER

private:
	ES_TimerManager *timer_manager;

	double delay, timeout_time, next_timeout_time;
	double previous_delay;

	BOOL is_repeating;
	unsigned int id;
#ifdef SCOPE_PROFILER
	unsigned int scope_thread_id;
#endif // SCOPE_PROFILER

	friend class ES_TimerManager;

	virtual OP_BOOLEAN Expire() = 0;
	/**< Called when the event expires.
	     @return OpBoolean::IS_TRUE if the event should be kept alive (in the
	             expired list) or OpBoolean::IS_FALSE if not. If the event is
	             kept alive it must be removed explicity with
	             ES_TimerManager::RemoveExpiredEvent(). Can also return
	             OpBoolean::ERR_NO_MEMORY. */

	void SetId(unsigned int new_id);
	/**< Set this event's id.  MUST NOT be called twice.
	     @param new_id The event's id. */

#ifdef SCOPE_PROFILER
	void SetScopeThreadId(unsigned int new_id);
	/**< Set the scope thread id.
	     @param new_id The id. */
#endif // SCOPE_PROFILER

	void SetTimeout();
	/**< Initialize the event's timeout time as delay + current time. */

	double GetTimeout();
	/**< Get the event's timeout time (i.e., the time in wallclock time
		 this event should be executed).
	     @return The timeout time in milliseconds. */

	BOOL TimeoutBefore(ES_TimerEvent *other);
	/**< Check if this event expires before another event.
	     @param other Event to check against.
	     @return TRUE if this event expires before the other thread. */

	BOOL TimeoutExpired();
	/**< Check if this event should fire now, i.e., if it's timeout
		 time is in the past.
	     @return TRUE if this event has expired. */

	void SetTimerManager(ES_TimerManager *timer_manager_);
	/**< Set the timer manager the event belongs to. */
};

class ES_TimeoutTimerEvent
	: public ES_TimerEvent,
	  public ES_ThreadListener
{
public:
	ES_TimeoutTimerEvent(ES_Runtime *runtime, double delay, BOOL is_repeating, ES_Thread *origin_thread);
	/**< Constructor. The delay is the time from now until the thread's execution time.
	     @param runtime The runtime this timeout event belongs to.
	     @param delay Delay in milliseconds.
	     @param is_repeating If TRUE, this event will fire repeatedly, every
	                         'delay' milliseconds.
	     @param origin_thread The thread that spawned this event. */

	virtual ~ES_TimeoutTimerEvent();

	void SetProgram(ES_Program *program_, BOOL push_program_ = TRUE);
	/**< Set a program to run when this event fires. The event will assume ownership of the program
	     and will delete it when it is deleted itself.
	     @param program_ A program.  MUST NOT be NULL.
	     @param push_program_ If TRUE, the program will be pushed onto the
	                          context when the thread is started. */

	OP_STATUS SetCallable(ES_Object *callable_, ES_Value* argv_ = NULL, int argc_ = 0);
	/**< Set an object to call as a function when this event fires.
	     @param callable_ An object. MUST NOT be NULL. The object is kept alive
	                      during the lifetime of this event.
	     @param argv_ Argument array. If NULL, no arguments are used. The array
	                  is copied and its items are kept alive.
	     @param argc_ Number of arguments in the argument array.  MUST be zero
	                  if argv_ is NULL.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	/* From ES_ThreadListener. */
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);
	/**< Called from ES_Thread::Signal() for every signal the thread receives.
	     Used for repeating the event when the thread has finished execution.
	     @param thread The thread this listener belongs to.
	     @param signal The signal the thread received.
	     @return OpStatus::ERR_NO_MEMORY or OpStatus::OK. */

protected:
	virtual OP_BOOLEAN Expire();

private:
	ES_Runtime *runtime;
	ES_SharedThreadInfo *shared;
	ES_ThreadInfo origin_info;
	BOOL from_plugin_origin;

	friend class ES_TimeoutThread;
	ES_Program *program;
	ES_Object *callable;
	ES_Value* callable_argv;
	int callable_argc;
	BOOL push_program;

	void ResetProgramAndCallable();
};

#endif // ES_UTILS_ESTIMEREVENT_H
