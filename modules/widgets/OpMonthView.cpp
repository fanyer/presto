/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpMonthView.h"

#include "modules/widgets/OpCalendar.h"
#include "modules/forms/datetime.h"
#include "modules/widgets/OpNumberEdit.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpButton.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/viewers/viewers.h"

/**
 * TODO:
 * optionally sunday as first day of the week.
 * Don't include the week list when not needed.
 * Support CSS in some meaningful way.
 * Update CSS while browsing.
 * Make month navigation skip impossible months.
 */

static void GetLanguageString(Str::LocaleString str_id, OpString& out_str)
{
	out_str.Empty();
	TRAPD(status, g_languageManager->GetStringL(str_id, out_str));
	OpStatus::Ignore(status); // Empty string and we'll live with that
}

#define MONTHVIEW_PADDING 2 ///< Padding inside the surrounding black border.

// == OpMonthView ===========================================================

DEFINE_CONSTRUCT(OpMonthView)

OpMonthView::OpMonthView() :
	m_has_max(FALSE),
	m_has_min(FALSE),
	m_last_click_was_on_clear(FALSE), // Set in OnClick
	m_focus_on_day(FALSE),
	m_focus_on_week(FALSE),
//	m_focused_week_no(0)
//	m_focused_day_in_week_no(0)
	m_has_step(FALSE),
	m_hovered_row(-1),
	m_hovered_column(-1),
	m_mode(MONTHVIEW_MODE_DAY),
	m_readonly(FALSE)
{
	m_last_clicked_day.m_year = 0; // The same as "no value"
	m_last_clicked_week.m_year = 0; // The same as "no value"

	OP_STATUS status;

	//status = OpButton::Construct(&m_clear_button);
	//CHECK_STATUS(status);
	//AddChild(m_clear_button, TRUE); // now owned by the widget system
	//m_clear_button->SetListener(this);

	status = OpButton::Construct(&m_today_button);
	CHECK_STATUS(status);
	AddChild(m_today_button, TRUE); // now owned by the widget system
	m_today_button->SetListener(this);

	status = OpButton::Construct(&m_prev_month_button);
	CHECK_STATUS(status);
	AddChild(m_prev_month_button, TRUE); // now owned by the widget system
	m_prev_month_button->SetListener(this);

	status = OpEdit::Construct(&m_month_name_field);
	CHECK_STATUS(status);
	AddChild(m_month_name_field, TRUE); // now owned by the widget system
	m_month_name_field->SetEllipsis(ELLIPSIS_END);
	m_month_name_field->SetFlatMode();

	status = OpButton::Construct(&m_next_month_button);
	CHECK_STATUS(status);
	AddChild(m_next_month_button, TRUE);  // now owned by the widget system
	m_next_month_button->SetListener(this);

	status = OpNumberEdit::Construct(&m_year_field);
	CHECK_STATUS(status);
	AddChild(m_year_field, TRUE); // now owned by the widget system
	// Listener set below after the SetViewedMonth to avoid it being called during initialization

#if LANGUAGE_DATABASE_VERSION > 829
	OpString str;
	//GetLanguageString(Str::S_CALENDAR_CLEAR, str);
	//m_clear_button->SetText(str.CStr());
	GetLanguageString(Str::S_CALENDAR_TODAY, str);
	m_today_button->SetText(str.CStr());
#else
#ifdef _DEBUG
	static BOOL warned_once = FALSE;
	if (!warned_once)
	{
		OP_ASSERT(!"You are using a too old strings module and will have wrong strings in the calendar");
		warned_once = TRUE;
	}
#endif // _DEBUG
	//m_clear_button->SetText(UNI_L("None"));
	m_today_button->SetText(UNI_L("Today"));
#endif // LANGUAGE_DATABASE_VERSION
#ifdef SKIN_SUPPORT
	m_prev_month_button->GetForegroundSkin()->SetImage("Left Arrow");
	m_next_month_button->GetForegroundSkin()->SetImage("Right Arrow");
	m_prev_month_button->SetButtonStyle(OpButton::STYLE_IMAGE);
	m_next_month_button->SetButtonStyle(OpButton::STYLE_IMAGE);
	//m_prev_month_button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	//m_next_month_button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
#endif
	m_prev_month_button->SetText(UNI_L("<"));
	m_next_month_button->SetText(UNI_L(">"));

	// Start with current month/week/day
	DaySpec today = GetToday();
	MonthSpec month = { today.m_year, today.m_month };

	SetViewedMonth(month);

	// Now we can set the listener without getting any strange
	// callbacks during init.
	m_year_field->SetListener(this);
}

class JustifyCenter
{
	WIDGET_FONT_INFO& m_font_info;
	JUSTIFY m_normal_justify;
public:
	JustifyCenter(WIDGET_FONT_INFO& font_info) :
		m_font_info(font_info), m_normal_justify(font_info.justify)
	{
		m_font_info.justify = JUSTIFY_CENTER;
	}
	~JustifyCenter()
	{
		m_font_info.justify = m_normal_justify;
	}
};
#define SetDayButtonFont JustifyCenter
#define SetHeaderFont JustifyCenter

void OpMonthView::OnKeyboardInputLost(OpInputContext* new_input_context,
									  OpInputContext* old_input_context,
									  FOCUS_REASON reason)
{
	if (m_parent_calendar)
	{
		while (new_input_context && new_input_context != this)
		{
			new_input_context = new_input_context->GetParentInputContext();
		}

		if (!new_input_context)
		{
			m_parent_calendar->OnMonthViewLostFocus();
		}
	}

	OpWidget::OnKeyboardInputLost(new_input_context, old_input_context, reason);

}

