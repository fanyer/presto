/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CPUUSAGETRACKING
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"

CPUUsageTrackActivator::CPUUsageTrackActivator(CPUUsageTracker* tracker)
{
	m_tracker = tracker;
	m_start_time = g_op_time_info->GetRuntimeMS();
	if (g_active_cputracker)
	{
		g_active_cputracker->Pause(m_start_time);
		m_interrupts = g_active_cputracker;
	}
	else
		m_interrupts = NULL;

	g_active_cputracker = this;
}

CPUUsageTrackActivator::~CPUUsageTrackActivator()
{
	// Stack based objects so if g_active_tracker is properly
	// maintained it should now point to us.
	OP_ASSERT(g_active_cputracker == this);
	double now = g_op_time_info->GetRuntimeMS();
	if (m_interrupts)
		m_interrupts->Resume(now);
	g_active_cputracker = m_interrupts;
	CPUUsageTracker* tracker = m_tracker ? m_tracker : g_fallback_cputracker;
	tracker->RecordCPUUsage(m_start_time, now - m_start_time);
}

void
CPUUsageTrackActivator::Pause(double now)
{
	OP_ASSERT(g_active_cputracker == this);
	CPUUsageTracker* tracker = m_tracker ? m_tracker : g_fallback_cputracker;
	tracker->RecordCPUUsage(m_start_time, now - m_start_time);
}

void
CPUUsageTrackActivator::Resume(double now)
{
	OP_ASSERT(g_active_cputracker != this);
	m_start_time = now;
}

/* static */ void
CPUUsageTrackActivator::TrackerDied(CPUUsageTracker* dying_tracker)
{
	CPUUsageTrackActivator* active_tracker = g_active_cputracker;
	while (active_tracker)
	{
		if (active_tracker->m_tracker == dying_tracker)
			active_tracker->m_tracker = NULL; // Redirect time to g_fallback_cputracker.
		active_tracker = active_tracker->m_interrupts;
	}
}

#endif // CPUUSAGETRACKING
