/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_TIME_WIDGET_H
#define OP_TIME_WIDGET_H

#include "modules/widgets/OpWidget.h"

#define PATTERN_EDIT

#ifdef PATTERN_EDIT
#include "modules/widgets/OpEdit.h"
#endif // PATTERN_EDIT

#include "modules/forms/datetime.h"

class OpEdit;
class OpNumberEdit;
class OpSpinner;

/** OpTime - Widget for inputting a time. Has a textfield and spinbutton */

class OpTime : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:
	OpTime();

public:
	static OP_STATUS Construct(OpTime** obj);

	// Override base class
    OP_STATUS GetText(OpString &str);
	OP_STATUS SetText(const uni_char* text);

	virtual void SetReadOnly(BOOL readonly);
	virtual void SetEnabled(BOOL enabled);

	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	OP_STATUS SetValue(TimeSpec date_time);

	/**
	 * Returns true if a value was returned.
	 */
	BOOL GetTime(TimeSpec& out_value) const;

	/**
	 * Returns TRUE if there is an input value.
	 */
	BOOL HasValue() const;

	void SetMinValue(TimeSpec new_min) { SetMinValueInternal(TRUE, new_min); }
	void SetMaxValue(TimeSpec new_max) { SetMaxValueInternal(TRUE, new_max); }

	/**
	 * @param step_value Step in seconds.
	 */
	void SetStep(TimeSpec step_base, double step_value) { SetStepInternal(TRUE, step_base, step_value*1000.0); }
	/**
	 * @param step_value Step in milliseconds.
	 */
	void SetStepMS(TimeSpec step_base, double step_value) { SetStepInternal(TRUE, step_base, step_value); }

	/** Clear the limitation of minimal time. */
	void ClearMinValue() { SetMinValueInternal(FALSE, TimeSpec()); }

	/** Clear the limitation of maximal time. */
	void ClearMaxValue() { SetMaxValueInternal(FALSE, TimeSpec()); }

	/** Clear the step value. */
	void ClearStep() { SetStepInternal(FALSE, TimeSpec(), 0.0); }

	// == Hooks ======================
	void OnResize(INT32* new_w, INT32* new_h);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	BOOL OnMouseWheel(INT32 delta,BOOL vertical);
	void EndChangeProperties();

	// Implementing the OpTreeModelItem interface
	//virtual Type		GetType() {return WIDGET_TYPE_TIME;}
	//virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_TIME || OpWidget::IsOfType(type); }

	// OpWidgetListener
	virtual void OnChangeWhenLostFocus(OpWidget *widget);
	virtual void OnClick(OpWidget *object, UINT32 id);

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void SetValue(INT32 value) {}
#endif // QUICK

	BOOL OnInputAction(OpInputAction* action);

	enum TimeFieldPrecision
	{
		TIME_PRECISION_MINUTES,				///< Minute precision
		TIME_PRECISION_SECONDS,				///< Second precision
		TIME_PRECISION_SUBSECOND,			///< 3 fraction digits
		TIME_PRECISION_HUNDRETHS = TIME_PRECISION_SUBSECOND,///< deprecated
		TIME_PRECISION_SUBSECOND2,			///< 6 fraction digits
		TIME_PRECISION_SUBSECOND3			///< 9 fraction digits
	};

	void SetTimePrecision(TimeFieldPrecision precision);
private:
#ifdef PATTERN_EDIT
	const uni_char* GetEditPattern() const;
	OP_STATUS SetEmptyValue();

	OpEdit*	m_hour_field;
#else
	OpNumberEdit*	m_hour_field;
	OpNumberEdit*	m_minute_field;
	OpEdit*			m_colon1_field;

	signed char m_seconds;
	signed char m_hundreths;
#endif // PATTERN_EDIT

	void GetPreferredSpinnerSize(int widget_height, INT32* spinner_width, INT32* spinner_height);

	/**
	 * Adjusts the unadjusted_value to be within min/max if those are set.
	 */
	void MinMaxAdjust(int delta, TimeSpec& unadjusted_value) const;

	/**
	 * Enables/disables spinner buttons depending on current value and min/max.
	 */
	void UpdateButtonState();

	void SetMinValueInternal(BOOL enable, TimeSpec new_min);
	void SetMaxValueInternal(BOOL enable, TimeSpec new_max);
	void SetStepInternal(BOOL enable, TimeSpec step_base, double step_value_milliseconds);

	OpSpinner* m_spinner;

	TimeSpec m_min_value;
	TimeSpec m_max_value;
	TimeSpec m_step_base;
	double m_step_value_milliseconds;
	BOOL m_has_min_value;
	BOOL m_has_max_value;
	BOOL m_has_step;

	TimeFieldPrecision m_precision;

	TimeSpec m_last_set_full_time;

	BOOL m_readonly;

#if 0 // Doesn't work inside OpDateTime. :-(
	// We don't want to do magical things with the time field when the user just
	// temporarily switches to another program/document so we keep track of why
	// focus was lost.
public:
	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
private:
	BOOL m_last_focus_lost_was_release;
	BOOL m_pending_changed_focus_lost;
#endif
};

#endif // OP_TIME_WIDGET_H
