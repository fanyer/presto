/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpMonthView.h"
#include "modules/widgets/OpDateTime.h"
#ifdef PLATFORM_FONTSWITCHING
#include "modules/display/fontdb.h"
#endif // PLATFORM_FONTSWITCHING
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpTime.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/viewers/viewers.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/widgets/OpCalendar.h"


// == OpDateTime ===========================================================

DEFINE_CONSTRUCT(OpDateTime)

OpDateTime::OpDateTime() :
	m_calendar(NULL),
	m_time(NULL),
	m_timezone_field(NULL),
	m_has_min_value(FALSE),
	m_has_max_value(FALSE),
	m_has_step(FALSE),
	m_include_timezone(FALSE)
{
	OP_STATUS status;

	status = OpCalendar::Construct(&m_calendar);
	CHECK_STATUS(status);
	AddChild(m_calendar, TRUE);

	status = OpTime::Construct(&m_time);
	CHECK_STATUS(status);
	AddChild(m_time, TRUE);

	m_calendar->SetTabStop(TRUE);
	m_time->SetTabStop(TRUE);

	// This might be deleted in SetIncludeTimezone, fixme: create on demand instead
	status = OpEdit::Construct(&m_timezone_field);
	CHECK_STATUS(status);
	AddChild(m_timezone_field, TRUE);
	m_timezone_field->SetText(UNI_L("UTC"));
	m_timezone_field->SetFlatMode();
	m_timezone_field->SetHasCssBorder(TRUE);
	m_timezone_field->SetHasCssBackground(TRUE);
	m_timezone_field->SetEnabled(FALSE);

	m_time->SetListener(this);
	m_calendar->SetListener(this);
}

OpDateTime::~OpDateTime()
{
}

/**
 * This is called to refill the widget after reflow or history movement.
 */
OP_STATUS OpDateTime::SetText(const uni_char* text)
{
	DateTimeSpec date_time;
	if (!text || !*text)
	{
		RETURN_IF_ERROR(m_calendar->SetText(text));
		return m_time->SetText(text);
	}
	else if (!date_time.SetFromISO8601String(text, m_include_timezone))
	{
		return OpStatus::ERR;
	}

	return SetValue(date_time);
}

OP_STATUS OpDateTime::SetValue(DateTimeSpec date_time)
{
	OP_STATUS status;
	status = m_calendar->SetValue(date_time.m_date);
	if (OpStatus::IsSuccess(status))
	{
		status = m_time->SetValue(date_time.m_time);
	}

	UpdateMinMaxStepForTime();

	return status;
}

/* virtual */
void OpDateTime::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	INT32 date_w;
	INT32 dummy_h;
	m_calendar->GetPreferedSize(&date_w, &dummy_h, 0, 0);

	INT32 time_w;
	m_time->GetPreferedSize(&time_w, &dummy_h, 0, 0);

	INT32 timezone_w = GetTimezoneWidth();

	// margin of 2 pixels
	*w = date_w + 2 + time_w + 2 + timezone_w;
}


void OpDateTime::SetMinValue(DateTimeSpec new_min)
{
	if (!m_has_min_value || new_min.AsDouble() != m_min_value.AsDouble())
	{
		m_has_min_value = TRUE;
		m_min_value = new_min;
		m_calendar->SetMinValue(new_min.m_date);
		UpdateMinMaxStepForTime();
	}
}

void OpDateTime::SetMaxValue(DateTimeSpec new_max)
{
	if (!m_has_max_value || new_max.AsDouble() != m_max_value.AsDouble())
	{
		m_has_max_value = TRUE;
		m_max_value = new_max;
		m_calendar->SetMaxValue(new_max.m_date);
		UpdateMinMaxStepForTime();
	}
}

void OpDateTime::SetStep(double step_base, double step)
{
	if (!m_has_step || step_base != m_step_base || m_step != step)
	{
		m_has_step = TRUE;
		m_step_base = step_base;
		m_step = step;
		// We have values in milliseconds and the calendar need values in days.
		m_calendar->SetStep(step_base/86400000.0, step/86400000.0);
		UpdateMinMaxStepForTime();
	}
}

void OpDateTime::SetTimeStepForSpecificDate(DaySpec date)
{
	// Convert the step to the timewidget by moving the step_base.
	double date_in_ms = date.AsDouble()*86400000.0;
	double distance_from_step_base_to_date = date_in_ms - m_step_base;
	int steps = static_cast<int>(op_ceil(distance_from_step_base_to_date/m_step));
	double time_step_base_double = steps*m_step + m_step_base - date_in_ms;
	OP_ASSERT(time_step_base_double >= 0.0);
	if (time_step_base_double >= 86400000.0)
	{
		// No values available this day. Should disable the time or something
		m_time->ClearStep();
		return;
	}
	OP_ASSERT(time_step_base_double < 86400000.0);
	int time_step_base_int = static_cast<int>(time_step_base_double); // Don't want to round in case we end up on the next day
	TimeSpec time_step_base;
	time_step_base.Clear();
	time_step_base.SetFromNumber(time_step_base_int, 1000);
	m_time->SetStepMS(time_step_base, m_step);
}

