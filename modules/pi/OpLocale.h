/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPLOCALE_H
#define OPLOCALE_H

/** @short Provider of information and operations that need to take the user's locale into account. */
class OpLocale
{
public:
	/** Create an OpLocale object. */
	static OP_STATUS Create(OpLocale** new_locale);

	virtual ~OpLocale() {}

	/** Compare two strings.
	 *
	 * This is a UTF-16 version of strcoll().
	 *
	 * @param str1 String to be compared with str2
	 * @param str2 String to be compared with str1
	 * @param len Number of characters to at most compare in each string. If
	 * -1, compare the entire strings.
	 * @param ignore_case If TRUE, upper/lower-case differences between the two
	 * strings will be ignored when doing the comparison.
	 * @return An integer less than, equal to, or greater than zero if str1 is
	 * found, respectively, to be less than, to match, or be greater than str2,
	 * when both are interpreted as appropriate for the current locale.
	 */
	virtual int CompareStringsL(const uni_char *str1, const uni_char *str2, long len=-1, BOOL ignore_case=FALSE) = 0;

#ifndef DOXYGEN_DOCUMENTATION
	/** Compare two strings.
	 *
	 * @deprecated Call CompareStringsL() instead.
	 */
	DEPRECATED(int CompareStrings(const uni_char *str1, const uni_char *str2, long len=-1));
#endif // !DOXYGEN_DOCUMENTATION

	/** Format date and time.
	 *
	 * This method formats the broken-down 'tm' according to the format
	 * specification 'fmt' and places the result in the character array 'dest'
	 * of size 'max'.
	 *
	 * This is a UTF-16 version of strftime() (C99 7.23.3.5).
	 *
	 * The implementation needs to support at least the following subset of
	 * the specifiers defined by the C99 standard:
	 *
	 * - %a is replaced by the locale's abbreviated weekday name.
	 * - %A is replaced by the locale's full weekday name.
	 * - %b is replaced by the locale's abbreviated month name.
	 * - %B is replaced by the locale's full month name.
	 * - %c is replaced by the locale's appropriate date and time
	 *   representation.
	 * - %d is replaced by the day of the month as a decimal number (01-31).
	 * - %H is replaced by the hour (24-hour clock) as a decimal number (00-23).
	 * - %I is replaced by the hour (12-hour clock) as a decimal number (01-12).
	 * - %j is replaced by the day of the year as a decimal number (001-366).
	 * - %m is replaced by the month as a decimal number (01-12).
	 * - %M is replaced by the minute as a decimal number (00-61).
	 * - %p is replaced by the locale's equivalent of the AM/PM designations
	 *   associated with a 12-hour clock.
	 * - %S is replaced by the second as a decimal number (00-59).
	 * - %U is replaced by the week number of the year (the first Sunday as the
	 *   first day of week 1) as a decimal number (00-53).
	 * - %w is replaced by the weekday as a decimal number (0-6), where Sunday
	 *   is 0.
	 * - %W is replaced by the week number of the year (the first Monday is the
	 *   first day of week 1) as a decimal number (00-53).
	 * - %x is replaced by the locale's appropriate date representation.
	 * - %X is replaced by the locale's appropriate time representation.
	 * - %y is replaced by the year without century as a decimal number (00-99).
	 * - %Y is replaced by the year with century as a decimal number.
	 * - %z is replaced by the offset from UTC (+HHMM or -HHMM), or an empty
	 *   string if no time zone information is available. Positive numbers are
	 *   east of Greenwich (ahead of UTC).
	 * - %% is replaced by %.
	 *
	 * Core only requires support for the specifiers above. If any other
	 * conversion specifiers are supported, they should behave according to
	 * the POSIX specification of strftime; for specifiers not covered by
	 * the POSIX specification, the behavior is undefined.
	 *
	 * @param dest Destination buffer for formatted date/time
	 * @param max Maximum number of characters to put in 'dest', including
	 * terminating NUL byte
	 * @param fmt Format string. Refer to strftime() documentation for details
	 * @param tm Time stamp to present as a string
	 *
	 * @return The number of characters placed in the array 'dest', not
	 * including the terminating NUL byte, provided the string, including the
	 * terminating NUL byte, fits. Otherwise, it returns 0, and the contents of
	 * the array is undefined.
	 */
	virtual size_t op_strftime(uni_char *dest, size_t max, const uni_char *fmt, const struct tm *tm) = 0;

