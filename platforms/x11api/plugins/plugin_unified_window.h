/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLUGIN_UNIFIED_WINDOW_H_
#define PLUGIN_UNIFIED_WINDOW_H_

#include "modules/pi/OpPluginWindow.h"
#include "modules/inputmanager/inputcontext.h"
#include "platforms/utilix/x11_all.h"

class OpPainter;
class OpView;
class UnixOpPluginAdapter;
class X11OpPainter;
typedef struct _NPWindow NPWindow;

class OpUnixPlatformEvent :	public OpPlatformEvent
{
public:
	OpUnixPlatformEvent(const XEvent& event) : m_event(event) { }

	/// Get the platform specific event
	virtual void* GetEventData() const { return const_cast<XEvent*>(&m_event); }
protected:
	OpUnixPlatformEvent() {}
private:
	XEvent m_event;
};

class OpUnixPlatformDummyEvent : public OpUnixPlatformEvent
{
public:
	OpUnixPlatformDummyEvent() {}
	virtual void* GetEventData() const { return NULL; }
};

/**
 * A rectangle or window containing a plug-in instance.
 *
 * This class contains common window methods that are window mode (wmode)
 * agnostic. Some of them are overloaded in PluginUnifiedWindowed for windowed
 * mode (wmode=window).
 */
class PluginUnifiedWindow : public OpPluginWindow
{
public:
	/// Convenient method for creating a plugin window without prior
	/// knowledge of available window types.
	static PluginUnifiedWindow* Create(const OpRect& rect, int scale, OpView* parent_view, const bool windowless = false, OpWindow* parent_window = NULL);

	PluginUnifiedWindow(const OpRect& rect, int scale, OpView* parent_view, OpWindow* parent_window) :
		m_plugin_parent(parent_view), m_parent_window(parent_window), m_adapter(0), m_plugin_rect(rect) { }

	virtual ~PluginUnifiedWindow() { }

	/// Detach window from parent window.
	virtual void Detach() { }

	/// Hide the appearance of the window.
	virtual void Hide() { }

	/// Make window visible.
	virtual void Show() { }

	/// Reparent the window (in case the toplevel window changed)
	virtual void Reparent() { }

	virtual unsigned int CheckPaintEvent();

	/// Paint directly without having to call core back to handle painting.
	virtual OP_STATUS EnableDirectPaint(const OpRect&);

	virtual void* GetHandle();

	/// Get the position of the plugin window, relative to the document body.
	OpPoint GetPosition();

	/// Return the plugin window handle.
	virtual unsigned int GetWindowHandle();

	void SetAdapter(UnixOpPluginAdapter* adapter);

	virtual void SetListener(OpPluginWindowListener*) { }
	virtual void SetMouseInputBlocked(const bool) { }
	virtual bool IsMouseInputBlocked() { return false; }

	virtual void SetPos(const int x, const int y);
	virtual void SetSize(const int w, const int h);

	virtual BOOL SendEvent(OpPlatformEvent* event);

	/// Triggered when a child has been added to this window.
	virtual void OnPluginWindowAdded(const unsigned int window, const bool use_xembed) { }

	/// Triggered when a child has been removed from this window.
	virtual void OnPluginWindowRemoved(const unsigned int window) { }

	virtual OP_STATUS FocusEvent(OpPlatformEvent** event, const bool got_focus);
	virtual OP_STATUS KeyEvent(OpPlatformEvent** event, const OpKey::Code key, const uni_char* key_value, const bool pressed, OpKey::Location location, const ShiftKeyState mod);
	virtual OP_STATUS MouseEvent(OpPlatformEvent** event, const OpPluginEventType event_type, const OpPoint& point, const int button_or_key, const ShiftKeyState mod);
	virtual OP_STATUS WindowMovedEvent(OpPlatformEvent** event);

	/// Ask platform whether it is using direct painting.
	virtual BOOL UsesDirectPaint() const;

	/******************************************
	 * Compatibility layer for OpPluginWindow *
	 ******************************************/

	virtual void BlockMouseInput(BOOL blocked) { SetMouseInputBlocked(blocked || false); }
	virtual BOOL IsBlocked() { return IsMouseInputBlocked(); }
	virtual OP_STATUS CreateMouseEvent(OpPlatformEvent** event, OpPluginEventType event_type, const OpPoint& point, int button_or_key, ShiftKeyState modifiers) { return MouseEvent(event, event_type, point, button_or_key, modifiers); }
	virtual OP_STATUS CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& rect) { return TRUE; }
	virtual OP_STATUS CreateKeyEvent(OpPlatformEvent** event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers)
	{
		if (key_state == PLUGIN_KEY_PRESSED)
		{
			/* We map core's KEYDOWN and KEYUP to X11 equivalents, but we suppress core KEYPRESS since there
			 * is no X11 equivalent for it. The KEYPRESS needs to be passed to the platform though because Mac is
			 * using it, so we pass a dummy event instead which will be silently ignored by OpPluginWindow::SendEvent(). */
			*event = OP_NEW(OpUnixPlatformDummyEvent, ());
			return OpStatus::OK;
		}
		return KeyEvent(event, key, key_value, key_state == PLUGIN_KEY_DOWN, location, modifiers);
	}
	virtual OP_STATUS CreateFocusEvent(OpPlatformEvent** event, BOOL got_focus) { return FocusEvent(event, got_focus); }

	virtual OP_STATUS CreateWindowPosChangedEvent(OpPlatformEvent** event) { return WindowMovedEvent(event); }
	virtual OP_STATUS PaintDirectly(const OpRect& rect) { return EnableDirectPaint(rect); }
	virtual void SetPluginObject(Plugin* plugin) { m_plugin_object = plugin; }

protected:
	OpView* m_plugin_parent;
	OpWindow* m_parent_window;
	Plugin* m_plugin_object;
	UnixOpPluginAdapter* m_adapter;
	OpRect m_plugin_rect;
};

inline OpPoint PluginUnifiedWindow::GetPosition()
{
	return OpPoint(m_plugin_rect.x, m_plugin_rect.y);
}

#endif // PLUGIN_UNIFIED_WINDOW_H_
