 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  */

/** @file WindowsHostResolver2.cpp
  * 
  * Windows implementation of the Opera host resolver.
  *
  * @author Johan Borg
  *
  */

#include "core/pch.h"

#include "platforms/windows/network/WindowsHostResolver2.h"
#include "platforms/windows/network/WindowsSocket2.h"
#include "modules/pi/OpThreadTools.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"


OP_STATUS OpHostResolver::Create(OpHostResolver** host_resolver, OpHostResolverListener* listener)
{
	if (!g_windows_host_resolver_manager->IsActive())
		return OpStatus::ERR;

	if (!WindowsSocket2::m_manager)
	{
		OpAutoPtr<WindowsSocketManager> manager(OP_NEW(WindowsSocketManager, ()));
		RETURN_OOM_IF_NULL(manager.get());
		RETURN_IF_ERROR(manager->Init());
		WindowsSocket2::m_manager = manager.release();
	}

	*host_resolver = OP_NEW(WindowsHostResolver, (listener));
	return *host_resolver ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}



WindowsThreadedHostResolver::WindowsThreadedHostResolver(WindowsHostResolver* public_resolver)
	: m_public_resolver(public_resolver)
	, m_locked(FALSE)
	, m_addresses(NULL)
	, m_address_count(0)
	, m_address_ptr(NULL)
	, m_address_idx(0)
{}

WindowsThreadedHostResolver::~WindowsThreadedHostResolver()
{
	if (m_addresses)
		freeaddrinfo(m_addresses);
}

OP_STATUS WindowsThreadedHostResolver::Init(const uni_char* server_name)
{
	return m_server_name.Set(server_name);
}

OP_STATUS WindowsThreadedHostResolver::PerformResolving(OpHostResolver::Error *resolver_error)
{
	if(m_server_name.IsEmpty())
	{
		*resolver_error = OpHostResolver::HOST_ADDRESS_NOT_FOUND;
		return OpStatus::ERR_NULL_POINTER;
	}

	// Core might have cancelled resolve operation while worker thread running this object
	// was waiting for CPU time.
	if (IsCancelled())
	{
		*resolver_error = OpHostResolver::ERROR_HANDLED;
		return OpStatus::ERR;
	}

	addrinfo* addresses = NULL;

	for (int attempts = 0; attempts < MAX_ATTEMPTS; ++attempts)
	{
		// No hints to getaddrinfo - GetAddress and GetAddressCount will simply skip addresses
		// with ai_family different from AF_INET and AF_INET6.
		int ret_val = getaddrinfo(m_server_name.CStr(), NULL, NULL, &addresses);

		// Core might have cancelled resolve operation while worker thread running this object
		// was blocked in getaddrinfo.
		if (IsCancelled())
		{
			if (ret_val == 0 && addresses)
			{
				freeaddrinfo(addresses);
			}
			*resolver_error = OpHostResolver::ERROR_HANDLED;
			return OpStatus::ERR;
		}

		int wsa_error = 0;

		if (ret_val != 0)
			wsa_error = WSAGetLastError();
		else if (addresses == NULL)
			wsa_error = WSANO_DATA;
		else
			break; // success

		switch (wsa_error)
		{
		case WSATRY_AGAIN:
			break;
		case WSANO_DATA:
		case WSAHOST_NOT_FOUND:
		case WSASERVICE_NOT_FOUND:
			*resolver_error = OpHostResolver::HOST_ADDRESS_NOT_FOUND;
			return OpStatus::ERR;
		case WSAENETDOWN:
		case WSAENOBUFS:
		case WSAEFAULT:
		case WSANO_RECOVERY:
		default:
			*resolver_error = OpHostResolver::NETWORK_ERROR;
			return OpStatus::ERR;
		}
	}

	unsigned count = 0;
	if (addresses)
	{
		addrinfo *current_addr = addresses;
		while (current_addr)
		{
			if(current_addr->ai_family == AF_INET6 || current_addr->ai_family == AF_INET)
			{
				++ count;
			}
			current_addr = current_addr->ai_next;
		}
	}

	// Core will not call GetAddress or GetAddressCount before OpHostListener is notified about status of this resolve operation, so
	// setting m_address and m_address_count here is thread-safe.

	if (count == 0)
	{
		m_address_count = 0;
		if (addresses)
			freeaddrinfo(addresses);
		*resolver_error = OpHostResolver::HOST_ADDRESS_NOT_FOUND;
		return OpStatus::ERR;
	}
	else
	{
		m_addresses = addresses;
		m_address_count = count;
		*resolver_error = OpHostResolver::NETWORK_NO_ERROR;
		return OpStatus::OK;
	}
}

void WindowsThreadedHostResolver::FinalizeResolving(OpHostResolver::Error status)
{
	OP_ASSERT(!IsCancelled()); // if cancelled then caller should simply delete this object
	if (m_public_resolver)
		m_public_resolver->OnResolvingFinished(status);
}

