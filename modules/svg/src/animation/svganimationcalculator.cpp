/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationarguments.h"
#include "modules/svg/src/animation/svganimationcalculator.h"
#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationschedule.h"
#include "modules/svg/src/animation/svganimationvalue.h"
#include "modules/svg/src/animation/svganimationvaluecontext.h"
#include "modules/svg/src/animation/svganimationtarget.h"

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGMotionPath.h"
#include "modules/svg/src/SVGTransform.h"

#include "modules/svg/src/parser/svglengthparser.h"

OP_STATUS
SVGAnimationCalculator::ApplyAnimationAtPosition(SVG_ANIMATION_INTERVAL_POSITION interval_position,
												 SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
												 SVGAnimationValue &animation_value,
												 const SVGAnimationValue &base_value,
												 SVGAnimationValueContext &context)
{
	OP_STATUS apply_status = LowApplyAnimationAtPosition(interval_position,
														 interval_repetition,
														 animation_value,
														 base_value,
														 context);

	if (apply_status == OpStatus::ERR_NOT_SUPPORTED)
	{
		SVGCalcMode old_calc_mode = animation_arguments.calc_mode;
		animation_arguments.calc_mode = SVGCALCMODE_DISCRETE;
		apply_status = LowApplyAnimationAtPosition(interval_position,
												   interval_repetition,
												   animation_value,
												   base_value,
												   context);
		animation_arguments.calc_mode = old_calc_mode;
	}

	return apply_status;
}

OP_STATUS
SVGAnimationCalculator::LowApplyAnimationAtPosition(SVG_ANIMATION_INTERVAL_POSITION interval_position,
													SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
													SVGAnimationValue &animation_value,
													const SVGAnimationValue &base_value,
													SVGAnimationValueContext &context)
{
	SVGAnimationValue::ReferenceType reference_type = animation_value.reference_type;

	if (animation_arguments.animation_element_type == Markup::SVGE_ANIMATE ||
		animation_arguments.animation_element_type == Markup::SVGE_ANIMATETRANSFORM ||
		animation_arguments.animation_element_type == Markup::SVGE_ANIMATECOLOR)
	{
		ValueMode value_mode;
		SVG_ANIMATION_INTERVAL_POSITION sub_interval_position;
		SVGAnimationValue original_from_value = SVGAnimationValue::Empty();
		SVGAnimationValue original_to_value = SVGAnimationValue::Empty();
		if (FindRelevantAnimationValues(interval_position, original_from_value,
										original_to_value, reference_type,
										context, sub_interval_position, value_mode))
		{
			SVGObjectContainer casted_from_value_container;
			SVGAnimationValue casted_from_value = SVGAnimationValue::Empty();

			SVGObjectContainer casted_to_value_container;
			SVGAnimationValue casted_to_value = SVGAnimationValue::Empty();

			if (value_mode == MODE_ONLY_BY &&
				animation_arguments.animation_element_type == Markup::SVGE_ANIMATETRANSFORM)
			{
				// Hack for animateTransform.  We can't handle the
				// additiveness in by-animations for animateTransform,
				// since the underlying value may be
				// un-interpolalatable with the base value. Instead we
				// flip back to normal mode, treat the from value as
				// zero and turn on additive mode.

				animation_arguments.additive_type = SVGADDITIVE_SUM;
				value_mode = MODE_NORMAL;
			}

			if (OpStatus::IsMemoryError(TypeCastValue(original_from_value,
													  casted_from_value,
													  casted_from_value_container,
													  reference_type, context, from_value_cache)) ||
				OpStatus::IsMemoryError(TypeCastValue(original_to_value,
													  casted_to_value,
													  casted_to_value_container,
													  reference_type, context, to_value_cache)))
			{
				return OpStatus::ERR_NO_MEMORY;
			}

			if (animation_arguments.calc_mode == SVGCALCMODE_DISCRETE)
			{
				RETURN_IF_ERROR(ApplyValueDiscretly(context, sub_interval_position,
													value_mode,
													casted_from_value,
													casted_to_value, base_value,
													animation_value));
			}
			else
			{
				RETURN_IF_ERROR(ApplyValueInterpolate(context, sub_interval_position,
													  value_mode,
													  casted_from_value,
													  casted_to_value, base_value,
													  animation_value));
			}

			if (interval_repetition > 0)
			{
				RETURN_IF_ERROR(HandleAccumulate(context, interval_repetition,
												 reference_type, animation_value));
			}

			RETURN_IF_ERROR(HandleAdditive(value_mode, base_value, animation_value));

			SVG_ANIMATION_NOTIFY_VALUE(animation_workplace,
									   animation_interface,
									   animation_arguments,
									   timing_arguments,
									   interval_position,
									   animation_value);

			return OpStatus::OK;
		}
		else
		{
			return OpSVGStatus::INVALID_ANIMATION;
		}
	}
	else if (animation_arguments.animation_element_type == Markup::SVGE_ANIMATEMOTION)
	{
		OP_STATUS status = ApplyValueMotionPath(context, interval_position,
												interval_repetition, base_value,
												animation_value);
		return status;
	}
	else if (animation_arguments.animation_element_type == Markup::SVGE_SET)
	{
		SVGObject *to = animation_arguments.GetTo();

		if (!to)
			return OpSVGStatus::INVALID_ANIMATION;

		SVGAnimationValue original_to_value = SVGAnimationValue::Empty();
		SVGAnimationValue::Initialize(original_to_value, to, context);

		SVGObjectContainer casted_to_value_container;
		SVGAnimationValue casted_to_value = SVGAnimationValue::Empty();

		if (OpStatus::IsMemoryError(TypeCastValue(original_to_value,
												  casted_to_value,
												  casted_to_value_container,
												  reference_type, context, to_value_cache)))
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		OP_STATUS assign_status = SVGAnimationValue::Assign(context, casted_to_value, animation_value);
		RETURN_IF_MEMORY_ERROR(assign_status);
		if (OpStatus::IsError(assign_status))
			return OpSVGStatus::INVALID_ANIMATION;

		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
	}
}

