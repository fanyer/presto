/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-12 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLUGIN_UNIFIED_WINDOWED_H_
#define PLUGIN_UNIFIED_WINDOWED_H_

#include "platforms/x11api/plugins/plugin_unified_window.h"
#include "platforms/x11api/plugins/plugin_unified_overlay.h"

class X11PlatformKeyEventData : public OpPlatformKeyEventData
{
public:
	/**
	 * Set up the platform key event.
	 *
	 * @param keyevent The platform key event. Overship is transferred. The event data
	 *        must never be destroyed by caller.
	 */
	X11PlatformKeyEventData(XKeyEvent* keyevent)
		: m_keyevent(keyevent)
		, m_ref_count(1)
	{
	}

	/**
	 * Called when reference count reaches 0. The saved platform key event data is then
	 * removed from memory.
	 */
	~X11PlatformKeyEventData()
	 {
		 OP_DELETE(m_keyevent);
	 }

	/**
	 * Returns the platform key event data 'state' and 'keycode'.
	 *
	 * @note The returned value is transported via an OpPluginWindowlessKeyEventMessage
	 *       to OpPluginTranslator::CreateKeyEventSequence(). That method can use the
	 *       returned values to construct a new XKeyEvent.
	 * @see UnixOpPluginTranslator::CreateKeyEventSequence()
	 *
	 * @param data1 returns the \c XKeyEvent::state of the associated XKeyEvent.
	 * @param data2 returns the \c XKeyEvent::keycode of the associated XKeyEvent.
	 */
	virtual void GetPluginPlatformData(UINT64& data1, UINT64& data2)
	{
		data1 = (UINT64)m_keyevent->state;
		data2 = (UINT64)m_keyevent->keycode;
	}

public:
	XKeyEvent* m_keyevent;
	unsigned int m_ref_count;
};


/**
 * A window containing a windowed plug-in instance.
 */
class PluginUnifiedWindowed : public PluginUnifiedWindow
{
public:
	PluginUnifiedWindowed(const OpRect& rect, int scale, OpView* parent_view, OpWindow* parent_window);
	virtual ~PluginUnifiedWindowed();

	virtual OP_STATUS Init(X11Types::Display*);

	virtual void Detach();
	virtual void Hide();
	virtual void Show();
	virtual void Reparent();

	virtual void* GetHandle();
	virtual unsigned int GetWindowHandle();

	virtual void SetListener(OpPluginWindowListener* listener);
	virtual void SetMouseInputBlocked(const bool block);
	virtual bool IsMouseInputBlocked();
	virtual void SetPos(const int x, const int y);
	virtual void SetSize(const int w, const int h);

	virtual BOOL SendEvent(OpPlatformEvent* event);

	/// Simulate a mouse click in order to correctly set keyboard focus.
	inline void OnMouseDown();

	virtual void OnPluginWindowAdded(unsigned int window, bool use_xembed);
	virtual void OnPluginWindowRemoved(unsigned int window);

	virtual OP_STATUS FocusEvent(OpPlatformEvent** event, const bool got_focus);
	virtual OP_STATUS KeyEvent(OpPlatformEvent** event, OpKey::Code key, const uni_char* key_value, const bool pressed, OpKey::Location location, const ShiftKeyState mod);

protected:
	X11Types::Window m_mother_window;
	X11Types::Window m_plugin_window;  ///< The actual X window handed over to the plugin
	X11Types::Window m_window_handle;  ///< The parent X window of the plugin

#ifdef MANUAL_PLUGIN_ACTIVATION
	PluginUnifiedOverlay m_activation_window;  ///< Plugin overlay (click-to-activate) window
#endif  // MANUAL_PLUGIN_ACTIVATION
	OpPluginWindowListener* m_listener;

	X11Types::Display* m_display;

private:
	OpMouseListener* GetMouseListenerForOpView(OpView* opview);
	void BroadcastEvent(X11Types::Display* display, X11Types::Window& window, XEvent* xevent, const int max_depth);

#ifdef MANUAL_PLUGIN_ACTIVATION
	void CreateActivationWindow();
#endif
};

inline void* PluginUnifiedWindowed::GetHandle()
{
	return reinterpret_cast<void*>(static_cast<uintptr_t>(GetWindowHandle()));
}

inline unsigned int PluginUnifiedWindowed::GetWindowHandle()
{
	return m_plugin_window ? m_plugin_window : m_window_handle;
}

inline void PluginUnifiedWindowed::SetListener(OpPluginWindowListener* listener)
{
#ifdef MANUAL_PLUGIN_ACTIVATION
	m_activation_window.SetWindowListener(listener);
#endif
	m_listener = listener;
}

void PluginUnifiedWindowed::OnMouseDown()
{
	m_listener->OnMouseDown();
}

inline void PluginUnifiedWindowed::OnPluginWindowRemoved(const unsigned int)
{
	m_plugin_window = 0;
}

#endif // PLUGIN_UNIFIED_WINDOWED_H_
