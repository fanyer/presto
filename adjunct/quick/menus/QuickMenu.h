/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef QUICK_MENU_H
#define QUICK_MENU_H

#include "adjunct/quick/menus/MenuShortcutLetterFinder.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/inputmanager/inputcontext.h"

class Application;
class ClassicGlobalUiContext;
class DesktopMenuContext;
class DesktopMenuHandler;
class DocumentDesktopWindow;
class OpInputContext;
class OpLineParser;
class OpSession;
class OpWidgetImage;
class PrefsSection;
class OpPopupMenuRequestMessage;
class OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItemList;

/**
 * Implements menu populating.
 *
 * Application knows how to build menus based on their menu name and menu value.
 * There are basicly 2 types of menus:
 *
 * 	- External ones, where menu name doesn't start with "Internal".. those are
 * 	  simply populated based on the menu setup file.
 *  - Internal ones that Application itself knows how to populate, for example
 *    bookmarks etc.
 * 
 * For most internal ones, the menu name and menu value is enough data to be
 * able to know how to build the menu, but some very "context sensitive" menus
 * must first query the input manager for a specific object to be able to
 * populate the menu.
 *
 * Example: the "Internal Column List" menu needs a OpTreeView object.
 */
class QuickMenu
{
public:
	enum MenuType
	{
		MENU_ITEM,		///< Menu item with associated action
		MENU_INCLUDE,	///< Placeholder for another menu that should be included
		MENU_SUBMENU,	///< Expands into another menu
		MENU_SEPARATOR,	///< Separates menu items into groups
		MENU_BREAK		///< Breaks the menu bar into a new column
	};


	QuickMenu(ClassicGlobalUiContext* global_context, void* platform_handle,
			const char* menu_name, INTPTR menu_value, INT32 start_at_index,
			OpInputContext* menu_input_context);

	/**
	 * This constructor meant for testing.
	 */
	QuickMenu(DesktopMenuHandler& menu_handler, Application& application,
			ClassicGlobalUiContext* global_context, void* platform_handle,
			const char* menu_name, INTPTR menu_value, INT32 start_at_index,
			OpInputContext* menu_input_context);

	void AddMenuItems(const char* menu_name, INTPTR menu_value);

	OP_STATUS AddPopupMessageMenuItems(DesktopMenuContext* context, const char* menu_name, INTPTR menu_value, BOOL is_sub_menu);
	static void OnPopupMessageMenuItemClick(OpInputAction* action);

	static BOOL IsMenuEnabled(ClassicGlobalUiContext* global_context,
			const OpStringC8& menu_name, INTPTR menu_value,
			OpInputContext* menu_input_context);

	/**
	 * Parse a menu line from .ini files
	 * @param Application the application
	 * @param line Line to parse
	 * @param item_name Localized name of the menu entry
	 * @param default_item_name Unlocalized name of the menu entry
	 * @param sub_menu_name If this line represents a submenu, the internal name of that submenu
	 * @param sub_menu_value If this line represents a submenu, value to be used for submenu (?)
	 * @param menu_value Value to be used for menu (?)
	 * @param menu_type Type of the item parsed, see QuickMenu::MenuType declaration
	 * @param menu_image Name of the skin image to be used if this is a submenu (otherwise action skin image is used automatically)
	 * @return TRUE if the parsing was successful and the menu should be displayed
	 */
	static BOOL ParseMenuEntry(Application& application,
			OpLineParser* line,
			OpString& item_name, OpString& default_item_name,
			OpString8 &sub_menu_name, INTPTR& sub_menu_value,
			INTPTR& menu_value, MenuType& menu_type,
			BOOL& ghost_item, OpWidgetImage& widget_image);
	static BOOL ParseMenuEntry(OpLineParser* line, OpString& item_name,
			OpString& default_item_name, OpString8 &sub_menu_name,
			INTPTR& sub_menu_value, INTPTR& menu_value, MenuType& menu_type,
			BOOL& ghost_item, OpWidgetImage& widget_image);

