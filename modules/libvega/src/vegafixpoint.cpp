/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegafixpoint.h"

#ifndef VEGA_USE_FLOATS

VEGA_FIX VEGA_FIXSQR(VEGA_FIX x)
{
	x = op_abs(x);

	unsigned x_hi = VG_HI16(x);
	unsigned x_lo = VG_LO16(x);

	unsigned tmp1 = x_lo * x_lo;
	unsigned tmpc = x_hi * x_lo;

	unsigned tmp2 = tmpc + VG_HI16(tmp1);
	unsigned tmp3 = tmpc + VG_LO16(tmp2);

	unsigned res_hi = x_hi * x_hi + VG_HI16(tmp2) + VG_HI16(tmp3);
	unsigned res_lo = (tmp3 << 16) + VG_LO16(tmp1);

	return (res_hi << (32 - VEGA_FIX_DECBITS)) | (res_lo >> VEGA_FIX_DECBITS);
}

VEGA_FIX VEGA_FIXDIV(VEGA_FIX x, VEGA_FIX y)
{
	BOOL positive = TRUE;
	if (x < 0)
	{
		x = -x;
		positive = FALSE;
	}
	if (y < 0)
	{
		y = -y;
		positive = !positive;
	}

	INT32 div_res = (INT32)int64_divu32(int64_shl(int64_load32(x), VEGA_FIX_DECBITS), y);

	if (!positive)
		div_res = -div_res;

	return (VEGA_FIX)div_res;
}

VEGA_DBLFIX VEGA_DBLFIXDIV(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	BOOL neg = FALSE;
	if (int64_isneg(a))
	{
		a = int64_neg(a);
		neg = TRUE;
	}
	if (int64_isneg(b))
	{
		b = int64_neg(b);
		neg = !neg;
	}

	VEGA_INT64 r = int128_divu64(int64_shr(a, 64-2*VEGA_FIX_DECBITS),
								 int64_shl(a, 2*VEGA_FIX_DECBITS),
								 b);

	if (neg)
		r = int64_neg(r);

	return r;
}

#if (VEGA_FIX_DECBITS & 1) == 1
#error "sqrt()-algorithm needs to be fixed for fixed point formats with odd bits of precision"
#endif

VEGA_FIX VEGA_FIXSQRT(VEGA_FIX x)
{
	if (x <= 0)
		return 0;

	UINT32 root = 0;
	UINT32 rem_hi = 0;
	UINT32 rem_lo = x;
	UINT32 count = 15 + (VEGA_FIX_DECBITS / 2); /* number of iterations */

	do {
		rem_hi = (rem_hi << 2) | (rem_lo >> 30);
		rem_lo <<= 2;
		root <<= 1;

		UINT32 test_div = 2*root + 1;
		if (rem_hi >= test_div)
		{
			rem_hi -= test_div;
			root++;
		}
	} while (count-- != 0);

	return (VEGA_FIX)root;
}

VEGA_DBLFIX VEGA_DBLFIXSQRT(VEGA_DBLFIX x)
{
	return int64_shl(int64_load32(int64_isqrt(x)), VEGA_FIX_DECBITS);
}

// Calculate length of the vector <vx, vy>
VEGA_FIX VEGA_VECLENGTH(VEGA_FIX vx, VEGA_FIX vy)
{
	// sqrt(|vx| * |vx| + |vy| * |vy|)
	return (VEGA_FIX)int64_isqrt(int64_add(int64_muls(vx, vx), int64_muls(vy, vy)));
}

VEGA_FIX VEGA_VECDOTSIGN(VEGA_FIX vx1, VEGA_FIX vy1, VEGA_FIX vx2, VEGA_FIX vy2)
{
	return int64_sign(int64_add(int64_muls(vx1, vx2), int64_muls(vy1, vy2)));
}

// Hardy's approximation
//
// cos(pi*x/2) = 1 - x^2 / (x + (1 - x) * sqrt((2 - x)/3))
//
VEGA_FIX VEGA_FIXCOS(VEGA_FIX x)
{
	// Clip to [0, 360)
	if (x < 0)
	{
		unsigned int q = 1 + VEGA_TRUNCFIXTOINT(VEGA_ABS(x)) / 360;
		x += VEGA_INTTOFIX(q * 360);
	}
	else if (x >= VEGA_INTTOFIX(360))
	{
		unsigned int q = VEGA_TRUNCFIXTOINT(x) / 360;
		x -= VEGA_INTTOFIX(q * 360);
	}

	if (x > VEGA_INTTOFIX(180))
		return -VEGA_FIXCOS(x-VEGA_INTTOFIX(180));

	if (x > VEGA_INTTOFIX(90))
		return -VEGA_FIXCOS(VEGA_INTTOFIX(180)-x);

	// x * (pi/180) / (pi/2) -> x/90
	x /= 90;

	VEGA_FIX sqr = (VEGA_INTTOFIX(2)-x)/3;
	sqr = VEGA_FIXSQRT(sqr);
	VEGA_FIX div = VEGA_INTTOFIX(1)-x;
	div = VEGA_FIXMUL(div, sqr);
	div += x;
	VEGA_FIX cosx = VEGA_FIXSQR(x);
	cosx = VEGA_FIXDIV(cosx, div);

	return VEGA_INTTOFIX(1)-cosx;
}

