/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (based on unix/base/common/mt.h by Morten Stenshorne).
 *
 * Note that using helgrind [1] may be constructive in threaded code.
 * [1] http://valgrind.org/docs/manual/hg-manual.html
 */
#ifndef POSIX_THREAD_UTIL_H
#define POSIX_THREAD_UTIL_H __FILE__
# ifdef POSIX_OK_THREAD

#include "platforms/posix/posix_mutex.h"

#include <pthread.h>
#include <semaphore.h>

typedef pthread_t THREAD_HANDLE; // used by memtools, probetools and some selftests.

/** Namespace providing basic POSIX thread operations.
 *
 * All methods are static; this is only a class for the sake of compatibility
 * with compilers that lack support for namespace.  Some debug functionality in
 * core uses it; and various platforms use it.  There is, however, no porting
 * interface.
 */
class PosixThread
{
public:
	/** Create thread
	 *
	 * @param entry The function (entry-point) to call, as entry(argument), in
	 * the new thread; when it returns, the thread terminates.
	 * @param argument The opaque datum to pass to entry().
	 * @return Identifier for the newly created thread on success; or
	 * THREAD_HANDLE_NULL on failure.
	 */
	static THREAD_HANDLE CreateThread(void* (*entry)(void*), void* argument);

	/** Return the thread identifier for the calling thread. */
	static THREAD_HANDLE Self() { return pthread_self(); }

	/** Wait for the given thread to terminate, forward its return value. */
	static int Join(THREAD_HANDLE handle) { return pthread_join(handle, NULL); }

	/** Detach and terminate the given thread. */
	static int Detach(THREAD_HANDLE handle) { return pthread_detach(handle); }

#ifdef POSIX_ASYNC_CANCELLATION
	/**
	 * @see SetCancelState(), Cancel()
	 */
	enum CancellationState {
		/** The thread is cancelable. This is the default state. */
		CANCEL_ENABLE = PTHREAD_CANCEL_ENABLE,
		/** The thread is not cancelable. If a cancellation request is received,
		 * it is blocked until cancelability is enabled. */
		CANCEL_DISABLE = PTHREAD_CANCEL_DISABLE
	};

	/** Sets the cancelability state of the calling thread to the specified
	 * state.
	 * @param state the new state.
	 * @return On success, these functions return 0; on error, they return a
	 *  nonzero error number.
	 * @retval EINVAL Invalid value for state.
	 * @see Cancel()
	 */
	static int SetCancelState(enum CancellationState state) { int o; return pthread_setcancelstate(state, &o); }
	static int EnableCancellation() { return SetCancelState(CANCEL_ENABLE); }
	static int DisableCancellation() { return SetCancelState(CANCEL_DISABLE); }

	/** Send a cancellation request to the specified thread.
	 * Whether and when the target thread reacts to the cancellation request
	 * depends on its CancelabilityState.
	 *
	 * @note It also depends on the cancelability type (see man
	 *  pthread_setcanceltype), but currently there is no use-case to support
	 *  any other cancelability type than the default.
	 *
	 * If a thread has disabled its CancelabilityState (see SetCancelState()),
	 * then a cancellation request remains queued until the thread enables
	 * cancellation. If a thread has enabled cancellation (and the
	 * cancelability type is the default type), then the thread is cancelled
	 * when it next calls a function that is a cancellation point. Posix system
	 * functions like sleep(), recv(), send(), poll(), select() are
	 * e.g. cancellation points.
	 *
	 * After a canceled thread has terminated, a PosixThread::Join() with that
	 * thread returns PTHREAD_CANCELED as the thread's exit status.
	 *
	 * @note The return status of this function informs the caller whether the
	 *  cancellation request was successfully queued. To get the return value of
	 *  the cancelled thread or to know that the cancelled thread is no longer
	 *  running, you need to call PosixThread::Join().
	 *
	 * @return 0 On success, a nonzero error number in case of an error. The
	 *  thread is terminated asynchronously.
	 * @retval ESRCH No thread with the ID thread could be found.
	 *
	 * Example thread function which changes its cancelability state:
	 * @code
	 * static void* thread_func(void*)
	 * {
	 *     printf("Started thread\n");
	 *     enum PosixThread::CancellationState ignore_old_state;
	 *
	 *     // Disable cancellation
	 *     if (0 == PosixThread::SetCancelState(PosixThread::CANCEL_DISABLE, ignore_old_state))
	 *     {
	 *         printf("Disabled cancelability\n");
	 *         .... // do something that may not be cancelled
	 *         printf("Enable cancelability\n");
	 *         PosixThread::SetCancelState(PosixThread::CANCEL_ENABLE, ignore_old_state);
	 *     }
	 *     printf("Sleeping ...\n");
	 *     sleep(1000);
	 *     printf("Not canceled!\n");
	 *     return NULL;
	 * }
	 * @endcode
	 * Example main thread that tries to cancel the thread_func:
	 * @code
	 * // start a new thread with the above thread_func:
	 * PosixThread::THREAD_HANDLE handle =
	 *     PosixThread::CreateThread(thread_func, 0);
	 * if (handle == THREAD_HANDLE_NULL) exit(1);
	 * sleep(2);
	 * printf("Sending cancellation request\n");
	 * if (PosixThread::Cancel(handle) != 0)
	 * {
	 *     printf("Error cancelling thread\n");
	 *     exit(1);
	 * }
	 * int result = PosixThread::Join(handle);
	 * printf("Joined thread with result %d\n", result);
	 * @endcode
	 * @see SetCancelState()
	 * @see TestCancel()
	 */
	static int Cancel(THREAD_HANDLE handle) { return pthread_cancel(handle); }

