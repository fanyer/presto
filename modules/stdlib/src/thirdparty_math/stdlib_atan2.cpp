/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* Id: atan2.c,v 1.3 1994/06/24 11:43:15 ceriel Exp */

#include "core/pch.h"

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#ifndef HAVE_ATAN2

double
op_atan2(double y, double x)
{
	double absx, absy, val;

	if (x == 0 && y == 0) {
		set_errno(EDOM);
		return 0;
	}
	absy = y < 0 ? -y : y;
	absx = x < 0 ? -x : x;
	if (absy - absx == absy) {
		/* x negligible compared to y */
		return y < 0 ? -M_PI_2 : M_PI_2;
	}
	if (absx - absy == absx) {
		/* y negligible compared to x */
		val = 0.0;
	}
	else	val = op_atan(y/x);
	if (x > 0) {
		/* first or fourth quadrant; already correct */
		return val;
	}
	if (y < 0) {
		/* third quadrant */
		return val - M_PI;
	}
	return val + M_PI;
}

#endif

#endif // ACK_MATH_LIBRARY
