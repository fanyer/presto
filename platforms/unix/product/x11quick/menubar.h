/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef MENUBAR_H
#define MENUBAR_H

#include "popupmenu.h"

#include "modules/pi/OpView.h"
#include "modules/util/simset.h"
#include "modules/widgets/WidgetContainer.h"

#include "platforms/unix/base/x11/x11_callback.h"
#include "platforms/unix/base/x11/x11_windowdecoration.h" // WMButtonSet and MDIButtonSet
#include "platforms/unix/product/x11quick/x11_global_desktop_application.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"

#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/desktop_util/settings/SettingsListener.h"

class OpWindow;
class PopupMenu;
class VisualDevice;

class MenuBar
	:public OpInputContext
	,public MenuElement
	,public OpPaintListener
	,public OpMouseListener
	,public X11CbObject
	,public GlobalKeyEventListener
{

private:
	class MenuBarItem : public Link
	{
	public:
		MenuBarItem() : button(NULL), value(0), menu(NULL) { }
		~MenuBarItem() { OP_DELETE(menu); }

	public:
		class MenuBarButton* button;
		OpString8 name;
		OpString visible_name;
		int value;
		PopupMenu *menu;
	};

	struct Controls
	{
		class MenuBackgroundWidget* background;
		OpToolbarMenuButton* menu; // The 'opera' menu button
		WMButtonSet* wm_buttonset;
		MDIButtonSet* mdi_buttonset;
	};

public:
	enum DisplayMode
	{
		ROW_MENU,   // Content is displayed as a regular horizontal menu bar
		OPERA_MENU  // Content is displayed as an 'opera' menu (only one button visible)
	};
	void SetDisplayMode(DisplayMode mode);

public:
	MenuBar();
	~MenuBar();
	OP_STATUS Init(void *parent_handle, OpWindow* parent_window);

	/*
	 * Cancels any open sub menu and removes all contents
	 */
	void Reset();

	/**
	 * Load menu bar content from spec file
	 */
	OP_STATUS Populate();

	OP_STATUS AddPopupMenu(const char *name, const uni_char *visible_name, INTPTR value);

	// For scope ( watir, automated testing)
	OP_STATUS SetQuickMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuInfo& info);
	BOOL GetMenubarButtons(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out);

	UINT32 CalculateHeight();
	void SetSize(int width, int height);
	void SetGeometry(int x, int y, int width, int height);
	void Show();
	void Hide();
	bool IsInside(int x, int y, bool is_global_coordinates);
	void Invalidate();
	void Show(MenuBarItem* item);
	void Activate();
	void Deactivate();

	UINT32 Height() const { return m_visible ? m_height : 0; }
	X11Widget::MoveResizeAction GetAction(const OpPoint& point, UINT32 width, UINT32 height);
	X11Widget::MoveResizeAction UpdateCursorShape(const OpPoint& point);
	bool IsCaptured() const;
	bool IsVisible() const { return m_visible; }
	void SetCanMoveResize(bool can_move_resize) { m_can_move_resize = can_move_resize; }
	void SetDrawBorder(bool draw_border) { m_draw_border = draw_border; }
	void SetBorderColor(UINT32 border_color) { m_border_color = border_color; }

	void ShowControls(bool show_wm_buttons, bool show_mdi_buttons);
	OpView* GetOpView();

	/*
	 * Implementation of MenuElement
	 */
	bool IsMenuBar() const { return true; }
	void Stop(bool cancel=false);
	void CancelSubMenu();
	void ShowNext(MenuElement* current, bool next);
	bool HandleMouseEvent(int gx, int gy, MouseEvent event, bool confine_to_area);
	bool ShowSubmenu(int gx, int gy);
	MenuElement* Hide(int gx, int gy) { return 0; }
	class PopupMenuComponent* ItemAt(int gx, int gy) { return 0; }

	/*
	 * Implementation of OpPaintListener
	 */
	void OnPaint(const OpRect &rect, OpView *view);

	/*
	 * Implementation of OpMouseListener
	 */
	void OnMouseMove(const OpPoint &point, ShiftKeyState shift_state, OpView *view);
	void OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState shift_state, OpView *view);
	void OnMouseUp(MouseButton button, ShiftKeyState shift_state, OpView *view);
	void OnMouseLeave();
	BOOL OnMouseWheel(INT32 delta, BOOL vertical, ShiftKeyState shift_state) { return FALSE; }
	void OnSetCursor() {}
	void OnMouseCaptureRelease() {}

	/*
	 * Implementation of X11CbObject
	 */
	void HandleCallBack(INTPTR ev);

	/**
	 * Implementation of GlobalKeyEventListener
	 */
	bool OnGlobalKeyEvent(XEvent *event);

private:
	OP_STATUS PopulateMenu(MenuBarItem *m);
	MenuBarItem *GetMenuBarItemAt(const OpPoint &p, OpRect *item_dimensions);
	void HandleMouseEventInternal(const OpPoint &point, bool was_just_pressed);
	void SetLocalFocus(bool on);

private:
	Head m_menus;
	ToolkitLibrary::MenuBarLayout m_toolkit_layout;
	OpRect m_pending_exec_rect;
	OpAutoPtr<OpWindow> m_op_window;
	OpAutoPtr<OpView> m_op_view;
	OpAutoPtr<VisualDevice> m_vis_dev;
	OpAutoPtr<WidgetContainer> m_widget_container;
	OpAutoPtr<Controls> m_controls;
	const int m_corner_size;
	const int m_resize_margin;
	MenuBarItem* m_active_menu;
	UINT32 m_border_color; // Format 0xAABBGGRR
	UINT32 m_height;
	bool m_visible;
	bool m_has_open_menu;
	bool m_has_pending_exec;
	bool m_holds_focus;
	bool m_keep_focus;
	bool m_active;
	bool m_opened_with_mouse;
	bool m_captured;
	bool m_can_move_resize;
	bool m_draw_border;
};

#endif // MENUBAR_H
