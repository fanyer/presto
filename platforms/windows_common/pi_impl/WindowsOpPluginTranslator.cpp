/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/ns4plugins/component/plugin_component_instance.h"
#include "modules/pi/OpPluginImage.h"

#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"
#include "platforms/windows_common/pi_impl/WindowsOpPluginTranslator.h"
#include "platforms/windows_common/src/generated/g_message_windows_common_messages.h"

#if defined(_PLUGIN_SUPPORT_) && defined(NS4P_COMPONENT_PLUGINS)

extern HINSTANCE hInst;

/* Class of the plugin window on the wrapper side. */
const uni_char WindowsOpPluginTranslator::s_wrapper_wndproc_class[] = UNI_L("PluginWrapperWindow");
OpWindowsPointerHashTable<const HWND, WindowsOpPluginTranslator*> WindowsOpPluginTranslator::s_wrapper_windows;
OpWindowsPointerHashTable<const HWND, void*> WindowsOpPluginTranslator::s_old_wndprocs;
/* Dummy window for use in hooked TrackPopupMenu. */
HWND WindowsOpPluginTranslator::s_dummy_contextmenu_window = NULL;
const uni_char WindowsOpPluginTranslator::s_dummy_activation_wndproc_class[] = UNI_L("PluginContextActivationWindow");
/* List of known browser window handles for use in hooked GetWindowInfo and others.
   Handle is a pointer but also it can be shared between processes so we are
   assuming that INT32 is appropriate in this case. */
OpINT32Vector WindowsOpPluginTranslator::s_browser_window_handles;
/* Tracking focused plugin instance. Answer is valid only within this process. */
WindowsOpPluginTranslator* WindowsOpPluginTranslator::s_focused_plugin = NULL;
/* Whether subclassing should be removed before (NO) or after (YES) calling window procedure of subclassed window (DSK-365584). */
BOOL3 WindowsOpPluginTranslator::s_unsubclass_after_wndproc = MAYBE;

