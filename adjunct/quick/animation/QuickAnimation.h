/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Tim Johansson, Emil Segerås
 */
#ifndef QUICKANIMATION_H
#define QUICKANIMATION_H

#ifdef VEGA_OPPAINTER_SUPPORT

#include "modules/util/simset.h"


class QuickAnimationListener
{
public:
	virtual ~QuickAnimationListener();

	virtual void OnAnimationComplete (OpWidget *anim_target, int callback_param) = 0;
	virtual void OnAnimationDestroyed(OpWidget *anim_target, int callback_param) {}
};


enum AnimationMoveType
{
	ANIM_MOVE_NONE,
	ANIM_MOVE_GROW,
	ANIM_MOVE_SHRINK,
	ANIM_MOVE_RECT_TO_ORIGINAL
};

enum AnimationFadeType
{
	ANIM_FADE_NONE,
	ANIM_FADE_IN,
	ANIM_FADE_OUT
};

enum AnimationCurve
{
	ANIM_CURVE_LINEAR,     ///< Transition is linear
	ANIM_CURVE_SPEED_UP,   ///< Transition speeds up
	ANIM_CURVE_SLOW_DOWN,  ///< Transition slows down
	ANIM_CURVE_BEZIER,     ///< Transition speeds up in the beginning and slow down in the end.
	ANIM_CURVE_BOUNCE,     ///< Similar to ANIM_CURVE_SLOW_DOWN, but with a bounce.
	NUM_ANIMATION_CURVES   ///< Just the number of curves. Do not use as curve type!
};

#define DEFAULT_ANIMATION_DURATION (0.2) // in seconds

/** Base class for animation */

class QuickAnimation : public Link
{
public:
	virtual ~QuickAnimation() { Out(); }

	/** Called when the animation has been started with startAnimation and the start time has been registered. */
	virtual void OnAnimationStart() = 0;

	/** Called when the animation should advance to the next step.
		position is the current progress (0 - 1.0) for the animation calculated from the animations duration and curve.
		This method will always be called with 1.0 before OnAnimationComplete is called. */
	virtual void OnAnimationUpdate(double position) = 0;

	/** Called when the animation has been completed and removed from animations list.
		This is not called if a animation is aborted with abortAnimation. */
	virtual void OnAnimationComplete() = 0;

	/** Return true if this animation is currently animating. */
	bool IsAnimating() { return !!InList(); }

	/** Called when QuickAnimationManager::removeListenerFromAllAnimations() is called. */
	virtual void RemoveIfListener(QuickAnimationListener* listener) {}

	double m_animation_start;
	double m_animation_duration;
	AnimationCurve m_animation_curve;
};

class OpWidget;

struct QuickAnimationParams
{
	// NOTE: In the future, it would be nice to add additional constructors for other animated objects.
	//       Currently, only OpWidget is supported.
	QuickAnimationParams(OpWidget* widget)
		: widget(widget)
		, move_type(ANIM_MOVE_NONE)
		, fade_type(ANIM_FADE_NONE)
		, callback_param(-1) // this is passed to the listener (and is otherwise not used by the animation system)
		, listener(NULL)
		, duration(DEFAULT_ANIMATION_DURATION)
		, curve(ANIM_CURVE_SLOW_DOWN) // chosen over LINEAR, as this seems to be the most commonly used curve
		, start_rect(-1, -1, -1, -1)  // if this is not changed, animation will begin from widget's current position
		, end_rect  (-1, -1, -1, -1)  // if this is not changed, then layout (through m_original_rect) controls destination rect
		{}

	OpWidget* widget;
	AnimationMoveType move_type;
	AnimationFadeType fade_type;
	int callback_param;
	QuickAnimationListener* listener;

	double duration; // duration of animation (in seconds)
	AnimationCurve curve;

	OpRect start_rect;
	OpRect end_rect;
};

#endif // VEGA_OPPAINTER_SUPPORT

#endif // QUICKANIMATION_H
