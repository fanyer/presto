/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_DNS
# ifndef THREAD_SUPPORT
#warning "You *really* want THREAD_SUPPORT with this host resolver !"
/* If this implementation is used single-threaded, your application is severely
 * likely to stall for protracted periods from time to time.  It has also never
 * been tested without THREAD_SUPPORT, although reasonable efforts have been
 * taken to ensure the design should still work in that case, albeit with
 * delays.
 *
 * It would be possible to implement a separate process serving instead of a
 * thread in support of this, if there's a serious need to do so.  Contact the
 * module owner if you think you need that - but try using threads, first !
 */
# endif // THREAD_SUPPORT

#include "modules/hardcore/mh/messobj.h"		// MessageObject
#include "modules/hardcore/mh/mh.h"				// MessageHandler (g_main_message_handler)

#include "platforms/posix/posix_system_info.h"	// for GetLocalHostName
#include "platforms/posix/src/posix_async.h"	// PendingDNSOp
#include "platforms/posix/posix_native_util.h"	// NativeString
#include "platforms/posix/posix_logger.h"		// PosixLogger
#include "platforms/posix/net/posix_network_address.h" // PosixNetworkAddress, <netdb.h>, ipv[46]_addr
#include "platforms/posix/posix_mutex.h"		// PosixMutex

#ifdef PREFS_HAVE_PREFER_IPV6
# include "modules/prefs/prefsmanager/collections/pc_network.h"
# define PREFS_PREFER_IPV6 g_pcnet->GetIntegerPref(PrefsCollectionNetwork::PreferIPv6)
#else
# define PREFS_PREFER_IPV6 true
#endif // PREFS_HAVE_PREFER_IPV6

#if defined(POSIX_DNS_USE_RES_INIT) || defined(POSIX_DNS_USE_RES_NINIT)
# include <resolv.h>
/**
 * This macro can be used to resolve a hostname with expression expr. If
 * the condition cond is true, ResolveInit() is called and we
 * try again to resolve a hostname with expression expr.
 *
 * @note The second call to resolve the hostname is only executed if
 *  the specified condition is true and the PosixHostResolver::Worker
 *  instance is still alive. When the PosixHostResolver::Worker has
 *  been aborted by its boss, there is no need to continue the work.
 *
 * @param var a variable that receives the result of expr.
 * @param cond is a condition on which to call ResolveInit() and try to resolve
 *  the hostname again. This condition should test if the first call to
 *  resolve the hostname was not successful.
 * @param expr is the expression to resolve the hostname.
 */
# define ResolveMaybeInit(var, cond, expr)	\
	var = expr;                             \
	if (cond && Live() && ResolveInit()) var = expr
#else // !(POSIX_DNS_USE_RES_INIT or POSIX_DNS_USE_RES_NINIT)
# define ResolveMaybeInit(var, cond, expr)	var = expr
#endif // POSIX_DNS_USE_RES_INIT or POSIX_DNS_USE_RES_NINIT

#include <sys/socket.h>
#include <netdb.h>
#undef USING_HOSTENT

#ifdef POSIX_DNS_IPNODE
#define USING_HOSTENT
#endif

#ifdef POSIX_DNS_GETHOSTBYNAME
#define USING_HOSTENT
#endif // POSIX_DNS_GETHOSTBYNAME

/** Implement OpHostResolver.
 *
 * This implementation can only Resolve() during the lifetime of g_opera.
 *
 * \msc
 * core,PosixHostResolver,Worker,Executor,PosixAsyncManager,process_queue;
 * core=>PosixHostResolver [label="creates"];
 * core=>PosixHostResolver [label="1. Resolve()", url="\ref PosixHostResolver::Resolve"];
 * PosixHostResolver=>Worker [label="2. creates"];
 * PosixHostResolver=>Executor [label="3. creates"];
 * Executor=>Executor [label="4. Enqueue()", url="\ref PosixAsyncManager::PendingOp::Enqueue()"];
 * Executor=>PosixAsyncManager [label="4. Queue(this)", url="\ref PosixAsyncManager::Queue()"];
 * PosixAsyncManager=>process_queue [label="5. starts thread"];
 * core<<process_queue;
 * --- [label="on the started process_queue thread:"];
 * process_queue:>PosixAsyncManager [label="6. Process()", url="\href PosixAsyncManager::Process()", linecolor="red"];
 * PosixAsyncManager:>Executor [label="7. Run()", url="\href PosixAsyncManager::Executor::Run()", linecolor="red"];
 * Executor:>Worker [label="8. Resolve()", url="\href PosixHostResolver::Worker::Resolve()", linecolour="red"];
 * Executor<<Worker;
 * Executor->PosixHostResolver [label="9. MSG_POSIX_ASYNC_DONE"];
 * Executor>>process_queue;
 * --- [label="on the core thread:"];
 * core=>PosixHostResolver [label="10. HandleCallback()", url="\ref PosixHostResolver::HandleCallback()"];
 * PosixHostResolver=>>core [label="11. OnHostResolved()", url="\ref OpHostResolverListener::OnHostResolved()"];
 * core=>PosixHostResolver [label="GetAddressCount()", url="\ref PosixHostResolver::GetAddressCount()"];
 * core=>PosixHostResolver [label="GetAddress()", url="\ref PosixHostResolver::GetAddress()"];
 * core=>PosixHostResolver [label="destroy"];
 * PosixHostResolver=>Executor [label="destroy"];
 * PosixHostResolver=>Worker [label="destroy"];
 * \endmsc
 *
 * -# core calls OpHostResolver::Resolve() to resolve the specified hostname.
 * -# PosixHostResolver::Resolve() creates an instance of
 *    PosixHostResolver::Worker (#m_worker) and registers itself as the
 *    MessageObject callback for the message MSG_POSIX_ASYNC_DONE with
 *    the new worker instance as first argument.
 * -# PosixHostResolver::Resolve() creates an instance of
 *    PosixHostResolver::Executor. The PosixHostResolver::Worker
 *    instance is the executor's job and the PosixHostResolver is the
 *    executor's boss.
 * -# The PosixHostResolver::Executor instance adds itself to the
 *    queue of pending operations (instances of
 *    PosixAsyncManager::PendingOp) by calling PendingOp::Enqueue(),
 *    which calls PosixAsyncManager::Queue().
 * -# PosixAsyncManager::Queue() may start a new \c process_queue
 *    thread, which will handle one by one PendingOp instance which is
 *    available in the PosixAsyncManager's queue.
 * -# The thread \c process_queue calls PosixAsyncManager::Process().
 * -# PosixAsyncManager::Process() eventually handles the enqueued
 *    PosixHostResolver::Executor by calling
 *    PosixHostResolver::Executor::Run().
 * -# PosixHostResolver::Executor::Run() calls
 *    PosixHostResolver::Worker::Resolve() to perform the resolving job.
 * -# When the job is done, PosixHostResolver::Executor::Run() posts
 *    the message MSG_POSIX_ASYNC_DONE to the PosixHostResolver instance.
 * -# PosixHostResolver::HandleCallback() handles the message
 *    MSG_POSIX_ASYNC_DONE by calling ...
 * -# ... OpHostResolverListener::OnHostResolved(). Now core can call
 *    GetAddressCount() and GetAddress() to retrieve the resolved addresses.
 */
