/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/pi/x11_client.h"
#include "platforms/x11api/plugins/plugin_unified_windowed.h"
#include "platforms/x11api/utils/OpKeyToX11Keysym.h"

#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "platforms/utilix/x11_all.h"
#include "platforms/x11api/plugins/xembed.h"
#include "modules/pi/OpView.h"
#include "platforms/x11api/plugins/plugin_window_tracker.h"

#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/ns4plugins/src/plugin.h"

// static
OP_STATUS OpPlatformKeyEventData::Create(OpPlatformKeyEventData** key_event_data, void* data)
{
	XKeyEvent* xkeyevent = OP_NEW(XKeyEvent,());
	RETURN_OOM_IF_NULL(xkeyevent);
	op_memcpy(xkeyevent, data, sizeof(XKeyEvent));

	X11PlatformKeyEventData* event = OP_NEW(X11PlatformKeyEventData, (xkeyevent));
	RETURN_OOM_IF_NULL(event);
	*key_event_data = event;

	return OpStatus::OK;
}

// static
void OpPlatformKeyEventData::IncRef(OpPlatformKeyEventData* key_event_data)
{
	if (key_event_data)
		static_cast<X11PlatformKeyEventData*>(key_event_data)->m_ref_count ++;
}

// static
void OpPlatformKeyEventData::DecRef(OpPlatformKeyEventData* key_event_data)
{
	if (key_event_data)
	{
		X11PlatformKeyEventData* event = static_cast<X11PlatformKeyEventData*>(key_event_data);
		event->m_ref_count --;
		if (event->m_ref_count == 0)
			OP_DELETE(event);
	}
}



static X11Types::Atom Atom_XEMBED = -1;

static bool ActivatePluginWindow(XEvent* xevent)
{
	if (xevent->type != ClientMessage
	    || xevent->xclient.message_type != Atom_XEMBED
	    || xevent->xclient.format != 32
	    || xevent->xclient.data.l[1] != XEMBED_REQUEST_FOCUS)
		return false;

	PluginUnifiedWindowed* plugin_window = static_cast<PluginUnifiedWindowed*>(PluginWindowTracker::GetInstance().GetPluginWindow(xevent->xclient.window));
	if (!plugin_window)
		return false;

	// Delete the input blocking overlay
	plugin_window->SetMouseInputBlocked(false);

	// Now we simulate a mouse click event in
	// order to correctly set keyboard focus
	plugin_window->OnMouseDown();
	return true;
}

PluginUnifiedWindowed::PluginUnifiedWindowed(const OpRect& rect, int scale, OpView* parent_view, OpWindow* parent_window)
	: PluginUnifiedWindow(rect, scale, parent_view, parent_window)
	, m_mother_window(0)
	, m_plugin_window(0)
	, m_window_handle(0)
#ifdef MANUAL_PLUGIN_ACTIVATION
	, m_activation_window(this)
#else
	, m_listener(0)
#endif  // MANUAL_PLUGIN_ACTIVATION
{
	OP_NEW_DBG("PluginUnifiedWindowed::PluginUnifiedWindow()", "plugix.window");
	OP_DBG(("this: %p", this));
}

PluginUnifiedWindowed::~PluginUnifiedWindowed()
{
	if (m_window_handle != None)
	{
		// Remove us from the manager table
		PluginWindowTracker::GetInstance().RemovePluginWindow(m_window_handle);

		/**
		 * The plugin plug/socket are in use by GTK and shouldn't be destroyed
		 * here. We reparent them so that XDestroyWindow doesn't kill them. This
		 * prevents "GdkWindow unexpectedly destroyed" messages.
		 */
		if (m_plugin_window)
		{
			XUnmapWindow(m_display, m_plugin_window);
			XReparentWindow(m_display, m_plugin_window, DefaultRootWindow(m_display), 0, 0);
			XFlush(m_display); // Let server receive message right away so that unmap is executed
		}
		XDestroyWindow(m_display, m_window_handle);
	}
#ifdef MANUAL_PLUGIN_ACTIVATION
	m_activation_window.Destroy();
#endif
	XDestroyWindow(m_display, m_mother_window);
}

OpMouseListener* PluginUnifiedWindowed::GetMouseListenerForOpView(OpView* opview)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	MDE_OpView* mde_opview = static_cast<MDE_OpView*>(opview);
	MDE_Widget* mde_widget = mde_opview->GetMDEWidget();
	return mde_widget->GetMouseListener();
