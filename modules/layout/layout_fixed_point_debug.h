/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LAYOUT_FIXED_POINT_DEBUG_H
#define LAYOUT_FIXED_POINT_DEBUG_H

#include "modules/layout/layout_debug_utils.h"

/** The LayoutFixed in debug mode. Ensures type safety and provides suitable
	debugging utils, like overflow detection. See also LayoutCoordDebug.

	In order to hide operations that could be strange and lead to an error, some
	standard operators are declared in private section.

	All the operations that are the same for LayoutFixed both in debug and
	release mode, are declared as public operators. Other operations (ones that
	could differ) are friendly inline functions instead.

	@see class LayoutCoordDebug. */

class LayoutFixedDebug
{
public:
	LayoutFixedDebug() {}

	/** All the ctors except the default one are explicit, because they do not
		perform any conversion, they just assign the exact value.

		If you want to convert e.g. and int to LayoutFixed type, use IntToLayoutFixed
		function.

		@see LayoutFixed IntToLayoutFixed(int).	*/

	explicit LayoutFixedDebug(int v) : value(v) {}
	explicit LayoutFixedDebug(unsigned int v) : value(int(v)) {}
	explicit LayoutFixedDebug(double v) : value(int(v)) {}

	LayoutFixedDebug operator+(const LayoutFixedDebug& other) const { LayoutFixedDebug res(*this); res += other; return res; }
	LayoutFixedDebug operator-(const LayoutFixedDebug& other) const { LayoutFixedDebug res(*this); res -= other; return res; }

	void operator+=(const LayoutFixedDebug& other)
	{
#ifdef LAYOUT_TYPES_RUNTIME_DEBUG
		layout_type_check_add(value, other.value);
#endif // LAYOUT_TYPES_RUNTIME_DEBUG

		value += other.value;
	}

	void operator-=(const LayoutFixedDebug& other)
	{
#ifdef LAYOUT_TYPES_RUNTIME_DEBUG
		layout_type_check_sub(value, other.value);
#endif // LAYOUT_TYPES_RUNTIME_DEBUG

		value -= other.value;
	}

	double operator*(float val) const { return value * val; }
	double operator*(double val) const { return value * val; }

	LayoutFixedDebug operator/(int other) const { return LayoutFixedDebug(value / other); }
	void operator/=(int other) { value /= other; }

	float operator/(float other) const { return value / other; }

	LayoutFixedDebug operator-() const { LayoutFixedDebug res(*this); res.value = - value; return res; }
	BOOL operator!() const { return !value; }

	BOOL operator>(const LayoutFixedDebug &other) const { return value > other.value; }
	BOOL operator<(const LayoutFixedDebug &other) const { return value < other.value; }
	BOOL operator>=(const LayoutFixedDebug &other) const { return value >= other.value; }
	BOOL operator<=(const LayoutFixedDebug &other) const { return value <= other.value; }

	BOOL operator==(const LayoutFixedDebug &other) const { return other.value == value; }
	BOOL operator!=(const LayoutFixedDebug &other) const { return other.value != value; }

	LayoutFixedDebug& operator++() { ++value; return *this; }
	LayoutFixedDebug& operator--() { --value; return *this; }
	LayoutFixedDebug operator++(int) { LayoutFixedDebug rv(value); ++value; return rv; }
	LayoutFixedDebug operator--(int) { LayoutFixedDebug rv(value); --value; return rv; }

private:

	/** Private, because the C++ int multiplication of two LayoutFixeds
		is not equal to the real, mathematical result. */

	LayoutFixedDebug operator*(const LayoutFixedDebug& other) const { LayoutFixedDebug res(*this); res *= other; return res; }
	LayoutFixedDebug operator*(int other) const { LayoutFixedDebug res(*this); res *= other; return res; }

	void operator*=(int other)
	{
#ifdef LAYOUT_TYPES_RUNTIME_DEBUG
		layout_type_check_mult(value, other);
#endif // LAYOUT_TYPES_RUNTIME_DEBUG

		value *= other;
	}

	/** The int cast operator. It is private, because in mathematical way
		the stored value means different number when considered LayoutFixed. */

	operator int() const { return value; }

	int value;

	friend class PixelOrLayoutFixedDebug;

	// Friend functions. Used for LayoutFixed type both in debug and release mode.

	friend int LayoutFixedToIntNonNegative(LayoutFixedDebug value);
	friend int LayoutFixedToIntRoundDown(LayoutFixedDebug value);
	friend int LayoutFixedToInt(LayoutFixedDebug value);
	friend float LayoutFixedToFloat(LayoutFixedDebug value);
	friend LayoutCoord IntToFixedPointPercentageAsLayoutCoord(int percent);
	friend int LayoutFixedAsInt(LayoutFixedDebug value);
	friend LayoutCoord LayoutFixedAsLayoutCoord(LayoutFixedDebug value);
	friend LayoutFixedDebug LayoutFixedMult(LayoutFixedDebug a, LayoutFixedDebug b);
	friend int LayoutFixedPercentageOfInt(LayoutFixedDebug percent, int base);
	friend LayoutCoord PercentageToLayoutCoord(LayoutFixedDebug percent, LayoutCoord base);
	friend LayoutCoord PercentageToLayoutCoordCeil(LayoutFixedDebug percent, LayoutCoord base);
	friend LayoutCoord PercentageOfPercentageToLayoutCoord(LayoutFixedDebug percent, LayoutCoord base, LayoutFixedDebug whole);
	friend LayoutCoord DoublePercentageToLayoutCoord(LayoutFixedDebug percent1, LayoutFixedDebug percent2, LayoutCoord base);
	friend LayoutCoord ReversePercentageToLayoutCoord(LayoutFixedDebug percent, LayoutCoord reverse_base);
	friend LayoutCoord ReversePercentageOfPercentageToLayoutCoord(LayoutFixedDebug percent, LayoutCoord reverse_base, LayoutFixedDebug whole);
	friend LayoutCoord ReversePercentageToLayoutCoordRoundDown(LayoutFixedDebug percent, LayoutCoord reverse_base);
	friend LayoutFixedDebug MultLayoutFixedByQuotient(LayoutFixedDebug percent, int n, int d);
};

typedef LayoutFixedDebug LayoutFixed;

/** This class acts as a temporary storage of either a pixel value or
	a layout fixed point value. It is needed in debug mode to maintain
	type safety provided by LayoutFixedDebug class.

	It allows two things:

	- avoid temporary conversion to LayoutFixed when the value would
	  be immediately converted back to a full pixel value represented
	  as standard int or LayoutCoord

	- allows to store more integers than LayoutFixed if being an
	  integer (pixel) value

	Use with caution. This class (and type) is planned to be removed
	in the near future.

	NOTE: Currently, in the layout engine, we sometimes also use
	LayoutCoord as a storage of the value that is in fact a
	LayoutFixed. Such trick is used for percentage based
	widths/heights - when a negative value means percentage based. */

class PixelOrLayoutFixedDebug
{
public:
	PixelOrLayoutFixedDebug() {}
	PixelOrLayoutFixedDebug(int v) : value(v) {}
	PixelOrLayoutFixedDebug(const LayoutFixedDebug& fixed) : value(fixed.value) {}

	operator int() const { return value; }
	operator LayoutFixedDebug() const { return LayoutFixedDebug(value); }
	operator LayoutCoord() const { return LayoutCoord(value); }

private:

	int	value;
};

typedef PixelOrLayoutFixedDebug PixelOrLayoutFixed;

#endif // LAYOUT_FIXED_POINT_DEBUG_H
