/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef MACOPPLUGINADAPTER_H__
#define MACOPPLUGINADAPTER_H__


#if defined(_PLUGIN_SUPPORT_)

class MacOpPluginWindow;

#include "modules/ns4plugins/src/plugin.h"

/***********************************************************************************
 **
 **	MacOpPluginAdapter
 **
 ***********************************************************************************/

class MacOpPluginAdapter : public OpNS4PluginAdapter
#ifdef NS4P_COMPONENT_PLUGINS
, public OpMessageListener
#endif // NS4P_COMPONENT_PLUGINS
{
private:
	MacOpPluginWindow*	m_plugin_window;
	NPDrawingModel		m_drawing_model;
	NPEventModel		m_event_model;
	bool				m_updated_text_input;
	bool				m_transparent;
	
#ifdef NS4P_COMPONENT_PLUGINS
	OpChannel*          m_plugin_channel;
#endif // NS4P_COMPONENT_PLUGINS
	
public:
	MacOpPluginAdapter(
#ifdef NS4P_COMPONENT_PLUGINS
                       OpComponentType component_type
#endif // NS4P_COMPONENT_PLUGINS
                       );
	~MacOpPluginAdapter();
	
	virtual void SetPluginWindow(OpPluginWindow* plugin_window);
	virtual BOOL GetValue(NPNVariable variable, void* value, NPError* result);
	virtual BOOL SetValue(NPPVariable variable, void* value, NPError* result);
	virtual OP_STATUS SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent);
	virtual OP_STATUS SetupNPPrint(NPPrint* npprint, const OpRect& rect);
	virtual void SaveState(OpPluginEventType event_type);
	virtual void RestoreState(OpPluginEventType event_type);
	virtual void SetVisualDevice(VisualDevice* visual_device) {}
	virtual BOOL ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect);
	virtual void StartIME();
	virtual bool IsIMEActive();
	virtual void NotifyFocusEvent(bool focus, FOCUS_REASON reason);

#ifdef NS4P_COMPONENT_PLUGINS
	virtual OpChannel* GetChannel();
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);
#endif // NS4P_COMPONENT_PLUGINS
	
	void SetDrawingModel(NPDrawingModel drawing_model);
	void SetEventModel(NPEventModel event_model);
	void SetSupportsUpdatedCocoaTextInput(bool updated_text_input);
	void SetTransparent(bool transparent);
};

#endif // _PLUGIN_SUPPORT_

#endif //  MACOPPLUGINADAPTER_H__

