/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "WindowsOpLocale.h"

char *StrToLocaleEncoding(const uni_char *str)
{
	int len;
	len = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, str, -1,
							  NULL, 0, NULL, NULL);
	char *result = new char[len];
	if (result)
		WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, str, -1,
							result, len, NULL, NULL);
	return result;
}


uni_char *StrFromLocaleEncoding(const char *str)
{
	int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
	uni_char *result = NULL;
	if (len)
	{
		result = new uni_char[len];
		if (result)
		{
			result[0] = '\0'; // If the next call fails
			MultiByteToWideChar(CP_ACP, 0, str, -1, result, len);
		}
	}
	return result;
}

static int LocaleGroupingToNumber()
{
	// This is ugly. Read for instance the article 
	// at http://blogs.msdn.com/shawnste/archive/2006/07/17/668741.aspx for background.
	uni_char buf[10];
	int ret = 0;
	if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SGROUPING, buf, 10) != 0)
	{
		int last_digit = 0;
		uni_char* p = buf;
		while (*p)
		{
			if (*p < '0' || *p > '9')
			{
				break;
			}
			last_digit = *p-'0';
			ret = 10*ret + last_digit;
			p++;
			if (*p == ';')
			{
				p++;
			}
		}
		if (last_digit == 0)
		{
			ret /= 10;
		}
		else
		{
			ret *= 10;
		}
	}
	return ret;
}

static int GetLocaleInfoInt(int locale_value)
{
	int i;
	if (!GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_RETURN_NUMBER | locale_value, (LPWSTR)&i, sizeof(i) / sizeof(TCHAR)))
	{
		return -1;
	}
	return i;
}

/**
 * Get a locale seperator from the windows locale.
 */
static void GetLocaleSeparator(int separator, uni_char buf[4])
{
	if (!GetLocaleInfo(LOCALE_USER_DEFAULT, separator, buf, 4))
	{
		buf[0] = separator == LOCALE_SDECIMAL ? '.' : '\0';
		buf[1] = '\0';
	}
}

OP_STATUS OpLocale::Create(OpLocale** new_locale)
{
	OP_ASSERT(new_locale);
	*new_locale = new WindowsOpLocale();
	if (*new_locale == NULL)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err;
	if (OpStatus::IsError(err =((WindowsOpLocale*)*new_locale)->InitLocale()))
	{
		delete *new_locale;
		return err;
	}
	return OpStatus::OK;
}

OP_STATUS WindowsOpLocale::InitLocale()
{
	OpString tmpbuf;
	if (!tmpbuf.Reserve(2))
		return OpStatus::ERR_NO_MEMORY;

	GetLocaleInfo( LOCALE_USER_DEFAULT, LOCALE_ITIME, tmpbuf, tmpbuf.Capacity());
	if (tmpbuf.Compare(UNI_L("0")))
		m_use_24_hours = TRUE;
	else
		m_use_24_hours = FALSE;

	m_leading_zero = GetLocaleInfoInt(LOCALE_ILZERO);
	m_grouping = LocaleGroupingToNumber();
	m_negative_order = GetLocaleInfoInt(LOCALE_INEGNUMBER);

	GetLocaleSeparator(LOCALE_STHOUSAND, m_thousands_sep);
	GetLocaleSeparator(LOCALE_SDECIMAL, m_decimal_sep);

	return OpStatus::OK;

}


int WindowsOpLocale::CompareStringsL(const uni_char *str1, const uni_char *str2, long len, BOOL ignore_case)
{
	int retval;

	errno = 0;

	if (len > 0) // Compare a specific number of characters.
		retval = ignore_case ? _wcsnicoll(str1, str2, len) : _wcsncoll(str1, str2, len);
	else if (len < 0) // Compare everything.
		retval = ignore_case ? wcsicoll(str1, str2) : wcscoll(str1, str2);
	else
		retval = 0;

	if (errno == EINVAL)
		LEAVE(OpStatus::ERR);

	// If the length is 0 we should compare the first zero characters.
	return retval;
}

const uni_char *WindowsOpLocale::op_ctime( const time_t *t )
{
	return _wctime(t);
}


size_t WindowsOpLocale::op_strftime( uni_char *dest, size_t max, const uni_char *fmt, const struct tm *tm )
{
	// do some validation
	if(max == 0 ||
		tm->tm_mday > 31 || tm->tm_mday < 1 ||		/* mday in decimal (01-31) */
		tm->tm_year > 8000 ||
		tm->tm_hour < 0 || tm->tm_hour > 23 ||		/* 24-hour decimal (00-23) */
		tm->tm_yday < 0 || tm->tm_yday > 365 ||		/* yday in decimal (001-366) */
		tm->tm_mon < 0 || tm->tm_mon > 12 ||		/* month in decimal (01-12) */
		tm->tm_sec < 0 || tm->tm_sec > 59 )			/* secs in decimal (00-59) */
	{
		return 0;
	}
	return wcsftime(dest, max, fmt, tm);
}

