/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpLocale.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/macutils.h"

OP_STATUS OpLocale::Create(OpLocale** new_systeminfo)
{
	*new_systeminfo = new MacOpLocale;

	return *new_systeminfo ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

MacOpLocale::MacOpLocale()
{
	CFLocaleRef locale = CFLocaleCopyCurrent();
	m_date_time_formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterLongStyle, kCFDateFormatterMediumStyle);
	m_date_formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterLongStyle, kCFDateFormatterNoStyle);
	m_time_formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterNoStyle, kCFDateFormatterMediumStyle);
	m_weekday_formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
	CFDateFormatterSetFormat(m_weekday_formatter, CFSTR("EEE"));
	m_weekdayfull_formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
	CFDateFormatterSetFormat(m_weekdayfull_formatter, CFSTR("EEEE"));
	m_month_formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
	CFDateFormatterSetFormat(m_month_formatter, CFSTR("MMM"));
	m_monthfull_formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
	CFDateFormatterSetFormat(m_monthfull_formatter, CFSTR("MMMM"));
	CFRelease(locale);
}

MacOpLocale::~MacOpLocale()
{
	CFRelease(m_date_time_formatter);
	CFRelease(m_date_formatter);
	CFRelease(m_time_formatter);
	CFRelease(m_weekday_formatter);
	CFRelease(m_weekdayfull_formatter);
	CFRelease(m_month_formatter);
	CFRelease(m_monthfull_formatter);
}

int MacOpLocale::CompareStringsL( const uni_char *str1, const uni_char *str2, long len
#ifdef PI_CAP_COMPARESTRINGSL_CASE
								  , BOOL ignore_case
#endif // PI_CAP_COMPARESTRINGSL_CASE
	)
{
	int ret = 0;
	Boolean got_result = false;
	CFOptionFlags flags = kCFCompareNonliteral | kCFCompareLocalized;
#ifdef PI_CAP_COMPARESTRINGSL_CASE
	if (ignore_case)
		flags |= kCFCompareCaseInsensitive;
#endif // PI_CAP_COMPARESTRINGSL_CASE
	CFStringRef string1 = CFStringCreateWithCharacters(kCFAllocatorDefault,(const UniChar *) str1, (len > 0) ? len : uni_strlen(str1));
	if (string1) {
		CFStringRef string2 = CFStringCreateWithCharacters(kCFAllocatorDefault,(const UniChar *) str2, (len > 0) ? len : uni_strlen(str2));
		if (string2) {
			ret = CFStringCompare(string1, string2, flags);
			got_result = true;
			CFRelease(string2);
		}
		CFRelease(string1);
	}
	if (!got_result)
		ret = uni_strnicmp(str1, str2, len);

	return ret;
}

size_t MacOpLocale::op_strftime( uni_char *dest, size_t max, const uni_char *fmt, const struct tm *tm )
{
	const long kSecondsFrom1904To1970 = 2082844800;  /* ((66 * 365 + 17) * 24 * 60 * 60) */
	struct tm foo = *tm;
	SInt64 my_time = mktime(&foo);
	size_t result = 0;
	uni_char f;
	char str_buff[255];
	size_t part_size;
	uni_char* uni_result = 0;
	char dummy_fmt[4] = "%a";

	my_time += kSecondsFrom1904To1970  - kCFAbsoluteTimeIntervalSince1904;

	while ((f = *fmt++) != 0)
	{
		if (f == UNI_L('%'))
		{
			CFStringRef date_time_string = NULL;
			f = *fmt++;
			switch (f)
			{
				case 0:
					break;
				case UNI_L('c'): // Date and time representation appropriate for locale
					date_time_string = CFDateFormatterCreateStringWithAbsoluteTime(0, m_date_time_formatter, my_time);
					break;
				case UNI_L('x'): // Date representation for current locale
					date_time_string = CFDateFormatterCreateStringWithAbsoluteTime(0, m_date_formatter, my_time);
					break;
				case UNI_L('X'): // Time representation for current locale
					date_time_string = CFDateFormatterCreateStringWithAbsoluteTime(0, m_time_formatter, my_time);
					break;
				case UNI_L('a'): // Weekday representation for current locale
					date_time_string = CFDateFormatterCreateStringWithAbsoluteTime(0, m_weekday_formatter, my_time);
					break;
				case UNI_L('A'): // Weekday (full) representation for current locale
					date_time_string = CFDateFormatterCreateStringWithAbsoluteTime(0, m_weekdayfull_formatter, my_time);
					break;
				case UNI_L('b'): // Month representation for current locale
					date_time_string = CFDateFormatterCreateStringWithAbsoluteTime(0, m_month_formatter, my_time);
					break;
				case UNI_L('B'): // Month (full) representation for current locale
					date_time_string = CFDateFormatterCreateStringWithAbsoluteTime(0, m_monthfull_formatter, my_time);
					break;
				case UNI_L('Z'): //Time zone name
					{
						CFTimeZoneRef tzone = CFTimeZoneCopySystem();
						if (tzone)
						{
							CFStringRef tzname = CFTimeZoneGetName(tzone);
							if (tzname) {
								part_size = CFStringGetLength(tzname);
								if (dest && (result < max)) {
									int copyLen = MIN(part_size, max - result - 1);
									CFStringGetCharacters(tzname, CFRangeMake(0, copyLen), (UniChar*)dest + result);
									dest[result + copyLen] = 0;
								}
								result += part_size;
							}
							CFRelease(tzone);
						}
					}
					break;
				case UNI_L('%'):
					if (dest && ((result + 1) < max)) {
						dest[result] = UNI_L('%');
						dest[result+1] = 0;
					}
					result++;
					break;
				default:
					// Let the system handle it.
					dummy_fmt[1] = f;
					strftime(str_buff,255,dummy_fmt,tm);
					uni_result = StrFromLocaleEncoding(str_buff);
					if (uni_result) {
						part_size = uni_strlen(uni_result);
						if (dest && (result < max))
							uni_strlcpy(dest + result, uni_result, max - result);
						result += part_size;
						delete [] uni_result;
						uni_result = 0;
					}
					break;
			}
			if (date_time_string) {
				part_size = CFStringGetLength(date_time_string);
				if (dest && (result < max)) {
					int copyLen = MIN(part_size, max - result - 1);
					CFStringGetCharacters(date_time_string, CFRangeMake(0, copyLen), (UniChar*)dest + result);
					dest[result + copyLen] = 0;
				}
				result += part_size;
				CFRelease(date_time_string);
			}
		}
		else
		{
			if (dest && ((result + 1) < max)) {
				dest[result] = f;
				dest[result+1] = 0;
			}
			result++;
		}
	}

	return result <= max ? result : 0;
}

