/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OPPLUGINADAPTER_H
#define WINDOWS_OPPLUGINADAPTER_H

#include "modules/hardcore/component/OpMessenger.h"
#include "modules/pi/OpNS4PluginAdapter.h"

#include "platforms/windows/pi/WindowsOpPluginWindow.h"
#include "platforms/windows_common/src/generated/g_message_windows_common_messages.h"

class WindowsOpNS4PluginAdapter : public OpNS4PluginAdapter, public OpMessageListener
{
public:
							WindowsOpNS4PluginAdapter() : m_channel(NULL), m_plugin_window(NULL) { }
	virtual					~WindowsOpNS4PluginAdapter() { OP_DELETE(m_channel); }

	OP_STATUS				Construct();
	OP_STATUS				OnWindowsPluginWindowInfoMessage(OpWindowsPluginWindowInfoMessage* message);

	/* Implement OpNS4PluginAdapter. */

	virtual void			SetPluginWindow(OpPluginWindow* plugin_window) { m_plugin_window = static_cast<WindowsOpPluginWindow*>(plugin_window); }
	virtual OpChannel*		GetChannel() { return m_channel; }

	virtual void			StartIME() {}
	virtual bool			IsIMEActive() {return false;}
	virtual void			NotifyFocusEvent(bool focus, FOCUS_REASON reason) {}

	/** Unused on windows (Native MDE window takes care of it instead) @{ */
	virtual BOOL			SetValue(NPPVariable variable, void* value, NPError* result) { return FALSE; }
#ifdef NS4P_COMPONENT_PLUGINS
	virtual void			SetVisualDevice(VisualDevice* visual_device) { }
	virtual BOOL			GetValue(NPNVariable variable, void* value, NPError* result) { return FALSE; }
	virtual BOOL			ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect) { return FALSE; }
	virtual OP_STATUS		SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent) { return OpStatus::OK; }
#else // !NS4P_COMPONENT_PLUGINS
	virtual void			SetVisualDevice(VisualDevice* visual_device) { m_plugin_window->SetVisualDevice(visual_device); }
	virtual BOOL			GetValue(NPNVariable variable, void* value, NPError* result);
	virtual BOOL			ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect);
	virtual OP_STATUS		SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent);
#endif // !NS4P_COMPONENT_PLUGINS
	virtual OP_STATUS		SetupNPPrint(NPPrint* npprint, const OpRect& rect) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual void			SaveState(OpPluginEventType event_type) { }
	virtual void			RestoreState(OpPluginEventType event_type) { }
	/** @} */

	/* Implement OpMessageListener. */

	virtual OP_STATUS		ProcessMessage(const OpTypedMessage* message);

private:
	OpChannel*				m_channel;
	WindowsOpPluginWindow*	m_plugin_window; ///< the Windows plugin window
};

#ifndef NS4P_COMPONENT_PLUGINS
/** OpWindowsPlatformEvent is the Windows platform representation of a plugin event. */
class OpWindowsPlatformEvent : public OpPlatformEvent
{
public:
	~OpWindowsPlatformEvent();

	NPEvent					m_event; ///< the Windows representation of the NPEvent

	/** Get the Windows plugin event */
	virtual void*		GetEventData() const { return (void*)&m_event; }
};
#endif // !NS4P_COMPONENT_PLUGINS

#endif // WINDOWS_OPPLUGINADAPTER_H
