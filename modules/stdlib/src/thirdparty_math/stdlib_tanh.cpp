/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* Id: tanh.c,v 1.4 1994/06/24 11:44:25 ceriel Exp */

#include "core/pch.h"

#ifdef STDLIB_OP_TANH

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#ifndef HAVE_TANH

double
op_tanh(double x)
{
	/*	Algorithm and coefficients from:
			"Software manual for the elementary functions"
			by W.J. Cody and W. Waite, Prentice-Hall, 1980
	*/

	static const double p[] = {
		-0.16134119023996228053e+4,
		-0.99225929672236083313e+2,
		-0.96437492777225469787e+0
	};
	static const double q[] = {
		 0.48402357071988688686e+4,
		 0.22337720718962312926e+4,
		 0.11274474380534949335e+3,
		 1.0
	};
	int 	negative = x < 0;

	if (op_isnan(x)) {
		set_errno(EDOM);
		return x;
	}
	if (negative) x = -x;

	if (x >= 0.5*M_LN_MAX_D) {
		x = 1.0;
	}
#define LN3D2	0.54930614433405484570e+0	/* ln(3)/2 */
	else if (x > LN3D2) {
		x = 0.5 - 1.0/(op_exp(x+x)+1.0);
		x += x;
	}
	else {
		/* ??? avoid underflow ??? */
		double g = x*x;
		x += x * g * POLYNOM2(g, p)/POLYNOM3(g, q);
	}
	return negative ? -x : x;
}

#endif // HAVE_TANH

#endif // ACK_MATH_LIBRARY

#endif // STDLIB_OP_TANH

