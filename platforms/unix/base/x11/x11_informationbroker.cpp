/**
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_informationbroker.h"

#include "platforms/utilix/x11_all.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widget.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

X11Client* X11Client::Instance()
{
	static X11InformationBroker broker;
	return &broker;
}

X11Types::Display* X11InformationBroker::GetDisplay()
{
	return g_x11->GetDisplay();
}

int X11InformationBroker::GetScreenNumber(X11Types::Window window)
{
	int screen = -1;
	if (window)
	{
		XWindowAttributes attr;
		if (XGetWindowAttributes(GetDisplay(), window, &attr))
			screen = XScreenNumberOfScreen(attr.screen);
	}

	if (screen < 0)
		return DefaultScreen(g_x11->GetDisplay());

	return screen;
}

X11Types::Window X11InformationBroker::GetTopLevelWindow(OpWindow* window)
{
	if (!window)
		return None;

	X11OpWindow* rootwindow = static_cast<X11OpWindow*>(window->GetRootWindow());
	if (!rootwindow || !rootwindow->GetWidget())
		return None;

	return rootwindow->GetWidget()->GetWindowHandle();
}

void X11InformationBroker::GetKeyEventPlatformData(uni_char key, bool pressed, ShiftKeyState modifiers, UINT64& data1, UINT64& data2)
{
	XEvent event = X11OpMessageLoop::Self()->GetPluginEvent(X11OpMessageLoop::KEY_EVENT);
	data1 = event.xkey.state;
	data2 = event.xkey.keycode;
}
