/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
   Note: this file uses the option "no-pch", so it is compiled without
   pre-compiled header.

   On compiling this file COMPILING_STDLIB_PI is defined. The
   platform's system.h needs to provide the declaration of some stdlib
   functions in some cases. This could be implemented like

   @code
   // Example for a platform's system.h:

   // first define the system's capabilities:
   #define SYSTEM_SNPRINTF   YES // or NO
   #define SYSTEM_SPRINTF    YES // or NO
   #define SYSTEM_STRTOD     YES // or NO
   #define SYSTEM_SSCANF     YES // or NO
   ...

   #if defined(COMPILING_STDLIB_PI) && !defined(STDLIB_DTOA_CONVERSION)
   // If this platform doesn't use thirdparty dtoa code, the stdlib module
   // needs to implement the some functions in
   // modules/stdlib/stdlib_pi.cpp, so if we include this header file
   // while compiling the file stdlib_pi.cpp, we might need (depending
   // on the above system definitions) require some system functions:

   # if !SYSTEM_STRTOD && SYSTEM_SSCANF
   // stdlib's StdlibPi::StringToDouble() uses the system's sscanf()
   // (not op_sscanf()) which is provided by including <stdio.h>:
   #  define INCLUDE_STDIO_H
   # else // SYSTEM_STRTOD || !SYSTEM_SSCANF
   // Either StdlibPi::StringToDouble() is not defined because
   // the system's strtod() can be used (SYSTEM_STRTOD == YES), or
   // stdlib's StdlibPi::StringToDouble() uses an approximate
   // implementation which does not need the system's sscanf() but is
   // good enough in many cases (SYSTEM_STRTOD == NO and
   // SYSTEM_SSCANF == NO)
   # endif // !SYSTEM_STRTOD && SYSTEM_SSCANF

   # if SYSTEM_SNPRINTF
   // stdlib's StdlibPI::SPrintfDouble() uses the system's snprintf()
   // to print a double, so include <stdio.h> to provide the
   // declaration of snprintf()
   #  define INCLUDE_STDIO_H
   # elif SYSTEM_VSNPRINTF
   // stdlib's StdlibPI::SPrintfDouble() uses the system's va_list,
   // va_start(), va_end() and vsnprintf() to print a double, so
   // include <stdio.h> to provide the declaration of vsnprintf and
   // include <stdarg.h> to provide the declaration of va_start()
   // etc.
   #  define INCLUDE_STDIO_H
   #  define INCLUDE_STDARG_H
   # elif SYSTEM_SPRINTF
   // if there is no snprintf nor vsnprintf, stdlib's
   // StdlibPI::SPrintfDouble() uses the system's sprintf() to print a
   // double, so include <stdio.h> to provide the declaration of
   // sprintf()
   #  define INCLUDE_STDIO_H
   # elif SYSTEM_VSPRINTF
   // if there is no snprintf nor vsnprintf, stdlib's
   // stdlib's StdlibPI::SPrintfDouble() uses the system's va_list,
   // va_start(), va_end() and vsprintf() to print a double, so
   // include <stdio.h> to provide the declaration of vsprintf and
   // include <stdarg.h> to provide the declaration of va_start()
   // etc.
   #  define INCLUDE_STDIO_H
   #  define INCLUDE_STDARG_H
   # else // !SYSTEM_S(N)PRINTF && !SYSTEM_VS(N)PRINTF
   // stdlib cannot provide an implementation for
   // StdlibPI::SprintfDouble(), so the platform must do it:
   # endif // !SYSTEM_S(N)PRINTF || !SYSTEM_VS(N)PRINTF
   #endif // COMPILING_STDLIB_PI && !STDLIB_DTOA_CONVERSION

   ...
   #ifdef INCLUDE_STDIO_H
   # include <stdio.h>
   #else !INCLUDE_STDIO_H
   // generate a compile error if anybody still uses the system stdio
   // functions instead of the op_... functions:
   # define sscanf dont_use_sscanf
   # define snprintf dont_use_snprintf
   # define sprintf dont_use_sprintf
   # define vsnprintf dont_use_vsnprintf
   # define vsprintf dont_use_vsprintf
   ...
   #endif // INCLUDE_STDIO_H
   #undef INCLUDE_STDIO_H

   #ifdef INCLUDE_STDARG_H
   # include <stdarg.h>
   #endif // INCLUDE_STDIO_H
   #undef INCLUDE_STDARG_H
   ...
   @endcode
 */
