/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPTIMEINFO_H
#define WINDOWSOPTIMEINFO_H

#include "modules/pi/OpTimeInfo.h"

class WindowsOpTimeInfo: public OpTimeInfo
{
public:
	WindowsOpTimeInfo();
	virtual ~WindowsOpTimeInfo() { 	OnSettingsChanged(); }

	virtual void GetWallClock(unsigned long& seconds, unsigned long& milliseconds);

	double GetWallClockMS()
	{
		unsigned long seconds, millisec;
		GetWallClock(seconds, millisec);
		// most FPUs, especially x86, can convert from int much easier than from unsigned long,
		// and millisec always fits in an int
		return ((double)seconds * 1000 + (int)millisec);
	}

	virtual double GetWallClockResolution();

	virtual double GetTimeUTC();

	virtual double GetRuntimeMS();

	virtual unsigned int GetRuntimeTickMS();

	virtual long GetTimezone();

	virtual double DaylightSavingsTimeAdjustmentMS(double t);

	void OnSettingsChanged();
private:

	BOOL	m_time_zone_use_cache;				// if set, use the cached value for timezone

	// GetTimezone() optimization
	long	m_timezone;

	// GetTimeUTC
	BOOL m_have_calibrated;
	double m_calibration_value_ftime;
	LARGE_INTEGER m_calibration_value_qpc;
	double m_last_time_utc;

	// DaylightSavingsTimeAdjustmentMS
	double m_last_dst; ///< Last change to DST
	double m_next_dst; ///< Next change to DST
	BOOL m_is_dst; ///< Does DST apply between last and next ?
};

#endif // WINDOWSOPTIMEINFO_H
