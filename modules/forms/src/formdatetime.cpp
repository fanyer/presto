/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/**
 * @author Daniel Bratell
 */
#include "core/pch.h"

#include "modules/forms/datetime.h"
#include "modules/util/str.h"
#include "modules/util/datefun.h"
#include "modules/stdlib/util/opdate.h"

// Local help funtion
inline static int digit(uni_char a_char) { return a_char - '0'; }

// Local help function
static BOOL IsLeapYear(int year)
{
	return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

// 4-digit years, prefixed with 0s if needed.
static void ToISO8601YearString(unsigned year, uni_char *buf)
{
	unsigned i = 0;
	if (year < 1000)
	{
		buf[i++] = '0';
		if (year < 100)
		{
			buf[i++] = '0';
			if (year < 10)
				buf[i++] = '0';
		}
	}
	uni_itoa(year, &buf[i], 10);
}

void WeekSpec::ToISO8601String(uni_char* buf) const
{
	OP_ASSERT(m_year <= 9999);
	ToISO8601YearString(m_year, buf);
	buf[4] = '-';
	buf[5] = 'W';
	buf[6] = m_week/10 + '0';
	buf[7] = m_week%10 + '0';
	buf[8] = '\0';
}

unsigned int WeekSpec::GetISO8601StringLength() const
{
	return 8;
}

BOOL WeekSpec::SetFromISO8601String(const uni_char* week_string)
{
	m_year = 0;
	for (int i = 0; i < 4; i++)
	{
		if (!uni_isdigit(week_string[i]))
		{
			return FALSE;
		}
		m_year = m_year*10 + digit(week_string[i]);
	}
	if (week_string[4] != '-')
	{
		return FALSE;
	}
	if (week_string[5] != 'W')
	{
		return FALSE;
	}
	if (!uni_isdigit(week_string[6]) || !uni_isdigit(week_string[7]) || week_string[8])
	{
		return FALSE;
	}
	m_week = digit(week_string[6])*10 + digit(week_string[7]);

	if (m_year < 1000 || m_week < 1 || m_week > 53)
	{
		return FALSE;
	}

	// XXX Doesn't check if week 53 is legal for the given year.

	return TRUE;
}

void WeekSpec::SetFromTime(double ms)
{
	DaySpec day;
	day.m_year = OpDate::YearFromTime(ms);
	day.m_month = OpDate::MonthFromTime(ms);
	day.m_day = OpDate::DateFromTime(ms);

	WeekSpec week = day.GetWeek();
	m_year = week.m_year;
	m_week = week.m_week;
}

double WeekSpec::AsDouble()
{
#if 0
	// XXXX This is buggy for weeks before 1970-W01.

	// 1970-01-01 was a thursday. Found the date of the
	// thursday in the specified week

	// Find date of thursday week 1 of the year
	DaySpec d = { m_year, 0, 1 };

	int day_of_week = d.DayOfWeek();
	int date_of_first_week_thursday;
	// thursday is 3, if we get > 3 then this is the last week last year
	if (day_of_week > 3)
	{
		date_of_first_week_thursday = 11 - day_of_week;
	}
	else
	{
		date_of_first_week_thursday = 4 - day_of_week;
	}

	DaySpec first_week_thursday = { m_year, 0, date_of_first_week_thursday };
	double days_since_epoch = first_week_thursday.AsDouble();
	BOOL negative = days_since_epoch < 0;
	if (negative)
	{
		days_since_epoch = -days_since_epoch;
	}
	int weeks_since_epoch = (int)((days_since_epoch+3.5) / 7);
	if (negative)
	{
		days_since_epoch = -days_since_epoch;
	}
	return double(weeks_since_epoch + m_week - 1);
#else
	WeekSpec base_week = { 1970, 01 };
	double diff_days = GetLastDay().AsDouble() - base_week.GetLastDay().AsDouble();
	OP_ASSERT(diff_days == static_cast<int>(diff_days));
	OP_ASSERT(diff_days/7.0 == static_cast<int>(diff_days/7.0));
	return static_cast<int>(diff_days)/7;
#endif // 0
}

DaySpec WeekSpec::GetFirstDay() const
{
	DaySpec as_day = GetLastDay();
	// Move from sunday to monday
	if (as_day.m_day > 6)
	{
		as_day.m_day -= 6;
	}
	else
	{
		MonthSpec month = { as_day.m_year, as_day.m_month };
		month = month.PrevMonth();
		as_day.m_day = as_day.m_day -6 + month.DaysInMonth();
		as_day.m_year = month.m_year;
		as_day.m_month = month.m_month;
	}

	return as_day;
}


DaySpec WeekSpec::GetLastDay() const
{
	DaySpec day_for_last_in_week_1 = { m_year, 0, 4 };
	day_for_last_in_week_1.m_day += 6 - day_for_last_in_week_1.DayOfWeek(); // To sunday
	if (day_for_last_in_week_1.m_day > 31)
	{
		day_for_last_in_week_1.m_day -= 31;
		day_for_last_in_week_1.m_month = 0;
		day_for_last_in_week_1.m_year++;
	}
	OP_ASSERT(day_for_last_in_week_1.DayOfWeek() == 6);

	if (day_for_last_in_week_1.GetWeek().m_week != 1)
	{
		day_for_last_in_week_1.m_day += 7; // 7 days a week (unless you're a Beatles fan)
		if (day_for_last_in_week_1.m_day > 31) // 31 days in dec
		{
			day_for_last_in_week_1.m_month = 0;
			day_for_last_in_week_1.m_day -= 31;
			day_for_last_in_week_1.m_year++;
		}
	}

	OP_ASSERT (day_for_last_in_week_1.GetWeek().m_week == 1);
	OP_ASSERT (day_for_last_in_week_1.m_year == m_year);

	// I have no good algorithm for this. Use a "brute force" method that is limited
	// to 11 iterations.
	int day_number = day_for_last_in_week_1.m_day;
	MonthSpec month = { m_year, 0 };
	day_number += 7 * (m_week - 1);
	while (day_number > month.DaysInMonth())
	{
		day_number -= month.DaysInMonth();
		if (month.m_month == 11)
		{
			OP_ASSERT(day_number < 4); // Bad week number. Should have been caught earlier
			month.m_month = 0;
			month.m_year++;
		}
		else
		{
			month.m_month++;
		}
	}
	OP_ASSERT(day_number >= 1 && day_number <= 31);
	DaySpec res = {month.m_year, month.m_month, static_cast<unsigned char>(day_number)};
	return res;
}

WeekSpec WeekSpec::NextWeek() const
{
	DaySpec d = GetLastDay().NextDay();
	OP_ASSERT(d.GetWeek().m_week != m_week);
	return d.GetWeek();
}

WeekSpec WeekSpec::PrevWeek() const
{
	DaySpec d = GetFirstDay().PrevDay();
	OP_ASSERT(d.GetWeek().m_week != m_week);
	return d.GetWeek();
}

// MonthSpec

MonthSpec MonthSpec::PrevMonth() const
{
	MonthSpec prev = {
		static_cast<unsigned short>(m_year - (m_month == 0 ? 1:0)),
		static_cast<unsigned char>((m_month + 11) % 12)
	};
	return prev;
}

MonthSpec MonthSpec::NextMonth() const
{
	MonthSpec next = {
		static_cast<unsigned short>(m_year + (m_month == 11 ? 1:0)),
		static_cast<unsigned char>((m_month + 1) % 12)
	};
	return next;
}

DaySpec MonthSpec::FirstDay() const
{
	DaySpec first = { m_year, m_month, 1 };
	return first;
}

DaySpec MonthSpec::LastDay() const
{
	DaySpec last = { m_year, m_month, DaysInMonth() };
	return last;
}

void MonthSpec::ToISO8601String(uni_char* buf) const
{
	OP_ASSERT(m_year <= 9999);
	ToISO8601YearString(m_year, buf);
	buf[4] = '-';
	buf[5] = (m_month+1)/10 + '0';
	buf[6] = (m_month+1)%10 + '0';
	buf[7] = '\0';
}

unsigned int MonthSpec::GetISO8601StringLength() const
{
	return 7;
}

BOOL MonthSpec::SetFromISO8601String(const uni_char* month_string)
{
	m_year = 0;
	for (int i = 0; i < 4; i++)
	{
		if (!uni_isdigit(month_string[i]))
		{
			return FALSE;
		}
		m_year = m_year*10 + digit(month_string[i]);
	}
	if (month_string[4] != '-')
	{
		return FALSE;
	}
	if (!uni_isdigit(month_string[5]) || !uni_isdigit(month_string[6]) || month_string[7])
	{
		return FALSE;
	}
	m_month = digit(month_string[5])*10 + digit(month_string[6]);

	if (m_year < 1000 || m_month < 1 || m_month > 12)
	{
		return FALSE;
	}

	m_month--; // our months are 0-11, not 1-12

	return TRUE;
}

void MonthSpec::SetFromTime(double ms)
{
	m_year = OpDate::YearFromTime(ms);
	m_month = OpDate::MonthFromTime(ms);
}

double MonthSpec::AsDouble()
{
	return 12.0 * (m_year - 1970) + m_month; // 1970-01 is the base == 0.0
}

#ifdef _DEBUG
static int OldDayOfWeek(int m_year, int m_month, int m_day)
{
	static const unsigned char mkeys[12] = { 1, 4, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6 };

	int day;
    day = (m_year-1900) + (m_year-1900)/4 + mkeys[static_cast<int>(m_month)] + m_day - 1;
    /* The above counts the leap day even if it occurs later in the year */
    if(m_year > 1900 && m_year % 4 == 0 && m_month < 2)
	{
      day--;
	}
	day--; // Change to make monday the first day of the week
    day %= 7;
    return day;
}
#endif // _DEBUG


// This one isn't as limited.
/**
 *
 * From http://www.faqs.org/faqs/sci-math-faq/dayWeek/
 *
 *  The following formula, which is for the Gregorian calendar only, may
 *  be more convenient for computer programming. Note that in some
 *  programming languages the remainder operation can yield a negative
 *  result if given a negative operand, so mod 7 may not translate to a
 *  simple remainder. W = (k + floor(2.6m - 0.2) - 2C + Y + floor(Y/4) +
 *  floor(C/4)) mod 7 where floor() denotes the integer floor function,
 *  k is day (1 to 31)
 *  m is month (1 = March, ..., 10 = December, 11 = Jan, 12 = Feb) Treat
 *  Jan &Feb as months of the preceding year
 *  C is century (1987 has C = 19)
 *  Y is year (1987 has Y = 87 except Y = 86 for Jan &Feb)
 *  W is week day (0 = Sunday, ..., 6 = Saturday)
 *  Here the century and 400 year corrections are built into the formula.
 *  The floor(2.6m - 0.2) term relates to the repetitive pattern that the
 *  30-day months show when March is taken as the first month.
 */
int DaySpec::DayOfWeek() const
{
	int year = m_year;
	// We have a cycle of 400 years so we can just move the year to the range 1700-2099
	if (year < 1700)
	{
		year += 400*((2099-year)/400);
	}
	else if (year > 2099)
	{
		year -= 400*((year-1700)/400);
	}

	OP_ASSERT(year > 1699 && year < 2100);

	// "Treat Jan &Feb as months of the preceding year"
	int month;
	if (m_month < 2)
	{
		year--;
		month = m_month + 11;
	}
	else
	{
		month = m_month - 1;
	}

	int century = year / 100;
	int year_in_century = year % 100;
	int day = (m_day + static_cast<int>(2.6*month - 0.2) - 2*century + year_in_century + (year_in_century/4) +
		century/4 + 70 - 1) % 7; // adding 70 to get a positive number, removing 1 since we have monday = 0, not sunday as the formula expects.

	// min = 1+2.6-0.2-2*40

#ifdef _DEBUG
	if (m_year > 1900 && m_year < 2100)
	{
		OP_ASSERT(day == OldDayOfWeek(m_year, m_month, m_day));
	}
#endif // _DEBUG

    return day;
}


void DaySpec::ToISO8601String(uni_char* buf) const
{
	OP_ASSERT(m_year <= 9999);
	ToISO8601YearString(m_year, buf);
	buf[4] = '-';
	OP_ASSERT(m_month <= 11);
	if (m_month < 9)
	{
		buf[5] = '0';
		buf[6] = m_month + 1 + '0';
	}
	else
	{
		buf[5] = '1';
		buf[6] = m_month + 1 - 10 + '0';
	}
	buf[7] = '-';
	OP_ASSERT(m_day >= 1 && m_day <= 31);
	buf[8] = m_day/10 + '0';
	buf[9] = m_day%10 + '0';
	buf[10] = '\0';
}

unsigned int DaySpec::GetISO8601StringLength() const
{
	return 4+1+2+1+2; // Not including null byte
}

BOOL DaySpec::SetFromISO8601String(const uni_char* value, BOOL partial /* = FALSE */)
{
	// A date (year, month, day) encoded according to [ISO8601], e.g.:
	// 1995-12-31.

	// Does this duplicate some existing tested code?
	if (uni_isdigit(value[0]) && uni_isdigit(value[1]) && uni_isdigit(value[2]) && uni_isdigit(value[3]) &&
		value[4] == '-' && uni_isdigit(value[5]) && uni_isdigit(value[6]) && value[7] == '-' &&
		uni_isdigit(value[8]) && uni_isdigit(value[9]) &&
		(partial || value[10] == '\0'))
	{
		m_year = digit(value[0])*1000+digit(value[1])*100+digit(value[2])*10+digit(value[3]);
		m_month = digit(value[5])*10 + digit(value[6]);
		m_day = digit(value[8])*10 + digit(value[9]);

		if (m_year == 0)
		{
			return FALSE;
		}

		if (m_month < 1 || m_month > 12)
		{
			return FALSE;
		}

		MonthSpec month = {m_year, static_cast<unsigned char>(m_month - 1)};
		if (m_day < 1 || m_day > month.DaysInMonth())
		{
			return FALSE;
		}

		m_month--; // We use months between 0 and 11

		return TRUE;
	}
	return FALSE;
}

void DaySpec::SetFromTime(double ms)
{
	m_year = OpDate::YearFromTime(ms);
	m_month = OpDate::MonthFromTime(ms);
	m_day = OpDate::DateFromTime(ms);
}

double DaySpec::AsDouble()
{
	// FIXME: Code duplication? Very similar code in OpDate

	double double_val = 0.0;
	// Let 1970-01-01 be our "zero" (it is in the specification)
	unsigned short year = 1970;
	int direction = year < m_year ? 1 : -1;
	while (year != m_year)
	{
		int year_to_skip = direction > 0 ? year : year - 1;
		if (IsLeapYear(year_to_skip))
		{
			double_val += direction*366.0;
		}
		else
		{
			double_val += direction*365.0;
		}
		year += direction;
	}

	// Correct year. We're a January 1st
	unsigned char month = 0;
	while (month < m_month)
	{
		MonthSpec m = {year, month};
		double_val += m.DaysInMonth();
		month++;
	}

	// Correct month. We're at The first day in the month
	double_val += m_day - 1;

	return double_val;
}

WeekSpec DaySpec::GetWeek() const
{
	// Algorithm taken from http://www.tondering.dk/claus/cal/node7.html#SECTION00780000000000000000

	// First calculate JD: http://www.tondering.dk/claus/cal/node3.html#sec-calcjd
	int a = (14 - (m_month+1)) / 12;
	int y = m_year+4800-a;
	int m = (m_month + 1)+12*a-3;

	// Julian day, gregorian calendar
	long JD = m_day + (153*m+2)/5 + 365*y + y / 4 - y / 100 + y / 400 - 32045;

	long d4 = (((JD + 31741 - (JD%7)) % 146097) % 36524) % 1461;
	int L = d4/1460;
	int d1 = (d4-L) % 365 + L;

	int week_number = d1/7+1;

	// To this we add the year and make a WeekSpec

	unsigned short year = m_year;
	if (m_month == 0 && week_number > 50)
	{
		// wrong year. This is week 52/53 of last year
		year--;
	}
	else if (m_month == 11 && week_number == 1)
	{
		// wrong year. This is week 1 of next year
		year++;
	}
	OP_ASSERT(week_number >= 1 && week_number <= 53);
	WeekSpec week = {year, static_cast<unsigned char>(week_number)};
	return week;
}


DaySpec DaySpec::NextDay() const
{
	DaySpec ret = *this;
	MonthSpec m = { m_year, m_month };
	if (ret.m_day == m.DaysInMonth())
	{
		ret.m_day = 1;
		if (ret.m_month == 11)
		{
			ret.m_month = 0;
			ret.m_year++;
		}
		else
			ret.m_month++;
	}
	else
	{
		ret.m_day += 1;
	}
	return ret;
}

DaySpec DaySpec::PrevDay() const
{
	DaySpec ret = *this;
	if (ret.m_day == 1)
	{
		MonthSpec m = { m_year, m_month };
		m = m.PrevMonth();
		ret.m_year = m.m_year;
		ret.m_month = m.m_month;
		ret.m_day = m.DaysInMonth();
	}
	else
	{
		ret.m_day -= 1;
	}
	return ret;
}

int TimeSpec::GetFraction() const
{
	// The world isn't ready for full fractions yet. Statically sized buffers for instance
	if (IsSecondUndefined() || m_fraction_digit_count < 1)
	{
		return 0;
	}
	int fraction_digits = m_fraction_digit_count;
	int fraction = m_fraction;
	while (fraction_digits < 2)
	{
		fraction *= 10;
		fraction_digits++;
	}
	while (fraction_digits > 2)
	{
		fraction /= 10;
		fraction_digits--;
	}
	return fraction;
}

int TimeSpec::GetFractionDigitCount() const
{
	if (IsSecondUndefined() || m_fraction_digit_count < 1)
	{
		return 0;
	}
	// The world isn't ready for full fractions yet. Statically sized buffers for instance
	return 2;
}

void TimeSpec::ToISO8601String(uni_char* buf) const
{
	OP_ASSERT(m_hour <= 23);
	buf[0] = m_hour/10 + '0';
	buf[1] = m_hour%10 + '0';
	buf[2] = ':';
	OP_ASSERT(m_minute >= 0 && m_minute <= 59);
	buf[3] = m_minute/10 + '0';
	buf[4] = m_minute%10 + '0';
	OP_ASSERT(m_second == -1 || (m_second >= 0 && m_second <= 59));
	if (m_second == -1)
	{
		// XXX The real format? Read the spec!
		buf[5] = '\0';
	}
	else
	{
		buf[5] = ':';
		buf[6] = m_second/10 + '0';
		buf[7] = m_second%10 + '0';
		int fraction_digits = GetFractionDigitCount();
		if (fraction_digits == 0)
		{
			buf[8] = '\0';
		}
		else
		{
			int factor = 1;
			for (int i = fraction_digits; i > 1; i--)
			{
				factor *= 10;
			}
			int buf_pos = 8;
			buf[buf_pos++] = '.';
			int fraction = GetFraction();
			while (factor)
			{
				buf[buf_pos++] = '0' + fraction/factor;
				fraction = fraction % factor;
				factor /= 10;
			}
			OP_ASSERT(buf_pos == (int)GetISO8601StringLength());
			buf[buf_pos++] = '\0';
		}
	}
}

unsigned int TimeSpec::GetISO8601StringLength() const
{
	 // Not including null byte
	if (m_second == -1)
	{
		return 5; // "HH:MM"
	}

	int fraction_digit_count = GetFractionDigitCount();
	if (fraction_digit_count == 0)
	{
		return 8; // "HH:MM:SS"
	}

	return 9 + fraction_digit_count; // "HH:MM:SS.xxxxx"
}

unsigned int TimeSpec::SetFromISO8601String(const uni_char* time_string, BOOL partial /* = FALSE */)
{
	// A time (hour, minute) encoded according to [ISO8601] with no
	// time zone, e.g.: 23:59. User agents are expected to show an
	// appropriate widget, such as a clock. UAs should make it clear
	// to the user that the time does not carry any time zone information.

	if (!(uni_isdigit(time_string[0]) && uni_isdigit(time_string[1]) &&
		time_string[2] == ':' && uni_isdigit(time_string[3]) && uni_isdigit(time_string[4])))
	{
		// Doesn't even start correctly
		return 0;
	}

	m_hour = 10 * digit(time_string[0]) + digit(time_string[1]);
	m_minute = 10 * (digit(time_string[3])) + digit(time_string[4]);

	if (m_hour > 23 || m_minute > 59)
	{
		return 0;
	}

	OP_ASSERT(m_hour <= 23);
	OP_ASSERT(m_minute <= 59);

	m_second = -1;
	m_fraction_digit_count = 0;
	m_fraction = 0;
	int len = 5; // Length of valid string.
	if (time_string[5] != '\0')
	{
		if (!(time_string[5] == ':' && uni_isdigit(time_string[6]) && uni_isdigit(time_string[7])))
		{
			// Had something after the minute that wasn't a second.
			return partial ? len : 0;
		}

		m_second = 10 * digit(time_string[6]) + digit(time_string[7]);

		if (m_second > 59)
		{
			return 0;
		}

		OP_ASSERT(m_second >= 0 && m_second <= 59);

		len = 8;
		if (time_string[8] != '\0')
		{
			if (!(time_string[8] == '.'))
			{
				// The seperator must be a period.
				return partial ? len : 0;
			}

			// Consume n digits, but only use 9 (~30 bits) for fraction.
			for (len = 9; uni_isdigit(time_string[len]); len++)
			{
				if (m_fraction_digit_count < 9)
				{
					m_fraction_digit_count++;
					m_fraction = m_fraction * 10 + digit(time_string[len]);
				}
			}

			OP_ASSERT(m_fraction_digit_count < 10);

			if (m_fraction_digit_count == 0)
			{
				// Must have at least one digit
				return partial ? 8 : 0;
			}

			OP_ASSERT(m_fraction_digit_count > 0);

			if (!partial && time_string[len] != '\0')
			{
				// Must be null-terminated if so requested
				return 0;
			}
		}
	}

	return len;
}

/**
 * Simple helper method to do powers of ten. op_pow(10, x)
 * seems to be broken with some compilers (MSVC7.1) in
 * optimizing mode. It returns ERROR for op_pow(10,-3) for instance.
 */
static double TenPower(int power)
{
	OP_ASSERT(power < 10 && power > -10); // made for small powers
	double val = 1.0;
	while (power > 0)
	{
		val *= 10;
		power--;
	}
	while (power < 0)
	{
		val /= 10;
		power++;
	}
	return val;
}

void TimeSpec::SetFromTime(double ms)
{
	m_hour = OpDate::HourFromTime(ms);
	m_minute = OpDate::MinFromTime(ms);
	m_second = OpDate::SecFromTime(ms);
	SetFractionUndefined();
}

double TimeSpec::AsDouble() const
{
	double val = 0.0;
	if (m_second != -1)
	{
		if (m_fraction_digit_count > 0)
			val += m_fraction/TenPower(m_fraction_digit_count);
		val += m_second;
	}
	val += 60.0 * (m_minute + 60.0 * m_hour);
	return val;
}

void TimeSpec::SetFromNumber(int value, int multiplied_factor)
{
	OP_ASSERT(value >= 0);
	OP_ASSERT(value <= 86400*multiplied_factor-1);
	m_hour = value / (3600 * multiplied_factor);
	value -= m_hour * 3600  * multiplied_factor;
	m_minute = value / (60 * multiplied_factor);
	value -= m_minute * 60 * multiplied_factor;
	if (m_second != -1 || value > 0)
	{
		m_second = value / multiplied_factor;
		value -= m_second * multiplied_factor;
		value = (value * 1000) / multiplied_factor; // Now in ms
		if (value > 0 || GetFractionDigitCount() > 0)
		{
			if (value == 0)
			{
				SetFraction(0, 0);
			}
			else if ((value/100) * 100 == value)
			{
				SetFraction(value/100, 1);
			}
			else if ((value/10) * 10 == value)
			{
				SetFraction(value/10, 2);
			}
			else
			{
				SetFraction(value, 3);
			}
		}
	}
	else
	{
		SetFractionUndefined();
	}
}

void TimeSpec::SetFraction(int fraction, int fraction_digit_count)
{
	OP_ASSERT(fraction_digit_count <= 9);
	m_fraction = fraction;
	m_fraction_digit_count = fraction_digit_count;
}


unsigned int TimeZoneSpec::SetFromISO8601String(const uni_char* tz_string, BOOL partial /* = FALSE */)
{
	// A time zone (hour, minute) encoded according to [ISO8601],
	// e.g.: 'Z', '+02:00' or '-07:45'. Time zones which don't exist
	// are OK as long as they are in the range -23:59 to +23:59.

	unsigned int len = 0;
	if (tz_string[0] == 'Z')
	{
		m_sign = '+';
		m_hour = 0;
		m_minute = 0;
		len = 1;
	}
	else if ((tz_string[0] == '+' || tz_string[0] == '-') &&
			 uni_isdigit(tz_string[1]) && uni_isdigit(tz_string[2]) &&
			 tz_string[3] == ':' &&
			 uni_isdigit(tz_string[4]) && uni_isdigit(tz_string[5]))
	{
		m_sign = static_cast<char>(tz_string[0]);
		m_hour = 10 * digit(tz_string[1]) + digit(tz_string[2]);
		m_minute = 10 * (digit(tz_string[4])) + digit(tz_string[5]);

		if (m_hour > 23 || m_minute > 59)
		{
			return 0;
		}

		len = 6;
	}
	else
	{
		// Neither Z nor +/-HH:MM format
		return 0;
	}

	OP_ASSERT(m_sign == '+' || m_sign == '-');
	OP_ASSERT(m_hour <= 23);
	OP_ASSERT(m_minute <= 59);

	if (!partial && tz_string[len] != '\0')
	{
		return 0;
	}

	return len;
}

double TimeZoneSpec::AsDouble() const
{
	OP_ASSERT(m_sign == '+' || m_sign == '-');
	OP_ASSERT(m_hour <= 23);
	OP_ASSERT(m_minute <= 59);
	double val = 60.0 * (m_minute + 60.0 * m_hour);
	return (m_sign == '+') ? val : -val;
}


void DateTimeSpec::ToISO8601String(uni_char* buf, BOOL include_timezone) const
{
	if (include_timezone && !m_timezone.IsZ())
	{
		DateTimeSpec norm = *this;
		norm.ConvertToUTC();
		norm.ToISO8601String(buf, include_timezone);
	}
	else
	{
		m_date.ToISO8601String(buf);
		buf += m_date.GetISO8601StringLength();
		*buf = 'T';
		buf++;
		m_time.ToISO8601String(buf);
		buf += m_time.GetISO8601StringLength();
		if (include_timezone)
		{
			*buf = 'Z'; // Timezone
			buf++;
		}
		*buf = '\0';
	}
}

unsigned int DateTimeSpec::GetISO8601StringLength(BOOL include_timezone) const
{
	// NOTE: the code below may miscalulate if we allow for years with > 4 digits
	return m_date.GetISO8601StringLength()+ // 10
		1 + // The 'T' char
		m_time.GetISO8601StringLength() + // 5 - ~ 12
		!!include_timezone; // The 'Z' char (and not including the null byte)
}

unsigned int DateTimeSpec::SetFromISO8601String(const uni_char* date_time_string, BOOL include_timezone, BOOL partial /* = FALSE */)
{
	if (!date_time_string)
	{
		return 0;
	}

	unsigned int len = 0;

	if (!m_date.SetFromISO8601String(date_time_string, TRUE))
	{
		return 0;
	}
	len = m_date.GetISO8601StringLength();

	if (date_time_string[len++] != 'T')
	{
		return 0;
	}

	unsigned int time_len = m_time.SetFromISO8601String(date_time_string+len, TRUE);
	if (time_len == 0)
	{
		return 0;
	}
	len += time_len;

	if (include_timezone)
	{
		//  A fully fledged time zone offset spec is allowed/required here
		// (see http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#date-and-time-state)
		//
		unsigned int timezone_length = m_timezone.SetFromISO8601String(date_time_string+len, TRUE);
		if (timezone_length == 0)
		{
			return 0;
		}
		len += timezone_length;
	}
	else
	{
		m_timezone.Clear();
	}

	if (!partial && date_time_string[len] != '\0')
	{
		return 0;
	}

	return len;
}

void DateTimeSpec::SetFromTime(double ms)
{
	m_date.SetFromTime(ms);
	m_time.SetFromTime(ms);
	m_timezone.Clear();
}

double DateTimeSpec::AsDouble()
{
	double ms = m_time.m_fraction/TenPower(m_time.m_fraction_digit_count-3);
	OP_ASSERT(TenPower(m_time.m_fraction_digit_count-3) != 0);
	OP_ASSERT(!op_isnan(ms) && !op_isinf(ms));

	double val = OpDate::MakeDate(OpDate::MakeDay(m_date.m_year, m_date.m_month, m_date.m_day),
		OpDate::MakeTime(m_time.m_hour, m_time.m_minute, m_time.GetSecond(), ms))
		- m_timezone.AsDouble()*1000;
	OP_ASSERT(!op_isnan(val) && !op_isinf(val));
	return val;
}

unsigned char MonthSpec::DaysInMonth() const
{
	static const unsigned char MONTH_DAYS[] =
		{31,28,31,30,31,30,31,31,30,31,30,31};

	unsigned char days = MONTH_DAYS[m_month];
	if (m_month == 1) // february
	{
		if (IsLeapYear(m_year))
		{
			days++;
		}
	}
	return days;
}

void  DateTimeSpec::ConvertToUTC()
{
	OP_ASSERT(m_time.m_minute >= 0 && m_time.m_minute < 60);
	OP_ASSERT(m_time.m_hour < 24);

	OP_ASSERT(m_timezone.m_minute < 60);
	OP_ASSERT(m_timezone.m_hour < 24);

	int sign   = (m_timezone.m_sign == '+' ? 1 : -1);
	int minute = m_time.m_minute - sign * m_timezone.m_minute;
	int hour   = m_time.m_hour   - sign * m_timezone.m_hour;

	if (minute < 0)
	{
		minute += 60;
		--hour;
	}
	else if (minute >= 60)
	{
		minute -= 60;
		++hour;
	}

	if (hour < 0)
	{
		hour += 24;
		m_date = m_date.PrevDay();
	}
	else if (hour >= 24)
	{
		hour -= 24;
		m_date = m_date.NextDay();
	}

	OP_ASSERT(minute >= 0 && minute < 60);
	OP_ASSERT(hour >= 0 && hour < 24);
	m_time.m_minute = minute;
	m_time.m_hour   = hour;
	m_timezone.m_minute = 0;
	m_timezone.m_hour   = 0;

	OP_ASSERT(m_timezone.IsZ());
}

