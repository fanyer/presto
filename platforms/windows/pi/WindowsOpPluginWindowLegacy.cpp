/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include <WinGdi.h>

#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/pi/WindowsOpPluginAdapter.h"
#include "platforms/windows/pi/WindowsOpPluginWindow.h"
#include "platforms/windows/pi/WindowsOpView.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows_common/utils/hookapi.h"
#include "platforms/windows_common/utils/fileinfo.h"

#include "modules/doc/frm_doc.h"
#include "modules/pi/OpBitmap.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/libvega/vega3ddevice.h"

extern HINSTANCE hInst;

ATOM atomPluginWindowClass = 0;

const uni_char szPluginClassName[] = UNI_L("aPluginWinClass");

// Tracking of active windowless plugin.
WindowsOpPluginWindow* s_focused_plugin = NULL;

// List of all plugin window instances.
OpWindowsPointerHashTable<const HWND, WindowsOpPluginWindow*> WindowsOpPluginWindow::s_plugin_windows;

OP_STATUS OpPluginWindow::Create(OpPluginWindow** new_object, const OpRect& rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window)
{
	WindowsOpPluginWindow* plugin_window = OP_NEW(WindowsOpPluginWindow, ());
	RETURN_OOM_IF_NULL(plugin_window);

	if (OpStatus::IsError(plugin_window->Construct(rect, scale, parent, windowless)))
	{
		OP_DELETE(plugin_window);
		return OpStatus::ERR;
	}

	*new_object = plugin_window;
	return OpStatus::OK;
}

WindowsOpPluginWindow::WindowsOpPluginWindow() :
	m_plugin(NULL)
	, m_hwnd(NULL)
	, m_hwnd_view(NULL)
	, m_parent_view(NULL)
	, m_windowless(FALSE)
	, m_opbmp(NULL)
	, m_white_bitmap(NULL)
	, m_black_bitmap(NULL)
	, m_hdc(NULL)
	, m_transparent(FALSE)
	, m_vis_dev(NULL)
	, m_npwin(NULL)
	, m_key_wParam(0)
	, m_key_lParam(0)
#ifdef MANUAL_PLUGIN_ACTIVATION
	, m_listener(NULL)
	, m_input_blocked(FALSE)
#endif // MANUAL_PLUGIN_ACTIVATION
	, m_paint_natively(g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_SW)
{
}

WindowsOpPluginWindow::~WindowsOpPluginWindow()
{
	OP_ASSERT(!m_plugin);
	OP_DELETE(m_opbmp);
	if (m_white_bitmap)
		DeleteObject(m_white_bitmap);
	if (m_black_bitmap)
		DeleteObject(m_black_bitmap);

	if (m_hwnd_view)
	{
		if (m_hwnd_view->m_parent)
			m_hwnd_view->m_parent->RemoveChild(m_hwnd_view);

		m_hwnd_view->DelayWindowDestruction();
		OP_DELETE(m_hwnd_view);
		m_hwnd_view = NULL;
	}

	if (m_hwnd && ISWINDOW(m_hwnd))
	{
		WindowsOpPluginWindow* opwin = NULL;
		OP_STATUS retval = s_plugin_windows.Remove(m_hwnd, &opwin);
		OP_ASSERT(retval == OpStatus::OK);
		OpStatus::Ignore(retval);
		::DestroyWindow(m_hwnd);
	}

	if (GetFocusedPluginWindow() == this)
		SetFocusedPluginWindow(NULL);
}

