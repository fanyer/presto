/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Espen Sand
 */

#include "core/pch.h"

#include "platforms/unix/product/x11quick/x11_desktop_popup_menu_handler.h"

#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/formats/uri_escape.h"

#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/menus/MenuShortcutLetterFinder.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/product/x11quick/menubar.h"
#include "platforms/unix/product/x11quick/popupmenu.h"
#include "platforms/unix/product/x11quick/popupmenuitem.h"

static void GetImageName(OpInputAction* input_action, OpString& name)
{
	OpWindowCommander* win_comm = NULL;
	BrowserDesktopWindow* active_browser_window = g_application->GetActiveBrowserDesktopWindow();
	if (active_browser_window)
	{
		DesktopWindow* active_window = active_browser_window->GetActiveDesktopWindow();
		if (active_window)
		{
			win_comm = active_window->GetWindowCommander();
		}
	}

	URL url;
	if (win_comm)
	{
		if (input_action->GetAction() == OpInputAction::ACTION_OPEN_IMAGE)
			url = WindowCommanderProxy::GetImageURL(win_comm);
		else
			url = WindowCommanderProxy::GetBGImageURL(win_comm);
	}

	if (!url.IsEmpty())
	{
		if (OpStatus::IsSuccess(name.Set(url.UniName(PASSWORD_HIDDEN))))
		{
			int pos = name.FindLastOf('/');
			if( pos != KNotFound )
			{
				name.Delete(0, pos+1);
			}

			if (!name.IsEmpty())
			{
				//Unescape the name associated with an image before it appears against the "Open Image" context menu item.
				int len = name.Length();
				if (len)
				{
					UriUnescape::ReplaceChars(name.CStr(), len, UriUnescape::All);
					name.CStr()[len] = 0;
				}

				UINT p = 0;
				uni_char* s;
				while ((s = uni_strchr(name.CStr()+p, '&')) != 0)
				{
					p = s-name.CStr()+1;
					name.Insert(p, UNI_L("&"));
					p++;
				}
			}
		}
	}
}


BOOL IsHotClickMenu(const OpStringC8& popup_name)
{
	return popup_name == "Console Hotclick Popup Menu" || popup_name == "Hotclick Popup Menu";
}

BOOL IsOMenu(const char* popup_name)
{
	return popup_name && !op_strcmp(popup_name,"Browser Button Menu Bar");
}

void X11DesktopPopupMenuHandler::OnPopupMenu(const char* popup_name, const PopupPlacement& placement,
		BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context)
{
	if (g_drag_manager->IsDragging())
		return;

	const BOOL is_hotclick_menu = IsHotClickMenu(popup_name);

	if (is_hotclick_menu && PopupMenu::GetHotClickMenu())
	{
		PopupMenu* menu = PopupMenu::GetHotClickMenu();
		menu->MoveTo(PopupMenu::BELOW_OR_ABOVE, placement.rect);
		return;
	}

	if (X11Widget::GetPopupWidget())
		return;

	OpAutoPtr<PopupMenu> menu(OP_NEW(PopupMenu, ()));
	if (!menu.get() || OpStatus::IsError(menu->Init(0)))
		return;

	menu->SetScopeName(popup_name);

	g_x11->GetWidgetList()->SetPopupParentWidget(g_x11->GetWidgetList()->GetInputWidget());

	menu->SetIsHotClickMenu(is_hotclick_menu);

	menu_ctx = use_keyboard_context ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext();

	g_application->GetMenuHandler()->AddMenuItems(menu.get(), popup_name, popup_value, 0, menu_ctx, FALSE);

	menu->SetExecContext( use_keyboard_context ? PopupMenu::KeyboardContext : PopupMenu::MouseContext );
	menu->SetSelectedItem(-1);

	BrowserDesktopWindow* window = g_application->GetActiveBrowserDesktopWindow();

	PopupPlacement::Type placement_type = placement.type;
	if (placement_type == PopupPlacement::POPUP_WINDOW_CENTER && window == NULL)
		placement_type = PopupPlacement::POPUP_CURSOR;

	PopupMenu::AnchorType anchor_type = PopupMenu::RIGHT_OR_LEFT_OF;
	OpRect anchor_rect;
	switch (placement_type)
	{
		case PopupPlacement::POPUP_WINDOW_CENTER:
		{
			INT32 x, y;
			window->GetOuterPos(x, y);
			anchor_rect.x = x + (window->GetOuterWidth() - menu->GetWidth()) / 2;
			anchor_rect.y = y + (window->GetOuterHeight() - menu->GetHeight()) / 2;
			break;
		}

		case PopupPlacement::POPUP_CURSOR:
		{
			OpPoint cursor = g_input_manager->GetMousePosition();
			anchor_rect.x = cursor.x;
			anchor_rect.y = cursor.y;
			break;
		}

		case PopupPlacement::POPUP_BELOW:
		case PopupPlacement::POPUP_CENTER:
			anchor_type = PopupMenu::BELOW_OR_ABOVE;
			// fall through
		case PopupPlacement::POPUP_RIGHT:
			anchor_rect = placement.rect;
			break;

		default:
			OP_ASSERT(!"Unhandled placement type");
	}

	menu->SetIsOMenu(IsOMenu(popup_name));

	// Only show context menus with content (bug DSK-315553)
	if (menu->HasContent())
	{
		OpString name;
		name.Set(popup_name);

		OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
		BOOL send_to_scope = OpStatus::IsSuccess(menu->SetQuickMenuInfo(info));
		OP_ASSERT(!PopupMenu::GetTopPopupMenu());
		menu->SetIsTopPopupMenu();
		if (send_to_scope)
			g_application->GetMenuHandler()->OnMenuShown(TRUE, info);

		PopupMenu::Exec(menu.release(), anchor_type, anchor_rect, true, true); // Will delete menu object

		if (send_to_scope)
			g_application->GetMenuHandler()->OnMenuShown(FALSE, info);
	}

	g_x11->GetWidgetList()->SetPopupParentWidget(0);
}