void OpMonthView::GetPreferedSize(INT32* width, INT32* height)
{
	int max_button_width = 0;
	int max_button_height = 0;

	{
		// If a header string is too wide, we might have to widen the calendar.
		// Don't include the week label, it will get 2 columns and fit anyway.
		SetHeaderFont use_header_font(font_info);
		const int header_count = 7;
		int start_column = ShouldIncludeWeekList() ? 2 : 0;
		for (int i = 0; i < header_count; i++)
		{
			OpWidgetString& header = GetHeaderString(i+start_column);
			int header_width = header.GetWidth() * 5/4; // add some padding
			max_button_width = MAX(max_button_width, header_width);
		}
	}

	{
		// The SetDayButtonFont class sets font while in scope
		OpWidgetString measure_string;
		SetDayButtonFont use_day_button_font(font_info);
		if (OpStatus::IsSuccess(measure_string.Set(UNI_L("99"), this)))
		{
			int calendar_number_width = measure_string.GetWidth()*3/2; // add some padding
			max_button_width = MAX(max_button_width, calendar_number_width);
			max_button_height = measure_string.GetHeight()*4/3; // add some padding
		}
	}
	int columns = DAYS_A_WEEK;
	if (ShouldIncludeWeekList())
		columns += 2;
	*width = columns * max_button_width + MONTHVIEW_PADDING*2; // Room for all days and the week
	*height = (NO_OF_WEEKS + 4)* max_button_height; // Room for header too
}

void OpMonthView::SetMinDay(DaySpec new_min)
{
	m_has_min = TRUE;
	m_min_day = new_min;
	MonthSpec min_month = {m_min_day.m_year, m_min_day.m_month };
	if (m_current_viewed_month.IsBefore(min_month))
	{
		SetViewedMonth(min_month);
	}
	else
	{
		// Have to recalculate enabled state for all days/weeks
		SetViewedMonth(m_current_viewed_month);
	}
	m_year_field->SetMinValue(new_min.m_year);
}

void OpMonthView::SetMaxDay(DaySpec new_max)
{
	m_has_max = TRUE;
	m_max_day = new_max;
	MonthSpec max_month = { new_max.m_year, new_max.m_month };
	if (m_current_viewed_month.IsAfter(max_month))
	{
		SetViewedMonth(max_month);
	}
	else
	{
		// Have to recalculate enabled state for all days/weeks
		SetViewedMonth(m_current_viewed_month);
	}
	m_year_field->SetMaxValue(new_max.m_year);
}

void OpMonthView::SetViewedMonth(MonthSpec month)
{
	m_current_viewed_month = month;

	if (m_has_min)
	{
		MonthSpec min_month = {m_min_day.m_year, m_min_day.m_month };
		if (m_current_viewed_month.IsBefore(min_month))
		{
			m_current_viewed_month = min_month;
		}
	}

	if (m_has_max &&
		(!m_has_min || !m_max_day.IsBefore(m_min_day)))
	{
		MonthSpec max_month = {m_max_day.m_year, m_max_day.m_month};
		if (m_current_viewed_month.IsAfter(max_month))
		{
			m_current_viewed_month = max_month;
		}
	}

	if (m_current_viewed_month.m_year < 1582)
	{
		m_current_viewed_month.m_year = 1582;
		m_current_viewed_month.m_month = 0;
		uni_char buf[15]; // ARRAY OK 2009-01-27 emil
		uni_itoa(1582, buf, 10);
		m_year_field->SetText(buf);
	}

	uni_char buf[15]; // ARRAY OK 2009-01-27 emil
	uni_itoa(m_current_viewed_month.m_year, buf, 10);
	m_year_field->SetText(buf);
	OP_ASSERT(m_current_viewed_month.m_month <= 11);

	const Str::LocaleString MONTH_NAME_ID[] =
	{
		Str::D_JANUARY,
		Str::D_FEBRUARY,
		Str::D_MARS,
		Str::D_APRIL,
		Str::D_MAY,
		Str::D_JUNE,
		Str::D_JULY,
		Str::D_AUGUST,
		Str::D_SEPTEMBER,
		Str::D_OCTOBER,
		Str::D_NOVEMBER,
		Str::D_DECEMBER
	};

	OpString month_name;
	GetLanguageString(MONTH_NAME_ID[m_current_viewed_month.m_month], month_name);
	m_month_name_field->SetText(month_name.CStr());

	DaySpec first_in_month = {m_current_viewed_month.m_year,
		m_current_viewed_month.m_month, 1 };
	int day_for_first_in_month = first_in_month.DayOfWeek();
	int days_in_past_month = m_current_viewed_month.PrevMonth().DaysInMonth();
	// Fill first row up to the first with the previous month
	int day_number = days_in_past_month;
	for (int column = day_for_first_in_month-1; column >= 0; column--, day_number--)
	{
		DayButtonInfo& day_info = m_day_button_info[0][column];
		day_info.m_day_or_week_number = day_number;
		day_info.m_clickable = FALSE;
		day_info.m_enabled = FALSE;
		day_info.m_special_color = (column == SUNDAY_COLUMN);
	}
	// The rest of the days
	int days_in_month = m_current_viewed_month.DaysInMonth();
	int pos = day_for_first_in_month;
	day_number = 1;
	BOOL inside_right_month = TRUE;
	while (pos < DAYS_A_WEEK*NO_OF_WEEKS)
	{
		int week = pos / DAYS_A_WEEK;
		int day_in_week = pos % DAYS_A_WEEK;
		DayButtonInfo& day_info = m_day_button_info[week][day_in_week];
		day_info.m_day_or_week_number = day_number;
		BOOL enabled = FALSE;
		if (ShouldAllowDayChoice() && inside_right_month)
		{
			DaySpec day = { m_current_viewed_month.m_year,
								 m_current_viewed_month.m_month,
								 day_number};
			enabled = IsLegalValue(day);
		}
		day_info.m_clickable = enabled && !m_readonly;
		day_info.m_enabled = enabled;
		day_info.m_special_color = (day_in_week == SUNDAY_COLUMN);
		pos++;
		day_number++;
		if (day_number > days_in_month)
		{
			// In next month
			day_number = 1;
			inside_right_month = FALSE;
		}
	}

	// The weeks
	DaySpec week_number_sample = first_in_month;
	for (int week = 0; week < NO_OF_WEEKS; week++)
	{
		WeekSpec week_with_year = week_number_sample.GetWeek();
		int week_number = week_with_year.m_week;

		DayButtonInfo& week_info = m_week_button_info[week];
		week_info.m_day_or_week_number = week_number;
		BOOL enabled = ShouldAllowWeekChoice() && IsLegalValue(week_with_year);
		week_info.m_clickable = enabled && !m_readonly;
		week_info.m_enabled = enabled;
		week_info.m_special_color = FALSE;

		// Calculate a day in the next week
		week_number_sample.m_day+=7;
		if (week_number_sample.m_day > days_in_month)
		{
			week_number_sample.m_day -= days_in_month;
			week_number_sample.m_month++;
			if (week_number_sample.m_month == 12)
			{
				week_number_sample.m_month = 0;
				week_number_sample.m_year++;
			}
		}
	}

	BOOL enable_prev_button = TRUE;
	if (m_has_min)
	{
		enable_prev_button = m_min_day.IsBefore(m_current_viewed_month.FirstDay());
	}
	m_prev_month_button->SetEnabled(enable_prev_button);
	BOOL enable_next_button = TRUE;
	if (m_has_max)
	{
		enable_next_button = m_max_day.IsAfter(m_current_viewed_month.LastDay());
	}

	m_next_month_button->SetEnabled(enable_next_button);

	InvalidateAll();
}


