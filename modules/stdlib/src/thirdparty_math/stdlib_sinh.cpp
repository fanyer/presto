/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* Id: sinh.c,v 1.4 1994/06/24 11:44:15 ceriel Exp */

#include "core/pch.h"

#ifdef STDLIB_OP_SINH_COSH

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#if !defined HAVE_SINH || !defined HAVE_COSH

static double
sinh_cosh(double x, int cosh_flag)
{
	/*	Algorithm and coefficients from:
			"Software manual for the elementary functions"
			by W.J. Cody and W. Waite, Prentice-Hall, 1980
	*/

	static const double p[] = {
		-0.35181283430177117881e+6,
		-0.11563521196851768270e+5,
		-0.16375798202630751372e+3,
		-0.78966127417357099479e+0
	};
	static const double q[] = {
		-0.21108770058106271242e+7,
		 0.36162723109421836460e+5,
		-0.27773523119650701167e+3,
		 1.0
	};
	int	negative = x < 0;
	double	y = negative ? -x : x;

	if (op_isnan(x)) {
		set_errno(EDOM);
		return x;
	}
	if (! cosh_flag && y <= 1.0) {
		/* ??? check for underflow ??? */
		y = y * y;
		return x + x * y * POLYNOM3(y, p)/POLYNOM3(y,q);
	}

	if (y >= M_LN_MAX_D) {
		/* exp(y) would cause overflow */
#define LNV	0.69316101074218750000e+0
#define VD2M1	0.52820835025874852469e-4
		double	w = y - LNV;
		
		if (w < M_LN_MAX_D+M_LN2-LNV) {
			x = op_exp(w);
			x += VD2M1 * x;
		}
		else {
			set_errno(ERANGE);
			x = HUGE_VAL;
		}
	}
	else {
		double	z = op_exp(y);
		
		x = 0.5 * (z + (cosh_flag ? 1.0 : -1.0)/z);
	}
	return negative ? -x : x;
}

#endif

#ifndef HAVE_SINH

double
op_sinh(double x)
{
	return sinh_cosh(x, 0);
}

#endif

#ifndef HAVE_COSH

double
op_cosh(double x)
{
	if (x < 0) x = -x;
	return sinh_cosh(x, 1);
}

#endif

#endif // ACK_MATH_LIBRARY

#endif // STDLIB_OP_SINH_COSH
