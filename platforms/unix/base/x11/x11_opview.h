/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_OPVIEW_H__
#define __X11_OPVIEW_H__

#include "modules/util/adt/oplisteners.h"
#include "modules/libgogi/pi_impl/mde_opview.h"

class X11OpWindow;

class X11OpView : public MDE_OpView
{
public:
	X11OpView() : m_im_listener(0) {}
	virtual ~X11OpView() {}
	virtual void GetMousePos(INT32 *xpos, INT32 *ypos);
	virtual OpPoint ConvertToScreen(const OpPoint &point);
	virtual OpPoint ConvertFromScreen(const OpPoint &point);
	virtual ShiftKeyState GetShiftKeys();

	virtual void SetInputMethodListener(OpInputMethodListener* imlistener) { m_im_listener = imlistener; }
	virtual void AbortInputMethodComposing();
	virtual void SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle);

	X11OpWindow* GetNativeWindow();

private:
	OpInputMethodListener* m_im_listener;
};

#endif // __X11_OPVIEW_H__
