/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationschedule.h"
#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationinterval.h"
#include "modules/svg/src/animation/svgtimingarguments.h"
#include "modules/svg/src/animation/svganimationarguments.h"

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGTimeValue.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/AttrValueStore.h"

SVGAnimationSchedule::SVGAnimationSchedule() :
	current_interval(NULL),
	previous_interval(NULL)
{
	activation_time = SVGAnimationTime::Indefinite();

	packed_init = 0;
	packed.instance_lists_dirty = 1;
}

SVGAnimationSchedule::~SVGAnimationSchedule()
{
	UINT32 i=0;
	for (;i<syncbase_time_objects.GetCount();i++)
		SVGObject::DecRef(syncbase_time_objects.Get(i));

	for (i=0;i<repeat_time_objects.GetCount();i++)
		SVGObject::DecRef(repeat_time_objects.Get(i));

	OP_DELETE(current_interval);
	OP_DELETE(previous_interval);
}

void
SVGAnimationSchedule::DiscardCurrentInterval()
{
	OP_DELETE(current_interval);
	current_interval = NULL;
}

OP_STATUS
SVGAnimationSchedule::UpdateIntervals(SVGAnimationWorkplace* animation_workplace,
									  SVGTimingInterface* timing_if,
									  SVGTimingArguments &timing_arguments)
{
	SVG_ANIMATION_TIME document_time = animation_workplace->DocumentTime();

	if (packed.instance_lists_dirty)
	{
		RebuildInstanceLists(timing_arguments);

		if (packed.is_computing_dependant)
			return OpStatus::OK;

		if (current_interval != NULL)
		{
			if (current_interval->Begin() > document_time)
				/* The interval hasn't begun yet, we recalculate it
				   based on the new instance lists */

				DiscardCurrentInterval();

			if (current_interval &&	document_time < current_interval->End())
			{
				OP_ASSERT(current_interval->Begin() <= document_time);

				/* The current interval has begun, the active duration
				   may have changed. */

				SVG_ANIMATION_TIME active_duration;
				SVG_ANIMATION_TIME repeat_duration;
				SVG_ANIMATION_TIME current_end_time =
					CalculateActiveDuration(timing_arguments, active_duration,
											repeat_duration, current_interval->Begin());

				if (SVGAnimationTime::IsUnresolved(current_end_time))
				{
					// We could not calculate the active duration this
					// time, throw away this interval.
					DiscardCurrentInterval();
					RETURN_IF_ERROR(NotifyDependingElements(animation_workplace, NULL));
				}
				else
				{
					// Adjust the end time but keep the local
					// invariant that end time is larger than the
					// document time, in order to not disrupt the
					// event dispatching.

					current_end_time = MAX(current_end_time, document_time + 1);

					if (current_interval->End() != current_end_time)
					{
						current_interval->SetEnd(current_end_time);
						RETURN_IF_ERROR(NotifyDependingElements(animation_workplace, current_interval));
					}
				}
			}
		}
	}


	BOOL should_calculate_next_interval =
		CheckCurrentInterval(animation_workplace, timing_arguments);

	if (should_calculate_next_interval)
	{
		SVGAnimationInterval* new_interval = NULL;

		SVG_ANIMATION_TIME begin_past_time = current_interval ?
			current_interval->End() :
			SVGAnimationTime::Earliest();

		SVG_ANIMATION_TIME end_past_time = document_time;

		RETURN_IF_ERROR(CalculateNextInterval(begin_past_time,
											  end_past_time,
											  timing_arguments,
											  new_interval));
		if (new_interval != NULL)
		{
			PushInterval(new_interval);
			RETURN_IF_ERROR(NotifyDependingElements(animation_workplace, new_interval));

			SVG_ANIMATION_NOTIFY_NEW_INTERVAL(animation_workplace,
											  timing_if, &timing_arguments, new_interval);
		}
	}

	return OpStatus::OK;
}

