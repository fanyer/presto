/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPVIEW_H
#define OPVIEW_H

#include "modules/inputmanager/inputcontext.h"
#include "modules/pi/OpDragManager.h"

#ifdef PLATFORM_FONTSWITCHING
# include "modules/display/fontdb.h"
#endif

class OpRect;
class OpPoint;
class OpPainter;
class OpView;
class OpWindow;


/* See OpInputMethod.h for the declaration and details about OpInputMethodListener. */
class OpInputMethodListener;

/** Listener for OpView to use to notify core about paint related events. */
class OpPaintListener
{
public:
	virtual ~OpPaintListener() {}

	/** Call before OnPaint().
	 *
	 * To be called each time a paint event is received from the
	 * system, before calling OnPaint(). Note that if the system is
	 * capable of delivering several paint rectangles per paint event,
	 * so that several calls to OnPaint() are necessary, only one call
	 * to BeforePaint() is necessary. This code helps the layout code
	 * complete any pending reflow of the document before painting it.
	 *
	 * @return TRUE if core is ready to paint, in which case the
	 * platform implementation should continue with calling
	 * OnPaint(). If FALSE is returned, it means that core is not ready
	 * to paint yet (typically an incomplete reflow due to yield). In
	 * this case the platform implementation should call BeforePaint()
	 * again later, probably after having processed other pending
	 * events (UI events and such). Note that it is harmless to ignore
	 * this return value, and always call OnPaint(), but that will
	 * cause unnecessary screen updates on systems that use the yield
	 * mechanism.
	 */
	virtual BOOL BeforePaint() { return TRUE; }

	/** Notify core that a part of the OpView needs to be repainted.
	 *
	 * @param rect The part of the view that needs to be repainted, relative to
	 * the rendering viewport, in screen coordinates.
	 * @param view OpView that needs to be repainted.
	 */
	virtual void OnPaint(const OpRect &rect, OpView* view) = 0;

	/** Used to optimize stuff.
	 *
	 * Called when the OpView is completely covered by some other view
	 * or window. The listener might then choose to stop updating,
	 * stop timers, etc., until BeforePaint() is called.
	 */
	virtual void OnCovered(OpView* view) {}
};

#ifndef MOUSELESS
/** Listener for OpView to use to notify core about mouse events (movement, button presses, etc.). */
class OpMouseListener
{
public:
	virtual ~OpMouseListener() {}

	/** The mouse cursor has moved to the specified position.
	 *
	 * @param point New position of mouse cursor, relative to the rendering
	 * viewport, in screen coordinates.
	 * @param keystate The keyboard modifier keys pressed when the mouse was moved.
	 * @param view The OpView in question
	 */
	virtual void OnMouseMove(const OpPoint &point, ShiftKeyState keystate, OpView* view) = 0;

