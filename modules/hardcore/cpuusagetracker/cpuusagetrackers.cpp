/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CPUUSAGETRACKING
#include "modules/hardcore/cpuusagetracker/cpuusagetrackers.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"

/* static */void
CPUUsageTrackers::AddTracker(CPUUsageTracker* tracker)
{
	CPUUsageTrackers* list = g_cputrackers;
	if (list->m_last)
	{
		list->m_last->m_next = tracker;
		tracker->m_previous = list->m_last;
		list->m_last = tracker;
	}
	else
	{
		list->m_first = list->m_last = tracker;
	}
}

/* static */ void
CPUUsageTrackers::RemoveTracker(CPUUsageTracker* tracker)
{
	// As long as this live in the dochand module, which isn't one
	// of the early base modules, there is a risk that a
	// CPUUsageTracker lives longer than g_cputrackers.
	// That case is easily covered with a NULL check and is
	// harmless in all other respects. We'll let a local
	// variable pretend to be m_first and m_last in that case.
	CPUUsageTracker* dummy;
	CPUUsageTracker*& next_ptr = tracker->m_previous ?
		tracker->m_previous->m_next : (g_cputrackers ? g_cputrackers->m_first : dummy);
	next_ptr = tracker->m_next;

	CPUUsageTracker*& prev_ptr = tracker->m_next ?
		tracker->m_next->m_previous : (g_cputrackers ? g_cputrackers->m_last : dummy);
	prev_ptr = tracker->m_previous;

	tracker->m_next = NULL;
	tracker->m_previous = NULL;
}

void
CPUUsageTrackers::StartIterating(CPUUsageTrackers::Iterator& iterator)
{
	iterator.m_next = m_first;
}

CPUUsageTracker*
CPUUsageTrackers::Iterator::GetNext()
{
	CPUUsageTracker* ret_val = m_next;
	if (m_next)
		m_next = m_next->m_next;
	return ret_val;
}

#endif // CPUUSAGETRACKING
