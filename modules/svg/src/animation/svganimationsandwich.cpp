/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/animation/svganimationsandwich.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/AttrValueStore.h"

SVGAnimationSandwich::~SVGAnimationSandwich()
{
	animations.Clear();
}

BOOL
SVGAnimationSandwich::MatchesTarget(SVGAnimationTarget *match_target, SVGAnimationAttributeLocation match_attribute_location)
{
    return (match_target == target && match_attribute_location == attribute_location);
}

SVG_ANIMATION_TIME
SVGAnimationSandwich::NextIntervalTime(SVG_ANIMATION_TIME document_time, BOOL at_prezero) const
{
	SVG_ANIMATION_TIME next_interval_time = SVGAnimationTime::Indefinite();

    SVGAnimationSlice *iter;
    for (iter = animations.First(); iter; iter = iter->Suc())
	{
		SVGAnimationSchedule &schedule = iter->animation->AnimationSchedule();
		next_interval_time = MIN(next_interval_time, schedule.NextIntervalTime(iter->timing_arguments, document_time, at_prezero));
	}

	return next_interval_time;
}

SVG_ANIMATION_TIME
SVGAnimationSandwich::NextAnimationTime(SVG_ANIMATION_TIME document_time) const
{
	SVG_ANIMATION_TIME next_animation_time = SVGAnimationTime::Indefinite();

    SVGAnimationSlice *iter;
    for (iter = animations.First(); iter && next_animation_time > document_time; iter = iter->Suc())
	{
		SVGAnimationSchedule &schedule = iter->animation->AnimationSchedule();
		next_animation_time = MIN(next_animation_time, schedule.NextAnimationTime(iter->timing_arguments, document_time));
	}

	return next_animation_time;
}

void
SVGAnimationSandwich::InsertSlice(SVGAnimationSlice* animation_slice,
								  SVG_ANIMATION_TIME document_time)
{
	animation_slice->Out();

	// Find correct position to insert the animation into

	SVGAnimationSchedule &new_schedule = animation_slice->animation->AnimationSchedule();
	SVG_ANIMATION_TIME new_begin_time = new_schedule.GetNextBeginTime(document_time, animation_slice->animation_arguments);
	HTML_Element *new_elm = animation_slice->animation->GetElement();

	if (new_begin_time <= SVGAnimationTime::Latest())
	{
		SVGAnimationSlice *iter;
		for (iter = animations.Last(); iter; iter = iter->Pred())
		{
			if (HasHigherPriority(document_time, *iter, new_schedule, new_begin_time, new_elm))
			{
				animation_slice->Follow(iter);
				return;
			}
		}

		animation_slice->IntoStart(&animations);
	}
	else
		animation_slice->Into(&animations);
}

void
SVGAnimationSandwich::Reset(SVG_ANIMATION_TIME document_time)
{
    SVGAnimationSlice *iter;
    for (iter = animations.First(); iter; iter = iter->Suc())
	{
		SVGAnimationSchedule &schedule = iter->animation->AnimationSchedule();
		schedule.Reset(iter->timing_arguments);
	}
}

OP_STATUS
SVGAnimationSandwich::UpdateIntervals(SVGAnimationWorkplace *animation_workplace)
{
    SVGAnimationSlice *iter;
	OP_STATUS status = OpStatus::OK;

    for (iter = animations.First();
		 iter && OpStatus::IsSuccess(status);)
	{
		SVGAnimationSlice *tmp = iter;
		iter = iter->Suc();

		SVGAnimationSchedule &schedule = tmp->animation->AnimationSchedule();
		status = schedule.UpdateIntervals(animation_workplace, tmp->animation, tmp->timing_arguments);
	}

	return status;
}

