/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_TIME_H
#define SVG_ANIMATION_TIME_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGObject.h"

#if defined(HAVE_INT64)
typedef INT64 SVG_ANIMATION_TIME;
# if defined(_MSC_VER) && _MSC_VER <= 1200
#  define SUFFIX_64(X) X##I64
# else
#  define SUFFIX_64(X) X##LL
# endif
# ifndef INT64_MAX
#  define INT64_MAX SUFFIX_64(9223372036854775807)
# endif
# ifndef INT64_MIN
#  define INT64_MIN (-INT64_MAX-SUFFIX_64(1))
# endif
#else
typedef int SVG_ANIMATION_TIME;
#endif

class SVGAnimationTime
{
public:
#ifdef HAVE_INT64
	static SVG_ANIMATION_TIME Unresolved() { return INT64_MAX; }
	static SVG_ANIMATION_TIME Indefinite() { return INT64_MAX - 1; }
	static SVG_ANIMATION_TIME Latest() { return INT64_MAX - 2; }
	static SVG_ANIMATION_TIME Earliest() { return INT64_MIN; }
#else
	static SVG_ANIMATION_TIME Unresolved() { return INT_MAX; }
	static SVG_ANIMATION_TIME Indefinite() { return INT_MAX - 1; }
	static SVG_ANIMATION_TIME Latest() { return INT_MAX - 2; }
	static SVG_ANIMATION_TIME Earliest() { return INT_MIN; }
#endif

	static double ToSeconds(SVG_ANIMATION_TIME animation_time) { return ((double)animation_time) / 1000.0; }
	static SVG_ANIMATION_TIME FromSeconds(double seconds) { return (SVG_ANIMATION_TIME)(seconds * 1000.0); }

	static BOOL IsNumeric(SVG_ANIMATION_TIME animation_time) { return animation_time < Indefinite(); }
	static BOOL IsIndefinite(SVG_ANIMATION_TIME animation_time) { return animation_time == Indefinite(); }
	static BOOL IsUnresolved(SVG_ANIMATION_TIME animation_time) { return animation_time == Unresolved(); }
};

/**
 * Simple wrapper object for animation times. Used for storing durations,
 * clock-values and the like in attributes of animation elements.
 */
class SVGAnimationTimeObject : public SVGObject
{
public:
	/* The clock-value in milli-seconds relative to the beginning of
	 * the animation */
	SVG_ANIMATION_TIME value;

  	SVGAnimationTimeObject(SVG_ANIMATION_TIME v) :
		SVGObject(SVGOBJECT_ANIMATION_TIME),
		value(v) {}

	virtual SVGObject *Clone() const;
	virtual BOOL IsEqual(const SVGObject& other) const;

	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
};

#endif // SVG_SUPPORT
#endif // !SVG_ANIMATION_TIME_H
