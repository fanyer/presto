/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/stdlib/util/opdate.h"

/* Note that the definition here is the ECMAScript specification: ES-262, 3rd edition. */

static const int year_days[13] =
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

static const int leap_year_days[13] =
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 };

#if defined HAS_COMPLEX_GLOBALS
// Use a proper table when possible
#  define ST_HEADER(name) static const char * const name[] = {
#  define ST_HEADER_UNI(name) static const uni_char * const name[] = {
#  define ST_ENTRY(name,value) value,
#  define ST_FOOTER 0 };
#  define ST_FOOTER_UNI 0 };
#  define ST_REF(name,x) name[x]
#else
// Otherwise use a function that switches on the index
#  define ST_HEADER(name) static const char * name(int x) { switch (x) {
#  define ST_HEADER_UNI(name) static const uni_char * name(int x) { switch (x) {
#  define ST_ENTRY(name,value) case name : return value;
#  define ST_FOOTER default: OP_ASSERT(0); return "***"; }; }
#  define ST_FOOTER_UNI default: OP_ASSERT(0); return UNI_L("***"); }; }
#  define ST_REF(name,x) name(x)
#endif

ST_HEADER_UNI(month_list)
	ST_ENTRY(0,UNI_L("January"))
	ST_ENTRY(1,UNI_L("February"))
	ST_ENTRY(2,UNI_L("March"))
	ST_ENTRY(3,UNI_L("April"))
	ST_ENTRY(4,UNI_L("May"))
	ST_ENTRY(5,UNI_L("June"))
	ST_ENTRY(6,UNI_L("July"))
	ST_ENTRY(7,UNI_L("August"))
	ST_ENTRY(8,UNI_L("September"))
	ST_ENTRY(9,UNI_L("October"))
	ST_ENTRY(10,UNI_L("November"))
	ST_ENTRY(11,UNI_L("December"))
ST_FOOTER_UNI

#define MONTHS(x) ST_REF(month_list,x)

ST_HEADER_UNI(day_list)
	ST_ENTRY(0,UNI_L("Sunday"))
	ST_ENTRY(1,UNI_L("Monday"))
	ST_ENTRY(2,UNI_L("Tuesday"))
	ST_ENTRY(3,UNI_L("Wednesday"))
	ST_ENTRY(4,UNI_L("Thursday"))
	ST_ENTRY(5,UNI_L("Friday"))
	ST_ENTRY(6,UNI_L("Saturday"))
ST_FOOTER_UNI

#define DAYS(x) ST_REF(day_list,x)

//
// The following is a static table of timezone names that are supported
// in IE, and a table that maps these timezones to an hour offset.
//
// Important: The two tabels below are linked, and needs to be kept
//            synchronized for proper operation.
//
static const char timezone_names[31] =
{
	'G', 'M', 'T',	// 0
	'U', 'T', 'C',	// 1
	'C', 'D', 'T',	// 2
	'C', 'S', 'T',	// 3
	'E', 'D', 'T',	// 4
	'E', 'S', 'T',	// 5
	'M', 'D', 'T',	// 6
	'M', 'S', 'T',	// 7
	'P', 'D', 'T',	// 8
	'P', 'S', 'T',	// 9
	0
};

static const signed short timezone_offsets[] =
{
	0,			// 0  GMT
	0,			// 1  UTC
	-5 * 60,	// 2  CDT
	-6 * 60,	// 3  CST
	-4 * 60,	// 4  EDT
	-5 * 60,	// 5  EST
	-6 * 60,	// 6  MDT
	-7 * 60,	// 7  MST
	-7 * 60,	// 8  PDT
	-8 * 60,	// 9  PST
};

//
// A table with the mapping from character to timezone offset
// defined by Nathaniel Bowditch. The 'J' is not used.

static const signed short naval_offsets[26] =
{
	-1 * 60,  // A (GMT-1)
	-2 * 60,  // B (GMT-2)
	-3 * 60,  // C (GMT-3)
	-4 * 60,  // D (GMT-4)
	-5 * 60,  // E (GMT-5)
	-6 * 60,  // F (GMT-6)
	-7 * 60,  // G (GMT-7)
	-8 * 60,  // H (GMT-8)
	-9 * 60,  // I (GMT-9)
	0,        // J
	-10 * 60, // K (GMT-10)
	-11 * 60, // L (GMT-11)
	-12 * 60, // M (GMT-12)
	1 * 60,   // N (GMT+1)
	2 * 60,   // O (GMT+2)
	3 * 60,   // P (GMT+3)
	4 * 60,   // Q (GMT+4)
	5 * 60,   // R (GMT+5)
	6 * 60,   // S (GMT+6)
	7 * 60,   // T (GMT+7)
	8 * 60,   // U (GMT+8)
	9 * 60,   // V (GMT+9)
	10 * 60,  // W (GMT+10)
	11 * 60,  // X (GMT+11)
	12 * 60,  // Y (GMT+12)
	0 * 60    // Z (GMT)
};

