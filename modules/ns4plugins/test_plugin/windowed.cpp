/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Implementations specific to windowed plugin instances.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#include <cassert>

#include "common.h"
#include "instance.h"


/**
 * Windowless plugin constructor.
 *
 * @param instance NPP instance.
 */
WindowedInstance::WindowedInstance(NPP instance, const char* bgcolor)
	: PluginInstance(instance, bgcolor)
{
#ifdef XP_UNIX
	event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	container = 0;
	canvas = 0;
#endif // XP_UNIX

#ifdef XP_WIN
	last_win_hwnd = 0;
	left_button_down = false;
	right_button_down = false;
#endif // XP_WIN
}


/**
 * Windowless plugin destructor.
 */
WindowedInstance::~WindowedInstance()
{
	UnregisterWindowHandler();
}


/**
 * Initialize plug-in instance.
 *
 * Called by NPP_New. Returning error will fail plug-in instantiation.
 *
 * @return True on success.
 */
bool WindowedInstance::Initialize()
{
#ifdef XP_UNIX
	NPBool value = false;

	/* Verify XEmbed support. */
	if (g_browser->getvalue(instance, NPNVSupportsXEmbedBool, &value) != NPERR_NO_ERROR || !value)
		return false;
#endif // XP_UNIX

	return true;
}


/**
 * Window create, move, resize or destroy notification.
 *
 * @See https://developer.mozilla.org/en/NPP_SetWindow.
 */
NPError WindowedInstance::SetWindow(NPWindow* window)
{
	UnregisterWindowHandler();
	if (!window)
		return NPERR_INVALID_PARAM;

	NPError set_window = PluginInstance::SetWindow(window);
	NPError register_handler = RegisterWindowHandler();

	paint();

	return set_window != NPERR_NO_ERROR ? set_window : register_handler;
}


#ifdef XP_UNIX
/**
 * Create a Gtk container.
 */
NPError WindowedInstance::CreateGtkWidgets()
{
	/* Create a GtkPlug container and plot a drawing canvas inside. */
	if (!(container = gtk_plug_new(static_cast<GdkNativeWindow>(reinterpret_cast<intptr_t>(plugin_window->window))))
		|| !(canvas = gtk_drawing_area_new()))
		return NPERR_OUT_OF_MEMORY_ERROR;

	/* Make sure the canvas is capable of receiving focus. */
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(canvas), GTK_CAN_FOCUS);

	/* All the events that our canvas wants to receive. */
	gtk_widget_add_events(canvas,
						  GDK_BUTTON_PRESS_MASK |
						  GDK_BUTTON_RELEASE_MASK |
						  GDK_KEY_PRESS_MASK |
						  GDK_KEY_RELEASE_MASK |
						  GDK_POINTER_MOTION_MASK |
						  GDK_SCROLL_MASK |
						  GDK_EXPOSURE_MASK |
						  GDK_VISIBILITY_NOTIFY_MASK |
						  GDK_ENTER_NOTIFY_MASK |
						  GDK_LEAVE_NOTIFY_MASK |
						  GDK_FOCUS_CHANGE_MASK
		);

	/* Connect event handler to the canvas. */
	m_gtk_event_handler_id = g_signal_connect(G_OBJECT(canvas), "event", G_CALLBACK(gtk_event), this);

	/* Show canvas. */
	gtk_widget_show(canvas);

	/* Add to contaiener. */
	gtk_container_add(GTK_CONTAINER(container), canvas);

	/* Show container. */
	gtk_widget_show(container);

	return NPERR_NO_ERROR;
}


/**
 * Receive event callback from Gtk library.
 *
 * @param widget Gtk widget this event pertains to.
 * @param event X event.
 *
 * @return True if event has been taken care of.
 */
