/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_MONTH_VIEW_EDIT_H
#define OP_MONTH_VIEW_EDIT_H

#include "modules/widgets/OpWidget.h"
#include "modules/forms/datetime.h"

#define NO_OF_WEEKS   6
#define DAYS_A_WEEK   7
#define SUNDAY_COLUMN 6

class OpNumberEdit;
class OpButton;
class OpCalendar;

/** OpMonthView - Has a grid of buttons for each day per month. There are buttons to change month. */

class OpMonthView : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:

	OpMonthView();

public:
	static OP_STATUS Construct(OpMonthView** obj);

	void GetPreferedSize(INT32* width, INT32* height);

	void SetEnabled(BOOL enabled);
	void SetReadOnly(BOOL readonly);

	void SetViewedMonth(MonthSpec month);
	void SetMinMonth(MonthSpec new_min) { SetMinDay(new_min.FirstDay()); }
	void SetMaxMonth(MonthSpec new_max) { SetMaxDay(new_max.LastDay()); }
	void SetMinDay(DaySpec min_day);
	void SetMaxDay(DaySpec max_day);

	void SetLastSelectedDay(const DaySpec& val);
	void SetLastSelectedWeek(const WeekSpec& val);

	/**
	 * Returns TRUE if there's anything to return.
	 */
	BOOL GetLastSelectedDay(DaySpec& return_val);

	/**
	 * Returns TRUE if there's anything to return.
	 */
	BOOL GetLastSelectedWeek(WeekSpec& return_val);

	/**
	 * If last click was on the "clear" button.
	 */
	BOOL LastClickWasOnClear() { return m_last_click_was_on_clear; }

	// == Hooks ======================
	void OnResize(INT32* new_w, INT32* new_h);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

	// OpWidgetListener
//	virtual void OnChangeWhenLostFocus(OpWidget *widget);
	virtual void OnClick(OpWidget *object, UINT32 id);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);

	BOOL OnInputAction(OpInputAction* action);
	void OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	void SetFocusToFirstWidget(FOCUS_REASON reason);
	void SetFocusToLastWidget(FOCUS_REASON reason);

	void SetParentCalendar(OpCalendar* cal) { m_parent_calendar = cal; }

	// Used by the painter
	/**
	 * @param i 0 for the week and 1 to 7 for days.
	 */
	OpWidgetString& GetHeaderString(int i);
	int GetHeaderCount() const;

	/**
	 * Help method to use when this widget is used in a popup.
	 * 
	 * @see OpCalendar
	 */
	BOOL HasFocusInWidget();

	/**
	 * Sets the widget to week selection mode. This assumes 
	 * that the WeekSpec version of SetMax and SetMin is 
	 * used and that step is in weeks (with 1970-W01 being 0.0).
	 */
	void SetToWeekMode();
	/**
	 * Sets the widget to month selection mode. This assumes 
	 * that the MonthSpec version of SetMax and SetMin is 
	 * used and that step is in month (with 1970-01 being 0.0).
	 */
	void SetToMonthMode();

	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseMove(const OpPoint &point);

	virtual void OnMouseLeave();

	/**
	 * Sets certain steps as legal values. The step base is defined as
	 * number of days since 1970-01-01. Non integer values are
	 * legal. Step is number of days (default 1.0, less than 1.0 is
	 * not very meaningful in itself). If an available step truncated
	 * to integer matches a day, that day should be selectable.
	 */
	void SetStep(double step_base, double step);

	/** Get todays date */
	static DaySpec GetToday();

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows) {}
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point) {}
	virtual void OnMouseLeave(OpWidget *widget) {}
#endif // QUICK

private:
	OpCalendar* m_parent_calendar;

	void LayoutCalendarAndWeek(const OpRect& area);
#if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED
	void MoveFocusInDirection(int key);
#endif // #if defined OP_KEY_DOWN_ENABLED && defined OP_KEY_UP_ENABLED && defined OP_KEY_LEFT_ENABLED && defined OP_KEY_RIGHT_ENABLED
	void MoveFocusToFirstDayButton();
	void MoveFocusToWeekButton(int week_no);
	void MoveFocusToDayButton(int week_no, int day_no);
	void ProcessDayClick(BOOL clicked_is_enabled, int week, int day_of_week, BOOL week_was_clicked, BOOL up);
	void ProcessClick(const OpPoint &point, MouseButton button, BOOL up);
	void AdjustFontSizes();
	BOOL IsLegalValue(DaySpec day);
	BOOL IsLegalValue(WeekSpec day);
	/**
	 * Returns the favoured size if it is to be |height| pixels high.
	 */
	INT32 GetYearFieldPreferredSize(int height);
	BOOL ActivateFocusedPart();
	BOOL IsHoveredWeek(int row);
	BOOL IsHoveredDay(int row, int column);
	BOOL IsSelectedDay(int row, int column);
	DaySpec GetDaySpecForButton(int row, int column);

	/**
	 * Sets the enabled state on the today button.
	 */
	void UpdateTodayButton();

	BOOL GetCoordinatesAtPos(const OpPoint &point, int& out_row, int& out_column, BOOL& was_week);

	struct DayButtonInfo
	{
		OpRect m_pos;
		int m_day_or_week_number; // 1-31, or 1-53 if used for weeks
		BOOL m_clickable;
		BOOL m_enabled;
		BOOL m_special_color;
	};

	OP_STATUS DrawDate(const DayButtonInfo& button_info,
					   BOOL is_week, BOOL draw_focused,
					   BOOL draw_hovered, BOOL draw_selected, UINT32 fg_color);

	 // 6 weeks, 7 days a week
	DayButtonInfo m_day_button_info[NO_OF_WEEKS][DAYS_A_WEEK];
	DayButtonInfo m_week_button_info[NO_OF_WEEKS];

	//OpButton*	m_clear_button;
	OpButton*	m_today_button;
	OpButton*	m_prev_month_button;
	OpButton*	m_next_month_button;
	OpEdit*	m_month_name_field;
	OpNumberEdit* m_year_field;

	MonthSpec m_current_viewed_month;

	/** So that we can return a reference to something to the painter. */
	OpWidgetString m_header_string;

	DaySpec m_last_clicked_day;
	WeekSpec m_last_clicked_week;
	DaySpec m_max_day;
	DaySpec m_min_day;
	BOOL m_has_max;
	BOOL m_has_min;
	BOOL m_last_click_was_on_clear;

	BOOL m_focus_on_day;
	BOOL m_focus_on_week;
	int m_focused_week_no;
	int m_focused_day_in_week_no;

	BOOL m_has_step;
	double m_step_base;
	double m_step;

	int m_hovered_row; // -1 if nothing is hovered
	int m_hovered_column; // -1 if nothing is hovered

	OpRect m_calendar_area; ///< The area for the weeks and days grid

	enum MonthViewMode // controls step
	{
		MONTHVIEW_MODE_DAY,
		MONTHVIEW_MODE_WEEK,
		MONTHVIEW_MODE_MONTH
	};

	MonthViewMode m_mode;

	BOOL m_readonly;

	BOOL ShouldAllowDayChoice() const { return TRUE; }
	BOOL ShouldAllowWeekChoice() const { return m_mode == MONTHVIEW_MODE_WEEK; }
	BOOL ShouldIncludeWeekList() const { return m_mode == MONTHVIEW_MODE_WEEK; }
};

#endif // OP_MONTH_VIEW_EDIT_H
