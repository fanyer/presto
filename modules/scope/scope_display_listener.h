/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_DISPLAY_LISTENER_H
#define OP_SCOPE_DISPLAY_LISTENER_H

#ifdef SCOPE_EXEC_SUPPORT

class Window;

/**
 * An interface for notifying scope of display-related events.
 */
class OpScopeDisplayListener
{
public:

	/**
	 * Call this when the root document of the Window is painted.
	 *
	 * @param window The Window that was just painted.
	 **/
	static void OnWindowPainted(Window* window, OpRect rect);

}; // OpScopeDisplayListener

#endif // SCOPE_EXEC_SUPPORT

#endif // OP_SCOPE_DISPLAY_LISTENER_H
