/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CPU_USAGE_TRACKERS_H
#define CPU_USAGE_TRACKERS_H

#ifdef CPUUSAGETRACKING

class CPUUsageTracker;

/**
 * List of all CPU Trackers in the system. Used for generating visual
 * displays of the CPU usage per tab (CPUUsageTracker is approximately
 * one per tab/Window, with a few global ones to cover the rest.
 */
class CPUUsageTrackers
{
	CPUUsageTracker* m_first;
	CPUUsageTracker* m_last;

public:
	/** Constructor */
	CPUUsageTrackers() : m_first(NULL), m_last(NULL) {}

	class Iterator;

private:
	friend class CPUUsageTracker;

	/**
	 * Adds a tracker to the global list. Run from the CPUUsageTracker
	 * constructor so nobody else should use it.
	 * @param tracker The tracker to add.
	 */
	static void AddTracker(CPUUsageTracker* tracker);

	/**
	 * Removes a tracker to the global list. Run from the
	 * CPUUsageTracker destructor so nobody else should use it.
	 * @param tracker The tracker to remove.
	 */
	static void RemoveTracker(CPUUsageTracker* tracker);

public:
	/**
	 * Initialises a cpu tracker iterator.
	 */
	void StartIterating(CPUUsageTrackers::Iterator& iterator);
};

/**
 * Iterator over all CPU trackers. The tracker list must not be
 * modified while this is used. Typically created at the stack as a
 * local variable. See CPUUsageTrackerIterator::GetNext() for an
 * example how to use it.
 */
class CPUUsageTrackers::Iterator
{
	friend class CPUUsageTrackers;
	CPUUsageTracker* m_next;
public:
	/** Constructor */
	Iterator() : m_next(NULL) {}

	/**
	 * Gets the next CPUUsageTracker or NULL.
	 *
	 * Example usage:
	 *
	 * while (CPUUsageTracker* tracker = iterator.GetNext()) {}
	 *
	 * @returns CPUUsageTracker or NULL if there are no more trackers.
	 */
	CPUUsageTracker* GetNext();
};

#endif // CPUUSAGETRACKING
#endif // !CPU_USAGE_TRACKERS_H
