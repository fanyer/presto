/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file stdlib_float.cpp
    Floating point operations and conversions.

    Many are inline, see the header file.

    Reference for IEEE floating point number layout:
    http://www.psc.edu/general/software/packages/ieee/ieee.html.

    Note, strtod, dtoa, g_fmt, toExponential, toPrecision, and toFixed
    are defined in thirdparty/stdlib_dtoa.cpp and thirdparty/stdlib_g_fmt.cpp.  */

#include "core/pch.h"

#if !defined(IEEE_MC68k) && !defined(IEEE_8087)
	/* If you hit the #error below you *MUST NOT* work around it by
	   commenting out.  Opera requires one or the other to be defined.

	   In core-2 these are defined in the SYSTEM system.

       IEEE_MC68k means that the most significant word of the double
       (sign, exponent, most significant bits of mantissa) are stored
       at lower addresses than the least significant word.  IEEE_8087
       is the opposite.  In neither case is anything implied about the
       order of bytes within that word.

       The names are what they are because some third party code uses
	   them; if this bothers you greatly, talk to lth about it.

	   Don't use OPERA_BIG_ENDIAN as a synonym for IEEE_MC68k; some
       platforms are little endian but store doubles big endian.  (ARM,
       at least, and I suspect the Hitachi SH-4.)

       Systems that have both emulators and hardware may find that
	   they use IEEE_8087 on one and IEEE_MC68k on the other (this is
	   often true for Symbian).
	   */
#	error "Unable to determine the layout of double precision numbers."
#endif

extern "C" {

double stdlib_intpart(double x)
{
	/* Not defined on NaN or Infinity. */
	/* The number is 1.f...f * 2^e where f...f is the 52-bit significand
	   and e is the exponent field minus the bias.  The integer part has
	   the same sign and exponent the MAX(52-e,0) low-order bits of the
	   significand will be cleared.

	   FRACBITS = 52
	   EXPSHIFT = 20
	   EXPBIAS = 1023
	   EXPMASK = 0x7FF << EXPSHIFT
	   */

	double_and_bits u;
	u.d = x;
	int expt = ((u.bits[IEEE_BITS_HI] & EXPMASK) >> EXPSHIFT) - EXPBIAS;

	if (expt < 0)
		return op_copysign(0.0,x);

	if (expt > FRACBITS)
		return x;

	if (expt <= 20)
	{
		u.bits[IEEE_BITS_LO] = 0;
		u.bits[IEEE_BITS_HI] &= -1 << (20 - expt);
	}
	else
		u.bits[IEEE_BITS_LO] &= -1 << (52 - expt);

	return u.d;
}

double op_nan(const char* tagp)
{
	/* The meaning of tagp is implementation-defined.  Here is our
	   meaning:

	   If tagp is not NULL then it is a pointer to an 8-byte area
	   which represents a 64-bit quantity as two native-endianness
	   32-bit unsigned integers: first the most significant part,
	   then the least significant part.  The upper 13 bits of the
	   64-bit number are ignored; the lower 51 bits are installed
	   in the NaN.

       If the lower 51 bits are all zero, and the system uses a 0
	   in the 52nd bit to represent a quiet NaN, then the 51st bit
	   will be set to 1 to prevent the number from being interpreted
	   as an infinity.

	   For example:

	       UINT32 bits[2];
	       bits[0] = ...;
	       bits[1] = ...;
	       d = op_nan((char*)&bits)  */

	if (tagp == NULL)
		return g_stdlib_quiet_nan;

	UINT32 *ints = (UINT32*)tagp;
	double_and_bits u;
	u.d = g_stdlib_quiet_nan;
	u.bits[IEEE_BITS_LO] = ints[1];
	u.bits[IEEE_BITS_HI] = (u.bits[IEEE_BITS_HI] & ~0x7ffff) | (ints[0] & 0x7ffff);

#ifndef QUIET_NAN_IS_ONE
	if (op_isinf(u.d))
		u.bits[IEEE_BITS_HI] |= 1 << 18;
#endif

	return u.d;
}

#ifndef HAVE_FLOOR

double op_floor(double x)
{
	if (x < 0)
	{
		double i = stdlib_intpart(x);
		return (x == i) ? x : i-1;
	}
	else
		return stdlib_intpart(x);
}

#endif

#ifndef HAVE_MODF

double op_modf(double x, double* iptr)
{
	double x2 = op_fabs(x);
	double i = stdlib_intpart(x2);
	*iptr = op_copysign(i, x);
	return op_copysign(x2 - i, x);
}

#endif

#ifndef HAVE_FMOD

double op_fmod(double x, double y)
{
	if (y == 0.0)
		return 0.0;
	double q = op_fabs(stdlib_intpart(x/y)*y);
	return (x >= 0) ? x - q : x + q;
}

#endif

#ifndef HAVE_FREXP

double op_frexp(double value, int* exp)
{
	if (value == 0.0)
	{
		*exp = 0;
		return 0.0;
	}
	else
	{
		double_and_bits u;
		u.d = value;

		*exp = ((u.bits[IEEE_BITS_HI] & EXPMASK) >> EXPSHIFT) - EXPBIAS + 1;
		u.bits[IEEE_BITS_HI] = (u.bits[IEEE_BITS_HI] & ~ EXPMASK) + ((EXPBIAS - 1) << EXPSHIFT);

		return u.d;
	}
}

#endif

} // extern "C"

