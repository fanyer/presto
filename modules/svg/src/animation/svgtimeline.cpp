/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/animation/svgtimeline.h"
#include "modules/svg/src/svgmediamanager.h"
#include "modules/svg/src/SVGManagerImpl.h"

/* virtual */ void
SVGTimeline::Reset(SVG_ANIMATION_TIME document_time)
{
    SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
    schedule.Reset(timing_arguments);
}

/* virtual */ OP_STATUS
SVGTimeline::UpdateIntervals(SVGAnimationWorkplace *animation_workplace)
{
    SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
    return schedule.UpdateIntervals(animation_workplace, timing_if, timing_arguments);
}

/* virtual */ OP_STATUS
SVGTimeline::CommitIntervals(SVGAnimationWorkplace *animation_workplace,
							 BOOL send_events,
							 SVG_ANIMATION_TIME document_time,
							 SVG_ANIMATION_TIME elapsed_time,
							 BOOL at_prezero)
{
    SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
    return schedule.CommitIntervals(animation_workplace, document_time, send_events,
									elapsed_time, at_prezero, timing_if, timing_arguments);
}

/* virtual */ SVG_ANIMATION_TIME
SVGTimeline::NextAnimationTime(SVG_ANIMATION_TIME document_time) const
{
	SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
	HTML_Element* timed_elm = timing_if->GetElement();

	if (timed_elm->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
		if (SVGAnimationWorkplace* sub_anim_wp = timing_if->GetSubAnimationWorkplace())
		{
			SVG_ANIMATION_TIME next_animation_time = sub_anim_wp->NextAnimationTime(document_time);

			// Offset by the begin time of the active interval
			if (SVGAnimationTime::IsNumeric(next_animation_time))
				if (SVGAnimationInterval* curr_interval = schedule.GetActiveInterval(document_time))
					next_animation_time += curr_interval->Begin();

			return next_animation_time;
		}

	return schedule.NextIntervalTime(timing_arguments, document_time, FALSE);
}

/* virtual */ SVG_ANIMATION_TIME
SVGTimeline::NextIntervalTime(SVG_ANIMATION_TIME document_time, BOOL at_prezero) const
{
    SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
    return schedule.NextIntervalTime(timing_arguments, document_time, at_prezero);
}

/* virtual */ OP_STATUS
SVGTimeline::HandleAccessKey(uni_char access_key, SVG_ANIMATION_TIME document_time)
{
	SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
	return schedule.HandleAccessKey(access_key, document_time, timing_if->GetElement());
}

/* virtual */ OP_STATUS
SVGTimeline::Prepare(SVGAnimationWorkplace *animation_workplace)
{
	SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
	return schedule.Prepare(animation_workplace, timing_if, timing_arguments, timing_if->GetElement());
}

/* virtual */ void
SVGTimeline::UpdateTimingParameters(HTML_Element *element)
{
	timing_if->SetTimingArguments(timing_arguments);

	SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
	schedule.MarkInstanceListsAsDirty();
}

/* virtual */ void
SVGTimeline::RemoveElement(SVGAnimationWorkplace* animation_workplace,
								   HTML_Element* element,
								   BOOL& remove_item)
{
	SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
	schedule.Destroy(animation_workplace, timing_if, timing_arguments, TargetElement());
	remove_item = TRUE;
}

/* virtual */ OP_STATUS
SVGTimeline::RecoverFromError(SVGAnimationWorkplace *animation_workplace,
							  HTML_Element *element)
{
	OP_ASSERT (timing_if->GetElement() == element);

	SVGAnimationSchedule &schedule = timing_if->AnimationSchedule();
	return schedule.TryRecoverFromError(animation_workplace, timing_if, timing_arguments, NULL);
}

/* virtual */ OP_STATUS
SVGTimeline::UpdateValue(SVGAnimationWorkplace* animation_workplace, SVG_ANIMATION_TIME document_time)
{
	HTML_Element* timed_elm = timing_if->GetElement();

	if (timed_elm->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG) &&
		timing_if->AnimationSchedule().IsActive(document_time))
	{
		if (SVGAnimationWorkplace* sub_animation_workplace = timing_if->GetSubAnimationWorkplace())
		{
			sub_animation_workplace->packed.animation_pending = 1;
			RETURN_IF_ERROR(sub_animation_workplace->UpdateAnimations());
		}
	}

	if (has_dirty_props)
	{
		has_dirty_props = FALSE;

#ifdef SVG_SUPPORT_MEDIA
		if (timed_elm->GetNsType() == NS_SVG &&
			(timed_elm->Type() == Markup::SVGE_AUDIO ||
			 timed_elm->Type() == Markup::SVGE_VIDEO))
		{
			SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
			if (SVGMediaBinding* mbind = mm->GetBinding(timed_elm))
			{
				float audio_level;
				RETURN_IF_MEMORY_ERROR(timing_if->GetAudioLevel(audio_level));
				mbind->GetPlayer()->SetVolume(audio_level);
			}
		}
#endif

	}

	return OpStatus::OK;
}

/* virtual */ void
SVGTimeline::MarkPropsDirty()
{
	has_dirty_props = TRUE;
}

#endif // SVG_SUPPORT
