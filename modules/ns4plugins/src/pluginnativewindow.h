/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef PLUGINNATIVEWINDOW_H__
#define PLUGINNATIVEWINDOW_H__

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW

class OpPluginWindow;

#if defined(MDE_NATIVE_WINDOW_SUPPORT)
# include "modules/libgogi/pi_impl/mde_native_window.h"
#endif

/***********************************************************************************
 **
 **	PluginNativeWindow
 **
 ***********************************************************************************/

class PluginNativeWindow : public MDENativeWindow
{
public:
	PluginNativeWindow(const MDE_RECT &rect, OpPluginWindow *plugin_window, Plugin *plugin);

	virtual void MoveWindow(int x, int y);
	virtual void ResizeWindow(int w, int h);
	virtual void UpdateWindow() {}
	virtual void ShowWindow(BOOL show);
	virtual void SetRedirected(BOOL redir) {}
	virtual void UpdateBackbuffer(OpBitmap* backbuffer) {}
	virtual void SetClipRegion(MDE_Region* rgn);
	virtual BOOL RecreateBackbufferIfNeeded() { return FALSE; }
	virtual bool GetHitStatus(int x, int y) { return false; }
	void Attach();
	void Detach();

private:
	OpPluginWindow		*m_plugin_window;
	Plugin				*m_plugin;
	bool				m_send_onmouseover_on_next_mousemove;
};

#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW

#endif //  PLUGINNATIVEWINDOW_H__