/* static */ int
OpDate::ParseNumber(const uni_char* &str, int &value)
{
	int digs = 0;
	uni_char* tmp;

	if ( uni_isdigit(*str) )
	{
		int v = static_cast<int>(uni_strtol(str, &tmp, 10, NULL));
		digs = static_cast<int>(tmp - str);

		if ( digs > 0 && digs < 10 )
		{
			str = tmp;
			value = v;
		}
		else
			digs = 0;
	}

	return digs;
}

/* static */ int
OpDate::ParseMilliseconds(const uni_char* &str, int &value)
{
	uni_char digits[4]; // ARRAY OK 2012-06-06 sof
	digits[0] = uni_isdigit(str[0]) ? str[0] : '0';
	digits[1] = str[0] && uni_isdigit(str[1]) ? str[1] : '0';
	digits[2] = str[0] && str[1] && uni_isdigit(str[2]) ? str[2] : '0';
	digits[3] = 0;

	while (uni_isdigit(*str))
		str++;

	const uni_char *ns = digits;
	return ParseNumber(ns, value);
}

/* static */ int
OpDate::ParseTimeOfDay(const uni_char* &date_str,
					   int &tm_hour, int &tm_min, int &tm_sec,
					   BOOL strict, int max_elems, BOOL &valid)
{
	/* "hh:[mm[:[ss]]]" */

	const uni_char* ptr = date_str;
	int value[3];
	int elems = 0;
	BOOL tmp_valid = TRUE;

	value[0] = 0;
	value[1] = 0;
	value[2] = 0;

	OP_ASSERT(max_elems <= 3);
	OP_ASSERT(max_elems >= 2);

	for (;;)
	{
		int digs = ParseNumber(ptr, value[elems]);

		//
		// Allow a time with a trailing colon, so e.g. "10:" is a valid
		// time of "10:00:00", but we don't allow this in strict mode.
		// This behaviour is for compatibility with IE parsing behaviour.
		//
		if ( digs == 0 && !strict )
			break;

		// Only accept two digits for hh, mm and ss when in strict mode
		if ( digs != 2 && strict )
			tmp_valid = FALSE;

		// We have a new element to the value array now
		elems++;

		// Break the loop if we have enough
		if ( elems == max_elems )
			break;

		// There is room for more, terminate loop if there isn't a colon
		if ( *ptr != ':' )
		{
			if ( elems == 1 )
				return 0; // For the first element, there must be a colon

			break; // Didn't reach max_elems due to lack of colons
		}

		ptr++;
	}

	if ( elems )
	{
		// Only allow 24:00:00 as a special case for 24:xx:xx
		if ( value[0] == 24 && (value[1] != 0 || value[2] != 0) )
			tmp_valid = FALSE;

		// Check remaining possible range errors
		if ( value[0] > 24 || value[1] > 59 || value[2] > 59 )
			tmp_valid = FALSE;

		date_str = ptr;
		tm_hour = value[0];
		tm_min = value[1];
		tm_sec = value[2];
		valid = tmp_valid;
	}

	return elems;
}

/* static */ BOOL
OpDate::ParseTimeAMPM(const uni_char *&date_str, BOOL &is_pm)
{
	/* "AM", "PM" */
	const uni_char *tmp = date_str;

	uni_char c = uni_tolower(*tmp++);
	if ( c != 0 )
	{
		if ( *tmp == '.' )
			tmp++;

		if ( uni_tolower(*tmp++) == 'm' )
		{
			if ( !uni_isalpha(*tmp) )
			{
				BOOL tmp_is_pm = FALSE;

				switch ( c )
				{
				case 'p':
					tmp_is_pm = TRUE;
					// Fall through

				case 'a':
					is_pm = tmp_is_pm;
					if ( *tmp == '.' )
						tmp++;
					date_str = tmp;
					return TRUE;

				default:
					// Illegal AM/PM
					break;
				}
			}
		}
	}

	return FALSE;
}

