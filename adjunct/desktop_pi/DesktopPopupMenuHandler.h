/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author George Refseth (rfz) / Arjan van Leeuwen (arjanl)
 */


#ifndef DESKTOP_POPUP_MENU_HANDLER_H
#define DESKTOP_POPUP_MENU_HANDLER_H

#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/windowcommander/OpWindowCommander.h"

/** @brief Handle popup menus on the platform
  * Quick UI does not implement popup menus, but it understands the concept and tells the
  * menu handler to show some kind of popup menu.
  *
  * There is no requirement for the menus to be blocking anymore.
  *
  * Typically, the host will want to use the AddMenuItems helper function in Application to populate
  * a platform specific menu container with something based on the name it got passed in OnPopupMenu,
  * in which case the menu will be populated according to the menu.ini file
  * The host implementation will probably look something like this:
  *
  * OnPopupMenu(const uni_char* popup_name, INTPTR popup_value, PopupPlacement placement, OpPoint* point, OpRect* avoid_rect, BOOL use_keyboard_context)
  * {
  * 	MyPlatformPopupMenu* menu = CreateMyPlatformPopupMenu();
  *
  * 	g_application->GetMenuHandler()->AddMenuItems(menu, popup_name, popup_value, 0, use_keyboard_context ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext());
  *
  * 	menu->Show(point, avoid_rect);
  * }
  *
  * When calling AddMenuItems(), the OnAddMenuItem callback will be called for each item. See below.
  *
  * Otherwise, the host might have some kind of fixed resource based menu system, in which case it will just
  * use the popup name to look up the correct menu.

  * In any case, there is no return code etc, actions will happen by the host invoking actions on the input manager.
  */
class DesktopPopupMenuHandler
{
public:
	virtual ~DesktopPopupMenuHandler() {}

	/** Create a DesktopPopupMenuHandler object. */
	static OP_STATUS Create(DesktopPopupMenuHandler** menu_handler);

	/** Show a popup menu
	 * @param popup_name menu name as described in resource files
	 * @param placement see PopupPlacement
	 * @param point_in_page If the point is in a webpage, this is typically used
	 *   by platform(Windows) to decide after the menu is showed
	 *   whether the click on the avoid_rect should be ate.
	 *   (On Windows the click won't be passed to core as otherwise
	 *   the menu will be displayed again, but in a webpage it shouldn't
	 *   be blocked to make it possible to double/triple click even
	 *   when the menu is shown).
	 * @param popup_value menu-specific data with some context information
	 * @param use_keyboard_context TRUE if the menu was opened using a keyboard shortcut
	 */
	virtual void OnPopupMenu(const char* popup_name, const PopupPlacement& placement, BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context) = 0;

	/** If AddMenuItems is called, OnAddMenuItem will be called for each item that should be added to the
	  * platform specific (void*) menu container. The parameters that are passed are explained here:
	  * 
	  * @param menu The platform specific handle the host passed to AddMenuItems
	  * @param seperator The item is just a seperator. Ignore all other fields.
	  * @param item_name Name of item (already translated)
	  * @param image Image you could use or NULL!. Only valid during this call. Copy the info you need from it.
	  *        (for example, create your own widgetimage and just SetWidgetImage to clone it)
	  * @param shortcut A generated string like "Ctrl+Alt+N" that the host may want to display in the menu too.
	  * @param sub_menu_name The item has a submenu, and this is its name. Wether the host want to
	  *        recursivly create new menus right now and call AddMenuItems, or wait for it to
	  *        be displayed is up to the host.
	  * @param sub_menu_value The sub menu value, also passed on to a following call to AddMenuItems
	  * @param sub_menu_start The sub menu start valeue. Pass this as "start_at_index" to AddMenuItems
	  * @param checked Consider the item checked. (draw a checkmark)
	  * @param selected Consider the item selected (draw a bullet, not a checkmark)
	  * @param disabled Consider the item disabled. (gray it)
	  * @param bold Consider the item bolded. (bold it! :)
	  * @param action The action that this item should invoke. This one is allocated for you. Host must use
	  *        DeleteInputActions to free them after use.
	  * @return OnAddMenuItem should normally return FALSE unless it detects that "there soon will be no more vertical space for items".
	  *         In reality that means you should return TRUE when there is only room for 2 more items, because then Application
	  *         will probably append a seperator and a "More items ->" submenu to it instead of continuing when the menu.
	  *         So it's not an error value! Just a hint back that it should wrap it up soon.. :)
	  */
	virtual BOOL OnAddMenuItem(void* menu, BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, BOOL checked, BOOL selected, BOOL disabled, BOOL bold, OpInputAction* input_action) = 0;

	/** Special case for adding platform specific shell menu items.. */
	virtual void	OnAddShellMenuItems(void* menu, const uni_char* path) = 0;

	/** Add a break to a new column */
	virtual BOOL OnAddBreakMenuItem() { return FALSE;}

	/** When this is called, the underline to show the keyboard shortcut should be painted regardless
	*/
	virtual void OnForceShortcutUnderline() {}
	
	/** Allows to build platform dependent menu basing on menu name
	 */
	virtual void BuildPlatformCustomMenuByName(void* menu, const char* menu_name) {}
	
	
	/**
	 * List all menus as QuickMenuInfo items to Scope (for automated UI testing)
	 *
	 * @param out List of all menus that are open
	 *
	 */
	virtual BOOL ListPopupMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuList &out) { return FALSE; }


};

#endif // DESKTOP_POPUP_MENU_HANDLER_H