OP_BOOLEAN
SVGAnimationSchedule::CommitIntervals(SVGAnimationWorkplace* animation_workplace,
									  SVG_ANIMATION_TIME document_time,
									  BOOL send_events,
									  SVG_ANIMATION_TIME elapsed_time,
									  BOOL at_prezero,
									  SVGTimingInterface* timing_if,
									  SVGTimingArguments &timing_arguments)
{
	// The current interval is now final, send events

	if (elapsed_time > 0 || at_prezero)
		DispatchEvents(animation_workplace, document_time, timing_if, timing_arguments, elapsed_time, send_events, at_prezero);

	BOOL has_switched_interval = GetActiveInterval(document_time - elapsed_time) != GetActiveInterval(document_time);
	OP_BOOLEAN ret = packed.has_new_interval || has_switched_interval ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	packed.has_new_interval = 0;
	return ret;
}

OP_STATUS
SVGAnimationSchedule::Reset(SVGTimingArguments &timing_arguments)
{
	OP_DELETE(current_interval);
	current_interval = NULL;

	OP_DELETE(previous_interval);
	previous_interval = NULL;

	packed.one_interval_started = 0;
	packed.instance_lists_dirty = 0;

	return RebuildInstanceLists(timing_arguments);
}

OP_STATUS
SVGAnimationSchedule::CalculateNextInterval(SVG_ANIMATION_TIME begin_past_time,
											SVG_ANIMATION_TIME end_past_time,
											SVGTimingArguments &timing_arguments,
											SVGAnimationInterval*& new_interval)
{
	while(TRUE)
	{
		SVG_ANIMATION_TIME current_begin_time =
			begin_instance_list.FirstEqualOrPast(begin_past_time);

		OP_ASSERT(!SVGAnimationTime::IsIndefinite(current_begin_time)); // No indefinte values in begin instance list allowed
		if (SVGAnimationTime::IsUnresolved(current_begin_time)) // No more intervals
		{
			return OpStatus::OK;
		}

		SVG_ANIMATION_TIME active_duration;
		SVG_ANIMATION_TIME repeat_duration;
		SVG_ANIMATION_TIME current_end_time =
			CalculateActiveDuration(timing_arguments,
									active_duration,
									repeat_duration,
									current_begin_time);

		if (SVGAnimationTime::IsUnresolved(current_end_time))
		{
			// No valid interval left
			OP_ASSERT(new_interval == NULL || !"We should not return an interval");
			return OpStatus::OK;
		}
		else if (current_end_time >= end_past_time)
		{
			new_interval = OP_NEW(SVGAnimationInterval, (current_begin_time,
													current_end_time,
													repeat_duration,
													timing_arguments.simple_duration));
			return new_interval ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			begin_past_time = current_end_time;
		}
	}
}

