/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGNUMBER_H
#define SVGNUMBER_H

#include "modules/libvega/vegafixpoint.h"

class SVGNumber
{
private:
#ifdef SVG_DOUBLE_PRECISION_FLOATS
	typedef double number_t;
	typedef float other_t;
#else
	typedef float number_t;
	typedef double other_t;
#endif // SVG_DOUBLE_PRECISION_FLOATS
#ifndef VEGA_SUPPORT
	typedef number_t VEGA_FIX;
#endif // VEGA_SUPPORT

public:
	enum Uninitialized
	{
		UNINITIALIZED_SVGNUMBER
	};

	SVGNumber(Uninitialized) {}
	SVGNumber() : value(0) {}
	SVGNumber(int intpart) : value((number_t)intpart) {}
	SVGNumber(unsigned int intpart) : value((number_t)intpart) {}
	SVGNumber(long intpart) : value((number_t)intpart) {}
	SVGNumber(unsigned long intpart) : value((number_t)intpart) {}
	// Don't use this
	SVGNumber(int intpart, unsigned int millions) : value((number_t)intpart + (number_t)millions/(number_t)1000000) { OP_ASSERT(!"This constructor will do terrible things for fixed point"); }
	SVGNumber(other_t val) : value((number_t)val) {}
	SVGNumber(number_t val) : value(val) {}

	void operator=(const SVGNumber& other) { value = other.value; }
	SVGNumber operator*(const SVGNumber& other) const { return SVGNumber(value * other.value); }
	SVGNumber operator/(const SVGNumber& other) const { return SVGNumber(value / other.value); }
	SVGNumber operator%(const SVGNumber& other) const { return SVGNumber(::op_fmod(value, other.value)); }
	void operator*=(const SVGNumber& other) { value *= other.value; }
	void operator/=(const SVGNumber& other) { value /= other.value; }

	SVGNumber operator+(const SVGNumber& other) const { return SVGNumber(value + other.value); }
	SVGNumber operator-(const SVGNumber& other) const { return SVGNumber(value - other.value); }
	void operator+=(const SVGNumber& other) { value += other.value; }
	void operator-=(const SVGNumber& other) { value -= other.value; }

	SVGNumber& operator++() { ++value; return *this; }
	SVGNumber& operator--() { --value; return *this; }
	SVGNumber operator++(int) { SVGNumber rv(value); ++value; return rv; }
	SVGNumber operator--(int) { SVGNumber rv(value); --value; return rv; }
	
	SVGNumber operator-() const { return SVGNumber(-value); }
	
	BOOL operator>(const SVGNumber &other) const { return value > other.value; }
	BOOL operator<(const SVGNumber &other) const { return value < other.value; }
	BOOL operator>=(const SVGNumber &other) const { return value >= other.value; }
	BOOL operator<=(const SVGNumber &other) const { return value <= other.value; }

	BOOL Equal(const SVGNumber& other) const { return value == other.value; }
	BOOL NotEqual(const SVGNumber& other) const { return value != other.value; }
	BOOL Close(const SVGNumber& other) const { return ::op_fabs(value - other.value) < eps().value; }
	BOOL Close(const SVGNumber& other, const SVGNumber& e) const { return ::op_fabs(value - other.value) < e.value; }

	BOOL operator==(const SVGNumber &other) const { return Equal(other); }
	BOOL operator!=(const SVGNumber &other) const { return NotEqual(other); }

	SVGNumber sqrt() const { return SVGNumber(::op_sqrt(value)); }
	SVGNumber abs() const { return SVGNumber(::op_fabs(value)); }
	SVGNumber ceil() const { return SVGNumber(::op_ceil(value)); }
	SVGNumber floor() const { return SVGNumber(::op_floor(value)); }
	SVGNumber pow(const SVGNumber& other) { return SVGNumber(::op_pow(value, other.value)); }

	static SVGNumber max_of(const SVGNumber &n1, const SVGNumber& n2) { return SVGNumber(MAX(n1.value, n2.value)); }
	static SVGNumber min_of(const SVGNumber &n1, const SVGNumber& n2) { return SVGNumber(MIN(n1.value, n2.value)); }

	float GetFloatValue() const { return (float)value; }
	int GetIntegerValue() const { return (int)value; }
	int GetRoundedIntegerValue() const { return (int)(value + 0.5); }
	long GetFixedPointValue(unsigned int decbits) const { return (long)(value*(1<<decbits)); }

	VEGA_FIX GetVegaNumber() const
	{
#if defined(VEGA_USE_FLOATS) || !defined(VEGA_SUPPORT)
		return (float)value; // same implementation as GetFloatValue()
#else
		return (long)(value*(1<<VEGA_FIX_DECBITS));
#endif // VEGA_FIX_DECBITS
	}

	void SetFixedPointValue(unsigned int decbits, unsigned int new_value) { value = ((number_t)new_value) / (1<<decbits); }
	void SetVegaNumber(VEGA_FIX vega_number)
	{
#if defined(VEGA_USE_FLOATS) || !defined(VEGA_SUPPORT)
# ifdef SVG_DOUBLE_PRECISION_FLOATS
		// go from float to double
		value = (double)vega_number;
# else
		// go from float to float
		value = vega_number;
# endif
#else // !VEGA_USE_FLOATS
		value = ((number_t)vega_number) / (1 << VEGA_FIX_DECBITS);
#endif // !VEGA_USE_FLOATS
	}

	SVGNumber atan2(const SVGNumber &other) const { return SVGNumber(::op_atan2(other.value, value)); }
	SVGNumber sin() const { return SVGNumber(::op_sin(value)); }
	SVGNumber cos() const { return SVGNumber(::op_cos(value)); }
	SVGNumber tan() const { return SVGNumber(::op_tan(value)); }
	SVGNumber acos() const { return SVGNumber(::op_acos(value)); }

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
	static SVGNumber pi() { return SVGNumber((number_t)M_PI); }
#ifdef SVG_DOUBLE_PRECISION_FLOATS
	// For ieee doubles
#ifndef DBL_EPSILON
# define DBL_EPSILON     2.2204460492503131e-016 /* smallest such that 1.0+DBL_EPSILON != 1.0 */
#endif // DBL_EPSILON
#ifndef DBL_MAX
# define DBL_MAX         1.7976931348623158e+308 /* max value */
#endif // DBL_MAX
#ifndef DBL_MIN
# define DBL_MIN         2.2250738585072014e-308 /* min positive value */
#endif // DBL_MIN
	static SVGNumber eps() { return SVGNumber(DBL_EPSILON*100000); } // arbitrary number, need tuning
	static SVGNumber max_num() { return SVGNumber(DBL_MAX); }
	static SVGNumber min_num() { return SVGNumber(-DBL_MAX); }
#else
	// For ieee flots
#ifndef FLT_EPSILON
# define FLT_EPSILON     1.192092896e-07F        /* smallest such that 1.0+FLT_EPSILON != 1.0 */
#endif // FLT_EPSILON
#ifndef FLT_MAX
# define FLT_MAX         3.402823466e+38F        /* max value */
#endif // FLT_MAX
#ifndef FLT_MIN
# define FLT_MIN         1.175494351e-38F        /* min positive value */
#endif // FLT_MIN
	static SVGNumber eps() { return SVGNumber(FLT_EPSILON*100); } // arbitrary number, need tuning
	static SVGNumber max_num() { return SVGNumber(FLT_MAX); }
	static SVGNumber min_num() { return SVGNumber(-FLT_MAX); }
#endif // SVG_DOUBLE_PRECISION_FLOATS
private:
	number_t value;
};
#endif // SVGNUMBER_H
