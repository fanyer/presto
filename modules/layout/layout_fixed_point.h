/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LAYOUT_FIXED_POINT_H
#define LAYOUT_FIXED_POINT_H

#ifdef SVG_SUPPORT
# include "modules/svg/svg_number.h"
#endif // SVG_SUPPORT

#include "modules/layout/layout_coord.h"
#include "modules/util/gen_math.h"

/* This constant defines all the layout's fixed point values precision.
   So the precision is roughly two digits after floating point when
   converting to fixed point values. When the value to be stored is
   e.g. 3.494, it will become 349 as fixed point integer.

   Use with caution! The operations regarding LayoutFixed type
   should be carried out using public methods of LayoutFixedDebug
   and inline functions in this file.

   If this is modified the following things need to be checked and possibly
   adjusted:
   - modules/layout/layout_fixed_point.h
   - class TableRowBox (stores percentage width in one of packed structs)
   - class TableColumnInfo::packed.percent */
#define LAYOUT_FIXED_POINT_BASE 100

/* The smallest number that has only ones in binary format that is
   not smaller than LAYOUT_FIXED_POINT_BASE. */
#define LAYOUT_FIXED_POINT_BITS 0x7f

/* The boundary used to constrain layout integer values (amounts of pixels).
   We leave some margin for arithmetic operations in order to avoid integer
   overflows. */
#define LAYOUT_INT_SAFE_MAX (INT_MAX / 2 + 1)
/* The boundary used to constrain computed floating point values that are
   a layout fixed or a layout coord (mathematically equal to
   LAYOUT_INT_SAFE_MAX). NOTE: This value has exact
   single floating point representation, because it is now 2^30. */
#define LAYOUT_FLOAT_SAFE_MAX float(LAYOUT_INT_SAFE_MAX)

/* The constrain such that when put on an integer, it will be inside the
   LAYOUT_INT_SAFE_MAX boundary when converted to layout fixed. */
#define LAYOUT_FIXED_AS_INT_SAFE_MAX (LAYOUT_INT_SAFE_MAX / LAYOUT_FIXED_POINT_BASE)
/* As the above but used for floats that will be converted to layout fixed.
   NOTE: Currently LAYOUT_FIXED_AS_INT_SAFE_MAX is small enough to ensure
   exact single precision floating point representation. */
#define LAYOUT_FIXED_AS_FLOAT_SAFE_MAX float(LAYOUT_FIXED_AS_INT_SAFE_MAX)

// Constrain a floating point value that is a layout fixed.
#define CONSTRAIN_LAYOUT_FIXED(x) MAX(MIN((x), LAYOUT_FLOAT_SAFE_MAX), -LAYOUT_FLOAT_SAFE_MAX)
// Constrain a floating point value that is a layout coord.
#define CONSTRAIN_LAYOUT_COORD(x) CONSTRAIN_LAYOUT_FIXED(x)

// Constrain an integer that will be converted to layout fixed.
#define CONSTRAIN_LAYOUT_FIXED_AS_INT(x) MAX(MIN((x), LAYOUT_FIXED_AS_INT_SAFE_MAX), -LAYOUT_FIXED_AS_INT_SAFE_MAX)
// Constrain a floating point value that will be converted to layout fixed.
#define CONSTRAIN_LAYOUT_FIXED_AS_FLOAT(x) MAX(MIN((x), LAYOUT_FIXED_AS_FLOAT_SAFE_MAX), -LAYOUT_FIXED_AS_FLOAT_SAFE_MAX)

#ifdef _DEBUG
# include "modules/layout/layout_fixed_point_debug.h"
#else // !_DEBUG
typedef int LayoutFixed;

/** The type is used to temporarily store LayoutFixed value or
	ordinary integer value. In debug mode it becomes PixelOrLayoutFixedDebug. */
typedef int PixelOrLayoutFixed;
#endif // _DEBUG

inline LayoutFixed IntToLayoutFixed(int value)
{
	return LayoutFixed(value * LAYOUT_FIXED_POINT_BASE);
}

#define LAYOUT_FIXED_HUNDRED_PERCENT IntToLayoutFixed(100)

inline LayoutFixed FloatToLayoutFixed(float value)
{
	return LayoutFixed(OpRound(value * LAYOUT_FIXED_POINT_BASE));
}

inline LayoutFixed FloatToLayoutFixed(double value)
{
	return LayoutFixed(OpRound(value * LAYOUT_FIXED_POINT_BASE));
}

/** Convert a non negative layout fixed point value to an integer.
	Round to closest one. */

inline int LayoutFixedToIntNonNegative(LayoutFixed value)
{
	OP_ASSERT(value >= 0);

	return (value + LAYOUT_FIXED_POINT_BASE / 2) / LAYOUT_FIXED_POINT_BASE;
}

/** Convert a layout fixed point value to an integer.
	Round towards zero. */

inline int LayoutFixedToIntRoundDown(LayoutFixed value)
{
	return value / LAYOUT_FIXED_POINT_BASE;
}

/** Convert a layout fixed point value to an integer.
	Round to closest one. */

inline int LayoutFixedToInt(LayoutFixed value)
{
	return value >= 0 ? LayoutFixedToIntNonNegative(value) :
		(value - LAYOUT_FIXED_POINT_BASE / 2) / LAYOUT_FIXED_POINT_BASE;
}

/** Convert a non negative layout fixed point value to an integer.
	Round to closest one. */

inline short LayoutFixedToShortNonNegative(LayoutFixed value)
{
	OP_ASSERT(LayoutFixedToIntNonNegative(value) <= SHRT_MAX);
	return short(LayoutFixedToIntNonNegative(value));
}

