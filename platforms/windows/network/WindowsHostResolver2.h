 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  */

/** @file WindowsHostResolver.h
  *
  * Windows version of the standard interface for host resolving in Opera.
  *  
  * @author Johan Borg
  *
  * @version $Id$ 
  */


#ifndef __WINDOWSHOSTRESOLVER2_H
#define __WINDOWSHOSTRESOLVER2_H

#include "modules/pi/network/OpHostResolver.h"
#include "platforms/windows/utils/sync_primitives.h"

class WindowsHostResolver;
class WindowsThreadedHostResolver;
class WindowsHostResolverThread;

#define g_windows_host_resolver_manager (WindowsHostResolverManager::GetInstance())

/**
 * Manages worker threads and assigns resolvers to threads.
 */
class WindowsHostResolverManager
{
public:
	WindowsHostResolverManager()
		: m_active(TRUE)
		, m_threads_count(0)
	{}

	~WindowsHostResolverManager();

	/**
	 * Shut down the manager.
	 * All worker threads are signalled to abort their tasks. Threads
	 * that do not exit before timeout are terminated.
	 */
	void ShutDown();

	/**
	 * Return whether this manager accepts new resolver tasks.
	 */
	BOOL IsActive() const { return m_active; }

	/**
	 * Run resolver in worker thread.
	 *
	 * This transfers ownership of the resolver to the manager. If resolver
	 * is cancelled while enqueued in the manager it is deleted.
	 *
	 * When a worker thread is ready to run the resolver then ownership is
	 * transfered to the thread object. Worker thread calls PerformResolving
	 * function once and posts status back to the main thread. When status
	 * is received on the main thread manager calls FinalizeResolving on
	 * the thread object.
	 *
	 * OpHostListener is never called from this function, even if it fails,
	 * so resolver remains a valid pointer when this function returns.
	 *
	 * @param resolver resolver to run in worker thread
	 *
	 * @return
	 *  - OpStatus::ERR_NULL_POINTER if resolver is NULL
	 *  - OpStatus::ERR if manager is not active
	 *  - OpStatus::ERR_NO_MEMORY on OOM
	 *  - OpStatus::OK if resolver is assigned to worker thread or enqueued
	 */
	OP_STATUS StartResolving(WindowsThreadedHostResolver *resolver);

	/**
	 * This function is called when status of resolve operation is received on the main thread.
	 * Manager locates thread object associated with the resolve operation and calls
	 * FinalizeResolving on that object. Thread object is then assigned to the next resolve
	 * operation.
	 *
	 * @param id id of WindowsHostResolverThread object
	 * @param status status returned by WindowsThreadedHostResolver2::PerformResolving
	 */
	void OnResolverUpdate(int id, OpHostResolver::Error status);

	static WindowsHostResolverManager* GetInstance();

private:
	BOOL m_active;            ///< TRUE if this manager accepts resolvers, FALSE after ShutDown()
	List<WindowsThreadedHostResolver> m_lookup_queue; ///< resolvers waiting to be assigned to working threads
	List<WindowsHostResolverThread> m_threads;        ///< all thread objects (both idle and running resolvers)
	unsigned m_threads_count; ///< number of object in m_threads list
	static const unsigned MAX_THREADS_COUNT = 8; ///< maximum number of objects in m_threads list

	/**
	 * Assign WindowsThreadedHostResolver2 objects from m_lookup_queue to idle worker threads.
	 */
	void AdvanceResolving();
};


/**
 * Manages worker thread used to run host resolvers.
 */
class WindowsHostResolverThread : public ListElement<WindowsHostResolverThread>
{
public:
	WindowsHostResolverThread();
	~WindowsHostResolverThread();

	OP_STATUS Init();

	/**
	 * Return unique id of this object.
	 * The id is used to identify this object in messages posted from worker thread to the main thread.
	 */
	int Id() const { return static_cast<int>(m_unique_id); }

	/**
	 * Run resolver in worker thread.
	 *
	 * If successful then this object takes ownership of the resolver.
	 *
	 * @param resolver
	 *
	 * @return TRUE if resolver is assigned to worker thread, FALSE if worker thread is busy and rejects resolver
	 */
	BOOL StartResolving(WindowsThreadedHostResolver* resolver);

	/**
	 * Pass status of resolve operation to Core if still relevant and get ready
	 * for new resolve operation.
	 *
	 * If resolve operation was cancelled then resolver object is deleted, otherwise
	 * this function calls FinalizeResolving function in resolver object, which transfer
	 * ownership of resolver object back to WindowsHostResolver2 object that created it.
	 *
	 * @param status status returned by WindowsThreadedHostResolver2::PerformResolving
	 */
	void FinalizeResolving(OpHostResolver::Error status);

private:
	OpAutoHANDLE m_start_event; ////< signals to worker thread to stop idling and either run resolver or exit (depending on m_active)
	OpAutoHANDLE m_thread;      ///< worker thread
	WindowsThreadedHostResolver* m_resolver; ///< resolver currently assigned to this thread
	OpCriticalSection m_resolver_cs; ///< critical section for m_resolver
	                                 ///< should be used when pointer is changed on main thread or read on worker thread
	volatile BOOL m_active;     ///< worker thread exits after this flag is set to FALSE
	UINT32 m_unique_id;         ///< identifies this object in messages posted via g_thread_tools

	static DWORD WINAPI ThreadProc(LPVOID param);