void OpMonthView::SetEnabled(BOOL enabled)
{
	for (int week = 0; week < NO_OF_WEEKS; week++)
	{
		DayButtonInfo& week_info = m_week_button_info[week];
		week_info.m_enabled = enabled && week_info.m_clickable;
		for (int day_of_week = 0; day_of_week < DAYS_A_WEEK; day_of_week++)
		{
			DayButtonInfo& day_info = m_day_button_info[week][day_of_week];
			day_info.m_enabled = enabled && day_info.m_clickable;
		}
	}
}

void OpMonthView::SetReadOnly(BOOL readonly)
{
	if (readonly != m_readonly)
	{
		m_readonly = readonly;
		// reinit
		SetViewedMonth(m_current_viewed_month);
		//m_clear_button->SetEnabled(!readonly);
		m_today_button->SetEnabled(!readonly);
	}
}

INT32 OpMonthView::GetYearFieldPreferredSize(int height)
{
	// The same width as 6 letters (4 letters + spinner)
	OpWidgetString measure_string;
	int width = 0;
	// The SetDayButtonFont sets the font while in scope
	SetDayButtonFont use_day_button_font(font_info);
	if (OpStatus::IsSuccess(measure_string.Set(UNI_L("9999W"), this)))
	{
		width = measure_string.GetWidth();
	}
	width += m_year_field->GetExtraWidth(height);

	return width;
}

void OpMonthView::OnResize(INT32* new_w, INT32* new_h)
{
	// There is 1 border around the widget
	int available_w = *new_w - MONTHVIEW_PADDING*2;
	int available_h = *new_h - MONTHVIEW_PADDING*2;
	int y = MONTHVIEW_PADDING;
	int next_x = MONTHVIEW_PADDING;

	int control_margin = 2; // Margin between month and year
	int days_top_bottom_padding = 5;

	INT32 button_width = 0;
	INT32 button_height = 0;
	m_prev_month_button->GetPreferedSize(&button_width, &button_height, 0, 0);
	button_width = button_height; // Make button a square
	//button_height -= 2; // Just a quick hunch. Better than the old code and prefered size is a too big.

	INT32 year_width = GetYearFieldPreferredSize(button_height);
	year_width = MIN(year_width, available_w / 2);
	INT32 month_width = available_w - button_width * 2 - year_width - control_margin;

	m_prev_month_button->SetRect(OpRect(next_x, y, button_width, button_height));
	next_x += button_width;

	m_month_name_field->SetRect(OpRect(next_x, y, month_width, button_height));
	next_x += month_width;

	m_next_month_button->SetRect(OpRect(next_x, y, button_width, button_height));
	next_x += button_width + control_margin;

	m_year_field->SetRect(OpRect(next_x, y, year_width, button_height));
	y += button_height + days_top_bottom_padding;

	int days_height = available_h - button_height * 2 - days_top_bottom_padding * 2;
	m_calendar_area.Set(MONTHVIEW_PADDING, y, available_w, days_height);
	LayoutCalendarAndWeek(m_calendar_area);

	int btn_w = available_w / 2;
	y = MONTHVIEW_PADDING + available_h - button_height;
	//m_clear_button->SetRect(OpRect(MONTHVIEW_PADDING, y, btn_w, button_height));
	m_today_button->SetRect(OpRect(MONTHVIEW_PADDING + btn_w, y, available_w - btn_w, button_height));
}

void OpMonthView::LayoutCalendarAndWeek(const OpRect& area)
{
	const BOOL include_weeks = ShouldIncludeWeekList();
	const int column_count = include_weeks ? (DAYS_A_WEEK + 2) : DAYS_A_WEEK;

	// A button that has two letters
	int button_width = area.width / column_count;
	int row_height = area.height / (NO_OF_WEEKS + 1);
	int y = area.y + row_height; ///< add room for header

	const int month_x_offset = include_weeks ? area.x + 2 * button_width - 2 : area.x;

	for (int week = 0; week < NO_OF_WEEKS; week++)
	{
		if (include_weeks)
		{
			DayButtonInfo& week_info = m_week_button_info[week];
			OpRect week_pos(area.x+button_width/4, y, button_width, row_height);
			week_info.m_pos = week_pos;
		}

		for (int day_of_week = 0; day_of_week < DAYS_A_WEEK; day_of_week++)
		{
			OpRect button_pos(month_x_offset + day_of_week*button_width, y,
							  button_width, row_height);
			m_day_button_info[week][day_of_week].m_pos = button_pos;
		}
		y += row_height;
	}
}

void OpMonthView::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (focus)
	{
#ifdef _DEBUG
//		Debug::SystemDebugOutput(UNI_L("OpMonthView::OnFocus(TRUE, ...) called\n"));
#endif // _DEBUG
		if (!m_focus_on_day && !m_focus_on_week)
		{
			SetFocusToFirstWidget(reason);
		}
	}
}

