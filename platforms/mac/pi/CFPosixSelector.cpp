/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if !defined(POSIX_OK_SIMPLE_SELECTOR)
#include "platforms/mac/pi/CFPosixSelector.h"
#include "platforms/posix/net/posix_network_address.h"
#include "platforms/mac/util/OpDelayedDelete.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"

//#define _DEBUG_CF_SOCKET

class CFSocketWrapper
{
public:
	CFSocketWrapper(int fd, PosixSelector::Type mode, PosixSelectListener *listener, bool accepts)
		: m_socket(NULL), m_run_loop_source(NULL), m_run_loop(NULL), m_fd(fd), m_mode(mode), m_listener(listener), m_accepts(accepts)
	{
#ifdef _DEBUG_CF_SOCKET
		printf("CFSocketWrapper(%d)\n", fd);
#endif
		CFSocketContext ctx;
		ctx.version = 0;
		ctx.info = this;
		ctx.retain = NULL;
		ctx.release = NULL;
		ctx.copyDescription = NULL;
		m_socket = CFSocketCreateWithNative(kCFAllocatorDefault, m_fd, kCFSocketReadCallBack|kCFSocketWriteCallBack|kCFSocketConnectCallBack, SocketCallback, &ctx);
		CFSocketSetSocketFlags(m_socket, CFSocketGetSocketFlags(m_socket) &~ (kCFSocketAutomaticallyReenableReadCallBack|kCFSocketAutomaticallyReenableWriteCallBack));
		CFSocketDisableCallBacks(m_socket, kCFSocketReadCallBack|kCFSocketWriteCallBack);
		m_run_loop_source = CFSocketCreateRunLoopSource(NULL, m_socket, 0);
		if(m_run_loop_source)
		{
#ifdef NO_CARBON
			m_run_loop = GetNSCFRunLoop();
#else
			m_run_loop = (CFRunLoopRef)GetCFRunLoopFromEventLoop(GetMainEventLoop());
#endif
			CFRunLoopAddSource(m_run_loop, m_run_loop_source, kCFRunLoopCommonModes);
			CFRunLoopSourceSignal(m_run_loop_source);
			CFRunLoopWakeUp(m_run_loop);
		}
		// we don't want our recv() calls to block
		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);	
	}
	~CFSocketWrapper() {
#ifdef _DEBUG_CF_SOCKET
		printf("~CFSocketWrapper(%d)\n", m_fd);
#endif
		m_listener = NULL;
		if (CFSocketIsValid(m_socket))
			CFSocketInvalidate(m_socket);
		CFRelease(m_socket);
		if(m_run_loop_source)
			CFRelease(m_run_loop_source);
	}
	void SetMode(PosixSelector::Type mode) {
		if ((mode & PosixSelector::READ) != (m_mode & PosixSelector::READ))
			if (mode & PosixSelector::READ)
				CFSocketEnableCallBacks(m_socket, kCFSocketReadCallBack);
			else
				CFSocketDisableCallBacks(m_socket, kCFSocketReadCallBack);
		if ((mode & PosixSelector::WRITE) != (m_mode & PosixSelector::WRITE))
			if (mode & PosixSelector::WRITE)
				CFSocketEnableCallBacks(m_socket, kCFSocketWriteCallBack);
			else
				CFSocketDisableCallBacks(m_socket, kCFSocketWriteCallBack);
		m_mode = mode;
	}
	void Detach() {
		CFSocketDisableCallBacks(m_socket, kCFSocketReadCallBack|kCFSocketWriteCallBack);
		CFSocketInvalidate(m_socket);
		m_listener = NULL;
		new OpDelayedDelete<CFSocketWrapper>(this);
	}
	CFSocketRef Socket() {return m_socket;}
	int File() {return m_fd;}
	const PosixSelectListener *Listener() {return m_listener;}
	void OnDetach() { m_listener->OnDetach(m_fd); }
