/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef STDLIB_DTOA_CONVERSION
/*
  Opera has two different dtoa implementations (both are
  provided by third party authors), the first one is written
  by David M. Gay (dmg), a solid library for producing a
  correct string for any double value. The second implementation
  was originally proposed by Florian Loitsch in [1]. This
  latter algorithm is faster than the original dmg code overall,
  considerably so on some inputs.

  Florian Loitsch presently works at Google and assisted
  the V8 team to integrate grisu into their code, but to make
  a general purpose library easier for others needing
  double<->string conversion routines, he has subsequenty
  packaged it up as a separate library,

     https://code.google.com/p/double-conversion/

  The Loitsch dtoa implementation is controlled by
  FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION, and is the default
  implementation.

  The feature defines DTOA_FLOITSCH_DOUBLE_CONVERSION and
  is a complete replacement for dmg, handling both the
  correct and minimal stringification of doubles and
  for converting a string to a double value.

  [1] http://florian.loitsch.com/publications/dtoa-pldi2010.pdf?attredirects=0&d=1
*/

#include "modules/stdlib/include/double_format.h"

#include "modules/stdlib/src/thirdparty_dtoa_dmg/double_format.h"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/double_format.h"

static inline BOOL
AsSpecialValue(char *buffer, double x)
{
	if (op_isnan(x))
	{
		op_strcpy(buffer, "NaN");
		return TRUE;
	}
	else if (op_isinf(x))
	{
		char *ptr = buffer;
		if (x < 0)
			*ptr++ = '-';
		op_strcpy(ptr, "Infinity");
		return TRUE;
	}
	else
		return FALSE;
}

/* static */ char *
OpDoubleFormat::ToString(char *b, double x)
{
    if (AsSpecialValue(b, x))
        return b;

    register int i, k;
    register char *s;
    int decpt = 0, j, sign = 0;
    char *b0, *s0, *se;

    b0 = b;
#ifdef IGNORE_ZERO_SIGN
    if (!x)
    {
		*b++ = '0';
		*b = 0;
		goto done;
    }
#endif
    s = s0 = stdlib_dtoa(x, 1, MaxPrecision, ROUND_BIAS_TO_EVEN, &decpt, &sign, &se);
    if (s == NULL)
		return NULL;
    if (sign)
		*b++ = '-';
#ifdef DTOA_DAVID_M_GAY
    OP_ASSERT( decpt < 9999 );
#endif // DTOA_DAVID_M_GAY
    if (decpt <= -6 || decpt > 21)
    {
		*b++ = *s++;
		if (*s)
		{
			*b++ = '.';
			while((*b = *s++) != 0)
				b++;
		}
		*b++ = 'e';
		/* sprintf(b, "%+.2d", decpt - 1); */
		if (--decpt < 0)
		{
			*b++ = '-';
			decpt = -decpt;
		}
		else
			*b++ = '+';
		for (j = 1, k = 1; 10*k <= decpt; j++, k *= 10)
			;
		for (;;)
		{
			i = decpt / k;
			*b++ = i + '0';
			if (--j <= 0)
				break;
			decpt -= i*k;
			decpt *= 10;
		}
		*b = 0;
    }
    else if (decpt <= 0)
    {
		/*if (sign)*/
		*b++ = '0';
		*b++ = '.';
		for (; decpt < 0; decpt++)
			*b++ = '0';
		while((*b++ = *s++) != 0)
			;
    }
    else
    {
		while((*b = *s++) != 0)
		{
			b++;
			if (--decpt == 0 && *s)
				*b++ = '.';
		}
		for (; decpt > 0; decpt--)
			*b++ = '0';
		*b = 0;
    }

    stdlib_freedtoa(s0);
    return b0;
}

/* Following variations on g_fmt hacked together by lth@opera.com */
static inline char
put( char*& b, char s, size_t& ndigit )
{
    if (ndigit > 0)
    {
		*b++ = s;
		--ndigit;
    }
    return s;
}

