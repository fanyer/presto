#ifndef SYNCHRONOUS_TASK_QUEUE_H
#define SYNCHRONOUS_TASK_QUEUE_H

#include "adjunct/desktop_pi/DesktopTaskQueue.h"

/** Sample of a synchronous task queue. Use at your own risk.
  */

class SynchronousTaskQueue : public DesktopTaskQueue
{
public:
	virtual void AddTask(OpTask* task) { task->Execute(); OP_DELETE(task); }
	virtual void HandleSynchronousTask(OpTask* task) { task->Execute(); OP_DELETE(task); }
	virtual void HandleSynchronousTaskWhenDone(OpTask* task) { task->Execute(); OP_DELETE(task); }
};

#endif // SYNCHRONOUS_TASK_QUEUE_H
