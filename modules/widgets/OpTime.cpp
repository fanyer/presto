/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpTime.h"

#include "modules/forms/datetime.h"
#include "modules/forms/webforms2number.h"

#include "modules/widgets/OpSpinner.h"
#include "modules/widgets/OpEdit.h"

#include "modules/display/vis_dev.h"


// == OpTime ===========================================================

DEFINE_CONSTRUCT(OpTime)

OpTime::OpTime() :
	m_hour_field(NULL),
#ifndef PATTERN_EDIT
	m_minute_field(NULL),
	m_colon1_field(NULL),
#endif
	m_has_min_value(FALSE),
	m_has_max_value(FALSE),
	m_has_step(FALSE),
	m_precision(TIME_PRECISION_MINUTES),
	m_readonly(FALSE)
{
	OP_STATUS status;

#ifdef PATTERN_EDIT
	status = OpEdit::Construct(&m_hour_field);
#else
	status = OpNumberEdit::Construct(&m_hour_field);
#endif // PATTERN_EDIT
	CHECK_STATUS(status);
	AddChild(m_hour_field, TRUE);
#ifdef PATTERN_EDIT
	m_hour_field->SetPattern(UNI_L("  :  "));
	m_hour_field->SetAllowedChars("0123456789");
#else
	m_hour_field->SetMaxValue(23.0); // 23 is the last hour
	m_hour_field->SetMinValue(0.0);
	m_hour_field->SetWrapAround(TRUE);

	status = OpEdit::Construct(&m_colon1_field);
	CHECK_STATUS(status);
	AddChild(m_colon1_field, TRUE);
	m_colon1_field->SetText(UNI_L(":"));
	m_colon1_field->SetFlatMode();
	m_colon1_field->SetEnabled(FALSE);

	status = OpNumberEdit::Construct(&m_minute_field);
	CHECK_STATUS(status);
	AddChild(m_minute_field, TRUE);
	m_minute_field->SetMaxValue(59.0); // 59 is the last minute
	m_minute_field->SetMinValue(0.0);
	m_minute_field->SetWrapAround(TRUE);

	m_minute_field->SetListener(this);
#endif
	m_hour_field->SetListener(this);

	status = OpSpinner::Construct(&m_spinner);
	CHECK_STATUS(status);
	AddChild(m_spinner, TRUE);
	m_spinner->SetListener(this);

	TimeSpec empty_time;
	empty_time.Clear();
	m_last_set_full_time = empty_time;
}

/**
 * This is called to refill the widget after reflow or history movement.
 */
OP_STATUS OpTime::SetText(const uni_char* text)
{
	if (!text || !*text)
	{
		// Clear the time
		return SetEmptyValue();
	}

	TimeSpec time;
	// Parse...
	if (!time.SetFromISO8601String(text))
	{
		return OpStatus::ERR;
	}

	return SetValue(time);
}

OP_STATUS OpTime::SetValue(TimeSpec time)
{
#ifdef PATTERN_EDIT
	m_last_set_full_time = time;
	switch (m_precision)
	{
//		case TIME_PRECISION_HOURS:
//			time.pattern = UNI_L("  ");
//			break;
		case TIME_PRECISION_MINUTES:
			time.SetSecondUndefined();
			time.SetFractionUndefined();
			break;
		case TIME_PRECISION_SECONDS:
			if (time.IsSecondUndefined())
			{
				time.m_second = 0;
			}
			time.SetFractionUndefined();
			break;
		case TIME_PRECISION_SUBSECOND:
		case TIME_PRECISION_SUBSECOND2:
		case TIME_PRECISION_SUBSECOND3:
			if (time.IsSecondUndefined())
			{
				time.m_second = 0;
			}
			if (time.IsFractionUndefined())
			{
				time.SetFraction(0,3);
			}
			break;
	}

	// We need 6-13 chars depending on how many decimals we have.
	OpString buf_obj;
	int str_len = time.GetISO8601StringLength();
	uni_char* buf = buf_obj.Reserve(MAX(str_len+1, 20));
	if (!buf)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	time.ToISO8601String(buf);
	int expected_len = str_len;
	if (m_precision == TIME_PRECISION_SUBSECOND)
	{
		expected_len = 12;
	}
	else if (m_precision == TIME_PRECISION_SUBSECOND2)
	{
		expected_len = 15;
	}
	else if (m_precision == TIME_PRECISION_SUBSECOND3)
	{
		expected_len = 18;
	}

	while (expected_len > str_len)
	{
		// Pad with zeroes
		buf[str_len++] = '0';
		buf[str_len] = '\0';
	}
	while (expected_len < str_len)
	{
		// Just truncate
		buf[--str_len] = '\0';
	}

	OP_STATUS status = m_hour_field->SetText(buf);
	OP_ASSERT(OpStatus::IsSuccess(status));
	UpdateButtonState();
	return status;
#else
	uni_char buf[3] = { 0, 0, 0};
	buf[0] = time.m_hour/10+'0';
	buf[1] = time.m_hour%10+'0';
	RETURN_IF_ERROR(m_hour_field->SetText(buf));
	buf[0] = time.m_minute/10+'0';
	buf[1] = time.m_minute%10+'0';
	return m_minute_field->SetText(buf);
#endif // PATTERN_EDIT
}


