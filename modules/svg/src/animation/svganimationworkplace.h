/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_WORKPLACE_H
#define SVG_ANIMATION_WORKPLACE_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationtime.h"
#include "modules/svg/src/animation/svganimationinterval.h"
#include "modules/svg/src/animation/svganimationtarget.h"
#include "modules/svg/src/animation/svganimationlistitem.h"

class SVGDocumentContext;
class SVGAnimationInterval;
class SVGTimingInterface;
class TimedElementLink;

#if defined(SELFTEST) || defined(_DEBUG)
#define SVG_SUPPORT_ANIMATION_LISTENER
#endif

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
#define SVG_ANIMATION_NOTIFY_NEW_INTERVAL(wpl, tif, args, ival) do { (wpl)->NotifyIntervalCreated((tif), (args), (ival)); } while (0)
#else
#define SVG_ANIMATION_NOTIFY_NEW_INTERVAL(wpl, tif, args, ival) ((void)0)
#endif

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
#define SVG_ANIMATION_NOTIFY_VALUE(wpl, tif, animargs, timingargs, pos, val) do { (wpl)->NotifyAnimationValue((tif), (animargs), (timingargs), (pos), (val)); } while (0)
#else
#define SVG_ANIMATION_NOTIFY_VALUE(wpl, tif, animargs, timingargs, pos, val) ((void)0)
#endif

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
/* Enabling this define writes a logfile of each animated SVG file to
 * the resource folder OPFILE_HOME_FOLDER. This log can be parsed and
 * evaluated by tools later. Enable debug output for
 * 'svg_animation_log' to see which file the output is sent to. */
/* # define SVG_ANIMATION_LOG */
#endif

class SVGAnimationSchedule;
struct SVGTimingArguments;
class SVGAnimationArguments;
class SVGAnimationInterface;
struct SVGAnimationSlice;

class TimedElementLink : public ListElement<TimedElementLink>
{
public:
	/** Error codes. Each error code signals an action attempted at
		next recover point. A larger error code should include the
		actions or a smaller one, so that two different errors can be
		joined into one action. */

	enum ErrorCode
	{
		UNRESOLVED_TIMEVALUE = 0,
		UNRESOLVED_HREF,
	};

	TimedElementLink(ErrorCode c, HTML_Element* he) : code(c), timed_element(he) {}
	HTML_Element*	GetElement() { return timed_element; }
	ErrorCode GetCode() { return code; }
	void SetCode(ErrorCode c) { code = c; }

private:
	ErrorCode code;
	HTML_Element *timed_element;
};

/**
 * The animation workplace is where the animation-specific code for a
 * document context is placed.
 *
 * Its responsibilities:
 *
 * - Manage the time-line for this SVG fragment. The time-line can be
 *   manipulated through animation commands (e.g. paused) and queried
 *   about its current time.
 *
 * - Maintain a set of animation elements.
 *
 * - Maintain a set of animated elements.
 *
 * - Handle animation events when scripting is disabled.
 *
 * - Handle navigation to animation element (activates them through a
 *   link)
 *
 * - Provide assorted helpers to the rest of the animation code, like
 *   SendEvent.
 *
 * - Schedule next animation frame
 *
 * - Provide a callback-based API for examination of what the
 *   animation code actually does in run-time. Used exclusively by
 *   selftest to track regressions. (Only enabled in debug and
 *   selftest builds)
 *
 */
class SVGAnimationWorkplace
{
public:
	SVGAnimationWorkplace(SVGDocumentContext *doc_ctx);
	~SVGAnimationWorkplace();

	friend class SVGTimeline; ///< Through <animation>, a SVGTimeline
							  ///  can fully control a nested
							  ///  animation

	BOOL HasAnimations();
	/**< Check if there are any animation/timed elements registered in this workplace. */

