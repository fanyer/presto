/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_ANIMATION_INTERVAL_H
#define SVG_ANIMATION_INTERVAL_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationtime.h"
#include "modules/svg/src/SVGInternalEnum.h"

typedef int SVG_ANIMATION_INTERVAL_REPETITION;

/**
 * Simple datacontainer class to represent a interval. The active and
 * previous intervals are stored by the animation contexts.
 */
class SVGAnimationInterval
{
private:
	SVG_ANIMATION_TIME begin;
	/**< The begin time of the interval */

	SVG_ANIMATION_TIME end;
	/**< The end time of the interval. The difference between end and
	 * begin is the 'active duration'. If end is indefinite, the
	 * active duration is indefinite.*/

	SVG_ANIMATION_TIME repeating_duration;
	/**< The duration in which the animation repeats, unaffected by
	 * the active duration (and thus min). */

	SVG_ANIMATION_TIME simple_duration;
	/**< The length of the simple duration. For indefinite durations
	 * no value interpolation inside the interval is possible. */

public:
	SVG_ANIMATION_TIME Begin() const { return begin; }
	SVG_ANIMATION_TIME End() const { return end; }
	SVG_ANIMATION_TIME SimpleDuration() const { return simple_duration; }
	SVG_ANIMATION_TIME RepeatingDuration() const { return repeating_duration; }

	void SetEnd(SVG_ANIMATION_TIME new_end) { end = new_end; }
	void SetSimpleDuration(SVG_ANIMATION_TIME new_simple_duration) { simple_duration = new_simple_duration; }

	/**
	 * Constructor
	 */
	SVGAnimationInterval(SVG_ANIMATION_TIME begin,
						 SVG_ANIMATION_TIME end,
						 SVG_ANIMATION_TIME repeat_duration,
						 SVG_ANIMATION_TIME simple_duration);

	BOOL IsActiveDurationIndefinite() const { return end == SVGAnimationTime::Indefinite(); }
	BOOL IsSimpleDurationIndefinite() const { return simple_duration == SVGAnimationTime::Indefinite(); }

	/**
	 * Check if an instance is inside the interval
	 */
	BOOL IsInside(SVG_ANIMATION_TIME animation_time) const
	{
		return animation_time >= begin && animation_time < end;
	}

	BOOL IsActive(SVG_ANIMATION_TIME animation_time, SVGAnimateFillType fill_mode) const;

	/**
	 * Compute the relative distance between start and end that the
	 * absPosition represents.
	 * Example:
	 * Let start = 4, end = 10 and absPosition = 7.
	 * then the result would be (7-4)/(10-4) = 1/2.
	 *
	 * @param animation_time Milliseconds since the documented started
	 *
	 * @param result A normalized number representing the position in
	 * the simple duration is saved here.
	 *
	 * @param rep The repetition number of the result. If the simple
	 * duration repeats, the repeat number is saved here
	 * (zero-based). Otherwise it is zero, as in the first repitition.
	 */
	void CalculateIntervalPosition(SVG_ANIMATION_TIME animation_time,
								   SVG_ANIMATION_INTERVAL_POSITION& result,
								   SVG_ANIMATION_INTERVAL_REPETITION& rep);

	/**
	 * Find out the final parameter value for this interval
	 */
	SVG_ANIMATION_INTERVAL_POSITION FinalParameterValue();
};

#endif // SVG_SUPPORT
#endif // SVG_ANIMATION_INTERVAL_H
