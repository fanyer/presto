#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "modules/hardcore/timer/optimer.h"


class OpScheduledTask;

/**
 * OpScheduledTaskListener
 *
 * Listener class for OpScheduledClass.
 *
 * Subclass the OpScheduledTaskListener class and
 * implement OnTaskTimeOut(), which is called when the task times out.
 *
 */

class OpScheduledTaskListener
{
public:
	virtual ~OpScheduledTaskListener() {}
	virtual void OnTaskTimeOut(OpScheduledTask* task) = 0;
};


/**
 * OpScheduledTask
 *
 * To add a task, make an instance of OpScheduledTask.
 * Call InitTask() with a unique task name to initialize the task.
 * Call AddScheduledTask() with timeout to add the scheduled task.
 *
 */

class OpScheduledTask : public OpTimerListener
{
public:
	OpScheduledTask();
	virtual ~OpScheduledTask();
	
	OP_STATUS InitTask(const char* task_name);
	
	OP_STATUS AddScheduledTask(time_t timeout);
	OP_STATUS SetLastTimeout(time_t last_timeout);
	
	OpString8*	GetTaskName() { return &m_task_name; }
	time_t		GetTimeout() const { return m_timeout; }
	
	void		SetListener(OpScheduledTaskListener* listener) { m_listener = listener; }
	
private:
	void		OnTimeOut(OpTimer* timer);
	
	OpString8					m_task_name;
	time_t						m_timeout;
	OpTimer*					m_timer;
	OpScheduledTaskListener*	m_listener;
};


/**
 * OpTaskScheduler
 *
 * Class for managing scheduling of tasks.
 *
 * The timeout value and time of last timeout are written to the file tasks.ini in the preferences folder, 
 * so the scheduling of tasks will continue after you quit and restart Opera.
 *
 */

class PrefsFile;

#define g_task_scheduler (OpTaskScheduler::GetInstance())

class OpTaskScheduler
{
public:
	OpTaskScheduler();
	~OpTaskScheduler();

	static OpTaskScheduler* GetInstance();
	
	void Cleanup();

	OP_STATUS AddScheduledTask(OpScheduledTask* scheduled_task);
	OP_STATUS RemoveScheduledTask(OpScheduledTask* scheduled_task);

	OP_STATUS SetTimeout(OpScheduledTask* scheduled_task, time_t timeout);
	
	OP_STATUS SetLastTimeout(OpScheduledTask* scheduled_task, time_t last_timeout);
	OP_STATUS GetLastTimeout(OpScheduledTask* scheduled_task, time_t& last_timeout);
	
private:
	PrefsFile* CreateTaskFile(const uni_char *filename, OpFileFolder folder);

	PrefsFile*								m_prefs_file;
	OpString8HashTable<OpScheduledTask>*	m_tasks;
};

#endif // TASK_SCHEDULER_H