OP_STATUS OpTime::GetText(OpString &str)
{
	// XXX Ever used?
	OP_ASSERT(FALSE); // If this never triggers we can remove this method
	if (HasValue())
	{
		TimeSpec time;
		if (GetTime(time))
		{
			uni_char* buf = str.Reserve(time.GetISO8601StringLength()+1);
			if (!buf)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			time.ToISO8601String(buf);
		}
	}
	return OpStatus::OK;
}

void OpTime::SetReadOnly(BOOL readonly)
{
	if (readonly != m_readonly)
	{
		m_readonly = readonly;
		m_hour_field->SetReadOnly(readonly);
#ifndef PATTERN_EDIT
		m_minute_field->SetReadOnly(readonly);
#endif // PATTERN_EDIT
		UpdateButtonState();
	}
}

/*virtual*/
void OpTime::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	UINT32 colon_char_w = GetVisualDevice()->GetFontStringWidth(UNI_L(":"), 1);

	// Place for two colons and 5% of the rest
	INT32 colon_width = colon_char_w; //  3*colon_char_w/2 + 5;

	UINT32 two_digits_w = GetVisualDevice()->GetFontStringWidth(UNI_L("00"), 2);

	int padding_border_overhead_w = 8; // Guesses
	int padding_border_overhead_h = 4; // Guesses

#ifdef PATTERN_EDIT
	switch (m_precision)
	{
//	case TIME_PRECISION_HOURS:
	case TIME_PRECISION_MINUTES:
		*w = two_digits_w*2 + colon_width;
		break;
	case TIME_PRECISION_SECONDS:
		*w = two_digits_w*3 + colon_width*2;
		break;
	case TIME_PRECISION_SUBSECOND:
		*w = two_digits_w*5 + colon_width*3;
		break;
	case TIME_PRECISION_SUBSECOND2:
		*w = two_digits_w*6 + colon_width*3;
		break;
	default:
		OP_ASSERT(FALSE); // Should not be reachable
	case TIME_PRECISION_SUBSECOND3:
		*w = two_digits_w*8 + colon_width*3;
		break;
	}
	INT32 spinner_width = 0;
	INT32 spinner_height = 0;
	// We guess a height. If we get this wrong we will answer with a too wide or too narrow size
	GetPreferredSpinnerSize(font_info.size+padding_border_overhead_h+4, &spinner_width, &spinner_height);
	*w += padding_border_overhead_w + spinner_width + 2; // 2 pixels between the field and the spinner
#else
	*w = (two_digits_w + 23) *2 + 1*colon_width;
#endif // PATTERN_EDIT
	// m_timezone_field
	*h = 0; // don't care
}

void OpTime::GetPreferredSpinnerSize(int widget_height, INT32* spinner_width, INT32* spinner_height)
{
	m_spinner->GetPreferedSize(spinner_width, spinner_height);
	if (*spinner_height != 0)
	{
		float spinner_height_scale = static_cast<float>(widget_height)/ *spinner_height;
		*spinner_width = static_cast<int>(spinner_height_scale * *spinner_width + 0.5f);
	}
	*spinner_height = widget_height;
}

