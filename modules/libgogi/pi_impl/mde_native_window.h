/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MDENATIVEWINDOW_H
#define MDENATIVEWINDOW_H

#ifdef MDE_NATIVE_WINDOW_SUPPORT
#include "modules/libgogi/mde.h"

#ifdef DRAG_SUPPORT
class OpDragListener;
class OpView;
#endif // DRAG_SUPPORT

/** Helper class to track the visible region of a native window. */
class MDENativeWindow : public MDE_View, public Link, public MessageObject
{
public:
    MDENativeWindow(const MDE_RECT& rect);
    virtual ~MDENativeWindow();

    static void UpdateRegionsForScreen(MDE_Screen* scr);

    void OnPaint(const MDE_RECT& rect, MDE_BUFFER* screen);
    void OnRegionInvalid();
    void UpdateRegion();
    virtual bool IsType(const char* type){if (!op_strcmp(type, "MDENativeWindow")) return true; return MDE_View::IsType(type);}
    virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef DRAG_SUPPORT
	virtual void SetDragListener(OpDragListener* drag_listener, OpView* view);
	virtual void OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y);
	virtual void OnDragDrop(int x, int y, ShiftKeyState modifiers);
	virtual void OnDragEnd(int x, int y, ShiftKeyState modifiers);
	virtual void OnDragMove(int x, int y, ShiftKeyState modifiers);
	virtual void OnDragLeave(ShiftKeyState modifiers);
	virtual void OnDragCancel(ShiftKeyState modifiers);
	virtual void OnDragEnter(int x, int y, ShiftKeyState modifiers);
#endif // DRAG_SUPPORT

    // Functions to be called from the platform size
    /** Move the native window.  This will trigger a move of the
     * visibility tracking and then call MoveWindow to move the
     * actual window. The actual move is not triggered immediatly, it is
     * synchronized with the mde state. */
    void Move(int x, int y);
    /** Resize the native window.  This will trigger a resize of the
     * visibility tracking and then call ResizeWindow to resize the
     * actual window. */
    void Resize(int w, int h);
    /** Hide or show the window. Will call ShowWindow to change the
     * visibility of the window after extra region checks. The actual
     * visibility change is not always immediate as it synchronizes with
     * mde. */
    void Show(BOOL vis);

    void SetClearBackground(BOOL clear_background) { m_clear_background = clear_background; }

    // Functions to implement on the platform side
    /** Move the platform window to a new position. All offsets are
     * already added so just moving to the specified x, y is needed. */
    virtual void MoveWindow(int x, int y) = 0;
    /** Resize the platform window to a new size. */
    virtual void ResizeWindow(int w, int h) = 0;
    /** Update the window, trigger a redraw immediatly. */
    virtual void UpdateWindow() = 0;
    /** Set a window to visible / no visible based on the 'show' flag. */
    virtual void ShowWindow(BOOL show) = 0;
    /** Set the redirection status of a window. A redirected window should
     * not be visible, but it should be possible to get the content of the
     * window in the UpdateBackbuffer call. Non redirected windows works
     * as normal and does not need to support UpdateBackbuffer. */
    virtual void SetRedirected(BOOL redir) = 0;
    /** Copy the content of a window to a bitmap backbuffer.
     * This function is only called when the window is set to 'redirected'.
     */
    virtual void UpdateBackbuffer(OpBitmap* backbuffer) = 0;
    virtual void SetClipRegion(MDE_Region* rgn) = 0;
	virtual BOOL RecreateBackbufferIfNeeded();
private:
    BOOL m_window_visible;
    BOOL m_window_should_be_visible;
    BOOL m_region_invalid;
    int m_old_winy;

    BOOL m_redirected;
    OpBitmap* m_backbuffer;
    BOOL m_callback_set;
    BOOL m_clear_background;

#ifdef DRAG_SUPPORT
	OpDragListener* m_drag_listener;
	OpView* m_view;
#endif // DRAG_SUPPORT
};
#endif // MDE_NATIVE_WINDOW_SUPPORT

#endif // MDENATIVEWINDOW_H
