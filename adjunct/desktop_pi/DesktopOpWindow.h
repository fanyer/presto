/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef DESKTOP_OP_WINDOW_H
#define DESKTOP_OP_WINDOW_H

#include "modules/pi/OpWindow.h"
#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#endif // VEGA_OPPAINTER_SUPPORT

#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"

class OpBrowserView;
class OpPageView;
class DesktopWindow;

/** @brief OpWindow functions needed in all Desktop implementations
  * OpWindow implementations on desktop platforms should inherit from this class
  */
class DesktopOpWindow : 
#ifdef VEGA_OPPAINTER_SUPPORT
	public MDE_OpWindow
#else
	public OpWindow
#endif
{
public:
	DesktopOpWindow() : m_browser_view(0), m_desktop_window(0) {}

	/** Create a window */
	static OP_STATUS Create(DesktopOpWindow** new_window);

	/** Set whether this OpWindow is visible
	  * @param visibility Whether this OpWindow is visible or completely invisible (ie.
	  *        completely covered by another window)
	  *
	  * Platform implementors should call OnVisibilityChanged() in their OpWindowListener
	  * when receiving this call.
	  */
	virtual void OnVisibilityChanged(BOOL visibility) = 0;

	/** Set whether this OpWindow has been locked (can't be closed) by user
	  * @param locked_by_user Whether this OpWindow is locked by the user
	  */
	virtual void SetLockedByUser(BOOL locked_by_user) = 0;

	/** Utility function to detect when the pagebar is on top so the frame can be extended. */
	virtual void OnPagebarMoved(BOOL onTop, int height){}
	virtual void OnTransparentAreaChanged(int top, int bottom, int left, int right){}

	// Called when the option "Full Transparency" is set on the skin
	virtual void SetFullTransparency(BOOL full_transparency) {};

	/**
	 * Request window to show or hide the window manager decoration
	 *
	 * @param show_decoration Show decoration where TRUE, otherwise hide it
	 */
	virtual void SetShowWMDecoration(BOOL show_decoration) {}

	/**
	 * Return TRUE if window has a decoration, otherwise FALSE
	 */
	virtual BOOL HasWMDecoration() { return TRUE; }

	/**
	 * Determine if external compositing is available. All platforms supports
	 * this except for X11 when a compositing manager is not present.
	 *
	 * @return TRUE or FALSE depending on environment where the window is shown 
	 */
	virtual BOOL SupportsExternalCompositing() { return TRUE; }

	/**
	 * Making it possible for platforms to optimize calls to SetOuterPos and SetOuterSize if wanted.
	 * The default implemention is fallback to separate calls.
	 * @param x For SetOuterPos
	 *        y For SetOuterPos
	 *        width For SetOuterSize
	 *        height For SetOuterSize
	 */
	virtual void SetOuterPosAndSize(INT32 x, INT32 y, UINT32 width, UINT32 height) { SetOuterPos(x, y);SetOuterSize(width, height); }

	/**
	 * Making it possible for platforms to optimize calls to SetInnerPos and SetInnerSize if wanted.
	 * The default implemention is fallback to separate calls.
	 * @param x For SetInnerPos
	 *        y For SetInnerPos
	 *        width For SetInnerSize
	 *        height For SetInnerSize
	 */
	virtual void SetInnerPosAndSize(INT32 x, INT32 y, UINT32 width, UINT32 height) { SetInnerPos(x, y);SetInnerSize(width, height); }

	void SetBrowserView(OpBrowserView* browser_view) { m_browser_view = browser_view; }
	OpBrowserView* GetBrowserView() const { return m_browser_view; }

	void SetDesktopWindow(DesktopWindow* w) { m_desktop_window = w; }
	DesktopWindow *GetDesktopWindow() { return m_desktop_window; }

	virtual void Fullscreen() {}	// get rid of the assert

	/**
	 *
	 * @param out List of menubar menus 
	 *
	 */
	virtual BOOL GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out) { return FALSE; }

private:
	OpBrowserView* m_browser_view;
	DesktopWindow* m_desktop_window;
};

#endif // DESKTOP_OP_WINDOW_H
