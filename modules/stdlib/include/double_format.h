/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef STDLIB_DOUBLE_FORMAT_H
#define STDLIB_DOUBLE_FORMAT_H

class OpDoubleFormat
{
public:
	 /** The ES Number builtins toFixed(), toPrecision(), and toExponential()
	     have a away-from-zero bias on rounding ties. C/C++ printf()-style
	     g/e/f format specifiers uses the bias of the default IEEE-754
	     rounding mode (round-to-nearest), which biases towards even. */
	enum RoundingBias {
		ROUND_BIAS_TO_EVEN,
		ROUND_BIAS_AWAY_FROM_ZERO
	};

	/** Format number on mixed decimal-and-exponent format like printf %g,
	    returning the closest decimal approximation to the given double value.

	    @param b The result buffer, at least 32 bytes long.
	    @param d The number to format.
	    @returns b, or NULL on OOM.

	    The buffer b is always null-terminated on success.

	    OOM NOTE: this function may allocate memory for working storage.
	    If an OOM event occurs, NULL is returned.  However, this behavior
	    is not reliable.

	    IMPLEMENTATION NOTE: As for op_strtod(), different code is used
	    depending on whether FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION or
	    FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION are defined or not.

	    If FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION is defined, then
	    Florian Loitsch's double-conversion library is used.

	    If FEATURE_3P_DTOA_DAVID_M_GAY is defined, then David Gay's code is used.

	    If either is not defined, then this function will be implemented
	    using the porting interfaces provided by the StdlibPI class. */
	static char *ToString(char *buffer, double d);

	static const unsigned MaxPrecision = 20;

	/** ECMA-262 15.7.4.6: Format number using the exponential format,
	    with one digit before the decimal point.

	    @param b A buffer, at least MAX(precision + 9,10) characters
	           long if precision >= 0, or at least 32 characters
	           long otherwise.
	    @param x The number to format.
	    @param precision The number of digits after the decimal
	           point, or -1 to signify "as many as required".
	    @param bias The rounding bias to use when a tie. The default
	           is the EcmaScript prescribed mode of away-from-zero.
	    @returns b, or NULL on OOM.

	    The buffer b is always null-terminated on success.

	    OOM NOTE: this function may allocate memory for working storage.
	    If an OOM event occurs, NULL is returned.  However, this behavior
	    is not reliable.

	    IMPLEMENTATION NOTE: As for op_strtod(), different code is used
	    depending on whether FEATURE_3P_DTOA_DAVID_M_GAY or
	    FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION are defined or not.

	    If FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION is defined, then
	    Florian Loitsch's double-conversion library is used.

	    If FEATURE_3P_DTOA_DAVID_M_GAY is defined, then David Gay's code
	    is used.

	    If either is not defined, then this function will be implemented
	    using the porting interfaces provided by the StdlibPI class. */
	static char *ToExponential(register char *b, double x, int precision, RoundingBias bias = ROUND_BIAS_AWAY_FROM_ZERO);

	/** ECMA-262 15.7.4.5: Format number using fixed-point format with
	    precision digits after the decimal point.

	    @param b The result buffer.
	    @param x The number to format.
	    @param precision The number of digits after the decimal point,
	           or -1 to signify "as many as necessary".
	    @param bufsiz The size of the buffer.
	    @param bias The rounding bias to use when a tie. The default
	           is the EcmaScript prescribed mode of away-from-zero.
	    @returns b, or NULL on OOM.

	    The buffer b is always null-terminated on success.

	    USAGE NOTE: Please do not rely on the default argument value for
	    "bufsiz".  It exists temporarily for backward compatibility and
	    will be removed.

	    OOM NOTE: this function may allocate memory for working storage.
	    If an OOM event occurs, NULL is returned.  However, this behavior
	    is not reliable.

	    IMPLEMENTATION NOTE: As for op_strtod(), different code is used
	    depending on whether FEATURE_3P_DTOA_DAVID_M_GAY or
	    FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION are defined or not.

	    If FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION is defined, then
	    Florian Loitsch's double-conversion library is used.

	    If FEATURE_3P_DTOA_DAVID_M_GAY is defined, then David Gay's code
	    is used.

	    If either is not defined, then this function will be implemented
	    using the porting interfaces provided by the StdlibPI class. */
	static char *ToFixed(register char *b, double x, int precision, size_t bufsiz = 32, RoundingBias bias = ROUND_BIAS_AWAY_FROM_ZERO);

	/** ECMA-262 15.7.4.5: Format number using the exponential-or-fixed
	    format, depending on the magnitude of the number.

	    @param b The result buffer.
	    @param x The number to format.
	    @param precision For exponential format, use precision - 1
	           digits after the decimal point; for fixed-point
	           format, use precision significant digits.
	    @param bufsiz The size of the buffer.
	    @param bias The rounding bias to use when a tie. The default
	           is the EcmaScript prescribed mode of away-from-zero.
	    @returns b, or NULL on OOM.

	    The buffer b is always null-terminated on success.

	    USAGE NOTE: Please do not rely on the default argument value for
	    "bufsiz".  It exists temporarily for backward compatibility and
	    will be removed.

	    OOM NOTE: this function may allocate memory for working storage.
	    If an OOM event occurs, NULL is returned.  However, this behavior
	    is not reliable.

	    IMPLEMENTATION NOTE: As for op_strtod(), different code is used
	    depending on whether FEATURE_3P_DTOA_DAVID_M_GAY or
	    FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION are defined or not.

	    If FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION is defined, then
	    Florian Loitsch's double-conversion library is used.

	    If FEATURE_3P_DTOA_DAVID_M_GAY is defined, then David Gay'scode
	    is used.

	    If either is not defined, then this function will be implemented
	    using the porting interfaces provided by the StdlibPI class. */
	static char *ToPrecision(register char *b, double x, int precision, size_t bufsiz = 32, RoundingBias bias = ROUND_BIAS_AWAY_FROM_ZERO);

	static char *PrintfFormat(double x, char* b, size_t bufsiz, char fmt, int precision);
};

#endif // STDLIB_DOUBLE_FORMAT_H