	static PrefsSection* ReadMenuSection(const char* menu_name);

private:
	void Init(DesktopMenuHandler& menu_handler, Application& application,
			ClassicGlobalUiContext* global_context, void* platform_handle,
			const char* menu_name, INTPTR menu_value, INT32 start_at_index,
			OpInputContext* menu_input_context);

	void						AddExternalMenuItems(const char* menu_name, INTPTR menu_value);
	void						AddHotlistMenuItems(INT32 type, OpInputAction::Action item_action, const char* folder_menu_name, INT32 id);
	void						AddOpenInMenuItems(DesktopMenuContext* context);
	void						AddPersonalInformationMenuItems();
	void						AddMailFolderMenuItems(UINT32 parent_id);

								/** Add menu entries for mail access points, calls itself recursively with different parent_ids, 
								  * normally used with parent_id = 0 to list all access points
								  * @param parent_id - when 0 it will list all categories and accounts, 
								  * @param action can be OpInputAction::ACTION_READ_MAIL, ACTION_MAIL_HIDE_INDEX or OpInputAction::SEARCH_IN_INDEX
								  */
	void						AddAccessPointsMenuItems(INT32 parent_id, OpInputAction::Action menu_action, BOOL include_parent_item = TRUE);
	void						AddMailCategoryMenuItems(UINT32 index_id);
	void						AddPopularListItems();
	void						AddHistoryPopupItems(DocumentDesktopWindow* desktop_window, bool forward, bool fast);
	void						AddWindowListMenuItems();
	void						AddClosedWindowListMenuItems();
	void						AddBlockedPopupListMenuItems();
	void						AddSessionMenuItems(OpSession* session, OpInputAction::Action item_action);
	void						AddMailViewMenuItems(INT32 index_id);
	void						AddColumnMenuItems();
	void						AddToolbarExtenderMenuItems();
	void						AddPanelListMenuItems();

	BOOL						AddMenuItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const char* sub_menu_name, INTPTR sub_menu_value, OpInputAction* input_action, BOOL bold = FALSE, const uni_char* default_language_item_name = NULL, BOOL menu_break = FALSE, BOOL force_disabled = FALSE, BOOL ghost_item = FALSE);
	BOOL						AddMenuItem(const uni_char* item_name, OpWidgetImage* image, OpInputAction* input_action, BOOL bold = FALSE, BOOL menu_break = FALSE);
	BOOL						AddMenuItem(const uni_char* item_name, OpWidgetImage* image, OpInputAction::Action action, INTPTR action_data, BOOL bold = FALSE, BOOL menu_break = FALSE);
	BOOL						AddSubMenu(const uni_char* item_name, OpWidgetImage* image, const char* sub_menu_name, INTPTR sub_menu_value, OpInputAction* input_action=0, BOOL bold = FALSE );
	BOOL						AddMenuSeparator();

	OP_STATUS					AddPopupMessageMenuItems(DesktopMenuContext* context, const OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItemList* menu_items_list);

	int							GetCurrentSpellcheckID(const DesktopMenuContext * menu_context);

	const char*					m_initial_menu_name;
	INTPTR						m_initial_menu_value;
	INT32						m_start_at_index;
	INT32						m_current_index;
	INT32						m_enabled_item_count;
	BOOL3						m_grown_too_large;

	DesktopMenuHandler*	m_menu_handler;
	Application* m_application;
	ClassicGlobalUiContext* m_global_context;
	void*						m_platform_handle;
	OpInputContext*				m_menu_input_context;
	MenuShortcutLetterFinder	m_shortcut_finder;

	static int					s_recursion_breaker;
};

class BookmarkMenuContext : public OpInputContext
{
public:
	BookmarkMenuContext(INT32 id)
	:m_bookmark_id(id){}

	virtual BOOL OnInputAction(OpInputAction* /*action*/);

private:
	BOOL NeedActionData(OpInputAction::Action action);

	INT32 m_bookmark_id;
};

#endif // QUICK_MENU_H
