/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/controls/mediafader.h"
#include "modules/pi/OpSystemInfo.h"

const int FADE_TIME = 200, FADE_TIME_OUT = 10;

MediaControlsFader::MediaControlsFader(FaderListener* listener, UINT8 min_opacity, UINT8 max_opacity)
	: m_listener(listener),
	  m_min_opacity(min_opacity),
	  m_max_opacity(max_opacity),
	  m_opacity(0),
	  m_state(None),
	  m_timer_running(FALSE)
{
 	m_timer.SetTimerListener(this);
}

/* virtual */ void
MediaControlsFader::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(m_state != None);

	m_timer_running = FALSE;

	if (m_state == Hiding)
	{
		SetNewOpacity(0);
		m_state = None;
		return;
	}

	double deltams = g_op_time_info->GetWallClockMS() - m_lastms;

	int step = (int)((deltams / (double)FADE_TIME) * (m_max_opacity - m_min_opacity));

	if (m_state == FadingOut)
		step = -step;

	SetNewOpacity(m_opacity + step);

	StartTimer(FADE_TIME_OUT);
}

void
MediaControlsFader::FadeIn()
{
	m_state = FadingIn;
	StartTimer(FADE_TIME_OUT);
}

void
MediaControlsFader::FadeOut()
{
	m_state = FadingOut;
	StartTimer(FADE_TIME_OUT);
}

void
MediaControlsFader::Show()
{
	StopTimer();

	SetNewOpacity(m_max_opacity);
}

void
MediaControlsFader::DelayedHide()
{
	m_state = Hiding;
	StartTimer(FADE_TIME);
}

void
MediaControlsFader::StartTimer(unsigned timeout)
{
	if (Done() || m_timer_running)
		return;

	m_lastms = g_op_time_info->GetWallClockMS();
	m_timer_running = TRUE;
	m_timer.Start(timeout);
}

void
MediaControlsFader::StopTimer()
{
	m_timer_running = FALSE;
	m_timer.Stop();
}

BOOL
MediaControlsFader::Done()
{
	return m_state == None || (m_state == FadingIn && m_opacity == m_max_opacity) || (m_state == FadingOut && m_opacity == m_min_opacity);
}

void
MediaControlsFader::SetNewOpacity(int new_opacity)
{
	new_opacity = MAX(m_min_opacity, MIN(new_opacity, m_max_opacity));

	if (!m_opacity && new_opacity)
		m_listener->OnVisibilityChange(this, TRUE);

	if (m_opacity && !new_opacity)
		m_listener->OnVisibilityChange(this, FALSE);

	m_opacity = new_opacity;

	m_listener->OnOpacityChange(this, m_opacity);
}

#endif // MEDIA_PLAYER_SUPPORT
