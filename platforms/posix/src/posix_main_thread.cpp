/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_MAIN_THREAD

#include "modules/pi/OpThreadTools.h"
#include "modules/hardcore/mh/mh.h"

#undef CROSS_THREAD_MALLOC // see below.
#ifdef THREAD_SUPPORT
# ifndef POSIX_OK_THREAD
#error "You need to import API_POSIX_THREAD to use OpThreadTools with thread support"
# endif // POSIX_OK_THREAD
# ifndef POSIX_THREADSAFE_MALLOC
#define CROSS_THREAD_MALLOC
# endif

#include "platforms/posix/posix_thread_util.h"
#include "platforms/posix/posix_selector.h"
#include "modules/util/simset.h"

/** A custom memory pool, for use with MsgOp.
 *
 * Must be instantiated on the core thread.
 */
template<class T>
class ObjectStack
{
	T * m_stack;
	size_t m_size;
	size_t m_used;
public:
	ObjectStack()
		: m_stack(reinterpret_cast<T*>(op_malloc(POSIX_MAINTHREAD_OBJECTS * sizeof(T)))),
		  m_size(m_stack ? POSIX_MAINTHREAD_OBJECTS : 0),
		  m_used(0) {}
	~ObjectStack() { op_free(m_stack); }
	void* Allocate()
	{
		if (m_used >= m_size)
			return 0;

		return m_stack + m_used++;
	}

	/**
	 * All objects have been processed; release their memory.  However, also
	 * look to see if we ran low on space: if so, expand, so next cycle might
	 * work more smoothly.
	 */
	void Reset()
	{
		const size_t gap = m_size > m_used ? m_size - m_used : 0;
		if (m_size < POSIX_MAINTHREAD_OBJECTS || gap * gap < m_size) // heuristic.
		{
			const size_t size = m_size + POSIX_MAINTHREAD_OBJECTS;
			m_stack = reinterpret_cast<T*>(op_realloc(m_stack, size * sizeof(T)));
			if (m_stack)
				m_size = size;
			else if (m_size > 0)
			{
				m_stack = reinterpret_cast<T*>(op_malloc(m_size * sizeof(T)));
				OP_ASSERT(m_stack); // because op_realloc just free()d this !
				if (m_stack == 0)
					m_size = 0;
			}
		}
		// TODO: else, if way bigger than we need, shrink ?

		m_used = 0;
	}
};

/** Simple task queue.
 *
 * Any thread can push tasks onto the queue; this will trigger core to run the
 * OnPressed method, which executes each pending task on the core thread.
 */
class PosixThreadQueue : public PosixSelector::Button, public Head
{
	PosixCondition m_cond;

	/** Task base-class.
	 *
	 * Each instance must be Strat()ed when ready to be Run(); each derived
	 * class must implement Run() to execute the task.  This will happen on the
	 * core thread.
	 */
	class QueuedOp : public Link
	{
		enum { OP_DONE, OP_QUEUE } op;
	protected:
		void Done()
		{
			op = OP_DONE;
			Out();
		}
	public:
		QueuedOp() : op(OP_QUEUE) {}
		virtual ~QueuedOp() {}
		/** Perform the task (called in the core thread).
		 *
		 * Virtual method each derived class must implement.
		 */
		virtual void Run() = 0;

		/** Signal that task is ready for processing.
		 *
		 * Adds the task to the task queue.  Do not call this until the task
		 * object is ready to be Run().
		 */
		void Start(PosixCondition &cond, Head *ku)
		{
			POSIX_CONDITION_GRAB(&cond);
			Into(ku);
			POSIX_CONDITION_GIVE(&cond);
		}

		/** Wait until Done().
		 *
		 * This causes the task to be blocking: it should only be called if
		 * necessary.
		 */
		void Wait(PosixCondition &cond)
		{
			bool done = false;
			do
			{
				POSIX_CONDITION_GRAB(&cond);
				if (op != OP_DONE)
					cond.Wait(false);
				done = op == OP_DONE;
				POSIX_CONDITION_GIVE(&cond);
			} while (!done);
		}
	};

	/** Deliver a message to the core message queue. */
	class MsgOp : public QueuedOp
	{
		const OpMessage m_msg;
		const MH_PARAM_1 m_one;
		const MH_PARAM_2 m_two;
		const unsigned long m_nap;
		OpTypedMessage* m_typed_msg;