	/** A mouse button has been pressed down.
	 *
	 * @param button The button that was pressed.
	 * @param nclicks The number of successive clicks on this button,
	 * without any other buttons being pressed or released, any
	 * significant mouse cursor movement, etc, inbetween. This is used
	 * to track double-clicking. How long to wait before resetting
	 * this counter is up to the system. 500 ms may be a good
	 * value. What "significant cursor movement" means is also up to
	 * the system.
	 * @param keystate The keyboard modifier keys pressed when the mouse button was pressed.
	 * @param view The OpView that received this event.
	 */
	virtual void OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState keystate, OpView* view) = 0;

	/** A mouse button has been released (after having been pressed down).
	 *
	 * @param button The button that was pressed.
	 * @param keystate The keyboard modifier keys pressed when the mouse button was released.
	 * @param view The OpView that received this event.
	*/
	virtual void OnMouseUp(MouseButton button, ShiftKeyState keystate, OpView* view) = 0;

	/** The mouse cursor has gone outside the boundaries of the OpView. */
	virtual void OnMouseLeave() = 0;

	/** A mouse wheel event has occurred.
	 *
	 * @param delta Positive values indicate motion downwards, and
	 * negative values indicate motion upwards. The absolute value of
	 * this argument indicates how many up/down key presses it would
	 * equal.
	 * @param vertical TRUE if the wheel motion is vertical, FALSE if
	 * horizontal.
	 * @param keystate The keyboard modifier keys pressed when the mouse wheel was moved.
	 * @return TRUE if the wheel event was eaten, FALSE if not (in
	 * which case a suitable reaction could be to propagate this event
	 * to the parent OpView)
	 */
	virtual BOOL OnMouseWheel(INT32 delta, BOOL vertical, ShiftKeyState keystate) = 0;

	/** Called when the system has changed the mouse cursor icon. */
	virtual void OnSetCursor() = 0;

	/** If the currently captured view is this OpView, release it.
	 *
	 * The core code may want to set an OpView as
	 * "captured". E.g. when a mouse button is pressed, all mouse
	 * motion events are to be treated as if they occurred in this
	 * view (even if the mouse cursor is outside the view), until the
	 * mouse button is released.
	 *
	 * Should be called if the platform knowns that a button release
	 * won't be sent (which may be the case if the view is deleted
	 * while a button is pressed).
	 *
	 */
	virtual void OnMouseCaptureRelease() = 0;

	/** Check if mouse events can be processed at the given point
	 *
	 * @param x co-ordinate relative to the rendering viewport
	 * @param y co-ordinate relative to the rendering viewport
	 * @param view OpView that is mouse is listening to
	 * @return true if mouse events can be processed
	 */
	virtual bool GetHitStatus(int x, int y, OpView *view) { return true; }
};
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
/** Listener for OpView to use to notify core about touch events.
 *
 * Implemented by Core.
 *
 * The current control flow of touch events is similar to that of mouse
 * events: the platform sends the event to an implementation of OpView,
 * which acquires an implementation of this class from Core (such as
 * display's TouchListener), and forwards the calls to the methods
 * defined below.
 */
class OpTouchListener
{
public:
	virtual ~OpTouchListener() {}

	/** A touch has been detected.
	 *
	 * @param id Touch identifier.
	 * @param point Position of touch, relative to rendering viewport, in
	 * screen coordinates.
	 * @param radius Radius of the touch, in screen units.
	 * @param modifiers State of keyboard modifier keys.
	 * @param view The OpView that received this event.
	 * @param user_data Platform supplied event identification. Returned
	 * by core with event related callbacks.
	 */
	virtual void OnTouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, OpView* view, void *user_data) = 0;

	/** A touch has vanished.
	 *
	 * @param id Touch identifier.
	 * @param point Position of touch, relative to rendering viewport, in
	 * screen coordinates.
	 * @param radius Radius of the touch, in screen units.
	 * @param view The OpView that received this event.
	 * @param user_data Platform supplied event identification. Returned
	 * by core with event related callbacks.
	 */
	virtual void OnTouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, OpView* view, void *user_data) = 0;

	/** A touch has moved to the specified position.
	 *
	 * @param id Touch identifier.
	 * @param point New position of touch, relative to the rendering
	 * viewport, in screen coordinates.
	 * @param radius Radius of the touch, in screen units.
	 * @param view The OpView in question.
	 * @param user_data Platform supplied event identification. Returned
	 * by core with event related callbacks.
	 */
	virtual void OnTouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, OpView *view, void *user_data) = 0;
};

#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
/** An OpView's drag'n'drop operation listener. It's created and set in OpView by core.
 *
 * OpDragManager uses this for notifing core about platform-specific drag'n'drop events.
 *
 * In case of the drag'n'drop operations started within Opera aside from OnDragStart() the methods of this listener
 * may only be called after core has called OpDragManager::StartDrag(), which it does in response to a call to OnDragStart()
 * if it agrees this is a real drag.
 *
 * @see OpDragManager
 */
class OpDragListener
{
public:
	virtual ~OpDragListener() {}

