/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

/** @file datefun.cpp
  * Generic date functions.
  */

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/util/datefun.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/timecache.h"
#include "modules/stdlib/util/opdate.h"
#ifdef UTIL_CHECKED_STRFTIME
#include "modules/pi/OpLocale.h"
#endif // UTIL_CHECKED_STRFTIME

/* We need to initialize in the constructor, which means we need to hack this */
#ifdef HAS_COMPLEX_GLOBALS
# define CONST_UTIL_ARRAY(name,localname,type) const type const name[] = {
#else
# define CONST_UTIL_ARRAY(name,localname,type) void init_##name (UtilModule *self) { const type *local = self->localname; int i=0;
# define CONST_UTIL_ARRAY_INIT(name) init_##name(this)
#endif

CONST_UTIL_ARRAY(g_wkday, m_wkday, uni_char*)
	CONST_ENTRY(UNI_L("Sun")),
	CONST_ENTRY(UNI_L("Mon")),
	CONST_ENTRY(UNI_L("Tue")),
	CONST_ENTRY(UNI_L("Wed")),
	CONST_ENTRY(UNI_L("Thu")),
	CONST_ENTRY(UNI_L("Fri")),
	CONST_ENTRY(UNI_L("Sat"))
CONST_END(g_wkday)

CONST_UTIL_ARRAY(g_weekday, m_weekday, uni_char*)
	CONST_ENTRY(UNI_L("Sunday")),
	CONST_ENTRY(UNI_L("Monday")),
	CONST_ENTRY(UNI_L("Tuesday")),
	CONST_ENTRY(UNI_L("Wednesday")),
	CONST_ENTRY(UNI_L("Thursday")),
	CONST_ENTRY(UNI_L("Friday")),
	CONST_ENTRY(UNI_L("Saturday"))
CONST_END(g_weekday)

CONST_UTIL_ARRAY(g_month, m_month, uni_char*)
	CONST_ENTRY(UNI_L("Jan")),
	CONST_ENTRY(UNI_L("Feb")),
	CONST_ENTRY(UNI_L("Mar")),
	CONST_ENTRY(UNI_L("Apr")),
	CONST_ENTRY(UNI_L("May")),
	CONST_ENTRY(UNI_L("Jun")),
	CONST_ENTRY(UNI_L("Jul")),
	CONST_ENTRY(UNI_L("Aug")),
	CONST_ENTRY(UNI_L("Sep")),
	CONST_ENTRY(UNI_L("Oct")),
	CONST_ENTRY(UNI_L("Nov")),
	CONST_ENTRY(UNI_L("Dec"))
CONST_END(g_month)

#ifndef HAS_COMPLEX_GLOBALS

void UtilModule::InitDateFunArrays()
{
	CONST_UTIL_ARRAY_INIT(g_wkday);
 	CONST_UTIL_ARRAY_INIT(g_weekday);
 	CONST_UTIL_ARRAY_INIT(g_month);
}
#endif // !HAS_COMPLEX_GLOBALS

#ifdef UTIL_GET_THIS_YEAR
int GetThisYear(unsigned int *month)
{
	// look a day ahead
	if (g_thistime != g_timecache->CurrentTime())
	{
		g_thistime = g_timecache->CurrentTime();
		struct tm* gmt_time = op_gmtime(&g_thistime);
		g_thisyear = gmt_time ? gmt_time->tm_year + 1900 : 1970;
		g_thismonth = gmt_time ? gmt_time->tm_mon+1 : 1;
	}
	if(month) *month = g_thismonth;
	return g_thisyear;
}
#endif // UTIL_GET_THIS_YEAR

#ifdef UTIL_MAKE_DATE3
BOOL MakeDate3(time_t* t, uni_char* buf, size_t buf_len)
{
	time_t loc_time = *t;
	struct tm* gmt_time = op_gmtime(&loc_time);

	if(!gmt_time)
		return FALSE;

	return FMakeDate(*gmt_time, "\247w, \247D \247n \247Y \247h:\247m:\247s UTC", buf, buf_len);
}
#endif // UTIL_MAKE_DATE3

