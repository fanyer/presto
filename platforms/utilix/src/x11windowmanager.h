/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Martin Olsson (molsson)
 */

#ifndef X11_WINDOW_MANAGER_H
#define X11_WINDOW_MANAGER_H

#include "platforms/utilix/x11types.h"

class X11WindowManager
{
private:
	X11Types::Display *m_display;
	X11Types::Window m_root_window;
	X11Types::Atom m_atom_net_supported;

	/**
	 * This is the Xatom with name "_NET_WM_WINDOW_OPACITY". The atom has a 32
	 * bit integer value which indicates the desired opacity for a x11 window.
	 * The value 0 means transparent and 0xffffffff is opaque.
	 * SetWindowOpacity() uses this atom.
	 */
	X11Types::Atom m_atom_net_wm_window_opacity;
	X11Types::Atom m_atom_net_wm_state;
	X11Types::Atom m_atom_net_wm_state_skip_taskbar;
	X11Types::Atom m_atom_net_wm_window_type;
	X11Types::Atom m_atom_net_wm_window_type_toolbar;

public:
	X11WindowManager(X11Types::Display *display);

	/**
	 * Asks the window manager whether it supports setting a custom window opacity
	 * for top-level windows to make them transparent. Most compositing window managers,
	 * like for example "compiz", does support this. Note that some distros, like Ubuntu,
	 * allows the user to switch window managers at runtime (Ubuntu uses compiz vs metacity
	 * for the items "Normal" and "None" in "System::Preferences::Apperance::Visual Effects"
	 * and the later doesn't support transparent windows.
	 *
	 * It's a necessary but not sufficient condition that the X11 Composite extension is
	 * installed and enabled, for this function to return true.
	 */
	bool SupportsWindowOpacity();

	/**
	 * Changes the opacity for a top-level window to make it more or less transparent.
	 * This function has no effect when SupportsWindowOpacity() return
	 * false.
	 *
	 * @param window specifies the X11 window which receives a new
	 *  opacity level.
	 * @param opacity specifies the opacity level. Allowed are values
	 *  between 0.0 = fully transparent and 1.0 = fully opaque.
	 */
	void SetWindowOpacity(X11Types::Window window, double opacity);

	/**
	 * Tells the window manager that no "taskbar" should be displayed for this top-level window.
	 * The exact definition of "taskbar" depends on which window manager is in use, a generic definition
	 * can be found in the X11 EWMH window manager specification. For a standard Ubuntu using GNOME,
	 * it means that the top-level window doesn't get a taskbar button in the lower part of the gnome-panel.
	 */
	void SuppressWindowTaskbar(X11Types::Window window);

	/**
	 * Sets the window type of a top-level window. The exact meaning of each window type depends on
	 * the window manager and a generic definition for each window type is provided in the X11 EWMH
	 * specification.
	 */
	void SetWindowType(X11Types::Window window);
};

#endif // X11_WINDOW_MANAGER_H

