/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/display/vis_dev.h"
#include "modules/display/coreview/coreview.h"
#include "modules/display/color.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/dochand/docman.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"

#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif

// == ScrollbarColors ==========================================================

UINT32 Brightness(UINT32 col, float brightness)
{
	INT32 r = (INT32)(((UINT8*)&col)[2] + brightness * 255 * 2.3);
	INT32 g = (INT32)(((UINT8*)&col)[1] + brightness * 255 * 2.59);
	INT32 b = (INT32)(((UINT8*)&col)[0] + brightness * 255 * 2.11);
	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	return OP_RGB(r, g, b);
}

ScrollbarColors::ScrollbarColors()
	: face(USE_DEFAULT_COLOR)
	, arrow(USE_DEFAULT_COLOR)
	, base(USE_DEFAULT_COLOR)
	, shadow(USE_DEFAULT_COLOR)
	, darkshadow(USE_DEFAULT_COLOR)
	, highlight(USE_DEFAULT_COLOR)
	, light(USE_DEFAULT_COLOR)
	, track(USE_DEFAULT_COLOR)
	, is_modified(FALSE)
	, is_enabled(TRUE)
{
}

UINT32 ScrollbarColors::Face(UINT32 def_col)
{
	if (!is_enabled)
		return def_col;
	UINT32 color = def_col;
	if (base != USE_DEFAULT_COLOR)
		color = base;
	return face == USE_DEFAULT_COLOR ? color : face;
}

UINT32 ScrollbarColors::Arrow(UINT32 def_col)
{
	if (!is_enabled)
		return def_col;
	return arrow == USE_DEFAULT_COLOR ? def_col : arrow;
}

UINT32 ScrollbarColors::Shadow(UINT32 def_col)
{
	if (!is_enabled)
		return def_col;
	UINT32 color = def_col;
	if (base != USE_DEFAULT_COLOR)
		color = Brightness(base, -0.5f);
	return shadow == USE_DEFAULT_COLOR ? color : shadow;
}

UINT32 ScrollbarColors::DarkShadow(UINT32 def_col)
{
	if (!is_enabled)
		return def_col;
	UINT32 color = def_col;
	if (base != USE_DEFAULT_COLOR)
		color = Brightness(base, -0.8f);
	return darkshadow == USE_DEFAULT_COLOR ? color : darkshadow;
}

UINT32 ScrollbarColors::Highlight(UINT32 def_col)
{
	if (!is_enabled)
		return def_col;
	UINT32 color = def_col;
	if (base != USE_DEFAULT_COLOR)
		color = Brightness(base, 0.2f);
	return highlight == USE_DEFAULT_COLOR ? color : highlight;
}

UINT32 ScrollbarColors::Light(UINT32 def_col)
{
	if (!is_enabled)
		return def_col;
	UINT32 color = def_col;
	if (base != USE_DEFAULT_COLOR)
		color = base;
	return light == USE_DEFAULT_COLOR ? color : light;
}

UINT32 ScrollbarColors::Track(UINT32 def_col)
{
	if (!is_enabled)
		return def_col;
	UINT32 color = def_col;
	if (base != USE_DEFAULT_COLOR)
		color = Brightness(base, 0.2f);
	else if (highlight != USE_DEFAULT_COLOR)
		color = Brightness(highlight, 0.5f);
	return track == USE_DEFAULT_COLOR ? color : track;
}

// == OpSmoothScroller ===========================================================

OpSmoothScroller::OpSmoothScroller()
#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
	: m_running(FALSE), m_value(0), m_target_value(0), m_start_time(0), m_scroll_time(20), m_length(0), m_auto_delta(0)
#endif // WIDGETS_ASYNC_SMOOTHSCROLL
{
}

OpSmoothScroller::~OpSmoothScroller()
{
	StopSmoothScroll();
}

void OpSmoothScroller::PrepareAutoScroll(int delta)
{
#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
#ifdef WIDGETS_AUTO_THROW_SCROLL
	m_start_time = g_op_time_info->GetWallClockMS();
	m_auto_delta = delta;
	m_auto_delta = MAX(m_auto_delta, -20);
	m_auto_delta = MIN(m_auto_delta, 20);
	if (m_auto_delta == 0)
		StopSmoothScroll();
#endif
#endif
}

void OpSmoothScroller::AutoScroll(int value)
{
#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
#ifdef WIDGETS_AUTO_THROW_SCROLL
	// A threshold for when we should just stop instead of accelerate. Depending on when ResetAutoScroll was called.
	if (g_op_time_info->GetWallClockMS() < m_start_time + 200 && (m_auto_delta > 5 || m_auto_delta < -5))
	{
		m_value = value;
		Run();
	}
#endif
#endif
}

void OpSmoothScroller::SetValueSmoothScroll(INT32 value, INT32 target_value)
{
#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
	if (!m_running)
		m_value = value;

	m_target_value = target_value;
	m_length = (INT32) (m_target_value - m_value);

	m_auto_delta = 0;

	StopSmoothScroll();

	m_start_time = g_op_time_info->GetWallClockMS() - 5; // Guess how long time it would take for a posted message to get delivered.

	// Start immediately
	HandleCallback(MSG_SMOOTHSCROLL_MESSAGE, (MH_PARAM_1)0, (MH_PARAM_2)0);
#else // WIDGETS_ASYNC_SMOOTHSCROLL
	int step = op_abs((int)(value - target_value)) / 10;

	if (step < 1)
		step = 1;
	else if (step > 60)
		step = 60;

	while (value != target_value)
	{
		if (value > target_value)
		{
			value -= step;
			if (value < target_value)
				value = target_value;
		}
		else
		{
			value += step;
			if (value > target_value)
				value = target_value;
		}

		double ticks = g_op_time_info->GetWallClockMS();

		BOOL ret = OnSmoothScroll(value);
		if (!ret)
			return;

		double ticks_after = g_op_time_info->GetWallClockMS();

		ticks = ticks_after - ticks;

		if (value != target_value && ticks < 10)
		{
#ifdef MSWIN
			Sleep(5);
#else
			while (g_op_time_info->GetWallClockMS() < ticks_after + 5) ;
#endif
		}
		else if (ticks >= 20)
		{
			step *= 2;
		}
	}
#endif // !WIDGETS_ASYNC_SMOOTHSCROLL
}

