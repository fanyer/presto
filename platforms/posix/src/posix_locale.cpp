/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (based on old unix/base/common/unix_oplocale.cpp).
 */
// #define INSIDE_PI_IMPL // suppresses deprecation warnings, not currently needed.
#include "core/pch.h"
#undef mktime // suppress lingogi's dont_use_mktime - we need it !

#ifdef POSIX_OK_LOCALE
# ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) X
# endif
#include "platforms/posix/posix_locale.h"
#include "platforms/posix/posix_native_util.h"
# if defined(POSIX_HAS_LANGINFO) || defined(POSIX_HAS_LANGINFO_1STDAY)
#include <langinfo.h>
#  ifdef POSIX_HAS_LANGINFO_1STDAY
#include <time.h> // mktime()
#  endif
# endif

// static
OP_STATUS OpLocale::Create(OpLocale** localep)
{
	*localep = OP_NEW(PosixLocale, ());
	return *localep ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


/* String sort order: */

int PosixLocale::CompareStringsL(const uni_char *str0,
								 const uni_char *str1,
								 long len /* = -1 */,
								 BOOL ignore_case /* = FALSE */)
{
	// Cheap pre-test:
	if (len == 0)
		return 0;

	// Make sure we're using the user's preferred locale:
	PosixNativeUtil::TransientLocale ignoreme(LC_COLLATE);
	ANCHOR(PosixNativeUtil::TransientLocale, ignoreme);

#ifdef POSIX_SYS_WCHAR_ENCODING
#undef DontNeedFallback
#else // system wchar_t is UTF-16, just like uni_char
#define DontNeedFallback
#define RetCheckEval(expr)		\
	int ret = expr;				\
	if (errno == EINVAL)		\
		LEAVE(OpStatus::ERR);	\
	OP_ASSERT(!errno);			\
	return ret
	// </RetCheckEval>

	errno = 0;
	if (len == -1)
		if (ignore_case)
		{
#ifdef posix_wcsicoll
			RetCheckEval(posix_wcsicoll(str0, str1));
#else
#undef DontNeedFallback
#endif
		}
		else
		{
#ifdef posix_wcscoll
			RetCheckEval(posix_wcscoll(str0, str1));
#else
#undef DontNeedFallback
#endif
		}
	else
		if (ignore_case)
		{
#ifdef posix_wcsnicoll
			RetCheckEval(posix_wcsnicoll(str0, str1, len));
#else
#undef DontNeedFallback
#endif
		}
		else
		{
#ifdef posix_wcsncoll
			RetCheckEval(posix_wcsncoll(str0, str1, len));
#else
#undef DontNeedFallback
#endif
		}
	// if the relevant system function is missing, we fall back on strcoll:
#endif // POSIX_SYS_WCHAR_ENCODING

#ifndef DontNeedFallback
	// Further (relatively) cheap pretest - identical strings:
	if ((len < 0
		 ? (ignore_case ? uni_stricmp(str0, str1) : uni_strcmp(str0, str1))
		 : (ignore_case ? uni_strnicmp(str0, str1, len) : uni_strncmp(str0, str1, len))) == 0)
		return 0;

	int res = 0;
	OP_STATUS err = OpStatus::OK;
	const size_t low_len = ignore_case ?
		(len < 0 ? MAX(uni_strlen(str0), uni_strlen(str1)) : static_cast<size_t>(len)) : 0;
	uni_char *lower = ignore_case ? OP_NEWA(uni_char, low_len + 1) : 0;
	if (ignore_case && lower == 0)
		err = OpStatus::ERR_NO_MEMORY;
	else
	{
		if (ignore_case)
		{
			uni_strlcpy(lower, str0, low_len + 1);
			Unicode::Lower(lower, TRUE);
		}

		OpString8 lstr0;
		err = PosixNativeUtil::ToNative(ignore_case ? lower : str0, &lstr0,
										len < 0 ? ~static_cast<size_t>(0) : static_cast<size_t>(len));
		if (OpStatus::IsSuccess(err))
		{
			if (ignore_case)
			{
				uni_strlcpy(lower, str1, low_len + 1);
				Unicode::Lower(lower, TRUE);
			}

			OpString8 lstr1;
			err = PosixNativeUtil::ToNative(ignore_case ? lower : str1, &lstr1,
											len < 0 ? ~static_cast<size_t>(0) : static_cast<size_t>(len));
			if (OpStatus::IsSuccess(err))
			{
				errno = 0;
				res = strcoll(lstr0.CStr(), lstr1.CStr());
				if (errno == EINVAL)
					err = OpStatus::ERR;
				OP_ASSERT(!errno);
			}
		}
		OP_DELETEA(lower);
	}
	LEAVE_IF_ERROR(err);
    return res;
#endif // POSIX_SYS_WCHAR_ENCODING or not all of the posix_wcs[ni]*coll.
}

/* Time-related API: */
size_t PosixLocale::op_strftime(uni_char *dest, size_t limit,
								const uni_char *fmt, const struct tm *when)
{
	/* TODO: use wcsftime (which is POSIX); plus encoding unless uni_char is
	 * natively wchar_t. */
	size_t unilen = 0;
	if (dest && limit > 0)
	{
		dest[0] = 0;
		OpString8 format;
		if (OpStatus::IsSuccess(PosixNativeUtil::ToNative(fmt, &format)))
		{

			const size_t buflen = MB_CUR_MAX * limit;
			char *buffer = OP_NEWA(char, buflen);
			if (buffer)
			{
				PosixNativeUtil::TransientLocale ignoreme(LC_TIME);

				/* Work round DSK-222194: glibc adds some extension fields
				 * (tm_gmtoff, tm_zone) and gets very upset if they're not
				 * initialized right.  Since they're non-standard, we can't
				 * refer to them in this code to clear them, so we do this the
				 * tedious way.  The POSIX documentation of strftime seems to
				 * imply that a call to localtime, gmtime or mktime is needed to
				 * properly initialize a struct tm, if %z or %Z is used; so we
				 * do that (to initialize \em all fields sanely), then
				 * over-write the advertised fields: */
				const time_t now = op_time(NULL);
				struct tm copy;
				if (now + 1 && &copy == localtime_r(&now, &copy))
				{
					copy.tm_sec		= when->tm_sec;
					copy.tm_min		= when->tm_min;
					copy.tm_hour	= when->tm_hour;
					copy.tm_mday	= when->tm_mday;
					copy.tm_mon		= when->tm_mon;
					copy.tm_year	= when->tm_year;
					copy.tm_wday	= when->tm_wday;
					copy.tm_yday	= when->tm_yday;
					copy.tm_isdst	= when->tm_isdst;
					when = &copy;

					size_t mbslen = strftime(buffer, buflen, format.CStr(), when);

					if (mbslen)
						if (OpStatus::IsSuccess(PosixNativeUtil::FromNative(buffer, dest, limit)))
						{
							unilen = uni_strlen(dest);
							OP_ASSERT(unilen < limit);
						}
						else
							unilen = 0;
				}
				else
				{
					OP_ASSERT(!"Hope this never happens :-(");
					/* Not much we can do about it if it does.  Refuse to
					 * proceed, and just return 0 without touching dest. */
				}

				OP_DELETEA(buffer);
			}
		}
	}
	return unilen;
}

BOOL PosixLocale::Use24HourClock()
{
#ifdef POSIX_HAS_LANGINFO
	PosixNativeUtil::TransientLocale ignoreme(LC_TIME);
	/* T_FMT gets a time format suitable for passing to strftime, to get the
	 * time expressed in a manner suitable for the locale.  One could simply
	 * test whether this is equal to T_FMT_AMPM's format,  */
	if (const char *fmt = nl_langinfo(T_FMT))
		while (*fmt)
			if (fmt++[0] == '%')
			{
				/* Parse format specifier.  If this is a %I (12-hour) or %H
				 * (24-hour) then we know our answer; the expansions of %r, %R
				 * and %T include one of these, so also answer our question.
				 * The glibc man page describes (from "Olson's timezone
				 * package") extensions %k and %l (ell), similar to %H and %I
				 * but using space in place of leading zero for single-digit
				 * values.
				 *
				 * A format specifier may be modified by a leading E or O, which
				 * we must pass over to find the format specifier letter we're
				 * looking for.  Neither glibc nor POSIX manual for strftime
				 * says anything about field width specifiers or leading '-' or
				 * '#' as modifier, but experiment (Eddy/2008/Feb/12) reveals
				 * that glibc, at least, supports them.  While nl_langinfo
				 * probably doesn't use these extensions, it seems prudent to be
				 * ready for the possibility of their use; so skip over these as
				 * well as E and O.
				 */
				while (fmt[0] == '#' || fmt[0] == '-' || fmt[0] == '0' ||
					   fmt[0] == 'E' || fmt[0] == 'O')
					fmt++; // skip over flags (not sure if [EO] should count as flags)
				while (op_isdigit(fmt[0]))
					fmt++; // skip over field width
				while (fmt[0] == 'E' || fmt[0] == 'O')
					fmt++; // skip over modifier
				switch (fmt++[0])
				{
				case 'I': case 'l': case 'r': return FALSE; // 12-hour time
				case 'H': case 'k': case 'R': case 'T': return TRUE; // 24-hour time
				default: break; // end of format specifier: keep looking for hour format.
				}
			}
#endif // POSIX_HAS_LANGINFO
	return TRUE; // default
}


/* Number-formatting API: */

#undef snprintf // suppress lingogi's dont_use_snprintf define.
/* Note: uses snprintf *not* op_snprintf.
 *
 * Some platforms set SYSTEM_SNPRINTF to NO because their native snprintf
 * returns -1 (instead of the buffer size needed) if the buffer isn't big
 * enough; their snprintf doesn't conform to op_snprintf's spec, so they fall
 * back on stdlib's version for op_snprintf.  However, stdlib's version doesn't
 * support locale-specific number formatting (and it'd need to call these
 * NumberFormat methods to do so if it did) so we can't use its versions here.
 * We take as given that POSIX systems do support snprintf, even if their
 * handling of overflow is antiquated.
 *
 * OTOH, op_snprintf is perfectly reasonable for preparing a format string.
 */

int PosixLocale::NumberFormat(uni_char *buffer, size_t limit,
							  double number, int precision, BOOL with_thousands_sep)
{
	PosixNativeUtil::TransientLocale ignoreme(LC_NUMERIC);
	int unlen = -1;
	if (buffer && limit > 0)
	{
		// '%', '\'', '.', digits, 'f', '\0'
		char format [5 + INT_STRSIZE(precision)]; // ARRAY OK - Eddy/2008/Jul/24

		op_snprintf(format, sizeof(format),
#ifndef POSIX_SNPRINTF_BROKEN_QUOTE
					with_thousands_sep ? "%%'.%df" :
#endif
					"%%.%df", precision);

		size_t tmpbuflen = MB_CUR_MAX * limit;
		char *tmpbuf = OP_NEWA(char, tmpbuflen);
		if (0 == tmpbuf)
			return 0;

		unlen = snprintf(tmpbuf, tmpbuflen, format, number);
		if (unlen > 0 && (size_t)unlen < tmpbuflen &&
			OpStatus::IsSuccess(PosixNativeUtil::FromNative(tmpbuf, buffer, limit)))
		{
			unlen = uni_strlen(buffer);
			OP_ASSERT((size_t)unlen < limit);
		}
		else
			unlen = -1;

		OP_DELETEA(tmpbuf);
	}
    return unlen;
}

int PosixLocale::NumberFormat(uni_char *buffer, size_t limit,
							  int number, BOOL with_thousands_sep)
{
	PosixNativeUtil::TransientLocale ignoreme(LC_NUMERIC);
	int unlen = -1;
	if (buffer && limit > 0)
	{
		size_t tmpbuflen = MB_CUR_MAX * limit;
		char *tmpbuf = OP_NEWA(char, tmpbuflen);
		if (0 == tmpbuf)
			return 0;

		unlen = snprintf(tmpbuf, tmpbuflen,
#ifndef POSIX_SNPRINTF_BROKEN_QUOTE
						 with_thousands_sep ? "%'d" :
#endif
						 "%d", number);

		if (unlen > 0 && (size_t) unlen < tmpbuflen &&
			OpStatus::IsSuccess(PosixNativeUtil::FromNative(tmpbuf, buffer, limit)))
		{
			unlen = uni_strlen(buffer);
			OP_ASSERT((size_t)unlen < limit);
		}
		else
			unlen = -1;

		OP_DELETEA(tmpbuf);
	}
    return unlen;
}

int PosixLocale::NumberFormat(uni_char *buffer, size_t limit,
							  OpFileLength number, BOOL with_thousands_sep)
{
	PosixNativeUtil::TransientLocale ignoreme(LC_NUMERIC);
	int unlen = -1;
	if (buffer && limit > 0)
	{
		// OpFileLength is an unsigned 64 bit integer (currently unsigned: this may change)
		size_t tmpbuflen = limit * MB_CUR_MAX;
		char * tmpbuf = OP_NEWA(char, tmpbuflen);
		if (0 == tmpbuf)
			return 0;

#ifdef SIXTY_FOUR_BIT
#define OFLfmt "lu"
#else
#define OFLfmt "llu"
#endif
		unlen = snprintf(tmpbuf, tmpbuflen,
#ifndef POSIX_SNPRINTF_BROKEN_QUOTE
						 with_thousands_sep ? "%'" OFLfmt :
#endif
						 "%" OFLfmt, number);
#undef OFLfmt
		if (unlen > 0 && (size_t) unlen < tmpbuflen &&
			OpStatus::IsSuccess(PosixNativeUtil::FromNative(tmpbuf, buffer, limit)))
		{
			unlen = uni_strlen(buffer);
			OP_ASSERT((size_t)unlen < limit);
		}
		else
			unlen = -1;

		OP_DELETEA(tmpbuf);
	}
    return unlen;
}

OP_STATUS PosixLocale::ConvertToLocaleEncoding(OpString8* dest, const uni_char* src)
{
	if (!dest || !src)
		return OpStatus::ERR_NULL_POINTER;

	return PosixNativeUtil::ToNative(src, dest);
}

OP_STATUS PosixLocale::ConvertFromLocaleEncoding(OpString* dest, const char* src)
{
	if (!dest || !src)
		return OpStatus::ERR_NULL_POINTER;

	return PosixNativeUtil::FromNative(src, dest);
}

/* virtual */
OP_STATUS PosixLocale::GetFirstDayOfWeek(int& day)
{
#ifdef POSIX_HAS_LANGINFO_1STDAY
	union { unsigned int origin; char* x; } date;
	/* nl_langinfo usually returns the result as a char*. However the
	 * implementation of _NL_TIME_WEEK_1STDAY uses a union { unsigned int; char*
	 * }, assigns the int part to the decimal reference date in format YYYYMMDD
	 * (i.e. year * (100*100) + month * 100 + day) and returns the char*. */
	date.x = nl_langinfo(_NL_TIME_WEEK_1STDAY);

	struct tm origin;	// Unpack YYYYMMDD:
	op_memset(&origin, 0, sizeof(origin));
	origin.tm_hour = 12;
	origin.tm_mday = date.origin % 100;
	origin.tm_mon = (date.origin / 100) % 100 - 1;
	origin.tm_year = (date.origin / (100 * 100)) - 1900;
	origin.tm_isdst = -1; /* we don't know the dst status of the date */
	// Sanity check for the unpacked date:
	if (origin.tm_mday == 0 || origin.tm_mday > 31 ||
		origin.tm_mon < 0 || origin.tm_mon > 11 ||
		// Assume that only a year between 1000 and 3000 is sensible:
		origin.tm_year < (1000 - 1900) ||
		origin.tm_year > (3000 - 1900))
		return OpStatus::ERR_NOT_SUPPORTED;

	/* Now let mktime() calculate the origin.tm_wday of the date returned by
	 * nl_langinfo: */
	if (mktime(&origin) == (time_t)-1 ||
		origin.tm_wday < 0 || origin.tm_wday > 6)
		return OpStatus::ERR_NOT_SUPPORTED;
	/* origin.tm_wday is now the week-day of the reference-date
	 * (this is the number of days since Sunday, in the range 0 to 6). */

	/* nl_langinfo(_NL_TIME_FIRST_WEEKDAY) returns a pointer to a byte which
	 * indicates which day of the week is the current locale's "first day of the
	 * week". It is 1, if it is the same day of the week as the date origin
	 * obtained above; 2 for the next day and so on up to 7. */
	char* offset = nl_langinfo(_NL_TIME_FIRST_WEEKDAY);
	if (offset == NULL || *offset <= 0 || *offset > 7)
		return OpStatus::ERR_NOT_SUPPORTED;

	day = origin.tm_wday + (*offset) -1;
	return OpStatus::OK;
#else // !POSIX_HAS_LANGINFO_1STDAY
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // POSIX_HAS_LANGINFO_1STDAY
}
#endif // POSIX_OK_LOCALE
