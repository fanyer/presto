/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/stdlib/util/opdate.h"


/* The following definition of UTIME_T is supposed to provide an unsigned
   version of the 'time_t' type.  This is not easy to establish for the
   64-bit versions of 'time_t' - so we use the 'time_t' itself in this case.
   This is OK since a 64-bit 'time_t' is more than large enough to hold
   the interesting (positive) dates anyway. */

#if STDLIB_SIXTY_FOUR_BIT_TIME_T
#define UTIME_T time_t
#else
#define UTIME_T UINT32
#endif

/* Code in this file assumes that a time_t represents a number of seconds 
   since the UNIX EPOCH of 1970-01-01 00:00:00 UTC. */

#define ONE_DAY_SECS		((UTIME_T)(24*60*60))
#define COMMON_YEAR_SECS	((UTIME_T)(365*24*60*60))
#define LEAP_YEAR_SECS		((UTIME_T)(366*24*60*60))
#define FOUR_YEARS_SECS		((UTIME_T)(1461*24*60*60))

#ifndef HAVE_TIME

time_t op_time(time_t* pt)
{
	/* This requires that GetTimeUTC() is not implemented in terms of op_time,
	   but then it's probably wrong for porting interfaces to be implemented
	   on top of the op_ functions in most cases, so let's not worry about it. */
	time_t t = (g_op_time_info->GetTimeUTC() / 1000.0);
	if (pt)
		*pt = t;
	return t;
}

#endif

#ifndef HAVE_MKTIME

/* mktime does two things: creates a time_t value, and adjusts the values
   of struct tm.  */

time_t op_mktime(struct tm* tm)
{
	tm->tm_sec = tm->tm_sec >= 0 && tm->tm_sec <= 60 ? tm->tm_sec : 0;
	tm->tm_min = tm->tm_min >= 0 && tm->tm_sec <= 59 ? tm->tm_min : 0;
	tm->tm_hour = tm->tm_hour >= 0 && tm->tm_hour <= 59 ? tm->tm_hour : 0;
	tm->tm_mday = tm->tm_mday >= 1 && tm->tm_mday <= 31 ? tm->tm_mday : 1;		// Will be adjusted later
	tm->tm_mon = tm->tm_mon >= 0 && tm->tm_mon <= 11 ? tm->tm_mon : 0;
	tm->tm_year = tm->tm_year >= 0 ? tm->tm_year : 0;
	tm->tm_isdst = tm->tm_isdst == 0 || tm->tm_isdst == 1 ? tm->tm_isdst : -1;	// Will be adjusted later if -1
	tm->tm_wday = 0;													// Will be set later
	tm->tm_yday = 0;													// Will be set later

	int tz_hour = 0;
	int tz_min = 0;

	// Interpret as in local timezone; correct for zone and for daylight
	// savings time at the time
	double tmptime = OpDate::MakeDate(OpDate::MakeDay(tm->tm_year + 1900, tm->tm_mon, tm->tm_mday), 
									  OpDate::MakeTime(tm->tm_hour, tm->tm_min, tm->tm_sec, 0));

	// If tm_isdst == 0 we do not adjust for DST
	int tz = op_double2int32(OpDate::LocalTZA() + (tm->tm_isdst != 0 ? OpDate::DaylightSavingTA(tmptime) : 0));
	tz_hour = (tz / (int)msPerHour);
	tz_min = (tz % (int)msPerHour) / (int)msPerMinute;
	
	/* Is this enough?  What if we pass into the previous day?  What if these fields go negative? */
	struct tm adjusted = *tm;
	adjusted.tm_hour -= tz_hour;
	adjusted.tm_min -= tz_min;

	double t = OpDate::MakeDate(OpDate::MakeDay(adjusted.tm_year + 1900, adjusted.tm_mon, adjusted.tm_mday), 
							    OpDate::MakeTime(adjusted.tm_hour, adjusted.tm_min, adjusted.tm_sec, 0));

	tm->tm_wday = OpDate::WeekDay(t);
	tm->tm_yday = OpDate::DayWithinYear(t);

	return op_double2uint32(t / msPerSecond);
}

#endif

#ifndef HAVE_GMTIME

