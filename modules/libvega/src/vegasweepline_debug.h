/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGA_SWEEP_LINE_DEBUG_H
#define VEGA_SWEEP_LINE_DEBUG_H

# ifdef _DEBUG
// enable to allow dumping debug images for sweepline - see specifics below
// #  define VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
# endif // _DEBUG

# ifdef VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT
#  include "modules/libvega/vegarenderer.h"
#  include "modules/pi/OpBitmap.h"

#  define VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_WIDTH	768
#  define VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_HEIGHT	768
#  define VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_PAD_X	16
#  define VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_PAD_Y	16
#  define VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT_FOLDER "/home/wonko/tmp/dump/"

// NOTE: these don't play well together
// #  define VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT
// #  define VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE

#  if defined(VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT) && defined(VEGA_DEBUG_SELF_INTERSECT_IMAGE_OUTPUT_OUTLINE)
#   warning triangulation and self-intersection debug image dumping do not play well together
#  endif  // !VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT
# endif // VEGA_DEBUG_SWEEP_LINE_IMAGE_OUTPUT

# ifdef VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT
#  define VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_OUTLINE
#  define VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRAPEZOID_CHAINS
#  define VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT_TRIANGLE_RUNS
# endif // VEGA_DEBUG_TRIANGULATION_IMAGE_OUTPUT

// enable asserts that may or may not indicate a problem:
//
// * when two different vertices with the exact same coordinates are
//   encountered in VEGATriangulator::determineVertexOrder: this can
//   indicate broken edge ordering, but can also trigger for correct
//   behavior (paths in eg 'weird e' or 'linked diamonds' selftests).
//
// * when number of vertices collected from a trapezoid run is less
//   than three: this can happen due to extra edges introduced by
//   reflex vertices (eg 'weird e' tc) or when subareas are connected
//   by a path with no width (eg 'linked rects'). for these trapezoid
//   runs no triangles will be produced. preferably this problem
//   should be fixed in some other way.
//
// #define VEGA_DEBUG_TRIANGULATION_DUBIOUS_ASSERTS

// enable support for paths with "empty" stretches. (see eg "linked*"
// selftests for what that means.) these are problematic because the
// number of triangles produced doesen't equal *number of edges* - 2,
// which means almost any bug in the triangulation code (not just for
// paths with "empty" stretches) will go unnoticed.
//
 #define VEGA_TRIANGULATION_SUPPORT_EMPTY_STRETCHES

#endif  // !VEGA_SWEEP_LINE_DEBUG_H
