/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef VEGA_OPPAINTER_SUPPORT

#include "adjunct/quick/managers/MouseGestureUI.h"

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/Application.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "modules/display/vis_dev.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


#define WINDOW_SIZE            (700)
#define FONT_HEIGHT            ( 15)
#define CIRCLE_SIZE            (446)
#define LABEL_WIDTH_TOP_BOTTOM (176)
#define LABEL_WIDTH_SIDES      (126)
#define MOVEMENT_SLIDE_AMOUNT  ( 58)

#define THIS_IS_A_CLOSING_ANIMATION (54321)

class SpriteWidget : public OpWidget
{
public:
	SpriteWidget(const char* element, UINT8 alpha = 255, int width = -1, int height = -1, bool wrap_in_circle = false)
		: m_alpha(alpha)
		, m_width(width)
		, m_height(height)
		, m_wrap_in_circle(wrap_in_circle)
	{
		GetBorderSkin()->SetImage(element);

		OpSkinElement* elem = GetBorderSkin()->GetSkinElement();

		if (elem && width  < 0)
			m_width  = elem->GetImage(0).Width();
		if (elem && height < 0)
			m_height = elem->GetImage(0).Height();

		SetRect(OpRect(0, 0, m_width, m_height));
	}

	virtual ~SpriteWidget() {}

	void CenterAt(int x, int y)
	{
		SetRect(OpRect(x - m_width/2, y - m_height/2, m_width, m_height));
	}

	virtual void GetPreferedSize(INT32* width, INT32* height, INT32 cols, INT32 rows)
	{
		// um... that's not how "preferred" is spelled...
		*width  = m_width;
		*height = m_height;
	}

	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
	{
		UINT8 opacity = m_alpha;

		if (opacity != 255)
		{
			if (OpStatus::IsError(vis_dev->BeginOpacity(GetBounds(), opacity)))
				opacity = 255; ///< It failed, so we should not call EndOpacity
		}

		OpRect draw_rect(0, 0, rect.width, rect.height);

		if (m_wrap_in_circle)
		{
			int line_width = 3;
			OpRect border = GetBounds();
			border    =    border.InsetBy(line_width);
			draw_rect = draw_rect.InsetBy(line_width + 5);

			vis_dev->SetColor(255, 255, 255);
			vis_dev->DrawEllipse(border, line_width);
		}

		GetBorderSkin()->Draw(vis_dev, draw_rect);

		if (opacity != 255)
			vis_dev->EndOpacity();
	}

	int  m_alpha;
	int  m_width;
	int  m_height;
	bool m_wrap_in_circle;
};


enum QUEUED_ANIM_TYPE {
	QANIM_TYPE_MOVE,
	QANIM_TYPE_FADE
};

class QueuedAnimation : public Link
{
public:
	QueuedAnimation(QUEUED_ANIM_TYPE type, int val)
		: m_type(type)
		, m_val(val)
	{}

	virtual ~QueuedAnimation() { Out(); }

	QUEUED_ANIM_TYPE m_type;
	int m_val;
};


void FadeInOrOut (OpWidget* widget, bool fading_in, QuickAnimationListener* listener)
{
	QuickAnimationParams params(widget);

	params.curve     = ANIM_CURVE_LINEAR;
	params.fade_type = fading_in ? ANIM_FADE_IN : ANIM_FADE_OUT;
	params.duration  = 0.2;
	params.listener  = listener;

	g_animation_manager->startAnimation(params);
}
void FadeIn (OpWidget* widget, QuickAnimationListener* listener = NULL) { FadeInOrOut(widget, true , listener); }
void FadeOut(OpWidget* widget, QuickAnimationListener* listener = NULL) { FadeInOrOut(widget, false, listener); }


MouseGestureUI::MouseGestureUI()
	: m_mouse_gesture_window(NULL)
	, m_mg_man(NULL)
	, m_waiting_for_animation(false)
	, m_closing_sequence(NOT_CLOSING_UI)
{
}

MouseGestureUI::~MouseGestureUI()
{
	g_input_manager->SetMouseGestureListener(NULL);
}

OP_STATUS MouseGestureUI::Init()
{
	m_mg_man = g_input_manager->SetMouseGestureListener(this);

	return OpStatus::OK;
}

