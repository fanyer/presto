/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_SCHEDULE_H
#define SVG_ANIMATION_SCHEDULE_H

#include "modules/svg/src/animation/svganimationinstancelist.h"
#include "modules/svg/src/animation/svganimationworkplace.h"

#include "modules/svg/src/SVGInternalEnum.h"

class SVGAnimationInterval;
struct SVGTimingArguments;
class SVGTimeObject;
class SVGAnimationWorkplace;

/**
 * Logic about how intervals are created
 */
class SVGAnimationSchedule
{
public:
	SVGAnimationSchedule();

	~SVGAnimationSchedule();

	void MarkInstanceListsAsDirty() { packed.instance_lists_dirty = 1; }
	/**< Tell the schedule that it needs to rebuild instance lists and
	 * re-evaluate the current interval at the next animation
	 * frame. */

	OP_STATUS UpdateIntervals(SVGAnimationWorkplace* animation_workplace,
							  SVGTimingInterface* timing_if,
							  SVGTimingArguments &timing_arguments);
	/**< Update intervals of this schedule according to the current
	 * time and the current present instances. This might create new
	 * intervals that give birth to other intervals in other schedules
	 * recursively.*/

	OP_BOOLEAN CommitIntervals(SVGAnimationWorkplace* animation_workplace,
							   SVG_ANIMATION_TIME document_time,
							   BOOL send_events,
							   SVG_ANIMATION_TIME elapsed_time,
							   BOOL at_prezero,
							   SVGTimingInterface* timing_if,
							   SVGTimingArguments &timing_arguments);
	/**< Commit to the current intervals and send events to external
	 * parties, like DOM and such.
	 *
	 * @return OpBoolean::IS_TRUE if a new interval has been created
	 * since the last commit (or beginning of this schedule),
	 * OpBoolean::IS_FALSE otherwise.
	 */

	OP_STATUS Reset(SVGTimingArguments &timing_arguments);
	/**< Reset the schedule back to the state it is before animations
	 * has begun. */

	BOOL IsActive(SVG_ANIMATION_TIME document_time);
	/**< Returns TRUE if the schedule is active in the time
	 * @[document_time].  An schedule is "active" if it has a current
	 * interval which has begun.  Since the schedule has no knowledge
	 * of the animation/timing arguments (fill-mode specifically) it
	 * cannot know if a ended current interval is active or not.  It
	 * will return TRUE for such intervals.  If the distinction is
	 * needed, use the other IsActive() that accepts animation
	 * arguments. */

	BOOL IsActive(SVG_ANIMATION_TIME document_time, const SVGAnimationArguments& animation_arguments);
	/**< Returns TRUE if the schedule is active in the time
	 * @[document_time]. */

	SVG_ANIMATION_TIME GetNextBeginTime(SVG_ANIMATION_TIME document_time, SVGAnimationArguments& animation_arguments);
	/**< Returns the next time the schedule begins, if any, given a
	 * specific document time. */

	OP_STATUS Prepare(SVGAnimationWorkplace *animation_workplace,
					  SVGTimingInterface* timing_if,
					  SVGTimingArguments &animation_arguments,
					  HTML_Element *default_target_element);
	/**< Prepare the schedule for animation. Register the schedule in
	 * time dependencies.
	 *
	 * Also tries to recover from error states. So
	 * it should also be called when a change has happended to the
	 * tree that may cause elements to come out from error states.*/

	OP_STATUS Destroy(SVGAnimationWorkplace *animation_workplace,
					  SVGTimingInterface* timing_if,
					  SVGTimingArguments &animation_arguments,
					  HTML_Element *target_element);
	/**< The opposite of Prepare. Unregisteres the schedules in time
	 * dependencies. */

	SVG_ANIMATION_TIME NextAnimationTime(const SVGTimingArguments &animation_arguments,
										 SVG_ANIMATION_TIME document_time);
	/**< Returns the next active document time where this schedule is
	 * active */

	SVGAnimationInterval *GetActiveInterval(SVG_ANIMATION_TIME animation_time) const;
	/**< Get the active interval at an given time
	 *
	 * If there is only one interval, that interval is the active
	 * one. But there can be more than one.  If we have started and
	 * finished one, and are waiting for the next interval to start,
	 * we have a previous and a current interval. Until the current
	 * interval begins, the previous interval is the active one. These
	 * semantic is required for treating fill="freeze" correctly.
	 *
	 * A figure:
	 *
	 * The intervals begins and ends like this:
	 *
	 * -----------| previous interval |---------| current interval |-------> t
	 *
	 * The active interval at a given point in time:
	 *
	 * ...       previous interval              |   current interval ...
	 *
	 */

	SVGAnimationInterval *GetIntervalBefore(SVG_ANIMATION_TIME animation_time);

	OP_STATUS AddSyncbase(SVGTimeObject* sbase);
	/**< Undocumented */

	OP_STATUS AddRepeat(SVGTimeObject* rtv);
	/**< Undocumented */

#ifdef SVG_DOM
	OP_STATUS BeginElement(SVGAnimationWorkplace *animation_workplace, SVG_ANIMATION_TIME offset);
	/**< Undocumented */

	OP_STATUS EndElement(SVGAnimationWorkplace *animation_workplace, SVG_ANIMATION_TIME offset);
	/**< Undocumented */
#endif

	SVG_ANIMATION_TIME GetActivationTime() { return activation_time; }
	void SetActivationTime(SVG_ANIMATION_TIME activation_time) { this->activation_time = activation_time; }

