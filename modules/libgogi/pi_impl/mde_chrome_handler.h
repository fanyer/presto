/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MDECHROMEHANDLER_H
#define MDECHROMEHANDLER_H
#ifdef MDE_OPWINDOW_CHROME_SUPPORT

class OpWindow;

/** Handler of chrome for a OpWindow in restored state.
	The chrome handler should set up graphics to draw the chrome in the chrome_window passed to InitChrome.
	The chrome_window is created for all restored OpWindows and will automatically track the target window.
*/

class MDEChromeHandler
{
public:
	virtual ~MDEChromeHandler() {}

	/** Create a new instance of MDEChromeHandler. */
	static MDEChromeHandler* Create();

	/** Called after the chrome window has been resized. */
	virtual void OnResize(int new_w, int new_h) = 0;

	/** Init the chrome for the chrome_window. target_window is the window that is controlled by the chrome. */
	virtual OP_STATUS InitChrome(OpWindow *chrome_window, OpWindow *target_window, int &chrome_sz_left, int &chrome_sz_top, int &chrome_sz_right, int &chrome_sz_bottom) = 0;

	/** Set icon and title. */
	virtual void SetIconAndTitle(OpBitmap* icon, uni_char* title) = 0;
private:
};

#endif // MDE_OPWINDOW_CHROME_SUPPORT
#endif // MDECHROMEHANDLER_H
