/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Petter Nilsen
 */

#include "core/pch.h"

#include "adjunct/quick/animation/QuickWidgetHoverBlend.h"

#include "modules/widgets/OpWidget.h"

/***********************************************************************************
**
**	QuickWidgetHoverBlend
**
***********************************************************************************/
QuickWidgetHoverBlend::QuickWidgetHoverBlend() :
	m_hover_counter(0),
	m_hover_start_time(0.0),
	m_timer(),
	m_timer_delay(0),
	m_is_hovering(false)
{
	m_timer.SetTimerListener(this);
}

QuickWidgetHoverBlend::~QuickWidgetHoverBlend()
{
	StopTimer();
}

#define QUICK_HOVER_BLEND_TIMEOUT   10
#define QUICK_HOVER_BLEND_TOTAL    100 // full duration should not be more than QUICK_HOVER_BLEND_TOTAL milliseconds

void QuickWidgetHoverBlend::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &m_timer);

	double time_now = g_op_time_info->GetRuntimeMS();

	// hover effect
	if (m_hover_start_time)
	{
		double elapsed = time_now - m_hover_start_time;
		int step = (int)(elapsed * 100 / QUICK_HOVER_BLEND_TOTAL);

		if (step <= 0)
			return;

		m_hover_start_time = time_now;

		// blend out
		if (!m_is_hovering)
		{
			m_hover_counter -= step;

			if (m_hover_counter <= 0)
			{
				m_hover_counter = 0;
				m_hover_start_time = 0;
			}
		}
		// blend in
		else
		{
			m_hover_counter += step;

			if (m_hover_counter >= 100)
			{
				m_hover_counter = 100;
				m_hover_start_time = 0;
			}
		}
		UpdateHover();

		if(m_hover_counter != 0 && m_hover_counter != 100)
			timer->Start(m_timer_delay);
	}
}

OP_STATUS QuickWidgetHoverBlend::AddWidgetToBlend(OpWidget *widget)
{
	RETURN_IF_ERROR(m_blend_widgets.Add(widget));

	return OpStatus::OK;
}

void QuickWidgetHoverBlend::RemoveWidgetToBlend(OpWidget *widget)
{
	m_blend_widgets.RemoveByItem(widget);
}

void QuickWidgetHoverBlend::HoverBlendIn()
{
	if(m_hover_start_time == 0)
	{
		m_hover_start_time = g_op_time_info->GetRuntimeMS();
		StartTimer(QUICK_HOVER_BLEND_TIMEOUT);
		m_is_hovering = true;
	}
}

void QuickWidgetHoverBlend::HoverBlendOut()
{
	// start timer to blend out hover effect
	m_hover_start_time = g_op_time_info->GetRuntimeMS();
	StartTimer(QUICK_HOVER_BLEND_TIMEOUT);
	m_is_hovering = false;
}

void QuickWidgetHoverBlend::UpdateHover()
{
	for(UINT32 x = 0; x < m_blend_widgets.GetCount(); x++)
	{
		OpWidget *widget = m_blend_widgets.Get(x);
		if(widget)
		{
			widget->GetBorderSkin()->SetState((m_is_hovering ? SKINSTATE_HOVER : 0), SKINSTATE_HOVER, m_hover_counter);
			widget->GetForegroundSkin()->SetState((m_is_hovering ? SKINSTATE_HOVER : 0), SKINSTATE_HOVER, m_hover_counter);
			widget->Sync();
		}
	}
}

void QuickWidgetHoverBlend::StartTimer(UINT32 millisec)
{
	StopTimer();
	m_timer_delay = millisec;
	m_timer.Start(millisec);
}

void QuickWidgetHoverBlend::StopTimer()
{
	m_timer.Stop();
	m_timer_delay = 0;
}