/* static */ BOOL
OpDate::ParseTimezone(const uni_char* &date_str, int &tz_min, BOOL &valid)
{
	//
	// "UTC", "GMT", "UT", "Z" or "[XYZ][+-][h]h[:]mm" or just [+-][h]h[:]mm
	// or one of the timezones in the top of this file, or the naval
	// single letter timezones.
	//
	const uni_char *tmp = date_str;
	BOOL tmp_valid = TRUE;

	// Don't allow the timezone name to be a month-name!
	int dummy;
	if ( ParseMonth(tmp, dummy) )
		return FALSE;

	int offset_sign = 1;

	char tz[5]; // ARRAY OK 2009-01-18 mortenro
	int k = 0;

	// Copy up to 4 characters from the TZ name, and zero-terminate
	while ( k < 4 && uni_isalpha(*tmp) )
		tz[k++] = (char)op_toupper(*tmp++);
	tz[k] = 0;

	// Skip any additional characters from the TZ name
	while ( uni_isalpha(*tmp) )
		tmp++;

	// Check if there is [+-]hh[:]mm appended to the TZ name
	switch ( *tmp )
	{
	case '-':
		offset_sign = -1;
		// Fall through
	case '+':
	{
		tmp++;

		int offset1;
		int offset2 = 0;
		int digs = ParseNumber(tmp, offset1);
		if ( digs == 0 || digs > 7 )
			return FALSE;

		if ( *tmp == ':' )
		{
			//
			// Timezone with colon specification. This format is really
			// broken in FF. Opera is trying to be compatible with FF for
			// this format, but we refuse to be bug-compatible.
			// For Opera we have decided to support [+-][h]h:mm timezones
			// like so:
			//
			// +0:mm means +mm minutes, mm < 60. In FF this is incorrectly
			// negated into a negative time offset.  Few, if any timezones
			// have an offset to GMT of less than one hour, so this should
			// not cause any practical problems.
			//
			// [+-]h:mm means +/- hh hours and mm minutes, h < 24 and
			// mm < 60. This is compatible with FF.
			//
			// When h >= 24, Opera will fail the format, since this format
			// has strange behaviour and is probably not intended in FF:
			//
			// +99:01 gives +1:40 (99 mins + 1 min)
			// 120:40 gives +2:00 (1:20 + 40 mins)
			//

			//
			// Having only the colon is allowed, which gives normal
			// behaviour (as if the colon wasn't there). This is to be
			// compatible with both FF and IE, even though this format
			// in un-intuitive for values larger than 23.
			//
			tmp++;
			digs = ParseNumber(tmp, offset2);

			// Return invalid TZ if the colon format is illegal
			if ( digs > 7 || offset2 >= 60 || (digs > 0 && offset1 >= 24 ) )
				tmp_valid = FALSE;
		}

		if ( offset1 < 24 )
		{
			// Short form: Interpret e.g. GMT+0[:mm] to GMT+23[:mm] as
			// hour timezones with optional minutes.
			tz_min = offset_sign * (offset1 * 60 + offset2);
		}
		else
		{
			// Long form: hhmm
			// Deliberately allow e.g. 0199 since IE/FF accepts this
			int mins = offset1 % 100;
			int hour = offset1 / 100;
			// FF supports 'hhmm[:mm]' which can be computed with:
			// 'tz_min = offset_sign * (hour * 60 + mins + offset2);'
			// But we don't support this at this time (see above).
			tz_min = offset_sign * (hour * 60 + mins);
		}
		break;
	}

	default:
	{
		//
		// There is no [+-]hhmm specified, so we can only accept UTC,
		// GMT, UT or Z for UTC timezone, A-Z naval timezones, or one
		// of the timezones listed in the top of this file.
		//
		// The inclusion of a number of fixed timezones is done to mimic
		// the behaviour of IE. This inclusion goes counter to the
		// following, original argument, why Opera should not support
		// timezone names other than for UTC:
		//
		// If you get tempted to handle other 'well known' timezones:
		// Consider that EST means at least three things:
		//
		//    Eastern Standard Time (USA)          UTC-05
		//    Eastern Standard Time (Australia)    UTC+10
		//    Eastern Brazil Standard Time         UTC-03
		//
		// So if you want to do some interpretation, it needs to be
		// made in the context of the country of origin of the page
		// you're looking at, unless you're looking at a page that is
		// 'clever' and uses EST if it observes that it is being
		// viewed in the US.  For example.
		//
		// http://www.worldtimezone.com/wtz-names/timezonenames.html
		//

		if ( tz[0] == 0 )
			return FALSE;

		if ( tz[1] == 0 )
		{
			// A single letter naval timezone specification
			int tznl = tz[0];
			if ( tznl >= 'A' && tznl <= 'Z' && tznl != 'J' )
			{
				tz_min = naval_offsets[tznl - 'A'];
				break;
			}

			return FALSE;
		}

		if ( tz[2] == 0 )
		{
			//
			// A two letter timezone specification. Accept UT as an alias
			// for UTC, and append a 'T' for all other two letter
			// timezones (so e.g. ES becomes EST)
			//
			if ( !op_strcmp(tz, "UT") )
				tz[2] = 'C';
			else if ( tz[2] == 0 )
				tz[2] = 'T';
		}
		else if ( tz[3] != 0 )
		{
			// The timezone is longer than three characters, which is
			// the longest timezone names we recognize
			return FALSE;
		}

		int tz_count = 0;
		const char* tzn = timezone_names;

		while ( *tzn != 0 )
		{
			if ( tzn[0] == tz[0] && tzn[1] == tz[1] && tzn[2] == tz[2] )
			{
				// We have a match
				tz_min = timezone_offsets[tz_count];
				goto found;
			}

			tz_count++;
			tzn += 3;
		}

		return FALSE;

	found:
		break;
	}

	}

	date_str = tmp;
	valid = tmp_valid;
	return TRUE;
}

/* static */ BOOL
OpDate::ParseDayOfWeek(const uni_char* &date_str)
{
	/* "Mon[day]", "Tue[sday]", ... */
	const uni_char *tmp = date_str;
	unsigned int len = 0;

	while ( uni_isalpha(tmp[len]) )
		len++;

	if ( len >= 2 )
	{
		for ( int wday = 0; wday < 7; wday++ )
			if ( uni_strlen(DAYS(wday)) >= len &&
				 uni_strnicmp(tmp, DAYS(wday), len) == 0 )
			{
				date_str = tmp + len;
				return TRUE;
			}
    }

	return FALSE;
}