	/** Check if a cancellation of the caller's thread was requested.
	 * By calling this function the calling thread creates a possible
	 * cancellation point. If cancelability is enabled (see SetCancelState())
	 * and a cancellation request was queued for this thread (see Cancel()), the
	 * thread may be terminated. If cancelability is disabled or no cancellation
	 * is requested for the thread, then this function has no effect.
	 */
	static void TestCancel() { pthread_testcancel(); }
#endif // POSIX_ASYNC_CANCELLATION

	/** Test whether a given thread handle has the reserved not-a-thread handle value. */
	static bool IsNullThread(THREAD_HANDLE handle)
#ifdef __hpux__
		{ return !op_memcmp(&handle, &cma_c_null, sizeof(pthread_t)); }
# define THREAD_HANDLE_NULL cma_c_null
#else
		{ return handle == 0; }
# define THREAD_HANDLE_NULL 0
#endif
};

/** Implement a condition variable.
 *
 * A condition variable can be used to suspend threads until some condition is
 * true. The use-cases for a condition variable are
 * - Wait until the condition is true (see Wait(), TimedWait()) and thus suspend
 *   the thread until some other thread signals the condition.
 * - Signal the condition (see Wake(), WakeAll())
 *
 * This is derived from PosixMutex so that the condition variable can be
 * associated with a recursive mutex.  You can't access the mutex directly.  You
 * don't need to Grab() or Give() in order to Wake(), Wait() or similar; the
 * methods that need it deal with the mutex for themselves.
 *
 * Following each call to Ask() that returns true; and each call to Grab(): you
 * *must* subsequently make a matching call to Give() to surrender the lock.
 */
class PosixCondition : private PosixMutex
{
	pthread_cond_t m_cond;
#ifdef DEBUG_ENABLE_OPASSERT
	int m_locked;
#endif
	PosixCondition(const PosixCondition&); // suppress copy-construction
	PosixCondition &operator =(const PosixCondition&); // suppress copy-assignment

public:
	PosixCondition();
	virtual ~PosixCondition();

	void WakeAll(); ///< Let anyone else have the mutex.
	void Wake(); ///< Let someone else have the mutex.