class PosixHostResolver
	: public OpHostResolver, public PosixLogger, public MessageObject
{
	/** Carriers for the results of a look-up; see m_answer4 and m_answer6. */
	template<typename addr_t> class Answer
	{
		size_t m_count;
		addr_t m_addrs[1];
		Answer() {}
	public:
		static Answer* Create(size_t count)
		{
			Answer *answer = reinterpret_cast<Answer*>(g_thread_tools->Allocate(sizeof(size_t) + sizeof(addr_t)*count));
			if (answer)
				answer->m_count = count;
			return answer;
		}

		size_t Count() const { return m_count; }
		const addr_t &Get(int i) const
			{ OP_ASSERT(i >= 0 && (size_t)i < m_count); return m_addrs[i]; }

		/** Overload operator delete.
		 *
		 * Answer objects are allocated on non-core thread, so must be deallocated
		 * via g_thread_tools (which is responsible for ensuring it's done
		 * thread-safely).
		 */
		void operator delete(void* p) { g_thread_tools->Free(p); }

#ifdef USING_HOSTENT
		void Copy(char **data)
		{
			if (!m_addrs)
				return;

			for (size_t i = 0; i < m_count; i++)
			{
				OP_ASSERT(data[i]);
				m_addrs[i] = *reinterpret_cast<addr_t*>(data[i]);
			}
			OP_ASSERT(!data[m_count]);
		}
#endif // USING_HOSTENT

#ifdef POSIX_DNS_GETADDRINFO
		inline void Ingest(struct sockaddr *addr, socklen_t len, size_t i);
		inline void Digest(struct addrinfo *info, int family)
		{
			if (!m_addrs)
				return;

			size_t i = 0;
			while (info)
			{
				if (info->ai_family == family)
				{
					OP_ASSERT(info->ai_addr->sa_family == family);
					Ingest(info->ai_addr, info->ai_addrlen, i);
					i++;
				}
				info = info->ai_next;
			}
			OP_ASSERT(i == m_count);
		}
#endif // POSIX_DNS_GETADDRINFO
	};

	/**
	 * The worker class does the actual job of resolving the hostname.
	 *
	 * The PosixHostResolver instance that created the worker is the
	 * worker's boss (#m_boss) and the worker instance is stored as
	 * #PosixHostResolver::m_worker.
	 */
	class Worker
		: public MessageObject
#if defined(POSIX_DNS_LOCKING) || defined(POSIX_DNS_USE_RES_INIT)
		, public PosixAsyncManager::Resolver // provides Get*Mutex()
#endif // POSIX_DNS_LOCKING or POSIX_DNS_USE_RES_INIT
		, public PosixLogger
	{
		// Being capable of resolving and remembering the answers:
		Answer<ipv4_addr> *m_answer4;
#ifdef POSIX_SUPPORT_IPV6
		Answer<ipv6_addr> *m_answer6;
#endif

#ifdef POSIX_SUPPORT_IPV6
		static bool IsIPv6OK() { return g_opera && g_opera->posix_module.GetIPv6Support(); }
		static bool IsIPv6Usable() { return g_opera && g_opera->posix_module.GetIPv6Usable(); }
#endif

#ifdef USING_HOSTENT
		void Store4(struct hostent* data)
		{
			OP_ASSERT(!m_answer4);
			OP_ASSERT(data->h_addrtype == AF_INET);
			const size_t n = HostentCount(data);
			if (n)
			{
				m_answer4 = Answer<ipv4_addr>::Create(n);
				if (m_answer4)
					m_answer4->Copy(&(data->h_addr));
			}
		}
# ifdef POSIX_SUPPORT_IPV6
		void Store6(struct hostent* data)
		{
			OP_ASSERT(!m_answer6);
			OP_ASSERT(data->h_addrtype == AF_INET6);
			const size_t n = IsIPv6OK() ? HostentCount(data) : 0;
			if (n)
			{
				m_answer6 = Answer<ipv6_addr>::Create(n);
				if (m_answer6)
					m_answer6->Copy(&(data->h_addr));
			}
		}
# endif // POSIX_SUPPORT_IPV6
#endif // USING_HOSTENT

#if defined(POSIX_DNS_USE_RES_INIT) || defined(POSIX_DNS_USE_RES_NINIT)
		/** Returns true if res_init() / res_ninit() was successful. */
		bool ResolveInit()
		{
# ifdef POSIX_DNS_USE_RES_NINIT
			return 0 == res_ninit(&_res);
# else // POSIX_DNS_USE_RES_INIT
			bool success = false;
			POSIX_MUTEX_LOCK(&(GetResInitMutex()));
			success = 0 == res_init();
			POSIX_MUTEX_UNLOCK(&(GetResInitMutex()));
			return success;
# endif // POSIX_DNS_USE_RES_NINIT
		}
#endif // POSIX_DNS_USE_RES_INIT or POSIX_DNS_USE_RES_NINIT

	public:
		OpHostResolver::Error Resolve();
		void DigestAddrs(struct addrinfo *const info, const char *name);

		UINT Count()
		{
			return (m_answer4 ? m_answer4->Count() : 0)
#ifdef POSIX_SUPPORT_IPV6
				+ (m_answer6 ? m_answer6->Count() : 0)
#endif
				;
		}

		OP_STATUS Get(UINT index,
#ifdef POSIX_SUPPORT_IPV6
					  bool prefer_v6,
#endif
					  PosixNetworkAddress *addr);
	private: // Being a worker object whose boss might vanish with no warning
		OP_STATUS SetAsCallback()
		{
			if (g_main_message_handler == 0)
			{
				PosixLogger::Log(PosixLogger::DNS, PosixLogger::TERSE,
								 "No main message handler"
								 " - can't register host resolver with it\n");
				return OpStatus::ERR_NULL_POINTER;
			}
			return g_main_message_handler->SetCallBack(this, MSG_POSIX_ASYNC_DONE,
													   (MH_PARAM_1)this);
		}

		const PosixNativeUtil::NativeString m_hostname;

	protected:
		PosixHostResolver *m_boss;
	public:
		virtual const char *SoughtName() const { return m_hostname.get(); }
		Worker(PosixHostResolver *boss, const uni_char *hostname)
			: PosixLogger(DNS)
			, m_answer4(0)
#ifdef POSIX_SUPPORT_IPV6
			, m_answer6(0)
#endif
			, m_hostname(hostname)
			, m_boss(boss) {}
		OP_STATUS Ready() { return m_hostname.Ready(); }
		~Worker()
		{
			if (g_main_message_handler)
				g_main_message_handler->UnsetCallBacks(this);
			OP_DELETE(m_answer4);
#ifdef POSIX_SUPPORT_IPV6
			OP_DELETE(m_answer6);
#endif
		}

		/* MessageObject API: */
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		OP_STATUS Abort()
		{
			OP_ASSERT(m_boss);
			if (g_main_message_handler && m_boss)
				g_main_message_handler->RemoveCallBacks(m_boss, (MH_PARAM_1) this);
			m_boss = 0;
			return SetAsCallback();
		}
		bool Live() { return m_boss != 0; }
	} *m_worker;

#ifdef SYNCHRONOUS_HOST_RESOLVING
	// <refugee> Class should be local to ResolveSync(), but gcc 2.95 can't cope.
	class LocalWorker : public Worker
	{
	public:
		LocalWorker(PosixHostResolver *boss, const uni_char *hostname)
			: Worker(boss, hostname) { m_boss->ClearWorker(); m_boss->m_worker = this; }
		~LocalWorker() { OP_ASSERT(m_boss->m_worker == this); m_boss->m_worker = NULL; }
	};
	friend class LocalWorker;
	// </refugee>
#endif // SYNCHRONOUS_HOST_RESOLVING

	// <refugee> Class should be local to Resolve(), but gcc 2.95 can't cope.
	class Executor : public PosixAsyncManager::PendingDNSOp
	{
		Worker *m_job;
	public:
		Executor(Worker *job, OpHostResolver *boss)
			: PosixAsyncManager::PendingDNSOp(boss)
			, m_job(job) { Enqueue(); }
		virtual ~Executor() { OP_DELETE(m_job); }
		/**
		 * Runs the resolving job by calling Worker::Resolve(). The result is
		 * posted with the message MSG_POSIX_ASYNC_DONE to the core thread.
		 *
		 * @note This method is called on one of the PosixAsyncManager threads.
		 */
		virtual void Run() { Done(m_job->Resolve()); }
		// Don't actually lock here: Run() takes too long
		virtual bool TryLock() {
			if (m_job->Live())
				return true;
			/* m_job->Live() returning false means that m_job->Abort() has been
			 * called, i.e. the boss (PosixHostResolver) has no longer a
			 * reference to the Worker instance m_job. So ensure that m_job
			 * deletes itself on handling MSG_POSIX_ASYNC_DONE (the job is its
			 * own message handler after calling Worker::Abort()): */
			Done(OpHostResolver::NETWORK_NO_ERROR);
			return false;
		}
	private:
		void Done(OpHostResolver::Error result) {
			MH_PARAM_1 job = reinterpret_cast<MH_PARAM_1>(m_job);
			m_job = 0;
			PosixAsyncManager::PostSyncMessage(MSG_POSIX_ASYNC_DONE,
											   job, (MH_PARAM_2) result);
		}
	};
	// </refugee>

	// see OpHostResolver's enum Error for possible error values.
	OpHostResolverListener * const m_listener;

	/**
	 * This attribute is true if the associated Worker instance m_worker has
	 * finished its resolver process. HandleCallback() sets this attribute to
	 * true on receiving the message MSG_POSIX_ASYNC_DONE.
	 *
	 * The attribute is initialized with false. On starting a new Resolve()
	 * job, i.e. on creating a Worker instance and handing it to the resolver
	 * thread, the attribute is set to false as well.
	 *
	 * This means that if the attribute is true, then the m_worker instance is
	 * no longer referenced by any resolver thread and the instance can be
	 * deleted safely. If the attribute is false, then the worker instance is
	 * still used on some resolver thread and Worker::Abort() must be called
	 * instead of deleting the instance. See ClearWorker().
	 */
	bool m_done;

	/**
	 * Deletes the Worker instance and sets m_worker to 0 and m_done to false.
	 * If the Worker instance is still referenced by a resolver thread, then
	 * Worker::Abort() is called instead of deleting the instance directly.
	 * Worker::Abort() causes the Worker::HandleCallback() to delete itself
	 * on receiving the MSG_POSIX_ASYNC_DONE.
	 */
	void ClearWorker() {
		if (m_done)
		{
			OP_DELETE(m_worker);
			m_worker = 0;
			m_done = false;
		}
		else if (m_worker)
		{   /* Worker::HandleCallback() will delete itself on handling the
			 * message MSG_POSIX_ASYNC_DONE: */
			m_worker->Abort();
			m_worker = 0;
		}
	}

#ifdef POSIX_SUPPORT_IPV6
	const bool m_prefer_ipv6;
	/* TODO: FIXME: conform to RFC 3484 !
	 * http://www.rfc-editor.org/rfc/rfc3484.txt
	 */
#endif

#ifdef USING_HOSTENT
	static size_t HostentCount(struct hostent *data)
	{
# ifdef h_addr
		size_t n = 0;
		while (data->h_addr_list[n])
			n++;

		return n;
# else
		return data->h_addr ? 1 : 0;
# endif
	}
#endif // USING_HOSTENT

	friend class PosixHostResolver::Worker;

#ifdef USING_HOSTENT
	static Error ByName2OpHost(int err, Error prior, const char *name)
	{
		// Where should OOM fit into this (if at all) ?
		switch (err)
		{
		case 0:
			// if (prior != NETWORK_NO_ERROR) return OpHostResolver::ERROR_HANDLED;
			return NETWORK_NO_ERROR;

		case NO_ADDRESS:
			if (prior != NETWORK_NO_ERROR)
				return HOST_ADDRESS_NOT_FOUND;
			break;

		case HOST_NOT_FOUND:
			if (prior != NETWORK_NO_ERROR &&
				prior != HOST_ADDRESS_NOT_FOUND)
				return HOST_NOT_AVAILABLE;
			break;

		case TRY_AGAIN:
			if (prior != NETWORK_NO_ERROR &&
				prior != HOST_ADDRESS_NOT_FOUND &&
				prior != HOST_NOT_FOUND)
				return TIMED_OUT;
			break;

		case NO_RECOVERY:
			if (prior != NETWORK_NO_ERROR &&
				prior != HOST_ADDRESS_NOT_FOUND &&
				prior != HOST_NOT_FOUND)
				return NETWORK_ERROR;
			break;

		default:
			PosixLogger::Log(PosixLogger::DNS, TERSE,
                             "Undocumented return (%d) from host lookup on (%s)\n",
                             err, name);
# if 0
			if (prior != NETWORK_NO_ERROR && prior != NETWORK_ERROR &&
				prior != HOST_ADDRESS_NOT_FOUND && prior != HOST_NOT_FOUND)
				return ERR_GENERIC; // what should we return in this case ?
# endif
		}
		return prior;
	}
#endif // USING_HOSTENT

#ifdef POSIX_DNS_GETADDRINFO
	static Error GetAddr2OpHost(int err, const char *name)
	{
		switch (err)
		{
		case EAI_AGAIN:		// temporary failure
			Log(DNS, NORMAL,
				"Temporary failure (%d) looking up %s\n", err, name);
			return TIMED_OUT;

		case EAI_MEMORY:	// OOM
			Log(DNS, TERSE, "OOMed (%d) looking up %s\n", err, name);
			return OUT_OF_MEMORY;

		case EAI_NONAME:	// no host with this name
		case EAI_FAIL:		// non-recoverable error
		case EAI_SYSTEM:	// system error
#ifdef POSIX_HAS_EAI_OVERFLOW
// The error code EAI_OVERFLOW indicates "An argument buffer
// overflowed." EAI_OVERFLOW was created to provide a meaningful error
// code for the getnameinfo() function, for the case where the buffers
// provided by the caller were too small. The EAI_OVERFLOW error code
// is not applicable to getaddrinfo(), which has no buffer arguments
// to overflow. EAI_OVERFLOW was mistakenly added to the getaddrinfo()
// page rather than the getnameinfo() page. The tweak
// TWEAK_POSIX_HAS_EAI_OVERFLOW controls whether or not this symbol is
// defined and if the code should check for it.
		case EAI_OVERFLOW:	// argument buffer overflowed (?)
#endif // POSIX_HAS_EAI_OVERFLOW
			Log(DNS, NORMAL, "Failed (%d) looking up %s\n", err, name);
			return NETWORK_ERROR;

			// None of these should be possible:
		case EAI_SERVICE:	// unknown service name
		case EAI_SOCKTYPE:	// unknown hints.ai_socktype
		case EAI_FAMILY:	// unrecognised hints.ai_family
		case EAI_BADFLAGS:	// bad hints.ai_flags
			Log(DNS, TERSE, "Internal error (%d) looking up %s\n", err, name);
			OP_ASSERT(!"How did that happen ?");
			return NETWORK_ERROR;

		case 0: OP_ASSERT(!"Should have been handled by caller"); break;
		default:
			OP_ASSERT(!"Undocumented return from getaddrinfo()");
			Log(DNS, TERSE, "Undocumented return, %d, from getaddrinfo(%s)", err, name);
			break;
		}
		return NETWORK_ERROR; // or what ?
	}

	void DigestAddrs(struct addrinfo *const info, const char *name);
#endif // POSIX_DNS_GETADDRINFO

public:
	PosixHostResolver()
		: PosixLogger(DNS), m_worker(0), m_listener(0), m_done(false)
#ifdef POSIX_SUPPORT_IPV6
		, m_prefer_ipv6(g_opera && g_opera->posix_module.GetIPv6Usable() && PREFS_PREFER_IPV6)
#endif
		{}

	PosixHostResolver(OpHostResolverListener* listener)
		: PosixLogger(DNS), m_worker(0), m_listener(listener), m_done(false)
#ifdef POSIX_SUPPORT_IPV6
		, m_prefer_ipv6(g_opera && g_opera->posix_module.GetIPv6Usable() && PREFS_PREFER_IPV6)
#endif
		{}
	virtual ~PosixHostResolver();

	OP_STATUS SetAsCallback(Worker * job)
	{
		if (g_main_message_handler == 0)
		{
			Log(TERSE,
				"No main message handler"
				" - can't register host resolver with it\n");
			return OpStatus::ERR_NULL_POINTER;
		}
		return g_main_message_handler->SetCallBack(
			this, MSG_POSIX_ASYNC_DONE, (MH_PARAM_1)job);
	}

	/**
	 * @name Implementation of the MessageObject API:
	 * @{
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	/** @} */

	/**
	 * @name Implementation of the OpHostResolver API
	 * @{
	 */

	// Only required to handle one request at a time.
	virtual OP_STATUS Resolve(const uni_char* hostname);
#ifdef SYNCHRONOUS_HOST_RESOLVING
	virtual OP_STATUS ResolveSync(const uni_char* hostname,
								  OpSocketAddress* socket_address, Error* error);
#endif // SYNCHRONOUS_HOST_RESOLVING

	// Always synchronous:
	virtual OP_STATUS GetLocalHostName(OpString* local_hostname, Error* error);

	// For listener->OnHostResolved(this) to call back to:
	virtual UINT GetAddressCount() { return m_worker ? m_worker->Count() : 0; }
	virtual OP_STATUS GetAddress(OpSocketAddress* socket_address, UINT index);
	/** @} */
};