void MouseGestureUI::FadeOutEntireUI()
{
	FadeOut(m_mouse_gesture_window->GetFadingRoot());

	m_waiting_for_animation = true;
	m_closing_sequence      = FADE_OUT_ENTIRE_UI;

	OpWidget* big_dark_circle = m_mouse_gesture_window->GetBigDarkCircle();

	QuickAnimationParams params(big_dark_circle);

	params.curve     = ANIM_CURVE_SPEED_UP;
	params.listener  = this;
	params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;
	params.callback_param = THIS_IS_A_CLOSING_ANIMATION;
	params.end_rect  = big_dark_circle->GetRect().InsetBy(big_dark_circle->GetRect().width/4);

	g_animation_manager->startAnimation(params);
}

void MouseGestureUI::FadeInEntireUI(int selected_gesture_idx)
{
	m_mouse_gesture_window = OP_NEW(MouseGestureWindow, ());

	if (!m_mouse_gesture_window || OpStatus::IsError(m_mouse_gesture_window->Init(m_mg_man)))
	{
		OP_DELETE(m_mouse_gesture_window);
		m_mouse_gesture_window = NULL;
		return;
	}

	if (selected_gesture_idx < 0)
	{
		OpPoint origin = m_mg_man->GetGestureOrigin();
		const OpRect show_rect(origin.x - WINDOW_SIZE/2, origin.y - WINDOW_SIZE/2, WINDOW_SIZE, WINDOW_SIZE);
		m_mouse_gesture_window->Show(true, &show_rect);

		m_waiting_for_animation = true;
		FadeIn(m_mouse_gesture_window->GetFadingRoot(), this);

		return;
	}

	// If we got here, it means this was a fast gesture, and we only
	// need to display the final selection.

	// First, put the move_rct where it needs to be
	OpWidget* move_root = m_mouse_gesture_window->GetMovingRoot();
	OpRect move_rct = move_root->GetRect();
	int slide_amount = MOVEMENT_SLIDE_AMOUNT;
	int dx = 0;
	int dy = 0;
	QueuedAnimation* qanim = (QueuedAnimation*)m_queued_animations.First();

	while (qanim)
	{
		qanim->Out();

		if (qanim->m_type == QANIM_TYPE_MOVE)
		{
			int dir = qanim->m_val;

			switch(dir)
			{
				case 0:  dx -= slide_amount; break;
				case 1:  dx += slide_amount; break;
				case 2:  dy -= slide_amount; break;
				case 3:  dy += slide_amount; break;
			}
		}

		OP_DELETE(qanim);

		qanim = (QueuedAnimation*)m_queued_animations.First();
	}

	move_rct.OffsetBy(dx, dy);
	move_root->SetRect(move_rct);

	m_mouse_gesture_window->SetVisibilityOfLabelsAndArrows(m_mg_man, true, true);
	FadeIn(m_mouse_gesture_window->GetFadingRoot());

	// turn off branding text for fast gestures
	m_mouse_gesture_window->GetBrandingLabel()->SetVisibility(false);

	// show the arrow representing this gesture instead of the mouse icon
	m_mouse_gesture_window->GetMouseIcon()->SetVisibility(false);

	MouseGestureWindow::ArrowType arrow = GetArrowType(selected_gesture_idx);
	OpWidget* arrow_widget = NULL;
	if (arrow != MouseGestureWindow::NO_ARROW)
	{
		arrow_widget = m_mouse_gesture_window->GetConfirmationArrow(arrow);

		if (arrow_widget)
			arrow_widget->SetVisibility(true);
	}

	m_waiting_for_animation = true; // we're not waiting for the fade, but for the big_dark_circle to appear
	m_closing_sequence      = MOVING_TO_FINAL_SELECTION;

	OpWidget* big_dark_circle = m_mouse_gesture_window->GetBigDarkCircle();
	OpRect dest_rct = GetDarkCircleDestRect(arrow_widget, m_mouse_gesture_window->GetLabel(selected_gesture_idx));

	OpRect rct = dest_rct.InsetBy(dest_rct.width/4);
	big_dark_circle->SetRect(rct);

	OpPoint origin = g_input_manager->GetMousePosition() - rct.Center();
	const OpRect show_rect(origin.x - dx, origin.y - dy, WINDOW_SIZE, WINDOW_SIZE);
	m_mouse_gesture_window->Show(true, &show_rect);

	QuickAnimationParams params(big_dark_circle);

	params.curve     = ANIM_CURVE_BOUNCE;
	params.listener  = this;
	params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;
	params.callback_param = THIS_IS_A_CLOSING_ANIMATION;
	params.end_rect  = dest_rct;

	g_animation_manager->startAnimation(params);

}