	static OP_STATUS Prepare(SVGDocumentContext *doc_ctx, HTML_Element *root_element);
	/**< Called on a element to find out if there are any animations
	 * in the subtree defined by that element.  If there is, an
	 * animation workplace is created in the document context and
	 * animation contexts are created on the animation elements. */

	OP_STATUS InvalidateTimingElement(HTML_Element* element);
	/** Invalidate animation element and schedule animation update
	 * accordingly. Call this when the element has been changed in
	 * terms of attribute. */

	void UpdateTimingParameters(HTML_Element* element);
	/** Update timing parameters of a timed element, usually video or
	 * audio */

	OP_STATUS SendEvent(DOM_EventType event_type, HTML_Element *element);
	/**< Send animation event */

	OP_STATUS SendEvent(DOM_EventType event_type, unsigned detail, HTML_Element *element);
	/**< Send animation event with detail */

	void MarkIntervalsDirty() { packed.intervals_dirty = 1; }
	/**< Mark intervals as dirty */

	void RequestUpdate();
	/**< Request animation update. */

	void HandleRemovedSubtree(HTML_Element *subroot);
	/**< Notify the animation code that animation elements and
	 * animated elements in a subtree may have been removed from the
	 * document tree */

	OP_STATUS HandleEvent(const SVGManager::EventData& data);
	/**< Let the animation workplace handle the event. Normally, the
	 * events are dispatched using DOM_EventsAPI of the DOM
	 * module. But when scripting is disabled (by a preference or
	 * otherwise), we have to handle the event ourselves. This method
	 * loops through registered animated elements and checks if anyone
	 * of them listens to this event. In such a case, dispatch the
	 * event to that element.
	 *
	 * @return OpStatus::ERR in case the target is not attached to a
	 * SVG fragment. OpStatus::ERR_NO_MEMORY on OOM and OpStatus::OK
	 * otherwise.
	 */

	OP_STATUS StartTimeline();
	/**< Should be called when we get a timeline begin "event", e.g SVGLoad
	 * or on parsing an 'svg' element start tag. */

	BOOL HasTimelineStarted() const { return packed.timeline_has_started; }

	OP_STATUS HandleAccessKey(uni_char access_key);
	/**< Access keys can begin and end animations. This method is used
	 * by an external caller to signal that an access key has been
	 * pressed on an element in this svg fragment */

	OP_STATUS ApplyAnimations();
	/**<  */

	enum AnimationCommand
	{
		ANIMATION_PAUSE,
		/**< Pause animations. Puts the animations in paused mode. */

		ANIMATION_UNPAUSE,
		/**< Unpause animations. If the animations is in paused mode
		 * the animations is unpaused otherwise the command does
		 * nothing. */

		ANIMATION_STOP,
		/**< Stop animations. Puts the animations in stopped
		 * mode. Rewinds the animation time to zero. */

		ANIMATION_SUSPEND
		/**< Suspend animations. */
	};

	OP_STATUS ProcessAnimationCommand(AnimationCommand command);
	/**< Process an animation command. May put the animation in
	 * another animation mode. */

	BOOL IsValidCommand(AnimationCommand command);
	/**< Check if this command is valid in the current animation mode,
	 * where valid is defined as a command that puts the animation in
	 * a new mode, if that command was processed. */

#ifdef SVG_DOM
	OP_STATUS BeginElement(HTML_Element *element, SVG_ANIMATION_TIME offset);
	/**< Trigger a begin of an animation element. Used by SVG DOM to
	 * implement ElementTimeControl::beginElement() */

	OP_STATUS EndElement(HTML_Element *element, SVG_ANIMATION_TIME offset);
	/**< Trigger an end of an animation element. Used by SVG DOM to
	 * implement ElementTimeControl::endElement() */
#endif // SVG_DOM

	SVGDocumentContext *GetSVGDocumentContext() { return doc_ctx; }

    SVG_ANIMATION_TIME DocumentTime() { return document_time; }
	/**< Get the current document time. */

