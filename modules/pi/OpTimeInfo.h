/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPTIMEINFO_H
#define OPTIMEINFO_H

class OpTimeInfo
{
public:
	virtual ~OpTimeInfo() {}
	/** Create an OpTimeInfo object. */
	static OP_STATUS Create(OpTimeInfo** new_timeinfo);
	/** Get the current local time as seconds and milliseconds since some epoch. */
	virtual void GetWallClock(unsigned long& seconds, unsigned long& milliseconds) = 0;

	/** Get the current local time as milliseconds since some epoch. */
	double GetWallClockMS()
	{
		unsigned long seconds, millisec;
		GetWallClock(seconds, millisec);
		// most FPUs, especially x86, can convert from int much easier than from unsigned long,
		// and millisec always fits in an int
		return ((double)seconds * 1000 + (int)millisec);
	}

	/** Get the resolution of the real-time clock, as seconds.
	 *
	 * @return Real-time clock resolution, in number of seconds
	 * (e.g. if the resolution is 50 Hz, return 0.02)
	 */
	virtual double GetWallClockResolution() = 0;

	/** Get the current time as milliseconds since 1970-01-01 00:00 UTC (GMT). */
	virtual double GetTimeUTC() = 0;

	/** Get the time elapsed since "something".
	 *
	 * It doesn't matter what "something" is, as long as it remains
	 * the same within one Opera session. In other words, using the
	 * local time is not a very good solution, since it may change due
	 * to switching between standard time and daylight saving time, by
	 * being updated by a time server, or even by being changed
	 * manually by the user.
	 *
	 * @return The number of milliseconds since "something"
	 */
	virtual double GetRuntimeMS() = 0;

	/** Get a lower-precision estimate of time elapsed since some reference point,
	 * but with no fixed guarantees that the elapsed time reported may not wrap around during an
	 * Opera session. Appropriate when you want to quickly sample interval durations by comparing the
	 * (absolute) difference between successive calls to GetRuntimeTickMS(). The required precision of such
	 * intervals is lower than the timer's precision, hence a faster timing source can be used.
	 *
	 * @return The number of milliseconds since some reference point. No guarantees
	 * that this will be monotonically increasing throughout an Opera session (it may wrap around),
	 * nor high precision.
	 */
	virtual unsigned int GetRuntimeTickMS() = 0;

	/** Get the number of seconds west of UTC (GMT).
	 *
	 * Example for Norway: return -3600 when in standard time (between
	 * 01:00:00 UTC on the last Sunday of October and 01:00:00 UTC on
	 * the last Sunday of March), and -7200 when in daylight saving
	 * time.
	 */
	virtual long GetTimezone() = 0;

	/** Get the difference between daylight saving time (if applicable
	 * for the local timezone and time of year) and standard time for
	 * a given timestamp.
	 *
	 * @param t Check if daylight saving time applies (or applied) at
	 * this timestamp, and if so, by how much. It is specified as
	 * number of milliseconds since 1970-01-01 00:00:00 UTC.
	 *
	 * @return The daylight saving time adjustment, as milliseconds,
	 * for the timestamp t. The adjustment returned is always
	 * nonnegative.
	 *
	 * Example for Norway: in the summer (after 01:00:00 UTC on the
	 * last Sunday of March) the returned value is 3600000 (one hour),
	 * and in the winter (after 01:00:00 UTC on the last Sunday of
	 * October) the returned value is 0.
	 */
	virtual double DaylightSavingsTimeAdjustmentMS(double t) = 0;
};

#endif // OPTIMEINFO_H