void MouseGestureUI::OnGestureShow()
{
	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowGestureUI))
		return;

	if (m_mouse_gesture_window)
	{
		OP_ASSERT(false); // should never get here
		// This should already have been called by the inputmanager, but
		// if we got here, then it didn't happen, so we'll call it outselves.
		OnKillGestureUI();
	}

	FadeInEntireUI();
}

void MouseGestureUI::OnGestureMove(int dir)
{
	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowGestureUI))
		return;

	QueuedAnimation* qanim = OP_NEW(QueuedAnimation, (QANIM_TYPE_MOVE, dir));

	if (qanim)
	{
		qanim->Into(&m_queued_animations);

		if (!m_waiting_for_animation)
			StartNextAnimation();
	}
}

void MouseGestureUI::OnFadeOutGestureUI(int selected_gesture_idx)
{
	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowGestureUI))
		selected_gesture_idx = -1;

	MouseGestureManager::GestureNode* node = m_mg_man->m_gesture_tree.Get(selected_gesture_idx);

	if (!m_mouse_gesture_window)
	{
		// FAST GESTURE: Gesture UI is still invisible, but we need to show the selection.
		if (node && node->action)
		{
			// If we want a confirmation someday, this is where we trigger it.
			//FadeInEntireUI(selected_gesture_idx);
		}
		else
		{
			OnKillGestureUI();
		}

		return;
	}

	// SLOW GESTURE: Gesture UI has already faded in, so zoom the focus to selection.

	if (!node || !node->action)
	{
		// no selection was made, so just fade everything out right now
		FadeOutEntireUI();

		return;
	}

	// OK, so we have m_mouse_gesture_window, node, and node->action all not NULL.

	m_mouse_gesture_window->SetVisibilityOfLabelsAndArrows(m_mg_man, true);

	if (m_mouse_gesture_window->GetBrandingLabel()->IsVisible())
		FadeOut(m_mouse_gesture_window->GetBrandingLabel());

	// show the arrow representing this gesture instead of the mouse icon
	FadeOut(m_mouse_gesture_window->GetMouseIcon());

	MouseGestureWindow::ArrowType arrow = GetArrowType(selected_gesture_idx);
	OpWidget* arrow_widget = NULL;
	if (arrow != MouseGestureWindow::NO_ARROW)
	{
		arrow_widget = m_mouse_gesture_window->GetConfirmationArrow(arrow);

		if (arrow_widget)
			FadeIn(arrow_widget);
	}

	m_waiting_for_animation = true;
	m_closing_sequence      = MOVING_TO_FINAL_SELECTION;

	QuickAnimationParams params(m_mouse_gesture_window->GetBigDarkCircle());

	params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;
	params.curve     = ANIM_CURVE_BEZIER;
	params.duration  = 0.5;
	params.listener  = this;
	params.callback_param = THIS_IS_A_CLOSING_ANIMATION;
	params.end_rect  = GetDarkCircleDestRect(arrow_widget, m_mouse_gesture_window->GetLabel(selected_gesture_idx));

	g_animation_manager->startAnimation(params);
}

void MouseGestureUI::OnKillGestureUI()
{
	if (m_mouse_gesture_window)
	{
		m_mouse_gesture_window->Show(false);
		m_mouse_gesture_window->Close(false);
	}
	m_mouse_gesture_window = NULL;

	m_closing_sequence = NOT_CLOSING_UI;

	m_queued_animations.Clear();
}