BOOL
SVGAnimationCalculator::FindRelevantAnimationValues(SVG_ANIMATION_INTERVAL_POSITION interval_position,
													SVGAnimationValue &from_value,
													SVGAnimationValue &to_value,
													SVGAnimationValue::ReferenceType reference_type,
													SVGAnimationValueContext &context,
													SVG_ANIMATION_INTERVAL_POSITION &sub_interval_position,
													ValueMode &value_mode)
{
    SVGObject *from = NULL;
    SVGObject *to =  NULL;
	SVGObject *by = NULL;

	unsigned sub_interval_index = 0;

    if (animation_arguments.GetValues())
    {
		if (animation_arguments.calc_mode == SVGCALCMODE_LINEAR ||
			animation_arguments.calc_mode == SVGCALCMODE_SPLINE)
		{
			FindRelevantAnimationValuesCalcModeLinear(interval_position, from, to, sub_interval_index, sub_interval_position);
		}
		else if (animation_arguments.calc_mode == SVGCALCMODE_PACED)
		{
			if (OpStatus::IsMemoryError(FindRelevantAnimationValuesCalcModePaced(interval_position, reference_type, context,
																				 from, to, sub_interval_position)))
			{
				return FALSE; // OOM
			}
		}
		else if (animation_arguments.calc_mode == SVGCALCMODE_DISCRETE)
		{
			FindRelevantAnimationValuesCalcModeDiscrete(interval_position, to);
			from = to;
			sub_interval_position = 0.0;
		}
    }
    else
    {
		from = animation_arguments.GetFrom();
		to = animation_arguments.GetTo();
		by = animation_arguments.GetBy();
		sub_interval_position = interval_position;

		if (animation_arguments.calc_mode == SVGCALCMODE_DISCRETE)
		{
			SVGVector *key_times = animation_arguments.GetKeyTimes();
			if (key_times && key_times->GetCount() == 2 && key_times->VectorType() == SVGOBJECT_NUMBER)
			{
				SVGNumberObject *time_begin = static_cast<SVGNumberObject*>(key_times->Get(0));
				// for discrete calcmode, the first keytimes value must be 0
				if (time_begin->value == 0)
				{
					SVGNumberObject *time_end = static_cast<SVGNumberObject*>(key_times->Get(1));
					SVG_ANIMATION_INTERVAL_POSITION interval_time = time_end->value.GetFloatValue();
					if (sub_interval_position >= interval_time)
						sub_interval_position = 1;
					else
						sub_interval_position = 0;
				}
			}
		}

		sub_interval_index = 0;
    }

	SVGVector* keySplines = animation_arguments.GetKeySplines();
	if (keySplines && keySplines->GetCount() > 0 && animation_arguments.calc_mode == SVGCALCMODE_SPLINE)
	{
		if (sub_interval_index < keySplines->GetCount() &&
			keySplines->VectorType() == SVGOBJECT_KEYSPLINE)
		{
			SVGKeySpline* spline = static_cast<SVGKeySpline*>(keySplines->Get(sub_interval_index));
			SVGNumber flatness(0.15);
			if (spline != NULL)
			{
				sub_interval_position = SVGMotionPath::CalculateKeySpline(spline,
																		  flatness,
																		  sub_interval_position).GetFloatValue();
			}
		}
		else
		{
			sub_interval_position = interval_position;
		}
	}

	if (from && to)
	{
		value_mode = MODE_NORMAL;
		return (SVGAnimationValue::Initialize(from_value, from, context) &&
				SVGAnimationValue::Initialize(to_value, to, context));
	}
	else if (from && by)
	{
		to = by;
		value_mode = MODE_BOTH_FROM_BY;
		return (SVGAnimationValue::Initialize(from_value, from, context) &&
				SVGAnimationValue::Initialize(to_value, to, context));
	}
	else if (by)
	{
		to = by;
		value_mode = MODE_ONLY_BY;
		return (SVGAnimationValue::Initialize(to_value, to, context));
	}
	else if (to)
	{
		value_mode = MODE_ONLY_TO;
		return (SVGAnimationValue::Initialize(to_value, to, context));
	}
	else
	{
		return FALSE;
	}
}

