/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Espen Sand
 */

#ifndef X11_DESKTOP_POPUP_MENU_HANDLER_H
#define X11_DESKTOP_POPUP_MENU_HANDLER_H

#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"

class OpInputContext;
class PopupMenu;

class X11DesktopPopupMenuHandler : public DesktopPopupMenuHandler
{
public:
	X11DesktopPopupMenuHandler() : menu_ctx(0) {}

	void OnPopupMenu(const char* popup_name, const PopupPlacement& placement, BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context);

	BOOL OnAddMenuItem(void* menu, BOOL separator, const uni_char* item_name,
					   OpWidgetImage* image, const uni_char* shortcut,
					   const char* sub_menu_name, INTPTR sub_menu_value,
					   INT32 sub_menu_start, BOOL checked, BOOL selected,
					   BOOL disabled, BOOL bold, OpInputAction* input_action);

	void OnAddShellMenuItems(void* menu, const uni_char* path) {}

	BOOL ListPopupMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuList &out);

	/**
	 * Opens a bookmark context menu. 
	 *
	 * @param Upper left location of menu in global coordinate
	 * @parem caller The popup menu from where this menu is called. Can be 0
	 * @param link_action The link action to be used in the menu
	 */
	static void ShowBookmarkContextMenu(const OpPoint& pos, PopupMenu* caller, OpInputAction* link_action);


private:
	OpInputContext *menu_ctx;
};

#endif // X11_DESKTOP_POPUP_MENU_HANDLER_H