gboolean WindowedInstance::HandleGtkEvent(GtkWidget* /* widget */, GdkEvent* event)
{
	CountUse of(this);

	switch (event->type)
	{
		case GDK_EXPOSE:
		{
			GdkEventExpose* expose = reinterpret_cast<GdkEventExpose*>(event);
			return OnPaint(canvas->window, canvas->style->fg_gc[GTK_STATE_NORMAL],
				expose->area.x, expose->area.y, expose->area.width, expose->area.height);
		}

		case GDK_FOCUS_CHANGE:
		{
			GdkEventFocus* focus = reinterpret_cast<GdkEventFocus*>(event);
			return OnFocus(true, focus->in);
		}

		case GDK_ENTER_NOTIFY:
		case GDK_LEAVE_NOTIFY:
		{
			GdkEventCrossing* crossing = reinterpret_cast<GdkEventCrossing*>(event);
			return OnMouseMotion(crossing->x, crossing->y, event->type == GDK_ENTER_NOTIFY ? MouseOver : MouseOut);
		}

		case GDK_MOTION_NOTIFY:
		{
			GdkEventMotion* motion = reinterpret_cast<GdkEventMotion*>(event);
			return OnMouseMotion(motion->x, motion->y, MouseMove);
		}

		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		{
			GdkEventButton* button = reinterpret_cast<GdkEventButton*>(event);

			if (button->button == 1)
				/* take the focus into our own hands */
				gtk_widget_grab_focus(canvas);

			return OnMouseButton(event->type == GDK_BUTTON_PRESS ? ButtonPress : ButtonRelease,
				button->x, button->y, button->button);
		}

		case GDK_KEY_PRESS:
		case GDK_KEY_RELEASE:
		{
			GdkEventKey* key = reinterpret_cast<GdkEventKey*>(event);
			return OnKey(event->type == GDK_KEY_PRESS ? KeyPress : KeyRelease, key->keyval, key->state, key->string, NULL);
		}

		case GDK_SCROLL:
		{
			GdkEventScroll* scroll = reinterpret_cast<GdkEventScroll*>(event);
			bool vertical = scroll->direction == GDK_SCROLL_LEFT || scroll->direction == GDK_SCROLL_RIGHT;
			return OnMouseWheel(scroll->state, vertical, scroll->x, scroll->y);
		}
	}

	return false;
}
#endif // XP_UNIX


#ifdef XP_WIN
/**
 * Handle window messages. Called by Windows as part of the browser message loop.
 */
LRESULT WindowedInstance::WinProc(HWND window_hwnd, UINT message, WPARAM w, LPARAM l)
{
	CountUse of(this);
	if (HWND plugin_hwnd = GetHWND())
	{
		RECT rect;

		switch (message)
		{
			case WM_PAINT:
				if (GetUpdateRect(plugin_hwnd, &rect, FALSE))
				{
					PAINTSTRUCT paint;
					HDC hdc = BeginPaint(plugin_hwnd, &paint);
					OnPaint(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
					EndPaint(plugin_hwnd, &paint);
					return 0;
				}
				break;

			case WM_MOUSEMOVE:
				OnMouseMotion(static_cast<int>(LOWORD(l)), static_cast<int>(HIWORD(l)), MouseMove);
				return 0;

			case WM_LBUTTONDOWN:
				if (!left_button_down)
				{
					SetActiveWindow(plugin_hwnd);
					SetFocus(plugin_hwnd);
					OnMouseButton(ButtonPress, static_cast<int>(LOWORD(l)), static_cast<int>(HIWORD(l)), 1);
					left_button_down = true;
				}
				return 0;

			case WM_LBUTTONUP:
				if (left_button_down)
				{
					OnMouseButton(ButtonRelease, static_cast<int>(LOWORD(l)), static_cast<int>(HIWORD(l)), 1);
					left_button_down = false;
				}
				return 0;

			case WM_RBUTTONDOWN:
				if (!right_button_down)
				{
					OnMouseButton(ButtonPress, static_cast<int>(LOWORD(l)), static_cast<int>(HIWORD(l)), 2);
					right_button_down = true;
				}
				return 0;

			case WM_RBUTTONUP:
				if (right_button_down)
				{
					OnMouseButton(ButtonRelease, static_cast<int>(LOWORD(l)), static_cast<int>(HIWORD(l)), 2);
					right_button_down = false;
				}
				return 0;

			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
				OnMouseWheel(GET_WHEEL_DELTA_WPARAM(w), message == WM_MOUSEWHEEL, static_cast<int>(LOWORD(l)), static_cast<int>(HIWORD(l)));
				return 0;

			case WM_KEYDOWN:
				OnKey(KeyPress, static_cast<int>(w), 0, NULL, NULL);
				return 0;

			case WM_KEYUP:
				OnKey(KeyRelease, static_cast<int>(w), 0, NULL, NULL);
				return 0;
		};
	}

	return CallWindowProc(old_win_proc, window_hwnd, message, w, l);
}
#endif // XP_WIN


/**
 * Return the coordinates of the plugin window's origin
 * relative to the window in which it is embedded.
 *
 * @param[out] x X coordinate of origin.
 * @param[out] y Y coordinate of origin.
 *
 * @return True on success.
 */
bool WindowedInstance::GetOriginRelativeToWindow(int& x, int& y)
{
	x = 0;
	y = 0;

	return true;
}


/**
 * Ecmascript native method.
 *
 * Perform a repaint of the plugin window. Mostly only useful for windowed plugins.
 * Takes four numeric arguments, x, y, width and height. Returns true on success.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool WindowedInstance::paint(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 4)
		return false;

	for (int i = 0; i < 4; i++)
		if (!NPVARIANT_IS_INT32(args[i]))
			return false;

	bool ret = false;
	int x = args[0].value.intValue;
	int y = args[1].value.intValue;
	int w = args[2].value.intValue;
	int h = args[3].value.intValue;

	paint(x, y, w, h);

	*result = *BoolVariant(ret);
	return true;
}


void WindowedInstance::paint(int x /* = 0 */, int y /* = 0 */, int w /* = 0 */, int h /* = 0 */)
{
	if (plugin_window && !x && !y && !w && !h)
	{
		w = plugin_window->width;
		h = plugin_window->height;
	}

#ifdef XP_UNIX
	if (canvas && canvas->window)
		gtk_widget_queue_draw_area(canvas, x, y, w, h);
#endif // XP_UNIX

#ifdef XP_WIN
	if (HWND hwnd = static_cast<WindowedInstance*>(this)->GetHWND())
	{
		RECT rect;
		rect.left = x;
		rect.top = y;
		rect.right = x + w;
		rect.bottom = y + h;

		InvalidateRect(hwnd, &rect, TRUE);
		UpdateWindow(hwnd);
	}
#endif // XP_WIN
}