	/** Informs core about potential start of a drag'n'drop operation.
	 *
	 * It has to be called when the drag'n'drop operation start was detected
	 * (e.g. due to mouse move with the left button pressed). If core agrees
	 * that the operation is valid (i.e. the drag started over a draggable
	 * element or over a text selection and was not cancelled by a page)
	 * it calls OpDragManager::StartDrag() (might be called synchronously or asynchronously).
	 * From this point other OpDragListener's methods have to be called accordingly.
	 * If the operation is not valid core calls OpDragManager::StopDrag(TRUE)
	 * (might be called synchronously or asynchronously) signalling the drag was rejected
	 * and no OpDragListener's method may be called for this drag'n'drop operation.
	 *
	 * @param view - the view in which the drag'n'drop operation started.
	 * @param start_point - a point where the drag started, relative to rendering viewport,
	 * in screen coordinates. If the drag is detected by e.g. mouse move with the left button
	 * pressed this will be the point mouse's button was pressed.
	 * @param modifiers - State of keyboard modifier keys.
	 * @param current_point - a point where the drag moved, relative to rendering viewport,
	 * in screen coordinates. This will be the same as start_point if the drag start does not depend on
	 * a pointing device movement e.g. a long mouse click. Otherwise this should be the curent possition of
	 * the pointing device.
	 *
	 * @see OpDragManager
	 * @see OpDragManager::OnStartDrag()
	 * @see OpDragManager::OnStopDrag()
	 */
	virtual void OnDragStart(OpView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point) = 0;

	/** Informs core that a new OpView was entered as the result of a drag'n'drop
	 * move event.
	 *
	 * This function must be called before any calls to OpDragListener's methods for
	 * this view.
	 *
	 * @param view - the view into which a drag'n'drop operation entered.
	 * @param point - a point where the drag entered, relative to rendering viewport,
	 * in screen coordinates.
	 * @param modifiers - State of keyboard modifier keys.
	 *
	 * @note when the drag comes from some external applciation and it enters Opera for the first time
	 * prior to calling this function the drag'n'drop data must be made accessible via
	 * OpDragManager::GetDragObject().
	 *
	 * @see OnDragMove()
	 * @see OpDragManager::StartDrag()
	 */
	virtual void OnDragEnter(OpView* view, const OpPoint& point, ShiftKeyState modifiers) = 0;

	/** Informs core that a drag'n'drop operation was cancelled e.g.
	 * because ESC key was pressed by a user.
	 * No other method of this OpDragListener may be called until
	 * core subsequently calls OpDragManager::StopDrag(TRUE).
	 *
	 * @param view - the view over which the drag'n'drop operation was cancelled.
	 * @param modifiers - State of keyboard modifier keys.
	 *
	 * @see OpDragManager::StartDrag()
	 */
	virtual void OnDragCancel(OpView* view, ShiftKeyState modifiers) = 0;

	/** Informs core that a drag'n'drop operation left some OpView
	 * as the result of a drag'n'drop move event.
	 *
	 * If a new OpView is entered it's important that this function is
	 * called before calling OnDragEnter() for the new view.
	 *
	 * @param view - the view which was left.
	 * @param modifiers - State of keyboard modifier keys.
	 *
	 * @see OnDragEnter()
	 * @see OpDragManager::StartDrag()
	 */
	virtual void OnDragLeave(OpView* view, ShiftKeyState modifiers) = 0;

	/** Informs core about a drag'n'drop move event.
	 *
	 * @param view - the view in which the move occured.
	 * @param point - a point where the drag moved to, relative to rendering viewport,
	 * in screen coordinates.
	 * @param modifiers - State of keyboard modifier keys.
	 *
	 * @see OpDragManager::StartDrag()
	 */
	virtual void OnDragMove(OpView* view, const OpPoint& point, ShiftKeyState modifiers) = 0;

	/** Informs core about a drag'n'drop drop event.
	 * It should be called when drag'n'drop was ended over Opera.
	 * It must not be called when drag'n'drop was ended externally e.g.
	 * over some external application: OnDragEnd() is for such cases.
	 * No other method of this OpDragListener may be called until
	 * core subsequently calls OpDragManager::StopDrag(TRUE).
	 *
	 * @param view - the view in which the drop occured.
	 * @param point - a point at which the drop occured, relative to rendering viewport,
	 * in screen coordinates.
	 * @param modifiers - State of keyboard modifier keys.
	 *
	 * @see OnDragEnded()
	 * @see OpDragManager::StartDrag()
	 */
	virtual void OnDragDrop(OpView* view, const OpPoint& point, ShiftKeyState modifiers) = 0;

