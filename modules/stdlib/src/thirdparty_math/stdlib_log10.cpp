/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* Id: log10.c,v 1.4 1994/06/24 11:44:02 ceriel Exp */

#include "core/pch.h"

#ifdef STDLIB_OP_LOG10

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#ifndef HAVE_LOG10

double
op_log10(double x)
{
	if (op_isnan(x)) {
		set_errno(EDOM);
		return x;
	}
	if (x < 0) {
		set_errno(EDOM);
		return -HUGE_VAL;
	}
	else if (x == 0) {
		set_errno(ERANGE);
		return -HUGE_VAL;
	}

	return op_log(x) / M_LN10;
}

#endif // HAVE_LOG10

#endif // ACK_MATH_LIBRARY

#endif // STDLIB_OP_LOG10
