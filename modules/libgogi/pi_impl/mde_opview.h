/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_OPVIEW_H
#define MDE_OPVIEW_H

#include "modules/pi/OpView.h"
#include "modules/util/simset.h"
#include "modules/libgogi/mde.h"
#include "modules/libgogi/pi_impl/mde_widget.h"

#ifdef UTIL_LISTENERS
#include "modules/util/adt/oplisteners.h"
#endif // UTIL_LISTENERS

#ifndef VEGA_OPPAINTER_SUPPORT
class OpPainter;
#endif

class OpInputMethodString;
class MDE_OpWindow;
class MDE_Widget;

class MDE_OpView : public OpView
{
public:
	MDE_OpView();
	virtual ~MDE_OpView();
	OP_STATUS Init(OpWindow *parentWindow);
	OP_STATUS Init(OpView *parentView);

	virtual void UseDoublebuffer(BOOL doublebuffer);
	virtual void ScrollRect(const OpRect &rect, INT32 dx, INT32 dy);
	virtual void Scroll(INT32 dx, INT32 dy, BOOL full_update);
	virtual void Sync();
	virtual void LockUpdate(BOOL lock);
	virtual void SetPos(INT32 x, INT32 y);
	virtual void GetPos(INT32* x, INT32* y);
	virtual void SetSize(UINT32 w, UINT32 h);
	virtual void GetSize(UINT32* w, UINT32* h);
	virtual void SetVisibility(BOOL visible);
	virtual BOOL GetVisibility();
	virtual OP_STATUS SetCustomOverlapRegion(OpRect *rects, INT32 num_rects);
	virtual void SetAffectLowerRegions(BOOL affect_lower_regions);
#ifdef WIDGETS_IME_SUPPORT
	virtual void SetInputMethodListener(OpInputMethodListener* imlistener);
#endif // WIDGETS_IME_SUPPORT
	virtual void SetPaintListener(OpPaintListener* paintlistener);
#ifndef MOUSELESS
	virtual void SetMouseListener(OpMouseListener* mouselistener);
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	virtual void SetTouchListener(OpTouchListener* touchlistener);
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	virtual void SetDragListener(OpDragListener* draglistener);
	virtual OpDragListener* GetDragListener() const;
#endif // DRAG_SUPPORT
	virtual ShiftKeyState GetShiftKeys();
#ifndef MOUSELESS
	virtual BOOL GetMouseButton(MouseButton button);
#ifdef TOUCH_EVENTS_SUPPORT
	virtual void SetMouseButton(MouseButton button, bool set);
#endif // TOUCH_EVENTS_SUPPORT
	virtual void GetMousePos(INT32* xpos, INT32* ypos);
#endif // !MOUSELESS
	virtual OpPoint ConvertToScreen(const OpPoint &point);
	virtual OpPoint ConvertFromScreen(const OpPoint &point);
#ifdef WIDGETS_IME_SUPPORT
	virtual void AbortInputMethodComposing();
	virtual void SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle);
	virtual void SetInputMoved();
	virtual void SetInputTextChanged();
	virtual void CommitIME(const uni_char *text);
	virtual void UpdateIME(const uni_char *text, int cursorpos, int highlight_start, int highlight_len);
	virtual void UpdateIME(const uni_char *text);
	virtual OP_STATUS SubmitIME();
#endif // WIDGETS_IME_SUPPORT
	virtual void OnHighlightRectChanged(const OpRect &rect);
	virtual OpPainter* GetPainter(const OpRect &rect, BOOL nobackbuffer = FALSE);
	virtual void ReleasePainter(const OpRect &rect);
	virtual void Invalidate(const OpRect& rect);
#ifdef PLATFORM_FONTSWITCHING
	virtual void SetPreferredCodePage(OpFontInfo::CodePage cp);
#endif
	virtual MDE_OpWindow* GetParentWindow()		{ return parentWindow; }
	virtual MDE_OpView* GetParentView()			{ return parentView; }
	virtual OpWindow* GetRootWindow() { return GetMDEWidget()->GetRootWindow(); }

#ifdef VEGA_OPPAINTER_SUPPORT
	void SetBitmapPainter(OpPainter *painter)	{ bitmap_painter = painter; }
	OpPainter *GetBitmapPainter()				{ return bitmap_painter; }
#endif // VEGA_OPPAINTER_SUPPORT

	// non-interface functions
	MDE_Widget* GetMDEWidget()					{ return mdeWidget; }

#ifdef UTIL_LISTENERS
	virtual OP_STATUS AddOpViewListener(OpViewListener* listener)    { return m_listeners.Add(listener); }
	virtual OP_STATUS RemoveOpViewListener(OpViewListener* listener) { return m_listeners.Remove(listener); }
#endif // UTIL_LISTENERS

private:
	MDE_OpView *parentView;
	MDE_OpWindow *parentWindow;
	Head childViews;

#ifdef UTIL_LISTENERS
	OpListeners<OpViewListener> m_listeners;
#endif // UTIL_LISTENERS

	MDE_Widget* mdeWidget;
#ifdef VEGA_OPPAINTER_SUPPORT
	VEGAOpPainter *painter;
	OpPainter *bitmap_painter;
#else
	OpPainter* painter;
#endif
	INT32 numPainters;
#ifdef MDE_DOUBLEBUF_SUPPORT
	BOOL doubleBuffer;
	MDE_BUFFER *backbuf;
	MDE_BUFFER frontbuf;
#endif

#ifdef WIDGETS_IME_SUPPORT
	OpInputMethodListener *imeListener;
	OpInputMethodString *imestr;
	void *currentIME;
	BOOL commiting;
	BOOL spawning;
	BOOL aborting;
#endif // WIDGETS_IME_SUPPORT
};

#endif // MDE_OPVIEW_H
