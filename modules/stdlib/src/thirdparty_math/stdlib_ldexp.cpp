/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* Id: ldexp.c,v 1.6 1994/06/24 11:43:53 ceriel Exp */

#include "core/pch.h"

#ifdef STDLIB_OP_LDEXP

#ifdef ACK_MATH_LIBRARY

#include "modules/stdlib/src/thirdparty_math/localmath.h"

#ifndef HAVE_LDEXP

double
op_ldexp(double fl, int exp)
{
	int sign = 1;
	int currexp;

	if (op_isnan(fl)) {
		set_errno(EDOM);
		return fl;
	}
	if (fl == 0.0) return 0.0;
	if (fl<0) {
		fl = -fl;
		sign = -1;
	}
	if (fl > DBL_MAX) {		/* for infinity */
		set_errno(ERANGE);
		return sign * fl;
	}
	fl = op_frexp(fl,&currexp);
	exp += currexp;
	if (exp > 0) {
		if (exp > DBL_MAX_EXP) {
			set_errno(ERANGE);
			return sign * HUGE_VAL;
		}
		while (exp>30) {
			fl *= (double) (1L << 30);
			exp -= 30;
		}
		fl *= (double) (1L << exp);
	}
	else	{
		/* number need not be normalized */
		if (exp < DBL_MIN_EXP - DBL_MANT_DIG) {
			return 0.0;
		}
		while (exp<-30) {
			fl /= (double) (1L << 30);
			exp += 30;
		}
		fl /= (double) (1L << -exp);
	}
	return sign * fl;
}

#endif

#endif // ACK_MATH_LIBRARY
#endif // STDLIB_OP_LDEXP