/* static */ char*
OpDoubleFormat::PrintfFormat(double x, char* b, size_t bufsiz, char fmt, int precision)
{
    int i = 0, k = 0, decpt = 0, j = 0, sign = 0;
    char *s = NULL, *b0 = b, *s0 = NULL, *se = NULL;

    OP_ASSERT( x >= 0.0 );
    OP_ASSERT( precision >= 0 );

    switch (fmt)
    {
    case 'f':
    case 'F':
		if (AsSpecialValue(b, x))
			return b;

		s = s0 = stdlib_dtoa(x, 3, precision, ROUND_BIAS_TO_EVEN, &decpt, &sign, &se);
		if (s == NULL)
			return NULL;
#ifdef DTOA_DAVID_M_GAY
		OP_ASSERT( decpt < 9999 );
#endif // DTOA_DAVID_M_GAY
		if (decpt <= 0)
		{
			put(b, '0', bufsiz);
			if (precision > 0)
			{
				char *pt = b;
				put(b, '.', bufsiz);
				for (; decpt < 0; decpt++)
					put(b, '0', bufsiz);
				while(put(b, *s++, bufsiz) != 0)
					;
				b--;
				bufsiz++;
				while (b - pt <= precision && bufsiz > 0)
					put(b, '0', bufsiz);
			}
		}
		else
		{
			char *pt = NULL;
			while(put(b, *s++, bufsiz) != 0)
			{
				if (--decpt == 0 && *s != 0)
				{
					pt = b;
					put(b, '.', bufsiz);
				}
			}
			b--;
			bufsiz++;
			for (; decpt > 0; decpt--)
				put(b, '0', bufsiz);
			if (pt == NULL && precision > 0)
			{
				pt=b;
				put(b, '.', bufsiz);
			}
			if (pt != NULL)
				while (b-pt <= precision && bufsiz > 0)
					put(b, '0', bufsiz);
		}
		b[-(bufsiz == 0)] = 0;
		break;

    case 'e':
    case 'E':
		if (AsSpecialValue(b, x))
			return b;

		s = s0 = stdlib_dtoa(x, 2, precision+1, ROUND_BIAS_TO_EVEN, &decpt, &sign, &se);
		if (s == NULL)
			return NULL;
#ifdef DTOA_DAVID_M_GAY
		OP_ASSERT( decpt < 9999 );
#endif // DTOA_DAVID_M_GAY
		put(b, *s++, bufsiz);
		if (*s || precision > 0)
		{
			put(b, '.', bufsiz);
			if (*s)
			{
				while(put(b, *s++, bufsiz) != 0)
					;
				--b;
				++bufsiz;
			}
			while (b-b0 < precision+2 && bufsiz > 0)
				put(b, '0', bufsiz);
		}

		put(b, fmt, bufsiz);
    format_exponent:
		/* sprintf(b, "%+.2d", decpt - 1); */
		if (--decpt < 0)
		{
			put(b, '-', bufsiz);
			decpt = -decpt;
		}
		else
			put(b, '+', bufsiz);
		for (j = 1, k = 1; 10*k <= decpt; j++, k *= 10)
			;
		if (j == 1)
			put(b, '0', bufsiz);	// At least two digits of exponent
		for (;;)
		{
			i = decpt / k;
			put(b, i + '0', bufsiz);
			if (--j <= 0)
				break;
			decpt -= i*k;
			decpt *= 10;
		}
		b[-(bufsiz == 0)] = 0;
		break;

    case 'g':
    case 'G':
		if (AsSpecialValue(b, x))
			return b;

		s = s0 = stdlib_dtoa(x, 2, precision, ROUND_BIAS_TO_EVEN, &decpt, &sign, &se);
		if (s == NULL)
			return NULL;
#ifdef DTOA_DAVID_M_GAY
		OP_ASSERT( decpt < 9999 );
#endif // DTOA_DAVID_M_GAY

		char *last_leading_zero = NULL;
#ifndef DTOA_DAVID_M_GAY
		if (*s)
		{
			/* Trailing zeroes are truncated with %g/%G. Remove anything
			   left behind by the dtoa implementation. DMG is documented
			   as suppressing them, so an extra pass is unnecessary. */
			char *ptr = s + 1;
			while (*ptr)
			{
				if (last_leading_zero && *ptr != '0')
					last_leading_zero = NULL;
				else if (*ptr == '0' && !last_leading_zero)
					last_leading_zero = ptr;
				ptr++;
			}
		}
#endif // !DTOA_DAVID_M_GAY
		if (decpt <= -4 || decpt > precision)
		{
			put(b, *s++, bufsiz);
			if (*s && s != last_leading_zero)
			{
				put(b, '.', bufsiz);
				if (*s)
				{
					while (s != last_leading_zero)
						if (put(b, *s++, bufsiz) == 0)
						{
							--b;
							++bufsiz;
							break;
						}
				}
			}
			put(b, fmt == 'g' ? 'e' : 'E', bufsiz);
			goto format_exponent;
		}
		else if (decpt <= 0)
		{
			put(b, '0', bufsiz);
			put(b, '.', bufsiz);
			for (; decpt < 0; decpt++)
				put(b, '0', bufsiz);
			while (put(b, *s++, bufsiz) && s != last_leading_zero + 1)
				;
			b--;
			++bufsiz;
		}
		else
		{
			if (precision > 0)
			{
				while (put(b, *s++, bufsiz) && s != last_leading_zero + 1)
					if (--decpt == 0 && *s && s != last_leading_zero)
						put(b, '.', bufsiz);
				--b;
				++bufsiz;
			}
			for (; decpt > 0; decpt--)
				put(b, '0', bufsiz);
		}
		b[-(bufsiz == 0)] = 0;
		break;
    }

    stdlib_freedtoa(s0);
    return b0;
}