OP_STATUS
SVGAnimationSandwich::CommitIntervals(SVGAnimationWorkplace *animation_workplace,
									  BOOL send_events,
									  SVG_ANIMATION_TIME document_time,
									  SVG_ANIMATION_TIME elapsed_time,
									  BOOL at_prezero)
{
    SVGAnimationSlice *iter;

	List<SVGAnimationSlice> reposition_list;
	OP_BOOLEAN has_new_interval = OpBoolean::IS_FALSE;

    for (iter = animations.First();
		 iter && OpStatus::IsSuccess(has_new_interval);)
	{
		SVGAnimationSlice *tmp = iter;
		iter = iter->Suc();

		SVGAnimationSchedule &schedule = tmp->animation->AnimationSchedule();

		has_new_interval = schedule.CommitIntervals(animation_workplace, document_time, send_events,
													elapsed_time, at_prezero, tmp->animation, tmp->timing_arguments);
		if (has_new_interval == OpBoolean::IS_TRUE)
		{
			tmp->Out();
			tmp->Into(&reposition_list);
		}
	}

    while ((iter = reposition_list.First()) != NULL)
		InsertSlice(iter, document_time);

#ifdef _DEBUG
	SVG_ANIMATION_TIME begin = SVGAnimationTime::Earliest();
	/* Check that the sandwich is properly sorted */
	for (iter = animations.First();
		 iter; iter = iter->Suc())
	{
		SVGAnimationSchedule &schedule = iter->animation->AnimationSchedule();
		SVG_ANIMATION_TIME this_begin = schedule.GetNextBeginTime(document_time, iter->animation_arguments);
		OP_ASSERT(begin <= this_begin);
		begin = this_begin;
	}
#endif // _DEBUG

	return OpStatus::IsError(has_new_interval) ? has_new_interval :OpStatus::OK;
}

OP_STATUS
SVGAnimationSandwich::UpdateValue(SVGAnimationWorkplace *animation_workplace, SVG_ANIMATION_TIME document_time)
{
	BOOL is_active = FALSE;
	HTML_Element *animated_element = target->TargetElement();
	SVGAttributeField computed_field = SVG_ATTRFIELD_BASE;
	SVGAttribute *attribute = AttrValueStore::GetSVGAttr(animated_element,
														 attribute_location.animated_name,
														 attribute_location.GetNsIdxOrSpecialNs(),
														 attribute_location.is_special);
	SVGAnimationValueContext context;
	context.Bind(animated_element);
	context.UpdateToAttribute(&attribute_location);

	SVGObject *base_svg_object = GetBaseObject(attribute_location);
	SVGObject *animation_svg_object;
	OP_STATUS status = GetAnimationObject(animated_element,
										  attribute,
										  animation_svg_object,
										  attribute_location,
										  computed_field);

	RETURN_IF_ERROR(status);

	OP_ASSERT(attribute && animation_svg_object);

	OpAutoPtr<SVGObject> old_svg_object(animation_svg_object->Clone());

	if (!old_svg_object.get())
		return OpStatus::ERR_NO_MEMORY;

	SVGAnimationValue animation_value;
	if (!SVGAnimationValue::Initialize(animation_value, animation_svg_object, context))
		return OpStatus::ERR;

	for (SVGAnimationSlice *iter = animations.First(); iter && !OpStatus::IsMemoryError(status); iter = iter->Suc())
	{
		SVGAnimationSchedule &schedule = iter->animation->AnimationSchedule();
		if (schedule.IsActive(document_time, iter->animation_arguments))
		{
			SVGObject *cloned_base_svg_object = base_svg_object ? base_svg_object->Clone() : NULL;
			SVGObject::IncRef(cloned_base_svg_object);

			SVGAnimationValue base_animation_value = SVGAnimationValue::Empty();
			SVGAnimationValue::Initialize(base_animation_value, cloned_base_svg_object, context);

			if (!attribute_location.is_special)
			{
				NS_Type ns = g_ns_manager->GetNsTypeAt(animated_element->ResolveNsIdx(attribute_location.GetNsIdxOrSpecialNs()));
				BOOL is_presentation_attribute = ns == NS_SVG && SVGUtils::IsPresentationAttribute(attribute_location.animated_name);
				if (base_animation_value.value_type == SVGAnimationValue::VALUE_EMPTY && is_presentation_attribute)
					SVGAnimationValue::SetCSSProperty(attribute_location, base_animation_value, context);
			}

			SVGAnimationInterval *active_interval = schedule.GetActiveInterval(document_time);

			SVG_ANIMATION_INTERVAL_POSITION interval_position;
			SVG_ANIMATION_INTERVAL_REPETITION interval_repetition;
			active_interval->CalculateIntervalPosition(document_time, interval_position,
													   interval_repetition);

			SVGAnimationCalculator calculator(animation_workplace, iter->animation,
											  target, iter->timing_arguments,
											  iter->animation_arguments, iter->from_value_cache, iter->to_value_cache);

			status = calculator.ApplyAnimationAtPosition(interval_position,
														 interval_repetition,
														 animation_value,
														 base_animation_value,
														 context);
			is_active = TRUE;
			SVGObject::DecRef(cloned_base_svg_object);
			base_svg_object = animation_svg_object;
		}
	}

	BOOL was_active = FALSE;

	if (is_active)
		was_active = attribute->ActivateAnimationObject(computed_field);
	else
	{
		was_active = attribute->DeactivateAnimation();
		if (animated_element->HasSpecialAttr(Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE, SpecialNs::NS_SVG))
		{
			animated_element->SetSpecialAttr(Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE,
											 ITEM_TYPE_NUM, NUM_AS_ATTR_VAL(1),
											 FALSE, SpecialNs::NS_SVG);
		}
	}

	if (SVGDocumentContext *element_doc_ctx = AttrValueStore::GetSVGDocumentContext(animated_element))
	{
		BOOL has_changed = is_active != was_active || !old_svg_object->IsEqual(*animation_svg_object);
		if (has_changed)
		{
			NS_Type ns = g_ns_manager->GetNsTypeAt(animated_element->ResolveNsIdx(attribute_location.ns_idx));
			SVGDynamicChangeHandler::HandleAttributeChange(element_doc_ctx,
														   animated_element,
														   attribute_location.animated_name, ns, FALSE);
		}
	}

	return status;
}