//
// Given a number of days into a year (yday), and info on leap year
// status (leap_year), the following function will calculate and
// return the month of the given day (range 0-11), and also return
// the numbe of days before the start of the month returned (this
// can be used to further calculate the day of the month etc.)
//
static int find_month(int yday, BOOL leap_year, int &prior_days)
{
	int month;				// The month to be returned
	int work_yday = yday;	// Modify this as we go

	int march;
	int mp;

	OP_ASSERT(yday >= 0);
	OP_ASSERT(yday < (leap_year ? 366 : 365));

	// Check for January
	if ( yday < 31 )
	{
		month = 0;   // We return 0 for January, like gmtime()
		goto done;
	}

	// Find the day when march starts
	march = 59;
	if ( leap_year )
		march++;

	// Check for February
	if ( yday < march )
	{
		// February
		month = 1;
		work_yday -= 31; // Subtract January
		goto done;
	}

	// Realign and make work_yday == 0 become the 1st of March and continue
	month = 2;
	work_yday -= march;

	// The remaining months March-December have days like this:
	// 31,30,31,30,31  and  31,30,31,30,31
	// Notice the repeating pattern; we can fold the upper block
	// Starting at 1st of August down into the lower one:
	// (153 is the number of days between 1st of March and 1st of August)

	if ( work_yday >= 153 )
	{
		work_yday -= 153;
		month += 5;
	}

	// Notice how the remaining months March to July have alternating
	// 31 and 30 days: We can exploit this and first do "months pairs"
	// of 31+30 days:

	mp = work_yday / 61;  // Number of "month pairs": 0, 1 or 2 only
	month += (2*mp);
	work_yday -= (mp*61);

	// What possibly remains is the first half of a "month pair":
	if ( work_yday >= 31 )
	{
		month++;
		work_yday -= 31;
	}

 done:
	prior_days = yday - work_yday;
	return month;
}

#ifndef STDLIB_UNSIGNED_TIME_T
//
// The type 'time_t' is signed, so we bias it so the original time_t value
// of zero (The UNIX epoc - 1970-01-01 00:00:00) will be represented as an
// unsigned and biased value of 0x80000000.
//
#define BIASED_TIME_T_VALUE 0x80000000

//
// A bias value of 0x80000000 means that we will have e.g. the following
// relationships between biased_time and signed time_t:
//
// Date                    biased_time        time_t value
// --------------------------------------------------------------
// 1901-12-13 20:45:52     0x0000000000      -2147483648 (-2^31)
// 1901-12-14 00:00:00     0x0000002d80      -2147472000 (first whole day)
// 1902-01-01 00:00:00     0x000017e880      -2145916800 (first whole year)
// 1904-01-01 00:00:00     0x0003da4f80      -2082844800 (first leap year)
// 1968-09-09 12:15:00     0x007d892dc4      -41341500
// 1969-12-31 23:59:59     0x007fffffff      -1
// 1970-01-01 00:00:00     0x0080000000       0
// 1972-01-01 00:00:00     0x0083c26700       63072000
// 2000-01-01 00:00:00     0x00b86d4380       946684800
// 2008-08-08 08:08:08     0x00c89bfee8       1218182888
// 2038-01-19 03:14:07     0x00ffffffff       2147483647 (2^31-1)
// 2100-01-01 00:00:00     0x0174865700       4102444800
// 2106-02-07 06:28:15     0x017fffffff       4294967295 (2^32-1)
// 9999-12-31 23:59:59     0x3b7ff4417f       253402300799
//
// This arrengement will make it easier to deal with leap years,
// undefined modulo operations etc. for negative time_t values.
//

//
// This defines the biased date of the year 2000.  This is used to
// calculate the leap-year exceptions that repeat every 400 years.
//
#define BIASED_YEAR_2000_SECS		(UTIME_T)0xb86d4380

//
// This define is the very last second in a "sensible" date, that of
// 9999-12-31 23:59:59 which is the last one with a 4-digit year.
//
#define BIASED_VERY_LAST_SECOND		(UTIME_T)0x3b7ff4417f

#else // STDLIB_UNSIGNED_TIME_T
//
// The 'time_t' datatype is unsigned.  This means that no dates prior to
// 1970-01-01 00:00:00 can be represented and time_t can't be used to
// hold a negative time-difference, but on the other hand, dates up to
// 2106-02-07 06:28:15 can be held in a 32-bit variable (On systems
// with a signed time_t, the maximum date is 2038-01-19 03:14:07).
//

