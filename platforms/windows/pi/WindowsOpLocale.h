/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWSOPLOCALE_H
#define WINDOWSOPLOCALE_H

#include "modules/pi/OpLocale.h"

char *StrToLocaleEncoding(const uni_char *str);
uni_char *StrFromLocaleEncoding(const char *str);

class WindowsOpLocale
:	public OpLocale
{
public:
	// OpLocale.
	// CompareStrings() is deprecated, call CompareStringsL() instead
	// int CompareStrings(const uni_char *str1, const uni_char *str2, long len);
	const uni_char *op_ctime(const time_t *t);
	size_t op_strftime(uni_char *dest, size_t max, const uni_char *fmt, const struct tm *tm );
	BOOL Use24HourClock();
	virtual OP_STATUS GetFirstDayOfWeek(int& day);
	int NumberFormat(uni_char * buffer, size_t max, int number, BOOL with_thousands_sep);
	int NumberFormat(uni_char * buffer, size_t max, double number, int precision, BOOL with_thousands_sep);
	int NumberFormat(uni_char * buffer, size_t max, OpFileLength number, BOOL with_thousands_sep);
	int CompareStringsL(const uni_char *str1, const uni_char *str2, long len=-1, BOOL ignore_case=FALSE);
	OP_STATUS ConvertToLocaleEncoding(OpString8* locale_str, const uni_char* utf16_str);
	OP_STATUS ConvertFromLocaleEncoding(OpString* utf16_str, const char* locale_str);

	OP_STATUS InitLocale();

private:
	// Internal methods.
	int InternalNumberFormat(uni_char* buffer, size_t max, const uni_char* number, int precision, BOOL with_thousands_sep);

	BOOL m_use_24_hours;
	UINT m_leading_zero;
	UINT m_grouping;
	UINT m_negative_order;
	uni_char m_thousands_sep[4];
	uni_char m_decimal_sep[4];

};

#endif // WINDOWSOPLOCALE_H
