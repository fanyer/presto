/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "platforms/windows_common/pi_impl/pluginwindow.h"
#include "modules/libvega/vega3ddevice.h"
#include "modules/pi/OpBitmap.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

#ifndef WS_EX_LAYERED
# define WS_EX_LAYERED           0x00080000
#endif // !WS_EX_LAYERED
#ifndef LWA_ALPHA
# define LWA_ALPHA               0x00000002
#endif // !LWA_ALPHA

HBITMAP WindowsPluginNativeWindow::dummyBitmap;
HDC WindowsPluginNativeWindow::dummyDC;

WindowsPluginNativeWindow::WindowsPluginNativeWindow(const MDE_RECT &rect, HWND win, HWND parent_hwnd, HINSTANCE global_instance) :
	MDENativeWindow(rect),
	m_hwnd(win),
	m_parent_hwnd(parent_hwnd),
	m_global_instance(global_instance),
	m_transwin(NULL),
	m_native_backbuffer(NULL),
	m_backbuffer_dc(NULL),
	m_transwin_dc(NULL),
	m_native_backbuffer_width(0),
	m_native_backbuffer_height(0),
	m_reparent_to_transwin(FALSE),
	m_updating(FALSE)
{
}

WindowsPluginNativeWindow::~WindowsPluginNativeWindow()
{
	OP_ASSERT(!m_hwnd || ::IsWindow(m_hwnd));
	OP_ASSERT(!m_transwin || ::IsWindow(m_transwin));

	if (m_hwnd && ::IsWindow(m_hwnd))
		::DestroyWindow((HWND)m_hwnd);

	if (m_backbuffer_dc)
		DeleteDC(m_backbuffer_dc);
	if (m_transwin_dc)
		ReleaseDC(m_transwin, m_transwin_dc);
	if (m_native_backbuffer)
		DeleteObject(m_native_backbuffer);
	if (m_transwin && ::IsWindow(m_transwin))
		::DestroyWindow(m_transwin);
}

void WindowsPluginNativeWindow::SetParent(HWND parent)
{
	Updating(TRUE);
	if (m_hwnd && (!m_transwin || GetParent(m_hwnd) != m_transwin))
		::SetParent(m_hwnd, parent);
	Updating(FALSE);
}

void WindowsPluginNativeWindow::UpdateBackbuffer(OpBitmap* backbuffer)
{
	if (m_rect.w < 0 || m_rect.h < 0)
		return;

	OP_ASSERT((INT32)backbuffer->Width() == m_rect.w && (INT32)backbuffer->Height() == m_rect.h);
	if ((INT32)backbuffer->Width() != m_rect.w || (INT32)backbuffer->Height() != m_rect.h)
		return;

	if (m_reparent_to_transwin)
	{
		Updating(TRUE);
		::SetParent(m_hwnd, m_transwin);
		::SetWindowPos(m_hwnd, 0, 0, 0, m_rect.w, m_rect.h, SWP_NOZORDER | SWP_NOACTIVATE);
		SetWindowRgn(m_hwnd, NULL, TRUE);
		m_reparent_to_transwin = FALSE;

		::InvalidateRect(m_hwnd, NULL, FALSE);
		::UpdateWindow(m_hwnd);
		Updating(FALSE);
	}

	UINT rect_wxh = m_rect.w * m_rect.h;

	if (m_native_backbuffer && (backbuffer->Width() != (UINT32)m_native_backbuffer_width || backbuffer->Height() != (UINT32)m_native_backbuffer_height))
	{
		DeleteObject(m_native_backbuffer);
		m_native_backbuffer = NULL;
	}

	if (!m_native_backbuffer)
	{
		if (!m_transwin_dc)
			m_transwin_dc = GetDC(m_hwnd);
		if (!m_backbuffer_dc)
			m_backbuffer_dc = CreateCompatibleDC(m_transwin_dc);

		if (!m_transwin_dc || !m_backbuffer_dc)
			return;

		m_native_backbuffer = CreateCompatibleBitmap(m_transwin_dc, m_rect.w, m_rect.h);
		m_native_backbuffer_width = m_rect.w;
		m_native_backbuffer_height = m_rect.h;

		if (!m_native_backbuffer)
			return;
	}

	HBITMAP old_bb = (HBITMAP)SelectObject(m_backbuffer_dc, m_native_backbuffer);

	BitBlt(m_backbuffer_dc, 0, 0, m_rect.w, m_rect.h, m_transwin_dc, 0, 0, SRCCOPY);

	SelectObject(m_backbuffer_dc, old_bb);

	BITMAPINFO bi;
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = m_rect.w;
	bi.bmiHeader.biHeight = -(int)m_rect.h;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = 0;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	GetDIBits(m_backbuffer_dc, m_native_backbuffer, 0, m_rect.h, NULL, &bi, DIB_RGB_COLORS);


	if (bi.bmiHeader.biSizeImage > rect_wxh * 4)
		return;

	void* data = backbuffer->GetPointer();
	if (!data)
		return;

	GetDIBits(m_backbuffer_dc, m_native_backbuffer, 0, m_rect.h, data, &bi, DIB_RGB_COLORS);
	for (unsigned int i = 0; i < rect_wxh; ++i)
		((UINT32*)data)[i] |= (255<<24);

	backbuffer->ReleasePointer();
}

