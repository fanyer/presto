/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT

#include "modules/libvega/src/vegainteger.h"

#ifndef VEGA_USE_FLOATS

#if 1//!defined(HAVE_INT64) || defined(VEGA_USE_OWN_INT64)

VG_INT64 int64_mulu(UINT32 a, UINT32 b)
{
	unsigned a_hi = VG_HI16(a);
	unsigned a_lo = VG_LO16(a);

	unsigned b_hi = VG_HI16(b);
	unsigned b_lo = VG_LO16(b);

	unsigned tmp1 = a_lo * b_lo;
	unsigned w3 = VG_LO16(tmp1);
	unsigned k = VG_HI16(tmp1);

	unsigned tmp2 = a_hi * b_lo + k;
	unsigned w2 = VG_LO16(tmp2);
	unsigned w1 = VG_HI16(tmp2);

	unsigned tmp3 = a_lo * b_hi + w2;
	k = VG_HI16(tmp3);

	VG_INT64 res;
	res.hi = a_hi * b_hi + w1 + k;
	res.lo = (tmp3 << 16) + w3;

	return res;
}

VG_INT64 int64_muls(INT32 x, INT32 y)
{
	/* multiply two 32-bit numbers to a 64-bit number */
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

	VG_INT64 res = int64_mulu(x, y);

	if (!positive)
		res = int64_neg(res);

	return res;
}

// number of of leading zero bits
static int nlz(UINT32 x)
{
#if 0 // use smaller constants
	int n = 32;
	int c = 16;

	UINT32 y;
#define NLZ_STEP								\
	y = x >> c;									\
	if (y != 0) { n -= c; x = y; }				\
	c >>= 1

	NLZ_STEP; // 16
	NLZ_STEP; // 8
	NLZ_STEP; // 4
	NLZ_STEP; // 2
	NLZ_STEP; // 1
#undef NLZ_STEP

	return n - x;
#else
	if (x == 0)
		return 32;

	int n = 0;
	if (x <= 0xffff) { n += 16; x <<= 16; }
	if (x <= 0xffffff) { n += 8; x <<= 8; }
	if (x <= 0xfffffff) { n += 4; x <<= 4; }
	if (x <= 0x3fffffff) { n += 2; x <<= 2; }
	if (x <= 0x7fffffff) { n += 1; }
	return n;
#endif
}

UINT32 int64_divu32(VG_INT64 n, UINT32 d)
{
	int shift = nlz(d);
	d <<= shift;

	unsigned d_hi = VG_HI16(d);
	unsigned d_lo = VG_LO16(d);

	n.hi <<= shift;
	n.hi |= (n.lo >> 32 - shift) & (-shift >> 31);

	n.lo <<= shift;

	unsigned lo_hi = VG_HI16(n.lo);
	unsigned lo_lo = VG_LO16(n.lo);

	unsigned quot_hi = n.hi / d_hi;
	unsigned rem = n.hi - quot_hi * d_hi;

	while (quot_hi >= 0x10000 ||
		   quot_hi * d_lo > (rem << 16) + lo_hi)
	{
		quot_hi--;
		rem += d_hi;

		if (rem >= 0x10000)
			break;
	}

	unsigned hl = (n.hi << 16) + lo_hi - quot_hi * d;

	unsigned quot_lo = hl / d_hi;
	rem = hl - quot_lo * d_hi;

	while (quot_lo >= 0x10000 ||
		   quot_lo * d_lo > (rem << 16) + lo_lo)
	{
		quot_lo--;
		rem += d_hi;

		if (rem >= 0x10000)
			break;
	}

	return (quot_hi << 16) + quot_lo;
}

INT32 int64_muldiv(INT32 a, INT32 b, INT32 c)
{
	BOOL positive = TRUE;
	if (a < 0)
	{
		a = -a;
		positive = FALSE;
	}
	if (b < 0)
	{
		b = -b;
		positive = !positive;
	}
	if (c < 0)
	{
		c = -c;
		positive = !positive;
	}

	// a [32 bit] * b [32 bit] / c [32 bit] => res [32 bit]
	INT32 div_res = (INT32)int64_divu32(int64_mulu(a, b), c);

	if (!positive)
		div_res = -div_res;

	return div_res;
}

#endif // !HAVE_INT64 || VEGA_USE_OWN_INT64

// [a:b] / c
VEGA_INT64 int128_divu64(VEGA_INT64 a, VEGA_INT64 b, VEGA_INT64 c)
{
	for (unsigned i = 0; i < 64; ++i)
	{
		UINT32 t = (INT32)a.hi >> 31; // ~0 || 0

		// a:b << 1
		a = int64_shl(a, 1);
		a = int64_or_low(a, b.hi >> 31);

		b = int64_shl(b, 1);

		if (int64_gteq(int64_or(a, t, t), c))
		{
			a = int64_sub(a, c);
			b = int64_or_low(b, 1);
		}
	}
	return b;
}

INT32 int64_isqrt(VG_INT64 v)
{
	UINT32 root = 0;
	UINT32 rem_hi = 0;
	UINT32 rem_lo1 = INT64_HIU32(v);
	UINT32 rem_lo0 = INT64_LOU32(v);
	UINT32 count = 31; /* number of iterations */

	do {
		rem_hi = (rem_hi << 2) | (rem_lo1 >> 30);
		rem_lo1 = (rem_lo1 << 2) | (rem_lo0 >> 30);
		rem_lo0 <<= 2;
		root <<= 1;
		UINT32 test_div = 2*root + 1;
		if (rem_hi >= test_div)
		{
			rem_hi -= test_div;
			root++;
		}
	} while (count-- != 0);

	return (INT32)root;
}

#endif // !VEGA_USE_FLOATS
#endif // VEGA_SUPPORT
