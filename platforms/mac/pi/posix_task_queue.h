#ifndef POSIX_TASK_QUEUE_H
#define POSIX_TASK_QUEUE_H

#include "adjunct/desktop_pi/DesktopTaskQueue.h"

class PosixTaskQueue : public DesktopTaskQueue
{
public:
	PosixTaskQueue();
	virtual ~PosixTaskQueue();
	virtual void AddTask(OpTask* task);
	virtual void HandleSynchronousTask(OpTask* task);
	virtual void HandleSynchronousTaskWhenDone(OpTask* task);
private:
	void* Run();
	static void* static_run(void*);
	OpTask* GetNextTask();
	OpVector<OpTask> tasks;
	pthread_mutex_t mutex;
	pthread_cond_t task_added;
	pthread_cond_t work_completed;
	pthread_t thread;
	bool exit;
};

#endif // POSIX_TASK_QUEUE_H