#ifdef POSIX_DNS_GETADDRINFO
template<>
inline void PosixHostResolver::Answer<ipv4_addr>::Ingest(struct sockaddr *info,
														 socklen_t len, size_t i)
{
	OP_ASSERT(len == sizeof(struct sockaddr_in));
	OP_ASSERT(i < m_count && m_addrs);
	m_addrs[i] = reinterpret_cast<struct sockaddr_in *>(info)->sin_addr;
}

# ifdef POSIX_SUPPORT_IPV6
template<>
inline void PosixHostResolver::Answer<ipv6_addr>::Ingest(struct sockaddr *info,
														 socklen_t len, size_t i)
{
	OP_ASSERT(len == sizeof(struct sockaddr_in6));
	OP_ASSERT(i < m_count && m_addrs);
	m_addrs[i] = reinterpret_cast<struct sockaddr_in6 *>(info)->sin6_addr;
}
# endif // POSIX_SUPPORT_IPV6

void PosixHostResolver::Worker::DigestAddrs(struct addrinfo *const info, const char *name)
{
	int n4 = 0, n6 = 0;
	for (struct addrinfo *run = info; run; run = run->ai_next)
		if (run->ai_family == AF_INET)
			n4++;
		else
		{
#ifdef POSIX_SUPPORT_IPV6
			OP_ASSERT(run->ai_family == AF_INET6);
#endif
			n6++;
		}
	OP_ASSERT(!m_answer4);
	if (n4)
	{
		m_answer4 = Answer<ipv4_addr>::Create(n4);
		if (m_answer4)
			m_answer4->Digest(info, AF_INET);
		Log(VERBOSE, "Found %d IPv4 address%s for %s\n", n4, n4 == 1 ? "" : "es", name);
	}
#ifdef POSIX_SUPPORT_IPV6
	OP_ASSERT(!m_answer6);
#endif
	if (n6)
	{
#ifdef POSIX_SUPPORT_IPV6
		OP_ASSERT(IsIPv6OK());
		m_answer6 = Answer<ipv6_addr>::Create(n6);
		if (m_answer6)
			m_answer6->Digest(info, AF_INET6);
		Log(VERBOSE, "Found %d IPv6 address%s for %s\n", n6, n6 == 1 ? "" : "es", name);
#else
		OP_ASSERT(!"Got IPv6 addresses when asking only for IPv4");
		Log(TERSE, "DNS supplied %d IPv6 address%s when asked only for IPv4, for %s\n",
			n6, n6 > 1 ? "es" : "", name);
#endif
	}
}
#endif // POSIX_DNS_GETADDRINFO

