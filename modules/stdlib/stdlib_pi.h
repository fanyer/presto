/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef STDLIB_PI_H
#define STDLIB_PI_H

#ifndef STDLIB_DTOA_CONVERSION

/**
  * Porting interfaces for stdlib's fallback number formatters for platforms
  * that can't use the thirdparty strtod/dtoa library.
  *
  * These APIs need to be folded into Opera's regular PI at some point, but
  * at least this way they are visible.
  *
  * For all of these the rules are:
  *
  *   - The ONLY valid decimal point is '.'.  Formatters must never
  *     produce, and scanners must never scan, any other form of decimal
  *     point.  This usually means that if you're using system functions
  *     to do your dirty work you must force the LC_NUMERIC locale to "C"
  *     in your implementations.
  *
  *   - The ONLY valid exponent markers are 'e' and 'E'.  Formatters
  *     must never produce other markers than these, and scanners must
  *     understand both upper and lower case and not scan other markers.
  *
  * There is some overlap of functionality among some of the functions, in
  * order to allow platforms to choose the best set of functions.
  *
  * None of these functions should be called except from within stdlib.  Most
  * clients (including most clients inside stdlib!) should call op_ functions
  * instead.
  *
  * The file stdlib_pi.cpp contains some implementation sketches and notes
  * that can be used by platforms to construct suitable implementations.
  */
class StdlibPI
{
public:

#ifndef HAVE_STRTOD

	/**
	 * Read a double-precision floating point number.
     *
	 * \param   p     The input string
	 * \param   endp  Location that receives a pointer to the first
	 *                location not consumed
	 * \return  The number read.
	 */
	static double StringToDouble(const char* p, char** endp);

#endif // !HAVE_STRTOD

	/**
	 * Format a double according to a printf %f, %g, or %e specification, with
	 * no field width but with an optional precision specifier.
	 *
	 * This function should normally never be called except from within
	 * stdlib.  Most clients should use the OpDoubleFormat interface instead.
     *
	 * \param buffer  The output buffer
	 * \param bufsiz  The size of that buffer (including space for the NUL)
	 * \param fmt     The format string
	 * \param d       The number to format
	 */
	static void SPrintfDouble(char* buffer, int bufsiz, const char* fmt, double d);

	/**
	 * Format a double-precision number as with the quasi-standard dtoa().  This function
	 * may be a no-op.
	 *
	 * Returns TRUE if it did anything.
	 */
	static BOOL LocaleSafeDTOA( double d, char* b, int precision=-1 );

	/**
	 * Format a double-precision number as with the quasi-standard etoa().  This function
	 * may be a no-op.
	 *
	 * Returns TRUE if it did anything.
	 */
	static BOOL LocaleSafeETOA( double d, char* b, int precision );
};

#endif // !STDLIB_DTOA_CONVERSION
#endif // STDLIB_PI_H