OP_STATUS
SVGAnimationCalculator::TypeCastValue(const SVGAnimationValue &from,
									  SVGAnimationValue &to,
									  SVGObjectContainer &to_container,
									  SVGAnimationValue::ReferenceType reference_type,
									  SVGAnimationValueContext &context,
									  AnimationValueCache &cache)
{
    if (from.reference_type != reference_type &&
		from.reference_type == SVGAnimationValue::REFERENCE_SVG_STRING)
    {
		SVGObject* result = NULL;

		if (cache.SourceString() == from.reference.svg_string)
		{
			result = cache.CachedObject();
			to_container.Set(result);
			SVGAnimationValue::Initialize(to, result, context);
			return OpStatus::OK;
		}

		const uni_char *str = from.reference.svg_string->GetString();
		unsigned str_length = from.reference.svg_string->GetLength();

		HTML_Element *element = animation_target->TargetElement();
		SVG_DOCUMENT_CLASS *doc = animation_workplace->GetSVGDocumentContext()->GetDocument();
		int attribute_name = animation_arguments.attribute.animated_name;
		int ns_idx = animation_arguments.attribute.ns_idx;

		SVGObjectType object_type = SVGAnimationValue::TranslateToSVGObjectType(reference_type);

		OP_ASSERT(object_type != SVGOBJECT_UNKNOWN);
		OP_STATUS parse_status = OpStatus::OK;
		if (object_type == SVGOBJECT_TRANSFORM &&
			animation_arguments.animation_element_type == Markup::SVGE_ANIMATETRANSFORM)
		{
			SVGTransform *transform_object = NULL;
			parse_status = SVGAttributeParser::ParseTransformFromToByValue(str, str_length,
																		   animation_arguments.GetTransformType(),
																		   &transform_object);
			result = transform_object;
		}
		else
		{
			parse_status = g_svg_manager_impl->ParseAttribute(element, doc, object_type,
															  attribute_name, ns_idx, str,
															  str_length, &result);
		}

		if (OpStatus::IsSuccess(parse_status))
		{
			OP_ASSERT(result != NULL);

			cache.SetSource(from.reference.svg_string);
			cache.SetCache(result);

			SVGAnimationValue::Initialize(to, result, context);
			to_container.Set(result);
		}

		return parse_status;
    }
    else if (from.reference_type == reference_type ||
			 from.reference_type == SVGAnimationValue::REFERENCE_EMPTY)
    {
		to = from;
		return OpStatus::OK;
    }
    else
    {
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
    }
}

void
SVGAnimationCalculator::FindRelevantAnimationValuesCalcModeLinear(SVG_ANIMATION_INTERVAL_POSITION interval_position,
																  SVGObject *&from, SVGObject *&to,
																  unsigned &sub_interval_index,
																  SVG_ANIMATION_INTERVAL_POSITION &sub_interval_position)
{
	SVGVector *values = animation_arguments.GetValues();
	OP_ASSERT(values != NULL);
	unsigned value_count = values->GetCount();

	SVGVector *key_times = animation_arguments.GetKeyTimes();
	if (key_times && key_times->GetCount() > 0 && key_times->VectorType() == SVGOBJECT_NUMBER)
	{
		SVGNumberObject *time_end = NULL;
		SVGNumberObject *time_begin = NULL;
		for (UINT32 i = 1; i < key_times->GetCount(); i++)
		{
			time_end = static_cast<SVGNumberObject*>(key_times->Get(i));
			if(interval_position >= time_end->value.GetFloatValue())
			{
				sub_interval_index++;
			}
			else
			{
				break;
			}
		}

		unsigned last_interval_index = key_times->GetCount() - 2; // ADS compiler is buggy and cannot compile expressions in ?: operator
		sub_interval_index = MIN(sub_interval_index, last_interval_index); 
		time_begin = static_cast<SVGNumberObject*>(key_times->Get(sub_interval_index));
		if (time_begin && time_end)
		{
			SVG_ANIMATION_INTERVAL_POSITION interval_time = time_end->value.GetFloatValue() - time_begin->value.GetFloatValue();
			if (interval_time > 0)
				sub_interval_position = (interval_position - time_begin->value.GetFloatValue()) / interval_time;
			else
				sub_interval_position = 1.0;
		}

		from = values->Get(sub_interval_index);
		to = values->Get(sub_interval_index+1);
	}
	else if (value_count > 1)
	{
		if (interval_position < 1.0)
		{
			unsigned sub_intervals = value_count - 1;
			SVG_ANIMATION_INTERVAL_POSITION sub_interval_length = 1.0f / (SVG_ANIMATION_INTERVAL_POSITION)sub_intervals;
			sub_interval_position = (SVG_ANIMATION_INTERVAL_POSITION)op_fmod(interval_position, sub_interval_length) / sub_interval_length;
			sub_interval_index = (unsigned)op_floor(interval_position / sub_interval_length);
			from = values->Get(sub_interval_index);
			to = values->Get(sub_interval_index+1);
		}
		else
		{
			sub_interval_position = 1.0;
			from = values->Get(value_count - 2);
			to = values->Get(value_count -1 );
		}
	}
	else
	{
		sub_interval_position = 0.0;
		sub_interval_index = 0;
		from = values->Get(0);
		to = values->Get(0);
	}
}