#else
	X11OpView* x11_opview = static_cast<X11OpView*>(opview);
	return x11_opview->GetMouseListener();
#endif
}

OP_STATUS PluginUnifiedWindowed::Init(X11Types::Display* dpy)
{
	m_display = dpy;
	X11Types::Window toplevel_window = X11Client::Instance()->GetTopLevelWindow(m_parent_window);
	const int screen = X11Client::Instance()->GetScreenNumber(toplevel_window);

	if (Atom_XEMBED == static_cast<X11Types::Atom>(-1))
		Atom_XEMBED = XInternAtom(m_display, "_XEMBED", False);

	XSetWindowAttributes attr;
	int attr_flags = 0;
	attr.bit_gravity = NorthWestGravity;
	attr.colormap = DefaultColormap(m_display, screen);
	attr.border_pixel = BlackPixel(m_display, screen);

	attr.event_mask =
		ExposureMask |
		KeyPressMask |
		KeyReleaseMask |
		ButtonPressMask |
		ButtonReleaseMask |
		PointerMotionMask |
		FocusChangeMask |
		LeaveWindowMask |
		StructureNotifyMask |
		PropertyChangeMask;

	attr_flags |= CWBitGravity | CWEventMask | CWColormap | CWBorderPixel;
	m_mother_window = XCreateWindow(m_display,
		toplevel_window,
		m_plugin_rect.x, m_plugin_rect.y,
		m_plugin_rect.width, m_plugin_rect.height,
		0,
		DefaultDepth(m_display, screen),
		InputOutput,
		DefaultVisual(m_display, screen),
		attr_flags,
		&attr);

	if (m_mother_window == None)
		return OpStatus::ERR;

	m_window_handle = XCreateWindow(m_display,
	                                m_mother_window,
	                                0, 0,
	                                m_plugin_rect.width, m_plugin_rect.height,
	                                0,
	                                DefaultDepth(m_display, screen),
	                                InputOutput,
	                                DefaultVisual(m_display, screen),
	                                attr_flags,
	                                &attr);
	if (!m_window_handle)
		return OpStatus::ERR;

	XStoreName(m_display, m_window_handle, "Opera: Plug-in window");
	XMapWindow(m_display, m_window_handle);
	XSync(m_display, False);

#ifdef MANUAL_PLUGIN_ACTIVATION
	CreateActivationWindow();
	m_activation_window.SetMouseListener(GetMouseListenerForOpView(m_plugin_parent));
#endif

	// Add us to the manager table
	PluginWindowTracker::GetInstance().AddPluginWindow(m_window_handle, this);
	RETURN_IF_ERROR(PluginWindowTracker::GetInstance().registerX11EventCallback(static_cast<unsigned int>(m_window_handle), ActivatePluginWindow));

	return OpStatus::OK;
}

void PluginUnifiedWindowed::Detach()
{
	OP_ASSERT(m_mother_window != None);
	XUnmapWindow(m_display, m_mother_window);

	X11Types::Window root = RootWindow(m_display, X11Client::Instance()->GetScreenNumber(m_mother_window));
	XReparentWindow(m_display, m_mother_window, root, 0, 0);

	m_plugin_parent = NULL;
	PluginWindowTracker::GetInstance().unregisterX11EventCallback(static_cast<unsigned int>(m_window_handle), 0);
}

void PluginUnifiedWindowed::Hide()
{
	XUnmapWindow(m_display, m_mother_window);
}

void PluginUnifiedWindowed::Show()
{
	XMapWindow(m_display, m_mother_window);
}

void PluginUnifiedWindowed::Reparent()
{
	XReparentWindow(m_display, m_mother_window, X11Client::Instance()->GetTopLevelWindow(m_parent_window), m_plugin_rect.x, m_plugin_rect.y);
}

void PluginUnifiedWindowed::SetMouseInputBlocked(const bool block)
{
#ifdef MANUAL_PLUGIN_ACTIVATION
	if (block)
		CreateActivationWindow();
	else
	{
		OP_ASSERT(m_plugin_object || !"PluginUnifiedWindowed::SetMouseInputBlocked: Missing plug-in object");
		OP_ASSERT(m_plugin_object->GetHtmlElement() || !"PluginUnifiedWindowed::SetMouseInputBlocked: Missing plug-in HTML element");
		m_activation_window.Destroy();
		m_plugin_object->GetHtmlElement()->SetPluginActivated(TRUE);
	}
#endif
}

