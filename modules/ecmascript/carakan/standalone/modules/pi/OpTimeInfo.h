/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
*/
#ifndef OP_TIME_INFO_H
#define OP_TIME_INFO_H

class OpTimeInfo
{
public:
	double GetTimeUTC();
	double GetRuntimeMS();
	unsigned int GetRuntimeTickMS();

#if defined _MACINTOSH_ || defined UNIX
	OpTimeInfo();

	int DaylightSavingsTimeAdjustmentMS(double t) { return IsDST(t) ? 36e5 : 0; }
	int GetTimezone() { return m_timezone; }

private:
	double m_last_dst; ///< Last change to DST
	double m_next_dst; ///< Next change to DST
	bool m_is_dst; ///< Does DST apply between last and next ?
	long m_timezone; ///< GetTimezone() cache: seconds west of UTC, DST-adjusted.
	long ComputeTimezone(); ///< Update m_*_isdst, return value for m_timezone.
	bool IsDST(double t); ///< Determines whether DST applies to time t.
#else
	int DaylightSavingsTimeAdjustmentMS(double t);
	int GetTimezone();
#endif
};
#endif // OP_TIME_INFO_H
