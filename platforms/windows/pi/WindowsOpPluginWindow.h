/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OPPLUGINWINDOW_H
#define WINDOWS_OPPLUGINWINDOW_H

#ifndef NS4P_COMPONENT_PLUGINS

# include "platforms/windows/pi/WindowsOpPluginWindowLegacy.h"

#else // NS4P_COMPONENT_PLUGINS

#include "modules/ns4plugins/src/plugin.h"
#include "modules/pi/OpPluginWindow.h"

#include "platforms/windows/windows_ui/hash_tables.h"
#include "platforms/windows_common/pi_impl/pluginwindow.h"

typedef struct {
  uint32 left;
  uint32 top;
  uint32 right;
  uint32 bottom;
} NPRect32;

class OpWindowsPlatformEvent;

class WindowsOpPluginWindow : public OpPluginWindow
{
public:
	WindowsOpPluginWindow();
	~WindowsOpPluginWindow();

	OP_STATUS		Construct(const OpRect &rect, int scale, OpView* parent, BOOL windowless = FALSE);

	/** Reparent plugin window to another window. */
	void			ReparentPluginWindow();

	/** Check if given hwnd is one of existing plugin windows. */
	static WindowsOpPluginWindow*
					GetPluginWindowFromHWND(HWND hwnd, BOOL search_children = FALSE);

	/** Called when loading (some) libraries. Library will not nessacary be
	 * a plugin but in general most other libraries are loaded from system
	 * directory and will not trigger this call.
	 *
	 * @param hModule pointer to loaded module.
	 * @param path to the plugin library (should be full file path).
	 */
	static void		OnLibraryLoaded(HMODULE hModule, const uni_char* path);

#ifdef _PLUGIN_SUPPORT_
	static BOOL		PluginRegClassDef(HINSTANCE hInstance);
#endif // _PLUGIN_SUPPORT_

	static LRESULT CALLBACK
					PluginStaticWinProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	/** Check if plugin is windowles. */
	BOOL			IsWindowless() const { return m_windowless; }

#ifdef MANUAL_PLUGIN_ACTIVATION

	/** Get listener of plugin window. */
	OpPluginWindowListener*
					GetListener() { return m_listener; }
#endif // MANUAL_PLUGIN_ACTIVATION

	WindowsPluginNativeWindow*
					GetNativeWindow() { return m_hwnd_view; }

	OpView*			GetParentView() const { return m_parent_view; }

#ifdef NS4P_COMPONENT_PLUGINS
	UINT64			GetWindowIdentifier() {return reinterpret_cast<UINT64> (m_hwnd);}
#endif // NS4P_COMPONENT_PLUGINS

	/* Implement OpPluginWindow. */

	virtual void	Show();
	virtual void	Hide();
	virtual void	SetPos(int x, int y);
	virtual void	SetSize(int w, int h);
	virtual void*	GetHandle();

	/**
	 * Set activation state.
	 *
	 * There are differences in how windowed and windowless plugins are activated.
	 *
	 * For windowed, platform blocks events targeted at plugin window.
	 * The actual plugin wrapperwindow gets disabled so that this plugin
	 * window gets mouse (and keyboard) events itself.
	 *
	 * For windowless, plugin gets activated when platform's CreateMouseEvent
	 * gets a call from core to handle mouseup event. Core won't send events
	 * until it gets mousedown event from DOM. Platform activates plugin from
	 * first mouse up event that it gets from core.
	 */
	virtual void	BlockMouseInput(BOOL block);
	virtual BOOL	IsBlocked() { return m_input_blocked; }

	/** Set listener for plugin window. */
	virtual void	SetListener(OpPluginWindowListener* listener) { m_listener = listener; }

#ifdef NS4P_SILVERLIGHT_WORKAROUND
	/** Invalidate windowed plugin window directly. Required by Silverlight. */
	virtual void	InvalidateWindowed(const OpRect& rect);
#endif // NS4P_SILVERLIGHT_WORKAROUND

	virtual void	SetPluginObject(Plugin* plugin) { m_plugin = plugin; }

	/** Detach the window. */
	virtual void	Detach();

	/** DEPRECATED @{ */

	virtual BOOL	UsesDirectPaint() const { return FALSE; }
	virtual OP_STATUS
					PaintDirectly(const OpRect& rect) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					CreateMouseEvent(OpPlatformEvent** mouse_event, OpPluginEventType event_type, const OpPoint& point, int button_or_delta, ShiftKeyState modifiers) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& paint_rect) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					CreateWindowPosChangedEvent(OpPlatformEvent** pos_event) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual unsigned int
					CheckPaintEvent(){ return 0; }
	virtual OP_STATUS
					ConvertPoint(double sourceX, double sourceY, int sourceSpace, double* destX, double* destY, int destSpace) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					PopUpContextMenu(void* menu) { return OpStatus::ERR_NOT_SUPPORTED; }

	/** @} */

	/** Send an event to the plug-in window. Only used for windowed plug-ins. */
	virtual BOOL	SendEvent(OpPlatformEvent* event) { return FALSE; }

	static OpWindowsPointerHashTable<const HWND, WindowsOpPluginWindow *>
					s_plugin_windows;

	static BOOL CALLBACK	EnumChildWindowsProc(HWND hwnd, LPARAM lParam);

private:
	Plugin*			m_plugin;
	HWND			m_hwnd;
	HWND			m_plugin_wrapper_window; ///> For windowed plugins, HWND of the corresponding plugin window in the wrapper
	WindowsPluginNativeWindow* m_hwnd_view;
	OpView*			m_parent_view;
	BOOL			m_windowless;
#ifdef MANUAL_PLUGIN_ACTIVATION
	OpPluginWindowListener* m_listener; ///< set the plugin's window listener, used when ignoring mouse event in blocked state
	BOOL m_input_blocked;
#endif // MANUAL_PLUGIN_ACTIVATION
};
#endif // NS4P_COMPONENT_PLUGINS

/** OpWindowsPlatformKeyEventData is the windows representation of
    event data required for reconstructing platform key events. */
class OpWindowsPlatformKeyEventData
	: public OpPlatformKeyEventData
{
public:
	OpWindowsPlatformKeyEventData(WPARAM wParam, LPARAM lParam)
		: wParam(wParam)
		, lParam(lParam)
		, ref_count(1)
	{
	}

	virtual ~OpWindowsPlatformKeyEventData()
	{
	}

	virtual void GetPluginPlatformData(UINT64& data1, UINT64& data2)
	{
		data1 = wParam;
		data2 = lParam;
	}

	WPARAM wParam;
	LPARAM lParam;
	unsigned ref_count;
};


#endif // WINDOWS_OPPLUGINWINDOW_H