    SVG_ANIMATION_TIME VirtualDocumentTime();
	/**< Get the momentaneous document time. This means the time
	 * returned by DocumentTime(), if the animation had been updated
	 * to exactly *now*.  This is useful for fetching the time
	 * corresponding to *now*, without actually affecting the
	 * documents animation state.
	 *
	 * Currently, this is used for time-stamping events and actions
	 * received from DOM, e.g. events and begin-/endElement
	 * invocations. **/

	OP_BOOLEAN SetDocumentTime(SVG_ANIMATION_TIME new_document_time);
	/**< Sends events if the time is fast-forwarded. If a previous
	 * animation time is set, sending of events cannot be assumed.
	 *
	 * @return OpBoolean::IS_TRUE is events were sent when the time
	 * was updated, OpBoolean::IS_FALSE if no events were
	 * sent. OpStatus::ERR_NO_MEMORY on OOM.
	 */

	OP_STATUS UpdateAnimations();
	/**< Update animations. This function writes the changes to the
	 * tree */

	void MarkPropsDirty(HTML_Element *element);
	/**< Update properties on an appropriate element (e.g. audio-level
	 *  on 'video' and 'audio') */

	enum AnimationStatus
	{
		STATUS_RUNNING,
		/** The animations are running. The default state after the
		 * animations has been started. */

		STATUS_PAUSED
		/** The animations has either been paused or stopped, or not
		 * started at all. */
	};

	AnimationStatus GetAnimationStatus();
	/** Query the current animation status */

	OP_STATUS Navigate(HTML_Element *animation_element);
	/**< Navigate to an animation element. This function is used to
	 * implement activation of an animation element through a relative
	 * link. */

	enum MovePolicy
	{
		MOVE_NORMAL,
		/**< This moves the time line in the usual fashion. More
		 * specific, it send events when moving the time forward. */

		MOVE_FAST_FORWARD
		/**< Do not send events when moving the time forward. This is
		 * used to implement going backwards in time through resetting
		 * time to zero and moving forwards to the specified time. */
	};

	void ForceTime(SVG_ANIMATION_TIME animation_time);
	void UnforceTime();

	void AddElementInTimeValueError(HTML_Element *timed_element);
	/**< Add timed element that needs to be reevaluated when the tree
	 *   changes. */

	void AddElementInError(HTML_Element *timed_element, TimedElementLink::ErrorCode code);
	/**< Add element in error */

	OP_STATUS RegisterElement(HTML_Element* element);
	/* Register element */

	OP_STATUS RegisterAnimation(HTML_Element* element);
	/**< Register animation */

	OP_STATUS RegisterTimeline(HTML_Element* element);
	/**< Register timeline (audio, video, <animation>, animation
	 * element without attributeName) */

	SVGAnimationListItem* LookupItem(HTML_Element* element) const;
	/**< Look-up animation list item amongst known elements. Returns
	 * NULL for unknown elements.*/

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
	/**
	 * Helper class for the callback interface
	 */
	class AnimationListener : public Link
	{
	public:
		virtual ~AnimationListener() {}

		struct NotificationData
		{
			enum NotificationType
			{
				INTERVAL_CREATED,
				ANIMATION_VALUE
			};

			const SVGTimingInterface *timing_if;
			const SVGTimingArguments *timing_arguments;
			const SVGAnimationArguments *animation_arguments;

			struct IntervalCreatedData
			{
				const SVGAnimationSchedule *animation_schedule;
				const SVGAnimationInterval *animation_interval;
			};

			struct AnimationValueData
			{
				const SVGAnimationValue *animation_value;
				SVG_ANIMATION_INTERVAL_POSITION interval_position;
			};

			NotificationType notification_type;
			union
			{
				IntervalCreatedData interval_created;
				AnimationValueData animation_value;
			} extra;
		};