OP_STATUS
SVGAnimationCalculator::FindRelevantAnimationValuesCalcModePaced(SVG_ANIMATION_INTERVAL_POSITION interval_position,
																 SVGAnimationValue::ReferenceType reference_type,
																 SVGAnimationValueContext &context,
																 SVGObject *&from, SVGObject *&to,
																 SVG_ANIMATION_INTERVAL_POSITION &sub_interval_position)
{
	SVGVector* values = animation_arguments.GetValues();

	if(values->GetCount() <= 1)
	{
		return OpSVGStatus::INVALID_ANIMATION;
	}

	unsigned sub_interval_index = 0;
	float totalChange = 0;
	UINT32 i = 0;

	float* sub_interval_distance = OP_NEWA(float, values->GetCount() - 1);
	if (!sub_interval_distance)
		return OpStatus::ERR_NO_MEMORY;

	while(TRUE)
	{
		SVGObject *val = values->Get(i);
		SVGObject *nextval = values->Get(i+1);
		if(val && nextval)
		{
			SVGAnimationValue original_from_value = SVGAnimationValue::Empty();
			SVGAnimationValue::Initialize(original_from_value, val, context);

			SVGAnimationValue original_to_value = SVGAnimationValue::Empty();
			SVGAnimationValue::Initialize(original_to_value, nextval, context);

			SVGObjectContainer casted_from_value_container;
			SVGAnimationValue casted_from_value = SVGAnimationValue::Empty();

			SVGObjectContainer casted_to_value_container;
			SVGAnimationValue casted_to_value = SVGAnimationValue::Empty();

			if (OpStatus::IsMemoryError(TypeCastValue(original_from_value,
													  casted_from_value,
													  casted_from_value_container,
													  reference_type, context, from_value_cache)) ||
				OpStatus::IsMemoryError(TypeCastValue(original_to_value,
													  casted_to_value,
													  casted_to_value_container,
													  reference_type, context, to_value_cache)))
			{
				OP_DELETEA(sub_interval_distance);
				return OpStatus::ERR_NO_MEMORY;
			}

			sub_interval_distance[i] = SVGAnimationValue::CalculateDistance(
				context,
				casted_from_value,
				casted_to_value);

			totalChange += sub_interval_distance[i];
			i++;
		}
		else
			break;
	}

	if (totalChange > 0)
	{
		float change = 0;
		for(i = 0; i < values->GetCount()-1; i++)
		{
			sub_interval_distance[i] /= totalChange;
			change += sub_interval_distance[i];

			if(interval_position > change)
			{
				sub_interval_index++;
			}
			else
				break;
		}
		sub_interval_position = 1.0f - ((change - interval_position) / sub_interval_distance[sub_interval_index]);
	}
	else
	{
		sub_interval_index = 0;
		sub_interval_position = 0.0f;
	}

	OP_DELETEA(sub_interval_distance);

	if(sub_interval_index == (values->GetCount() - 1))
	{
		from = values->Get(sub_interval_index-1);
		to = values->Get(sub_interval_index);
	}
	else
	{
		from = values->Get(sub_interval_index);
		to = values->Get(sub_interval_index+1);
	}

	return OpStatus::OK;
}