int OpMonthView::GetHeaderCount() const
{
	return ShouldIncludeWeekList() ? (DAYS_A_WEEK + 2) : DAYS_A_WEEK;
}

BOOL OpMonthView::IsHoveredWeek(int row)
{
	if (m_mode == MONTHVIEW_MODE_MONTH)
	{
		return m_hovered_row != -1;
	}

	return row == m_hovered_row;
}

BOOL OpMonthView::IsHoveredDay(int row, int column)
{
	if (m_mode == MONTHVIEW_MODE_MONTH)
	{
		return m_hovered_row != -1;
	}

	if (m_mode == MONTHVIEW_MODE_WEEK)
	{
		return row == m_hovered_row;
	}

	return row == m_hovered_row && column == m_hovered_column;
}

BOOL OpMonthView::IsSelectedDay(int row, int column)
{
	DaySpec ds = GetDaySpecForButton(row, column);
	if (m_mode == MONTHVIEW_MODE_WEEK)
		return ds.GetWeek().m_year == m_last_clicked_week.m_year &&
				ds.GetWeek().m_week == m_last_clicked_week.m_week;

	DaySpec selected_ds;
	if (!GetLastSelectedDay(selected_ds))
		return FALSE;
	if (m_mode == MONTHVIEW_MODE_MONTH)
		return (ds.m_year == selected_ds.m_year &&
				ds.m_month == selected_ds.m_month);
	else if (m_mode == MONTHVIEW_MODE_DAY)
		return (ds.m_year == selected_ds.m_year &&
				ds.m_month == selected_ds.m_month &&
				ds.m_day == selected_ds.m_day);
	return FALSE;
}

void OpMonthView::OnPaint(OpWidgetPainter* widget_painter,
						  const OpRect &paint_rect)
{
	// Background and similar stuff:
	vis_dev->SetColor(GetBackgroundColor(GetInfo()->GetSystemColor(IsEnabled() ? OP_SYSTEM_COLOR_BACKGROUND : OP_SYSTEM_COLOR_BACKGROUND_DISABLED)));
	vis_dev->FillRect(GetBounds());

	// Border (Fix: draw css border)
	vis_dev->SetColor(0, 0, 0);
	vis_dev->DrawRect(GetBounds());

	UINT32 fgcol = GetForegroundColor(GetInfo()->GetSystemColor(IsEnabled() ? OP_SYSTEM_COLOR_TEXT : OP_SYSTEM_COLOR_TEXT_DISABLED), 0);

	{ // scope for SetHeaderFont
		SetHeaderFont use_header_font(font_info);

		int row_height = m_calendar_area.height / (NO_OF_WEEKS + 1);
		OpRect top_part_rect(m_calendar_area.x, m_calendar_area.y, m_calendar_area.width, row_height);

		// "Week" header
		if (ShouldIncludeWeekList())
		{
			OpWidgetString& header = GetHeaderString(0);
			OpRect r(m_week_button_info[0].m_pos.x, m_calendar_area.y, m_week_button_info[0].m_pos.width, row_height);
			r.x -= 5; // Give it some extra space. There is a empty column next to it anyway.
			r.width += 10;
			//vis_dev->DrawRect(r);
			header.Draw(r, vis_dev, fgcol);
		}

		// Mon, Tue, Wed...
		int start_column = ShouldIncludeWeekList() ? 2 : 0;
		for (int day_of_week = 0; day_of_week < DAYS_A_WEEK; day_of_week++)
		{
			const DayButtonInfo& day_info = m_day_button_info[0][day_of_week];
			OpRect r(day_info.m_pos.x, m_calendar_area.y, day_info.m_pos.width, row_height);

			OpWidgetString& header = GetHeaderString(day_of_week + start_column);
			//vis_dev->DrawRect(r);
			header.Draw(r, vis_dev, fgcol);
		}
	}

	{ // Scope for SetDayButtonFont
		SetDayButtonFont use_day_button_font(font_info);

		for (int week = 0; week < NO_OF_WEEKS; week++)
		{
			if (ShouldIncludeWeekList())
			{
				const DayButtonInfo& week_info = m_week_button_info[week];
				BOOL focused = GetFocused() == this &&
					 m_focus_on_week &&
					 week == m_focused_week_no;
				if (OpStatus::IsMemoryError(DrawDate(week_info, TRUE, focused, IsHoveredWeek(week), FALSE, fgcol)))
				{
					ReportOOM();
					return; // abort painting
				}
			}
			for (int day_of_week = 0; day_of_week < DAYS_A_WEEK; day_of_week++)
			{
				const DayButtonInfo& day_info = m_day_button_info[week][day_of_week];
				BOOL focused = GetFocused() == this &&
					 m_focus_on_day &&
					 week == m_focused_week_no &&
					 day_of_week == m_focused_day_in_week_no;
				OP_STATUS status = DrawDate(day_info, FALSE, focused, IsHoveredDay(week, day_of_week), IsSelectedDay(week, day_of_week), fgcol);
				if (OpStatus::IsMemoryError(status))
				{
					ReportOOM();
					return; // abort painting
				}
			}
		}
	}

	// Now the child widgets, i.e. the buttons and text fields, will be painted
	AdjustFontSizes();
}

void OpMonthView::AdjustFontSizes()
{
	OpWidget* const widgets[] = {
		m_prev_month_button,
		m_next_month_button,
		//m_clear_button,
		m_today_button,
		m_month_name_field,
		m_year_field
	};

	const int widget_count = sizeof(widgets) / sizeof(*widgets);
	const int font_size = font_info.size; // font_info.size * 4 / 5
	for (int i = 0; i < widget_count; i++)
	{
		OpWidget& widget = *widgets[i];
		widget.SetFontInfo(NULL, font_size,
			widget.font_info.italic,
			widget.font_info.weight,
			JUSTIFY_CENTER,
			widget.font_info.char_spacing_extra);
	}
}

