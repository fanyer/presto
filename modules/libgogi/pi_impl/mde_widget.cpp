/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/pi/OpView.h"
#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/libvega/src/oppainter/vegaoppainter.h"
# include "modules/skin/OpSkinManager.h"
# include "modules/skin/OpSkinElement.h"
#else
# include "modules/libgogi/pi_impl/mde_oppainter.h"
#endif

#include "modules/libgogi/pi_impl/mde_widget.h"
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

#ifdef VEGA_OPPAINTER_SUPPORT
MDE_Widget::MDE_Widget(OpRect size)
{
#else
MDE_Widget::MDE_Widget(OpPainter *painter, OpRect size)
{
#endif
	paintListener = NULL;
	view = NULL;
	window = NULL;
	is_shadow = false;
#ifndef VEGA_OPPAINTER_SUPPORT
	this->painter = painter;
#endif
#ifndef MOUSELESS
	mouseListener = NULL;
	mouseButton[0] = mouseButton[1] = mouseButton[2] = false;
	mx = my = 0;
#endif // MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	touchListener = NULL;
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	dragListener = NULL;
#endif // DRAG_SUPPORT
	m_rect.x = size.x;
	m_rect.y = size.y;
	m_rect.w = size.width;
	m_rect.h = size.height;
	is_cache_invalid = true;

	visListener = NULL;
}

MDE_Widget::~MDE_Widget()
{
	/*
	   Workaround for when a window with live child windows is
	   deleted - see CORE-39756.

	   Deleting a window deletes its mdeWidget along with its child
	   views, including the mdeWidget belonging to any live child
	   window. To work around this any window associated with an
	   MDE_Widget must be deleted along with the widget.
	 */
	if (window)
		reinterpret_cast<MDE_OpWindow*>(window)->WidgetDeleted();

	if (m_parent)
		m_parent->RemoveChild(this);
}

void MDE_Widget::SetPaintListener(OpPaintListener *pl, OpView *view)
{
	OP_ASSERT(this->view==NULL||this->view==view);
	this->view=view;
	paintListener = pl;
}

#ifndef MOUSELESS
void MDE_Widget::SetMouseListener(OpMouseListener *ml, OpView *view)
{
	OP_ASSERT(this->view==NULL||this->view==view);
	this->view=view;
	mouseListener = ml;
}
OpMouseListener* MDE_Widget::GetMouseListener()
{
	return this->mouseListener;
}
#endif // MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
void MDE_Widget::SetTouchListener(OpTouchListener *listener, OpView *view)
{
	OP_ASSERT(this->view == NULL || this->view == view);
	this->view = view;
	touchListener = listener;
}
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
void MDE_Widget::SetDragListener(OpDragListener *drag_listener, OpView *view)
{
	OP_ASSERT(this->view == NULL || this->view == view);
	this->view = view;
	dragListener = drag_listener;
}
#endif // DRAG_SUPPORT

bool MDE_Widget::OnBeforePaintEx()
{
	//MDE_OpWindow *mdewin = (MDE_OpWindow*) GetParentWindow();
	//FIX: Should we do this?
	//if (mdewin && mdewin->GetCacheBitmap() && !is_cache_invalid)
	//	return true;
	if (paintListener)
		return paintListener->BeforePaint() ? true : false;
	return true;
}

OpWindow *MDE_Widget::GetParentWindow()
{
	MDE_OpView *v = (MDE_OpView*) view;
	MDE_OpWindow *mde_win = (MDE_OpWindow*) window;
	while (!mde_win && v)
	{
		mde_win = v->GetParentWindow();
		v = v->GetParentView();
	}
	return mde_win;
}

OpWindow* MDE_Widget::GetRootWindow()
{
	OpWindow *window = NULL;
	MDE_View *v = this;
	while (v)
	{
		if (v->IsType("MDE_Widget") && ((MDE_Widget*)v)->GetOpWindow())
			window = ((MDE_Widget*)v)->GetOpWindow();
		v = v->m_parent;
	}
	return window;
}

INT32 MDE_Widget::GetWindowTransparency()
{
	MDE_OpWindow *mde_win = (MDE_OpWindow*) GetParentWindow();
	return mde_win ? mde_win->GetTransparency() : 255;
}

void MDE_Widget::OnInvalidSelf()
{
	MDE_OpWindow *mdewin = (MDE_OpWindow*) GetParentWindow();
	if (mdewin && mdewin->GetCacheBitmap() && !is_cache_invalid)
		is_cache_invalid = true;
}

bool MDE_Widget::GetScrollMoveAllowed()
{
	MDE_OpWindow *mdewin = (MDE_OpWindow*) GetParentWindow();
	if (mdewin && mdewin->GetCacheBitmap())
		return false;
	return true;
}

void MDE_Widget::OnRectChanged(const MDE_RECT &old_rect)
{
	MDE_OpWindow *mde_win = (MDE_OpWindow*) window;
	if (mde_win && !is_shadow)
		mde_win->OnRectChanged(old_rect);
}

void MDE_Widget::InvalidateCache()
{
	is_cache_invalid = true;
	MDE_View *tmp = m_first_child;
	while (tmp)
	{
		if (tmp->IsType("MDE_Widget"))
			((MDE_Widget*)tmp)->InvalidateCache();
		tmp = tmp->m_next;
	}
}

void MDE_Widget::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	if( paintListener )
	{
		OP_ASSERT(view);

		OpBitmap *back_buffer = NULL;

		MDE_OpWindow *mdewin = (MDE_OpWindow*) GetParentWindow();
		if (mdewin && mdewin->GetCacheBitmap())
		{
			if (is_cache_invalid)
			{
				is_cache_invalid = false;
				paintListener->OnPaint(OpRect(rect.x, rect.y, rect.w, rect.h), view);
				//OutputDebugString(L"Painting to the buffer\n");
			}
			back_buffer = mdewin->GetCacheBitmap();
		}

#ifdef VEGA_OPPAINTER_SUPPORT
		VEGAOpPainter* painter = m_screen->GetVegaPainter();
		OP_ASSERT(painter);
		int x = 0;
		int y = 0;
		ConvertToScreen(x, y);
		painter->SetVegaTranslation(x, y);

#else
#ifndef MDE_PLATFORM_IMPLEMENTS_OPPAINTER
		((MDE_OpPainter*)painter)->beginPaint(screen);
#endif // !MDE_PLATFORM_IMPLEMENTS_OPPAINTER
#endif

		// FIXME: should I set an initial cliprect?
		if (OpStatus::IsError(painter->SetClipRect(OpRect(rect.x, rect.y, rect.w, rect.h))))
		{
#if !defined(VEGA_OPPAINTER_SUPPORT) && !defined(MDE_PLATFORM_IMPLEMENTS_OPPAINTER)
			((MDE_OpPainter*)painter)->endPaint();
#endif // !VEGA_OPPAINTER_SUPPORT && !MDE_PLATFORM_IMPLEMENTS_OPPAINTER
			return;
		}

		if (back_buffer)
		{
#ifdef VEGA_OPPAINTER_SUPPORT
			INT32 win_transp = GetWindowTransparency();
			INT32 old_transp = painter->GetImageOpacity();
			painter->SetImageOpacity(old_transp * win_transp / 255);
#endif

			if (m_is_transparent)
				painter->DrawBitmapAlpha(mdewin->GetCacheBitmap(), OpPoint(0, 0));
			else
				painter->DrawBitmap(mdewin->GetCacheBitmap(), OpPoint(0, 0));
			//OutputDebugString(L"Using buffer\n");

#ifdef VEGA_OPPAINTER_SUPPORT
			painter->SetImageOpacity(old_transp);
#endif
		}
		else
		{
#ifdef VEGA_OPPAINTER_SUPPORT
		BOOL has_opacity = FALSE;
		INT32 win_transp = GetWindowTransparency();
		if (win_transp != 255)
			has_opacity = OpStatus::IsSuccess(painter->BeginOpacity(OpRect(rect.x, rect.y, rect.w, rect.h), win_transp));
#endif
		// set some stuff in the painter
		painter->SetRenderingHint(m_screen->IsResizing() ? OpPainter::Speed : OpPainter::Quality);
		paintListener->OnPaint(OpRect(rect.x, rect.y, rect.w, rect.h), view);

#ifdef VEGA_OPPAINTER_SUPPORT
		if (has_opacity)
			painter->EndOpacity();
#endif
		}

		//Debug
		//painter->SetColor(255, 0, 255, 255);
		//painter->DrawRect(OpRect(0, 0, m_rect.w, m_rect.h), 1);

		painter->RemoveClipRect();

#if !defined(VEGA_OPPAINTER_SUPPORT) && !defined(MDE_PLATFORM_IMPLEMENTS_OPPAINTER)
		((MDE_OpPainter*)painter)->endPaint();
#endif // !VEGA_OPPAINTER_SUPPORT && !MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	} else {
#ifndef VEGA_OPPAINTER_SUPPORT
		MDE_View::OnPaint(rect, screen);
#else
		VEGAOpPainter* painter = m_screen->GetVegaPainter();
		OP_ASSERT(painter);
		int x = 0;
		int y = 0;
		ConvertToScreen(x, y);
		painter->SetVegaTranslation(x, y);
		BOOL has_clip = OpStatus::IsSuccess(painter->SetClipRect(OpRect(rect.x, rect.y, rect.w, rect.h)));

		if (is_shadow)
		{
			INT32 win_transp = GetWindowTransparency();

#ifdef SKIN_SUPPORT
			OpSkinElement *skin = NULL;
			if (!is_fullscreen_shadow)
			{
				g_skin_manager->GetSkinElement("Overlay Shadow Skin", SKINTYPE_DEFAULT, SKINSIZE_DEFAULT, FALSE);
				if (!skin)
					skin = g_skin_manager->GetSkinElement("Overlay Shadow Skin", SKINTYPE_DEFAULT, SKINSIZE_DEFAULT, TRUE);
			}

			if (skin)
			{
				Image img = skin->GetImage(0);
				OpBitmap *bitmap = img.GetBitmap(NULL);
				if (bitmap)
				{
					INT32 old_transp = painter->GetImageOpacity();
					painter->SetImageOpacity(old_transp * win_transp / 255);

					int e = (bitmap->Width() - 1) / 2;
					int bw = bitmap->Width();
					int bh = bitmap->Height();
					// FIX: We need to clip if the window is transparent. Lazy hack for now is to remove hidden parts below (since size and position is hardcoded anyway)
					// Corners
					//painter->DrawBitmapClippedAlpha(bitmap, OpRect(0, 0, e, e), OpPoint(0, 0));
					painter->DrawBitmapClippedAlpha(bitmap, OpRect(bw - e, 0, e, e), OpPoint(m_rect.w - e, 0));
					painter->DrawBitmapClippedAlpha(bitmap, OpRect(0, bh - e, e, e), OpPoint(0, m_rect.h - e));
					painter->DrawBitmapClippedAlpha(bitmap, OpRect(bw - e, bh - e, e, e), OpPoint(m_rect.w - e, m_rect.h - e));
					// Left & right edge
					//painter->DrawBitmapScaledAlpha(bitmap, OpRect(0, e, e, bh - e * 2), OpRect(0, e, e, m_rect.h - e * 2));
					painter->DrawBitmapScaledAlpha(bitmap, OpRect(bw - e, e, e, bh - e * 2), OpRect(m_rect.w - e, e, e, m_rect.h - e * 2));
					// Top & bottom edge
					//painter->DrawBitmapScaledAlpha(bitmap, OpRect(e, 0, bw - e * 2, e), OpRect(e, 0, m_rect.w - e * 2, e));
					painter->DrawBitmapScaledAlpha(bitmap, OpRect(e, bh - e, bw - e * 2, e), OpRect(e, m_rect.h - e, m_rect.w - e * 2, e));
					img.ReleaseBitmap();

					painter->SetImageOpacity(old_transp);
				}
			}
			else
#endif // SKIN_SUPPORT
			{
				painter->SetColor(0, 0, 0, 90 * win_transp / 255);
				painter->FillRect(OpRect(rect.x, rect.y, rect.w, rect.h));
			}
		}
		else if (!m_is_transparent && !m_screen->UseTransparentBackground())
		{
			painter->SetColor(128, 128, 128, 255);
			painter->FillRect(OpRect(rect.x, rect.y, rect.w, rect.h));
		}
		if (has_clip)
			painter->RemoveClipRect();
#endif
	}
}

