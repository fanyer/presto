/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_WIDGET_H
#define MDE_WIDGET_H

#include "modules/libgogi/mde.h"
#include "modules/pi/OpWindow.h"

class OpPaintListener;
class OpMouseListener;
class OpTouchListener;
class OpDragListener;
class OpView;

#ifndef VEGA_OPPAINTER_SUPPORT
class OpPainter;
#endif

class MDE_WidgetVisibilityListener
{
public:
	virtual ~MDE_WidgetVisibilityListener(){}
	virtual void OnVisibilityChanged(bool vis) = 0;
};


class MDE_Widget : public MDE_View
{
public:
#ifdef VEGA_OPPAINTER_SUPPORT
	MDE_Widget(OpRect size);
#else
	MDE_Widget(OpPainter *painter, OpRect size);
#endif
	~MDE_Widget();
	
	void SetOpWindow(OpWindow* win){window = win;}
	void SetOpView(OpView* v){view = v;}
	OpWindow* GetOpWindow(){return window;}
	OpView* GetOpView(){return view;}
	OpWindow *GetParentWindow();
	OpWindow* GetRootWindow();
	void SetIsShadow(bool shadow, bool fullscreen) { is_shadow = shadow; is_fullscreen_shadow = fullscreen; }

	void SetPaintListener(OpPaintListener *pl, OpView *view);
	bool OnBeforePaintEx();
	void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);
	void OnRectChanged(const MDE_RECT &old_rect);
	void OnInvalidSelf();
	bool GetScrollMoveAllowed();
	bool Paint(OpPainter *painter);

#ifndef MOUSELESS
	void SetMouseListener(OpMouseListener *ml, OpView *view);
	OpMouseListener* GetMouseListener();

	void OnMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate);
	void OnMouseUp(int x, int y, int button, ShiftKeyState keystate);
	void OnMouseMove(int x, int y, ShiftKeyState keystate);
	/** Positive is a turn against the user. return true if you implement it. If it return false, the parent will get it. */
	bool OnMouseWheel(int delta, bool vertical, ShiftKeyState keystate);
	void OnMouseLeave();
	void OnMouseCaptureRelease();

	void GetMousePos(int *x, int *y);
	bool GetMouseButton(int button);
#ifdef TOUCH_EVENTS_SUPPORT
	void SetMouseButton(int button, bool set);
#endif // TOUCH_EVENTS_SUPPORT
	bool GetHitStatus(int x, int y);
#endif // MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	void SetTouchListener(OpTouchListener* listener, OpView *view);

	void OnTouchDown(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);
	void OnTouchUp(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);
	void OnTouchMove(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
	void SetDragListener(OpDragListener *drag_listener, OpView *view);
	OpDragListener* GetDragListener() { return dragListener; }
	void OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y);
	void OnDragDrop(int x, int y, ShiftKeyState modifiers);
	void OnDragEnd(int x, int y, ShiftKeyState modifiers);
	void OnDragMove(int x, int y, ShiftKeyState modifiers);
	void OnDragLeave(ShiftKeyState modifiers);
	void OnDragCancel(ShiftKeyState modifiers);
	void OnDragEnter(int x, int y, ShiftKeyState modifiers);
	void OnDragUpdate(int x, int y);
#endif // DRAG_SUPPORT

	void ScrollRect(const OpRect& rect, INT32 dx, INT32 dy, bool includeChildren);

	virtual bool GetOnVisibilityChangeWanted() { return window ? true : false; }
	virtual void OnVisibilityChanged(bool vis){if (visListener) visListener->OnVisibilityChanged(vis);}
	void SetVisibilityListener(MDE_WidgetVisibilityListener* v){visListener = v;}	

	bool IsFullscreenShadow(){return is_shadow && is_fullscreen_shadow;}

	virtual bool IsType(const char* type){if (!is_shadow && !op_strcmp(type, "MDE_Widget")) return true; return MDE_View::IsType(type);}

	/** Invalidate the cached backbuffer for this widget and all child widgets */
	void InvalidateCache();
private:
	OpPaintListener *paintListener;
#ifndef VEGA_OPPAINTER_SUPPORT
	OpPainter *painter;
#endif
#ifndef MOUSELESS
	OpMouseListener *mouseListener;
	bool mouseButton[3];
	int mx, my;
#endif // MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	OpTouchListener *touchListener;
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	OpDragListener *dragListener;
#endif // DRAG_SUPPORT

	OpWindow* window;
	OpView *view; // needed for listeners
	bool is_shadow;
	bool is_fullscreen_shadow;
	bool is_cache_invalid;

	MDE_WidgetVisibilityListener* visListener;

	INT32 GetWindowTransparency();
};

#endif // MDE_WIDGET_H

