/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef MACOPPLUGINWINDOW_H__
#define MACOPPLUGINWINDOW_H__


#if defined(_PLUGIN_SUPPORT_)

#include "modules/pi/OpPluginWindow.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/pi/OpPluginImage.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/MacSharedMemory.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"

#ifdef __OBJC__
#include "platforms/mac/pi/plugins/MacOpPluginLayer.h"
#include "platforms/mac/model/OperaNSView.h"
#else
struct MacOpPluginLayer;
struct OperaNSView;
#endif

struct NP_Port;
struct NP_CGContext;
class Plugin;
class MacOpPluginWindow;
class MDE_Region;

class SentKey
{
public:
	SentKey(uni_char ch, unsigned short key) : ch(ch), key(key) {}
	uni_char ch;
	unsigned short key;
};

#ifndef NS4P_COMPONENT_PLUGINS

/***********************************************************************************
 **
 **	MacOpPlatformEvent
 **
 ***********************************************************************************/

template<class T>
class MacOpPlatformEvent : public OpPlatformEvent
{
public:
	OpAutoVector<T>m_events;
    
public:
	virtual ~MacOpPlatformEvent() {}
    
	int GetEventDataCount() { return m_events.GetCount(); }
	/** Get the platform specific event. */
	void* GetEventData(uint index) const { OP_ASSERT(index < m_events.GetCount()); OP_ASSERT(m_events.Get(index) != NULL); return m_events.Get(index); }
	void* GetEventData() const { return GetEventData(0); }
};

#endif // NS4P_COMPONENT_PLUGINS


/***********************************************************************************
 **
 **	MacOpPluginWindow
 **
 ***********************************************************************************/

class MacOpPluginWindow : public OpPluginWindow, public DesktopWindowListener
#ifndef NS4P_COMPONENT_PLUGINS
                            , public OpTimerListener
#endif // NS4P_COMPONENT_PLUGINS
{
	friend class MacPluginNativeWindow;

private:
	OpView*							m_parent_view;
	OpPluginWindowListener*			m_listener;
	BOOL							m_blocked_input;
	Plugin*							m_plugin;

#ifndef NS4P_COMPONENT_PLUGINS
# ifndef NP_NO_QUICKDRAW
	NP_Port*						m_np_port;
# endif
	OpAutoVector<SentKey>			m_sent_keys;
# ifndef SIXTY_FOUR_BIT
	EventHandlerRef					m_key_handler;
# endif
	NP_CGContext*					m_np_context;

    INT32							m_last_width;
	INT32							m_last_height;
	GWorldPtr						m_qd_port;
	NPBool							m_last_window_focus_sent;

	OpTimer* m_timer;
#endif // !NS4P_COMPONENT_PLUGINS

#ifndef SIXTY_FOUR_BIT
	static ScriptLanguageRecord		s_slRec;
#endif
	
	MacOpPluginLayer                *m_plugin_layer;
	INT32							m_x;
	INT32							m_y;
	INT32							m_width;
	INT32							m_height;
	INT32							m_title_height;
	INT32							m_win_height;
	OpRect							m_last_rect;
	BOOL							m_use_gworld;
	BOOL							m_was_suppressed;
	BOOL							m_hw_accel;
	OpRect							m_painter_clip_rect;
	CGDataProviderRef				m_provider;
	bool 							m_drawing_model_inited;
	NPDrawingModel					m_drawing_model;
	NPEventModel					m_event_model;
	OpRect							m_plugin_rect;
	bool							m_updated_text_input;
	bool							m_transparent;

public:
#ifndef NS4P_COMPONENT_PLUGINS
# ifndef NP_NO_QUICKDRAW
    // Caller is responsible for deleting object
	NP_Port *GetNPPort();
# endif
#endif // !NS4P_COMPONENT_PLUGINS
    
	MacOpPluginWindow();
	virtual ~MacOpPluginWindow();

	virtual OP_STATUS Construct(const OpRect &rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window);

	virtual void SetIsJavaWindow() { };

	virtual void Show();
	virtual void Hide();

	virtual void SetPos(int x, int y);
	virtual void SetSize(int w, int h);

	virtual void* GetHandle();

	virtual void SetListener(OpPluginWindowListener *listener) { m_listener = listener; }

	virtual void BlockMouseInput(BOOL block) { m_blocked_input = block; }
	virtual BOOL IsBlocked() { return m_blocked_input; }

	OpPluginWindowListener* GetListener() { return m_listener; }

	virtual void SaveState(OpPluginEventType event);
	virtual void RestoreState(OpPluginEventType event);
	virtual OP_STATUS CreateMouseEvent(OpPlatformEvent** event, OpPluginEventType event_type, const OpPoint& point, int button, ShiftKeyState modifiers);
	virtual OP_STATUS CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& rect);
	virtual OP_STATUS CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers);
	virtual OP_STATUS CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in);
	virtual OP_STATUS CreateWindowPosChangedEvent(OpPlatformEvent** event) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent);
	virtual OP_STATUS SetupNPPrint(NPPrint* npprint, const OpRect& rect);
	virtual BOOL SendEvent(OpPlatformEvent* event);

	virtual void Detach();

	// Inherited from DesktopWindowListener.
	virtual void OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	virtual void OnDesktopWindowMoved(DesktopWindow* desktop_window);
	virtual void OnDesktopWindowParentChanged(DesktopWindow* desktop_window);

	// See documentation of OpPluginWindow::CheckPaintEvent for following value.
	virtual unsigned int CheckPaintEvent() { return 1; };

	/** Ask platform whether it supports direct painting. */

	virtual BOOL UsesDirectPaint() const { return UsesPluginNativeWindow(); }

	/** Paint directly, without having to call back core to handle painting. */

	virtual OP_STATUS PaintDirectly(const OpRect& paintrect);
	
	virtual BOOL UsesPluginNativeWindow() const;

	void	SetLayerFrame();
