/**
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef X11_CLIENT_H_
#define X11_CLIENT_H_

#include "modules/hardcore/keys/opkeys.h"
#include "platforms/utilix/x11types.h"

class OpWindow;

/**
 * Retrieve information about the X11 client without taking on unwanted dependencies.
 *
 * An interface, currently implemented by core and desktop, that allows Unix platform
 * code shared between these teams, to retrieve information about the X11 client without
 * taking on a dependency on either desktop (unix module) or core-specific (lingogi module)
 * code.
 *
 */
class X11Client
{
public:
	static X11Client* Instance();

	virtual ~X11Client() { }

	/** Return the connection to the X server. */
	virtual X11Types::Display* GetDisplay() = 0;

	/** Return the screen number in use by a window.
	 * \param window  The window whose screen number is desired.  */
	virtual int GetScreenNumber(X11Types::Window window) = 0;

	/** Return the top-level window harboring a window.
	 * \param window  The window whose top-level window is desired. */
	virtual X11Types::Window GetTopLevelWindow(OpWindow* window) = 0;

	/** Returns the platform specific data1/data2 parameters
	 * associated with the specified key stroke. See the
	 * documentation of OpNS4PluginAdapter::GetKeyEventPlatformData().
	 * NOTE: This function should be moved from X11Client since it
	 * related to an Opera specific key event workaround and no to X11. **/
	virtual void GetKeyEventPlatformData(uni_char key, bool pressed, ShiftKeyState modifiers, UINT64& data1, UINT64& data2) = 0;
};

#endif  // X11_CLIENT_H_
