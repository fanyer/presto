/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/**
 * @file datetime.h
 *
 * Help classes for the web forms 2 support. Dates mostly.
 *
 * @author Daniel Bratell
 */

#ifndef _DATETIME_H_
#define _DATETIME_H_

// Forward declarations
struct DaySpec;

/**
 * Contains a week number and a year. This is small enough to be
 * used by value. Disregarding padding, it's ~3 bytes.
 */
struct WeekSpec
{
	// Only ~4 bytes large: can pass by value easily
	/**
	 * The year (year 0 is possible though it doesn't exist).
	 */
	unsigned short m_year;
	/**
	 * The week - a number between 1 and 53.
	 */
	unsigned char m_week; // 1 - 53

	/**
	 * Converts this week to it's null terminated ISO-8601
	 * string equivalent.
	 * Caller must check length of buf before sending it in.
	 * @param buf A buffer to write the string in. The buf must be
	 * big enough for the string. What's big enough can be found out by
	 * calling GetISO8601StringLength.
	 */
	void ToISO8601String(uni_char* buf) const;

	/**
	 * The length of the string *NOT* including any terminating null chars.
	 *
	 * @returns The length.
	 */
	unsigned int GetISO8601StringLength() const;

	/**
	 * Sets the current WeekSpec from the specified string or returns
	 * FALSE if it fails to do so.
	 *
	 * @param date_string The string representing a date
	 * in ISO-8601 format.
	 *
	 * @returns FALSE if the string couldn't be parsed.
	 */
	BOOL SetFromISO8601String(const uni_char* date_string);

	/**
	 * Sets this WeekSpec by converting the "ms since 1970" time
	 * value given as input.
	 */
	void SetFromTime(double ms_since_1970);

	/**
	 * For comparisons and step calculations.
	 */
	double AsDouble();

	/**
	 * Returns TRUE if this WeekSpec is later than the
	 * other weekspec.
	 */
	BOOL IsAfter(const WeekSpec& other)
	{
		return m_year > other.m_year ||
			(m_year == other.m_year && m_week > other.m_week);
	}

	/**
	 * Returns TRUE if this WeekSpec is earlier than the
	 * other weekspec.
	 */
	BOOL IsBefore(const WeekSpec& other)
	{
		return m_year < other.m_year ||
			(m_year == other.m_year && m_week < other.m_week);
	}

	/**
	 * Returns the day that is the last day of the week (week
	 * according to european standard, monday - sunday
	 */
	DaySpec GetLastDay() const;

	/**
	 * Returns the day that is the first day of the week (week
	 * according to european standard, monday - sunday
	 */
	DaySpec GetFirstDay() const;

	/**
	 * The week after this.
	 */
	WeekSpec NextWeek() const;

	/**
	 * The week before this.
	 */
	WeekSpec PrevWeek() const;
};

/**
 * Contains a year and a month.
 */
struct MonthSpec
{
	// Only ~4 bytes large: can pass by value easily
	/**
	 * The year (year 0 is possible though it doesn't exist).
	 */
	unsigned short m_year;

	/**
	 * The month as a number 0 to 11. It uses a zero based index to make
	 * simpler to use the number as index into arrays.
	 */
	unsigned char m_month; // 0 (jan) - 11 (dec)

	/**
	 * Returns the month after this month.
	 */
	MonthSpec PrevMonth() const;

	/**
	 * Returns the month before this month.
	 */
	MonthSpec NextMonth() const;

	/**
	 * The first day in the month.
	 */
	DaySpec FirstDay() const;

	/**
	 * The last day in the month.
	 */
	DaySpec LastDay() const;

	/**
	 * Calculates the number of days in the month. This takes leap years
	 * into account.
	 *
	 * @return The number of days in the month (28, 29, 30 or 31).
	 */
	unsigned char DaysInMonth() const;

	/**
	 * Compares this month to another month and returns TRUE is this month
	 * is strictly after the other month.
	 *
	 * @param other The other month.
	 */
	BOOL IsAfter(MonthSpec other)
	{
		return	m_year > other.m_year ||
				(m_year == other.m_year &&
				 m_month > other.m_month);
	}

