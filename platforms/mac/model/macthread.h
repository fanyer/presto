#if !defined(MACTHREAD_H)
#define MACTHREAD_H

#define USE_PTHREADS

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

/** A simple thread class which implements in an encapsulated style the Carbon
  * Multiprocessing services. The use of this class is to create a UNIX style
  * thread which runs preemptively. Generally 'Threads' on a Macintosh refers
  * to a cooperative multitasking set of functions. This thread class however
  * implements a proper preemptive thread which can even run on a seperate
  * processor. */
class MacThread
{
public:
	MacThread();

	virtual ~MacThread();

	/** Starts the thread and sets isRunning to true */
	OSStatus start();

	/** Terminates the thread in a 'Kill' fashion. and sets the isRunning
	  * status to false. */
	void terminate();

	/** The thread function. This should be inherited and implemented
	  * the return value of this function doesn't matter at this time,
	  * so it is best just to return noErr */
	virtual OSStatus run() = 0;

	/** Statis thread running function. Uses parameter in order to forward
	  * this call the the thread's run() function. Parameter will be equal to
	  * the 'this' value of the class instance which it belongs. */
	static OSStatus sRun(void *parameter);

protected:
#ifdef USE_PTHREADS
	pthread_t thread;
#else
	MPTaskID task;
	MPQueueID notifyQueue;
#endif
	bool isRunning;
};

#endif
