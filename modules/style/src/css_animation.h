/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_ANIMATION_H
#define CSS_ANIMATION_H

#ifdef CSS_ANIMATIONS

#include "modules/layout/transitions/transitions.h"

/** Describes on specific animation with data gathered from all the
	different animation properties. */

struct CSS_AnimationDescription
{
	/** Corresponds to animation-duration, represented in
		milliseconds.

		Note: May be zero which means that the iteration-count will be
		treated as zero as well. */

	double duration;

	/** Corresponds to animation-delay, represented in milliseconds */

	double delay;

	/** Corresponds to animation-iteration-count. A negative means infinite.

		Note: May be non-integer. */

	double iteration_count;

	/** Corresponds to animation-fill-mode: forwards */

	BOOL fill_forwards;

	/** Corresponds to animation-fill-mode: backwards */

	BOOL fill_backwards;

	/** Corresponds to animation-play-state: paused */

	BOOL is_paused;

	/** Corresponds to animation-direction */

	BOOL reverse_on_odd;

	/** Corresponds to animation-direction */

	BOOL reverse_on_even;

	/** Corresponds to animation-timing-function */

	const CSS_generic_value* timing;
};

/** CSS_Animation handles the state of one animation, meaning one
	mapping between a keyframes rule and an element, with the set of
	animation properties controlling the behavior.

	The animation will be in play as long as that animation-name is
	still applied (there are special rules when an animation-name
	occurs multiple times, see CSS_Animation::Trace()). */