	/**
	 * Compares this month to another month and returns TRUE is this month
	 * is strictly before the other month.
	 *
	 * @param other The other month.
	 */
	BOOL IsBefore(MonthSpec other)
	{
		// Equiv code but is bigger than the same code written out
		// return !other.IsAfter(*this) &&
		// !(!other.IsAfter(*this)) && !IsAfter(other))
		return	m_year < other.m_year ||
				(m_year == other.m_year &&
				 m_month < other.m_month);
	}

	/**
	 * Compares this month to another month and returns TRUE if this month
	 * is exactly the same the other month.
	 *
	 * @param other The other month.
	 */
	BOOL operator == (const MonthSpec& other) const { return other.m_year == m_year && other.m_month == m_month; }

	/**
	 * Converts this week to it's null terminated ISO-8601
	 * string equivalent.
	 * Caller must check length of buf before sending it in.
	 * @param buf A buffer to write the string in. The buf must be
	 * big enough for the string. What's big enough can be found out by
	 * calling GetISO8601StringLength.
	 */
	void ToISO8601String(uni_char* buf) const;

	/**
	 * The length of the string *NOT* including any terminating null chars.
	 *
	 * @returns The length.
	 */
	unsigned int GetISO8601StringLength() const;

	/**
	 * Sets the current WeekSpec from the specified string or returns
	 * FALSE if it fails to do so.
	 *
	 * @param date_string The string representing a date
	 * in ISO-8601 format.
	 *
	 * @returns FALSE if the string couldn't be parsed.
	 */
	BOOL SetFromISO8601String(const uni_char* date_string);

	/**
	 * Sets this MonthSpec by converting the "ms since 1970" time
	 * value given as input.
	 */
	void SetFromTime(double ms_since_1970);

	/**
	 * For comparisons and step calculations. This is the number of months
	 * since 1970-01 ( = 0.0).
	 */
	double AsDouble();
};

/**
 * This struct represents a certain day a certain year.
 *
 * @author Daniel Bratell
 */
struct DaySpec
{
	// Only ~4 bytes large: can pass by value easily
	/**
	 * The year (year 0 is possible though it doesn't exist).
	 */
	unsigned short m_year;

	/**
	 * The month as a number 0 to 11. It uses a zero based index to make
	 * simpler to use the number as index into arrays.
	 */
	unsigned char m_month; // 0 (jan) - 11 (dec)

	/**
	 * The day in the month, a number between 1 and 31.
	 */
	unsigned char m_day; // 1 to 31

	/**
	 * Sets the date to an illegal but initialized value. Must be set
	 * by some other method before being used.
	 */
	void Clear() { m_year = 0; m_month = 0; m_day = 0; }

	/**
	 * Returns 0 to 6. 0 is monday. 6 is sunday.
	 */
	int DayOfWeek() const;

	/**
	 * Converts this date to it's null terminated ISO-8601
	 * string equivalent.
	 * Caller must check length of buf before sending it in.
	 * @param buf A buffer to write the string in. The buf must be
	 * big enough for the string. What's big enough can be found out by
	 * calling GetISO8601StringLength.
	 */
	void ToISO8601String(uni_char* buf) const;

	/**
	 * The length of the string *NOT* including any terminating null chars.
	 *
	 * @returns The length.
	 */
	unsigned int GetISO8601StringLength() const;

	/**
	 * Sets the current DaySpec from the specified string or returns
	 * FALSE if it fails to do so.
	 *
	 * @param date_string The string representing a date
	 * in ISO-8601 format.
	 *
	 * @param partial If TRUE, parse from the beginning of the string,
	 * but accepting trailing characters after the valid input.
	 *
	 * @returns FALSE if the string couldn't be parsed.
	 */
	BOOL SetFromISO8601String(const uni_char* date_string, BOOL partial = FALSE);

	/**
	 * Sets this DaySpec by converting the "ms since 1970" time
	 * value given as input.
	 */
	void SetFromTime(double ms_since_1970);