#define COMPILING_STDLIB_PI
#include "core/pch_system_includes.h"

/* NOTE: The implementation of these functions MUST NOT call any op_ functions
 * that may end up parsing or printing floating point numbers.
 *
 * Occasionally setting the locale may not have the desired effect on producing
 * the right decimal point in the formatters.  If that's so, then use the following
 * snippet to transform commas to decimal points.
 */

#if 0 // Helper code, use as needed
    for ( char *b = buffer; *b ; ++b )
        if (*b == ',')
            *b = '.';
#endif

#ifndef STDLIB_DTOA_CONVERSION

#include "modules/stdlib/stdlib_pi.h"

#ifndef HAVE_STRTOD

#ifdef HAVE_SSCANF  // Normally you want some platform names here, too

double
StdlibPI::StringToDouble( const char* p, char** endp )
{
	// FIXME! set endp!
	double d;

	char *oldlocale = op_setlocale( LC_NUMERIC, NULL );
	op_setlocale( LC_NUMERIC, "C" );

	sscanf(p, "%lf", &d);  // Not op_sscanf!

	op_setlocale( LC_NUMERIC, oldlocale );
	return d;
}

#else // !HAVE_SSCANF

/* This is approximate but good enough in many cases */

double
StdlibPI::StringToDouble( const char* p, char** endp )
{
	double value = 0.0, fraction = 0.0;
	int divisor = 0;
	int exponent = 0;
	int exp_sign = 1;
	int int_sign = 1;
	BOOL has_start = FALSE;

	// Skip any initial whitespace
	for (; op_isspace(*reinterpret_cast<const unsigned char *>(p)); ++ p)
		/* nothing */;

	if (endp != NULL)
		*endp = const_cast<char*>(p);

	// Integer part

	if (*p == '+' || *p == '-')
	{
		int_sign = (*(p ++) == '-') ? -1 : 1;
	}

	// Handle infinity

	if (*p == 'i' || *p == 'I')
	{
		if (strni_eq(p, "INFINITY", 8))
		{
			if (endp != NULL)
				*endp = const_cast<char *>(p + 8);
			return int_sign == 1 ? g_opera->stdlib_module.m_infinity : g_opera->stdlib_module.m_negative_infinity;
		}
	}

	while (*p >= '0' && *p <= '9')
	{
		has_start = TRUE;
		value = value * 10.0 + (*p - '0');
		p++;
	}

	value *= int_sign;

	// Fractional part.  Fractional part is optional; digits in fractional part
	// are optional if there was an integer part.

	if (*p == '.' && (p[1] >= '0' && p[1] <= '9' || has_start))
	{
		has_start = TRUE;
		p++;
		while (*p >= '0' && *p <= '9')
		{
			fraction = fraction * 10.0 + (*p - '0');
			divisor++;
			p++;
		}
	}

	// Exponent marker.  Exponent marker is valid if there was an integer or
	// fractional part.  The exponent must have at least one digit.

	if (has_start &&
		(*p == 'e' || *p == 'E') &&
		(((p[1] == '+' || p[1] == '-') && p[2] >= '0' && p[2] <= '9') ||
		 (p[1] >= '0' && p[1] <= '9')))
	{
		p++;
		if (*p == '+')
			++p;
		else if (*p == '-')
		{
			exp_sign = -1;
			++p;
		}
		while (*p >= '0' && *p <= '9')
		{
			exponent = exponent*10 + (*p - '0');
			++p;
		}
	}

	if (has_start)
		value = (value + fraction / op_pow(10.0, divisor)) * op_pow(10.0, (exponent*exp_sign)) ;

	if (endp != NULL)
		*endp = const_cast<char*>(p);

	return value;
}