bool MDE_Widget::Paint(OpPainter *painter)
{
	OP_ASSERT(painter);
#ifdef VEGA_OPPAINTER_SUPPORT
	bool ret = false;
	if (paintListener && view)
	{
		VEGAOpPainter* vpainter = (VEGAOpPainter*) painter;

		OpPainter *old_bitmap_painter = ((MDE_OpView*)view)->GetBitmapPainter();
		((MDE_OpView*)view)->SetBitmapPainter(vpainter);

		paintListener->OnPaint(OpRect(0, 0, m_rect.w, m_rect.h), view);

		((MDE_OpView*)view)->SetBitmapPainter(old_bitmap_painter);
		ret = true;
	}
	MDE_View *tmp = m_first_child;
	while (tmp)
	{
		if (tmp->IsType("MDE_Widget"))
			ret |= ((MDE_Widget*)tmp)->Paint(painter);
		tmp = tmp->m_next;
	}
	return ret;
#else
	return false;
#endif
}

#ifndef MOUSELESS
void MDE_Widget::OnMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate)
{
	if (x != mx || y != my)
	{
		// Since we don't have any x, y parameters in the OpView interface, we have to emulate mousemove properly by sending OnMouseMove.
		OnMouseMove(x, y, keystate);
	}
	// update mouse position
	if( mouseListener ){
		OP_ASSERT(view);
		switch(button){
		case 1:
			mouseButton[MOUSE_BUTTON_1] = true;
			mouseListener->OnMouseDown(MOUSE_BUTTON_1, clicks, keystate, view);
			break;
		case 2:
			mouseButton[MOUSE_BUTTON_2] = true;
			mouseListener->OnMouseDown(MOUSE_BUTTON_2, clicks, keystate, view);
			break;
		case 3:
			mouseButton[MOUSE_BUTTON_3] = true;
			mouseListener->OnMouseDown(MOUSE_BUTTON_3, clicks, keystate, view);
			break;
		default:
			OP_ASSERT(FALSE);
		}
	}
}

