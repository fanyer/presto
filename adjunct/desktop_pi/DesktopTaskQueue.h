#ifndef DESKTOP_TASK_QUEUE_H
#define DESKTOP_TASK_QUEUE_H

class OpTask
{
public:
	virtual ~OpTask() {}
	virtual void Execute() = 0;
};

/** DesktopTaskQueue is a pi class that conceptually is an execution queue. Tasks added must be executed one after the
  * other, in the order given. UNDER NO CIRCUMSTANCE can tasks on the same queue be executed simultaneously.
  * In most cases, that is if you have underutilised porcessor cores, you'd make a new thread.
  * In that case you would add the OpTasks to a mutex_lock'ed vector or something have the thread pull them out
  * and Execute() them (the mutex lock is there, of course, to prevent adding and removing objects at the same time).
  *
  * You are not required to do this, however. If all processors are busy or if you are on a single-core
  * machine, it is perfectly acceptable to have the Make function return a SynchronousTaskQueue
  * (see desktop_util/SychronousTaskQueue.h). The synchronous task queue will simply call
  * task->Execute() inline rather than doing anything fancy. There is NOTHING wrong with the synchronous queue, and
  * it is probably the most efficient if the machine is strapped for resources. Having heurisics for when to use it
  * will probably be more efficient than creating more threads than the machine can handle.
  *
  * For instance, if the machine has 1 dual-core processor, with no other tasks really doing much, you might want to
  * return a threaded task queue at the first call to DesktopTaskQueue::Make and SynchronousTaskQueue after that until
  * the threaded queue has finished. Systems vary a lot, though, so profile.
  *
  * If you ARE implementing a proper threaded queue it might be worthwile to check out the pthread'ed implementation
  * in platforms/mac/pi/posix_task_queue.h. Most threading implentations are pretty close, so it might just be a matter of renaming a few
  * functions.
  */

class DesktopTaskQueue
{
public:
	static OP_STATUS Make(DesktopTaskQueue** new_queue);

	/** Destroy the queue. When this function returns, all tasks MUST be completed.
	  * essentially, the main thread must join with the queue thread.
	  */
	virtual ~DesktopTaskQueue() {}

	/** Add the given task to the queue to be executed. All tasks added this way MUST be executed in the
	  * order given, although the timing is up to the implentor.
	  * You are responsible for deleting the task when it has been dealt with.
	  */
	virtual void AddTask(OpTask* task) = 0;

	/** Execute this task as soon as possible (waiting for any currently executing task to finish).
	  * This is needed for tasks that need to return a status code to the caller.
	  * DO NOT RETURN UNTIL THE TASK HAS COMPLETED.
	  * You are responsible for deleting the task when it has been dealt with.
	  * Only use this call if you have to: It really hurts performance.
	  */
	virtual void HandleSynchronousTask(OpTask* task) = 0;

	/** Wait for the entire queue of tasks to finish, then execute the given task.
	  * This is needed for tasks that not only need to report completion status to caller, but is also
	  * dependant on the thread's state matching that of the caller.
	  * DO NOT RETURN UNTIL EVERYTHING HAS COMPLETED.
	  * You are responsible for deleting the task when it has been dealt with.
	  * Only use this call if you have to: It really hurts performance.
	  */
	virtual void HandleSynchronousTaskWhenDone(OpTask* task) = 0;
};

#endif // DESKTOP_TASK_QUEUE_H