	/** Check if the locale uses 24h or AM/PM clock.
	 *
	 * @return TRUE if locale is 24h, FALSE otherwise.
	 */
	virtual BOOL Use24HourClock() = 0;

	/** Get the first day of week for current localization.
	 *
	 * @param day - The index of day in week: 0 - Sunday, 1 - Monday,
	 * 2 - Tuesday and so on.
	 * @return See OpStatus. The value of day is only defined on success.
	 */
	virtual OP_STATUS GetFirstDayOfWeek(int& day) = 0;

	/** Represent a number as a string.
	 *
	 * @param[out] buffer destination buffer
	 * @param limit maximum number of characters to write to 'buffer',
	 * including NUL terminator
	 * @param number to be rendered localized
	 * @param with_thousands_sep TRUE if thousands shall be grouped according to locale
	 * @return Number of characters written, or -1 on failure, in which case
	 * the contents of 'buffer' are undefined.
	 */
	virtual int NumberFormat(uni_char *buffer, size_t limit, int number, BOOL with_thousands_sep) = 0;

	/** Represent a number as a string.
	 *
	 * @param buffer (out) destination buffer
	 * @param limit maximum number of characters to write to 'buffer',
	 * including NUL terminator
	 * @param number to be rendered localized
	 * @param precision Number of digits after decimal point
	 * @param with_thousands_sep TRUE if thousands shall be grouped according to locale
	 * @return Number of characters written, or -1 on failure, in which case
	 * the contents of 'buffer' are undefined.
	 */
	virtual int NumberFormat(uni_char *buffer, size_t limit, double number, int precision, BOOL with_thousands_sep) = 0;

	/** Represent a number as a string.
	 *
	 * @param buffer (out) destination buffer
	 * @param limit maximum number of characters to write to 'buffer',
	 * including NUL terminator
	 * @param number to be rendered localized
	 * @param with_thousands_sep TRUE if thousands shall be grouped according to locale
	 * @return Number of characters written, or -1 on failure, in which case
	 * the contents of 'buffer' are undefined.
	 */
	virtual int NumberFormat(uni_char *buffer, size_t limit, OpFileLength number, BOOL with_thousands_sep) = 0;

	/** Convert a string from UTF-16 to the system's locale encoding.
	 *
	 * In some cases Opera needs to convert from its internal representation of
	 * character data (UTF-16) to the way the system's locale represents it.
	 *
	 * @param[out] locale_str Set to the string converted (in the locale encoding)
	 * @param utf16_str UTF-16 string that is to be converted
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other generic
	 * conversion failure.
	 */
	virtual OP_STATUS ConvertToLocaleEncoding(OpString8* locale_str, const uni_char* utf16_str) = 0;

	/** Convert a string from the system's locale encoding to UTF-16.
	 *
	 * In some cases Opera needs to convert character data from the way the
	 * system's locale represents it to Opera's internal representation
	 * (UTF-16).
	 *
	 * @param[out] utf16_str Set to the string converted (in UTF-16)
	 * @param locale_str Locale-encoded string that is to be converted
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other generic
	 * conversion failure.
	 */
	virtual OP_STATUS ConvertFromLocaleEncoding(OpString* utf16_str, const char* locale_str) = 0;
};

#ifndef DOXYGEN_DOCUMENTATION
inline int OpLocale::CompareStrings(const uni_char *str1, const uni_char *str2, long len)
{
	int retval = -1;

	TRAPD(dummy, retval = CompareStringsL(str1, str2, len));

	return retval;
}
#endif // !DOXYGEN_DOCUMENTATION

#endif // !OPLOCALE_H