OP_STATUS OpPluginTranslator::Create(OpPluginTranslator** translator, const OpMessageAddress& instance, const OpMessageAddress* adapter, const NPP npp)
{
	if (!adapter)
		return OpStatus::ERR_NULL_POINTER;

	// Create the channel and store inside newly created instance.
	OpChannel* channel = g_component->CreateChannel(*adapter);
	RETURN_OOM_IF_NULL(channel);

	// Create the plugin translator.
	*translator = OP_NEW(WindowsOpPluginTranslator, (channel, reinterpret_cast<PluginComponentInstance*>(npp->ndata)));
	if (!*translator)
	{
		OP_DELETE(channel);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

bool OpPluginTranslator::GetGlobalValue(NPNVariable variable, void* ret_value, NPError* result)
{
	return false;
}

WindowsOpPluginTranslator::WindowsOpPluginTranslator(OpChannel* channel, PluginComponentInstance* component_instance)
		: m_channel(channel)
		, m_plugin_window(NULL)
		, m_wrapper_window(NULL)
		, m_hdc(NULL)
		, m_old_cliprect(NULL)
		, m_shared_mem(NULL)
		, m_bitmap(NULL)
		, m_need_to_recreate_bitmap(true)
		, m_component_instance(component_instance)
{
}

WindowsOpPluginTranslator::~WindowsOpPluginTranslator()
{
	OP_DELETE(m_channel);
	DeleteDC(m_hdc);

	if (GetFocusedPlugin() == this)
		SetFocusedPlugin(NULL);

	WindowsOpPluginTranslator* wrapper = NULL;
	OpStatus::Ignore(s_wrapper_windows.Remove(m_wrapper_window, &wrapper));

	if (m_shared_mem)
		if (WindowsSharedBitmapManager* bitmap_manager = WindowsSharedBitmapManager::GetInstance())
			bitmap_manager->CloseMemory(m_identifier);
}

OP_STATUS WindowsOpPluginTranslator::UpdateNPWindow(NPWindow& out_npwin, const OpRect& rect, const OpRect& clip_rect, NPWindowType type)
{
	// Request window information from the browser component.
	// Hopefully we won't have to do it later, if core updates window handle by itself.
	if (m_channel && !m_plugin_window)
	{
		OpSyncMessenger sync(m_channel);
		RETURN_IF_ERROR(sync.SendMessage(OpWindowsPluginWindowInfoMessage::Create()));

		// Take back ownership of the channel.
		m_channel = sync.TakePeer();

		// Get response message from browser component.
		OpWindowsPluginWindowInfoResponseMessage* response = OpWindowsPluginWindowInfoResponseMessage::Cast(sync.GetResponse());
		RETURN_OOM_IF_NULL(response);

		m_plugin_window = reinterpret_cast<HWND>(response->GetWindow());

		if (type == NPWindowTypeDrawable)
		{
			if (!m_hdc)
			{
				HDC dc = GetDC(m_plugin_window);
				m_hdc = CreateCompatibleDC(dc);
				ReleaseDC(m_plugin_window, dc);
			}

			// Create dummy context menu activation window for Flash. We should
			// have some quirks system so that we do that only for Flash.
			if (!s_dummy_contextmenu_window)
				CreateDummyPopupActivationWindow(m_plugin_window);

			// Add the browser window pointer to the list.
			if (s_browser_window_handles.Find(reinterpret_cast<INT32>(m_plugin_window) == -1))
				s_browser_window_handles.Add(reinterpret_cast<INT32>(m_plugin_window));
		}
		else if (!m_wrapper_window)
		{
			m_wrapper_window = CreateWindow(s_wrapper_wndproc_class, UNI_L(""), WS_CHILD | WS_VISIBLE, 0, 0, rect.width, rect.height, m_plugin_window, NULL, hInst, NULL);

			if (!m_wrapper_window)
				return OpStatus::ERR;

#ifndef SIXTY_FOUR_BIT
			/* Some plugins use ANSI API's to retrieve the window procedure. If we
			   don't set it explicitly, the plugin will get garbage pointer and
			   crash on calling it.*/
			SetWindowLongPtrA(m_wrapper_window, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(WindowsOpPluginTranslator::WrapperWndProc));
#endif // !SIXTY_FOUR_BIT

			s_wrapper_windows.Add(m_wrapper_window, this);
			PostMessage(m_plugin_window, WM_WRAPPER_WINDOW_READY, 0, reinterpret_cast<LPARAM>(m_wrapper_window));
		}
	}

	if (rect.width > m_plugin_rect.width || rect.height > m_plugin_rect.height)
		m_need_to_recreate_bitmap = true;

	// Store plugin rect.
	m_plugin_rect.Set(rect);

	// Set up position and size.
	out_npwin.x = rect.x;
	out_npwin.y = rect.y;
	out_npwin.width = rect.width;
	out_npwin.height = rect.height;

	// Set-up clip-rect.
	out_npwin.clipRect.top = clip_rect.Top();
	out_npwin.clipRect.right = clip_rect.Right();
	out_npwin.clipRect.bottom = clip_rect.Bottom();
	out_npwin.clipRect.left = clip_rect.Left();

	// Set up window and type.
	out_npwin.type = type;
	out_npwin.window = type == NPWindowTypeWindow ? reinterpret_cast<void*>(m_wrapper_window) : m_hdc;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginTranslator::CreateFocusEvent(OpPlatformEvent** out_event, bool got_focus)
{
	SetFocusedPlugin(got_focus ? this : NULL);

	WindowsOpPlatformEvent* event = OP_NEW(WindowsOpPlatformEvent, ());
	RETURN_OOM_IF_NULL(event);

	event->m_event.event = got_focus ? WM_SETFOCUS : WM_KILLFOCUS;
	event->m_event.wParam = 0;
	event->m_event.lParam = 0;

	*out_event = event;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginTranslator::CreateKeyEventSequence(OtlList<OpPlatformEvent*>& events, OpKey::Code key, uni_char value, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64 data1, UINT64 data2)
{
	WindowsOpPlatformEvent* event = OP_NEW(WindowsOpPlatformEvent, ());
	RETURN_OOM_IF_NULL(event);

	// Core generates a key_down and a key_pressed for the first WM_KEYDOWN it receives.
	// If bit 30 of lParam is not set, it means that the key_press message isn't indicating
	// a key repeat and we can ignore it.
	if ((key_state == PLUGIN_KEY_PRESSED) && ((data2 & 1<<30) == 0))
		return OpStatus::OK;

	event->m_event.event = (key_state == PLUGIN_KEY_UP) ? WM_KEYUP : WM_KEYDOWN;
	event->m_event.wParam = static_cast<WPARAM>(data1);
	event->m_event.lParam = static_cast<LPARAM>(data2);

	RETURN_IF_ERROR(events.Append(event));

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginTranslator::CreateMouseEvent(OpPlatformEvent** out_event, OpPluginEventType event_type, const OpPoint& point, int button_or_delta, ShiftKeyState modifiers)
{
	WindowsOpPlatformEvent* event = OP_NEW(WindowsOpPlatformEvent, ());
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
			// Cached value of the SPI_GETWHEELSCROLLLINES user setting.
			static UINT lines_to_scroll_value = 0;

			// Delta value got from core is converted to the number of lines that
			// should be scrolled. It needs to be converted back to delta.
			// We can't do that precisely though as number of lines is rounded
			// but that is not really an issue.
			if (!lines_to_scroll_value)
			{
				SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0 , &lines_to_scroll_value, 0);
				// Guard from 0 value that would crash on dividing.
				lines_to_scroll_value = MAX(1, lines_to_scroll_value);
			}

			int delta = (-button_or_delta / lines_to_scroll_value) * WHEEL_DELTA;

			event->m_event.event = event_type == PLUGIN_MOUSE_WHEELV_EVENT ? WM_MOUSEWHEEL : WM_MOUSEHWHEEL;
			event->m_event.wParam = MAKEWPARAM(0, delta);
			break;
		}

		default:
			OP_DELETE(event);
			return OpStatus::ERR_NOT_SUPPORTED;
	}

	// Convert from plugin to window coordinates.
	event->m_event.lParam = MAKELONG(m_plugin_rect.x + point.x, m_plugin_rect.y + point.y);

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

	*out_event = event;

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginTranslator::CreatePaintEvent(OpPlatformEvent** out_event, OpPluginImageID dest, OpPluginImageID bg, const OpRect& paint_rect, NPWindow* npwin, bool* npwindow_was_modified)
{
	OpAutoPtr<WindowsOpPlatformEvent> event = OP_NEW(WindowsOpPlatformEvent, ());
	RETURN_OOM_IF_NULL(event.get());

	// Create rect in window coordinates.
	OpAutoPtr<RECT> dirty_area = OP_NEW(RECT, ());
	RETURN_OOM_IF_NULL(dirty_area.get());

	dirty_area->left = m_plugin_rect.x; // + paint_rect.x;
	dirty_area->top = m_plugin_rect.y; // + paint_rect.y;
	dirty_area->right = m_plugin_rect.x + m_plugin_rect.width; // dirty_area->left + paint_rect.width;
	dirty_area->bottom = m_plugin_rect.y + m_plugin_rect.height; // dirty_area->top + paint_rect.height;

	SetViewportOrgEx(m_hdc, -m_plugin_rect.x, -m_plugin_rect.y, NULL);

	// Open shared memory and create bitmap the size of plugin rect.
	if (!m_shared_mem || m_need_to_recreate_bitmap)
	{
		WindowsSharedBitmapManager* bitmap_manager = WindowsSharedBitmapManager::GetInstance();
		RETURN_OOM_IF_NULL(bitmap_manager);

		m_shared_mem = bitmap_manager->OpenMemory(dest);
		RETURN_OOM_IF_NULL(m_shared_mem);
		m_identifier = dest;

		int width;
		int height;
		m_shared_mem->GetDimensions(width, height);

		BITMAPINFO bi = {0};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = width;
		bi.bmiHeader.biHeight = -height;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		bi.bmiHeader.biSizeImage = 0;
		bi.bmiHeader.biXPelsPerMeter = 0;
		bi.bmiHeader.biYPelsPerMeter = 0;
		bi.bmiHeader.biClrUsed = 0;
		bi.bmiHeader.biClrImportant = 0;

		void* bitmap_data = NULL;
		m_bitmap = CreateDIBSection(m_hdc, &bi, DIB_RGB_COLORS, &bitmap_data, m_shared_mem->GetFileMappingHandle(), m_shared_mem->GetDataOffset());
		RETURN_OOM_IF_NULL(m_bitmap);
		SelectObject(m_hdc, m_bitmap);

		m_need_to_recreate_bitmap = false;
	}

	event->m_event.event = WM_PAINT;
	event->m_event.wParam = reinterpret_cast<uintptr_t>(m_hdc);
	event->m_event.lParam = reinterpret_cast<uintptr_t>(dirty_area.release());

	*out_event = event.release();

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginTranslator::FinalizeOpPluginImage(OpPluginImageID, const NPWindow&)
{
	SetViewportOrgEx(m_hdc, 0, 0, NULL);

	return OpStatus::OK;
}

OP_STATUS WindowsOpPluginTranslator::CreateWindowPosChangedEvent(OpPlatformEvent** out_event)
{
	WindowsOpPlatformEvent* event = OP_NEW(WindowsOpPlatformEvent, ());
	RETURN_OOM_IF_NULL(event);

	// Set structure with plugin rect relative to the browser window.
	WINDOWPOS *winpos = OP_NEW(WINDOWPOS, ());
	op_memset(winpos, 0, sizeof(WINDOWPOS));
	winpos->x = m_plugin_rect.x;
	winpos->y = m_plugin_rect.y;
	winpos->cx = m_plugin_rect.width;
	winpos->cy = m_plugin_rect.height;

	event->m_event.event = WM_WINDOWPOSCHANGED;
	event->m_event.wParam = 0;
	event->m_event.lParam = reinterpret_cast<uintptr_t>(winpos);

	*out_event = event;

	return OpStatus::OK;
}

bool WindowsOpPluginTranslator::GetValue(NPNVariable variable, void* ret_value, NPError* result)
{
	if (variable == NPNVnetscapeWindow)
	{
		if (!m_plugin_window)
			return false;

		*(static_cast<void**>(ret_value)) = m_plugin_window;
		return true;
	}

	return false;
}

bool WindowsOpPluginTranslator::ConvertPlatformRegionToRect(NPRegion invalid_region, NPRect &invalid_rect)
{
	RECT rect;
	if (GetRgnBox(static_cast<HRGN>(invalid_region), &rect) == SIMPLEREGION)
	{
		invalid_rect.top = static_cast<uint16>(rect.top);
		invalid_rect.left = static_cast<uint16>(rect.left);
		invalid_rect.bottom = static_cast<uint16>(rect.bottom);
		invalid_rect.right = static_cast<uint16>(rect.right);
		return TRUE;
	}

	return FALSE;
}

void WindowsOpPluginTranslator::SubClassPluginWindowproc(HWND hwnd)
{
	WNDPROC plugin_wndproc;
	if (!s_old_wndprocs.Contains(hwnd))
	{
		if (hwnd == m_wrapper_window)
			plugin_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowsOpPluginTranslator::WrapperSubclassWndProc)));
		else
			plugin_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowsOpPluginTranslator::WrapperSubclassChildWndProc)));
		s_old_wndprocs.Add(hwnd, plugin_wndproc);
	}
}