/* static */ BOOL
OpDate::ParseMonth(const uni_char* &date_str, int &month)
{
	/* "Jan[uary]", "Feb[ruary]", ... */
	unsigned int len = 0;

	while ( uni_isalpha(date_str[len]) )
		len++;

	if ( len >= 3 )
	{
		for ( int idx = 0; idx < 12; idx++ )
		{
			if ( uni_strlen(MONTHS(idx)) >= len &&
				 uni_strnicmp(date_str, MONTHS(idx), len) == 0 )
			{
				date_str += len;
				month = idx + 1;
				return TRUE;
			}
		}
	}

	return FALSE;
}

/* static */ BOOL
OpDate::ParseComment(const uni_char* &date_str)
{
	const uni_char* tmp = date_str;
	uni_char c;

	if ( *tmp++ == '(' )
	{
		int level = 1;

		while ( (c = *tmp) != 0 && level )
		{
			if ( c == '(' )
				level++;
			else if ( c == ')' )
				level--;
			tmp++;
		}

		date_str = tmp;
		return TRUE;
	}

	return FALSE;
}

/* static */ BOOL
OpDate::ParseRFC3339Date(const uni_char* &date_str,
						 int &year, int &mon, int &day,
						 int &hour, int &min, double &sec,
						 int &tz_min)
{
	const uni_char* tmp = date_str;

	int tm_year;
	int tm_month;
	int tm_day;
	int tm_hour;
	int tm_min;
	double tm_sec;
	int tzmin;
	BOOL valid;

	// Parse 'yyyy-mm-ddThh:mm:ss' first, and expect strict conformance
	if ( (ParseDateISO(tmp, tm_year, tm_month, tm_day, valid) != 3) ||
		 (!valid) ||
		 (ParseTimeISO(tmp, tm_hour, tm_min, tm_sec, TRUE, valid) < 3) ||
		 (!valid) ||
		 (tm_hour > 23) ||
		 (!ParseTimezoneISO(tmp, tzmin)) )
		return FALSE;

	date_str = tmp;
	year = tm_year;
	mon = tm_month;
	day = tm_day;
	hour = tm_hour;
	min = tm_min;
	sec = tm_sec;
	tz_min = tzmin;

	return TRUE;
}

int DayOfMonth(int year, int month)
{
	const int* month_table = year_days;

	OP_ASSERT(month >= 0);
	OP_ASSERT(month <= 12);

	switch ( year % 400 )
	{
	case 100: case 200:	case 300:
		break;

	default:
		if ( (year & 0x3) == 0 )
			month_table = leap_year_days;
		break;
	}

	return month_table[month];
}

int Month2Day(int year, int month)
{
	int leap_year_comp;
	if ( year >= 1970 )
		leap_year_comp = ((year - 1969) / 4)
			- ((year - 1901) / 100)
			+ ((year - 1601) / 400);
	else
	    leap_year_comp = -((1972 - year) / 4)
			+ ((2000 - year) / 100)
			- ((2000 - year) / 400);

	return ((year - 1970) * 365) + DayOfMonth(year, month) + leap_year_comp;
}

double Date2MS(int year, int month, int day)
{
	int m;

	if ( month >= 0 )
	{
		m = month % 12;
		year += (month / 12);
	}
	else
	{
		// This is needed since esp. modulo is undefined on negative numbers
		year -= ((-month+11) / 12);
		m = 11 - ((-month-1) % 12);
	}

	return (Month2Day(year, m) + day - 1) * msPerDay;
}

double Time2MS(int hour, int min, double sec)
{
	return hour * msPerHour
		+ min * msPerMinute
		+ sec * msPerSecond;
}

/* static */ double
OpDate::ParseRFC3339Date(const uni_char* date_str)
{
	int year = 0;
	int mon = 0;
	int day = 0;
	int hour = 0;
	int min = 0;
	double sec = 0.0;
	int tz_min = 0;

	if ( ParseRFC3339Date(date_str, year, mon, day, hour, min, sec, tz_min) )
	{
		while ( uni_isspace(*date_str) )
			date_str++;

		if ( *date_str == 0 )
			return Date2MS(year, mon, day) + Time2MS(hour, min, sec)
				- tz_min * msPerMinute;
	}

	return op_nan(NULL);  // illegal RFC 3339 date
}

/* static */ int
OpDate::ParseDateISO(const uni_char* &date_str,
					 int &tm_year, int &tm_mon, int &tm_day, BOOL &valid)
{
	const uni_char* tmp = date_str;
	int rc;
	int year;
	int month = 1;
	int day = 1;

	if ( ParseNumber(tmp, year) != 4 )
		return 0;

	rc = 1;
	if ( *tmp == '-' )
	{
		tmp++;
		if ( ParseNumber(tmp, month) != 2 )
			return 0;

		rc = 2;
		if ( *tmp == '-' )
		{
			tmp++;
			if ( ParseNumber(tmp, day) != 2 )
				return 0;
			rc = 3;
		}
	}

	month--;

	// For ISO format, don't allow overflowing dates
	valid = FALSE;
	if ( month >= 0 && month <= 11 )
	{
		int days = DayOfMonth(year, month + 1) - DayOfMonth(year, month);
		if ( day >= 1 && day <= days )
			valid = TRUE;
	}

	date_str = tmp;
	tm_year = year;
	tm_mon = month;
	tm_day = day;
	return rc;
}

