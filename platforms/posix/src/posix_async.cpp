/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_ASYNC
#include "platforms/posix/src/posix_async.h"

PosixAsyncManager::~PosixAsyncManager()
{
#ifdef THREAD_SUPPORT
	OP_ASSERT(g_opera->posix_module.OnCoreThread());
	Log(NORMAL,
		"Harvesting %d/%d threads used for asynchronous operations.\n",
		m_handling, POSIX_ASYNC_THREADS);

	/* Transfer all pending tasks to a local list, so we can drain the list of
	 * pending jobs after terminating all threads. */
	m_flag.Grab();
	Head pending;
	pending.Append(&m_pending);

	// tell the worker threads to stop:
	int threads = m_handling;
	m_handling = -1;
	m_flag.WakeAll();
	m_flag.Give();

# ifdef POSIX_ASYNC_CANCELLATION
	Log(VERBOSE, "Cancelling threads\n");
	for (int current = 0; current < threads; ++current)
	{
		int res = PosixThread::Cancel(m_handlers[current]);
		Log(VERBOSE, "Send cancelling request for thread[%d]: %p (%d)\n",
			current, m_handlers[current], res);
	}
# endif // POSIX_ASYNC_CANCELLATION
#else // !THREAD_SUPPORT
	g_main_message_handler->UnsetCallBacks(this);
#endif // THREAD_SUPPORT

	// Drain queue:
	Log(CHATTY,
		"Draining queue of %d pending asynchronous operations.\n",
		pending.Cardinal());
	while (PendingOp* job = static_cast<PendingOp*>(pending.First()))
	{
		job->Out();
		if (job->TryLock())
			job->Run();
		OP_DELETE(job);
	}

#ifdef THREAD_SUPPORT
	for (int current = 0; current < threads; ++current)
	{
		Log(VERBOSE, "Send another cancelling request for thread[%d]: %p (%d)\n",
			current, m_handlers[current],
			PosixThread::Cancel(m_handlers[current]));
		Log(VERBOSE, "Joined thread[%d] %p with return value %d\n",
			current, m_handlers[current],
			PosixThread::Join(m_handlers[current]));
	}
	DrainDone();
#endif
}

# ifdef THREAD_SUPPORT
/**
 * This class is used as a cleanup handler to insert a Link (used for an
 * instance of PosixAsyncManager::PendingOp) into a Head (used for
 * PosixAsyncManager::m_done) where the operation is protected by a PosixMutex.
 */
class InsertLink {
private:
	Link* m_item;
	Head* m_target;
	PosixMutex* m_mutex;

	void Into()
	{
		POSIX_MUTEX_LOCK(m_mutex);
		m_item->Into(m_target);
		POSIX_MUTEX_UNLOCK(m_mutex);
	}

public:
	InsertLink(Link* item, Head* target, PosixMutex* mutex)
		: m_item(item), m_target(target), m_mutex(mutex)
	{
		OP_ASSERT(item && target && mutex);
	}
	~InsertLink() { if (m_item) Into(); }
#ifdef POSIX_ASYNC_CANCELLATION
	static void CleanupHandler(void* arg) { static_cast<InsertLink*>(arg)->Into(); }
#endif
};

bool PosixAsyncManager::Process()
{
	/* Run by individual threads. */
	OP_ASSERT(!g_opera->posix_module.OnCoreThread());

	bool is_active = false;
	// Test if there is something to do, else take a nap:
	PendingOp *item;
	{
		POSIX_CONDITION_GRAB(&m_flag);
		if (!HasPending() && IsActive())
			m_flag.Wait(false);
		is_active = IsActive();
		item = FirstPending();
		if (item)
			item->Out();
		POSIX_CONDITION_GIVE(&m_flag);
	}

	// Then do what we were told:
	if (item)
	{
		InsertLink item_into_done(item, &m_done, &m_done_flag);
		POSIX_CLEANUP_PUSH(InsertLink::CleanupHandler, &item_into_done);
		if (item->TryLock())
			item->Run();
		POSIX_CLEANUP_POP();
	}

	return is_active;
}

static void* process_queue(void* manager)
{
	PosixAsyncManager *mgr = reinterpret_cast<PosixAsyncManager *>(manager);
	while (mgr->Process())
		/* skip */ ;

	/* We *could* #define _POSIX_THREAD_CPUTIME and return the CPU time used by
	 * this thread (see man 3posix pthread_create).
	 */
	return 0;
}
# else // !THREAD_SUPPORT

void PosixAsyncManager::HandleCallback(OpMessage msg, MH_PARAM_1 one, MH_PARAM_2 two)
{
	switch (msg)
	{
	case MSG_POSIX_ASYNC_TODO:
		OP_ASSERT(two == 0);
		/* Although one was originally a PendingOp*, it may since have been
		 * handled synchronously, so don't try to read it as a PendingOp*.
		 * Queue ensures we get at least as many messages as there are queued
		 * items to process, so we only need to do one at a time.
		 */
		if (PendingOp *op = FirstPending())
		{
#ifdef DEBUG_ENABLE_OPASSERT
			bool locked =
#endif
				op->TryLock();
			OP_ASSERT(locked && "if THREAD_SUPPORT is disabled, we expect to always succeed on locking the operation!");
			op->Out();
			op->Run();
			OP_DELETE(op);
		}
		break;
	}
	return;
}
# endif // THREAD_SUPPORT

void PosixAsyncManager::Queue(PendingOp *whom)
{
#ifdef THREAD_SUPPORT
	OP_ASSERT(m_handling >= 0);
	OP_ASSERT(g_opera->posix_module.OnCoreThread());
	m_flag.Grab();
#endif
	whom->Into(&m_pending);
#ifdef THREAD_SUPPORT
	m_flag.WakeAll();
	int queued = m_pending.Cardinal();
	m_flag.Give();
	Log(SPEW, "Added %010p to async/%d queue/%d\n", whom, m_handling, queued);
	/* If too busy, and we're not yet at our limit for active threads, start a
	 * new one.
	 */
	if (0 <= m_handling &&
		m_handling < queued && m_handling < POSIX_ASYNC_THREADS)
	{
		THREAD_HANDLE thread = PosixThread::CreateThread(process_queue, this);
		if (!PosixThread::IsNullThread(thread))
			m_handlers[m_handling++] = thread;
	}
	DrainDone();
#else
	g_main_message_handler->PostMessage(MSG_POSIX_ASYNC_TODO, (MH_PARAM_1)this, 0);
#endif
}

/* Generic implementation of Find methods.
 *
 * Structure is all the same, so use macros ...
 */
#define ImplementFind(client, type, result)							\
	PosixAsyncManager::result *										\
	PosixAsyncManager::Find(const client *whom, bool out)			\
	{																\
		PendingOp *item = NULL;										\
		{															\
			POSIX_CONDITION_GRAB(&m_flag);							\
			PendingOp *item = FirstPending();						\
			while (item &&											\
				   !(item->m_type == PendingOp::type &&				\
					 static_cast<result*>(item)->Boss() == whom))	\
				item = item->Suc();									\
			if (item && out) item->Out();							\
			POSIX_CONDITION_GIVE(&m_flag);							\
		}															\
		return static_cast<result*>(item);							\
	} // </ImplementFind>

# ifdef POSIX_ASYNC_FILE_OP
ImplementFind(OpLowLevelFile, FILE, PendingFileOp)
# endif
# ifdef POSIX_OK_DNS
ImplementFind(OpHostResolver,  DNS, PendingDNSOp)
# endif

#undef ImplementFind

#endif // POSIX_OK_ASYNC
