/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLUGIN_UNIFIED_OVERLAY_H_
#define PLUGIN_UNIFIED_OVERLAY_H_

#include "platforms/utilix/x11types.h"

class OpMouseListener;
class OpPluginWindowListener;
class PluginUnifiedWindowed;

/**
 * Plug-in overlay window.
 *
 * The intention of this window is to lay on top of the plug-in instance's
 * rectangle/window and intercept input. This forces users to "activate"
 * the plug-in rectangle/window in order to interact with it.
 */
class PluginUnifiedOverlay
{
public:
	static OpINT32HashTable<PluginUnifiedOverlay>* s_overlays;
	static void DeleteAllWindows();

	PluginUnifiedOverlay(PluginUnifiedWindowed* plugin_window) :
		m_is_blocking(false),
		m_motion_counter(0),
		m_activation_window(0),
		m_mouse_listener(0),
		m_window_listener(0),
		m_plugin_window(plugin_window),
		m_display(0) { }

	/**
	 * Create an overlay, i.e. stop input to our plugin.
	 *
	 * @param display  X11 display connection.
	 * @param parent   X11 id of the window containing the plug-in instance.
	 * @param width    Width of the plug-in instance.
	 * @param height   Height of the plug-in instance.
	 *
	 * @return See OpStatus.
	 */
	OP_STATUS Create(X11Types::Display* display, X11Types::Window parent, const int width, const int height);

	/// Destroy an overlay, i.e. let input to our plugin through.
	void Destroy();

	/// Return whether the overlay has been removed or not.
	bool IsDestroyed();

	/// Resize overlay to given width and height.
	void Resize(const int width, const int height);

	void SetMouseListener(OpMouseListener* listener);
	void SetWindowListener(OpPluginWindowListener* listener);

	void CrossingEvent(XEvent* xevent);
	void MouseButtonEvent(XEvent* xevent);
	void MouseMovementEvent(XEvent *xevent);
	void WindowChangedEvent(XEvent* xevent);

private:
	static bool PluginOverlayEventHandler(XEvent* xevent);

	bool m_is_blocking;
	int m_motion_counter;
	X11Types::Window m_activation_window;
	OpMouseListener* m_mouse_listener;
	OpPluginWindowListener* m_window_listener;
	PluginUnifiedWindowed* m_plugin_window;
	X11Types::Display* m_display;
};

inline bool PluginUnifiedOverlay::IsDestroyed()
{
	return !m_activation_window;
}

inline void PluginUnifiedOverlay::SetMouseListener(OpMouseListener* listener)
{
	m_mouse_listener = listener;
}

inline void PluginUnifiedOverlay::SetWindowListener(OpPluginWindowListener* listener)
{
	m_window_listener = listener;
}

#endif  // PLUGIN_UNIFIED_OVERLAY_H_
