/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "x11_popupwidget.h"
#include "x11_opwindow.h"
#include "modules/widgets/OpWidget.h"

X11PopupWidget::X11PopupWidget(X11OpWindow *opwindow)
	: X11WindowWidget(opwindow), disable_autoclose(false), wait_for_enternotify(true)
{
}

OP_STATUS X11PopupWidget::Init(X11Widget *parent, const char *name, int flags)
{
	flags |= TRANSIENT | BYPASS_WM | POPUP | AUTOGRAB;
	return X11WindowWidget::Init(parent, name, flags);
}

bool X11PopupWidget::HandleEvent(XEvent *event)
{
	if (!disable_autoclose)
	{
		switch (event->type)
		{
		case EnterNotify:
		{
			wait_for_enternotify = false;
			break;
		}
		case MapNotify:
		{
			X11Types::Window root, child;
			int root_x, root_y;
			int win_x, win_y;
			unsigned int mask;
			XQueryPointer(GetDisplay(), GetWindowHandle(), &root, &child,
						  &root_x, &root_y, &win_x, &win_y, &mask);
			if (win_x >= 0 && win_y >= 0)
			{
				int width, height;
				GetSize(&width, &height);
				if (win_x < width && win_y < height)
					wait_for_enternotify = false;
			}
			break;
		}

		case ButtonPress:
		{
			int x = event->xmotion.x_root;
			int y = event->xmotion.y_root;
			PointFromGlobal(&x, &y);
			bool outside = false;
			if (x < 0 || y < 0)
			{
				outside = true;
			}
			else
			{
				int width, height;
				GetSize(&width, &height);
				if (x >= width || y >= height)
					outside = true;
			}
			if (outside && m_op_window)
			{
				Hide();
				OpWindowListener *listener = m_op_window->GetWindowListener();
				if (listener)
					listener->OnClose(TRUE);
				return true;
			}
		}
		}
	}
	return X11WindowWidget::HandleEvent(event);
}
