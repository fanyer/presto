/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined(NEW_PI_MH)
#include "platforms/mac/pi/MacOpMessageHandler.h"
#include "modules/hardcore/timer/optimer.h"
#include "platforms/mac/model/CWidgetEvents.h"

#include "platforms/mac/util/URestoreQDState.h"
#include "platforms/mac/util/UTempResFileChange.h"

#include "modules/hardcore/mh/constant.h"

extern EventQueueRef gMainEventQueue;
extern ThreadID gMainThreadID;


MacOpMessageLoop::MacOpMessageLoop()
{
//	mh = NULL;
	mHandlerRef = NULL;
	mHandlerUPP = NULL;
}

MacOpMessageLoop::~MacOpMessageLoop()
{
	if (mHandlerRef)
	{
		RemoveEventHandler(mHandlerRef);
	}
	if (mHandlerUPP)
	{
		DisposeEventHandlerUPP(mHandlerUPP);
	}
}

OP_STATUS MacOpMessageLoop::Init()
{
	static EventTypeSpec appEventList[] =
	{
		{kEventClassOperaPlatformIndependent, kEventOperaMessage}
	};

	mHandlerUPP = NewEventHandlerUPP(sMacMessageLoopHandler);
	InstallEventHandler(GetApplicationEventTarget(), mHandlerUPP, GetEventTypeCount(appEventList), appEventList, this, &mHandlerRef);

	return OpStatus::OK;
}

OP_STATUS MacOpMessageLoop::Run()
{
	return OpStatus::OK;
}

OP_STATUS MacOpMessageLoop::PostMessage(UINT32 msg, void *data, UINT32 param1, UINT32 param2)
{
//	if(!mh)
//	{
//		return OpStatus::OK;
//	}

	EventRef operaEvent = NULL;

	OSStatus err = CreateEvent(NULL, kEventClassOperaPlatformIndependent, kEventOperaMessage,
								0, kEventAttributeUserEvent,
								&operaEvent);

	if(err == noErr)
	{
		err = SetEventParameter(operaEvent,
								   kEventParamOperaMessage,
								   typeInteger,
								   sizeof(UINT32),
								   &msg);

		err |= SetEventParameter(operaEvent,
								   kEventParamOperaMessageData,
								   typeInteger,
								   sizeof(void *),
								   &data);

		err |= SetEventParameter(operaEvent,
								   kEventParamOperaMessageParam1,
								   typeInteger,
								   sizeof(UINT32),
								   &param1);

		err |= SetEventParameter(operaEvent,
								   kEventParamOperaMessageParam2,
								   typeInteger,
								   sizeof(UINT32),
								   &param2);

		if (err == noErr)
		{
			EventPriority priority;
			if (msg >= MSG_LOW_PRI_START && msg <= MSG_LOW_PRI_END) {
				priority = kEventPriorityLow;
			} else {
				priority = kEventPriorityStandard;
			}
			PostEventToQueue(gMainEventQueue, operaEvent, priority);
		}
	}

	ReleaseEvent(operaEvent);

	return OpStatus::OK;
}

#ifdef USE_UNTESTED_DISPATCH_CODE
pascal Boolean OperaEventComparatorProc(EventRef inEvent, void * inCompareData)
{
	if(inEvent)
	{
		UInt32 carbonEventClass = GetEventClass(inEvent);

		if(carbonEventClass == kEventClassOperaPlatformIndependent || carbonEventClass == kEventClassOpera)
		{
			return true;
		}
	}

	return false;
}
#endif

void MacOpMessageLoop::DispatchAllPostedMessagesNow()
{
#ifdef USE_UNTESTED_DISPATCH_CODE
	OpVector<EventRef> eventList;
	OSStatus err;
	EventRef nextEvent;

	do
	{
		nextEvent = FindSpecificEventInQueue(gMainEventQueue, OperaEventComparatorProc, NULL);
		if(nextEvent)
		{
			if(noErr == RemoveEventFromQueue(gMainEventQueue, nextEvent))
			{
				eventList.Add(&nextEvent);
			}
		}
	} while(nextEvent);


	if(eventList.GetCount() > 0)
	{
		for(int i = 0; i < eventList.GetCount(); i++)
		{
			nextEvent = *eventList.Get(i);
			err = SendEventToEventTarget(nextEvent, GetEventDispatcherTarget());
			ReleaseEvent(nextEvent);
		}
	}
#endif
}

//void MacOpMessageLoop::SetMessageHandler(OpMessageHandler *mh)
//{
//	this->mh = mh;
//}

OSStatus MacOpMessageLoop::MessageLoopHandler(EventRef inEvent)
{
	OSStatus err;
	UINT32 message;
	void *data;
	UINT32 param1;
	UINT32 param2;

	err = GetEventParameter(inEvent, kEventParamOperaMessage, typeInteger, NULL, sizeof(message), NULL, &message);
	err |= GetEventParameter(inEvent, kEventParamOperaMessageData, typeInteger, NULL, sizeof(data), NULL, &data);
	err |= GetEventParameter(inEvent, kEventParamOperaMessageParam1, typeInteger, NULL, sizeof(param1), NULL, &param1);
	err |= GetEventParameter(inEvent, kEventParamOperaMessageParam2, typeInteger, NULL, sizeof(param2), NULL, &param2);

//	if (err == noErr)
//		mh->MessageReceived(message, data, param1, param2);

	return noErr;
}

//NOTE: The commented-away g_isInside stuff looks like it is needed to prevent Ecmascript crashes when using stack-modal
// dialogs (like "alert('foo');" inside the html body), but
// It has the nasty side-effect that ALL dialogs stop responding to mouse & keyboard events (effectively making opera useless).

pascal OSStatus MacOpMessageLoop::sMacMessageLoopHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData)
{
	OSStatus retVal = noErr;
	ThreadID currThread = 0;
	GetCurrentThread(&currThread);

	if (gMainThreadID == currThread)
	{
		URestoreQDState savedState;

		MacOpMessageLoop *messageHandler = (MacOpMessageLoop *)inUserData;
		retVal = messageHandler->MessageLoopHandler(inEvent);
	}
	else
	{
		::PostEventToQueue(gMainEventQueue, inEvent, kEventPriorityLow);
		YieldToThread(gMainThreadID);
	}

	return retVal;
}


#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
OP_STATUS MacOpMessageLoop::RunSlice()
{
	//	You can try this one if the other code doesn't work. This one will handle all sorts of things waiting,
	//	including idle events, until 10 milliseconds have passed. You can of course change the value,
	//	but be warned that it WILL spend all this time, even if there isn't anything to do.

//	RunCurrentEventLoop(10 * kEventDurationMillisecond);


	// This will only handle system events, not timeouts (such as delayed messages or plugin idler).
	// Note that, like the Windows version, it may deadlock.
	EventRef 			nextEvent;
	while (noErr == ::ReceiveNextEvent(0, NULL, kEventDurationNoWait, true, &nextEvent))
	{
		::SendEventToEventTarget(nextEvent, GetEventDispatcherTarget());
		::ReleaseEvent(nextEvent);
	}
	return OpStatus::OK;
}
#endif // MESSAGELOOP_RUNSLICE_SUPPORT

#endif
