/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "WindowsTimer.h"
#include "platforms/windows/utils/sync_primitives.h"
#include "platforms/windows/win_handy.h"

// 15ms is the typical default timer resolution
#define HIGH_RESOLUTION_TIMER_THRESHOLD 15

int WindowsTimer::s_high_resolution_timers_count;
OpCriticalSection WindowsTimer::s_criticalSection;

bool IsSystemVista()
{
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	GetVersionEx(&osvi);
	return osvi.dwMajorVersion >= 6;
}

void NTAPI WindowsTimer::QueueTimerProc(PVOID lParam, BOOLEAN)
{
	WindowsTimer* timer = (WindowsTimer*)lParam;
	timer->OnTimeOut();
}

WindowsTimer::WindowsTimer(WindowsTimerListener* listener)
	:m_timer_queue_timer(NULL)
	,m_listener(listener)
	,m_high_resolution(false)
{
	OP_ASSERT(listener);
}

WindowsTimer::~WindowsTimer()
{
	Stop();
}

OP_STATUS WindowsTimer::Start(UINT32 ms)
{
	Stop();

	if (ms < HIGH_RESOLUTION_TIMER_THRESHOLD)
		UpdateHighResolutionTimerCount(true);

	BOOL ret = CreateTimerQueueTimer(&m_timer_queue_timer, NULL, (WAITORTIMERCALLBACK)QueueTimerProc, this, ms, 0, 0);
	return ret ? OpStatus::OK : OpStatus::ERR;
}

void WindowsTimer::Stop()
{
	if (m_timer_queue_timer)
	{
		HANDLE timer = m_timer_queue_timer;
		m_timer_queue_timer = NULL;
		::DeleteTimerQueueTimer(NULL, timer, INVALID_HANDLE_VALUE);
	}

	UpdateHighResolutionTimerCount(false);
}

void WindowsTimer::UpdateHighResolutionTimerCount(bool increase)
{
	WinAutoLock lock(&s_criticalSection);

	if (m_high_resolution == increase)
		return;

	m_high_resolution = increase;

	s_high_resolution_timers_count += increase ? 1 : -1;
	OP_ASSERT(s_high_resolution_timers_count >= 0);
	WindowsTimerResolutionManager::GetInstance()->UpdateTimerResolution(s_high_resolution_timers_count > 0);
}

void WindowsTimer::OnTimeOut()
{
	UpdateHighResolutionTimerCount(false);

	if (m_timer_queue_timer)
		m_listener->OnTimeOut(this);
}

/*static*/
OpAutoPtr<WindowsTimerResolutionManager> WindowsTimerResolutionManager::s_time_resolution_manager;

/*static*/
WindowsTimerResolutionManager* WindowsTimerResolutionManager::GetInstance()
{
	if (s_time_resolution_manager.get() == NULL)
		s_time_resolution_manager =OP_NEW(WindowsTimerResolutionManager, ());

	return s_time_resolution_manager.get();
}

WindowsTimerResolutionManager::WindowsTimerResolutionManager()
	: m_timer_resolution(0)
	, m_is_using_battery(FALSE)
{
	UpdatePowerStatus();
}

WindowsTimerResolutionManager::~WindowsTimerResolutionManager()
{
	if(m_timer_resolution)
	{
		timeEndPeriod(m_timer_resolution);
		m_timer_resolution = 0;
//		OutputDebugString(UNI_L("*** RESETTING timer resolution to 10ms again\n"));
	}
}

void WindowsTimerResolutionManager::UpdatePowerStatus()
{
	SYSTEM_POWER_STATUS power_status;

	// Get power status
	if(GetSystemPowerStatus(&power_status))
	{
		// see http://msdn.microsoft.com/en-us/library/aa373232(v=vs.85).aspx
		m_is_using_battery = (power_status.ACLineStatus == 0);
	}
	if(m_is_using_battery)
	{
		// update timer resolution to make sure we're not using more power than we need to
		UpdateTimerResolution(FALSE);
	}
}

void WindowsTimerResolutionManager::UpdateTimerResolution(BOOL need_high_resolution)
{
	if (need_high_resolution && !m_is_using_battery)
	{
		if(!m_timer_resolution)
		{
			m_timer_resolution = INCREASED_TIMER_RESOLUTION;
			timeBeginPeriod(m_timer_resolution);
//			OutputDebugString(UNI_L("*** Setting timer resolution to 3ms\n"));
		}
	}
	else
	{
		// Don't turn off high resolution timer when on XP and not using battery once it's enabled (DSK-366405)
		if(m_timer_resolution && (m_is_using_battery || IsSystemVista()))
		{
			timeEndPeriod(m_timer_resolution);
			m_timer_resolution = 0;
//			OutputDebugString(UNI_L("*** RESETTING timer resolution to 10ms again\n"));
		}
	}
}