double uni_atof(const uni_char * str)
{
	// According to the standard, atof is strtod but without the error
	// checking. But since we don't have any error checking anyway,
	// we just call that code
	return uni_strtod(str, NULL);
}

/* Note that uni_strtod may not assume that the string is null-terminated,
   and there is no way to find out where the number prefix ends without
   actually scanning the string.  Thus we must duplicate some of the
   work of op_strtod here.  Given that we have to do that anyway, we also
   allow an input length to be provided.  */

double uni_strntod(const uni_char * nptr, uni_char ** endptr, int nchars)
{
	uni_char sign = '+';

	while (nchars > 0 && uni_isspace(*nptr))
	{
		--nchars;
		++nptr;
	}

	const uni_char* start = nptr;

	if (nchars > 0 && (*nptr == '+' || *nptr == '-'))
	{
		sign = *nptr;
		--nchars;
		++nptr;
	}

	if (nchars >= 3 && (*nptr == 'I' || *nptr == 'i'))
	{
		if (uni_tolower(nptr[1]) == 'n' && uni_tolower(nptr[2]) == 'f')
		{
			nptr += 3;
			nchars -= 3;
			if (nchars >= 5 &&
				uni_tolower(nptr[0]) == 'i' &&
				uni_tolower(nptr[1]) == 'n' &&
				uni_tolower(nptr[2]) == 'i' &&
				uni_tolower(nptr[3]) == 't' &&
				uni_tolower(nptr[4]) == 'y')
			{
				nptr += 5;
			}
			if (endptr)
				*endptr = const_cast<uni_char*>(nptr);
			return g_stdlib_infinity * (sign == '+' ? 1 : -1);
		}
	}

	if (nchars >= 3 && (*nptr == 'N' || *nptr == 'n'))
	{
		if (uni_tolower(nptr[1]) == 'a' && uni_tolower(nptr[2]) == 'n')
		{
			nptr += 3;
			if (endptr)
				*endptr = const_cast<uni_char*>(nptr);
			return op_nan(NULL);
		}
	}

	while (nchars > 0 && (*nptr >= '0' && *nptr <= '9'))
	{
		--nchars;
		++nptr;
	}

	if (nchars > 0 && *nptr == '.')
	{
		++nptr;
		--nchars;

		while (nchars > 0 && *nptr >= '0' && *nptr <= '9')
		{
			--nchars;
			++nptr;
		}
	}

	if (nchars > 0 && (*nptr == 'e' || *nptr == 'E'))
	{
		++nptr;
		--nchars;

		if (nchars > 0 && (*nptr == '+' || *nptr == '-'))
		{
			--nchars;
			++nptr;
		}

		while (nchars > 0 && *nptr >= '0' && *nptr <= '9')
		{
			--nchars;
			++nptr;
		}
	}

	/* Now copy strings from start to nptr-1 and append a null byte, and
	   pass it all to op_strtod. */

	// Use a stack-array for short numbers (most common case):
	char buf[100];		// ARRAY OK 2009-02-16 mortenro
	char *bufp = buf;	// Pointer to buf or to heap-allocated storage

	size_t len = nptr - start + 1;
	if (len > sizeof(buf))
	{
		bufp = (char*)op_malloc(len);
		if (bufp == NULL)
			return g_stdlib_infinity;
	}


	const uni_char *src = start;
	char *dest = bufp;
	while (src < nptr)
		*dest++ = (char)*src++;
	*dest = 0;

	char* newend;
	double res = op_strtod(bufp, &newend);

	if (endptr)
		*endptr = const_cast<uni_char*>(start) + (newend - bufp);

	if (bufp != buf)
		op_free(bufp);

	return res;
}