/* static */ int
OpDate::ParseTimeISO(const uni_char* &date_str,
					 int &tm_hour, int &tm_min, double &tm_sec, BOOL strict_fractions, BOOL &valid)
{
	const uni_char* tmp = date_str;
	double fractsec = 0.0;
	int sec;
	BOOL tmp_valid;

	if ( *tmp++ != 'T' )
		return 0;

	int elems = ParseTimeOfDay(tmp, tm_hour, tm_min, sec, TRUE, 3, tmp_valid);

	if ( elems == 3 && *tmp == '.' )
	{
		int ms;
		tmp++;
		if ( strict_fractions )
		{
			if ( ParseNumber(tmp, ms) != 3 )
				return 0;
		}
		else if ( ParseMilliseconds(tmp, ms) == 0 )
			return 0;

		fractsec = (double)ms / 1000.0;
		if ( tm_hour == 24 && ms != 0 )
			tmp_valid = FALSE;
		elems++;
	}

	if ( elems )
	{
		date_str = tmp;
		tm_sec = (double)sec + fractsec;
		valid = tmp_valid;
	}

	return elems;
}

/* static */ BOOL
OpDate::ParseTimezoneISO(const uni_char* &date_str, int &tz_min)
{
	const uni_char* tmp = date_str;

	int tzsign = 1;
	int tzhour = 0;
	int tzmin = 0;
	int dummy;
	BOOL valid;

	// Parse the time zone
	switch ( *tmp++ )
	{
	case 'Z':
		break;
	case '-':
		tzsign = -1;
		// Fall through
	case '+':
		if ( ParseTimeOfDay(tmp, tzhour, tzmin, dummy, TRUE, 2, valid) == 2 &&
			 valid )
			break;
		// Fall through and fail when 'hh:mm' format not found
	default:
		return FALSE;
	}

	date_str = tmp;
	tz_min = tzsign * (tzhour * 60 + tzmin);

	return TRUE;
}

/* static */ BOOL
OpDate::SkipSeps(const uni_char* &date_str)
{
	const uni_char* tmp = date_str;
	BOOL separator_detected = TRUE;

	// Use this to determine what can be a separator
	uni_char prev = date_str[-1];

	if ( *tmp == '(' )
		goto done;

	if ( uni_isdigit(prev) )
	{
		if ( uni_isalpha(*tmp) )
			goto done;
	}
	else
	{
		if ( uni_isdigit(*tmp) )
			goto done;
	}

	separator_detected = (prev ==  ')');

	while ( *tmp )
	{
		uni_char c = *tmp;
		if ( c != '/' && c != ',' && !uni_isspace(c) )
			break;

		tmp++;
		separator_detected = TRUE;
	}

	if ( separator_detected )
		date_str = tmp;

 done:
	return separator_detected;
}

// Flags to keep track of what has been seen so far
#define PARSEDATE_DATE      1
#define PARSEDATE_TIME      2
#define PARSEDATE_AM        4
#define PARSEDATE_PM        8
#define PARSEDATE_TZ        16

#define PARSEDATE_AMPM	   (PARSEDATE_AM + PARSEDATE_PM)
#define PARSEDATE_DATETIME (PARSEDATE_DATE + PARSEDATE_TIME)