	/**
	 * Compares this date to another date and returns TRUE is this date
	 * is strictly after the other date.
	 *
	 * @param other The other date.
	 */
	BOOL IsAfter(DaySpec other)
	{
		return	m_year > other.m_year ||
				(m_year == other.m_year &&
					(m_month > other.m_month ||
					m_month == other.m_month && m_day > other.m_day));
	}

	/**
	 * Compares this date to another date and returns TRUE is this date
	 * is strictly before the other date.
	 *
	 * @param other The other date.
	 */
	BOOL IsBefore(DaySpec other)
	{
		// Equiv code but is bigger than the same code written out
		// return !other.IsAfter(*this) &&
		// !(!other.IsAfter(*this)) && !IsAfter(other))
		return	m_year < other.m_year ||
				(m_year == other.m_year &&
					(m_month < other.m_month ||
					m_month == other.m_month && m_day < other.m_day));
	}

	/**
	 * Returns the week number of a date according to the definition in
	 * ISO-8601.
	 */
	WeekSpec GetWeek() const;

	/**
	 * For use in step calculations. Base (0.0) is 1970-W01.
	 */
	double AsDouble();

	/**
	 * The day after this.
	 */
	DaySpec NextDay() const;

	/**
	 * The day before this.
	 */
	DaySpec PrevDay() const;

	/**
	 * The month of the day. Exists from April 2006.
	 */
	MonthSpec Month() const { MonthSpec month = { m_year, m_month }; return month; }

	/**
	 * Sets this DaySpec to the first day of the given MonthSpec.
	 */
	void SetFromMonth(const MonthSpec &month) { m_year = month.m_year; m_month = month.m_month; m_day = 1; }
};

/**
 * A time of day. Might be without specified seconds and minutes.
 */
struct TimeSpec
{
	// Only ~8 bytes large: can pass by value easily
	/**
	 * The hour, 0-23.
	 */
	unsigned char m_hour; // 0-23

	/**
	 * The minute - 0-59 or -1 if not specified.
	 */
	signed char m_minute; // 0-59 (-1 == not specified)

	/**
	 * The second - 0-59 or -1 if not specified.
	 */
	signed char m_second; // 0-59 (-1 == not specified)

	/**
	 * The number of digits in the fraction or -1 if no fraction specified.
	 */
	signed char m_fraction_digit_count; // 0-9 (-1 == not specified)

	/**
	 * The hundreths - 0-99
	 */
	unsigned int m_fraction; // 0-99

	/**
	 * Empties, sets to 00:00 with no seconds.
	 */
	void Clear()
	{
		m_hour = 0;
		m_minute = 0;
		m_second = -1;
		SetFraction(0,0);
	}

	/**
	 * Converts the TimeSpec to its ISO-8601 string.
	 *
	 * @param buf A buffer to write the string in. The buf must be
	 * big enough for the string. What's big enough can be found out by
	 * calling GetISO8601StringLength.
	 */
	void ToISO8601String(uni_char* buf) const;
	/**
	 * The length of the time as an ISO-8601 string (not counting the
	 * terminating NULL).
	 *
	 * @returns The length.
	 */
	unsigned int GetISO8601StringLength() const;

	/**
	 * Sets the current TimeSpec from the specified string or returns
	 * FALSE if it fails to do so.
	 *
	 * @param time_string The string representing a time
	 * in ISO-8601 format.
	 *
	 * @param partial If TRUE, parse from the beginning of the string,
	 * but accepting trailing characters after the valid input.
	 *
	 * @returns The number of characters of the string consumed, or 0
	 * if the string couldn't be parsed.
	 */
	unsigned int SetFromISO8601String(const uni_char* time_string, BOOL partial = FALSE);

	/**
	 * Sets this TimeSpec by converting the "ms since 1970" time
	 * value given as input.
	 */
	void SetFromTime(double ms_since_1970);