	public:
		MsgOp(OpMessage m, MH_PARAM_1 a, MH_PARAM_1 b, unsigned long t)
			: m_msg(m), m_one(a), m_two(b) , m_nap(t), m_typed_msg(NULL) {}
		explicit MsgOp(OpTypedMessage* typed_msg)
			: m_msg(MSG_NO_MESSAGE), m_one(0), m_two(0) , m_nap(0), m_typed_msg(typed_msg) { OP_ASSERT(m_typed_msg); }
		~MsgOp() { OP_DELETE(m_typed_msg); }

		void * operator new (size_t size, ObjectStack<MsgOp> &stack) THROWCLAUSE
			{ return stack.Allocate(); }

		virtual void Run()
		{
			Done();
			if (m_typed_msg)
			{
#ifdef DEBUG_ENABLE_OPASSERT
				OP_STATUS res =
#endif
				g_component_manager->SendMessage(m_typed_msg);
				m_typed_msg = NULL;
				OP_ASSERT(OpStatus::IsSuccess(res));
			}
			else
			{
#ifdef DEBUG_ENABLE_OPASSERT
				OP_STATUS res =
#endif
				g_main_message_handler->PostMessage(m_msg, m_one, m_two, m_nap);
				OP_ASSERT(OpStatus::IsSuccess(res));
			}
		}
	};
	ObjectStack<MsgOp> m_msg_stack;

#ifdef CROSS_THREAD_MALLOC
	// <refugee> classes that belong inside functions, but gcc 2.95 can't cope:
	/** Allocate some memory (thread-safely; but blocks). */
	class MallocOp : public QueuedOp
	{
		const size_t m_size;
		void * m_res;
	public:
		MallocOp(size_t s) : m_size(s) {}

		virtual void Run()
		{
			m_res = op_malloc(m_size);
			Done();
		}

		void *GetAddr(PosixCondition &cond)
		{
			Wait(cond); // result not yet available
			return m_res;
		}
	};

	/** Release some memory (thread-safely; but blocks). */
	class FreeOp : public QueuedOp
	{
		void *const m_ptr;
	public:
		FreeOp(void *p) : m_ptr(p) {}

		virtual void Run()
		{
			Done();
			op_free(m_ptr);
		}
	};
	// </refugee>
#endif // CROSS_THREAD_MALLOC

public:
	PosixThreadQueue() : m_msg_stack() {}

	virtual void OnPressed() // Called in the main thread.
	{
		POSIX_CONDITION_GRAB(&m_cond);
		while (QueuedOp *op = static_cast<QueuedOp *>(First()))
			op->Run();

		m_msg_stack.Reset();
		m_cond.WakeAll();
		POSIX_CONDITION_GIVE(&m_cond);
	}

#ifdef CROSS_THREAD_MALLOC
	// Methods invoked on other threads, to request help from main thread.
	void *Allocate(size_t size)
	{
		MallocOp ku(size);
		ku.Start(m_cond, this);
		Press();
		return ku.GetAddr(m_cond);
	}

	void Free(void *memblock)
	{
		FreeOp ku(memblock);
		ku.Start(m_cond, this);
		Press();
		ku.Wait(m_cond); // can't return (would delete ku) until done
		return;
	}
#endif // CROSS_THREAD_MALLOC

	OP_STATUS PostMessage(OpMessage msg, MH_PARAM_1 one, MH_PARAM_2 two,
						  unsigned long delay)
	{
		MsgOp * op = NULL;
		POSIX_CONDITION_GRAB(&m_cond);
		op = new (m_msg_stack) MsgOp(msg, one, two, delay);
		if (op)
			/* Insert the new message before we surrender the lock,
			 * otherwise another thread may call OnPressed(), which
			 * resets the m_msg_stack, before we insert the message. */
			op->Start(m_cond, this);
		POSIX_CONDITION_GIVE(&m_cond);
		if (op == NULL)
			/* Let caller know we failed.
			 *
			 * OpThreadTools::PostMessage() specifies that we must not block, as
			 * doing so may cause deadlock; so callers need to notice failure
			 * and (somehow) record that they need to re-try posting this
			 * message.  Given that they can't (reliably) call malloc to store
			 * the message details, there really isn't much they can do aside
			 * from block - and, unlike this function, they don't have access to
			 * the exact one thing, m_cond.Wait(), that'll accurately tell them
			 * when it's worth re-trying.  See CORE-37144 for a possible way out
			 * of this; and CORE-37139 for a design review of OpThreadTools.
			 */
			return OpStatus::ERR_NO_MEMORY;

		/* We aren't waiting for it to complete; that's why it has to be new()d
		 * and Run() must delete it ! */
		Press();
		return OpStatus::OK;
	}