void MDE_Widget::OnMouseUp(int x, int y, int button, ShiftKeyState keystate)
{
	// update mouse position
	if( mouseListener ){
		OP_ASSERT(view);
		switch(button){
		case 1:
			mouseButton[MOUSE_BUTTON_1] = false;
			mouseListener->OnMouseUp(MOUSE_BUTTON_1, keystate, view);
			break;
		case 2:
			mouseButton[MOUSE_BUTTON_2] = false;
			mouseListener->OnMouseUp(MOUSE_BUTTON_2, keystate, view);
			break;
		case 3:
			mouseButton[MOUSE_BUTTON_3] = false;
			mouseListener->OnMouseUp(MOUSE_BUTTON_3, keystate, view);
			break;
		default:
			OP_ASSERT(FALSE);
		}
	}
}

void MDE_Widget::OnMouseMove(int x, int y, ShiftKeyState keystate)
{
	// update the mouse position
	if( mouseListener ){
		OP_ASSERT(view);
		mx = x;
		my = y;
		mouseListener->OnMouseMove(OpPoint(x,y), keystate, view);
	}
}

/** Positive is a turn against the user. return true if you implement it. If it return false, the parent will get it. */
bool MDE_Widget::OnMouseWheel(int delta, bool vertical, ShiftKeyState keystate)
{
	if (mouseListener)
		return mouseListener->OnMouseWheel(delta, vertical, keystate) ? true : false;
	return false;
}