OP_STATUS
SVGAnimationSchedule::RebuildInstanceLists(SVGTimingArguments &timing_arguments)
{
	begin_instance_list.Clear();
	end_instance_list.Clear();

	SVGRestartType restart_type = timing_arguments.restart_type;

	unsigned i=0;
	for (;i<extra_begin_instance_list.GetCount();i++)
		RETURN_IF_ERROR(AddBeginInstance(extra_begin_instance_list[i], restart_type));

	for (i=0;i<extra_end_instance_list.GetCount();i++)
		RETURN_IF_ERROR(end_instance_list.Add(extra_end_instance_list[i]));

	if (SVGAnimationTime::IsNumeric(activation_time))
		RETURN_IF_ERROR(AddBeginInstance(activation_time, restart_type));

	if (timing_arguments.packed.begin_is_empty)
	{
		AddBeginInstance(0, restart_type);
	}
	else if (SVGVector *begin_list = timing_arguments.GetBeginList())
	{
		for (UINT32 i=0;i<begin_list->GetCount();i++)
		{
			SVGTimeObject* time_value = static_cast<SVGTimeObject *>(begin_list->Get(i));

			SVGAnimationInstanceList tmp_list;
			RETURN_IF_MEMORY_ERROR(time_value->GetInstanceTimes(tmp_list, FALSE));
			for (unsigned i=0;i<tmp_list.GetCount();i++)
				RETURN_IF_ERROR(AddBeginInstance(tmp_list[i], restart_type));
		}
	}

	if (SVGVector *end_list = timing_arguments.GetEndList())
	{
		for (UINT32 i=0;i<end_list->GetCount();i++)
		{
			SVGTimeObject* time_value = static_cast<SVGTimeObject *>(end_list->Get(i));
			RETURN_IF_MEMORY_ERROR(time_value->GetInstanceTimes(end_instance_list, TRUE));
		}
	}

	packed.instance_lists_dirty = 0;

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::AddBeginInstance(SVG_ANIMATION_TIME instance, SVGRestartType restart_type)
{
	BOOL allow_add = TRUE;
	if (restart_type == SVGRESTART_WHENNOTACTIVE)
	{
		if (current_interval != NULL)
		{
			if (current_interval->IsInside(instance))
			{
				allow_add = FALSE;
			}
		}
	}
	else if (restart_type == SVGRESTART_NEVER && packed.one_interval_started)
	{
		allow_add = FALSE;
	}

	if (allow_add)
	{
		return begin_instance_list.Add(instance);
	}
	else
	{
		return OpStatus::OK;
	}
}

void
SVGAnimationSchedule::PushInterval(SVGAnimationInterval* animation_interval)
{
	OP_ASSERT(animation_interval != NULL);
	if (current_interval != NULL)
	{
		OP_DELETE(previous_interval);
		previous_interval = current_interval;
	}

	packed.has_new_interval = 1;
	current_interval = animation_interval;
}

SVG_ANIMATION_TIME
SVGAnimationSchedule::CalculateActiveDuration(SVGTimingArguments &timing_arguments,
											  SVG_ANIMATION_TIME &active_duration,
											  SVG_ANIMATION_TIME &repeating_duration,
											  SVG_ANIMATION_TIME current_begin_time)
{
	active_duration = SVGAnimationTime::Indefinite();

	SVG_ANIMATION_TIME simple_duration = timing_arguments.simple_duration;
	SVG_ANIMATION_TIME repeat_duration = timing_arguments.repeat_duration;
	SVGRepeatCount repeat_count = timing_arguments.repeat_count;

	if (repeat_count.IsIndefinite() ||
		SVGAnimationTime::IsIndefinite(repeat_duration))
	{
		/* do nothing */
	}
	else if (SVGAnimationTime::IsNumeric(simple_duration))
	{
		/* We have a definite simple duration */
		if (SVGAnimationTime::IsUnresolved(repeat_duration) &&
			repeat_count.IsUnspecified())
		{
			active_duration = simple_duration;
		}
		else if (repeat_count.IsValue() &&
				 SVGAnimationTime::IsNumeric(repeat_duration))
		{
			SVG_ANIMATION_TIME tmp = (SVG_ANIMATION_TIME)(simple_duration * repeat_count.value);
			active_duration = MIN(tmp, repeat_duration);
		}
		else if (SVGAnimationTime::IsNumeric(repeat_duration))
		{
			active_duration = repeat_duration;
		}
		else if (repeat_count.IsValue())
		{
			active_duration = (SVG_ANIMATION_TIME)(simple_duration * repeat_count.value);
		}
	}
	else if (SVGAnimationTime::IsNumeric(repeat_duration))
		active_duration = repeat_duration;

	repeating_duration = active_duration;

	/* active_duration here is very much like the IAD in
	   http://www.w3.org/TR/2005/REC-SMIL2-20051213/smil-timing.html#q82 */

	/* By looking at end instances we acquire PAD */

	/* This part of the function calculates the end time of the active
	   duration beginning in 'current_begin_time' and with length
	   'active_duration'. */

	SVG_ANIMATION_TIME current_end_time = end_instance_list.FirstPast(current_begin_time);
	if (SVGAnimationTime::IsNumeric(current_end_time))
	{
		SVG_ANIMATION_TIME duration = current_end_time - current_begin_time;
		active_duration = MIN(active_duration, duration);
	}

	/* NOTE: If there is an end-list, and the end list has no time
	   values that have pending conditions, we must not allow the
	   interval. Unallowed intervals are signaled with an unresolved
	   end value. */
	if (timing_arguments.GetEndList() && SVGAnimationTime::IsUnresolved(current_end_time) &&
		!HasEndPendingConditions(timing_arguments.GetEndList()) &&
		end_instance_list.GetCount() > 0)
	{
		active_duration = SVGAnimationTime::Unresolved();
	}

	if (!SVGAnimationTime::IsUnresolved(active_duration))
	{
		/* Finally, for valid active_durations, compute AD by trimming
		   with max and min values.  'max' limits the repeating
		   duration, 'min' does not. */

		if (active_duration > timing_arguments.active_duration_max)
		{
			OP_ASSERT(repeating_duration >= active_duration);
			repeating_duration = active_duration = timing_arguments.active_duration_max;
		}
		else if (active_duration < timing_arguments.active_duration_min)
			active_duration = timing_arguments.active_duration_min;
	}

	if (SVGAnimationTime::IsNumeric(active_duration))
		return current_begin_time + active_duration;
	else
		return active_duration;
}

SVGAnimationInterval *
SVGAnimationSchedule::GetActiveInterval(SVG_ANIMATION_TIME document_time) const
{
	if (current_interval != NULL &&	(!previous_interval || current_interval->Begin() <= document_time))
		return current_interval;
	else
		return previous_interval;
}

SVG_ANIMATION_TIME
SVGAnimationSchedule::GetNextBeginTime(SVG_ANIMATION_TIME document_time, SVGAnimationArguments &animation_arguments)
{
	SVGAnimationInterval *fill_freeze_interval = animation_arguments.fill_mode == SVGANIMATEFILL_FREEZE ? previous_interval : NULL;

	if (fill_freeze_interval)
	{
		// If we have both have an interval for "freezing" animations
		// and one upcoming, select the begin time depending on the
		// current document time.

		if (current_interval && current_interval->Begin() <= document_time)
			return current_interval->Begin();
		else
			return fill_freeze_interval->Begin();
	}
	else if (current_interval)
		return current_interval->Begin();
	else
		return SVGAnimationTime::Indefinite();
}

SVGAnimationInterval *
SVGAnimationSchedule::GetIntervalBefore(SVG_ANIMATION_TIME animation_time)
{
	SVGAnimationInterval *intervals[2] = { current_interval, previous_interval };
	for (int i = 0;i < 2;i++)
		if (intervals[i] != NULL && intervals[i]->End() >= animation_time)
			return intervals[i];

	return NULL;
}

OP_STATUS
SVGAnimationSchedule::DispatchEvents(SVGAnimationWorkplace *animation_workplace,
									 SVG_ANIMATION_TIME document_time,
									 SVGTimingInterface* timing_if,
									 SVGTimingArguments &timing_arguments,
									 SVG_ANIMATION_TIME elapsed_time,
									 BOOL send_events,
									 BOOL at_prezero)
{
	SVGAnimationInterval *active_interval = GetActiveInterval(document_time);
	if (!active_interval)
		return OpStatus::OK;

	HTML_Element *timed_element = timing_if->GetElement();
	SVG_ANIMATION_TIME previous_document_time = document_time - elapsed_time;

	SVGAnimationInterval *possibly_ended_interval = GetIntervalBefore(document_time);

	if (possibly_ended_interval &&
		possibly_ended_interval->End() > previous_document_time &&
		possibly_ended_interval->End() <= document_time)
	{
		if (send_events)
		{
			RETURN_IF_ERROR(animation_workplace->SendEvent(ENDEVENT, timed_element));
			RETURN_IF_ERROR(DoTimedElementCallback(TIMED_ELEMENT_END, timing_if, timing_arguments));
		}
	}

	if ((active_interval->Begin() <= document_time &&
		 active_interval->Begin() > previous_document_time) ||
		(at_prezero && active_interval->Begin() == 0)) // special-case for time equals zero
	{
		if (send_events)
		{
			RETURN_IF_ERROR(animation_workplace->SendEvent(BEGINEVENT, timed_element));
			RETURN_IF_ERROR(DoTimedElementCallback(TIMED_ELEMENT_BEGIN, timing_if, timing_arguments));
		}

		packed.one_interval_started = 1;
	}

	SVG_ANIMATION_TIME simple_duration = active_interval->SimpleDuration();

	if (SVGAnimationTime::IsNumeric(simple_duration))
	{
		// repeat
		SVG_ANIMATION_TIME delta = simple_duration;
		SVG_ANIMATION_TIME B = active_interval->Begin();
		SVG_ANIMATION_TIME E = active_interval->End() - 1;
		SVG_ANIMATION_TIME a = MIN(E, MAX(B, previous_document_time + 1));
		SVG_ANIMATION_TIME b = MAX(B, MIN(E, document_time));
		SVG_ANIMATION_TIME alpha = a - B;
		SVG_ANIMATION_TIME x = delta - (alpha % delta);
		for (SVG_ANIMATION_TIME q = (a + x); q <= b; q+=delta)
		{
			unsigned repetition = (unsigned)((q - B) / delta);
			for (unsigned i=0;i<repeat_time_objects.GetCount();i++)
			{
				SVGTimeObject *repeat_time_value = repeat_time_objects.Get(i);
				if (repetition == repeat_time_value->GetRepetition())
				{
					repeat_time_value->AddInstanceTime(q + repeat_time_value->GetOffset());
					if (SVGTimingInterface *notifier = repeat_time_value->GetNotifier())
					{
						notifier->AnimationSchedule().MarkInstanceListsAsDirty();
						animation_workplace->MarkIntervalsDirty();
					}
				}
			}

			if (send_events)
			{
				RETURN_IF_ERROR(animation_workplace->SendEvent(REPEATEVENT, repetition, timed_element));
				RETURN_IF_ERROR(DoTimedElementCallback(TIMED_ELEMENT_REPEAT, timing_if, timing_arguments));
			}
		}
	}

	return OpStatus::OK;
}

SVG_ANIMATION_TIME
SVGAnimationSchedule::NextIntervalTime(const SVGTimingArguments &timing_arguments,
									   SVG_ANIMATION_TIME document_time,
									   BOOL at_prezero)
{
	SVG_ANIMATION_TIME next_interval_change = SVGAnimationTime::Indefinite();

	OP_ASSERT(!packed.instance_lists_dirty);

	if (current_interval != NULL)
	{
		if (current_interval->Begin() > document_time)
		{
			next_interval_change = current_interval->Begin();
		}
		else if (current_interval->End() > document_time)
		{
			if (SVGAnimationTime::IsNumeric(current_interval->SimpleDuration()))
			{
				SVG_ANIMATION_TIME alpha = document_time - current_interval->Begin();
				OP_ASSERT(alpha >= 0);
				SVG_ANIMATION_TIME delta = current_interval->SimpleDuration();
				SVG_ANIMATION_TIME x = delta - (alpha % delta);
				OP_ASSERT(x > 0);
				next_interval_change = MIN(x + document_time, current_interval->End());
			}
			else
			{
				next_interval_change = current_interval->End();
			}
		}
	}

	next_interval_change = MIN(next_interval_change, begin_instance_list.FirstPast(document_time));
	return next_interval_change;
}

SVG_ANIMATION_TIME
SVGAnimationSchedule::NextAnimationTime(const SVGTimingArguments &timing_arguments,
										SVG_ANIMATION_TIME document_time)
{
	SVG_ANIMATION_TIME next_animation_time = SVGAnimationTime::Indefinite();

	if (current_interval != NULL)
	{
		if (current_interval->Begin() > document_time)
		{
			next_animation_time = current_interval->Begin();
		}
		else if (current_interval->End() > document_time)
		{
			if (SVGAnimationTime::IsNumeric(current_interval->SimpleDuration()))
			{
				next_animation_time = document_time;
			}
			else if (SVGAnimationTime::IsNumeric(current_interval->End()))
			{
				next_animation_time = MIN(next_animation_time, current_interval->End());
			}
		}
	}
	next_animation_time = MIN(next_animation_time, begin_instance_list.FirstPast(document_time));
	return next_animation_time;
}

BOOL
SVGAnimationSchedule::IsActive(SVG_ANIMATION_TIME document_time)
{
	SVGAnimationInterval *active_interval = GetActiveInterval(document_time);
	return (active_interval != NULL && active_interval->Begin() <= document_time);
}

BOOL
SVGAnimationSchedule::IsActive(SVG_ANIMATION_TIME document_time, const SVGAnimationArguments& animation_arguments)
{
	SVGAnimationInterval *active_interval = GetActiveInterval(document_time);
	return active_interval && active_interval->IsActive(document_time, animation_arguments.fill_mode);
}

BOOL
SVGAnimationSchedule::CheckCurrentInterval(SVGAnimationWorkplace* animation_workplace,
										   SVGTimingArguments& timing_arguments)
{
	SVG_ANIMATION_TIME document_time = animation_workplace->DocumentTime();
 	SVGRestartType restart_type = timing_arguments.restart_type;
	if (packed.one_interval_started && restart_type == SVGRESTART_NEVER)
		return FALSE;

	if (current_interval == NULL)
		return TRUE;

	if (current_interval->End() <= document_time)
		return TRUE;

    /* "If restart is set to "always", then the current interval will
	 *  end early if there is an instance time in the begin list that
	 *  is before (i.e. earlier than) the defined end for the current
	 *  interval. Ending in this manner will also send a changed time
	 *  notice to all time dependents for the current interval
	 *  end."  */

	if (restart_type == SVGRESTART_ALWAYS)
	{
		SVG_ANIMATION_TIME next_begin =
			begin_instance_list.FirstPast(current_interval->Begin());

		if (next_begin <= document_time)
		{
			current_interval->SetEnd(next_begin);
			OpStatus::Ignore(NotifyDependingElements(animation_workplace, current_interval));
			return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS
SVGAnimationSchedule::SetupTimeDependency(SVGAnimationWorkplace* animation_workplace,
										  SVGTimeObject* time_value,
										  HTML_Element *timed_element,
										  HTML_Element *default_target_element)
{
	OP_ASSERT(time_value != NULL);
	SVGDocumentContext *doc_ctx = animation_workplace->GetSVGDocumentContext();

	const uni_char* rel = time_value->GetElementId();
	HTML_Element *target_element = NULL;
	if (rel != NULL)
	{
		target_element = SVGUtils::FindElementById(doc_ctx->GetLogicalDocument(), rel);

		// In the candidate recommendation to SVG 1.0 there were a possible value "prev.end" that
		// referred to the previous animation element in document order. That was not included
		// in SVG 1.0 but is supported by Adobe and used by some SVG:s still. We warn but support
		// it for now. Might be removed in the future.
		if (!target_element && uni_str_eq(rel, "prev"))
		{
			SVG_NEW_ERROR(timed_element);
			SVG_REPORT_ERROR((UNI_L("The value 'prev' is obsolete as time dependency and was removed before the SVG 1.0 specification. Use an id instead.")));
			target_element = timed_element->Pred();
			while (target_element && !SVGUtils::IsTimedElement(target_element))
			{
				target_element = target_element->Pred();
			}
		}
	}
	else
	{
		target_element = default_target_element;
	}

	if (target_element != NULL)
	{
		OP_STATUS res = time_value->RegisterTimeValue(doc_ctx, target_element);

		// timing_interface
		time_value->SetNotifier(AttrValueStore::GetSVGTimingInterface(timed_element));
		return res;
	}
	else
	{
		if (rel != NULL && animation_workplace->HasTimelineStarted())
		{
			SVG_NEW_ERROR(timed_element);
			SVG_REPORT_ERROR((UNI_L("Time dependency to unknown element: '%s'"), rel));
		}
		return OpSVGStatus::INVALID_ANIMATION;
	}
}

OP_STATUS
SVGAnimationSchedule::AddSyncbase(SVGTimeObject* sbase)
{
	OP_ASSERT (sbase != NULL);

	INT32 idx = syncbase_time_objects.Find(sbase);
	if (idx == -1)
	{
		RETURN_IF_MEMORY_ERROR(syncbase_time_objects.Add(sbase));
		SVGObject::IncRef(sbase);
	}
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::AddRepeat(SVGTimeObject* rtv)
{
	OP_ASSERT (rtv != NULL);

	INT32 idx = repeat_time_objects.Find(rtv);
	if (idx == -1)
	{
		RETURN_IF_MEMORY_ERROR(repeat_time_objects.Add(rtv));
		SVGObject::IncRef(rtv);
	}
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::NotifyDependingElements(SVGAnimationWorkplace* animation_workplace,
											  SVGAnimationInterval* interval)
{
	for (UINT32 i=0;i<syncbase_time_objects.GetCount();i++)
    {
        SVGTimeObject *sbase = syncbase_time_objects.Get(i);
		OP_ASSERT(sbase != NULL);

		if (interval != NULL)
		{
			SVG_ANIMATION_TIME now;
			if (sbase->GetSyncbaseEvent() == SVGSYNCBASE_BEGIN)
			{
				now = interval->Begin() + sbase->GetOffset();
			}
			else if (sbase->GetSyncbaseEvent() == SVGSYNCBASE_END &&
					 SVGAnimationTime::IsNumeric(interval->End()))
			{
				now = interval->End() + sbase->GetOffset();
			}
			else
			{
				// Unknown syncbase, ignore.
				continue;
			}

			RETURN_IF_ERROR(sbase->AddInstanceTime(now));
		}

		if (SVGTimingInterface *notifier = sbase->GetNotifier())
		{
			packed.is_computing_dependant = 1;

			if (SVGAnimationListItem* dependant = animation_workplace->LookupItem(notifier->GetElement()))
			{
				notifier->AnimationSchedule().MarkInstanceListsAsDirty();
				dependant->UpdateIntervals(animation_workplace);
			}

			packed.is_computing_dependant = 0;
		}
    }

    return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::BeginElement(SVGAnimationWorkplace *animation_workplace,
								   SVG_ANIMATION_TIME offset)
{
	SVG_ANIMATION_TIME time_to_add = animation_workplace->VirtualDocumentTime() + offset;
	RETURN_IF_ERROR(extra_begin_instance_list.Add(time_to_add));
	packed.instance_lists_dirty = 1;
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::EndElement(SVGAnimationWorkplace *animation_workplace,
								 SVG_ANIMATION_TIME offset)
{
	SVG_ANIMATION_TIME time_to_add = animation_workplace->VirtualDocumentTime() + offset;
	RETURN_IF_ERROR(extra_end_instance_list.Add(time_to_add));
	packed.instance_lists_dirty = 1;
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::RegisterTimeValues(SVGAnimationWorkplace* animation_workplace,
										 SVGTimingInterface* timing_if,
										 SVGTimingArguments& timing_arguments,
										 HTML_Element* default_target_element)
{
	if (SVGVector *begin_list = timing_arguments.GetBeginList())
	{
		for (UINT32 i=0;i<begin_list->GetCount();i++)
		{
			SVGTimeObject* time_value = static_cast<SVGTimeObject *>(begin_list->Get(i));
			OP_STATUS status = SetupTimeDependency(animation_workplace, time_value, timing_if->GetElement(), default_target_element);
			RETURN_IF_MEMORY_ERROR(status);

			if (OpStatus::IsError(status))
			{
				packed.in_syncbase_error = 1;
				animation_workplace->AddElementInTimeValueError(timing_if->GetElement());
			}
		}
	}

	if (SVGVector *end_list = timing_arguments.GetEndList())
	{
		for (UINT32 i=0;i<end_list->GetCount();i++)
		{
			SVGTimeObject* time_value = static_cast<SVGTimeObject *>(end_list->Get(i));

			OP_STATUS status = SetupTimeDependency(animation_workplace, time_value, timing_if->GetElement(), default_target_element);
			RETURN_IF_MEMORY_ERROR(status);

			if (OpStatus::IsError(status))
			{
				packed.in_syncbase_error = 1;
				animation_workplace->AddElementInTimeValueError(timing_if->GetElement());
			}
		}
	}

	return OpStatus::OK;
}

void
SVGAnimationSchedule::UnregisterTimeValues(SVGAnimationWorkplace *animation_workplace,
										   SVGTimingInterface* timing_if,
										   SVGTimingArguments &timing_arguments,
										   HTML_Element* target_element)
{
	SVGDocumentContext* doc_ctx = animation_workplace->GetSVGDocumentContext();
	OP_ASSERT(doc_ctx);

	if (SVGVector *begin_list = timing_arguments.GetBeginList())
		for (UINT32 i=0;i<begin_list->GetCount();i++)
		{
			SVGTimeObject* time_value = static_cast<SVGTimeObject *>(begin_list->Get(i));
			time_value->UnregisterTimeValue(doc_ctx, target_element);
		}

	if (SVGVector *end_list = timing_arguments.GetEndList())
		for (UINT32 i=0;i<end_list->GetCount();i++)
		{
			SVGTimeObject* time_value = static_cast<SVGTimeObject *>(end_list->Get(i));
			time_value->UnregisterTimeValue(doc_ctx, target_element);
		}
}

OP_STATUS
SVGAnimationSchedule::Prepare(SVGAnimationWorkplace *animation_workplace,
							  SVGTimingInterface* timing_if,
							  SVGTimingArguments& timing_arguments,
							  HTML_Element* default_target_element)
{
	if (!packed.prepared_called)
	{
		packed.prepared_called = 1;
		RETURN_IF_ERROR(DoTimedElementCallback(TIMED_ELEMENT_PREPARE, timing_if, timing_arguments));

		RETURN_IF_ERROR(RegisterTimeValues(animation_workplace, timing_if, timing_arguments, default_target_element));
	}

	return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::Destroy(SVGAnimationWorkplace* animation_workplace,
							  SVGTimingInterface* timing_if,
							  SVGTimingArguments &timing_arguments,
							  HTML_Element* target_element)
{
	UnregisterTimeValues(animation_workplace, timing_if, timing_arguments, target_element);
	packed.prepared_called = 0;
	packed.instance_lists_dirty = 1;
	return OpStatus::OK;
}

OP_STATUS
SVGAnimationSchedule::DoTimedElementCallback(TimedElementCallbackType callback_type,
											 SVGTimingInterface* timing_if,
											 SVGTimingArguments &timing_arguments)
{
	OP_ASSERT(SVGUtils::IsTimedElement(timing_if->GetElement()) ||
			  timing_if->GetElement()->IsMatchingType(Markup::SVGE_DISCARD, NS_SVG));
	OP_ASSERT(timing_if);

	switch(callback_type)
	{
	case TIMED_ELEMENT_BEGIN:
		return timing_if->OnIntervalBegin();
	case TIMED_ELEMENT_END:
		return timing_if->OnIntervalEnd();
	case TIMED_ELEMENT_PREPARE:
		return timing_if->OnPrepare();
	case TIMED_ELEMENT_REPEAT:
		return timing_if->OnIntervalRepeat();
	default:
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
	}
}

/* static */ BOOL
SVGAnimationSchedule::HasEndPendingConditions(SVGVector *end_list)
{
	OP_ASSERT(end_list != NULL);
	OP_ASSERT(end_list->VectorType() == SVGOBJECT_TIME);

	for (unsigned i=0; i<end_list->GetCount(); i++)
	{
		SVGTimeObject* time_value = static_cast<SVGTimeObject *>(end_list->Get(i));
        if (time_value->TimeType() == SVGTIME_EVENT ||
			time_value->TimeType() == SVGTIME_ACCESSKEY ||
			time_value->TimeType() == SVGTIME_REPEAT)
        {
            return TRUE;
        }
	}
	return FALSE;
}

BOOL
SVGAnimationSchedule::HasDependant(SVGAnimationSchedule *schedule)
{
	if (schedule == this)
		return TRUE;

	if (packed.is_computing_dependant)
		return FALSE;

	packed.is_computing_dependant = 1;

	BOOL ret = FALSE;

	for (UINT32 i=0; i<syncbase_time_objects.GetCount(); i++)
    {
        SVGTimeObject *sbase = syncbase_time_objects.Get(i);
		OP_ASSERT(sbase != NULL);

		if (SVGTimingInterface *notifier = sbase->GetNotifier())
			if (notifier->AnimationSchedule().HasDependant(schedule))
			{
				ret = TRUE;
				break;
			}
    }

	packed.is_computing_dependant = 0;

    return ret;
}

OP_STATUS
SVGAnimationSchedule::HandleAccessKey(uni_char access_key, SVG_ANIMATION_TIME document_time, HTML_Element *focused_element)
{
	for (int i = 0; i < 2; i++)
	{
		Markup::AttrType attr = i ? Markup::SVGA_END : Markup::SVGA_BEGIN;
		SVGVector* list;
		AttrValueStore::GetVector(focused_element, attr, list);
		if (!list)
			continue;

		UINT32 count = list->GetCount();
		for (UINT32 i=0;i<count;i++)
		{
			SVGTimeObject* time_val = static_cast<SVGTimeObject*>(list->Get(i));
			if (time_val->TimeType() == SVGTIME_ACCESSKEY)
			{
				if (access_key == uni_toupper(time_val->GetAccessKey()))
				{
					// Process it
					SVG_ANIMATION_TIME clk = document_time + time_val->GetOffset();
					RETURN_IF_ERROR(time_val->AddInstanceTime(clk));
				}
			}
		}

		packed.instance_lists_dirty = 1;
	}

	return OpStatus::OK;
}

#endif // SVG_SUPPORT
