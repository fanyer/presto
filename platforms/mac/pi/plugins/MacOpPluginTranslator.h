/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __MACOPPLUGINTRANSLATOR_H__
#define __MACOPPLUGINTRANSLATOR_H__

#if defined(_PLUGIN_SUPPORT_)

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/pi/OpPluginTranslator.h"
#include "platforms/mac/src/generated/g_message_mac_messages.h"
#include "platforms/mac/util/MacSharedMemory.h"
#include "platforms/mac/util/MachOCompatibility.h"

// Remove this define and the code inside it to get rid of
// the old-school plugin loading from the resource
// Currently this is only here for the VLC plugin, everything else
// loads with the new Info.plist code
#define MAC_OLD_RESOURCE_LOADING_SUPPORT

/**
 * A platform interface for loading, probing and initializing libraries
 * conforming to the Netscape Plug-in API (NPAPI).
 */
class MacOpPluginTranslator : public OpPluginTranslator, public OpMessageListener
{
	enum PluginName {
		None,
		Quicktime,
		Flip4Mac
	};

	class SentKey
	{
	public:
		SentKey(uni_char ch, unsigned short key) : ch(ch), key(key) {}
		uni_char ch;
		unsigned short key;
	};

public:
	MacOpPluginTranslator(const OpMessageAddress& instance, const OpMessageAddress* adapter, const NPP npp);
	~MacOpPluginTranslator();

	virtual OP_STATUS FinalizeOpPluginImage(OpPluginImageID image, const NPWindow& np_window);
	virtual OP_STATUS UpdateNPWindow(NPWindow& out_npwin, const OpRect& rect, const OpRect& clip_rect, NPWindowType type);
	virtual OP_STATUS CreateFocusEvent(OpPlatformEvent** out_event, bool got_focus);
	virtual OP_STATUS CreateKeyEventSequence(OtlList<OpPlatformEvent*>& out_events, OpKey::Code key, const uni_char key_value, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64 keycode, UINT64 sent_char);
	virtual OP_STATUS CreateMouseEvent(OpPlatformEvent** out_event,
	                                   const OpPluginEventType event_type,
	                                   const OpPoint& point,
	                                   const int button,
	                                   const ShiftKeyState modifiers);
	virtual OP_STATUS CreatePaintEvent(OpPlatformEvent** event, OpPluginImageID dest, OpPluginImageID bg, const OpRect& paint_rect, NPWindow* npwin, bool* npwindow_was_modified);
	virtual OP_STATUS CreateWindowPosChangedEvent(OpPlatformEvent** out_event);
	virtual bool GetValue(NPNVariable variable, void* ret_value, NPError* result);
	virtual bool SetValue(NPPVariable variable, void* value, NPError* result);
	virtual bool Invalidate(NPRect* rect);
	virtual bool ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect);
	virtual OP_STATUS ConvertPoint(const double src_x, const double src_y, const int src_space,
	                               double* dst_x, double* dst_y, const int dst_space);
	virtual OP_STATUS PopUpContextMenu(NPMenu* menu);

    void DrawInBuffer();
	void SetFullscreenMode();
	IOSurfaceID GetSurfaceID() { return m_surface_id; }
	
	void SendBrowserWindowShown(bool show);
	void SendBrowserCursorShown(bool show);

	OP_STATUS SendWindowFocusEvent(BOOL focus);

	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);
	void OnVisibilityMessage(OpMacPluginVisibilityMessage* message);
	void OnDesktopWindowMovementMessage(OpMacPluginDesktopWindowMovementMessage* message);
	void OnIMETextMessage(OpMacPluginIMETextMessage* message);

	static MacOpPluginTranslator *GetActivePluginTranslator();

private:
	void		SendFullscreenMessage(bool fullscreen);
	void		DeleteSurface();
	OP_STATUS	CreateAdapterChannel();

	OpMessageAddress	m_instance;	// Message address to send events like focus in/out to
	NPDrawingModel		m_drawing_model;
	NPEventModel		m_event_model;

# ifndef NP_NO_QUICKDRAW
	GWorldPtr			m_qd_port;
# endif // NP_NO_QUICKDRAW
	
	OpRect				m_plugin_rect;
	bool				m_resized;
	
	OpPoint				m_window_position;

	NPP					m_npp;

	void				*m_window;
    void                *m_view;

	// QuickDraw
#ifndef NP_NO_QUICKDRAW
	NP_Port				*m_np_port;
#endif
	// Core Graphics context
	NP_CGContext		*m_np_cgcontext;

	// IOSurface members
    CGLContextObj       m_cgl_ctx;
    IOSurfaceRef        m_surface;
    IOSurfaceID         m_surface_id;
    void                *m_renderer;
    void                *m_layer;
    GLuint              m_texture;
    GLuint				m_fbo;
    GLuint				m_render_buffer;
	void				*m_plugin_drawing_timer;
	void				*m_fullscreen_timer;
	
	OpAutoVector<SentKey> m_sent_keys;
	
	// Channel to MacOpPluginAdapter.
	OpChannel				*m_adapter_channel;
	OpMessageAddress		m_adapter;

	// Coordinates of the top-level desktop window in screen space.
	OpPoint				m_desktop_window_pos;

	MacSharedMemory		m_mac_shm;
	bool				m_fullscreen;
	bool				m_got_focus;

	bool				m_updated_text_input;

	PluginName			m_plugin_name;
	void				*m_plugin_menu_delegate;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // defined(_PLUGIN_SUPPORT_)

#endif // __MACOPPLUGINTRANSLATOR_H__
