/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/utilix/OpGtk.h"
#include <X11/Xregion.h>

using X11Types::Colormap;
using X11Types::Display;
using X11Types::Region;
using X11Types::Visual;

#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/pi/OpPluginTranslator.h"
#include "modules/pi/OpPluginImage.h"
#include "modules/opdata/UniStringUTF8.h"

#include "platforms/x11api/utils/OpKeyToX11Keysym.h"
#include "platforms/x11api/plugins/unix_opplugintranslator.h"
#include "platforms/x11api/plugins/unix_opplatformevent.h"
#include "platforms/x11api/plugins/xembed.h"
#include "platforms/x11api/plugins/unix_oppluginimage.h"
#include "platforms/x11api/src/generated/g_message_x11api_messages.h"
#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"

OP_STATUS OpPluginTranslator::Create(OpPluginTranslator** translator, const OpMessageAddress& instance, const OpMessageAddress* address, const NPP)
{
	OP_ASSERT(address || !"OpPluginTranslator::Create: Missing address");

	// Request window information from the browser component
	OpBrowserWindowInformationMessage* msg = OpBrowserWindowInformationMessage::Create();
	RETURN_OOM_IF_NULL(msg);

	OpSyncMessenger sync(g_component->CreateChannel(*address));
	RETURN_IF_ERROR(sync.SendMessage(msg));
	OpChannel* channel = sync.TakePeer();

	// Get response message from browser component
	OpBrowserWindowInformationResponseMessage* response = OpBrowserWindowInformationResponseMessage::Cast(sync.GetResponse());
	if (!response)
		return OpStatus::ERR;

	// Retrieve X11 display
	char* display_name = 0;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&display_name, response->GetDisplayName()));

	X11Types::Display* display = XOpenDisplay(display_name);
	if (!display)
		return OpStatus::ERR;

	XVisualInfo vis_info;
	if (!XMatchVisualInfo(display, response->GetScreenNumber(), UnixOpPluginImage::kDepth, TrueColor, &vis_info))
	{
		XCloseDisplay(display);
		return OpStatus::ERR;
	}

	// Create the plugin translator
	OpAutoPtr<UnixOpPluginTranslator> plugin_translator(OP_NEW(UnixOpPluginTranslator, (display, vis_info.visual, response->GetScreenNumber(), response->GetWindow(), channel)));
	if (!plugin_translator.get())
	{
		XCloseDisplay(display);
		return OpStatus::ERR_NO_MEMORY;
	}
	// display is now owned by translator and does not need to be closed.

	OP_ASSERT(channel->IsDirected());
	RETURN_IF_ERROR(channel->AddMessageListener(plugin_translator.get()));
	RETURN_IF_ERROR(channel->Connect());
	*translator = plugin_translator.release();
	return OpStatus::OK;
}

UnixOpPluginTranslator::UnixOpPluginTranslator(X11Types::Display* display, X11Types::Visual* visual, const int screen_number, const X11Types::Window window, OpChannel* channel)
	: m_screen_number(screen_number)
	, m_browser_window(window)
	, m_gtksocket(NULL)
	, m_gtkplug(NULL)
	, m_channel(channel)
{
	m_window_info.type     = NP_SETWINDOW;
	m_window_info.display  = display;
	m_window_info.visual   = visual;
	m_window_info.colormap = DefaultColormap(display, m_screen_number);
	m_window_info.depth    = DefaultDepth(display, m_screen_number);
}

UnixOpPluginTranslator::~UnixOpPluginTranslator()
{
	/* TODO: we should probably free m_gtkplug here and possibly the m_gtksocket as well, but we must do that without causing:
	 *   (<unknown>:24013): GLib-GObject-CRITICAL **: g_object_unref: assertion `G_IS_OBJECT (object)' failed
	 * when a playing youtube video is reloading using CTRL-R. Chrome currently shows this g_log error though. */
	XCloseDisplay(m_window_info.display);

	if (m_channel)
		/* We are listening to this channel, and must remove ourselves from the list of
		 * listeners. Incidentally we own it too, so we might as well just delete it. */
		OP_DELETE(m_channel);
}

OP_STATUS UnixOpPluginTranslator::FinalizeOpPluginImage(OpPluginImageID, const NPWindow&)
{
	// This *must* be done or we'll get a blank frame.
	X11Types::Display* display = OpGtk::gdk_x11_display_get_xdisplay(OpGtk::gdk_display_get_default());
	if (display && display != m_window_info.display)
		XSync(display, False);

	return OpStatus::OK;
}