class CSS_Animation : public ListElement<CSS_Animation>,
					  public OpTimerListener
{

	/** The animation state machine

					 +------------+
		+------------|	PRESTART  |-------+
		|		   7 +------------+		  |
		|		  |			|			  v
		| +---------+		|		 +---------+
		| |	STOPPED	|<--+	|		 |	DELAY  |
		| +---------+	|	|		 +---------+
		|				|	v			  |
		|			 +------------+		  |
		|	 +-------|	 ACTIVE	  |<------+
		|	 |		 +------------+
		|	 |			| ^	  | ^
		|	 |			| |	  | |
		|	 |			| |	  | +-------------+
		|	 |			| |	  +-------------+ |
		|	 |			| |				    | |
		v	 v			v |				    v |
	  +------------+  +----------------+  +----------+
	  |	 FINISHED  |  | ITERATION_DONE |  |	 PAUSED	 |
	  +------------+  +----------------+  +----------+ */

	enum AnimationState {
		STATE_PRESTART,
		STATE_DELAY,
		STATE_ACTIVE,
		STATE_ITERATION_DONE,
		STATE_PAUSED,
		STATE_STOPPED,
		STATE_FINISHED
	};

public:

	/** Construct an animation, part one. See CompileAnimation() for part two. */

	CSS_Animation(FramesDocument* doc, CSS_ElementAnimations* animations, const CSS_AnimationDescription& animation_description) :
		m_animated_properties(NULL),
		m_keyframes(NULL),
		m_state(STATE_PRESTART),
		m_animations(animations),
		m_description(animation_description),
		m_iterations_left(animation_description.iteration_count),
		m_iteration_count(0),
		m_reverse_iteration(animation_description.reverse_on_even),
		m_elapsed_time(0),
		m_traced(TRUE)
	{
		m_delay_timer.SetTimerListener(this);
		if (animation_description.duration == 0)
			m_iterations_left = 0;
	}

	~CSS_Animation();

	/** Compile animation from ruleset. Second phase constructor.

		Implementation note: This method copies the keyframe rules to
		a separate structure for use during the animation. This makes
		the animation independent of changes made to keyframes rule
		for the duration of the animation. This is inline with the
		specification as of 2011. However, this is a topic of
		discussion though so it might see changes in the future.

		Returns OpStatus::ERR_NO_MEMORY on OOM, otherwise OpStatus::OK */

	OP_STATUS CompileAnimation(CSS_KeyframesRule* keyframes);

	/** Apply the animation value at a specific point in time and
		advance the current state of the animation to that time.

		This method, together with callbacks from the transition
		machinery that marks properties dirty, drives the animation
		forward */

	void Apply(CSS_AnimationManager* animation_manager, HTML_Element* element, double runtime_ms, CSS_Properties* cssprops, BOOL apply_partially);

	/** Stop the animation.

		Stopping is asynchronous because the transitions must be
		stopped as well. Use Stopped() to find out when the animation
		has finished stopping.

		Returns TRUE when the animation was moved into stopped state.
		It otherwise returns FALSE, which means that the animation was
		already in stopped state. */

	BOOL Stop(FramesDocument* doc, HTML_Element* element);

	/** Can the animation be removed?  */

	BOOL IsRemovable() const;

	/** Restart an animation. Called when the animation is removed and
		then re-added before all the shutdown machinery has finished
		(stopping an animation is an asynchronous process, see
		Stop()). */

	void Restart(FramesDocument* doc, HTML_Element* element);

	/** Trace this animation given that name matches the name of this
		animation. Untraced animations (unreferenced by
		animation-name) are stopped and deleted. Each animation can
		only be traced once between each call to apply. This allows
		multiple animations with the same name to run along side each
		other. */

	BOOL Trace(const uni_char* name);

	/** Check if the animation has been traced. */

	BOOL Traced() const { return m_traced; }

	/** Reset trace state. */

	void Untrace() { m_traced = FALSE; }

	/** The only dynamic changed allowed to an animation is changes to
		its play-state. This method updates the internal state of the
		animation to a new play-state, if necessary. */

	void UpdatePausedState(FramesDocument* doc, HTML_Element* element, double runtime_ms, BOOL is_paused);

	/** Compiled keyframe, implicit or explict. Represents the
		keyframe value for one property, e.g. '14% { color: blue }'
		and an optional associated animation-timing-function
		property. */

	class Keyframe : public ListElement<Keyframe>
	{
	public:
		Keyframe() : m_to(NULL), m_timing(NULL) {}
		~Keyframe();
		void Set(double at, CSS_decl* v, CSS_decl* t);

		void Invert() { m_at = 1 - m_at; }

		double At() const { return m_at; }
		CSS_decl* Target() const { return m_to; }
		const CSS_generic_value* Timing() const { return m_timing ? m_timing->GenArrayValue() : NULL; }

	private:
		double m_at;
		CSS_decl* m_to;
		CSS_decl* m_timing;
	};

	/** Animated property

		Represents a list of keyframes for one property and the
		current state of the animation for that property. It also
		handles the communication with the transition machinery by
		implementing the TransitionCallback interface. */

	class PropertyAnimation : public TransitionCallback
	{
	public:
		PropertyAnimation() : from(NULL), to(NULL), transition_state(ACTIVE) {}
		~PropertyAnimation() { keyframes.RemoveAll(); transition_description.Out(); }

		/** Callback from the transition machinery when a transition
			has finished. Used to drive the animation forward since
			the animation has no timer of itself (with the exception
			of animation-delay, before any transition has been
			setup.) */

		virtual OP_STATUS TransitionEnd(FramesDocument* doc, HTML_Element* elm);

		/** Callback from the transition machinery to check if the
			transition should be put on pause. */

		virtual TransitionState GetCurrentState(StateData& data);

		/** Apply declaration from keyframe to css property array */

		void ApplyKeyframe(CSS_Animation::Keyframe* keyframe, CSS_Properties* cssprops, BOOL apply_partially);

		/** Apply animated property and set up transition to take it
			there gradually.

			If Apply() returns TRUE, the 'from' value is was set, and
			we need to be applied again to set the 'to' value of the
			transition. */

		BOOL Apply(double position, double animation_end, BOOL reverse,
				   CSS_Properties* cssprops,
				   BOOL apply_partially,
				   const CSS_AnimationDescription& animation_description,
				   CSS_AnimationManager* animation_manager);

		/** Returns TRUE if the animation has finished and can be
			removed from the list of animations. */

		BOOL IsRemovable() const { return transition_state == FINISHED; }

		/** Stop property animation, by moving into the stopped state,
			unless already finished. */

		void Stop();

		/** Set property of this property animation. Only used during
			compilation. */

		void SetProperty(CSSProperty p) { property = p; }

		/** Append a keyframe. Only used during compilation. */

		void Append(Keyframe& keyframe) { keyframe.Into(&keyframes); }

		/** Reset state of the property animation, for use at each
			iteration. */

		void Reset() { from = NULL; to = NULL; }

		/** Invert keyframes; for use with animation-direction. */

		void Invert();

		/** Apply first keyframe; for use with animation-delay and
			animation-fill-mode: backwards. */

		void ApplyFirst(CSS_Properties* cssprops, BOOL apply_partially);

		/** Apply first keyframe; for use with animation-fill-mode:
			forwards. */

		void ApplyLast(CSS_Properties* cssprops, BOOL apply_partially);

		/** Apply current keyframe in paused mode; for use with
			animation-play-state: paused. */

		void ApplyPaused(CSS_Properties* cssprops, BOOL apply_partially, double runtime_ms);

	private:
		CSSProperty property;

		List<Keyframe> keyframes;

		Keyframe* from;
		Keyframe* to;

		TransitionState transition_state;
		double paused_runtime_ms;

		TransitionDescription transition_description;

#ifdef _DEBUG
		friend Debug& operator<<(Debug& d, const CSS_Animation::PropertyAnimation& p);
#endif // _DEBUG
	};

private:

	/** OpTimerListener implementation. Used to implement
		animation-delay. */

	virtual void OnTimeOut(OpTimer* timer);

	/** Compute the normalized position into 'position' and the number
		of iterations that have spanned relative to the start of the
		current iteration. */

	void Normalize(double runtime_ms, double& position, int& iterations) const;

	/** Move the animation state and send animation event when
		applicable. */

	void MoveTo(FramesDocument* doc, HTML_Element* element, AnimationState state);

	/** Send a animation event */

	OP_STATUS SendAnimationEvent(FramesDocument* doc, HTML_Element* element, DOM_EventType event_type);

	/** Holds the information of each animated property, after the
		keyframes have been compiled. */

	PropertyAnimation* m_animated_properties;

	/** Holds the number of allocated animated properties. */

	int m_animated_properties_count;

	/** Points to the allocated animation keyframes, after the
		keyframes have been compiled.*/

	Keyframe* m_keyframes;

	/** Holds the number of allocated animation keyframes, after the
		keyframes have been compiled. */

	int m_keyframe_count;

	/** Holds the current state of the animation. See description of
		AnimationState. */

	AnimationState m_state;

	/** The animations of the element. Used for animation-delay, we go
		through the animations to get to the (referenced) element when
		a delay has timed out. */

	CSS_ElementAnimations* m_animations;

	/** Contains the name of the @keyframes rule */

	OpString m_animation_name;

	/** Describes the animation, derived from animation-*
		properties */

	CSS_AnimationDescription m_description;

	/** Counts how many iterations that are left to do.

		Note: It's negative for an infinite number.

		Note 2: Zero duration animations can't have an infinite
		iteration-count. For zero duration animations, the iteration
		count is immediately set to zero. */

	double m_iterations_left;

	/** Keeps track of the number of iterations passed. */

	int m_iteration_count;

	/** TRUE if the current iteration is reversed, which is basically
		odd iterations given animation-direction:alternate. */

	BOOL m_reverse_iteration;

	/** Keeps track of when the current iteration started

		Initially the GetRuntimeMS value from when moving from
		PRESTART to ACTIVE, but updated at each move from
		ITERATION_DONE to ACTIVE. */

	double m_iteration_start;

	/** Keeps track of paused time */

	double m_iteration_paused;

	/** Keeps track of elapsed time, accumulator. Used when sending
		DOM events which include the current elapsed time. */

	double m_elapsed_time;

	/** Timer for animation-delay, used when animation-delay is
		greater than zero. */

	OpTimer m_delay_timer;

	/** Flag used for tracing the current animation-name property when
		reloading properties for an element. A simple mark-and-sweep
		process is used to remove animations no longer referenced. */

	BOOL m_traced;

#ifdef _DEBUG
	friend Debug& operator<<(Debug& d, const CSS_Animation& a);
	friend Debug& operator<<(Debug& d, const CSS_Animation::AnimationState& s);
#endif // _DEBUG
};

#ifdef _DEBUG
Debug& operator<<(Debug& d, const CSS_Animation& a);
Debug& operator<<(Debug& d, const CSS_Animation::PropertyAnimation& p);
Debug& operator<<(Debug& d, const CSS_Animation::AnimationState& s);
#endif // _DEBUG

#endif // CSS_ANIMATIONS
#endif // !CSS_ANIMATION_H
