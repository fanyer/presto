/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpNumberEdit.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpSpinner.h"

#include "modules/forms/webforms2number.h"

#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"

#define SPINNER_MARGIN 1

// == OpNumberEdit ===========================================================

DEFINE_CONSTRUCT(OpNumberEdit)

OpNumberEdit::OpNumberEdit() :
	m_edit(NULL),
	m_spinner(NULL),
	m_min_value(-DBL_MAX),
	m_max_value(DBL_MAX),
	m_step_base(0.0),
	m_step(1.0),
	m_wrap_around(FALSE)
{
	OP_STATUS status;

	status = OpEdit::Construct(&m_edit);
	CHECK_STATUS(status);
	AddChild(m_edit, TRUE);

	status = OpSpinner::Construct(&m_spinner);
	CHECK_STATUS(status);
	AddChild(m_spinner, TRUE);

	m_edit->SetListener(this);
	m_spinner->SetListener(this);
}

OP_STATUS OpNumberEdit::SetText(const uni_char* text)
{
	if (text && *text)
	{
		double val;
		uni_char* end_ptr = NULL;
		val = uni_strtod(text, &end_ptr);
		// Check for NaN and Infinity
		if (!end_ptr || *end_ptr || op_isinf(val) || op_isnan(val))
		{
			return OpStatus::ERR;
		}
	}
	RETURN_IF_ERROR(m_edit->SetText(text));

	UpdateButtonState();

	return OpStatus::OK;
}

OP_STATUS OpNumberEdit::GetText(OpString &str)
{
	double val;
	if (HasValue() && GetValue(val))
	{
		OP_STATUS status;
		uni_char* value_buf = str.Reserve(33);
		if (!value_buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		status = WebForms2Number::DoubleToString(val, value_buf);
		return status;
	}

	return OpStatus::OK;
}

OP_STATUS OpNumberEdit::GetGhostText(OpString &str)
{
	return m_edit->GetGhostText(str);
}

OP_STATUS OpNumberEdit::SetGhostText(const uni_char* ghost_text)
{
	return m_edit->SetGhostText(ghost_text);
}

void OpNumberEdit::SetReadOnly(BOOL readonly)
{
	m_edit->SetReadOnly(readonly);
	m_spinner->SetUpEnabled(!readonly && IsEnabled());
	m_spinner->SetDownEnabled(!readonly && IsEnabled());
}

static INT32 GetSpinnerWidth(OpSpinner* spinner, INT32 height)
{
	INT32 spinner_width = 0;
	INT32 spinner_height = 0;
	spinner->GetPreferedSize(&spinner_width, &spinner_height);
	float spinner_height_scale = static_cast<float>(height)/ spinner_height;
	spinner_width = static_cast<int>(spinner_height_scale * spinner_width + 0.5f);
	return spinner_width;
}

INT32 OpNumberEdit::GetExtraWidth(int height)
{
	INT32 left_padding = 0;
	INT32 right_padding = 0;
#ifdef SKIN_SUPPORT
	INT32 top = 0;
	INT32 bottom = 0;
	if (m_edit->GetBorderSkin())
		m_edit->GetBorderSkin()->GetPadding(&left_padding, &top, &right_padding, &bottom);
#else
	left_padding = right_padding = 2;
#endif // SKIN_SUPPORT
	return GetSpinnerWidth(m_spinner, height) + SPINNER_MARGIN + left_padding + right_padding;
}

void OpNumberEdit::Layout(INT32 width, INT32 height)
{
	INT32 spinner_width = GetSpinnerWidth(m_spinner, height);
	INT32 spinner_height = height;

	if (!GetRTL())
	{
		m_spinner->SetRect(OpRect(width - spinner_width, 0, spinner_width, spinner_height));
		m_edit->SetRect(OpRect(0, 0, width - spinner_width - SPINNER_MARGIN, height));
	}
	else
	{
		m_spinner->SetRect(OpRect(0, 0, spinner_width, spinner_height));
		m_edit->SetRect(OpRect(spinner_width + SPINNER_MARGIN, 0, width - spinner_width - SPINNER_MARGIN, height));
	}
}

void OpNumberEdit::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (focus)
	{
		if(reason != FOCUS_REASON_RELEASE)
		{
			m_edit->SetFocus(reason);
		}
	}
}