OP_STATUS WindowsOpPluginWindow::Construct(const OpRect &rect, int scale, OpView* parent, BOOL windowless)
{
	DWORD win_style = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	HWND parent_hwnd = NULL;

	m_parent_view = parent;
	m_windowless = windowless;

	if (static_cast<WindowsOpView*>(parent)->GetNativeWindow())
		parent_hwnd = static_cast<WindowsOpView*>(parent)->GetNativeWindow()->m_hwnd;

	if (!windowless)
	{
		m_hwnd = ::CreateWindowEx(WS_EX_NOPARENTNOTIFY,
								  szPluginClassName,    //  window class registered earlier
								  UNI_L(""),                   //  window caption
								  win_style,
								  rect.x,               //  x position
								  rect.y,               //  y position
								  rect.width,           //  width
								  rect.height,          //  iheight
								  parent_hwnd,
								  0,                    //  menu or child ID
								  hInst,                //  instance
								  NULL);                //  additional info

		RETURN_OOM_IF_NULL(m_hwnd);

		/* Some plugins use ANSI API's to retrieve the window procedure. If we
		   don't set it explicitly, the plugin will get garbage pointer and
		   crash on calling it.*/
		SetWindowLongPtrA(m_hwnd, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(PluginStaticWinProc));

		HDC new_dc = ::GetDC(m_hwnd);
		::SetBkMode(new_dc, TRANSPARENT);
		::SetMapMode(new_dc, MM_ANISOTROPIC);
		::SetWindowExtEx(new_dc, 100, 100, NULL);
		::SetViewportExtEx(new_dc, scale, scale, NULL);
		::ReleaseDC(m_hwnd, new_dc);

		MDE_RECT mr = MDE_MakeRect(rect.x, rect.y, rect.width, rect.height);
		m_hwnd_view = OP_NEW(WindowsPluginNativeWindow, (mr, m_hwnd, parent_hwnd, hInst));

		if (!m_hwnd_view)
		{
			::DestroyWindow(m_hwnd);
			return OpStatus::ERR_NO_MEMORY;
		}

		OP_STATUS status = s_plugin_windows.Add(m_hwnd, this);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(m_hwnd_view);
			return status;
		}
	}

	return OpStatus::OK;
}

void WindowsOpPluginWindow::Show()
{
	if (m_hwnd_view)
	{
		m_hwnd_view->Show(TRUE);
		if (!m_hwnd_view->m_parent)
		{
			static_cast<MDE_OpView*>(m_parent_view)->GetMDEWidget()->AddChild(m_hwnd_view);
			m_hwnd_view->SetZ(MDE_Z_TOP);
		}
	}
}

void WindowsOpPluginWindow::BlockMouseInput(BOOL block)
{
	m_input_blocked = block;
}

void WindowsOpPluginWindow::Hide()
{
	if (m_hwnd_view)
	{
		m_hwnd_view->Show(FALSE);
		if (m_hwnd_view->m_parent)
			m_hwnd_view->m_parent->RemoveChild(m_hwnd_view);
	}
}

void WindowsOpPluginWindow::SetPos(int x, int y)
{
	if (m_hwnd_view)
		m_hwnd_view->Move(x, y);
}

void WindowsOpPluginWindow::SetSize(int w, int h)
{
	if (m_hwnd_view)
		m_hwnd_view->Resize(w, h);
}

void* WindowsOpPluginWindow::GetHandle()
{
	if (m_hwnd)
		return m_hwnd;

	if (IsWindowless() && m_parent_view && static_cast<WindowsOpView*>(m_parent_view)->GetNativeWindow())
		return static_cast<WindowsOpView*>(m_parent_view)->GetNativeWindow()->m_hwnd;

	return NULL;
}

void* WindowsOpPluginWindow::GetParentWindowHandle()
{
	if (m_parent_view && static_cast<WindowsOpView*>(m_parent_view)->GetNativeWindow())
		return static_cast<WindowsOpView*>(m_parent_view)->GetNativeWindow()->m_hwnd;
	else
		return NULL;
}

void WindowsOpPluginWindow::ReparentPluginWindow()
{
	if (m_hwnd && ISWINDOW(m_hwnd))
	{
		/* Hide and reparent window. The window handle will be destroyed when the
		   plugin is deleted which might happen after the deletion of the document. */
		if (m_hwnd_view && m_hwnd_view->m_parent)
		{
			/* Reset to orginal parent and backbuffer related items. */
			m_hwnd_view->SetRedirected(FALSE);

			m_hwnd_view->m_parent->RemoveChild(m_hwnd_view);
		}
		::ShowWindow((HWND)m_hwnd, SW_HIDE);

		HWND hwnd_parent = GetParent((HWND)m_hwnd);
		if (hwnd_parent && WindowsOpWindow::IsDestroyingOpWindow(hwnd_parent))
		{
			/* Reparent plugin window only if we're actually destroying the parent
			   (as when closing the window). Some plugins (f.ex acrobat 7) will cause
			   crashes when doing something to the original parent window if we
			   reparent it and then destroy it. DSK-274615 */
			::SetParent((HWND)m_hwnd, NULL);
		}
	}
}

HDC WindowsOpPluginWindow::GetRenderContext()
{
	// Return a fake dc for the parent since windowless plugins render to a dib
	WindowsOpWindow* nw = static_cast<WindowsOpView*>(m_parent_view)->GetNativeWindow();

	if (m_paint_natively)
	{
		if (nw && nw->GetRenderTargetDC())
			return nw->GetRenderTargetDC();
	}
	else if (!m_hdc && nw)
	{
		HWND phwnd = static_cast<WindowsOpView*>(m_parent_view)->GetNativeWindow()->m_hwnd;
		HDC winDC = ::GetDC(phwnd);
		m_hdc = CreateCompatibleDC(winDC);
		ReleaseDC(phwnd, winDC);
	}
	return m_hdc;
}

