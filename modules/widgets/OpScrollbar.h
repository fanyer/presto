/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SCROLLBAR_H
#define OP_SCROLLBAR_H

#include "modules/widgets/OpWidget.h"
#include "modules/hardcore/mh/messobj.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleScrollbarPart.h"
#endif

class Settings;

/**
	ScrollbarColors handles 7 different colors for scrollbars, for use with the IE CSS Extension to
	give scrollbars custom colors.
*/

class ScrollbarColors
{
public:
	ScrollbarColors();
	UINT32 Face(UINT32 def_col);
	UINT32 Arrow(UINT32 def_col);
	UINT32 Shadow(UINT32 def_col);
	UINT32 DarkShadow(UINT32 def_col);
	UINT32 Highlight(UINT32 def_col);
	UINT32 Light(UINT32 def_col);
	UINT32 Track(UINT32 def_col);

	void SetBase(UINT32 col) { base = col; is_modified = TRUE; }
	void SetFace(UINT32 col) { face = col; is_modified = TRUE; }
	void SetArrow(UINT32 col) { arrow = col; is_modified = TRUE; }
	void SetShadow(UINT32 col) { shadow = col; is_modified = TRUE; }
	void SetDarkShadow(UINT32 col) { darkshadow = col; is_modified = TRUE; }
	void SetHighlight(UINT32 col) { highlight = col; is_modified = TRUE; }
	void SetLight(UINT32 col) { light = col; is_modified = TRUE; }
	void SetTrack(UINT32 col) { track = col; is_modified = TRUE; }

	void SetEnabled(BOOL enabled) { is_enabled = enabled; }

	BOOL IsModified() { return is_modified; }
	BOOL IsEnabled() { return is_enabled; }
private:
	UINT32 face, arrow, base, shadow, darkshadow, highlight, light, track;
	BOOL is_modified;
	BOOL is_enabled;
};

/**
	OpSmoothScroller handles timing and messages for smooth scrolling.
	There is tweaks to modify its smoothness and deacceleration.
*/

class OpSmoothScroller : private MessageObject
{
public:
	OpSmoothScroller();
	~OpSmoothScroller();

	/** Set the value that should be scrolled to smootly.
	 * @param value should be the current value.
	 * @param target_value should be the target value for the scroll.
	 */
	void SetValueSmoothScroll(INT32 value, INT32 target_value);

	/** Stop the smooth scroll on the current value. */
	void StopSmoothScroll();

#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
	BOOL IsSmoothScrollRunning() { return m_running; }
	INT32 GetSmoothScrollTargetValue() { return m_target_value; }
#else
	BOOL IsSmoothScrollRunning() { return FALSE; }
	INT32 GetSmoothScrollTargetValue() { return 0; }
#endif

	/** Call from each update of the scroll position from user action. If you want to use AutoScroll. */
	void PrepareAutoScroll(int delta);
	/** Call when auto-scroll should start. Value should be the current scroll value. */
	void AutoScroll(int value);

	/** Called when the smooth scroll is being done, for each scrollstep */
	virtual BOOL OnSmoothScroll(INT32 value) = 0;
	virtual BOOL GetLimits(INT32 &min, INT32 &max) { return FALSE; }

private:
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef WIDGETS_ASYNC_SMOOTHSCROLL
	void Run();
	BOOL m_running;
	double m_value;			///< Current value
	INT32 m_target_value;	///< Target value
	double m_start_time;	///< Start time of last post message
	double m_scroll_time;	///< The time it took last time, between post message to after scroll was done.
	INT32 m_length;			///< Delta between original value and target value.
	double m_auto_delta;
#endif // WIDGETS_ASYNC_SMOOTHSCROLL
};

/** OpScrollbar is a horizontal or vertical scrollbar widget for use in documents, listboxes, textfields, or anything else that needs a scrollbar. */