#ifdef POSIX_DNS_GETHOSTBYNAME
# if defined(POSIX_GHBN2) || defined(POSIX_GHBN_2R6) || defined(POSIX_GHBN_2R7)
#define USING_GHBN2
# endif

/** Glue class to unify the many gethostbyname variants.
 */
class HostByName
{
#ifdef GHBN_BUFFER_SIZE
	char m_buffer[GHBN_BUFFER_SIZE];	// ARRAY OK 2010-08-12 markuso
	struct hostent m_hostent;
#elif defined(POSIX_GHBN_R3)
	struct hostent_data m_buffer;
	struct hostent m_hostent;
#endif
	const char *const m_name;
	int m_error;
	bool m_oom;
public:
	HostByName(const char *name) : m_name(name), m_error(0), m_oom(false) {}

#ifdef USING_GHBN2
	struct hostent *Get(int type);
#else
	struct hostent *Get();
#endif
	int Error(bool &oom) { if (m_oom) oom = true; return m_error; }
};

# ifdef POSIX_GHBN_2R7 // linux
inline struct hostent *HostByName::Get(int type)
{
	struct hostent *res = 0;
	int err = gethostbyname2_r(m_name, type, &m_hostent,
							   m_buffer, GHBN_BUFFER_SIZE,
							   &res, &m_error);
	if (err == ENOMEM || err == ENOBUFS)
		m_oom = true;
	else if (res)
		m_error = 0;
	return res;
}
# elif defined(POSIX_GHBN_2R6) // QNX
inline struct hostent *HostByName::Get(int type)
{
	errno = 0;
	struct hostent *res = gethostbyname2_r(m_name, type, &m_hostent,
										   m_buffer, GHBN_BUFFER_SIZE,
										   &m_error);
	if (errno == ENOMEM || errno == ENOBUFS)
		m_oom = true;
	else if (res)
		m_error = 0;
	return res;
}
# elif defined(POSIX_GHBN_R5) // SunOS
inline struct hostent *HostByName::Get()
{
	errno = 0;
	struct hostent *res = gethostbyname_r(m_name, &m_hostent,
										  m_buffer, GHBN_BUFFER_SIZE,
										  &m_error);
	if (errno == ENOMEM || errno == ENOBUFS)
		m_oom = true;
	else if (res)
		m_error = 0;
	return res;
}
# elif defined(POSIX_GHBN_R3) // HP/UX
inline struct hostent *HostByName::Get()
{
	int err = gethostbyname_r(m_name, &m_hostent, &m_buffer);
	if (err == 0)
	{
		m_error = 0;
		return &m_hostent;
	}
	if (err == ENOMEM || err == ENOBUFS)
		m_oom = true;
	return 0;
}
# elif defined(POSIX_GHBN2) // not thread-safe
inline struct hostent *HostByName::Get(int type)
{
	errno = 0;
	h_errno = 0;
	struct hostent *res = gethostbyname2(m_name, type);
	m_error = res ? 0 : h_errno;
	if (errno == ENOMEM || errno == ENOBUFS)
		m_oom = true;
	return res;
}
# else // Strict POSIX, fully generic, but not thread-safe:
inline struct hostent *HostByName::Get()
{
	errno = 0;
	struct hostent *res = gethostbyname(m_name);
	m_error = res ? 0 : h_errno;
	if (errno == ENOMEM || errno == ENOBUFS)
		m_oom = true;
	return res;
}
# endif // gethosbyname variant selection
#endif // POSIX_DNS_GETHOSTBYNAME