#define BIASED_TIME_T_VALUE				(UTIME_T)0
#define BIASED_YEAR_2000_SECS			(UTIME_T)946684800
#define BIASED_VERY_LAST_SECOND			(UTIME_T)253402300799

#endif // STDLIB_UNSIGNED_TIME_T

#define FOUR_CENTURIES_DAYS		(97*366 + 303*365)
#define FOUR_CENTURIES_SECS		(UTIME_T)(ONE_DAY_SECS * FOUR_CENTURIES_DAYS)
#define BIASED_YEAR_1904_SECS	(UTIME_T)0x03da4f80
#define YEAR_1972_SECS			(UTIME_T)63072000

struct tm* op_gmtime(const time_t *timer)
{
	time_t time = *timer;

	//
	// Verify that our predicted size of 'time_t' is correct. If it is not
	// correct, we will not be able to reproduce the dates of the system,
	// and possibly make incorrect computations if 'time_t' is 32-bits when
	// we think it is 64-bit.
	//
#if STDLIB_SIXTY_FOUR_BIT_TIME_T
	OP_STATIC_ASSERT(sizeof(time_t) == 8);
#else
	OP_STATIC_ASSERT(sizeof(time_t) == 4);
#endif

	UTIME_T biased_time = time + BIASED_TIME_T_VALUE;

	//
	// Make sure we return NULL if the 'time_t' value is spectacularly
	// outside the range that we support, which is:
	//
	// 1901-12-13 20:45:52 ==> 2038-01-19 03:14:07 (32-bit signed time_t)
	// 1901-12-13 20:45:52 ==> 9999-12-31 23:59:59 (64-bit signed time_t)
	// 1970-01-01 00:00:00 ==> 2106-02-07 06:28:15 (32-bit unsigned time_t)
	// 1970-01-01 00:00:00 ==> 9999-12-31 23:59:59 (64-bit unsigned time_t)
	//
#if STDLIB_SIXTY_FOUR_BIT_TIME_T
#  ifndef STDLIB_UNSIGNED_TIME_T
	// Make sure we don't go earlier than 1901-12-13 20:45:52
	if ( biased_time < 0 )
		return 0;
#  endif
	// Make sure we don't go past 9999-12-31 23:59:59
	if ( biased_time > BIASED_VERY_LAST_SECOND )
		return 0;
#endif

	int base_year;
	int base_wday;
	int days;
	int wday;
	int sec;
	BOOL leap_year;

#if STDLIB_SIXTY_FOUR_BIT_TIME_T
	//
	// We can do multiple centuries, so we need to take the leap year
	// rules of 100 and 400 years into account.  First map all 400
	// year blocks into a block starting at some year:
	//
	if ( biased_time < BIASED_YEAR_2000_SECS )
	{
		base_year = 1600;
		base_wday = 6;  // FIXME: Don't know
		biased_time += (FOUR_CENTURIES_SECS - BIASED_YEAR_2000_SECS);
	}
	else
	{
		base_year = 2000;
		base_wday = 6;
		biased_time -= BIASED_YEAR_2000_SECS;
	}

	days = (int)(biased_time / ONE_DAY_SECS);
	wday = (base_wday + days) % 7;
	int qcent = (int)(days / FOUR_CENTURIES_DAYS);
	biased_time -= qcent * FOUR_CENTURIES_SECS;
	days -= qcent * FOUR_CENTURIES_DAYS;

	if ( days < 366 )
	{
		// This is the first year of a quad-century; it is a leap year.
		leap_year = TRUE;
	}
	else
	{
		days--;  // Compensate for the leap year first in the quad-century

		int cent = days / (365*76 + 366*24);
		base_year += cent * 100;
		days -= cent * (365*76 + 366*24);

		if ( days < 365 )
		{
			// This is the first year of a century; it is not a leap year.
			leap_year = FALSE;
		}
		else
		{
			days++;  // Compensate for missing leap year first in every century

			int quad = days / (365*3 + 366);
			base_year += quad * 4;
			days -= quad * (365*3 + 366);

			if ( days < 366 )
			{
				// First year in every four year cycle; it is a leap year
				leap_year = TRUE;
			}
			else
			{
				days--;  // Compensate for leap year first in four year cycle

				int y = days / 365;
				base_year += y;
				days -= y * 365;
				leap_year = FALSE;
			}
		}
	}

#else // STDLIB_SIXTY_FOUR_BIT_TIME_T

