/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DTOA_FLOITSCH_DOUBLE_CONVERSION
#include "modules/stdlib/include/double_format.h"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/double-conversion.h"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/fast-dtoa.h"

#ifndef HAVE_STRTOD
double
op_strtod(const char *s, char **se)
{
	if (!s)
		return 0.0;

	int flags = StringToDoubleConverter::ALLOW_LEADING_SPACES | StringToDoubleConverter::ALLOW_TRAILING_JUNK | StringToDoubleConverter::ALLOW_SCANF_NAN_INFINITIES;
	double empty_string_value = 0.0;
	double junk_string_value = g_stdlib_quiet_nan;
	const char *infinity_symbol = NULL;
	const char *nan_symbol = NULL;
	StringToDoubleConverter converter(flags, empty_string_value, junk_string_value, infinity_symbol, nan_symbol);

	/* The StringToDouble() method takes an explicit length, which is not
	   a 1-1 match with strtod(). So, compute it by looking ahead for next
	   character not matching what could legally appear. Note that overestimating
	   the length is not a problem as trailing "junk" is accepted. */
	int length = op_strspn(s, " \t\n\r0123456789eE.+-nNaAIfity");
	int processed = 0;
	double result = converter.StringToDouble(s, length, &processed);
	if (se)
		*se = const_cast<char *>(s) + processed;
	return result;
}
#endif // !HAVE_STRTOD

char *
stdlib_dtoa(double d, int mode, int ndigits, OpDoubleFormat::RoundingBias bias, int *decpt, int *sign, char **rve)
{
	char *buffer = g_stdlib_dtoa_buffer;
	unsigned buffer_length = ARRAY_SIZE(g_stdlib_dtoa_buffer);
	OP_ASSERT(op_isfinite(d) && rve != NULL && sign != NULL || decpt != NULL);

	DoubleToStringConverter::DtoaMode dtoa_mode;
	switch (mode)
	{
	default:
		OP_ASSERT(!"Unexpected mode");
		/* Deliberate fallthrough */
	case 1:
		dtoa_mode = DoubleToStringConverter::SHORTEST;
		break;
	case 2:
		dtoa_mode = DoubleToStringConverter::PRECISION;
		break;
	case 3:
		dtoa_mode = DoubleToStringConverter::FIXED;
		break;
	}

	DoubleToStringConverter::RoundingBias rounding_bias;
	switch (bias)
	{
	case OpDoubleFormat::ROUND_BIAS_AWAY_FROM_ZERO:
		rounding_bias = DoubleToStringConverter::ROUND_NEAREST_AWAY_FROM_ZERO;
		break;
	default:
		OP_ASSERT(!"Unrecognized rounding bias");
		/* Deliberate fallthrough */
	case OpDoubleFormat::ROUND_BIAS_TO_EVEN:
		rounding_bias = DoubleToStringConverter::ROUND_NEAREST_TO_EVEN;
		break;
	}

	int length = 0;
	bool is_signed = false;
	DoubleToStringConverter::DoubleToAscii(d, dtoa_mode, MIN(buffer_length - 1, static_cast<unsigned>(ndigits)), rounding_bias, buffer, buffer_length, &is_signed, &length, decpt);
	*sign = !!is_signed;
	*rve = buffer + length;

	return buffer;
}

#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/bignum.cc"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/bignum-dtoa.cc"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/cached-powers.cc"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/diy-fp.cc"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/double-conversion.cc"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/fast-dtoa.cc"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/fixed-dtoa.cc"
#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/strtod.cc"

#endif // DTOA_FLOITSCH_DOUBLE_CONVERSION