void WindowsPluginNativeWindow::MoveWindow(int x, int y)
{
	Updating(TRUE);

	// SWP_DEFERERASE is required to prevent Windows from performing some synchronous background erasing,
	// which cause flickers when scrolling page with plugins.
	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_DEFERERASE;

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKEND_OPENGL)
	// When it comes to opengl window we have to be more careful, anything that triggers a copyover from the
	// internal gdi buffer for the plugin window to the hardware buffer of the parent window would cause
	// serious flicker as the clip region of the plugin window is not respected! We can't apply SWP_NOREDRAW
	// when using software window which result in the newly uncovered area in parent window not invalidated,
	// this however isn't a issue for opengl window. In short, gdi window on a hardware parent window work
	// quite differently. DSK-350023
	if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D &&
		g_vegaGlobals.vega3dDevice->getDeviceType() == PrefsCollectionCore::OpenGL)
	{
		flags |= SWP_NOREDRAW;
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
#endif

	::SetWindowPos(m_hwnd, 0, x, y, 0, 0, flags);

#if defined(VEGA_3DDEVICE) && (defined(VEGA_BACKEND_DIRECT3D10))
	// work around to cope with mixing hardware/software surface situation. DSK-342318
	if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D &&
		(false
#ifdef VEGA_BACKEND_DIRECT3D10
		|| g_vegaGlobals.vega3dDevice->getDeviceType() == PrefsCollectionCore::DirectX10
#endif
		))
	{
		WINBOOL composition = FALSE;

		//DwmIsCompositionEnabled is not present on XP, and thus OPDwmIsCompositionEnabled
		//will not actually do anything.
		OPDwmIsCompositionEnabled(&composition);

		if (composition)
		{
			HDC dc = GetDC(m_hwnd);

			if (!dummyDC)
			{
				dummyDC = ::CreateCompatibleDC(dc);
				dummyBitmap = ::CreateCompatibleBitmap(dc, 1, 1);
				SelectObject(dummyDC, dummyBitmap);
			}

			// Cheat the system into update the area the gdi child window occupied in the parent hardware
			// window without asking the gdi window to repaint.
			RECT rect;
			GetWindowRect(m_hwnd, &rect);
			BitBlt(dummyDC, 0, 0, 1, 1, dc, 0, 0, SRCCOPY);
			BitBlt(dummyDC, 0, 0, 1, 1, dc, rect.right - rect.left - 1, 0, SRCCOPY);
			BitBlt(dummyDC, 0, 0, 1, 1, dc, rect.right - rect.left - 1, rect.bottom-rect.top-1, SRCCOPY);
			BitBlt(dummyDC, 0, 0, 1, 1, dc, 0, rect.bottom-rect.top-1, SRCCOPY);
			ReleaseDC(m_hwnd, dc);
		}
	}
#endif

	Updating(FALSE);
}