	//
	// Provide the 32-bit version(s) here.  All 32-bit numbers will
	// represent valid dates.  Signed/unsigned will determine the
	// range of dates that 'time_t' represents.
	//

#ifdef STDLIB_UNSIGNED_TIME_T

	//
	// For unsigned 32-bit support, we have to handle 2100 specially; it
	// is not a leap year (but 2000 is).
	//
	if ( biased_time < YEAR_1972_SECS )
	{
		// Fold 1970 and 1971 into 1974 and 1975 to 
		base_year = 1968;
		base_wday = 1;
		biased_time += (FOUR_YEARS_SECS - YEAR_1972_SECS);
	}
	else
	{
		base_year = 1972;
		base_wday = 6;
		biased_time -= YEAR_1972_SECS;
	}

	days = biased_time / ONE_DAY_SECS;
	wday = (days + base_wday) % 7;

	//
	// Check for the year 2100 - which is not a leap year
	//
	if ( days >= 46752 )
	{
		// 2100-01-01 or later
		if ( days < 47117 )
		{
			// 2100-12-31 or earlier: Fake it so it doesn't become a leap year
			days += 366;
			base_year--;
		}
		else
		{
			// Adjust so years 2101 and up are correct
			days++;
		}
	}

#else // STDLIB_UNSIGNED_TIME_T

	//
	// 32-bit signed time_t will not need any special rules for leap year
	// calculation, so we just fold years 1-3 into years 5-7.
	//
	if ( biased_time < BIASED_YEAR_1904_SECS )
	{
		base_year = 1900;
		base_wday = 0; // FIXME
		biased_time += (FOUR_YEARS_SECS - BIASED_YEAR_1904_SECS);
	}
	else
	{
		base_year = 1904;
		base_wday = 5; // FIXME
		biased_time -= BIASED_YEAR_1904_SECS;
	}
		
	days = biased_time / ONE_DAY_SECS;
	wday = (days + base_wday) % 7;

#endif // STDLIB_UNSIGNED_TIME_T

	//
	// At this point, assume the year is aligned so year+0 is a leap year,
	// while year+1..year+3 are not.
	//

	int quad = days / (365*3 + 366);
	base_year += quad * 4;
	days -= quad * (365*3 + 366);

	if ( days < 366 )
	{
		// First year in every four year cycle; it is a leap year
		leap_year = TRUE;
	}
	else
	{
		days--;

		int y = days / 365;
		base_year += y;
		days -= y * 365;
		leap_year = FALSE;
	}

#endif // STDLIB_SIXTY_FOUR_BIT_TIME_T

	struct tm* r = &g_opera->stdlib_module.m_gmtime_result;

	int prior_days;

	r->tm_mon = find_month(days, leap_year, prior_days);
	r->tm_mday = days - prior_days + 1;
	r->tm_wday = wday;
	r->tm_yday = days;
	r->tm_year = base_year - 1900;

	sec = (int)(biased_time % ONE_DAY_SECS);
	r->tm_hour = sec / 3600;
	r->tm_min = (sec - r->tm_hour*3600) / 60;
	r->tm_sec = sec % 60;
	r->tm_isdst = 0;

	return r;
}

#endif // !HAVE_GMTIME

#ifndef HAVE_LOCALTIME

struct tm* op_localtime(const time_t *time_p)
{
	/* This will cause a loop if either OpSystemInfo::GetTimezone() or
	 * ::DaylightSavingsTimeAdjustmentMS() is implemented using
	 * op_localtime().
	 */
	time_t adjusted_time =
		*time_p
		// subtract current time zone
		- g_op_time_info->GetTimezone()
		// add daylight saving relevant for target date
		+ static_cast<time_t>(g_op_time_info->DaylightSavingsTimeAdjustmentMS(static_cast<double>(*time_p) * 1000.0) / 1000.0)
		// subtract current daylight saving
		- static_cast<time_t>(g_op_time_info->DaylightSavingsTimeAdjustmentMS(static_cast<double>(op_time(NULL)) * 1000.0) / 1000.0);

	return op_gmtime(&adjusted_time);
}

#endif // !HAVE_LOCALTIME

