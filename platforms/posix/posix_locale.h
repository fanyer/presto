/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (based on old unix/base/common/unix_oplocale.cpp).
 */
#ifndef POSIX_LOCALE_H
#define POSIX_LOCALE_H __FILE__
# ifdef POSIX_OK_LOCALE

#include "modules/pi/OpLocale.h"

/** Implementation of OpLocale.
 *
 * Note that none of the methods needs the object: this class is effectively a
 * namespace, but we want to support platforms whose compilers don't support
 * namespace.  Consequently, this implementation deliberately makes it possible
 * (for non-core code; obviously core is forbidden to cheat) to instantiate an
 * auto variable of type PosixLocale and simply use its methods, without going
 * through the palava of calling OpLocale::Create() and checking its return
 * status.  This should be as cheap as if the class were a namespace ;-)
 */
class PosixLocale : public OpLocale
{
public:
	PosixLocale() {} // Deliberately public (see above).
	// Not needed, since it has nothing to do, and inherits no-op ~OpLocale:
	// virtual ~PosixLocale();

	// Unicode strcoll()/strncoll()
	virtual int CompareStringsL(const uni_char *str1, const uni_char *str2,
								long len=-1 , BOOL ignore_case=FALSE);

	// Unicode strftime
	virtual
	size_t op_strftime(uni_char *dest, size_t max,
									 const uni_char *fmt, const struct tm *tm);

	// 24hr vs AM/PM
	virtual BOOL Use24HourClock();

	// locale-dependent number formatting; yields number of bytes written.
	virtual int NumberFormat(uni_char *buffer, size_t max,
							 int number, BOOL with_thousands_sep);
    virtual int NumberFormat(uni_char *buffer, size_t max,
							 OpFileLength number, BOOL with_thousands_sep);
    virtual int NumberFormat(uni_char *buffer, size_t max,
							 double number, int precision, BOOL with_thousands_sep);

	virtual OP_STATUS ConvertToLocaleEncoding(OpString8* dest, const uni_char* src);
	virtual OP_STATUS ConvertFromLocaleEncoding(OpString* dest, const char* src);

	virtual OP_STATUS GetFirstDayOfWeek(int& day);
};

# endif // POSIX_OK_LOCALE
#endif // POSIX_LOCALE_H