/** Perform actual name resolution, if we can.
 *
 * This method gets called from the worker (possibly in another thread).  It is
 * the heart of the selection between available resolvers; hence its heavy use
 * of \#if-ery.  Where possible, variation should be mediated by tool classes
 * (c.f. HostByName) that isolate as much of the \#if-ery as possible, to limit
 * the amount of it needed here.
 */
OpHostResolver::Error PosixHostResolver::Worker::Resolve()
{
	/* TODO: arrange to be able to do IPv4 and IPv6 lookups in parallel, in the
	 * cases where the two have to be looked up separately anyway. */
	OpHostResolver::Error result = OpHostResolver::DNS_NOT_FOUND;
#undef ResolverImplemented

#ifdef POSIX_DNS_IPNODE
	if (!Live())
		return OpHostResolver::NETWORK_NO_ERROR;

	Log(VERBOSE, "Resolving getipnodebyname(%s, ...)\n", SoughtName());
	int err = 0;
	hostent * ResolveMaybeInit(ans, !ans,
							   getipnodebyname(SoughtName(), AF_INET, AI_ADDRCONFIG, &err));
	if (ans)
	{
		POSIX_CLEANUP_PUSH(freehostent, ans);
		Store4(ans);
		POSIX_CLEANUP_POP();
		freehostent(ans);
	}
	result = PosixHostResolver::ByName2OpHost(err, result, SoughtName());
	OP_ASSERT(!ans == (result != OpHostResolver::NETWORK_NO_ERROR));

# ifdef POSIX_SUPPORT_IPV6
	if (IsIPv6OK() && Live() && (!ans || IsIPv6Usable()))
	{
		Log(VERBOSE, "Checking getipnodebyname() for IPv6\n", SoughtName());
		err = 0;
		ResolveMaybeInit(ans, !ans,
						 getipnodebyname(SoughtName(), AF_INET6, AI_ADDRCONFIG, &err));
		if (ans)
		{
			POSIX_CLEANUP_PUSH(freehostent, ans);
			Store6(ans);
			POSIX_CLEANUP_POP();
			freehostent(ans);
		}
		result = PosixHostResolver::ByName2OpHost(err, result, SoughtName());
		OP_ASSERT(result == OpHostResolver::NETWORK_NO_ERROR || !ans);
	}
# endif // POSIX_SUPPORT_IPV6
#define ResolverImplemented
#endif // POSIX_DNS_IPNODE

#ifdef POSIX_DNS_GETADDRINFO
	if (!Live())
		return OpHostResolver::NETWORK_NO_ERROR;

	// TODO: research why unix-net deprecated this
	struct addrinfo *info = 0;
	struct addrinfo hints;
	op_memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;

	hints.ai_family =
# ifdef POSIX_SUPPORT_IPV6
		// man 3posix getaddrinfo: AF_UNSPEC means we accept any address family:
		(IsIPv6OK()) ? AF_UNSPEC :
# endif // POSIX_SUPPORT_IPV6
		AF_INET;
	// FIXME: TODO: getaddrinfo calls malloc - and we're in a non-main thread !
	// 0 as DNS service name; indicates generic service:
	int ResolveMaybeInit(err, err, getaddrinfo(SoughtName(), 0, &hints, &info));
	if (err == 0)
	{
		POSIX_CLEANUP_PUSH((void (*)(void*))freeaddrinfo, info);
		DigestAddrs(info, SoughtName());
		POSIX_CLEANUP_POP();
		freeaddrinfo(info);
		result = OpHostResolver::NETWORK_NO_ERROR;
	}
	else
	{
		OP_ASSERT(!info);
		result = PosixHostResolver::GetAddr2OpHost(err, SoughtName());
	}
#define ResolverImplemented
#endif // POSIX_DNS_GETADDRINFO

#ifdef POSIX_DNS_GETHOSTBYNAME
	// Sub-divides into *many* factions; see also HostByName class above.
	if (!Live())
		return OpHostResolver::NETWORK_NO_ERROR;

	HostByName query(SoughtName());
	bool oom = false;
	{
# ifdef POSIX_DNS_LOCKING // The cases whose variants aren't thread-safe.
		POSIX_MUTEX_LOCK(&GetDnsMutex());
# endif // POSIX_DNS_LOCKING

# ifdef USING_GHBN2
		struct hostent * ResolveMaybeInit(ans, !ans, query.Get(AF_INET));
		result = PosixHostResolver::ByName2OpHost(query.Error(oom), result, SoughtName());
		if (ans)
			Store4(ans);

#  ifdef POSIX_SUPPORT_IPV6
		if (Live() && IsIPv6OK() && (!ans || IsIPv6Usable()))
		{
			ResolveMaybeInit(ans, !ans, query.Get(AF_INET6));
			result = PosixHostResolver::ByName2OpHost(query.Error(oom), result, SoughtName());
			if (ans)
				Store6(ans);
		}
#  endif // POSIX_SUPPORT_IPV6
# else // !USING_GHBN2
		struct hostent * ResolveMaybeInit(ans, !ans, query.Get());
		result = PosixHostResolver::ByName2OpHost(query.Error(oom), result, SoughtName());
		if (ans)
		{
			if (ans->h_addrtype == AF_INET)
				Store4(ans);
#  ifdef POSIX_SUPPORT_IPV6
			else if (IsIPv6OK() && ans->h_addrtype == AF_INET6)
				Store6(ans);
#  else // !POSIX_SUPPORT_IPV6
			else
				Log(CHATTY, "Unsupported or unrecognized (%d) address type found for %s",
					ans->addrtype, SoughtName());
#  endif // POSIX_SUPPORT_IPV6
		}
# endif // USING_GHBN2
# ifdef POSIX_DNS_LOCKING // The cases whose variants aren't thread-safe.
		POSIX_MUTEX_UNLOCK(&GetDnsMutex());
# endif // POSIX_DNS_LOCKING
	}

#define ResolverImplemented
#endif // POSIX_DNS_GETHOSTBYNAME

#ifndef ResolverImplemented
# error "You need to activate one of the supported ways to do DNS !"
	/* See: TWEAK_POSIX_DNS_GETADDRINFO, TWEAK_POSIX_DNS_GETHOSTBYNAME,
	 * TWEAK_POSIX_DNS_IPNODE */
#endif
#undef ResolverImplemented

	return result;
}
#undef ResolveMaybeInit

