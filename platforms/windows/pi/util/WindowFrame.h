/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifndef WINDOWS_WINDOWFRAME_H
#define WINDOWS_WINDOWFRAME_H

class WindowsOpWindow;

#define WINDOW_DEFAULT_BORDER_SIZE		5
#define WINDOW_DEFAULT_UI_BORDER_SIZE	2
#define WINDOW_MINIMAL_BORDER_SIZE		2
#define WINDOW_DEFAULT_AERO_BORDER_SIZE	6

class WindowFrameSizes
{
	friend class WindowFrame;

public:
	WindowFrameSizes() 
	{
		m_ui_rect.left = m_ui_rect.bottom = m_ui_rect.right = m_ui_rect.top = 0;
		m_native_rect.left = m_native_rect.bottom = m_native_rect.right = m_native_rect.top = 0;
	}
	/*
	* Get the sizes used for the UI frame. The UI frame defines how much
	* the UI will be inset compared to the background view (which is only visibly drawn 
	* when a persona is active).  This allows for rendering the persona outside the UI 
	* frame onto the native Windows frame, if needed, and have a area for the background
	* MDE_View used for rendering a border around UI, a personas and similar.
	*/
	RECT GetUIFrameSize()		{ return m_ui_rect; }
	/*
	* Get the sizes used for the native frame. When a persona is active, this is usually
	* set to 0.  It defines how large the Windows border size will be in when personas
	* is not active.
	*/
	RECT GetNativeFrameSize()	{ return m_native_rect; }

private:
	RECT m_ui_rect;
	RECT m_native_rect;
};

/*
* This is the class for abstracting how a top-level window frame is presented
*/

class WindowFrame
{
private:
	WindowFrame() {}

public:
	WindowFrame(WindowsOpWindow *window) : m_window(window) {}
	virtual ~WindowFrame() {}

	void CalculateSizes();
	BOOL UseCustomNativeFrame();
	BOOL UseCustomUIFrame();

	// return the size of the frame that the UI view will have compared to the inner background view view
	RECT GetUIFrameSizes();

	// return the size of the Windows aero/non-client frame that Opera will use
	RECT GetNativeFrameSizes();

protected:
	WindowsOpWindow	*m_window;
	WindowFrameSizes m_sizes;
};

#endif // WINDOWS_WINDOWFRAME_H
