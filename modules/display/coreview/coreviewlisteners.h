/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef COREVIEW_LISTENERS_H
#define COREVIEW_LISTENERS_H

class CoreView;

class CoreViewPaintListener
{
public:
	virtual ~CoreViewPaintListener() {}
	virtual BOOL BeforePaint() { return TRUE; }
	virtual void OnPaint(const OpRect &rect, OpPainter* painter, CoreView* view, int translate_x, int translate_y) = 0;
	/**
	* This method is called when the core view is moved, so that iframe's scrollbars can be repainted if necessary.
	*/
	virtual void OnMoved() {};
};

class CoreViewMouseListener
{
public:
	virtual ~CoreViewMouseListener() {}
	virtual void OnMouseMove(const OpPoint &point, ShiftKeyState keystate, CoreView* view) = 0;
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, ShiftKeyState keystate, CoreView* view) = 0;
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, ShiftKeyState keystate, CoreView* view) = 0;
	virtual void OnMouseLeave() = 0;

	/**
	 * @param delta Positive values indicate motion downwards, and negative
	 * values indicate motion upwards. The absolute value of this argument
	 * indicates how many up/down key presses it would equal.
	 *
	 * return TRUE if wheel event was eaten, FALSE if not
	 */
	virtual BOOL OnMouseWheel(INT32 delta, BOOL vertical, ShiftKeyState keystate, CoreView* view) = 0;
	virtual void OnSetCursor() = 0; // Probably only needed on windows

	/** Return the visible child of view that intersects point, or NULL if no child does. */
	virtual CoreView* GetMouseHitView(const OpPoint &point, CoreView* view) = 0;

	/** The overlapped status of this view should be updated if needed.
		(CoreView::SetIsOverlapped should be called so the view know if it might be overlapped).
		The updated status might be needed by a following call to GetMouseHitView. */
	virtual void UpdateOverlappedStatus(CoreView* view) {}
};

#ifdef TOUCH_EVENTS_SUPPORT
class CoreViewTouchListener
{
public:
	virtual ~CoreViewTouchListener() {}
	virtual void OnTouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data) = 0;
	virtual void OnTouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data) = 0;
	virtual void OnTouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data) = 0;

	/** Return the visible child of view that intersects point, or NULL if no child does. */
	virtual CoreView* GetTouchHitView(const OpPoint &point, CoreView* view) = 0;

	/** The overlapped status of this view should be updated if needed.
		(CoreView::SetIsOverlapped should be called so the view know if it might be overlapped).
		The updated status might be needed by a following call to GetTouchHitView. */
	virtual void UpdateOverlappedStatus(CoreView* view) {}
};
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT

class OpDragObject;

/**
 * This class reflects PI's OpDragListener API. All calls to OpDragListener are
 * passed through this API as well. It's needed so all CoreViewes are
 * informed about drag'n'drop events properly.
 *
 * @note Despite the fact that OpDragListener API does not pass OpDragObject
 * through it may be retrieved using OpDragManager::GetDragObject().
 *
 * @see OpDragListener
 * @see OpDragManager
 * @see OpDragObject
 */
class CoreViewDragListener
{
public:
	virtual ~CoreViewDragListener() {};
	virtual void OnDragStart(CoreView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point) = 0;
	virtual void OnDragEnter(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers) = 0;
	virtual void OnDragCancel(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers) = 0;
	virtual void OnDragLeave(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers) = 0;
	virtual void OnDragMove(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers) = 0;
	virtual void OnDragDrop(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers) = 0;
	virtual void OnDragEnd(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers) = 0;
};

#endif

class CoreViewScrollListener : public Link
{
public:
	enum SCROLL_REASON {
		SCROLL_REASON_USER_INPUT,
		SCROLL_REASON_UNKNOWN
	};

	/** Called after the view has scrolled to a new position. */
	virtual void OnScroll(CoreView* view, INT32 dx, INT32 dy, SCROLL_REASON reason) = 0;

	/** Called after a parent view has scrolled to a new position. */
	virtual void OnParentScroll(CoreView* view, INT32 dx, INT32 dy, SCROLL_REASON reason) = 0;
};

#endif // COREVIEW_LISTENERS_H