/* static */ BOOL
SVGAnimationSandwich::HasHigherPriority(SVG_ANIMATION_TIME document_time,
										SVGAnimationSlice &item,
										SVGAnimationSchedule &new_schedule,
										SVG_ANIMATION_TIME new_begin_time,
										HTML_Element *new_elm)
{
	/* Lists are sorted by begin time. If the begin times are equal,
	   we evaluate if one is time dependant of the other.  Time
	   dependants has higher priority.  The specification doesn't tell
	   what to do if there are time dependencies both ways, so that
	   case is undefined.

	   In the case where there are no time dependency between the two,
	   the logical order is used. */

	SVGAnimationSchedule &schedule = item.animation->AnimationSchedule();
	SVG_ANIMATION_TIME begin_time = schedule.GetNextBeginTime(document_time, item.animation_arguments);

	if (new_begin_time > begin_time)
		return TRUE;
	else if (begin_time == new_begin_time)
	{
		if (schedule.HasDependant(&new_schedule))
			return TRUE;
		else if (!new_schedule.HasDependant(&schedule))
		{
			HTML_Element *elm = item.animation->GetElement();
			if (elm->Precedes(new_elm))
				return TRUE;
		}
	}
	return FALSE;
}

SVGObject*
SVGAnimationSandwich::GetBaseObject(SVGAnimationAttributeLocation &location)
{
	if (location.base_name == Markup::HA_XML)
		return NULL;

	SVGAttribute *attribute = AttrValueStore::GetSVGAttr(target->TargetElement(),
														 attribute_location.base_name,
														 attribute_location.ns_idx,
														 FALSE);

	SVGObject* presentation_object = attribute ? attribute->GetSVGObject(SVG_ATTRFIELD_BASE, SVGATTRIBUTE_AUTO) : NULL;
	if (presentation_object && !presentation_object->Flag(SVGOBJECTFLAG_UNINITIALIZED))
		return presentation_object;
	else
		return NULL;
}

