/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpDragObject.h"
#include "platforms/windows/pi/WindowsOpDragManager.h"
#include "platforms/windows/CustomWindowMessages.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/pi/WindowsOpMessageLoop.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

// formats we send but don't accept:
CLIPFORMAT CF_INETURLW;
CLIPFORMAT CF_INETURLA;
CLIPFORMAT CF_FILECONTENTS;
CLIPFORMAT CF_FILEDESCRIPTOR;

// we accept these
CLIPFORMAT CF_HTML;
CLIPFORMAT CF_URILIST;
CLIPFORMAT CF_MOZURL;

extern WindowsOpDragManager* g_windows_drag_manager;

/* static */
OP_STATUS OpDragObject::Create(OpDragObject*& object, OpTypedObject::Type type)
{
	RETURN_OOM_IF_NULL(object = OP_NEW(WindowsOpDragObject, (type)));
	return OpStatus::OK;
}

WindowsOpDragObject::~WindowsOpDragObject()
{
}

WindowsOpDragObject::WindowsOpDragObject(OpTypedObject::Type type) : DesktopDragObject(type)
{

}

DropType WindowsOpDragObject::GetSuggestedDropType() const
{
	ShiftKeyState modifiers = g_op_system_info->GetShiftKeyState();
	if (modifiers == 0) 
		return DesktopDragObject::GetSuggestedDropType(); 

	if((modifiers & (SHIFTKEY_ALT | SHIFTKEY_CTRL)) == (SHIFTKEY_ALT | SHIFTKEY_CTRL))
		return DROP_COPY;
	if((modifiers & (SHIFTKEY_SHIFT | SHIFTKEY_CTRL)) == (SHIFTKEY_SHIFT | SHIFTKEY_CTRL))
		return DROP_LINK;
	if((modifiers & (SHIFTKEY_ALT | SHIFTKEY_SHIFT)) == (SHIFTKEY_ALT | SHIFTKEY_SHIFT))
		return DROP_COPY;
	else if ((modifiers & SHIFTKEY_CTRL) != 0) 
		return DROP_COPY; 
	else if ((modifiers & SHIFTKEY_SHIFT) != 0) 
		return DROP_MOVE; 
	else
		return DROP_LINK;
}

static BOOL IsInsideClientRect(int x, int y)
{
	WindowsOpWindow *window = WindowsOpWindow::FindWindowUnderCursor();
	if(!window)
		return FALSE;

	RECT client_rect;
	::GetClientRect(window->m_hwnd, &client_rect);
	POINT client_point;
	client_point.x = x;
	client_point.y = y;
	::ScreenToClient(window->m_hwnd, &client_point);

	return ::PtInRect(&client_rect, client_point);
}

WindowsDropTarget::WindowsDropTarget(HWND target_hwnd)
	: m_ref_count(0)
	, m_allow_drop(FALSE)
	, m_supported_format(NULL)
	, m_drop_target_helper(NULL)
	, m_drag_from_external_application(TRUE)
	, m_call_drag_enter(FALSE)
	, m_has_files(FALSE)
	, m_target_native_window(target_hwnd)
{
	HRESULT res = CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, IID_IDropTargetHelper, (LPVOID*) &m_drop_target_helper);
	if (res < 0)
		m_drop_target_helper = NULL;

	CF_HTML = ::RegisterClipboardFormatW(UNI_L("HTML Format"));
	CF_URILIST = ::RegisterClipboardFormatW(UNI_L("text/uri-list"));
	CF_MOZURL = ::RegisterClipboardFormatW(UNI_L("text/x-moz-url"));

	// We only send the following formats, we do not accept them: 
	CF_INETURLW = ::RegisterClipboardFormatW(CFSTR_INETURLW);
	CF_INETURLA = ::RegisterClipboardFormatW(CFSTR_INETURLA);
	CF_FILECONTENTS = ::RegisterClipboardFormatW(CFSTR_FILECONTENTS);
	CF_FILEDESCRIPTOR = ::RegisterClipboardFormatW(CFSTR_FILEDESCRIPTOR);

	AddSupportedFormat(CF_HTML, DVASPECT_CONTENT, TYMED_HGLOBAL);
	AddSupportedFormat(CF_UNICODETEXT, DVASPECT_CONTENT, TYMED_HGLOBAL);
	AddSupportedFormat(CF_TEXT, DVASPECT_CONTENT, TYMED_HGLOBAL);
	AddSupportedFormat(CF_HDROP, DVASPECT_CONTENT, TYMED_HGLOBAL); // File.  Could also use CFSTR_SHELLIDLIST.
	AddSupportedFormat(CF_URILIST, DVASPECT_CONTENT, TYMED_HGLOBAL);
	AddSupportedFormat(CF_MOZURL, DVASPECT_CONTENT, TYMED_HGLOBAL);
}

WindowsDropTarget::~WindowsDropTarget()
{
	if (m_drop_target_helper)
	{
		m_drop_target_helper->Release();
		m_drop_target_helper = NULL;
	}
	m_formatetc.DeleteAll();
}

OP_STATUS WindowsDropTarget::AddSupportedFormat(CLIPFORMAT cf_format, /*DVTARGETDEVICE *ptd,*/ DWORD dw_aspect, /*LONG lindex,*/ DWORD tymed)
{
	OpAutoPtr<FORMATETC> ftetc(OP_NEW(FORMATETC, ()));
	if (!ftetc.get())
		return OpStatus::ERR_NO_MEMORY;
	ftetc->ptd = 0;
	ftetc->cfFormat = cf_format;
	ftetc->dwAspect = dw_aspect;
	ftetc->lindex = -1;
	ftetc->tymed = tymed;
	return m_formatetc.Add(ftetc.release());
}


BOOL WindowsDropTarget::QueryDrop(DWORD keyboard_state, DWORD* effect)
{
	DWORD effects_allowed_by_source = *effect;

	if (!m_allow_drop)
	{
		*effect = DROPEFFECT_NONE;
		return FALSE;
	}
	if ((keyboard_state & (MK_CONTROL | MK_SHIFT)) == (MK_CONTROL | MK_SHIFT))
		*effect = DROPEFFECT_LINK;
	else if (keyboard_state & MK_CONTROL)
		*effect = DROPEFFECT_COPY;
	else if (keyboard_state & MK_SHIFT)
		*effect = DROPEFFECT_MOVE;
	else
	{
		// No modifier - get state specified by drag event.
		if (effects_allowed_by_source & DROPEFFECT_COPY)
			*effect = DROPEFFECT_COPY;
		else if (effects_allowed_by_source & DROPEFFECT_MOVE)
			*effect = DROPEFFECT_MOVE;
		else if (effects_allowed_by_source & DROPEFFECT_LINK)
			*effect = DROPEFFECT_LINK;
		else
			*effect = DROPEFFECT_NONE;
	}
	if (!(effects_allowed_by_source & *effect))
		*effect = DROPEFFECT_NONE;

	return (*effect == DROPEFFECT_NONE) ? FALSE : TRUE;
}