void MDE_Widget::OnMouseLeave()
{
	if (mouseListener)
		mouseListener->OnMouseLeave();
}

void MDE_Widget::OnMouseCaptureRelease()
{
	if (mouseListener)
		mouseListener->OnMouseCaptureRelease();
}

void MDE_Widget::GetMousePos(int *x, int *y)
{
	if (m_screen)
	{
		(*x) = m_screen->m_mouse_x;
		(*y) = m_screen->m_mouse_y;
	}
	else
		*x = *y = 0;
	ConvertFromScreen(*x, *y);
}

bool MDE_Widget::GetMouseButton(int button){
	return mouseButton[button];
}

#ifdef TOUCH_EVENTS_SUPPORT
void MDE_Widget::SetMouseButton(int button, bool set)
{
	OP_ASSERT(button >= MOUSE_BUTTON_1 && button <= MOUSE_BUTTON_3);
	mouseButton[button] = set;
}
#endif // TOUCH_EVENTS_SUPPORT

bool MDE_Widget::GetHitStatus(int x, int y)
{
	if (is_shadow && !is_fullscreen_shadow)
		return false;

	if (!MDE_View::GetHitStatus(x, y))
		return false;

	// Check if the window has ignore-mouse-input flag set.
	MDE_OpView *v = (MDE_OpView*) view;
	MDE_OpWindow *mde_win = (MDE_OpWindow*) window;
	while (!mde_win && v)
	{
		mde_win = v->GetParentWindow();
		v = v->GetParentView();
	}
	if (mde_win && mde_win->GetIgnoreMouseInput())
		return false;

	if (mouseListener)
		return mouseListener->GetHitStatus(x, y, v);

	return true;
}

