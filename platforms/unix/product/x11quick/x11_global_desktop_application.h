/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon / Arjan van Leeuwen (arjanl)
 */

#ifndef X11_DESKTOP_GLOBAL_APPLICATION_H
#define X11_DESKTOP_GLOBAL_APPLICATION_H

#include "modules/util/adt/oplisteners.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_util/settings/SettingsListener.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_optaskbar.h"
#include "platforms/unix/base/x11/x11_sessionmanager.h"

class X11SystemTrayIcon;


class GlobalKeyEventListener
{
public:
	virtual bool OnGlobalKeyEvent(XEvent* e) = 0;
};


class X11DesktopGlobalApplication 
	: public DesktopGlobalApplication
	, public X11OpTaskbarListener
	, public X11SessionManagerListener
	, public SettingsListener
{
public:

	X11DesktopGlobalApplication();
	~X11DesktopGlobalApplication();

	/** Called when application starts */
	virtual void OnStart();

	/** Application has started the exit sequence, please ensure no new windows can be opened */
	virtual void ExitStarted();
	
	/** Tell system session manager (if any) user made a choice on exit */
	virtual BOOL OnConfirmExit(BOOL exit);

	/** Make the application exit */
	virtual void Exit();

	/** Called when application is being hidden */
	virtual void OnHide();

	/** Called when application is being shown */
	virtual void OnShow();

	/** Returns TRUE when tray icon is initialized, FALSE otherwise. */
	BOOL HasSystemTrayIcon() { return NULL==m_tray_icon ? FALSE : TRUE; }

	/** Platform specific functions */

	/**
	 * Set/Remove global key listener. Note: The first listener that consumes event will block other listeners
	 */
	OP_STATUS AddGlobalKeyEventListener(GlobalKeyEventListener* listener) 
		{ return m_global_keyevent_listener.Add(listener); }
	OP_STATUS RemoveGlobalKeyEventListener(GlobalKeyEventListener* listener) 
		{ return m_global_keyevent_listener.Remove(listener); }

	/**
	 * A filter function that deals with events that are sent to the application
	 * and not to a specific window. Event should not be used anymore if TRUE 
	 * is returned
	 */
	BOOL HandleEvent(XEvent* event );

	/**
	 * Determines if an input event should be blocked or not. Returns 
	 * TRUE if is should be blocked
	 */
	BOOL BlockInputEvent(XEvent* event, X11Widget* widget);

	/**
	 * [X11OpTaskbarListener]
	 * Sets the task bar attention mode for chat message
	 */
	void OnUnattendedChatCount( OpWindow* op_window, UINT32 count );

	/**
	 * [X11SessionManagerListener]
	 *  Called when the system session manager asks us to save application data
	 */
	void OnSaveYourself(X11SessionManager& sm);

	/**
	 * [X11SessionManagerListener]
	 * Called when the system session manager decided to cancel an ongoing shutdown
	 */
	void OnShutdownCancelled();

	/**
	 * [SettingsListener] 
	 * Called when a setting has changed
	 */
	void OnSettingsChanged(DesktopSettings* settings);

private:
	/**
	 * Applies updated color scheme
	 */
	void UpdateColorScheme();

private:
	X11SystemTrayIcon* m_tray_icon;
	X11OpTaskbar* m_taskbar;
	BOOL m_running;
	OpListeners<GlobalKeyEventListener> m_global_keyevent_listener;
};

#endif // X11_DESKTOP_GLOBAL_APPLICATION_H
