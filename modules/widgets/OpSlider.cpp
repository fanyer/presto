/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpSlider.h"
#include "modules/forms/webforms2number.h"
#include "modules/display/vis_dev.h"
#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif // SKIN_SUPPORT


// == OpSlider ===========================================================

DEFINE_CONSTRUCT(OpSlider)

OpSlider::OpSlider() :
	m_min(0),
	m_max(100),
	m_anchor(50),
	m_current(50),
	m_step(1),
	m_step_base(0),
	m_read_only(FALSE),
	m_drag_in_progress(FALSE),
	m_is_hovering_knob(FALSE)
	, m_tick_values(NULL)
	, m_num_tick_values(0)
	, m_show_labels(FALSE)
{
}

OpSlider::~OpSlider()
{
	OP_DELETEA(m_tick_values);
}

/**
 * This is called to refill the widget after reflow or history movement.
 */
OP_STATUS OpSlider::SetText(const uni_char* text)
{
	if (text && *text)
	{
		uni_char* end_ptr;
		double val = uni_strtod(text, &end_ptr);
		// Check for NaN and Infinity
		if (*end_ptr || op_isinf(val) || op_isnan(val))
		{
			return OpStatus::ERR;
		}
		SetValue(val, TRUE);
	}

	return OpStatus::OK;
}

void OpSlider::SetValue(double val, BOOL force_no_onchange)
{
	val = MIN(val, m_max);
	val = MAX(val, m_min);
	if (val != m_current)
	{
		m_current = val;
		InvalidateAll();
		if (!force_no_onchange)
			HandleOnChange();
	}
}

/* virtual */
void OpSlider::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	*w = 160;
	*h = 20;
	if (m_show_labels)
		*h += 20; // FIX: Check font height
	return;
}

OP_STATUS OpSlider::GetText(OpString &str)
{
	OP_STATUS status;
	uni_char* value_buf = str.Reserve(33);
	if (!value_buf)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	status = WebForms2Number::DoubleToString(GetNumberValue(), value_buf);
	return status;
}

void OpSlider::SetTickValues(int num_ticks, const TICK_VALUE *tick_values, double snap_threshold)
{
	if (m_tick_values && num_ticks == m_num_tick_values)
	{
		// Check if it's changed or not.
		int i;
		for(i = 0; i < num_ticks; i++)
			if (tick_values[i].snap != m_tick_values[i].snap ||
				tick_values[i].value != m_tick_values[i].value)
				break;
		if (i == num_ticks)
			// There's no change, so just return
			return;
	}
	OP_DELETEA(m_tick_values);
	m_num_tick_values = 0;
	m_tick_values = OP_NEWA(TICK_VALUE, num_ticks);
	if (m_tick_values)
	{
		op_memcpy(m_tick_values, tick_values, num_ticks * sizeof(TICK_VALUE));
		m_num_tick_values = num_ticks;
	}
	InvalidateAll();
}

