/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpDragManager.h"
#include "platforms/windows/pi/WindowsOpDragObject.h"
#include "platforms/windows/CustomWindowMessages.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/desktopwindowcollection.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows/pi/WindowsOpView.h"
#include "platforms/windows/utils/sync_primitives.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "platforms/windows/utils/win_icon.h"

#include "modules/dragdrop/dragdrop_manager.h"

extern WindowsOpWindow* g_main_opwindow;
extern HCURSOR hArrowCursor, hDragCopyCursor, hDragMoveCursor, hDragBadCursor;
extern HINSTANCE hInst;

#define DRAGWINDOW_CLASS_NAME UNI_L("Opera Drag and Drop Feedback")

/***********************************************************************************
**
**	OpDragManager
**
***********************************************************************************/
struct WindowsDragTemporaryData* g_drag_data;
WindowsOpDragManager* g_windows_drag_manager;

/*static*/
OP_STATUS OpDragManager::Create(OpDragManager*& manager)
{
	manager = OP_NEW(WindowsOpDragManager, ());
	return manager ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
**
**	WindowsOpDragManager
**
***********************************************************************************/

WindowsOpDragManager::WindowsOpDragManager()
{
	m_window = NULL;
	m_drag_object = NULL;
	m_has_image = FALSE;

	g_windows_drag_manager = this;
	m_notify_start = true;
	m_drop_target = NULL;
	m_source_win = NULL;
	m_wndclass = 0;
}

WindowsOpDragManager::~WindowsOpDragManager() 
{ 
	OP_DELETE(m_drop_target); 
	OP_DELETE(g_drag_data);
	g_drag_data = NULL;

	if(m_wndclass)
	{
		UnregisterClass(DRAGWINDOW_CLASS_NAME, hInst);
	}
}

/* virtual */
void WindowsOpDragManager::SetDragObject(OpDragObject* drag_object)
{
	if (drag_object != m_drag_object)
	{
		OP_DELETE(m_drag_object);
		m_drag_object = static_cast<DesktopDragObject *>(drag_object);
	}
}

OP_STATUS WindowsOpDragManager::RegisterDropTarget(HWND hwnd_target)
{
	::OleInitialize(NULL);

	if(!m_drop_target)
	{
		m_drop_target = OP_NEW(WindowsDropTarget, (hwnd_target));
		RETURN_OOM_IF_NULL(m_drop_target);
		m_drop_target->AddRef();
	}

	HRESULT res = ::RegisterDragDrop(hwnd_target, m_drop_target);

	return SUCCEEDED(res) ? OpStatus::OK : OpStatus::ERR;
}

void WindowsOpDragManager::UnregisterDropTarget(HWND hwnd_target)
{
	::RevokeDragDrop(hwnd_target);

	::OleUninitialize();
}

HWND WindowsOpDragManager::CreateDNDVisualFeedbackWindow(HWND parentHWND, RECT* size)
{
	if(m_wndclass == 0)
	{
		WNDCLASSEX wcex = {};

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_DBLCLKS;
		wcex.lpfnWndProc	= (WNDPROC) DefWindowProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInst;
		wcex.hCursor		= NULL;
		wcex.hbrBackground	= (HBRUSH)COLOR_WINDOW;
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= DRAGWINDOW_CLASS_NAME;
		wcex.hIcon  = NULL;
		wcex.hIconSm  = NULL;

		m_wndclass = RegisterClassEx(&wcex);
	}
	RECT r = *size;
	DWORD style_ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT;
	DWORD style = WS_OVERLAPPED | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	style &= ~(WS_SIZEBOX | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);

	AdjustWindowRect(&r, style, FALSE);

	HWND hwnd = CreateWindowEx(style_ex, DRAGWINDOW_CLASS_NAME, NULL, style, r.left, r.top, r.right - r.left, r.bottom - r.top, parentHWND, NULL, hInst, NULL);
	if (!hwnd)
	{
		return NULL;
	}

	// show it not activated
	::ShowWindow(m_window, SW_SHOWNA);
	::UpdateWindow(hwnd);

	return hwnd;
}

/***********************************************************************************
**
**	StartDrag
**
***********************************************************************************/

void WindowsOpDragManager::StartDrag()
{
	if (!m_drag_object)
		return;

	m_drag_object->SynchronizeCoreContent();

	g_application->SettingsChanged(SETTINGS_DRAG_BEGIN);

	// set a reasonable default size, we'll resize it later anyway
	RECT size = { 0, 0, 10, 10};
	m_window = CreateDNDVisualFeedbackWindow(g_main_opwindow->m_hwnd, &size);
	if(!m_window)
		return;

	WindowsOpWindow* window = WindowsOpWindow::FindWindowUnderCursor();
	m_source_toplevel_win = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(window);

	if(m_source_toplevel_win && m_source_toplevel_win->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		m_source_win = static_cast<BrowserDesktopWindow *>(m_source_toplevel_win)->GetActiveDesktopWindow();
	}
	m_has_image = m_drag_object->GetBitmap() ? TRUE : FALSE;

	if (m_has_image)
	{
		POINT point;

		GetCursorPos(&point);

		OpPoint bitmap_pos = m_drag_object->GetBitmapPoint();

		m_bitmap_offset.x = bitmap_pos.x;
		m_bitmap_offset.y = bitmap_pos.y;

		m_bitmap_offset.x = min(m_bitmap_offset.x, (INT32) m_drag_object->GetBitmap()->Width());
		m_bitmap_offset.y = min(m_bitmap_offset.y, (INT32) m_drag_object->GetBitmap()->Height());

		m_bitmap_offset.x = max(m_bitmap_offset.x, 0);
		m_bitmap_offset.y = max(m_bitmap_offset.y, 0);
		
		::SetWindowPos(m_window, NULL, point.x - m_bitmap_offset.x, point.y - m_bitmap_offset.y, 
			m_drag_object->GetBitmap()->Width(), m_drag_object->GetBitmap()->Height(), 
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
	else
	{
		::SetWindowPos(m_window, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE);
	}
	// show it not activated
	::ShowWindow(m_window, SW_SHOWNA);

	if(g_drag_data)
		g_drag_data->drag_acknowledged = true;
}

/***********************************************************************************
**
**	StopDrag
**
***********************************************************************************/

void WindowsOpDragManager::StopDrag(BOOL cancelled)
{
	WindowsOpWindow *window = WindowsOpWindow::FindWindowUnderCursor();
	if(!window)
	{
		// we need some window to send the event to
		BrowserDesktopWindow* bw = g_application->GetActiveBrowserDesktopWindow();  
		if(bw)
		{
			window = static_cast<WindowsOpWindow*>(bw->GetOpWindow());
		}
	}
	if(IsDragging())
	{
		if(window && window->m_hwnd)
		{
			OpStatus::Ignore(OpenFilesIfNotHandled(cancelled, m_drag_object, window));

			if (cancelled)
			{
				PostMessage(window->m_hwnd, WM_OPERA_DRAG_CANCELLED, 0, 0);
			}
			else
			{
				PostMessage(window->m_hwnd, WM_OPERA_DRAG_ENDED, 0, 0);
			}
		}
		OP_DELETE(m_drag_object);
		m_drag_object = NULL;

		if (m_window)
		{
			::DestroyWindow(m_window);
			m_window = NULL;
			SetCursor(hArrowCursor);
		}
		m_has_image = FALSE;
	}
	else if(window && window->m_hwnd)
	{
		PostMessage(window->m_hwnd, WM_OPERA_DRAG_ENDED, 0, 0);
	}
}

void WindowsOpDragManager::HideDragWindow()
{
	if(m_window)
		ShowWindow(m_window, SW_HIDE);
}

/***********************************************************************************
**
**	UpdateDrag
**
***********************************************************************************/
extern void PPTBlt (HWND hWnd, HDC dst, HDC src, BYTE transparency = 0xff);

void WindowsOpDragManager::UpdateDrag(WindowsOpWindow *window)
{
	if (IsDragging())
	{
		POINT point;

		if (m_drag_object->GetType() == OpTypedObject::DRAG_TYPE_BOOKMARK && DragDrop_Data_Utils::HasURL(m_drag_object))
		{
			m_drag_object->SetSuggestedDropType(DROP_COPY);
			m_drag_object->SetDesktopDropType(DROP_COPY);
		}
		if (window && window->m_mde_screen)
		{
			PostMessage(window->m_hwnd, WM_OPERA_DRAG_MOVED, 0, 0);
		}
		else if(g_drag_data && !g_drag_data->dnd_thread.IsRunning())
		{
			OpString origin_url;
			// don't start external dnd if we can't find our own window (eg. for the installer)
			if(window)
			{
				// Outside Opera window start system d'n'd handling.  Treat window decoration as external.
				g_drag_data->dnd_thread.Start(window->m_mde_screen, origin_url);
			}
		}
		if (m_has_image)
		{
			GetCursorPos(&point);

			::SetWindowPos(m_window, HWND_TOP, point.x - m_bitmap_offset.x, point.y - m_bitmap_offset.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
		PAINTSTRUCT ps;

		// layered windows are not painted in WM_PAINT
		HDC hdc = BeginPaint(m_window, &ps);

		if (m_drag_object->GetBitmap())
		{
			OpBitmap* opbmp = (OpBitmap*)m_drag_object->GetBitmap();
			HBITMAP img = CreateHBITMAPFromOpBitmap(opbmp);
			if(img)
			{
				HDC imgdc = CreateCompatibleDC(hdc);
				HBITMAP oldbitmap = (HBITMAP)SelectObject(imgdc, img);

				PPTBlt(m_window, hdc, imgdc, 175);

				SelectObject(imgdc, oldbitmap);
				DeleteDC(imgdc);

				DeleteObject(img);
			}
		}
		else
		{
			RECT rect;

			GetClientRect(m_window, &rect);

			FillRect(hdc, &rect, (HBRUSH) GetStockObject(BLACK_BRUSH));
		}
		EndPaint(m_window, &ps);
	}
}

WindowsDragDropExternalThread::WindowsDragDropExternalThread()
	: m_thread(NULL)
	, m_owner_thread_id(0)
	, m_is_running(FALSE)
	, m_screen(NULL)
{
}

BOOL WindowsDragDropExternalThread::IsRunning()
{
	return m_is_running;
}

void WindowsDragDropExternalThread::Finish()
{
	m_is_running = FALSE;
}

WindowsDragDropExternalThread::~WindowsDragDropExternalThread()
{
}

/* static */
void* WindowsDragDropExternalThread::ThreadProc(void* ptr)
{
	WindowsDragDropExternalThread* thread = static_cast<WindowsDragDropExternalThread*>(ptr);
	::AttachThreadInput(::GetCurrentThreadId(), thread->m_owner_thread_id, TRUE);
	thread->Run();
	return 0;
}

OP_STATUS WindowsDragDropExternalThread::Start(VegaMDEScreen* screen, OpStringC origin_url)
{
	OP_ASSERT(!m_is_running && "If this asserts, the thread is already running!");
	if(m_is_running)
		return OpStatus::ERR;

	m_is_running = TRUE;
	m_screen = screen;

	g_windows_drag_manager->GetDragData().Clear();

	OpDragObject* drag_object = g_drag_manager->GetDragObject();
	OP_ASSERT(drag_object);
	if(drag_object)
	{
		RETURN_IF_ERROR(g_windows_drag_manager->GetDragData().InitializeFromDragObject(drag_object));
		RETURN_IF_ERROR(m_origin_url.Set(DragDropManager::GetOriginURL(drag_object)));
	}
	if(m_origin_url.IsEmpty())
	{
		RETURN_IF_ERROR(m_origin_url.Set(origin_url));
	}
	screen->TrigDragLeave(g_op_system_info->GetShiftKeyState());

	DWORD id;
	m_owner_thread_id = ::GetCurrentThreadId();
	m_thread = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WindowsDragDropExternalThread::ThreadProc, this, 0, &id);
	if (!m_thread)
		Finish();

	return OpStatus::OK;
}

void WindowsDragDropExternalThread::Run()
{
	CF_HTML = ::RegisterClipboardFormatW(UNI_L("HTML Format"));
	CF_URILIST = ::RegisterClipboardFormatW(UNI_L("text/uri-list"));
	CF_MOZURL = ::RegisterClipboardFormatW(UNI_L("text/x-moz-url"));

	OpAutoPtr<WindowsDropSource> ids_ptr(OP_NEW(WindowsDropSource, ()));
	OpAutoPtr<WindowsDataObject> ido_ptr(OP_NEW(WindowsDataObject, (ids_ptr.get())));
	if (!ido_ptr.get() || !ids_ptr.get())
	{
		Finish();
		return;
	}
	WindowsDNDData& drag_data = g_windows_drag_manager->GetDragData();
	{
		WinAutoLock lock(&drag_data.m_cs);

		ido_ptr->SetOriginURL(m_origin_url);
		const char *uri_list = NULL;
		unsigned long uri_list_len = 0;
		OpDragObject *drag_object = g_drag_manager->GetDragObject();

		for (unsigned int i = 0; i < drag_data.m_data_array.GetCount(); i++)
		{
			WindowsTypedBuffer* data_buffer = drag_data.m_data_array.Get(i);
			if(data_buffer->data)	// might be NULL for Speed Dial tabs and similar
			{
				if(!ido_ptr->SetData(drag_object, data_buffer->content_type, data_buffer->data, data_buffer->length))
				{
					Finish();
					return;
				}
				else if (op_strcmp(data_buffer->content_type, "text/uri-list") == 0)
				{
					uri_list = data_buffer->data;
					uri_list_len = data_buffer->length;
				}
			}
		}

		if (uri_list && !ido_ptr->SetData(drag_object, "text/x-moz-url", uri_list, uri_list_len))
		{
			Finish();
			return;
		}
	}
	void* reserved = 0;
	HRESULT res = ::OleInitialize(reserved);
	OP_ASSERT(SUCCEEDED(res));

	DWORD drop_effect;

	WindowsDropTarget *drop_target = g_windows_drag_manager->GetDropTarget();
//	OP_ASSERT(drop_target);
	if(drop_target)
	{
		drop_target->SetExternal(FALSE);
	}
	ids_ptr->AddRef();

	DWORD effects = DROPEFFECT_COPY | DROP_MOVE | DROP_LINK;
	if (drag_data.m_effects_allowed != DROP_UNINITIALIZED)
	{
		if (drag_data.m_effects_allowed != DROP_NONE)
		{
			effects = DROPEFFECT_SCROLL;
			if ((drag_data.m_effects_allowed & DROP_COPY) == DROP_COPY)
			{
				effects |= DROPEFFECT_COPY;
			}

			if ((drag_data.m_effects_allowed & DROP_LINK) == DROP_LINK)
			{
				effects |= DROPEFFECT_LINK;
			}

			if ((drag_data.m_effects_allowed & DROP_MOVE) == DROP_MOVE)
			{
				effects |= DROPEFFECT_MOVE;
			}
		}
		else
			effects = DROPEFFECT_NONE;
	}
	WindowsDropSource* ids = ids_ptr.release();

	res = ::DoDragDrop(ido_ptr.release(), ids, effects, &drop_effect);

	ids->Release();

//	OP_ASSERT(drop_target);
	if(drop_target)
	{
		drop_target->SetExternal(TRUE);
	}
	BrowserDesktopWindow* bw = g_application->GetActiveBrowserDesktopWindow();  
	WindowsOpWindow* window = static_cast<WindowsOpWindow*>(bw->GetOpWindow());
	OP_ASSERT(window);
	if(!window)
	{
		window = WindowsOpWindow::FindWindowUnderCursor();
	}
	// We want to call op_drag_enter(), op_drag_cancel() or op_drag_end() after
	// external dnd has finished, but those functions have to be called from
	// main thread, thus there's need to communicate with the main thread using
	// system SendMessage()s.
	if (res == (DRAGDROP_S_CANCEL | 0x01000000L))
	{
		// Dragging has returned back to the Opera window.
		// Continue internal d'n'd.
		POINT client_point;
		::GetCursorPos(&client_point);

		if (window && window->m_hwnd)
		{
			::ScreenToClient(window->m_hwnd, &client_point);
			::PostMessageW(window->m_hwnd, WM_OPERA_DRAG_CAME_BACK, client_point.x, client_point.y);
		}
	}
	else if (res == DRAGDROP_S_CANCEL)
	{
		// User has pressed ESC key during dragging.
		// End of d'n'd operation.
		if (window && window->m_hwnd)
		{
			::PostMessage(window->m_hwnd, WM_OPERA_DRAG_EXTERNAL_CANCEL, 0, 0);
		}
	}
	else // if (res == DRAGDROP_S_DROP) or something went wrong.
	{
		if (window && window->m_hwnd)
		{
			::PostMessage(window->m_hwnd, WM_OPERA_DRAG_EXTERNAL_DROPPED, 0, 0);
		}
	}
	OleUninitialize();

	Finish();
}

/*
* Drag and drop support functions
*/

/* static */
OP_STATUS WindowsOpDragManager::PrepareDragOperation(WindowsOpWindow* window, int x, int y)
{
	OP_ASSERT(window->m_hwnd);

	// only do this for the top-level window
	if(window->m_hwnd && !g_drag_data)
	{
		g_drag_data = OP_NEW(WindowsDragTemporaryData, ());
		RETURN_OOM_IF_NULL(g_drag_data);
	}
	g_drag_data->drag_x = x;
	g_drag_data->drag_y = y;
	g_drag_data->drag_acknowledged = false;

	return OpStatus::OK;
}

OP_STATUS WindowsOpDragManager::OpenFilesIfNotHandled(BOOL drop_cancelled, DesktopDragObject* drag_object, WindowsOpWindow *window)
{
	// if cancelled param is FALSE or DROP effect is DROP_NONE and files are in the object then they should be opened
	if(!drag_object || drop_cancelled || drag_object->GetDropType() != DROP_NOT_AVAILABLE)
		return OpStatus::OK;

	POINT point;

	GetCursorPos(&point);
	ScreenToClient(window->m_hwnd, &point);

	// open the file(s) normally as core didn't want it for HTML 5 dnd
	OpAutoVector<OpString> file_names;

	OpDragDataIterator& iter = drag_object->GetDataIterator();
	if (!iter.First())
		return OpStatus::OK;
	do
	{
		if (iter.IsFileData())
		{
			OpAutoPtr<OpString> filename(OP_NEW(OpString, ()));

			RETURN_IF_ERROR(filename->Set(iter.GetFileData()->GetFullPath()));
			RETURN_IF_ERROR(file_names.Add(filename.get()));
			filename.release();
		}
	}
	while (iter.Next());

	if(file_names.GetCount())
	{
		// Find the topmost DesktopWindow for the child found at the given coordinate.
		WindowsOpWindow *desktop_window = NULL;
		MDE_View *view = window->m_mde_screen->GetViewAt(point.x, point.y, true);
		while (view)
		{
			if (view->IsType("MDE_Widget"))
			{
				WindowsOpWindow *win = (WindowsOpWindow*) ((MDE_Widget*)view)->GetOpWindow();
				if (win && win != window && win->GetDesktopWindow())
					desktop_window = win;
			}
			view = view->m_parent;
		}
		if (desktop_window && desktop_window->m_window_listener)
			desktop_window->m_window_listener->OnDropFiles(file_names);
		else if (window->m_window_listener)
			window->m_window_listener->OnDropFiles(file_names);					
	}
	return OpStatus::OK;
}