void OpSmoothScroller::StopSmoothScroll()
{
#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
	if (g_main_message_handler && m_running)
	{
		m_start_time = 0;
		m_running = FALSE;
		g_main_message_handler->RemoveDelayedMessage(MSG_SMOOTHSCROLL_MESSAGE, (MH_PARAM_1)this, (MH_PARAM_2)this);
		g_main_message_handler->UnsetCallBacks(this);
	}
#endif // WIDGETS_ASYNC_SMOOTHSCROLL
}

#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
void OpSmoothScroller::Run()
{
	if (!m_running)
	{
		m_start_time = g_op_time_info->GetWallClockMS();
		OP_ASSERT(!g_main_message_handler->HasCallBack(this, MSG_SMOOTHSCROLL_MESSAGE, (MH_PARAM_1)this));
		g_main_message_handler->SetCallBack(this, MSG_SMOOTHSCROLL_MESSAGE, (MH_PARAM_1)this);
		g_main_message_handler->PostDelayedMessage(MSG_SMOOTHSCROLL_MESSAGE, (MH_PARAM_1)this, (MH_PARAM_2)this, WIDGETS_SMOOTHSCROLL_DELAY_TIME);
		m_running = TRUE;
	}
}
#endif // WIDGETS_ASYNC_SMOOTHSCROLL

void OpSmoothScroller::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
	if (msg != MSG_SMOOTHSCROLL_MESSAGE)
		return;

	double last_start_time = m_start_time;

	StopSmoothScroll();

	INT32 limit_min, limit_max;
	if (m_auto_delta && GetLimits(limit_min, limit_max))
	{
		int old_value = (int)m_value;
		m_value += m_auto_delta;

		if (m_value > limit_max)
			m_value = limit_max;
		if (m_value < limit_min)
			m_value = limit_min;

		m_auto_delta = m_auto_delta / 1.15;
		if (op_fabs(m_auto_delta) < 0.25)
			m_auto_delta = 0;

		if (m_auto_delta)
			Run();

		if (old_value != (int)m_value)
		{
			BOOL succeeded = OnSmoothScroll((int)m_value);
			if (!succeeded)
				StopSmoothScroll();
		}
	}
	else if (m_value != m_target_value)
	{
		// Initialize or limit m_scroll_time to WIDGETS_SMOOTHSCROLL_DELAY_TIME
		if (m_scroll_time < WIDGETS_SMOOTHSCROLL_DELAY_TIME)
			m_scroll_time = WIDGETS_SMOOTHSCROLL_DELAY_TIME;

		double delta = (double)m_length / (WIDGETS_SMOOTHSCROLL_COMPLETION_TIME / m_scroll_time);
		if (delta > 0)
			delta = MIN((double)m_target_value - m_value, delta);
		else
			delta = MAX((double)m_target_value - m_value, delta);

#if WIDGETS_SMOOTHSCROLL_SLOW_STOP != 0
		// Only if we render fast enough, and make sure that immediate calls (when par1 is zero) at
		// least scrolls one pixel so the action that triggered the call will know it was handled
		if (m_scroll_time < WIDGETS_SMOOTHSCROLL_COMPLETION_TIME / 2)
		{
			double deaccel_delta = (m_target_value - m_value) / WIDGETS_SMOOTHSCROLL_SLOW_STOP;
			if (delta < 0)
			{
				delta = MAX(delta, deaccel_delta);
				if (!par1)
					delta = MIN(delta, -1.0);
				else
					delta = MIN(delta, -0.25);
			}
			else
			{
				delta = MIN(delta, deaccel_delta);
				if (!par1)
					delta = MAX(delta, 1.0);
				else
					delta = MAX(delta, 0.25);
			}
		}
#endif

		int changed = (int)m_value != (int)(m_value + delta);
		m_value += delta;

		// If not ready, post a new message for next scroll. Do it before OnSmoothScroll so timestamp isn't affected by the scroll.
		if ((int)m_value != m_target_value)
			Run();

		if (changed)
		{
			BOOL succeeded = OnSmoothScroll((int)m_value);
			if (!succeeded)
				StopSmoothScroll();

			// Update m_scroll_time with time passed from last post message to after scroll (now).
			if (last_start_time && succeeded)
				m_scroll_time = g_op_time_info->GetWallClockMS() - last_start_time;
		}
	}
#endif // WIDGETS_ASYNC_SMOOTHSCROLL
}

// == OpScrollbar ===========================================================

#ifdef WIDGETS_IME_SUPPORT
void OpScrollbar::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OpWidget::OnKeyboardInputLost(new_input_context, old_input_context, reason);
	if (old_input_context == this && reason == FOCUS_REASON_MOUSE && new_input_context == GetParentInputContext() && parent && parent->IsOfType(WIDGET_TYPE_MULTILINE_EDIT))
	{
		OpMultilineEdit* parent_medit = static_cast<OpMultilineEdit*>(parent);
		parent_medit->SetFocusComesFromScrollbar();
	}
}
#endif // WIDGETS_IME_SUPPORT

