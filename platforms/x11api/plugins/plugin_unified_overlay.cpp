/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)
#ifdef MANUAL_PLUGIN_ACTIVATION

#include "modules/pi/OpView.h"
#include "platforms/x11api/plugins/plugin_window_tracker.h"
#include "platforms/x11api/plugins/plugin_unified_overlay.h"
#include "platforms/x11api/plugins/plugin_unified_windowed.h"
#include "platforms/utilix/x11_all.h"


OpINT32HashTable<PluginUnifiedOverlay>* PluginUnifiedOverlay::s_overlays = 0;

void PluginUnifiedOverlay::DeleteAllWindows()
{
	if (!s_overlays)
		return;

	s_overlays->DeleteAll();
	OP_DELETE(s_overlays);
	s_overlays = 0;
}

OP_STATUS PluginUnifiedOverlay::Create(X11Types::Display* display, X11Types::Window parent, const int width, const int height)
{
	if (m_activation_window || !display)
		return OpStatus::ERR;

	if (!s_overlays)
	{
		s_overlays = OP_NEW(OpINT32HashTable<PluginUnifiedOverlay>, ());
		RETURN_OOM_IF_NULL(s_overlays);
	}

	XSetWindowAttributes attr;
	op_memset(&attr, 0, sizeof(XSetWindowAttributes));
	attr.event_mask = ButtonPressMask | ButtonReleaseMask | StructureNotifyMask | PointerMotionMask;
	attr.do_not_propagate_mask = ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;

	X11Types::Window activation_window = XCreateWindow(display, parent, 0, 0, width, height, 0,
	                                        CopyFromParent,
	                                        InputOnly,
	                                        CopyFromParent,
	                                        CWEventMask | CWDontPropagate,
	                                        &attr);
	if (!activation_window)
		return OpStatus::ERR;

	OP_STATUS status = PluginWindowTracker::GetInstance().registerX11EventCallback(activation_window, PluginOverlayEventHandler);
	if (OpStatus::IsError(status)
	    || OpStatus::IsError(status = s_overlays->Add(activation_window, this)))
	{
		XDestroyWindow(display, activation_window);
		return status;
	}

	m_activation_window = activation_window;
	m_display = display;

	XStoreName(m_display, m_activation_window, "Opera: Plug-in activation window");
	XMapRaised(m_display, m_activation_window);
	m_is_blocking = true;

	return OpStatus::OK;
}

void PluginUnifiedOverlay::Destroy()
{
	if (!m_activation_window)
		return;

	PluginWindowTracker::GetInstance().unregisterX11EventCallback(m_activation_window, 0);

	XDestroyWindow(m_display, m_activation_window);

	PluginUnifiedOverlay* removed_window = 0;
	s_overlays->Remove(m_activation_window, &removed_window);

	OP_ASSERT(removed_window == this || !"PluginUnifiedOverlay::Destroy: Failed to remove plugin activation window from list");

	m_activation_window = X11Constants::XNone;
}

void PluginUnifiedOverlay::Resize(const int width, const int height)
{
	if (!m_activation_window)
		return;

	XResizeWindow(m_display, m_activation_window, width, height);
}

void PluginUnifiedOverlay::CrossingEvent(XEvent* xevent)
{
	if (m_is_blocking)
		return;

	OpUnixPlatformEvent event(*xevent);
	m_plugin_window->SendEvent(&event);
}

void PluginUnifiedOverlay::MouseButtonEvent(XEvent* xevent)
{
	OP_ASSERT(m_mouse_listener || !"PluginUnifiedOverlay::MouseButtonEvent: Missing mouse listener");
	OP_ASSERT(m_window_listener || !"PluginUnifiedOverlay::MouseButtonEvent: Missing plugin window listener");

	switch (xevent->xbutton.button)
	{
		case Button1:
		case Button2:
		case Button3:
			if (xevent->type != ButtonPress)
			{
				OP_ASSERT(xevent->type == ButtonRelease
				          || !"PluginUnifiedOverlay::MouseButtonEvent: ButtonRelease event expected");
				if (m_window_listener)
					m_window_listener->OnMouseUp();
			}
			else
			{
				if (m_window_listener)
					m_window_listener->OnMouseDown();
				if (m_is_blocking)
					m_is_blocking = false;
				else
				{
					OpUnixPlatformEvent event(*xevent);
					m_plugin_window->SendEvent(&event);
				}
			}
			break;

		case Button4:
		case Button5:
			if (m_mouse_listener && xevent->type == ButtonPress)
			{
				ShiftKeyState shift_state = 0;
				if (xevent->xbutton.state & ShiftMask)
					shift_state |= SHIFTKEY_SHIFT;
				if (xevent->xbutton.state & ControlMask)
					shift_state |= SHIFTKEY_CTRL;
				if (xevent->xbutton.state & Mod1Mask)
					shift_state |= SHIFTKEY_ALT;
				m_mouse_listener->OnMouseWheel(xevent->xbutton.button == Button4 ? -1 : 1, TRUE, shift_state);
			}
			break;
		default:
			break;
	}
}

void PluginUnifiedOverlay::MouseMovementEvent(XEvent *xevent)
{
	if (m_is_blocking)
	{
		if (m_window_listener)
			m_window_listener->OnMouseHover();
	}
	else
	{
		// Comment from old code, not sure if it still applies:
		//
		// Pass only motion events when SendMouseEvent says so, it is
		// modulated by the calls to IncMotionCounter() and a factor
		// (currently 5). This limits the propagation for performance
		// reasons; it is dead slow if you send all of them.
		if ((m_motion_counter % 5) == 0)
		{
			OpUnixPlatformEvent event(*xevent);
			m_plugin_window->SendEvent(&event);
		}
	}
	++m_motion_counter;
}

void PluginUnifiedOverlay::WindowChangedEvent(XEvent* xevent)
{
	if (xevent->xconfigure.above == None)
		XRaiseWindow(xevent->xconfigure.display, xevent->xconfigure.window);
}

bool PluginUnifiedOverlay::PluginOverlayEventHandler(XEvent* xevent)
{
	PluginUnifiedOverlay* overlay = 0;
	if (xevent->type == EnterNotify || xevent->type == LeaveNotify)
		s_overlays->GetData(xevent->xcrossing.subwindow, &overlay);
	else
		s_overlays->GetData(xevent->xany.window, &overlay);
	if (!overlay)
		return false;

	switch (xevent->type)
	{
		case ButtonPress:
		case ButtonRelease:
			overlay->MouseButtonEvent(xevent);
			break;
		case MotionNotify:
			overlay->MouseMovementEvent(xevent);
			break;
		case EnterNotify:
		case LeaveNotify:
			overlay->CrossingEvent(xevent);
			break;
		case ConfigureNotify:
			overlay->WindowChangedEvent(xevent);
			break;
		default:
			break;
	}
	return true;
}

#endif  // MANUAL_PLUGIN_ACTIVATION
#endif // X11API && NS4P_COMPONENT_PLUGINS