OP_STATUS
SVGAnimationSandwich::GetAnimationObject(HTML_Element* target_elm,
										 SVGAttribute*& attribute,
										 SVGObject*& obj,
										 SVGAnimationAttributeLocation &location,
										 SVGAttributeField &computed_field)
{
	HTML_Element *animated_element = target->TargetElement();

	Markup::AttrType attribute_name = location.animated_name;
	int ns_idx = location.GetNsIdxOrSpecialNs();
	BOOL is_special = location.is_special;

	obj = NULL;
	if (attribute == NULL)
	{
		attribute = OP_NEW(SVGAttribute, (NULL));
		if (!attribute)
			return OpStatus::ERR_NO_MEMORY;

		if (is_special)
		{
			OP_ASSERT(Markup::IsSpecialAttribute(attribute_name));
			if (target_elm->SetSpecialAttr(attribute_name, ITEM_TYPE_COMPLEX,
										   attribute, TRUE, static_cast<SpecialNs::Ns>(ns_idx)) < 0)
				return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			OP_ASSERT(!Markup::IsSpecialAttribute(attribute_name));
			if (target_elm->SetAttr(attribute_name, ITEM_TYPE_COMPLEX,
									attribute, TRUE, ns_idx) < 0)
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	NS_Type attr_ns = is_special ? NS_SPECIAL : g_ns_manager->GetNsTypeAt(target_elm->ResolveNsIdx(ns_idx));
	BOOL matches_css_value = ((location.type == SVGATTRIBUTE_CSS || location.type == SVGATTRIBUTE_AUTO) &&
							  attr_ns == NS_SVG && SVGUtils::IsPresentationAttribute(attribute_name));

	computed_field = matches_css_value ? SVG_ATTRFIELD_CSS : SVG_ATTRFIELD_ANIM;

	obj = attribute->GetAnimationSVGObject(location.type);
	if (obj == NULL)
	{
		RETURN_IF_ERROR(AttrValueStore::CreateDefaultAttributeObject(animated_element, attribute_name, ns_idx, is_special, obj));

		if (matches_css_value)
			obj->SetFlag(SVGOBJECTFLAG_IS_CSSPROP);

		if (OpStatus::IsMemoryError(attribute->SetAnimationObject(computed_field, obj)))
		{
			OP_DELETE(obj);
			return OpStatus::ERR_NO_MEMORY;
		}
		else
			return OpStatus::OK;
	}

	return attribute->SetAnimationObject(computed_field, obj);
}

/* virtual */ OP_STATUS
SVGAnimationSandwich::HandleAccessKey(uni_char access_key, SVG_ANIMATION_TIME document_time)
{
	SVGAnimationSlice *iter;
	for (iter = animations.First(); iter; iter = iter->Suc())
	{
		SVGAnimationSchedule &schedule = iter->animation->AnimationSchedule();
		RETURN_IF_MEMORY_ERROR(schedule.HandleAccessKey(access_key, document_time, iter->animation->GetElement()));
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
SVGAnimationSandwich::Prepare(SVGAnimationWorkplace* animation_workplace)
{
	SVGAnimationSlice* iter;
	for (iter = animations.First(); iter; iter = iter->Suc())
	{
		SVGAnimationSchedule& schedule = iter->animation->AnimationSchedule();
		RETURN_IF_MEMORY_ERROR(schedule.Prepare(animation_workplace, iter->animation,
												iter->timing_arguments, target->TargetElement()));
	}
	return OpStatus::OK;
}

/* virtual */ void
SVGAnimationSandwich::RemoveElement(SVGAnimationWorkplace* animation_workplace,
									HTML_Element *element,
									BOOL &remove_item)
{
	SVGAnimationSlice* iter;
	for (iter = animations.First(); iter;)
	{
		SVGAnimationSlice* tmp = iter;
		iter = iter->Suc();

		if (tmp->animation->GetElement() == element)
		{
			SVGAnimationSchedule &schedule = tmp->animation->AnimationSchedule();
			schedule.Destroy(animation_workplace, tmp->animation, tmp->timing_arguments, target->TargetElement());

			tmp->Out();
			OP_DELETE(tmp);
		}
	}

	remove_item = animations.Empty();
}

/* virtual */ void
SVGAnimationSandwich::UpdateTimingParameters(HTML_Element* element)
{
	SVGAnimationSlice* iter;
	for (iter = animations.First(); iter; iter = iter->Suc())
		if (iter->animation->GetElement() == element)
		{
			iter->animation->SetTimingArguments(iter->timing_arguments);

			SVGAnimationSchedule &schedule =
				iter->animation->AnimationSchedule();
			schedule.MarkInstanceListsAsDirty();

			break;
		}
}

/* virtual */ void
SVGAnimationSandwich::Deactivate()
{
	HTML_Element* animated_element = target->TargetElement();
	SVGAttribute *attribute = AttrValueStore::GetSVGAttr(animated_element,
														 attribute_location.animated_name,
														 attribute_location.GetNsIdxOrSpecialNs(),
														 attribute_location.is_special);

	if (attribute)
		attribute->DeactivateAnimation();

#ifdef SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
	// Remove/reset the additive flag if we set one.
	animated_element->RemoveSpecialAttribute(Markup::SVGA_ANIMATE_TRANSFORM_ADDITIVE, SpecialNs::NS_SVG);
#endif // SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN
}

/* virtual */ OP_STATUS
SVGAnimationSandwich::RecoverFromError(SVGAnimationWorkplace *animation_workplace, HTML_Element *element)
{
	SVGAnimationSlice *iter;
	for (iter = animations.First(); iter; iter = iter->Suc())
	{
		if (iter->animation->GetElement() == element)
		{
			SVGAnimationSchedule &schedule = iter->animation->AnimationSchedule();
			RETURN_IF_MEMORY_ERROR(schedule.TryRecoverFromError(animation_workplace, iter->animation, iter->timing_arguments, NULL));
		}
	}

	return OpStatus::OK;
}

#endif // SVG_SUPPORT

