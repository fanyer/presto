/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/posix_ipc/posix_ipc_process_manager.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "modules/pi/OpLocale.h"
#include "platforms/posix_ipc/pi/posix_ipc_binary_selector.h"
#include "platforms/posix_ipc/posix_ipc.h"
#include "platforms/posix_ipc/posix_ipc_event_handler.h"
#include "platforms/posix_ipc/posix_ipc_process.h"
#include "platforms/posix_ipc/posix_ipc_process_token.h"

#include <sys/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>

class PosixIpcProcessManager::PipeHolder
{
public:
	PipeHolder() { ReleaseRead(); ReleaseWrite(); }
	OP_STATUS Init();
	~PipeHolder();
	
	void ReleaseRead() { m_fd[0] = -1; }
	void ReleaseWrite() { m_fd[1] = -1; }

	int GetRead() { return m_fd[0]; }
	int GetWrite() { return m_fd[1]; }

private:
	OP_STATUS SetNonBlocking(int fd);
	int m_fd[2];
};

OP_STATUS PosixIpcProcessManager::PipeHolder::Init()
{
	if (pipe(m_fd) != 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(SetNonBlocking(m_fd[0]));
	return SetNonBlocking(m_fd[1]);
}

PosixIpcProcessManager::PipeHolder::~PipeHolder()
{
	if (m_fd[0] != -1)
		close(m_fd[0]);
	if (m_fd[1] != -1)
		close(m_fd[1]);
}

OP_STATUS PosixIpcProcessManager::PipeHolder::SetNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS PosixIpcProcessManager::CreateComponent(int& peer, OpMessageAddress requester, OpComponentType type)
{
	OpAutoPtr<PipeHolder> manager_to_process (OP_NEW(PipeHolder, ()));
	RETURN_OOM_IF_NULL(manager_to_process.get());
	RETURN_IF_ERROR(manager_to_process->Init());

	OpAutoPtr<PipeHolder> process_to_manager (OP_NEW(PipeHolder, ()));
	RETURN_OOM_IF_NULL(process_to_manager.get());
	RETURN_IF_ERROR(process_to_manager->Init());

	peer = m_nextid++;
	OpAutoPtr<PosixIpcProcessToken> token (OP_NEW(PosixIpcProcessToken,
		(requester, PosixIpcBinarySelector::GetMappedComponentType(type), peer, manager_to_process->GetRead(), process_to_manager->GetWrite())));
	if (!token.get() || !token->Encode())
		return OpStatus::ERR_NO_MEMORY;

	OpString8 log_path;
	RETURN_IF_ERROR(GetLogPath(log_path));

	OpAutoPtr<PosixIpcBinarySelector> selector (PosixIpcBinarySelector::Create(type, token->Encode(), log_path.CStr()));
	RETURN_OOM_IF_NULL(selector.get());

	pid_t pid = fork();
	if (pid == -1)
	{
		return OpStatus::ERR;
	}
	else if (pid != 0)
	{
		OpAutoPtr<IpcHandle> ipc (OP_NEW(IpcHandle, (process_to_manager->GetRead(), manager_to_process->GetWrite())));
		RETURN_OOM_IF_NULL(ipc.get());
		process_to_manager->ReleaseRead();
		manager_to_process->ReleaseWrite();

		OpAutoPtr<PosixIpcProcess> proc (OP_NEW(PosixIpcProcess, (type, peer, *this, ipc.get())));
		RETURN_OOM_IF_NULL(proc.get());
		ipc.release();
		RETURN_IF_ERROR(proc->Init());

		proc->SetPid(pid);
		proc->Into(&m_processes);
		proc.release();
	}
	else
	{
		manager_to_process->ReleaseRead();
		process_to_manager->ReleaseWrite();
		manager_to_process.reset();
		process_to_manager.reset();

		if (execvp(selector->GetBinaryPath(), selector->GetArgsArray()) == -1)
		{
			/* Starting Opera sub-process failed. _exit() to prevent any other code
			 * being executed. For example, if we used exit() here, we'd invoke
			 * atexit() hooks etc.
			 */
			fprintf(stderr, "opera [%d]: can't start sub-process\n", getpid());
			_exit(1);
		}
	}

	return OpStatus::OK;
}

OP_STATUS PosixIpcProcessManager::GetLogPath(OpString8& log_path)
{
	OpString log_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_CRASHLOG_FOLDER, log_folder));

	OpLocale* locale;
	RETURN_IF_ERROR(OpLocale::Create(&locale));
	OpAutoPtr<OpLocale> holder(locale);
	return locale->ConvertToLocaleEncoding(&log_path, log_folder);
}