	/** Informs core that a drag'n'drop operation ended somewhere outside Opera.
	 * It must not be called when drag'n'drop was ended over Opera: OnDragDrop() is
	 * for such cases.
	 * No other method of this OpDragListener may be called until
	 * core subsequently calls OpDragManager::StopDrag(TRUE).
	 *
	 * @param view - the last known drag'n'drop view.
	 * @param point - a point at which the operation ended, relative to rendering viewport,
	 * in screen coordinates (might be nagative).
	 * @param modifiers - State of keyboard modifier keys.
	 *
	 * @see OpDragManager::StartDrag()
	 */
	virtual void OnDragEnd(OpView* view, const OpPoint& point, ShiftKeyState modifiers) = 0;
};
#endif // DRAG_SUPPORT

#ifdef PLATFORM_FONTSWITCHING
/** Listener for OpView to use to notify core about new fonts being loaded.
 *
 * When OpFont::StringWidth() is called, all fonts may not have been loaded
 * yet. Fontswitching may trigger fonts to be loaded. This is a potentially
 * slow operation, and this class provides a way to do this in an asychronous
 * manner. OpFont::StringWidth() may return a "bogus" width if it wants to load
 * additional fonts in the background (due to fontswitching), and then later,
 * when all needed fonts have been loaded, the platform implementation calls
 * OnAsyncFontLoaded(), to notify core that one or more documents need to be
 * reflowed, since the fonts needed to do proper fontswitching now are
 * available, and that may affect font metrics.
 *
 * During Opera start-up, all fonts are enumerated and checked for some
 * capabilities (weights available, italics and so on), but they are probably
 * not "loaded" at that time, since that's potentially expensive.
 */
class OpAsyncFontListener
{
public:
	virtual ~OpAsyncFontListener() {}

	/** Notify core that this OpView needs to be reflowed, because additional fonts have been loaded. */
	virtual void OnAsyncFontLoaded() = 0;
};
#endif // PLATFORM_FONTSWITCHING

#ifdef UTIL_LISTENERS
/** Listener for OpView notifications. */
class OpViewListener
{
public:
	virtual ~OpViewListener() {}

	/** OpView is about to be deleted.
	 *
	 * @param view This OpView that is about to be deleted.
	 */
	virtual void OnOpViewDeleted(OpView* view) = 0;
};
#endif // UTIL_LISTENERS

/** @short A window/view/widget paint device in the platform's windowing system.
 *
 * An OpView has no window decoration (title bar, border, etc.).
 *
 * It is always embedded inside an OpWindow or another OpView. It will report
 * events (damage/paint, mouse motion, etc.) sent to this view to core via
 * various listeners, and core will instruct an OpView to move, resize and hide
 * itself, provide a painter (which is the means of painting on this OpView),
 * etc.
 *
 * A top-level OpView (i.e. one whose parent is an OpWindow) typically
 * represents a browser window's rendering viewport. The size of the OpView
 * will then be equal to the size of the rendering viewport, in screen
 * coordinates.
 *
 * An OpView may be a parent of OpWindow, OpPluginWindow, and other
 * OpView objects.
 *
 * Many of the methods in this class may trigger paint events; however, such
 * paint events must never be handled synchronously (by calling
 * OpPaintListener::OnPaint() directly). Any repainting needed must be handled
 * asynchronously, by posting paint events. The reason for this is that core is
 * not re-entrant when it comes to painting. (Scenario: Core handles
 * OpPaintListener::OnPaint(), and as part of that job ScrollRect() may be
 * called. It would then be dangerous to call OpPaintListener::OnPaint() a
 * second time, while the first one is still on the call stack.) The only
 * method exempt from this rule is Sync(), whose sole purpose after all is to
 * handle pending events.
 */
