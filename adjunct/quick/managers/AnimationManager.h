/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Chris Pine, Tim Johansson, Emil Segerås
 */
#ifndef ANIMATIONMANAGER_H
#define ANIMATIONMANAGER_H

#ifdef VEGA_OPPAINTER_SUPPORT

#include "modules/libgogi/mde.h"
#include "modules/libvega/vegapath.h"
#include "modules/widgets/OpWidget.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/animation/QuickAnimation.h"

/** Class for animating widgets. */

class QuickAnimationWidget : public QuickAnimation
{
public:
	QuickAnimationWidget(OpWidget* widget,
	                     AnimationMoveType move_type,
	                     bool horizontal = true,
	                     AnimationFadeType fade_type = ANIM_FADE_NONE,
	                     int callback_param = -1,
	                     QuickAnimationListener* listener = NULL);
	virtual ~QuickAnimationWidget();
	virtual void OnAnimationStart();
	virtual void OnAnimationUpdate(double position);
	virtual void OnAnimationComplete();
	void GetCurrentValue(int &default_width, int &default_height);
	UINT8 GetOpacity();
	AnimationMoveType GetMoveType() { return m_move_type; }
	void RemoveListener() { m_listener = NULL; }
	virtual void RemoveIfListener(QuickAnimationListener* listener) { if (m_listener == listener) m_listener = NULL; }
	void SetEndRect(OpRect rct) { m_end_rect = rct; }

	/** Set to true if the widget should be hidden on animation completion */
	void SetHideOnCompletion(bool hide_on_completion) { m_hide_on_completion = hide_on_completion; }
	bool GetHideOnCompletion()                        { return m_hide_on_completion; }

private:
	OpWidget*         m_widget;
	AnimationMoveType m_move_type;
	AnimationFadeType m_fade_type;
	bool   m_horizontal;  //  It would be *really* nice to get rid of m_horizontal and start specifying the rects you want to animate to/from.
	bool   m_hide_on_completion;
	double m_curr_position;
	OpRect m_start_rect;
	OpRect m_end_rect;
	bool   m_animation_ran_to_completion;
	int    m_callback_param;          // the value that will be passed to the listener
	QuickAnimationListener* m_listener; // listener to get notification of the status of the animation
};

/** class for animation of a window (the window isn't owned by this class) */

class QuickAnimationWindowObject : public QuickAnimation, public DesktopWindowListener
{
public:
	QuickAnimationWindowObject()
		: m_animation_op_window(NULL)
		, m_animation_window(NULL)
		, m_animation_widget(NULL)
		, m_cached_window(NULL)
		, m_hide_when_completed(false)
		, m_opacity_on(false)
		, m_opacity_s(0)
		, m_opacity_t(0)
		, m_rect_on(false)
		, m_ignore_mouse_input_on(false)
		, m_iminp_s(false)
		, m_iminp_t(false)
		{}
	OP_STATUS Init(DesktopWindow *animation_window, OpWidget *animation_widget, OpWindow *animation_op_window = NULL);
	virtual ~QuickAnimationWindowObject();

	virtual void OnAnimationStart();
	virtual void OnAnimationUpdate(double position);
	virtual void OnAnimationComplete();

	/** Reset all animation properties (opacity, rect, ignore mouse input, hide when completed flag) */
	void reset();
	void animateHideWhenCompleted(bool hide) { m_hide_when_completed = hide; }
	void animateOpacity(double source, double target);
	void animateRect(const OpRect &source, const OpRect &target);
	void animateIgnoreMouseInput(bool source, bool target);

	void setCachedWindow(OpWindow *cached_window);

	// Implementing DesktopWindowListener
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	virtual void OnDesktopWindowAnimationRectChanged(DesktopWindow* desktop_window, const OpRect &source, const OpRect &target);
private:
	OpWindow *m_animation_op_window;
	DesktopWindow *m_animation_window;
	OpWidget *m_animation_widget;
	OpWindow *m_cached_window;

	bool m_hide_when_completed;

	bool m_opacity_on;
	double m_opacity_s, m_opacity_t;

	bool m_rect_on;
	OpRect m_rect_s, m_rect_t;

	bool m_ignore_mouse_input_on;
	bool m_iminp_s, m_iminp_t;
};

/** class for a window which animate itself */

class QuickAnimationWindow : public DesktopWindow, public QuickAnimationWindowObject
{
public:
	QuickAnimationWindow() : m_close_on_complete(false) {}

