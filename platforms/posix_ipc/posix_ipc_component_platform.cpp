/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/posix_ipc/posix_ipc_component_platform.h"

#include "platforms/posix_ipc/posix_ipc.h"
#include "platforms/posix_ipc/posix_ipc_event_handler.h"
#include "platforms/posix_ipc/posix_ipc_process_token.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#include <poll.h>

OP_STATUS PosixIpcComponentPlatform::Create(PosixIpcComponentPlatform*& component, const char* process_token)
{
	OpAutoPtr<PosixIpcProcessToken> token (PosixIpcProcessToken::Decode(process_token));
	if (!token.get())
		return OpStatus::ERR;

	OpAutoPtr<PosixIpcComponentPlatform> platform;
	platform.reset(CreateComponent(token->GetComponentType()));

	if (!platform.get())
		platform.reset(OP_NEW(PosixIpcComponentPlatform, ()));

	if (!platform.get())
		return OpStatus::ERR;

	// Create a component manager
	OpComponentManager* manager;
	RETURN_IF_ERROR(OpComponentManager::Create(&manager, token->GetComponentManagerId(), platform.get()));
	g_component_manager = manager;

	// Create IPC channel
	platform->m_ipc = OP_NEW(IpcHandle, (token->GetReadPipe(), token->GetWritePipe()));
	if (!platform->m_ipc)
		return OpStatus::ERR_NO_MEMORY;

	// Answer over IPC channel
	RETURN_IF_ERROR(g_component_manager->HandlePeerRequest(token->GetRequester(), token->GetComponentType()));

	component = platform.release();
	return OpStatus::OK;
}

PosixIpcComponentPlatform::PosixIpcComponentPlatform()
	: m_ipc(NULL)
	, m_timeout(-1)
	, m_nesting_level(0)
	, m_stop_running(false)
{
}

PosixIpcComponentPlatform::~PosixIpcComponentPlatform()
{
	OP_DELETE(g_component_manager);
#ifdef POSIX_IPC_COMPONENT_MANAGER_KEY
	pthread_key_delete(g_component_manager_key);
#else
	g_component_manager = NULL;
#endif

	OP_DELETE(m_ipc);
}

void PosixIpcComponentPlatform::Exit()
{
	m_stop_running = true;
}

void PosixIpcComponentPlatform::RequestRunSlice(unsigned int limit)
{
	m_timeout = g_component_manager->GetRuntimeMS() + (double)limit;
}

OP_STATUS PosixIpcComponentPlatform::RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS PosixIpcComponentPlatform::SendMessage(OpTypedMessage* message)
{
	return m_ipc->SendMessage(message);
}

OP_STATUS PosixIpcComponentPlatform::ProcessEvents(unsigned int timeout, EventFlags flags)
{
	if (timeout == UINT_MAX)
		m_timeout = -1;
	else
		m_timeout = g_component_manager->GetRuntimeMS() + (double)timeout;

	m_nesting_level++;
	int value = ProcessMessages();
	m_nesting_level--;

	return value == 0 ? OpStatus::OK : OpStatus::ERR;
}

void PosixIpcComponentPlatform::OnComponentDestroyed(OpMessageAddress address)
{
	if (address.IsSameComponentManager(g_component_manager->GetAddress()) &&
		g_component_manager->GetComponentCount() == 0)
		Exit();
}

int PosixIpcComponentPlatform::ProcessMessages()
{
	PosixIpcEventHandler handler(NULL);
	RETURN_VALUE_IF_ERROR(handler.AddEventSource(m_ipc), 1);
	RETURN_VALUE_IF_ERROR(handler.HandleEvents(m_nesting_level > 0 ? &m_timeout : NULL, m_stop_running), 1);

	return 0;
}