#endif // MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
void MDE_Widget::OnTouchDown(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data)
{
	if( touchListener ){
		OP_ASSERT(view);

		/* Make primary touch affect mouse state. */
		if (id == 0)
			mouseButton[MOUSE_BUTTON_1] = true;

		touchListener->OnTouchDown(id, OpPoint(x, y), radius, modifiers, view, user_data);
	}
}

void MDE_Widget::OnTouchUp(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data)
{
	if( touchListener ){
		OP_ASSERT(view);

		/* Make primary touch affect mouse state. */
		if (id == 0)
			mouseButton[MOUSE_BUTTON_1] = false;

		touchListener->OnTouchUp(id, OpPoint(x, y), radius, modifiers, view, user_data);
	}
}

void MDE_Widget::OnTouchMove(int id, int x, int y, int radius, ShiftKeyState modifiers, void* user_data)
{
	if( touchListener ){
		OP_ASSERT(view);

		/* Make primary touch affect mouse state. */
		if (id == 0){
			mx = x;
			my = y;
		}

		touchListener->OnTouchMove(id, OpPoint(x, y), radius, modifiers, view, user_data);
	}
}
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
void MDE_Widget::OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		dragListener->OnDragStart(view, OpPoint(start_x, start_y), modifiers, OpPoint(current_x, current_y));
	}
}

void MDE_Widget::OnDragDrop(int x, int y, ShiftKeyState modifiers)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		dragListener->OnDragDrop(view, OpPoint(x, y), modifiers);
	}
	else
		g_drag_manager->StopDrag();
}

void MDE_Widget::OnDragEnd(int x, int y, ShiftKeyState modifiers)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		dragListener->OnDragEnd(view, OpPoint(x, y), modifiers);
	}
	else
		g_drag_manager->StopDrag();
}

void MDE_Widget::OnDragMove(int x, int y, ShiftKeyState modifiers)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		dragListener->OnDragMove(view, OpPoint(x, y), modifiers);
	}
}

void MDE_Widget::OnDragLeave(ShiftKeyState modifiers)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		dragListener->OnDragLeave(view, modifiers);
	}
}

void MDE_Widget::OnDragEnter(int x, int y, ShiftKeyState modifiers)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		dragListener->OnDragEnter(view, OpPoint(x, y), modifiers);
	}
}

void MDE_Widget::OnDragCancel(ShiftKeyState modifiers)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		dragListener->OnDragCancel(view, modifiers);
	}
	else
		g_drag_manager->StopDrag(TRUE);
}

void MDE_Widget::OnDragUpdate(int x, int y)
{
	if(dragListener)
	{
		OP_ASSERT(view);

		// The ShiftkeyState is not too important here.
		dragListener->OnDragMove(view, OpPoint(x, y), SHIFTKEY_NONE);
	}
}
#endif // DRAG_SUPPORT

void MDE_Widget::ScrollRect(const OpRect& rect, INT32 dx, INT32 dy, bool includeChildren)
{
	MDE_View::ScrollRect(MDE_MakeRect(rect.x, rect.y,
				rect.width, rect.height), dx, dy, includeChildren);
}
