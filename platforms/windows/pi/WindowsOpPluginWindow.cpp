/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifndef NS4P_COMPONENT_PLUGINS

# include "platforms/windows/pi/WindowsOpPluginWindowLegacy.cpp"

#else // NS4P_COMPONENT_PLUGINS

#include <WinGdi.h>

#include "modules/skin/OpSkinManager.h"

#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/pi/WindowsOpPluginAdapter.h"
#include "platforms/windows/pi/WindowsOpPluginWindow.h"
#include "platforms/windows/pi/WindowsOpView.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows_common/utils/hookapi.h"
#include "platforms/windows_common/utils/fileinfo.h"

extern HINSTANCE hInst;

ATOM atomPluginWindowClass = 0;

const uni_char szPluginClassName[] = UNI_L("aPluginWinClass");

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
	, m_plugin_wrapper_window(NULL)
	, m_hwnd_view(NULL)
#ifdef MANUAL_PLUGIN_ACTIVATION
	, m_listener(NULL)
	, m_input_blocked(FALSE)
#endif // MANUAL_PLUGIN_ACTIVATION
{
}

WindowsOpPluginWindow::~WindowsOpPluginWindow()
{
	OP_ASSERT(!m_plugin);

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

		HDC new_dc = ::GetDC(m_hwnd);
		::SetBkMode(new_dc, TRANSPARENT);
		::SetMapMode(new_dc, MM_ANISOTROPIC);
		::SetWindowExtEx(new_dc, 100, 100, NULL);
		::SetViewportExtEx(new_dc, scale, scale, NULL);
		::ReleaseDC(m_hwnd, new_dc);

		MDE_RECT mr = MDE_MakeRect(rect.x, rect.y, rect.width, rect.height);
		m_hwnd_view = OP_NEW(WindowsPluginNativeWindow, (mr, m_hwnd, parent_hwnd, hInst));
		RETURN_OOM_IF_NULL(m_hwnd_view);

		RETURN_IF_ERROR(s_plugin_windows.Add(m_hwnd, this));

		static_cast<MDE_OpView*>(m_parent_view)->GetMDEWidget()->AddChild(m_hwnd_view);
		m_hwnd_view->SetZ(MDE_Z_TOP);
	}

	return OpStatus::OK;
}

void WindowsOpPluginWindow::BlockMouseInput(BOOL block)
{
	m_input_blocked = block;
	if (m_plugin_wrapper_window)
		EnableWindow(m_plugin_wrapper_window, !block);
}

void WindowsOpPluginWindow::Show()
{
	if (m_hwnd_view)
		m_hwnd_view->Show(TRUE);
}

void WindowsOpPluginWindow::Hide()
{
	if (m_hwnd_view)
		m_hwnd_view->Show(FALSE);
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

#ifdef NS4P_SILVERLIGHT_WORKAROUND
void WindowsOpPluginWindow::InvalidateWindowed(const OpRect& rect)
{
	if (m_plugin_wrapper_window && ISWINDOW(m_plugin_wrapper_window))
	{
		RECT winrect = { rect.x, rect.y, rect.x + rect.width, rect.y + rect.height };
		::InvalidateRect(m_plugin_wrapper_window, &winrect, FALSE);
	}
}
#endif // NS4P_SILVERLIGHT_WORKAROUND

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
BOOL CALLBACK WindowsOpPluginWindow::EnumChildWindowsProc(HWND hwnd, LPARAM lParam)
{
	RETURN_FALSE_IF(!hwnd);

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
					RedrawWindow(plugin_window->m_plugin_wrapper_window, &update_rect, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
			}

			break;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
			if (WindowsOpPluginWindow* plugin_window = GetPluginWindowFromHWND(hWnd))
				if (OpPluginWindowListener* listener = plugin_window->GetListener())
					if (msg == WM_LBUTTONDOWN)
						listener->OnMouseDown();
					else if (msg == WM_LBUTTONUP)
						listener->OnMouseUp();
					else
						listener->OnMouseHover();

			return 0;

		case WM_WRAPPER_WINDOW_READY:
			if (WindowsOpPluginWindow* plugin_window = GetPluginWindowFromHWND(hWnd))
			{
				plugin_window->m_plugin_wrapper_window = reinterpret_cast<HWND>(lParam);
				if (plugin_window->m_input_blocked)
					EnableWindow(plugin_window->m_plugin_wrapper_window, FALSE);
			}

			return 0;

		case WM_CLOSE:
			/* Plugin might send WM_CLOSE message not only to the plugin window
			   but also parent window so ignore WM_CLOSE here. */
			return 0;

		case WM_STYLECHANGING:
		{
			/* Disallow style changes if change is not initiated by us.
				Plugins can cause painting problems when changing them (DSK-340165). */
			if (WindowsOpPluginWindow* plugin_window = GetPluginWindowFromHWND(hWnd))
				if (plugin_window->GetNativeWindow() && !plugin_window->GetNativeWindow()->IsBeingUpdated())
				{
					STYLESTRUCT* styles = (STYLESTRUCT*)lParam;
					styles->styleNew = styles->styleOld;
				}

			return 0;
		}

		case WM_SIZE:
			if (WindowsOpPluginWindow* plugin_window = GetPluginWindowFromHWND(hWnd))
				SetWindowPos(plugin_window->m_plugin_wrapper_window, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOSENDCHANGING);
			break;
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

#endif // _PLUGIN_SUPPORT_
#endif // NS4P_COMPONENT_PLUGINS

/* static */ OP_STATUS
OpPlatformKeyEventData::Create(OpPlatformKeyEventData **key_event_data, void *data)
{
	MSG *msg = reinterpret_cast<MSG *>(data);
	OpWindowsPlatformKeyEventData *instance = OP_NEW(OpWindowsPlatformKeyEventData, (msg->wParam, msg->lParam));
	if (!instance)
		return OpStatus::ERR_NO_MEMORY;
	*key_event_data = instance;
	return OpStatus::OK;
}

/* static */ void
OpPlatformKeyEventData::IncRef(OpPlatformKeyEventData *key_event_data)
{
	static_cast<OpWindowsPlatformKeyEventData *>(key_event_data)->ref_count++;
}

/* static */ void
OpPlatformKeyEventData::DecRef(OpPlatformKeyEventData *key_event_data)
{
	OpWindowsPlatformKeyEventData *event_data = static_cast<OpWindowsPlatformKeyEventData *>(key_event_data);
	if (event_data->ref_count-- == 1)
		OP_DELETE(event_data);
}