void
SVGAnimationCalculator::FindRelevantAnimationValuesCalcModeDiscrete(SVG_ANIMATION_INTERVAL_POSITION interval_position,
																	SVGObject *&to)
{
	SVGVector *values = animation_arguments.GetValues();
	OP_ASSERT(values != NULL);

	SVGVector *key_times = animation_arguments.GetKeyTimes();
	if (key_times && key_times->GetCount() > 0 &&
		key_times->VectorType() == SVGOBJECT_NUMBER)
	{
		SVGNumberObject *time_end = NULL;
		unsigned sub_interval_index = 0;
		for (UINT32 i = 1; i < key_times->GetCount(); i++)
		{
			time_end = static_cast<SVGNumberObject*>(key_times->Get(i));
			if (interval_position >= time_end->value.GetFloatValue())
			{
				sub_interval_index++;
			}
			else
			{
				break;
			}
		}

		sub_interval_index = MIN(sub_interval_index, key_times->GetCount() - 1);
		to = values->Get(sub_interval_index);
	}
	else
	{
		unsigned sub_intervals = values->GetCount();
		if (interval_position == 1.0)
		{
			to = values->Get(sub_intervals - 1);
		}
		else
		{
			SVG_ANIMATION_INTERVAL_POSITION sub_interval_length = 1.0f / (SVG_ANIMATION_INTERVAL_POSITION)sub_intervals;
			unsigned sub_interval_index = (unsigned)op_floor(interval_position / sub_interval_length);
			to = values->Get(sub_interval_index);
		}
	}
}

