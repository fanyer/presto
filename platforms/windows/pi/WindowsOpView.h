/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPVIEW_H
#define WINDOWSOPVIEW_H

#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/pi/OpView.h"


#ifdef WIDGETS_IME_SUPPORT
typedef BOOL (WINAPI *LPFIMMASSOCIATECONTEXTEX)(HWND,HIMC,DWORD);
LPFIMMASSOCIATECONTEXTEX LoadImmAssociateContextEx();
#endif


class WindowsOpWindow;

class WindowsOpView : public MDE_OpView
{
public:
	WindowsOpView();
	virtual ~WindowsOpView();

	virtual ShiftKeyState GetShiftKeys();
	virtual BOOL GetMouseButton(MouseButton button);
	virtual void SetMouseButton(MouseButton button, bool set);
	virtual void GetMousePos(INT32* xpos, INT32* ypos);
	
	virtual OpPoint ConvertToScreen(const OpPoint &point);
	virtual OpPoint ConvertFromScreen(const OpPoint &point);
	WindowsOpWindow* GetNativeWindow();

#ifdef WIDGETS_IME_SUPPORT
	static void AbortInputMethodComposing(HWND hwnd);
	virtual void AbortInputMethodComposing();
	virtual void SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle);

	static HIMC GetGlobalHIMC();
	static HIMC s_hIMC;
	static WindowsOpView* s_safe_view_pointer; ///< Will be set to NULL automatically if the view is deleted.
#endif
	virtual void OnHighlightRectChanged(const OpRect &rect) {}
	virtual void OnCaretPosChanged(const OpRect &rect);

	virtual void SetDragListener(OpDragListener* draglistener);
	virtual void SetMouseListener(OpMouseListener* mouselistener);

	BOOL IsFocusable();

	OpDragListener* m_drag_listener;
	OpMouseListener* mouseListener;

	static void OpCreateCaret(HWND hwnd, INT32 w, INT32 h);
	static void OpDestroyCaret(HWND hwnd);
	static void OpSetCaretPos(HWND hwnd, INT32 x, INT32 y);
	static HWND GetGlobalCaretHWND() { return s_global_caret_hwnd; }

private:
	static HWND s_global_caret_hwnd;
	static int s_global_caret_height;
	static int s_global_caret_width;

	bool m_mouse_buttons[3];	// Used by SetMouseButton
};
#endif //WINDOWSOPVIEW_H