	/// Main function of worker thread.
	void ResolveMain();
};

/**
 * Performs host name lookups.
 *
 * Object of this class can be used to perform single resolve operation.
 */
class WindowsThreadedHostResolver : public ListElement<WindowsThreadedHostResolver>
{
public:
	WindowsThreadedHostResolver(WindowsHostResolver* public_resolver);

	virtual ~WindowsThreadedHostResolver();

	OP_STATUS Init(const uni_char* server_name);

	/**
	 * Start resolving server name.
	 *
	 * This function is usually called from worker thread.
	 *
	 * @param status gets status to be passed to OpHostResolverListener
	 *
	 * @return
	 *  - OpStatus::OK on success
	 *  - OpStatus::ERR_NULL_POINTER if server name is empty
	 *  - OpStatus::ERR if resolve operation failed or was cancelled
	 */
	OP_STATUS PerformResolving(OpHostResolver::Error* status);

	/**
	 * Pass status of resolve operation to Core. This function is called when status of
	 * PerformsResolving is received on the main thread.
	 *
	 * @param status status returned by PerformResolving
	 */
	void FinalizeResolving(OpHostResolver::Error status);

	/**
	 * Get number of Internet addresses available from resolve operation.
	 * This function can be only called after FinalizeResolving notifies
	 * Core that resolve operation is finished.
	 */
	UINT GetAddressCount() const { return m_address_count; }

	/**
	 * Get an Internet address from resolve operation.
	 * This function can be only called after FinalizeResolving notifies
	 * Core that resolve operation is finished.
	 *
	 * @param socket_address get the address
	 * @param index index of the address, should be less than GetAddressCount()
	 *
	 * @return
	 *  - OpStatus::ERR_NULL_POINTER if socket_address is NULL
	 *  - OpStatus::ERR_OUT_OF_RANGE if index is outside the range
	 *  - OpStatus::OK on success
	 */
	OP_STATUS GetAddress(OpSocketAddress* socket_address, UINT index) const;

	/**
	 * Result of this resolve operation is no longer needed.
	 * This object will be simply deleted when it is not busy.
	 */
	void Cancel() { m_public_resolver = NULL; }
	/**
	 * Return whether this resolve operation was cancelled by Core.
	 * This function can be called both from main thread and from worker thread.
	 */
	BOOL IsCancelled() const { return m_public_resolver == NULL; }

	/**
	 * Lock or unlock this resolver.
	 * When resolver is locked it is referenced by a worker thread, so it cannot be deleted.
	 *
	 * @param lock TRUE to lock this resolver, FALSE to unlock it
	 */
	void SetLock(BOOL lock) { m_locked = lock; }

	/**
	 * If resolver is busy it is either enqueued in the manager or is running on a worker
	 * thread, so it cannot be deleted by WindowsHostResolver2 object that created it.
	 */
	BOOL IsBusy() const { return InList() || m_locked; }

private:
	WindowsHostResolver* m_public_resolver; ///< object that created this resolver and is notified when resolve operation is finished
	OpString8 m_server_name;           ///< name to resolve
	BOOL m_locked;                     ///< TRUE if this is resolving (DSK-109098)
	static const int MAX_ATTEMPTS = 3; ///< maximum number of resolving attempts before giving up

	addrinfo* m_addresses; ///< resolved addresses
	UINT m_address_count;  ///< number of Internet Family addresses in m_addresses

	// Simple iterator used to speed up consecutive calls to GetAddress.
	mutable addrinfo* m_address_ptr; ///< last address returned by GetAddress or NULL
	mutable UINT m_address_idx;      ///< index of last address returned by GetAddress
};


class WindowsHostResolver : public OpHostResolver
{
public:
	// Implementation of OpHostResolver interface.
	virtual OP_STATUS Resolve(const uni_char* hostname);
	virtual OP_STATUS ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error);
	virtual OP_STATUS GetLocalHostName(OpString* local_hostname, Error* error);
	virtual UINT GetAddressCount() { return m_real_host_resolver->GetAddressCount(); }
	virtual OP_STATUS GetAddress(OpSocketAddress* socket_address, UINT index) { return m_real_host_resolver->GetAddress(socket_address, index); }

	virtual ~WindowsHostResolver() { DestroyResolver(); }

	/**
	 * Pass final status of resolve operation to Core.
	 *
	 * Call to this function means that m_real_host_resolver is no longer referenced by manager or
	 * thread object and ownership is transferred back to this object.
	 *
	 * @param status status of resolve operation
	 */
	void OnResolvingFinished(OpHostResolver::Error status);

protected:
	friend class OpHostResolver; // only OpHostResolver can create objects of this class

	WindowsHostResolver(OpHostResolverListener *listener)
		: m_real_host_resolver(NULL)
		, m_listener(listener)
	{}

private:
	OpHostResolverListener* m_listener;
	WindowsThreadedHostResolver* m_real_host_resolver; ///< Object that performs host resolving in a worker thread.
														///< Created by this object. Resolve() transfers ownership to the manager.
														///< OnResolvingFinished() transfers ownership back to this object.

	/**
	 * Destroy m_real_host_resolver. The object is deleted immediately if it is not busy, otherwise it is cancelled
	 * and will be deleted by manager or thread object when it is safe to do so.
	 */
	void DestroyResolver();
};

#endif	//	__WINDOWSHOSTRESOLVER_H
