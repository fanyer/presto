/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SLIDER_H
#define OP_SLIDER_H

#include "modules/widgets/OpWidget.h"

/** OpSlider is a horizontal or vertical slider (depending on its size).
	The value and the limits are double. */

class OpSlider : public OpWidget
#ifndef QUICK
			 , public OpWidgetListener
#endif // QUICK
{
protected:

	OpSlider();
	virtual	~OpSlider();

public:
	static OP_STATUS Construct(OpSlider** obj);

	// Override base class
	OP_STATUS GetText(OpString &str);
	OP_STATUS SetText(const uni_char* text);
	void SetValue(INT32 value) { SetValue((double)value); }
	INT32 GetValue() { return (INT32)GetNumberValue(); }

	void SetReadOnly(BOOL readonly) { m_read_only = readonly; }

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	void SetMax(double max) { m_max = max; }
	void SetMin(double min) { m_min = min; }
	double GetMax() const { return m_max; }
	double GetMin() const { return m_min; }
	int GetNumTickValues() const { return m_num_tick_values; }
	void SetValue(double val, BOOL force_no_onchange = TRUE);
	double GetNumberValue() { return m_drag_in_progress ? m_drag_value : m_current; }
	double GetSliderValue() { return m_current; }
	void SetStepBase(double step_base) { m_step_base = step_base; }
	double GetStepBase() { return m_step_base; }
	double GetStep() { return m_step; }
	void SetStep(double step) { m_step = step; }
	
	struct TICK_VALUE {
		double value; ///< Value of the tick
		BOOL snap;   ///< If the tick should be larger than the others (and snap)
	};
	/** Set a list of tick values, to be used instead of the default automatic ones. */
	void SetTickValues(int num_ticks, const TICK_VALUE *tick_values, double snap_threshold);

	/** Set if text should be drawn under the ticks. Currently only supporting integers, shown as percent. */
	void ShowTickLabels(BOOL show) { m_show_labels = show; InvalidateAll(); }
	BOOL GetShowTickLabels() { return m_show_labels; }

	/** Will be called when the value is changed (also the drag value which is changed during dragging).
		Should trig the listeners OnChange */
	virtual void HandleOnChange();

	// == Hooks ======================
	void OnFocus(BOOL focus, FOCUS_REASON reason);

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_SLIDER;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_SLIDER || OpWidget::IsOfType(type); }

	// OpWidgetListener
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseMove(const OpPoint &point);
	virtual void OnMouseLeave();
	virtual BOOL OnMouseWheel(INT32 delta,BOOL vertical);
	virtual void OnClick(OpWidget *object, UINT32 id);
//	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse /*= FALSE */);
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	BOOL OnInputAction(OpInputAction* action);

	BOOL IsPointInKnob(const OpPoint& point);
	BOOL IsValidDragEndPoint(const OpPoint& point);
	double SnapToValue(const OpPoint& point);

	BOOL IsHoveringKnob() { return m_is_hovering_knob; }
	BOOL IsPressingKnob() { return m_drag_in_progress; }
	BOOL IsHorizontal();

	/** Get the skin element to use for the knob element, override in derived classes if needed */
#ifdef SKIN_SUPPORT
	virtual const char* GetKnobSkin(BOOL horizontal) { return horizontal ? "Slider Horizontal Knob" : "Slider Vertical Knob"; }
	virtual const char* GetTrackSkin(BOOL horizontal) { return horizontal ? "Slider Horizontal Track" : "Slider Vertical Track"; }
#endif // SKIN_SUPPORT

#ifdef SELFTEST
	/**
	   sets up the track and knob without calling OnPaint
	 */
	void UpdateTrack(const OpRect& inner)
	{
		UpdateTrackPoints(inner);
		UpdateKnobRect(inner);
	}
#endif // SELFTEST

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point) {}
	virtual void OnMouseLeave(OpWidget *widget) {}
#endif // QUICK

private:
	/**
	   updates m_start_track and m_end_track based on inner - normally triggered from OnPaint
	 */
	void UpdateTrackPoints(const OpRect& inner);
	/**
	   updates m_knob_position based on inner - normally triggered from OnPaint
	 */
	void UpdateKnobRect(const OpRect& inner);

	BOOL IsReadOnly() { return m_read_only; }
	void SetToClosestSnapPoint(const OpPoint& point);
	double SnapToStepMinMax(double unsnapped_value);
	void SetIsHoveringKnob(BOOL hovering);
	int GetTrackThickness();
	BOOL GetKnobSize(INT32& w, INT32& h);

	double m_min;
	double m_max;
	double m_anchor;
	double m_current;
	double m_step;
	double m_step_base;
	BOOL m_read_only;

	BOOL m_drag_in_progress;
	OpPoint m_start_drag_pos;
	double m_drag_value;

	OpRect m_knob_position;
	OpPoint m_start_track;
	OpPoint m_end_track;
	BOOL m_is_hovering_knob;
	TICK_VALUE *m_tick_values;
	int m_num_tick_values;
	BOOL m_show_labels;
};

#endif // OP_SLIDER_H