OP_STATUS WindowsThreadedHostResolver::GetAddress(OpSocketAddress* op_socket_address, UINT index) const
{
	if (!op_socket_address)
		return OpStatus::ERR_NULL_POINTER;

	// Core usually calls GetAddress(addr, i) in a loop for i = 0..GetAddressCount().
	// This function stores last result in m_address_ptr and its index in m_address_idx
	// to avoid repeatedly iterating over the list when called in that manner.

	if (index < m_address_idx || m_address_ptr == NULL)
	{
		m_address_idx = 0;
		m_address_ptr = m_addresses;
	}

	while (m_address_ptr)
	{
		if(m_address_ptr->ai_family == AF_INET6 || m_address_ptr->ai_family == AF_INET)
		{
			if (m_address_idx == index)
				break;
			else
				++ m_address_idx;
		}
		m_address_ptr = m_address_ptr->ai_next;
	}

	if (m_address_ptr)
	{
		SOCKET_ADDRESS socket_address;
		socket_address.iSockaddrLength = m_address_ptr->ai_addrlen;
		socket_address.lpSockaddr = m_address_ptr->ai_addr;
		return static_cast<WindowsSocketAddress2*>(op_socket_address)->SetAddress(&socket_address);
	}

	return OpStatus::ERR_OUT_OF_RANGE;
}



void WindowsHostResolver::DestroyResolver()
{
	if (m_real_host_resolver)
	{
		if (m_real_host_resolver->IsBusy())
		{
			m_real_host_resolver->Cancel(); // will be deleted by WindowsHostResolverThread or WindowsHostResolverManager
											// depending on which of them keeps it busy now
		}
		else
		{
			OP_DELETE(m_real_host_resolver);
		}
		m_real_host_resolver = NULL;
	}
}

OP_STATUS WindowsHostResolver::Resolve(const uni_char* hostname)
{
	DestroyResolver();

	OpAutoPtr<WindowsThreadedHostResolver> resolver = OP_NEW(WindowsThreadedHostResolver, (this));
	RETURN_OOM_IF_NULL(resolver.get());
	RETURN_IF_ERROR(resolver->Init(hostname));
	RETURN_IF_ERROR(g_windows_host_resolver_manager->StartResolving(resolver.get()));
	m_real_host_resolver = resolver.release();
	return OpStatus::OK;
}

OP_STATUS WindowsHostResolver::ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, OpHostResolver::Error* error)
{
	OP_ASSERT(0); // Do you *really* need to do this operation in synchronous mode. It is no longer recommended

	// Synchronous resolving, so FinalizeResolving is not called (it would notify the listener).
	// This pointer must be passed to resolver anyway, otherwise resolver will be in "cancelled" state.
	WindowsThreadedHostResolver resolver(this);
	RETURN_IF_ERROR(resolver.Init(hostname));
	RETURN_IF_ERROR(resolver.PerformResolving(error));
	return resolver.GetAddress(socket_address, 0);
}

OP_STATUS WindowsHostResolver::GetLocalHostName(OpString* local_hostname, OpHostResolver::Error* error)
{
	char hostname[1024];

	if(!gethostname(hostname,1024) )
	{
		// documentation in OpHostResolver states that error argument must be ignored in this case
		return local_hostname->Set(hostname);
	}
	else
	{
		*error = OpHostResolver::NETWORK_ERROR;
		return OpStatus::ERR;
	}
}

void WindowsHostResolver::OnResolvingFinished(OpHostResolver::Error status)
{
	if (m_listener)
	{
		if(status != OpHostResolver::NETWORK_NO_ERROR)
			m_listener->OnHostResolverError(this, status);
		else
			m_listener->OnHostResolved(this);
	}
}



WindowsHostResolverThread::WindowsHostResolverThread()
	: m_resolver(NULL)
	, m_active(FALSE)
{
	m_unique_id = g_unique_id_counter++;
}

WindowsHostResolverThread::~WindowsHostResolverThread()
{
	// Opera is shutting down and Core should have cancelled
	// all host resolvers.
	OP_ASSERT(!m_resolver || m_resolver->IsCancelled());

	if (m_active)
	{
		m_active = FALSE;
		SetEvent(m_start_event);
		if(WaitForSingleObject(m_thread, 3000) != WAIT_OBJECT_0)
			TerminateThread(m_thread, 0); // Cleanup. Last resort. May crash if we are unlucky.
		if (m_resolver && m_resolver->IsCancelled())
			OP_DELETE(m_resolver); // critical section not necessary, because thread is terminated
	}
}

OP_STATUS WindowsHostResolverThread::Init()
{
	m_start_event.reset(CreateEvent(NULL, FALSE, FALSE, NULL));
	RETURN_OOM_IF_NULL(m_start_event.get());
	m_thread.reset(CreateThread(NULL, 32*1024, ThreadProc, this, 0, NULL));
	RETURN_OOM_IF_NULL(m_thread.get());
	m_active = TRUE;

	return OpStatus::OK;
}

