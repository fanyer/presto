/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Eirik Byrkjeflot Anonsen (eirik)
 */

#ifndef PLUGIN_WINDOW_TRACKER_H
#define PLUGIN_WINDOW_TRACKER_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "platforms/x11api/plugins/event_callback_list.h"
#include "platforms/x11api/plugins/plugin_unified_window.h"

class PluginWindowTracker
{
public:

	/** Get singleton instance
	 */
	static PluginWindowTracker& GetInstance();
	static void DeleteInstance();

	~PluginWindowTracker();

	void ReparentedPluginWindow(unsigned int child_window, unsigned int parent_window);
	OP_STATUS AddPluginWindow(unsigned int window, PluginUnifiedWindow* plugin_window);
	OP_STATUS RemovePluginWindow(unsigned int window);

	/**
	 * Registers a callback to be called on every XEvent for a
	 * particular window.
	 *
	 * @param win The X window ID of the window for which the callback
	 * should be called.
	 *
	 * @param cb The callback to call whenever an XEvent for the given
	 * window happens.  If the callback returns true, the event will
	 * not be processed further.
	 *
	 * @return OOM or OK
	 */
	virtual OP_STATUS registerX11EventCallback(uint32_t win, bool (*cb)(XEvent *));

	/**
	 * Stop calling the given callback for the given window.
	 *
	 * @param win The X window ID of the window for which
	 * the callback should no longer be called.
	 *
	 * @param cb The callback that should no longer be called.
	 * If 0, all callbacks registered on *win will be dropped.
	 */
	virtual void unregisterX11EventCallback(uint32_t win, bool (*cb)(XEvent *));

	PluginUnifiedWindow* GetPluginWindow(unsigned int window);

	/**
	 * Each XEvent processed by the platform must be passed to this function
	 * before it's processed and if 'true' is returned, no further processing
	 * should be done. Typically, the X11Client implementation can register itself
	 * in the event loop and then forward the events to this function.
	 */
	virtual bool HandleEvent(XEvent * event);

protected:
	inline PluginWindowTracker();
	static PluginWindowTracker* s_instance;
	OpINT32HashTable<PluginUnifiedWindow> m_plugin_windows;
	X11EventCallbackList m_x11_callbacks;
};

PluginWindowTracker::PluginWindowTracker() { }

#endif // NS4P_COMPONENT_PLUGINS
#endif // PLUGIN_WINDOW_TRACKER_H