char*
OpDoubleFormat::ToExponential(char *b, double x, int precision, RoundingBias bias)
{
    if (AsSpecialValue(b, x))
        return b;

    register int i, k;
    register char *s;
    int decpt = 0, j, sign = 0;
    char *b0, *bret, *s0, *se;

    bret = b0 = b;
#ifdef IGNORE_ZERO_SIGN
    if (!x)
    {
		*b++ = '0';
		*b = 0;
		goto done;
    }
#endif
    if (precision >= 0)
		s = s0 = stdlib_dtoa(x, 2, precision + 1, bias, &decpt, &sign, &se);
    else
		s = s0 = stdlib_dtoa(x, 1, MaxPrecision, bias, &decpt, &sign, &se);

    if (s == NULL)
		return NULL;
    if (sign && x != 0.0)
    {
		*b++ = '-';
		b0 = b;
    }
#ifdef DTOA_DAVID_M_GAY
    OP_ASSERT( decpt < 9999 );
#endif // DTOA_DAVID_M_GAY
    *b++ = *s++;
    if (*s || precision > 0)
    {
		*b++ = '.';
		if (*s)
			while ((*b = *s++) != 0)
				b++;
		// pad with zeros until we're reached the desired precision
		while (b - b0 < precision + 2)
			*b++ = '0';
    }

    *b++ = 'e';
    /* sprintf(b, "%+.2d", decpt - 1); */
    if (--decpt < 0)
    {
		*b++ = '-';
		decpt = -decpt;
    }
    else
		*b++ = '+';
    for (j = 1, k = 1; 10*k <= decpt; j++, k *= 10)
		;
    for (;;)
    {
		i = decpt / k;
		*b++ = i + '0';
		if (--j <= 0)
			break;
		decpt -= i*k;
		decpt *= 10;
    }
    *b = 0;

    stdlib_freedtoa(s0);
    return bret;
}