HRESULT WINAPI WindowsDropTarget::QueryInterface(const IID& riid, void** void_object)
{
	*void_object = NULL;
	if (IID_IUnknown == riid || IID_IDropTarget == riid)
		*void_object = this;

	if (*void_object)
	{
		((LPUNKNOWN)*void_object)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG WINAPI WindowsDropTarget::AddRef(void)
{
	return ++m_ref_count;
}

ULONG WINAPI WindowsDropTarget::Release(void)
{
	--m_ref_count;
	if (m_ref_count != 0)
		return m_ref_count;
	OP_DELETE(this);
	return 0;
}

HRESULT WINAPI WindowsDropTarget::DragEnter(IDataObject* drag_data, DWORD keyboard_state, POINTL lpt, DWORD* effect)
{
	if (!drag_data)
		return E_INVALIDARG;

	if (m_drop_target_helper)
	{
		POINT pt = { lpt.x, lpt.y };

		HWND hwnd = WindowFromPoint(pt);

		m_drop_target_helper->DragEnter(hwnd, drag_data, &pt, *effect);
	}
	DispatchDragOperation(m_target_native_window, WM_OPERA_DRAG_EXTERNAL_ENTER);

	m_supported_format = NULL;
	m_allow_drop = FALSE;
	for (UINT32 i = 0; i < m_formatetc.GetCount(); i++)
	{
		if (drag_data->QueryGetData(m_formatetc.Get(i)) == S_OK)
		{
			m_allow_drop = TRUE;
			m_supported_format = m_formatetc.Get(i);
			break;
		}
	}
	if (m_drag_from_external_application)
	{
		STGMEDIUM medium;

		if (OpStatus::IsError(FillWindowsDragDataFromDataObject(drag_data, medium)))
		{
			return E_OUTOFMEMORY;
		}
		RETURN_VALUE_IF_ERROR(g_windows_drag_manager->GetDragData().FillDragObjectFromData(g_drag_manager->GetDragObject()), E_OUTOFMEMORY);

		if (IsInsideClientRect(lpt.x, lpt.y))
			CallDragEnter();
		else
		{
			// Drag entered opera window, but it didn't reached client area yet.
			// Thus, memorize the data, which will be used when DragOver comes
			// over client area.
			m_call_drag_enter = TRUE;
		}
	}

	if (m_allow_drop && m_supported_format)
		QueryDrop(keyboard_state, effect);
	return S_OK;
}

void WindowsDropTarget::CallDragEnter()
{
	m_call_drag_enter = FALSE;
	DispatchDragOperation(m_target_native_window, WM_OPERA_DRAG_ENTER);
}
/* static */
void WindowsDropTarget::DispatchDragOperation(HWND target_window, UINT message)
{
	if(target_window)
	{
		SendMessage(target_window, message, 0, 0);
	}
}

HRESULT WINAPI WindowsDropTarget::DragOver(DWORD keyboard_state, POINTL lpt, DWORD* effect)
{
	if (m_drop_target_helper)
	{
		POINT pt = { lpt.x, lpt.y };
		m_drop_target_helper->DragOver(&pt, *effect);
	}
	// ask source what we're supposed to do with the drag data
	QueryDrop(keyboard_state, effect);

	DWORD source_effect = *effect;

	// Check if we're above the client area yet
	if (IsInsideClientRect(lpt.x, lpt.y))
	{
		if (m_drag_from_external_application && m_call_drag_enter)
			CallDragEnter(); // When over core UI or webpage.
		else
			DispatchDragOperation(m_target_native_window, WM_OPERA_DRAG_MOVED);
	}
	// check what we want to do with the drag data
	DesktopDragObject *drag_object = static_cast<DesktopDragObject *>(g_drag_manager->GetDragObject());
	OP_ASSERT(drag_object);
	if(drag_object)
	{
		switch (drag_object->GetVisualDropType())
		{
		default:
		case DROP_COPY:
			*effect = DROPEFFECT_COPY;
			break;
		case DROP_MOVE:
			*effect = DROPEFFECT_MOVE;
			break;
		case DROP_LINK:
			*effect = DROPEFFECT_LINK;
			break;
		case DROP_NONE:
			*effect = DROPEFFECT_NONE;
			break;
		}
	}
	// core didn't want the data, but source gave us a file to copy, so make sure we handle it
	if ((*effect == DROPEFFECT_NONE && source_effect == DROPEFFECT_COPY && m_has_files) || (*effect == DROPEFFECT_NONE && DragDrop_Data_Utils::HasURL(drag_object)))
		*effect = DROPEFFECT_COPY;
	return S_OK;
}

HRESULT WINAPI WindowsDropTarget::DragLeave()
{
	if (m_drop_target_helper)
		m_drop_target_helper->DragLeave();

	if (m_drag_from_external_application)
	{
		DispatchDragOperation(m_target_native_window, WM_OPERA_DRAG_CANCELLED);
	}
	return S_OK;
}

HRESULT WINAPI WindowsDropTarget::Drop(IDataObject* drag_data, DWORD keyboard_state, POINTL lpt, DWORD* effect)
{
	if (!drag_data)
		return E_INVALIDARG;
	if (m_drop_target_helper)
	{
		POINT pt = { lpt.x, lpt.y };
		m_drop_target_helper->Drop(drag_data, &pt, *effect);
	}
	if (m_allow_drop && m_supported_format && QueryDrop(keyboard_state, effect))
	{
		STGMEDIUM medium;

		if (OpStatus::IsError(FillWindowsDragDataFromDataObject(drag_data, medium)))
		{
			DispatchDragOperation(m_target_native_window, WM_OPERA_DRAG_CANCELLED);
			*effect = DROPEFFECT_NONE;
			return E_OUTOFMEMORY;
		}
		RETURN_VALUE_IF_ERROR(g_windows_drag_manager->GetDragData().FillDragObjectFromData(g_drag_manager->GetDragObject()), E_OUTOFMEMORY);

		if (IsInsideClientRect(lpt.x, lpt.y))
		{
			DispatchDragOperation(m_target_native_window, WM_OPERA_DRAG_DROP);
			*effect = DROPEFFECT_COPY;
		}
		else
		{
			DispatchDragOperation(m_target_native_window, WM_OPERA_DRAG_CANCELLED);
			*effect = DROPEFFECT_NONE;
		}
		g_windows_drag_manager->GetDragData().Clear();
	}
	m_allow_drop = FALSE;
	m_supported_format = NULL;

	return S_OK;
}

OP_STATUS WindowsDropTarget::FillWindowsDragDataFromDataObject(IDataObject* native_drag_data, STGMEDIUM& medium)
{
	WindowsDNDData& dnd_data = g_windows_drag_manager->GetDragData();

	WinAutoLock lock(&dnd_data.m_cs);

	if (!m_supported_format)
		return OpStatus::ERR;

	dnd_data.Clear();

	m_has_files = FALSE;
	/* A flag keeping track if text/unicode string is present in the input data.
	 * It's set when text/unicode string is encountered in the loop below.
	 * If it is TRUE text/plain string will not be taken if encountered later.
	 */
	BOOL has_unicode_text = FALSE;
	/* An index of text/plain string in the output data. It's set in the loop below when text/plain string
	 * is set in the output data. It's needed for replacing text/plain string in the output data if text/unicode string
	 * is found in the input data later (we prefer text/unicode string).
	 */
	int utf8_data_idx = -1;
	/* A flag keeping track if an uri-list is present in the input data. It's needed because the list
	 * may be provided as text/uri-list or text/x-moz-url or both. In the latter case text/x-moz-url is the
	 * same as text/uri-list and we don't want to have them both
	 * (they are present for better compatibility between applications).
	 */
	BOOL has_uri_list = FALSE;

	for(UINT32 i = 0; i < m_formatetc.GetCount(); i++)
	{
		if(SUCCEEDED(native_drag_data->QueryGetData(m_formatetc.Get(i))))
		{
			FORMATETC* i_format = m_formatetc.Get(i);
			if(i_format->cfFormat == CF_UNICODETEXT)
			{
				if(SUCCEEDED(native_drag_data->GetData(i_format, &medium)))
				{
					WindowsTypedBuffer *data = NULL;
					// Add unicode text as a new entry or replace current utf8 entry.
					unsigned idx = utf8_data_idx != -1 ? utf8_data_idx : dnd_data.m_data_array.GetCount();
					if (idx == (unsigned)utf8_data_idx)
					{
						WindowsTypedBuffer *data = dnd_data.m_data_array.Get(idx);
						op_free(const_cast<char*>(data->data));
					}
					else
					{
						data = OP_NEW(WindowsTypedBuffer, ());
						if(!data)
							goto oom;
					}
					WCHAR* uni_str = static_cast<WCHAR*>(GlobalLock(medium.hGlobal));
					OpAutoGlobalLock lock(medium.hGlobal);
					data->data = StringUtils::UniToUTF8(uni_str);
					if (!data->data)
						goto oom;

					// length = a number of characters got by op_strlen() + 1 because of the ending '\0'.
					data->length = op_strlen(data->data) + 1;
					if (idx != (unsigned)utf8_data_idx)
					{
						data->content_type = op_strdup("text/plain");
					}
					if (!data->content_type)
					{
						op_free(const_cast<char*>(data->data));
						data->data = NULL;
						goto oom;
					}
					if (idx != (unsigned)utf8_data_idx)
					{
						if(OpStatus::IsError(dnd_data.m_data_array.Add(data)))
							goto oom;
					}
					has_unicode_text = TRUE; // mark that unicode text is taken.
				}
			}
			else if (i_format->cfFormat == CF_HTML)
			{
				if(SUCCEEDED(native_drag_data->GetData(i_format, &medium)))
				{
					WindowsTypedBuffer *data = OP_NEW(WindowsTypedBuffer, ());
					if(!data)
						goto oom;

					char* html_str = WindowsClipboardUtil::GetHtmlString(medium.hGlobal);
					data->data = html_str;
					if (!data->data)
						goto oom;
					// length = a number of characters got by op_strlen() + 1 because of the ending '\0'.
					data->length = op_strlen(html_str) + 1;
					data->content_type = op_strdup("text/html");
					if (!data->content_type)
					{
						op_free(const_cast<char*>(data->data));
						data->data = NULL;
						goto oom;
					}
					if(OpStatus::IsError(dnd_data.m_data_array.Add(data)))
						goto oom;
				}
			}
			// Take utf8 text only if utf16 hasn't been taken yet.
			else if(i_format->cfFormat == CF_TEXT && !has_unicode_text)
			{
				if(SUCCEEDED(native_drag_data->GetData(i_format, &medium)))
				{
					WindowsTypedBuffer *data = OP_NEW(WindowsTypedBuffer, ());
					if(!data)
						goto oom;

					char* str = static_cast<char*>(GlobalLock(medium.hGlobal));
					OpAutoGlobalLock lock(medium.hGlobal);
					data->data = op_strdup(str);
					if (!data->data)
						goto oom;

					// length = a number of characters got by op_strlen() + 1 because of the ending '\0'.
					data->length = op_strlen(str) + 1;
					data->content_type = op_strdup("text/plain");
					if (!data->content_type)
					{
						op_free(const_cast<char*>(data->data));
						data->data = NULL;
						goto oom;
					}
					// Remember utf8 text index to replace it with utf16 if there is one.
					utf8_data_idx = dnd_data.m_data_array.GetCount();

					if(OpStatus::IsError(dnd_data.m_data_array.Add(data)))
						goto oom;
				}
			}
			else if(i_format->cfFormat == CF_HDROP)
			{
				if(SUCCEEDED(native_drag_data->GetData(i_format, &medium)))
				{
					HDROP files = static_cast<HDROP>(medium.hGlobal);
					UINT number_of_files = ::DragQueryFile(files, (UINT)-1, NULL, 0);
					TCHAR filename[MAX_PATH];
					OpString file_str;
					for (UINT i_files = 0; i_files < number_of_files; i_files++)
					{
						if (::DragQueryFile(files, i_files, filename, MAX_PATH))
						{
							if (!file_str.IsEmpty())
							{
								if (OpStatus::IsError(file_str.Append("\r\n")))
									goto oom;
							}
							if (OpStatus::IsError(file_str.Append(filename)))
								goto oom;
						}
					}
					WindowsTypedBuffer *data = OP_NEW(WindowsTypedBuffer, ());
					if(!data)
						goto oom;

					data->data = StringUtils::UniToUTF8(file_str.CStr());
					if (!data->data)
						goto oom;
					// length = a number of characters got by op_strlen() + 1 because of the ending '\0'.
					data->length = op_strlen(data->data) + 1;
					data->content_type = op_strdup("text/x-opera-files");
					if (!data->content_type)
					{
						op_free(const_cast<char*>(data->data));
						data->data = NULL;
						goto oom;
					}
					if(OpStatus::IsError(dnd_data.m_data_array.Add(data)))
						goto oom;

					m_has_files = TRUE;
				}
			}
			else if ((i_format->cfFormat == CF_URILIST || i_format->cfFormat == CF_MOZURL) && !has_uri_list)
			{
				if (native_drag_data->GetData(i_format, &medium) == S_OK)
				{
					WCHAR* uni_str = static_cast<WCHAR*>(GlobalLock(medium.hGlobal));

					WindowsTypedBuffer *data = OP_NEW(WindowsTypedBuffer, ());
					if(!data)
						goto oom;

					data->data = StringUtils::UniToUTF8(uni_str);
					if (!data->data)
						goto oom;
					// length = a number of characters got by op_strlen() + 1 because of the ending '\0'.
					data->length = op_strlen(data->data) + 1;
					data->content_type = op_strdup("text/uri-list");
					if (!data->content_type)
					{
						op_free(const_cast<char*>(data->data));
						data->data = NULL;
						goto oom;
					}
					if(OpStatus::IsError(dnd_data.m_data_array.Add(data)))
						goto oom;

					has_uri_list = TRUE;
				}
			}
		}
	}
	return OpStatus::OK;
oom:

	dnd_data.Clear();
	return OpStatus::ERR_NO_MEMORY;
}

WindowsDropSource::WindowsDropSource()
	: m_ref_count(0)
{}

HRESULT WINAPI WindowsDropSource::QueryInterface(const IID& riid, void** void_object)
{
	if (riid != IID_IUnknown && riid != IID_IDropSource)
	{
		*void_object = NULL;
		return E_NOINTERFACE;
	}

	*void_object = this;
	((LPUNKNOWN)*void_object)->AddRef();
	return S_OK;
}

ULONG WINAPI WindowsDropSource::AddRef(void)
{
	return ++m_ref_count;
}
ULONG WINAPI WindowsDropSource::Release(void)
{
	--m_ref_count;
	if (m_ref_count != 0)
		return m_ref_count;
	delete this;
	return 0;
}

HRESULT WINAPI WindowsDropSource::QueryContinueDrag(BOOL escape_pressed, DWORD keyboard_state)
{
	BrowserDesktopWindow *window = g_application->GetActiveBrowserDesktopWindow();
	WindowsOpWindow *op_window = window ? static_cast<WindowsOpWindow *>(window->GetOpWindow()) : NULL;

	POINT screen_point;
	::GetCursorPos(&screen_point);

	// main thread might have aborted the thread, so check for that
	if(escape_pressed || !op_window)
		return DRAGDROP_S_CANCEL;

	if (!(keyboard_state & (MK_LBUTTON | MK_RBUTTON)))
	{
		if (IsInsideClientRect(screen_point.x, screen_point.y)) // We're both the source and the target.
			WindowsDropTarget::DispatchDragOperation(op_window->m_hwnd, WM_OPERA_DRAG_DROP);
		else
			WindowsDropTarget::DispatchDragOperation(op_window->m_hwnd, WM_OPERA_DRAG_ENDED);
		return DRAGDROP_S_DROP;
	}

	HWND wnd_foreground = ::GetForegroundWindow();
	if (op_window && op_window->m_hwnd == wnd_foreground)
	{
		// If d'n'd originating in opera comes back over opera window,
		// use core handling of d'n'd instead of system.
		HWND wnd_under_cursor = ::WindowFromPoint(screen_point);
		if (wnd_under_cursor == op_window->m_hwnd)
		{
			// Hovering over opera window decoration is treated as external drag.
			if (IsInsideClientRect(screen_point.x, screen_point.y))
				return DRAGDROP_S_CANCEL | 0x01000000L;
		}
	}

	// Set visual feedback in the same position relative to cursor, as inside
	// core window.
	WindowsDropTarget::DispatchDragOperation(op_window->m_hwnd, WM_OPERA_DRAG_UPDATE);

	return S_OK;
}

extern HINSTANCE hInst;
extern HCURSOR hLinkCursor, hDragCopyCursor, hDragMoveCursor, hDragBadCursor;

static DropType OSToOpera(DWORD& effect)
{
	effect &= ~DROPEFFECT_SCROLL;	// we don't need this one for our comparison
	if ((effect & DROPEFFECT_COPY) == DROPEFFECT_COPY)
		return DROP_COPY;
	else if ((effect & DROPEFFECT_LINK) == DROPEFFECT_LINK)
		return DROP_LINK;
	else if ((effect & DROPEFFECT_MOVE) == DROPEFFECT_MOVE)
		return DROP_MOVE;
	else 
		return DROP_NONE;
}

HRESULT WINAPI WindowsDropSource::GiveFeedback(DWORD effect)
{
	HCURSOR hcursor = NULL;
	DropType type = OSToOpera(effect);
	DWORD local_effect = effect;
	DesktopDragObject* drag_obj = static_cast<DesktopDragObject*>(g_drag_manager->GetDragObject());

	if (drag_obj && drag_obj->GetType() == OpTypedObject::DRAG_TYPE_WINDOW && local_effect == DROPEFFECT_NONE)
	{
		// If we think it's OK to move the tab and the target does not accept the drop - let's move the tab!
		WindowsOpWindow* win = WindowsOpWindow::FindWindowUnderCursor();
		if (!win)
			local_effect = DROPEFFECT_MOVE;
	}

	if ((local_effect & DROPEFFECT_COPY) == DROPEFFECT_COPY)
		hcursor = hDragCopyCursor;
	else if ((local_effect & DROPEFFECT_LINK) == DROPEFFECT_LINK)
		hcursor = hLinkCursor;
	else if ((local_effect & DROPEFFECT_MOVE) == DROPEFFECT_MOVE)
		hcursor = hDragMoveCursor;
	else
		hcursor = hDragBadCursor;

	if (drag_obj)
		drag_obj->SetDesktopDropType(type);

	::SetCursor(hcursor);

	return S_OK;
}

WindowsDataObject::WindowsDataObject(IDropSource* ids)
	: m_ref_count(0)
	, m_ids(ids)
{}

WindowsDataObject::~WindowsDataObject()
{
	m_fetcs.DeleteAll();

	for (UINT32 i = 0; i < m_mediums.GetCount(); i++)
	{
		STGMEDIUM *medium = m_mediums.Get(i);
		if(medium->hGlobal)
		{
			GlobalFree(medium->hGlobal);
		}
	}
	m_mediums.DeleteAll();
}

void WindowsDataObject::CopyMedium(STGMEDIUM* medium_destination, STGMEDIUM* medium_source, FORMATETC* fetc_source)
{
	switch (medium_source->tymed)
	{
	case TYMED_HGLOBAL:
		medium_destination->hGlobal = (HGLOBAL)OleDuplicateData(medium_source->hGlobal, fetc_source->cfFormat, NULL);
		break;
	case TYMED_NULL:
	default:
		medium_destination->hGlobal = NULL;
		break;
	}

	medium_destination->tymed = medium_source->tymed;
	if (medium_source->pUnkForRelease)
	{
		medium_destination->pUnkForRelease = medium_source->pUnkForRelease;
		medium_source->pUnkForRelease->AddRef();
	}
	else
		medium_destination->pUnkForRelease = NULL;
}

HRESULT WINAPI WindowsDataObject::QueryInterface(const IID& riid, void** void_object)
{
	*void_object = NULL;
	if (riid == IID_IUnknown || riid == IID_IDataObject)
		*void_object = this;
	if (*void_object)
	{
		((LPUNKNOWN)*void_object)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG WINAPI WindowsDataObject::AddRef()
{
	return ++m_ref_count;
}

ULONG WINAPI WindowsDataObject::Release()
{
	--m_ref_count;
	if (m_ref_count != 0)
		return m_ref_count;
	delete this;
	return 0;
}

HRESULT WINAPI WindowsDataObject::GetData(FORMATETC* fetc, STGMEDIUM* medium)
{
	if (!fetc || !medium)
		return E_INVALIDARG;
	medium->hGlobal = NULL;

	for (UINT32 i = 0; i < m_fetcs.GetCount(); i++)
	{
		if ((fetc->tymed & m_fetcs.Get(i)->tymed)
			&& fetc->dwAspect == m_fetcs.Get(i)->dwAspect
			&& fetc->cfFormat == m_fetcs.Get(i)->cfFormat)
		{
			CopyMedium(medium, m_mediums.Get(i), m_fetcs.Get(i));
			return S_OK;
		}
	}
	return DV_E_FORMATETC;
}

HRESULT WINAPI WindowsDataObject::GetDataHere(FORMATETC* fetc, STGMEDIUM* medium)
{
	return E_NOTIMPL;
}

HRESULT WINAPI WindowsDataObject::QueryGetData(FORMATETC* formatetc)
{
	if (!formatetc)
		return E_INVALIDARG;

	if (!(formatetc->dwAspect & DVASPECT_CONTENT))
		// We are not interested in DVASPECT_THUMBNAIL, DVASPECT_ICON nor
		// DVASPECT_DOCPRINT.
		return DV_E_DVASPECT;
	HRESULT ret = DV_E_TYMED;
	for (UINT32 i = 0; i < m_fetcs.GetCount(); i++)
	{
		if (formatetc->tymed & m_fetcs.Get(i)->tymed)
		{
			if(formatetc->cfFormat == m_fetcs.Get(i)->cfFormat)
				return S_OK;
			else
				ret = DV_E_CLIPFORMAT;
		}
		else
			ret = DV_E_TYMED;
	}
	return ret;
}

HRESULT WINAPI WindowsDataObject::GetCanonicalFormatEtc(FORMATETC* fetc_in, FORMATETC* fetc_out)
{
	if (!fetc_out)
		return E_INVALIDARG;

	*fetc_out = *fetc_in;
	fetc_out->ptd = NULL;

	return DATA_S_SAMEFORMATETC;
}

OP_STATUS WindowsDataObject::CommitFileData()
{
	if(m_drop_files.GetCount())
	{
		// we got files we need to expose as CF_HDROP
		size_t bufsize = 0;

		// because of the special way DROPFILES is filled out, we need one pass first to count 
		// the total size needed for all structures and strings
		for(UINT32 x = 0; x < m_drop_files.GetCount(); x++)
		{
			OpString *file = m_drop_files.Get(x);
			bufsize += file->Length() + 1;
		}
		// add one extra for the final NUL char and the size of the DROPFILES structure, and convert
		// to the size to hold unicode
		bufsize = sizeof(DROPFILES) + sizeof(TCHAR) * (bufsize + 1);

		OpAutoGlobalAlloc hglobal(GlobalAlloc(GHND | GMEM_SHARE, bufsize));
		if(!hglobal.get())
			return OpStatus::ERR_NO_MEMORY;

		OpAutoGlobalLock tmp_drop_files(::GlobalLock(hglobal));
		if(!tmp_drop_files.get())
			return OpStatus::ERR_NO_MEMORY;

		DROPFILES *dropfiles = static_cast<DROPFILES *>(tmp_drop_files.get());
		dropfiles->pFiles = sizeof(DROPFILES);	// offset to the file list
		dropfiles->fWide = TRUE;				// all file names are in unicode

		// set the start of the filename array
		TCHAR *buffer = reinterpret_cast<TCHAR *>(LPBYTE(dropfiles) + sizeof(DROPFILES));
		for(UINT32 x = 0; x < m_drop_files.GetCount(); x++)
		{
			OpString *file = m_drop_files.Get(x);
			uni_strcpy(buffer, file->CStr());

			// skip past the NUL terminator
			buffer = buffer + file->Length() + 1;
		}
		tmp_drop_files.reset();

		m_drop_files.DeleteAll();

		FORMATETC fetc = { };

		fetc.cfFormat = CF_HDROP;
		fetc.ptd = 0;
		fetc.dwAspect = DVASPECT_CONTENT;
		fetc.lindex = -1;
		fetc.tymed = TYMED_HGLOBAL;
		STGMEDIUM medium = { };
		medium.tymed = TYMED_HGLOBAL;

		medium.hGlobal = hglobal.get();
		HRESULT res = SetData(&fetc, &medium, FALSE);
		if(FAILED(res))
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

BOOL WindowsDataObject::SetData(OpDragObject *drag_object, const char* type, const char* contents, unsigned int len)
{
	BOOL retval = TRUE;
	char* str = NULL;
	FORMATETC fetc = { };
	OpVector<OpString> urls;
	bool is_image = drag_object->GetType() == OpTypedObject::DRAG_TYPE_IMAGE;

	if (op_strcmp(type, ("text/plain")) == 0)
	{
		fetc.cfFormat = CF_UNICODETEXT;
	}
	else if (op_strcmp(type, ("text/unicode")) == 0)
	{
		fetc.cfFormat = CF_UNICODETEXT;
	}
	else if (op_strcmp(type, ("text/uri-list")) == 0)
	{
		if(OpStatus::IsSuccess(DragDrop_Data_Utils::GetURLs(drag_object, urls)))
		{
			fetc.cfFormat = CF_URILIST;
		}
	}
	else if (op_strcmp(type, ("text/x-moz-url")) == 0)
	{
		fetc.cfFormat = CF_MOZURL;
	}
	else if (op_strcmp(type, ("text/html")) == 0)
	{
		fetc.cfFormat = CF_HTML;
		str = WindowsClipboardUtil::HtmlStringToClipboardFormat(contents, m_origin_url.CStr(), len);
	}
	else
		fetc.cfFormat = ::RegisterClipboardFormatA(type);

	if(is_image && fetc.cfFormat == CF_UNICODETEXT)
	{
		// silently ignore CF_UNICODETEXT if it's an image
		return TRUE;
	}
	if (fetc.cfFormat == CF_MOZURL || fetc.cfFormat == CF_UNICODETEXT)
	{
		// The data we got from CORE is UTF8 so convert it to UTF16.
		if (str = static_cast<char*>(::GlobalAlloc(GMEM_ZEROINIT, (len+1) * sizeof(uni_char))))
			MultiByteToWideChar(CP_UTF8, 0, contents, len, reinterpret_cast<LPWSTR>(str), len);
		else
			return FALSE;
	}

	fetc.ptd = 0;
	fetc.dwAspect = DVASPECT_CONTENT;
	fetc.lindex = -1;
	fetc.tymed = TYMED_HGLOBAL;
	STGMEDIUM medium = { };
	medium.tymed = TYMED_HGLOBAL;

	if (!str)
	{
		if (str = static_cast<char*>(::GlobalAlloc(GMEM_ZEROINIT, len+1)))
			op_strcpy(str, contents);
		else
			return FALSE;
	}
	medium.hGlobal = str;
	HRESULT res = SetData(&fetc, &medium, FALSE);
	if(FAILED(res))
		retval = FALSE;

	if(retval && fetc.cfFormat == CF_URILIST)
	{
		// for this type, we also set it as CF_UNICODETEXT and various other formats

		if(!is_image)
		{
			// don't create text/unicode if it's an image
			retval = SetData(drag_object, "text/unicode", contents, len);
		}
		// unless it's a Window (tab basically), create the windows types too
		if(retval)
		{
			bool is_window = drag_object->GetType() == OpTypedObject::DRAG_TYPE_WINDOW;
			bool is_file = false;
			// check if we're a file in which case we'll expose them as CF_HDROP instead
			OpFile file;

			if(is_image)
			{
				do
				{
					CopyImageFileToLocationData data;

					data.file_url = contents;

					BrowserDesktopWindow* bw = g_application->GetActiveBrowserDesktopWindow();
					WindowsOpWindow* window = static_cast<WindowsOpWindow*>(bw->GetOpWindow());
					OP_ASSERT(window);
					if(!window)
					{
						window = WindowsOpWindow::FindWindowUnderCursor();
					}
					if(window && window->m_hwnd)
					{
						// access some core stuff like URL from the main thread
						if(SendMessage(window->m_hwnd, WM_OPERA_DRAG_COPY_FILE_TO_LOCATION, (WPARAM)&data, 0))
						{
							if(!data.image.IsEmpty())
							{
								// create the CF_DIB format, but ignore any error
								SetClipboardDIB(drag_object, data.image);
							}
							else
							{
								is_file = true;
							}
							// we can now let it be handled as a regular CF_HDROP
							OpString *filename = OP_NEW(OpString, ());
							if(filename)
							{
								filename->TakeOver(data.destination);

								if(OpStatus::IsError(m_drop_files.Add(filename)))
									break;
							}
						}
					}
				} while (false);
			}
			else if(!is_window)
			{
				for(UINT32 x = 0; x < urls.GetCount(); x++)
				{
					OpString* url = urls.Get(x);

					if(OpStatus::IsSuccess(file.Construct(url->CStr())))
					{
						BOOL exists = FALSE;

						if(OpStatus::IsSuccess(file.Exists(exists)) && exists)
						{
							// file exists, we'll deal with it later when we know we have all the files
							OpString *filename = OP_NEW(OpString, ());
							if(filename)
							{
								filename->TakeOver(*url);

								if(OpStatus::IsError(m_drop_files.Add(filename)))
									retval = FALSE;

								is_file = true;
							}
						}
					}
				}
			}
			// if we have files, it's important to place them before the HTML due to XP's active desktop
			// otherwise asking to create an active desktop item
			RETURN_VALUE_IF_ERROR(CommitFileData(), FALSE);

			// if it's a window or a file, don't expose more drag types
			if(!is_window && !is_file)
			{
				if(!is_image)
				{
					// create the internet shortcut suitable for the desktop etc.
					retval = SetClipboardVirtualFileData(drag_object, contents, len);
				}
				if(retval)
				{
					// not a file, expose all the formats needed
					retval = SetClipboardFormatData(CF_INETURLW, contents, len, true);
					if(retval)
					{
						retval = SetClipboardFormatData(CF_INETURLA, contents, len, false);
					}
				}
			}
		}
	}
	urls.DeleteAll();

	// SetData will copy the data, so free our copy now
	GlobalFree(medium.hGlobal);

	return retval;
}

BOOL WindowsDataObject::SetClipboardDIB(OpDragObject *drag_object, Image image)
{
	BOOL retval = FALSE;
	OpBitmap *bitmap = image.GetBitmap(NULL);
	if(bitmap)
	{
		UINT32 width = bitmap->Width();
		UINT32 height = bitmap->Height();
		UINT8 bpp = 32;  // always get bitmap data in 32 bit

		UINT32 header_size = sizeof(BITMAPINFOHEADER);
		BITMAPINFO bmpi;
		bmpi.bmiHeader.biSize = header_size;
		bmpi.bmiHeader.biWidth = width;
		bmpi.bmiHeader.biHeight = height;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = bpp;
		bmpi.bmiHeader.biCompression = BI_RGB;
		bmpi.bmiHeader.biSizeImage = 0;
		bmpi.bmiHeader.biXPelsPerMeter = 0;
		bmpi.bmiHeader.biYPelsPerMeter = 0;
		bmpi.bmiHeader.biClrUsed = 0;
		bmpi.bmiHeader.biClrImportant = 0;

		UINT32 bytes_per_line = width * (bpp >> 3);
		UINT32 len = bytes_per_line * height + header_size;
		FORMATETC fetc = { };

		fetc.cfFormat = CF_DIB;
		fetc.ptd = 0;
		fetc.dwAspect = DVASPECT_CONTENT;
		fetc.lindex = -1;
		fetc.tymed = TYMED_HGLOBAL;

		STGMEDIUM medium = { };
		medium.tymed = TYMED_HGLOBAL;

		// The data we got from CORE is UTF8 so convert it to UTF16.
		OpAutoGlobalAlloc hglobal(GlobalAlloc(GHND | GMEM_SHARE, len));
		if(!hglobal.get())
			return FALSE;

		medium.hGlobal = hglobal.get();

		LPSTR buffer = (LPSTR) GlobalLock(medium.hGlobal);

		memcpy(buffer, &bmpi, header_size);

		// write all bitmap data into the clipboard bitmap
		for (UINT32 line = 0; line < height; line++)
		{
			UINT8* dest = (UINT8*) &buffer[line * bytes_per_line + header_size];
			bitmap->GetLineData(dest, height - line - 1);
		}
		HRESULT res = SetData(&fetc, &medium, FALSE);
		if(FAILED(res))
			retval = FALSE;

		GlobalUnlock(medium.hGlobal);

		image.ReleaseBitmap();

		retval = TRUE;
	}
	return retval;
}

BOOL WindowsDataObject::SetClipboardVirtualFileData(OpDragObject *drag_object, const char* contents, unsigned int len)
{
	BOOL retval = FALSE;
	OpString8 file_contents;

	if(OpStatus::IsSuccess(file_contents.AppendFormat("[InternetShortcut]\r\nURL=%s\r\n", contents)))
	{
		// set the contents of our virtual file first
		retval = SetClipboardFormatData(CF_FILECONTENTS, file_contents.CStr(), file_contents.Length(), false);

		FILEGROUPDESCRIPTOR fgd = { };
		FORMATETC fetc = { };

		fetc.cfFormat = CF_FILEDESCRIPTOR;
		fetc.ptd = 0;
		fetc.dwAspect = DVASPECT_CONTENT;
		fetc.lindex = 0;
		fetc.tymed = TYMED_HGLOBAL;

		STGMEDIUM medium = { };
		medium.tymed = TYMED_HGLOBAL;

		FILETIME filetime;

		// fake the modified time as being this moment for our virtual file
		GetSystemTimeAsFileTime(&filetime);

		fgd.cItems = 1;	// we only have a single item

		fgd.fgd[0].dwFlags = FD_FILESIZE | FD_WRITESTIME;
		fgd.fgd[0].nFileSizeLow = file_contents.Length();
		fgd.fgd[0].ftLastWriteTime.dwLowDateTime = filetime.dwLowDateTime;
		fgd.fgd[0].ftLastWriteTime.dwHighDateTime = filetime.dwHighDateTime;

		OP_STATUS s = OpStatus::OK;
		OpString filename;

		// create a suitable filename. Shortcuts needs to be named with .url as extension
		const uni_char *descr = DragDrop_Data_Utils::GetDescription(drag_object);
		if(descr == NULL)
		{
			BrowserDesktopWindow* bw = g_application->GetActiveBrowserDesktopWindow();
			WindowsOpWindow* window = static_cast<WindowsOpWindow*>(bw->GetOpWindow());
			OP_ASSERT(window);
			if(!window)
			{
				window = WindowsOpWindow::FindWindowUnderCursor();
			}
			if(window && window->m_hwnd)
			{
				DNDGetSuggestedFilenameData data;

				data.file_url = contents;

				// access some core stuff like URL from the main thread
				SendMessage(window->m_hwnd, WM_OPERA_DRAG_GET_SUGGESTED_FILENAME, (WPARAM)&data, 0);

				s = filename.TakeOver(data.destination);
			}
		}
		else
		{
			OpString file_name;

			s = file_name.Set(descr);
			if(OpStatus::IsSuccess(s))
			{
				// strip illegal characters
				if(!g_desktop_op_system_info->RemoveIllegalFilenameCharacters(file_name, TRUE))
					s = OpStatus::ERR;
				else
					s = filename.AppendFormat("%s.url", file_name.CStr());
			}
		}
		if(OpStatus::IsSuccess(s))
		{
			uni_strncpy(fgd.fgd[0].cFileName, filename.CStr(), ARRAY_SIZE(fgd.fgd[0].cFileName));

			medium.hGlobal = ::GlobalAlloc(GMEM_ZEROINIT, sizeof(fgd));
			if(medium.hGlobal)
			{
				op_memcpy(medium.hGlobal, &fgd, sizeof(fgd));
				HRESULT res = SetData(&fetc, &medium, FALSE);
				if(SUCCEEDED(res))
					retval = TRUE;
			}
		}
	}
	return retval;
}

BOOL WindowsDataObject::SetClipboardFormatData(CLIPFORMAT cf_type, const char* contents, unsigned int len, bool convert_to_unicode)
{
	BOOL retval = TRUE;
	char* str = NULL;
	FORMATETC fetc = { };

	fetc.cfFormat = cf_type;

	fetc.ptd = 0;
	fetc.dwAspect = DVASPECT_CONTENT;
	fetc.lindex = cf_type == CF_FILECONTENTS ? 0 : -1;
	fetc.tymed = TYMED_HGLOBAL;
	STGMEDIUM medium = { };
	medium.tymed = TYMED_HGLOBAL;

	if(convert_to_unicode)
	{
		// The data we got from CORE is UTF8 so convert it to UTF16.
		if (str = static_cast<char*>(::GlobalAlloc(GMEM_ZEROINIT, (len+1) * sizeof(uni_char))))
			MultiByteToWideChar(CP_UTF8, 0, contents, len, reinterpret_cast<LPWSTR>(str), len);
		else
			return FALSE;
	}
	else
	{
		if (str = static_cast<char*>(::GlobalAlloc(GMEM_ZEROINIT, len+1)))
			op_strcpy(str, contents);
		else
			return FALSE;
	}
	medium.hGlobal = str;
	HRESULT res = SetData(&fetc, &medium, FALSE);
	if(FAILED(res))
		retval = FALSE;

	// SetData will copy the data, so free our copy now
	GlobalFree(medium.hGlobal);

	return retval;
}

HRESULT WINAPI WindowsDataObject::SetData(FORMATETC* fetc_out, STGMEDIUM* medium_in, BOOL release)
{
	if (!fetc_out || !medium_in)
		return E_INVALIDARG;

	OpAutoPtr<FORMATETC> fetc_local(OP_NEW(FORMATETC, ()));
	OpAutoPtr<STGMEDIUM> medium_local(OP_NEW(STGMEDIUM, ()));
	if (!fetc_local.get() || !medium_local.get())
	{
		return E_OUTOFMEMORY;
	}
	op_memset(fetc_local.get(), 0, sizeof(FORMATETC));
	op_memset(medium_local.get(), 0, sizeof(STGMEDIUM));

	*(fetc_local.get()) = *fetc_out;
	if (OpStatus::IsError(m_fetcs.Add(fetc_local.get())))
	{
		return E_OUTOFMEMORY;
	}
	if (release)
		*(medium_local.get()) = *medium_in;
	else
		CopyMedium(medium_local.get(), medium_in, fetc_out);

	if (OpStatus::IsError(m_mediums.Add(medium_local.get())))
	{
		m_fetcs.RemoveByItem(fetc_local.get());
		if(medium_local.get())
		{
			GlobalFree(medium_local->hGlobal);
		}
		return E_OUTOFMEMORY;
	}
	fetc_local.release();
	medium_local.release();
	return S_OK;
}

HRESULT WINAPI WindowsDataObject::EnumFormatEtc(DWORD direction, IEnumFORMATETC** enum_fetc)
{
	if (!enum_fetc)
		return E_POINTER;

	*enum_fetc = NULL;
	switch (direction)
	{
	case DATADIR_GET:
		*enum_fetc = OP_NEW(WindowsEnumFormatEtc, (m_fetcs));
		if (!*enum_fetc)
			return E_OUTOFMEMORY;
		(*enum_fetc)->AddRef();
		break;

	case DATADIR_SET:
	default:
		return E_NOTIMPL;
		break;
	}

	return S_OK;
}

HRESULT WINAPI WindowsDataObject::DAdvise(FORMATETC* ,DWORD , IAdviseSink*, DWORD *)
{
	return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT WINAPI WindowsDataObject::DUnadvise(DWORD)
{
	return E_NOTIMPL;
}

HRESULT WINAPI WindowsDataObject::EnumDAdvise(IEnumSTATDATA**)
{
	return OLE_E_ADVISENOTSUPPORTED;
}

WindowsEnumFormatEtc::WindowsEnumFormatEtc(const OpVector<FORMATETC>& fetcs)
	: m_ref_count(0)
	, m_current(0)
{
	for (UINT32 i = 0; i < fetcs.GetCount(); i++)
		m_fetcs.Add(fetcs.Get(i));
}

HRESULT WINAPI WindowsEnumFormatEtc::QueryInterface(REFIID refiid, void** void_object)
{
	*void_object = NULL;
	if (refiid == IID_IUnknown || refiid == IID_IEnumFORMATETC)
		*void_object = this;

	if (*void_object)
	{
		((LPUNKNOWN)*void_object)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG WINAPI WindowsEnumFormatEtc::AddRef()
{
	return ++m_ref_count;
}

ULONG WINAPI WindowsEnumFormatEtc::Release()
{
	--m_ref_count;
	if (m_ref_count != 0)
		return m_ref_count;
	delete this;
	return 0;
}

HRESULT WINAPI WindowsEnumFormatEtc::Next(ULONG no_of_elements, FORMATETC* fetc, ULONG* no_of_elements_fetched)
{
	if (no_of_elements_fetched)
		*no_of_elements_fetched = 0;

	ULONG left_to_fetch = no_of_elements;

	if (no_of_elements <= 0 || !fetc || m_current >= m_fetcs.GetCount())
		return S_FALSE;

	if (!no_of_elements_fetched && no_of_elements != 1)
		return S_FALSE;

	while (m_current < m_fetcs.GetCount() && left_to_fetch > 0)
	{
		*fetc++ = *m_fetcs.Get(m_current++);
		left_to_fetch--;
	}
	if (no_of_elements_fetched)
		*no_of_elements_fetched = no_of_elements - left_to_fetch;

	return (left_to_fetch == 0) ? S_OK : S_FALSE;
}

HRESULT WINAPI WindowsEnumFormatEtc::Skip(ULONG no_of_elements)
{
	if ((m_current + static_cast<unsigned int>(no_of_elements)) >= m_fetcs.GetCount())
		return S_FALSE;
	m_current += no_of_elements;
	return S_OK;
}

HRESULT WINAPI WindowsEnumFormatEtc::Reset()
{
	m_current = 0;
	return S_OK;
}

HRESULT WINAPI WindowsEnumFormatEtc::Clone(IEnumFORMATETC** clone_enum_fetc)
{
	if (!clone_enum_fetc)
		return E_POINTER;

	WindowsEnumFormatEtc *new_enum = OP_NEW(WindowsEnumFormatEtc, (m_fetcs));
	if (!new_enum)
		return E_OUTOFMEMORY;
	new_enum->AddRef();
	new_enum->m_current = m_current;
	*clone_enum_fetc = new_enum;
	return S_OK;
}

/* static */
char* WindowsClipboardUtil::HtmlStringToClipboardFormat(const char* str, const char* page_url, unsigned int len)
{
	if(!page_url)
		page_url = "http://localhost/";

#define HEADER_LENGTH_BEFORE_URL 97
#define HEADER_LENGTH_URL 12
#define HEADER_LENGTH_WITHOUT_URL 221
#define ENDING_LENGTH 36
	/* The start of the html text is at 97 characters + url line,
	the start of the actual text to be put on the clipboard is
	at 221 + url line.  If you change the any of the text below, you
	must make sure that the numbers continue to be correct. */
	int start_html, start_fragment, end_html, end_fragment;

	int url_line_length = HEADER_LENGTH_URL + op_strlen(page_url);
	start_html = HEADER_LENGTH_BEFORE_URL + url_line_length;
	start_fragment = HEADER_LENGTH_WITHOUT_URL + url_line_length;
	end_html = HEADER_LENGTH_WITHOUT_URL + url_line_length + len + ENDING_LENGTH;
	end_fragment = start_fragment + len;

	char* out = static_cast<char*>(::GlobalAlloc(GMEM_ZEROINIT, end_html + 1));
	if (!out)
		return NULL;
	sprintf(out,
		"Version:1.0\r\n"                  // 13
		"StartHTML:%08d\r\n"               // 20
		"EndHTML:%08d\r\n"                 // 18
		"StartFragment:%08d\r\n"           // 24
		"EndFragment:%08d\r\n"             // 22 = 97 = HEADER_LENGTH_BEFORE_URL
		"SourceURL:%s\r\n"                 // 12 + page_url = HEADER_LENGTH_URL + page_url
		"<html>\r\n"                       //  8
			"<head>\r\n"                   //  8
				"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\r\n" // 71
			"</head>\r\n"                  //  9
			"<body>\r\n"                   //  8
		"<!--StartFragment-->",            // 20
		start_html, end_html, start_fragment, end_fragment, page_url);
	OP_ASSERT(strlen(out) == (size_t)start_fragment);
	sprintf(out + strlen(out), "%s", str);
	sprintf(out + strlen(out), "<!--EndFragment-->\r\n</body>\r\n</html>"); // 36

	return out;
}

/* static */
char* WindowsClipboardUtil::GetHtmlString(HGLOBAL in)
{
	char* html_str = static_cast<char*>(GlobalLock(in));
	OpAutoGlobalLock lock(in);
	OpString text;
	if (OpStatus::IsError((text.Set(html_str))))
	{
		return NULL;
	}
	
	int length = op_strlen("<!--StartFragment-->");
	int pos = text.Find(UNI_L("<!--StartFragment-->"));
	if (pos != KNotFound)
		text.Delete(0, pos + length);
	pos = text.Find(UNI_L("<!--EndFragment-->"));
	if (pos != KNotFound)
		text.Delete(pos);

	return StringUtils::UniToUTF8(text.CStr());
}
/*
* Support methods for the dnd drag data manipulation
*/
OP_STATUS WindowsDNDData::InitializeFromDragObject(OpDragObject *drag_object)
{
	if(!drag_object)
		return OpStatus::OK;	// silently ignore this condition

	OP_STATUS status = OpStatus::OK;

	WinAutoLock lock(&m_cs);

	m_effects_allowed = drag_object->GetEffectsAllowed();

	if (!drag_object->IsInProtectedMode())
	{
		OpDragDataIterator& iter = drag_object->GetDataIterator();
		if (!iter.First())
			return OpStatus::OK;	// iterator has no data
		OpString file_data;

		do
		{
			if (iter.IsStringData())
			{
				WindowsTypedBuffer* new_data_buffer = OP_NEW(WindowsTypedBuffer, ());
				if(!new_data_buffer)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				const uni_char* data_format = iter.GetMimeType();
				const char* mime_type = StringUtils::UniToUTF8(data_format);
				new_data_buffer->content_type = mime_type ? mime_type : "";

				const uni_char* str_data = iter.GetStringData();
				new_data_buffer->data = StringUtils::UniToUTF8(str_data);
				new_data_buffer->length = new_data_buffer->data ? op_strlen(new_data_buffer->data) + 1 : 0;

				RETURN_IF_ERROR(m_data_array.Add(new_data_buffer));
			}
			else if (iter.IsFileData())
			{
				const OpFile* file = iter.GetFileData();
				const uni_char* filename = file->GetFullPath();
				RETURN_IF_ERROR(file_data.AppendFormat(UNI_L("%s\r\n"), filename));
			}
		} while (iter.Next());

		if (!file_data.IsEmpty())
		{
			WindowsTypedBuffer* new_data_buffer = OP_NEW(WindowsTypedBuffer, ());
			if(!new_data_buffer)
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				new_data_buffer->data = StringUtils::UniToUTF8(file_data.CStr());
				new_data_buffer->length = new_data_buffer->data ? op_strlen(new_data_buffer->data) + 1 : 0;

				const char* mime_type = op_strdup("text/x-opera-files");
				new_data_buffer->content_type = mime_type ? mime_type : "";

				RETURN_IF_ERROR(m_data_array.Add(new_data_buffer));
			}
		}
	}
	return status;
}

OP_STATUS WindowsDNDData::FillDragObjectFromData(OpDragObject *drag_object)
{
	if (!drag_object)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	WinAutoLock lock(&m_cs);

	if (drag_object->GetType() != OpTypedObject::UNKNOWN_TYPE || drag_object->IsReadOnly())
		return OpStatus::ERR;

	drag_object->SetEffectsAllowed(m_effects_allowed);
	drag_object->ClearData();
	for (unsigned int i = 0; i < m_data_array.GetCount(); i++)
	{
		OpString data_str;
		OpString type_str;
		WindowsTypedBuffer *buffer = m_data_array.Get(i);

		if (OpStatus::IsError(data_str.SetFromUTF8(buffer->data, buffer->length)))
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError(type_str.SetFromUTF8(buffer->content_type)))
			return OpStatus::ERR_NO_MEMORY;

		if (op_stricmp(buffer->content_type, "text/x-opera-files") == 0)
		{
			int filenames_str_len = data_str.Length();
			int devoured = 0;
			while (devoured < filenames_str_len)
			{
				int filename_end = data_str.Find("\r\n", devoured);
				if (filename_end > 0)
				{
					if (OpStatus::IsError(drag_object->SetData(data_str.SubString(devoured, filename_end-devoured).CStr(), type_str.CStr(), TRUE, FALSE)))
						return OpStatus::ERR_NO_MEMORY;

					devoured = filename_end + 2;
				}
				else
				{
					if (OpStatus::IsError(drag_object->SetData(data_str.SubString(devoured).CStr(), type_str.CStr(), TRUE, FALSE)))
						return OpStatus::ERR_NO_MEMORY;

					devoured = filenames_str_len;
				}
			}
		}
		else
		{
			if (OpStatus::IsError(drag_object->SetData(data_str.CStr(), type_str.CStr(), FALSE, TRUE)))
				return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::OK;
}
