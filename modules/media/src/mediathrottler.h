/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIATHROTTLER_H
#define MEDIATHROTTLER_H

#if VIDEO_THROTTLING_FPS_HIGH > 0

#ifndef VIDEO_THROTTLING_FPS_LOW
#define VIDEO_THROTTLING_FPS_LOW VIDEO_THROTTLING_FPS_HIGH
#else
#if VIDEO_THROTTLING_FPS_LOW <= 0
#undef VIDEO_THROTTLING_FPS_LOW
#define VIDEO_THROTTLING_FPS_LOW 1
#endif // VIDEO_THROTTLING_FPS_LOW <= 0
#endif // ! VIDEO_THROTTLING_FPS_LOW

#include "modules/hardcore/timer/optimer.h"

class MediaThrottlerListener
{
public:
	virtual void OnFrameUpdateAllowed() = 0;
};

class MediaThrottler : public OpTimerListener
{
public:
	MediaThrottler(MediaThrottlerListener* listener = NULL);
	virtual ~MediaThrottler() { Break(); }
	void OnTimeOut(OpTimer* timer);
	virtual void Break();
	virtual BOOL IsFrameUpdateAllowed();
#ifdef SELFTEST
	void SetSelftestThrottlingAlwaysNeeded(BOOL val) { m_selftest_throttling_always_needed = val; }
	void SetFrameMinUpdateInterval(UINT32 interval) { m_frame_update_min_interval = interval; }
#endif
private:
	double m_last_allowed_update_timestamp;
	double m_next_allowed_update_timestamp;
	double m_last_load_check_timestamp;
	double m_time_to_update;
	UINT32 m_frame_update_min_interval;
	BOOL m_throttling_needed;
	unsigned int m_current_throttling_fps;
	unsigned int m_last_adapt_step;
#ifdef SELFTEST
	BOOL m_selftest_throttling_always_needed;
#endif // SELFTEST
	BOOL m_update_scheduled;
	OpTimer m_timer;
	MediaThrottlerListener* m_listener;

	void UpdateTimestamps(BOOL frame_allowed, double timestamp);
	BOOL IsThrottlingNeeded(double timestamp);
	void ScheduleUpdate(double timestamp);
	BOOL IsUpdateAllowed(double timestamp);
	BOOL AdaptFps(int adapt_interval);
};

#ifdef SELFTEST
#define VIDEO_THROTTLING_SUPPORT
#endif

#endif // VIDEO_THROTTLING_FPS_HIGH > 0

#endif // MEDIATHROTTLER_H