double uni_strtod(const uni_char * nptr, uni_char ** endptr)
{
	return uni_strntod(nptr, endptr, INT_MAX);
}

double op_implode_double( UINT32 hi, UINT32 lo )
{
    double_and_bits u;

	OP_STATIC_ASSERT(sizeof(u) == sizeof(double));

	u.bits[IEEE_BITS_HI] = hi;
	u.bits[IEEE_BITS_LO] = lo;

	return u.d;
}

void op_explode_double( double r, UINT32& hiword, UINT32& loword )
{
    double_and_bits u;

	OP_STATIC_ASSERT(sizeof(u) == sizeof(double));

	u.d = r;
	hiword = u.bits[IEEE_BITS_HI];
	loword = u.bits[IEEE_BITS_LO];
}

UINT32 op_double_high( double r )
{
    double_and_bits u;
	u.d = r;
	return u.bits[IEEE_BITS_HI];
}

UINT32 op_double_low( double r )
{
    double_and_bits u;
	u.d = r;
	return u.bits[IEEE_BITS_LO];
}

INT32 op_double2int32( double x )
{
#ifdef INT_CAST_IS_ES262_COMPLIANT	// a tweak
	return (INT32)(UINT32)x;
#else
	double q, r;

	// Step 2: discard boundary cases
	if (!op_isfinite(x) || x == 0.0)
		return 0;

	// Step 3: Adjust x
	if (x < 0.0)
		x = -op_floor(op_fabs(x));
	else
		x = op_floor(x);

	// Step 4: Compute r = x MOD 2^32
	if (0.0 <= x && x < 4294967296.0)
		r = x;
	else if (-4294967296.0 < x && x < 0.0)
		r = x + 4294967296.0;
	else {
		q = op_floor(x / 4294967296.0);
		r = x - (q * 4294967296.0);
	}
	if (r == 0.0)                    // Special case
        return 0;
    if (r < 0.0)                     // I Am Not Yet a Numerical Analyst
		r = r + 4294967296.0;

	// Adjust because we're converting to INT
	if (r >= 2147483648.0)
		r = r - 4294967296.0;

	UINT32 hiword, loword;
	UINT32 shift, e, s, res;

	op_explode_double( r, hiword, loword );

	s = (hiword >> 31);
	e = ((hiword >> 20) & 0x7FF) - 1023;
	hiword = (hiword & 0x000FFFFF) | 0x00100000;

	OP_ASSERT( e < 32 );

	// Divide the significand by 2^(52-e).
	// The significand is the 20 low bits of hiword and all the bits of loword.
	shift = 52 - e;
	if (shift > 31)
		res = hiword >> (shift - 32);
	else
		res = (hiword << (32 - shift)) | (loword >> shift);

	// Adjust for the sign.
	if (s)
		res = ~res + 1;
	return (int)res;
#endif // INT_CAST_IS_ES262_COMPLIANT
}

UINT32 op_double2uint32( double x )
{
#ifdef INT_CAST_IS_ES262_COMPLIANT	// a tweak
	return (UINT32)x;
#else
	double q, r;

	// Step 2: discard boundary cases
	if (!op_isfinite(x) || x == 0.0)
		return 0;

	// Step 3: Adjust x
	if (x < 0.0)
		x = -op_floor(op_fabs(x));
	else
		x = op_floor(x);

	// Step 4: Compute r = x MOD 2^32
	if (0.0 <= x && x < 4294967296.0)
		r = x;
	else if (-4294967296.0 < x && x < 0.0)
		r = x + 4294967296.0;
	else {
		q = op_floor(x / 4294967296.0);
		r = x - (q * 4294967296.0);
	}

    if (r == 0.0)                      // Special case
        return 0;
	if (r < 0.0)                       // I Am Not Yet a Numerical Analyst
		r = r + 4294967296.0;

	UINT32 hiword, loword;
	UINT32 shift, e, res;

	op_explode_double(r, hiword, loword);

	e = ((hiword >> 20) & 0x7FF) - 1023;
	hiword = (hiword & 0x000FFFFF) | 0x00100000;

	OP_ASSERT( e < 32 );

	// Divide the significand by 2^(52-e).
	// The significand is the 20 low bits of hiword and all the bits of loword.
	shift = 52 - e;
	if (shift > 31)
		res = hiword >> (shift - 32);
	else
		res = (hiword << (32 - shift)) | (loword >> shift);

	return res;
#endif // INT_CAST_IS_ES262_COMPLIANT
}
