/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Patryk Obara
*/

#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "x11_gadget.h"

#include "x11_atomizer.h"
#include "x11_globals.h"
#include "x11_ipcmanager.h"

#include "platforms/utilix/x11_all.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/DesktopGadget.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/widgetruntime/GadgetStartup.h"

using X11Types::Atom;

static int num_x11_gadget = 0;

X11Gadget::X11Gadget(X11OpWindow* op_window, OpWindow::Style style)
	: X11WindowWidget(op_window)
	, m_window_flags(0)
{
	if (style == OpWindow::STYLE_GADGET)
		m_window_flags = X11Widget::TRANSPARENT|X11Widget::NODECORATION;
	num_x11_gadget++;
}


X11Gadget::~X11Gadget()
{
	num_x11_gadget--;
}


OP_STATUS X11Gadget::Init(X11Widget *parent, const char *name, int flags)
{
	RETURN_IF_ERROR( X11WindowWidget::Init(parent, name, m_window_flags|flags) );

	SetWindowRole("widget");

#ifdef WIDGET_RUNTIME_SUPPORT
	X11IPCManager::Init(GetWindowHandle());
#endif // WIDGET_RUNTIME_SUPPORT

	return OpStatus::OK;
}


bool X11Gadget::HandleEvent(XEvent* event)
{
	if (GadgetStartup::IsGadgetStartupRequested())
	{
		switch (event->type)
		{
			case LeaveNotify:
			{
				// hack; we have to shoot straight to DesktopGadget atm
				DesktopWindow *dw = g_application->GetActiveDesktopWindow(TRUE);
				DesktopGadget *dg = static_cast<DesktopGadget*>(dw);
				if (dg)
				{
					dg->OnMouseGadgetLeave(NULL);
					return true;
				}
			}
			break;

			case PropertyNotify:
			{
				// Workaround for metacity not being able to un-minimize gadget properly (DSK-285640)
				// This must not be done for decorated gadgets (DSK-304431)
				if (event->xproperty.atom == ATOMIZE(_NET_WM_STATE) &&
					!g_x11->IsCompositingManagerActive(m_screen) &&
					GetOpWindow() && GetOpWindow()->GetStyle() != OpWindow::STYLE_DECORATED_GADGET)
				{
					X11Types::Atom rettype;
					int format;
					unsigned long nitems;
					unsigned long bytes_remaining;
					unsigned char *data = 0;

					int rc = XGetWindowProperty(GetDisplay(), GetWindowHandle(), 
												event->xproperty.atom, 0, 10, False, AnyPropertyType, &rettype,
												&format, &nitems, &bytes_remaining, &data);

					if (rc == Success && data && rettype == XA_ATOM && format == 32 && nitems > 0)
					{
						X11Types::Atom *state = (X11Types::Atom *)data;
						bool hidden			= false;
						bool skip_taskbar	= false;
						bool below			= false;
						bool above			= false;

						for (unsigned long i=0; i<nitems; i++)
						{
							X11Types::Atom cur = state[i];
							if (cur == ATOMIZE(_NET_WM_STATE_HIDDEN))
								hidden = true;
							else if (cur == ATOMIZE(_NET_WM_STATE_SKIP_TASKBAR))
								skip_taskbar = true;
							else if (cur == ATOMIZE(_NET_WM_STATE_BELOW))
								below = true;
							else if (cur == ATOMIZE(_NET_WM_STATE_ABOVE))
								above = true;
						}

						bool normal = !below && !above;

						if (skip_taskbar && (hidden || normal))
							SetWMSkipTaskbar(false);
						else if (!skip_taskbar && !hidden && !normal)
							SetWMSkipTaskbar(true);
					}
					if (data)
						XFree(data);
				}
			}
			break;
		}
	}
	return X11WindowWidget::HandleEvent(event);
}


// static
DesktopWindow* X11Gadget::GetDesktopWindow()
{
	// what we should do in here is search through desktop window collection,
	// find one created with this opwindow and return it here
	// however GadgetApplication does not maintain proper collection
	// and we have to keep DesktopWindow structure b0rken because
	// of Unite dependency. So for now, this is enough. [pobara 2010-02-26]
	return g_application->GetActiveDesktopWindow(TRUE);
}


#endif // GADGET_SUPPORT
