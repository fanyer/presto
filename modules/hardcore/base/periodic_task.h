/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PERIODICTASK_H
#define PERIODICTASK_H

#include "modules/util/simset.h"
#include "modules/hardcore/base/opstatus.h"

class PeriodicTask : public Link
{
private:
	BOOL enabled;
	UINT disabled_count;

public:
	/**
	 * The constructor initializes a periodic task that is enabled.
	 * @see IsEnabled()
	 */
	PeriodicTask()
		: enabled(TRUE)
		, disabled_count(0)
		{}

	virtual ~PeriodicTask() {}

	/**
	 * Returns TRUE if the task is enabled.
	 *
	 * If the task is enabled, PeriodicTaskManager::RunTasks() calls
	 * the abstract method Run(). If a task is disabled,
	 * PeriodicTaskManager::RunTasks() increments the counter of how
	 * many times this task has been triggered since it was disabled
	 * (see IncrementDisabledTriggerCount()) and calls
	 * RunWasDisabled().
	 */
	BOOL IsEnabled() const { return enabled; }

	/**
	 * Enable the task. The next time PeriodicTaskManager::RunTasks()
	 * is called, Run() is called on this task. Calling SetEnabled()
	 * on a task that is already running does not change anything.
	 *
	 * @see IsEnabled()
	 * @see SetDisabled()
	 */
	void SetEnabled() { enabled = TRUE; disabled_count=0; }

	/**
	 * Disables the task. If the task is enabled, then the disabled
	 * trigger counter is reset, i.e. GetDisabledTriggerCount()
	 * returns 0. Calling SetDisabled() on a disabled
	 * task does not reset the counter. The next time
	 * PeriodicTaskManager::RunTasks() is called, the disabled trigger
	 * counter is increased and RunWasDisabled() is called.
	 *
	 * @see IsEnabled()
	 * @see SetEnabled()
	 */
	void SetDisabled()
		{
			if (enabled)
			{
				enabled = FALSE;
				disabled_count=0;
			}
		}

	/**
	 * Get the number of times that this task has been triggered since
	 * it was disabled.
	 *
	 * @see SetDisabled()
	 * @see IsEnabled()
	 */
	UINT GetDisabledTriggerCount() const { return disabled_count; }

	/**
	 * Increments the counter of how many times this task has been
	 * triggered since it was disabled.
	 *
	 * @note This method is called by PeriodicTaskManager::RunTasks()
	 *  if the task is disabled. No one else should need to call this
	 *  method.
	 *
	 * @return the counter after it was incremented.
	 */
	UINT IncrementDisabledTriggerCount() { return ++disabled_count; }

	/**
	 * This method is called by PeriodicTaskManager::RunTasks() each
	 * time the interval for which the task was registered (see
	 * PeriodicTaskManager::RegisterTask()) has passed and this task
	 * is enabled.
	 *
	 * @see IsEnabled()
	 * @see PeriodicTaskManager::RegisterTask()
	 */
	virtual void Run() = 0;

	/**
	 * This method is called by PeriodicTaskManager::RunTasks() each
	 * time the interval for which the task was registered (see
	 * PeriodicTaskManager::RegisterTask()) has passed and this task
	 * is disabled.
	 *
	 * The default implementation is to do nothing. Implementations of
	 * this task may use this method to detect when they've lost an
	 * operation.
	 *
	 * @note This method is called after
	 *  IncrementDisabledTriggerCount() was called. So unless there
	 *  is an integer overflow, the counter is always > 0 when this
	 *  method is called.
	 */
	virtual void RunWasDisabled() {}
};

class PeriodicTaskInterval : public Link
{
	friend class PeriodicTaskManager;

	int interval;
	double fire_time;
	Head tasks;
};

/** PeriodicTaskManager is a singleton that can be accessed using the
 *  global symbol g_periodic_task_manager.
 */
class PeriodicTaskManager
{
public:
	PeriodicTaskManager();
	~PeriodicTaskManager();

	/** Registers a new periodic task.
	 *
	 * The task will run repeatedly at the specified interval if the message
	 * loop is active, otherwise the task will pause.
	 *
	 * If the messageloop has been inactive longer than the requested interval
	 * the task will run immediately when a new message arrives and then
	 * interval processing resumes from that point in time.
	 *
	 * @param t			The task to run.
	 * @param interval	Interval in milliseconds.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS RegisterTask(PeriodicTask *t, int interval);

	/** Unregisters a periodic task.
	 *
	 * @param t			The task to remove.
	 *
	 * @return OpStatus::OK or OpStatus::ERR if the task was not previously registered.
	 */
	OP_STATUS UnregisterTask(PeriodicTask *t);

	void RunTasks();

private:
	Head intervals;
	double fire_time;
};

#endif // PERIODICTASK_H