class OpScrollbar : public OpWidget, public OpSmoothScroller
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, private AccessibleTextObjectListener
#endif
{
	protected:

	OpScrollbar(BOOL horizontal = FALSE);
	virtual				~OpScrollbar() {}

public:
#ifdef WIDGETS_IME_SUPPORT
	virtual void		OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
#endif // WIDGETS_IME_SUPPORT
	// Implementing OpInputContext interface.
	virtual BOOL		OnInputAction(OpInputAction* action);
	virtual const char*	GetInputContextName() {return "Scroll Widget";}

	static OP_STATUS Construct(OpScrollbar** obj, BOOL horizontal = FALSE);

	/** Copy the scrollbarcolors to this scrollbar */
	void SetScrollbarColors(ScrollbarColors* colors);

	/** Set the limits for this scrollbar, that decides the minimum and maximum value, and the visible amount which will
	 * affect the length of the moving knob.
	 *
	 * @param limit_min The minimum value, typically 0.
	 * @param limit_max The maximum value, typically the length of the document - how_much_visible.
	 * @param how_much_visible How much that is visible in the viewport.
	 */
	void SetLimit(INT32 limit_min, INT32 limit_max, INT32 how_much_visible);

	/** Set how far the scrollbar should move when the user click it.
	 * @param small_step When the arrow buttons is clicked.
	 * @param big_step When the background (behind the moving knob) is clicked.
	 */
	void SetSteps(INT32 small_step, INT32 big_step);

	/** Set the value of the scrollbar. The value will be clipped within the current limits.
	 *	The scroll will be done with smooth scroll if enabled and all parameters are TRUE.
	 * @param new_value The new value that should be scrolled to.
	 * @param caused_by_input Should be TRUE if caused by user input. FALSE if caused by something else (f.ex script or indirectly from user input of something else).
	 * @param send_onscroll Should be TRUE of the OnScroll callback in the OpWidgetListener of this widget should be called if the value changed.
	 * @param smooth_scroll Should be TRUE if smooth scroll is wanted.
	 */
	void SetValue(INT32 new_value, BOOL caused_by_input, BOOL send_onscroll = TRUE, BOOL smooth_scroll = FALSE);

	/** Set the value of the scrollbar, not caused by user input directly on the scrollbar. May invoke OnScroll */
	void SetValue(INT32 value) {SetValue(value, FALSE);}

	/** Get the current value */
	INT32 GetValue() {return value;}

	BOOL GetLimits(INT32 &min, INT32 &max) { min = limit_min; max = limit_max; return TRUE; }

	/** Get last clicked part */
	SCROLLBAR_PART_CODE GetHitPart() {return hit_part;}

	/** Get the minimal length (width for horizontal scrollbar and height for vertical) of the knob. */
	int GetKnobMinLength();

	/** Get the last hovered part */
	SCROLLBAR_PART_CODE GetHoverPart() {return hover_part;}

	/** Set the rectangle of the moving knob. */
	void SetKnobRect(const OpRect &knobRect);

	/** Return TRUE if this scrollbar is horizontal, FALSE if vertical */
	BOOL IsHorizontal() { return horizontal; }

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	/** Get which kind of scrollbar part that is on the given point */
	void GetScrollbarPartByPoint(const OpPoint &point, SCROLLBAR_PART_CODE& part);

	/** Return TRUE if the scrollbar is enabled and can be scrolled in some direction. */
	BOOL CanScroll();

	/** Return TRUE if the scrollbar can be scrolled in the given direction.
	 * @param ARROW_LEFT, ARROW_UP, ARROW_RIGHT or ARROW_DOWN
	 */
	BOOL CanScroll(int direction);

	// == Hooks ======================
	void OnBeforePaint();
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	void OnMouseMove(const OpPoint &point);
	void OnMouseLeave();
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	BOOL OnScroll(INT32 delta, BOOL vertical, BOOL smooth = TRUE);
	BOOL OnMouseWheel(INT32 delta,BOOL vertical);
	void OnTimer();

	BOOL OnSmoothScroll(INT32 value);

	// Implementing the OpTreeModelItem interface

	virtual Type		GetType() {return WIDGET_TYPE_SCROLLBAR;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_SCROLLBAR || OpWidget::IsOfType(type); }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual OP_STATUS GetAbsolutePositionOfObject(int i, OpRect& rect);
	virtual OP_STATUS SetFocusToObject(int i) { return OpStatus::ERR; }
	virtual OP_STATUS GetTextOfObject(int i, OpString& str) { return OpStatus::ERR; }
	virtual OpAccessibleItem* GetAccessibleParentOfObject() {return this;}
	virtual Accessibility::State GetStateOfObject(int i);
	virtual OP_STATUS ObjectClicked(int i);
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OP_STATUS AccessibilityGetMinValue(int &value) { value = limit_min; return OpStatus::OK; }
	virtual OP_STATUS AccessibilityGetMaxValue(int &value) { value = limit_max; return OpStatus::OK; }

	virtual void OnDeleted();
#endif

public:
	INT32 small_step, big_step;
	INT32 limit_min, limit_max, limit_visible;
	INT32 value;
	INT32 value_on_start_move;

	BOOL horizontal;
	BOOL is_scrolling_down; ///< Used in OnTimer to know which direction
	BOOL is_scrolling_fast;

	OpRect knob_rect;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OpRect button_scrollbk;
	OpRect button_scrollfw;
	OpRect button_pgbk;
	OpRect button_pgfw;
#endif
	BOOL is_moving;
	INT32 moveofs;

	ScrollbarColors scrollbar_colors;
	
	SCROLLBAR_PART_CODE hit_part;
	SCROLLBAR_PART_CODE hover_part;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibleScrollbarPart* m_accessible_arrow_up_part;
	AccessibleScrollbarPart* m_accessible_arrow_down_part;
	AccessibleScrollbarPart* m_accessible_page_up_part;
	AccessibleScrollbarPart* m_accessible_page_down_part;
	AccessibleScrollbarPart* m_accessible_knob_part;
#endif
};

#endif // OP_SCROLLBAR_H