void OpTime::OnResize(INT32* new_w, INT32* new_h)
{
	int width = *new_w;
	int height = *new_h;

	INT32 spinner_width = 0;
	INT32 spinner_height = 0;
	GetPreferredSpinnerSize(height, &spinner_width, &spinner_height);
	m_spinner->SetRect(OpRect(width - spinner_width, 0, spinner_width, spinner_height));

	width -= spinner_width + 2; // Margin of 2 pixels
#ifdef PATTERN_EDIT
	INT32 field_width = width;
#else
	UINT32 colon_char_w = GetVisualDevice()->GetFontStringWidth(UNI_L(":"), 1);
	const int field_count = 2;
	const int colon_count = 1;
	// Place for two colons and 5% of the rest
	INT32 colon_width =  3*colon_char_w/2 + (width - 3*colon_char_w/2)/20;
	INT32 field_width = (width - colon_count*colon_width) / field_count; // One third each of the rest
#endif // PATTERN_EDIT
	int current_x = 0;
	m_hour_field->SetRect(OpRect(current_x, 0, field_width, height));

#ifndef PATTERN_EDIT
	current_x += field_width;
	m_colon1_field->SetRect(OpRect(current_x, 0, colon_width, height));

	current_x += colon_width;
	m_minute_field->SetRect(OpRect(current_x, 0, field_width, height));
#endif // !PATTERN_EDIT
}

const uni_char* OpTime::GetEditPattern() const
{
	// Could be a table instead
	switch (m_precision)
	{
//		case OpTime::TIME_PRECISION_HOURS:
//			pattern = UNI_L("  ");
//			break;
		case OpTime::TIME_PRECISION_MINUTES:
			return UNI_L("  :  ");
		case OpTime::TIME_PRECISION_SECONDS:
			return UNI_L("  :  :  ");
			break;
		case OpTime::TIME_PRECISION_SUBSECOND:
			return UNI_L("  :  :  .   ");
		case OpTime::TIME_PRECISION_SUBSECOND2:
			return UNI_L("  :  :  .      ");
	}
	OP_ASSERT(m_precision == OpTime::TIME_PRECISION_SUBSECOND3);
	return UNI_L("  :  :  .         ");
}

void OpTime::EndChangeProperties()
{
	// propagate background color to edit field
	if (!m_color.use_default_background_color)
		m_hour_field->SetBackgroundColor(m_color.background_color);
	m_hour_field->SetHasCssBackground(HasCssBackgroundImage());
	m_hour_field->SetHasCssBorder(HasCssBorder());
}

void OpTime::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (focus)
	{
		m_hour_field->SetFocus(reason);
	}
	else
	{
		// Don't leave anything broken so that the user won't be tricked into
		// believing that we'll use what he, she wrote.
		if (HasValue())
		{
			TimeSpec dummy_time_spec;
			if (!GetTime(dummy_time_spec))
			{
				// Broken input
				SetEmptyValue();
			}
		}
	}
}

BOOL OpTime::OnMouseWheel(INT32 delta,BOOL vertical)
{
	if (IsEnabled() && !m_readonly && !IsDead())
	{
		for(int i = 0; i < op_abs(delta); i++)
			OnClick(m_spinner, delta > 0 ? 1 : 0);
		return TRUE;
	}
	return FALSE;
}

void OpTime::OnChangeWhenLostFocus(OpWidget *widget)
{
	if (listener &&
		(widget == m_hour_field
#ifndef PATTERN_EDIT
		 ||
		 widget == m_minute_field
#endif // PATTERN_EDIT
		 ))
	{
		// We no longer need any backup to keep parts of the time since the user has
		// written a new time that we will use instead.
		TimeSpec empty_time;
		empty_time.Clear();
		m_last_set_full_time = empty_time;

//		m_seconds = 0;
//		m_hundreths = 0;
		listener->OnChange(this);
	}
}

OP_STATUS OpTime::SetEmptyValue()
{
	const uni_char* pattern = GetEditPattern();
	return m_hour_field->SetText(pattern);
}