void OpMonthView::OnChange(OpWidget *widget, BOOL changed_by_mouse /*= FALSE*/)
{
	if (widget == m_year_field)
	{
		int new_year = 0;
		if (m_year_field->HasValue() && m_year_field->GetIntValue(new_year))
		{
			MonthSpec new_month = m_current_viewed_month;
			new_month.m_year = new_year;
			SetViewedMonth(new_month);
		}
	}
}


void OpMonthView::OnClick(OpWidget *object, UINT32 id)
{
	// Assume that it wasn't
	m_last_click_was_on_clear = FALSE;

	// New value (or no value)
	/*if (object == m_clear_button)
	{
		m_last_click_was_on_clear = TRUE;
		if (listener)
		{
			listener->OnClick(this);
		}
	}
	else */if (object == m_today_button)
	{
		m_last_clicked_day = GetToday();
		m_last_clicked_week = m_last_clicked_day.GetWeek();
		InvalidateAll();
		if (listener)
		{
			listener->OnClick(this);
		}
	}
	else if (object == m_prev_month_button)
	{
		SetViewedMonth(m_current_viewed_month.PrevMonth());
	}
	else if (object == m_next_month_button)
	{
		SetViewedMonth(m_current_viewed_month.NextMonth());
	}
}

DaySpec OpMonthView::GetDaySpecForButton(int row, int column)
{
	int week = row, day_of_week = column;
	int offset = week*DAYS_A_WEEK + day_of_week;
	DaySpec first_in_month = {m_current_viewed_month.m_year,
		m_current_viewed_month.m_month, 1 };
	offset -= first_in_month.DayOfWeek();
	MonthSpec month;
	int day;
	if (offset < 0)
	{
		// Prev month
		month = m_current_viewed_month.PrevMonth();
		day = month.DaysInMonth() + 1 + offset;
	}
	else
	{
		int days_in_month = m_current_viewed_month.DaysInMonth();
		if (offset >= days_in_month)
		{
			// Next month
			month = m_current_viewed_month.NextMonth();
			day = offset - days_in_month + 1;
		}
		else
		{
			month = m_current_viewed_month;
			day = offset + 1;
		}
	}
	DaySpec ds = { month.m_year, month.m_month, day };
	return ds;
}

void OpMonthView::ProcessDayClick(BOOL clicked_is_enabled,
								  int week, int day_of_week,
								  BOOL week_was_clicked, BOOL up)
{
	if (clicked_is_enabled)
	{
		if (week_was_clicked)
		{
			MoveFocusToWeekButton(week);
		}
		else
		{
			MoveFocusToDayButton(week, day_of_week);
		}
	}

	DaySpec clicked = GetDaySpecForButton(week, day_of_week);
	if (!week_was_clicked)
	{
		if (clicked.Month() == m_current_viewed_month.PrevMonth())
		{
			if (up)
				SetViewedMonth(clicked.Month());
			return;
		}
		if (clicked.Month() == m_current_viewed_month.NextMonth())
		{
			if (up)
				SetViewedMonth(clicked.Month());
			return;
		}
	}

	if (clicked_is_enabled)
	{
		// FIX: Don't really change anything unless we release the button on the same date. (Or rewrite the entire code to make use of OpButton instead)
		m_last_clicked_week = clicked.GetWeek();

		if (!week_was_clicked)
		{
			m_last_clicked_day = clicked;
		}

		if (listener && up)
		{
			listener->OnClick(this);
		}
	}
}

void OpMonthView::SetLastSelectedDay(const DaySpec& val)
{
	m_last_clicked_day = val;
	m_last_clicked_week = m_last_clicked_day.GetWeek();
	InvalidateAll();
}

void OpMonthView::SetLastSelectedWeek(const WeekSpec& val)
{
	m_last_clicked_week = val;
	InvalidateAll();
}

BOOL OpMonthView::GetLastSelectedDay(DaySpec& return_val)
{
	if (m_last_clicked_day.m_year > 0)
	{
		return_val = m_last_clicked_day;
		return TRUE;
	}

	return FALSE;
}


BOOL OpMonthView::GetLastSelectedWeek(WeekSpec& return_val)
{
	if (m_last_clicked_week.m_year > 0)
	{
		return_val = m_last_clicked_week;
		return TRUE;
	}

	return FALSE;
}


OpWidgetString& OpMonthView::GetHeaderString(int i)
{
	OP_ASSERT(i >= 0 && i <= 8);
	OP_ASSERT(ShouldIncludeWeekList() || i < DAYS_A_WEEK);

	// 0 is "Week", 2 is empty and 2-8 are day names.
	const Str::LocaleString headers[9] =
	{
		Str::S_CALENDAR_SHORT_HEADER_WEEK,
		Str::S_NOT_A_STRING,
		Str::S_CALENDAR_SHORT_HEADER_MONDAY,
		Str::S_CALENDAR_SHORT_HEADER_TUESDAY,
		Str::S_CALENDAR_SHORT_HEADER_WEDNESDAY,
		Str::S_CALENDAR_SHORT_HEADER_THURSDAY,
		Str::S_CALENDAR_SHORT_HEADER_FRIDAY,
		Str::S_CALENDAR_SHORT_HEADER_SATURDAY,
		Str::S_CALENDAR_SHORT_HEADER_SUNDAY,
	};
	Str::LocaleString header_str_id = headers[ShouldIncludeWeekList() ? i : i+2];
	OpString header_str;
	if (header_str_id != Str::S_NOT_A_STRING)
	{
		TRAPD(status, g_languageManager->GetStringL(header_str_id, header_str));
		OP_ASSERT(OpStatus::IsSuccess(status)); // Missing ui string
		// XXX No way to propagate errors.
		OpStatus::Ignore(status);
	}
	m_header_string.Set(header_str.CStr(), this);
	return m_header_string;
}