	/**
	 * Compares this TimeSpec to another TimeSpec and returns TRUE if
	 * This TimeSpec is strictly after the other TimeSpec. No fields
	 * may be unspecified for this to work.
	 *
	 * @param other The other TimeSpec.
	 */
	BOOL IsAfter(TimeSpec other) const
	{
#if 1
		return AsDouble() > other.AsDouble();
#else
		return	m_hour > other.m_hour ||
				(m_hour == other.m_hour &&
					(m_minute > other.m_minute
					|| m_minute == other.m_minute && m_second > other.m_second
					));
#endif // 0
	}

	/**
	 * Compares this TimeSpec to another TimeSpec and returns TRUE if
	 * This TimeSpec is strictly before the other TimeSpec. No fields
	 * may be unspecified for this to work.
	 *
	 * @param other The other TimeSpec.
	 */
	BOOL IsBefore(TimeSpec other) const
	{
		return AsDouble() < other.AsDouble();
	}

	/**
	 * For use in step calculations. Base (0.0) is when?
	 */
	double AsDouble() const;

	/**
	 * Returns the fraction part of a second. Use
	 * GetFractionDigitCount() to get the number of digits in the
	 * fraction. The fraction is then
	 * GetFraction()/(10^GetFractionDigitCount()) seconds.
	 */
	int GetFraction() const;

	/**
	 * Returns the number of decimal digits in the fraction.
	 */
	int GetFractionDigitCount() const;

	/**
	 * Returns the number of seconds, or 0 if not specified.
	 */
	int GetSecond() const { return m_second == -1 ? 0 : m_second; }
	/**
	 * Returns the number of minutes, or 0 if not specified.
	 */
	int GetMinute() const { return m_minute == -1 ? 0 : m_minute; }
	/**
	 * Returns the number of hours.
	 */
	int GetHour() const { return m_hour; }

	/**
	 * Makes the second undefined.
	 */
	void SetSecondUndefined() { m_second = -1; }

	/**
	 * Makes the fraction undefined.
	 */
	void SetFractionUndefined() { m_fraction_digit_count = 0; }

	/**
	 * Checks if Makes the second undefined.
	 */
	BOOL IsSecondUndefined() const { return m_second == -1; }

	/**
	 * Checks if the fraction is undefined.
	 */
	BOOL IsFractionUndefined() const { return m_fraction_digit_count < 1; }

	/**
	 * Sets the sub second part as
	 * fraction * (10^-fraction_digit_count)
	 *
	 * @param fraction The number
	 * @param fraction_digit_count How many digits there are in the fraction.
	 */
	void SetFraction(int fraction, int fraction_digit_count);

	/**
	 * Converts a number of seconds multiplied with multiplied_factor
	 * to a time in this struct. It uses the existing second field and
	 * fraction field to decide if they should be left out or set to
	 * 0, in case there are no second/fraction part of the number.
	 */
	void SetFromNumber(int value, int multiplied_factor);
};

#if 0
struct TimeSpecEx : public TimeSpec
{
	TimeSpecEx()
	{
		m_hour = 0;
		m_minute = 0;
		m_second = -1;
		m_fraction_digit_count = 0;
	}
};
#endif

/**
 * A time-zone offset specified as hours and minutes. The format is
 * either 'Z' (UTC), +HH:MM or -HH:MM.
 */
struct TimeZoneSpec
{
	/**
	 * The sign, '-' or '+'
	 */
	char m_sign;

	/**
	 * The hour, 0-23.
	 */
	unsigned char m_hour;

	/**
	 * The minute, 0-59.
	 */
	unsigned char m_minute;

	/**
	 * Empties, sets to +00:00
	 */
	void Clear()
	{
		m_sign = '+';
		m_hour = 0;
		m_minute = 0;
	}

	/**
	 * Sets the current TimeSpec from the specified string or returns
	 * FALSE if it fails to do so.
	 *
	 * @param time_string The string representing a time
	 * in ISO-8601 format.
	 *
	 * @param partial If TRUE, parse from the beginning of the string,
	 * but accepting trailing characters after the valid input.
	 *
	 * @returns The number of characters of the string consumed, or 0
	 * if the string couldn't be parsed.
	 */
	unsigned int SetFromISO8601String(const uni_char* time_string, BOOL partial = FALSE);