/* Relatively easy stuff: */

PosixHostResolver::~PosixHostResolver()
{
	if (g_main_message_handler)
		g_main_message_handler->UnsetCallBacks(this);

	if (m_worker)
	{
		PosixAsyncManager::PendingDNSOp *check = g_opera && g_posix_async
			? g_posix_async->Find(this, TRUE) // That Out()ed it
			: 0;

		if (check)
		{	/* check was a pending Executor instance and m_worker is its
			 * m_job. This means that Executor::Run() was not yet called,
			 * and thus Worker::Resolve() was not yet called and we can
			 * safely delete the Executor, which deletes the worker: */
			OP_ASSERT(!m_done);
			OP_DELETE(check);
		}
		else
			ClearWorker();
	}
	else
		OP_ASSERT(!g_opera || !g_posix_async || !g_posix_async->Find(this, TRUE));

	Log(CHATTY, "Host resolver destroyed (%p, %p)\n", this, m_listener);
}

OP_STATUS PosixHostResolver::GetLocalHostName(OpString* local_hostname, Error* error)
{
	OP_STATUS ret = static_cast<PosixSystemInfo *>(
		g_op_system_info)->GetLocalHostName(*local_hostname);
	if (OpStatus::IsSuccess(ret) && local_hostname->IsEmpty())
		ret = OpStatus::ERR;

	if (ret == OpStatus::ERR)
		*error = HOST_ADDRESS_NOT_FOUND;

	if (error)
		Log(TERSE, "Failed to determine local host name\n");
	else
		Log(CHATTY, "Found local host name of length %d\n", local_hostname->Length());

	return ret;
}

