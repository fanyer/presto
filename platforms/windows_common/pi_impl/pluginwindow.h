/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSCOMMON_PLUGINWINDOW_H
#define WINDOWSCOMMON_PLUGINWINDOW_H

#ifdef _PLUGIN_SUPPORT_

#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/libgogi/pi_impl/mde_native_window.h"

class WindowsPluginNativeWindow : public MDENativeWindow
{
public:
	WindowsPluginNativeWindow(const MDE_RECT &rect, HWND win, HWND global_hwnd, HINSTANCE global_instance);
	~WindowsPluginNativeWindow();

	void SetParent(HWND parent);
	void DelayWindowDestruction() { m_hwnd = NULL; }
	HWND GetTransparentWindow() { return m_transwin; }

	virtual void MoveWindow(int x, int y);
	virtual void ResizeWindow(int w, int h);
	virtual void UpdateWindow();
	virtual void ShowWindow(BOOL show);
	virtual void SetRedirected(BOOL redir);
	virtual void UpdateBackbuffer(OpBitmap* backbuffer);
	virtual void SetClipRegion(MDE_Region* rgn);

	/**
	 * Call Updating(TRUE) before calling function that can trigger
	 * WM_WINDOWPOSCHANGING message or other similar that we want
	 * to handle only when call is made from Opera. Remember to call
	 * Updating(FALSE) after.
	 */
	void Updating(BOOL update) { m_updating = update; }
	/**
	 * Can be called inside window procedure for example to check
	 * if given message was triggered by Opera. Might be used to filter
	 * out messages sent by plugins that try to change position, size or
	 * visibility of our (plugin) windows.
	 */
	BOOL IsBeingUpdated() { return m_updating; }

private:
	HWND m_hwnd;

	HWND m_parent_hwnd;
	HINSTANCE m_global_instance;

	HWND m_transwin;
	HBITMAP m_native_backbuffer;
	HDC m_backbuffer_dc;
	HDC m_transwin_dc;
	int m_native_backbuffer_width;
	int m_native_backbuffer_height;

	BOOL m_reparent_to_transwin;
	BOOL m_updating;

	// Used to work around scrolling artifact when having gdi window as a child of directx
	// window
	static HBITMAP dummyBitmap;
	static HDC dummyDC;
};

#endif // _PLUGIN_SUPPORT_
#endif // WINDOWSCOMMON_PLUGINWINDOW_H