void OpSlider::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpRect rect = GetBounds();
	BOOL horizontal = IsHorizontal();
	BOOL highlighted = IsFocused() && HasFocusRect();
	VisualDevice *vd = GetVisualDevice();

	if (m_show_labels && horizontal)
		rect.height -= 20; // FIX: Check font height

	if (widget_painter->DrawSlider(rect, horizontal, m_min, m_max, GetNumberValue(), highlighted, m_drag_in_progress, m_knob_position, m_start_track, m_end_track))
		return;

	// Slider track

	//OpRect inner(rect.x+5, rect.y+5, rect.width-10, rect.height-10);
	int height_margin = 2 + rect.height/50;
	int width_margin = 2 + rect.width/50;
	OpRect inner = rect.InsetBy(width_margin, height_margin);

	const int track_thickness = GetTrackThickness();

	int track_len;
	int slider_thickness;
	if (horizontal)
	{
		track_len = inner.width;
		slider_thickness = inner.height;
	}
	else
	{
		track_len = inner.height;
		slider_thickness = inner.width;
	}

	UpdateTrackPoints(inner);
	OpRect track(m_start_track.x, m_start_track.y, m_end_track.x - m_start_track.x, m_end_track.y - m_start_track.y);
	widget_painter->DrawSliderTrack(rect, track);

	// Draw ticks
	UINT32 fgcol;
	if (IsEnabled())
		fgcol = OP_RGBA(0, 0, 0, 100);
	else
		fgcol = OP_RGBA(0, 0, 0, 50);
	vd->SetColor32(fgcol);

	double min_val = m_min;
	double max_val = m_max;
	double step = GetStep();
	double val_range = max_val - min_val;
	if (step <= 0.0)
	{
		step = val_range / 10;
	}

	// tick_len is in [2, 8 + track_len/40]
	const int max_tick_len = 8 + track_len/40;
	int tick_len = MIN(max_tick_len, MAX(2, (slider_thickness - track_thickness) / 2 - 4));

	if (m_num_tick_values && m_tick_values)
	{
		int large_tick_len = tick_len * 2;
		if (max_val - min_val)
			for(int i = 0; i < m_num_tick_values; i++)
			{
				int x, y;
				int tick_offset = (int)((m_tick_values[i].value - min_val) * (track_len - 1) / (max_val - min_val));
				if (horizontal)
				{
					if (GetRTL())
						x = track.Right() - (tick_offset + 1);
					else
						x = track.x + tick_offset;
					y = track.y + track.height + 1;
				}
				else
				{
					x = track.x + track.width + 1;
					y = track.y + tick_offset;
				}
				vd->DrawLine(OpPoint(x, y), m_tick_values[i].snap ? large_tick_len : tick_len, !horizontal, 1);

				if (m_show_labels && horizontal)
				{
					OpWidgetString tmp;
					uni_char buf[10]; /* ARRAY OK 2009-07-01 emil */
					uni_snprintf(buf, 10, UNI_L("%d%%"), (int)m_tick_values[i].value);
					tmp.Set(buf, this);
					x = x - tmp.GetWidth() / 2;
					x = MAX(x, 0);
					x = MIN(x, GetBounds().width - tmp.GetWidth());
					tmp.Draw(OpRect(x, y + large_tick_len, tmp.GetWidth(), tmp.GetHeight()), vd, fgcol);
				}
			}
	}
	else if (step > 0.0 && val_range > 0.0)
	{
		int base = 16;
		if (int(val_range / 10) * 10 == val_range)
			base = 10;

		int num_ticks = (int)(val_range / step);
		if (num_ticks > base)
		{
			double test_step = int(val_range / base);
			test_step = int(test_step / step) * step;
			while (num_ticks > base)
			{
				num_ticks = (int)(val_range / test_step);
				test_step += step;
			}
		}

		int avail_track_len = (int) ((double)(int(val_range / step) * step) / val_range * (track_len - 1));
		for(int i = 0; i <= num_ticks; i++)
		{
			int x;
			int y;
			int tick_offset = (int) (avail_track_len / (double)num_ticks * i);
			if (horizontal)
			{
				x = track.x + tick_offset;
				y = track.y + track.height + 1;
			}
			else
			{
				x = track.x + track.width + 1;
				y = track.y + tick_offset;
			}

			vd->DrawLine(OpPoint(x, y), tick_len, !horizontal, 1);
		}
	} // end if (step > 0 && max > min)

	// Draw numbers
#if 0 // Don't draw numbers. The slider might be used for something that's not arithmetic
	max_val = MAX(min_val, max_val);

	OpRect min_text_rect;
	OpRect max_text_rect;

	if (horizontal)
	{
		int text_width = track_len/3;
		int text_height = inner.height*30/100;
		min_text_rect = OpRect(track.x-text_width/2, track.y + track.height, text_width, text_height);
		max_text_rect = OpRect(track.x+track.width-text_width/2, track.y + track.height - text_height, text_width, text_height);
	}
	else
	{
		int text_width = inner.width/2 - track_thickness;
		int text_height = track_len/15;
		min_text_rect = OpRect(track.x+2*track_thickness, track.y-text_height/2, text_width, text_height);
		max_text_rect = OpRect(track.x+2*track_thickness, track.y+track.height-text_height/2, text_width, text_height);
	}

	font_info.justify = horizontal ? JUSTIFY_LEFT : JUSTIFY_RIGHT;
	GetMinString().Draw(min_text_rect, vd, fgcol, -1);
	font_info.justify = JUSTIFY_RIGHT;
	GetMaxString().Draw(max_text_rect, vd, fgcol, -1);