BOOL OpScrollbar::OnInputAction(OpInputAction* action)
{
	if (!parent || parent->GetType() != WIDGET_TYPE_MULTILINE_EDIT)
		return FALSE;
	OpMultilineEdit* parent_multiline = static_cast<OpMultilineEdit*>(parent);
	// When OpScrollbar is focused, underlying OpMultilineEdit won't get scrolling
	// actions - they are handled by OpScrollbar, external_input_context (outside
	// of OpMultilineEdit and TextAreaObject) or silently discarded (that is
	// always return TRUE from this method).  This is done - when only scrollbar
	// is focused - to prevent OpMultiEdit:
	// 1. showing blinking cursor,
	// 2. showing IME,
	// 3. making editing changes to text contents.
	OpInputContext* external_input_context = GetParentInputContext();
	if (external_input_context)
		external_input_context = external_input_context->GetParentInputContext();
	if (external_input_context)
		external_input_context = external_input_context->GetParentInputContext();
	// OpMultilineEdit always has both scrollbars, so OnScrollAction() and
	// SetScrollValue() won't fail.
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SCROLL_UP:
		{
			if (!parent_multiline->OnScrollAction(-parent_multiline->GetLineHeight(), TRUE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
		case OpInputAction::ACTION_SCROLL_DOWN:
		{
			if (!parent_multiline->OnScrollAction(parent_multiline->GetLineHeight(), TRUE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
		case OpInputAction::ACTION_SCROLL_LEFT:
		{
			if (!parent_multiline->OnScrollAction(-parent_multiline->GetLineHeight(), FALSE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
		case OpInputAction::ACTION_SCROLL_RIGHT:
		{
			if (!parent_multiline->OnScrollAction(parent_multiline->GetLineHeight(), FALSE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
		case OpInputAction::ACTION_GO_TO_START:
		{
			if (!parent_multiline->SetScrollValue(limit_min, TRUE, TRUE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
		case OpInputAction::ACTION_GO_TO_END:
		{
			if (!parent_multiline->SetScrollValue(limit_max, TRUE, TRUE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
		case OpInputAction::ACTION_PAGE_UP:
		{
			if (!parent_multiline->OnScrollAction(-parent_multiline->GetVisibleHeight(), TRUE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
		case OpInputAction::ACTION_PAGE_DOWN:
		{
			if (!parent_multiline->OnScrollAction(parent_multiline->GetVisibleHeight(), TRUE))
				g_input_manager->InvokeAction(action, external_input_context);
			return TRUE;
		}
#ifdef ACTION_FOCUS_FORM_ENABLED
		case OpInputAction::ACTION_FOCUS_FORM:
		{
			GetParentInputContext()->SetFocus(FOCUS_REASON_ACTION);
#ifdef WIDGETS_IME_SUPPORT
			parent_multiline->IMEHandleFocusEvent(TRUE);
#endif // WIDGETS_IME_SUPPORT
			return TRUE;
		}
#endif // ACTION_FOCUS_FORM_ENABLED
#ifdef ACTION_UNFOCUS_FORM_ENABLED
		case OpInputAction::ACTION_UNFOCUS_FORM:
		{
#ifdef WIDGETS_IME_SUPPORT
			parent_multiline->IMEHandleFocusEvent(FALSE);
#endif // WIDGETS_IME_SUPPORT
			FramesDocument* doc = vis_dev->GetDocumentManager()->GetCurrentDoc();
			if (doc && doc->GetHtmlDocument())
				doc->GetHtmlDocument()->ResetFocusedElement();
			g_input_manager->InvokeAction(action, GetParentInputContext());
			return TRUE;
		}
#endif // ACTION_UNFOCUS_FORM_ENABLED
#ifdef USE_OP_CLIPBOARD
		case OpInputAction::ACTION_CUT:
		case OpInputAction::ACTION_COPY:
		case OpInputAction::ACTION_PASTE:
		case OpInputAction::ACTION_PASTE_MOUSE_SELECTION:
#endif // USE_OP_CLIPBOARD
#ifdef WIDGETS_UNDO_REDO_SUPPORT
		case OpInputAction::ACTION_UNDO:
		case OpInputAction::ACTION_REDO:
#endif // WIDGETS_UNDO_REDO_SUPPORT
		case OpInputAction::ACTION_CONVERT_HEX_TO_UNICODE:
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_WORD:
		case OpInputAction::ACTION_DELETE_TO_END_OF_LINE:
		case OpInputAction::ACTION_BACKSPACE:
		case OpInputAction::ACTION_BACKSPACE_WORD:
		{
			// Don't allow editing of OpMultilineEdit.
			return TRUE;
		}
	}
	// Don't allow editing of OpMultilineEdit.
	if (action->IsMoveAction() || action->IsRangeAction())
		return TRUE;

	return FALSE;
}

OP_STATUS OpScrollbar::Construct(OpScrollbar** obj, BOOL horizontal)
{
	*obj = OP_NEW(OpScrollbar, (horizontal));
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpScrollbar::OpScrollbar(BOOL horizontal) :
	small_step(10)
	, big_step(50)
	, limit_min(0)
	, limit_max(0)
	, limit_visible(0)
	, value(0)
	, horizontal(horizontal)
	, is_moving(FALSE)
	, moveofs(0)
	, scrollbar_colors()
	, hit_part(INVALID_PART)
	, hover_part(INVALID_PART)
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, m_accessible_arrow_up_part(NULL)
	, m_accessible_arrow_down_part(NULL)
	, m_accessible_page_up_part(NULL)
	, m_accessible_page_down_part(NULL)
	, m_accessible_knob_part(NULL)
#endif
{
#ifdef SKIN_SUPPORT
	GetBorderSkin()->SetImage(horizontal ? "Scrollbar Horizontal Skin" : "Scrollbar Vertical Skin");
	SetSkinned(TRUE);
#endif
}

void OpScrollbar::SetScrollbarColors(ScrollbarColors* colors)
{
	scrollbar_colors = *colors;
//	Invalidate(GetBounds());
}

void OpScrollbar::SetLimit(INT32 limit_min, INT32 limit_max, INT32 how_much_visible)
{
	limit_max = MAX(limit_min, limit_max);

	if (this->limit_min != limit_min || 
		this->limit_max != limit_max ||
		this->limit_visible != how_much_visible)
		Invalidate(GetBounds());

	this->limit_min = limit_min;
	this->limit_max = limit_max;
	this->limit_visible = how_much_visible;
	if (value < this->limit_min)
		SetValue(this->limit_min, FALSE);
	else if (value > this->limit_max)
		SetValue(this->limit_max, FALSE);
}

void OpScrollbar::SetSteps(INT32 small_step, INT32 big_step)
{
	this->small_step = small_step;
	this->big_step = big_step;
}

void OpScrollbar::SetValue(INT32 new_value, BOOL caused_by_input, BOOL send_onscroll, BOOL smooth_scroll)
{
	if (new_value > limit_max)
		new_value = limit_max;
	if (new_value < limit_min)
		new_value = limit_min;
	if (new_value == value)
		return;

	if (smooth_scroll && caused_by_input && listener && send_onscroll && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SmoothScrolling))
	{
		SetValueSmoothScroll(value, new_value);
	}
	else
	{
		StopSmoothScroll();

		int old_value = value;
		value = new_value;

		if (listener && send_onscroll)
			listener->OnScroll(this, old_value, value, caused_by_input); // Invoke!

		Invalidate(GetBounds());
	}
}

BOOL OpScrollbar::OnSmoothScroll(INT32 new_value)
{
	int old_value = value;
	value = new_value;

	if (listener)
		listener->OnScroll(this, old_value, value, TRUE); // Invoke!

	Invalidate(GetBounds());

	Sync();

	return value != old_value;
}

BOOL OpScrollbar::CanScroll()
{
	INT32 limit_total = (limit_max - limit_min) + limit_visible;
	INT32 limit_delta = (limit_max - limit_min);
	if (limit_total <= 0 || limit_delta <= 0 || IsEnabled() == FALSE)
		return FALSE;
	return TRUE;
}

BOOL OpScrollbar::CanScroll(int direction)
{
	switch (direction)
	{
		case ARROW_LEFT:
		case ARROW_UP:
			if (value == limit_min)
				return FALSE;
			break;
		case ARROW_RIGHT:
		case ARROW_DOWN:
			if (value == limit_max)
				return FALSE;
			break;
	}
	return TRUE;
}

void OpScrollbar::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this, GetType(), w, h, cols, rows);
}

void OpScrollbar::OnBeforePaint()
{
#ifdef CSS_SCROLLBARS_SUPPORT
	const uni_char* host = NULL;
	if (vis_dev && vis_dev->GetDocumentManager()
		&& vis_dev->GetDocumentManager()->GetCurrentDoc()
		)
	{
		host = vis_dev->GetDocumentManager()->GetCurrentDoc()->GetHostName();
	}

	scrollbar_colors.SetEnabled(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableScrollbarColors, host));
#endif
}

void OpScrollbar::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if(widget_painter->DrawScrollbar(paint_rect))
		return;

#ifndef ACCESSIBILITY_EXTENSION_SUPPORT
	OpRect button_scrollbk;
	OpRect button_scrollfw;
	OpRect button_pgbk;
	OpRect button_pgfw;
#endif

	int button1 = GetInfo()->GetScrollbarFirstButtonSize();
	int button2 = GetInfo()->GetScrollbarSecondButtonSize();

	int tmplen = horizontal ? rect.width : rect.height;
	if (button1 + button2 > tmplen)
	{
		if(button1 && button2)
		{
			button1 = tmplen / 2;
			button2 = button1;
		}
		else if(button1)
		{
			button1 = tmplen;
		}
		else
		{
			button2 = tmplen;
		}
	}
	tmplen -= button1 + button2 - 1;

	float limit_total = (float) (limit_max - limit_min) + limit_visible;
	float limit_delta = (float) (limit_max - limit_min);
	float a = float(limit_visible);
	float b = float(limit_total);
	float scaled_knobsize = 0;
	if(b <= 0)
		scaled_knobsize = (float) tmplen;
	else
		scaled_knobsize = (a / b) * float(tmplen);
	int min_knob_len = GetKnobMinLength();
	if (scaled_knobsize < (float) min_knob_len)
		scaled_knobsize = (float) min_knob_len;

	int tmppos;
	if(limit_delta <= 0)
	{
		tmppos = 0;
		scaled_knobsize = (float) tmplen - 1;
	}
	else
		tmppos = (int)(float(float(tmplen) - scaled_knobsize) * float(float(value - limit_min) / limit_delta));
	tmppos += button1;

	BOOL is_scrolling_up = FALSE;
	BOOL is_scrolling_down = FALSE;
	if (IsTimerRunning() && !is_scrolling_fast && CanScroll())
	{
		is_scrolling_up = !this->is_scrolling_down;
		is_scrolling_down = this->is_scrolling_down;
	}

	BOOL to_small = scaled_knobsize > tmplen; // (if the scrollbar is to small for the moving knob)

	if (horizontal)
	{
		int horizontal_height = GetInfo()->GetHorizontalScrollbarHeight();
		if(button1)
		{
			switch(GetInfo()->GetScrollbarArrowStyle())
			{
				case ARROWS_NORMAL:
					button_scrollbk = OpRect(0, 0, button1, horizontal_height);
					widget_painter->DrawScrollbarDirection(button_scrollbk, ARROW_LEFT, is_scrolling_up, hover_part == LEFT_OUTSIDE_ARROW);
					break;
				case ARROWS_AT_TOP:
				case ARROWS_AT_BOTTOM_AND_TOP:
					{
						int buttonDual = button1/2;
						button_scrollbk = OpRect(0, 0, buttonDual, horizontal_height);
						button_scrollfw = OpRect(buttonDual, 0, buttonDual, horizontal_height);
						widget_painter->DrawScrollbarDirection(button_scrollbk, ARROW_LEFT, is_scrolling_up && (hit_part == LEFT_OUTSIDE_ARROW), hover_part == LEFT_OUTSIDE_ARROW);
						widget_painter->DrawScrollbarDirection(button_scrollfw, ARROW_RIGHT, is_scrolling_down && (hit_part == LEFT_INSIDE_ARROW), hover_part == LEFT_INSIDE_ARROW);
					}
					break;
				default:
					break;
			}
		}	
		// First part of background
		button_pgbk = OpRect(button1, 0, tmppos - button1, GetBounds().height);
		widget_painter->DrawScrollbarBackground(button_pgbk);

		SetKnobRect(OpRect((int)tmppos, 0, (int)scaled_knobsize, (int)horizontal_height));
	}
	else
	{
		int vertical_width = GetInfo()->GetVerticalScrollbarWidth();
		if(button1)
		{
			switch(GetInfo()->GetScrollbarArrowStyle())
			{
				case ARROWS_NORMAL:
					button_scrollbk = OpRect(0, 0, vertical_width, button1);
					widget_painter->DrawScrollbarDirection(button_scrollbk, ARROW_UP, is_scrolling_up, hover_part == TOP_OUTSIDE_ARROW);
					break;
				case ARROWS_AT_TOP:
				case ARROWS_AT_BOTTOM_AND_TOP:
					{
						int buttonDual = button1/2;
						button_scrollbk = OpRect(0, 0, vertical_width, buttonDual);
						button_scrollfw = OpRect(0, buttonDual, vertical_width, buttonDual);
						widget_painter->DrawScrollbarDirection(button_scrollbk, ARROW_UP, is_scrolling_up && (hit_part == TOP_OUTSIDE_ARROW), hover_part == TOP_OUTSIDE_ARROW);
						widget_painter->DrawScrollbarDirection(button_scrollfw, ARROW_DOWN, is_scrolling_down && (hit_part == TOP_INSIDE_ARROW), hover_part == TOP_INSIDE_ARROW);
					}
					break;
				default:
					break;
			}
		}	

		// First part of background
		button_pgbk = OpRect(0, button1, GetBounds().width, tmppos - button1);
		if (!to_small)
			widget_painter->DrawScrollbarBackground(button_pgbk);

		SetKnobRect(OpRect(0, (int)tmppos, (int)vertical_width, (int)scaled_knobsize));
	}
	// Draw the knob
	if (!to_small)
		widget_painter->DrawScrollbarKnob(knob_rect, hit_part == KNOB, hover_part == KNOB);

	// Second part of background
	if (horizontal)
	{
		int x = knob_rect.x + knob_rect.width;
		button_pgfw = OpRect(x, 0, GetBounds().width - button2 - x, GetBounds().height);
	}
	else
	{
		int y = knob_rect.y + knob_rect.height;
		button_pgfw = OpRect(0, y, GetBounds().width, GetBounds().height - button2 - y);
	}
	widget_painter->DrawScrollbarBackground(button_pgfw);

	if(button2)
	{
		OpRect tmprect;
		if (horizontal)
			tmprect.Set(rect.width - button2, 0, button2, GetInfo()->GetHorizontalScrollbarHeight());
		else
			tmprect.Set(0, rect.height - button2, GetInfo()->GetVerticalScrollbarWidth(), button2);
	
		switch(GetInfo()->GetScrollbarArrowStyle())
		{
			case ARROWS_NORMAL:
				button_scrollfw = tmprect;
				widget_painter->DrawScrollbarDirection(tmprect,  horizontal ? ARROW_RIGHT : ARROW_DOWN, is_scrolling_down, hover_part == BOTTOM_OUTSIDE_ARROW);
				break;
			case ARROWS_AT_BOTTOM:
			case ARROWS_AT_BOTTOM_AND_TOP:
				{
					OpRect tmprect2 = tmprect;
					if (horizontal)
					{
						tmprect2.x += button2 / 2;
						tmprect2.width /= 2;
						tmprect.width = tmprect2.width;
					}
					else
					{
						tmprect2.y += button2 / 2;
						tmprect2.height /= 2;
						tmprect.height = tmprect2.height;
					}
					if (GetInfo()->GetScrollbarArrowStyle() == ARROWS_AT_BOTTOM)
					{
						button_scrollbk = tmprect;
						button_scrollfw = tmprect;
					}
					widget_painter->DrawScrollbarDirection(tmprect,  horizontal ? ARROW_LEFT : ARROW_UP, is_scrolling_up && (hit_part == BOTTOM_INSIDE_ARROW), hover_part == BOTTOM_INSIDE_ARROW);
					widget_painter->DrawScrollbarDirection(tmprect2,  horizontal ? ARROW_RIGHT : ARROW_DOWN, is_scrolling_down && (hit_part == BOTTOM_OUTSIDE_ARROW), hover_part == BOTTOM_OUTSIDE_ARROW);
				}
				break;
			default:
				break;
		}
	}	
}

void OpScrollbar::OnMouseLeave()
{
	if (hover_part != INVALID_PART)
	{
		hover_part = INVALID_PART;
		Invalidate(GetBounds());
	}
}

void OpScrollbar::OnMouseMove(const OpPoint &point)
{
	SCROLLBAR_PART_CODE part;

	GetScrollbarPartByPoint(point, part);

	if (part != hover_part)
	{
		hover_part = part;
		Invalidate(GetBounds());
	}

#ifndef MOUSELESS
	if (vis_dev && !vis_dev->GetView()->GetMouseButton(MOUSE_BUTTON_1))
		is_moving = FALSE;
#endif // MOUSELESS

	if(is_moving)
	{
		// Check if we drop the knob. (moving to far away). 
		// This is something we do not really want on UNIX. It is 
		// windows behavior to stop scrolling [espen 2003-01-21]
#ifdef WIDGETS_SCROLLBAR_KNOB_SNAP
		if (!GetBounds().Contains(point))
		{
			INT32 how_far_away_x = MAX(-point.x, point.x - rect.width);
			INT32 how_far_away_y = MAX(-point.y, point.y - rect.height);
			if (how_far_away_x < 0)
				how_far_away_x = 0;
			if (how_far_away_y < 0)
				how_far_away_y = 0;
			if (how_far_away_x > GetInfo()->GetDropScrollbarLength() ||
				how_far_away_y > GetInfo()->GetDropScrollbarLength())
			{
				SetValue(value_on_start_move, TRUE);
				return;
			}
		}
#endif

		int button1 = GetInfo()->GetScrollbarFirstButtonSize();
		int button2 = GetInfo()->GetScrollbarSecondButtonSize();

		int w = rect.width;
		int h = rect.height;
		float len = horizontal ? (float) w : (float) h;
		if (button1 + button2 > len)
		{
			button1 = (int)(len / 2);
			button2 = button1;
		}
		len -= button1 + button2;
		INT32 knobsize = horizontal ? knob_rect.width : knob_rect.height;
		float prop = float(limit_max - limit_min) / float(len - knobsize);
		float box_len = prop * len;
		INT32 mousepos = horizontal ? point.x : point.y;
		len -= box_len;

		int32 newv = (int32)(((mousepos + moveofs) - button1) * prop) + limit_min;
		int dy = newv - GetValue();
		SetValue(newv, TRUE);
		PrepareAutoScroll(dy);
	}
}

void OpScrollbar::GetScrollbarPartByPoint(const OpPoint &point, SCROLLBAR_PART_CODE& part)
{
	part = INVALID_PART;

	if (!GetBounds().Contains(point))
		return;


	if(GetInfo()->GetScrollbarPartHitBy(this, point, part))
		return;


	// Find out where we clicked
	int len = horizontal ? rect.width : rect.height;
	int mousepos = horizontal ? point.x : point.y;
	int button1 = GetInfo()->GetScrollbarFirstButtonSize();
	int button2 = GetInfo()->GetScrollbarSecondButtonSize();
	if (button1 + button2 > len)
	{
		if(button1 && button2)
		{
			button1 = len / 2;
			button2 = button1;
		}
		else if(button1)
		{
			button1 = len;
		}
		else
		{
			button2 = len;
		}
	}

	if (mousepos < button1 || mousepos > len - button2) // ArrowButton pressed
	{
		// Find out which button we clicked
		BOOL first_button = (mousepos < button1);
		BOOL first_half;
		INT32 pos;
		if (first_button)
			pos = mousepos;
		else
			pos = mousepos - (len - button2);
		
		first_half = first_button ? (pos < (button1/2)) : (pos < (button2/2));
		
		switch(GetInfo()->GetScrollbarArrowStyle())
		{
			case ARROWS_NORMAL:
				part = first_button ? TOP_OUTSIDE_ARROW : BOTTOM_OUTSIDE_ARROW;
				break;
			case ARROWS_AT_BOTTOM:
				if(first_button)
				{
					part = INVALID_PART;
				}
				else
				{
					part = first_half ? BOTTOM_INSIDE_ARROW : BOTTOM_OUTSIDE_ARROW;
				}
				break;
			case ARROWS_AT_BOTTOM_AND_TOP:
				if(first_button)
				{
					part = first_half ? TOP_OUTSIDE_ARROW : TOP_INSIDE_ARROW;
				}
				else
				{
					part = first_half ? BOTTOM_INSIDE_ARROW : BOTTOM_OUTSIDE_ARROW;
				}
				break;
			case ARROWS_AT_TOP:
				if(first_button)
				{
					part = first_half ? TOP_OUTSIDE_ARROW : TOP_INSIDE_ARROW;
				}
				else
				{
					part = INVALID_PART;
				}
				break;
		}

	}
	else
	{
		OpPoint p = horizontal ? OpPoint(point.x, 1) : OpPoint(1, point.y);

		if (knob_rect.Contains(p))
		{
			part = KNOB;
		}
		else // Big step
		{
			int knobpos = horizontal ? knob_rect.x : knob_rect.y;
			if (mousepos <= knobpos)
				part = TOP_TRACK;
			else
				part = BOTTOM_TRACK;
		}
	}
}

void OpScrollbar::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
#if defined _X11_
	if (button == MOUSE_BUTTON_2)
	    return;
#else
	if (button != MOUSE_BUTTON_1)
		return;
#endif

	PrepareAutoScroll(0);
	GetScrollbarPartByPoint(point, hit_part);
	
	if(hit_part != INVALID_PART)
	{
		int newv = value;
		BOOL arrow_hit = FALSE;
		int mousepos = horizontal ? point.x : point.y;
#if defined _X11_
		int len = horizontal ? rect.width : rect.height;
		int button1 = GetInfo()->GetScrollbarFirstButtonSize();
		int button2 = GetInfo()->GetScrollbarSecondButtonSize();
		float limit_delta = (limit_max - limit_min);
		float vd = len - button1 - button2 - (horizontal ? knob_rect.width : knob_rect.height);
		int dv = 0;
		int knobpos = horizontal ? knob_rect.x : knob_rect.y;
#endif
		
		value_on_start_move = value;
		
		switch(hit_part)
		{
			case TOP_OUTSIDE_ARROW:
			case BOTTOM_INSIDE_ARROW:
				newv -= small_step;
				is_scrolling_down = FALSE;
				arrow_hit = TRUE;
				break;
			case TOP_INSIDE_ARROW:
			case BOTTOM_OUTSIDE_ARROW:
				newv += small_step;
				is_scrolling_down = TRUE;
				arrow_hit = TRUE;
				break;
			case TOP_TRACK:
#if defined _X11_
				if (button == MOUSE_BUTTON_3)
				{					
					dv = (int) ((float)(knobpos - mousepos)/vd*limit_delta);
					newv -= dv;
				}
				else
#endif
				newv -= big_step;
				is_scrolling_down = FALSE;
				break;
			case BOTTOM_TRACK:
#if defined _X11_
				if (button == MOUSE_BUTTON_3)
				{
					dv = (int) ((float)(mousepos - knobpos)/vd*limit_delta);
					newv += dv;
				}
				else
#endif
				newv += big_step;
				is_scrolling_down = TRUE;
				break;
			case KNOB:
				break;
		}
		
		if(arrow_hit)
		{
			SetValue(newv, TRUE);

			Invalidate(GetBounds());
			
			// Start the scroll timer
			is_scrolling_fast = FALSE;
			StartTimer(GetInfo()->GetScrollDelay(FALSE, TRUE));
		}
		else if(hit_part == KNOB)
		{
			int rectpos = horizontal ? knob_rect.x : knob_rect.y;
			moveofs = - (mousepos - rectpos);
			is_moving = TRUE;
			Invalidate(GetBounds());
		} 
		else
		{	
			SetValue(newv, TRUE);
			if (button == MOUSE_BUTTON_1)
			{
				is_scrolling_fast = TRUE;
				StartTimer(GetInfo()->GetScrollDelay(TRUE, TRUE));
			}
		}
	}
}

void OpScrollbar::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	is_moving = FALSE;
	hit_part = INVALID_PART;
	StopTimer();
	Invalidate(GetBounds());
	AutoScroll(GetValue());
}

BOOL OpScrollbar::OnScroll(INT32 delta, BOOL vertical, BOOL smooth/* = TRUE*/)
{
	int old_value = value;
	int from_val = (smooth && IsSmoothScrollRunning()) ?  GetSmoothScrollTargetValue() : GetValue();
	SetValue(from_val + delta, TRUE, TRUE, smooth);
	return value != old_value;
}

BOOL OpScrollbar::OnMouseWheel(INT32 delta, BOOL vertical)
{
	// if the widget's scrollbar can't scroll, don't try to scroll it
	if (small_step == 0)
		return FALSE;

	int scroll_by = op_abs(DISPLAY_SCROLLWHEEL_STEPSIZE * delta);
	int scroll_limit = vertical ? GetParent()->GetBounds().height : GetParent()->GetBounds().width;

	if (scroll_by > scroll_limit)
		/* If the scroll amount exceeds dimensions of the parent widget, limit
		   to the full number of steps or just a single step, depending on
		   which is bigger. */
		scroll_by = MAX(small_step, scroll_limit - (scroll_limit % small_step));
	else if (scroll_by > small_step)
		/* Round down scroll amount to the full number of steps. In case of list
		   boxes this will ensure that full number of items are scrolled. */
		scroll_by -= scroll_by % small_step;

	return OnScroll(delta > 0 ? scroll_by : -scroll_by, vertical, TRUE);
}

void OpScrollbar::OnTimer()
{
	if (GetTimerDelay() == GetInfo()->GetScrollDelay(is_scrolling_fast, TRUE))
	{
		StopTimer();
		StartTimer(GetInfo()->GetScrollDelay(is_scrolling_fast, FALSE));
	}

	SCROLLBAR_PART_CODE part;
	GetScrollbarPartByPoint(GetMousePos(), part);

#ifdef _MACINTOSH_
	// This code allows the scrolling to switch directions without lifting the mouse button
	// This behavior is required on Mac
    switch (hit_part) {
    	case TOP_OUTSIDE_ARROW:
    	case BOTTOM_INSIDE_ARROW:
          	if (part == TOP_INSIDE_ARROW || part == BOTTOM_OUTSIDE_ARROW) {
    	     	// We are over the opposite arrow so switch direction and update the hit_part
    	     	is_scrolling_down = TRUE;
          	    hit_part = part;
          	}
          	else if (part == TOP_OUTSIDE_ARROW || part == BOTTOM_INSIDE_ARROW) {
 				// Flow through and scroll the same way
          	}
          	else {
          		// We are over something other than an arrow so do nothing
          		return;
          	}
    	break;

    	case TOP_INSIDE_ARROW:
    	case BOTTOM_OUTSIDE_ARROW:
          	if (part == TOP_OUTSIDE_ARROW || part == BOTTOM_INSIDE_ARROW) {
    	     	// We are over the opposite arrow so switch direction and update the hit_part
    	     	is_scrolling_down = FALSE;
          	    hit_part = part;
          	}
          	else if (part == TOP_INSIDE_ARROW || part == BOTTOM_OUTSIDE_ARROW) {
 				// Flow through and scroll the same way
          	}
          	else {
          		// We are over something other than an arrow so do nothing
          		return;
          	}
  		break;
    
		default:
       		// We are over something other than an arrow so do nothing
			if (part != hit_part)
				return;
		break;
    }
#else
	if (part != hit_part)
		return;
#endif

	int newv = value;
	if (is_scrolling_fast)
		newv += (is_scrolling_down ? big_step : -big_step);
	else
		newv += (is_scrolling_down ? small_step : -small_step);
	SetValue(newv, TRUE);
}

int OpScrollbar::GetKnobMinLength()
{
#ifdef SKIN_SUPPORT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(horizontal ? "Scrollbar Horizontal Knob Skin"
																		: "Scrollbar Vertical Knob Skin");
	if (skin_elm)
	{
		INT32 w = 0, h = 0;
		skin_elm->GetMinSize(&w, &h, 0);
		int length = horizontal ? w : h;
		if (length > WIDGETS_SCROLLBAR_KNOB_MIN_LENGTH)
			return length;
	}
#endif
	return (int) WIDGETS_SCROLLBAR_KNOB_MIN_LENGTH;
}

void OpScrollbar::SetKnobRect(const OpRect &knobRect)
{
	if((knobRect.width > 0) && (knobRect.height > 0))
	{
		knob_rect = knobRect;
	}
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
Accessibility::ElementKind OpScrollbar::AccessibilityGetRole() const
{
	return horizontal ? Accessibility::kElementKindHorizontalScrollbar : Accessibility::kElementKindVerticalScrollbar;
}

OP_STATUS OpScrollbar::GetAbsolutePositionOfObject(int i, OpRect& rect)
{
	switch (i)
	{
		case AccessibleScrollbarPart::kPartArrowUp:
			rect = button_scrollbk;
			break;
		case AccessibleScrollbarPart::kPartArrowDown:
			rect = button_scrollfw;
			break;
		case AccessibleScrollbarPart::kPartPageUp:
			rect = button_pgbk;
			break;
		case AccessibleScrollbarPart::kPartPageDown:
			rect = button_pgfw;
			break;
		case AccessibleScrollbarPart::kPartKnob:
			rect = knob_rect;
			break;
		default:
			return OpStatus::ERR;
	}
	OpRect offset = GetScreenRect();
	rect.OffsetBy(offset.x, offset.y);

	return OpStatus::OK;
}

Accessibility::State OpScrollbar::GetStateOfObject(int i)
{
	return AccessibilityGetState();
}

OP_STATUS OpScrollbar::ObjectClicked(int i)
{
	int newv = value;
	switch (i)
	{
		case AccessibleScrollbarPart::kPartArrowUp:
			newv -= small_step;
			break;
		case AccessibleScrollbarPart::kPartArrowDown:
			newv += small_step;
			break;
		case AccessibleScrollbarPart::kPartPageUp:
			newv -= big_step;
			break;
		case AccessibleScrollbarPart::kPartPageDown:
			newv += big_step;
			break;
	}
	if (newv != value)
		SetValue(newv, TRUE);
	return OpStatus::OK;
}

int OpScrollbar::GetAccessibleChildrenCount()
{
	return 5;
}

OpAccessibleItem* OpScrollbar::GetAccessibleChild(int i)
{
	AccessibleScrollbarPart::PartCode part_code = (AccessibleScrollbarPart::PartCode) i;
	AccessibleScrollbarPart** part = NULL;
	switch (part_code)
	{
		case AccessibleScrollbarPart::kPartArrowUp:
			part = &m_accessible_arrow_up_part;
			break;
		case AccessibleScrollbarPart::kPartArrowDown:
			part = &m_accessible_arrow_down_part;
			break;
		case AccessibleScrollbarPart::kPartPageUp:
			part = &m_accessible_page_up_part;
			break;
		case AccessibleScrollbarPart::kPartPageDown:
			part = &m_accessible_page_down_part;
			break;
		case AccessibleScrollbarPart::kPartKnob:
			part = &m_accessible_knob_part;
			break;
		default:
			return NULL;
	}
	if (!*part)
		*part = OP_NEW(AccessibleScrollbarPart, (this, part_code));

	if (*part)
		return *part;

	return NULL;
}

int OpScrollbar::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	for (int i = 0; i < 5; i++)
	{
		if (GetAccessibleChild(i) == child)
			return i;
	}
	return Accessibility::NoSuchChild;
}

OpAccessibleItem* OpScrollbar::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect widget_rect = GetScreenRect();
	OpPoint point(x-widget_rect.x,y-widget_rect.y);
	SCROLLBAR_PART_CODE scrollbar_part;
	GetScrollbarPartByPoint(point, scrollbar_part);
	AccessibleScrollbarPart::PartCode part_code;
	AccessibleScrollbarPart** part = NULL;
	switch (scrollbar_part)
	{
		case TOP_OUTSIDE_ARROW:
		case BOTTOM_INSIDE_ARROW:
			part_code = AccessibleScrollbarPart::kPartArrowUp;
			part = &m_accessible_arrow_up_part;
			break;
		case TOP_INSIDE_ARROW:
		case BOTTOM_OUTSIDE_ARROW:
			part_code = AccessibleScrollbarPart::kPartArrowDown;
			part = &m_accessible_arrow_down_part;
			break;
		case TOP_TRACK:
			part_code = AccessibleScrollbarPart::kPartPageUp;
			part = &m_accessible_page_up_part;
			break;
		case BOTTOM_TRACK:
			part_code = AccessibleScrollbarPart::kPartPageDown;
			part = &m_accessible_page_down_part;
			break;
		case KNOB:
			part_code = AccessibleScrollbarPart::kPartKnob;
			part = &m_accessible_knob_part;
			break;
		default:
			return OpWidget::GetAccessibleChildOrSelfAt(x, y);
	}
	if (!*part)
		*part = OP_NEW(AccessibleScrollbarPart, (this, part_code));

	if (*part)
		return *part;

	return OpWidget::GetAccessibleChildOrSelfAt(x, y);
}

void OpScrollbar::OnDeleted()
{
	OP_DELETE(m_accessible_arrow_up_part);
	m_accessible_arrow_up_part = NULL;
	OP_DELETE(m_accessible_arrow_down_part);
	m_accessible_arrow_down_part = NULL;
	OP_DELETE(m_accessible_page_up_part);
	m_accessible_page_up_part = NULL;
	OP_DELETE(m_accessible_page_down_part);
	m_accessible_page_down_part = NULL;
	OP_DELETE(m_accessible_knob_part);
	m_accessible_knob_part = NULL;
	OpWidget::OnDeleted();
}
#endif

