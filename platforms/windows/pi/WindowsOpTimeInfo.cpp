/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpTimeInfo.h"
#include "platforms/windows/WindowsTimer.h"
#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"
#include <sys/timeb.h>

OP_STATUS OpTimeInfo::Create(OpTimeInfo** new_timeinfo)
{
	OP_ASSERT(new_timeinfo != NULL);
	*new_timeinfo = OP_NEW(WindowsOpTimeInfo,());
	if(*new_timeinfo == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

WindowsOpTimeInfo::WindowsOpTimeInfo()
		: m_time_zone_use_cache(FALSE)
		, m_have_calibrated(FALSE)
		, m_calibration_value_ftime(0.0)
		, m_last_time_utc(0.0)
		, m_is_dst(FALSE)
		, m_last_dst(0.0)
		, m_next_dst(0.0)
{
	m_timezone = GetTimezone();
	m_calibration_value_qpc.QuadPart = 0;
}

void WindowsOpTimeInfo::GetWallClock( unsigned long& seconds, unsigned long& milliseconds )
{
	DWORD currenttime = timeGetTime();		// greater accuracy than GetTickCount

	seconds = currenttime / 1000;
	milliseconds = currenttime % 1000;
}

double WindowsOpTimeInfo::GetWallClockResolution()
{
	DWORD timer_resolution = 0;
	if (WindowsTimerResolutionManager::GetInstance())
		timer_resolution = WindowsTimerResolutionManager::GetInstance()->GetTimerResolution();
	return timer_resolution ? timer_resolution / 1000.0 : 0.01;
}

double WindowsOpTimeInfo::GetTimeUTC()
{
	// ftime is fast but is based on a 60Hz source. QueryPerformanceCounter (QPC) has
	// a high resolution but is slightly unreliable. See for instance:
	// http://support.microsoft.com/kb/274323
	// http://support.microsoft.com/kb/895980
	// Here we combine them by calibrating them together. If QPC strays too
	// far off the ftime time then we do a new calibration.
	struct _timeb now_ftime;
	_ftime(&now_ftime);
	double now_ftime_ms = now_ftime.time * 1000.0 + now_ftime.millitm;
	LARGE_INTEGER now_qpc;
	if (!QueryPerformanceCounter(&now_qpc))
		return now_ftime_ms;

	if (!m_have_calibrated)
	{
		m_have_calibrated = TRUE;
		// Calibrate since we've never calibrated
		m_calibration_value_ftime = m_last_time_utc = now_ftime_ms;
		m_calibration_value_qpc = now_qpc;
		return now_ftime_ms;
	}

	// Time since last calibration
	double passed_ftime_ms = now_ftime_ms - m_calibration_value_ftime;
	static LARGE_INTEGER qpc_freq;
	static BOOL computed_freq = FALSE;
	if (!computed_freq)
	{
		if (!QueryPerformanceFrequency(&qpc_freq))
			return now_ftime_ms;

		computed_freq = TRUE;
	}

	double passed_qpc_ms = ((now_qpc.QuadPart - m_calibration_value_qpc.QuadPart) / static_cast<double>(qpc_freq.QuadPart)) * 1000.0;

	double result;
	// 16 (~ 1000/60) is a possible legal "error" because of bad ftime() resolution.
	if (fabs(passed_qpc_ms - passed_ftime_ms) > 16)
	{
		// Calibrate since QPC and ftime didn't agree at all
		m_calibration_value_ftime = now_ftime_ms;
		m_calibration_value_qpc = now_qpc;
		result = now_ftime_ms;
	}
	else
		result = m_calibration_value_ftime + passed_qpc_ms;

	// The sequence of reported UTC times must be non-monotonic,
	// the exception being a change to system time like DST or other
	// time system adjustments.
	//
	// This check prevents jumps back in time of upto one second.
	if (result < m_last_time_utc && (m_last_time_utc - result) < 1000.0)
		return m_last_time_utc;

	m_last_time_utc = result;
	return result;
}

double WindowsOpTimeInfo::GetRuntimeMS()
{
	return WindowsOpComponentPlatform::GetRuntimeMSImpl();
}


unsigned int WindowsOpTimeInfo::GetRuntimeTickMS()
{
    // Some contexts wants lower-precision estimates of time intervals; provide basic tick counter access for these.
    return GetTickCount();
}

static struct tm *localtime_r(const time_t *t, struct tm *result)
{
	if (localtime_s(result, t) == 0)
		return result;
	else
		return NULL;
}

/*
* Code shamelessly stolen from WinGogi which shamelessly stole it from the posix module:
*
* Find when DST changes between now and then, to within tol seconds.
* 
*
* Uses binary chop on the interval between now and then, so number of calls to
* localtime_r and mktime grows as the log of the interval divided by tol.
* However, the algorithm also tweaks towards selecting the last and first
* seconds of an hour, or minute, if such a second falls in the interval to be
* bisected; this may slow the convergence, but should tend to give more
* accurate answers.
*
* @param now Describes the time (roughly) now
* @param then Describes a later or earlier time with distinct .tm_isdst
* @param tol Accuracy level sought for DST change-over betweeen now and then
* @return A time, in milliseconds since the epoch, within tol seconds of a
* moment between now and then when DST changes; the indicated moment is always
* strictly on then's side (future or past) of now, never equal to now; the
* returned time has the same DST as now unless it is the exact moment of the
* change (in which case the time one second closer to now has the same DST as
* now).
*/
static double NarrowInterval(struct tm now, struct tm &then, time_t tol)
{
	OP_ASSERT(tol > 0 && now.tm_isdst != then.tm_isdst);
	/* NB (see bug 361242): stdlib's op_mktime needs to know timezone, which it
	* can only know once we've been here; so we must use a real mktime here.
	*/
	time_t n = mktime(&now), t = mktime(&then);
	bool initial = true;
	while (initial || (n > t + tol || n + tol < t))
	{
		OP_ASSERT(t != n);
		time_t m = t / 2 + n / 2 + (n % 2 && t % 2); // != (n+t)/2 due to overflow !
		if (m == n || m == t) // We've found the *exact* second before a change !
		{
			OP_ASSERT(t > n ? n + 1 == t : n == t + 1);
			n = t;
			break;
		}

		// Round m up to end of minute or hour, or down to start, if practical:
		if (t > m)
		{
			if (m/60 > n/60)
			{
				if (m/3600 > n/3600)
					m -= m % 3600;
				else
					m -= m % 60;
			}
			else if (m/60 < t/60)
			{
				if (m/3600 < t/3600)
					m += 3599 - m % 3600;
				else
					m += 59 - m % 60;
			}
		}
		else if (n > m)
		{
			if (m/60 > t/60)
			{
				if (m/3600 > t/3600)
					m -= m % 3600;
				else
					m -= m % 60;
			}
			else if (m/60 < n/60)
			{
				if (m/3600 < n/3600)
					m += 3599 - m % 3600;
				else
					m += 59 - m % 60;
			}
		}

		struct tm mid;
		if (localtime_r(&m, &mid) != &mid)
		{
			// meh - do this by hand ... fortunately, mktime allows non-normal values
			time_t delta = n - m;
			op_memset(&mid, 0, sizeof(mid));
			mid.tm_isdst = -1;
			mid.tm_sec = now.tm_sec + delta % 60;
			delta /= 60;
			mid.tm_min = now.tm_min + delta % 60;
			delta /= 60;
			mid.tm_hour = now.tm_hour + delta % 24;
			mid.tm_mday = now.tm_mday + delta / 24;
			mid.tm_mon = now.tm_mon;
			mid.tm_year = now.tm_year;
			// Input tm_wday and tm_yday are ignored.
			m = mktime(&mid); // normalizes all fields
			OP_ASSERT((n < m && m < t) || (n > m && m > t));
		}

		if (mid.tm_isdst == now.tm_isdst)
		{
			now = mid;
			initial = false;
			n = mktime(&now);
		}
		else
		{
			then = mid;
			t = mktime(&then);
		}
	}
	return n * 1e3;
}

/**
* Code shamelessly stolen from WinGogi which shamelessly stole it from the posix module:
* 
* Find (to adequate accuracy) when DST last changed or shall next change.
*
* The further from now the relevant change happened or shall happen, the
* broader the error-bar allowed on its value.
*
* @param when Describes the time (approximately) now.
* @param now Number of seconds since epoch, should equal mktime(&when).
* @param sign Indicates future if > 0 or past if < 0; should be +1 or -1.
* @return A time, in milliseconds since the epoch, reasonably close to when DST
* changes from when.tm_isdst, on the selected side of now; any moment strictly
* closer to now than the returned time has the same DST as now.
*/
static double BoundDST(const struct tm &when, time_t now, int sign)
{
	const time_t day = 24 * 60 * 60; // time_t is in seconds
	time_t then = now + sign * day;
	struct tm what;
	if (localtime_r(&then, &what) == &what && 
		what.tm_isdst != when.tm_isdst)
	{
		return NarrowInterval(when, what, sign < 0 ? 60 * 60 : 60);
	}
	else if (then = now + 14 * sign * day, localtime_r(&then, &what) == &what && 
		what.tm_isdst != when.tm_isdst)
	{
		return NarrowInterval(when, what, day);
	}
	// Assumption: DST changes happen > 4 months apart
	else if (then = now + 122 * sign * day, // year/3 away
		localtime_r(&then, &what) == &what &&
		what.tm_isdst != when.tm_isdst)
	{
		return NarrowInterval(when, what, 14 * day);
	}
	else if (then = now + 244 * sign * day, // year * 2/3 away
		localtime_r(&then, &what) == &what &&
		what.tm_isdst != when.tm_isdst)
	{
		return NarrowInterval(when, what, 14 * day);
	}
	// else, well, at least that's a long interval on this side of now :-)
	return then * 1e3;
}

long WindowsOpTimeInfo::GetTimezone()
{
	if(m_time_zone_use_cache)
	{
		return m_timezone;
	}
	time_t timenow = time(NULL);
	if(timenow == -1)
	{
		return 0;
	}
	tm* time_local = localtime(&timenow);
	if(!time_local)
	{
		return 0;
	}
	m_timezone = timezone + (time_local && time_local->tm_isdst ? _dstbias : 0);
	m_time_zone_use_cache = TRUE;

	m_is_dst = time_local->tm_isdst;
	m_next_dst = BoundDST(*time_local, timenow, +1);
	m_last_dst = BoundDST(*time_local, timenow, -1);

	return m_timezone;
}

double WindowsOpTimeInfo::DaylightSavingsTimeAdjustmentMS(double t)
{
	// Use cached data if sufficient:
	if (m_last_dst < t && t < m_next_dst)
	{
		return m_is_dst ? 36e5 : 0;
	}	
	// else compute:
	time_t sec_time = (time_t)(t / 1000);

	struct tm when;
	if (localtime_r(&sec_time, &when) == &when)
	{
		return when.tm_isdst > 0 ? 36e5 : 0;
	}
	return 0; 
}

void WindowsOpTimeInfo::OnSettingsChanged()
{
	tzset();
	m_time_zone_use_cache = FALSE;
}
