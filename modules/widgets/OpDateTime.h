/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_DATE_TIME_H
#define OP_DATE_TIME_H

#include "modules/widgets/OpWidget.h"
#include "modules/forms/datetime.h"

/** OpDateTime - Widget for inputting a date and a time. Has a OpCalendar and OpTime widget. */

class OpTime;
class OpCalendar;

class OpDateTime : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:
	OpDateTime();
	virtual	~OpDateTime();

public:
	static OP_STATUS Construct(OpDateTime** obj);

	// Override base class
    OP_STATUS GetText(OpString &str);
	OP_STATUS SetText(const uni_char* text);

	void SetReadOnly(BOOL readonly);

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	OP_STATUS SetValue(DateTimeSpec date_time);

	/**
	 * Returns true if a value was returned.
	 */
	BOOL GetDateTime(DateTimeSpec& out_value) const;

	/**
	 * Returns TRUE if there is an input value.
	 */
	BOOL HasValue() const;

	void SetMinValue(DateTimeSpec new_min);
	void SetMaxValue(DateTimeSpec new_max);
	void SetIncludeTimezone(BOOL val);
	/**
	 * step = 0.0 disables step
	 *
	 * Unit: milliseconds.
	 */
	void SetStep(double step_base, double step);

	// == Hooks ======================
	void OnResize(INT32* new_w, INT32* new_h);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	void EndChangeProperties();

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_DATETIME;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_DATETIME || OpWidget::IsOfType(type); }

	// OpWidgetListener
//	virtual void OnChangeWhenLostFocus(OpWidget *widget);
//	virtual void OnClick(OpWidget *object, UINT32 id);
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse /*= FALSE */);

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void SetValue(INT32 value) {}
#endif // QUICK

private:
	OpCalendar*	m_calendar;
	OpTime*		m_time;
	OpEdit*		m_timezone_field;

	DateTimeSpec m_min_value;
	DateTimeSpec m_max_value;
	BOOL m_has_min_value;
	BOOL m_has_max_value;
	BOOL m_has_step;
	double m_step_base; // milliseconds
	double m_step; // milliseconds

	BOOL m_include_timezone;

	INT32 GetTimezoneWidth() const;
	void UpdateMinMaxStepForTime();
	void SetTimeStepForSpecificDate(DaySpec date);
};

#endif // OP_DATE_TIME_H
