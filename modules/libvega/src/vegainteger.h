/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAINTEGER_H
#define VEGAINTEGER_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegaconfig.h"

#ifndef VEGA_USE_FLOATS

#define VG_LO16(v) ((v) & 0xFFFF)
#define VG_HI16(v) ((v) >> 16)

#if 0//defined(HAVE_INT64) && !defined(VEGA_USE_OWN_INT64)
// Using INT64 will, in most cases, be overkill (performing full 64 *
// 64 operations, which is most likely just passed through some
// library helper)
typedef INT64 VG_INT64;

#define INT64_HIU32(v) (UINT32)((v) >> 32)
#define INT64_LOU32(v) (UINT32)((v))

#define INT64_TO_DBLFIX(v) (v)
#define DBLFIX_TO_INT64(v) (v)

static inline int int64_sign(VG_INT64 v) { return (v < 0) ? -1 : v != 0; }

static inline BOOL int64_isneg(VG_INT64 v) { return (v < 0); }
static inline VG_INT64 int64_neg(VG_INT64 v) { return -v; }

static inline VG_INT64 int64_add(VG_INT64 a, VG_INT64 b) { return a + b; }
static inline VG_INT64 int64_sub(VG_INT64 a, VG_INT64 b) { return a - b; }

static inline INT32 int64_shr32(VG_INT64 v, const unsigned s) { return (INT32)(v >> s); }
static inline VG_INT64 int64_shl(VG_INT64 v, const unsigned s) { return v << s; }
static inline VG_INT64 int64_shr(VG_INT64 v, const unsigned s)
{
#ifdef HAVE_UINT64
	return (VG_INT64)((UINT64)v >> s);
#else
	return (v >> s) & (((INT64)1 << s) - 1);
#endif // HAVE_UINT64
}
static inline VG_INT64 int64_sar(VG_INT64 v, const unsigned s) { return v >> s; }

static inline BOOL int64_eq(VG_INT64 a, VG_INT64 b)
{
	return a == b;
}

static inline BOOL int64_lt(VG_INT64 a, VG_INT64 b)
{
	return a < b;
}

static inline BOOL int64_gt(VG_INT64 a, VG_INT64 b)
{
	return a > b;
}

static inline BOOL int64_lteq(VG_INT64 a, VG_INT64 b)
{
	return a <= b;
}

static inline BOOL int64_gteq(VG_INT64 a, VG_INT64 b)
{
	return a >= b;
}

// a | (0:b)
static inline VG_INT64 int64_or_low(VG_INT64 a, UINT32 b)
{
	return a | b;
}

// a | (b:c)
static inline VG_INT64 int64_or(VG_INT64 a, UINT32 b, UINT32 c)
{
	return a | (b << 32) | c;
}

static inline VG_INT64 int64_load32(INT32 v)
{
	return (VG_INT64)v;
}

static inline VG_INT64 int64_mulu(UINT32 x, UINT32 y) { return (VG_INT64)x * y; }
static inline VG_INT64 int64_muls(INT32 x, INT32 y) { return (VG_INT64)x * y; }

static inline UINT32 int64_divu32(VG_INT64 n, UINT32 d) { return (UINT32)(n / d); }

static inline INT32 int64_muldiv(INT32 a, INT32 b, INT32 c) { return (INT32)((VG_INT64)a * b / c); }

#else

struct VEGA_INT64
{
	UINT32 hi;
	UINT32 lo;
};

typedef VEGA_INT64 VG_INT64;

#define INT64_HIU32(v) (v).hi
#define INT64_LOU32(v) (v).lo

#if 0
static inline VEGA_DBLFIX INT64_TO_DBLFIX(VG_INT64 v)
{
	return ((INT64)v.hi << 32) | v.lo;
}

static inline VG_INT64 DBLFIX_TO_INT64(VEGA_DBLFIX v)
{
	VG_INT64 r;
	r.hi = INT64_HIU32(v);
	r.lo = INT64_LOU32(v);
	return r;
}
#endif // 0

static inline BOOL int64_isneg(VG_INT64 v)
{
	return ((INT32)v.hi < 0);
}

static inline int int64_sign(VG_INT64 v)
{
	if (int64_isneg(v))
		return -1;

	return (v.hi | v.lo) != 0;
}

