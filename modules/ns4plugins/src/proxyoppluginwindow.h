/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef PROXYOPPLUGINWINDOW_H__
#define PROXYOPPLUGINWINDOW_H__

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW

#include "modules/pi/OpPluginWindow.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/pi/OpPluginImage.h"

class PluginNativeWindow;

/***********************************************************************************
 **
 **	ProxyOpPluginWindow
 **
 ** Windowless plug-ins do not get all of the show, hide, move or resize events.
 ** To make sure they do, we create a PluginNativeWindow which will receive these 
 ** events. This class (ProxyOpPluginWindow) acts as a proxy to make sure that the 
 ** OpPluginWindow calls will be sent to the platform OpPluginWindow and to the
 ** PluginNativeWindow.
 **
 ***********************************************************************************/

class ProxyOpPluginWindow : public OpPluginWindow
{
public:
	static OP_STATUS Create(OpPluginWindow** new_object, const OpRect& rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window, Plugin* plugin);

	virtual ~ProxyOpPluginWindow();

	OpPluginWindow *GetInternalPluginWindow() { return m_plugin_window; }

	virtual void SetListener(OpPluginWindowListener *listener);

	virtual void Show();
	virtual void Hide();
	virtual void SetPos(int x, int y);
	virtual void SetSize(int w, int h);
	virtual void *GetHandle();
	virtual void BlockMouseInput(BOOL block);
	virtual BOOL IsBlocked();

	virtual OP_STATUS CreateMouseEvent(OpPlatformEvent** event, OpPluginEventType event_type, const OpPoint& point, int button, ShiftKeyState modifiers);
	virtual OP_STATUS CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& rect);
	virtual OP_STATUS CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers);
	virtual OP_STATUS CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in);
	virtual OP_STATUS CreateWindowPosChangedEvent(OpPlatformEvent** event);
	virtual OP_STATUS ConvertPoint(double source_x, double source_y, int source_space, double* dest_x, double* dest_y, int dest_space);
	virtual OP_STATUS PopUpContextMenu(void* menu);
	virtual BOOL SendEvent(OpPlatformEvent* event);
	virtual BOOL UsesDirectPaint() const;
	virtual OP_STATUS PaintDirectly(const OpRect& rect);

#ifdef NS4P_SILVERLIGHT_WORKAROUND
	virtual void InvalidateWindowed(const OpRect& rect);
#endif // NS4P_SILVERLIGHT_WORKAROUND

	virtual void SetPluginObject(Plugin* plugin);

	virtual void Detach();
	virtual unsigned int CheckPaintEvent();

	virtual BOOL UsesPluginNativeWindow() const;
	virtual OpView *GetParentView();
	virtual void SetClipRegion(MDE_Region *rgn);

private:
	ProxyOpPluginWindow();

	OpPluginWindow 		*m_plugin_window;
	PluginNativeWindow 	*m_plugin_native_window;
};

#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW	

#endif //  PROXYOPPLUGINWINDOW_H__
