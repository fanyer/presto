/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_CALENDAR_EDIT_H
#define OP_CALENDAR_EDIT_H

#include "modules/widgets/OpWidget.h"
#include "modules/forms/datetime.h"
#include "modules/hardcore/mh/messobj.h"
#ifdef PLATFORM_FONTSWITCHING
#include "modules/display/fontdb.h"
#endif // PLATFORM_FONTSWITCHING

/** OpCalendar - Widget for inputting a full date, a month or week. Shows the date and has a dropdownmenu to edit it. */

class OpCalendarWindow;

class OpCalendar : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:
	OpCalendar();
	virtual	~OpCalendar();

public:
	static OP_STATUS Construct(OpCalendar** obj);

private:
	enum CalendarMode
	{
		CALENDAR_MODE_MONTH,	///< Enter month
		CALENDAR_MODE_WEEK,		///< Enter week
		CALENDAR_MODE_DAY		///< Enter full date
	};

public:
	OP_STATUS GetText(OpString &str);
	OP_STATUS SetText(const uni_char* text);
	
	OP_STATUS GetLocalizedTextValue(OpString &str);

	void SetReadOnly(BOOL read_only);

	INT32 GetTextLength();

	virtual void OnScaleChanged()
	{
		ClosePopup();
	}

	void SetMinValue(DaySpec new_min) { OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY); SetMinValueInternal(new_min); }
	void SetMaxValue(DaySpec new_max) { OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY); SetMaxValueInternal(new_max); }
	void SetMinValue(WeekSpec new_min);
	void SetMaxValue(WeekSpec new_max);
	void SetMinValue(MonthSpec new_min);
	void SetMaxValue(MonthSpec new_max);
	BOOL HasValue() { return m_has_value; };
	DaySpec GetDaySpec() { OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY); return m_day_value; };
	OP_STATUS SetValue(DaySpec new_value)  { OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY); return SetValueInternal(new_value); }

	/**
	 * Sets certain steps as legal values. The step base is in day
	 * mode defined as number of days since 1970-01-01. Non integer values
	 * are legal. Step is number of days (default 1.0, less than 1.0
	 * is not very meaningful in itself). If an available step
	 * truncated to integer matches a day, that day should be
	 * selectable.
	 *
	 * In week mode it is the number of weeks and should be an integer.
	 *
	 * In month mode it is the number of months and should be an integer.
	 *
	 * A step value of 0.0 disables steps.
	 */
	void SetStep(double step_base, double step);

	/**
	 * Default is day mode.
	 */
	void SetToWeekMode() { OP_ASSERT(!m_has_value && !m_has_max_value && !m_has_min_value); m_calendar_mode = CALENDAR_MODE_WEEK; }
	void SetToMonthMode() { OP_ASSERT(!m_has_value && !m_has_max_value && !m_has_min_value); m_calendar_mode = CALENDAR_MODE_MONTH; }
	WeekSpec GetWeekSpec() { OP_ASSERT(m_calendar_mode == CALENDAR_MODE_WEEK); return m_week_value; }
	MonthSpec GetMonthSpec() { OP_ASSERT(m_calendar_mode == CALENDAR_MODE_MONTH); return m_month_value; }
	OP_STATUS SetValue(WeekSpec new_value);
	OP_STATUS SetValue(MonthSpec new_value);

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);


	// == Hooks ======================
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	void OnMove();
	BOOL OnInputAction(OpInputAction* action);
	virtual void OnKeyboardInputLost(OpInputContext* new_input_context, 
									 OpInputContext* old_input_context, 
									 FOCUS_REASON reason);

	void OnMonthViewLostFocus();

	BOOL IsHoveringButton()	{ return m_is_hovering_button; }

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_CALENDAR;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_CALENDAR || OpWidget::IsOfType(type); }

	virtual BOOL		IsParentInputContextAvailabilityRequired();


	// OpWidgetListener
	virtual void OnClick(OpWidget *object, UINT32 id);
#ifdef MOUSE_SUPPORT
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnMouseMove(const OpPoint &point);
	virtual void OnMouseLeave();
#endif

	// MessageObject method(s)
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef PLATFORM_FONTSWITCHING
	void SetPreferredCodePage(OpFontInfo::CodePage cp) { m_preferred_codepage = cp; }
#endif

	virtual const char*	GetInputContextName() {return "Calendar Widget";}

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void SetValue(INT32 value) {}
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point) {}
	virtual void OnMouseLeave(OpWidget *widget) {}
#endif // QUICK

private:
	void ToggleMonthView();
	void ClosePopup(BOOL immediately = FALSE);
	OP_STATUS SetValueInternal(DaySpec new_value);
	void SetMinValueInternal(DaySpec new_min);
	void SetMaxValueInternal(DaySpec new_max);

	void Layout();
	OpPoint ConvertToMonthViewWindow(OpPoint point);

	BOOL m_readonly;
	DaySpec m_min_value;
	DaySpec m_max_value;
	BOOL m_has_min_value;
	BOOL m_has_max_value;
	BOOL m_has_value;
	BOOL m_is_hovering_button;

	BOOL m_has_step;
	double m_step_base;
	double m_step;

	union
	{
		DaySpec m_day_value;
		WeekSpec m_week_value;
		MonthSpec m_month_value;
	};

	CalendarMode m_calendar_mode;

public:
	OpCalendarWindow* m_popup_window;

private:
#ifdef PLATFORM_FONTSWITCHING
	OpFontInfo::CodePage m_preferred_codepage;
#endif
};

#endif // OP_CALENDAR_EDIT_H