void OpDateTime::UpdateMinMaxStepForTime()
{
	if (m_calendar->HasValue())
	{
		DaySpec date = m_calendar->GetDaySpec();
		TimeSpec min_time;
		min_time.Clear(); // Midnight as min value if nothing else said
		if (m_has_min_value && date.AsDouble() == m_min_value.m_date.AsDouble())
		{
			min_time = m_min_value.m_time;
		}
		m_time->SetMinValue(min_time);

		TimeSpec max_time;
		max_time.Clear();
		max_time.SetFromNumber(86399999, 1000); // Midnight as max value if nothing else said
		if (m_has_max_value && date.AsDouble() == m_max_value.m_date.AsDouble())
		{
			max_time = m_max_value.m_time;
		}
		m_time->SetMaxValue(max_time);

		if (m_has_step && m_step > 0.0)
		{
			SetTimeStepForSpecificDate(date);
		}
		else
		{
			m_time->ClearStep();
		}
	}
	else if (m_has_max_value && m_has_min_value &&
		m_min_value.m_date.AsDouble() == m_max_value.m_date.AsDouble())
	{
		m_time->SetMinValue(m_min_value.m_time);
		m_time->SetMaxValue(m_max_value.m_time);
		SetTimeStepForSpecificDate(m_min_value.m_date);
	}
	else
	{
		TimeSpec min_time;
		min_time.Clear(); // Midnight as min value if nothing else said
		m_time->SetMinValue(min_time);
		TimeSpec max_time;
		max_time.Clear();
		max_time.SetFromNumber(86399999, 1000); // Midnight as max value if nothing else said
		m_time->SetMaxValue(max_time);

		if (m_has_step &&
			m_step > 0.0 &&
			86400000.0 / m_step == static_cast<int>(86400000.0/m_step))
		{
			// Same step every day
			DaySpec dummy_day = { 2000, 0, 1 };
			SetTimeStepForSpecificDate(dummy_day);
		}
		else
		{
			m_time->ClearStep();
		}
	}
}