void MouseGestureUI::StartNextAnimation(int callback_param)
{
	bool is_closing_animation = callback_param == THIS_IS_A_CLOSING_ANIMATION;

	m_waiting_for_animation = false;

	switch (m_closing_sequence)
	{
		case FADE_OUT_ENTIRE_UI:
		{
			// ui is now all faded out, so kill it
			if (is_closing_animation)
				OnKillGestureUI();
			return;
		}
		case PAUSE_TO_SHOW_FINAL_SELECTION:
		{
			// the pause is done, so fade out the entire ui
			if (is_closing_animation)
			{
				if (!m_mouse_gesture_window)
				{
					OnKillGestureUI();
					return;
				}
				FadeOutEntireUI();
			}
			return;
		}
		case MOVING_TO_FINAL_SELECTION:
		{
			if (is_closing_animation)
			{
				if (!m_mouse_gesture_window)
				{
					OnKillGestureUI();
					return;
				}
				// now that we have moved to the final selection,
				// pause a bit so the user can see what just happened
				m_waiting_for_animation = true;
				m_closing_sequence      = PAUSE_TO_SHOW_FINAL_SELECTION;

				OpWidget* moving_root = m_mouse_gesture_window->GetMovingRoot();

				// for the pause, we'll just animate to move to where it already is... but sloooowly...
				QuickAnimationParams params(moving_root);

				params.curve     = ANIM_CURVE_LINEAR;
				params.duration  = 0.7;
				params.listener  = this;
				params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;
				params.callback_param = THIS_IS_A_CLOSING_ANIMATION;
				params.end_rect  = moving_root->GetRect();

				g_animation_manager->startAnimation(params);
			}
			return;
		}
		case NOT_CLOSING_UI:
		{
			// not closing the ui, so just move on to the next animation
			break;
		}
		default:
		{
			OP_ASSERT(false); // m_closing_sequence should only ever have a value in the ClosingSequence enum
			break;
		}
	}

	QueuedAnimation* qanim = (QueuedAnimation*)m_queued_animations.First();

	if (!qanim || !m_mouse_gesture_window)
		return;

	qanim->Out();

	switch(qanim->m_type)
	{
		case QANIM_TYPE_MOVE:
		{
			int dx  = 0;
			int dy  = 0;
			int dir = qanim->m_val;
			int slide_amount = MOVEMENT_SLIDE_AMOUNT;

			switch(dir)
			{
				case 0:  dx = -slide_amount; break;
				case 1:  dx =  slide_amount; break;
				case 2:  dy = -slide_amount; break;
				case 3:  dy =  slide_amount; break;
			}

			OpWidget* moving_root = m_mouse_gesture_window->GetMovingRoot();
			OpRect rct = moving_root->GetRect();
			rct.OffsetBy(dx, dy);

			QuickAnimationParams params(moving_root);

			params.curve     = ANIM_CURVE_SLOW_DOWN;
			params.duration  = 0.4;
			params.listener  = this;
			params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;
			params.end_rect  = rct;

			g_animation_manager->startAnimation(params);

			m_waiting_for_animation = true;

			m_mouse_gesture_window->SetVisibilityOfLabelsAndArrows(m_mg_man);

			break;
		}
		case QANIM_TYPE_FADE:
		{
			break;
		}
	}

	OP_DELETE(qanim);
}

OpRect MouseGestureUI::GetDarkCircleDestRect(OpWidget* arrow, OpLabel* label)
{
	OpRect dest_rct;

	if (!label)
	{
		if (arrow)
			dest_rct = arrow->GetRect(true);
		else
			dest_rct = OpRect(WINDOW_SIZE/2 - 100, WINDOW_SIZE/2 - 100, 200, 200);
	}
	else
	{
		if (!arrow)
		{
			dest_rct = label->GetRect(true);
		}
		else
		{
			dest_rct = arrow->GetRect(true);
			dest_rct.UnionWith(label->GetRect(true));
		}
	}

	// Since the rect will be drawn on the moving-root, but
	// was calculated for the absolute root, we have to offset it.
	OpRect root_rct = m_mouse_gesture_window->GetMovingRoot()->GetRect(true);
	dest_rct.OffsetBy(-(root_rct.TopLeft()));

	// now make it square
	if (dest_rct.width >= dest_rct.height)
	{
		dest_rct.y += (dest_rct.height - dest_rct.width) / 2;
		dest_rct.height = dest_rct.width;
	}
	else
	{
		dest_rct.x += (dest_rct.width - dest_rct.height) / 2;
		dest_rct.width = dest_rct.height;
	}

	return dest_rct.InsetBy(-25); // just expand it a bit to give it some negative space
}

