/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined(NEW_PI_MH)
#include "platforms/mac/pi/MacOpTimer.h"

#include "platforms/mac/util/URestoreQDState.h"
#include "platforms/mac/util/UTempResFileChange.h"

UINT32 MacOpTimer::timer_id = 0;
EventLoopTimerUPP MacOpTimer::timerUPP = NULL;
extern ThreadID gMainThreadID;
extern EventLoopRef gMainEventLoop;
extern Boolean g_isInside;


MacOpTimer::MacOpTimer()
{
	m_listener = 0;
	m_id = 0;
	m_interval = 0;
	m_startTime = 0;
	m_timerRef = 0;
	if (!timerUPP) {
		timerUPP = NewEventLoopTimerUPP(sOpTimerEvent);
	}
}

MacOpTimer::~MacOpTimer()
{
	if (m_timerRef) {
		EventLoopTimerRef timer = m_timerRef;
		m_timerRef = NULL;
		::RemoveEventLoopTimer(timer);
	}
}

OP_STATUS MacOpTimer::Init()
{
	m_id = ++timer_id;
	return OpStatus::OK;
}

void MacOpTimer::opTimerEvent(EventLoopTimerRef timerRef)
{
	EventLoopTimerRef timer = m_timerRef;
	OpTimerListener *listener = m_listener;
	if (timer) {
		if (listener)
		{
			listener->OnTimeOut(this);
		}
	}
}

pascal void MacOpTimer::sOpTimerEvent(EventLoopTimerRef timerRef, void *opTimer)
{
	ThreadID currThread = 0;
	GetCurrentThread(&currThread);
	if (gMainThreadID == currThread)
	{
		URestoreQDState savedState;
		UTempResFileChange resFile;
		MacOpTimer *macOpTimer = (MacOpTimer *) opTimer;
		macOpTimer->opTimerEvent(timerRef);
	}
}

void MacOpTimer::Start(UINT32 ms)
{
	UnsignedWide ticks;
	Microseconds(&ticks);
	long long msecs = ticks.hi;
	msecs <<= 32;
	msecs |= ticks.lo;
	m_startTime = msecs / 1000;
	m_interval = ms;

	EventTimerInterval fireDelay = ms * kEventDurationMillisecond;

	if (m_timerRef)
	{
		SetEventLoopTimerNextFireTime(m_timerRef,fireDelay);
	}
	else
	{
		InstallEventLoopTimer(gMainEventLoop,//GetMainEventLoop(),
							 fireDelay,
							 kEventDurationNoWait,
							 timerUPP,
							 this,
							 &m_timerRef);
	}
}

UINT32 MacOpTimer::Stop()
{
	EventLoopTimerRef timer = m_timerRef;
	m_timerRef = NULL;
	if (timer)
		::RemoveEventLoopTimer(timer);
	UnsignedWide ticks;
	Microseconds(&ticks);
	long long msecs = ticks.hi;
	msecs <<= 32;
	msecs |= ticks.lo;
	UINT32 endTime = msecs / 1000;
	UINT32 elapsed = endTime - m_startTime;

	if( (elapsed < m_interval) && (elapsed >= 0) )
		return (UINT32)(m_interval - elapsed);
	else
		return 0;
}

void MacOpTimer::SetTimerListener(OpTimerListener *listener)
{
	m_listener = listener;
}

UINT32 MacOpTimer::GetId()
{
	return m_id;
}

#endif