	/**
	 * The length of the string <b>NOT</b> including any
	 * terminating null chars.
	 *
	 * @returns The length.
	 */
	unsigned int GetISO8601StringLength() const
	{
		// As it is either 'Z' or [+-]HH:MM
		return IsZ() ? 1 : 6;
	}

	/**
	 * The offset in seconds.
	 */
	double AsDouble() const;

	/**
	* Returns true iff this is the +00:00 (AKA 'Z') timezone.
	 */
	BOOL  IsZ() const
	{
		return m_hour == 0 && m_minute == 0;
	}
};

/**
 * Combines a date with a time and a time zone.
 */
struct DateTimeSpec
{
	/**
	 * The date part of the datetime.
	 */
	DaySpec m_date;
	/**
	 * The time part of the datetime.
	 */
	TimeSpec m_time;
	/**
	 * The time zone part of the datetime.
	 */
	TimeZoneSpec m_timezone;

	/**
	 * Sets the datetime to an illegal but initialized value. Must be
	 * set by some other method before being used.
	 */
	void Clear() { m_date.Clear(); m_time.Clear(); m_timezone.Clear(); }

	/**
	 * Converts this datetime to a null terminated string according
	 * to the ISO-8601 format.
	 * Caller must check length of buf before sending it in.
	 * @param buf A buffer to write the string in. The buf must be
	 * big enough for the string. What's big enough can be found out by
	 * calling GetISO8601StringLength.
	 *
	 * @param include_timezone Set to TRUE if the DateTimeSpec has its TimeZoneSpec set.
	 * In this case DateTimeSpec is not rendered "as is" but as its equivalent
	 * moment in time in UTC (in the format with trailing 'Z').
	 * E.g. "2011-03-24T10:42+02:00" is rendered as "2011-03-24T08:42Z"
	 */
	void ToISO8601String(uni_char* buf, BOOL include_timezone) const;

	/**
	 * The length of the string <b>NOT</b> including any
	 * terminating null chars.
	 *
	 * @returns The length.
	 */
	unsigned int GetISO8601StringLength(BOOL include_timezone) const;

	/**
	 * Sets this datetime by reading the specified string that should
	 * be a string in the ISO8601 datetime format.
	 *
	 * @param partial If TRUE, parse from the beginning of the string,
	 * but accepting trailing characters after the valid input.
	 *
	 * @returns The number of characters of the string consumed, or 0
	 * if the string couldn't be parsed.
	 */
	unsigned int SetFromISO8601String(const uni_char* date_time_string,
									  BOOL include_timezone,
									  BOOL partial = FALSE);


	/**
	 * Sets this datetime by converting the "ms since 1970" time
	 * value given as input.
	 */
	void SetFromTime(double ms_since_1970);

	/**
	 * Compares this datetime to another date and returns
	 * TRUE is this date
	 * is strictly after the other datetime.
	 *
	 * @param other The other datetime.
	 */
	BOOL IsAfter(DateTimeSpec other)
	{
		return	m_date.IsAfter(other.m_date) ||
				(!other.m_date.IsAfter(m_date) &&  // implies the same m_date
				  m_time.IsAfter(other.m_time));
	}

	/**
	 * Compares this datetime to another date and returns
	 * TRUE is this date
	 * is strictly before the other datetime.
	 *
	 * @param other The other datetime.
	 */
	BOOL IsBefore(DateTimeSpec other)
	{
#if 1
		return AsDouble() < other.AsDouble();
#else
		return m_date.IsBefore(other.m_date) ||
				(!other.m_date.IsBefore(m_date) &&  // implies the same m_date
				  m_time.IsBefore(other.m_time));
#endif
	}

	/**
	 * For use in step calculations. Number of millisconds since
	 * 1970-01-01 00:00:00.
	 */
	double AsDouble();

	/**
	 * (Inplace) convert *this to a DateTimeSpec, specifying the same moment in time, but with TimeZoneSpec set to +00:00 (i.e. UTC)
	 */
	void  ConvertToUTC();
};

#endif // _DATETIME_H_
