/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* Id: asin.c,v 1.4 1994/06/24 11:43:08 ceriel Exp */

#include "core/pch.h"

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#if !defined HAVE_ASIN || !defined HAVE_ACOS

static double
asin_acos(double x, int cosfl)
{
	int negative = x < 0;
	int     i;
	double  g;
	static const double p[] = {
		-0.27368494524164255994e+2,
		 0.57208227877891731407e+2,
		-0.39688862997540877339e+2,
		 0.10152522233806463645e+2,
		-0.69674573447350646411e+0
	};
	static const double q[] = {
		-0.16421096714498560795e+3,
		 0.41714430248260412556e+3,
		-0.38186303361750149284e+3,
		 0.15095270841030604719e+3,
		-0.23823859153670238830e+2,
		 1.0
	};

	if (op_isnan(x)) {
		set_errno(EDOM);
		return x;
	}

	if (negative) {
		x = -x;
	}
	if (x > 0.5) {
		i = 1;
		if (x > 1) {
			set_errno(EDOM);
			return 0;
		}
		g = 0.5 - 0.5 * x;
		x = - op_sqrt(g);
		x += x;
	}
	else {
		/* ??? avoid underflow ??? */
		i = 0;
		g = x * x;
	}
	x += x * g * POLYNOM4(g, p) / POLYNOM5(g, q);
	if (cosfl) {
		if (! negative) x = -x;
	}
	if ((cosfl == 0) == (i == 1)) {
		x = (x + M_PI_4) + M_PI_4;
	}
	else if (cosfl && negative && i == 1) {
		x = (x + M_PI_2) + M_PI_2;
	}
	if (! cosfl && negative) x = -x;
	return x;
}

#endif // !defined HAVE_ASIN || !defined HAVE_ACOS

#ifndef HAVE_ASIN

double
op_asin(double x)
{
	return asin_acos(x, 0);
}

#endif // HAVE_ASIN

#ifndef HAVE_ACOS

double
op_acos(double x)
{
	return asin_acos(x, 1);
}

#endif // HAVE_ACOS

#endif // ACK_MATH_LIBRARY
