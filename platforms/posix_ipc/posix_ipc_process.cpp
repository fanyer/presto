/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/posix_ipc/posix_ipc_process.h"

#include "platforms/posix_ipc/posix_ipc.h"
#include "platforms/posix_ipc/posix_ipc_process_manager.h"
#include "platforms/posix/posix_selector.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

PosixIpcProcess::PosixIpcProcess(OpComponentType type, int cm_id, PosixIpcProcessManager& mgr, IpcHandle* handle)
	: m_killonexit(true)
	, m_pid(-1)
	, m_ipc(handle)
	, m_cm_id(cm_id)
	, m_type(type)
	, m_manager(mgr)
{
}

PosixIpcProcess::~PosixIpcProcess()
{
	OP_DELETE(m_ipc);

	if (m_killonexit && m_pid != -1)
	{
		Kill();
		WaitForExit();
	}
}

OP_STATUS PosixIpcProcess::Init()
{
	RETURN_IF_ERROR(m_manager.RegisterReadPipe(m_ipc->GetReadPipe()));
	return m_manager.RegisterWritePipe(m_ipc->GetWritePipe());
}

bool PosixIpcProcess::HasReadFd(int fd)
{
	return m_ipc->GetReadPipe() == fd;
}

bool PosixIpcProcess::HasWriteFd(int fd)
{
	return m_ipc->GetWritePipe() == fd;
}

OP_STATUS PosixIpcProcess::HandleRemainingBytes()
{
	while (m_ipc->MessageAvailable())
	{
		OpTypedMessage* msg = NULL;
		RETURN_IF_ERROR(m_ipc->ReceiveMessage(msg));
		RETURN_IF_MEMORY_ERROR(g_component_manager->DeliverMessage(msg));
	}
	return OpStatus::OK;
}

OP_STATUS PosixIpcProcess::Send(OpTypedMessage* msg)
{
	RETURN_IF_ERROR(m_ipc->SendMessage(msg));
	g_posix_selector->SetMode(&m_manager, m_ipc->GetWritePipe(), PosixSelector::WRITE);
	return OpStatus::OK;
}

OP_STATUS PosixIpcProcess::Read(int fd)
{
	RETURN_IF_ERROR(HandleRemainingBytes());

	if (fd == m_ipc->GetReadPipe())
	{
		RETURN_IF_ERROR(m_ipc->Receive());
		RETURN_IF_ERROR(HandleRemainingBytes());
	}

	return OpStatus::OK;
}

OP_STATUS PosixIpcProcess::Write(int fd)
{
	if (fd != m_ipc->GetWritePipe())
		return OpStatus::OK;

	RETURN_IF_ERROR(m_ipc->Send());
	if (!m_ipc->WantSend())
		g_posix_selector->SetMode(&m_manager, m_ipc->GetWritePipe(), PosixSelector::NONE);

	return OpStatus::OK;
}

void PosixIpcProcess::Kill()
{
	kill(m_pid, SIGKILL);
}

int PosixIpcProcess::WaitForExit()
{
	int code;
	waitpid(m_pid, &code, 0);
	return code;
}