/*static*/
LRESULT WindowsOpPluginTranslator::CallSubclassedWindowProc(void* wndproc, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (s_unsubclass_after_wndproc == MAYBE)
	{
		WindowsOpPluginTranslator* wrapper = NULL;
		if (OpStatus::IsSuccess(s_wrapper_windows.GetData(hWnd, &wrapper)) && wrapper)
		{
			PluginComponent* component = wrapper->m_component_instance ? wrapper->m_component_instance->GetComponent() : NULL;
			if (component && component->GetLibrary())
			{
				UniString name;
				if (OpStatus::IsSuccess(component->GetLibrary()->GetName(&name)) && name.StartsWith(UNI_L("Google Talk Plugin")))
				{
					s_unsubclass_after_wndproc = YES; // fix for DSK-365364
				}
				else
				{
					s_unsubclass_after_wndproc = NO;
				}
			}
		}
	}

	if (s_unsubclass_after_wndproc != YES)
	{
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndproc));
	}
	LRESULT ret = CallWindowProc(reinterpret_cast<WNDPROC>(wndproc), hWnd, msg, wParam, lParam);
	if (s_unsubclass_after_wndproc == YES)
	{
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndproc));
	}
	return ret;
}

OP_STATUS WindowsOpPluginTranslator::GetPluginFromHWND(HWND hwnd, BOOL search_parents, WindowsOpPluginTranslator* &wrapper)
{
	OP_STATUS err = s_wrapper_windows.GetData(hwnd, &wrapper);

	if (!search_parents || (OpStatus::IsSuccess(err) && wrapper))
		return err;

	hwnd = GetParent(hwnd);
	if (hwnd == NULL)
		return OpStatus::ERR;
	else
		return GetPluginFromHWND(hwnd, search_parents, wrapper);
}