	OP_STATUS SendMessage(OpTypedMessage* message)
	{
		MsgOp * op = NULL;
		POSIX_CONDITION_GRAB(&m_cond);
		op = new (m_msg_stack) MsgOp(message);
		if (op)
			/* Insert the new message before we surrender the lock,
			* otherwise another thread may call OnPressed(), which
			* resets the m_msg_stack, before we insert the message. */
			op->Start(m_cond, this);
		POSIX_CONDITION_GIVE(&m_cond);
		if (op == NULL)
		{
			OP_DELETE(message);
			return OpStatus::ERR_NO_MEMORY;
		}

		/* We aren't waiting for it to complete; that's why it has to be new()d
		* and Run() must delete it ! */
		Press();
		return OpStatus::OK;
	}
};

#else
# ifdef __GNUC__
#warning "No THREAD_SUPPORT: are you sure you need API_POSIX_MAIN_THREAD ?"
# endif
#endif // THREAD_SUPPORT

/** Implement OpThreadTools.
 *
 * When single-threaded (THREAD_SUPPORT not defined), this simply assumes it's
 * safe to do tasks directly, because it's on the core thread.  Otherwise, it
 * maintains a simple task queue to which it adds tasks for the core thread to
 * run.  Relies on PosixSelector::Button to alert the core thread to the need to
 * check for tasks in the work queue.
 */
class PosixCoreThread : public OpThreadTools
{
#ifdef THREAD_SUPPORT
	PosixThreadQueue m_queue;
	// Convenience look-up:
	static bool OffCoreThread()
		{ return !g_opera->posix_module.OnCoreThread(); }
#endif // THREAD_SUPPORT
	// Need proper thread-safe implementation !

	friend class OpThreadTools;
	PosixCoreThread() {}
	OP_STATUS Init()
	{
#ifdef THREAD_SUPPORT
		return m_queue.Ready();
#else
		return OpStatus::OK;
#endif
	}
public:

	virtual ~PosixCoreThread()
	{
#if 0 //def THREAD_SUPPORT
		/* Absent a few milliseconds' warning of shut-down, this is always apt
		 * to be in the process of delivering some messages; this destructor is
		 * called in pi_module->Destroy(), after posix_module->Destroy(), but
		 * the message loop has had no time to let us flush our queue in the
		 * interval.
		 */
		OP_ASSERT(m_queue.Empty());
#endif
	}

	// Methods called by thread to get main thread to provide a service:
	virtual void* Allocate(size_t size)
	{
#ifdef CROSS_THREAD_MALLOC
		if (OffCoreThread())
			return m_queue.Allocate(size);
#endif
		return op_malloc(size);
	}

	virtual void Free(void* memblock)
	{
#ifdef CROSS_THREAD_MALLOC
		if (OffCoreThread())
			m_queue.Free(memblock);
		else
#endif
		op_free(memblock);
	}

	// Post to main thread's loop:
	virtual OP_STATUS PostMessageToMainThread(OpMessage msg, MH_PARAM_1 one, MH_PARAM_2 two,
								  unsigned long delay=0)
	{
		// non-blocking
#ifdef THREAD_SUPPORT
		if (OffCoreThread())
			return m_queue.PostMessage(msg, one, two, delay);
#endif
		return g_main_message_handler->PostMessage(msg, one, two, delay);
	}

	virtual OP_STATUS SendMessageToMainThread(OpTypedMessage* message)
	{
#ifdef THREAD_SUPPORT
		if (OffCoreThread())
			return m_queue.SendMessage(message);
#endif
		return g_component_manager->SendMessage(message);
	}
};

// static (and the only thing in this file visible to anything outside)
OP_STATUS OpThreadTools::Create(OpThreadTools** new_main_thread)
{
#ifdef THREAD_SUPPORT
	/* We're called from pi's InitL(), which is called before posix's, so
	 * g_posix_selector hasn't been initialized yet: and Button's constructor
	 * needs it. */
	RETURN_IF_ERROR(g_opera->posix_module.InitSelector());
#endif
	PosixCoreThread *local = OP_NEW(PosixCoreThread, ());
	if (!local)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS res = local->Init();
	if (OpStatus::IsSuccess(res))
		*new_main_thread = local;
	else
		OP_DELETE(local);

	return res;
}

#endif // POSIX_OK_MAIN_THREAD