BOOL OpTime::HasValue() const
{
#ifdef PATTERN_EDIT
	OpString text_val;
	m_hour_field->GetText(text_val);
	if (text_val.IsEmpty())
	{
		return FALSE;
	}

	const uni_char* pattern = GetEditPattern();
	return uni_strcmp(text_val.CStr(), pattern) != 0;
#else
	return m_hour_field->HasValue() ||
		m_minute_field->HasValue();
#endif // PATTERN_EDIT
}

BOOL OpTime::GetTime(TimeSpec& out_value) const
{
#ifdef PATTERN_EDIT
	if (!HasValue())
#else
	if (!m_hour_field->HasValue() ||
		!m_minute_field->HasValue())
#endif // PATTERN_EDIT
	{
		return FALSE;
	}

	// All has values

#ifdef PATTERN_EDIT
	OpString text_val;
	m_hour_field->GetText(text_val);
	if (!out_value.SetFromISO8601String(text_val.CStr()))
	{
		return FALSE;
	}

	switch (m_precision)
	{
	case TIME_PRECISION_MINUTES: // We have lost seconds and hundreths
		if (!m_last_set_full_time.IsSecondUndefined())
		{
			out_value.m_second = m_last_set_full_time.m_second;
		}
		// fall through
	case TIME_PRECISION_SECONDS: // We have lost hundreths
		if (!m_last_set_full_time.IsFractionUndefined())
		{
			OP_ASSERT(!m_last_set_full_time.IsSecondUndefined());
			out_value.SetFraction(m_last_set_full_time.GetFraction(),
				m_last_set_full_time.GetFractionDigitCount());
		}
	}

	return TRUE;
#else
	int hour;
	int minute;
	if (!m_hour_field->GetIntValue(hour) ||
		!m_minute_field->GetIntValue(minute))
	{
		return FALSE;
	}

	if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
	{
		return FALSE;
	}

	TimeSpec time_spec = { hour, minute };
	out_value = time_spec;

	return TRUE;
#endif // PATTERN_EDIT
}

void OpTime::SetTimePrecision(TimeFieldPrecision precision)
{
	if (precision != m_precision)
	{
		m_precision = precision;

		const uni_char* pattern = GetEditPattern();
		m_hour_field->SetPattern(pattern);
	}
}

void OpTime::SetMinValueInternal(BOOL enable, TimeSpec new_min)
{
	if (enable && (!m_has_min_value || new_min.AsDouble() != m_min_value.AsDouble()))
	{
		m_has_min_value = TRUE;
		m_min_value = new_min;
		UpdateButtonState();
	}
	else if (!enable && m_has_min_value)
	{
		m_has_min_value = FALSE;
		UpdateButtonState();
	}
	// else same state - avoid triggering repaint
}

void OpTime::SetMaxValueInternal(BOOL enable, TimeSpec new_max)
{
	if (enable && (!m_has_max_value || new_max.AsDouble() != m_max_value.AsDouble()))
	{
		m_has_max_value = TRUE;
		m_max_value = new_max;
		UpdateButtonState();
	}
	else if (!enable && m_has_max_value)
	{
		m_has_max_value = FALSE;
		UpdateButtonState();
	}
	// else same state - avoid triggering repaint
}

void OpTime::SetStepInternal(BOOL enable, TimeSpec step_base, double step_value_milliseconds)
{
	if (enable && (!m_has_step || m_step_base.AsDouble() != step_base.AsDouble() || m_step_value_milliseconds != step_value_milliseconds))
	{
		m_has_step = TRUE;
		m_step_base = step_base;
		m_step_value_milliseconds = step_value_milliseconds;
		UpdateButtonState(); // Can affect the button state if the step only allows a single date of the day
	}
	else if (!enable && m_has_step)
	{
		m_has_step = FALSE;
		UpdateButtonState(); // Can affect the button state if the step only allows a single date of the day
	}
	// else same state - avoid triggering repaint
}