private:
	static void SocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info) {
		CFSocketWrapper* self = (CFSocketWrapper*) info;
		SInt32 err = 0;
#ifdef _DEBUG_CF_SOCKET
		printf("SocketCallback(%d, %d)\n", self->m_fd, type);
#endif
		if (self && self->m_listener)
		{
			switch (type)
			{
				case kCFSocketConnectCallBack:
					if(data)
						err = *((SInt32*)data);
#ifdef _DEBUG_CF_SOCKET
					printf("   kCFSocketConnectCallBack: %li\n", err);
#endif
					if(err == 0) {
						self->m_mode = PosixSelector::READ;
						self->m_listener->OnConnected(self->m_fd);
					}
					else {
						CFSocketDisableCallBacks(self->m_socket, kCFSocketReadCallBack|kCFSocketWriteCallBack);
						self->m_listener->OnError(self->m_fd, err);
					}

					break;
				case kCFSocketReadCallBack:
					if (self->m_mode & PosixSelector::READ) {
						self->m_listener->OnReadReady(self->m_fd);
						if (self->m_listener && (self->m_mode & PosixSelector::READ) && !self->m_accepts) {
							CFSocketEnableCallBacks(self->m_socket, kCFSocketReadCallBack);
						}
					}
					break;
				case kCFSocketWriteCallBack:
					if (self->m_mode & PosixSelector::WRITE) {
						self->m_listener->OnWriteReady(self->m_fd);
						if (self->m_listener && (self->m_mode & PosixSelector::WRITE) && !self->m_accepts) {
							CFSocketEnableCallBacks(self->m_socket, kCFSocketWriteCallBack);
						}
					}
					break;
			}
		}
	}
	CFSocketRef m_socket;
	CFRunLoopSourceRef m_run_loop_source;
	CFRunLoopRef m_run_loop;
	int m_fd;
	PosixSelector::Type m_mode;
	PosixSelectListener *m_listener;
	bool m_accepts;
};

OP_STATUS PosixSelector::Create(PosixSelector * &selector)
{
	signal(SIGPIPE, SIG_IGN); 	// ignore SIGPIPE
	
	selector = new CFPosixSelector;
	return (selector ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
}

CFPosixSelector::CFPosixSelector()
{
}

CFPosixSelector::~CFPosixSelector()
{
	while (m_ports.GetCount())
		m_ports.Remove(0)->OnDetach();	
}

void CFPosixSelector::Detach(const PosixSelectListener *whom, int fd)
{
	for (int i=m_ports.GetCount() - 1; i >= 0; i--)
	{
		CFSocketWrapper* port = m_ports.Get(i);
		if (port && port->Listener()==whom && port->File()==fd) {
			m_ports.Remove(i)->Detach();
		}
	}
}

void CFPosixSelector::Detach(const PosixSelectListener *whom)
{
	for (int i=m_ports.GetCount() - 1; i >= 0; i--)
	{
		CFSocketWrapper* port = m_ports.Get(i);
		if (port && port->Listener()==whom) {
			m_ports.Remove(i)->Detach();
		}
	}
}

void CFPosixSelector::SetMode(const PosixSelectListener * listener, int fd, Type mode)
{
	for (UINT32 i=0; i<m_ports.GetCount(); i++)
	{
		CFSocketWrapper* port = m_ports.Get(i);
		if (port && port->Listener()==listener && port->File()==fd) {
			m_ports.Get(i)->SetMode(mode);
		}
	}
}

void CFPosixSelector::SetMode(const PosixSelectListener * listener, Type mode)
{
	for (UINT32 i=0; i<m_ports.GetCount(); i++)
	{
		CFSocketWrapper* port = m_ports.Get(i);
		if (port && port->Listener()==listener) {
			m_ports.Get(i)->SetMode(mode);
		}
	}
}

OP_STATUS CFPosixSelector::Watch(int fd, Type mode, PosixSelectListener *listener, const class OpSocketAddress *connect, bool accepts)
{
	CFSocketWrapper* port = new CFSocketWrapper(fd, mode, listener, accepts);
	m_ports.Add(port);
	if (connect) {
		((PosixNetworkAddress*)connect)->Connect(fd);
	}
	return OpStatus::OK;
}

#endif // !POSIX_OK_SIMPLE_SELECTOR