BOOL WindowsHostResolverThread::StartResolving(WindowsThreadedHostResolver *resolver)
{
	if(!resolver || m_resolver || !m_active)
		return FALSE;

	m_resolver_cs.Enter();
	m_resolver = resolver;
	m_resolver->SetLock(TRUE);
	m_resolver_cs.Leave();

	SetEvent(m_start_event);
	return TRUE;
}

void WindowsHostResolverThread::FinalizeResolving(OpHostResolver::Error status)
{
	WinAutoLock resolver_lock(&m_resolver_cs);
	m_resolver->SetLock(FALSE);
	if (m_resolver->IsCancelled())
		OP_DELETE(m_resolver);
	else
		m_resolver->FinalizeResolving(status);
	m_resolver = NULL;
}

DWORD WINAPI WindowsHostResolverThread::ThreadProc(LPVOID param)
{
	WindowsHostResolverThread* _this = static_cast<WindowsHostResolverThread*>(param);
	_this->ResolveMain();
	return 1;
}

void WindowsHostResolverThread::ResolveMain()
{
	while (true)
	{
		WaitForSingleObject(m_start_event, INFINITE);

		if (!m_active)
			break;

		OpHostResolver::Error resolver_status = OpHostResolver::NETWORK_NO_ERROR;
		OP_STATUS status;
		bool post_message = false;

		{
			WinAutoLock resolver_lock(&m_resolver_cs);
			if (m_resolver)
			{
				status = m_resolver->PerformResolving(&resolver_status);
				if (OpStatus::IsError(status) && resolver_status == OpHostResolver::NETWORK_NO_ERROR)
					resolver_status = OpHostResolver::NETWORK_ERROR;
				post_message = true;
			}
		}

		if (!m_active)
			break;

		if (post_message)
			g_thread_tools->PostMessageToMainThread(MSG_RESOLVER_UPDATE, Id(), resolver_status);
	}
}



WindowsHostResolverManager* WindowsHostResolverManager::GetInstance()
{
	static WindowsHostResolverManager s_manager;
	return &s_manager;
}

WindowsHostResolverManager::~WindowsHostResolverManager()
{
	// ShutDown should be called first.
	OP_ASSERT(!m_active);
	OP_ASSERT(m_threads.First() == NULL);
}

void WindowsHostResolverManager::ShutDown()
{
	if (m_active)
	{
		OP_PROFILE_METHOD("Host resolver threads terminated");

		m_active = FALSE;
		while (m_threads.First())
		{
			WindowsHostResolverThread* thread = m_threads.First();
			thread->Out();
			OP_DELETE(thread); // this will terminate the thread
			--m_threads_count;
		}
	}
}

// called from WindowsOpMainThread.cpp
void HandleResolverMessages(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_RESOLVER_UPDATE)
		g_windows_host_resolver_manager->OnResolverUpdate(par1, static_cast<OpHostResolver::Error>(par2));
}

void WindowsHostResolverManager::OnResolverUpdate(int id, OpHostResolver::Error status)
{
	if (!m_active)
		return;

	WindowsHostResolverThread* thread = m_threads.First();
	while (thread && thread->Id() != id)
	{
		thread = thread->Suc();
	}

	if (thread)
	{
		thread->FinalizeResolving(status);
	}

	AdvanceResolving();
}

void WindowsHostResolverManager::AdvanceResolving()
{
	OP_ASSERT(m_threads_count > 0);

	while (m_lookup_queue.First())
	{
		WindowsThreadedHostResolver* resolver = m_lookup_queue.First();
		if (resolver->IsCancelled())
		{
			resolver->Out();
			OP_DELETE(resolver);
		}
		else
		{
			WindowsHostResolverThread* thread = m_threads.First();
			while (thread)
			{
				if (thread->StartResolving(resolver))
				{
					// ownership is transferred to the thread object
					resolver->Out();
					break;
				}
				thread = thread->Suc();
			}
			if (!thread)
				break;
		}
	}
}

OP_STATUS WindowsHostResolverManager::StartResolving(WindowsThreadedHostResolver *resolver)
{
	if (!resolver)
		return OpStatus::ERR_NULL_POINTER;

	if (!IsActive())
		return OpStatus::ERR;

	OP_ASSERT(!resolver->InList());

	while (m_threads_count < MAX_THREADS_COUNT)
	{
		WindowsHostResolverThread* thread = OP_NEW(WindowsHostResolverThread, ());

		if (!thread)
			break;

		if (OpStatus::IsError(thread->Init()))
		{
			OP_DELETE(thread);
			break;
		}
		
		thread->Into(&m_threads);
		++ m_threads_count;
	}

	if (m_threads_count == 0)
		return OpStatus::ERR_NO_MEMORY;

	resolver->Into(&m_lookup_queue);

	AdvanceResolving();

	return OpStatus::OK;
}
