/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* Id: pow.c,v 1.5 1995/12/05 12:29:36 ceriel Exp */

#include "core/pch.h"

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#ifndef HAVE_POW

double
op_pow(double x, double y)
{
	double	y_intpart, y_fractpart, fp;
	int	negexp, negx;
	int	ex, newexp;
	unsigned long yi;

	if (x == 1.0) return x;

	if (x == 0 && y <= 0) {
		set_errno(EDOM);
		return 0;
	}

	if (y == 0) return 1.0;

	if (y < 0) {
		y = -y;
		negexp = 1;
	}
	else	negexp = 0;

	y_fractpart = op_modf(y, &y_intpart);

	if (y_fractpart != 0) {
		if (x < 0) {
			set_errno(EDOM);
			return 0;
		}
	}

	negx = 0;
	if (x < 0) {
		x = -x;
		negx = 1;
	}

	if (y_intpart > ULONG_MAX) {
		if (negx && op_modf(y_intpart/2.0, &y_fractpart) == 0) {
			negx = 0;
		}

		x = op_log(x);

		/* Beware of overflow in the multiplication */
		if (x > 1.0 && y > DBL_MAX/x) {
			set_errno(ERANGE);
			return HUGE_VAL;
		}
		if (negexp) y = -y;

		if (negx) return -op_exp(x*y);
		return op_exp(x * y);
	}

	if (y_fractpart != 0) {
		fp = op_exp(y_fractpart * op_log(x));
	}
	else	fp = 1.0;
	// Modified by Opera: use op_double2int32() instead of cast
	yi = op_double2int32(y_intpart);
	if (! (yi & 1)) negx = 0;
	x = op_frexp(x, &ex);
	newexp = 0;
	for (;;) {
		if (yi & 1) {
			fp *= x;
			newexp += ex;
		}
		yi >>= 1;
		if (yi == 0) break;
		x *= x;
		ex <<= 1;
		if (x < 0.5) {
			x += x;
			ex -= 1;
		}
	}
	if (negexp) {
		fp = 1.0/fp;
		newexp = -newexp;
	}
	if (negx) {
		return -op_ldexp(fp, newexp);
	}
	return op_ldexp(fp, newexp);
}

#endif // HAVE_POW

#endif // ACK_MATH_LIBRARY