bool PluginUnifiedWindowed::IsMouseInputBlocked()
{
#ifdef MANUAL_PLUGIN_ACTIVATION
	return !m_activation_window.IsDestroyed();
#else
	return false;
#endif
}

void PluginUnifiedWindowed::SetPos(const int x, const int y)
{
	PluginUnifiedWindow::SetPos(x, y);
	XMoveWindow(m_display, m_mother_window, x, y);
}

void PluginUnifiedWindowed::SetSize(const int w, const int h)
{
	PluginUnifiedWindow::SetSize(w, h);
	XResizeWindow(m_display, m_mother_window, w, h);
#ifdef MANUAL_PLUGIN_ACTIVATION
	m_activation_window.Resize(m_plugin_rect.width, m_plugin_rect.height);
#endif
	XResizeWindow(m_display, m_window_handle, w, h);
	XResizeWindow(m_display, m_plugin_window, w, h);
}

BOOL PluginUnifiedWindowed::SendEvent(OpPlatformEvent* event)
{
	OP_ASSERT(event || !"PluginUnifiedWindowed::SendEvent: No event to send");

	XEvent* xevent = static_cast<XEvent*>(event->GetEventData());

	/* Silently ignore dummy events for core KEYPRESS, see PluginUnifiedWindow::CreateKeyEvent(). */
	if (xevent == NULL)
		return TRUE;

	int max_levels = -1;
	switch (xevent->type)
	{
		case KeyPress:
		case KeyRelease:
			max_levels = 1;
			break;
		case ButtonPress:
			break;
		case MotionNotify:
			max_levels = 4;
			break;
		case EnterNotify:
		case LeaveNotify:
			break;
		case FocusIn:
		case FocusOut:
		case ClientMessage:
			max_levels = 1;
			break;
		default:
			OP_ASSERT(!"PluginUnifiedWindowed::SendEvent: Unhandled XEvent");
			return FALSE;
	}

	BroadcastEvent(m_display, m_plugin_window, xevent, max_levels);
	XSync(m_display, False);

	return TRUE;
}

void PluginUnifiedWindowed::OnPluginWindowAdded(const unsigned int window, const bool)
{
	if (m_plugin_window)
		return;

	XMapWindow(m_display, window);
	XSync(m_display, False);
	m_plugin_window = window;

	// TODO (copied over from old code --tommyn)
	// In accordance with the XEmbed spec the embedder should listen for changes in the
	// _XEMBED_INFO.flags - currently to see if the embedee would like to be mapped/unmapped.
	// This will in that case be indicated in the XEMBED_MAPPED bit which will be set
	// to 1. This code here will make sure we get PropertyNotify events on the window
	// so that we can check to see if the flags have been altered.
}

