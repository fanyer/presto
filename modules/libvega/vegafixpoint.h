/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGA_H
#define VEGA_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vegaconfig.h"

#ifdef VEGA_USE_FLOATS

typedef float VEGA_FIX;
typedef double VEGA_DBLFIX;

#define VEGA_FIXTOFLT(x) (x)
#define VEGA_FLTTOFIX(x) ((VEGA_FIX) (x))

#define VEGA_DBLFIXTOFIX(v) ((VEGA_FIX)(v))
#define VEGA_FIXTODBLFIX(v) ((VEGA_DBLFIX)(v))

#define VEGA_DBLFIXADD(a,b) ((a)+(b))
#define VEGA_DBLFIXSUB(a,b) ((a)-(b))

#define VEGA_DBLFIXDIV(a,b) ((VEGA_DBLFIX)(a)/(b))

#define VEGA_DBLFIXNEG(a) (-(a))
static inline int VEGA_DBLFIXSIGN(VEGA_DBLFIX a)
{
	return (a < 0) ? -1 : (a > 0);
}
#define VEGA_DBLFIXEQ(a,b) ((a) == (b))
#define VEGA_DBLFIXLT(a,b) ((a) < (b))
#define VEGA_DBLFIXGT(a,b) ((a) > (b))
#define VEGA_DBLFIXLTEQ(a,b) ((a) <= (b))
#define VEGA_DBLFIXGTEQ(a,b) ((a) >= (b))

#define VEGA_DBLFIXSQRT(a) (op_sqrt(a))

#define VEGA_INTTODBLFIX(a) ((double)(a))

static const VEGA_FIX VEGA_INFINITY = 10000.0f;
static const VEGA_FIX VEGA_EPSILON = 1.19209e-07f; // std::numeric_limits<float>::epsilon()

#define VEGA_FLOOR(x) ((float)op_floor((double)(x)))
#define VEGA_CEIL(x) ((float)op_ceil((double)(x)))

#define VEGA_FIXTOINT(x) ((int)((x)+0.5f))
#define VEGA_INTTOFIX(x) ((float)(x))
#define VEGA_TRUNCFIX(x) ((float)((int)(x)))
#define VEGA_TRUNCFIXTOINT(x) ((int)(x))
// Scale with (1 << ss)
#define VEGA_FIXTOSCALEDINT(x,ss) ((int)(((x)*(1 << (ss)))+0.5f))
#define VEGA_FIXTOSCALEDINT_T(x,ss) ((int)((x)*(1 << (ss))))

#define VEGA_FIXMUL(x,y) ((x)*(y))
#define VEGA_FIXMUL_DBL(x,y) ((VEGA_DBLFIX)(x)*(VEGA_DBLFIX)(y))

#define VEGA_FIXDIV(x,y) ((x)/(y))

#define VEGA_FIXMULDIV(x,y,z) ((x)*(y)/(z))

#define VEGA_FIXMUL2(x) ((x)*2)
#define VEGA_FIXDIV2(x) ((x)*0.5f)

#define VEGA_FIX_PI  3.14159265f
#define VEGA_FIX_2PI 6.28318531f

#define VEGA_FIXSIN(x) ((float)op_sin((double)((x)*(VEGA_FIX_PI/180.f))))
#define VEGA_FIXCOS(x) ((float)op_cos((double)((x)*(VEGA_FIX_PI/180.f))))

#define VEGA_FIXACOS(x) (180.f*(float)op_acos((double)(x))/VEGA_FIX_PI)

#define VEGA_FIXSQR(x) ((x)*(x))
#define VEGA_FIXSQRT(x) ((float)op_sqrt((double)(x)))

#define VEGA_FIXPOW(x,y) ((float)op_pow((double)(x),(double)(y)))

#define VEGA_FIXLOG(x) ((float)op_log((double)(x)))
#define VEGA_FIXEXP(x) ((float)op_exp((double)(x)))

#define VEGA_ABS(x) ((float)op_fabs((double)(x)))

#define VEGA_VECLENGTH(x, y) VEGA_FIXSQRT(VEGA_FIXSQR((x))+VEGA_FIXSQR((y)))
#define VEGA_VECDOTSIGN(vx1,vy1,vx2,vy2) (VEGA_FIXMUL((vx1),(vx2))+VEGA_FIXMUL((vy1),(vy2)))

#else // VEGA_USE_FLOATS

#include "modules/libvega/src/vegainteger.h"

#define VEGA_FIX_DECBITS 12
#define VEGA_FIX_DECMASK 0xfff

typedef INT32 VEGA_FIX;
typedef VG_INT64 VEGA_DBLFIX;

#define VEGA_FIXTOFLT(x) ((float)(x) / (float)(1<<VEGA_FIX_DECBITS))
#define VEGA_FLTTOFIX(x) ((VEGA_FIX)(((x)*(1 << VEGA_FIX_DECBITS))+.5f))

static inline VEGA_FIX VEGA_DBLFIXTOFIX(VEGA_DBLFIX v)
{
	return (VEGA_FIX)int64_shr32(v, VEGA_FIX_DECBITS);
}
static inline VEGA_DBLFIX VEGA_FIXTODBLFIX(VEGA_FIX v)
{
	return int64_shl(int64_load32(v), VEGA_FIX_DECBITS);
}

static const VEGA_FIX VEGA_INFINITY = ~(1<<31);
static const VEGA_FIX VEGA_EPSILON = 1;

