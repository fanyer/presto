/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef POPUPMENU_H
#define POPUPMENU_H


#include "menuelement.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/pi/OpView.h"
#include "modules/skin/OpWidgetImage.h"
#include "modules/util/simset.h"
#include "platforms/unix/base/x11/x11_widget.h"

#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"

class OpFont;
class OpInputAction;
class OpWidget;
class OpWindow;
class PopupMenuItem;
class PopupMenuComponent;

class PopupMenu 
	:public MenuElement
	,public OpPaintListener 
	,public OpMouseListener
	,public X11Widget::InputEventListener
	,public OpTimerListener
{
public:
	enum ContextType
	{
		UnknownContext, KeyboardContext, MouseContext
	};

	class PopupMenuListener
	{
	public:
		virtual void OnPopupOpened() = 0;
		virtual void OnPopupClosed() = 0;
	};

	enum AnchorType { BELOW_OR_ABOVE, RIGHT_OR_LEFT_OF };

private:
	struct SubMenuData
	{
		SubMenuData() { value = 0; start = 0;}
		OpString8 name;
		INTPTR value;
		INT32 start;
	};

public:
	PopupMenu();
	~PopupMenu();
	OP_STATUS Init() { return OpStatus::OK; } // For OpMouseListener. What's the point?
	OP_STATUS Init(OpView *parent);

	/**
	 * Appends an item or a sub menu entry to the menu
	 */
	OP_STATUS AddItem(const uni_char* name, const uni_char* shortcut, OpInputAction* action, UINT32 state, OpWidgetImage* image, PopupMenu* submenu=0);

	/**
	 * Appends a separator to the menu. If last item already is a separator no 
	 * new separator is created.
	 */
	OP_STATUS AddSeparator();

	/**
	 * Sets the selected item. Inf index is less than 0 
	 * the selected item will be cleared
	 */
	void SetSelectedItem(INT32 index);

	/**
	 * Sets that menu element that acts as the parent of this menu 
	 */
	void SetParent(MenuElement *parent) {m_parent_menu = parent;}
	
	/**
	 * Sets the context to be used when executing an action 
	 */
	void SetExecContext( ContextType context_type ) { m_context_type = context_type; }

	/**
	 * Returns the context to be used when executing an action 
	 */
	ContextType GetExecContext() const { return m_context_type; }

	/**
	 * Sets data used for opening a submenu
	 *
	 * @return OpStatus::OK on success
	 */
	OP_STATUS SetSubMenuData(const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start);

	/**
	 * Sets this menu as the hotclick menu or removes all
	 * references to a hotclick menu
	 *
	 * @param is_hotclick Set if TRUE, otherwise reset
	 */
	void SetIsHotClickMenu(BOOL is_hotclick) { m_hotclick_menu = is_hotclick ? this : 0; }
	
	/**
	 * Cancels (hide and do not execute any action) the menu
	 */
	void Cancel();

	/**
	 * Repaints the entire window
	 */
	void Invalidate();

	/**
	 * Returns currently selected action. 
	 *
	 * @return Returns 0 if not slected item or menu was cancelled
	 */
	OpInputAction* TakeInputAction();

	/**
	 * Returns TRUE if menu has any content, otherwise FALSE
	 */
	BOOL HasContent() const { return !!m_menu_items.First(); }

	/**
	 * Returns TRUE if the height of the menu will extend the 
	 * screen height if adding a new normal item
	 */
	BOOL SoonTooTall();

	/**
	 * Returns width of menu should it be shown right now
	 */
	UINT32 GetWidth() const;

	/**
	 * Returns height of menu should it be shown right now
	 */
	UINT32 GetHeight() const;

	/**
	 * Returns item height.
	 */
	UINT32 GetItemHeight() const;

	/**
	 * Computes item position relative to menu window
	 * 
	 * @return true if item (and thus x and y) is valid
	 */
	bool GetItemPosition(PopupMenuComponent* item, INT32& x, INT32& y);

	/**
	 * Returns separator height
	 */
	UINT32 GetSeparatorHeight() const;

  	/**
	 * Returns internal view
	 */
	OpView *GetOpView() const { return m_opview; }

	/**
	 * @return Font to use for bold menu items
	 */
	OpFont* GetBoldFont();

	/**
	 * @return Font to use for normal menu items (non-bold)
	 */
	OpFont* GetNormalFont();

	/**
	 * Returns TRUE if a menu can be closed. This is just an advisory. 
	 * Automatically opened sub menus can return FALSE if not enough time
	 * has elapsed since they were opened
	 *
	 * @return See above
	 */
	BOOL CanHideNow() const;

	/**
	 * Returns a pointer to the menu (itself, a sub menu, sub sub menu etc)
	 * that is visible and contains the global coordinate. 0 is returned on 
	 * no match
	 *
	 * @param gx Global X coordinate
	 * @param gy Global Y coordinate
	 * @param include_submenu Include sub menu(s) in test if TRUE 
	 *
	 * @return See above
	 */
	PopupMenu* Contains(int gx, int gy, BOOL include_submenu);


	void OnItemHover(PopupMenuComponent* item);
	void ClearStatusBar();

	/**
	 * Grabs mouse and keyboard 
	 */
	void Grab();

	/**
	 * Moves this menu to a new position.
	 *
	 * @param anchor_type how to compute the position from @a anchor_rect
	 * @param anchor_rect calculate the position as adjacent to this rect.
	 * 		Screen coordinates are assumed.
	 */
	void MoveTo(AnchorType anchor_type, const OpRect& anchor_rect);

	/*
	 * Implementation of MenuElement
	 */
	bool IsMenuBar() const { return false; }
	void Stop(bool cancel=false);
	void CancelSubMenu();
	void ShowNext(MenuElement* current, bool next);
	bool HandleMouseEvent(int gx, int gy, MouseEvent event, bool confine_to_area);
	bool ShowSubmenu(int gx, int gy);
	MenuElement* Hide(int gx, int gy);
	PopupMenuComponent* ItemAt(int gx, int gy);

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
	BOOL OnMouseWheel(INT32 delta, BOOL vertical, ShiftKeyState shift_state ) { return TRUE; }
	void OnSetCursor();
	void OnMouseCaptureRelease();

	/**
	 * Implementation of X11Widget::InputEventListener
	 */
	BOOL OnInputEvent(XEvent* e);

	/**
	 * Implementation of OpTimerListener
	 */
	void OnTimeOut(OpTimer* timer);

	void PostPendingMenu();

	/**
	 * Specify Opera menu
	 *
	 * @param is_o_menu The menu is an O-menu if true
	 */
	void SetIsOMenu(bool is_o_menu) { m_is_o_menu = is_o_menu; }

	/**
	 * Set popup listener. There can only be one listener
	 */
	void SetPopupMenuListener(PopupMenuListener* listener) {m_listener = listener;}

	/**
	 * For Scope Automated UI testing. 
	 *
	 * @param menu_name The name of the menu
	 * @param info Scope Info structure that is to be be filled with information about the menu
	 */
	OP_STATUS SetQuickMenuInfo(class OpScopeDesktopWindowManager_SI::QuickMenuInfo& info);

	/**
	 * For Scope Automated testing
	 *
	 * @param menu The menu that is about to be shown/closed
	 * @param show TRUE or FALSE
	 */
	void SendOnMenuShown(PopupMenu* menu, BOOL show);


	/**
	 * Sets this menu as the topmost popup menu 
	 *  (used so scope can retrieve the open menus)
	 *
	 */
	void SetIsTopPopupMenu() { m_topmost_popup_menu = this; }
	
	/**
	 * Sets this menu as the bookmark context menu
	 *  (used so Scope can retrieve all open menus)
	 */
	void SetIsBookmarkContextMenu() { m_bookmark_context_menu = this; }

	/**
	 * Set the name to be used to identify this menu to Scope
	 * (this name is normally the one found in standard_menu.ini)
	 */
	void SetScopeName(const char* name) { m_scope_name.Set(name); }

	/**
	 * Get ScopeName that can be used to identify the menu to Scope
	 */
	const OpStringC& GetScopeName() { return m_scope_name;}


	/**
	 * Opens a menu and invokes the action if user chooses to do so.
	 *
	 * @param menu the menu to show
	 * @param anchor_type how to compute the position from @a anchor_rect
	 * @param anchor_rect calculate the position as adjacent to this rect.
	 * 		Screen coordinates are assumed.
	 * @param delete_menu delete menu before action is invoked if @c true. The
	 *        menu is not deleted at all if @c false.
	 * @param invoke_action invoke the selected action before the function
	 * 		returns
	 *
	 * @return the selected action type or OpInputAction::ACTION_UNKNOWN if the
	 *		menu is canceled
	 */
	static OpInputAction::Action Exec(PopupMenu* menu, AnchorType anchor_type, const OpRect& anchor_rect, bool delete_menu, bool invoke_action);

	static OpInputAction::Action Exec(PopupMenu* menu, const OpPoint& anchor_point, bool delete_menu, bool invoke_action)
		{ return Exec(menu, BELOW_OR_ABOVE, OpRect(anchor_point.x, anchor_point.y, 0, 0), delete_menu, invoke_action); }

	/**
	 * Returns the active hotclick menu, if any
	 */
	static PopupMenu* GetHotClickMenu();

	/**
	 * Returns the topmost (not a submenu) popupmenu open, if any
	 */
	static PopupMenu* GetTopPopupMenu();

	/**
	 * Returns the bookmark context menu (from rightclicking a bookmark menu item) 
	 * if it is Open, otherwise NULL
	 */
	static PopupMenu* GetBookmarkContextMenu();

	/**
	 * Returns the submenu of this menu that is currently open, if any
	 */
	PopupMenu* GetOpenSubMenu() { return m_open_submenu; }

	/**
	 * Returns TRUE if event shall be blocked from further processing
	 *
	 * @param e X11 event to test
	 *
	 * @return TRUE if event should be filtered, otherwise FALSE
	 */
	static BOOL FilterEvent(XEvent* e);
	static bool SupportMouseHover();

private:
	void AboutToShow();
	void Show(AnchorType anchor_type, const OpRect& anchor_rect);
	void Hide();
	void Exec(AnchorType anchor_type, const OpRect& anchor_rect);
	bool ShowSubmenu(bool activate);
	bool IsSubmenu() const { return m_parent_menu != NULL; }

	/**
	 * Select and repaint all items in the parent menu chain
	 * if pupup menu contains a selected item
	 */
	void SelectParents();

	/**
	 * Converts the position into a global coordinate
	 *
	 * @param point Local position in window
	 *
	 * @return The global position
	 */
	OpPoint ConvertToScreen(const OpPoint& point);

	/**
	 * @param anchor_type how to compute the position from @a anchor_rect
	 * @param anchor_rect calculate the position as adjacent to this rect.
	 * 		Screen coordinates are assumed.
	 * @return menu position in screen coordinates
	 */
	OpPoint CalculateMenuPosition(AnchorType anchor_type, const OpRect& anchor_rect) const;

	/**
	 * Returns the menu item located a the coordinate or 0 
	 * if outside menu area. 
	 *
	 * @param x X position (local to the window)
	 * @param y Y position (local to the window)
	 *
	 * @return The item or 0
	 */
	PopupMenuComponent* GetItem(INT32 x, INT32 y);

	/**
	 * Returns the menu item located a the y coordinate. 
	 *
	 * @param x X position (local to the window)
	 *
	 * @return The item or 0
	 */
	PopupMenuComponent* GetItem(INT32 y) { return GetItem(0,y); }

	void PaintMenuFrame(OpPainter* painter, const OpRect& view_rect, const OpRect& rect);


private:
	Head m_menu_items;
	X11OpWindow* m_opwindow;
	OpView* m_opview;
	OpWidget* m_paint_widget;
	OpFont* m_normalfont;
	OpFont* m_boldfont;
	MenuElement* m_parent_menu;
	PopupMenu* m_open_submenu;
	PopupMenuComponent* m_selected_item;
	PopupMenuComponent* m_mouse_selected_item;
	PopupMenuListener* m_listener;
	ContextType m_context_type;
	UINT32 m_image_width;
	UINT32 m_image_height;
	UINT32 m_max_text_width;
	UINT32 m_max_shortcut_width;
	double m_auto_shown_time;
	bool m_has_submenus;
	bool m_cancelled;
	bool m_mouse_has_moved;
	bool m_is_o_menu;
	SubMenuData m_submenu;
	OpTimer m_timer;
	OpString m_scope_name;

private:
	static PopupMenu* m_hotclick_menu;
	static bool m_block_mouse_up;
	static bool m_block_after_close;
	static OpInputAction* m_exec_action;
	static ContextType m_exec_context;
	static PopupMenu* m_topmost_popup_menu;
	static PopupMenu* m_bookmark_context_menu;

};



#endif // POPUPMENU_H