	OP_STATUS TryRecoverFromError(SVGAnimationWorkplace* animation_workplace,
								  SVGTimingInterface* timing_if,
								  SVGTimingArguments &timing_arguments,
								  HTML_Element* default_target_element) {
		return RegisterTimeValues(animation_workplace, timing_if, timing_arguments, default_target_element);
	}
	/**< Try recover from a previous error state */

	SVG_ANIMATION_TIME NextIntervalTime(const SVGTimingArguments &animation_arguments,
										SVG_ANIMATION_TIME document_time,
										BOOL at_prezero);
	/**< Returns the next active document time where a interval begins
	 * or ends
	 *
	 * After we have created an interval we have the following stops:
	 *
	 * ---------------|       interval         |----------------> t
	 *                b     r_1 ...r_(n-1)     e
	 *
	 * b = begin (send begin event)
	 * r_1 = repetition 1 (we send rep event 1)
	 * r_i = repetition i (we send rep event i)
	 * r_(n-1) = repetition (n-1) (we send rep event n-1)
	 * e = we compute next interval and send end event
	 *
	 * Nothing is done in this function, all the work is done in
	 * Update(). Update() can be called more often than specified by
	 * NextIntervalTime(), but the times specified here is the bare
	 * minimum.
	 */

	BOOL HasDependant(SVGAnimationSchedule *schedule);
	/**< Check if 'schedule' depends on this animation schedule.  Used
	 * to resolve priority conflicts when the begin times are
	 * equal. */

	OP_STATUS HandleAccessKey(uni_char access_key, SVG_ANIMATION_TIME document_time, HTML_Element *focused_element);
	/**< Handle access key. If the access key is registered in this
	 * schedule (in begin or end), an time instance is added to the
	 * respective list. */

private:
	BOOL CheckCurrentInterval(SVGAnimationWorkplace* animation_workplace,
							  SVGTimingArguments& animation_arguments);

	OP_STATUS CalculateNextInterval(SVG_ANIMATION_TIME begin_past_time,
									SVG_ANIMATION_TIME end_past_time,
									SVGTimingArguments &animation_arguments,
									SVGAnimationInterval*& result);

	SVG_ANIMATION_TIME CalculateActiveDuration(SVGTimingArguments &animation_arguments,
											   SVG_ANIMATION_TIME& active_duration,
											   SVG_ANIMATION_TIME& repeat_duration,
											   SVG_ANIMATION_TIME current_begin_time);

	OP_STATUS RebuildInstanceLists(SVGTimingArguments &animation_arguments);

	OP_STATUS RegisterTimeValues(SVGAnimationWorkplace* animation_workplace,
								 SVGTimingInterface* timing_if,
								 SVGTimingArguments &animation_arguments,
								 HTML_Element* default_target_element);

	void UnregisterTimeValues(SVGAnimationWorkplace* animation_workplace,
							  SVGTimingInterface* timing_if,
							  SVGTimingArguments &animation_arguments,
							  HTML_Element* target_element);

	void PushInterval(SVGAnimationInterval* animation_interval);

	OP_STATUS DispatchEvents(SVGAnimationWorkplace* animation_workplace,
							 SVG_ANIMATION_TIME document_time,
							 SVGTimingInterface* timing_if,
							 SVGTimingArguments &animation_arguments,
							 SVG_ANIMATION_TIME elapsed_time,
							 BOOL send_events,
							 BOOL at_prezero);

	OP_STATUS SetupTimeDependency(SVGAnimationWorkplace *animation_workplace,
								  SVGTimeObject *time_value,
								  HTML_Element *timed_element,
								  HTML_Element *default_target_element);

	OP_BOOLEAN NotifyDependingElements(SVGAnimationWorkplace* animation_workplace,
									   SVGAnimationInterval* interval);

	OP_STATUS AddBeginInstance(SVG_ANIMATION_TIME instance,
							   SVGRestartType restart_type);

	static BOOL HasEndPendingConditions(SVGVector *end_list);
	/**< Check if the vector has a time value which may contain
	 * pending instances: a syncbase or an event.
	 */

	void DiscardCurrentInterval();

	enum TimedElementCallbackType
	{
		TIMED_ELEMENT_PREPARE,
		TIMED_ELEMENT_BEGIN,
		TIMED_ELEMENT_REPEAT,
		TIMED_ELEMENT_END
	};

	OP_STATUS DoTimedElementCallback(TimedElementCallbackType callback_type,
									 SVGTimingInterface* timing_if,
									 SVGTimingArguments &timing_arguments);

	SVGAnimationInstanceList begin_instance_list;
	SVGAnimationInstanceList end_instance_list;

	SVGAnimationInstanceList extra_begin_instance_list;
	/**< Instances from DOM is saved here, so when we rebuild the
	 * instance-lists, we don't lose them. */

	SVGAnimationInstanceList extra_end_instance_list;
	/**< Instances from DOM is saved here, so when we rebuild the
	 * instance-lists, we don't lose them. */

	SVGAnimationInterval *current_interval;
	SVGAnimationInterval *previous_interval;

    OpVector<SVGTimeObject> syncbase_time_objects;
	OpVector<SVGTimeObject> repeat_time_objects;

	SVG_ANIMATION_TIME activation_time;

	union {
		struct {
			unsigned int instance_lists_dirty:1;
			unsigned int one_interval_started:1; // for restart="never"
			unsigned int prepared_called:1;
			unsigned int has_new_interval:1;
			unsigned int is_computing_dependant:1; // for breaking cycles when computing time dependencies

			// If we point to a non-existing syncbase this flag gets
			// set. This means that we will try to recover from the
			// error in each following frame.
			unsigned int in_syncbase_error:1;

		} packed;
		unsigned int packed_init;
	};
};

#endif // !SVG_ANIMATION_SCHEDULE_H