/*static*/
void WindowsOpPluginTranslator::RegisterWrapperWndProcClass()
{
	WNDCLASSEX plugin_window_class = {0};
	plugin_window_class.cbSize = sizeof(plugin_window_class);
	plugin_window_class.style = CS_DBLCLKS;
	plugin_window_class.lpfnWndProc = WindowsOpPluginTranslator::WrapperWndProc;
	plugin_window_class.hInstance = hInst;
	plugin_window_class.lpszClassName = s_wrapper_wndproc_class;

	RegisterClassEx(&plugin_window_class);
}

/*static*/
void WindowsOpPluginTranslator::UnregisterWrapperWndProcClass()
{
	UnregisterClass(s_wrapper_wndproc_class, NULL);
}

/*static*/
LRESULT WindowsOpPluginTranslator::WrapperWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_NCDESTROY:
		{
			if (s_wrapper_windows.Contains(hWnd))
			{
				WindowsOpPluginTranslator* wrapper = NULL;
				OpStatus::Ignore(s_wrapper_windows.Remove(hWnd, &wrapper));
				wrapper->m_wrapper_window = NULL;
			}
			break;
		}

		case WM_ENTERMENULOOP:
		case WM_EXITMENULOOP:
		case WM_MENUSELECT:
			/* Opera window has to know about these events so that it can reset
			   InputManager state when invoking plugin's context menu. It doesn't
			   really matter to which Opera window these messages are sent. */
			if (s_browser_window_handles.GetCount())
				SendMessage(reinterpret_cast<HWND>(s_browser_window_handles.Get(0)), msg, wParam, lParam);
			break;

		// Fix for DSK-256843, workaround for a bug in flash.
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			::ReleaseCapture();
			break;


		default:
		{
			LRESULT result;
			if (HandleMessage(hWnd, msg, wParam, lParam, result))
				return result;
		}
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