#endif // 0

	// Draw focus border
	if (highlighted)
	{
		widget_painter->DrawSliderFocus(rect, track, inner);
	}

	UpdateKnobRect(inner);
	widget_painter->DrawSliderKnob(rect, track, m_knob_position);
}

void OpSlider::UpdateTrackPoints(const OpRect& inner)
{
	// XXX Negative numbers
	const int track_thickness = GetTrackThickness();
	if (IsHorizontal())
	{
		m_start_track = OpPoint(inner.x, inner.y+inner.height/2-track_thickness/2);
		m_end_track = OpPoint(m_start_track.x + inner.width, m_start_track.y + track_thickness);
	}
	else
	{
		m_start_track = OpPoint(inner.x+inner.width/2-track_thickness/2, inner.y);
		m_end_track = OpPoint(m_start_track.x + track_thickness, m_start_track.y + inner.height);
	}
}

void OpSlider::UpdateKnobRect(const OpRect& inner)
{
	double relpos;
	if (m_max == m_min)
	{
		relpos = 0.0;
	}
	else
	{
		// 0.0 <= relpos <= 1.0
		relpos = (GetNumberValue()-m_min)/(m_max-m_min);
		relpos = MIN(1.0, MAX(0.0, relpos));
	}

	const BOOL horizontal = IsHorizontal();
	if (!horizontal || GetRTL()) // Bottom is 0, so is Right when in RTL
		relpos = 1.0 - relpos;

	OpRect rect = GetBounds();
	int knob_width, knob_height;
	BOOL valid_size = GetKnobSize(knob_width, knob_height);
	if (horizontal)
	{
		if (!valid_size)
			knob_width = 6 + 2*inner.width/100;
		int middlex = int(inner.x+relpos*inner.width+0.5);
		if (GetRTL())
			middlex += 1;
		m_knob_position = OpRect(middlex-knob_width/2, inner.y, knob_width, inner.height);

		// Ensure that knob is never clipped. Issue for larger than default knobs
		if (m_knob_position.width > rect.width)
			m_knob_position.width = rect.width;
		if (m_knob_position.Right() > rect.Right())
			m_knob_position.x = rect.Right() - m_knob_position.width;
		if (m_knob_position.Left() < rect.Left())
			m_knob_position.x = rect.x;
	}
	else
	{
		if (!valid_size)
			knob_height = 6 + 2*inner.height/100;
		int middley = int(inner.y+relpos*inner.height+0.5);
		m_knob_position = OpRect(inner.x, middley-knob_height/2, inner.width, knob_height);

		// Ensure that knob is never clipped. Issue for larger than default knobs
		if (m_knob_position.height > rect.height)
			m_knob_position.height = rect.height;
		if (m_knob_position.Bottom() > rect.Bottom())
			m_knob_position.y = rect.Bottom() - m_knob_position.height;
		if (m_knob_position.Top() < rect.Top())
			m_knob_position.y = rect.y;
	}
}

int OpSlider::GetTrackThickness()
{
#ifdef SKIN_SUPPORT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(GetTrackSkin(IsHorizontal()));

	if (skin_elm)
	{
		INT32 w = 0, h = 0;
		skin_elm->GetSize(&w, &h, 0);
		int thickness = IsHorizontal() ? h : w;
		if (thickness > 0)
			return thickness;
	}
#endif
	return 4;
}

BOOL OpSlider::GetKnobSize(INT32& w, INT32& h)
{
#ifdef SKIN_SUPPORT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(GetKnobSkin(IsHorizontal()));

	if (skin_elm)
	{
		w = h = 0;
		skin_elm->GetSize(&w, &h, 0);
		return w > 0 && h > 0;
	}
#endif
	return FALSE;
}

