/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LAYOUT_DEBUG_UTILS_H
#define LAYOUT_DEBUG_UTILS_H

/* Enable this flag to get runtime debugging of layout types
   (such as LayoutCoord and LayoutFixed) arithmetics
   (overflow detection for addition, subtraction and multiplication). */

#if 1
#define LAYOUT_TYPES_RUNTIME_DEBUG
#endif

#ifdef LAYOUT_TYPES_RUNTIME_DEBUG

inline void layout_type_check_add(int a, int b)
{
	/* Check overflow. */
	int result = a + b;
	BOOL positive_a = a >= 0;
	/* Overflow can't happen if signs of "a" and "b" are different.
	   Overflow happened if sign of sum does not match sign of "a". */
	if (positive_a == (b >= 0) && positive_a != (result >= 0))
		OP_ASSERT(!"Overflow");
}

inline void layout_type_check_sub(int a, int b)
{
	layout_type_check_add(a, -b);
}

inline void layout_type_check_mult(int a, int b)
{
	double res = double(a) * double(b);

	// Check whether we exceed int capacity.
	if (res > double(INT_MAX) || res < double(INT_MIN))
		OP_ASSERT(!"Overflow");
}

#endif // LAYOUT_TYPES_RUNTIME_DEBUG

#endif // LAYOUT_DEBUG_UTILS_H
