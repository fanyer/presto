/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_opview.h"
#include "x11_opwindow.h"
#include "x11_windowwidget.h"
#include "x11_globals.h"
#include "inpututils.h"


void X11OpView::GetMousePos(INT32 *xpos, INT32 *ypos)
{
	X11Types::Window root, chld;
	X11Types::Display* disp = g_x11->GetDisplay();
	int rx, ry, wx, wy;
	unsigned int mask;
	XQueryPointer(disp, DefaultRootWindow(disp), &root, &chld, &rx, &ry, &wx, &wy, &mask);
	OpPoint rel = ConvertToScreen(OpPoint(0,0));
	*xpos = rx-rel.x;
	*ypos = ry-rel.y;
}

OpPoint X11OpView::ConvertToScreen(const OpPoint &point)
{
	X11OpWindow* tw = GetNativeWindow();
	OpPoint sp = MDE_OpView::ConvertToScreen(point);
	int x = sp.x;
	int y = sp.y;
	tw->GetWidget()->PointToGlobal(&x, &y);
	sp.x = x;
	sp.y = y;
	return sp;
}

OpPoint X11OpView::ConvertFromScreen(const OpPoint &point)
{
	X11OpWindow* tw = GetNativeWindow();

	int x = point.x;
	int y = point.y;
	tw->GetWidget()->PointFromGlobal(&x, &y);

	return MDE_OpView::ConvertFromScreen( OpPoint(x,y));
}

ShiftKeyState X11OpView::GetShiftKeys()
{
	return InputUtils::GetOpModifierFlags();
}

X11OpWindow* X11OpView::GetNativeWindow()
{
	MDE_OpView* v = this;
	X11OpWindow* w = NULL;
	while (v || (w && !w->IsToplevel()))
	{
		if (v)
		{
			if (v->GetParentView())
				v = (MDE_OpView*)v->GetParentView();
			else
			{
				w = (X11OpWindow*)v->GetParentWindow();
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
				w = (X11OpWindow*)w->GetParentWindow();
		}
	}
	return w;
}

void X11OpView::AbortInputMethodComposing()
{
	// TODO. We really want the nearest toplevel widget of the OpView
	X11Widget* widget = X11WindowWidget::GetActiveWindowWidget();
	if (widget)
		widget = widget->GetNearestTopLevel();

	if (!widget)
		return;

	widget->AbortIM();
}

void X11OpView::SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle)
{
	// TODO. We really want the nearest toplevel widget of the OpView
	// TODO. We do not use 'context' yet. We may not have have to
	X11Widget* widget = X11WindowWidget::GetActiveWindowWidget();
	if (widget)
		widget = widget->GetNearestTopLevel();

	if (!widget)
		return;

	if (mode == IME_MODE_TEXT)
		widget->ActivateIM(m_im_listener);
	else
		widget->DeactivateIM(m_im_listener);
}