/** ***********************************************************************
**
**	OnInputAction
**
************************************************************************ */
BOOL OpMonthView::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
# if 0
		case OpInputAction::ACTION_CLEAR:
		{
			// XXX Is this used?
			m_last_clicked_day.m_year = -1;
			return TRUE;
		}
# endif // 0

		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			switch (action->GetChildAction()->GetAction())
			{
				case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
				{
					OpKey::Code key = static_cast<OpKey::Code>(action->GetChildAction()->GetActionData());
					switch (key)
					{
#if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED
					case OP_KEY_DOWN:
					case OP_KEY_UP:
					case OP_KEY_LEFT:
					case OP_KEY_RIGHT:
						MoveFocusInDirection(key);
						return TRUE;
#endif // defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED

#if defined OP_KEY_SPACE_ENABLED || defined OP_KEY_ENTER_ENABLED
#ifdef OP_KEY_SPACE_ENABLED
					case OP_KEY_SPACE:
#endif // OP_KEY_SPACE_ENABLED
#ifdef OP_KEY_ENTER_ENABLED
					case OP_KEY_ENTER:
#endif // OP_KEY_ENTER_ENABLED
						return ActivateFocusedPart();
#endif // OP_KEY_SPACE || OP_KEY_ENTER
					}
				}
			}

			return FALSE;
		}
	}

	return FALSE;
}

BOOL OpMonthView::ActivateFocusedPart()
{
	if (GetFocused() == this && !m_readonly && IsEnabled())
	{
		if (m_focus_on_day || m_focus_on_week)
		{
			ProcessDayClick(TRUE, /* clicked_is_focused */
							m_focused_week_no, m_focused_day_in_week_no,
							m_focus_on_week/* was_week_click */, FALSE);
			ProcessDayClick(TRUE, /* clicked_is_focused */
							m_focused_week_no, m_focused_day_in_week_no,
							m_focus_on_week/* was_week_click */, TRUE);
			return TRUE;
		}
	}
	return FALSE;
}

void OpMonthView::SetFocusToFirstWidget(FOCUS_REASON reason)
{
#ifdef _DEBUG
//	Debug::SystemDebugOutput(UNI_L("OpMonthView::SetFocusToFirstWidget called\n"));
#endif // _DEBUG
	if (m_prev_month_button->IsEnabled())
	{
		m_prev_month_button->SetFocus(reason);
	}
	else
	{
		m_year_field->SetFocus(reason);
	}
}

void OpMonthView::SetFocusToLastWidget(FOCUS_REASON reason)
{
	int pos = NO_OF_WEEKS * DAYS_A_WEEK - 1;
	do
	{
		int week_no = pos/DAYS_A_WEEK;
		int day_in_week_no = pos%DAYS_A_WEEK;
		if (m_day_button_info[week_no][day_in_week_no].m_enabled)
		{
			MoveFocusToDayButton(week_no, day_in_week_no);
			return;
		}
		pos--;
	}
	while (pos >= 0);

	if (m_next_month_button->IsEnabled())
	{
		m_next_month_button->SetFocus(reason);
		return;
	}

	m_year_field->SetFocus(reason);
}

BOOL OpMonthView::HasFocusInWidget()
{
	OpInputContext* context = g_input_manager->GetKeyboardInputContext();
	while (context)
	{
		if (context == this)
		{
			return TRUE;
		}
		context = context->GetParentInputContext();
	}
	return FALSE;
}

void OpMonthView::SetToWeekMode()
{
	m_mode = MONTHVIEW_MODE_WEEK;
	// reinit
	SetViewedMonth(m_current_viewed_month);
}

void OpMonthView::SetToMonthMode()
{
	m_mode = MONTHVIEW_MODE_MONTH;
	// reinit
	SetViewedMonth(m_current_viewed_month);
}

#if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED
void OpMonthView::MoveFocusInDirection(int key)
{
	OpWidget* currently_focused = GetFocused();
	if (!currently_focused)
	{
		return;
	}

	if (currently_focused == this)
	{
		int week_no = m_focused_week_no;
		if (m_focus_on_week)
		{
			switch (key)
			{
			case OP_KEY_UP:
					week_no-=2;
					// fallthrough
			case OP_KEY_DOWN:
					week_no++;
					MoveFocusToWeekButton(week_no);
					break;
			case OP_KEY_RIGHT:
					MoveFocusToDayButton(week_no, 0);
			}
			return;
		}

		OP_ASSERT(m_focus_on_day);
		int day_no = m_focused_day_in_week_no;
		switch (key)
		{
		case OP_KEY_UP:
				week_no--;
				break;
		case OP_KEY_DOWN:
				week_no++;
				break;
		case OP_KEY_RIGHT:
				day_no++;
				break;
		case OP_KEY_LEFT:
				day_no--;
				break;
		}

		MoveFocusToDayButton(week_no, day_no);
		return;
	}

	OpWidget* first_row_widget[] = {
		//m_clear_button,
		m_prev_month_button,
		m_month_name_field,
		m_next_month_button,
		m_year_field
	};
	const int first_row_widget_count = sizeof(first_row_widget) /
		sizeof(*first_row_widget);

	for (int i = 0; i < first_row_widget_count; i++)
	{
		BOOL matches = FALSE;
		OpWidget* widget = currently_focused;  // The number widget is nested
		while (widget)
		{
			if (widget == first_row_widget[i])
			{
				matches = TRUE;
				break;
			}
			widget = widget->GetParent();
		}

		if (matches)
		{
			switch (key)
			{
			case OP_KEY_DOWN:
					MoveFocusToFirstDayButton();
					return;
			case OP_KEY_LEFT:
					i-=2;
					// fallthrough
			case OP_KEY_RIGHT:
					i++;
					if (i>= 0 && i < first_row_widget_count)
					{
						first_row_widget[i]->SetFocus(FOCUS_REASON_KEYBOARD);
						return;
					}
			}
			return;
		}
	}

	OP_ASSERT(FALSE);  // Not implemented
	return;
}
#endif // #if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED

