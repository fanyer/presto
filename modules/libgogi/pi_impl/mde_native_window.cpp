/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef MDE_NATIVE_WINDOW_SUPPORT
#include "modules/libgogi/pi_impl/mde_native_window.h"
#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif // VEGA_OPPAINTER_SUPPORT
#include "modules/hardcore/mh/mh.h"
#include "modules/pi/OpView.h"
#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

#define MDE_NATIVE_WINDOW_MAX_FRAMERATE 20

void MDENativeWindow::UpdateRegionsForScreen(MDE_Screen* scr)
{
    Link* view;
    for (view = g_mdeNativeWindows.First(); view; view = view->Suc())
    {
        if (((MDENativeWindow*)view)->m_screen == scr)
            ((MDENativeWindow*)view)->UpdateRegion();
    }
    for (view = g_mdeNativeWindows.First(); view; view = view->Suc())
    {
        if (((MDENativeWindow*)view)->m_screen == scr)
            ((MDENativeWindow*)view)->UpdateWindow();
    }
}

MDENativeWindow::MDENativeWindow(const MDE_RECT& rect) : m_window_visible(FALSE),
    m_window_should_be_visible(FALSE), m_region_invalid(FALSE),
    m_old_winy(0), m_redirected(FALSE), m_backbuffer(NULL), m_callback_set(FALSE),
    m_clear_background(TRUE)
#ifdef DRAG_SUPPORT
	, m_drag_listener(NULL), m_view(NULL)
#endif // DRAG_SUPPORT
{
    Into(&g_mdeNativeWindows);
    SetRect(rect);
}

MDENativeWindow::~MDENativeWindow()
{
    if (m_callback_set)
    {
        g_main_message_handler->RemoveDelayedMessage(MSG_MDE_NATIVE_WINDOW_UPDATE, (MH_PARAM_1)this, 0);
        g_main_message_handler->UnsetCallBack(this, MSG_MDE_NATIVE_WINDOW_UPDATE, (MH_PARAM_1)this);
    }
    Out();
    OP_DELETE(m_backbuffer);
}

BOOL MDENativeWindow::RecreateBackbufferIfNeeded()
{
	BOOL recreated = FALSE;
	if (m_backbuffer && ((int)m_backbuffer->Width() != m_rect.w || (int)m_backbuffer->Height() != m_rect.h))
	{
		OP_DELETE(m_backbuffer);
		m_backbuffer = NULL;
		recreated = TRUE;
	}
	if (!m_backbuffer)
	{
		if (OpStatus::IsError(OpBitmap::Create(&m_backbuffer, m_rect.w, m_rect.h)))
		{
			m_backbuffer = NULL;
			return FALSE;
		}
	}
	return recreated;
}

void MDENativeWindow::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    OP_ASSERT(msg == MSG_MDE_NATIVE_WINDOW_UPDATE);
	RecreateBackbufferIfNeeded();
	if (m_backbuffer)
	    UpdateBackbuffer(m_backbuffer);
    Invalidate(MDE_MakeRect(0,0,m_rect.w,m_rect.h));
    g_main_message_handler->PostDelayedMessage(MSG_MDE_NATIVE_WINDOW_UPDATE, (MH_PARAM_1)this, 0, 1000/MDE_NATIVE_WINDOW_MAX_FRAMERATE);
}

#ifdef DRAG_SUPPORT
/*virtual*/
void MDENativeWindow::SetDragListener(OpDragListener* drag_listener, OpView* view)
{
	if (m_drag_listener)
		OP_ASSERT(m_view);
	m_drag_listener = drag_listener;
	m_view = view;
}

/*virtual*/
void MDENativeWindow::OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y)
{
	if(m_drag_listener)
	{
		OP_ASSERT(m_view);
		m_drag_listener->OnDragStart(m_view, OpPoint(start_x, start_y), modifiers, OpPoint(current_x, current_y));
	}
}

/*virtual*/
void MDENativeWindow::OnDragDrop(int x, int y, ShiftKeyState modifiers)
{
	if(m_drag_listener)
	{
		OP_ASSERT(m_view);
		m_drag_listener->OnDragDrop(m_view, OpPoint(x, y), modifiers);
	}
	else
		g_drag_manager->StopDrag();
}

/*virtual*/
void MDENativeWindow::OnDragEnd(int x, int y, ShiftKeyState modifiers)
{
	if(m_drag_listener)
	{
		OP_ASSERT(m_view);
		m_drag_listener->OnDragEnd(m_view, OpPoint(x, y), modifiers);
	}
	else
		g_drag_manager->StopDrag();
}

/*virtual*/
void MDENativeWindow::OnDragMove(int x, int y, ShiftKeyState modifiers)
{
	if(m_drag_listener)
	{
		OP_ASSERT(m_view);
		m_drag_listener->OnDragMove(m_view, OpPoint(x, y), modifiers);
	}
}

/*virtual*/
void MDENativeWindow::OnDragLeave(ShiftKeyState modifiers)
{
	if(m_drag_listener)
	{
		OP_ASSERT(m_view);
		m_drag_listener->OnDragLeave(m_view, modifiers);
	}
}

/*virtual*/
void MDENativeWindow::OnDragCancel(ShiftKeyState modifiers)
{
	if(m_drag_listener)
	{
		OP_ASSERT(m_view);
		m_drag_listener->OnDragCancel(m_view, modifiers);
	}
	else
		g_drag_manager->StopDrag(TRUE);
}

