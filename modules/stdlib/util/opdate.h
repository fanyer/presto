/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPDATE_H
#define OPDATE_H

/* Defines a class OpDate and many utility functions to manipulate, parse, and
   print dates representing millisecond quantities since epoch, represented using
   a double.  Note that the time stamps can be negative, representing times
   before the epoch.

   The definition here is the ECMAScript specification: ES-262, 3rd edition. */

static const int	HoursPerDay = 24;
static const int	MinutesPerHour = 60;
static const int	SecondsPerMinute = 60;

static const double	msPerSecond = 1000.0;
static const double	msPerMinute = 60000.0;
static const double	msPerHour = 3600000.0;
static const double	msPerDay = 86400000.0;

class OpDate
{
public:
	static double GetCurrentUTCTime();
		/**< @return the current UTC time stamp  */

	static double Day(double t);
		/**< @return the day number for a UTC time stamp */

	static double TimeWithinDay(double t);
		/**< @return the time within a day for a UTC time stamp */

	static int DaysInYear(int y);
		/**< @return the number of days in a given year */

	static int DayFromYear(double y);
		/**< @return the day number for the first day of the year 'y' */

	static int YearFromTime(double t);
		/**< @return the year number for the UTC time stamp 't' */

	static double TimeFromYear(int y);
		/**< @return the time stamp for the start of the year 'y' */

	static BOOL	InLeapYear(double t);
		/**< @return TRUE iff t is a time stamp in a leap year */

	static int DayWithinYear(double t);
		/**< @return the number of days within the year represented
			         by the UTC time stamp 't' */

	static int MonthFromTime(double t);
		/**< @return the month number (zero-based) for the UTC time
					 stamp 't'  */

	static int DateFromTime(double t);
		/**< @return the day-in-month number (zero-based) for the UTC
					 time stamp 't' */

	static int WeekDay(double t);
		/**< @return the day-in-week number (zero-based) for the UTC
					 time stamp 't' */

	static double LocalTZA();
		/**< @return the time zone adjustment needed to bring a UTC time
					 stamp to local standard time, ie, not including any
					 daylight savings adjustment */

	static double LocalTZA(double utc);
		/**< @return the time zone adjustment needed to bring given UTC time
					 stamp to local standard time, ie, not including any
					 daylight savings adjustment */

	static double DaylightSavingTA(double t, double now_utc = 0);
		/**< @return the daylight savings time adjustment for the time
					 stamp t, using the current daylight savings time
					 algorithm.

					 ("Use the current algorithm" simplifies the computation.
					 For example, in 1996 Norway changed the day on which the
					 switch from standard time to daylight savings occurs.
					 When DaylightSavingTA is given a date in 1995 or
					 earlier, it will still use the algorithm for computing
					 the daylight savings time in 2005, not the algorithm
					 that was current at the time.)  */

	static double LocalTime(double t);
		/**< @return the local time corresponding to the UTC time stamp 't' */

	static double UTC(double t);
		/**< @return the UTC time stamp corresponding to the local time 't' */

	static int HourFromTime(double t);
		/**< @return the hour (zero-based) for the UTC time stamp 't' */

	static int MinFromTime(double t);
		/**< @return the minute (zero-based) for the UTC time stamp 't' */

	static int SecFromTime(double t);
		/**< @return the second (zero-based) for the UTC time stamp 't' */

	static double msFromTime(double t);
		/**< @return the millisecond (zero-based) for the UTC time stamp 't' */

	static double MakeTime(double hour, double min, double sec, double ms);
		/**< @return a millisecond (zero-based) within a day for the given
					 hour, minute, second, and millisecond timestamp */

	static double MakeDay(double year, double month, double day);
		/**< @return a day number relative to epoch for the given year, month,
					 and day, where year can be negative */

	static double MakeDate(double day, double time);
		/**< @return a date stamp when given a day value as returned by
		             MakeDay and a time value as returned by MakeTime  */

	static double TimeClip(double time);
		/**< @return a normalized time value for the time 'time' */

	static double ParseDate(const uni_char* date_str, BOOL strict_fractions = TRUE);
		/**< Parse the given date string.
		     @param date_str a unicode string representing a date in RFC3339
		            format.
		     @param strict_fractions If TRUE and a fractional second field is
		            used in the input, then only allow a 3-digit field. If FALSE,
		            the fractional seconds is either first right zero-padded to
		            3 digits or truncated at 3 digits instead.
		            EcmaScript permits implementation-dependent behaviour for
		            parsing Date strings, and the strict_fractions = FALSE behaviour
		            aligns better with others.
		     @return the UTC time stamp corresponding to the date represented
		             by the string 'date_str'.  This is an inexact science; the
		             parser will try its best to get unambiguous dates right, and
		             will use heuristics to extract information from other strings.  */