MouseGestureWindow::ArrowType MouseGestureUI::GetArrowType(int selected_gesture_idx)
{
	MouseGestureManager::GestureNode* node = m_mg_man->m_gesture_tree.Get(selected_gesture_idx);

	if (!node || node->addr[0] < 0 || node->addr[0] >= NUM_GESTURE_DIRECTIONS)
	{
		OP_ASSERT(false); // How did this even get called? This shouldn't be possible.
		return MouseGestureWindow::NO_ARROW;
	}

	if (node->addr[2] >= 0)
		return MouseGestureWindow::NO_ARROW; // we don't yet have art for gestures this long

	if (node->addr[1] >= 0) // a 2-move gesture
	{
		switch (node->addr[0])
		{
			case 0:
				switch (node->addr[1])
				{
					case 1:  return MouseGestureWindow::ARROW_LR;
					case 2:  return MouseGestureWindow::ARROW_LU;
					case 3:  return MouseGestureWindow::ARROW_LD;
					default: return MouseGestureWindow::NO_ARROW;
				}
			case 1:
				switch (node->addr[1])
				{
					case 0:  return MouseGestureWindow::ARROW_RL;
					case 2:  return MouseGestureWindow::ARROW_RU;
					case 3:  return MouseGestureWindow::ARROW_RD;
					default: return MouseGestureWindow::NO_ARROW;
				}
			case 2:
				switch (node->addr[1])
				{
					case 0:  return MouseGestureWindow::ARROW_UL;
					case 1:  return MouseGestureWindow::ARROW_UR;
					case 3:  return MouseGestureWindow::ARROW_UD;
					default: return MouseGestureWindow::NO_ARROW;
				}
			case 3:
				switch (node->addr[1])
				{
					case 0:  return MouseGestureWindow::ARROW_DL;
					case 1:  return MouseGestureWindow::ARROW_DR;
					case 2:  return MouseGestureWindow::ARROW_DU;
					default: return MouseGestureWindow::NO_ARROW;
				}
			default: return MouseGestureWindow::NO_ARROW;
		}
	}
	else // (node->addr[1] < 0), so a 1-move gesture
	{
		switch (node->addr[0])
		{
			case 0:  return MouseGestureWindow::ARROW_L;
			case 1:  return MouseGestureWindow::ARROW_R;
			case 2:  return MouseGestureWindow::ARROW_U;
			case 3:  return MouseGestureWindow::ARROW_D;
			default: return MouseGestureWindow::NO_ARROW;
		}
	}
}


/////////  MouseGestureWindow   ////////////


MouseGestureWindow::MouseGestureWindow()
	: m_parent_window(NULL)
	, m_fading_root(NULL)
	, m_moving_root(NULL)
	, m_big_dark_circle(NULL)
	, m_branding_label(NULL)
	, m_mouse_icon(NULL)
	, m_arrow_l(NULL)
	, m_arrow_r(NULL)
	, m_arrow_u(NULL)
	, m_arrow_d(NULL)
{
	for (int i = 0; i < NUM_ARROWS; i++)
	{
		m_arrows[i] = NULL;
	}
}

MouseGestureWindow::~MouseGestureWindow()
{
	g_mouse_gesture_ui->OnMouseGestureWindowClosing();

	if (m_parent_window)
		m_parent_window->RemoveListener(this);
}

enum AlignmentType
{
	CORNER_TOP_LEFT,
	CORNER_TOP_RIGHT,
	CORNER_BOTTOM_LEFT,
	CORNER_BOTTOM_RIGHT,
	EDGE_LEFT,
	EDGE_RIGHT,
	EDGE_TOP,
	EDGE_BOTTOM
};

OP_STATUS make_new_arrow(OpWidget* root, OpWidget** arrow, const char* name, int offset_x = 0, int offset_y = 0)
{
	SpriteWidget* sprite = OP_NEW(SpriteWidget, (name));
	*arrow = sprite;

	if (!sprite)
		return OpStatus::ERR_NO_MEMORY;

	root->AddChild(sprite);
	sprite->CenterAt(WINDOW_SIZE/2 + offset_x, WINDOW_SIZE/2 + offset_y);
	sprite->SetVisibility(false);

	return OpStatus::OK;
}
OP_STATUS make_new_arrow(OpWidget* root, OpWidget** arrow, const char* name, OpWidget* alignment_widget, AlignmentType corner_or_edge)
{
	RETURN_IF_ERROR(make_new_arrow(root, arrow, name));

	OpRect al_rect   = alignment_widget->GetRect();
	OpRect curr_rect = (*arrow)->GetRect();

	switch (corner_or_edge)
	{
		case CORNER_TOP_LEFT:
		{
			curr_rect.x = al_rect.x;
			curr_rect.y = al_rect.y;
			break;
		}
		case CORNER_TOP_RIGHT:
		{
			curr_rect.x = al_rect.Right() - curr_rect.width;
			curr_rect.y = al_rect.y;
			break;
		}
		case CORNER_BOTTOM_LEFT:
		{
			curr_rect.x = al_rect.x;
			curr_rect.y = al_rect.Bottom() - curr_rect.height;
			break;
		}
		case CORNER_BOTTOM_RIGHT:
		{
			curr_rect.x = al_rect.Right()  - curr_rect.width;
			curr_rect.y = al_rect.Bottom() - curr_rect.height;
			break;
		}
		case EDGE_LEFT:
		{
			curr_rect.x = al_rect.x;
			break;
		}
		case EDGE_RIGHT:
		{
			curr_rect.x = al_rect.Right()  - curr_rect.width;
			break;
		}
		case EDGE_TOP:
		{
			curr_rect.y = al_rect.y;
			break;
		}
		case EDGE_BOTTOM:
		{
			curr_rect.y = al_rect.Bottom() - curr_rect.height;
			break;
		}
		default:
		{
			OP_ASSERT(false); // how did we get here?
			break;
		}
	}

	(*arrow)->SetRect(curr_rect);

	return OpStatus::OK;
}

