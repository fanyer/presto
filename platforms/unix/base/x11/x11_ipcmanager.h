/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __X11_IPC_MANAGER_H__
#define __X11_IPC_MANAGER_H__

#include "platforms/utilix/x11_all.h"



class X11IPCManager
{
public:
	enum ErrorCode
	{
		WINDOW_NOT_FOUND = 0,
		WINDOW_EXISTS_WITH_OTHER_NAME,
		GADGET_WINDOW_EXISTS,
		WINDOW_EXISTS
	};

public:
	/**
	 * Inspects events and uses them if appropriate
	 *
	 * @return TRUE if event is consumed, otherwise FALSE
	 */
	BOOL HandleEvent(XEvent* event);

	/**
	 * Creates the ipc manager instance
	 *
	 * @return OpStatus::OK on sucess, otherwise an error code
	 */	
	static OP_STATUS Create();
	
	/**
	 * Destroys the ipc manager instance
	 */
	static void Destroy();

	/**
	 * Sets properties on the window so that it can be located
	 * by a remote instance.
	 *
	 * @param window The window 
	 *
	 * @return OpStatus::OK on sucess, otherwise an error code
	 */
	static OP_STATUS Init(X11Types::Window window);

	/**
	 * Scans descendants of the root window on the default screen to determine if one of them is
	 * a root window. On a match this window is returned.
	 *
	 * @param window Only this window is tested if not None. This window can be on a non-default screen
	 * @param name If not empty the matched window must match this name as well
	 * @param error On return this entry is filled with an appropriate code
	 *
	 * @return The matched window identifier or None
	 */
	static X11Types::Window FindOperaWindow(X11Types::Window window, const OpStringC& name, ErrorCode& error);

	/**
	 * Tests window to determine if it is an opera window or if refers to another
	 * valid opera window. The name can refer to another window within the group
	 * of toplevel windows. If so that window it is returned.
	 *
	 * @param window The window to test. On return it will be a valid opera window
	 *        if the return code is not WINDOW_NOT_FOUND
	 * @param home_folder If not empty the mached window must match this home folder
	 * @param name If not empty the mached window must match this name. The special
	 *        names of "first" and "last" refer to the first and last toplevel window
	 *        within the group of toplevel windows.
	 *
	 * @param On return this entry is filled with an appropriate code
	 */
	static ErrorCode IsOperaWindow(X11Types::Window& window, const OpStringC& home_folder, const OpStringC& name);
	
	/**
	 * Returns TRUE if the window has a property (OPERA_WINDOW_NAME) with a content
	 * that matches the name. An empty name is never matched
	 *
	 * @param window The window to test
	 * @param name The name to match
	 * 
	 * @return See above
	 */
	static BOOL HasWindowName(X11Types::Window window, const OpStringC& name);

	/**
	 * Sets a message as a property (OPERA_MESSAGE) on the window
	 *
	 * @param window The window 
	 * @param message (in utf8) The text
	 *
	 * @return TRUE of success, otherwise FALSE
	 */
	static BOOL Set(X11Types::Window window, const OpStringC& message);

	/**
	 * Read a message from a property (OPERA_MESSAGE) on the window
	 *
	 * @param window The window 
	 * @param message (in utf8) On return the text read
	 *
	 * @return TRUE of success, otherwise FALSE
	 */
	static void Get(X11Types::Window window, OpString& message);

	/**
	 * Sets or releases a sempahore on the window using the property OPERA_SEMAPHORE
	 *
	 * @param window The window 
	 * @param set Set semaphore if TRUE and release if FALSE
	 *
	 * @return TRUE of success, otherwise FALSE
	 */
	static BOOL Lock(X11Types::Window window, BOOL set);

private:
	/**
	 * Internal window list item structure used by FindOperaWindow.
	 */
	struct WindowList
	{
		WindowList(X11Types::Window* w, unsigned int n) : windows(w), num_windows(n), next(0) {}
		X11Types::Window *windows;
		unsigned int num_windows;
		WindowList *next;
	};

	/**
	 * Creates object
	 */
	X11IPCManager();

	/**
	 * Removes object and deletes all allocated resources
	 */
	~X11IPCManager();

private:
	static X11IPCManager* m_manager;
};

extern X11IPCManager* g_ipc_manager;


#endif