void OpMonthView::MoveFocusToDayButton(int week_no, int day_no)
{
	OP_ASSERT(week_no >= -1 && week_no <= NO_OF_WEEKS);
	OP_ASSERT(day_no >= -1 && day_no <= DAYS_A_WEEK);
	if (week_no == -1)
	{
		// Focus the today button instead. Room for improvement.
		if (m_today_button->IsEnabled())
		{
			m_today_button->SetFocus(FOCUS_REASON_KEYBOARD);
		}
		return;
	}

	if (day_no == -1)
	{
		MoveFocusToWeekButton(week_no);
		return;
	}

	if (week_no == NO_OF_WEEKS ||
		day_no == DAYS_A_WEEK)
	{
		// Nothing below or to the right
		return;
	}

	DayButtonInfo day_info = m_day_button_info[week_no][day_no];
	if (day_info.m_enabled)
	{
		m_focus_on_day = TRUE;
		m_focus_on_week = FALSE;
		m_focused_week_no = week_no;
		m_focused_day_in_week_no = day_no;
		this->SetFocus(FOCUS_REASON_KEYBOARD);
		InvalidateAll();
	}
}

void OpMonthView::MoveFocusToWeekButton(int week_no)
{
	if (week_no == -1)
	{
		// Focus the clear button instead. Room for improvement.
		if (m_today_button->IsEnabled())
		{
			m_today_button->SetFocus(FOCUS_REASON_KEYBOARD);
		}
		return;
	}

	if (week_no == NO_OF_WEEKS)
	{
		// Nothing below
		return;
	}

	DayButtonInfo week_info = m_week_button_info[week_no];
	if (week_info.m_enabled)
	{
		m_focus_on_day = FALSE;
		m_focus_on_week = TRUE;
		m_focused_week_no = week_no;
		this->SetFocus(FOCUS_REASON_KEYBOARD);
		InvalidateAll();
	}
}


void OpMonthView::MoveFocusToFirstDayButton()
{
	int week_no;
	for (week_no = 0; week_no < NO_OF_WEEKS; week_no++)
	{
		for (int day_no = 0; day_no < DAYS_A_WEEK; day_no++)
		{
			DayButtonInfo& day_info = m_day_button_info[week_no][day_no];
			if (day_info.m_enabled)
			{
				MoveFocusToDayButton(week_no, day_no);
				return;
			}
		}
	}

	for (week_no = 0; week_no < NO_OF_WEEKS; week_no++)
	{
		DayButtonInfo& week_info = m_week_button_info[week_no];
		if (week_info.m_enabled)
		{
			MoveFocusToWeekButton(week_no);
			return;
		}
	}
}

static void DrawFocusRect(VisualDevice* vd, const OpRect& drawrect)
{
	int i;
	for(i = 1; i < drawrect.width - 1; i += 2)
	{
		vd->InvertRect(OpRect(i + drawrect.x, drawrect.y, 1, 1));
		vd->InvertRect(OpRect(i + drawrect.x, drawrect.y + drawrect.height - 1,
							  1, 1));
	}
	for(i = 1; i < drawrect.height - 1; i += 2)
	{
		vd->InvertRect(OpRect(drawrect.x, i + drawrect.y, 1, 1));
		vd->InvertRect(OpRect(drawrect.x + drawrect.width - 1, i + drawrect.y,
							  1, 1));
	}
}


OP_STATUS OpMonthView::DrawDate(const DayButtonInfo& button_info,
								BOOL is_week, BOOL draw_focused, BOOL draw_hovered, BOOL draw_selected, UINT32 fg_color)
{
	const OpRect& date_area = button_info.m_pos;

	int date_number /* or a string? */ = button_info.m_day_or_week_number;
	BOOL clickable = button_info.m_clickable;
	BOOL enabled = button_info.m_enabled;
	BOOL special_colour = button_info.m_special_color; /* red that is */

	if (draw_hovered && clickable && enabled)
	{
		vis_dev->SetColor(OpWidgetPainter::GetColorMulAlpha(fg_color, 0.2f));
		vis_dev->FillRect(date_area);
	}
	if (draw_selected)
	{
		vis_dev->SetColor(OpWidgetPainter::GetColorMulAlpha(fg_color, 0.2f));
		vis_dev->FillRect(date_area);
	}

	OP_ASSERT(!date_area.IsEmpty());
	OP_ASSERT(is_week || date_number <= 31 && date_number >= 1);
	OP_ASSERT(!is_week || date_number <= 53 && date_number >= 1);
	// Use WidgetString?
	OpWidgetString date;
	uni_char buf[15]; // ARRAY OK 2009-01-27 emil
	uni_itoa(date_number, buf, 10);
	OpWidget* widget_with_fontinfo = this;
	RETURN_IF_ERROR(date.Set(buf, widget_with_fontinfo));
	UINT32 color;
	if (special_colour)
		color = OP_RGB(255, 0, 0); // red
	else
		color = fg_color;

	if (!enabled)
		color = OpWidgetPainter::GetColorMulAlpha(color, 0.5);

	date.Draw(date_area, vis_dev, color);
	//vis_dev->DrawRect(date_area);

	if (draw_focused && HasFocusRect())
	{
		DrawFocusRect(vis_dev, date_area);
	}
	return OpStatus::OK;
}

BOOL OpMonthView::GetCoordinatesAtPos(const OpPoint &point, int& out_row, int& out_column, BOOL& was_week)
{
	for (int week_no = 0; week_no < NO_OF_WEEKS; week_no++)
	{
		DayButtonInfo& week_info = m_week_button_info[week_no];
		if (week_info.m_pos.Contains(point))
		{
			out_row = week_no;
			out_column = -1;
			was_week = TRUE;
			return TRUE;
		}

		for (int day_no = 0; day_no < DAYS_A_WEEK; day_no++)
		{
			DayButtonInfo& day_info = m_day_button_info[week_no][day_no];
			if (day_info.m_pos.Contains(point))
			{
				out_row = week_no;
				out_column = day_no;
				was_week = FALSE;
				return TRUE;
			}
		}
	}

	return FALSE;
}

