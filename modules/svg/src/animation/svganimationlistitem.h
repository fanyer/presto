/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2010 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_LIST_ITEM_H
#define SVG_ANIMATION_LIST_ITEM_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationtime.h"
#include "modules/svg/src/animation/svganimationattributelocation.h"

struct SVGAnimationSlice;
class SVGAnimationTarget;
class SVGAnimationWorkplace;
class SVGDocumentContext;

/**
 * Common interface for use by the animation engine to control both
 * simple timelines (video, audio, etc) and full blown animation
 * elements.
 */
class SVGAnimationListItem : public ListElement<SVGAnimationListItem>
{
public:
	virtual ~SVGAnimationListItem() {}

	virtual void InsertSlice(SVGAnimationSlice* animation_slice,
							 SVG_ANIMATION_TIME document_time) = 0;
	/** Insert animation slice. */

	virtual BOOL MatchesTarget(SVGAnimationTarget *target, SVGAnimationAttributeLocation attribute_location) = 0;
	/** Does the animation animate a specific target? */

	virtual void Reset(SVG_ANIMATION_TIME document_time) = 0;
	/**< Resets associated animation schedules.  This restores the
	 * schedules to the state before animations has begun. */

	virtual OP_STATUS UpdateIntervals(SVGAnimationWorkplace *animation_workplace) = 0;
	/**< Updates the intervals on associated animation schedules to
	 * current time. Recursively updates intervals on schedules that
	 * depend on this schedule, using simple flag-setting. */

	virtual OP_STATUS CommitIntervals(SVGAnimationWorkplace *animation_workplace,
									  BOOL send_events,
									  SVG_ANIMATION_TIME document_time,
									  SVG_ANIMATION_TIME elapsed_time,
									  BOOL at_prezero) = 0;
	/** Commit to the current interval. This means that repeat events,
	 * DOM events, etc. can be sent. */

	virtual OP_STATUS UpdateValue(SVGAnimationWorkplace* animation_workplace, SVG_ANIMATION_TIME document_time) = 0;
	/**< Update the value to the corresponding final value of this
	 * animation sandwich. */

	virtual SVG_ANIMATION_TIME NextAnimationTime(SVG_ANIMATION_TIME document_time) const = 0;
	/** Return the next time this animation is active */

	virtual SVG_ANIMATION_TIME NextIntervalTime(SVG_ANIMATION_TIME document_time, BOOL at_prezero) const = 0;
	/** Return the next time the interval may need to be recomputed */

	virtual OP_STATUS HandleAccessKey(uni_char access_key, SVG_ANIMATION_TIME animation_time) = 0;
	/** Handle access key */

	virtual OP_STATUS Prepare(SVGAnimationWorkplace* animation_workplace) = 0;
	/** Prepare the animation list item. */

	virtual void RemoveElement(SVGAnimationWorkplace* animation_workplace, HTML_Element* element, BOOL& remove_item) = 0;
	/** Remove handling of element. If the out parameter 'remove_item'
	 * is TRUE this animation item is empty and should be removed.	*/

	virtual void UpdateTimingParameters(HTML_Element* element) = 0;
	/** Re-read timing parameters from timing element */

	virtual void Deactivate() = 0;
	/** Deactivate list item.  For animation items, this means that
		the attribute should be restored to the base value. */

	virtual HTML_Element *TargetElement() const = 0;
	/** Return the "target" element for the list item.  For SMIL
		animations, this is normally the animation target.  For
		external timelines it is the element itself. */

	virtual OP_STATUS RecoverFromError(SVGAnimationWorkplace *animation_workplace, HTML_Element *element) = 0;
	/** Let element try to recover from a previous error. */

	virtual void MarkPropsDirty() = 0;
};

#endif // SVG_SUPPORT
#endif // !SVG_ANIMATION_LIST_ITEM_H
