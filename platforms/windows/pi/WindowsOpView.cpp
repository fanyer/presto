/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#include "WindowsOpView.h"
#include "adjunct/desktop_pi/DesktopOpView.h"
#include "platforms/windows/pi/WindowsOpWindow.h"

WindowsOpView* WindowsOpView::s_safe_view_pointer = NULL;

#ifdef WIDGETS_IME_SUPPORT
OpInputMethodString imstring;
IM_WIDGETINFO widgetinfo;
BOOL compose_started = FALSE;
BOOL compose_started_by_message = FALSE;
BOOL result_was_commited = FALSE;

// Must update global caret position for some IMEs.
HKL current_input_language = 0;

HWND WindowsOpView::s_global_caret_hwnd = NULL;
int WindowsOpView::s_global_caret_height = 0;
int WindowsOpView::s_global_caret_width = 0;

INT32 UpdateIMString(HIMC hIMC, UINT32 flag)
{
	INT32 len = ImmGetCompositionString(hIMC, flag, NULL, 0) / sizeof(uni_char);
	if (len > 0)
	{
		uni_char* str = new uni_char[len + 1];
		if (str == NULL)
			 return 0;

		ImmGetCompositionString(hIMC, flag, str, len * sizeof(uni_char));
		imstring.Set(str, len);
		delete[] str;
	}
	else
	{
		imstring.Set(UNI_L(""), 0);
	}
	return len;
}

#endif // WIDGETS_IME_SUPPORT


WindowsOpView::WindowsOpView() : m_drag_listener(NULL), mouseListener(NULL)
{
	m_mouse_buttons[0] = m_mouse_buttons[1] = m_mouse_buttons[2] = false;
}

WindowsOpView::~WindowsOpView()
{
	if (this == s_safe_view_pointer)
		s_safe_view_pointer = NULL;
}

inline ShiftKeyState WindowsOpView::GetShiftKeys() 
{ 
	return g_op_system_info->GetShiftKeyState(); 
}

BOOL WindowsOpView::GetMouseButton(MouseButton button)
{
	if (GetSystemMetrics(SM_SWAPBUTTON))
	{
		if (button == MOUSE_BUTTON_1)
			button = MOUSE_BUTTON_2;
		else
			if (button == MOUSE_BUTTON_2)
				button = MOUSE_BUTTON_1;
	}

	switch (button)
	{
	case MOUSE_BUTTON_1:
		return m_mouse_buttons[MOUSE_BUTTON_1] || GetAsyncKeyState(VK_LBUTTON) & 0x8000;

	case MOUSE_BUTTON_2:
		return m_mouse_buttons[MOUSE_BUTTON_2] || GetAsyncKeyState(VK_RBUTTON) & 0x8000;

	case MOUSE_BUTTON_3:
		return m_mouse_buttons[MOUSE_BUTTON_3] || GetAsyncKeyState(VK_MBUTTON) & 0x8000;

	default:
		return FALSE;
	}
}

void WindowsOpView::SetMouseButton(MouseButton button, bool set)
{
	OP_ASSERT(button >= MOUSE_BUTTON_1 && button <= MOUSE_BUTTON_3);

	m_mouse_buttons[button] = set;
}

#ifdef WIDGETS_IME_SUPPORT

HIMC WindowsOpView::s_hIMC = NULL;

void WindowsOpView::AbortInputMethodComposing(HWND hwnd)
{
	OpDestroyCaret(hwnd);

	if (compose_started)
	{
		HIMC hIMC = ImmGetContext(hwnd);
		ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
		ImmReleaseContext(hwnd, hIMC);
	}
}

void WindowsOpView::AbortInputMethodComposing()
{
	WindowsOpWindow* tw = GetNativeWindow();
	if (!tw || !tw->m_hwnd)
		return;
	AbortInputMethodComposing(tw->m_hwnd);
}

void WindowsOpView::SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle)
{
	WindowsOpWindow* tw = GetNativeWindow();
	if (!tw || !tw->m_hwnd)
		return;
	HWND hwnd = tw->m_hwnd;

	if (mode == IME_MODE_TEXT)
	{
		HIMC imc = GetGlobalHIMC();
		ImmAssociateContext(hwnd, imc);
	}
	else
	{
		ImmAssociateContext(hwnd, NULL);
	}
}

HIMC WindowsOpView::GetGlobalHIMC()
{
	if(!s_hIMC)
	{
		s_hIMC = ImmCreateContext();
	}

	return s_hIMC;
}

#endif // WIDGETS_IME_SUPPORT

