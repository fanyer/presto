/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/unix/product/pluginwrapper/plugin_component_platform.h"

#include "platforms/posix_ipc/posix_ipc.h"

OP_STATUS PluginComponentPlatform::RunGTK2Loop()
{
	struct LoopResetter {
		LoopResetter(ToolkitLoop*& loop, ToolkitLoop* value) : m_loop(loop) { m_loop = value; }
		~LoopResetter() { OP_DELETE(m_loop); m_loop = NULL; }
		ToolkitLoop*& m_loop;
	} resetter(m_loop, ToolkitLoop::Create(ToolkitLoop::GTK2, m_ipc->GetReadPipe(), m_ipc->GetWritePipe()));

	if (!m_loop)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_loop->WatchReadDescriptor(this));

	if (m_ipc->WantSend())
		RETURN_IF_ERROR(m_loop->WatchWriteDescriptor(this));

	return m_loop->Run();
}

void PluginComponentPlatform::Exit()
{
	if (m_loop)
		m_loop->Exit();
	PosixIpcComponentPlatform::Exit();
}

OP_STATUS PluginComponentPlatform::SendMessage(OpTypedMessage* message)
{
	RETURN_IF_ERROR(PosixIpcComponentPlatform::SendMessage(message));
	if (m_ipc->WantSend() && m_loop)
		return m_loop->WatchWriteDescriptor(this);

	return OpStatus::OK;
}

OP_STATUS PluginComponentPlatform::OnReadReady(bool& keep_listening)
{
	RETURN_IF_ERROR(m_ipc->Receive());

	while (m_ipc->MessageAvailable())
	{
		OpTypedMessage* msg = NULL;
		RETURN_IF_ERROR(m_ipc->ReceiveMessage(msg));
		RETURN_IF_MEMORY_ERROR(g_component_manager->DeliverMessage(msg));
	}

	return OpStatus::OK;
}

OP_STATUS PluginComponentPlatform::OnWriteReady(bool& keep_listening)
{
	if (m_ipc->WantSend())
		RETURN_IF_ERROR(m_ipc->Send());
	keep_listening = m_ipc->WantSend();
	return OpStatus::OK;
}
