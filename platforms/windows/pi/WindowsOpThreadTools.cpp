/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(USE_OP_THREAD_TOOLS)

#include "WindowsOpThreadTools.h"

#include "platforms/windows/CustomWindowMessages.h"

extern void HandleResolverMessages(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
extern void HandleAsyncFileMessages(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

/*static*/
MainThreadSender* MainThreadSender::s_instance = NULL;
BOOL MainThreadSender::s_destroyed = FALSE;

/*static*/
MainThreadSender* MainThreadSender::Construct()
{
	OP_ASSERT(!s_instance); //Construct should only ever be called once, when initializing the process.
	OP_ASSERT(!s_destroyed); //We shouldn't create a new instance after destroying

	s_instance = OP_NEW(MainThreadSender, ());
	return s_instance;
}

/*static*/
MainThreadSender* MainThreadSender::GetInstance()
{
	return s_instance;
}

void MainThreadSender::Destruct()
{
	OP_DELETE(s_instance);
	s_instance = NULL;
	s_destroyed = TRUE;
}

MainThreadSender::MainThreadSender()
{
	WindowsOpComponentPlatform* component_platform = static_cast<WindowsOpComponentPlatform*>(g_component_manager->GetPlatform());
	component_platform->SetWMListener(WM_MAIN_THREAD_MESSAGE, this);
	m_message_window = component_platform->GetMessageWindow();
	m_active = TRUE;
}

MainThreadSender::~MainThreadSender()
{
	if (m_active)
		Finalize();
	else
	{
		m_vector_lock.Enter();
		m_messages.DeleteAll();
		m_vector_lock.Leave();
	}
}

void MainThreadSender::Finalize()
{
	OP_ASSERT(m_active);

	EncapsulatedMessage *msg;
	while ((msg = m_messages.Get(0)) != NULL)
	{
		ProcessMessage(msg);
	}

	OpTypedMessage *message;
	while ((message = m_typed_messages.Get(0)) != NULL)
	{
		ProcessMessage(message);
	}

	m_active = FALSE;
}

void MainThreadSender::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam)
		ProcessMessage(reinterpret_cast<OpTypedMessage*>(wParam));
	else
		ProcessMessage(reinterpret_cast<EncapsulatedMessage*>(wParam));

}

LRESULT MainThreadSender::ProcessMessage(EncapsulatedMessage* msg)
{
# if (!defined(PLUGIN_WRAPPER) && !defined(OPERA_MAPI))
	if (m_active)
	{
		switch (msg->msg)
		{
			// TODO Remove these special cases (arjanl 20071213)
			case MSG_ASYNC_FILE_WRITTEN:
			case MSG_ASYNC_FILE_READ:
			case MSG_ASYNC_FILE_DELETED:
			case MSG_ASYNC_FILE_FLUSHED:
			case MSG_ASYNC_FILE_SYNC:
				HandleAsyncFileMessages(msg->msg, msg->par1, msg->par2);
				break;
			case MSG_RESOLVER_UPDATE:
				HandleResolverMessages(msg->msg, msg->par1, msg->par2);
				break;
			default:
				if (g_main_message_handler)
					g_main_message_handler->PostMessage(msg->msg, msg->par1, msg->par2, msg->delay);
				break;
		}
	}
	m_vector_lock.Enter();
	m_messages.Delete(msg);
	m_vector_lock.Leave();
#endif //(!defined(PLUGIN_WRAPPER) && !defined(OPERA_MAPI))
	return 0;
}

LRESULT MainThreadSender::ProcessMessage(OpTypedMessage* message)
{
	if (m_active)
		g_component_manager->SendMessage(message);

	m_vector_lock.Enter();
	m_typed_messages.RemoveByItem(message);
	m_vector_lock.Leave();

	return 0;
}

OP_STATUS MainThreadSender::PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay)
{
	if (m_active == FALSE)
		return OpStatus::ERR;

# if (!defined(PLUGIN_WRAPPER) && !defined(OPERA_MAPI))
	EncapsulatedMessage* message = new EncapsulatedMessage(msg, par1, par2, delay);

	if (!message)
		return OpStatus::ERR_NO_MEMORY;

	m_vector_lock.Enter();
	m_messages.Add(message);
	m_vector_lock.Leave();

	BOOL done = ::SendNotifyMessage(m_message_window, WM_MAIN_THREAD_MESSAGE, (WPARAM)message, 0);

	return done ? OpStatus::OK : OpStatus::ERR;
#else //PLUGIN_WRAPPER
	return OpStatus::ERR_NOT_SUPPORTED;
#endif //PLUGIN_WRAPPER
}

OP_STATUS MainThreadSender::SendMessageToMainThread(OpTypedMessage* message)
{
	if (m_active == FALSE)
		return OpStatus::ERR;

	m_vector_lock.Enter();
	m_typed_messages.Add(message);
	m_vector_lock.Leave();

	BOOL done = ::SendNotifyMessage(m_message_window, WM_MAIN_THREAD_MESSAGE, (WPARAM)message, 1);

	return done ? OpStatus::OK : OpStatus::ERR;
}



OP_STATUS OpThreadTools::Create(OpThreadTools** new_main_thread)
{
	OP_ASSERT(new_main_thread != NULL);
	*new_main_thread = OP_NEW(WindowsOpThreadTools, ());
	if (!new_main_thread)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void* WindowsOpThreadTools::Allocate(size_t size)
{
	return op_malloc(size);
}

void WindowsOpThreadTools::Free(void* memblock)
{
	op_free(memblock);
}

OP_STATUS WindowsOpThreadTools::PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay)
{
	return MainThreadSender::GetInstance()->PostMessageToMainThread(msg, par1, par2, delay);
}

OP_STATUS WindowsOpThreadTools::SendMessageToMainThread(OpTypedMessage* message)
{
	return MainThreadSender::GetInstance()->SendMessageToMainThread(message);
}

#endif // USE_OP_THREAD_TOOLS