/* static */ double
OpDate::ParseDate(const uni_char* date_str, BOOL strict_fractions /* = TRUE */)
{
	double nan = op_nan(NULL);

	// Resulting date, month is base 0
	int tm_year = 0;
	int tm_mon = 0;
	int tm_day = 0;

	// Resulting time
	int tm_hour = 0;
	int tm_min = 0;
	double tm_sec = 0.0;
	int tz_min = 0;

	// State information
	UINT32 flags = 0;
	BOOL separator_detected = TRUE;

	// Temporary date while parsing it
	int date_elem[3];
	int date_elem_count = 0;
	int date_elem_textmonth = 0;

	// Misc temporary variables
	BOOL valid;
	const uni_char* tmp;
	int value;

	//
	// Try to parse according to ES3.1 - 15.9.1.15 "Date Time string format"
	// first before invoking the regular date parser loop, which is much
	// more forgiving in what it accepts as valid dates.
	//
	tmp = date_str;
	if ( ParseDateISO(tmp, tm_year, tm_mon, tm_day, valid) )
	{
		// This will recognize both YYYY, YYYY-MM and YYYY-MM-DD formats, but
		// make sure we fail with NaN if there are problems with the format:
		if ( !valid )
			return nan;

		int tzmin = 0;

		// This will recognise Thh:mm:ss.sss time format variations
		if ( ParseTimeISO(tmp, tm_hour, tm_min, tm_sec, strict_fractions, valid) )
		{
			if ( !valid )
				return nan;
			ParseTimezoneISO(tmp, tzmin);
		}

		if ( *tmp == 0 )
		{
			//
			// The complete input has parsed as ISO, accept it and continue
			// to compute the correct time, add timezone etc. (The next loop
			// will be skipped completely).
			//
			// Note: We will continue into the regular date/time parser loop
			// below, but quickly exit since there is no more data remaining.
			//
			flags = PARSEDATE_DATETIME | PARSEDATE_TZ;
			tz_min = tzmin;
			date_str = tmp;
		}
	}

	// Initial whitespace allowed in non-ISO Date strings; skip.
	while ( uni_isspace(*date_str) )
		date_str++;

	for (;;)
	{
		if ( !separator_detected )
			separator_detected = SkipSeps(date_str);

		if ( *date_str == 0 )
			break;

		if ( !separator_detected )
			return nan;

		separator_detected = FALSE;

		// Check AM/PM
		{
			BOOL is_pm;
			if ( ParseTimeAMPM(date_str, is_pm) )
			{
				if ( flags & PARSEDATE_AMPM )
					return nan;
				flags |= (is_pm ? PARSEDATE_PM : PARSEDATE_AM);
				continue;
			}
		}

		// Check timezone
		if ( ParseTimezone(date_str, tz_min, valid) )
		{
			if ( flags & PARSEDATE_TZ || !valid )
				return nan;
			flags |= PARSEDATE_TZ;
			continue;
		}

		// Check comments
		if ( ParseComment(date_str) )
			continue;

		// ISO date format used free-form
		{
			int y;
			int m;
			int d;

			tmp = date_str;
			if ( ParseDateISO(tmp, y, m, d, valid) == 3 )
			{
				if ( flags & PARSEDATE_DATE || !valid )
					return nan;
				flags |= PARSEDATE_DATE;
				date_str = tmp;
				tm_year = y;
				tm_mon = m;
				tm_day = d;
				continue;
			}
		}

		// Time of day
		{
			int h;
			int m;
			int s;
			tmp = date_str;
			int rc = ParseTimeOfDay(tmp, h, m, s, FALSE, 3, valid);
			if ( rc > 0 )
			{
				if ( flags & PARSEDATE_TIME )
					return nan;
				flags |= PARSEDATE_TIME;
				date_str = tmp;
				tm_hour = h;
				tm_min = m;
				tm_sec = (double)s;
				continue;
			}
		}

		if ( ParseMonth(date_str, value) )
		{
			// Textual month name
			if ( date_elem_textmonth )
				return nan; // Duplicate
			date_elem_textmonth = value;
			continue;
		}

		if ( ParseNumber(date_str, value) )
		{
			if ( date_elem_count >= 3 )
				return nan;
			date_elem[date_elem_count++] = value;
			continue;
		}

		if ( ParseDayOfWeek(date_str) )
			continue;

		//
		// If we get here, there is some rubbish in the string
		// that prevents us from making sense of it.
		//
		return nan;
	}

	//
	// Check if we have a complete date with a textual month name
	// as part of the date.  If we do, we arrange for the date to
	// be represented as mm/dd/yy in the date_elem[] array and
	// continue into the code to recognize such dates.
	//
	if ( date_elem_textmonth )
	{
		// Fail if we have too much information
		if ( date_elem_count > 2 )
			return nan;

		if ( date_elem_count == 2 )
		{
			if ( date_elem[0] >= 70 )
			{
				// yy/dd ==> mm/dd/yy
				date_elem[2] = date_elem[0];
				date_elem[0] = date_elem_textmonth;
				if ( date_elem[1] >= 70 )
					return nan;
			}
			else
			{
				// dd/yy ==> mm/dd/yy
				date_elem[2] = date_elem[1];
				date_elem[1] = date_elem[0];
				date_elem[0] = date_elem_textmonth;
				if ( date_elem[2] < 70 )
					return nan;
			}
			date_elem_count++;
		}
	}

	//
	// This section recognizes the two main date formats of
	// 'mm/dd/yy' and 'yy/mm/dd' from the date_elem[] array.
	// When the date_elem[] array holds the information needed,
	// we process this info into a proper date.
	//
	if ( date_elem_count == 3 )
	{
		if ( date_elem[0] < 70 )
		{
			// mm/dd/yy
			tm_mon = date_elem[0] - 1;
			tm_day = date_elem[1];
			tm_year = date_elem[2];
		}
		else
		{
			// yy/mm/dd
			tm_year = date_elem[0];
			if ( date_elem[1] >= 70 )
				return nan;
			tm_mon = date_elem[1] - 1;
			tm_day = date_elem[2];
		}
		if ( tm_year < 100 )
			tm_year += 1900;
		flags |= PARSEDATE_DATE;
		date_elem_count = 0;
		date_elem_textmonth = 0;
	}

	// Fail if a date is not given, or too much date information
	if ( date_elem_textmonth || date_elem_count || !(flags & PARSEDATE_DATE) )
		return nan;

	// Adjust for AM/PM
	UINT32 ampm_flags = flags & PARSEDATE_AMPM;
	if ( ampm_flags )
	{
		if ( !(flags & PARSEDATE_TIME) )
			return nan;

		if ( ampm_flags == PARSEDATE_AM )
		{
			if ( tm_hour == 12 )
				tm_hour = 0;
			if ( tm_hour > 23 )
				return nan;
		}
		else
		{
			if ( tm_hour > 12 )
				return nan;
			if ( tm_hour < 12 )
				tm_hour += 12;
		}
	}

	double raw_time = Date2MS(tm_year, tm_mon, tm_day) +
		Time2MS(tm_hour, tm_min, tm_sec);

	if ( !(flags & PARSEDATE_TZ) )
	{
		//
		// Interpret date as in local timezone. Subtract Timezone offset
		// to get UTC time but with a possible DST offset, and find
		// daylight saving time from that.
		//
		double now_utc = g_op_time_info->GetTimeUTC();
		double local_tza = LocalTZA(now_utc);
		double utc_time = raw_time - local_tza;
		double tz = (local_tza + DaylightSavingTA(utc_time, now_utc)) / msPerMinute;
		tz_min = op_double2int32(tz);
	}

	return raw_time	- tz_min * msPerMinute;
}

