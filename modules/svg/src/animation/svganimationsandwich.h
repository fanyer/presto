/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_SANDWICH_H
#define SVG_ANIMATION_SANDWICH_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svgtimingarguments.h"
#include "modules/svg/src/animation/svganimationarguments.h"
#include "modules/svg/src/animation/svganimationcalculator.h"
#include "modules/svg/src/animation/svganimationlistitem.h"

struct SVGAnimationSlice : public ListElement<SVGAnimationSlice>
{
	SVGAnimationInterface* animation;
	SVGTimingArguments timing_arguments;
	SVGAnimationArguments animation_arguments;
	SVGAnimationCalculator::AnimationValueCache from_value_cache;
	SVGAnimationCalculator::AnimationValueCache to_value_cache;
};

/**
 * This class represents one "animation sandwich" from the SMIL 2.1
 * specification.
 *
 * Its responsibilities lies in updating the SVGAttribute it
 * corresponds to with the current value at a specific point in time.
 *
 * It knows about one SVGAnimationTarget representing the element
 * being animated.  Multiple SVGAnimationSandwiches can point to a
 * single SVGAnimationTarget, one for each animated attribute.
 *
 * It knows about one or more SVGAnimationSchedules, one for each
 * animation element that takes part in this animation sandwich.
 */
class SVGAnimationSandwich : public SVGAnimationListItem
{
public:
	SVGAnimationSandwich(SVGAnimationTarget *target, SVGAnimationAttributeLocation attribute_location) :
		target(target), attribute_location(attribute_location) {}

	virtual ~SVGAnimationSandwich();

	virtual BOOL MatchesTarget(SVGAnimationTarget *target, SVGAnimationAttributeLocation attribute_location);

	virtual void InsertSlice(SVGAnimationSlice* animation_slice,
							 SVG_ANIMATION_TIME document_time);

	virtual void Reset(SVG_ANIMATION_TIME document_time);

	virtual OP_STATUS UpdateIntervals(SVGAnimationWorkplace *animation_workplace);

	virtual OP_STATUS CommitIntervals(SVGAnimationWorkplace *animation_workplace,
									  BOOL send_events,
									  SVG_ANIMATION_TIME document_time,
									  SVG_ANIMATION_TIME elapsed_time,
									  BOOL at_prezero);

	virtual OP_STATUS UpdateValue(SVGAnimationWorkplace* animation_workplace, SVG_ANIMATION_TIME document_time);

	virtual SVG_ANIMATION_TIME NextAnimationTime(SVG_ANIMATION_TIME document_time) const;

	virtual SVG_ANIMATION_TIME NextIntervalTime(SVG_ANIMATION_TIME document_time, BOOL at_prezero) const;

	virtual OP_STATUS HandleAccessKey(uni_char access_key, SVG_ANIMATION_TIME document_time);

	virtual OP_STATUS Prepare(SVGAnimationWorkplace* animation_workplace);

	virtual void RemoveElement(SVGAnimationWorkplace* animation_workplace, HTML_Element* element, BOOL& remove_item);

	virtual void UpdateTimingParameters(HTML_Element* element);

	virtual void Deactivate();

	virtual HTML_Element *TargetElement() const { return target->TargetElement(); }

	virtual OP_STATUS RecoverFromError(SVGAnimationWorkplace *animation_workplace, HTML_Element *element);

	virtual void MarkPropsDirty() { }

private:

	/** Check if a given animation item has higher priority than another. */

	static BOOL	HasHigherPriority(SVG_ANIMATION_TIME document_time,
								  SVGAnimationSlice &item,
								  SVGAnimationSchedule &new_schedule,
								  SVG_ANIMATION_TIME new_begin_time,
								  HTML_Element *new_elm);

	/** Helper method to retrieve the base object from an SVG
		attribute */

	SVGObject* GetBaseObject(SVGAnimationAttributeLocation& location);

	/** Helper method to retrieve the animation object from an SVG
		attribute. Returns NULL on error.  If non-NULL is returned,
		computed_field is set to the field of the attribute where
		animation object was taken from. */

	OP_STATUS GetAnimationObject(HTML_Element* target_elm,
								 SVGAttribute*& attribute,
								 SVGObject*& obj,
								 SVGAnimationAttributeLocation& location,
								 SVGAttributeField &computed_field);

	/** Helper method to retrieve an object from an SVG attribute. */

	SVGObject* GetObject(SVGAnimationAttributeLocation& location,
						 SVGAttribute* attribute,
						 SVGAttributeField field);

	/** The target element of this animation */

	SVGAnimationTarget* target;

	/* The attribute at the element of this animation */

	SVGAnimationAttributeLocation attribute_location;

	/** Intervals that has begun and that hasn't yet ended or is in
		fill="freeze" mode.

		FIXME: Prune the active_animations from useless animations,
		where a useless animation is a lower-priority animation under
		an higher-priority non-additive animation. */

	List<SVGAnimationSlice> animations;
};

#endif // SVG_SUPPORT
#endif // !SVG_ANIMATION_SANDWICH_H
