/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(USE_OP_MAIN_THREAD) && !defined(POSIX_OK_MAIN_THREAD)

#ifdef MACGOGI
#include "platforms/macgogi/pi/MacOpMainThread.h"
#include "platforms/macgogi/pi/MacOpSystemInfo.h"
#else
#include "platforms/mac/pi/MacOpMainThread.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"
#endif
#include "modules/hardcore/mh/mh.h"

extern ThreadID gMainThreadID;
extern EventQueueRef gMainEventQueue;

OP_STATUS OpMainThread::Create(OpMainThread** new_main_thread)
{
	OP_ASSERT(new_main_thread != NULL);
	*new_main_thread = new (ELeave) MacOpMainThread();
	if(*new_main_thread == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

MacOpMainThread::MacOpMainThread()
	: mEventHandlerRef(NULL)
	, mEventHandlerUPP(NULL)
{
	mEventHandlerUPP = NewEventHandlerUPP(HandlePostEvent);
	EventTypeSpec events[] = {
		{kOperaMessage, kOperaMessage}
	};
	if (mEventHandlerUPP)
		InstallEventHandler(GetApplicationEventTarget(), mEventHandlerUPP, GetEventTypeCount(events), events, NULL, &mEventHandlerRef);
}

MacOpMainThread::~MacOpMainThread()
{
	if (mEventHandlerRef)
		RemoveEventHandler(mEventHandlerRef);
	mEventHandlerRef = NULL;
}

OP_STATUS MacOpMainThread::Init()
{
	return OpStatus::OK;
}

void* MacOpMainThread::Allocate(size_t size)
{
	return malloc(size);
}

void MacOpMainThread::Free(void* memblock)
{
	free(memblock);
}

OP_STATUS MacOpMainThread::PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay)
{
#ifdef MACGOGI
	ThreadID currThread = 0;
	GetCurrentThread(&currThread);
	if (gMainThreadID == currThread)
#else
	if (g_op_system_info->IsInMainThread())
#endif
	{
		if (g_main_message_handler)
		{
			g_main_message_handler->PostMessage(msg, par1, par2, delay);
			return OpStatus::OK;
		}
	}
	else
	{
		EventRef event = NULL;
		MacCreateEvent(kCFAllocatorDefault, kOperaMessage, kOperaMessage, 0, 0, &event);
		SetEventParameter(event, kOperaMessageId, typeLongInteger, 4, &msg);
		SetEventParameter(event, kOperaMessageParam1, typeLongInteger, 4, &par1);
		SetEventParameter(event, kOperaMessageParam2, typeLongInteger, 4, &par2);
		SetEventParameter(event, kOperaDelay, typeLongInteger, 4, &delay);
		PostEventToQueue(gMainEventQueue, event, kEventPriorityStandard);
		ReleaseEvent(event);
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OSStatus MacOpMainThread::HandlePostEvent(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
#ifdef MACGOGI
	ThreadID currThread = 0;
	GetCurrentThread(&currThread);
	if (gMainThreadID == currThread)
#else
	if (g_op_system_info->IsInMainThread())
#endif
	{
		OSStatus err;
		OpMessage msg;
		MH_PARAM_1 par1;
		MH_PARAM_2 par2;
		unsigned long delay;
		err = GetEventParameter(inEvent, kOperaMessageId, typeLongInteger, NULL, 4, NULL, &msg);
		err |= GetEventParameter(inEvent, kOperaMessageParam1, typeLongInteger, NULL, 4, NULL, &par1);
		err |= GetEventParameter(inEvent, kOperaMessageParam2, typeLongInteger, NULL, 4, NULL, &par2);
		err |= GetEventParameter(inEvent, kOperaDelay, typeLongInteger, NULL, 4, NULL, &delay);
		if (err == noErr)
		{
			if (g_main_message_handler)
			{
				if(delay > 0)
				{
					g_main_message_handler->PostMessage(msg, par1, par2, delay);
				}
				else
				{
					g_main_message_handler->HandleMessage(msg, par1, par2);
				}
			}
		}
	}
	else
	{
		PostEventToQueue(gMainEventQueue, inEvent, kEventPriorityStandard);
		YieldToThread(gMainThreadID);
	}
	return noErr;
}

#endif //USE_OP_MAIN_THREAD && !POSIX_OK_MAIN_THREAD