OP_STATUS PosixIpcProcessManager::SendMessage(OpTypedMessage* message)
{
	OP_ASSERT(message != NULL);
	PosixIpcProcess* process = GetProcess(message->GetDst().cm);
	if (!process)
	{
		OP_DELETE(message);
		return OpStatus::ERR;
	}

	return process->Send(message);
}

PosixIpcProcess* PosixIpcProcessManager::GetProcess(int cm_id)
{
	for (PosixIpcProcess* process = m_processes.First(); process; process = process->Suc())
	{
		if (process->GetComponentManagerId() == cm_id)
			return process;
	}

	return NULL;
}

OP_STATUS PosixIpcProcessManager::ProcessEvents(unsigned int next_timeout)
{
	PosixIpcEventHandler handler(this);
	for (PosixIpcProcess* process = m_processes.First(); process; process = process->Suc())
		RETURN_IF_ERROR(handler.AddEventSource(process->GetHandle()));

	m_timeout = next_timeout == UINT_MAX ? -1 : g_component_manager->GetRuntimeMS() + next_timeout;
	bool stop_running = false;

	return handler.HandleEvents(&m_timeout, stop_running);
}

void PosixIpcProcessManager::RequestRunSlice(unsigned int limit)
{
	m_timeout = g_component_manager->GetRuntimeMS() + (double)limit;
}

void PosixIpcProcessManager::OnComponentCreated(OpMessageAddress address)
{
	PosixIpcProcess* process = GetProcess(address.cm);
	if (!process)
		return;

	process->MarkInitialized();
}

void PosixIpcProcessManager::OnReadReady(int fd)
{
	for (PosixIpcProcess *process = m_processes.First(), *next = NULL; process; process = next)
	{
		next = process->Suc();
		if (OpStatus::IsError(process->Read(fd)))
			KillProcess(process);
	}
}

void PosixIpcProcessManager::OnWriteReady(int fd)
{
	for (PosixIpcProcess *process = m_processes.First(), *next = NULL; process; process = next)
	{
		next = process->Suc();
		if (OpStatus::IsError(process->Write(fd)))
			KillProcess(process);
	}
}

void PosixIpcProcessManager::OnError(int fd, int err)
{
	for (PosixIpcProcess *process = m_processes.First(), *next = NULL; process; process = next)
	{
		next = process->Suc();
		if (process->HasReadFd(fd) || process->HasWriteFd(fd))
			KillProcess(process);
	}
}

void PosixIpcProcessManager::OnDetach(int fd)
{
	for (PosixIpcProcess *process = m_processes.First(), *next = NULL; process; process = next)
	{
		next = process->Suc();
		if (process->HasReadFd(fd) || process->HasWriteFd(fd))
			KillProcess(process, false);
	}
}

void PosixIpcProcessManager::OnSourceError(IpcHandle* source)
{
	for (PosixIpcProcess *process = m_processes.First(), *next = NULL; process; process = next)
	{
		next = process->Suc();
		if (process->GetHandle() == source)
			KillProcess(process);
	}
}

void PosixIpcProcessManager::KillProcess(PosixIpcProcess* process, bool detach)
{
	g_component_manager->PeerGone(process->GetComponentManagerId());
	process->Out();

	if (detach)
	{
		g_posix_selector->Detach(this, process->GetHandle()->GetReadPipe());
		g_posix_selector->Detach(this, process->GetHandle()->GetWritePipe());
	}

	OP_DELETE(process);
}

OP_STATUS PosixIpcProcessManager::RegisterPipe(int fd, bool write)
{
	return g_posix_selector->Watch(fd, write ? PosixSelector::NONE : PosixSelector::READ, this);
}