BOOL WindowsOpLocale::Use24HourClock()
{
	return m_use_24_hours;
}

OP_STATUS WindowsOpLocale::GetFirstDayOfWeek(int& day)
{
	int val = 0;
	int retval = GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_IFIRSTDAYOFWEEK|LOCALE_RETURN_NUMBER, (LPWSTR)&val, sizeof(int));
	if (retval == 0)
		return OpStatus::ERR;
	day = (val + 1) % 7;
	return OpStatus::OK;
}

int WindowsOpLocale::NumberFormat(uni_char * buffer, size_t max, int number, BOOL with_thousands_sep)
{
	OpString number_string;
	OpStatus::Ignore(number_string.AppendFormat(UNI_L("%i"), number));

	return InternalNumberFormat(buffer, max, number_string.CStr(), 0, with_thousands_sep);
}


int WindowsOpLocale::NumberFormat(uni_char * buffer, size_t max, double number, int precision, BOOL with_thousands_sep)
{
	int decimal_pos = 0;
	int sign = 0;

	const char* fcvt_buf = _fcvt(number, precision, &decimal_pos, &sign);
	if (fcvt_buf == 0)
		return 0;

	OpString8 number_string;

	if (sign != 0)
		OpStatus::Ignore(number_string.Append("-"));

	if (decimal_pos >= 0)
	{
		OpStatus::Ignore(number_string.Append(fcvt_buf, decimal_pos));
		OpStatus::Ignore(number_string.AppendFormat(".%s", fcvt_buf + decimal_pos));
	}
	else
	{
		OpString8 format;
		OpStatus::Ignore(format.AppendFormat(".%%0%uu%%s", -decimal_pos));

		OpStatus::Ignore(number_string.AppendFormat(format.CStr(), 0, fcvt_buf));
	}
	OpString number_string16;
	number_string16.Set(number_string);

	return InternalNumberFormat(buffer, max, number_string16.CStr(), precision, with_thousands_sep);
}


int WindowsOpLocale::NumberFormat(uni_char * buffer, size_t max, OpFileLength number, BOOL with_thousands_sep)
{
	OpString number_string;
	if (number_string.Reserve(max) == 0)
		return 0;

	// Use the MS version here, since it supports 64 bit numbers.
	swprintf(number_string.CStr(), UNI_L("%I64u"), number);

	return InternalNumberFormat(buffer, max, number_string.CStr(), 0, with_thousands_sep);
}

int WindowsOpLocale::InternalNumberFormat(uni_char* buffer, size_t max, const uni_char* number, int precision, BOOL with_thousands_sep)
{
	if (buffer == 0 || max == 0)
		return 0;

	int characters_written = 0;

	NUMBERFMT format;
	format.NumDigits = precision; // GetLocaleInfoInt(LOCALE_IDIGITS);
	format.LeadingZero = m_leading_zero;
	if (with_thousands_sep)
	{
		format.Grouping = m_grouping;
		format.lpThousandSep = m_thousands_sep;
	}
	else
	{
		format.Grouping = 0;
		format.lpThousandSep = UNI_L("");
	}
	format.lpDecimalSep = m_decimal_sep;
	format.NegativeOrder = m_negative_order;

	characters_written = GetNumberFormat(LOCALE_USER_DEFAULT, 0, number, &format, buffer, max);
	if (characters_written > 0)
		--characters_written; // GetNumberFormat() seems to include the null terminator in its count.

	return characters_written;
}

OP_STATUS WindowsOpLocale::ConvertToLocaleEncoding(OpString8* locale_str, const uni_char* utf16_str)
{
	char* locale_buf = StrToLocaleEncoding(utf16_str);

	if (!locale_buf)
		return OpStatus::ERR_NO_MEMORY;

	locale_str->TakeOver(locale_buf);

	return OpStatus::OK;
}

OP_STATUS WindowsOpLocale::ConvertFromLocaleEncoding(OpString* utf16_str, const char* locale_str)
{
	uni_char* utf16_buf = StrFromLocaleEncoding(locale_str);

	if (!utf16_buf)
		return OpStatus::ERR_NO_MEMORY;

	utf16_str->TakeOver(utf16_buf);

	return OpStatus::OK;
}

