/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_OP_PLUGIN_TRANSLATOR_H
#define WINDOWS_OP_PLUGIN_TRANSLATOR_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/pi/OpPluginTranslator.h"
#include "platforms/windows/IPC/SharedPluginBitmap.h"
#include "platforms/windows/windows_ui/hash_tables.h"

class PluginComponentInstance;

class WindowsOpPluginTranslator : public OpPluginTranslator
{
public:
	WindowsOpPluginTranslator(OpChannel* channel, PluginComponentInstance* component_instance);
	virtual ~WindowsOpPluginTranslator();

	HWND GetPluginWindowHWND() {return m_plugin_window;}

	static OP_STATUS	Create(OpPluginTranslator** translator, const OpMessageAddress& instance, const OpMessageAddress* adapter, const NPP npp);
	static bool			GetGlobalValue(NPNVariable variable, void* ret_value, NPError* result);

	virtual OP_STATUS	UpdateNPWindow(NPWindow& out_npwin, const OpRect& rect, const OpRect& clip_rect, NPWindowType type);
	virtual OP_STATUS	CreateFocusEvent(OpPlatformEvent** out_event, bool got_focus);
	virtual OP_STATUS	CreateKeyEventSequence(OtlList<OpPlatformEvent*>& events, OpKey::Code key, uni_char value, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64 data1, UINT64 data2);
	virtual OP_STATUS	CreateMouseEvent(OpPlatformEvent** out_event, OpPluginEventType event_type, const OpPoint& point, int button_or_delta, ShiftKeyState modifiers);
	virtual OP_STATUS	CreatePaintEvent(OpPlatformEvent** out_event, OpPluginImageID dest, OpPluginImageID bg, const OpRect& paint_rect, NPWindow* npwin, bool* npwindow_was_modified);
	virtual OP_STATUS	FinalizeOpPluginImage(OpPluginImageID image, const NPWindow& np_window);
	virtual OP_STATUS	CreateWindowPosChangedEvent(OpPlatformEvent** out_event);
	virtual bool		GetValue(NPNVariable variable, void* ret_value, NPError* result);
	virtual bool		SetValue(NPPVariable variable, void* value, NPError* result) { return false; }
	virtual bool		Invalidate(NPRect* rect) { return false; }
	virtual bool		ConvertPlatformRegionToRect(NPRegion invalid_region, NPRect &invalid_rect);
	virtual OP_STATUS	ConvertPoint(double src_x, double src_y, int src_space, double* dst_x, double* dst_y, int dst_space) { return OpStatus::ERR_NOT_SUPPORTED; }

	/** Override the window procedure of a plugin window
	 * @param hwnd Window handle to override window procedure for
	 */
	void				SubClassPluginWindowproc(HWND hwnd);

	static void			RegisterWrapperWndProcClass();
	static void			UnregisterWrapperWndProcClass();
	static LRESULT CALLBACK WrapperWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WrapperSubclassWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WrapperSubclassChildWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	/** Handles messages that we want to treat in a special way (implement a workaround or similar). */
	static bool			HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result);

	static void			CreateDummyPopupActivationWindow(HWND parent_window);
	static void			RemoveDummyPopupActivationWindow();
	static WINBOOL WINAPI TrackPopupMenuHook(HMENU menu, UINT flags, int x, int y, int reserved, HWND hwnd, const RECT* rect);

	static WINBOOL WINAPI GetWindowInfoHook(HWND hwnd, PWINDOWINFO pwi);

	/** Retrieve the WindowsOpPluginTranslator associated with a specific window handle
	  * @param hwnd Window handle to search for
	  * @param search_parents If true, will proceed to search for plugin associated with
	  *  the parents of @a hwnd until a plugin is found or there are no	more parents
	  * @param wrapper Contains the WindowsOpPluginTranslator associated with @a hwnd on success
	  * @return OpStatus::OK if a result was found, OpStatus::ERR if not found
	  */
	static OP_STATUS	GetPluginFromHWND(HWND hwnd, BOOL search_parents, WindowsOpPluginTranslator* &wrapper);

	static WindowsOpPluginTranslator*
					GetFocusedPlugin() { return s_focused_plugin; }
	static void		SetFocusedPlugin(WindowsOpPluginTranslator* plugin) { s_focused_plugin = plugin; }
	static HWND WINAPI	GetFocusHook();

private:
	static LRESULT CallSubclassedWindowProc(void* wndproc, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	static const uni_char
						s_wrapper_wndproc_class[];
	static OpWindowsPointerHashTable<const HWND, WindowsOpPluginTranslator*>
						s_wrapper_windows;
	static OpWindowsPointerHashTable<const HWND, void*>
						s_old_wndprocs;
	static HWND			s_dummy_contextmenu_window;
	static const uni_char
						s_dummy_activation_wndproc_class[];
	static OpINT32Vector
						s_browser_window_handles;
	static WindowsOpPluginTranslator*
						s_focused_plugin;
	static BOOL3		s_unsubclass_after_wndproc;

	OpChannel*			m_channel;				///< channel used for communicating with WindowsOpPluginAdapter
	HWND				m_plugin_window;		///< pointer to the plugin window on the browser side (windowed) or browser window (windowless)
	HWND				m_wrapper_window;		///< pointer the plugin window on the wrapper side
	OpRect				m_plugin_rect;			///< plugin rect in coordinates relative to the browser window
	HDC					m_hdc;					///< HDC of the browser window for windowless painting
	HRGN				m_old_cliprect;			///< cliprect taken from HDC before triggering paint, should be restore after paint
	int					m_clip_retval;			///< return value of GetClipRgn, 1 - has clipping, 0 - no clipping, -1 - error
	PluginComponentInstance* m_component_instance;

	WindowsSharedBitmap*
						m_shared_mem;
	HBITMAP				m_bitmap;
	bool				m_need_to_recreate_bitmap;
	OpPluginImageID		m_identifier;
};

class WindowsOpPlatformEvent : public OpPlatformEvent
{
public:
	virtual ~WindowsOpPlatformEvent()
	{
		if (m_event.event == WM_WINDOWPOSCHANGED && m_event.lParam)
			OP_DELETE(reinterpret_cast<WINDOWPOS*>(m_event.lParam));
		else if (m_event.event == WM_PAINT && m_event.lParam)
			OP_DELETE(reinterpret_cast<RECT*>(m_event.lParam));
	}

	virtual void*		GetEventData() const { return const_cast<NPEvent*>(&m_event); }

	NPEvent				m_event;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // WINDOWS_OP_PLUGIN_TRANSLATOR_H
