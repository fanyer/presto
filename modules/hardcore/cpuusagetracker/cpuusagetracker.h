/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CPU_USAGE_TRACKER_H
#define CPU_USAGE_TRACKER_H

#ifdef CPUUSAGETRACKING

#include "modules/util/opstring.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackers.h"

/**
 * A class that records a sequence of CPU Usage numbers. These should
 * exist on a level that is useful to an end user, so Windows/Tabs and
 * certain global operations.
 *
 * To record data in this, use a CPUUsageTrackActivator.
 *
 * @see class CPUUsageTrackActivator
 */
class CPUUsageTracker
{
 public:
	enum
	{
		/**
		 * The number of samples in the tracker. High number will result
		 * in more data saved. Lower number will result in lower memory usage.
		 *
		 * 60 represents 60 seconds (or 59 full seconds plus the parts
		 * of the current second that has already been used).
		 */
		SAMPLE_LENGTH = 120
	};

 private:
	unsigned m_id;
	double m_sample_start_time_second;
	unsigned m_sample_start_time_index;
	float m_last_X_seconds[SAMPLE_LENGTH];
	OpString m_display_name;
	bool m_activatable:1;

	// For the CPUUsageTrackers collection.
	friend class CPUUsageTrackers;
	friend class CPUUsageTrackers::Iterator;
	CPUUsageTracker* m_next;
	CPUUsageTracker* m_previous;

public:
	/**
	 * Constructor. Remember to also give it a name with SetDisplayName so that
	 * user can understand what it represents.
	 */
	CPUUsageTracker();

	/**
	 * Destructor.
	 */
	~CPUUsageTracker();

	/**
	 * The main function for recording CPU Usage. If the time is smaller
	 * than the previous recorded time, it will assume that the time has
	 * wrapped and restart. If this is later than earlier timer (which it
	 * should be) older data may be delete to make room for this.
	 *
	 * The system uses a cyclic buffer so there are no allocations and the
	 * method is fast.
	 *
	 * @param start_ms The start time in the unit returned from
	 * OpSystemInfo::GetRuntimeMS().
	 *
	 * @param duration_ms The number of milliseconds that the function
	 * ran.  Must be 0.0 or larger and should also be based on
	 * OpSystemInfo::GetRuntimeMS().
	 */
	void RecordCPUUsage(double start_ms, double duration_ms);

	/**
	 * Returns an array containing data for the last SAMPLE_LENGTH
	 * seconds. Can be used to present CPU Usage graphs for the
	 * tracker.
	 *
	 * @param now_time_ms The time where the samples will end. If
	 * it represents a fraction of a second, that second will not be
	 * included in the output. The time should come from
	 * OpSystemInfo::GetRuntimeMS();
	 *
	 * @param[out] cpu_usage The array that will be given the output
	 * from the function. Values will be between 0.0 and 1.0 (if
	 * RecordCPUUsage has been used in a consistant way) or -1.0 if
	 * the sample represents a time before the first
	 * RecordCPUUsage(). The results will be in chronological order
	 * with the last second last in the array.
	 */
	void GetLastXSeconds(double now_time_ms, double cpu_usage[SAMPLE_LENGTH]);

	/**
	 * Returns an average CPU usage for the last couple of seconds.
	 *
	 * @param now_time_ms The time where the samples will end. If
	 * it represents a fraction of a second, that second will not be
	 * included in the output. The time should come from
	 * OpSystemInfo::GetRuntimeMS();
	 *
	 * @param second_count The number of seconds to include in the
	 * calculation. A high number will give an indication of long term
	 * CPU Usage. A low number will show sudden changes.
	 */
	double GetLastXSecondAverage(double now_ms, int second_count);

	/**
	 * Give this tracker a (new) name. This name needs to be localized since
	 * it will be displayed to the user. If no name is set, a default name
	 * or the tracker id will be displayed. Neither being very useful to
	 * an end user. Example for this could be "Tab: about:blank" or
	 * "Addon: NoScript".
	 *
	 * The Display Name can change over time. For instance because a tab
	 * changes contents.
	 *
	 * @param new_display_name The name to display or NULL to clear
	 * the display name.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS SetDisplayName(const uni_char* new_display_name)
	{
		return m_display_name.Set(new_display_name);
	}

	/**
	 * Returns the display name (localized). See SetDisplayName().
	 *
	 * @returns A pointer to the display name or NULL if there is
	 * none.
	 *
	 * @see SetDisplayName()
	 */
	const uni_char* GetDisplayName() { return m_display_name.CStr(); }

	/**
	 * Mark this tracker as possible to activate in the UI. This should be used
	 * if the function that can activate things (tabs typically) will be able to
	 * find a high level visible object with this CPUUsageTracker.
	 */
	void SetActivatable() { m_activatable = true; }

	/**
	 * @returns true if this CPUUsageTracker can be found among the high level objects
	 * that we can activate with the opera:cpu activation functions.
	 */
	bool IsActivatable() { return m_activatable; }

	/**
	 * In the system there exists a default tracker used by
	 * CPUUsageTrackActivators when they don't know which
	 * CPUUsageTracker to use. Check if this is it.
	 *
	 * @returns TRUE if this is the system default fallback tracker.
	 */
	BOOL IsFallbackTracker() { return this == g_fallback_cputracker; }

	/**
	 * Every CPUUsageTracker has a unique id. Should be used to match
	 * results over time.
	 *
	 * @returns The unique id.
	 */
	unsigned GetId() { return m_id; }

private:
	/** Returns the index in the array for the second in question. */
	unsigned MakeRoomForSecond(double second);

	/**
	 *Does the wrapping math to go from offset (0->SAMPLE_LENGTH) to
	 * index (start->SAMPLE_LENGTH_1; 0 -> start-1).
	 */
	unsigned OffsetToIndex(unsigned offset) const;
};

#endif // CPUUSAGETRACKING
#endif // !CPU_USAGE_TRACKER_H