#define VEGA_FLOOR(x) ((x)&(~VEGA_FIX_DECMASK))
#define VEGA_CEIL(x) (((x)+VEGA_FIX_DECMASK)&(~VEGA_FIX_DECMASK))

// fix to int will round correctly
#define VEGA_FIXTOINT(x) (((x)+(1<<(VEGA_FIX_DECBITS-1)))>>VEGA_FIX_DECBITS)
#define VEGA_INTTOFIX(x) (VEGA_FIX)((x)<<VEGA_FIX_DECBITS)
#define VEGA_TRUNCFIX(x) ((x)&(~VEGA_FIX_DECMASK))
#define VEGA_TRUNCFIXTOINT(x) ((x)>>VEGA_FIX_DECBITS)
// Scale with (1 << ss)
#define VEGA_FIXTOSCALEDINT(x,ss) \
((ss)<VEGA_FIX_DECBITS ? (x) >> (VEGA_FIX_DECBITS-(ss)) :\
((ss)>VEGA_FIX_DECBITS ? (x) << ((ss)-VEGA_FIX_DECBITS) : (x)))
#define VEGA_FIXTOSCALEDINT_T(x,ss) VEGA_FIXTOSCALEDINT(x,ss)

static inline VEGA_DBLFIX VEGA_INTTODBLFIX(INT32 v)
{
	return int64_shl(int64_load32(v), 2*VEGA_FIX_DECBITS);
}

static inline VEGA_DBLFIX VEGA_DBLFIXADD(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	return int64_add(a, b);
}

static inline VEGA_DBLFIX VEGA_DBLFIXSUB(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	return int64_sub(a, b);
}

static inline VEGA_DBLFIX VEGA_DBLFIXNEG(VEGA_DBLFIX a)
{
	return int64_neg(a);
}

static inline int VEGA_DBLFIXSIGN(VEGA_DBLFIX a)
{
	return int64_sign(a);
}

static inline BOOL VEGA_DBLFIXEQ(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	return int64_eq(a, b);
}
static inline BOOL VEGA_DBLFIXLT(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	return int64_lt(a, b);
}
static inline BOOL VEGA_DBLFIXGT(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	return int64_gt(a, b);
}
static inline BOOL VEGA_DBLFIXLTEQ(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	return int64_lteq(a, b);
}
static inline BOOL VEGA_DBLFIXGTEQ(VEGA_DBLFIX a, VEGA_DBLFIX b)
{
	return int64_gteq(a, b);
}

static inline VEGA_FIX VEGA_FIXMUL(VEGA_FIX x, VEGA_FIX y)
{
	return (VEGA_FIX)int64_shr32(int64_muls(x, y), VEGA_FIX_DECBITS);
}
static inline VEGA_DBLFIX VEGA_FIXMUL_DBL(VEGA_FIX x, VEGA_FIX y)
{
	return int64_muls(x, y);
}

VEGA_FIX VEGA_FIXDIV(VEGA_FIX x, VEGA_FIX y);
VEGA_DBLFIX VEGA_DBLFIXDIV(VEGA_DBLFIX a, VEGA_DBLFIX b);

static inline VEGA_FIX VEGA_FIXMULDIV(VEGA_FIX x, VEGA_FIX y, VEGA_FIX z)
{
	return (VEGA_FIX)int64_muldiv(x, y, z);
}

#define VEGA_FIXMUL2(x) ((x)<<1)
#define VEGA_FIXDIV2(x) ((x)>>1)

VEGA_FIX VEGA_FIXSQR(VEGA_FIX x);
VEGA_FIX VEGA_FIXSQRT(VEGA_FIX x);
VEGA_DBLFIX VEGA_DBLFIXSQRT(VEGA_DBLFIX x);

/* 3.14159265f * (1 << DECBITS) => 12868 */
#define VEGA_FIX_PI  12868
#define VEGA_FIX_2PI 25736

/* 2,718281828 * (1 << DECBITS) => 11134 */
#define VEGA_FIX_E 11134

#define VEGA_FIX_PI_OVER_180 (37480660 >> (31-VEGA_FIX_DECBITS))
#define VEGA_FIX_PI_HALF (3373259426 >> (31-VEGA_FIX_DECBITS))
#define VEGA_FIXSIN(x) VEGA_FIXCOS(x-VEGA_INTTOFIX(90))

VEGA_FIX VEGA_FIXCOS(VEGA_FIX x);
VEGA_FIX VEGA_FIXACOS(VEGA_FIX x);

VEGA_FIX VEGA_FIXLOG(VEGA_FIX x);
VEGA_FIX VEGA_FIXEXP(VEGA_FIX x);

VEGA_FIX VEGA_FIXPOW(VEGA_FIX x, VEGA_FIX y);

#define VEGA_ABS(x) op_abs(x)

VEGA_FIX VEGA_VECLENGTH(VEGA_FIX vx, VEGA_FIX vy);
VEGA_FIX VEGA_VECDOTSIGN(VEGA_FIX vx1, VEGA_FIX vy1, VEGA_FIX vx2, VEGA_FIX vy2);

#endif // VEGA_USE_FLOATS

#define VEGA_EQ(x,y) (VEGA_ABS(x-y) <= VEGA_EPSILON)

#define VEGA_FIXCOMP(x,y) (((x) < (y)) ? -1 : (((x) > (y)) ? 1 : 0))

#endif // VEGA_SUPPORT
#endif // VEGA_H