#endif  // HAVE_SSCANF && platform names

#endif // !HAVE_STRTOD

#if !defined HAVE_SNPRINTF && defined HAVE_VSNPRINTF

/* Synthesize snprintf from vsnprintf if necessary and possible */

static int snprintf(char* buf, size_t bufsiz, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int res = vsnprintf(buf, bufsiz, fmt, args);
	va_end(args);
	return res;
}

#define HAVE_SNPRINTF

#elif !defined HAVE_SPRINTF && defined HAVE_VSPRINTF

/* Synthesize sprintf from vsprintf if necessary and possible */

static int sprintf(char* buf, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int res = vsprintf(buf, fmt, args);
	va_end(args);
	return res;
}

#define HAVE_SPRINTF

#endif // !defined HAVE_SNPRINTF && defined HAVE_VSNPRINTF

#if defined HAVE_SNPRINTF

void
StdlibPI::SPrintfDouble(char *buffer, int bufsiz, const char* fmt, double d)
{
	char *oldlocale = op_setlocale( LC_NUMERIC, NULL );
	op_setlocale( LC_NUMERIC, "C" );

	snprintf(buffer, bufsiz-1, fmt, d);  // Not op_snprintf!

	op_setlocale( LC_NUMERIC, oldlocale );
}

#elif defined HAVE_SPRINTF

void
StdlibPI::SPrintfDouble(char *buffer, int bufsiz, const char* fmt, double d)
{
	//
	// A 64-bit IEEE 754-1985 'double' has a maximum value of
	// +/-1.7976931348623157*10^308, and has 15 decimal digits of precision,
	// so 350 characters should be plenty to hold a number written out
	// in full (i.e. a format without an exponent).
	//
	char localbuf[350];	// ARRAY OK 2012-03-13 peter

	OP_STATIC_ASSERT(sizeof(d) <= 8);	// Give error for larger than 64-bit doubles

#ifdef DEBUG_ENABLE_OPASSERT
	// Set up a guard when OP_ASSERT is enabled
	localbuf[349] = 0;
#endif

	char *oldlocale = op_setlocale( LC_NUMERIC, NULL );
	op_setlocale( LC_NUMERIC, "C" );
	sprintf(localbuf, fmt, d);			// Not op_sprintf!
	OP_ASSERT(localbuf[349] == 0);		// Overflowed anyway! How?

	op_strncpy( buffer, localbuf, bufsiz );
	buffer[bufsiz-1] = 0;

	op_setlocale( LC_NUMERIC, oldlocale );
}

#else // !HAVE_SNPRINTF and !HAVE_SPRINTF

// No code available, which means that we need platform code to implement
// StdlibPI::SPrintfDouble(). There will be a link error.

#endif // HAVE_SNPRINTF or HAVE_SPRINTF

// LocaleSafeDTOA() and LocaleSafeETOA() must be implemented in platform
// code. If you have no locale-safe dtoa() or etoa(), just make them
// return FALSE, which means that the stdlib module will fall back on
// StdlibPI::SPrintfDouble():
//
//	/* static */ BOOL
//	StdlibPI::LocaleSafeDTOA( double d, char* b, int precision )
//	{
//		// If you have a locale-safe dtoa(), call it here
//		return FALSE;	// Fall back on StdlibPI::SPrintfDouble
//	}
//
//	/* static */ BOOL
//	StdlibPI::LocaleSafeETOA( double d, char* b, int precision )
//	{
//		// If you have a locale-safe etoa(), call it here
//		return FALSE;	// Fall back on StdlibPI::SPrintfDouble
//	}

#endif // !STDLIB_DTOA_CONVERSION