void OpMonthView::OnMouseLeave()
{
	m_hovered_row = -1;
	m_hovered_column = -1;
	InvalidateAll();
}

/* virtual */
void OpMonthView::OnMouseMove(const OpPoint &point)
{
	int hovered_row;
	int hovered_column;
	BOOL is_hovering_week;

	BOOL has_changed = FALSE;

	if (GetCoordinatesAtPos(point, hovered_row, hovered_column, is_hovering_week))
	{
		if (is_hovering_week)
		{
			if (m_hovered_row != hovered_row || m_hovered_column != -1)
			{
				has_changed = TRUE;
			}
		}
		else if (m_hovered_row != hovered_row || m_hovered_column != hovered_column)
		{
			has_changed = TRUE;
		}
	}
	else if (m_hovered_row != -1 || m_hovered_column != -1)
	{
		hovered_row = -1;
		hovered_column = -1;
		has_changed = TRUE;
	}

	if (has_changed)
	{
		if (hovered_row != -1 && hovered_column != -1 &&
			!(m_day_button_info[hovered_row][hovered_column].m_clickable &&
			  m_day_button_info[hovered_row][hovered_column].m_enabled))
		{
			hovered_row = -1;
			hovered_column = -1;
		}
		m_hovered_row = hovered_row;
		m_hovered_column = hovered_column;
		InvalidateAll();
	}
}

void OpMonthView::ProcessClick(const OpPoint &point, MouseButton button, BOOL up)
{
	m_last_click_was_on_clear = FALSE;
	int week_no;
	int day_no;
	BOOL is_hovering_week;

	if (GetCoordinatesAtPos(point, week_no, day_no, is_hovering_week))
	{
		if (is_hovering_week)
		{
	 		DayButtonInfo& week_info = m_week_button_info[week_no];
			ProcessDayClick(week_info.m_clickable && week_info.m_enabled, week_no, 1, TRUE/* was_week_click */, up);
		}
		else
		{
 			DayButtonInfo& day_info = m_day_button_info[week_no][day_no];
			ProcessDayClick(day_info.m_clickable && day_info.m_enabled, week_no, day_no, FALSE /* was_week_click */, up);
		}
	}
}

void OpMonthView::OnMouseDown(const OpPoint &point,
							  MouseButton button,
							  UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1)
		ProcessClick(point, button, FALSE);
}

void OpMonthView::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1)
		ProcessClick(point, button, TRUE);
}

void OpMonthView::SetStep(double step_base, double step)
{
	m_has_step = TRUE;
	m_step_base = step_base;
	m_step = step;

	// Have to recalculate enabled state for all days/weeks
	SetViewedMonth(m_current_viewed_month);
}

BOOL OpMonthView::IsLegalValue(DaySpec day)
{
	OP_ASSERT(ShouldAllowDayChoice());
	OP_ASSERT(m_mode == MONTHVIEW_MODE_DAY || m_mode == MONTHVIEW_MODE_MONTH || m_mode == MONTHVIEW_MODE_WEEK);

	if (m_mode == MONTHVIEW_MODE_WEEK)
	{
		return IsLegalValue(day.GetWeek());
	}

	OP_ASSERT(m_mode == MONTHVIEW_MODE_DAY || m_mode == MONTHVIEW_MODE_MONTH);
	if (m_has_max && day.IsAfter(m_max_day))
	{
		return FALSE;
	}

	if (m_has_min && day.IsBefore(m_min_day))
	{
		return FALSE;
	}

	if (m_has_step && m_step > 1.0) // Steps less than or equal to 1.0 makes every day allowed
	{
		if (m_mode == MONTHVIEW_MODE_MONTH)
		{
			MonthSpec month = { day.m_year, day.m_month };
            double month_double = month.AsDouble();
			OP_ASSERT(month_double == static_cast<int>(month_double));
			double distance = month_double - m_step_base;
			double steps = distance/m_step;
			int int_steps = static_cast<int>(steps);
			if (int_steps != steps) // Was it an integer?
			{
				return FALSE;
			}
		}
		else
		{
			OP_ASSERT(m_mode == MONTHVIEW_MODE_DAY);
			double day_double = day.AsDouble();
			OP_ASSERT(day_double == static_cast<int>(day_double));
			double distance = day_double - m_step_base;
			double steps = distance/m_step;
			int int_steps = static_cast<int>(op_floor(steps));
			if (int_steps != steps) // Was it an integer?
			{
				int next_legal_step = int_steps + 1;
				double allowed = next_legal_step*m_step + m_step_base;
				OP_ASSERT(allowed > day_double);
				if (allowed-day_double >= 1.0)
				{
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}

BOOL OpMonthView::IsLegalValue(WeekSpec week)
{
	OP_ASSERT(m_mode == MONTHVIEW_MODE_WEEK);
	if (m_has_max && week.GetFirstDay().IsAfter(m_max_day))
	{
		return FALSE;
	}

	if (m_has_min && week.GetLastDay().IsBefore(m_min_day))
	{
		return FALSE;
	}

	if (m_has_step && m_step > 1.0) // Steps less than or equal to 1.0 makes every week allowed
	{
		double week_double = week.AsDouble();
		OP_ASSERT(week_double == static_cast<int>(week_double));
		double distance = week_double - m_step_base;
		double steps = distance/m_step;
		int int_steps = static_cast<int>(steps);
		if (int_steps != steps) // Was it an integer?
		{
			return FALSE;
		}
	}

	return TRUE;
}

//static
DaySpec OpMonthView::GetToday()
{
	time_t current_time = op_time(NULL);
	struct tm* tm_time = op_localtime( &current_time ); /* Convert to local time. */
	DaySpec today = {tm_time->tm_year+1900, tm_time->tm_mon, tm_time->tm_mday};
	return today;
}

void OpMonthView::UpdateTodayButton()
{
	DaySpec today = GetToday();
	BOOL enabled = !m_readonly && IsLegalValue(today);
	if (enabled != m_today_button->IsEnabled())
	{
		m_today_button->SetEnabled(enabled);
	}
}