static inline VG_INT64 int64_neg(VG_INT64 v)
{
	v.hi = ~v.hi;
	v.lo = ~v.lo;
	if (v.lo == ~0u)
		++v.hi;
	++v.lo;
	return v;
}

static inline VG_INT64 int64_add(VG_INT64 a, VG_INT64 b)
{
	a.hi += b.hi;
	a.lo += b.lo;
	if (a.lo < b.lo)
		a.hi++;
	return a;
}

static inline VG_INT64 int64_sub(VG_INT64 a, VG_INT64 b)
{
	return int64_add(a, int64_neg(b));
}

static inline INT32 int64_shr32(VG_INT64 v, const unsigned s)
{
	return (v.hi << (32 - s)) | (v.lo >> s);
}

static inline VG_INT64 int64_shl(VG_INT64 v, const unsigned s)
{
	if (s >= 32)
	{
		v.hi = v.lo << (s & 31);
		v.lo = 0;
	}
	else
	{
		v.hi = (v.hi << s) | (v.lo >> (32 - s));
		v.lo <<= s;
	}
	return v;
}

static inline VG_INT64 int64_shr(VG_INT64 v, const unsigned s)
{
	if (s >= 32)
	{
		v.lo = v.hi >> (s & 31);
		v.hi = 0;
	}
	else
	{
		v.lo = (v.hi << (32 - s)) | (v.lo >> s);
		v.hi >>= s;
	}
	return v;
}

static inline VG_INT64 int64_sar(VG_INT64 v, const unsigned s)
{
	if (s >= 32)
	{
		v.lo = (INT32)v.hi >> (s & 31);
		v.hi = (INT32)v.hi >> 31;
	}
	else
	{
		v.lo = (v.hi << (32 - s)) | (v.lo >> s);
		v.hi = ((INT32)v.hi >> s);
	}
	return v;
}

static inline BOOL int64_eq(VG_INT64 a, VG_INT64 b)
{
	return a.hi == b.hi && a.lo == b.lo;
}

// Below comparisons use:
// a <signed cmp> b = (a <unsigned cmp> b) ^ signbit(a) ^ signbit(b)

// a < b
static inline BOOL int64_lt(VG_INT64 a, VG_INT64 b)
{
	return ((a.hi ^ b.hi) >> 31) ^
		(a.hi < b.hi || a.hi == b.hi && a.lo < b.lo);
}

static inline BOOL int64_gt(VG_INT64 a, VG_INT64 b)
{
	return ((a.hi ^ b.hi) >> 31) ^
		(a.hi > b.hi || a.hi == b.hi && a.lo > b.lo);
}

// a <= b
static inline BOOL int64_lteq(VG_INT64 a, VG_INT64 b)
{
	return ((a.hi ^ b.hi) >> 31) ^
		(a.hi < b.hi || a.hi == b.hi && a.lo <= b.lo);
}

// a >= b
static inline BOOL int64_gteq(VG_INT64 a, VG_INT64 b)
{
	return ((a.hi ^ b.hi) >> 31) ^
		(a.hi > b.hi || a.hi == b.hi && a.lo >= b.lo);
}

// a | (0:b)
static inline VG_INT64 int64_or_low(VG_INT64 a, UINT32 b)
{
	a.lo |= b;
	return a;
}

// a | (b:c)
static inline VG_INT64 int64_or(VG_INT64 a, UINT32 b, UINT32 c)
{
	a.hi |= b;
	a.lo |= c;
	return a;
}

static inline VG_INT64 int64_load32(INT32 v)
{
	VG_INT64 a;
	a.hi = v >> 31;
	a.lo = v;
	return a;
}

VG_INT64 int64_mulu(UINT32 a, UINT32 b);
VG_INT64 int64_muls(INT32 x, INT32 y);

UINT32 int64_divu32(VG_INT64 n, UINT32 d);

INT32 int64_muldiv(INT32 a, INT32 b, INT32 c);

#endif // HAVE_INT64 && !VEGA_USE_OWN_INT64

VEGA_INT64 int128_divu64(VEGA_INT64 a, VEGA_INT64 b, VEGA_INT64 c);

INT32 int64_isqrt(VG_INT64 v);

#endif // !VEGA_USE_FLOATS
#endif // VEGA_SUPPORT
#endif // VEGAINTEGER_H