#ifndef NS4P_COMPONENT_PLUGINS
    void    SetPluginLayerFrame();
#endif // !NS4P_COMPONENT_PLUGINS
	
	/** Set plugin instance. */

	virtual void SetPluginObject(Plugin* plugin);

#ifdef NS4P_COMPONENT_PLUGINS
	void	UpdatePluginView();

	virtual OpView *GetParentView() { return m_parent_view; }
	virtual void SetClipRegion(MDE_Region* rgn);
#endif // NS4P_COMPONENT_PLUGINS

	OP_STATUS ConvertPoint(double sourceX, double sourceY, int sourceSpace, double* destX, double* destY, int destSpace);
	OP_STATUS PopUpContextMenu(void* menu);

	INT32	GetX() const { return m_x; }
	INT32	GetY() const { return m_y; }
	INT32	GetWidth() const { return m_width; }
	INT32	GetHeight() const { return m_height; }
	UINT	GetPressedMouseButtons();

#ifndef NS4P_COMPONENT_PLUGINS
# ifndef SIXTY_FOUR_BIT
	static OSStatus CocoaCarbonIMEEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *inUserData);
	static OSStatus KeyEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *inUserData);
# endif

    virtual void OnTimeOut(OpTimer* timer);
    BOOL	ShouldPluginBeVisible();
#endif // !NS4P_COMPONENT_PLUGINS

    double GetBackingScaleFactor();

private:
	OP_STATUS	CreateCALayer();
	void		DestroyCALayer();

    int         GetPixelScalerScale();
	OperaNSView *GetOperaNSView();

public:
	void		UpdateCALayer(const OpRect& rect);
	void		ShowCALayer(BOOL show);

	BOOL	IsHWAccelerated() { return m_hw_accel; }
	Plugin* GetPlugin() { return m_plugin; }

#ifndef NS4P_COMPONENT_PLUGINS
    void SendIMEText(UniString &text);
#endif // !NS4P_COMPONENT_PLUGINS

	NPDrawingModel		GetDrawingModel() const { return m_drawing_model; }
	void				SetDrawingModel(NPDrawingModel drawing_model) { m_drawing_model_inited = true; m_drawing_model = drawing_model; }
	NPEventModel		GetEventModel() const { return m_event_model; }
	void				SetEventModel(NPEventModel event_model) { m_event_model = event_model; }
	bool				GetSupportsUpdatedCocoaTextInput() const { return m_updated_text_input; }
	void				SetSupportsUpdatedCocoaTextInput(bool updated_text_input) { m_updated_text_input = updated_text_input; }
	void				SetTransparent(bool transparent) { m_transparent = transparent; }

#ifdef NS4P_COMPONENT_PLUGINS
	void			SetPluginChannel(OpChannel *channel);
	void			SendOpMacPluginDesktopWindowMovementMessage();
	void			SendOpMacPluginVisibilityMessage(BOOL visible);
	virtual UINT64	GetWindowIdentifier();
#endif // NS4P_COMPONENT_PLUGINS
    
private:
#ifdef NS4P_COMPONENT_PLUGINS
    OP_STATUS CreateIOSurface(const OpRect& rect);
    
	IOSurfaceRef m_surface;
    IOSurfaceID  m_surface_id;
	
	OpChannel*          m_plugin_channel;
#endif // NS4P_COMPONENT_PLUGINS

	DesktopWindow*      m_desktop_window;
	DesktopWindow*      m_desktop_window_this_tab;
};

/***********************************************************************************
 **
 **	MacOpPlatformKeyEventData
 **
 ***********************************************************************************/

struct MacKeyData
{
	unsigned short m_key_code;
	uni_char m_sent_char;
};

class MacOpPlatformKeyEventData: public OpPlatformKeyEventData
{
public:
	MacOpPlatformKeyEventData(MacKeyData* key_data)
	: m_ref_count(1)
	{
		m_key_data = *key_data;
	}

	virtual ~MacOpPlatformKeyEventData()
	{
	}

	virtual void GetPluginPlatformData(UINT64& data1, UINT64& data2)
	{
		data1 = m_key_data.m_key_code;
		data2 = m_key_data.m_sent_char;
	}

	MacKeyData m_key_data;
	unsigned short m_ref_count;
};

#endif // _PLUGIN_SUPPORT_

#endif //  MACOPPLUGINWINDOW_H__