BOOL X11DesktopPopupMenuHandler::ListPopupMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuList& out)
{
	// Get the top menu open (not a submenu)
	PopupMenu* menu = PopupMenu::GetTopPopupMenu();
	if (!menu)
		return FALSE;

	// Add menu and all sub, sub-sub and so on, menus to out list
	while (menu)
	{
		OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuInfo, ()));
		if (info.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(menu->SetQuickMenuInfo(*info.get()));
		RETURN_IF_ERROR(out.GetMenuListRef().Add(info.get()));
		info.release();

		menu = menu->GetOpenSubMenu();
	}

	// Now check for the bookmark context menu
	menu = PopupMenu::GetBookmarkContextMenu();
	if (menu)
	{
		OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuInfo, ()));
		if (info.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(menu->SetQuickMenuInfo(*info.get()));
		RETURN_IF_ERROR(out.GetMenuListRef().Add(info.get()));
		info.release();
	}

	return TRUE;
}


BOOL X11DesktopPopupMenuHandler::OnAddMenuItem(void* vmenu,
										   BOOL separator,
										   const uni_char* item_name,
										   OpWidgetImage* image,
										   const uni_char* shortcut,
										   const char* sub_menu_name,
										   INTPTR sub_menu_value,
										   INT32 sub_menu_start,
										   BOOL checked,
										   BOOL selected,
										   BOOL disabled,
										   BOOL bold,
										   OpInputAction* input_action)
{
	UINT32 state = 0;
	if (checked)
		state |= PopupMenuItem::MI_CHECKED;
	if (selected)
		state |= PopupMenuItem::MI_SELECTED;
	if (disabled)
		state |= PopupMenuItem::MI_DISABLED;
	if (bold)
		state |= PopupMenuItem::MI_BOLD;

	PopupMenu* menu = (PopupMenu *)vmenu;
	if (sub_menu_name)
	{
		OpString8 name8;
		if (OpStatus::IsSuccess(name8.Set(sub_menu_name)))
		{
			PopupMenu *submenu = OP_NEW(PopupMenu, ());
			submenu->SetScopeName(sub_menu_name);

			if (submenu && OpStatus::IsSuccess(submenu->Init(menu->GetOpView())))
			{
				menu->AddItem(item_name, shortcut, input_action, state, image, submenu);
				submenu->SetSubMenuData(sub_menu_name, sub_menu_value, sub_menu_start);
				submenu->SetExecContext( menu->GetExecContext() );
			}
			else
			{
				OP_DELETE(submenu);
			}
		}
	}
	else
	{
		if (separator)
		{
			menu->AddSeparator();
		}
		else
		{
			OpString image_name;
			if (input_action && 
				(input_action->GetAction() == OpInputAction::ACTION_OPEN_IMAGE ||
				 input_action->GetAction() == OpInputAction::ACTION_OPEN_BACKGROUND_IMAGE))
			{
				GetImageName(input_action, image_name);
				if (image_name.HasContent())
				{
					OpString s;
					s.AppendFormat(UNI_L("%s (%s)"), item_name, image_name.CStr());
					image_name.Set(s);
				}
			}

			menu->AddItem(image_name.HasContent() ? image_name.CStr() : item_name, shortcut, input_action, state, image);
		}
	}

	return menu->SoonTooTall();
}