OP_STATUS PluginUnifiedWindowed::FocusEvent(OpPlatformEvent** event, const bool got_focus)
{
#ifdef MANUAL_PLUGIN_ACTIVATION
	if (got_focus)
		SetMouseInputBlocked(!got_focus);
#endif  // MANUAL_PLUGIN_ACTIVATION

	XEvent xevent;

	xevent.type                 = ClientMessage;
	xevent.xclient.serial       = 0;
	xevent.xclient.send_event   = False;
	xevent.xclient.display      = m_display;
	xevent.xclient.window       = GetWindowHandle();
	xevent.xclient.message_type = Atom_XEMBED;
	xevent.xclient.format       = 32;
	xevent.xclient.data.l[0]    = X11Constants::XCurrentTime;
	xevent.xclient.data.l[1]    = got_focus ? XEMBED_WINDOW_ACTIVATE : XEMBED_WINDOW_DEACTIVATE;
	xevent.xclient.data.l[2]    = 0;
	xevent.xclient.data.l[3]    = 0;
	xevent.xclient.data.l[4]    = 0;

	OpUnixPlatformEvent activate_event(xevent);
	SendEvent(&activate_event);

	if (got_focus)
	{
		xevent.xclient.data.l[1] = XEMBED_FOCUS_IN;
		xevent.xclient.data.l[2] = XEMBED_FOCUS_CURRENT;
	}
	else
		xevent.xclient.data.l[1] = XEMBED_FOCUS_OUT;

	*event = OP_NEW(OpUnixPlatformEvent, (xevent));
	if (!*event)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS PluginUnifiedWindowed::KeyEvent(OpPlatformEvent** event, OpKey::Code key, const uni_char* key_value, const bool pressed, OpKey::Location location, const ShiftKeyState shift_key)
{
	XEvent key_event;
	key_event.xkey.display = m_display;
	key_event.xkey.window = X11Client::Instance()->GetTopLevelWindow(m_parent_window);

	unsigned int count = 0;
	X11Types::Window parent, *children;
	if (!XQueryTree(key_event.xkey.display, key_event.xkey.window, &key_event.xkey.root, &parent, &children, &count))
		return OpStatus::ERR;

	if (children)
		XFree(children);

	XWindowAttributes attr;
	if (!XGetWindowAttributes(key_event.xkey.display, key_event.xkey.window, &attr))
		return OpStatus::ERR;

	UINT64 state, keycode;
	X11Client::Instance()->GetKeyEventPlatformData(key, pressed, shift_key, state, keycode);

	key_event.type              = pressed ? KeyPress: KeyRelease;
	key_event.xkey.serial       = LastKnownRequestProcessed(key_event.xkey.display);
	key_event.xkey.send_event   = False;
	key_event.xkey.subwindow    = None;
	key_event.xkey.time         = X11Constants::XCurrentTime;
	key_event.xkey.x            = 0;
	key_event.xkey.y            = 0;
	key_event.xkey.x_root       = attr.x + attr.border_width;
	key_event.xkey.y_root       = attr.y + attr.border_width;
	key_event.xkey.state        = state;
	key_event.xkey.keycode      = keycode;
	key_event.xkey.keycode      = XKeysymToKeycode(key_event.xkey.display, OpKeyToX11Keysym(key, location, shift_key, key_value));
	key_event.xkey.same_screen  = True;

	*event = OP_NEW(OpUnixPlatformEvent, (key_event));
	if (!*event)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

#ifdef MANUAL_PLUGIN_ACTIVATION

void PluginUnifiedWindowed::CreateActivationWindow()
{
	/**
	 * Plug-ins that need activation should not receive events (in particular
	 * mouse events) until the user has clicked in the window.  The solution
	 * is to create an InputOnly window as a sibling of 'm_window_handle', and
	 * on top of it. That window will then self-destruct when it is first
	 * clicked on.
	 */
	OpStatus::Ignore(m_activation_window.Create(
		m_display,
		m_mother_window,
		m_plugin_rect.width,
		m_plugin_rect.height));
}

#endif  // MANUAL_PLUGIN_ACTIVATION

void PluginUnifiedWindowed::BroadcastEvent(X11Types::Display* display, X11Types::Window& window, XEvent* xevent, const int max_depth)
{
	OP_NEW_DBG("PluginUnifiedWindowed::BroadcastEvent","ns4p.input.keys");
	if(!xevent || !window || max_depth == 0)
		return;

	long event_mask = 0;
	unsigned int num_children = 0;

	xevent->xany.window = window;
	switch (xevent->type)
	{
		case KeyPress:
			OP_DBG(("Preparing KeyPress event"));
			event_mask = KeyPressMask;
			break;
		case KeyRelease:
			OP_DBG(("Preparing KeyRelease event"));
			event_mask = KeyReleaseMask;
			break;
		case ButtonPress:
			event_mask = ButtonPressMask;
			break;
		case ButtonRelease:
			event_mask = ButtonReleaseMask;
			break;
		case MotionNotify:
			event_mask = PointerMotionMask;
			break;
		case EnterNotify:
			event_mask = EnterWindowMask;
			break;
		case LeaveNotify:
			event_mask = LeaveWindowMask;
			break;
		case FocusIn:
			event_mask = FocusChangeMask;
			break;
		case FocusOut:
			event_mask = FocusChangeMask;
			break;
		case ClientMessage:
			event_mask = NoEventMask;
			break;
		default:
			OP_ASSERT(!"PluginUnifiedWindowed::BroadcastEvent: Unhandled event");
			return;
	}

	OP_DBG(("Sending XEvent with type=%d", xevent->type));
	// Send the event to the window:
	XSendEvent(display, window, False, event_mask, xevent);

	// Send it to entire subtree:
	X11Types::Window *children = 0;
	X11Types::Window w_parent = 0;
	X11Types::Window w_root = 0;
	if (XQueryTree(display, window, &w_root, &w_parent, &children, &num_children) != 0)
	{
		for (unsigned int i = 0; i < num_children; ++i)
			BroadcastEvent(display, children[i], xevent, max_depth - 1);

		if (children)
			XFree(children);
	}
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
