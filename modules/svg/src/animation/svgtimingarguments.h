/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_TIMING_ARGUMENTS_H
#define SVG_TIMING_ARGUMENTS_H

class SVGVector;

#include "modules/svg/src/animation/svganimationtime.h"

#include "modules/svg/src/SVGRepeatCountObject.h"
#include "modules/svg/src/SVGAttribute.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGVector.h"

class SVGAnimationSchedule;
class SVGAnimationWorkplace;
class SVGTimingInterface;
class OpBpath;
class SVGRotate;

/**
 * This structure contains timing related values extracted from a timed element.
 *
 * The purpose of this structure is to allow a timed_element to avoid
 * looking up these values for every animation frame that is
 * evaluated, since that can be costly.
 */
struct SVGTimingArguments
{
	SVGTimingArguments() :
		begin_list(NULL),
		end_list(NULL) {
		packed_init = 0;
	}

	~SVGTimingArguments() { SVGObject::DecRef(begin_list); SVGObject::DecRef(end_list); }

	void SetBeginList(SVGVector* begin) { SVGObject::DecRef(begin_list); SVGObject::IncRef(begin_list = begin); }
	/**< Set the begin list corresponding to the timed
	 * element. */

	SVGVector* GetBeginList() { return begin_list; }
	/**< Get the begin list that is part of the timing arguments. */

	void SetEndList(SVGVector* end) { SVGObject::DecRef(end_list); SVGObject::IncRef(end_list = end); }
	/**< Set the end list corresponding to the timed
	 * element. */

	SVGVector* GetEndList() { return end_list; }
	/**< Get the end list that is part of the timing arguments. */

	SVG_ANIMATION_TIME simple_duration;

	SVG_ANIMATION_TIME active_duration_min;
	SVG_ANIMATION_TIME active_duration_max;

	// SVGAnimationTime::Unresolved() means unspecified, which have no
	// corresponding value
	SVG_ANIMATION_TIME repeat_duration;
	SVGRepeatCount repeat_count;

	SVGRestartType restart_type;

	union
	{
		struct {
			unsigned int begin_is_empty:1; // We track non-existing
										   // begin attributes so that
										   // we can insert the
										   // default value
		} packed;
		unsigned int packed_init;
	};

private:

	SVGVector *begin_list;
	SVGVector *end_list;
};

#endif // !SVG_TIMING_ARGUMENTS_H
