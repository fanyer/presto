/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
 
#ifndef DOCHAND_WINDOWLISTENER_H
#define DOCHAND_WINDOWLISTENER_H

#include "modules/pi/OpWindow.h"

class Window;

class WindowListener
	: public OpWindowListener
{
private:
	Window *window;

public:
	WindowListener(Window *window);

	void OnResize(UINT32 width, UINT32 height);
	void OnMove();
	void OnRenderingBufferSizeChanged(UINT32 width, UINT32 height);
	void OnActivate(BOOL active);
#ifdef GADGET_SUPPORT
	void OnShow(BOOL show);
#endif // GADGET_SUPPORT
	void OnVisibilityChanged(BOOL vis);
};

#endif // DOCHAND_WINDOWLISTENER_H

