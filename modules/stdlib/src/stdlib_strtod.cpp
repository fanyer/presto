/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2001-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file stdlib_strtod.cpp
 * Approximately-correct string-to-double and double-to-string conversion
 * procedures based on porting interfaces mapping to native printf, dtoa/etoa,
 * or fctv, ecvt, gcvt, not on third party code.
 */

/* There are two reasons for disabling the dtoa/strtod libraries:

     - code size -- if we fall back on existing platform
	   libraries that may be in shared libraries (in ROM),
	   code size is reduced by maybe 8KB

	 - removing third-party code, when the customer is morbidly
	   paranoid about having it

   Since duplicating the functionality of dtoa and strtod is
   unreasonably difficult and would not save code size, we fall
   back on platform libraries even when they are less correct,
   and even if they do not provide the necessary functionality.

   As a result, the expected consequence of disabling the libraries
   for dtoa and strtod is that the ECMAScript interpreter will be
   less correct and will not conform to the ECMAScript spec.
   */

#include "core/pch.h"

#if !defined(STDLIB_DTOA_CONVERSION)

#include "modules/stdlib/stdlib_pi.h"
#include "modules/stdlib/include/double_format.h"

static void strip_exponent(char* buffer);
static BOOL handle_special_cases(char* buffer, double d, BOOL zero);

#ifndef HAVE_STRTOD

double op_strtod( const char *nptr, char **endptr )
{
	return StdlibPI::StringToDouble( nptr, endptr );
}

#endif // !HAVE_STRTOD

/* static */ char *
OpDoubleFormat::ToString(char *buffer, double d)
{
	/* buffer is guaranteed to be at least 32 bytes long */

	if (handle_special_cases(buffer, d, TRUE))
		return buffer;

	if (!StdlibPI::LocaleSafeDTOA(d,buffer))
	{
		/* Try to format with %f first, and if the output looks OK then
		   use it, otherwise redo with %g, adjust the output, and return
		   that instead.
		   */
		StdlibPI::SPrintfDouble(buffer, 32, "%f", d);
		buffer[31] = 0;
		char *dpos=op_strchr(buffer, '.');
		int must_do_g=0;
		if (dpos)
		{
			dpos++;
			while (*dpos == '0')
				dpos++;
			if (!*dpos)
				*op_strchr(buffer, '.') = 0;
			else
				must_do_g=1;
		}
		else
			must_do_g=1;

		if (must_do_g || op_strlen(buffer) > 21)
		{
			StdlibPI::SPrintfDouble(buffer, 32, "%.20g", d);
			buffer[31] = 0;
			strip_exponent(buffer);
		}
	}

	return buffer;
}

/* static */ char *
OpDoubleFormat::ToExponential(register char *buffer, double d, int precision, RoundingBias)
{
	/* buffer is guaranteed to be at least 32 bytes long */  /* FIXME: wrong number -- see testcase in stdlib_float_overflow.ot ) */

	// fmt[24] fits the largest possible int64 followed by "e" into fmt+2
	char fmt[24]; /* ARRAY OK 2009-08-05 molsson */

	if (handle_special_cases(buffer, d, FALSE))
		return buffer;

	if (!StdlibPI::LocaleSafeETOA(d, buffer, precision))
	{
		fmt[0] = '%';
		if (precision == -1)
			fmt[1] = 0;
		else
		{
			fmt[1] = '.';
			op_itoa(precision, fmt+2, 10);
		}
		op_strcat( fmt, "e" );
		StdlibPI::SPrintfDouble(buffer, 31, fmt, d);
		buffer[31] = 0;
	}

	strip_exponent(buffer);
	if (precision == -1)
	{
		/* fixup: trailing zeroes in significand must be removed */
		char *epos = op_strchr( buffer, 'e' );
		if (epos)
		{
			char *epos2=epos;
			epos--;
			while (epos > buffer && *epos == '0')
				epos--;
			if (*epos != '.')
				epos++;
			while (*epos++ = *epos2++)
				;
		}
	}
	return buffer;
}

/* static */ char *
OpDoubleFormat::ToFixed(register char *buffer, double d, int precision, size_t bufsiz, RoundingBias)	/* FIXME: honor bufsiz */
{
	/* buffer is guaranteed to be at least 32 bytes long */

	// fmt[24] fits the largest possible int64 followed by "f" into fmt+2
	char fmt[24]; /* ARRAY OK 2009-08-05 molsson */

	if (handle_special_cases(buffer, d, FALSE))
		return buffer;

	if (!StdlibPI::LocaleSafeDTOA(d, buffer, precision))
	{
		fmt[0] = '%';
		if (precision == -1)
			fmt[1] = 0;
		else
		{
			fmt[1] = '.';
			op_itoa(precision, fmt+2, 10);
		}
		op_strcat( fmt, "f" );
		StdlibPI::SPrintfDouble(buffer, 31, fmt, d);
		buffer[31] = 0;
	}

	return buffer;
}

/* static */ char *
OpDoubleFormat::ToPrecision(register char *buffer, double d, int precision, size_t bufsiz, RoundingBias)	/* FIXME: honor bufsiz */
{
	/* buffer is guaranteed to be at least 32 bytes long

	   Note %g may switch to exponential notation at negative exponents other than
	   the (expt < -6) mandated by the ECMAScript spec.  (For example, on Windows
	   it is expt < -4, and that is what ANSI requires too).  */

	// fmt[24] fits the largest possible int64 followed by "g" into fmt+2
	char fmt[24]; /* ARRAY OK 2009-08-05 molsson */

	if (handle_special_cases(buffer, d, FALSE))
		return buffer;

	fmt[0] = '%';
	if (precision == -1)
		fmt[1] = 0;
	else
	{
		fmt[1] = '.';
		op_itoa(precision, fmt+2, 10);
	}
	op_strcat( fmt, "g" );
	StdlibPI::SPrintfDouble(buffer, 31, fmt, d);
	buffer[31] = 0;
	strip_exponent( buffer );

	return buffer;
}

static BOOL
handle_special_cases(char* buffer, double d, BOOL zero)
{
	if (op_isnan(d))
	{
		op_strcpy( buffer, "NaN" );
		return TRUE;
	}

	if (op_isinf(d))
	{
		if (d < 0)
			op_strcpy( buffer, "-" );
		else
			*buffer = 0;
		op_strcat( buffer, "Infinity" );
		return TRUE;
	}

	if (zero && d == 0.0)
	{
		double_and_bits u;
		u.d = d;

		if (u.bits[IEEE_BITS_HI] == 0x80000000U && u.bits[IEEE_BITS_LO] == 0)
		{
			op_strcpy( buffer, "-0" );
			return TRUE;
		}
	}

	return FALSE;
}

static void
strip_exponent( char *buffer )
{
	/* fixup: leading zeroes in exponent must be removed */
	char *epos = op_strchr( buffer, 'e' );
	char *epos2;
	if (!epos)
	{
		/* Workaround for Symbian bug: they always format with 'E' */
		epos = op_strchr( buffer, 'E' );
		if (epos) *epos = 'e';
	}
	if (epos)
	{
		epos++;
		if (*epos == '+' || *epos == '-')
			epos++;
		epos2 = epos;
		while (*epos == '0' && *(epos+1))
			epos++;
		while (*epos2++ = *epos++)
			;
	}
}

#endif // !STDLIB_DTOA_CONVERSION