BOOL
SVGAnimationCalculator::FindAccumulationValue(SVGAnimationValueContext &context,
											  ValueMode &value_mode,
											  SVGAnimationValue &accumulation_value_1,
											  SVGAnimationValue &accumulation_value_2)
{
	SVGVector *values = animation_arguments.GetValues();

	SVGObject *accumulation_object_1 = NULL;
	SVGObject *accumulation_object_2 = NULL;

	value_mode = MODE_NORMAL;

	if (values)
	{
		accumulation_object_1 = values->Get(values->GetCount() - 1);
	}
	else
	{
		SVGObject *to = animation_arguments.GetTo();
		SVGObject *by = animation_arguments.GetBy();
		SVGObject *from = animation_arguments.GetFrom();
		if (from && to)
		{
			value_mode = MODE_NORMAL;
			accumulation_object_1 = to;
		}
		else if (to)
		{
			value_mode = MODE_ONLY_TO;
			accumulation_object_1 = to;
		}
		else if (by && from)
		{
			value_mode = MODE_BOTH_FROM_BY;
			accumulation_object_1 = by;
			accumulation_object_2 = from;
		}
		else if (by)
		{
			value_mode = MODE_ONLY_BY;
			accumulation_object_1 = by;
		}
		else
		{
			// Have no values, from by or to
			return FALSE;
		}
	}

	if (SVGAnimationValue::Initialize(accumulation_value_1, accumulation_object_1, context))
	{
		SVGAnimationValue::Initialize(accumulation_value_2, accumulation_object_2, context);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

OP_STATUS
SVGAnimationCalculator::ApplyValueDiscretly(SVGAnimationValueContext &context,
											SVG_ANIMATION_INTERVAL_POSITION interval_position,
											ValueMode value_mode,
											SVGAnimationValue &from_value,
											SVGAnimationValue &to_value,
											const SVGAnimationValue &base_value,
											SVGAnimationValue &animation_value)
{
	OP_STATUS assign_status = OpStatus::OK;

	if (interval_position < 0.5)
	{
		if (value_mode == MODE_ONLY_TO ||
			value_mode == MODE_ONLY_BY)
		{
			assign_status = SVGAnimationValue::Assign(context, base_value, animation_value);
		}
		else
		{
			assign_status = SVGAnimationValue::Assign(context, from_value, animation_value);
		}
	}
	else
	{
		if (value_mode == MODE_ONLY_BY)
		{
			assign_status = SVGAnimationValue::Assign(context, base_value, animation_value);
			if (OpStatus::IsSuccess(assign_status))
				assign_status = SVGAnimationValue::AddBasevalue(to_value, animation_value);
		}
		else if (value_mode == MODE_BOTH_FROM_BY)
		{
			assign_status = SVGAnimationValue::Assign(context, from_value, animation_value);
			if (OpStatus::IsSuccess(assign_status))
				assign_status = SVGAnimationValue::AddBasevalue(to_value, animation_value);
		}
		else
		{
			SVGAnimationValue::Assign(context, to_value, animation_value);
		}
	}

	RETURN_IF_MEMORY_ERROR(assign_status);
	if (OpStatus::IsError(assign_status))
		return OpSVGStatus::INVALID_ANIMATION;

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationCalculator::ApplyValueInterpolate(SVGAnimationValueContext &context,
											  SVG_ANIMATION_INTERVAL_POSITION interval_position,
											  ValueMode value_mode,
											  const SVGAnimationValue &from_value,
											  const SVGAnimationValue &to_value,
											  const SVGAnimationValue &base_value,
											  SVGAnimationValue &animation_value)
{
	OP_ASSERT (animation_arguments.calc_mode == SVGCALCMODE_LINEAR ||
			   animation_arguments.calc_mode == SVGCALCMODE_PACED ||
			   animation_arguments.calc_mode == SVGCALCMODE_SPLINE);

	SVGAnimationValue final_from_value = from_value;
	SVGAnimationValue final_to_value = to_value;

	SVGAnimationValue::ExtraOperation extra_operation = SVGAnimationValue::EXTRA_OPERATION_NOOP;

	if (value_mode == MODE_ONLY_BY || value_mode == MODE_BOTH_FROM_BY)
	{
		extra_operation = SVGAnimationValue::EXTRA_OPERATION_TREAT_TO_AS_BY;
	}

	if (value_mode == MODE_ONLY_BY || value_mode == MODE_ONLY_TO)
	{
		final_from_value = base_value;
	}

	OP_STATUS interpolation_status =
		SVGAnimationValue::Interpolate(context,
									   interval_position,
									   final_from_value,
									   final_to_value,
									   extra_operation,
									   animation_value);
	return interpolation_status;
}

OP_STATUS
SVGAnimationCalculator::HandleAccumulate(SVGAnimationValueContext &context,
										 SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
										 SVGAnimationValue::ReferenceType reference_type,
										 SVGAnimationValue &animation_value)
{
	if (animation_arguments.accumulate_type == SVGACCUMULATE_SUM)
	{
		SVGAnimationValue accumulation_value = SVGAnimationValue::Empty();
		SVGAnimationValue accumulation_value_base = SVGAnimationValue::Empty();

		ValueMode value_mode;
		if (FindAccumulationValue(context, value_mode, accumulation_value, accumulation_value_base))
		{
			SVGObjectContainer casted_value_container;
			SVGAnimationValue casted_value = SVGAnimationValue::Empty();

			AnimationValueCache no_cache = AnimationValueCache::NoCache();

			if (OpStatus::IsMemoryError(TypeCastValue(accumulation_value,
													  casted_value,
													  casted_value_container,
													  reference_type, context, no_cache)))
			{
				return OpStatus::ERR_NO_MEMORY;
			}

			SVGObjectContainer secondary_casted_value_container;
			SVGAnimationValue secondary_casted_value = SVGAnimationValue::Empty();
			if (!SVGAnimationValue::IsEmpty(accumulation_value_base))
			{
				if (OpStatus::IsMemoryError(TypeCastValue(accumulation_value_base,
														  secondary_casted_value,
														  secondary_casted_value_container,
														  reference_type, context, no_cache)))
				{
					return OpStatus::ERR_NO_MEMORY;
				}
			}

			RETURN_IF_ERROR(SVGAnimationValue::AddAccumulationValue(casted_value,
																	secondary_casted_value,
																	interval_repetition,
																	animation_value));
		}
	}
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationCalculator::HandleAdditive(ValueMode value_mode,
									   const SVGAnimationValue &base_value,
									   SVGAnimationValue &animation_value)
{
	if (animation_arguments.additive_type == SVGADDITIVE_SUM &&
		value_mode != MODE_ONLY_TO &&
		value_mode != MODE_ONLY_BY)
	{
		RETURN_IF_ERROR(SVGAnimationValue::AddBasevalue(base_value, animation_value));
	}
#ifdef SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
	else if (animation_arguments.animation_element_type == Markup::SVGE_ANIMATETRANSFORM)
	{
		animation_interface->MarkTransformAsNonAdditive(animation_target->TargetElement());
	}
#endif // SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationCalculator::CheckMotionPath(SVGAnimationValueContext &context)
{
	HTML_Element* mpath_element = NULL;
	HTML_Element* path_element = NULL;

	if (animation_arguments.GetMotionPath())
	{
		mpath_element = FindMPathElement();

		if (mpath_element)
		{
			/* mpath elements can themselves be animated. Check that
			   the motion path is still valid. */

			SVGDocumentContext* elm_doc_ctx = AttrValueStore::GetSVGDocumentContext(mpath_element);
			path_element = elm_doc_ctx ? SVGUtils::FindHrefReferredNode(NULL, elm_doc_ctx, mpath_element) : NULL;
			if (path_element)
			{
				if (SVGAttribute *path_attribute = AttrValueStore::GetSVGAttr(path_element, Markup::SVGA_D, NS_IDX_SVG))
				{
					if (path_attribute->GetSerial() == animation_arguments.GetMotionPathSerial())
					{
						SVGObject *obj = path_attribute->GetSVGObject(SVG_ATTRFIELD_DEFAULT, SVGATTRIBUTE_AUTO);
						if (animation_arguments.GetMotionPath()->HasPath(obj))
						{
							SVGNumber n;
							OpStatus::Ignore(AttrValueStore::GetNumber(path_element, Markup::SVGA_PATHLENGTH, n, -1));

							if (animation_arguments.GetMotionPath()->HasPathLength(n.GetFloatValue()))
								return OpStatus::OK;
						}
					}
				}
			}
		}
		else
			return OpStatus::OK;
	}

	HTML_Element* animation_element = animation_interface->GetElement();
	OpBpath* opbpath = NULL;
	float path_length = -1;

	animation_arguments.SetMotionPath(OP_NEW(SVGMotionPath, ()));
	if (!animation_arguments.GetMotionPath())
		return OpStatus::ERR_NO_MEMORY;

	if (!mpath_element)
		mpath_element = FindMPathElement();

	if (mpath_element)
	{
		if (!path_element)
		{
			SVGDocumentContext* elm_doc_ctx = AttrValueStore::GetSVGDocumentContext(mpath_element);
			path_element = elm_doc_ctx ? SVGUtils::FindHrefReferredNode(NULL, elm_doc_ctx, mpath_element) : NULL;
		}

		if (path_element)
		{
			if (!opbpath)
				OpStatus::Ignore(AttrValueStore::GetPath(path_element, Markup::SVGA_D, &opbpath));

			SVGNumber n;
			OpStatus::Ignore(AttrValueStore::GetNumber(path_element, Markup::SVGA_PATHLENGTH, n, -1));
			path_length = n.GetFloatValue();
		}
	}
	else
	{
		OpStatus::Ignore(AttrValueStore::GetPath(animation_element, Markup::SVGA_PATH, &opbpath));
		SVGNumber n;
		OpStatus::Ignore(AttrValueStore::GetNumber(animation_element, Markup::SVGA_PATHLENGTH, n, -1));
		path_length = n.GetFloatValue();
	}

	SVGRotate* rotate;
	OpStatus::Ignore(AttrValueStore::GetRotateObject(animation_element, &rotate));
	animation_arguments.SetMotionPathRotate(rotate);

	SVGVector* key_points;
	AttrValueStore::GetVector(animation_element, Markup::SVGA_KEYPOINTS, key_points);
	animation_arguments.SetMotionPathKeyPoints(key_points);

	OP_STATUS status = OpStatus::OK;

	if (opbpath != NULL)
		status = SetMotionPathFromPath(animation_arguments.GetMotionPath(),
									   opbpath,
									   path_length);
	else if (animation_arguments.GetValues() != NULL)
		status = SetMotionPathFromValues(context, animation_arguments.GetMotionPath(),
										 animation_arguments.GetValues());
	else
	{
		BOOL is_by_animation = FALSE;
		status = SetMotionPathFromSimplified(context, animation_arguments.GetMotionPath(), animation_arguments.GetFrom(),
											 animation_arguments.GetTo(), animation_arguments.GetBy(), is_by_animation);
		if (is_by_animation)
			animation_arguments.additive_type = SVGADDITIVE_SUM;
	}

	return status;
}

OP_STATUS
SVGAnimationCalculator::ApplyValueMotionPath(SVGAnimationValueContext &context,
											 SVG_ANIMATION_INTERVAL_POSITION interval_position,
											 SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
											 const SVGAnimationValue &base_value,
											 SVGAnimationValue &animation_value)
{
	RETURN_IF_ERROR(CheckMotionPath(context));

	SVGMotionPath::PositionDescriptor pos;
	pos.where = interval_position;
	pos.keyTimes = animation_arguments.GetKeyTimes();
	pos.keySplines = animation_arguments.GetKeySplines();
	pos.keyPoints = animation_arguments.GetMotionPathKeyPoints();
	pos.rotate = animation_arguments.GetMotionPathRotate();
	pos.calcMode = animation_arguments.calc_mode;

	SVGMotionPath* motion_path = animation_arguments.GetMotionPath();

	SVGMatrix *matrix = NULL;
	if (animation_value.reference_type == SVGAnimationValue::REFERENCE_SVG_MATRIX)
	{
		matrix = animation_value.reference.svg_matrix;
	}

	if (matrix != NULL)
	{
		RETURN_IF_ERROR(motion_path->GetCurrentTransformationValue(pos, *matrix));

		if (animation_arguments.accumulate_type == SVGACCUMULATE_SUM &&
			interval_repetition > 0)
		{
			SVGMatrix final_matrix;
			pos.where = 1.0;
			pos.rotate = NULL;
			RETURN_IF_ERROR(motion_path->GetCurrentTransformationValue(pos, final_matrix));

			SVGMatrix accumulation_matrix;
			accumulation_matrix.Copy(final_matrix);
			for (SVG_ANIMATION_INTERVAL_REPETITION i=1;i<interval_repetition; ++i)
				accumulation_matrix.Multiply(final_matrix);
			matrix->Multiply(accumulation_matrix);
		}

		if (animation_arguments.additive_type == SVGADDITIVE_SUM &&
			base_value.reference_type == SVGAnimationValue::REFERENCE_SVG_MATRIX)
		{
			matrix->Multiply(*base_value.reference.svg_matrix);
		}

		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
	}
}


HTML_Element *
SVGAnimationCalculator::FindMPathElement() const
{
	HTML_Element *animation_element = animation_interface->GetElement();

	for(HTML_Element* child = animation_element->FirstChild(); child != NULL; child = child->Suc())
		if(child->IsMatchingType(Markup::SVGE_MPATH, NS_SVG))
			return child;

	return NULL;
}

/* static */ OP_STATUS
SVGAnimationCalculator::SetMotionPathFromPath(SVGMotionPath *motion_path,
											  OpBpath *path_impl, float path_length)
{
	RETURN_IF_ERROR(path_impl->SetUsedByDOM(TRUE));
	return motion_path->Set(path_impl, path_length);
}

/* static */ OP_STATUS
SVGAnimationCalculator::SetMotionPathFromValues(SVGAnimationValueContext &context,
												SVGMotionPath *motion_path, SVGVector *values)
{
	if (values->GetCount() == 0)
		return OpStatus::ERR;

	OpBpath* path = NULL;
	RETURN_IF_ERROR(OpBpath::Make(path, FALSE));

	OP_STATUS status = path->MoveTo(ResolveCoordinatePair(context, *values->Get(0)), FALSE);
	for (UINT32 i = 1; OpStatus::IsSuccess(status) && i<values->GetCount(); i++)
	{
		status = path->LineTo(ResolveCoordinatePair(context, *values->Get(i)), FALSE);
	}

	if (OpStatus::IsSuccess(status))
		status = motion_path->Set(path, -1);

	if (OpStatus::IsError(status))
		OP_DELETE(path);

	return status;
}

/* static */ OP_STATUS
SVGAnimationCalculator::SetMotionPathFromSimplified(SVGAnimationValueContext &context,
													SVGMotionPath *motion_path, SVGObject *from,
													SVGObject *to, SVGObject *by, BOOL &is_by_animation)
{
	// Check if the animation is invalid
	if (!(by || to))
	{
		return OpSVGStatus::INVALID_ANIMATION;
	}

	OP_STATUS status = OpStatus::OK;
	OpBpath* path = NULL;
	RETURN_IF_ERROR(OpBpath::Make(path, FALSE, 2));

	if (from)
	{
		status = path->MoveTo(ResolveCoordinatePair(context, *from), FALSE);
		if (OpStatus::IsSuccess(status))
		{
			if (to)
			{
				status = path->LineTo(ResolveCoordinatePair(context, *to), FALSE);
			}
			else if (by)
			{
				status = path->LineTo(ResolveCoordinatePair(context, *by), TRUE);
			}
		}
	}
	else
	{
		status = path->MoveTo(SVGNumberPair(0,0), FALSE);
		if (OpStatus::IsSuccess(status))
		{
			if (by)
			{
				status = path->LineTo(ResolveCoordinatePair(context, *by), TRUE);
				is_by_animation = TRUE;
			}
			else if (to)
			{
				status = path->LineTo(ResolveCoordinatePair(context, *to), FALSE);
			}
		}
	}

	if (OpStatus::IsSuccess(status))
		status = motion_path->Set(path, -1);

	if (OpStatus::IsError(status))
		OP_DELETE(path);

	return status;
}

/* static */ SVGNumberPair
SVGAnimationCalculator::ResolveCoordinatePair(SVGAnimationValueContext &context,
											 const SVGObject &object)
{
	if (object.Type() != SVGOBJECT_STRING)
	{
		return SVGNumberPair(0,0);
	}

	const SVGString &string_object = static_cast<const SVGString &>(object);
	const uni_char *str = string_object.GetString();
	unsigned str_length = string_object.GetLength();

	SVGLengthParser length_parser;
	SVGLength coordinate_x, coordinate_y;
	if (OpStatus::IsSuccess(length_parser.ParseCoordinatePair(str, str_length,
															  coordinate_x, coordinate_y)))
	{
		float n1 = SVGAnimationValue::ResolveLength(coordinate_x.GetScalar().GetFloatValue(),
													coordinate_x.GetUnit(), context);
		float n2 = SVGAnimationValue::ResolveLength(coordinate_y.GetScalar().GetFloatValue(),
													coordinate_y.GetUnit(), context);
		return SVGNumberPair(n1, n2);
	}
	else
	{
		return SVGNumberPair(0, 0);
	}
}

#endif // SVG_SUPPORT