#ifdef NS4P_SILVERLIGHT_WORKAROUND
void WindowsOpPluginWindow::InvalidateWindowed(const OpRect& rect)
{
	if (m_hwnd && ISWINDOW(m_hwnd))
	{
		RECT winrect = { rect.x, rect.y, rect.x + rect.width, rect.y + rect.height };
		::InvalidateRect(m_hwnd, &winrect, FALSE);
	}
}
#endif // NS4P_SILVERLIGHT_WORKAROUND

OP_STATUS WindowsOpPluginWindow::CreateMouseEvent(OpPlatformEvent** key_event, OpPluginEventType event_type, const OpPoint& point, int button_or_delta, ShiftKeyState modifiers)
{
#ifdef MANUAL_PLUGIN_ACTIVATION
	if (m_listener && m_input_blocked)
	{
		if (event_type == PLUGIN_MOUSE_UP_EVENT && button_or_delta == MOUSE_BUTTON_1)
		{
			//m_input_blocked = FALSE;
			// Activate plugin html element.
			m_listener->OnMouseUp();
		}
		else
			m_listener->OnMouseHover();

		return OpStatus::ERR_NO_ACCESS;
	}
#endif // MANUAL_PLUGIN_ACTIVATION

	OpWindowsPlatformEvent* event = OP_NEW(OpWindowsPlatformEvent, ());
	RETURN_OOM_IF_NULL(event);

	event->m_event.wParam = 0;

	switch (event_type)
	{
		case PLUGIN_MOUSE_DOWN_EVENT:
			event->m_event.event = button_or_delta == MOUSE_BUTTON_1 ? WM_LBUTTONDOWN : button_or_delta == MOUSE_BUTTON_2 ? WM_RBUTTONDOWN : WM_MBUTTONDOWN;
			break;

		case PLUGIN_MOUSE_UP_EVENT:
			event->m_event.event = button_or_delta == MOUSE_BUTTON_1 ? WM_LBUTTONUP : button_or_delta == MOUSE_BUTTON_2 ? WM_RBUTTONUP : WM_MBUTTONUP;
			break;

		case PLUGIN_MOUSE_MOVE_EVENT:
		case PLUGIN_MOUSE_ENTER_EVENT:
		case PLUGIN_MOUSE_LEAVE_EVENT:
			event->m_event.event = WM_MOUSEMOVE;
			break;

		case PLUGIN_MOUSE_WHEELV_EVENT:
		case PLUGIN_MOUSE_WHEELH_EVENT:
		{
			// Cached value of SPI_GETWHEELSCROLLLINES user setting.
			static UINT lines_to_scroll_setting = 0;

			/* Delta value got from core is converted to number of lines that
			   should be scrolled. It needs to be converted back to delta.
			   We can't do that precisely though as number of lines is rounded
			   but that is not really an issue. */
			if (!lines_to_scroll_setting)
			{
				SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0 , &lines_to_scroll_setting, 0);
				// Guard from 0 value that would crash on dividing.
				lines_to_scroll_setting = MAX(1, lines_to_scroll_setting);
			}

			int delta = (-button_or_delta / lines_to_scroll_setting) * WHEEL_DELTA;

			event->m_event.event = event_type == PLUGIN_MOUSE_WHEELV_EVENT ? WM_MOUSEWHEEL : WM_MOUSEHWHEEL;
			event->m_event.wParam = MAKEWPARAM(0, delta);
			break;
		}

		default:
			OP_DELETE(event);
			return OpStatus::ERR_NOT_SUPPORTED;
	}

	// Convert to screen coordinates.
	OpRect plugin_rect = GetRect();
	OpRect view_rect = GetViewRect();
	OpPoint view_offset = GetViewOffset();
	/* Point rect is in rendering coordinates and we need screen coordinates.
	   Convert by subtracting rendering offset and then adding offset relative to
	   OpView plus offset of OpView itself. */
	OpPoint screen_point(point.x-view_rect.x+plugin_rect.x+view_offset.x,
		point.y-view_rect.y+plugin_rect.y+view_offset.y);

	event->m_event.lParam = MAKELONG(screen_point.x, screen_point.y);

	if (modifiers & SHIFTKEY_CTRL)
		event->m_event.wParam |= MK_CONTROL;

	if (modifiers & SHIFTKEY_SHIFT)
		event->m_event.wParam |= MK_SHIFT;

	if (GetKeyState(VK_LBUTTON) & 0x8000)
		event->m_event.wParam |= MK_LBUTTON;

	if (GetKeyState(VK_RBUTTON) & 0x8000)
		event->m_event.wParam |= MK_RBUTTON;

	if (GetKeyState(VK_MBUTTON) & 0x8000)
		event->m_event.wParam |= MK_MBUTTON;

	*key_event = event;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginWindow::PaintDirectly(const OpRect& paint_rect)
{
	if (paint_rect.IsEmpty())
		return OpStatus::OK;

	if (m_paint_natively)
	{
		WindowsOpWindow* nw = static_cast<WindowsOpView*>(m_parent_view)->GetNativeWindow();

		if (nw && nw->GetRenderTargetDC())
		{
			HDC hdc = nw->GetRenderTargetDC();

			// Set the current clipping on the hdc
			MDE_Screen* screen = static_cast<MDE_OpView*>(m_parent_view)->GetMDEWidget()->m_screen;
			class VEGAOpPainter* painter = screen->GetVegaPainter();

			OpRect cliprect;
			painter->GetClipRect(&cliprect);
			/* Cliprect rectangle is relative to viewport coordinates. Add screen
			   offset of viewport as it needs to be intersected with npwin.x/y
			   which is relative to browser window. */
			cliprect.OffsetBy(GetViewOffset());

			OpRect rect(paint_rect);
			rect.OffsetBy(GetViewOffset());

			HRGN old_clip_rgn = 0;
			int clipret = GetClipRgn(hdc, old_clip_rgn);

			SelectClipRgn(hdc, NULL);
			IntersectClipRect(hdc, cliprect.x, cliprect.y, cliprect.x + cliprect.width, cliprect.y + cliprect.height);

			NPRect32 dirty_area;
			dirty_area.left = rect.x;
			dirty_area.top = rect.y;
			dirty_area.right = rect.Right();
			dirty_area.bottom = rect.Bottom();

			OpWindowsPlatformEvent event;
			event.m_event.event = WM_PAINT;
			event.m_event.wParam = reinterpret_cast<uintptr_t>(hdc);
			event.m_event.lParam = reinterpret_cast<uintptr_t>(&dirty_area);

			m_plugin->HandleEvent(&event, PLUGIN_PAINT_EVENT, TRUE);

			// Restore clip region
			if (clipret == 1)
			{
				SelectClipRgn(hdc, old_clip_rgn);
				DeleteObject(old_clip_rgn);
			}
			else
				SelectClipRgn(hdc, NULL);
		}
	}
	else
	{
		OpRect screen_rect(paint_rect);
		screen_rect.OffsetBy(GetViewOffset());
		if (!m_opbmp || m_opbmp->Width() != (UINT)screen_rect.width || m_opbmp->Height() != (UINT)screen_rect.height)
		{
			OP_DELETE(m_opbmp);
			m_opbmp = NULL;

			if (m_white_bitmap)
				DeleteObject(m_white_bitmap);

			if (m_black_bitmap)
			{
				DeleteObject(m_black_bitmap);
				m_black_bitmap = NULL;
			}

			BITMAPINFO bi;
			bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth = screen_rect.width;
			bi.bmiHeader.biHeight = -screen_rect.height;
			bi.bmiHeader.biPlanes = 1;
			bi.bmiHeader.biBitCount = 32;
			bi.bmiHeader.biCompression = BI_RGB;
			bi.bmiHeader.biSizeImage = 0;
			bi.bmiHeader.biXPelsPerMeter = 0;
			bi.bmiHeader.biYPelsPerMeter = 0;
			bi.bmiHeader.biClrUsed = 0;
			bi.bmiHeader.biClrImportant = 0;

			m_white_bitmap = CreateDIBSection(GetRenderContext(), &bi, DIB_RGB_COLORS, (void**)&m_white_bitmap_data, NULL, 0);
			RETURN_OOM_IF_NULL(m_white_bitmap);

			if (IsTransparent())
			{
				m_black_bitmap = CreateDIBSection(m_hdc, &bi, DIB_RGB_COLORS, (void**)&m_black_bitmap_data, NULL, 0);
				RETURN_OOM_IF_NULL(m_black_bitmap);
			}

			RETURN_IF_ERROR(OpBitmap::Create(&m_opbmp, screen_rect.width, screen_rect.height));
		}

		POINT origo;
		SetViewportOrgEx(m_hdc, -screen_rect.x, -screen_rect.y, &origo);

		OpWindowsPlatformEvent event;
		NPRect32 dirty_area;

		dirty_area.left = screen_rect.x;
		dirty_area.top = screen_rect.y;
		dirty_area.right = screen_rect.x + screen_rect.width;
		dirty_area.bottom = screen_rect.y + screen_rect.height;

		event.m_event.event = WM_PAINT;
		event.m_event.wParam = reinterpret_cast<uintptr_t>(m_hdc);
		event.m_event.lParam = reinterpret_cast<uintptr_t>(&dirty_area);

		HBITMAP hOldBitmap = (HBITMAP)SelectObject(m_hdc, m_white_bitmap);
		if (IsTransparent())
		{
			RECT re = {screen_rect.x, screen_rect.y, screen_rect.x+screen_rect.width, screen_rect.y+screen_rect.height};
			FillRect(m_hdc, &re, (HBRUSH)GetStockObject(WHITE_BRUSH));
		}

		m_plugin->HandleEvent(&event, PLUGIN_PAINT_EVENT);

		if (IsTransparent())
		{
			SelectObject(m_hdc, m_black_bitmap);
			RECT re = {screen_rect.x, screen_rect.y, screen_rect.x+screen_rect.width, screen_rect.y+screen_rect.height};
			FillRect(m_hdc, &re, (HBRUSH)GetStockObject(BLACK_BRUSH));
			m_plugin->HandleEvent(&event, PLUGIN_PAINT_EVENT);
		}

		SetViewportOrgEx(m_hdc, origo.x, origo.y, NULL);
		SelectObject(m_hdc, hOldBitmap);

		UINT32* bmpdata = (UINT32*)m_opbmp->GetPointer(OpBitmap::ACCESS_WRITEONLY);
		RETURN_OOM_IF_NULL(bmpdata);

		if (IsTransparent())
		{
#ifdef USE_PREMULTIPLIED_ALPHA
			UINT32* srcw = m_white_bitmap_data;
			UINT32* srcb = m_black_bitmap_data;

			for (UINT32 y = 0; y < (UINT)screen_rect.height; ++y)
			{
				for (UINT32 x = 0; x < (UINT)screen_rect.width; ++x)
				{
					*bmpdata = ((*srcb)&0x00ffffff) | ((255 - (((*srcw)&0xff)-((*srcb)&0xff)))<<24);
					++bmpdata;
					++srcw;
					++srcb;
				}
			}
#else
			// transparent windowless plugins are only implemented for premultiplied alpha
			OP_ASSERT(FALSE);
			return OpStatus::ERR_NOT_SUPPORTED;
#endif
		}
		else
		{
			UINT32* src = m_white_bitmap_data;

			for (UINT32 y = 0; y < (UINT)screen_rect.height; ++y)
			{
				for (UINT32 x = 0; x < (UINT)screen_rect.width; ++x)
				{
					*bmpdata = ((*src)&0x00ffffff) | (255<<24);
					++bmpdata;
					++src;
				}
			}
		}

		m_opbmp->ReleasePointer(TRUE);

		/* Have to convert paint rect which is relative to OpView into rect that
		   is relative to local rendering viewport. */
		OpPoint rendering_viewport_off(m_rect.x - m_view_rect.x, m_rect.y - m_view_rect.y);
		OpRect view_paint_rect(screen_rect);
		view_paint_rect.OffsetBy(-m_view_offset.x, -m_view_offset.y);
		view_paint_rect.OffsetBy(-rendering_viewport_off.x, -rendering_viewport_off.y);

		GetVisualDevice()->BlitImage(m_opbmp, OpRect(0,0,screen_rect.width,screen_rect.height), view_paint_rect);
	}

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginWindow::CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers)
{
	// No point in sending an event if we don't have event data.
	if (!key_event_data)
		return OpStatus::ERR;

	UINT64 data1, data2;
	OpWindowsPlatformKeyEventData* platform_data = static_cast<OpWindowsPlatformKeyEventData*>(key_event_data);
	platform_data->GetPluginPlatformData(data1, data2);

	// Core generates a key_down and a key_pressed for the first WM_KEYDOWN it receives.
	// If bit 30 of lParam is not set, it means that the key_press message isn't indicating
	// a key repeat and we can ignore it.
	if ((key_state == PLUGIN_KEY_PRESSED) && ((data2 & 1<<30) == 0))
		return OpStatus::OK;

	OpWindowsPlatformEvent* event = OP_NEW(OpWindowsPlatformEvent, ());
	RETURN_OOM_IF_NULL(event);

	event->m_event.event = (key_state == PLUGIN_KEY_UP) ? WM_KEYUP : WM_KEYDOWN;
	event->m_event.wParam = static_cast<WPARAM>(data1);
	event->m_event.lParam = static_cast<LPARAM>(data2);

	*key_event = event;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginWindow::CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in)
{
	SetFocusedPluginWindow(focus_in ? this : NULL);

	OpWindowsPlatformEvent* event = OP_NEW(OpWindowsPlatformEvent, ());
	RETURN_OOM_IF_NULL(event);

	event->m_event.event = focus_in ? WM_SETFOCUS : WM_KILLFOCUS;
	event->m_event.wParam = 0;
	event->m_event.lParam = 0;

	*focus_event = event;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginWindow::CreateWindowPosChangedEvent(OpPlatformEvent** pos_event)
{
	OpAutoPtr<OpWindowsPlatformEvent> event(OP_NEW(OpWindowsPlatformEvent, ()));
	RETURN_OOM_IF_NULL(event.get());

	// Convert to screen coordinates.
	OpRect plugin_rect = GetRect();
	plugin_rect.OffsetBy(GetViewOffset());

	WINDOWPOS *winpos = OP_NEW(WINDOWPOS, ());
	RETURN_OOM_IF_NULL(winpos);
	op_memset(winpos, 0, sizeof(*winpos));
	winpos->x = plugin_rect.x;
	winpos->y = plugin_rect.y;
	winpos->cx = plugin_rect.width;
	winpos->cy = plugin_rect.height;

	event->m_event.event = WM_WINDOWPOSCHANGED;
	event->m_event.wParam = 0;
	event->m_event.lParam = reinterpret_cast<uintptr_t>(winpos);

	*pos_event = event.release();

	return OpStatus::OK;
}

BOOL WindowsOpPluginWindow::HandleEvent(OpWindowsPlatformEvent* ev, OpPluginEventType type)
{
	BOOL ret = FALSE;

	if (m_windowless && m_plugin)
		ret = m_plugin->HandleEvent(ev, type) != 0;

	return ret;
}

void WindowsOpPluginWindow::Detach()
{
	if (m_hwnd_view && m_hwnd_view->m_parent)
		m_hwnd_view->m_parent->RemoveChild(m_hwnd_view);

	m_parent_view = NULL;
	m_plugin = NULL;
}

/* static */
WindowsOpPluginWindow* WindowsOpPluginWindow::GetPluginWindowFromHWND(HWND hwnd, BOOL search_children)
{
	HWND hwndParent = hwnd;

	while(hwnd)
	{
		WindowsOpPluginWindow* win = NULL;

		if (OpStatus::IsSuccess(s_plugin_windows.GetData(hwnd, &win)))
			return win;
		else
			// might be a child window, but we want the top level window
			hwnd = ::GetParent(hwnd);
	}

	if (search_children)
	{
		WindowsOpPluginWindow* win = NULL;

		EnumChildWindows(hwndParent, EnumChildWindowsProc, reinterpret_cast<LPARAM>(&win));

		return win;
	}

	return NULL;
}

/* static */
void WindowsOpPluginWindow::SetFocusedPluginWindow(WindowsOpPluginWindow* pwindow)
{
	s_focused_plugin = pwindow;
}

/* static */
WindowsOpPluginWindow* WindowsOpPluginWindow::GetFocusedPluginWindow()
{
	return s_focused_plugin;
}

/* static */
BOOL WindowsOpPluginWindow::HandleFocusedPluginWindow(MSG* msg)
{
	BOOL ret = FALSE;

	if (s_focused_plugin)
	{
		OpWindowsPlatformEvent ev;
		OpPluginEventType type = msg->message == WM_KEYDOWN || msg->message == WM_KEYUP ? PLUGIN_KEY_EVENT : PLUGIN_OTHER_EVENT;

		ev.m_event.event = msg->message;
		ev.m_event.lParam = msg->lParam;
		ev.m_event.wParam = msg->wParam;

		ret = s_focused_plugin->HandleEvent(&ev, type);
	}

	return ret;
}

/* static */
BOOL CALLBACK WindowsOpPluginWindow::EnumChildWindowsProc(HWND hwnd, LPARAM lParam)
{
	WindowsOpPluginWindow* win = NULL;
	if (OpStatus::IsSuccess(s_plugin_windows.GetData(hwnd, &win)))
	{
		*reinterpret_cast<WindowsOpPluginWindow**>(lParam) = win;
		return FALSE;
	}

	return TRUE;
}

#ifdef _PLUGIN_SUPPORT_

LRESULT CALLBACK WindowsOpPluginWindow::PluginStaticWinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
		if (WindowsOpPluginWindow* plugin_window = GetPluginWindowFromHWND(hWnd))
		{
			/* Update plugin wrapper window and all its childrens. This
			   is needed because our in process plugin window painting
			   is optimized to only paint scrolled-in areas and that way
			   the paint is not carried on into the wrapper window. */
			RECT update_rect;
			if (GetUpdateRect(hWnd, &update_rect, FALSE))
				RedrawWindow(hWnd, &update_rect, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
		}
		break;

	// fix for DSK-256843,workaround for a bug in flash
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		::ReleaseCapture();
		break;

	case WM_CLOSE:
		{
			/* Plugin might send WM_CLOSE message not only to plugin window but also parent
			   window thus close tab without user's consent. If plugin window receives
			   message here, we will set flag on plugin's parent window to ignore next WM_CLOSE
			   message. That can at worst case make parent window not close first time when user
			   tries to close tab but generally plugins either not send this message at all or
			   send both to plugin and parent window so that corner case should happen very rarely. */
			if (WindowsOpPluginWindow* pw = WindowsOpPluginWindow::GetPluginWindowFromHWND(hWnd))
				if (WindowsOpView* op_view = static_cast<WindowsOpView*>(pw->GetParentView()))
					if (WindowsOpWindow* op_window = op_view->GetNativeWindow())
					{
						op_window->SetIgnoreNextCloseMsg(TRUE);
						return 0;
					}
		}
		break;

	case WM_WINDOWPOSCHANGING:
	{
		WINDOWPOS *winpos = (WINDOWPOS*)lParam;
		WindowsOpPluginWindow *plugin_window = WindowsOpPluginWindow::GetPluginWindowFromHWND(winpos->hwnd);

		/* Prevent some modifications to plugin window if message is not triggered by us.
		   This message is triggered by ShowWindow, SetWindowPos and probably many other
		   API calls so it should handle most cases. */
		if (plugin_window && plugin_window->GetNativeWindow() && !plugin_window->GetNativeWindow()->IsBeingUpdated())
		{
			// Don't let plugin hide, show, move or resize our window.
			winpos->flags &= ~SWP_SHOWWINDOW & ~SWP_HIDEWINDOW;
			winpos->flags |= SWP_NOMOVE | SWP_NOSIZE;
			return 0;
		}
		break;
	}

	case WM_STYLECHANGING:
	{
		WindowsOpPluginWindow* plugin_window = WindowsOpPluginWindow::GetPluginWindowFromHWND(hWnd);

		/* Disallow style changes if change is not initiated by us.
		   Plugins can cause painting problems when changing them (DSK-340165). */
		if (plugin_window && plugin_window->GetNativeWindow() && !plugin_window->GetNativeWindow()->IsBeingUpdated())
		{
			STYLESTRUCT* styles = (STYLESTRUCT*)lParam;
			styles->styleNew = styles->styleOld;
		}

		return 0;
	}

	case WM_ERASEBKGND: // RL 310300
		return TRUE;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

BOOL WindowsOpPluginWindow::PluginRegClassDef(HINSTANCE hInstance)
{
	WNDCLASSEX WndClass;

	WndClass.cbSize        = sizeof(WNDCLASSEX);
	WndClass.style         = CS_DBLCLKS;
	WndClass.lpfnWndProc   = WindowsOpPluginWindow::PluginStaticWinProc;
	WndClass.cbClsExtra    = 0;
	WndClass.cbWndExtra    = 0;
	WndClass.hInstance     = hInstance;
	WndClass.hIcon         = NULL;
	WndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	WndClass.lpszMenuName  = NULL;
	WndClass.lpszClassName = szPluginClassName;
	WndClass.hIconSm       = NULL;
	atomPluginWindowClass  = RegisterClassEx(&WndClass);

	return (BOOL)atomPluginWindowClass;
}

/* static */
void WindowsOpPluginWindow::OnLibraryLoaded(HMODULE hModule, const uni_char* path)
{
	// Get base name
	const uni_char* last_pathsep = uni_strrchr(path, PATHSEPCHAR);

	/* Overwrite original function pointer if base name starts with "npswf".
	   Workaround for flash player's internal settings dialog not working
	   due to security checks that are made against client rect instead of
	   window rect. See DSK-327707. */

	if (last_pathsep && uni_strnicmp(last_pathsep+1, "npswf", 5) == 0)
	{
		WindowsCommonUtils::HookAPI hook(hModule);
		WindowsCommonUtils::FileInformation fileinfo;
		RETURN_VOID_IF_ERROR(fileinfo.Construct(path));

		if (fileinfo.GetMajorVersion() <= 10)
			hook.SetImportAddress("user32.dll", "GetWindowInfo", (FARPROC)GetWindowInfoHook);

		hook.SetImportAddress("gdi32.dll", "ExtTextOutA", (FARPROC)ExtTextOutHookA);
		hook.SetImportAddress("user32.dll", "GetFocus", (FARPROC)GetFocusHook);
	}
}

/* static */
BOOL WINAPI GetWindowInfoHook(HWND hwnd, PWINDOWINFO pwi)
{
	// Optimization to not check who owns hwnd every single time.
	static HWND last_opera_hwnd = NULL;

	BOOL ret = ::GetWindowInfo(hwnd, pwi);

	if (ret && (hwnd == last_opera_hwnd || WindowsOpWindow::GetWindowFromHWND(hwnd)))
	{
		pwi->rcWindow = pwi->rcClient;

		/* This is Opera's hwnd. When plugin will call GetWindowInfo
		   again with same hwnd, we won't have to look it up. */
		last_opera_hwnd = hwnd;
	}

	return ret;
}

/* static */
BOOL WINAPI ExtTextOutHookA(HDC hdc, int X, int Y, UINT fuOptions, RECT *lprc, char* lpString, UINT cbCount, const INT *lpDx)
{
	BOOL ret = ::ExtTextOutA(hdc,X,Y, fuOptions, lprc, lpString,cbCount, lpDx);

	BOOL composition = FALSE;
	if (ret && g_skin_manager && g_skin_manager->GetOptionValue("Full Transparency", FALSE)
		&& S_OK == OPDwmIsCompositionEnabled(&composition) && composition)
	{
		HDC memDC = NULL;
		HBITMAP bitmap = NULL;
		do
		{
			SIZE size;
			if (!::GetTextExtentPoint32A(hdc, lpString, cbCount, &size))
				break;

			RECT rect, bound, clip;

			bound.bottom = Y + size.cy;
			bound.top = Y - size.cy;
			bound.right = X + size.cx;
			bound.left = X - size.cx;

			if (ERROR == ::GetClipBox(hdc, &clip))
				break;

			if (!::IntersectRect(&rect, &bound, &clip))
				break;

			long rect_h = rect.bottom - rect.top;
			long rect_w = rect.right - rect.left;

			memDC = ::CreateCompatibleDC(hdc);
			if (!memDC)
				break;

			// Copy to dib
			BITMAPINFO bi;
			bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth = rect_w;
			bi.bmiHeader.biHeight = -(int)rect_h;
			bi.bmiHeader.biPlanes = 1;
			bi.bmiHeader.biBitCount = 32;
			bi.bmiHeader.biCompression = BI_RGB;
			bi.bmiHeader.biSizeImage = 0;
			bi.bmiHeader.biXPelsPerMeter = 0;
			bi.bmiHeader.biYPelsPerMeter = 0;
			bi.bmiHeader.biClrUsed = 0;
			bi.bmiHeader.biClrImportant = 0;

			UINT32* bitmapData = NULL;
			bitmap = ::CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void**)&bitmapData, NULL, 0);
			if (!bitmap || !bitmapData)
				break;
			for (long i = 0; i < rect_w*rect_h; ++i)
				bitmapData[i] = 0xff000000;

			HBITMAP oldbitmap = (HBITMAP)::SelectObject(memDC, bitmap);
			if (!oldbitmap)
				break;

			::BitBlt(hdc, rect.left, rect.top, rect_w, rect_h, memDC, 0, 0, SRCPAINT);
			::SelectObject(memDC, oldbitmap);
		} while(FALSE);

		::DeleteDC(memDC);
		::DeleteObject(bitmap);
	}

	return ret;
}

/* static */
HWND WINAPI WindowsOpPluginWindow::GetFocusHook()
{
	/* Faulty check in Flash makes it think that Opera is focused when both
	   GetFocus() and GetTopWindow() return NULL, causing security issue.
	   We'll force garbage value when plugin asks for the focus when no
	   instance has focus. DSK-351604. */

	if (!GetFocusedPluginWindow())
		return reinterpret_cast<HWND>(-1);

	return GetFocus();
}

#endif // _PLUGIN_SUPPORT_