BOOL MacOpLocale::Use24HourClock()
{
	// mariusab 20070228: Body of this function was cut and pasted from ALocale in softcore which was discontinued with the release of arthropoda_2_beta_38
	BOOL twentyfour_hour_clock = TRUE;
#ifdef HAVE_NL_LANGINFO
char *fmt = nl_langinfo(T_FMT);
while (fmt && *fmt)
	if (fmt++[0] == '%')
		switch (fmt++[0]) {
		case 'I': case 'l': // 12-hour time
			twentyfour_hour_clock = FALSE;
			// fall through
		case 'H': case 'k': case 'R': case 'T': // 24-hour time
			// exit loop:
			fmt = 0;
		default: // other formats: keep going, looking for hour.
			break;
	}
#endif // HAVE_NL_LANGINFO}
	return twentyfour_hour_clock;
}

int MacOpLocale::NumberFormat(uni_char * buffer, size_t max, int number, BOOL with_thousands_sep)
{
	char *low_fmt;

	if (with_thousands_sep)
		low_fmt = "%'d";
	else
		low_fmt = "%d";

	unsigned int unlen = snprintf((char *) buffer, max*2, low_fmt, number);

	if (unlen <= 0)
		return 0;

    uni_char *high_res = StrFromLocaleEncoding( (char *)buffer );

	if( !high_res )
		return 0;

    unlen = uni_strlen( high_res );

    if( unlen >= max )
		unlen = max-1;

    memcpy( buffer, high_res, unlen*2 );
    buffer[unlen]=0;

	delete[] high_res;

    return unlen;
}

int MacOpLocale::NumberFormat(uni_char * buffer, size_t max, double number, int precision, BOOL with_thousands_sep)
{
	char low_fmt [24];

	if (with_thousands_sep)
		sprintf(low_fmt, "%%'.%df", precision);
	else
		sprintf(low_fmt, "%%.%df", precision);

	unsigned int unlen = snprintf((char *) buffer, max * 2, low_fmt, number);

	if (unlen <= 0)
		return 0;

    uni_char *high_res = StrFromLocaleEncoding( (char *)buffer );

	if( !high_res )
		return 0;

    unlen = uni_strlen( high_res );

    if( unlen >= max )
		unlen = max-1;

    memcpy( buffer, high_res, unlen*2 );
    buffer[unlen]=0;

	delete[] high_res;

    return unlen;
}

int MacOpLocale::NumberFormat(uni_char * buffer, size_t max, OpFileLength number, BOOL with_thousands_sep)
{
	char *low_fmt;

#ifdef MAC_64BIT_FILES
	if (with_thousands_sep)
		low_fmt = "%'llu";
	else
		low_fmt = "%llu";
#else
	if (with_thousands_sep)
		low_fmt = "%'lu";
	else
		low_fmt = "%lu";
#endif

	char * low_res = new char [max * 2 + 1];
	if (!low_res)
		return 0;

	unsigned int unlen = snprintf(low_res, max*2, low_fmt, number);
	if (unlen <= 0)
	{
		delete [] low_res;
		return 0;
	}

	uni_char *high_res = StrFromLocaleEncoding(low_res);

	delete [] low_res;

	if (!high_res)
		return 0;

    unlen = uni_strlen( high_res );

    if( unlen >= max )
		unlen = max-1;

    memcpy( buffer, high_res, unlen*2 );
    buffer[unlen]=0;

	delete[] high_res;

    return unlen;
}

OP_STATUS MacOpLocale::ConvertFromLocaleEncoding(OpString* utf16_str, const char* locale_str)
{
	return utf16_str->SetFromUTF8(locale_str);
}

OP_STATUS MacOpLocale::ConvertToLocaleEncoding(OpString8* locale_str, const uni_char* utf16_str)
{
	return locale_str->SetUTF8FromUTF16(utf16_str);
}
