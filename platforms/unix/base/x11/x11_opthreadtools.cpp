/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#ifdef USE_OP_THREAD_TOOLS

#include "platforms/unix/base/x11/x11_opthreadtools.h"

#include "modules/hardcore/mh/mh.h"

#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

X11OpThreadTools* X11OpThreadTools::s_instance = 0;

OP_STATUS OpThreadTools::Create(OpThreadTools **new_obj)
{
	OpAutoPtr<X11OpThreadTools> thread_tools (OP_NEW(X11OpThreadTools, ()));
	if (!thread_tools.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(thread_tools->Init());
	*new_obj = thread_tools.release();

	return OpStatus::OK;
}

OP_STATUS X11OpThreadTools::Init()
{
	m_main_thread = pthread_self();
	s_instance = this;

	// Setup empty signal handler for SIGIO, which we use to interrupt
	// polling in the main thread
	struct sigaction action;
	action.sa_handler = &X11OpThreadTools::HandleSIGIO;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);

	struct sigaction oldaction;
	if (sigaction(SIGIO, &action, &oldaction) != 0)
		return OpStatus::ERR;

	OP_ASSERT(oldaction.sa_handler == SIG_DFL || oldaction.sa_handler == SIG_IGN);

	return m_mutex.Init();
}

void X11OpThreadTools::HandleSIGIO(int)
{
}

void* X11OpThreadTools::Allocate(size_t size)
{
	return malloc(size);
}

void X11OpThreadTools::Free(void *memblock)
{
	free(memblock);
}

OP_STATUS X11OpThreadTools::PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay)
{
	if (pthread_self() == m_main_thread)
		return g_main_message_handler->PostMessage(msg, par1, par2, delay);

	MainThreadMessage* message = OP_NEW(MainThreadMessage, (msg, par1, par2, delay));
	if (!message)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_mutex.Acquire());
	message->Into(&m_message_list);
	RETURN_IF_ERROR(m_mutex.Release());

	// Signal main thread that there are messages
	pthread_kill(m_main_thread, SIGIO);

	return OpStatus::OK;
}

OP_STATUS X11OpThreadTools::SendMessageToMainThread(OpTypedMessage* message)
{
	if (pthread_self() == m_main_thread)
		return g_component_manager->SendMessage(message);

	MainThreadMessage* thread_msg = OP_NEW(MainThreadMessage, (message));
	if (!thread_msg)
	{
		OP_DELETE(message);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(m_mutex.Acquire());
	thread_msg->Into(&m_message_list);
	RETURN_IF_ERROR(m_mutex.Release());

	// Signal main thread that there are messages
	pthread_kill(m_main_thread, SIGIO);

	return OpStatus::OK;
}

bool X11OpThreadTools::DeliverMessages()
{
	bool processed_messages = false;

	DesktopMutexLock lock(m_mutex);

	MainThreadMessage* message;
	while (message = m_message_list.First())
	{
		if (message->typed_msg)
		{
			OpStatus::Ignore(g_component_manager->SendMessage(message->typed_msg));
			message->typed_msg = NULL;
		}
		else
		{
			OpStatus::Ignore(g_main_message_handler->PostMessage(message->msg, message->par1, message->par2, message->delay));
		}
		message->Out();
		OP_DELETE(message);
		processed_messages = true;
	}

	return processed_messages;
}

#endif // USE_OP_THREAD_TOOLS