/* static */ double
OpDate::GetCurrentUTCTime()
{
	return TimeClip(g_op_time_info->GetTimeUTC());
}

/* static */ int
OpDate::YearFromTime(double t)
{
	// IMPLEMENTATION NOTE #1: We used to have a loop in the implementation of this function which
	// caused poor performance in the SunSpider "date-format-xparb" benchmark.

	// IMPLEMENTATION NOTE #2: Ecmascript uses ISO 8601:2004 dates with an actual (leap) year zero.

	const double days_between_year_0_and_year_1970 = 1 + 1970*365 + (1970-1)/4 - (1970-1)/100 + (1970-1)/400;
	const double day_ms = 24.0*60*60*1000;
	double ms_since_year_zero = t + days_between_year_0_and_year_1970 * day_ms;

	int year;

	if (ms_since_year_zero < 0.0)
	{
		int days_before_year_0 = -static_cast<int>(op_floor(ms_since_year_zero / day_ms));
		year = 0;

		// Regardless of which particular years it is; 400 consecutive years always has
		// _exactly_ 400*365 + 97 == 146097 days; i.e. 303 regular years and 97 leap years.
		const int days_in_400_years = 400 * 365 + 97;
		int q = days_before_year_0 / days_in_400_years;
		int days = days_before_year_0 % days_in_400_years;
		year -= q * 400;

		const int days_in_regular_cent = 100*365 + 24;
		q = days / days_in_regular_cent;
		days = days % days_in_regular_cent;
		year -= q * 100;

		const int days_in_regular_4_years = 4*365 + 1;
		q = days / days_in_regular_4_years;
		days = days % days_in_regular_4_years;
		year -= q * 4;

		const int days_in_1_regular_year = 365;
		q = days / days_in_1_regular_year;
		days = days % days_in_1_regular_year;
		year -= q * 1;

		// If you go X years and Y days forward in time from year 0 (where Y is not a full year) then you're always at year X.
		// However, if you go backwards in time the same amount you're gonna be at year X-1 unless Y==0.0 because then you're at year X;
		return (days > 0) ? (year - 1) : year;
	} else {
		int days_since_year_0 = int(ms_since_year_zero / day_ms);

		// Regardless of which particular years it is; 400 consecutive years always has
		// _exactly_ 400*365 + 97 == 146097 days; i.e. 303 regular years and 97 leap years.
		const int days_in_400_years = 400 * 365 + 97;
		int q = days_since_year_0 / days_in_400_years;
		int days = days_since_year_0 % days_in_400_years;
		year = q * 400;

		// Handle first century of every 400 years specially - it has 25 leap years.
		if (days < 36525)
			return year + (days*4)/(3*365+366);
		days -= 36525;
		year += 100;

		// A "regular" century has no years divisable by 400 and contains 24 leap years.
		const int days_in_regular_cent = 100*365 + 24;
		q = days / days_in_regular_cent;
		days = days % days_in_regular_cent;
		year += q * 100;

		// Handle year XX00 specially; it has no leap year after all.
		if (days < 365)
			return year;

		// Handle the remaining years of the century as if there where 25 leap years.
		days += 1;

		return year + (days*4)/(3*365+366);
	}
}

/* static */ int
OpDate::DaysInYear(int y)
{
    if (y % 4)
        return 365;
    else
        if (y % 100)
            return 366;
        else
            if (y % 400)
                return 365;
            else
                return 366;
}

/* static */ int
OpDate::DayFromYear(double y)
{
	return Month2Day((int)op_floor(y), 0);
}

/* static */ void
OpDate::FindMonth(double t, const int *&day_array, int &month, int &day_in_year)
{
    int dayWithinYear = DayWithinYear(t);
    const int* day_ptr = year_days;

    if (InLeapYear(t))
        day_ptr = leap_year_days;

    for (int i = 0;; ++i)
        if (dayWithinYear < *++day_ptr)
		{
			month = i;
			day_array = day_ptr;
			day_in_year = dayWithinYear;
			return;
		}
}