void OpSlider::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener)
	{
#ifndef MOUSELESS
		listener->OnMouseEvent(this, -1, point.x, point.y, button, TRUE, nclicks);
#endif // MOUSELESS
	}

	if (!IsEnabled() || IsDead())
	{
		return;
	}

	// Clicking in it -> Give us focus
	SetFocus(FOCUS_REASON_MOUSE);

	if (button != MOUSE_BUTTON_1)
	{
		return;
	}

	if (IsReadOnly())
	{
		return;
	}

	if (IsPointInKnob(point))
	{
		m_drag_in_progress = TRUE;
		m_drag_value = m_current;
		m_anchor = m_current; // Anchor shall only be set on mouse down
		m_start_drag_pos = point;
		Invalidate(GetBounds());
		return;
	}

	if (GetBounds().Contains(point))
	{
		SetToClosestSnapPoint(point);
	}
}

BOOL OpSlider::OnMouseWheel(INT32 delta,BOOL vertical)
{
	if (IsEnabled() && !IsDead())
	{
		delta = -delta;
		SetValue((double)(GetNumberValue() + (delta > 0 ? 1 : -1) * m_step), FALSE);
		return TRUE;
	}
	return FALSE;
}

double OpSlider::SnapToStepMinMax(double unsnapped_value)
{
	double new_value = unsnapped_value;

	// Check with step
	if (m_step != 0)
	{
		double steps = (new_value - m_step_base) / m_step;
		// round it
		if (steps < 0)
		{
			steps -= 0.5;
		}
		else
		{
			steps += 0.5;
		}

		if (steps > INT_MAX || steps < INT_MIN) // this is an integer but too large for an int32
		{
			new_value = steps * m_step + m_step_base;
		}
		else
		{
			int whole_steps = int(steps);
			new_value = whole_steps * m_step + m_step_base;
		}
	}

	// Put inside [m_min, m_max]
	new_value = MAX(m_min, MIN(m_max, new_value));
	return new_value;
}

void OpSlider::SetToClosestSnapPoint(const OpPoint& point)
{
	int clicked_at;
	int track_start;
	int track_end;
	if (IsHorizontal())
	{
		clicked_at = point.x - m_start_track.x;
		track_start = m_start_track.x;
		track_end = m_end_track.x;
	}
	else
	{
		clicked_at = point.y - m_start_track.y;
		track_start = m_start_track.y;
		track_end = m_end_track.y;
	}

	int total_track = track_end - track_start;
	if (total_track == 0)
	{
		return;
	}

	double click_percent = clicked_at/(double)total_track;
	if (!IsHorizontal() || GetRTL()) // Bottom is 0, so is Right when in RTL
		click_percent = 1 - click_percent;
	double unsnapped_value = m_min + click_percent*(m_max-m_min);

	double new_value = SnapToStepMinMax(unsnapped_value);

#if 0 // Not sure if this is a good idea. Clicking the same spot repeatadly will then move the knob back and forth between the two step positions around the spot.
	// We didn't click close to another step, but make
	// sure we move it anyway in the direction indicated by the click.
	if (new_value == m_current && m_step > 0)
	{
		unsnapped_value = m_current + (unsnapped_value > m_current ? m_step : -m_step);
		new_value = SnapToStepMinMax(unsnapped_value);
	}
#endif // 0

	if (new_value != m_current)
	{
		SetValue(new_value, FALSE);
		Invalidate(GetBounds());
	}
}

void OpSlider::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener)
	{
#ifndef MOUSELESS
		listener->OnMouseEvent(this, -1, point.x, point.y, button, FALSE, nclicks);
#endif // MOUSELESS
	}

	if (!m_drag_in_progress)
	{
		return;
	}

	m_drag_in_progress = FALSE;
	if (IsValidDragEndPoint(point))
	{
		double new_value = SnapToValue(point);
		SetValue(new_value, FALSE);
	}
	Invalidate(GetBounds());
}

void OpSlider::OnMouseMove(const OpPoint &point)
{
	if (IsDead())
		return;

	if (!m_drag_in_progress)
	{
		SetIsHoveringKnob(IsPointInKnob(point));
		return;
	}
	SetIsHoveringKnob(TRUE);

	double old_drag_value = m_drag_value;
	if (IsValidDragEndPoint(point))
	{
		m_drag_value = SnapToValue(point);
	}
	else
	{
		m_drag_value = m_current;
	}

	if (m_drag_value != old_drag_value)
	{
		Invalidate(GetBounds());
		HandleOnChange();
	}
}