void WindowsPluginNativeWindow::ResizeWindow(int w, int h)
{
	Updating(TRUE);
	::SetWindowPos(m_hwnd, 0, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
	if (m_transwin)
		::SetWindowPos(m_transwin, 0, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
	Updating(FALSE);
}

void WindowsPluginNativeWindow::ShowWindow(BOOL show)
{
	Updating(TRUE);
	::ShowWindow(m_hwnd, show?SW_SHOWNA:SW_HIDE);
	Updating(FALSE);
}

void WindowsPluginNativeWindow::SetClipRegion(MDE_Region* mdergn)
{
	Updating(TRUE);

	/* The purpose of the code below is to repaint only scrolled in areas of
	   the plugin window. Otherwise the parts of the plugin can momentarily
	   paint on top of the UI when scrolling. See DSK-342237. */
	HRGN rgn = ::CreateRectRgn(0, 0, 0, 0);
	for (int i = 0; mdergn && i < mdergn->num_rects; ++i)
	{
		HRGN rect = ::CreateRectRgn(mdergn->rects[i].x, mdergn->rects[i].y,
			mdergn->rects[i].x+mdergn->rects[i].w,
			mdergn->rects[i].y+mdergn->rects[i].h);
		::CombineRgn(rgn, rgn, rect, RGN_OR);
		::DeleteObject(rect);
	}

	// Region that was NOT visible, need to be invalidated
	HRGN uncovered_rgn = ::CreateRectRgn(0, 0, 0, 0);
	::GetWindowRgn(m_hwnd, uncovered_rgn);
	int ret = ::CombineRgn(uncovered_rgn, rgn, uncovered_rgn, RGN_DIFF);
	if (ret == NULLREGION || ret == ERROR)
	{
		::DeleteObject(uncovered_rgn);
		uncovered_rgn = NULL;
	}

	// Region of parent window that was obscured by this plugin, need to be invalidated
	HRGN uncovered_parent_rgn = ::CreateRectRgn(0, 0, 0, 0);
	::GetWindowRgn(m_hwnd, uncovered_parent_rgn);
	ret = ::CombineRgn(uncovered_parent_rgn, uncovered_parent_rgn, rgn, RGN_DIFF);
	if (ret == NULLREGION || ret == ERROR)
	{
		::DeleteObject(uncovered_parent_rgn);
		uncovered_parent_rgn = NULL;
	}

	// Passing True to bRedraw would remove the necessity to do all these invalidations
	// manually, but the system would then do some synchronized painting etc, which
	// cause flickers when scrolling the plugin, especially when enabled hardware
	// acceleration, see DSK-350023 and related.
	if (!::SetWindowRgn(m_hwnd, rgn, FALSE))
		::DeleteObject(rgn);

	if (uncovered_rgn)
	{
		::InvalidateRgn(m_hwnd, uncovered_rgn, FALSE);
		::DeleteObject(uncovered_rgn);
	}

	if (uncovered_parent_rgn)
	{
		int x = 0, y = 0;
		ConvertToScreen(x, y);
		::OffsetRgn(uncovered_parent_rgn, x, y);
		::InvalidateRgn(m_parent_hwnd, uncovered_parent_rgn, FALSE);
		::DeleteObject(uncovered_parent_rgn);
	}

	Updating(FALSE);
}

void WindowsPluginNativeWindow::UpdateWindow()
{
	Updating(TRUE);
	::UpdateWindow(m_hwnd);
	Updating(FALSE);
}

void WindowsPluginNativeWindow::SetRedirected(BOOL redir)
{
	Updating(TRUE);
	if (redir)
	{
		if (!m_transwin)
		{
			// move m_hwnd to a transparent window
			DWORD win_style = WS_OVERLAPPED | WS_POPUP;
			DWORD win_style_ex = /*WS_EX_TOPMOST |*/ WS_EX_TOOLWINDOW;
			m_transwin = ::CreateWindowExA(win_style_ex,
									  "aPluginWinClass",	//  window class registered earlier
									  "Hidden Plugin Container",	//  window caption
									  win_style,
									  0,					//  x position
									  0,					//  y position
									  m_rect.w,				//  width
									  m_rect.h,				//  iheight
									  NULL, // Parent win
									  0,					//  menu or child ID
									  m_global_instance,	//  instance
									  NULL);				//  additional info

			// Set WS_EX_LAYERED on this window
			SetWindowLongPtr(m_transwin, GWL_EXSTYLE,
					GetWindowLongPtr(m_transwin, GWL_EXSTYLE) | WS_EX_LAYERED);
			// Make this window 0% alpha
			SetLayeredWindowAttributes(m_transwin, 0, (255 * 0) / 100, LWA_ALPHA);
			::ShowWindow(m_transwin, SW_SHOWNA);
		}
		else
		{
			::SetWindowPos(m_transwin, 0, 0, 0, m_rect.w, m_rect.h, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		m_reparent_to_transwin = TRUE;
	}
	else
	{
		m_reparent_to_transwin = FALSE;
		::SetParent(m_hwnd, m_parent_hwnd);

		if (m_native_backbuffer)
		{
			// We don't need the backbuffer anymore
			DeleteObject(m_native_backbuffer);
			m_native_backbuffer = NULL;
		}
		if (m_backbuffer_dc)
		{
			DeleteDC(m_backbuffer_dc);
			m_backbuffer_dc = NULL;
		}
		if (m_transwin_dc)
		{
			ReleaseDC(m_transwin, m_transwin_dc);
			m_transwin_dc = NULL;
		}
	}
	Updating(FALSE);
}


#endif // _PLUGIN_SUPPORT_
