/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2006-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Patryk Obara
*/

#ifndef X11_GADGET_H
#define X11_GADGET_H

#ifdef GADGET_SUPPORT

#include "x11_windowwidget.h"
#include "x11_opwindow.h"

class X11Widget;
class DesktopWindow;

/**
 * \brief X11Gadget is toplevel window that represents gadget to the window
 * manager.
 */
class X11Gadget : public X11WindowWidget
{
public:

	X11Gadget(X11OpWindow *opwindow, OpWindow::Style style);

	virtual ~X11Gadget();

	OP_STATUS Init(X11Widget *parent, const char *name, int flags=0);

	virtual bool HandleEvent(XEvent* event);

	BOOL PreferWindowDragging() const { return TRUE; }

	static DesktopWindow* GetDesktopWindow();

private:
	int m_window_flags;
};

#endif // GADGET_SUPPORT
#endif // X11_GADGET_H