void OpSlider::OnMouseLeave()
{
	SetIsHoveringKnob(FALSE);
	m_drag_in_progress = FALSE;
	Invalidate(GetBounds());
}

void OpSlider::SetIsHoveringKnob(BOOL hovering)
{
	if (m_is_hovering_knob != hovering)
	{
		m_is_hovering_knob = hovering;
		Invalidate(GetBounds());
	}
}

BOOL OpSlider::IsPointInKnob(const OpPoint& point)
{
	return m_knob_position.Contains(point);
}

BOOL OpSlider::IsValidDragEndPoint(const OpPoint& point)
{
	// Close enough to the slider?
	OpRect size = GetBounds();
	size = size.InsetBy(IsHorizontal() ? -150 : -40, IsHorizontal() ? -40 : -150); // OutsetBy
	return size.Contains(point);
}

BOOL OpSlider::IsHorizontal()
{
	const OpRect size = GetBounds();
	return size.width >= size.height;
}

double OpSlider::SnapToValue(const OpPoint& point)
{
	// Look at point and m_start_drag_pos
	int dragged;
	int track_start;
	int track_end;
	if (IsHorizontal())
	{
		dragged = point.x - m_start_drag_pos.x;
		if (GetRTL())
			dragged = -dragged;
		track_start = m_start_track.x;
		track_end = m_end_track.x;
	}
	else
	{
		dragged = m_start_drag_pos.y - point.y;
		track_start = m_start_track.y;
		track_end = m_end_track.y;
	}

	int total_track = track_end - track_start;
	if (total_track == 0)
	{
		return m_min;
	}

	double drag_percent = (double)dragged/(double)total_track;
	double new_value = drag_percent*(m_max-m_min)+m_anchor;
	new_value = SnapToStepMinMax(new_value);
	return new_value;
}

BOOL OpSlider::OnInputAction(OpInputAction* action)
{
#if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED
	if (m_drag_in_progress || !IsEnabled() || IsReadOnly())
	{
		return FALSE;
	}

	if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED)
	{
		INT32 increase_key;
		INT32 decrease_key;
		if (IsHorizontal())
		{
			increase_key = OP_KEY_RIGHT;
			decrease_key = OP_KEY_LEFT;
		}
		else
		{
			increase_key = OP_KEY_UP;
			decrease_key = OP_KEY_DOWN;
		}

		OpKey::Code key = action->GetActionKeyCode();
		if (key == increase_key || key == decrease_key)
		{
			// If we have no step move one tenth of the scale
			double step = m_step == 0.0 ? (m_max-m_min)*0.1 : m_step;
			int dir = (key == increase_key) ? 1 : -1;
			int step_count = 1;
#if 0 // Never any repeat count when I test anyway
			int repeat_count = action->GetRepeatCount();
			if (repeat_count > 0)
			{
				step_count += int(log((float)repeat_count));
			}
#endif
			double new_value = m_current + dir*step*step_count;

			// In some cases the new value is out of valid range
			if (new_value > m_max)
				new_value = m_max;
			else if(new_value < m_min)
				new_value = m_min;
			// Update value if it has actually changed
			if (new_value != m_current)
			{
				m_current = new_value;
				Invalidate(GetBounds());
				HandleOnChange();
			}
			return TRUE;
		}
	}
#endif // #if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED

	return FALSE;
}

/*virtual */
void OpSlider::HandleOnChange()
{
	if (listener)
	{
		listener->OnChange(this);
	}
}

/* virtual */
void OpSlider::OnClick(OpWidget *object, UINT32 id)
{
	// Send onclick
	if (listener)
	{
		listener->OnClick(this, GetID()); // Invoke!
	}
}

/* virtual */
void OpSlider::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (!focus && !m_drag_in_progress)
	{
		// If we lose focus during a drag operation, we'll abort the drag
		m_drag_in_progress = FALSE;
		Invalidate(GetBounds());
	}
}
