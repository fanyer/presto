/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_ASYNC_IO_H
#define POSIX_ASYNC_IO_H
#ifdef POSIX_OK_ASYNC
/* TODO: simplify and re-implement.
 *
 * See android/src/android_jni_queue.{h,cpp} for a partial start on that.
 *
 * Define a generic TaskQueue base class; derive a message-loop variant of it
 * and a ThreadPool<size> variant; then make g_posix_async an instance of a
 * class based on one or the other, depending on THREAD_SUPPORT.
 *
 * Have the TaskQueue specify an empty Context base-class to be used as
 * Task::Run()'s arg, so that derived classes can pass in relevant thread-local
 * data (an instance of this would typically be created on each thread) that the
 * Task's client couldn't have told it.  Make Queue virtual on TaskQueue, so
 * that it can launch suitable machinery to initiate repeated calls to
 * Process().
 *
 * Replace the specific-type boss with a mindless void*, simplifying Find.  Move
 * m_dns_mutex to module object.  Eliminate type; have an m_live member that's
 * initially true and gets set false by core thread when it wants to exit (or
 * when it discovers it failed to launch a thread).  Have a static Live() method
 * on the leaf class used for g_posix_async, that checks g_opera, g_posix_async
 * and its m_live.
 */

#include "modules/util/simset.h"
#include "platforms/posix/posix_logger.h"

#ifdef THREAD_SUPPORT
# ifndef POSIX_OK_THREAD
#error "You need to import API_POSIX_THREAD to use API_POSIX_ASYNC with threading enabled"
/* Unfortunately, THREAD_SUPPORT is controlled too late in the baseincludes
 * cascade for that to be automated. */
# endif
#include "platforms/posix/posix_mutex.h"
#else
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/mh/mh.h"
#endif

#ifdef PI_ASYNC_FILE_OP
# ifdef POSIX_OK_FILE
#include "modules/pi/system/OpLowLevelFile.h"
#define POSIX_ASYNC_FILE_OP
# endif
#endif // PI_ASYNC_FILE_OP

#ifdef POSIX_OK_DNS
#include "modules/pi/network/OpHostResolver.h"
# if defined(THREAD_SUPPORT) && defined(POSIX_DNS_GETHOSTBYNAME)
#  if ! (defined(POSIX_GHBN_2R7) || defined(POSIX_GHBN_2R6) || defined(POSIX_GHBN_R5) || defined(POSIX_GHBN_R3))
#define POSIX_DNS_LOCKING // Using a gethostbyname variant that isn't thread-safe, in threaded environment.
#  endif
# endif // THREAD_SUPPORT && gethostbyname
#endif // POSIX_OK_DNS

#ifdef POSIX_OK_SOCKET
#include "modules/pi/network/OpSocket.h"
#endif

#ifdef USE_OP_THREAD_TOOLS // for PostSyncMessage
#include "modules/pi/OpThreadTools.h"	// OpThreadTools (g_thread_tools)
#else
#include "modules/hardcore/mh/mh.h"		// MessageHandler (g_main_message_handler)
#endif // USE_OP_THREAD_TOOLS

#define g_posix_async (g_opera->posix_module.GetAsyncManager())

/** Manager for asynchronous workers and threads.
 *
 * Created as part of PosixModule, accessed as g_posix_async.  Mediates choices
 * between using threads, when supported, and message-queue events.  When
 * threaded, manages a pool of threads which it grows progressively, on demand,
 * up to a maximum size (see TWEAK_POSIX_ASYNC_THREADS).  Provides a PendingOp
 * base-class for asynchronous operations it can manage.  Supports queries for
 * pending operations owned by particular clients, optionally dequeueing the
 * operation in the process of returning it.
 */
class PosixAsyncManager
	: public PosixLogger
#ifndef THREAD_SUPPORT
	, public MessageObject