VEGA_FIX VEGA_FIXACOS(VEGA_FIX x)
{
	BOOL neg = FALSE;
	if (x < 0)
	{
		neg = TRUE;
		x = -x;
	}

	BOOL asin = FALSE;
	if (x > VEGA_FIXDIV2(VEGA_INTTOFIX(1)))
	{
		asin = TRUE;
		x = VEGA_FIXSQRT(VEGA_INTTOFIX(1)-VEGA_FIXSQR(x));
	}

	VEGA_FIX angle = x;
	VEGA_FIX curfrac = VEGA_INTTOFIX(1);
	VEGA_FIX curx = x;
	VEGA_FIX sqr_x = VEGA_FIXSQR(x);
	for (int i = 0; i < 10; ++i)
	{
		curx = VEGA_FIXMUL(curx, sqr_x);

		curfrac = VEGA_FIXMUL(curfrac, VEGA_INTTOFIX(i*2+1)/(i*2+2));

		angle += VEGA_FIXMUL(curx / (i*2+3), curfrac);
	}

	angle = VEGA_FIXDIV(angle, VEGA_FIX_PI_OVER_180);

	if (!asin)
		angle = VEGA_INTTOFIX(90)-angle;

	if (neg)
		angle = VEGA_INTTOFIX(180)-angle;

	return angle;
}

/* Approximation: 6 * (x - 1) / (x + 1 + 4 * (x^0.5)) */
VEGA_FIX VEGA_FIXLOG(VEGA_FIX x)
{
	return VEGA_FIXDIV(6*(x - VEGA_INTTOFIX(1)),
					   x + VEGA_INTTOFIX(1) + 4*VEGA_FIXSQRT(x));
}

/* MacLaurin series: exp(x) = 1 + x/1! + x^2/2! + x^3/3! + ... */
/* Expecting 0 <= x < 1 */
static inline VEGA_FIX VEGA_FIXEXP_FRAC(VEGA_FIX x)
{
	if (x == 0)
		return VEGA_INTTOFIX(1);

	VEGA_FIX res = VEGA_INTTOFIX(1) + x;
	VEGA_FIX x_to_y = VEGA_FIXSQR(x);
	res += VEGA_FIXDIV2(x_to_y);
	int fac = 2;

	for (int i = 3; i <= 7; i++)
	{
		x_to_y = VEGA_FIXMUL(x_to_y, x);
		fac *= i;
		res += x_to_y / fac;
	}

	return res;
}

static inline VEGA_FIX VEGA_INTPOW(VEGA_FIX x, int y)
{
	if (y == 0)
		return VEGA_INTTOFIX(1);

	BOOL sign_y = (y < 0);
	int abs_y = sign_y ? -y : y;
	VEGA_FIX res = x;
	for (int i = 1; i < abs_y; i++)
		res = VEGA_FIXMUL(res, x);

	if (sign_y)
		res = VEGA_FIXDIV(VEGA_INTTOFIX(1), res);

	return res;
}

VEGA_FIX VEGA_FIXEXP(VEGA_FIX x)
{
	VEGA_FIX int_x = VEGA_FLOOR(x);
	VEGA_FIX frac_x = x - int_x;

	VEGA_FIX e_int_x = VEGA_INTPOW(VEGA_FIX_E, VEGA_FIXTOINT(int_x)); /* e^(int(x)) */
	VEGA_FIX factor = VEGA_FIXEXP_FRAC(frac_x); /* e^(frac(x)) */

	return VEGA_FIXMUL(e_int_x, factor);
}

/* x^y = e^(y * log/ln(x)) */
VEGA_FIX VEGA_FIXPOW(VEGA_FIX x, VEGA_FIX y)
{
	return VEGA_FIXEXP(VEGA_FIXMUL(y, VEGA_FIXLOG(x)));
}

#endif // !VEGA_USE_FLOATS
#endif // VEGA_SUPPORT