void PosixHostResolver::Worker::HandleCallback(OpMessage msg,
											   MH_PARAM_1 one,
											   MH_PARAM_2 two)
{
	OP_ASSERT(msg == MSG_POSIX_ASYNC_DONE && one == (MH_PARAM_1)this);
	OP_ASSERT(!m_boss);
	OP_DELETE(this);
}

void PosixHostResolver::HandleCallback(OpMessage msg, MH_PARAM_1 one, MH_PARAM_2 two)
{
	OP_ASSERT(msg == MSG_POSIX_ASYNC_DONE && one == (MH_PARAM_1)m_worker);
	m_done = true;
	// Async
	switch (Error error = (Error)two)
	{
	case NETWORK_NO_ERROR:
		Log(CHATTY, "DNS look-up succeeded for %s\n", m_worker->SoughtName());
		m_listener->OnHostResolved(this);
		break;
	// case ERROR_HANDLED: // ?
	default:
		Log(NORMAL, "DNS look-up failed for %s\n", m_worker->SoughtName());
		m_listener->OnHostResolverError(this, error);
		break;
	}
	// Warning: listener methods may have deleted this, so don't reference it !
}

OP_STATUS PosixHostResolver::Resolve(const uni_char* hostname)
{
	if (!(g_opera && g_posix_async && g_posix_async->IsActive()))
	{
		OP_ASSERT(!"Attempted DNS resolution outside life-time of g_opera");
		return OpStatus::ERR;
	}

	if (g_posix_async->Find(this, FALSE))
	{
		OP_ASSERT(!"Attempted Resolve while look-up pending");
		return OpStatus::ERR;
	}
	ClearWorker();

	Worker *job = OP_NEW(Worker, (this, hostname));
	OP_STATUS err = OpStatus::ERR_NO_MEMORY;
	if (job)
	{
		OpStatus::Ignore(err);
		if (OpStatus::IsSuccess(err = job->Ready()))
		{
			if (OpStatus::IsSuccess(err = SetAsCallback(job)))
			{
				Executor * runner = OP_NEW(Executor, (job, this));
				if (runner)
				{
					m_worker = job;
					Log(CHATTY, "Queued DNS look-up for %s\n", job->SoughtName());
					return OpStatus::OK;
				}
				err = OpStatus::ERR_NO_MEMORY;
				g_main_message_handler->UnsetCallBacks(this);
			}
		}
		OP_DELETE(job);
	}

	Log(TERSE, "OOM: failed to queue DNS look-up\n");
	return err;
}