/*static*/
LRESULT WindowsOpPluginTranslator::WrapperSubclassChildWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//We are receiving a sent (and thus probably blocking) message, probably from a parent process.
	//Reply to it before handling it
	if (InSendMessage() && (msg == WM_WINDOWPOSCHANGED || msg == WM_KILLFOCUS || msg == WM_SETFOCUS || msg == WM_PAINT))
		ReplyMessage(0);

	void* old_wndproc;
	if (OpStatus::IsError(s_old_wndprocs.Remove(hWnd, &old_wndproc)))
		return NULL;

	if (old_wndproc)
	{
		return CallSubclassedWindowProc(old_wndproc, hWnd, msg, wParam, lParam);
	}

	//If this happens, it means that some window got its wndproc subclassed by this one, but the old window proc
	//wasn't found in s_old_wndprocs.
	OP_ASSERT(false);
	SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_wndproc));
	return WrapperWndProc(hWnd, msg, wParam, lParam);
}

/*static*/
LRESULT WindowsOpPluginTranslator::WrapperSubclassWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//We are receiving a sent (and thus probably blocking) message, probably from a parent process.
	//Reply to it before handling it
	if (InSendMessage() && (msg == WM_WINDOWPOSCHANGED || msg == WM_KILLFOCUS || msg == WM_SETFOCUS || msg == WM_PAINT))
		ReplyMessage(0);

	void* old_wndproc;
	if (OpStatus::IsError(s_old_wndprocs.Remove(hWnd, &old_wndproc)))
		return NULL;

	if (old_wndproc)
	{
		switch (msg)
		{
			case WM_DESTROY:
			{
				WindowsOpPluginTranslator* wrapper = NULL;
				GetPluginFromHWND(hWnd, FALSE, wrapper);
				//Fix for DSK-350479
				if (InSendMessage())
					wrapper->m_component_instance->SetRequestsAllowed(FALSE);
				break;
			}

			case WM_NCCALCSIZE:
				if (InSendMessage())
				{
					SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_wndproc));
					return 0;
				}
		}

		LRESULT result;
		if (HandleMessage(hWnd, msg, wParam, lParam, result))
		{
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_wndproc));
			return result;
		}
		else
		{
			return CallSubclassedWindowProc(old_wndproc, hWnd, msg, wParam, lParam);
		}
	}

	//If this happens, it means that some window got its wndproc subclassed by this one, but the old window proc
	//wasn't found in s_old_wndprocs.
	OP_ASSERT(false);
	SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old_wndproc));
	return WrapperWndProc(hWnd, msg, wParam, lParam);
}