BOOL OpNumberEdit::OnMouseWheel(INT32 delta, BOOL vertical)
{
	if (IsEnabled() && !m_edit->IsReadOnly() && !IsDead())
	{
		int direction = delta > 0 ? -1 : 1;
		ChangeNumber(direction);
		return TRUE;
	}
	return FALSE;
}

void OpNumberEdit::EndChangeProperties()
{
	// propagate background color to edit field
	if (!m_color.use_default_background_color)
		m_edit->SetBackgroundColor(m_color.background_color);
	m_edit->SetHasCssBackground(HasCssBackgroundImage());
	m_edit->SetHasCssBorder(HasCssBorder());
}

void OpNumberEdit::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	if (!IsForm())
	{
		m_edit->GetPreferedSize(w, h, cols, rows);
		*w += GetExtraWidth(*h);
	}
}

void OpNumberEdit::OnChangeWhenLostFocus(OpWidget *widget)
{
	if (listener && widget == m_edit)
		listener->OnChangeWhenLostFocus(this);
}

BOOL OpNumberEdit::HasValue() const
{
	OpString value_str_obj;
	OP_STATUS status = m_edit->GetText(value_str_obj);
	if (!OpStatus::IsSuccess(status))
	{
		return FALSE;
	}

	return !value_str_obj.IsEmpty();
}

BOOL OpNumberEdit::GetValue(double& return_value) const
{
	OpString value_str_obj;
	OP_STATUS status = m_edit->GetText(value_str_obj);
	if (!OpStatus::IsSuccess(status))
	{
		return FALSE;
	}

	const uni_char* value_str = value_str_obj.CStr();
	if (!value_str)
	{
		OP_ASSERT(!"OOM or failure to check with HasValue()");
		return FALSE;
	}
	uni_char* end_ptr = NULL;
	return_value = uni_strtod(value_str, &end_ptr);
	if (end_ptr)
		return *end_ptr == '\0';
	return FALSE;
}

void OpNumberEdit::ChangeNumber(int direction)
{
	OP_ASSERT(direction != 0);
	double delta = direction * m_step;
	double value_number;
	if (HasValue() && GetValue(value_number))
	{
		value_number = WebForms2Number::SnapToStep(value_number, m_step_base, m_step, direction);
		double result_value;
		OP_STATUS status =
			WebForms2Number::StepNumber(value_number, m_min_value,
										m_max_value, m_step_base,
										m_step, direction, m_wrap_around,
										TRUE, /* fuzzy step, be helpful. */
										result_value);
		if (OpStatus::IsSuccess(status))
		{
			SetNumberValue(result_value);
		}
	}
	else if (m_wrap_around)
	{
		// If we have a wrappable number control and the user
		// clicks up or down, start the number even if there was none from the beginning.
		double value_number = delta >= 0 ? m_min_value : m_max_value;
		SetNumberValue(value_number);
	}
	else
	{
		// Not wrap around and no current content.
		if (m_min_value > m_max_value)
		{
			return;
		}
		if (delta >= 0)
		{
			// some low value
			if (m_min_value != -DBL_MAX)
			{
				SetNumberValue(m_min_value);
			}
			else if (m_max_value >= 0.0)
			{
				SetNumberValue(0.0);
			}
			else
			{
				SetNumberValue(m_max_value); // What else? -DBL_MAX?
			}
		}
		else
		{
			// some high value
			if (m_max_value != DBL_MAX)
			{
				SetNumberValue(m_max_value);
			}
			else if (m_min_value <= 0.0)
			{
				SetNumberValue(0.0);
			}
			else
			{
				SetNumberValue(m_min_value); // What else? DBL_MAX?
			}
		}
	}

	if (listener)
	{
		listener->OnChangeWhenLostFocus(this); // since normal OnChange is filtered
	}
}

void OpNumberEdit::SetNumberValue(double value_number)
{
	OpString value_str;
	uni_char* value_buf = value_str.Reserve(33);
	if (!value_buf)
	{
		// FIXME OOM!
		return;
	}
	OP_STATUS status = WebForms2Number::DoubleToString(value_number, value_buf);
	if (OpStatus::IsSuccess(status))
	{
		m_edit->SetTextAndFireOnChange(value_str.CStr());
	}

	UpdateButtonState();
}

