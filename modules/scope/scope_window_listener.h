/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_WINDOW_LISTENER_H
#define OP_SCOPE_WINDOW_LISTENER_H

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT

class Window;

/**
 * Use this interface to notify the client of new, removed
 * and changed Windows.
 */
class OpScopeWindowListener
{
public:

	/**
	 * Notify the client that a new Window was created.
	 *
	 * @param window The Window that was just created.
	 **/
	static void OnNewWindow(Window *window);

	/**
	 * Notify the client that a Window has been removed (but not yet deleted).
	 *
	 * @param window The Window that was just removed.
	 **/
	static void OnWindowRemoved(Window *window);

	/**
	 * Notify the client that the title of a Window has changed.
	 *
	 * @param window The Window with an updated title.
	 **/
	static void OnWindowTitleChanged(Window *window);

	/**
	 * Notify the client that the active Window has changed.
	 *
	 * @param window The new active Window.
	 **/
	static void OnActiveWindowChanged(Window *window);

}; // OpScopeWindowListener

#endif // SCOPE_WINDOW_MANAGER_SUPPORT

#endif // OP_SCOPE_WINDOW_LISTENER_H