/* static */ char *
OpDoubleFormat::ToFixed(char *b, double x, int precision, size_t bufsiz, RoundingBias bias)
{
    if (AsSpecialValue(b, x))
        return b;

    register char *s;
    int decpt = 0, sign = 0;
    char *bret, *s0, *se;

    bret = b;
#ifdef IGNORE_ZERO_SIGN
    if (!x)
    {
		put(b, '0', bufsiz);
		goto done;
    }
#endif
    s = s0 = stdlib_dtoa(x, 3, precision, bias, &decpt, &sign, &se);
    if (s == NULL)
		return NULL;
    if (sign && x != 0.0)
		put(b, '-', bufsiz);
#ifdef DTOA_DAVID_M_GAY
    OP_ASSERT( decpt < 9999 );
#endif // DTOA_DAVID_M_GAY
    if (decpt <= 0)
    {
		put(b, '0', bufsiz);
		if (precision > 0)
		{
			char *pt = b;
			put(b, '.', bufsiz);
			for (; decpt < 0; decpt++)
				put(b, '0', bufsiz);
			while (put(b, *s++, bufsiz) != 0)
				;
			b--;
			++bufsiz;
			while (b-pt <= precision && bufsiz > 0)
				put(b, '0', bufsiz);
		}
    }
    else
    {
		char *pt = 0;
		while (put(b, *s++, bufsiz) != 0)
		{
			if (--decpt == 0 && *s)
			{
				pt = b;
				put(b, '.', bufsiz);
			}
		}
	--b;
	++bufsiz;
	for (; decpt > 0; decpt--)
	    put(b, '0', bufsiz);
	if (!pt && precision > 0)
	{
	    pt=b;
	    put(b, '.', bufsiz);
	}
	if (pt)
	    while (b-pt <= precision && bufsiz > 0)
		put(b, '0', bufsiz);
    }

    b[-(bufsiz == 0)] = 0;
    stdlib_freedtoa(s0);
    return bret;
}

/* static */ char*
OpDoubleFormat::ToPrecision(char *b, double x, int precision, size_t bufsiz, RoundingBias bias)
{
    if (AsSpecialValue(b, x))
        return b;

    register int i, k;
    register char *s;
    int decpt = 0, j, sign = 0;
    char *b0, *bret, *s0, *se;

    OP_ASSERT( precision > 0 );

    bret = b0 = b;
#ifdef IGNORE_ZERO_SIGN
    if (!x)
    {
		put(b, '0', bufsiz);
		goto done;
    }
#endif
    s = s0 = stdlib_dtoa(x, 2, precision, bias, &decpt, &sign, &se);
    if (s == NULL)
		return NULL;
    if (sign && x != 0.0)
    {
		put(b, '-', bufsiz);
		b0 = b;
    }
#ifdef DTOA_DAVID_M_GAY
    OP_ASSERT( decpt < 9999 );
#endif // DTOA_DAVID_M_GAY
    if (decpt <= -6 || decpt > precision)
    {
		put(b, *s++, bufsiz);
		if (*s)
		{
			put(b, '.', bufsiz);
			if (*s)
			{
				while (put(b, *s++, bufsiz) != 0)
					;
				--b;
				++bufsiz;
			}
			while (b-b0 < precision+1 && bufsiz > 0)
				put(b, '0', bufsiz);
		}
		put(b, 'e', bufsiz);
		/* sprintf(b, "%+.2d", decpt - 1); */
		if (--decpt < 0)
		{
			put(b, '-', bufsiz);
			decpt = -decpt;
		}
		else
			put(b, '+', bufsiz);
		for (j = 1, k = 1; 10*k <= decpt; j++, k *= 10)
			;
		for (;;)
		{
			i = decpt / k;
			put(b, i + '0', bufsiz);
			if (--j <= 0)
				break;
			decpt -= i*k;
			decpt *= 10;
		}
    }
    else if (decpt <= 0)
    {
		put(b, '0', bufsiz);
		put(b, '.', bufsiz);
		for (; decpt < 0; decpt++)
		{
			put( b, '0', bufsiz );
			precision++;
		}
		while (put(b, *s++, bufsiz) != 0)
			;
		b--;
		++bufsiz;
		/* In decimal fixed notation: 'precision' significant digits. */
		while (b-b0 < precision+2 && bufsiz > 0)
			put(b, '0', bufsiz);
    }
    else
    {
		char *pt = 0;
		if (precision > 0)
		{
			while (put(b, *s++, bufsiz))
			{
				if (--decpt == 0 && *s)
				{
					pt = b;
					put(b, '.', bufsiz);
				}
			}
			--b;
			++bufsiz;
		}
		for (; decpt > 0; decpt--)
			put(b, '0', bufsiz);
		if (!pt && b-b0 < precision)
		{
			pt=b;
			put(b, '.', bufsiz);
		}
		if (pt)
			while (b-b0 < precision+1 && bufsiz > 0)
				put(b, '0', bufsiz);
    }

    b[-(bufsiz == 0)] = 0;
    stdlib_freedtoa(s0);
    return bret;
}

#endif // STDLIB_DTOA_CONVERSION