#endif
{
	Head m_pending; ///< List of PendingOp async tasks to handle

#ifdef THREAD_SUPPORT
	Head m_done; ///< List of PendingOp async tasks awaiting deletion.

	/** Array of threads performing asynchronous operations.
	 *
	 * Only the first m_handling of these are actual threads; the rest a just
	 * slots waiting to be filled if demand for threads ever gets high enough to
	 * justify starting new threads.  Each thread simply runs the manager's
	 * Process() method repeatedly until it returns false, upon which the thread
	 * terminates, returning 0 (which is equivalent to calling pthread_exit with
	 * 0 as parameter).
	 */
	THREAD_HANDLE m_handlers[POSIX_ASYNC_THREADS];
	//* Number of threads currently in the array m_handlers.
	int m_handling;

	/** Condition variable signalled when threads have something to do. */
	PosixCondition m_flag;

	/** Mutex to control access to m_done. */
	PosixMutex m_done_flag;
	void DrainDone()
	{
		m_done_flag.Acquire();
		int done = m_done.Cardinal();
		m_done.Clear();
		m_done_flag.Release();
		Log(VERBOSE, "Cleared %d completed async tasks\n", done);
	}
#endif

#if defined(POSIX_DNS_LOCKING) || defined(POSIX_DNS_USE_RES_INIT)
# ifdef POSIX_DNS_LOCKING
	/** Mutex for the DNS workers.
	 *
	 * If we're using a variant of gethostbyname that isn't thread-safe, and we
	 * are using threads, we have to make sure that calls to it (and subsequent
	 * digestion of its results) are handled synchronously.  Since the memory
	 * used by gethostbyname is global, the mutex to lock it must also be
	 * global, so it lives here.  Since Resolver exists in a synchronous flavour
	 * (for the sake of OpHostResolver::ResolveSync and
	 * PosixSystemInfo::ResolveLocalHost), it's not sufficient to provide
	 * PendingDNSOp with access to this mutex.
	 */
	PosixMutex m_dns_mutex; // Acquire/Release
# endif
# ifdef POSIX_DNS_USE_RES_INIT
	/**
	 * Mutex for calling res_init() on platforms where thread-saferes_ninit()
	 * is not available. This mutex is returned by GetResInitMutex().
	 *
	 * See TWEAK_POSIX_DNS_USE_RES_INIT
	 */
	PosixMutex m_resinit_mutex;
# endif
public:
	class Resolver // for use by PosixHostResolver::Resolver
	{
	protected:
# ifdef POSIX_DNS_LOCKING
		PosixMutex& GetDnsMutex() { return g_posix_async->m_dns_mutex; }
# endif
# ifdef POSIX_DNS_USE_RES_INIT
		PosixMutex& GetResInitMutex() { return g_posix_async->m_resinit_mutex; }
# endif
	};
#endif // POSIX_DNS_LOCKING || POSIX_DNS_USE_RES_INIT

	/** Bass class for asynchronous operations.
	 *
	 * Defines the API via which the PosixAsyncManager interacts with pending
	 * operations.  See also PostSyncMessage, which pending operations can use
	 * to communicate back to their initiators.
	 */
	class PendingOp : public Link
	{
		friend class PosixAsyncManager;
	protected:
		/** Identify the type of pending operation.
		 *
		 * General clients should use the SIMPLE type.  Those in need of Find()
		 * support are provided for via the remaining candidate values for
		 * m_type; these may eventually be unified into a common type.
		 */
		const enum Type { SIMPLE, FILE, DNS } m_type;

		/** Join the queue to be ->Run() by an execution thread.
		 *
		 * See constructor.  Call (from main thread) once ready to be ->Run(),
		 * and no sooner; in particular, not until it is completely constructed.
		 * (An instance isn't ready to be ->Run() until it's got its actual
		 * class's vtable, which doesn't happen until its actual class's
		 * constructor gets run - all base classes are run with incomplete
		 * vtables which may include pure virtual methods.)
		 */
		void Enqueue() { g_posix_async->Queue(this); }

	public:
		/** Constructor.
		 *
		 * Leaf derived classes' constructors should end by calling Enqueue();
		 * but only leaves, since otherwise the execution thread may get round
		 * to ->Run() before the main thread has finished constructing !
		 * (Actually witnessed with sockets, Eddy/2007/11/26, on open_1.)
		 *
		 * @param type The type of this pending operation; see m_type and enum Type.
		 */
		PendingOp(Type type) : m_type(type) {}
		virtual ~PendingOp() {}
		PendingOp *Suc() { return static_cast<PendingOp *>(Link::Suc()); }

		/** Implementation of the pending operation.
		 *
		 * The Run() method shall be called when processing resources come
		 * available and the operation has reached the front of the queue.  The
		 * pending operation is dequeued before invoking Run() and deleted once
		 * it has completed.  If your asynchronous operation needs data that
		 * must out-last this, it should be associated with a persistent object,
		 * to hold that data, which remains attached to the asynchronous
		 * operation's invoker.  See also: TryLock().
		 */
		virtual void Run() = 0;

		/** Test whether it's safe to still Run, and lock if so.
		 *
		 * After the work item is removed from the work queue, the queue is
		 * unlocked before the item is Run().  Its Boss() might then Find() that
		 * it has no queued worker and conclude that it can safely destroy data
		 * referenced by the worker.  To prevent this, we TryLock() before we
		 * Out() the worker.  This should fail if Boss() is in the process of
		 * destruction, or has otherwise decided it does not wish to continue
		 * with the work item.  Otherwise, it should lock relevant data so that
		 * Boss() shall know to not delete it.  Run() should arrange to check
		 * (both when entered and, if it takes significant time, at intervals
		 * subsequently) for Boss() having decided, since TryLock() was called,
		 * to abort.
		 *
		 * Note that TryLock() is allowed to block, if Boss() is happy to let
		 * the work item go ahead; however, it should not block on any operation
		 * of the async manager, as this is locked and shall not be released
		 * until after TryLock() returns.  Even if TryLock() fails, the
		 * PendingOp is dequeued and destroyed, so client code should either
		 * only have it fail if it is now redundant or remember failing it so as
		 * to know, once the needed data is available again, to launch a fresh
		 * PendingOp to do its job.
		 *
		 * @return False if it's not safe to Run(); otherwise, true and relevant
		 * data has been locked, so that it can't be deleted until Run() is called.
		 */
		virtual bool TryLock() = 0;
	}; // entries in the m_pending list

	PendingOp* FirstPending() { return static_cast<PendingOp *>(m_pending.First()); }
	bool HasPending() const { return !m_pending.Empty(); }

	template <class T, PendingOp::Type type>
	class PendingBossOp : public PendingOp
	{
		T* const m_boss;
	public:
		T* Boss() const { return m_boss; }
		/** Constructor.
		 *
		 * @param boss The associated object, which can later be used with
		 * Find() to retrieve (and optionally dequeue - but see TryLock() for
		 * complications) this PendingBossOp.
		 */
		PendingBossOp(T* boss)
			: PendingOp(type), m_boss(boss) { OP_ASSERT(boss); }
	};

public:
#ifdef POSIX_ASYNC_FILE_OP
	/** Pending File I/O operation.
	 *
	 * Variant on PendingOp to support PosixLowLevelFile.
	 */
	typedef PendingBossOp<OpLowLevelFile, PendingOp::FILE> PendingFileOp;
#endif // POSIX_ASYNC_FILE_OP

#ifdef POSIX_OK_DNS
	/** Pending DNS lookup operation.
	 *
	 * Variant on PendingOp to support PosixHostResolver.
	 */
	typedef PendingBossOp<OpHostResolver, PendingOp::DNS> PendingDNSOp;
#endif // POSIX_OK_DNS

public:
	/* Constructor.
	 *
	 * Should only be invoked by PosixModule's InitL(): if you need an instance,
	 * access the global one as g_posix_async.
	 */
	PosixAsyncManager()
		: PosixLogger(ASYNC)
#ifdef THREAD_SUPPORT
		, m_handling(0)
#endif
#ifdef POSIX_DNS_LOCKING
		, m_dns_mutex(PosixMutex::MUTEX_NORMAL)
#endif
#ifdef POSIX_RESINIT_LOCKING
		, m_resinit_mutex(PosixMutex::MUTEX_NORMAL)
#endif
		{ OP_ASSERT(g_opera && !g_posix_async); }
	~PosixAsyncManager();

#ifndef THREAD_SUPPORT
	OP_STATUS Init()
	{
		return g_main_message_handler->SetCallBack(this,
												   MSG_POSIX_ASYNC_TODO,
												   (MH_PARAM_1)this);
	}
#endif

	/** Returns true if the instance is still handling new operations. */
	bool IsActive() const { return m_handling >= 0; }

	/** Add a PendingOp to the queue of tasks for the threads to perform.
	 *
	 * May also start threads (if available) or post messages (otherwise).
	 *
	 * @note This method is called on the core-thread.
	 *
	 * @param whom The PendingOp to be added to the queue.
	 */
	void Queue(PendingOp *whom);

	/** Find first pending operation, if any, for a given client.
	 *
	 * @param whom Low-level file, host resolver or socket object whose operation is sought.
	 * @param out TRUE if the operation should be dequeued before return, else FALSE.
	 * @return First matching pending operation found, else a NULL pointer.
	 */
#ifdef POSIX_ASYNC_FILE_OP
	PendingFileOp * Find(const OpLowLevelFile *whom, bool out);
#endif
#ifdef POSIX_OK_DNS
	/**
	 * @overload
	 */
	PendingDNSOp * Find(const OpHostResolver *whom, bool out);
#endif

#ifdef THREAD_SUPPORT
	/** Perform one queued operation, if any available.
	 *
	 * If a pending operation is available (and it is allowed to run - see
	 * PendingOp::TryLock()), PendingOp::Run() is called and the pending
	 * operation is added to the list of finished operations (see #m_done).
	 * If no pending operation is available, it waits until a new operation is
	 * enqueued (see Queue()).
	 *
	 * @note This method is called continuously by possibly multiple threads
	 *  after (at least) one PendingOp has been enqueued (see Queue()).
	 *
	 * @return True if the thread wants to continue processing the next
	 *  PendingOp.
	 */
	bool Process();
#else
	// MessageObject API:
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 one, MH_PARAM_2 two);
#endif

	/** Convenience method.
	 *
	 * Posts a message from a PendingOp (which may be on a non-main thread) via
	 * the global OpThreadTools or MessageHandler, as appropriate.  Parameters
	 * are as for the the global main message handler's PostMessage() method.
	 *
	 * @param msg Message to send.
	 * @param one First parameter, usually the intended recipient.
	 * @param two Optional second parameter, usually a data value; defaults to zero.
	 * @param delay If the specified value is not 0, the message will not be
	 *  delivered to the recipient before the specified time in ms has passed.
	 */
	static OP_STATUS PostSyncMessage(OpMessage msg, MH_PARAM_1 one, MH_PARAM_2 two=0, unsigned long delay=0)
	{
#ifdef USE_OP_THREAD_TOOLS
		return g_thread_tools
			? g_thread_tools->PostMessageToMainThread(msg, one, two, delay)
			: OpStatus::ERR_NULL_POINTER;
#else
		return g_main_message_handler
			? (g_main_message_handler->PostMessage(msg, one, two, delay)
			   ? OpStatus::OK : OpStatus::ERR)
			: OpStatus::ERR_NULL_POINTER;
#endif
	}
};
#endif // POSIX_OK_ASYNC
#endif // POSIX_ASYNC_IO_H