bool UnixOpPluginTranslator::ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect)
{
	if (invalidRegion->numRects == 0)
		return false;

	invalidRect.top = invalidRegion->rects[0].y1;
	invalidRect.left = invalidRegion->rects[0].x1;
	invalidRect.bottom = invalidRegion->rects[0].y2;
	invalidRect.right = invalidRegion->rects[0].x2;

	for (int i = 1; i < invalidRegion->numRects; ++i)
	{
		if (invalidRegion->rects[i].y1 < invalidRect.top)
			invalidRect.top = invalidRegion->rects[i].y1;
		if (invalidRegion->rects[i].x1 < invalidRect.left)
			invalidRect.left = invalidRegion->rects[i].x1;
		if (invalidRegion->rects[i].y2 < invalidRect.bottom)
			invalidRect.bottom = invalidRegion->rects[i].y2;
		if (invalidRegion->rects[i].x2 < invalidRect.right)
			invalidRect.right = invalidRegion->rects[i].x2;
	}

	return true;
}

OP_STATUS UnixOpPluginTranslator::UpdateNPWindow(NPWindow& out_npwin, const OpRect& rect, const OpRect& clip_rect, NPWindowType type)
{
	switch (type)
	{
		case NPWindowTypeWindow:
			if (!m_gtksocket)
			{
				/**
				 * We create a window, using the window created by
				 * PluginUnifiedWindow as parent, and attach a GtkSocket to it in
				 * order for the plugin to create windows.
				 *
				 * Ideally, we would create a socket in PluginUnifiedWindow and
				 * simply forward it to the plugin.
				 */
				GdkNativeWindow parent = reinterpret_cast<uintptr_t>(out_npwin.window);

				m_gtkplug = OpGtk::gtk_plug_new(parent);
				OpGtk::gtk_widget_set_size_request(m_gtkplug, rect.width, rect.height);
				OpGtk::gtk_widget_show(m_gtkplug);

				m_gtksocket = OpGtk::gtk_socket_new();
				OpGtk::gtk_widget_set_size_request(m_gtksocket, rect.width, rect.height);
				OpGtk::gtk_widget_show(m_gtksocket);
				GTK_WIDGET_SET_FLAGS(m_gtksocket, GTK_CAN_FOCUS);
				GTK_WIDGET_SET_FLAGS(m_gtksocket, GTK_HAS_DEFAULT);
				OpGtk::gtk_container_add(GTK_CONTAINER(m_gtkplug), m_gtksocket);

				OpGtk::g_signal_connect(m_gtkplug, "destroy", G_CALLBACK(OpGtk::gtk_widget_destroyed), &m_gtkplug);

				/**
				 * We need to intercept plug_removed signal to prevent the
				 * socket from being destroyed because the plug won't be
				 * notified of its demise, causing a double-delete.
				 */
				OpGtk::g_signal_connect(m_gtksocket, "plug_removed", G_CALLBACK(OpGtk::gtk_true), NULL);

				XStoreName(m_window_info.display, GDK_WINDOW_XID(m_gtkplug->window), "Opera: Plug-in plug");
				XStoreName(m_window_info.display, GDK_WINDOW_XID(m_gtksocket->window), "Opera: Plug-in socket");

				XReparentWindow(m_window_info.display,
								GDK_WINDOW_XID(m_gtkplug->window),
								parent,
								0, 0);
				XSync(m_window_info.display, False);
				//printf("[plugin] %lx -> %lx -> %lx\n", parent, GDK_WINDOW_XID(m_gtkplug->window), GDK_WINDOW_XID(m_gtksocket->window));

				// Notify browser process that the plugin has attached its window.
				if (m_channel)
				{
					OpPluginGtkPlugAddedMessage* msg = OpPluginGtkPlugAddedMessage::Create(static_cast<uint64_t>(GDK_WINDOW_XID(m_gtkplug->window)));
					RETURN_OOM_IF_NULL(msg);
					OpSyncMessenger sync(m_channel);
					sync.SendMessage(msg);
					m_channel = sync.TakePeer();
				}
			}
			out_npwin.window = reinterpret_cast<void*>(GDK_WINDOW_XID(m_gtksocket->window));
			break;
		case NPWindowTypeDrawable:
		default:
			out_npwin.window = 0;
	}

	out_npwin.x               = 0;
	out_npwin.y               = 0;
	out_npwin.width           = rect.width;
	out_npwin.height          = rect.height;
	out_npwin.clipRect.left   = 0;
	out_npwin.clipRect.top    = 0;
	out_npwin.clipRect.right  = 0;
	out_npwin.clipRect.bottom = 0;
	out_npwin.ws_info         = &m_window_info;
	out_npwin.type            = type;

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginTranslator::CreateFocusEvent(OpPlatformEvent** event,
                                                   bool got_focus)
{
	XFocusChangeEvent* focus_event = OP_NEW(XFocusChangeEvent, ());
	if(!focus_event)
		return OpStatus::ERR_NO_MEMORY;
	*event = OP_NEW(UnixOpPlatformEvent, (focus_event));
	if (!*event)
	{
		OP_DELETE(focus_event);
		return OpStatus::ERR_NO_MEMORY;
	}

	focus_event->type = got_focus ? FocusIn : FocusOut;
	focus_event->serial = LastKnownRequestProcessed(m_window_info.display);
	focus_event->send_event = False;
	focus_event->display = m_window_info.display;
	focus_event->window = m_browser_window;
	focus_event->mode = NotifyNormal;

	/* Not entirely correct, but to replicate X behaviour perfectly, I
	 * believe we would have to send multiple events. */
	focus_event->detail = NotifyAncestor;

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginTranslator::CreateKeyEventSequence(OtlList<OpPlatformEvent*>& events, OpKey::Code key, uni_char value,  OpPluginKeyState key_state, ShiftKeyState shift_state, UINT64 state, UINT64 keycode)
{
	XKeyEvent* key_event = OP_NEW(XKeyEvent, ());
	if (!key_event)
		return OpStatus::ERR_NO_MEMORY;

	key_event->type = (key_state == PLUGIN_KEY_UP) ? KeyRelease : KeyPress;

	OP_STATUS status = SetPointEventFields(reinterpret_cast<XEvent*>(key_event), OpPoint());
	if (OpStatus::IsError(status))
	{
		OP_DELETE(key_event);
		return status;
	}

	key_event->same_screen = True;

	/* Use cached data from UnixOpPluginAdapter::GetPlatformKeyEventData() .. */
	key_event->state = static_cast<unsigned int>(state);
	key_event->keycode = static_cast<unsigned int>(keycode);

	/*
	 * .. in favor of data that has lots several crucial bits (see DSK-347388).
	 *
	 * key_event->state |= (shift_state & SHIFTKEY_ALT ? Mod1Mask : 0)
	 *     | (shift_state & SHIFTKEY_SHIFT ? ShiftMask : 0)
	 *	   | (shift_state & SHIFTKEY_CTRL ? ControlMask : 0);
	 *
	 * key_event->keycode = XKeysymToKeycode(key_event->display, OpKeyToX11Keysym(key));
	 */

	OpPlatformEvent* event = OP_NEW(UnixOpPlatformEvent, (key_event));
	if (!event)
	{
		OP_DELETE(key_event);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsError(events.Append(event)))
	{
		OP_DELETE(event);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginTranslator::CreateMouseEvent(OpPlatformEvent** event,
                                                   OpPluginEventType event_type,
                                                   const OpPoint& point,
                                                   int button,
                                                   ShiftKeyState mod)
{
	OP_STATUS status = OpStatus::OK;
	switch (event_type)
	{
		case PLUGIN_MOUSE_MOVE_EVENT:
		{
			XMotionEvent* motion_event = OP_NEW(XMotionEvent, ());
			if (!motion_event)
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}

			motion_event->type = MotionNotify;
			status = SetPointEventFields(reinterpret_cast<XEvent*>(motion_event), point);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(motion_event);
				break;
			}

			motion_event->state |= (mod & SHIFTKEY_ALT ? Mod1Mask : 0)
				| (mod & SHIFTKEY_SHIFT ? ShiftMask : 0)
				| (mod & SHIFTKEY_CTRL ? ControlMask : 0);
			motion_event->same_screen = True;
			motion_event->is_hint = NotifyNormal;

			*event = OP_NEW(UnixOpPlatformEvent, (motion_event));
			if (!*event)
			{
				OP_DELETE(motion_event);
				status = OpStatus::ERR_NO_MEMORY;
			}
			break;
		}
		case PLUGIN_MOUSE_DOWN_EVENT:
			status = CreateMouseButtonEvent(event, point, button, mod, ButtonPress);
			break;
		case PLUGIN_MOUSE_UP_EVENT:
			status = CreateMouseButtonEvent(event, point, button, mod, ButtonRelease);
			break;
		case PLUGIN_MOUSE_ENTER_EVENT:
			status = CreateMouseCrossingEvent(event, point, EnterNotify);
			break;
		case PLUGIN_MOUSE_LEAVE_EVENT:
			status = CreateMouseCrossingEvent(event, point, LeaveNotify);
			break;
		default:
			status = OpStatus::ERR_NOT_SUPPORTED;
			break;
	}
	return status;
}

OP_STATUS UnixOpPluginTranslator::CreatePaintEvent(OpPlatformEvent** event,
                                                   OpPluginImageID image,
                                                   OpPluginImageID background,
                                                   const OpRect& paint_rect,
                                                   NPWindow* np_win,
                                                   bool*)
{
	OP_ASSERT(paint_rect.width > 0 && paint_rect.height > 0 && "UnixOpPluginTranslator::CreatePaintEvent: Received invalid dimensions");

	if (!image)
		return OpStatus::ERR;

	X11Types::Drawable drawable = static_cast<X11Types::Drawable>(image);
	if (background)
	{
		/**
		 * We have a background and therefore must be in transparent mode. We
		 * need to copy it to the buffer that will be sent to the plugin.
		 */
		X11Types::Drawable drawable_bg = static_cast<X11Types::Drawable>(background);
		GC gc = XCreateGC(m_window_info.display, drawable_bg, 0, 0);
		XCopyArea(m_window_info.display, drawable_bg, drawable, gc, 0, 0, np_win->width, np_win->height, 0, 0);
		XSync(m_window_info.display, False);
		XFreeGC(m_window_info.display, gc);
	}

	XGraphicsExposeEvent* xevent = OP_NEW(XGraphicsExposeEvent, ());
	if (!xevent)
		return OpStatus::ERR_NO_MEMORY;

	xevent->type       = GraphicsExpose;
	xevent->serial     = LastKnownRequestProcessed(m_window_info.display);
	xevent->send_event = False;
	xevent->display    = m_window_info.display;
	xevent->drawable   = drawable;
	xevent->x          = 0;
	xevent->y          = 0;
	//xevent->width      = paint_rect.width;
	//xevent->height     = paint_rect.height;
	xevent->width      = np_win->width;
	xevent->height     = np_win->height;
	xevent->count      = 0;
	xevent->major_code = 0;
	xevent->minor_code = 0;

	*event = OP_NEW(UnixOpPlatformEvent, (xevent));
	if (!*event)
	{
		OP_DELETE(xevent);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS UnixOpPluginTranslator::CreateWindowPosChangedEvent(OpPlatformEvent**)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

bool UnixOpPluginTranslator::GetValue(NPNVariable variable, void* value, NPError* result)
{
	if (variable == NPNVnetscapeWindow)
	{
		*reinterpret_cast<void**>(value) = reinterpret_cast<void*>(static_cast<INTPTR>(m_browser_window));
		return true;
	}

	return false;
}

/* static */
bool OpPluginTranslator::GetGlobalValue(NPNVariable variable, void* value, NPError* result)
{
	*result = NPERR_NO_ERROR;

	switch (variable)
	{
		case NPNVSupportsWindowless:
		case NPNVSupportsXEmbedBool:
			*reinterpret_cast<bool*>(value) = true;
			return true;

		case NPNVToolkit:
			*reinterpret_cast<NPNToolkitType*>(value) = NPNVGtk2;
			return true;

		default:
			return false;
	}
}

bool UnixOpPluginTranslator::SetValue(NPPVariable variable, void* value, NPError* result)
{
	return false;
}

OP_STATUS UnixOpPluginTranslator::ProcessMessage(const OpTypedMessage* message)
{
	OP_STATUS status = OpStatus::OK;
	switch (message->GetType())
	{
		case OpPeerConnectedMessage::Type:
		case OpPeerDisconnectedMessage::Type:
			break;

		case OpPluginErrorMessage::Type:
		case OpStatusMessage::Type:
			/* Handled by OpSyncMessenger. */
			break;

		case OpPluginParentChangedMessage::Type:
			status = OnPluginParentChangedMessage(OpPluginParentChangedMessage::Cast(message));
			break;

		default:
			OP_ASSERT(!"UnixOpPluginTranslator::ProcessMessage: Received unknown/unhandled message");
			break;
	}

	return status;
}

OP_STATUS UnixOpPluginTranslator::CreateMouseButtonEvent(OpPlatformEvent** e,
                                                   const OpPoint& pt,
                                                   const int button,
                                                   const ShiftKeyState mod,
                                                   const int type)
{
	XButtonEvent* button_event = OP_NEW(XButtonEvent, ());
	if (!button_event)
		return OpStatus::ERR_NO_MEMORY;

	button_event->type = type;
	OP_STATUS status = SetPointEventFields(reinterpret_cast<XEvent*>(button_event), pt);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(button_event);
		return status;
	}

	switch (button)
	{
		case MOUSE_BUTTON_1:
			button_event->state = Button1Mask;
			button_event->button = Button1;
			break;
		case MOUSE_BUTTON_2:
			button_event->state = Button2Mask;
			button_event->button = Button2;
			break;
		case MOUSE_BUTTON_3:
			button_event->state = Button3Mask;
			button_event->button = Button3;
			break;
		default:
			// Unknown buttons are unhandled
			break;
	};

	button_event->state |= (mod & SHIFTKEY_ALT ? Mod1Mask : 0)
		| (mod & SHIFTKEY_SHIFT ? ShiftMask : 0)
		| (mod & SHIFTKEY_CTRL ? ControlMask : 0);
	button_event->same_screen = True;

	*e = OP_NEW(UnixOpPlatformEvent, (button_event));
	if (!*e)
	{
		OP_DELETE(button_event);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS UnixOpPluginTranslator::CreateMouseCrossingEvent(OpPlatformEvent** e,
                                                     const OpPoint& pt,
                                                     const int type)
{
	XCrossingEvent* ev = OP_NEW(XCrossingEvent, ());
	if (!ev)
		return OpStatus::ERR_NO_MEMORY;

	ev->type = type;
	OP_STATUS status = SetPointEventFields(reinterpret_cast<XEvent*>(ev), pt);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(ev);
		return status;
	}

	ev->same_screen = True;

	/* Neither are strictly correct. */
	ev->focus = (type == EnterNotify);
	ev->state = 0;
	ev->mode = NotifyNormal;

	*e = OP_NEW(UnixOpPlatformEvent, (ev));
	if (!*e)
	{
		OP_DELETE(ev);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS UnixOpPluginTranslator::SetPointEventFields(XEvent* event, const OpPoint& pt)
{
	/* Retrieve root window. */
	X11Types::Window root = RootWindow(m_window_info.display, m_screen_number);

	/* Retrieve position of top level window. */
	XWindowAttributes attributes;
	if (!XGetWindowAttributes(m_window_info.display, m_browser_window, &attributes))
		return OpStatus::ERR;

	/**
	 * Set a time for the event. X11 server events are specified in
	 * milliseconds since a particular event, usually the X11 servers
	 * boot time.
	 *
	 * Unfortunately, we don't seem to be able to retrieve this value in
	 * any other way than by checking the system time when receiving a
	 * real X11 event, so for now, we're simply using the system time
	 * directly (truncated to a legal value.)
	 *
	 * This may cause problems when generating events for a plugin that
	 * also receives real X11 events.
	 */
	X11Types::Time timestamp = static_cast<X11Types::Time>(g_component_manager->GetRuntimeMS());

	/* Fill out event form. Do not use any later value than y_root. */
	XKeyEvent* ev = reinterpret_cast<XKeyEvent*>(event);
	ev->serial = LastKnownRequestProcessed(m_window_info.display);
	ev->send_event = False;
	ev->display = m_window_info.display;
	ev->window = m_browser_window;
	ev->root = root;
	ev->subwindow = None;
	ev->time = timestamp;
	ev->x = pt.x;
	ev->y = pt.y;
	ev->x_root = attributes.x + attributes.border_width + pt.x;
	ev->y_root = attributes.y + attributes.border_width + pt.y;

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginTranslator::OnPluginParentChangedMessage(OpPluginParentChangedMessage* message)
{
	m_browser_window = message->GetParent();
	return message->Reply(OpPluginErrorMessage::Create());
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
