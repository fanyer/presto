/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef _X11_SETTINGS_CLIENT_
#define _X11_SETTINGS_CLIENT_

#include "platforms/unix/base/x11/x11_widget.h"
#include "modules/util/adt/oplisteners.h"

class XSettingsItem;


class X11SettingsClient
{
public:
	enum EventType
	{
		GTK_CURSOR_THEME_NAME,
		GTK_CURSOR_THEME_SIZE,
		GTK_MENUBAR_ACCELERATOR,
		GTK_COLOR_PALETTE,
		NET_CURSOR_BLINK,
		NET_CURSOR_BLINK_TIME,
		NET_DND_DRAG_THRESHOLD,
		NET_DOUBLE_CLICK_TIME,
		NET_ENABLE_EVENT_SOUNDS,
		NET_ENABLE_FEEDBACK_SOUNDS
	};

	enum ResourceCommand
	{
		ResourceAdded,
		ResourceChanged,
		ResourceRemoved
	};

	enum DataType
	{
		TypeUnknown=-1,
		TypeInteger=0,
		TypeString,
		TypeColor
	};

	struct Color
	{
		bool operator==(const Color& other)
		{
			return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
		}

		UINT16 red;
		UINT16 green; 
		UINT16 blue; 
		UINT16 alpha;
	};

public:
	class Listener
	{
	public:
		virtual void OnResourceUpdated(INT32 id, INT32 cmd, UINT32 value) = 0;
		virtual void OnResourceUpdated(INT32 id, INT32 cmd, const OpString8& value) = 0;
		virtual void OnResourceUpdated(INT32 id, INT32 cmd, const Color& value) = 0;
	};

public:
	/**
	 * Create an XSettings protocol client instance (only one can exists)
	 *
	 * @return OpStatus::OK on sucess, otherwise an error code
	 */
	static OP_STATUS Create();

	/**
	 * Destroy the existing XSettings protocol client instance
	 */
	static void Destroy();

	/**
	 * Inspect events and use them if appropriate
	 *
	 * @return TRUE if event is consumed, otherwise FALSE
	 */
	BOOL HandleEvent(XEvent* event);

	/**
	 * Add resource listener. 
	 *
	 * @param listener The listener
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS AddListener(Listener* listener) { return m_listeners.Add(listener); }

	/**
	 * Remove resource listener
	 *
	 * @param listener The listener
	 */
	void RemoveListener(Listener* listener) { m_listeners.Remove(listener); }

	/**
	 * Force reading all XSettings. All connected listeneres will be called
	 */
	void UpdateState();

	OP_STATUS GetStringValue(INT32 id, OpString8& value);
	OP_STATUS GetIntegerValue(INT32 id, UINT32& value);

private:
	X11SettingsClient();
	~X11SettingsClient();

	/**
	 * Initialize the XSettings protocol client
	 *
	 * @return OpStatus::OK on success, otherwise an error code describing the problem 
	 */
	OP_STATUS Init();

	/**
	 * Locate and start listening for changes on the XSettings protocol manager window
	 */
	void UpdateManagerWindow();

	/**
	 * Read configuration and preference settings provided by the remote
	 * XSettings protocol manager
	 */
	void ReadXSettings();

	/**
	 * Parse configuration and preference settings
	 */
	void ParseXSettings(class XSettingsData& settings);


	void NotifyChanges(OpAutoVector<XSettingsItem>* list);
	INT32 GetId(const OpString8& name);

private:
	X11Types::Atom m_manager_atom;
	X11Types::Atom m_selection_atom;
	X11Types::Atom m_xsettings_atom;
	X11Types::Window m_root_window;
	X11Types::Window m_manager_window;
	OpAutoVector<XSettingsItem>* m_list;
	OpListeners<Listener> m_listeners;
};

extern X11SettingsClient* g_xsettings_client;

#endif