class OpView
 : public OpInputContext
{
private:
	BOOL	is_fixed;

public:
	/** Create an OpView as a child of an OpWindow */
	static OP_STATUS Create(OpView** new_opview, OpWindow* parentwindow);

	/** Create an OpView as a child of another OpView. */
	static OP_STATUS Create(OpView** new_opview, OpView* parentview);

	OpView() : is_fixed(FALSE) {}

	virtual ~OpView() {}

	/** Enable or disable the back-buffer (Flicker-free drawing). */
	virtual void UseDoublebuffer(BOOL doublebuffer) = 0;

	/** Scroll the specified rectangle of this view.
	 *
	 * It is up to the platform how to achieve this. It will typically involve
	 * moving the part of the rectangle that doesn't need repainting, and
	 * invalidating the damaged "scrolled in" area. Just as for all the methods
	 * in this class except for Sync(), repainting must NOT be done
	 * synchronously. Further complication to consider: If this method is
	 * called again before a paint event triggered by a previous call has been
	 * handled, the rectangle of that paint event needs to be changed if the
	 * rectangles in the two calls to ScrollRect() intersect. Both the
	 * coordinates and the size of the paint rectangle may change. If the
	 * implementation is unable to change the rectangle of pending paint
	 * events, invalidating the entire rectangle may be the only viable option.
	 *
	 * Core should, if possible, avoid calling ScrollRect() a second time
	 * before a paint event triggered by a previous call to ScrollRect() has
	 * been handled. This can be achieved explicitly by calling Sync().
	 *
	 * @param rect The rectangle to scroll, relative to the rendering viewport,
	 * in screen coordinates.
	 * @param dx Numbers of pixels to scroll right (or left if
	 * negative)
	 * @param dy Numbers of pixels to scroll down (or up if negative)
	 */
	virtual void ScrollRect(const OpRect &rect, INT32 dx, INT32 dy) = 0;

	/** Scroll the entire view.
	 *
	 * It should move all the child views / widgets (e.g. plug-ins) when
	 * scrolling.
	 *
	 * It is up to the platform how to achieve this. It will typically involve
	 * moving the part of the view that doesn't need repainting, and
	 * invalidating the damaged "scrolled in" area. Just as for all the methods
	 * in this class except for Sync(), repainting must NOT be done
	 * synchronously. Further complication to consider: If this method is
	 * called again before a paint event triggered by a previous call to either
	 * this method or ScrollRect() has been handled, the rectangle of that
	 * paint event may need to be changed. If the implementation is unable to
	 * change the rectangle of pending paint events, invalidating the entire
	 * view may be the only viable option.
	 *
	 * Core should, if possible, avoid calling Scroll() before a paint event
	 * triggered by a previous call to Scroll() or ScrollRect() has been
	 * handled. This can be achieved explicitly by calling Sync().
	 *
	 * @param dx Numbers of pixels to scroll right (or left if negative)
	 * @param dy Numbers of pixels to scroll down (or up if negative)
	 * @param full_update If TRUE, the entire view should be invalidated (no
	 * scrolling) but the children should be moved as usual.
	 */
	virtual void Scroll(INT32 dx, INT32 dy, BOOL full_update) = 0;

	/** Handle pending paint operations now.
	 *
	 * This is the only method in this class that may repaint synchronously
	 * (i.e. call OpPaintListener::OnPaint() directly).
	 *
	 * It is not required to reimplement this method, but it is highly
	 * recommended; otherwise things may start to lag behind (e.g. when
	 * scrolling, the scrollbar may be updated, but not the actual page
	 * contents).
	 */
	virtual void Sync() {}

	/** Lock/unlock the view for updates.
	 *
	 * Try to avoid any kind of invalidation until view is
	 * unlocked. You need to implement this if you want to support
	 * loading redraw delay. Note that this isn't the final
	 * solution. Core should at some point implement proper support
	 * for loading new page in background and switch document after x
	 * seonds. However, since core doesn't support that now, it will
	 * always switch document instantly when header data is available,
	 * and we must instead rely on locking of the view to achieve
	 * redraw delay.
	 */
	virtual void LockUpdate(BOOL lock) = 0;

	/** Set the position of the view, relative to the parent. */
	virtual void SetPos(INT32 x, INT32 y) = 0;

	/** Get the position of the view. None of the supplied parameters may be NULL. */
	virtual void GetPos(INT32* x, INT32* y) = 0;

	/** Set the size of the view (including scrollbars). */
	virtual void SetSize(UINT32 w, UINT32 h) = 0;

	/** Get the size of the view (including scrollbars). */
	virtual void GetSize(UINT32* w, UINT32* h) = 0;

	/** Set the visibility of the view. */
	virtual void SetVisibility(BOOL visible) = 0;

	/** Get the visibility of the view. */
	virtual BOOL GetVisibility() = 0;

	/** Set a custom overlap region. Any rect in the overlap region will
		cause the visible region to exclude that area on both this view and child views.
		If combined with SetAffectLowerRegions(FALSE), the lower views will be visible through the overlapping area. */
	virtual OP_STATUS SetCustomOverlapRegion(OpRect *rects, INT32 num_rects) = 0;

	/** Set if this view should affect the visible region of lower views (parent or children to parents with lower z).
		The default is TRUE. */
	virtual void SetAffectLowerRegions(BOOL affect_lower_regions) = 0;

#ifdef WIDGETS_IME_SUPPORT
	/** Set the listener for this OpView to use to report input method events to core. */
	virtual void SetInputMethodListener(OpInputMethodListener* imlistener) = 0;
#endif // WIDGETS_IME_SUPPORT
	/** Set the listener for this OpView to use to report paint events to core. */
	virtual void SetPaintListener(OpPaintListener* paintlistener) = 0;
#ifndef MOUSELESS
	/** Set the listener for this OpView to use to report mouse events to core. */
	virtual void SetMouseListener(OpMouseListener* mouselistener) = 0;
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	/** Set the listener for this OpView to use to report touch events to core. */
	virtual void SetTouchListener(OpTouchListener* touchlistener) = 0;
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	/** Set the drag listener for this OpView to use for reporting drag events to core. */
	virtual void SetDragListener(OpDragListener* draglistener) = 0;
	/** Get the drag listener for this OpView to use it for reporting drag events to core. */
	virtual OpDragListener* GetDragListener() const = 0;
#endif // DRAG_SUPPORT
#ifdef PLATFORM_FONTSWITCHING
	/** Add a delayed font loading listener.
	 *
	 * Multiple listeners are allowed.
	 */
	virtual OP_STATUS AddAsyncFontListener(OpAsyncFontListener* asyncfontlistener) = 0;

	/** Remove a delayed font loading listener. */
	virtual void RemoveAsyncFontListener(OpAsyncFontListener* asyncfontlistener) = 0;
#endif // PLATFORM_FONTSWITCHING

	/** Get the current state of the shiftkeys (Shift, control, alt) */
	virtual ShiftKeyState GetShiftKeys() = 0;

#ifndef MOUSELESS

	/** Get the status of a mouse button.
	 *
	 * @param button Which button to check.
	 * @return TRUE if the button is currently pressed.
	 */
	virtual BOOL GetMouseButton(MouseButton button) = 0;

#ifdef TOUCH_EVENTS_SUPPORT
	/** Set the status of a mouse button.  This can be called to emulate
	 * mouse buttons on touch, if enabled
	 *
	 * @param button Which button to set
	 * @param set Status of the button
	 */
	virtual void SetMouseButton(MouseButton button, bool set) = 0;
#endif // TOUCH_EVENTS_SUPPORT

	/** Get the position of the mouse cursor.
	 *
	 * Coordinates are relative to this OpView. None of the parameters
	 * may be NULL.
	 */
	virtual void GetMousePos(INT32* xpos, INT32* ypos) = 0;

#endif // !MOUSELESS

	/** Translate a coordinate in this OpView to the screen's global coordinate system. */
	virtual OpPoint ConvertToScreen(const OpPoint &point) = 0;

	/** Translate a coordinate from the screen's global to this OpView coordinate system. */
	virtual OpPoint ConvertFromScreen(const OpPoint &point) = 0;

# ifdef WIDGETS_IME_SUPPORT
	enum IME_MODE
	{
		IME_MODE_TEXT,		///< A text input form is focused
		IME_MODE_PASSWORD,	///< A password form is focused
		IME_MODE_UNKNOWN	///< Something else is focused
	};

	/** Specifies the context to which the input method belongs. */
	enum IME_CONTEXT
	{
		IME_CONTEXT_DEFAULT,	///< The default context when none of the other contexts apply. The input field associated with the IME doesn't belong to a submittable form.
		IME_CONTEXT_FORM,		///< The IME is associated with an input field that belongs to a submittable form.
		IME_CONTEXT_SEARCH		///< The IME is associated with an input field of type INPUT_SEARCH that belongs to a submittable form.
	};

	/** Abort the IM compose mode.
	 *
	 * It is called if a widget loses focus. It must commit or cancel
	 * the current compose string, so that the widget isn't left with
	 * an ongoing composition active. More info in OpInputMethod.h
	 *
	 * Also, the operating system can assume that the caret has been
	 * destroyed when this is called.
	 */
 	virtual void AbortInputMethodComposing() {}

	/** Set the current input method mode.
	 *
	 * When a password field is focused, it might not be a good idea
	 * to use IME at all, or at least be cautious about what to
	 * display, but this is up to the platform implementation.
	 *
	 * @param mode The mode to enter.
	 * @param context The input method context.
	 * @param istyle The istyle attribute of a formobject, if
	 * specified, NULL otherwise.
	 */
	virtual void SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle) {}

	/** The focused textfield has moved.
	 *
	 * An implementation should call
	 * OpInputMethodListener::GetWidgetInfo() to get updated
	 * information. */
	virtual void SetInputMoved() {}
	/** The focused textfield has changed its text.
	 *
	 * An implementation should call
	 * OpInputMethodListener::GetWidgetInfo() to get updated
	 * information. */
	virtual void SetInputTextChanged() {}

	/** The caret position has changed.
	  *
	  * @param rect The area occupied by the caret, relative to the OpView
	  */
	virtual void OnCaretPosChanged(const OpRect &rect) {}

