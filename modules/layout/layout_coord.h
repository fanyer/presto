/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef LAYOUT_COORD_H
#define LAYOUT_COORD_H

#ifdef _DEBUG
# define LAYOUT_DEBUG_COORDS
#endif

// Define layout coordinates and associated utility macros

#ifdef LAYOUT_DEBUG_COORDS
# include "layout_coord_debug.h"
#else
typedef int LayoutCoord;
#define LAYOUT_COORD_MAX INT_MAX
#define LAYOUT_COORD_MIN INT_MIN
#endif

struct LayoutPoint
{
	LayoutCoord x;
	LayoutCoord y;
};

#define LAYOUT_COORD_HALF_MAX (LAYOUT_COORD_MAX / LayoutCoord(2))
#define LAYOUT_COORD_HALF_MIN (LAYOUT_COORD_MIN / LayoutCoord(2))

/* Calculate the square root and round to closest integer.

   Extra int cast needed in debug mode because of 'possible loss of
   data' warning when going from double to int. In release mode it is
   an explicit cast already, but having two won't hurt much. */

static inline LayoutCoord LayoutSqrt(LayoutCoord v) { return LayoutCoord(int(0.5 + op_sqrt(double(v)))); }

#endif // !LAYOUT_COORD_H
