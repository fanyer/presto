/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CPU_USAGE_TRACKER_ACTIVATOR_H
#define CPU_USAGE_TRACKER_ACTIVATOR_H

#ifdef CPUUSAGETRACKING
# define TRACK_CPU_PER_TAB(x) CPUUsageTrackActivator cputracker(x)
#else
# define TRACK_CPU_PER_TAB(x) ((void)0)
#endif // CPUUSAGETRACKING


#ifdef CPUUSAGETRACKING

class CPUUsageTracker;


/**
 * Object that you put on the stack to attribute the time for a function
 * to a certain CPUUsageTracker (typically a tab but there are also a few
 * global objects. These can be used directly but if the
 * macro TRACK_CPU_PER_TAB is used instead you can avoid ifdefs.
 */
class CPUUsageTrackActivator
{
private:
	CPUUsageTracker* m_tracker;
	double m_start_time;
	CPUUsageTrackActivator* m_interrupts;

	CPUUsageTrackActivator(CPUUsageTrackActivator& other);
	CPUUsageTrackActivator operator=(CPUUsageTrackActivator& other);

public:
	/**
	 * Constructor. Create at the stack to make the tracker
	 * record the time in the current function. This should be used
	 * when the main thread enters a Core Window or a global
	 * system. For instance in the message handler, when event arrive to OpView
	 * or when a non-Window connected message arrived.
	 *
	 * It should not be used in low level code.
	 */
	CPUUsageTrackActivator(CPUUsageTracker* tracker);

	/**
	 * Destructor.
	 */
	~CPUUsageTrackActivator();

	/**
	 * Checks if a CPUUsageTrackActivator is already enabled. Can be
	 * used when deciding whether to enable a low quality
	 * CPUUsageTrackActivator just in case in functions that are
	 * sometimes used internally, sometimes from external sources, such
	 * as NPN_Evaluate (plugin function).
	 */
	static BOOL IsActive() { return g_active_cputracker != NULL; }

	/**
	 * Code to support a tracker that is active when it's being destroyed.
	 * This happens when tabs delete the MessageHandler and themselves from
	 * inside a message. Extremely rare but the MessageHandler
	 * system is designed to handle that case so to use CPUUsageTracker in
	 * MessageHandler, CPUUsageTracker needs to support it as well.
	 *
	 * Call this function from the CPUUsageTracker destructor.
	 *
	 * @param[in] dying_tracker The tracker that is being deleted.
	 */
	static void TrackerDied(CPUUsageTracker* dying_tracker);

private:

	/**
	 * To be used when CPUUsageTrackActivators are nested.
	 */
	void Pause(double now);

	/**
	 * To be used when CPUUsageTrackActivators are nested.
	 */
	void Resume(double now);
};

#endif // CPUUSAGETRACKING
#endif // !CPU_USAGE_TRACKER_ACTIVATOR_H
