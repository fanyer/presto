/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_PAGECONTROL_H
#define OP_PAGECONTROL_H

#include "modules/widgets/OpWidget.h"

class OpPageControlListener;

/** Page control widget.

	This is used in paged overflow mode to indicate the current page, and the
	total number of pages. The exact look depends on the number of pages. If
	there are few enough pages, each page may be represented by a radio button
	(first to last page laid out left-to-right or right-to-left, depending on
	writing direction), and the one representing the current page is
	checked. If there are too many pages for that, it will just show the page
	number and the total number of pages, along with "previous" and "next"
	buttons.

	Currently, only horizontal controls are allowed. They are typically placed
	at the bottom of an element or viewport.
 */

class OpPageControl : public OpWidget
{
public:
	static OP_STATUS Construct(OpPageControl** obj);

	OpPageControl();

	/** Set OpPageControlListener. May be NULL. */
	void SetPageControlListener(OpPageControlListener* pcl) { page_control_listener = pcl; }

	/** Get the preferred height for a page control widget. */
	int GetPreferredHeight() { return GetInfo()->GetPageControlHeight(); }

#ifdef DEBUG
	virtual void AddChild(OpWidget* child, BOOL is_internal_child = FALSE, BOOL first = FALSE)
	{
		OP_ASSERT(!"Unauthorized addition of child - will confuse the code");
		OpWidget::AddChild(child, is_internal_child, first);
	}
#endif

	/** Specify which page the control should indicate that is the current one. */
	void SetPage(unsigned int page);

	/** Specify total number of pages. */
	void SetPageCount(unsigned int count);

	/** Get the page that the control is currently indicating as the current one. */
	unsigned int GetPage() const { return current_page; }

	/** Get the total number of pages. */
	unsigned int GetPageCount() const { return page_count; }

#ifdef SUPPORT_TEXT_DIRECTION
	/** Specify whether text direction is left-to-right (FALSE) or right-to-left (TRUE). */
	void SetRTL(BOOL rtl) { packed.is_rtl = !!rtl; }

	/** Return TRUE if text direction is right-to-left. */
	BOOL IsRTL() const { return packed.is_rtl; }
#endif // SUPPORT_TEXT_DIRECTION

	/** Specify whether pagination is vertical (TRUE) or horizontal (FALSE). */
	void SetVertical(BOOL vertical) { packed.is_vertical = vertical; }

	virtual void OnLayout();
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
#ifndef MOUSELESS
	virtual void OnSetCursor(const OpPoint &point);
	virtual void OnMouseMove(const OpPoint &point);
	virtual void OnMouseLeave();
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
#endif // !MOUSELESS
	virtual void OnResize(INT32* new_w, INT32* new_h);

private:
	OP_STATUS Init();
	void GetLeftSwitcherDimensions(int& x, int& width);
	void GetRightSwitcherDimensions(int& x, int& width);
	void GetUpSwitcherDimensions(int& x, int& width);
	void GetDownSwitcherDimensions(int& x, int& width);
	BOOL IsOverLeftButton(const OpPoint &point);
	BOOL IsOverRightButton(const OpPoint &point);
	int CalculateRadioButtonSize() const { return rect.height - 8; }

	/** Delete all child widgets. */
	void DeleteChildren();

	/** Lay out the widget. Create necessary child widgets. */
	OP_STATUS Layout();

	/** Paint the widget. */
	OP_STATUS Paint(OpWidgetPainter* widget_painter);

	/** Widget listener implementation for OpPageControl.

		Letting OpPageControl inherit OpWidgetListener is risky
		business, since there's QUICK stuff somewhere that makes
		OpWidget already inherit that class. */
	class PcWidgetListener : public OpWidgetListener
	{
		friend class OpPageControl;

	private:
		OpPageControl* page_control;

		PcWidgetListener(class OpPageControl* page_control) : page_control(page_control) { }
		virtual void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	} widget_listener;

	friend class PcWidgetListener;

	OpPageControlListener* page_control_listener;
	unsigned int current_page;
	unsigned int page_count;

	union
	{
		struct
		{
#ifdef SUPPORT_TEXT_DIRECTION
			unsigned int is_rtl:1;
#endif // SUPPORT_TEXT_DIRECTION
			unsigned int is_vertical:1;
			unsigned int left_pressed:1;
			unsigned int right_pressed:1;
			unsigned int left_hovered:1;
			unsigned int right_hovered:1;
			unsigned int one_button_per_page:1;
		} packed;
		UINT32 packed_init;
	};
};

/** Listener used to report events to the owner of an OpPageControlListener. */

class OpPageControlListener
{
public:
	/** Called when the user attempts to navigate to a different page. */

	virtual void OnPageChange(unsigned int page) = 0;
};

#endif // OP_PAGECONTROL_H
