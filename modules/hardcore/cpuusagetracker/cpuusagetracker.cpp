/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CPUUSAGETRACKING
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackers.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"

CPUUsageTracker::CPUUsageTracker()
	: m_activatable(false), m_next(NULL), m_previous(NULL)
{
	m_id = g_next_cputracker_id++;
	m_last_X_seconds[0] = -1.0; // Signals "not started".
	m_sample_start_time_index = 0;
	m_sample_start_time_second = 0.0;
	CPUUsageTrackers::AddTracker(this);
}

CPUUsageTracker::~CPUUsageTracker()
{
	CPUUsageTrackers::RemoveTracker(this);
	CPUUsageTrackActivator::TrackerDied(this);
}

/** Converts 3450 ms to 3 s, 4000 ms to 3 s (+ 1000 ms) */
static double GetEndBaseSecond(double time_ms)
{
	double time_s = time_ms / 1000.0;
	double result_time_s = op_ceil(time_s - 1.0);
	return result_time_s;
}

static double GetStartBaseSecond(double time_ms)
{
	double time_s = time_ms / 1000.0;
	double result_time_s = op_floor(time_s);
	return result_time_s;
}

static double GetMilliSecondPart(double time_ms, double base_second)
{
	return time_ms - base_second * 1000;
}

static unsigned DecreaseSampleIndex(unsigned index)
{
	if (index == 0)
		return CPUUsageTracker::SAMPLE_LENGTH - 1;
	else
		return index - 1;
}

static unsigned IncreaseSampleIndex(unsigned index)
{
	if (index == CPUUsageTracker::SAMPLE_LENGTH - 1)
		return 0;
	else
		return index + 1;
}

unsigned CPUUsageTracker::OffsetToIndex(unsigned offset) const
{
	unsigned index = offset + m_sample_start_time_index;
	if (index >= SAMPLE_LENGTH)
		index -= SAMPLE_LENGTH;
	return index;
}

void CPUUsageTracker::RecordCPUUsage(double start_ms, double duration_ms)
{
	if (duration_ms == 0.0)
		return;
	double start_base_s = GetStartBaseSecond(start_ms);
	MakeRoomForSecond(start_base_s); // Will forward the buffer to cover the start second.
	double end_ms = start_ms + duration_ms;
	double end_base_s = GetEndBaseSecond(end_ms);
	unsigned last_index = MakeRoomForSecond(end_base_s); // Will forward the buffer to cover the end second.
	if (end_base_s == start_base_s)
		m_last_X_seconds[last_index] += static_cast<float>(duration_ms / 1000);
	else
	{
		m_last_X_seconds[last_index] += static_cast<float>(GetMilliSecondPart(end_ms, end_base_s) / 1000);
		end_ms = end_base_s * 1000.0;
		end_base_s -= 1.0;
		unsigned index = DecreaseSampleIndex(last_index);
		while (start_base_s != end_base_s && index != last_index)
		{
			m_last_X_seconds[index] += 1.0;
			end_ms -= 1000.0;
			end_base_s -= 1.0;
			index = DecreaseSampleIndex(index);
		}
		if (index != last_index)
			m_last_X_seconds[index] += static_cast<float>((end_ms - start_ms) / 1000);
	}
}

unsigned CPUUsageTracker::MakeRoomForSecond(double second)
{
	if (m_last_X_seconds[0] == -1.0)
	{
		// We just started. Put second at slot 0.
		m_sample_start_time_index = 0;
		m_sample_start_time_second = second;
		for (int i = 0; i < SAMPLE_LENGTH; i++)
			m_last_X_seconds[i] = 0.0;
	}
	else if (m_sample_start_time_second > second || m_sample_start_time_second + SAMPLE_LENGTH <= second)
	{
		// If the second is earlier than our window, then assume the time has wrapped (even if
		// we are using a type that do not wrap right now).
		double new_start_time_second = second - SAMPLE_LENGTH + 1;
		if (second < m_sample_start_time_second ||
		    new_start_time_second - m_sample_start_time_second > SAMPLE_LENGTH) // CHECKME FOR OFFBYONE
		{
			// Clear it all.
			m_sample_start_time_second = new_start_time_second;
			m_sample_start_time_index = 0;
			for (int i = 0; i < SAMPLE_LENGTH; i++)
				m_last_X_seconds[i] = 0.0;
		}
		else
		{
			// Slide the sample window.
			int clear_length = static_cast<int>(new_start_time_second - m_sample_start_time_second);
			while (clear_length-- > 0)
			{
				m_last_X_seconds[m_sample_start_time_index] = 0.0;
				m_sample_start_time_index = IncreaseSampleIndex(m_sample_start_time_index);
			}
			m_sample_start_time_second = new_start_time_second;
		}
	}

	unsigned index = OffsetToIndex(static_cast<unsigned>(second - m_sample_start_time_second));
	return index;
}