#ifdef SYNCHRONOUS_HOST_RESOLVING
OP_STATUS PosixHostResolver::ResolveSync(const uni_char* hostname,
										 OpSocketAddress* socket_address,
										 Error* error)
{
	if (g_posix_async->Find(this, FALSE))
	{
		OP_ASSERT(!"Attempted ResolveSync while look-up pending");
		return OpStatus::ERR;
	}

	LocalWorker job(this, hostname);
	RETURN_IF_ERROR(job.Ready());

	switch (*error = job.Resolve())
	{
		// case ERROR_HANDLED: // ?
	case NETWORK_NO_ERROR:
		if (GetAddressCount())
		{
			Log(CHATTY, "Successful synchronous DNS for %s\n", job.SoughtName());
			return GetAddress(socket_address, 0);
		}
		Log(NORMAL, "DNS reports no match for %s\n", job.SoughtName());
		*error = DNS_NOT_FOUND;
		break;

	default:
		Log(TERSE, "Failed (%d) synchronous DNS for %s\n", (int)*error, job.SoughtName());
		break;
	}
	return OpStatus::ERR;
}
#endif // SYNCHRONOUS_HOST_RESOLVING

OP_STATUS PosixHostResolver::Worker::Get(UINT index,
#ifdef POSIX_SUPPORT_IPV6
										   bool prefer_v6,
#endif
										   PosixNetworkAddress *addr)
{
#ifndef POSIX_SUPPORT_IPV6
	const size_t cut = 0;
#else
	int cut = 0;
	int indsix = index;
	if (prefer_v6)
		cut = m_answer6 ? m_answer6->Count() : 0;
	else if (m_answer4)
		indsix -= m_answer4->Count();

	OP_ASSERT(IsIPv6OK() || cut == 0);

	if (prefer_v6 == (indsix < cut))
	{
		RETURN_IF_ERROR(addr->SetFromIPv(m_answer6->Get(indsix)));
	}
	else
#endif // POSIX_SUPPORT_IPV6
	{
		OP_ASSERT(m_answer4 && m_answer4->Count() > index - cut);
		RETURN_IF_ERROR(addr->SetFromIPv(m_answer4->Get(index - cut)));
	}
	return OpStatus::OK;
}

OP_STATUS PosixHostResolver::GetAddress(OpSocketAddress* socket_address, UINT index)
{
	if (index >= GetAddressCount())
	{
		OP_ASSERT(!"Requested socket address past end of range");
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	if (socket_address == 0)
	{
		OP_ASSERT(!"No socket address object provided.");
		return OpStatus::ERR_NULL_POINTER;
	}
	PosixNetworkAddress local;

	OP_ASSERT(m_worker); // else index >= GetAddressCount
	OP_STATUS stat = m_worker->Get(index,
#ifdef POSIX_SUPPORT_IPV6
								   m_prefer_ipv6,
#endif
								   &local);
	return OpStatus::IsSuccess(stat) ? socket_address->Copy(&local) : stat;
}

/* Finally, the one thing visible outside this compilation unit: */
OP_STATUS OpHostResolver::Create(OpHostResolver** host_resolver,
								 OpHostResolverListener* listener)
{
	if (host_resolver == NULL)
		return OpStatus::ERR_NULL_POINTER;
	else
		*host_resolver = NULL;

	PosixHostResolver *local = OP_NEW(PosixHostResolver, (listener));
	if (local == 0)
	{
		PosixHostResolver::Log(PosixLogger::DNS, PosixLogger::TERSE,
							   "OOM: failed to create host resolver\n");
		return OpStatus::ERR_NO_MEMORY;
	}

	local->Log(PosixLogger::CHATTY,
			   "Created host resolver (%p, %p)\n", local, listener);
	*host_resolver = static_cast<OpHostResolver*>(local);
	return OpStatus::OK;
}

#endif // POSIX_OK_DNS