/*virtual*/
void MDENativeWindow::OnDragEnter(int x, int y, ShiftKeyState modifiers)
{
	if(m_drag_listener)
	{
		OP_ASSERT(m_view);
		m_drag_listener->OnDragEnter(m_view, OpPoint(x, y), modifiers);
	}
}
#endif // DRAG_SUPPORT

void MDENativeWindow::OnPaint(const MDE_RECT& rect, MDE_BUFFER* screen)
{
    if (m_num_overlapping_transparentviews>0)
    {
        if (RecreateBackbufferIfNeeded())
            UpdateBackbuffer(m_backbuffer);
        if (!m_redirected)
        {
            SetRedirected(TRUE);
            if (!m_callback_set)
            {
                if (OpStatus::IsSuccess(g_main_message_handler->SetCallBack(this, MSG_MDE_NATIVE_WINDOW_UPDATE, (MH_PARAM_1)this)))
                    m_callback_set = TRUE;
            }
            if (m_callback_set)
                g_main_message_handler->PostDelayedMessage(MSG_MDE_NATIVE_WINDOW_UPDATE, (MH_PARAM_1)this, 0, 1000/MDE_NATIVE_WINDOW_MAX_FRAMERATE);
            m_redirected = TRUE;
        }
    }
    else
    {
        if (m_redirected)
        {
            if (m_callback_set)
                g_main_message_handler->RemoveDelayedMessage(MSG_MDE_NATIVE_WINDOW_UPDATE, (MH_PARAM_1)this, 0);
            m_redirected = FALSE;
            UpdateRegion();
            SetRedirected(FALSE);

			// We don't need the backbuffer anymore
			OP_DELETE(m_backbuffer);
			m_backbuffer = NULL;
        }
    }

#ifdef VEGA_OPPAINTER_SUPPORT
    class VEGAOpPainter* painter = (class VEGAOpPainter*)screen->user_data;
    painter->SetVegaTranslation(0, 0);
    int xp = rect.x;
    int yp = rect.y;
    ConvertToScreen(xp, yp);
    painter->SetClipRect(OpRect(xp, yp, rect.w, rect.h));
    xp = 0;
    yp = 0;
    ConvertToScreen(xp, yp);

    if (m_redirected)
    {
		if (m_backbuffer)
			painter->DrawBitmap(m_backbuffer, OpPoint(xp, yp));
    }
    else if(m_clear_background)
    {
        painter->SetColor(255,255,255,255);
        painter->FillRect(OpRect(xp, yp, m_rect.w, m_rect.h));
    }
    painter->RemoveClipRect();
#else
    if (m_redirected)
    {
        // FIXME: noone is using this yet, so it has not been implemented
        OP_ASSERT(FALSE);
    }
    else if (m_clear_background)
    {
        MDE_SetColor(MDE_RGB(255, 255, 255), screen);
        MDE_DrawRectFill(rect, screen);
    }
#endif // VEGA_OPPAINTER_SUPPORT
}

void MDENativeWindow::OnRegionInvalid()
{
    m_region_invalid = TRUE;
}

void MDENativeWindow::UpdateRegion()
{
    if (!m_window_should_be_visible)
        return;
    if (!m_region_invalid)
        return;
    if (IsVisible())
        ValidateRegion();
    else
        m_region.Reset();
    m_region_invalid = FALSE;
    if (m_region.num_rects == 0)
    {
        if (m_window_visible)
        {
            m_window_visible = FALSE;
            ShowWindow(FALSE);
        }
    }
    int xp = 0;
    int yp = 0;
    int miny = m_rect.h, maxy = 0;
    ConvertToScreen(xp, yp);

    int i;
    for (i = 0; i < m_region.num_rects && !m_num_overlapping_transparentviews; ++i)
    {
        m_region.rects[i].x -= xp;
        m_region.rects[i].y -= yp;
        int y = m_region.rects[i].y;
        miny = MIN(miny, y);
        maxy = MAX(maxy, y + m_region.rects[i].h);
    }
    BOOL moveFirst = TRUE;
    if ((miny > 0 && yp < m_old_winy) || (maxy < m_rect.h && yp > m_old_winy))
        moveFirst = FALSE;

    m_old_winy = yp;

    if (!m_redirected)
    {
        if (moveFirst)
        {
            MoveWindow(xp, yp);
        }

        SetClipRegion(&m_region);

        if (!moveFirst)
        {
            MoveWindow(xp, yp);
        }
    }

    if (!m_window_visible && m_region.num_rects)
    {
        m_window_visible = TRUE;

        ShowWindow(TRUE);
    }
    for (i = 0; i < m_region.num_rects && !m_num_overlapping_transparentviews; ++i)
    {
        m_region.rects[i].x += xp;
        m_region.rects[i].y += yp;
    }
}

void MDENativeWindow::Move(int x, int y)
{
    if (x != m_rect.x || y != m_rect.y)
        SetRect(MDE_MakeRect(x, y, m_rect.w, m_rect.h));
}

void MDENativeWindow::Resize(int w, int h)
{
    if (m_rect.w == w && m_rect.h == h)
        return;

    SetRect(MDE_MakeRect(m_rect.x, m_rect.y, w, h));
    int xp = 0, yp = 0;
    ConvertToScreen(xp, yp);
    ResizeWindow(w, h);
}

void MDENativeWindow::Show(BOOL vis)
{
    m_window_should_be_visible = vis;
    if (m_window_visible && !vis)
    {
        m_window_visible = FALSE;
        ShowWindow(FALSE);
    }
}
#endif // MDE_NATIVE_WINDOW_SUPPORT