	/** Wait for some other thread to Wake() us.
	 *
	 * The most common use-case is to
	 * -# Grab() the mutex
	 * -# test some condition that is shared between the thread which may Wake()
	 *    us and the waiting thread,
	 * -# based on that condition decide if we should start to wait for some
	 *    other thread to Wake() us.
	 * -# Give() the mutex.
	 *
	 * In this use-case the argument \p grab should be \c false. Usually it is
	 * necessary to use a condition that is shared between the waking thread and
	 * the waiting thread to synchronise two threads.
	 *
	 * Example: exchange or synchronise some data between two threads:
	 * \code
	 * // shared variables:
	 * PosixCondition condition;
	 * void* data = NULL;
	 * ...
	 *
	 * // thread 1: providing some data
	 * condition.Grab();
	 * if (!data)
	 * {
	 *     data = some_data;
	 *     condition.Wake();
	 * }
	 * condition.Give();
	 * ...
	 *
	 * // thread 2: working with the data if available
	 * while (thread_is_running)
	 * {
	 *     void* received_data;
	 *     condition.Grab();
	 *     if (!data) condition.Wait(false);
	 *     received_data = data;
	 *     data = NULL;
	 *     condition.Give();
	 *     if (received_data) DoSomethingWith(received_data);
	 * }
	 * \endcode
	 *
	 * @param grab Should usually be false. In that case the caller must Grab()
	 *        this condition before calling this method (i.e., lock the
	 *        condition's mutex) and after this method returns to the caller,
	 *        the caller must Give() the condition.
	 *        In the rare case where no additional synchronisation is necessary,
	 *        the caller may use the value true. In that case Wait() takes care
	 *        of calling Grab() and Give().
	 */
	void Wait(bool grab);

	/** Wait, but no longer than delay ms.
	 *
	 * The most common use-case is to
	 * -# Grab() the mutex
	 * -# test some condition that is shared between the thread which may Wake()
	 *    us and the waiting thread,
	 * -# based on that condition decide if we should start to wait for some
	 *    other thread to Wake() us (but no longer than \p delay ms).
	 * -# Give() the mutex.
	 *
	 * In this use-case the argument \p grab should be \c false. Usually it is
	 * necessary to use a condition that is shared between the waking thread and
	 * the waiting thread to synchronise two threads.
	 *
	 * Example: exchange or synchronise some data between two threads:
	 * \code
	 * // shared variables:
	 * PosixCondition condition;
	 * void* data = NULL;
	 * ...
	 *
	 * // thread 1: providing some data
	 * condition.Grab();
	 * if (!data)
	 * {
	 *     data = some_data;
	 *     condition.Wake();
	 * }
	 * condition.Give();
	 * ...
	 *
	 * // thread 2: working with the data if available
	 * while (thread_is_running)
	 * {
	 *     void* received_data;
	 *     condition.Grab();
	 *     if (!data)
	 *         condition.TimedWait(754, false); // wait no longer than 754 ms
	 *     received_data = data;
	 *     data = NULL;
	 *     condition.Give();
	 *     if (received_data) DoSomethingWith(received_data);
	 * }
	 * \endcode
	 *
	 * @param grab Should usually be false. In that case the caller must Grab()
	 *        this condition before calling this method (i.e., lock the
	 *        condition's mutex) and after this method returns to the caller,
	 *        the caller must Give() the condition.
	 *        In the rare case where no additional synchronisation is necessary,
	 *        the caller may use the value true. In that case Wait() takes care
	 *        of calling Grab() and Give().
	 *
	 * @param delay Maximum number of milliseconds to wait.
	 * @return True precisely iff some other thread did Wake() us; else we timed out.
	 */
	bool TimedWait(int delay, bool grab);

	/// Try to lock; return true on success; doesn't block.
	bool Ask() { bool ans = Request(); OP_ASSERT(!ans || ++m_locked > 0); return ans; }
	void Grab() { Acquire(); OP_ASSERT(++m_locked > 0); } ///< Lock (may block)
	void Give() { OP_ASSERT(m_locked-- > 0); Release(); } ///< Unlock

#ifdef POSIX_ASYNC_CANCELLATION
	/**
	 * When you use a PosixCondition on a cancelable thread you should push this
	 * cleanup handler using pthread_cleanup_push() to the thread's stack of
	 * thread-cancellation clean-up handlers. Thus the mutex associated to the
	 * PosixCondition can be released if the thread is cancelled.
	 *
	 * Example:
	 * @code
	 * PosixCondition* condition = ...;
	 * condition->Grab();
	 * pthread_cleanup_push(PosixCondition::CleanupGive, condition);
	 * ...
	 * // do something, if this thread is cancelled, the cleanup handler
	 * // will be executed and give the condition.
	 * ...
	 * pthread_cleanup_pop(0); // don't execute the handler
	 * condition->Give();
	 * @endcode
	 *
	 * @param condition is the PosixCondition instance to release.
	 * @see PosixThread::Cancel()
	 */
	static void CleanupGive(void* condition)
	{
		if (condition) static_cast<PosixCondition*>(condition)->Give();
	}
#endif // POSIX_ASYNC_CANCELLATION

#ifdef DEBUG_ENABLE_OPASSERT
	/**
	 * When enabling debug messages, we use this helper class to monitor the
	 * owner and ref-counter of the associated mutex before calling any of the
	 * pthread_cond_* functions. Create an instance of this class before calling
	 * any pthread_cond_* function. The constructor resets the owner and depth
	 * of the mutex to 0. The destructor restores the original values.
	 */
	class AssertOwner
	{
		PosixCondition* condition;
		pthread_t owned;
		int deep;

