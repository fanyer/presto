/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_CALCULATOR_H
#define SVG_ANIMATION_CALCULATOR_H

#ifdef SVG_SUPPORT

class SVGAnimationValue;
class SVGMotionPath;
class SVGCoordinatePairObject;

#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/animation/svgtimingarguments.h"
#include "modules/svg/src/animation/svganimationinterval.h"
#include "modules/svg/src/animation/svganimationvalue.h"

#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGNumberPair.h"
#include "modules/util/simset.h"

class SVGAnimationCalculator
{
public:
	class AnimationValueCache
	{
	public:
		AnimationValueCache() : cached_object(NULL), source_object(NULL) {}
		~AnimationValueCache() { SVGObject::DecRef(cached_object); SVGObject::DecRef(source_object); }
		static AnimationValueCache NoCache() { return AnimationValueCache(); }

		void SetCache(SVGObject *c) { SVGObject::DecRef(cached_object); cached_object = c; SVGObject::IncRef(cached_object); }
		void SetSource(SVGString *s) { SVGObject::DecRef(source_object); source_object = s; SVGObject::IncRef(source_object); }

		SVGObject *CachedObject() { return cached_object; }
		SVGString *SourceString() { return source_object; }

	private:
		SVGObject *cached_object;
		SVGString *source_object;
	};

    SVGAnimationCalculator(SVGAnimationWorkplace *animation_workplace,
						   SVGAnimationInterface *animation_interface,
						   SVGAnimationTarget *animation_target,
						   SVGTimingArguments &timing_arguments,
						   SVGAnimationArguments &animation_arguments,
						   AnimationValueCache &from_value_cache,
						   AnimationValueCache &to_value_cache) :
		animation_workplace(animation_workplace),
		animation_interface(animation_interface),
		animation_target(animation_target),
		timing_arguments(timing_arguments),
		animation_arguments(animation_arguments),
		from_value_cache(from_value_cache),
		to_value_cache(to_value_cache) {}

	/** Apply animation specified by arguments to the animation value,
		building upon the base value. */

    OP_STATUS ApplyAnimationAtPosition(SVG_ANIMATION_INTERVAL_POSITION interval_position,
									   SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
									   SVGAnimationValue &animation_value,
									   const SVGAnimationValue &base_value,
									   SVGAnimationValueContext &context);

    /** Undocumented */

	SVGTimingArguments &TimingArguments() { return timing_arguments; }

private:

    OP_STATUS TypeCastValue(const SVGAnimationValue &from, SVGAnimationValue &to,
							SVGObjectContainer& to_container,
							SVGAnimationValue::ReferenceType reference_type,
							SVGAnimationValueContext &context,
							AnimationValueCache &cache);

    OP_STATUS LowApplyAnimationAtPosition(SVG_ANIMATION_INTERVAL_POSITION interval_position,
										  SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
										  SVGAnimationValue &animation_value,
										  const SVGAnimationValue &base_value,
										  SVGAnimationValueContext &context);

    enum ValueMode {
		MODE_NORMAL,
		/**<  Use 'from' and 'to' as normal
		 */

		MODE_BOTH_FROM_BY,
		/**< The 'to' value is really 'by' and should be added to
		 * 'from' to get the computed 'to'
		 */

		MODE_ONLY_BY,
		/**< The additivness is forced by a 'by' without
		 * 'from'. 'from' is empty and 'base' should be used as
		 * from. 'to' is the 'by' value and the base value must be
		 * added to it to get the final 'to'
		 */

		MODE_ONLY_TO
		/**< 'to' is used without 'from', use base_value as 'from'
		 */
    };

	OP_STATUS ApplyValueDiscretly(SVGAnimationValueContext &context,
								  SVG_ANIMATION_INTERVAL_POSITION interval_position,
								  ValueMode value_mode,
								  SVGAnimationValue &from_value,
								  SVGAnimationValue &to_value,
								  const SVGAnimationValue &base_value,
								  SVGAnimationValue &animation_value);

	OP_STATUS ApplyValueInterpolate(SVGAnimationValueContext &context,
									SVG_ANIMATION_INTERVAL_POSITION interval_position,
									ValueMode value_mode,
									const SVGAnimationValue &from_value,
									const SVGAnimationValue &to_value,
									const SVGAnimationValue &base_value,
									SVGAnimationValue &animation_value);


    BOOL FindRelevantAnimationValues(SVG_ANIMATION_INTERVAL_POSITION interval_position,
									 SVGAnimationValue &from_value,
									 SVGAnimationValue &to_value,
									 SVGAnimationValue::ReferenceType reference_type,
									 SVGAnimationValueContext &context,
									 SVG_ANIMATION_INTERVAL_POSITION &sub_interval_position,
									 ValueMode &special_mode);

    BOOL FindAccumulationValue(SVGAnimationValueContext &context,
							   ValueMode &value_mode,
							   SVGAnimationValue &accumulation_value_1,
							   SVGAnimationValue &accumulation_value_2);

    void FindRelevantAnimationValuesCalcModeLinear(SVG_ANIMATION_INTERVAL_POSITION interval_position,
												   SVGObject *&from, SVGObject *&to,
												   unsigned &sub_interval_index,
												   SVG_ANIMATION_INTERVAL_POSITION &sub_interval_position);

    void FindRelevantAnimationValuesCalcModeDiscrete(SVG_ANIMATION_INTERVAL_POSITION interval_position,
													 SVGObject *&from);

    OP_STATUS FindRelevantAnimationValuesCalcModePaced(SVG_ANIMATION_INTERVAL_POSITION interval_position,
													   SVGAnimationValue::ReferenceType reference_type,
													   SVGAnimationValueContext &context,
													   SVGObject *&from, SVGObject *&to,
													   SVG_ANIMATION_INTERVAL_POSITION &sub_interval_position);

	OP_STATUS HandleAccumulate(SVGAnimationValueContext &context,
							   SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
							   SVGAnimationValue::ReferenceType reference_type,
							   SVGAnimationValue &animation_value);

	OP_STATUS HandleAdditive(ValueMode value_mode,
							 const SVGAnimationValue &base_value,
							 SVGAnimationValue &animation_value);

	OP_STATUS CheckMotionPath(SVGAnimationValueContext &context);

	OP_STATUS ApplyValueMotionPath(SVGAnimationValueContext &context,
								   SVG_ANIMATION_INTERVAL_POSITION interval_position,
								   SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
								   const SVGAnimationValue &base_value,
								   SVGAnimationValue &animation_value);

	HTML_Element* FindMPathElement() const;

	static OP_STATUS SetMotionPathFromPath(SVGMotionPath *motion_path, OpBpath *path_impl, float path_length);

	static OP_STATUS SetMotionPathFromSimplified(SVGAnimationValueContext &context, SVGMotionPath *motion_path,
												 SVGObject *from, SVGObject *to, SVGObject *by, BOOL &is_by_animation);

	static OP_STATUS SetMotionPathFromValues(SVGAnimationValueContext &context, SVGMotionPath *motion_path, SVGVector *values);

	static SVGNumberPair ResolveCoordinatePair(SVGAnimationValueContext &context,
											   const SVGObject &object);

	SVGAnimationWorkplace *animation_workplace;
	SVGAnimationInterface *animation_interface;
	SVGAnimationTarget *animation_target;

    SVGTimingArguments &timing_arguments;
    SVGAnimationArguments &animation_arguments;

	AnimationValueCache &from_value_cache;
	AnimationValueCache &to_value_cache;
};

#endif // SVG_SUPPORT
#endif // !SVG_ANIMATION_CALCULATOR_H