#ifdef UTIL_FMAKE_DATE
static const struct {
	int lower, upper;
} FormatDateLimits[] = {
	{ 0, 23}, // h
	{ 0, 59}, // m
	{ 0, 59}, // s
	{ 1, 31}, // D
	{ 1, 12}, // M
	{ 0, 99}, // y
	{ 1900, INT_MAX}  // Y
};

static const char DateFormatChars[] = "hmsDMyYwWnN";

BOOL FMakeDate(struct tm gt, const char* format, uni_char* buf, size_t buf_len)
{
	if (!format)
		return FALSE;
	size_t i = 0,j;
	int value[7];
	const char* ftmp = format;
	char fchar;

	value[0] = gt.tm_hour;
	value[1] = gt.tm_min;
	value[2] = gt.tm_sec;
	value[3] = gt.tm_mday;
	value[4] = gt.tm_mon+1;
	value[5] = gt.tm_year % 100;
	value[6] = gt.tm_year+1900;

	while (i < buf_len && *ftmp)
	{
		if((unsigned char)*ftmp != (unsigned char)'\247') // Section sign
		{
			buf[i++] = *(ftmp++);
			continue;
		}

		if (i+2 >= buf_len)
			return FALSE;
		ftmp++;

		fchar = *(ftmp++);

		for(j = 0; j<11 ;j++)
		{
			if(DateFormatChars[j] ==  fchar)
			{
				if(j<7)
				{
					int len;
					if(j == 6 && i+5 >= buf_len)
						return FALSE;

					int val = value[j];
					if (val < FormatDateLimits[j].lower || val > FormatDateLimits[j].upper)
						return FALSE;
					len = uni_sprintf(buf+i, UNI_L("%02d"), val);
					if (len <=1)
						return FALSE;
					i+=len;
					break;
				}
				else if(j==7)
				{
					if(i+3 >= buf_len || gt.tm_wday<0 || gt.tm_wday >= 7)
						return FALSE;

					uni_strncpy(buf+i,g_wkday[gt.tm_wday],3);
					i += 3;
				}
				else if(j==9)
				{
					if(i+3 >= buf_len || gt.tm_mon<0 || gt.tm_mon >= 12)
						return FALSE;

					uni_strncpy(buf+i,g_month[gt.tm_mon],3);
					i += 3;
				}
			}
		}
	}

	if (i >= buf_len)
		return FALSE;
	buf[i] = '\0';

	return TRUE;
}
#endif // UTIL_FMAKE_DATE