OP_STATUS OpDateTime::GetText(OpString &str)
{
	// XXX Ever used?
	OP_ASSERT(FALSE); // Remove this function?
	if (m_calendar->HasValue())
	{
		DaySpec date = m_calendar->GetDaySpec();
		uni_char* buf = str.Reserve(date.GetISO8601StringLength()+1);
		if (!buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		date.ToISO8601String(buf);
		OP_STATUS status = str.Append("T");
		if (OpStatus::IsError(status))
		{
			return status;
		}
	}

	TimeSpec time;
	if (m_time->HasValue() && m_time->GetTime(time))
	{
		OpString time_string;
		uni_char* buf = time_string.Reserve(time.GetISO8601StringLength()+1);
		if (!buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		time.ToISO8601String(buf);

		OP_STATUS status = str.Append(time_string);
		if (OpStatus::IsError(status))
		{
			return status;
		}
		status = str.Append("Z"); // Timezone
		if (OpStatus::IsError(status))
		{
			return status;
		}
	}
	return OpStatus::OK;
}

void OpDateTime::SetReadOnly(BOOL readonly)
{
	m_time->SetReadOnly(readonly);
	m_calendar->SetReadOnly(readonly);
}

INT32 OpDateTime::GetTimezoneWidth() const
{
	UINT32 width;
	if (m_include_timezone)
	{
		// Guessing. Not very good. And I hope VisualDevice has the correct font set.
		int border_padding_overhead = 6;
		width = GetVisualDevice()->GetFontStringWidth(UNI_L("UTC"), 3) + border_padding_overhead;
	}
	else
	{
		width = 0;
	}
	return width;
}

void OpDateTime::OnResize(INT32* new_w, INT32* new_h)
{
	INT32 time_w;
	INT32 time_h;
	m_time->GetPreferedSize(&time_w, &time_h, 0, 0); // dummy cols and rows
	int max_time_w = 3 * *new_w / 5;
	if (time_w > max_time_w) // if it's more than 3/5 of the available size, we restrict it to 3/5.
		time_w = max_time_w;

	INT32 timezone_width = GetTimezoneWidth();

	int max_tz_width = 3 * (*new_w - time_w) / 5;
	if (timezone_width > max_tz_width)
	{
		timezone_width = max_tz_width;
	}
	int calendar_width = *new_w - time_w - 2 - timezone_width - 2;
	m_calendar->SetRect(OpRect(0, 0, calendar_width, *new_h));
	// 2 pixel margin between the blocks
	m_time->SetRect(OpRect(calendar_width+2, 0, time_w, *new_h));

	if (m_timezone_field)
	{
		m_timezone_field->SetRect(OpRect(calendar_width + 2 + time_w + 2, 0, timezone_width, *new_h));
	}
}

void OpDateTime::EndChangeProperties()
{
	m_calendar->BeginChangeProperties();
	m_time->BeginChangeProperties();
	// propagate background color to edit field
	if (!m_color.use_default_background_color)
	{
		m_calendar->SetBackgroundColor(m_color.background_color);
		m_time->SetBackgroundColor(m_color.background_color);
	}
	m_calendar->SetHasCssBorder(HasCssBorder());
	m_time->SetHasCssBorder(HasCssBorder());

	m_time->EndChangeProperties();
	m_calendar->EndChangeProperties();
}

void OpDateTime::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (focus)
	{
		m_calendar->SetFocus(reason);
	}

	// We look different with and without focus so we need to repaint ourselves.
	Invalidate(GetBounds());
}

BOOL OpDateTime::HasValue() const
{
	return m_calendar->HasValue() || m_time->HasValue();
}

void OpDateTime::OnChange(OpWidget *widget, BOOL changed_by_mouse /*= FALSE */)
{
	if (widget == m_time || widget == m_calendar)
	{
		BOOL has_date = m_calendar->HasValue();
		BOOL has_time = m_time->HasValue();
		if (widget == m_calendar)
		{
			if (has_date)
			{
				if (!has_time)
				{
					TimeSpec t;
					t.Clear();
					t.m_hour = 12; // Default time
					DateTimeSpec dt;
					dt.m_date = m_calendar->GetDaySpec();
					dt.m_time = t;
					if (m_has_min_value && dt.IsBefore(m_min_value))
					{
						t = m_min_value.m_time;
					}
					else if (m_has_max_value && dt.IsAfter(m_max_value))
					{
						t = m_max_value.m_time;
					}

					m_time->SetValue(t);
				}
			}
			else
			{
				if (has_time)
				{
					m_time->SetText(NULL);
				}
			}
			UpdateMinMaxStepForTime();
		}
		else
		{
			OP_ASSERT(widget == m_time);
			if (has_date)
			{
				if (!has_time)
				{
					m_calendar->SetText(NULL);
				}
			}
			else
			{
				if (has_time)
				{
					TimeSpec current_written_time;
#ifdef DEBUG_ENABLE_OPASSERT
					BOOL ok_time =
#endif // DEBUG_ENABLE_OPASSERT
						m_time->GetTime(current_written_time);
					OP_ASSERT(ok_time); // The time widget shouldn't allow malformed times.

					DaySpec min_allowed_day_for_time = { 1973, 0, 15 }; // XXX
					DaySpec max_allowed_day_for_time = { 2020, 4, 5 }; // XXX
					if (m_has_min_value)
					{
						if (m_min_value.m_time.IsBefore(current_written_time))
						{
							min_allowed_day_for_time = m_min_value.m_date;
						}
						else
						{
							min_allowed_day_for_time = m_min_value.m_date.NextDay();
						}
					}

					if (m_has_max_value)
					{
						if (m_max_value.m_time.IsAfter(current_written_time))
						{
							max_allowed_day_for_time = m_min_value.m_date;
						}
						else
						{
							max_allowed_day_for_time = m_min_value.m_date.PrevDay();
						}
					}

					if (max_allowed_day_for_time.IsBefore(min_allowed_day_for_time))
					{
						// This time isn't allowed any day.
						m_time->SetText(NULL); // Clear it
					}
					else
					{
						DaySpec d = OpMonthView::GetToday();
						if (d.IsBefore(min_allowed_day_for_time))
						{
							d = min_allowed_day_for_time;
						}
						else if (d.IsAfter(max_allowed_day_for_time))
						{
							d = max_allowed_day_for_time;
						}

						m_calendar->SetValue(d);
					}
				}
			}
		}

		if (listener)
		{
			listener->OnChange(this, FALSE);
		}
	}
}

BOOL OpDateTime::GetDateTime(DateTimeSpec& out_value) const
{
	if (m_calendar->HasValue() && m_time->HasValue())
	{
		out_value.m_timezone.Clear();
		out_value.m_date = m_calendar->GetDaySpec();
		return m_time->GetTime(out_value.m_time);
	}

	return FALSE;
}

void OpDateTime::SetIncludeTimezone(BOOL val)
{
	if (val != m_include_timezone)
	{
		m_include_timezone = val;
		if (val && !m_timezone_field)
		{
			OP_STATUS status = OpEdit::Construct(&m_timezone_field);
			if (OpStatus::IsSuccess(status))
			{
				AddChild(m_timezone_field, TRUE);
			}
			else
			{
				OP_ASSERT(FALSE);
			}
		}
		else if (!val && m_timezone_field)
		{
			m_timezone_field->Remove();
			OP_DELETE(m_timezone_field);
			m_timezone_field = NULL;
		}
	}
}
