/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if VIDEO_THROTTLING_FPS_HIGH > 0

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/media/src/mediathrottler.h"
#include "modules/pi/OpSystemInfo.h"

MediaThrottler::MediaThrottler(MediaThrottlerListener* listener /* = NULL */)
	: m_last_allowed_update_timestamp(0.0)
	, m_next_allowed_update_timestamp(0.0)
	, m_last_load_check_timestamp(0.0)
	, m_time_to_update(0.0)
	, m_frame_update_min_interval(1000 / VIDEO_THROTTLING_FPS_HIGH)
	, m_throttling_needed(FALSE)
	, m_current_throttling_fps(VIDEO_THROTTLING_FPS_HIGH)
	, m_last_adapt_step(0)
#ifdef SELFTEST
	, m_selftest_throttling_always_needed(FALSE)
#endif // SELFTEST
	, m_update_scheduled(FALSE)
	, m_listener(listener)
{
	m_timer.SetTimerListener(this);
}

BOOL
MediaThrottler::IsFrameUpdateAllowed()
{
	double curr_time = g_op_time_info->GetRuntimeMS();
	if (IsThrottlingNeeded(curr_time))
	{
		BOOL allowed = IsUpdateAllowed(curr_time);

		if (allowed)
		{
			UpdateTimestamps(TRUE, curr_time);
		}
		else
		{
			ScheduleUpdate(curr_time);
		}

		return allowed;
	}

	return TRUE;
}

BOOL
MediaThrottler::IsThrottlingNeeded(double curr_time)
{
#ifdef SELFTEST
	if (m_selftest_throttling_always_needed)
		return TRUE;
#endif // SELFTEST

	int switch_throttling_interval = g_pccore->GetIntegerPref(PrefsCollectionCore::SwitchAnimationThrottlingInterval);
	if (curr_time > m_last_load_check_timestamp + switch_throttling_interval)
	{
		int threshold_for_throttling = g_pccore->GetIntegerPref(PrefsCollectionCore::LagThresholdForAnimationThrottling);
		BOOL adapted = AdaptFps(switch_throttling_interval);
		m_throttling_needed = adapted || (g_message_dispatcher->GetAverageLag() >= threshold_for_throttling);
		m_last_load_check_timestamp = curr_time;
	}

	return m_throttling_needed;
}

void
MediaThrottler::OnTimeOut(OpTimer*)
{
	if (m_listener)
		m_listener->OnFrameUpdateAllowed();

	m_update_scheduled = FALSE;
}

void
MediaThrottler::UpdateTimestamps(BOOL frame_allowed, double curr_time)
{
	if (frame_allowed)
	{
		m_last_allowed_update_timestamp = curr_time;
		m_next_allowed_update_timestamp = m_last_allowed_update_timestamp + m_frame_update_min_interval;
		m_time_to_update = m_next_allowed_update_timestamp - m_last_allowed_update_timestamp;
		OP_ASSERT(m_time_to_update == m_frame_update_min_interval);
	}
	else
	{
		OP_ASSERT(m_next_allowed_update_timestamp >= curr_time);
		m_time_to_update = m_next_allowed_update_timestamp - curr_time;
	}
}

void
MediaThrottler::ScheduleUpdate(double curr_time)
{
	UpdateTimestamps(FALSE, curr_time);
	if (!m_update_scheduled)
	{
		OP_ASSERT(m_time_to_update >= 0);
		m_timer.Start(static_cast<UINT32>(m_time_to_update));
		m_update_scheduled = TRUE;
	}
}

void
MediaThrottler::Break()
{
	m_timer.Stop();
	m_update_scheduled = FALSE;
	m_last_allowed_update_timestamp = 0.0;
	m_next_allowed_update_timestamp = 0.0;
	m_time_to_update = 0.0;
}

BOOL MediaThrottler::IsUpdateAllowed(double curr_time)
{
	return (m_next_allowed_update_timestamp == 0 || (curr_time >= m_next_allowed_update_timestamp));
}

BOOL MediaThrottler::AdaptFps(int adapt_interval)
{
	BOOL balanced = FALSE;
	if (m_throttling_needed)
	{
		//decrease FPS if possible
		if (m_current_throttling_fps > VIDEO_THROTTLING_FPS_LOW)
		{
			/* count a decrease step. The step is counted dynamically
			 * depending on the load check interval to reach proper FPS in reasonable time
			 * because AdaptFps is only performed at times when the load is checked
			 */
			m_last_adapt_step = (adapt_interval + 1000 - 1)/ 1000;
			m_current_throttling_fps -= m_last_adapt_step;
			m_current_throttling_fps = MAX(m_current_throttling_fps, VIDEO_THROTTLING_FPS_LOW);
		}
	}
	else
	{
		if (m_last_adapt_step != 0)
		{
			//if last time throttling was needed and FPS was decreased, try to increase it to find an equilibrium
			m_current_throttling_fps += m_last_adapt_step;
			m_last_adapt_step = 0;
			balanced = TRUE;
		}
		else
		{
			//throttling may be turned off
			m_current_throttling_fps = VIDEO_THROTTLING_FPS_HIGH;
		}
	}

	m_frame_update_min_interval = 1000 / m_current_throttling_fps;

	return balanced;
}

#endif // #if VIDEO_THROTTLING_FPS_HIGH > 0