#ifdef UTIL_GET_TM_DATE
BOOL GetTmDate(const uni_char* date_str, struct tm &gmt_time)
{
	int i = 0;
	int j = 0;
	gmt_time.tm_isdst = 0;

	int type = 0; // type: 0 = undetermined, 1 = rfc 822, 2 = rfc 850, 3 = ansi
	const uni_char* tmp = date_str;

	if (uni_isdigit(*tmp))
		gmt_time.tm_wday = 0;
	else
	{
		while (uni_isalpha(tmp[i]))
			i++;
		if (i != 3)
			type = 2;
		const uni_char* const * week = (i == 3) ? g_wkday : g_weekday;
		for (j=0; j<7; j++)
			if (uni_strnicmp(tmp, week[j], i) == 0)
				break;
		if (j == 7)
			return FALSE;
		gmt_time.tm_wday = j;
	}

	while (tmp[i] && !uni_isalnum(tmp[i]))
		i++;

	if (type == 0 && uni_isalpha(tmp[i]))
	{
		type = 3; // ansi
	}
	else if (uni_isdigit(tmp[i]))
	{
		if (!type)
			type = 1; // rfc 822
	}
	else
		return FALSE;

	uni_char mnth[6]; /* ARRAY OK 2009-04-03 adame */
	int year;
	if (type == 1)
	{
		// Some servers apparently uses type 2 dates, but with three letter weekday
		// This also conforms with the original cookie draft
		// %*1[ -] skips one space or one dash
		if (uni_sscanf(date_str+i, UNI_L("%u%*1[ -]%3c%*1[ -]%u %u:%u:%u"), &gmt_time.tm_mday, &mnth, &year, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec) != 6)
		{
			gmt_time.tm_sec = 0;
			if (uni_sscanf(date_str+i, UNI_L("%u%*1[ -]%3c%*1[ -]%u %u:%u"), &gmt_time.tm_mday, &mnth, &year, &gmt_time.tm_hour, &gmt_time.tm_min) != 5)
				return FALSE;
		}
	}
	else if (type == 2)
	{
		if (uni_sscanf(date_str+i, UNI_L("%u-%3c-%u %u:%u:%u"), &gmt_time.tm_mday, &mnth, &year, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec) != 6)
		{
			unsigned int mon=13;
			if (uni_sscanf(date_str+i, UNI_L("%u-%u-%u %u:%u:%u"), &gmt_time.tm_mday, &mon, &year, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec) == 6)
			{
				if(mon>0 && mon <= 12)
					uni_strncpy(mnth, g_month[mon-1],3);
				else
					mnth[0] = mnth[1] = mnth[2] = '\0';
			}
			else if (uni_sscanf(date_str+i, UNI_L("%u%*1[ -]%3c%*[^ -]%*1[ -]%u %u:%u:%u"), &gmt_time.tm_mday, &mnth, &year, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec) != 6)
				if (uni_sscanf(date_str+i, UNI_L("%u%*1[ -]%3c%*1[ -]%u %u:%u:%u"), &gmt_time.tm_mday, &mnth, &year, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec) != 6)
				return FALSE;
		}
	}
	else if (type == 3)
	{
		if (uni_sscanf(date_str+i, UNI_L("%3c%*[^ ] %u %u:%u:%u %u"), &mnth, &gmt_time.tm_mday, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec, &year) != 6 &&
			uni_sscanf(date_str+i, UNI_L("%3c %u %u:%u:%u %u"), &mnth, &gmt_time.tm_mday, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec, &year) != 6)
		{
			if (uni_sscanf(date_str+i, UNI_L("%3c%*[^ ] %u %u:%u:%u %u"), &mnth, &gmt_time.tm_mday, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec, &year) != 6 &&
				uni_sscanf(date_str+i, UNI_L("%3c %u, %u, %u:%u:%u"), &mnth, &gmt_time.tm_mday, &year, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec) != 6 &&
				uni_sscanf(date_str+i, UNI_L("%3c%*[^ ] %u, %u, %u:%u:%u"), &mnth, &gmt_time.tm_mday, &year, &gmt_time.tm_hour, &gmt_time.tm_min, &gmt_time.tm_sec) != 6)
					return FALSE;
		}
	}
	else
		return FALSE;

	if (year >= 1900)
		year -= 1900;
	else
		if(year >= 0 && year<70)
			year += 100;

	gmt_time.tm_year = year;

	for (j=0; j<12; j++)
		if (uni_strnicmp(mnth, g_month[j], 3) == 0)
			break;
	if (j == 12)
		return FALSE;
	gmt_time.tm_mon = j;

	return TRUE;
}
#endif // UTIL_GET_TM_DATE

#ifdef UTIL_GET_DATE

/**
 * Inverse operation of gmtime().
 * "Inspired" by mktime() in modules/stdlib/src/stdlib_date.cpp.
 */
