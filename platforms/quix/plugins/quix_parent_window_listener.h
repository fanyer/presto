/**
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "platforms/x11api/pi/plugixparentwindowlistener.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"

class QuixParentWindowListener
	: public PlugixParentWindowListener
	, public DesktopWindowListener
{
public:
	QuixParentWindowListener(UnixOpPluginAdapter& adapter) : m_adapter(adapter), m_parent_window(NULL) {}
	virtual ~QuixParentWindowListener();

	// From PlugixParentWindowListener
	virtual OP_STATUS SetParent(OpWindow* parent);

	// From DesktopWindowListener
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	virtual void OnDesktopWindowParentChanged(DesktopWindow* desktop_window);
private:
	UnixOpPluginAdapter& m_adapter;
	DesktopWindow* m_parent_window;
};
