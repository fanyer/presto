/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* Id: exp.c,v 1.6 1994/06/24 11:43:26 ceriel Exp */

#include "core/pch.h"

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#ifndef HAVE_EXP

double
op_exp(double x)
{
	/*	Algorithm and coefficients from:
			"Software manual for the elementary functions"
			by W.J. Cody and W. Waite, Prentice-Hall, 1980
	*/

	static const double p[] = {
		0.25000000000000000000e+0,
		0.75753180159422776666e-2,
		0.31555192765684646356e-4
	};

	static const double q[] = {
		0.50000000000000000000e+0,
		0.56817302698551221787e-1,
		0.63121894374398503557e-3,
		0.75104028399870046114e-6
	};
	double	xn, g;
	int	n;
	int	negative = x < 0;

	if (op_isnan(x)) {
		set_errno(EDOM);
		return x;
	}
	if (x < M_LN_MIN_D) {
		set_errno(ERANGE);
		return 0.0;
	}
	if (x > M_LN_MAX_D) {
		set_errno(ERANGE);
		return HUGE_VAL;
	}

	if (negative) x = -x;
 
	/* ??? avoid underflow ??? */

	// Modified by Opera: use op_double2int32() instead of cast
	n = op_double2int32(x * M_LOG2E + 0.5);	/* 1/ln(2) = log2(e), 0.5 added for rounding */
	xn = n;
	{
		double	x1 = (long) x;
		double	x2 = x - x1;

		g = ((x1-xn*0.693359375)+x2) - xn*(-2.1219444005469058277e-4);
	}
	if (negative) {
		g = -g;
		n = -n;
	}
	xn = g * g;
	x = g * POLYNOM2(xn, p);
	n += 1;
	return (op_ldexp(0.5 + x/(POLYNOM3(xn, q) - x), n));
}

#endif

#endif // ACK_MATH_LIBRARY