static time_t op_timegm(struct tm* tm)
{
	tm->tm_sec = tm->tm_sec >= 0 && tm->tm_sec <= 60 ? tm->tm_sec : 0;
	tm->tm_min = tm->tm_min >= 0 && tm->tm_min <= 59 ? tm->tm_min : 0;
	tm->tm_hour = tm->tm_hour >= 0 && tm->tm_hour <= 23 ? tm->tm_hour : 0;
	tm->tm_mday = tm->tm_mday >= 1 && tm->tm_mday <= 31 ? tm->tm_mday : 1;  // Will be adjusted later
	tm->tm_mon = tm->tm_mon >= 0 && tm->tm_mon <= 11 ? tm->tm_mon : 0;
	tm->tm_year = tm->tm_year >= 0 ? tm->tm_year : 0;
	tm->tm_isdst = 0;                                                       // Assume UTC and no DST
	tm->tm_wday = 0;                                                        // Will be set later
	tm->tm_yday = 0;                                                        // Will be set later

	// Interpret as in UTC
	double t = OpDate::MakeDate(OpDate::MakeDay(tm->tm_year + 1900, tm->tm_mon, tm->tm_mday),
	                            OpDate::MakeTime(tm->tm_hour, tm->tm_min, tm->tm_sec, 0));

	tm->tm_wday = OpDate::WeekDay(t);
	tm->tm_yday = OpDate::DayWithinYear(t);

#if STDLIB_SIXTY_FOUR_BIT_TIME_T
	return static_cast<time_t>(t / msPerSecond);
#else
	return op_double2uint32(t / msPerSecond);
#endif
}

time_t GetDate(const char* date_str)
{
	return GetDate(make_doublebyte_in_tempbuffer(date_str, op_strlen(date_str)));
}

time_t GetDate(const uni_char* date_str)
{
	struct tm gmt_time;
	if (GetTmDate(date_str, gmt_time))
	{
		if (gmt_time.tm_year < 70 ||
			(gmt_time.tm_year == 70 && (gmt_time.tm_mon <= 0 || gmt_time.tm_mday < 1 ||
			   (gmt_time.tm_hour == 0 && gmt_time.tm_min == 0 && gmt_time.tm_sec == 0))) )
			return 1;
		/* Avoid overflowing time functions: A four-digit time_t wraps
		 * after 2037; a eight-digit time_t overflows a 32-bit signed
		 * int for time formatting in the year 2147483647 (0x7FFFFFFF),
		 * which adjusted for the 1900 offset is 0x7FFFF893 */
		static const int max_tm_year = sizeof (time_t) == 4 ? 136 : 0x7FFFF893;
		if(gmt_time.tm_year > max_tm_year)
			gmt_time.tm_year = max_tm_year;
		time_t ret_time = op_timegm(&gmt_time);
		if ((long)ret_time == -1)
			return 0;

		return ret_time;
	}
	return 0;
}
#endif // UTIL_GET_DATE

#ifdef UTIL_CHECKED_STRFTIME
OP_STATUS CheckedStrftime(uni_char *date_buf, unsigned int buf_len, const uni_char *fmt, struct tm *tm)
{
	op_mktime(tm); // ignore return, called for side-effect on pointer.
	OP_ASSERT(0 <= tm->tm_sec  && tm->tm_sec  <= 61); // ANSI C: up to 2 leap seconds
	OP_ASSERT(0 <= tm->tm_min  && tm->tm_min  <  60);
	OP_ASSERT(0 <= tm->tm_hour && tm->tm_hour <  24);
	OP_ASSERT(0 <  tm->tm_mday && tm->tm_mday <= 31);
	OP_ASSERT(0 <= tm->tm_mon  && tm->tm_mon  <  12);
	OP_ASSERT(0 <= tm->tm_wday && tm->tm_wday <   7);
	OP_ASSERT(0 <= tm->tm_yday && tm->tm_yday < 366);

#ifdef MSWIN
	if (tm->tm_year >= 8100) // See bug 338462
		return OpStatus::ERR_OUT_OF_RANGE;
#endif // Stupid platform that can't cope if formatted year needs > 4 digits ...

	if (g_oplocale->op_strftime(date_buf, buf_len, fmt, tm) == 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}
#endif // UTIL_CHECKED_STRFTIME
