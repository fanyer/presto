/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSTIMER_H
#define WINDOWSTIMER_H

class OpCriticalSection;

// 3 ms timer resolution when needed
#define INCREASED_TIMER_RESOLUTION	3

class WindowsTimerListener
{
public:
	virtual ~WindowsTimerListener() {};

	/** Will be called on timeout, the timer will stop after timeout.
	    NOTE this will be run in a different thread.

		@param timer The timer that has timed out
	*/
	virtual void OnTimeOut(class WindowsTimer* timer) = 0;
};

/** This is more accurate than OpTimer. OpTimer uses core message
	queue, thus can be delayed by the messages in the queue. However
	be aware that this timer runs in different thread.
*/
class WindowsTimer
{
public:	
	WindowsTimer(WindowsTimerListener* listener);
	~WindowsTimer();

	OP_STATUS Start(UINT32 ms);

	// Not thread safe, don't call this for the same object in more than
	// 1 thread
	void Stop();

private:
	WindowsTimer(const WindowsTimer&);
	void UpdateHighResolutionTimerCount(bool increase);
	void OnTimeOut();

	HANDLE m_timer_queue_timer;
	WindowsTimerListener* m_listener;
	bool m_high_resolution;

	static int s_high_resolution_timers_count;
	static OpCriticalSection s_criticalSection;
	static void NTAPI QueueTimerProc(PVOID lParam, BOOLEAN);
};

class WindowsTimerResolutionManager
{
public:
	~WindowsTimerResolutionManager();

	static WindowsTimerResolutionManager* GetInstance();

	void UpdatePowerStatus();	// update the battery/ac status
	void UpdateTimerResolution(BOOL need_high_resolution);

	// @return 0 means default resolution, otherwise INCREASED_TIMER_RESOLUTION
	DWORD GetTimerResolution() {return m_timer_resolution;}

private:
	WindowsTimerResolutionManager();

	DWORD m_timer_resolution;
	BOOL m_is_using_battery;	// TRUE if we're using battery, otherwise FALSE

	static OpAutoPtr<WindowsTimerResolutionManager> s_time_resolution_manager;
};

#endif //WINDOWSTIMER_H
