/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MOUSEGESTUREUI_H
#define MOUSEGESTUREUI_H

#ifdef VEGA_OPPAINTER_SUPPORT

#include "modules/inputmanager/inputmanager.h"
//#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"
#include "adjunct/quick/managers/AnimationManager.h"

class OpLabel;

class MouseGestureWindow : public DesktopWidgetWindow, public DesktopWindowListener
{
public:
	enum ArrowType
	{
		NO_ARROW = -1,
		ARROW_L,
		ARROW_R,
		ARROW_U,
		ARROW_D,
		ARROW_LR,
		ARROW_LU,
		ARROW_LD,
		ARROW_RL,
		ARROW_RU,
		ARROW_RD,
		ARROW_UL,
		ARROW_UR,
		ARROW_UD,
		ARROW_DL,
		ARROW_DR,
		ARROW_DU,
		NUM_ARROWS
	};

	MouseGestureWindow();
	virtual ~MouseGestureWindow();

	OP_STATUS Init(MouseGestureManager* mg_man);

	OpWidget* GetBigDarkCircle() { return m_big_dark_circle; }
	OpWidget* GetFadingRoot()    { return m_fading_root;     }
	OpWidget* GetMovingRoot()    { return m_moving_root;     }
	OpWidget* GetBrandingLabel() { return m_branding_label;  }
	OpWidget* GetMouseIcon()     { return m_mouse_icon;      }
	OpLabel*  GetLabel(int idx)  { return m_labels.Get(idx); }
	OpWidget* GetAvailabilityArrow(int dir);
	OpWidget* GetConfirmationArrow(ArrowType type);

	void SetVisibilityOfLabelsAndArrows(MouseGestureManager* mg_man,
		bool keep_only_active_selection = false,
		bool no_fading_animations       = false);

	// == DesktopWindow::Listener ====================
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

private:
	DesktopWindow* m_parent_window;

	OpVector<OpLabel> m_labels;

	// Because we can't currently do moving and fading animations at the
	// same time on the same widget, and because you cannot animate the
	// widget container root, we create these two roots. All widgets are
	// attached to these. We can fade the entire UI by fading m_fading_root,
	// and all the parts that move are attached to m_moving_root.
	OpWidget* m_fading_root;
	OpWidget* m_moving_root;

	OpWidget* m_big_dark_circle;
	OpWidget* m_branding_label;
	OpWidget* m_mouse_icon;

	// arrows for when performing the gesture
	OpWidget* m_arrow_l;
	OpWidget* m_arrow_r;
	OpWidget* m_arrow_u;
	OpWidget* m_arrow_d;

	// arrows for showing which gesture was completed
	OpWidget* m_arrows[NUM_ARROWS];
};


class MouseGestureUI : public DesktopManager<MouseGestureUI>,
                       public MouseGestureListener,
                       public QuickAnimationListener
{
public:
	MouseGestureUI();
	virtual ~MouseGestureUI();

	OP_STATUS Init();
	void StartNextAnimation(int callback_param = -1);
	void FadeInEntireUI(int selected_gesture_idx = -1);
	void FadeOutEntireUI();

	void OnMouseGestureWindowClosing() { m_mouse_gesture_window = NULL; }

	MouseGestureWindow::ArrowType GetArrowType(int selected_gesture_idx);

	OpRect GetDarkCircleDestRect(OpWidget* arrow, OpLabel* label);

	// == MouseGestureListener ===============================
	virtual void OnGestureShow();
	virtual void OnGestureMove(int dir);
	virtual void OnFadeOutGestureUI(int selected_gesture_idx);
	virtual void OnKillGestureUI();

	// == QuickAnimationListener ===============================
	virtual void OnAnimationComplete (OpWidget *anim_target, int callback_param) { StartNextAnimation(callback_param); }
	virtual void OnAnimationDestroyed(OpWidget *anim_target, int callback_param) { StartNextAnimation(callback_param); }

	enum ClosingSequence {
		NOT_CLOSING_UI,
		MOVING_TO_FINAL_SELECTION,
		PAUSE_TO_SHOW_FINAL_SELECTION,
		FADE_OUT_ENTIRE_UI
		// and then we kill the UI when that is done
	};

private:
	MouseGestureWindow* m_mouse_gesture_window;
	MouseGestureManager* m_mg_man;
	AutoDeleteHead m_queued_animations;
	bool m_waiting_for_animation;

	ClosingSequence m_closing_sequence;
};


// Redefine for a more Opera like name
#define g_mouse_gesture_ui (MouseGestureUI::GetInstance())


#endif // VEGA_OPPAINTER_SUPPORT

#endif // MOUSEGESTUREUI_H