		virtual void Notification(const NotificationData &notification_data) = 0;
		virtual BOOL Accept(SVGAnimationWorkplace *animation_workplace) = 0;
	};

	void RegisterAnimationListener(AnimationListener *listener);
	void Notify(const AnimationListener::NotificationData &data);

	void NotifyIntervalCreated(const SVGTimingInterface *timing_interface,
							   const SVGTimingArguments *animation_arguments,
							   const SVGAnimationInterval *animation_interval);
	void NotifyAnimationValue(const SVGTimingInterface *timing_interface,
							  const SVGAnimationArguments &animation_arguments,
							  const SVGTimingArguments &timing_arguments,
							  SVG_ANIMATION_INTERVAL_POSITION interval_position,
							  const SVGAnimationValue &animation_value);

#endif // SVG_SUPPORT_ANIMATION_LISTENER
private:
	OP_STATUS HandleAccessKey(uni_char access_key, HTML_Element *focused_element);
	/**< Handles access key for the given element, if it has one that
	 * depends on this particular key */

	BOOL IsAnimationsAllowed();
	/**< Check if animations are allowed. Currently there is a prefs
	 * for GIF/SVG animation that can disable animations. If that pref
	 * is 'off', no SVGAnimationWorkplaces should be created. */

	void MoveDocumentTime(SVG_ANIMATION_TIME animation_time);
	/* Move document time by 'animation_time' ms. */

	void UpdateIntervals(MovePolicy move_policy, SVG_ANIMATION_TIME elapsed_time);
	/**< Compute new intervals valid at document_time. 'elapsed_time'
	 * is used to determine when the last known valid interval time
	 * occurred. The interval in-between is used to send events in
	 * that time span. */

	OP_STATUS ResetIntervals();
	/**< Reset intervals of all animation elements. Used when
	 * restarting and stopping animations. */

	SVG_ANIMATION_TIME NextIntervalTime() const;
	/**< Get the next active _interval_ time from animation
	 * element. Interval time means a time when a interval either
	 * begins, ends or repeats. These points in time is used for
	 * moving the animation forwards, interval wise. */

	SVG_ANIMATION_TIME NextAnimationTime(SVG_ANIMATION_TIME document_time) const;

	enum UpdatePolicy
	{
		COMPUTE_VALUES,
		/**< Compute intervals and values for each animation element. */

		COMPUTE_INTERVAL
		/**< Compute intervals for each animation element. */
	};

	OP_STATUS UpdateAnimations(UpdatePolicy update_policy, MovePolicy move_policy);
	/**< Update animations with a specified update and move
	 * policy. Helper function used for implementing other
	 * functionality in this class. */

	OP_STATUS MoveAnimation(MovePolicy move_policy, SVG_ANIMATION_TIME animation_time);
	/**< Move the animation time and update the animations
	 * accordingly. */

	OP_STATUS RecoverTimedElementsInError();
	/** Recover times elements in error that now may come out of
	 * error.  A change in the tree might trigger timed elements to go
	 * in or out of error states. */

	struct TimedElementsActionArgument
	{
		SVG_ANIMATION_TIME elapsed_time; ///< Used by UPDATE_INTERVALS
		MovePolicy move_policy; ///< Used by UPDATE_INTERVALS
		BOOL at_prezero; ///< Used by UPDATE_INTERVALS and NEXT_INTERVAL_TIME
	};

	struct TimedElementsActionResult
	{
		SVG_ANIMATION_TIME next_time; // Used by NEXT_INTERVAL_TIME || NEXT_ANIMATION_TIME
	};

	OP_STATUS PrepareAnimationInSubtree(HTML_Element *subroot);
	/**< Prepare subtree for animation. If there was one or more
	 * animation elements this function returns OpBoolean::IS_TRUE,
	 * otherwise OpBoolean:IS_FALSE. It returns
	 * OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_BOOLEAN PrepareTimedElementsInSubtree(SVGDocumentContext *doc_ctx, HTML_Element *subroot);
	/**< Prepare subtree for animation. If there was one or more
	 * animation elements this function returns OpBoolean::IS_TRUE,
	 * otherwise OpBoolean:IS_FALSE. It returns
	 * OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS PrepareTimedElement(HTML_Element* animation_element);

	void ConnectToRealTime();
	/**< Connect the time of the document to the real time. */

