/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OPPLUGINWINDOW_LEGACY_H
#define WINDOWS_OPPLUGINWINDOW_LEGACY_H

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

	/** Get handle to native window that is plugin's parent. */
	void*			GetParentWindowHandle();

	/** Check if direct painting should be used. */
	virtual BOOL	UsesDirectPaint() const { return TRUE; }

	/** Direct method of painting that dispatches events by itself. */
	virtual OP_STATUS
					PaintDirectly(const OpRect& rect);

#ifdef NS4P_SILVERLIGHT_WORKAROUND
	/** Invalidate windowed plugin window directly. Required by Silverlight. */
	virtual void	InvalidateWindowed(const OpRect& rect);
#endif // NS4P_SILVERLIGHT_WORKAROUND

	virtual void	SetPluginObject(Plugin* plugin) { m_plugin = plugin; }

	/** Detach the window. */
	virtual void	Detach();

	/** DEPRECATED @{ */

	virtual OP_STATUS
					CreateMouseEvent(OpPlatformEvent** mouse_event, OpPluginEventType event_type, const OpPoint& point, int button_or_delta, ShiftKeyState modifiers);
	virtual OP_STATUS
					CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& paint_rect) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers);
	virtual OP_STATUS
					CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in);
	virtual OP_STATUS
					CreateWindowPosChangedEvent(OpPlatformEvent** pos_event);
	virtual unsigned int
					CheckPaintEvent(){ return 0; }
	virtual OP_STATUS
					ConvertPoint(double sourceX, double sourceY, int sourceSpace, double* destX, double* destY, int destSpace) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS
					PopUpContextMenu(void* menu) { return OpStatus::ERR_NOT_SUPPORTED; }

	/** @} */


	/** Set VisualDevice for this plugin window. */
	void			SetVisualDevice(VisualDevice* vd) { m_vis_dev = vd; }

	/** Get VisualDevice for this plugin window, used when blitting image while painting. */
	VisualDevice*	GetVisualDevice() { return m_vis_dev; }

	/** Set NPWindow for this plugin window. */
	void			SetNPWindow(NPWindow* npwin) { m_npwin = npwin; }

	/** Get NPWindow for this plugin window, used when painting. */
	const NPWindow*	GetNPWindow() const { return m_npwin; }

	/** Set plugin rectangle. */
	void			SetRect(const OpRect& rect) { m_rect = rect; }

	/** Get plugin rectangle, used when handling mouse and paint events. */
	const OpRect&	GetRect() const { return m_rect; }

	/** Set view offset. */
	void			SetViewOffset(const OpPoint& offset) { m_view_offset = offset; }

	/** Get view offset, used when handling mouse and paint events. */
	const OpPoint&	GetViewOffset() const { return m_view_offset; }

	/** Set plugin rectangle relative to parent OpView. */
	void			SetViewRect(const OpRect& rect) { m_view_rect = rect; }

	/** Get rectangle relative to parent OpView, used when handling mouse and paint events. */
	const OpRect&	GetViewRect() const { return m_view_rect; }

	/** Set transparency of windowless plugin. */
	void			SetTransparent(BOOL transparent) { m_transparent = transparent; }

	/** Check transparency of windowless plugin. */
	BOOL			IsTransparent() const { return m_transparent; }

	BOOL			HandleEvent(OpWindowsPlatformEvent* ev, OpPluginEventType type);

	HDC				GetRenderContext();

	void			RenderWindowless(Plugin* plugin);

	/** Send an event to the plug-in window. Only used for windowed plug-ins. */
	virtual BOOL	SendEvent(OpPlatformEvent* event) { return FALSE; }
	static WindowsOpPluginWindow*
					GetFocusedPluginWindow();

	static void		SetFocusedPluginWindow(WindowsOpPluginWindow* pwindow);
	static BOOL		HandleFocusedPluginWindow(MSG* msg);

	static HWND WINAPI
					GetFocusHook();

	static OpWindowsPointerHashTable<const HWND, WindowsOpPluginWindow *>
					s_plugin_windows;

	static BOOL CALLBACK	EnumChildWindowsProc(HWND hwnd, LPARAM lParam);

private:
	Plugin*			m_plugin;
	HWND			m_hwnd;
	WindowsPluginNativeWindow* m_hwnd_view;
	OpView*			m_parent_view;
	BOOL			m_windowless;

	OpBitmap*		m_opbmp;
	HBITMAP			m_white_bitmap;
	HBITMAP			m_black_bitmap;
	UINT32*			m_white_bitmap_data;
	UINT32*			m_black_bitmap_data;
	HDC				m_hdc;

	BOOL			m_transparent; ///< indicate whether the windowless plugin is transparent or opaque
	VisualDevice*	m_vis_dev; ///< the visual device of the plugin window
	NPWindow*		m_npwin; ///< the plugin's window information
	OpRect			m_rect; ///< the plugin rectangle
	OpPoint			m_view_offset; ///< the offset of plugin's OpView
	OpRect			m_view_rect; ///< the plugin's rectangle relative to parent OpView
	UINT			m_key_message;
	WPARAM			m_key_wParam; ///< the focused and windowless plugin's key event information
	LPARAM			m_key_lParam; ///< the focused and windowless plugin's key event information
#ifdef MANUAL_PLUGIN_ACTIVATION
	OpPluginWindowListener* m_listener; ///< set the plugin's window listener, used when ignoring mouse event in blocked state
	BOOL m_input_blocked;
#endif // MANUAL_PLUGIN_ACTIVATION

	bool			m_paint_natively; ///< if true, will paint to the native window directly, otherwise go through visual device
};

static BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam);
static BOOL WINAPI	GetWindowInfoHook(HWND hwnd, PWINDOWINFO pwi);
static BOOL WINAPI	ExtTextOutHookA(HDC hdc, int X, int Y, UINT fuOptions, RECT *lprc, char* lpString, UINT cbCount, const INT *lpDx);

#endif // WINDOWS_OPPLUGINWINDOW_LEGACY_H