/*static*/
void WindowsOpView::OpCreateCaret(HWND hwnd, INT32 w, INT32 h)
{
	if (hwnd == s_global_caret_hwnd && h == s_global_caret_height && w == s_global_caret_width)
		return;

	if (s_global_caret_hwnd && DestroyCaret())
		s_global_caret_hwnd = NULL;

	HBITMAP empty_bitmap = CreateBitmap(0, 0, 1, 1, NULL);

	if (CreateCaret(hwnd, empty_bitmap, w, h))
	{
		s_global_caret_hwnd = hwnd;
		s_global_caret_height = h;
		s_global_caret_width = w;
	}
}

/*static*/
void WindowsOpView::OpDestroyCaret(HWND hwnd)
{
	if (hwnd == s_global_caret_hwnd && DestroyCaret())
		s_global_caret_hwnd = NULL;
}

/*static*/
void WindowsOpView::OpSetCaretPos(HWND hwnd, INT32 x, INT32 y)
{
	if (s_global_caret_hwnd)
	{
		SetCaretPos(x, y);
		ShowCaret(hwnd);
	}
}

void WindowsOpView::OnCaretPosChanged(const OpRect &rect)
{
	WindowsOpWindow* window = GetNativeWindow();
	if (!window || !window->m_hwnd)
		return;

	OpPoint screen_caret_pos = ConvertToScreen(rect.TopLeft());
	INT32 window_x, window_y;
	window->GetInnerPos(&window_x, &window_y);

	OpCreateCaret(window->m_hwnd, rect.width, rect.height);
	OpSetCaretPos(window->m_hwnd, screen_caret_pos.x - window_x, screen_caret_pos.y-window_y);
}

void WindowsOpView::GetMousePos(INT32* xpos, INT32* ypos)
{
	WindowsOpWindow* tw = GetNativeWindow();
  	POINT pt;

   	GetCursorPos(&pt);
	if (tw && tw->m_hwnd)
   		ScreenToClient(tw->m_hwnd, &pt);
	*xpos = pt.x;
	*ypos = pt.y;
	GetMDEWidget()->ConvertFromScreen(*xpos, *ypos);
}

OpPoint WindowsOpView::ConvertToScreen(const OpPoint &point)
{
	WindowsOpWindow* tw = GetNativeWindow();
	OpPoint sp = MDE_OpView::ConvertToScreen(point);
	POINT wsp = {sp.x, sp.y};
	if (tw && tw->m_hwnd)
		ClientToScreen(tw->m_hwnd, &wsp);
	sp.x = wsp.x;
	sp.y = wsp.y;
	return sp;
}

OpPoint WindowsOpView::ConvertFromScreen(const OpPoint &point)
{
	WindowsOpWindow* tw = GetNativeWindow();
	POINT pt = {point.x, point.y};

	if (tw && tw->m_hwnd)
		ScreenToClient(tw->m_hwnd, &pt);

	return MDE_OpView::ConvertFromScreen(OpPoint(pt.x, pt.y));
}

WindowsOpWindow* WindowsOpView::GetNativeWindow()
{
	MDE_OpView* v = this;
	WindowsOpWindow* w = NULL;
	while (v || (w && !w->m_hwnd))
	{
		if (v)
		{
			if (v->GetParentView())
				v = v->GetParentView();
			else
			{
				w = (WindowsOpWindow*)v->GetParentWindow();
				v = NULL;
			}
		}
		else
		{
			if (w->GetParentView())
			{
				v = (MDE_OpView*)w->GetParentView();
				w = NULL;
			}
			else
			{
				w = (WindowsOpWindow*)w->GetParentWindow();
			}
		}
	}
	return w;
}

void WindowsOpView::SetDragListener(OpDragListener* draglistener)
{
	m_drag_listener = draglistener;
	MDE_OpView::SetDragListener(draglistener);
}

void WindowsOpView::SetMouseListener(OpMouseListener* mouselistener)
{
	mouseListener = mouselistener;
	MDE_OpView::SetMouseListener(mouselistener);
}

BOOL WindowsOpView::IsFocusable()
{
	return !GetParentWindow() || ((WindowsOpWindow*)GetParentWindow())->IsFocusable();
}

/*static*/
OP_STATUS DesktopOpView::Create(OpView** new_opview, OpWindow* parentwindow)
{
	OP_ASSERT(new_opview);
	*new_opview = OP_NEW(WindowsOpView, ());
	if (*new_opview == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = ((MDE_OpView*)(*new_opview))->Init(parentwindow);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(*new_opview);
		*new_opview = 0;
	}

	return status;
}

/*static*/
OP_STATUS DesktopOpView::Create(OpView** new_opview, OpView* parentview)
{
	OP_ASSERT(new_opview);
	*new_opview = OP_NEW(WindowsOpView, ());
	if (*new_opview == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = ((MDE_OpView*)(*new_opview))->Init(parentview);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(*new_opview);
		*new_opview = 0;
	}
	return status;
}