	public:
		AssertOwner(PosixCondition* c)
			: condition(c), owned(c->m_owner), deep(c->m_depth)
		{
			if (owned == PosixThread::Self() && deep > 0)
			{
				condition->m_depth = 0;
				condition->m_owner = THREAD_HANDLE_NULL;
			}
			else
				OP_ASSERT(!"Condition variable's un/locking has been done "
						  "inconsistently. This condition is expected to be "
						  "locked by the calling thread. Check which thread "
						  "owns the condition and why it is locked or not "
						  "locked.");
		}

		~AssertOwner() { Reset(); }
		void Reset()
		{
			if (owned == PosixThread::Self() && deep > 0)
			{
				OP_ASSERT(condition->m_depth == 0 && condition->m_owner == THREAD_HANDLE_NULL);
				condition->m_depth = deep;
				condition->m_owner = owned;
			}
		}

# ifdef POSIX_ASYNC_CANCELLATION
		static void CleanupHandler(void* arg)
		{
			AssertOwner* assert_owner = reinterpret_cast<AssertOwner*>(arg);
			assert_owner->Reset();
		}
# endif // POSIX_ASYNC_CANCELLATION
	};
	friend class AssertOwner;
#endif // DEBUG_ENABLE_OPASSERT
};

#ifdef POSIX_ASYNC_CANCELLATION
/** Lock a mutex safely for cancellation.
 *
 * Acquire the mutex and push an entry onto the thread's cleanup handler stack
 * which shall release the mutex if the thread is cancelled. This must be paired
 * with POSIX_MUTEX_UNLOCK(mutex) in the same context level to unlock the
 * mutex.
 *
 * @note POSIX_MUTEX_LOCK and POSIX_MUTEX_UNLOCK use '{' and '}' to
 *  organize their code. So any variable declared between these two macros will
 *  not be visible after POSIX_MUTEX_UNLOCK. If a value obtained between locking
 *  and unlocking shall be needed after unlocking, declare the variable to store
 *  it in before locking.
 *
 * @param mutex is a pointer to the PosixMutex instance to lock. */
#  define POSIX_MUTEX_LOCK(mutex)								\
	(mutex)->Acquire();											\
	pthread_cleanup_push(PosixMutex::CleanupRelease, mutex)

/** Cancellation-safe unlocking of a mutex.
 *
 * Pop the cleanup handler from the thread's cleanup handler stack and execute
 * it to releases the mutex. This must be paired with a POSIX_MUTEX_LOCK() on
 * the same context level.
 * @param mutex is a pointer to the PosixMutex instance to unlock. */
#  define POSIX_MUTEX_UNLOCK(mutex) pthread_cleanup_pop(1)

/** Push a cleanup handler to the thread's cleanup handler stack. The specified
 * handler is executed if the thread is cancelled. This must be paired with
 * POSIX_CLEANUP_POP() in the same context level. This macro can e.g. be used to
 * free some memory that was allocated by the thread.
 *
 * @note POSIX_CLEANUP_PUSH and POSIX_CLEANUP_POP use '{' and '}' to
 *  organize their code. So any variable declared between these two macros will
 *  not be visible after POSIX_CLEANUP_POP. If a value obtained between pushing
 *  a cleanup handler and the corresponding pop shall be needed after the pop,
 *  declare the variable to store it in before pushing.
 *
 * @param handler is the cleanup handler function of type
 *  void (*handler)(void* arg), the specified arg is passed to this function
 *  when it is invoked.
 * @param arg is the void* argument that is passed to handler if the function
 *  is invoked. */
#  define POSIX_CLEANUP_PUSH(handler, arg) pthread_cleanup_push(handler, arg)