// static 
void X11DesktopPopupMenuHandler::ShowBookmarkContextMenu(const OpPoint& pos, PopupMenu* caller, OpInputAction* link_action)
{
	if (!link_action)
		return;

	struct
	{
		int id;
		OpInputAction::Action action;
		Str::LocaleString string_id;
	} menu_settings[] =
	{
		{ 0, OpInputAction::ACTION_OPEN_LINK, Str::SI_OPEN_BUTTON_TEXT},
		{ 1, OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE, Str::MI_IDM_MENU_PAGEBAR_LINK_IN_NEW_PAGE},
		{ 2, OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE, Str::MI_IDM_MENU_PAGEBAR_LINK_IN_BG_PAGE},
		{-1, OpInputAction::ACTION_UNKNOWN, Str::NOT_A_STRING},
		{ 3, OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW, Str::MI_IDM_HLITEM_GET_IN_NEW_WINDOW},
		{ 4, OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW, Str::M_OPEN_BACKGROUND_WINDOW},
		{-1, OpInputAction::ACTION_UNKNOWN, Str::NOT_A_STRING},
		{ 5, OpInputAction::ACTION_EDIT_PROPERTIES, Str::MI_IDM_HLITEM_PROPERTIES}
	};

	PopupMenu* menu = OP_NEW(PopupMenu,());
	if (!menu || OpStatus::IsError(menu->Init(0)))
	{
		OP_DELETE(menu);
		return;
	}

	int state = 0;
	uni_char* shortcut = 0;
	MenuShortcutLetterFinder letter_finder;

	for (unsigned i = 0; i < ARRAY_SIZE(menu_settings); i++)
	{
		if (menu_settings[i].id == -1)
			menu->AddSeparator();
		else
		{
			OpString item_name, item_name_with_shortcut;
			TRAPD(rc,g_languageManager->GetStringL(menu_settings[i].string_id, item_name));
			letter_finder.CreateMenuString(item_name.CStr(), item_name_with_shortcut);

			OpWidgetImage image;
			OpInputAction* action = OP_NEW( OpInputAction,(menu_settings[i].action));
			image.SetImage(action->GetActionImage());

			menu->AddItem(item_name_with_shortcut.CStr(), shortcut, action, state, &image);
		}
	}

	menu->SetScopeName(MENU_NAME_BOOKMARK_CONTEXT_MENU);
	OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
	BOOL send_to_scope = OpStatus::IsSuccess(menu->SetQuickMenuInfo(info));
	menu->SetIsBookmarkContextMenu();
	if (send_to_scope)
		g_application->GetMenuHandler()->OnMenuShown(TRUE, info);

	OpInputAction::Action action = PopupMenu::Exec(menu, PopupMenu::BELOW_OR_ABOVE, OpRect(pos.x, pos.y, 0, 0), true, false);

	if (send_to_scope)
		g_application->GetMenuHandler()->OnMenuShown(FALSE, info);
	
	// Hide caller menu 
	if (caller)
	{
		if (action == OpInputAction::ACTION_UNKNOWN)
			caller->Grab(); // Menu was canceled
		else
			caller->Stop(TRUE);
	}

	switch(action)
	{
		case OpInputAction::ACTION_OPEN_LINK:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
			g_input_manager->InvokeAction(action, link_action->GetActionData(), link_action->GetActionDataString() );
		break;	
		case OpInputAction::ACTION_EDIT_PROPERTIES:
			g_hotlist_manager->EditItem(link_action->GetActionData(), 0);
		break;
	}
}