	static double ParseRFC3339Date(const uni_char* date_str);
		/**< Parse a RFC 3339 date (e.g. "1937-11-31T12:05:27.871-03:20").
		     RFC 3339 dates is a profile of ISO 8601 standard for
		     representation of dates and times.
		     This parser is very strict and will not accept any deviations from
		     the standard except for leading whitespace and any trailing
		     characters. Years MUST be four digits, and all the other numbers
		     except fractional seconds MUST be two digits.

		     @param date_str a unicode string representing a date in RFC3339
		                     format
		     @return the UTC time stamp corresponding to the date represented
		             by the string 'date_str', or op_nan if date_str is not
		             a valid RFC 3339 date. */
private:
	static int	ParseTimeOfDay(const uni_char* &date_str, int &hour, int &min, int &sec, BOOL strict, int max_elems, BOOL &valid);
	static int  ParseTimeISO(const uni_char* &date_str, int &tm_hour, int &tm_min, double &tm_sec, BOOL strict_fractions, BOOL &valid);
	static int  ParseDateISO(const uni_char* &date_str, int &tm_year, int &tm_mon, int &tm_day, BOOL &valid);
	static BOOL ParseTimezoneISO(const uni_char* &date_str, int &tz_min);
	static BOOL ParseTimezone(const uni_char *&date_str, int &tz_min, BOOL &valid);
	static BOOL ParseMonth(const uni_char *&date_str, int &month);
	static BOOL ParseDayOfWeek(const uni_char* &date_str);
	static BOOL ParseComment(const uni_char* &date_str);
	static BOOL ParseTimeAMPM(const uni_char* &date_str, BOOL &is_pm);
	static BOOL ParseRFC3339Date(const uni_char*& date_str, int &year, int &mon, int &day, int &hour, int &min, double &sec, int& tm_min);
	static int  ParseNumber(const uni_char* &str, int &value);
	static int  ParseMilliseconds(const uni_char* &str, int &value);

	static void FindMonth(double t, const int *&day_array, int &month, int &day_in_year);
	static BOOL SkipSeps(const uni_char* &date_str);
};

/* static */ inline double
OpDate::Day(double t)
{
	return op_floor(t / msPerDay);
}

/* static */ inline double
OpDate::TimeWithinDay(double t)
{
	double ms = op_fmod(t, msPerDay);
	if (ms < 0)
		ms += msPerDay;
	return ms;
}

/* static */ inline double
OpDate::TimeFromYear(int y)
{
	return msPerDay * DayFromYear(y);
}

/* static */ inline BOOL
OpDate::InLeapYear(double t)
{
	return DaysInYear(YearFromTime(t)) == 365 ? FALSE : TRUE;
}

/* static */ inline int
OpDate::DayWithinYear(double t)
{
	return op_double2int32(Day(t) - DayFromYear(YearFromTime(t)));
}

/* static */ inline int
OpDate::WeekDay(double t)
{
	int day = op_double2int32(Day(t) + 4) % 7;
	if (day < 0)
		day += 7;
	return day;
}

/* static */ inline int
OpDate::HourFromTime(double t)
{
	int hour = op_double2int32(op_fmod(op_floor(t / msPerHour), (double)HoursPerDay));
	if (hour < 0)
		hour += HoursPerDay;
	return hour;
}

/* static */ inline int
OpDate::MinFromTime(double t)
{
	int min = op_double2int32(op_fmod(op_floor(t / msPerMinute), (double)MinutesPerHour));
	if (min < 0)
		min += MinutesPerHour;
	return min;
}

/* static */ inline int
OpDate::SecFromTime(double t)
{
	int sec = op_double2int32(op_fmod(op_floor(t / msPerSecond), (double)SecondsPerMinute));
	if (sec < 0)
		sec += SecondsPerMinute;
	return sec;
}

/* static */ inline double
OpDate::msFromTime(double t)
{
	double ms = op_fmod(t, msPerSecond);
	if (ms < 0)
		ms += msPerSecond;
	return ms;
}

/* static */ inline double
OpDate::MakeTime(double hour, double min, double sec, double ms)
{
	if (!op_isfinite(hour) || !op_isfinite(min) || !op_isfinite(sec) || !op_isfinite(ms))
		return op_nan(NULL);

	return op_truncate(hour) * msPerHour + op_truncate(min) * msPerMinute + op_truncate(sec) * msPerSecond + op_truncate(ms);
}

/* static */ inline double
OpDate::MakeDate(double day, double time)
{
	if (!op_isfinite(day) || !op_isfinite(time))
		return op_nan(NULL);

	return day * msPerDay + time;
}

#endif /* OPDATE_H */
