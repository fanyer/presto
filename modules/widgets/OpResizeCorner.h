/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_RESIZE_CORNER_H
#define OP_RESIZE_CORNER_H

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpScrollbar.h"

class OpResizeCorner : public OpWidget
{
public:
	// Implements OpWidget.
	void OnBeforePaint();

	/**
	 * Set the ScrollbarColors associated with this widget.
	 *
	 * If the resize corner is not skinned, it will be rendered using
	 * these colors.
	 *
	 * @param colors The new scrollbar colors.
	 */
	void SetScrollbarColors(const ScrollbarColors* colors) { this->colors = *colors; }

	/** @return The current ScrollbarColors. */
	ScrollbarColors& GetScrollbarColors() { return colors; }
protected:
	ScrollbarColors colors;
};

/** OpWindowResizeCorner is the lower right corner grip, for resizing the window. */
class OpWindowResizeCorner : public OpResizeCorner
{
protected:
	OpWindowResizeCorner();

public:
	static OP_STATUS Construct(OpWindowResizeCorner** obj);

	/** Set the Window that should be resized when this resizecorner is used. */
	void SetTargetWindow(Window* window) { this->window = window; }

	/** Enable/disable the resizecorner. When active is FALSE, it will be blank and disabled. */
	void SetActive(BOOL active) { this->active = active; }
	BOOL IsActive() { return active; }

	// == Hooks ======================
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
#ifndef MOUSELESS
	void OnSetCursor(const OpPoint &point);
	void OnMouseMove(const OpPoint &point);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
#endif // !MOUSELESS

private:
	Window* window;
	OpPoint down_point;
	BOOL resizing;
	BOOL active;
};

/**
 * A widget which allows the user to resize other widgets.
 *
 * This class may be used to expose a handle which allow the user
 * to resize a target widget.
 *
 * The original use-case is resizing textareas.
 */
class OpWidgetResizeCorner : public OpResizeCorner
{
public:
	/**
	 * Create a new OpWidgetResizeCorner
	 *
	 * @param widget The OpWidget to make resizable. May not be NULL, and
	 *               must exist as long as this OpWidget exists.
	 */
	OpWidgetResizeCorner(OpWidget* widget);

	/**
	 * Indicates whether the OpWidgetResizeCorner has nearby scrollbars visible.
	 *
	 * This may change how the resize corner is drawn.
	 *
	 * @param horizontal TRUE if the horizontal scrollbar is visible.
	 * @param vertical TRUE if the vertical scrollbar is visible, and
	 *                 visually "touching" the resize corner.
	 */
	void SetHasScrollbars(BOOL horizontal, BOOL vertical)
	{
		this->has_horizontal_scrollbar = horizontal;
		this->has_vertical_scrollbar = vertical;
	}

	// Overrides OpWidget.
	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect& paint_rect);
#ifndef MOUSELESS
	void OnSetCursor(const OpPoint& point);
	void OnMouseMove(const OpPoint& point);
	void OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks);
#endif // !MOUSELESS

private:
	/** @return TRUE if the resize corner is enabled. */
	BOOL IsActive() const;

	/** @return TRUE if the associated widget has visible scrollbars. */
	BOOL HasScrollbars() const { return has_horizontal_scrollbar || has_vertical_scrollbar; }

	/**
	 * Resize the Target to the specified dimensions.
	 *
	 * @param target The target to resize.
	 * @param w The new width (in pixels).
	 * @param h The height (in pixels).
	 */
	void Resize(int w, int h);

	/// The target OpWidget to make resizable.
	OpWidget* widget;

	/// The mousedown point (in document coordinates) when resizing begun.
	OpPoint down_point;

	/// TRUE if we are resizing.
	BOOL resizing;

	/// The width of 'widget' when the resizing begun.
	INT32 down_width;

	/// The height of 'widget' when the resizing begun.
	INT32 down_height;

	/// The width of 'widget' before it was modified.
	INT32 original_width;

	/// The height of 'widget' before it was modified.
	INT32 original_height;

	/// True if the target widget has scrollbars visible.
	BOOL has_horizontal_scrollbar;
	BOOL has_vertical_scrollbar;
};
#endif // OP_RESIZE_CORNER_H