	OP_STATUS Init(DesktopWindow* parent_window, OpWindow* parent_op_window, OpWindow::Style style = OpWindow::STYLE_TRANSPARENT, UINT32 effects = 0);

	void setCloseOnComplete(bool close) { m_close_on_complete = close; }

	// Implementing DesktopWindow
	/** This window has no skin by itself */
	virtual	const char* GetFallbackSkin() { return NULL; }

	// Implementing QuickAnimation

	/** Closes this window. */
	virtual void OnAnimationComplete();

	// Implementing DesktopWindowListener
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	// Implementing OpInputContext

	/** By default, we don't want animation window to get keyboard focus */
	virtual BOOL IsInputContextAvailable(FOCUS_REASON reason) { return false; }

protected:
	bool m_close_on_complete;
};

/** class for a window which animate itself. This window will show a screenshot of the OpWindow or OpWidget
	given to its Init function. */

class QuickAnimationBitmapWindow : public QuickAnimationWindow
{
public:
	QuickAnimationBitmapWindow() : bitmap(NULL) {}
	virtual ~QuickAnimationBitmapWindow();

	OP_STATUS Init(DesktopWindow* parent_window, OpWindow* source_window, OpWidget* source_widget);

	// Implementing QuickAnimation

	/** Closes this bitmap animation window. */
	virtual void OnAnimationComplete();

	// Implementing OpWidgetListener
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect);
private:
	OpBitmap *bitmap;
};

/** Quick and easy way to disable animations in the current scope. Just create QuickDisableAnimationsHere on the stack. */

class QuickDisableAnimationsHere
{
public:
	QuickDisableAnimationsHere();
	~QuickDisableAnimationsHere();
};

/** Manager for animations */

class QuickAnimationManager : public DesktopManager<QuickAnimationManager>, public MessageObject
{
public:
	QuickAnimationManager();
	~QuickAnimationManager();
	void advanceAnimations();

	OP_STATUS Init();

	bool GetEnabled();
	bool IsAnimating() { return !m_active_animations.Empty(); }

	/** Start animation with the given parameters. For values you don't specify,
		reasonable defaults will be used.

		This is the preferred way of animating widgets. */
	void startAnimation(QuickAnimationParams& params);

	/** Start animation with the given curve and duration. If duration is -1, a default will be used.

		If delay_start_time is true, the duration from the animation will start when the animation will
		advance the first time instead of immediately. */
	void startAnimation(QuickAnimation* anim, AnimationCurve curve, double duration = -1, bool delay_start_time = false);

	/** Reschedule this animation.
		The remaining time after this call will be based on how far it had progressed when called.
		So if it had reached 75% of its duration when called, the remaining time will be 75% of the new duration from time of the call. */
	void rescheduleAnimationFlip(QuickAnimation* anim, double new_duration = -1);

	/** Abort animation. No callbacks to the animation object will be called. */
	void abortAnimation(QuickAnimation* anim);

	enum AnimationEvent {
		EVT_LOAD_PAGE,
		EVT_LOAD_PAGE_BACK,
		EVT_LOAD_PAGE_FWD
	};

	class PageAnimationInfo
	{
	public:
		PageAnimationInfo()
			: anim_win(NULL) {}
		void Set(QuickAnimationBitmapWindow* anim_win, AnimationCurve curve, double duration)
		{
			this->anim_win = anim_win;
			this->curve = curve;
			this->duration = duration;
		}
		void Reset() { this->anim_win = NULL; }
	public:
		QuickAnimationBitmapWindow *anim_win;
		AnimationCurve curve;
		double duration;
	} anim_info;

	OP_STATUS preparePageLoadAnimation(OpView *view, OpWindow *window, AnimationEvent evt);
	OP_STATUS startPageLoadAnimation(OpView *view, OpWindow *window);
	void abortPageLoadAnimation();

	void removeListenerFromAllAnimations(QuickAnimationListener* listener);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
private:
	friend class QuickDisableAnimationsHere;
	Head m_active_animations;
	bool m_callback_set;
	bool m_posted_msg;
	int m_disable_animations_counter;
	bool m_is_advancing_animations;
	VEGAPath animationCurvePath[NUM_ANIMATION_CURVES];
};

// Redefine for a more Opera like name
#define g_animation_manager (QuickAnimationManager::GetInstance())

#endif // VEGA_OPPAINTER_SUPPORT

#endif // ANIMATIONMANAGER_H
