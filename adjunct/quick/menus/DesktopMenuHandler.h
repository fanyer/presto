/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOP_MENU_HANDLER_H
#define DESKTOP_MENU_HANDLER_H

#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"
#include "modules/inputmanager/inputcontext.h"

class OpPoint;
class OpRect;
class PrefsSection;
class DesktopMenuContext;


/**
 * Transports information on the desired popup placement strategy.
 */
struct PopupPlacement
{
	enum Type
	{
		POPUP_CURSOR,		///< pop up from the current mouse cursor position
		POPUP_RIGHT,		///< pop up to the right of a rect, or to the left if there's no room
		POPUP_BELOW,		///< pop up below a rect, or above if there's no room
		POPUP_CENTER,		///< pop up to make the menu center the center of a rect
		POPUP_WINDOW_CENTER	///< pop up in the center of the window
	};

	static PopupPlacement AnchorAtCursor()
		{ return PopupPlacement(POPUP_CURSOR, OpRect()); }

	static PopupPlacement AnchorAt(const OpPoint& point, BOOL center = FALSE)
		{ return PopupPlacement(center ? POPUP_CENTER : POPUP_RIGHT, OpRect(point.x, point.y, 0, 0)); }

	static PopupPlacement AnchorBelow(const OpRect& rect)
		{ return PopupPlacement(POPUP_BELOW, rect); }

	static PopupPlacement AnchorRightOf(const OpRect& rect)
		{ return PopupPlacement(POPUP_RIGHT, rect); }

	static PopupPlacement CenterInWindow()
		{ return PopupPlacement(POPUP_WINDOW_CENTER, OpRect()); }

	Type type;
	OpRect rect;

private:
	PopupPlacement(Type type, const OpRect& rect) : type(type), rect(rect) {}
};


class DesktopMenuHandler
{
public:
	virtual ~DesktopMenuHandler()  {}

	virtual PrefsSection* GetMenuSection(const char* menu_name) = 0;

	/**
	 * Show a popup menu
	 *
	 * There several type of menus:
	 * - Dynamic menus which represent some kind of dynamic data set within opera (for instance bookmarks).
	 *   Usually they have a name that starts with "Internal " or other hardcoded names.
	 * - Static menus that are defined in standard_menu.ini
	 * @param popup_name Name of the popup menu that should be loaded. This name matches an "Internal ..."
	 *   popup menu name, or other hardcoded name, or a section defined in standard_menu.ini or other resource files.
	 * @param placement see PopupPlacement
	 * @param popup_value A parameter to be passed to the actions executed from menu entries
	 *   If you use this with static menus, make sure you add -2 as a action parameter in standard_menu.ini,
	 *   otherwise it uses the default value: 0.
	 * @param use_keyboard_context TRUE if the menu was opened using a keyboard shortcut
	 * @param context contextual information about the point where the menu appears
	 * @see DesktopPopupMenuHandler::OnPopupMenu
	 */
	virtual void ShowPopupMenu(const char* popup_name, const PopupPlacement& placement, INTPTR popup_value = 0, BOOL use_keyboard_context = FALSE, DesktopMenuContext* context = NULL) = 0;

	virtual BOOL IsShowingMenu() const = 0;

	virtual void OnMenuShown(BOOL shown, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info) = 0;

	/**
	 * Fills the menu with items
	 *
	 * This method is called for each menu and each submenu separately.
	 * Usually this happens after ShowPopupMenu() is called.
	 * When the menu is about to become visible it is populated with this method.
	 * @param platform_handle OS-specific menu object pointer
	 * @param menu_name Name of the menu that should be loaded. This name matches an "Internal ..."
	 *   menu name, or other hardcoded name, or a section defined in standard_menu.ini or other resource files.
	 * @param menu_value Menu-specific data that can be passed from a parent menu to a child menu,
	 *   or to the actions executed from menu entries.
	 * @param start_at_index ???
	 * @param menu_input_context input context
	 * @param is_sub_menu TRUE if this menu is a child menu opened from another popup menu
	 * @see QuickMenu::AddMenuItems
	 * @see DesktopMenuHandler::ShowPopupMenu
	 */
	virtual void AddMenuItems(void* platform_handle, const char* menu_name,
			INTPTR menu_value, INT32 start_at_index,
			OpInputContext* menu_input_context, BOOL is_sub_menu = FALSE) = 0;

	virtual void SetSpellSessionID(int id) = 0;
    virtual int GetSpellSessionID() const = 0;
    
#ifdef EMBROWSER_SUPPORT
	virtual void SetContextMenuWasBlocked(BOOL blocked) = 0;
	virtual BOOL GetContextMenuWasBlocked() = 0;
#endif // EMBROWSER_SUPPORT
};

#endif // DESKTOP_MENU_HANDLER_H