OP_STATUS MouseGestureWindow::Init(MouseGestureManager* mg_man)
{
	// Linux requires us to get a parent window when using OpWindow::EFFECT_TRANSPARENT.
	// Listen to it so we can delete ourself if it happens to be removed when we're still alive.
	m_parent_window = g_application->GetActiveBrowserDesktopWindow();
	if (m_parent_window)
		RETURN_IF_ERROR(m_parent_window->AddListener(this));

	RETURN_IF_ERROR(DesktopWidgetWindow::Init(OpWindow::STYLE_TOOLTIP, "Mouse Gesture Window", m_parent_window ? m_parent_window->GetOpWindow() : NULL,
											  NULL, OpWindow::EFFECT_TRANSPARENT));

	OpWidget *root = m_widget_container->GetRoot();
	root->SetHasCssBorder(false);
	root->SetSkinned(true);

	m_fading_root = OP_NEW(OpWidget, ());
	if (!m_fading_root)
		return OpStatus::ERR_NO_MEMORY;
	m_fading_root->SetRect(OpRect(0, 0, WINDOW_SIZE, WINDOW_SIZE));
	root->AddChild(m_fading_root);

	m_moving_root = OP_NEW(OpWidget, ());
	if (!m_moving_root)
		return OpStatus::ERR_NO_MEMORY;
	m_moving_root->SetRect(OpRect(0, 0, WINDOW_SIZE, WINDOW_SIZE));

	bool we_got_some_errors_up_in_there = false;

	for (int i = 0; i < (int)mg_man->m_gesture_tree.GetCount(); i++)
	{
		MouseGestureManager::GestureNode* node = mg_man->m_gesture_tree.Get(i);

		int j;
		for (j = 0; j < MAX_SHORTCUT_SEQUENCE_LENGTH; j++)
		{
			if (node->addr[j] == -1)
				break;
		}
		j = max(0, j-1);
		int dir = node->addr[j];

		JUSTIFY justify = JUSTIFY_CENTER;
		if (dir == 0) // left
			justify = JUSTIFY_RIGHT;
		if (dir == 1) // right
			justify = JUSTIFY_LEFT;

		OpLabel* label;
		if (OpStatus::IsError(OpLabel::Construct(&label)))
		{
			we_got_some_errors_up_in_there = true;
			continue;
		}

		m_labels.Add(label);
		m_fading_root->AddChild(label);

		label->SetWrap(true);
		label->UpdateSkinPadding();
		label->SetRect(OpRect(0, 0, (dir == 0 || dir == 1) ? LABEL_WIDTH_SIDES : LABEL_WIDTH_TOP_BOTTOM, 0)); // shorter on the left and right sides

		if (node->action)
		{
			OpString str;
			OpStatus::Ignore(OpInputAction::GetLocaleStringFromAction(node->action->GetAction(), str));
			OpStatus::Ignore(label->SetText(str));
		}
        
		label->SetFontInfo(NULL, FONT_HEIGHT, false, 7, JUSTIFY_LEFT);
		int w = label->GetTextWidth();
		int h = label->GetTextHeight();
		label->SetFontInfo(NULL, FONT_HEIGHT, false, 7, justify);
		int x = WINDOW_SIZE/2;
		int y = WINDOW_SIZE/2;

		for (; j >= 0; j--)
		{
			switch(node->addr[j])
			{
				case 0:  x -= MOVEMENT_SLIDE_AMOUNT; break;
				case 1:  x += MOVEMENT_SLIDE_AMOUNT; break;
				case 2:  y -= MOVEMENT_SLIDE_AMOUNT; break;
				case 3:  y += MOVEMENT_SLIDE_AMOUNT; break;
			}
		}


		switch (dir)
		{
			// NOTE: These values (1, 2, 5) are tweaks based on the size
			//       of the art assets and font metrics/rendering.
			case 0: // left
				x -= 1.0 * w     + MOVEMENT_SLIDE_AMOUNT/2;
				y -= 0.5 * h + 1;
				break;
			case 1: // right
				x += 0.0 * w     + MOVEMENT_SLIDE_AMOUNT/2;
				y -= 0.5 * h + 2;
				break;
			case 2: // up
				x -= 0.5 * w;
				y -= 1.0 * h     + MOVEMENT_SLIDE_AMOUNT/2;
				break;
			case 3: // down
				x -= 0.5 * w;
				y += 0.0 * h - 5 + MOVEMENT_SLIDE_AMOUNT/2;
				break;
			default: // do nothing
				break;
		}

		label->SetRect(OpRect(x, y, w, h));
		label->SetVisibility(false);
	}

	m_fading_root->AddChild(m_moving_root);

	if (we_got_some_errors_up_in_there)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(make_new_arrow(m_moving_root, &m_arrow_l, "Mouse Gesture Skin Arrow Left" , -MOVEMENT_SLIDE_AMOUNT,  0));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, &m_arrow_r, "Mouse Gesture Skin Arrow Right",  MOVEMENT_SLIDE_AMOUNT,  0));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, &m_arrow_u, "Mouse Gesture Skin Arrow Up"   ,  0, -MOVEMENT_SLIDE_AMOUNT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, &m_arrow_d, "Mouse Gesture Skin Arrow Down" ,  0,  MOVEMENT_SLIDE_AMOUNT));

	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_L , "Mouse Gesture Skin Arrow Left"));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_R , "Mouse Gesture Skin Arrow Right"));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_U , "Mouse Gesture Skin Arrow Up"));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_D , "Mouse Gesture Skin Arrow Down"));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_LR, "Mouse Gesture Skin Arrow LR", m_arrows[ARROW_R], EDGE_RIGHT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_LU, "Mouse Gesture Skin Arrow LU", m_arrows[ARROW_U], CORNER_TOP_LEFT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_LD, "Mouse Gesture Skin Arrow LD", m_arrows[ARROW_D], CORNER_BOTTOM_LEFT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_RL, "Mouse Gesture Skin Arrow RL", m_arrows[ARROW_L], EDGE_LEFT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_RU, "Mouse Gesture Skin Arrow RU", m_arrows[ARROW_U], CORNER_TOP_RIGHT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_RD, "Mouse Gesture Skin Arrow RD", m_arrows[ARROW_D], CORNER_BOTTOM_RIGHT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_UL, "Mouse Gesture Skin Arrow UL", m_arrows[ARROW_L], CORNER_TOP_LEFT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_UR, "Mouse Gesture Skin Arrow UR", m_arrows[ARROW_R], CORNER_TOP_RIGHT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_UD, "Mouse Gesture Skin Arrow UD", m_arrows[ARROW_D], EDGE_BOTTOM));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_DL, "Mouse Gesture Skin Arrow DL", m_arrows[ARROW_L], CORNER_BOTTOM_LEFT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_DR, "Mouse Gesture Skin Arrow DR", m_arrows[ARROW_R], CORNER_BOTTOM_RIGHT));
	RETURN_IF_ERROR(make_new_arrow(m_moving_root, m_arrows + ARROW_DU, "Mouse Gesture Skin Arrow DU", m_arrows[ARROW_U], EDGE_TOP));

	SpriteWidget* sprite = OP_NEW(SpriteWidget, ("Mouse Gesture Skin Mouse"));
	if (!sprite)
		return OpStatus::ERR_NO_MEMORY;
	m_moving_root->AddChild(sprite);
	sprite->CenterAt(WINDOW_SIZE/2, WINDOW_SIZE/2);
	m_mouse_icon = sprite;

	//if (should_we_display_the_branding_text_every_time?)
	{
		OpLabel* label;

		if (OpStatus::IsError(OpLabel::Construct(&label)))
			return OpStatus::ERR_NO_MEMORY;

		m_moving_root->AddChild(label);

		label->SetWrap(true);
		label->UpdateSkinPadding();
		label->SetFontInfo(NULL, FONT_HEIGHT*3/4, false, 7, JUSTIFY_CENTER);
		int w = 110;
		label->SetRect(OpRect(0, 0, w, 0));
		label->SetForegroundColor(OP_RGB(255, 255, 255));

		OpString str;
		OpStatus::Ignore(g_languageManager->GetString(Str::S_OPERA_MOUSE_GESTURES, str));
		OpStatus::Ignore(label->SetText(str));

		int h = label->GetTextHeight();
		int x = WINDOW_SIZE/2 - w/2;
		int y = WINDOW_SIZE/2 + CIRCLE_SIZE/2 - h - 12;

		label->SetRect(OpRect(x, y, w, h));

		m_branding_label = label;

		QuickAnimationParams params(m_branding_label);

		params.curve     = ANIM_CURVE_SPEED_UP;
		params.duration  = 4.0;
		params.fade_type = ANIM_FADE_OUT;

		g_animation_manager->startAnimation(params);
	}

	SpriteWidget* big_dark_circle = OP_NEW(SpriteWidget, ("Mouse Gesture Skin Circle", 192, CIRCLE_SIZE, CIRCLE_SIZE, true));
	if (!big_dark_circle)
		return OpStatus::ERR_NO_MEMORY;
	m_moving_root->AddChild(big_dark_circle);
	big_dark_circle->CenterAt(WINDOW_SIZE/2, WINDOW_SIZE/2);
	m_big_dark_circle = big_dark_circle;

	OpRect dest_rect = big_dark_circle->GetRect();

	QuickAnimationParams params(big_dark_circle);

	params.curve      = ANIM_CURVE_BOUNCE;
	params.move_type  = ANIM_MOVE_RECT_TO_ORIGINAL;
	params.start_rect = dest_rect.InsetBy(dest_rect.width/4);
	params.end_rect   = dest_rect;

	g_animation_manager->startAnimation(params);

	// no need to fade these in, as the entire ui is fading in
	SetVisibilityOfLabelsAndArrows(mg_man, false, true);

	return OpStatus::OK;
}

