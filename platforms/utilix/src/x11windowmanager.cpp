/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Martin Olsson (molsson)
 */

#include "core/pch.h"
#ifdef UTILIX

#include "platforms/utilix/src/x11windowmanager.h"
#include "platforms/utilix/x11_all.h"

X11WindowManager::X11WindowManager(X11Types::Display *display)
	: m_display(display)
{
	m_root_window = RootWindow(display, DefaultScreen(display));
	m_atom_net_supported = XInternAtom(display, "_NET_SUPPORTED", False);
	m_atom_net_wm_window_opacity = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
	m_atom_net_wm_state = XInternAtom (display, "_NET_WM_STATE", False);
	m_atom_net_wm_state_skip_taskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
	m_atom_net_wm_window_type = XInternAtom (display, "_NET_WM_WINDOW_TYPE", False);
	m_atom_net_wm_window_type_toolbar = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
}

bool X11WindowManager::SupportsWindowOpacity()
{
	X11Types::Atom actual_type_return = 0;
	int actual_format_return = 0;
	unsigned long nitems_return = 0, bytes_after_return = 0;
	X11Types::Atom *prop_return = 0;
	if (XGetWindowProperty(m_display, m_root_window, m_atom_net_supported, 0, LONG_MAX, /* delete= */ False, /* req_type=XA_ATOM */ (X11Types::Atom)4,
			&actual_type_return, &actual_format_return, &nitems_return, &bytes_after_return, (unsigned char**)&prop_return) == Success)
	{
		if (actual_type_return == (X11Types::Atom)4)
		{
			for (unsigned long i = 0; i < nitems_return; ++i)
				if (prop_return[i] == m_atom_net_wm_window_opacity)
				{
					XFree(prop_return);
					return true;
				}
		}
		XFree(prop_return);
	}
	return false;
}

void X11WindowManager::SetWindowOpacity(X11Types::Window window, double opacity)
{
	OP_ASSERT(0.0 <= opacity && opacity <= 1.0);
	unsigned int opacity_int = (unsigned int) (0xffffffff * opacity);
	XChangeProperty(m_display, window, m_atom_net_wm_window_opacity, (X11Types::Atom)6 /* = XA_CARDINAL */, 32, PropModeReplace, (unsigned char *) &opacity_int, 1);
}

void X11WindowManager::SuppressWindowTaskbar(X11Types::Window window)
{
	XChangeProperty(m_display, window, m_atom_net_wm_state, (X11Types::Atom)4 /* = XA_ATOM */, 32, PropModeAppend, (unsigned char *)&m_atom_net_wm_state_skip_taskbar, 1);
}

void X11WindowManager::SetWindowType(X11Types::Window window)
{
	XChangeProperty(m_display, window, m_atom_net_wm_window_type, (X11Types::Atom)4 /* = XA_ATOM */, 32, PropModeReplace, (unsigned char *)&m_atom_net_wm_window_type_toolbar, 1);
}

#endif // UTILIX