/** Pop a cleanup handler from the thread's cleanup handler stack without
 * invoking it. */
#  define POSIX_CLEANUP_POP() pthread_cleanup_pop(0)

/** Grab a condition variable safely against cancellation.
 *
 * Grab the specified condition and push a cleanup handler (to unlock the
 * condition) to the thread's cleanup handler stack. The cleanup handler is
 * executed if the thread is cancelled. This macro must be pared with
 * POSIX_CONDITION_GIVE() in the same context level.
 *
 * @note POSIX_CONDITION_GRAB and POSIX_CONDITION_GIVE use '{' and '}' to
 *  organize their code. So any variable declared between these two macros will
 *  not be visible after POSIX_CONDITION_GIVE. If a value obtained between grab
 *  and give shall be needed after the give, declare the variable to store it in
 *  before grabbing.
 *
 * Example:
 * @code
 * int MyFunction(PosixCondition* condition)
 * {
 *     int result = 0;
 *     {
 *         POSIX_CONDITION_GRAB(condition);
 *         result = ...;
 *         POSIX_CONDITION_GIVE(condition);
 *     }
 *     return result;
 * }
 * @endcode
 *
 * @param condition is a pointer to the PosixCondition instance to grab.
 */
#  define POSIX_CONDITION_GRAB(condition)							\
	(condition)->Grab();											\
	pthread_cleanup_push(PosixCondition::CleanupGive, condition)

/** Cancellation-safe release of a condition variable.
 *
 * Unlock a PosixCondition instance that was grabbed with the macro
 * POSIX_CONDITION_GRAB().
 * @param condition is a pointer to the PosixCondition instance to unlock.
 */
#  define POSIX_CONDITION_GIVE(condition)							\
	pthread_cleanup_pop(0);											\
	(condition)->Give()

/** The cancellation macros seem to be implemented using setjmp/longjmp and
 * because setjmp/longjmp may be implemented in a way such that the values of
 * local variables at the setjmp call are not preserved after the call returns
 * from longjmp. So all local variables that are declared before using
 * POSIX_MUTEX_LOCK, POSIX_CLEANUP_PUSH or POSIX_CONDITION_GRAB and which are
 * used after such a macro, need to be declared with POSIX_THREAD_VOLATILE to
 * avoid the compiler warning "variable may be clobbered by vfork or longjmp".
 *
 * Example:
 * @code
 * POSIX_THREAD_VOLATILE int foo = 1;
 * const char * POSIX_THREAD_VOLATILE some_text = "Hallo World";
 * @endcode
 */
#  define POSIX_THREAD_VOLATILE OP_MEMORY_VAR

#else // !POSIX_ASYNC_CANCELLATION
/* If we use threads without cancellation support, we only need to lock, unlock,
 * grab and give: */
#  define POSIX_MUTEX_LOCK(mutex)			(mutex)->Acquire()
#  define POSIX_MUTEX_UNLOCK(mutex)			(mutex)->Release()
#  define POSIX_CLEANUP_PUSH(handler, arg)
#  define POSIX_CLEANUP_POP()
#  define POSIX_CONDITION_GRAB(condition)	(condition)->Grab()
#  define POSIX_CONDITION_GIVE(condition)	(condition)->Give()
#  define POSIX_THREAD_VOLATILE
#endif // POSIX_ASYNC_CANCELLATION

# else // !POSIX_OK_THREAD
/* If we don't use threads these macros can remain empty, because no mutex
 * requires locking and no thread can be cancelled.
 * Note that, in so far as member variables that only exist #ifdef
 * POSIX_OK_THREAD are referenced only via these macros, these macros save code
 * a bunch of #if-ery on POSIX_OK_THREAD, when referencing such members; when
 * POSIX_OK_THREAD isn't defined, their apparent reference to the undeclared
 * members doesn't actually reference them. */
#  define POSIX_MUTEX_LOCK(mutex)
#  define POSIX_MUTEX_UNLOCK(mutex)
#  define POSIX_CLEANUP_PUSH(handler, arg)
#  define POSIX_CLEANUP_POP()
#  define POSIX_CONDITION_GRAB(condition)
#  define POSIX_CONDITION_GIVE(condition)
#  define POSIX_THREAD_VOLATILE
# endif // POSIX_OK_THREAD
#endif // POSIX_THREAD_UTIL_H
