/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_POPUPWIDGET_H
#define X11_POPUPWIDGET_H

#include "x11_windowwidget.h"


class X11PopupWidget : public X11WindowWidget
{
private:
	bool disable_autoclose;
	bool wait_for_enternotify;

public:
	X11PopupWidget(X11OpWindow *opwindow);

	OP_STATUS Init(X11Widget *parent, const char *name, int flags);

	bool HandleEvent(XEvent *event);

	/** Disable automatic closure when clicking outside widget?
	 * Initial value: false
	 */
	void DisableAutoClose(bool dis) { disable_autoclose = dis; }
};

#endif // X11_POPUPWIDGET_H
