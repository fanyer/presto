/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationinterval.h"

SVGAnimationInterval::SVGAnimationInterval(SVG_ANIMATION_TIME b,
										   SVG_ANIMATION_TIME e,
										   SVG_ANIMATION_TIME rd,
										   SVG_ANIMATION_TIME d) :
	begin(b),
	end(e),
	repeating_duration(rd),
	simple_duration(d)
{
	OP_ASSERT(SVGAnimationTime::IsNumeric(begin));
}

void
SVGAnimationInterval::CalculateIntervalPosition(SVG_ANIMATION_TIME animation_time,
												SVG_ANIMATION_INTERVAL_POSITION& interval_position,
												SVG_ANIMATION_INTERVAL_REPETITION& interval_repetition)
{
	OP_ASSERT(SVGAnimationTime::IsNumeric(begin));
	OP_ASSERT(SVGAnimationTime::IsNumeric(animation_time));

	if (SVGAnimationTime::IsNumeric(simple_duration))
	{
		if (animation_time < begin)
		{
			interval_position = 0.0;
			interval_repetition = 0;
		}
		else if ((SVGAnimationTime::IsNumeric(repeating_duration) &&
				  animation_time >= (repeating_duration + begin)) ||
				 (animation_time >= end))

		{
			OP_ASSERT(SVGAnimationTime::IsNumeric(end));

			// If the repeat duration (the active duration in which
			// the animation repeats) is a even multiple of the simple
			// duration, we should use the normalized value 1.0 for
			// computing the frozen value. Otherwise, if the repeat
			// duration is not an even multiple, we choose the value
			// one millisecond before the end as frozen value. This
			// will make us stay within the subinterval of which the
			// animation was frozen.

			SVG_ANIMATION_TIME last_active_duration_time = (animation_time >= end) ?
				end - begin : repeating_duration;

			if (last_active_duration_time % simple_duration)
				last_active_duration_time--;

			interval_repetition = (SVG_ANIMATION_INTERVAL_REPETITION)(last_active_duration_time / simple_duration);
			interval_position = (SVG_ANIMATION_INTERVAL_POSITION)((last_active_duration_time % simple_duration)
																  / (SVG_ANIMATION_INTERVAL_POSITION)simple_duration);

			if (interval_position == 0.0)
			{
				interval_position = 1.0;
				interval_repetition--;
			}
		}
		else
		{
			SVG_ANIMATION_TIME interval_time = animation_time - begin;
			interval_repetition = (SVG_ANIMATION_INTERVAL_REPETITION)(interval_time / simple_duration);
			interval_position = (SVG_ANIMATION_INTERVAL_POSITION)((interval_time % simple_duration)
																  / (SVG_ANIMATION_INTERVAL_POSITION)simple_duration);
		}
	}
	else
	{
		// With indefinite simple duration we can't get futher than
		// this in the interval
		interval_repetition = 0;
		interval_position = 0.0;
	}
}

BOOL
SVGAnimationInterval::IsActive(SVG_ANIMATION_TIME animation_time, SVGAnimateFillType fill_mode) const
{
	if (animation_time < begin)
		return FALSE;
	else
	{
		if (fill_mode == SVGANIMATEFILL_FREEZE)
			return TRUE;

		const SVG_ANIMATION_TIME repeating_end = SVGAnimationTime::IsNumeric(repeating_duration) ?
			begin + repeating_duration : SVGAnimationTime::Indefinite();

		if (repeating_end > end && animation_time < end)
			return TRUE;

		if (repeating_end <= end && animation_time < repeating_end)
			return TRUE;

		return FALSE;
	}
}

#endif // SVG_SUPPORT