inline float LayoutFixedToFloat(LayoutFixed value)
{
	return float(value) / LAYOUT_FIXED_POINT_BASE;
}

#ifdef SVG_SUPPORT
inline SVGNumber LayoutFixedToSVGNumber(LayoutFixed value)
{
	return SVGNumber(LayoutFixedToFloat(value));
}
#endif // SVG_SUPPORT

/** Convert integer percentage to layout fixed point percentage
	stored in LayoutCoord. It will be the opposite integer to the one
	that would be stored as LayoutFixed. This is the way we store
	a layout fixed point percentages in LayoutCoord type sometimes. */

inline LayoutCoord IntToFixedPointPercentageAsLayoutCoord(int percent)
{
	return LayoutCoord(-IntToLayoutFixed(percent));
}

inline int LayoutFixedAsInt(LayoutFixed value)
{
	return value;
}

inline LayoutCoord LayoutFixedAsLayoutCoord(LayoutFixed value)
{
	return LayoutCoord(value);
}

/** Multiply two layout fixed values in mathematical way. */

inline LayoutFixed LayoutFixedMult(LayoutFixed a, LayoutFixed b)
{
	return (a * b) / LAYOUT_FIXED_POINT_BASE;
}

/** Calculate a percent from a base value. Round towards zero. */

inline int LayoutFixedPercentageOfInt(LayoutFixed percent, int base)
{
	return (percent * LayoutFixed(base)) / LAYOUT_FIXED_HUNDRED_PERCENT;
}

/** Calculate a percent from a base value. Round towards zero. */

inline LayoutCoord PercentageToLayoutCoord(LayoutFixed percent, LayoutCoord base)
{
	double res = double(percent) / 100 * base;
	res = Round(res, 0, ROUND_NORMAL) / LAYOUT_FIXED_POINT_BASE;
	return LayoutCoord(int(CONSTRAIN_LAYOUT_COORD(res)));
}

/** Calculate a percent from a non negative base value. Round towards +infinity. */

inline LayoutCoord PercentageToLayoutCoordCeil(LayoutFixed percent, LayoutCoord base)
{
	OP_ASSERT(base >= 0);
	double res = double(percent) / LAYOUT_FIXED_HUNDRED_PERCENT * base;
	return LayoutCoord(int(op_ceil(CONSTRAIN_LAYOUT_COORD(res))));
}

/** Calculate a percent from a base value. Round towards zero.
	Allows to specify a percentage value that is treated as a whole. */

inline LayoutCoord PercentageOfPercentageToLayoutCoord(LayoutFixed percent, LayoutCoord base, LayoutFixed whole)
{
	double res = double(percent) / double(whole) * LAYOUT_FIXED_POINT_BASE * base;
	res = Round(res, 0, ROUND_NORMAL) / LAYOUT_FIXED_POINT_BASE;
	return LayoutCoord(int(CONSTRAIN_LAYOUT_COORD(res)));
}

/** Calculate a combined percent from a base value.
	Round towards zero. Combines two percentages. */

inline LayoutCoord DoublePercentageToLayoutCoord(LayoutFixed percent1, LayoutFixed percent2, LayoutCoord base)
{
	double res = (double(percent1) / 100) * (double(percent2) / LAYOUT_FIXED_HUNDRED_PERCENT) * base;
	res = Round(res, 0, ROUND_NORMAL) / LAYOUT_FIXED_POINT_BASE;
	return LayoutCoord(int(CONSTRAIN_LAYOUT_COORD(res)));
}

/** Having non negative reverse_base and percent, calculate base, such that
	percent * base = reverse_base. Round towards +infinity. */

inline LayoutCoord ReversePercentageToLayoutCoord(LayoutFixed percent, LayoutCoord reverse_base)
{
	OP_ASSERT(reverse_base >= 0);
	double res = double(reverse_base) / double(percent) * LAYOUT_FIXED_HUNDRED_PERCENT;
	return LayoutCoord(int(op_ceil(CONSTRAIN_LAYOUT_COORD(res))));
}

/** Like ReversePercentageToLayoutCoord, but has third param that is treated as
	the whole instead of 100%. Round towards +infinity. */

inline LayoutCoord ReversePercentageOfPercentageToLayoutCoord(LayoutFixed percent, LayoutCoord reverse_base, LayoutFixed whole)
{
	OP_ASSERT(reverse_base >= 0);
	double res = double(reverse_base) / double(percent) * double(whole);
	return LayoutCoord(int(op_ceil(CONSTRAIN_LAYOUT_COORD(res))));
}

/** Having reverse_base and percent, calculate base, such that
	percent * base = reverse_base. Round towards zero. */

inline LayoutCoord ReversePercentageToLayoutCoordRoundDown(LayoutFixed percent, LayoutCoord reverse_base)
{
	double res = double(reverse_base) / double(percent) * LAYOUT_FIXED_HUNDRED_PERCENT;
	return LayoutCoord(int(CONSTRAIN_LAYOUT_COORD(res)));
}

/** Multiply layout fixed point value by n/d (in mathematical way). */

inline LayoutFixed MultLayoutFixedByQuotient(LayoutFixed value, int n, int d)
{
	return (value * n) / d;
}

#define SHORT_AS_LAYOUT_FIXED_POINT_MAX (SHRT_MAX * LAYOUT_FIXED_POINT_BASE)
#define SHORT_AS_LAYOUT_FIXED_POINT_MIN (SHRT_MIN * LAYOUT_FIXED_POINT_BASE)

#define NORMAL_LINE_HEIGHT_FACTOR_I FloatToLayoutFixed(NORMAL_LINE_HEIGHT_FACTOR)

#endif // !LAYOUT_FIXED_POINT_H