# endif // WIDGETS_IME_SUPPORT

	/** The highlighted rectangle has changed.
	 *
	 * If editable fields have focus, the rectangle will follow the
	 * caret. If lists (SELECT element in HTML) have focus, it will
	 * follow the focused item. In spatial navigating/tabbing it will
	 * follow the highlighted rectangle.
	 *
	 * @param rect The new rectangle (relative to this OpView) for the
	 * highlight.
	 */
	virtual void OnHighlightRectChanged(const OpRect &rect) = 0;

	/** Get an OpPainter for this OpView.
	 *
	 * Sets up a paint context for the view and returns a
	 * painter. Only one painter per view may exist at a time. The
	 * painter is used to perform various draw operations on the
	 * view. ReleasePainter() must be called as soon as painting is
	 * done (this will flush the double-buffer, if any).
	 *
	 * @param rect The area to paint on, relative to the upper left corner of
	 * the rendering viewport, in screen coordinates. This will be the initial
	 * clip rectangle of the painter; no painting outside the rectangle will be
	 * visible. The rectangle will not affect coordinate translation on
	 * OpPainter methods.
	 * @param nobackbuffer Should ONLY be TRUE when painting directly
	 * to the screen (force single-buffer). If it is FALSE, there will
	 * be double-buffering if it has been switched on with
	 * UseDoublebuffer().
	 *
	 * @return A painter, or NULL on OOM.
	 */
	virtual OpPainter* GetPainter(const OpRect &rect, BOOL nobackbuffer = FALSE) = 0;

	/** Release the painter.
	 *
	 * This will commit the contents of the back-buffer
	 * (double-buffer) (if any) to the screen.
	 *
	 * @param rect The area you have painted on. Should be the same as
	 * passed to GetPainter().
	 */
	virtual void ReleasePainter(const OpRect &rect) = 0;

	/** Invalidate the specified area.
	 *
	 * This will schedule the specified area for repainting. It must
	 * not be done synchronously from within this method. Post a paint
	 * event instead.
	 *
	 * @param rect The area to invalidate, relative to the rendering viewport,
	 * in screen coordinates.
	 */
	virtual void Invalidate(const OpRect& rect) = 0;

	/** Is this view fixed-positioned inside the parent?
	 *
	 * If it is fixed-positioned, it must not be moved when the parent
	 * is scrolled.
	 */
	BOOL GetFixedPosition() { return is_fixed; }

	/** Mark this view as fixed positioned inside parent */
	void SetFixedPosition() { is_fixed = TRUE; }

	/** Find the top-level window.
	 *
	 * Traverse to the root window. Must be dynamic and must be able
	 * to go through parent OpViews to get there. it is needed to go
	 * from any OpWindow/OpView and to the OpWindow defining the UI,
	 * where there are no parents. Returns NULL when not supported.
	 *
	 * Fixme, who uses this?
	 */
	virtual OpWindow* GetRootWindow() { return NULL;}

#ifdef PLATFORM_FONTSWITCHING
	/** Set the preferred code page for this OpView.
	 *
	 * The font-switcher on the platform side needs this information
	 * when substituting fonts.
	 */
	virtual void SetPreferredCodePage(OpFontInfo::CodePage cp) = 0;
#endif
};

#endif // OPVIEW_H