/*static*/
bool WindowsOpPluginTranslator::HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result)
{
	bool handled = false;

	switch (msg)
	{
		/* Prevent some modifications to plugin window by the plugin. This
		   message is triggered by ShowWindow, SetWindowPos and probably many
		   other API calls so it should handle most cases. */
		case WM_WINDOWPOSCHANGING:
		{
			WINDOWPOS* winpos = reinterpret_cast<WINDOWPOS*>(lparam);
			winpos->flags &= ~SWP_SHOWWINDOW & ~SWP_HIDEWINDOW;
			winpos->flags |= SWP_NOMOVE;
			result = 0;
			handled = true;
		}

		case WM_SETFOCUS:
		{
			WindowsOpPluginTranslator* wrapper = NULL;
			if (OpStatus::IsSuccess(s_wrapper_windows.GetData(hwnd, &wrapper)))
				SetFocusedPlugin(wrapper);
			break;
		}

		case WM_KILLFOCUS:
			SetFocusedPlugin(NULL);
			break;
	}

	return handled;
}

/*static*/
void WindowsOpPluginTranslator::CreateDummyPopupActivationWindow(HWND parent_window)
{
	if (s_dummy_contextmenu_window)
		return;

	// Reusing normal wrapper window proc. Might be better to create new class for it.
	s_dummy_contextmenu_window = CreateWindow(s_wrapper_wndproc_class, NULL, WS_CHILD, 0, 0, 0, 0, parent_window, NULL, hInst, NULL);
}

/*static*/
void WindowsOpPluginTranslator::RemoveDummyPopupActivationWindow()
{
	if (s_dummy_contextmenu_window)
	{
		DestroyWindow(s_dummy_contextmenu_window);
		s_dummy_contextmenu_window = NULL;
	}
}

/*static*/
WINBOOL WINAPI WindowsOpPluginTranslator::TrackPopupMenuHook(HMENU menu, UINT flags, int x, int y, int reserved, HWND hwnd, const RECT* rect)
{
	/* Overriding window handle because Flash in opaque mode calls this method
	   with handle that is in another process and that fails.
	   If plugin wants to be notified about menu selection using the return
	   value then we can safely override window handle as it should not need
	   to handle the menu messages. Otherwise better not mess with it. */
	if (s_dummy_contextmenu_window && (flags & TPM_RETURNCMD))
		hwnd = s_dummy_contextmenu_window;

	return TrackPopupMenu(menu, flags, x, y, reserved, hwnd, rect);
}

/*static*/
WINBOOL WINAPI WindowsOpPluginTranslator::GetWindowInfoHook(HWND hwnd, PWINDOWINFO pwi)
{
	/* Replace window rect with client rect if plugin attempts
	   to retrieve info about our browser window. */
	static HWND last_opera_hwnd = NULL;

	WINBOOL ret = GetWindowInfo(hwnd, pwi);

	if (ret && (hwnd == last_opera_hwnd || s_browser_window_handles.Find(reinterpret_cast<INT32>(hwnd) != -1)))
	{
		pwi->rcWindow = pwi->rcClient;
		last_opera_hwnd = hwnd;
	}

	return ret;
}

/* static */
HWND WINAPI WindowsOpPluginTranslator::GetFocusHook()
{
	if (!GetFocusedPlugin())
		return reinterpret_cast<HWND>(-1);

	return GetFocus();
}

#endif // _PLUGIN_SUPPORT_ && NS4P_COMPONENT_PLUGINS

#endif // NS4P_COMPONENT_PLUGINS