/**
 * Register our window handler for the current window class.
 *
 * @returns NPERR_NO_ERROR on success, NPERR_INVALID_PARAM if
 * set window parameter does not follow the standard.
 */
NPError WindowedInstance::RegisterWindowHandler()
{
	NPError ret = NPERR_NO_ERROR;

#ifdef XP_UNIX
	if (!canvas)
		ret = CreateGtkWidgets();
#endif // XP_UNIX

#ifdef XP_WIN
	if (HWND hwnd = GetHWND())
	{
		/* Replace the window handler. */
		old_win_proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)win_proc);

		g_window_instance_map[hwnd] = this;
		last_win_hwnd = hwnd;
	}
#endif // XP_WIN

	return ret;
}


/**
 * Restore window handler state to before we attached our handler.
 */
void WindowedInstance::UnregisterWindowHandler()
{
#ifdef XP_UNIX
	if (canvas)
	{
		g_signal_handler_disconnect(G_OBJECT(canvas), m_gtk_event_handler_id);

		/* FIXME: Figure out how to free these. The Gtk documentation buried that information. */
		canvas = NULL;
		container = NULL;
	}
#endif // XP_UNIX

#ifdef XP_WIN
	if (last_win_hwnd)
	{
		WindowInstanceMap::iterator it = g_window_instance_map.find(last_win_hwnd);
		if (it != g_window_instance_map.end())
		{
			g_window_instance_map.erase(it);
			SetWindowLongPtr(last_win_hwnd, GWLP_WNDPROC, (LONG_PTR)old_win_proc);

			last_win_hwnd = 0;
		}
	}
#endif // XP_WIN
}


/* virtual */ void WindowedInstance::SafeDelete()
{
	UnregisterWindowHandler();
	PluginInstance::SafeDelete();
}


#ifdef XP_WIN
/**
 * Return window handle of plugin window.
 *
 * @return Window handle if valid or 0.
 */
HWND WindowedInstance::GetHWND()
{
	if (plugin_window && plugin_window->window)
		return static_cast<HWND>(plugin_window->window);

	return 0;
}
#endif // XP_WIN