void OpNumberEdit::OnClick(OpWidget *object, UINT32 id)
{
	if (object == m_spinner && m_step != 0 && !m_edit->IsReadOnly())
	{
		int direction = (id == 1) ? -1 : 1;
		ChangeNumber(direction);
	}
}


void OpNumberEdit::OnChange(OpWidget *widget, BOOL changed_by_mouse /*= FALSE */)
{
	OP_ASSERT(widget == m_edit);
//	if (widget == m_edit)
	{
		if (listener)
		{
			listener->OnChange(this);
		}

		UpdateButtonState();
	}
}

void OpNumberEdit::SelectText()
{
	m_edit->SelectText();
}

/* virtual */
void OpNumberEdit::GetSelection(INT32 &start_ofs, INT32 &stop_ofs, SELECTION_DIRECTION &direction, BOOL line_normalized)
{
	m_edit->GetSelection(start_ofs, stop_ofs, direction, line_normalized);
}

/* virtual */
void OpNumberEdit::SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction, BOOL line_normalized)
{
	m_edit->SetSelection(start_ofs, stop_ofs, direction, line_normalized);
}

INT32 OpNumberEdit::GetCaretOffset()
{
	return m_edit->GetCaretOffset();
}

void OpNumberEdit::SetCaretOffset(INT32 caret_ofs)
{
	m_edit->SetCaretOffset(caret_ofs);
}

void OpNumberEdit::UpdateButtonState()
{
	double value_number;
	if (HasValue() && GetValue(value_number))
	{
		BOOL is_changeable = IsEnabled() && !m_edit->IsReadOnly();

		BOOL enable_down_button = is_changeable;
		if (enable_down_button && !m_wrap_around)
		{
			double dummy_result_value;
			OP_STATUS status =
				WebForms2Number::StepNumber(value_number, m_min_value,
											m_max_value, m_step_base,
											m_step, -1, m_wrap_around,
											TRUE, /* fuzzy step, be helpful. */
											dummy_result_value);
			if (OpStatus::IsError(status))
			{
				enable_down_button = FALSE;
			}
		}
		m_spinner->SetDownEnabled(enable_down_button);

		BOOL enable_up_button = is_changeable && (m_wrap_around || value_number < m_max_value);
		if (enable_up_button && !m_wrap_around)
		{
			double dummy_result_value;
			OP_STATUS status =
				WebForms2Number::StepNumber(value_number, m_min_value,
											m_max_value, m_step_base,
											m_step, 1, m_wrap_around,
											TRUE, /* fuzzy step, be helpful. */
											dummy_result_value);
			if (OpStatus::IsError(status))
			{
				enable_up_button = FALSE;
			}
		}
		m_spinner->SetUpEnabled(enable_up_button);
	}
}

void OpNumberEdit::SetMinValue(double new_min)
{
	if (m_min_value != new_min)
	{
		m_min_value = new_min;
		UpdateButtonState();
	}
}

void OpNumberEdit::SetMaxValue(double new_max)
{
	if (m_max_value != new_max)
	{
		m_max_value = new_max;
		UpdateButtonState();
	}
}

OP_STATUS OpNumberEdit::SetAllowedChars(const char* new_allowed_chars)
{
	return m_edit->SetAllowedChars(new_allowed_chars);
}

void OpNumberEdit::SetWrapAround(BOOL wrap_around)
{
	if (m_wrap_around != wrap_around)
	{
		m_wrap_around = wrap_around;
		UpdateButtonState();
	}
}

/** ***********************************************************************
**
**	OnInputAction
**
************************************************************************ */
BOOL OpNumberEdit::OnInputAction(OpInputAction* action)
{
#if defined OP_KEY_UP_ENABLED && defined OP_KEY_DOWN_ENABLED
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			switch (action->GetChildAction()->GetAction())
			{
				case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
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
			}

			return FALSE;
		}
	}
#endif // defined OP_KEY_UP_ENABLED && defined OP_KEY_DOWN_ENABLED

	return FALSE;
}