void OpTime::UpdateButtonState()
{
	BOOL is_enabled = IsEnabled() && !m_readonly;
	BOOL enable_up_button = is_enabled;
	BOOL enable_down_button = is_enabled;
	TimeSpec written_time;
	if (GetTime(written_time))
	{
		if (m_has_max_value && !(m_has_min_value && m_min_value.IsAfter(m_max_value)))
		{
			if (!written_time.IsBefore(m_max_value))
			{
				enable_up_button = FALSE;
			}
		}

		if (m_has_min_value && !(m_has_max_value && m_min_value.IsAfter(m_max_value)))
		{
			if (!written_time.IsAfter(m_min_value))
			{
				enable_down_button = FALSE;
			}
		}

		if (m_has_step)
		{
			double value_number = written_time.AsDouble()*1000;
			BOOL wrap_around = !m_has_min_value && !m_has_max_value;
			double min_value = m_has_min_value ? m_min_value.AsDouble()*1000.0 : 0.0;
			double max_value = m_has_max_value ? m_max_value.AsDouble()*1000.0 : (wrap_around ? 86400000.0 : 86399999.0); // 23:59:59.999
			double dummy_result_value;

			for (int direction = -1; direction <= 1; direction += 2)
			{
				OP_STATUS status =
					WebForms2Number::StepNumber(value_number, min_value,
												max_value,
												m_has_step ? (m_step_base.AsDouble()*1000) : value_number,
												m_step_value_milliseconds, direction,
												wrap_around, /* wrap_around */
												TRUE, /* fuzzy step, be helpful. */
												dummy_result_value);
				if (OpStatus::IsError(status))
				{
					if (direction == -1)
					{
						enable_down_button = FALSE;
					}
					else
					{
						enable_up_button = FALSE;
					}
				}
			}
		}
	}
	m_spinner->SetDownEnabled(enable_down_button);
	m_spinner->SetUpEnabled(enable_up_button);
}

void OpTime::MinMaxAdjust(int delta, TimeSpec& unadjusted_value) const
{
	// Correct value with respect to min and max
	if (delta > 0 && m_has_max_value && !(m_has_min_value && m_min_value.IsAfter(m_max_value)))
	{
		if (unadjusted_value.IsAfter(m_max_value))
		{
			unadjusted_value = m_max_value;
		}
	}
	else if (delta < 0 && m_has_min_value && !(m_has_max_value && m_min_value.IsAfter(m_max_value)))
	{
		if (unadjusted_value.IsBefore(m_min_value))
		{
			unadjusted_value = m_min_value;
		}
	}
}