	void DisconnectFromRealTime();
	/**< Disconnect the time of the document from the real time. */

	SVG_ANIMATION_TIME RemainingDocumentTime();
	/**< Get the document time remaining before the next animation */

	void SetNextRealTime();
	/**< Set the next real time to animate to */

	void SetAnimationPending(BOOL pending) {
		if (pending)
			activity_svganim.Begin();
		else
			activity_svganim.Cancel();

		packed.animation_pending = !!pending;
	}

#ifdef SVG_SUPPORT_MEDIA
	void SuspendResumeMedia(BOOL suspend);
	/**< Suspend or resume the 'media' elements (<audio> and <video>)
	 * this workplace knows about */
#endif // SVG_SUPPORT_MEDIA

	OP_STATUS SendEventToShadow(DOM_EventType event_type, unsigned detail, HTML_Element *element, SVG_DOCUMENT_CLASS *doc, SVG_DOCUMENT_CLASS *elm_doc);
	/** Send animation event to the corresponding shadow element when
		in different documents. */

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
	Head animation_listeners;
	/**< Listeners to animation events. */
#endif // SVG_SUPPORT_ANIMATION_LISTENER

	void InsertAnimationItem(SVGAnimationListItem *item);
	/**< Insert animation list item at the correct place into the list
	 * of animations. */

	OpPointerHashTable<HTML_Element, SVGAnimationListItem> known_elements;
	/**< All currently known timed elements (including animation elements) */

	List<SVGAnimationListItem> animations;
	/**< The list of animations (timed elements are included here) */

	OpVector<HTML_Element> pending_timed_elements;
	/**< (Timed) Elements pending for update of schedule */

	List<TimedElementLink> timed_elements_in_error;
	/**< List of timed elements currently in error */

	SVG_ANIMATION_TIME document_time;
	/**< The time of the document */

	SVG_ANIMATION_TIME forced_document_time;
	/**< The forced document time. */

	SVG_ANIMATION_TIME start_time;
	/** The start time to begin at. Normally '0' but can be changed by
	 * the author using DOM calls before the animation starts. */

	double document_real_time;
	/**< The continuous time. This is the real time that corresponds
	 * to the current document time. */

	double document_next_real_time;
	/**< The next real time of the document. This is the real time
	 * that corresponds to the next document time. */

	enum TimeState
	{
		TIME_TRACKING,
		/**< The time is continually updated from
		 * g_op_time_info->GetRuntimeMS() */

		TIME_BROKEN
		/**< We are paused and do not update the document time */
	};

	TimeState time_state;
	float time_multiplier;
	SVG_ANIMATION_TIME accumulated_interval_time;

	union
	{
		struct
		{
			unsigned int animation_pending : 1;
			unsigned int animation_at_prezero : 1;
			unsigned int force_time : 1;
			unsigned int invalidation_pending : 1;
			unsigned int intervals_dirty : 1;
			unsigned int is_applying_anim : 1;
			unsigned int is_updating_intervals : 1;
			unsigned int timeline_has_started : 1; 	///< Indicates whether the timeline has started (either onload or onstart)
			unsigned int timeline_is_paused : 1;	///< Indicates whether the timeline is paused (can be done before the timeline has started)
		} packed;
		unsigned int packed_init;
	};

	/** Keeps a local 'is-active' state for svg animations.*/
	OpActivity activity_svganim;

	SVGDocumentContext *doc_ctx;
};

#endif // SVG_SUPPORT
#endif // !SVG_ANIMATION_WORKPLACE_H
