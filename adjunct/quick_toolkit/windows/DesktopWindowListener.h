/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DESKTOP_WINDOW_LISTENER_H
#define DESKTOP_WINDOW_LISTENER_H

class DesktopWindow;

enum DesktopWindowStatusType
{
	STATUS_TYPE_ALL = 0,
	STATUS_TYPE_TITLE,
	STATUS_TYPE_INFO,
	STATUS_TYPE_TOOLBAR,    // Field text shall not be updated on page changes
	STATUS_TYPE_TOTAL		// must come last
};

class DesktopWindowListener
{
	public:
		virtual ~DesktopWindowListener() {}

		virtual void	OnDesktopWindowChanged(DesktopWindow* desktop_window) {};
		virtual void	OnDesktopWindowStatusChanged(DesktopWindow* desktop_window, DesktopWindowStatusType type) {};
		virtual void	OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active) {};
		virtual void	OnDesktopWindowLowered(DesktopWindow* desktop_window) {};
		virtual void	OnDesktopWindowAttention(DesktopWindow* desktop_window, BOOL attention) {};
		virtual BOOL	OnDesktopWindowIsCloseAllowed(DesktopWindow* desktop_window) {return TRUE;}
		virtual void	OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated) {};
		virtual void	OnDesktopWindowParentChanged(DesktopWindow* desktop_window) {};
		virtual void	OnDesktopWindowBrowserViewChanged(DesktopWindow* desktop_window) {};
		virtual void	OnDesktopWindowMoved(DesktopWindow* desktop_window) {};
		virtual void	OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height) {};
		virtual void	OnDesktopWindowShown(DesktopWindow* desktop_window, BOOL shown) {};
		virtual void    OnDesktopWindowPageChanged(DesktopWindow* desktop_window) {};
		virtual void    OnDesktopWindowAnimationRectChanged(DesktopWindow* desktop_window, const OpRect &source, const OpRect &target) {}
				
		/** Called when the content of this desktop window has changed, so that others relying on content
		  * (e.g. thumbnails) should update
		  */
		virtual void	OnDesktopWindowContentChanged(DesktopWindow* desktop_window) {};
};

#endif // DESKTOP_WINDOW_LISTENER_H