/* static */ int
OpDate::MonthFromTime(double t)
{
	const int *day_ptr = NULL;
	int month, yearday;
	FindMonth(t, day_ptr, month, yearday);
	return month;
}

/* static */ int
OpDate::DateFromTime(double t)
{
	const int *day_ptr = NULL;
	int month, yearday;
	FindMonth(t, day_ptr, month, yearday);
	return yearday - *(day_ptr - 1) + 1;
}

/* static */ double
OpDate::DaylightSavingTA(double t, double utc)
{
	OP_ASSERT( op_isfinite(t) && sizeof(time_t) >= 4 );

	//
	// Use rule for current year for previous years. This doesn't sound
	// good, but this is how IE/FF does it due to limitations in the
	// windows platform. Also use current year for unsafe future dates.
	//
	// We don't actually use the current year, but one of the following
	// years with same number of days, and with same weekday on Jan 1st.
	// If such a year can't be found in the safe range, go backwards from
	// the current date as a last resort.
	//
	int current_year = YearFromTime(utc ? utc : g_op_time_info->GetTimeUTC());
	int target_year = YearFromTime(t);

#if STDLIB_SIXTY_FOUR_BIT_TIME_T
	const int max_safe_year = 9999;
#else
#  ifdef STDLIB_UNSIGNED_TIME_T
	const int max_safe_year = 2105;
#  else
	const int max_safe_year = 2037;
#  endif
#endif

	if ( target_year < current_year || target_year > max_safe_year )
	{
		int k;
		int days_in_year = DaysInYear(target_year);
		double target_year_time = TimeFromYear(target_year);
		int weekday = WeekDay(target_year_time);
		double tt;

		// Bring current year into the safe range if it is not:
		if ( current_year > max_safe_year )
			current_year = max_safe_year;
		else if ( current_year < 0 )
			current_year = 0;

		for ( k = current_year; k <= max_safe_year; k++ )
		{
			tt = TimeFromYear(k);
			if ( DaysInYear(k) == days_in_year && WeekDay(tt) == weekday )
				goto found;
		}

		for ( k = current_year - 1; k >= 0; k-- )
		{
			tt = TimeFromYear(k);
			if ( DaysInYear(k) == days_in_year && WeekDay(tt) == weekday )
				goto found;
		}

		OP_ASSERT(!"One of the loops should always terminate");
		return 0.0; // Bail out with no DST info

	found:

		//
		// Fold the time we are seeking into the year we have now
		// identified which: (1) Has same number of days, and
		// (2) starts with the same weekday.  Based on this, the DST info
		// should hopefully end up more or less correct.
		//
		t = tt + (t - target_year_time);
	}

	return g_op_time_info->DaylightSavingsTimeAdjustmentMS(t);
}

/* static */ double
OpDate::LocalTime(double t)
{
	// Catch invalid dates.  DaylightSavingTA and functions called from it does not.
	if (!op_isfinite(t))
		return op_nan(NULL);
	else
	{
		double now_utc = g_op_time_info->GetTimeUTC();
		return t + LocalTZA(now_utc) + DaylightSavingTA(t, now_utc);
	}
}

/* static */ double
OpDate::UTC(double t)
{
	// Catch invalid dates.  DaylightSavingTA and functions called from it does not.
	if (!op_isfinite(t))
		return op_nan(NULL);
	else
	{
		double utc_now = g_op_time_info->GetTimeUTC();
		double ltza  = LocalTZA(utc_now);
		return t - ltza - DaylightSavingTA(t - ltza, utc_now);
	}
}

/* static */ double
OpDate::MakeDay(double year, double month, double day)
{
    double t;

	if (!op_isfinite(year) || !op_isfinite(month) || !op_isfinite(day))
		return op_nan(NULL);

    year = op_truncate(year);
    month = op_truncate(month);
    day = op_truncate(day);

    int m = op_double2int32(op_fmod(month, 12.0));

    if (m < 0)
        m += 12;

    year += op_floor(month / 12.0);

    t = DayFromYear(static_cast<int>(year));
    t += DaysInYear(static_cast<int>(year)) == 365 ? year_days[m] : leap_year_days[m];

    return t + day-1;
}

/* static */ double
OpDate::TimeClip(double time)
{
    if (!op_isfinite(time) || op_fabs(time) > 8.64e15)
        return op_nan(NULL);

    return op_truncate(time);
}

/* FIXME: most contexts that call LocalTZA will probably use
   DaylightSavingsTimeAdjustmentMS and/or GetTimeZone and/or GetTimeUTC also,
   may wish to inline and refactor to clean this up, given that the
   PI is really not the way we want it.  */

/* static */ double
OpDate::LocalTZA(double now_utc)
{
	return -(double)(g_op_time_info->GetTimezone()) * msPerSecond - g_op_time_info->DaylightSavingsTimeAdjustmentMS(now_utc);
}

/* static */ double
OpDate::LocalTZA()
{
	return -(double)(g_op_time_info->GetTimezone()) * msPerSecond - g_op_time_info->DaylightSavingsTimeAdjustmentMS(g_op_time_info->GetTimeUTC());
}