void MouseGestureWindow::SetVisibilityOfLabelsAndArrows(MouseGestureManager* mg_man,
                                                        bool keep_only_active_selection,
                                                        bool no_fading_animations)
{
	MouseGestureManager::GestureNode* node = mg_man->m_gesture_tree.Get(mg_man->m_active_idx);
	if (!node)
		return;

	// first set visibility of labels
	for (int i = 0; i < (int)mg_man->m_gesture_tree.GetCount(); i++)
	{
		OpWidget* child = m_labels.Get(i);
		if (!child)
		{
			OP_ASSERT(false); // should not be possible
			continue;
		}

		bool was_visible     = !!child->IsVisible();
		bool will_be_visible = false;

		if (i == mg_man->m_active_idx) // the selected gesture node
		{
			child->SetForegroundColor(OP_RGB(255, 255, 160));
			will_be_visible = true;
		}
		if (!keep_only_active_selection)
		{
			for (int dir = 0; dir < NUM_GESTURE_DIRECTIONS; dir++)
			{
				if (node->dirs[dir] == i) // an available neighbor node
				{
					child->SetForegroundColor(OP_RGB(255, 255, 255));
					will_be_visible = true;
				}
			}
		}

		if (will_be_visible)
		{
			if (!was_visible && !no_fading_animations)
				FadeIn(child);
			else
				child->SetVisibility(true);
		}
		else
		{
			if (was_visible && !no_fading_animations)
				FadeOut(child);
			else
				child->SetVisibility(false);
		}
	}

	// then set visibility of arrows
	for (int dir = 0; dir < NUM_GESTURE_DIRECTIONS; dir++)
	{
		OpWidget* child = GetAvailabilityArrow(dir);
		if (!child)
		{
			OP_ASSERT(false); // should not be possible
			continue;
		}

		bool was_visible     = !!child->IsVisible();
		bool will_be_visible = !keep_only_active_selection && (node->dirs[dir] >= 0);

		if (will_be_visible)
		{
			if (!was_visible && !no_fading_animations)
				FadeIn(child);
			else
				child->SetVisibility(true);
		}
		else
		{
			if (was_visible && !no_fading_animations)
				FadeOut(child);
			else
				child->SetVisibility(false);
		}
	}
}

OpWidget* MouseGestureWindow::GetAvailabilityArrow(int dir)
{
	switch(dir)
	{
		case 0:  return m_arrow_l;
		case 1:  return m_arrow_r;
		case 2:  return m_arrow_u;
		case 3:  return m_arrow_d;
		default: return NULL;
	}
}

OpWidget* MouseGestureWindow::GetConfirmationArrow(MouseGestureWindow::ArrowType type)
{
	if (type == NO_ARROW || type >= NUM_ARROWS)
		return NULL;

	return m_arrows[type];
}

void MouseGestureWindow::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (desktop_window == m_parent_window)
		Close(true);
	else
		OP_DELETE(this);

	g_mouse_gesture_ui->OnMouseGestureWindowClosing();
}


#endif // VEGA_OPPAINTER_SUPPORT

