/**
 * Copyright (C) 2011-12 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef UNIX_OPPLUGINADAPTER_H_
#define UNIX_OPPLUGINADAPTER_H_

#include "modules/hardcore/component/OpMessenger.h"
#include "modules/pi/OpNS4PluginAdapter.h"

class OpBrowserWindowInformationMessage;
class OpPluginGtkPlugAddedMessage;
class PluginUnifiedWindow;
class PlugixParentWindowListener;
class UnixOpPluginAdapter;

class UnixOpPluginAdapter : public OpNS4PluginAdapter, public OpMessageListener
{
public:
	UnixOpPluginAdapter();
	~UnixOpPluginAdapter();

	OP_STATUS Construct();

	OP_STATUS OnBrowserWindowInformationMessage(OpBrowserWindowInformationMessage* message);
	OP_STATUS OnParentChanged(UINT64 parent_window_handle);
	OP_STATUS OnPluginGtkPlugAddedMessage(OpPluginGtkPlugAddedMessage* message);

	/* Implement OpNS4PluginAdapter */

	void SetPluginParentWindow(OpWindow* parent_win);
	virtual void SetPluginWindow(OpPluginWindow* plug_win);
	virtual void SetVisualDevice(VisualDevice* vis_dev);
	virtual BOOL GetValue(NPNVariable var, void* val, NPError* err);
	virtual BOOL SetValue(NPPVariable var, void* val, NPError* err);
	virtual BOOL ConvertPlatformRegionToRect(NPRegion invalid_region, NPRect& invalid_rect);
	virtual OP_STATUS SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent);
	virtual OP_STATUS SetupNPPrint(NPPrint* np_print, const OpRect& rect);
	virtual void SaveState(OpPluginEventType event_type);
	virtual void RestoreState(OpPluginEventType event_type);
	virtual OpChannel* GetChannel();
	virtual void GetKeyEventPlatformData(uni_char key, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64& data1, UINT64& data2);
	virtual void StartIME() {}
	virtual bool IsIMEActive() { return false; }
	virtual void NotifyFocusEvent(bool focus, FOCUS_REASON reason) {}

	/* Implement OpMessageListener */

	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

protected:
	OpChannel* m_channel;
	OpWindow* m_parent_window;
	PlugixParentWindowListener* m_parent_listener;
	PluginUnifiedWindow* m_plugin_window;
};

#endif  // UNIX_OPPLUGINADAPTER_H_