double CPUUsageTracker::GetLastXSecondAverage(double now_ms, int second_count)
{
	if (m_last_X_seconds[0] == -1.0 || second_count == 0)
		return 0.0;

	// Extrapolate that everything longer than SAMPLE_LENGTH is the same as SAMPLE_LENGTH.
	if (second_count > SAMPLE_LENGTH)
		second_count = SAMPLE_LENGTH;

	// Don't include the last partial second.
	double now_base_s = GetStartBaseSecond(now_ms) - 1;
	int last_second_offset =  static_cast<int>(now_base_s - m_sample_start_time_second);
	if (last_second_offset < 0)
	{
		// Someone asks for a time that is earlier than the recorded time. There are multiple
		// interpretations of this. The time could have wrapped (if time can wrap) or the
		// caller can be asking for historical data. In either case return 0.0.
		return 0.0;
	}
	int first_second_offset = last_second_offset - second_count + 1;
	if (first_second_offset < 0)
	{
		// Assume that all the values earlier than the window was 0.
		first_second_offset = 0;
	}

	if (last_second_offset >= SAMPLE_LENGTH)
	{
		// Missing values for the last part of the period.
		if (first_second_offset >= SAMPLE_LENGTH)
		{
			// Missing values for the whole period.
			return 0.0;
		}
		last_second_offset = SAMPLE_LENGTH - 1;
	}

	unsigned int index = OffsetToIndex(last_second_offset);
	unsigned stop_index = OffsetToIndex(first_second_offset);
	stop_index = DecreaseSampleIndex(stop_index);
	double result = 0.0;
	do
	{
		result += m_last_X_seconds[index];
		index = DecreaseSampleIndex(index);
	}
	while (index != stop_index);

	// Do the division last to be able to work with integers (if that is what we have in the sample) for as
	// long as possible.
	return result  / second_count;
}

void CPUUsageTracker::GetLastXSeconds(double now_ms, double cpu_usage[SAMPLE_LENGTH])
{
	for (int i = 0; i < SAMPLE_LENGTH; i++)
		cpu_usage[i] = -1.0;

	if (m_last_X_seconds[0] == -1.0)
		return;

	double now_base_s = GetStartBaseSecond(now_ms) - 1;
	int last_second_offset =  static_cast<int>(now_base_s - m_sample_start_time_second);
	if (last_second_offset < 0)
	{
		// Not in sample range
		return;
	}

	unsigned out_index = SAMPLE_LENGTH - 1; // We can't fill all slots since we skip the last second. The API can be redone to avoid this.
	while (last_second_offset >= SAMPLE_LENGTH && (out_index + 1) != 0)
	{
		// Nothing has happened at the end of the requested period. Fill with zeros.
		cpu_usage[out_index--] = 0.0;
		last_second_offset--;
	}

	if (out_index + 1 == 0)
		return;

	unsigned int index = OffsetToIndex(last_second_offset);
	unsigned stop_index = DecreaseSampleIndex(m_sample_start_time_index);
	do
	{
		cpu_usage[out_index--] = m_last_X_seconds[index];
		index = DecreaseSampleIndex(index);
	}
	while (index != stop_index && (out_index + 1) != 0);
	return; // So that I have somewhere to put a breakpoint.
}
#endif // CPUUSAGETRACKING