void OpTime::OnClick(OpWidget *object, UINT32 id)
{
	if (m_readonly)
		return;
	if (object == m_spinner)
	{
		int caret_pos = -1;
		int delta = id ? -1 : 1;
		TimeSpec written_time;
		if (GetTime(written_time))
		{
			int step_value_milliseconds = m_has_step ? static_cast<int>(m_step_value_milliseconds + 0.5): 0;
			caret_pos = m_hour_field->GetCaretOffset();
			if (step_value_milliseconds == 0 ||
				(step_value_milliseconds < 1000 && caret_pos < 9) ||
				(step_value_milliseconds == 60000 && caret_pos < 6) ||
				(step_value_milliseconds == 3600000 && caret_pos < 3))
			{
				if (caret_pos < 3)
				{
					// hour field - step an hour
					if (m_has_step)
					{
						int new_delta = static_cast<int>((delta * 3600000)/step_value_milliseconds + delta/2); // delta/2 to get rounding
						if (new_delta != 0)
						{
							OP_ASSERT(new_delta * delta > 0); // Same sign
							delta = new_delta;
						}
					}
					else
					{
						step_value_milliseconds = 3600000;
					}
				}
				else if (caret_pos < 6)
				{
					// minute field - step a minute
					if (step_value_milliseconds > 0)
					{
						int new_delta = static_cast<int>((delta * 60000)/step_value_milliseconds + delta/2);
						if (new_delta != 0)
						{
							OP_ASSERT(new_delta * delta > 0); // Same sign
							delta = new_delta;
						}
					}
					else
					{
						step_value_milliseconds = 60000;
					}
				}
				else if (caret_pos < 9)
				{
					// second field - step a second
					if (step_value_milliseconds > 0)
					{
						int new_delta = static_cast<int>((delta * 1000)/step_value_milliseconds + delta/2);
						if (new_delta != 0)
						{
							OP_ASSERT(new_delta * delta > 0); // Same sign
							delta = new_delta;
						}
					}
					else
					{
						step_value_milliseconds = 1000;
					}
				}
				else
				{
					// hundreths
					if (step_value_milliseconds == 0)
					{
						int digit_count = written_time.GetFractionDigitCount();
						int precision = static_cast<int>(op_pow(10.0, digit_count));
						step_value_milliseconds = 1000/precision;
						OP_ASSERT(step_value_milliseconds > 0); // 1, 10 or 100
					}
				}
			}


			// Step with the step_value after making every value into the unit "milliseconds since midnight"
			double result_value;
			double value_number = written_time.AsDouble()*1000;
			BOOL wrap_around = !m_has_min_value && !m_has_max_value;
			double min_value = m_has_min_value ? m_min_value.AsDouble()*1000 : 0.0;
			double max_value = m_has_max_value ? m_max_value.AsDouble()*1000 : (wrap_around ? 86400000.0 : 86399999.0); // 23:59:59.999
			OP_STATUS status =
				WebForms2Number::StepNumber(value_number, min_value,
											max_value,
											m_has_step ? (m_step_base.AsDouble()*1000) : value_number,
											step_value_milliseconds, delta,
											wrap_around, /* wrap_around */
											TRUE, /* fuzzy step, be helpful. */
											result_value);
			if (OpStatus::IsSuccess(status))
			{
				// Now we have result value in thousands of seconds
				OP_ASSERT(result_value >= 0.0);
				int int_result = static_cast<int>(result_value + 0.5);
				if (int_result >= 86400000)
				{
					int_result = wrap_around ? 0 : 86399999;
				}
				written_time.m_hour = int_result / 3600000;
				int_result -= written_time.m_hour * 3600000;
				written_time.m_minute = int_result / 60000;
				int_result -= written_time.m_minute * 60000;
				if (written_time.m_second != -1 || int_result > 0)
				{
					written_time.m_second = int_result / 1000;
					int_result -= written_time.m_second*1000;
					if (int_result > 0 || written_time.GetFractionDigitCount() > 0)
					{
						if (int_result == 0)
						{
							written_time.SetFraction(0, 0);
						}
						else if ((int_result/100) * 100 == int_result)
						{
							written_time.SetFraction(int_result/100, 1);
						}
						else if ((int_result/10) * 10 == int_result)
						{
							written_time.SetFraction(int_result/10, 2);
						}
						else
						{
							written_time.SetFraction(int_result, 3);
						}
					}
				}
			}
		}
		else
		{
			// No current time, start with 00:00:00.000
			written_time.m_hour = 0;
			written_time.m_minute = 0;
			written_time.m_second = 0;
			written_time.SetFraction(0, 3);
			MinMaxAdjust(delta, written_time);
		}

		OP_STATUS status = SetValue(written_time);
#ifdef _DEBUG
		OP_ASSERT(OpStatus::IsSuccess(status));
#else
		OpStatus::Ignore(status);
#endif // _DEBUG

		if (caret_pos != -1)
		{
			// Set back caret where it was so next spinner action affects the same unit as this time.
			m_hour_field->SetCaretOffset(caret_pos);
		}

		if (listener)
		{
			listener->OnChange(this);
		}
	}
}

void OpTime::SetEnabled(BOOL enabled)
{
	OpWidget::SetEnabled(enabled);
	UpdateButtonState();
}

/** ***********************************************************************
**
**	OnInputAction
**
************************************************************************ */
BOOL OpTime::OnInputAction(OpInputAction* action)
{
#if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED
	if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION &&
		action->GetChildAction()->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED)
	{
		OpKey::Code key = action->GetChildAction()->GetActionKeyCode();
		switch (key)
		{
		case OP_KEY_UP:
		case OP_KEY_DOWN:
			// Simulate click on the up or down button
			OnClick(m_spinner, key == OP_KEY_DOWN ? 1 : 0);
			return TRUE;
		}
	}
#endif // defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED

	return FALSE;
}
