/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
** 
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "menubar.h"

#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/menus/MenuShortcutLetterFinder.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/quick-widget-names.h"

#include "modules/display/VisDevListeners.h"
#include "modules/doc/frm_doc.h"
#include "modules/formats/uri_escape.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/util/OpLineParser.h"
#include "modules/widgets/OpButton.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "platforms/windows/user_fun.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/win_handy.h"

extern UINT	idMsgLostCursor;
extern HCURSOR hArrowCursor;

BOOL MonitorRectFromPoint(RECT& hMonitorRect, POINT point, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);
BOOL MonitorRectFromWindow(RECT& hMonitorRect, HWND wnd, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);
BOOL MonitorRectFromRect(RECT& hMonitorRect, LPCRECT rect, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);

#ifndef SPI_GETKEYBOARDCUES
#define SPI_GETKEYBOARDCUES 4106
#endif
#ifndef SPI_GETFLATMENU
#define SPI_GETFLATMENU                     0x1022
#endif // SPI_GETFLATMENU

OUIMenuManager*				g_windows_menu_manager					= NULL;

#define IDM_SEPARATOR				0
#define IDM_POPUP					(UINT)(-1)
#define IDM_MENUBAR_MDI_MINIMIZE	10533	 
#define IDM_MENUBAR_MDI_RESTORE		10534	 
#define IDM_MENUBAR_MDI_CLOSE		10535

void MenuW_Cleanup( HMENU hMenu)
{	
	int nItems = GetMenuItemCount( hMenu);
	UINT idPrevItem = 0;					//	starting with 0 to remove trailing seps.

	for( int i=nItems-1; i>=0; --i)
	{
		UINT idItem = GetMenuItemID( hMenu, i);
		if( ((idItem == idPrevItem) && (idItem != IDM_POPUP))	//	no duplicate items 
			|| (i==0 && idItem==IDM_SEPARATOR))					//	no separator at menu top
		{
			g_windows_menu_manager->RemoveMenuItemData(hMenu, i);

			RemoveMenu( hMenu, i, MF_BYPOSITION);
			-- nItems;
		}
		else
			idPrevItem = idItem;
	}
	
	//	We *might* still have one sep at top
	if( nItems && (GetMenuItemID( hMenu, 0) == IDM_SEPARATOR))
	{
		g_windows_menu_manager->RemoveMenuItemData(hMenu, 0);

		RemoveMenu( hMenu, 0, MF_BYPOSITION);
	}
};

/* static */
int OUIMenuManager::GetMenuItemHeight()
{
	int menu_item_height = GetSystemMetrics(SM_CYMENU);
	if(g_desktop_op_ui_info->IsTouchUIWanted())
	{
		menu_item_height *= 1.4;
	}
	return menu_item_height;
}

/***********************************************************************************
**
**	OUIMenuManager::MenuItemData
**
***********************************************************************************/

class OUIMenuManager::MenuItemData
{
public:
	MenuItemData(const uni_char* item_name, OpWidgetImage* image, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, OpInputAction* input_action, const uni_char* shortcut, BOOL selected, BOOL bold, BOOL separator);
	~MenuItemData();

	BOOL IsBookmark();

	OpString		m_item_name;
	OpWidgetImage	m_image;
	OpString8		m_sub_menu_name;
	OpString		m_shortcut;
	INTPTR			m_sub_menu_value;
	INT32			m_sub_menu_start;
	OpInputAction*	m_input_action;
	BOOL			m_selected;
	BOOL			m_bold;
	BOOL			m_separator;
	uni_char        m_accelerator_key;
};


OUIMenuManager::MenuItemData::MenuItemData(const uni_char* item_name, OpWidgetImage* image, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, OpInputAction* input_action, const uni_char* shortcut, BOOL selected, BOOL bold, BOOL separator)
	: m_sub_menu_value(sub_menu_value)
	, m_sub_menu_start(sub_menu_start)
	, m_input_action(input_action)
	, m_selected(selected)
	, m_bold(bold)
	, m_separator(separator)
{
	g_windows_menu_manager->m_popup_counter++;
	m_item_name.Set(item_name);
	m_sub_menu_name.Set(sub_menu_name);
	m_shortcut.Set(shortcut);
	if (image)
		m_image.SetWidgetImage(image);
}

OUIMenuManager::MenuItemData::~MenuItemData()
{
	g_windows_menu_manager->m_popup_counter--;
	OpInputAction::DeleteInputActions(m_input_action);
}

BOOL OUIMenuManager::MenuItemData::IsBookmark()
{
	if (m_sub_menu_name.CompareI("Bookmark Folder Menu") == 0
		|| m_sub_menu_name.CompareI("Active Bookmark Folder Menu") == 0)
		return TRUE; // Folder

	if (m_input_action && OpInputAction::ACTION_OPEN_LINK == m_input_action->GetAction() &&
		m_input_action->GetActionData())
			if (HotlistModelItem* item = g_hotlist_manager->GetItemByID(m_input_action->GetActionData()))
				return item->IsBookmark();
	return FALSE;
}


/***********************************************************************************
**
**	OUIMenuManager::MenuStackItem
**
***********************************************************************************/

class OUIMenuManager::MenuStackItem
{
public:
	MenuStackItem(HMENU parent);
	~MenuStackItem();

	HMENU GetNewMenuHandle();
	BOOL Contains(HMENU menu) const;

	OpVector<HMENU> m_menus;
	HMENU			m_parent;
};


OUIMenuManager::MenuStackItem::MenuStackItem(HMENU parent)
	: m_parent(parent)
{
}

OUIMenuManager::MenuStackItem::~MenuStackItem()
{
	g_windows_menu_manager->m_popup_counter -= m_menus.GetCount();

	for (unsigned i = 0; i < m_menus.GetCount(); i++)
	{
		HMENU* handle = m_menus.Get(i);
		DestroyMenu(*handle);
		OP_DELETE(handle);
	}
}

HMENU OUIMenuManager::MenuStackItem::GetNewMenuHandle()
{
	OpAutoPtr<HMENU> handle(OP_NEW(HMENU, ()));
	if (!handle.get())
		return NULL;

	*handle = CreatePopupMenu();
	if (!*handle)
		return NULL;

	g_windows_menu_manager->m_popup_counter++;
	RETURN_VALUE_IF_ERROR(m_menus.Add(handle.get()), NULL);
	return *handle.release();
}

BOOL OUIMenuManager::MenuStackItem::Contains(HMENU menu) const
{
	for (unsigned i = 0; i < m_menus.GetCount(); i++)
	{
		if (menu == *m_menus.Get(i))
			return TRUE;
	}

	return FALSE;
}


/***********************************************************************************
**
**	OUIMenuManager
**
***********************************************************************************/

OUIMenuManager::OUIMenuManager() : 
m_alt_down_bar(NULL),
m_opening_menu(FALSE),
m_pIContextMenu2(NULL),
m_popup_root_name(NULL),
m_alt_key_down(FALSE),
m_popup_root_value(-1),
m_use_keyboard_context(FALSE),
m_menu_input_context(NULL),
m_activate_first_item(FALSE),
m_inpage_popup_menu(NULL),
m_active_menu(NULL),
m_active_menu_owner(NULL),
m_leftmost(FALSE),
m_rightmost(FALSE),
m_hook(NULL),
m_keyboard_hook(NULL),
m_active_menu_bar(NULL),
m_active_menu_bar_item(NULL),
m_popup_counter(0),
m_eat_mousemove(0),
m_id_counter(5000),
m_menu_break(FALSE),
m_force_shortcut_underline(FALSE),
m_max_width(0),
m_menu_item_count(0),
m_menu_height(0),
m_menu_parameters_changed(FALSE),
m_old_menu_animation(FALSE),
m_old_menu_selection_fade(FALSE)
{
	m_screen_reader_menus = FALSE; //!prefsManager->GetIntegerPrefDirect(PrefsManager::HotlistOwnerDraw);

	m_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD, StaticKeyboardHook, 0, GetCurrentThreadId());
	m_hook = SetWindowsHookEx(WH_MSGFILTER, StaticMsgHook, 0, GetCurrentThreadId());

	// create generic root menus, one for any popups and an array with MAX_MENUBAR_ITEMS popups
	// for all menubars. (that means a menubar can have at most MAX_MENUBAR_ITEMS dropdown menus)
	// the content of these menus and additional submenus are created on the fly.

	m_popup_root = CreatePopupMenu();

	for (INT32 i = 0; i < MAX_MENUBAR_ITEMS; i++)
	{
		m_menubar_roots[i] = CreatePopupMenu();
	}
	m_use_flat_menus = FALSE;

	m_avoid_rect.Set(0, 0, 0, 0);

	if(IsSystemWinXP())
	{
		// When enabled, the menu bar uses COLOR_MENUBAR for the menubar background, COLOR_MENU for the menu-popup background, 
		// COLOR_MENUHILIGHT for the fill of the current menu selection, and COLOR_HILIGHT for the outline of the current menu 
		// selection. If disabled, menus are drawn using the same metrics and colors as in Windows 2000.
		SystemParametersInfo(SPI_GETFLATMENU, 0, (void *)&m_use_flat_menus, 0);
	}
}

/**********************************************************************************/

OUIMenuManager::~OUIMenuManager()
{
	// destroy all menus and menuitems

	DestroyMenuItems(m_popup_root);
	DestroyMenu(m_popup_root);

	for (int i = 0; i < MAX_MENUBAR_ITEMS; i++)
	{
		DestroyMenuItems(m_menubar_roots[i]);
		DestroyMenu(m_menubar_roots[i]);
	}

	m_menu_stack.DeleteAll();

	OP_ASSERT(m_popup_counter == 0);		// notify Trond if assert triggers

	if(m_pIContextMenu2)
	{
		m_pIContextMenu2->Release();
	}

	UnhookWindowsHookEx(m_hook);
	UnhookWindowsHookEx(m_keyboard_hook);
}

/**********************************************************************************/

void OUIMenuManager::CreateMenuBar(WindowsOpWindow* op_window, const char* menu_bar_name)
{
	/*if (m_screen_reader_menus)
	{
		CreateScreenReaderCompatibleMenuBar(op_window->m_hwnd, menu_bar_name);
	}
	else*/ if (!op_window->m_menubar)
	{
		OUIMenuBar* menu_bar = OP_NEW(OUIMenuBar, (this, op_window->m_hwnd, menu_bar_name));
		if (menu_bar)
		{
			menu_bar->Into(&m_menu_bars);
			op_window->m_menubar = menu_bar;
		}
		menu_bar->m_parent_opwindow = op_window;
		menu_bar->UpdateMDIButtons(op_window);
	}
}

/**********************************************************************************/

void OUIMenuManager::RecreateMenuBars()
{
//	m_screen_reader_menus = !prefsManager->GetIntegerPrefDirect(PrefsManager::HotlistOwnerDraw);
}

/**********************************************************************************/

void OUIMenuManager::DestroyMenuBar(WindowsOpWindow* op_window)
{
	// remove new style menu bar if any

	OUIMenuBar* menu_bar = NULL;

	menu_bar = op_window->m_menubar;
	op_window->m_menubar = NULL;

	if (menu_bar)
	{
		if (m_alt_down_bar == menu_bar)
		{
			m_alt_down_bar = NULL;
		}

		menu_bar->Out();
		OP_DELETE(menu_bar);
	}

/*	unused code unless CreateScreenReaderCompatibleMenuBar is resurrected
	// remove old style menu bar if any

	HWND hwnd = op_window->m_hwnd;

	if (hwnd && !(GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD))
	{
		HMENU menu = ::GetMenu(hwnd);

		if (IsMenu(menu))
		{
			SetMenu(hwnd, NULL);
			DestroyMenuItems(menu);
			DestroyMenu(menu);
		}
	}
*/
	if(m_pIContextMenu2)
	{
		m_pIContextMenu2->Release();
		m_pIContextMenu2 = NULL;
	}
}

/**********************************************************************************/

/*
void OUIMenuManager::CreateScreenReaderCompatibleMenuBar(HWND parent, const char* menu_bar_name)
{
	HMENU menu = CreateMenu();

	PrefsSection *section = NULL;
	
	TRAPD(err, section = g_setup_manager->GetSectionL(menu_bar_name, OPMENU_SETUP));

	if (section)
	{
		INT32 i = 0;

		MenuShortcutLetterFinder letter_finder;

		for (const PrefsEntry *entry = section->Entries(); entry && i < MAX_MENUBAR_ITEMS; entry = (const PrefsEntry *) entry->Suc(), i++)
		{
			OpLineParser line(entry->Key());

			OpString item_type;
			OpString item_name;
			OpString8 sub_menu_name;
			INTPTR sub_menu_value = 0;

			RETURN_VOID_IF_ERROR(line.GetNextToken(item_type));

			BOOL submenu = uni_stristr(item_type.CStr(), UNI_L("Submenu")) != NULL;

			if (!submenu)
				continue;

			line.GetNextLanguageString(item_name);
			RETURN_VOID_IF_ERROR(line.GetNextToken8(sub_menu_name));
			line.GetNextValue(sub_menu_value);

			MENUITEMINFO item_info;
			memset(&item_info, 0, sizeof(MENUITEMINFO));
			item_info.cbSize = 0x2C;	// sizeof(MENUITEMINFO) is 0x30 on WINVER >= 5, but Windows NT fails on that

			MenuItemData* item_data = new MenuItemData(item_name.CStr(), NULL, sub_menu_name.CStr(), sub_menu_value, 0, NULL, NULL, FALSE, FALSE, FALSE);

			if (item_data)
			{
				if (OpStatus::IsError(m_item_data_hashtable.Add(item_data, item_data)))
				{
					delete item_data;
				}
				else
				{
					OpString item_name_new;
					letter_finder.CreateMenuString(item_name.CStr(), item_name_new);
	
					item_info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_SUBMENU;
					item_info.fType = MFT_STRING;
					item_info.dwTypeData = item_name_new.CStr();
					item_info.wID = 0;
					item_info.dwItemData = (ULONG) item_data;
					item_info.hSubMenu = m_menubar_roots[i];
					item_info.cch = item_name.Length();
	
					InsertMenuItem(menu, UINT_MAX, TRUE, &item_info);
				}
			}
		}

		delete section;
	}

	SetMenu(parent, menu);
}
*/

/**********************************************************************************/

BOOL OUIMenuManager::SetAltKey(BOOL down)
{
	// ignore the alt key when dragging
	if(g_drag_manager->IsDragging())
		return FALSE;

	if (m_alt_down_bar)
	{
		return m_alt_down_bar->SetAltKey(FALSE);
	}
	else if (down && !(GetKeyState(VK_CONTROL) & 0x8000))
	{
		HWND active = GetActiveWindow();

		OUIMenuBar* menu_bar;

		for (menu_bar = (OUIMenuBar*) m_menu_bars.First(); menu_bar; menu_bar = (OUIMenuBar*) menu_bar->Suc())
		{
			if (active == menu_bar->GetParentHWnd())
			{
				return menu_bar->SetAltKey(TRUE);
			}
		}
		m_alt_key_down = TRUE;
	}
	return FALSE;
}

/**********************************************************************************/

void OUIMenuManager::CancelAltKey()
{
	if (m_alt_down_bar)
		m_alt_down_bar->CancelAltKey();
	m_alt_key_down = FALSE;
}

/**********************************************************************************/

BOOL OUIMenuManager::SetKeyDown(int key)
{
	return m_alt_down_bar ? m_alt_down_bar->SetKeyDown(key) : FALSE;
}

/**********************************************************************************/

/* Unused code
HWND OUIMenuManager::GetRestoreFocus(HWND hwnd)
{
	OUIMenuBar* menu_bar;

	for (menu_bar = (OUIMenuBar*) m_menu_bars.First(); menu_bar; menu_bar = (OUIMenuBar*) menu_bar->Suc())
	{
		if (menu_bar->GetHWnd() == hwnd)
		{
			if (menu_bar->m_old_focus)
			{
				return menu_bar->m_old_focus;
			}
			else
			{
				return menu_bar->GetParentHWnd();
			}
		}
	}

	return hwnd;
}*/


/**********************************************************************************/

int OUIMenuManager::ShowMenuBarItemRect(OUIMenuBar* menu_bar, OUIMenuBarItem* menu_bar_item, RECT* rect, HWND owner, BOOL keyboard_activated, BOOL return_command)
{
	OP_NEW_DBG("OUIMenuManager::ShowMenuBarItemRect()", "desktop-scope");

	HMENU menu = NULL;

	if(menu_bar_item->IsMDIItem())
	{
		BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
		if(browser_window)
		{
			WindowsOpWindow *browser_op_window = reinterpret_cast<WindowsOpWindow *>(browser_window->GetOpWindow());
			if(browser_op_window)
			{
				menu = GetSystemMenu(browser_op_window->m_hwnd, FALSE);
			}
		}
	}

	if(!menu)
	{
		menu = m_popup_root;
		m_popup_root_name = menu_bar_item->m_sub_menu_name.CStr();
		m_popup_root_value = menu_bar_item->m_sub_menu_value;
		m_use_keyboard_context = TRUE;
	}

	m_activate_first_item	= keyboard_activated;
	m_active_menu_bar		= menu_bar;
	m_active_menu_bar_item	= menu_bar_item;
	m_leftmost				= TRUE;
	m_rightmost				= TRUE;
	m_active_menu			= m_popup_root;
	m_active_menu_owner		= owner;

	OP_DBG(("m_active_menu = %p, m_popup_root_name = '%s'", m_active_menu, m_popup_root_name));

	TPMPARAMS avoid;

	avoid.cbSize = sizeof(TPMPARAMS);

	avoid.rcExclude = *rect;
	avoid.rcExclude.left -= 2000;
	avoid.rcExclude.right += 2000;

	int flags = TPM_TOPALIGN | TPM_VERTICAL;
	if (return_command)
		flags |= TPM_RETURNCMD;

	RECT mon_rect;
	MonitorRectFromRect(mon_rect, rect, FALSE);
	rect->left = MAX(rect->left, mon_rect.left);
	rect->right = MIN(rect->right, mon_rect.right);

	int x;
	if (UiDirection::Get() == UiDirection::LTR)
	{
		x = rect->left;
		flags |= TPM_LEFTALIGN;
	}
	else
	{
		x = rect->right;
		flags |= TPM_RIGHTALIGN | TPM_LAYOUTRTL | TPM_HORPOSANIMATION | TPM_VERPOSANIMATION;
	}
	int y = rect->bottom;

	m_menu_input_context = NULL;

	DisableMenuAnimationParameter();
	int cmd = TrackPopupMenuEx(menu, flags, x, y, owner,  &avoid);
	RestoreMenuAnimationParameter();

	m_menu_input_context = m_use_keyboard_context ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext();

	m_active_menu			= NULL;
	m_active_menu_owner		= NULL;
	m_active_menu_bar		= NULL;
	m_active_menu_bar_item	= NULL;
	m_leftmost				= FALSE;
	m_rightmost				= FALSE;
	m_activate_first_item	= FALSE;
	m_popup_root_name = NULL;
	m_popup_root_value = -1;

	OP_DBG(("m_active_menu = NULL, m_popup_root_name = NULL"));

	return cmd;
}

/**********************************************************************************/
BOOL OUIMenuManager::OnAddBreakMenuItem()
{
	m_menu_break = TRUE;
	m_menu_item_count = 0;
	m_menu_height = 0;

	return FALSE;
}

BOOL OUIMenuManager::OnAddMenuItem(void* menu, BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, BOOL checked, BOOL selected, BOOL disabled, BOOL bold, OpInputAction* input_action)
{
	BOOL need_to_wrap_soon = FALSE;

	OpString item_string;

	MENUITEMINFO item_info = {};
	item_info.cbSize = sizeof(item_info);

	m_menu_item_count++;

	if (seperator)
	{
		m_menu_height += SEPARATOR_HEIGHT;

		item_info.fMask = MIIM_TYPE | MIIM_DATA;
		item_info.fType = MFT_SEPARATOR | MFT_OWNERDRAW;

		MenuItemData* item_data = OP_NEW(MenuItemData, (NULL, image, sub_menu_name, sub_menu_value, sub_menu_start, NULL, shortcut, selected, bold, TRUE));
		if(item_data)
		{
			item_info.dwItemData = (ULONG_PTR) item_data;

			if (OpStatus::IsError(m_item_data_hashtable.Add(item_data, item_data)))
			{
				OP_DELETE(item_data);
				return need_to_wrap_soon;
			}
		}
	}
	else
	{
		item_string.Set(item_name);

		if(input_action)
		{
			if( input_action->GetAction() == OpInputAction::ACTION_OPEN_IMAGE ||
				input_action->GetAction() == OpInputAction::ACTION_OPEN_BACKGROUND_IMAGE )
			{
				URL url;
				OpWindowCommander* win_comm = NULL;
				BrowserDesktopWindow* active_browser_window = g_application->GetActiveBrowserDesktopWindow();
				if( active_browser_window )
				{
					DesktopWindow* active_window = active_browser_window->GetActiveDesktopWindow();
					if( active_window )
					{
						win_comm = active_window->GetWindowCommander();
					}
				}

				if(win_comm)
				{
					if( input_action->GetAction() == OpInputAction::ACTION_OPEN_IMAGE )
					{
						url = WindowCommanderProxy::GetImageURL(win_comm);
					}
					else
					{
						url = WindowCommanderProxy::GetBGImageURL(win_comm);
					}
				}

				if( !url.IsEmpty() )
				{
					OpString name;
					if(OpStatus::IsSuccess(name.Set(url.UniName(PASSWORD_HIDDEN))))
					{
						int pos = name.FindLastOf('/');
						if( pos != KNotFound )
						{
							name.Delete(0, pos+1);
						}

						if( !name.IsEmpty() )
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
							item_string.AppendFormat(UNI_L(" (%s)"), name.CStr());
						}
					}
				}
			}
		}

		HFONT font = ((WindowsOpSystemInfo*)g_op_system_info)->GetMenuFont();
		HDC hdc = GetDC(g_main_hwnd);
		HFONT old_font = (HFONT) SelectObject(hdc, font);
		INT32 allowed_length;
		SIZE size;

		OpScreenProperties screen_props;
		DesktopWindow* desktop_window =
				g_application->GetActiveBrowserDesktopWindow();
		// We might not be running the browser.
		if (NULL == desktop_window)
		{
			desktop_window = g_application->GetActiveDesktopWindow(TRUE);
		}
		g_op_screen_info->GetProperties(&screen_props, desktop_window ? desktop_window->GetOpWindow() : NULL);

		GetTextExtentExPoint(hdc, item_string.CStr(), item_string.Length(), screen_props.width / 3, &allowed_length, NULL, &size);
		SelectObject(hdc, old_font);
		ReleaseDC(g_main_hwnd, hdc);

		if (item_string.Length() > allowed_length)
			item_string[allowed_length > 1 ? allowed_length - 1 : 1] = 0;

		MenuItemData* item_data = OP_NEW(MenuItemData, (item_string.CStr(), image, sub_menu_name, sub_menu_value, sub_menu_start, input_action, shortcut, selected, bold, seperator));
		if (!item_data)
		{
			OpInputAction::DeleteInputActions(input_action);
			return need_to_wrap_soon;
		}

		if (OpStatus::IsError(m_item_data_hashtable.Add(item_data, item_data)))
		{
			OP_DELETE(item_data);
			return need_to_wrap_soon;
		}

		item_info.fMask = MIIM_STATE | MIIM_TYPE | MIIM_DATA | (sub_menu_name ? MIIM_SUBMENU : MIIM_ID);

		if (m_screen_reader_menus)
		{
			item_info.fType = MFT_STRING;
		}
		else
		{
			item_info.fType = MFT_OWNERDRAW;
		}

		item_info.fState = disabled ? MFS_DISABLED : MFS_ENABLED;
		
		if (checked || selected)
			item_info.fState |= MFS_CHECKED;

		if (selected)
			item_info.fType |= MFT_RADIOCHECK;

		if(m_menu_break)
		{
			item_info.fType |= MFT_MENUBARBREAK;
			m_menu_break = FALSE;
		}

		int menu_item_height = OUIMenuManager::GetMenuItemHeight();
		menu_item_height = MAX(size.cy + 4,menu_item_height); // see OnMeasureItem
		m_menu_height += menu_item_height;

		INT32 limit =  screen_props.height;
		
		// for dropdown menus,the height should be restricted according to 
		// the position of the menubar in desktop work area
		if(m_active_menu_bar)	// menu drop down from menubar
		{
			RECT rect;
			GetWindowRect(m_active_menu_bar->GetHWnd(),&rect);

			INT32 limit_above = rect.top;				// limit of height of menu if the menu will be displayed above the menubar
			INT32 limit_below = limit - rect.bottom;	// limit of height of menu if the menu will be displayed below the menubar
			limit = max(limit_above,limit_below);	
		}

		// menu_item_height + SEPARATOR_HEIGHT : preserved for potential "More Items" item
		// 5: height of menu border
		if ((INT32)m_menu_height + menu_item_height + SEPARATOR_HEIGHT + 5 > limit - menu_item_height)
		{
			need_to_wrap_soon = TRUE;
			m_menu_item_count = 0;
			m_menu_height = 0;
		}

		if (shortcut)
		{
			item_string.Append(UNI_L("\t"));
			item_string.Append(shortcut);
		}

		item_info.dwTypeData = item_string.CStr();
		item_info.wID = m_id_counter++;
		item_info.dwItemData = (ULONG_PTR) item_data;
		item_info.hSubMenu = sub_menu_name ? m_menu_stack.Get(m_menu_stack.GetCount() - 1)->GetNewMenuHandle() : NULL;
		item_info.cch = item_string.Length();
	}

	InsertMenuItem((HMENU) menu, UINT_MAX, TRUE, &item_info);

	if (m_id_counter >= 10000)
		m_id_counter = 5000;

	return need_to_wrap_soon;
}

/**********************************************************************************/

void OUIMenuManager::OnAddShellMenuItems(void* menu, const uni_char* path)
{
    //  Split path
    uni_char szDirectory[_MAX_PATH];
    uni_char szFileName[_MAX_FNAME];
	IShellFolder* pIShellFolder = NULL;
    IContextMenu *pIContextMenu;
	
    extern void SplitPath( const uni_char *szPath, uni_char *szDirectory, uni_char *szFileName);
    SplitPath( path, szDirectory, szFileName);
		
    HRESULT hRes = SHGetDesktopFolder( &pIShellFolder);
    if( SUCCEEDED( hRes))
    {
        LPCITEMIDLIST pidl = GetPidlFromPath( g_main_hwnd, pIShellFolder, szDirectory);
        if( pidl)
        {
            IShellFolder * pIBoundShellFolder;
			
            hRes = pIShellFolder->BindToObject( pidl, NULL, IID_IShellFolder, (LPVOID*)&pIBoundShellFolder);
            if( SUCCEEDED( hRes))
            {
                pidl = GetPidlFromPath( g_main_hwnd, pIBoundShellFolder, szFileName);
				if (!pidl)
				{
					pIBoundShellFolder->Release();
					pIShellFolder->Release();
					return;
				}
				
                hRes = pIBoundShellFolder->GetUIObjectOf(
                    g_main_hwnd,	            //  Owner window
                    1,                          //  Number of objects specified below
                    &pidl,                      //  Pointer to an array of pointers to an ITEMIDLIST structure
                    IID_IContextMenu,           //  interface requested
                    0,                          //  reserved
                    (LPVOID*)&pIContextMenu); //  OUT -> Interface requested
				
                if( SUCCEEDED( hRes))
                {
					pIContextMenu->QueryInterface(IID_IContextMenu2, (void**)&m_pIContextMenu2);
					if(m_pIContextMenu2)
					{
						hRes = m_pIContextMenu2->QueryContextMenu( (HMENU)menu, GetMenuItemCount((HMENU)menu), 0, INT_MAX, CMF_EXPLORE );
					}
					pIContextMenu->Release();
                }
				pIBoundShellFolder->Release();
            }
		}
		pIShellFolder->Release();
	}
}

/**********************************************************************************/

OUIMenuManager::MenuItemData* OUIMenuManager::GetMenuItemData(HMENU menu, INT32 pos, BOOL by_position)
{
	MENUITEMINFO item_info;

	item_info.cbSize = sizeof(MENUITEMINFO);

	item_info.fMask = MIIM_DATA;
	item_info.dwItemData = NULL;

	GetMenuItemInfo(menu, pos, by_position, &item_info);

	void* data;

	if (m_item_data_hashtable.GetData((void*) item_info.dwItemData, &data) == OpStatus::OK)
	{
		return (MenuItemData*) item_info.dwItemData;
	}
	else
	{
		return NULL;
	}
}

OUIMenuManager::MenuItemData* OUIMenuManager::GetMenuItemByLocation(HMENU menu, POINT pt)
{
	MenuItemData* ret = NULL;
	int pos = MenuItemFromPoint(m_active_menu_owner, menu, pt);
	if (pos != -1)
	{
		ret = GetMenuItemData(menu, pos, TRUE);
	}	

	// Search sub menus
	for (UINT32 i=0; i<m_menu_stack.GetCount(); i++)
	{
		if (m_menu_stack.Get(i)->m_parent == menu)
		{
			for (UINT32 j=0; j<m_menu_stack.Get(i)->m_menus.GetCount(); j++)
			{
				HMENU sub_menu = *m_menu_stack.Get(i)->m_menus.Get(j);
				MenuItemData* item = GetMenuItemByLocation(sub_menu, pt);
				if (item)
					ret = item;
				// Don't break, continue searching sub menus since menus may overlap
			}
		}
	}

	return ret;
}


/**********************************************************************************/

void OUIMenuManager::DestroyMenuItems(HMENU menu)
{
	while (GetMenuItemCount(menu) > 0)
	{
		HMENU sub_menu = GetSubMenu(menu, 0);

		if (sub_menu)
		{
			DestroyMenuItems(sub_menu);
		}

		MenuItemData* item_data = GetMenuItemData(menu, 0, TRUE);

		if (item_data)
		{
			m_item_data_hashtable.Remove(item_data, (void**) &item_data);
			OP_DELETE(item_data);
		}

		RemoveMenu(menu, 0, MF_BYPOSITION);
	}
	if(m_pIContextMenu2)
	{
		m_pIContextMenu2->Release();
		m_pIContextMenu2 = NULL;
	}
	m_menu_item_count = 0;
	m_menu_height = 0;
}

void OUIMenuManager::RemoveMenuItemData(HMENU menu, UINT pos)
{
	MenuItemData* item_data = GetMenuItemData(menu, pos, TRUE);

	if (item_data)
	{
		m_item_data_hashtable.Remove(item_data, (void**) &item_data);
		OP_DELETE(item_data);
	}
}

/**********************************************************************************/
// The sub sub menu name is correct here ...
void OUIMenuManager::InitPopupMenuFromName(HMENU menu, const char* menu_name, INTPTR menu_value, INT32 menu_start, UINT32 stack_pos)
{
	OP_NEW_DBG("OUIMenuManager::InitPopupMenuFromName()", "desktop-scope");

	// clear previous substack
	if (stack_pos < m_menu_stack.GetCount())
	{
		DestroyMenuItems(m_menu_stack.Get(stack_pos)->m_parent);
	}

	// now destroy the actual popups further down the stack
	m_menu_stack.Delete(stack_pos, m_menu_stack.GetCount() - stack_pos);

	// make sure this one is cleared
	DestroyMenuItems(menu);

	// then create a new popup on the stack, ready to be used for submenus of this menu
	MenuStackItem* stack = OP_NEW(MenuStackItem, (menu));
	if (stack)
	{
		if (OpStatus::IsError(m_menu_stack.Add(stack)))
			OP_DELETE(stack);
	}

	// now populate this menu with its items
	OpInputContext* input_context = m_use_keyboard_context
		? g_input_manager->GetKeyboardInputContext()
		: g_input_manager->GetMouseInputContext();
	g_application->GetMenuHandler()->AddMenuItems(menu, menu_name, menu_value,
		menu_start, input_context, stack_pos > 0);

	// clean away leftover separators
	MenuW_Cleanup(menu);
}

/**********************************************************************************/

BOOL OUIMenuManager::OnInitPopupMenu(HWND hwnd, HMENU menu, INTPTR pos_to_init)
{
	// Win32 is about to show this menu. Find out if it one of
	// our menus

	if (menu == m_popup_root)
	{
		m_current_root = menu;

		// it is our popup menu.
		InitPopupMenuFromName(menu, m_popup_root_name, m_popup_root_value, 0, 0);
		return TRUE;
	}

	UINT32 i;

	for (i = 0; i < MAX_MENUBAR_ITEMS; i++)
	{
		if (menu ==	m_menubar_roots[i])
		{
			m_use_keyboard_context = TRUE;
			m_current_root = menu;

			HMENU parent = GetMenu(hwnd);

			if (parent)
			{
				MenuItemData* item_data = GetMenuItemData(parent, pos_to_init, TRUE);

				if (item_data)
				{
					InitPopupMenuFromName(menu, item_data->m_sub_menu_name.CStr(), item_data->m_sub_menu_value, 0, 0); 
				}
			}

			return TRUE;
		}
	}

	for (i = 0; i < m_menu_stack.GetCount(); i++)
	{
		if (m_menu_stack.Get(i)->Contains(menu))
		{
			MenuItemData* item_data = GetMenuItemData(m_menu_stack.Get(i)->m_parent, pos_to_init, TRUE);

			InitPopupMenuFromName(menu, item_data->m_sub_menu_name.CStr(), item_data->m_sub_menu_value, item_data->m_sub_menu_start, i + 1); // This is the correct name for the sub sub menu

			return TRUE;
		}
	}

	if ( m_pIContextMenu2 )
	{
		if(NOERROR == m_pIContextMenu2->HandleMenuMsg( WM_INITMENUPOPUP, (WPARAM)menu, MAKELONG(pos_to_init, TRUE) ))
		{
			//this is not our menu
			return FALSE;
		}
	}

	return FALSE;
}

/**********************************************************************************/

BOOL OUIMenuManager::OnMenuCommand(INT32 command)
{
	MenuItemData* item_data = GetMenuItemData(m_current_root, command, FALSE);

	if (item_data)
	{
		if(!item_data->m_input_action)
		{
			return FALSE;
		}
		item_data->m_input_action->SetActionPosition(m_context_menu_mousepos);
		return g_input_manager->InvokeAction(item_data->m_input_action, m_menu_input_context, NULL, TRUE, OpInputAction::METHOD_MENU);
	}

	if (m_pIContextMenu2 )
	{
		// The following paragraph is a kludge to fix bug 100985.
		// Extensive disassembly of the W2K shell showed that it contains an ugly hack,
		// interpreting dwItemData of the first context menu entry as a pointer,
		// expecting its own context menu entries at the beginning.
		// Because we have our Transfer Window entries first, we have to temporarily
		// set this to zero.

		MENUITEMINFO item_info;

		item_info.cbSize = sizeof(MENUITEMINFO);

		item_info.fMask = MIIM_DATA;
		GetMenuItemInfo(m_current_root, 0, TRUE, &item_info);
		ULONG_PTR temp_data = item_info.dwItemData;
		item_info.dwItemData = 0;
		SetMenuItemInfo(m_current_root, 0, TRUE, &item_info);
		item_info.dwItemData = temp_data;

		//  Invoke shell command
        CMINVOKECOMMANDINFO pici;
        pici.cbSize = sizeof( pici);
        pici.fMask = 0;
        pici.hwnd = g_main_hwnd;
        pici.lpVerb = MAKEINTRESOURCEA(command);
        pici.lpParameters = NULL;
        pici.lpDirectory = NULL;
        pici.nShow = SW_SHOWNORMAL;
        pici.dwHotKey = 0;
        pici.hIcon = NULL;
		HRESULT res = m_pIContextMenu2->InvokeCommand(&pici);

		SetMenuItemInfo(m_current_root, 0, TRUE, &item_info);

		if(res == NOERROR)
			return TRUE;	//handled by system
	}

	return FALSE;
}

/**********************************************************************************/

BOOL OUIMenuManager::OnMeasureItem(HWND hwnd, MEASUREITEMSTRUCT* measure_item)
{
	MenuItemData* item_data = NULL;

	if (m_item_data_hashtable.GetData((void*) measure_item->itemData, (void**) &item_data) == OpStatus::OK)
	{
		HFONT font = ((WindowsOpSystemInfo*)g_op_system_info)->GetMenuFont();

		if (item_data->m_bold)
		{
			LOGFONT logfont;

			GetObject( font, sizeof(logfont), &logfont);
			logfont.lfWeight = FW_BOLD;

			font = CreateFontIndirect(&logfont);
		}

		if(item_data->m_separator)
		{
			measure_item->itemHeight = SEPARATOR_HEIGHT;
		}
		else
		{
			HDC hdc = GetDC(hwnd);
			HFONT old_font = (HFONT) SelectObject(hdc, font);

			OpString item_name;
			int amp_pos;
			RETURN_VALUE_IF_ERROR(ParseMenuItemName(item_data->m_item_name, item_name, amp_pos), FALSE);

			SIZE size;
			GetTextExtentPoint(hdc, item_name.CStr(), item_name.Length(), &size);

			if (item_data->m_shortcut.HasContent())
			{
				SIZE shortcut_size;
				GetTextExtentPoint(hdc, item_data->m_shortcut.CStr(), item_data->m_shortcut.Length(), &shortcut_size);
				size.cx += shortcut_size.cx + SUBMENU_ARROW_WIDTH;
			}

			SelectObject(hdc, old_font);
			ReleaseDC(hwnd, hdc);

			if (item_data->m_bold)
			{
				DeleteObject(font);
			}
			int item_height = OUIMenuManager::GetMenuItemHeight();

			INT32 image_height = max(item_height, size.cy);	// 20
			INT32 image_width = image_height;

			size.cy += 4;
			size.cx += (4 + IMAGE_TEXT_OFFSET);

			ULONG tmp_width = size.cx + image_width;
			m_max_width = measure_item->itemWidth = max(m_max_width, tmp_width);
			measure_item->itemHeight = max(size.cy, image_height);
		}
		return TRUE;

	}

	if ( m_pIContextMenu2 )
	{
		if(NOERROR == m_pIContextMenu2->HandleMenuMsg( WM_MEASUREITEM, 0, (LPARAM)measure_item ))
			return TRUE;
	}

	return FALSE;
}

/**********************************************************************************/

#ifndef ODS_NOACCEL
#define ODS_NOACCEL        0x0100
#endif

void AlphaBlt(HDC dst, INT32 dstx, INT32 dsty, INT32 dstwidth, INT32 dstheight, HDC src, INT32 srcx, INT32 srcy, INT32 srcwidth, INT32 srcheight);
extern HBITMAP CreateHBITMAPFromOpBitmap(OpBitmap *bitmap);

BOOL OUIMenuManager::OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* draw_item)
{
	MenuItemData* item_data = NULL;

	m_max_width = 0;

	// When flat menu is enabled, the menu bar uses COLOR_MENUBAR for the menubar background, COLOR_MENU for the menu-popup background, 
	// COLOR_MENUHILIGHT for the fill of the current menu selection, and COLOR_HILIGHT for the outline of the current menu 
	// selection. If disabled, menus are drawn using the same metrics and colors as in Windows 2000.

	COLORREF color_highlight = ::GetSysColor(m_use_flat_menus ? COLOR_MENUHILIGHT : COLOR_HIGHLIGHT);
	HBRUSH brush_highlight = NULL, brush_border = NULL;
	double dGamma = 1.50;
	COLORREF border_highlight = RGB(
		(BYTE)(pow(GetRValue(color_highlight) / 255.0, dGamma) * 255),
		(BYTE)(pow(GetGValue(color_highlight) / 255.0, dGamma) * 255),
		(BYTE)(pow(GetBValue(color_highlight) / 255.0, dGamma) * 255));
	
	dGamma = 0.80;
	color_highlight = RGB(
		(BYTE)(pow(GetRValue(color_highlight) / 255.0, dGamma) * 255),
		(BYTE)(pow(GetGValue(color_highlight) / 255.0, dGamma) * 255),
		(BYTE)(pow(GetBValue(color_highlight) / 255.0, dGamma) * 255));

	brush_highlight = CreateSolidBrush(color_highlight);
	brush_border = CreateSolidBrush(border_highlight);

	if (m_item_data_hashtable.GetData((void*) draw_item->itemData, (void**) &item_data) == OpStatus::OK && draw_item->itemAction & (ODA_DRAWENTIRE|ODA_SELECT))
	{
		HFONT font = ((WindowsOpSystemInfo*)g_op_system_info)->GetMenuFont();

		if (item_data->m_bold)
		{
			LOGFONT logfont;

			GetObject( font, sizeof(logfont), &logfont);
			logfont.lfWeight = FW_BOLD;

			font = CreateFontIndirect(&logfont);
		}

		HFONT old_font = (HFONT) SelectObject(draw_item->hDC, font);

		SetBkMode(draw_item->hDC, TRANSPARENT);

		BOOL disabled = draw_item->itemState & ODS_DISABLED;
		BOOL selected = draw_item->itemState & ODS_SELECTED && !disabled;

		INT32 state = 0;

		if (selected)
		{
			// if action has a desctiption text, print it as status text

			if (item_data->m_input_action && item_data->m_input_action->GetActionInfo().HasStatusText() && g_application->GetActiveDesktopWindow())
			{
				g_application->GetActiveDesktopWindow()->SetStatusText(item_data->m_input_action->GetActionInfo().GetStatusText());
			}

			state = SKINSTATE_HOVER | (item_data->m_sub_menu_name.HasContent() ? SKINSTATE_OPEN : 0);
		}
		else
		{
			OpString empty;
			DesktopWindow* desktop_window = g_application->GetActiveDesktopWindow();

			if (desktop_window)
			{
				desktop_window->SetStatusText(empty);
			}
		}

		if (draw_item->itemState & ODS_CHECKED)
		{
			state |= SKINSTATE_SELECTED;
		}

		if (disabled)
		{
			state |= SKINSTATE_DISABLED;
		}

		if (UiDirection::Get() == UiDirection::RTL)
			state |= SKINSTATE_RTL;

		INT32 real_width, real_height;

		item_data->m_image.GetSize(&real_width, &real_height);
		
		OpRect image_rect(&draw_item->rcItem);

		INT32 image_width = image_rect.height - 4;

		image_width = max(image_width, 16);
		image_rect.width = image_width + IMAGE_TEXT_OFFSET;

		item_data->m_image.SetRestrictImageSize(real_height >= 16 || real_height >= 16);

		image_rect = item_data->m_image.CalculateScaledRect(image_rect, TRUE, TRUE);

		// paint background
		FillRect(draw_item->hDC, &draw_item->rcItem, ::GetSysColorBrush(COLOR_MENU));

		if(selected && !disabled)
		{
			FillRect(draw_item->hDC, &draw_item->rcItem, brush_highlight);
//			FrameRect(draw_item->hDC, &draw_item->rcItem, ::GetSysColorBrush(m_use_flat_menus ? COLOR_MENUHILIGHT : COLOR_HIGHLIGHT));
			FrameRect(draw_item->hDC, &draw_item->rcItem, brush_border);
		}
		if(selected && disabled)
		{
			FrameRect(draw_item->hDC, &draw_item->rcItem, ::GetSysColorBrush(m_use_flat_menus ? COLOR_MENUHILIGHT : COLOR_HIGHLIGHT));
		}
		if(!selected)
		{
			// left, top, right, bottom
//			RECT box_rect = { draw_item->rcItem.left, draw_item->rcItem.top, draw_item->rcItem.left + image_width + IMAGE_TEXT_OFFSET, draw_item->rcItem.bottom };

//			FillRect(draw_item->hDC, &box_rect, ::GetSysColorBrush(COLOR_BTNHIGHLIGHT));
		}
		if(item_data->m_separator)
		{
			// draw separator
			RECT rc = draw_item->rcItem;
			rc.top += (rc.bottom - rc.top) / 2;      // vertical center
			rc.left += image_width + IMAGE_TEXT_OFFSET;

			::DrawEdge(draw_item->hDC, &rc, EDGE_ETCHED, BF_TOP);   // draw separator line
		}
		else
		{
			item_data->m_image.SetState(state, -1, state & SKINSTATE_HOVER ? 100 : 0);

			if (image_rect.width && image_rect.height)
			{
				OpBitmap* opbmp;
				if (OpStatus::IsSuccess(OpBitmap::Create(&opbmp, image_rect.width, image_rect.height, FALSE, TRUE, 0, 0, TRUE)))
				{
					// Clear the bitmap to transparent
					opbmp->SetColor(NULL, TRUE, NULL);
					OpPainter* painter = opbmp->GetPainter();
					if (painter)
					{
						VisualDevice vd;
						vd.painter = painter;

						item_data->m_image.Draw(&vd, OpRect(0, 0, image_rect.width, image_rect.height), NULL, SKINSTATE_MINI);

						opbmp->ReleasePainter();

						HBITMAP bm = CreateHBITMAPFromOpBitmap(opbmp);
						if(bm)
						{
							HDC screen_dc = GetDC(NULL);
							HDC imgdc = CreateCompatibleDC(screen_dc);

							HBITMAP old_bm = (HBITMAP) SelectObject(imgdc, bm);

							AlphaBlt(draw_item->hDC, image_rect.x, image_rect.y, image_rect.width, image_rect.height, imgdc, 0, 0, image_rect.width, image_rect.height);

							SelectObject(imgdc, old_bm);

							ReleaseDC(NULL, screen_dc);
							DeleteDC(imgdc);

							DeleteObject(bm);
						}
					}
					OP_DELETE(opbmp);
				}
			}

			BOOL show_underline = TRUE;
			if (!m_force_shortcut_underline)
				SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_underline, 0);

			if (IsSystemWin2000orXP() && !show_underline)
				show_underline = (draw_item->itemState & ODS_NOACCEL) == 0;

			OpString item_name_copy;
			INT32 item_name_len = item_data->m_item_name.Length();
			item_name_copy.Set(item_data->m_item_name.DataPtr(), item_name_len);

			// remove the appended shortcut key "(X)" when underline is not needed
			if(!show_underline)
			{
				INT32 pos = item_name_copy.FindLastOf(UNI_L('('));
				if(pos != KNotFound
					&& item_name_len > pos+3
					&& item_name_copy[pos+1] == UNI_L('&')
					&& item_name_copy[pos+3] == UNI_L(')') )
				{
					item_name_copy.Delete(pos,4);
				}
			}

			OpString item_name;
			int underline_pos = -1;
			ParseMenuItemName(item_name_copy, item_name, underline_pos);

			RECT name_rc = draw_item->rcItem;
			RECT shortcut_rc = draw_item->rcItem;

			name_rc.left += image_width + IMAGE_TEXT_OFFSET;
			if (item_data->m_shortcut.HasContent())
			{
				SIZE shortcut_size;
				GetTextExtentPoint(draw_item->hDC, item_data->m_shortcut.CStr(), item_data->m_shortcut.Length(), &shortcut_size);
				shortcut_rc.left = draw_item->rcItem.right - shortcut_size.cx - SUBMENU_ARROW_WIDTH;
			}
			name_rc.bottom -= 1;
			shortcut_rc.bottom -= 1;

			COLORREF color_text = ::GetSysColor(disabled ? (selected ? COLOR_GRAYTEXT : COLOR_3DSHADOW) : (selected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));
			SetTextColor(draw_item->hDC, color_text);

			DrawMenuItemText(draw_item->hDC, item_name, &name_rc, show_underline ? underline_pos : -1);

			if (item_data->m_shortcut.HasContent())
				DrawMenuItemText(draw_item->hDC, item_data->m_shortcut, &shortcut_rc);
		}

		SelectObject(draw_item->hDC, old_font);

		if (item_data->m_bold)
		{
			DeleteObject(font);
		}
		DeleteObject(brush_highlight);
		DeleteObject(brush_border);

		return TRUE;
	}
	else
	{
		OP_ASSERT(!"Item without item data attached to it, something is wrong with the creation of it!");
	}
	DeleteObject(brush_highlight);
	DeleteObject(brush_border);

	if ( m_pIContextMenu2 )
	{
		if(NOERROR == m_pIContextMenu2->HandleMenuMsg( WM_DRAWITEM, 0, (LPARAM)draw_item ))
			return TRUE;
	}

	return FALSE;
}

/**********************************************************************************/
INT32	OUIMenuManager::GetSelectedMenuItem(HMENU menu)
{
	INT32 count = GetMenuItemCount(menu);
	for (INT32 i = 0; i < count; i++)
	{
		MENUITEMINFO item_info;

		item_info.cbSize = sizeof(MENUITEMINFO);

		item_info.fMask = MIIM_STATE;
		item_info.dwItemData = NULL;

		if(GetMenuItemInfo(menu, i, TRUE, &item_info) && (item_info.fState & MFS_HILITE))
			return i;
	}
	return -1;
}



/**********************************************************************************/

INT32 OUIMenuManager::OnMenuChar(uni_char letter, HMENU menu)
{
	letter = uni_toupper(letter);
	
	INT32 count = GetMenuItemCount(menu);
	INT32 current_selected = GetSelectedMenuItem(menu);
	INT32 current_item = current_selected+1;
	INT32 to_be_selected = -1;

	for (INT32 i = 0; i < count; i++)
	{
		if(current_item>count-1)
			current_item -= count;

		MenuItemData* item = GetMenuItemData(menu, current_item, TRUE);

		if (item)
		{
			const uni_char* name = item->m_item_name.CStr();
			
			while (name && *name)
			{
				if (*name == '&')
				{
					name++;
					
					if (uni_toupper(*name) == letter)
					{
						// ignore disabled item
						MENUITEMINFO item_info;

						item_info.cbSize = sizeof(MENUITEMINFO);

						item_info.fMask = MIIM_STATE;
						item_info.dwItemData = NULL;
						GetMenuItemInfo(menu, current_item, TRUE, &item_info);

						if(~item_info.fState & MFS_DISABLED)
						{
							if(to_be_selected != -1)
							{
								// can be sure now there are multiple items with the same shortcut key. 
								return MAKELONG(to_be_selected, MNC_SELECT);
							}
							else
							{
								to_be_selected = current_item;
							}
						}
					}
					
					continue;
				}
				name++;
			}

		}
		current_item++;
	}

	// the only item that matches the shortcut key
	if(to_be_selected != -1)
		return MAKELONG(to_be_selected, MNC_EXECUTE);

	if ( m_pIContextMenu2 )
	{
		m_pIContextMenu2->HandleMenuMsg( WM_MENUCHAR, (WPARAM)letter, (LPARAM)menu );
	}
	
	return 0;
}

BOOL OUIMenuManager::OnMenuButtonUp(POINT pt)
{
	MenuItemData* item = GetMenuItemByLocation(m_current_root, pt);
	if (item && item->IsBookmark())
	{
		ShowBookmarkContextMenu(item);
		return TRUE;
	}
	return FALSE;
}

void OUIMenuManager::OnPopupMenu(const char* popup_name, const PopupPlacement& placement, BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context)
{
	OP_NEW_DBG("OUIMenuManager::OnPopupMenu()", "desktop-scope");

	OP_DBG(("popup_name = '%s'", popup_name));

	m_use_keyboard_context = use_keyboard_context;

	m_popup_root_name = popup_name;
	m_popup_root_value = popup_value;

	POINT cursor_pt;
	GetCursorPos(&cursor_pt);
	m_context_menu_mousepos = OpPoint(cursor_pt.x, cursor_pt.y);

	int flags = 0;
	OpPoint point;
	OpRect avoid_rect;

	switch (placement.type)
	{
		case PopupPlacement::POPUP_WINDOW_CENTER:
		{
			flags = TPM_CENTERALIGN | TPM_VCENTERALIGN;
			RECT rect;
			MonitorRectFromPoint(rect, cursor_pt, FALSE);
			m_context_menu_pos = OpRect(&rect).Center();
			break;
		}
		case PopupPlacement::POPUP_CENTER:
			flags = TPM_CENTERALIGN | TPM_VCENTERALIGN;
			m_context_menu_pos = placement.rect.Center();
			break;

		case PopupPlacement::POPUP_CURSOR:
			flags = TPM_TOPALIGN | (UiDirection::Get() == UiDirection::LTR ? TPM_LEFTALIGN : TPM_RIGHTALIGN);
			m_context_menu_pos = OpPoint(cursor_pt.x, cursor_pt.y);
			break;

		case PopupPlacement::POPUP_BELOW:
			flags = TPM_TOPALIGN;
			if (UiDirection::Get() == UiDirection::LTR)
			{
				flags |= TPM_LEFTALIGN;
				m_context_menu_pos = placement.rect.BottomLeft();
			}
			else
			{
				flags |= TPM_RIGHTALIGN;
				m_context_menu_pos = placement.rect.BottomRight();
				// Ugly workaround! Used to display correctly popup OMenu
				// when Opera window is maximized on the left display (in case
				// of more than 1 display)
				OpScreenProperties screen_props;
				g_op_screen_info->GetProperties(&screen_props, g_application->GetActiveBrowserDesktopWindow()->GetOpWindow());
				if (m_context_menu_pos.x == screen_props.workspace_rect.Right())
					m_context_menu_pos.x -= 1;
			}
			avoid_rect = placement.rect;
			break;

		case PopupPlacement::POPUP_RIGHT:
			flags = TPM_TOPALIGN;
			if (UiDirection::Get() == UiDirection::LTR)
			{
				flags |= TPM_LEFTALIGN;
				m_context_menu_pos = placement.rect.TopRight();
			}
			else
			{
				flags |= TPM_RIGHTALIGN;
				m_context_menu_pos = placement.rect.TopLeft();
			}
			avoid_rect = placement.rect;
			break;

		default:
			OP_ASSERT(!"Unhandled placement type");
	}

	// Fix for DSK-295426, don't eat mouse down message if it's in a page
	if (!point_in_page)
		m_avoid_rect = avoid_rect;

	m_active_menu_owner = g_main_hwnd;

	BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
	if (browser_window)
	{
		WindowsOpWindow *browser_op_window = reinterpret_cast<WindowsOpWindow *>(browser_window->GetOpWindow());
		if (browser_op_window)
		{
			m_active_menu_owner = browser_op_window->m_hwnd;
		}
	}

	if (!(GetKeyState(VK_LBUTTON) & 0x8000))
		flags |= TPM_RIGHTBUTTON;

	if (UiDirection::Get() == UiDirection::RTL)
		flags |= TPM_LAYOUTRTL | TPM_HORPOSANIMATION | TPM_VERPOSANIMATION;

	TPMPARAMS avoid;
	avoid.cbSize = sizeof(TPMPARAMS);
	avoid.rcExclude = avoid_rect;

	m_menu_input_context = NULL;
	m_active_menu = m_popup_root;

	SetCursor(hArrowCursor);

	OP_DBG(("m_active_menu = %p, m_popup_root_name = '%s'", m_active_menu, m_popup_root_name));

	DisableMenuAnimationParameter();
	int cmd_id = TrackPopupMenuEx(m_popup_root, flags | TPM_RETURNCMD,
			m_context_menu_pos.x, m_context_menu_pos.y, m_active_menu_owner,
			avoid_rect.IsEmpty() ? NULL : &avoid);
	RestoreMenuAnimationParameter();

	m_force_shortcut_underline = FALSE;
	m_active_menu = NULL;
	m_active_menu_owner = NULL;
	m_menu_input_context = m_use_keyboard_context ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext();

	OP_DBG(("m_active_menu = NULL, m_popup_root_name = NULL"));

	m_avoid_rect.Set(0, 0, 0, 0);

	m_popup_root_name = NULL;
	m_popup_root_value = -1;

	if (cmd_id)
		OnMenuCommand(cmd_id);

	if (m_pIContextMenu2)
	{
		m_pIContextMenu2->Release();
		m_pIContextMenu2 = NULL;
	}
}

void OUIMenuManager::ShowBookmarkContextMenu(MenuItemData* item)
{
	OpInputAction* item_action = item->m_input_action;
	HMENU ctx_menu = CreatePopupMenu();

	OpInputContext* ctx = m_use_keyboard_context ? g_input_manager->GetKeyboardInputContext()
								: g_input_manager->GetMouseInputContext();

	BookmarkMenuContext context(item_action->GetActionData());
	context.SetParentInputContext(ctx);

	g_application->GetMenuHandler()->AddMenuItems(ctx_menu, "Bookmark Menu Context Menu", 0, 0, &context);

	POINT cursor_pt;
	GetCursorPos(&cursor_pt);

	BOOL is_changed = FALSE;
	if (m_active_menu_owner == NULL)
	{
		BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
		if (browser_window)
		{
			WindowsOpWindow *browser_op_window = reinterpret_cast<WindowsOpWindow *>(browser_window->GetOpWindow());
			if (browser_op_window)
			{
				m_active_menu_owner = browser_op_window->m_hwnd;
				is_changed = TRUE;
			}
		}
	}

	int cmd_id = TrackPopupMenuEx(ctx_menu, TPM_RECURSE | TPM_RETURNCMD, cursor_pt.x, cursor_pt.y, m_active_menu_owner, NULL);

	if (cmd_id)
	{
		MenuItemData* item_data = GetMenuItemData(ctx_menu, cmd_id, FALSE);
		if (item_data)
		{
			item_data->m_input_action->SetActionData(item_action->GetActionData());
			item_data->m_input_action->SetActionDataString(item_action->GetActionDataString());

			g_input_manager->InvokeAction(item_data->m_input_action, &context, NULL, TRUE, OpInputAction::METHOD_MENU);

			PostMessage(m_active_menu_owner, WM_CANCELMODE, 0, 0);
		}
	}

	if (is_changed)
		m_active_menu_owner = NULL;

	DestroyMenuItems(ctx_menu);
	DestroyMenu(ctx_menu);
}

OP_STATUS OUIMenuManager::GetAllMenuInfo(HMENU menu, const char* menu_name, OpScopeDesktopWindowManager_SI::QuickMenuList &out)
{
	OP_NEW_DBG("OUIMenuManager::GetAllMenuInfo()", "desktop-scope");
	OP_ASSERT(menu);

	OP_DBG(("menu = %p, menu_name = '%s'", menu, menu_name));

	OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuInfo, ()));
	RETURN_OOM_IF_NULL(info.get());

	if (OpStatus::IsSuccess(SetQuickMenuInfo(menu, m_active_menu_owner, menu_name, *info.get())))
	{
		if (OpStatus::IsSuccess(out.GetMenuListRef().Add(info.get())))
			info.release();
	}

	for (UINT32 i=0; i<m_menu_stack.GetCount(); i++) //OR: if (m_menu_stack.Get(i)->m_parent == m_active_menu)
	{
		UINT32 num_submenus = m_menu_stack.Get(i)->m_menus.GetCount();

		OP_DBG(("Iterating m_menu_stack, item %d", i));
		for (UINT32 j=0; j<num_submenus; j++)
		{
			OP_DBG(("\tIterating submenus, item %d", j));

			HMENU sub_menu = *m_menu_stack.Get(i)->m_menus.Get(j);
			if (GetMenuItemCount(sub_menu) <= 0)
				continue;
				
			OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuInfo, ()));
			RETURN_OOM_IF_NULL(info.get());

			// This must be possible to do in a simpler way!
			HMENU parent_menu = m_menu_stack.Get(i)->m_parent;
			UINT32 count = GetMenuItemCount(parent_menu);
			MenuItemData* submenuitemdata = NULL;
			for (UINT32 k = 0; k < count; k++)
			{
				HMENU menu_item = GetSubMenu(parent_menu, k);
				if (menu_item == sub_menu)
				{
					// Find the item at that position to find the menus name
					submenuitemdata = GetMenuItemData(parent_menu, k, TRUE);
				}
			}

			OP_DBG(("\t\tsub_menu = %p, m_active_menu_owner = %p, name = '%s'", sub_menu, m_active_menu_owner, submenuitemdata->m_sub_menu_name.CStr()));

			if (OpStatus::IsSuccess(SetQuickMenuInfo(sub_menu, m_active_menu_owner, submenuitemdata->m_sub_menu_name, *info.get())))
			{
				if (OpStatus::IsSuccess(out.GetMenuListRef().Add(info.get())))
					info.release();
			}
		}
	}

	return OpStatus::OK;
}

BOOL OUIMenuManager::ListPopupMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuList &out)
{
	OP_NEW_DBG("OUIMenuManager::ListPopupMenuInfo()", "desktop-scope");
	OP_DBG(("m_active_menu = %p, m_inpage_popup_menu = %p", m_active_menu, m_inpage_popup_menu));

	// In-page pop-up menu
	if (m_inpage_popup_menu)
		RETURN_VALUE_IF_ERROR(GetAllMenuInfo(m_inpage_popup_menu, m_inpage_popup_menu_name.CStr(), out), FALSE);
	else if (m_active_menu)
		RETURN_VALUE_IF_ERROR(GetAllMenuInfo(m_active_menu, m_popup_root_name, out), FALSE);

#if _DEBUG
	OP_DBG(("Available menus:"));
	for (UINT32 i=0; i<out.GetMenuList().GetCount(); i++)
	{
		OP_DBG(("\t'%S'", out.GetMenuList().Get(i)->GetMenuId().GetMenuName()));
	}
#endif

	return TRUE;
}


/**********************************************************************************/

LRESULT CALLBACK OUIMenuManager::StaticMsgHook(int code, WPARAM wparam, LPARAM lparam)
{
	return g_windows_menu_manager->MsgHook(code, wparam, lparam); 
}

LRESULT CALLBACK OUIMenuManager::MsgHook(int code, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = CallNextHookEx(m_hook, code, wparam, lparam);

	/*if(code == MSGF_MENU && lparam = 1024)
	{

	}*/

	if (m_activate_first_item)
	{
		keybd_event(VK_DOWN, 0, 0, 0);
		m_activate_first_item = FALSE;
	}

	if (!m_active_menu_bar && code == MSGF_MENU)
	{
		MSG* msg = (MSG*) lparam;

		// Cancel popup menu and eat lbuttondown if user clicked the same button
		if (msg->message == WM_LBUTTONDOWN)
		{ 
			OpPoint p = OpPoint(LOWORD(msg->lParam), HIWORD(msg->lParam));
			POINT pp;
			pp.x = p.x;
			pp.y = p.y;

			if (m_avoid_rect.Contains(p) 
				// in case avoid rect and menu rect overlap, click on menu shouldn't be intercepted
				&& !GetMenuItemByLocation(m_current_root,pp))
			{
				PostMessage(msg->hwnd, WM_CANCELMODE, 0, 0);
				return TRUE;
			}
		}
		else if (msg->message == WM_RBUTTONUP)
		{
			// Fix for DSK-319854.
			// When right clicking on flash we won't get the RButtonUp message
			// in which case the mouse gesture UI will pop up. So kill it here.
			g_input_manager->ResetInput();

			// Show the context menu for bookmark menu item
			POINT p;
			p.x = LOWORD(msg->lParam);
			p.y = HIWORD(msg->lParam);
			if (g_windows_menu_manager->OnMenuButtonUp(p))
				return TRUE; // consumed
		}
	}

	// OUIMenuManager needs to sniff in on the following messages
	// WM_KEYDOWN		so we can move to next menu
	// WM_MOUSEMOVE		so we can detect moving to another menu
	// WM_MENUSELECT    so we know which item is selected

	if (m_active_menu_bar && code == MSGF_MENU)
	{
		MSG* msg = (MSG*) lparam;

		switch (msg->message)
		{
			case WM_LBUTTONDOWN:
			{
				POINT pt = {LOWORD(msg->lParam), HIWORD(msg->lParam)};

				ScreenToClient(m_active_menu_bar->GetHWnd(), &pt);

				OUIMenuBarItem* item = m_active_menu_bar->GetMenuBarItemFromPoint(pt.x, pt.y);

				if (item && item == m_active_menu_bar_item && item != m_active_menu_bar->m_mdi_item)
				{
					PostMessage(msg->hwnd, WM_CANCELMODE, 0, 0);
					return TRUE;
				}
				break;
			}

			case WM_MOUSEMOVE:
			{

				if (m_eat_mousemove == 0)
				{			
					POINT pt = {(short) LOWORD(msg->lParam), (short) HIWORD(msg->lParam)};

					HWND hwnd = WindowFromPoint(pt);

					if (hwnd)
					{
						OUIWindow* oui_window = OUIWindow::StaticGetWindowObject(hwnd);

						if (oui_window && oui_window == m_active_menu_bar)
						{
							OUIMenuBar* menu_bar = (OUIMenuBar*) oui_window;

							ScreenToClient(menu_bar->GetHWnd(), &pt);

							OUIMenuBarItem* item = menu_bar->GetMenuBarItemFromPoint(pt.x, pt.y);

							if (item && item != m_active_menu_bar_item)
							{
								PostMessage(msg->hwnd, WM_CANCELMODE, 0, 0);

								PostMessage(menu_bar->GetHWnd(), WM_LBUTTONDOWN, 0, (LPARAM) item);
							}
						}
					}
				}
				else
				{
					m_eat_mousemove--;
				}
				break;
			}

			case WM_KEYDOWN:
			{
				const bool ltr = UiDirection::Get() == UiDirection::LTR;
				switch (msg->wParam)
				{
					case VK_LEFT:
					{
						if ((ltr && m_leftmost) || (!ltr && m_rightmost))
						{
							m_eat_mousemove = 3;
							PostMessage(msg->hwnd, WM_CANCELMODE, 0, 0);
							SendMessage(m_active_menu_bar->GetHWnd(), WM_KEYDOWN, VK_LEFT, 0);
						}
						break;
					}

					case VK_RIGHT:
					{
						if ((ltr && m_rightmost) || (!ltr && m_leftmost))
						{
							m_eat_mousemove = 3;
							PostMessage(msg->hwnd, WM_CANCELMODE, 0, 0);
							SendMessage(m_active_menu_bar->GetHWnd(), WM_KEYDOWN, VK_RIGHT, 0);
						}
						break;
					}
				}
				break;
			}

			case WM_MENUSELECT:
			{
				UINT item = (UINT) LOWORD(msg->wParam);
				UINT flags = (UINT) HIWORD(msg->wParam);
				HMENU menu = (HMENU) msg->lParam;

				m_leftmost = menu == m_active_menu;

				if(m_active_menu_bar_item && m_active_menu_bar_item->IsMDIItem())
				{
					BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
					if(browser_window)
					{
						WindowsOpWindow *browser_op_window = reinterpret_cast<WindowsOpWindow *>(browser_window->GetOpWindow());
						if(browser_op_window)
						{
							m_leftmost = m_leftmost || menu == GetSystemMenu(browser_op_window->m_hwnd, FALSE);
						}
					}
				}

				if (flags & MF_POPUP)
				{
					MENUITEMINFO item_info;
					memset(&item_info, 0, sizeof(MENUITEMINFO));

					item_info.cbSize = sizeof(MENUITEMINFO);

					item_info.fMask = MIIM_STATE;

					GetMenuItemInfo(menu, item, TRUE, &item_info);

					m_rightmost = item_info.fState & MFS_DISABLED;
				}
				else
				{
					m_rightmost = TRUE;
				}

				break;
			}
		}
	}

	return result;
}

/**********************************************************************************/

LRESULT CALLBACK OUIMenuManager::StaticKeyboardHook(int code, WPARAM wparam, LPARAM lparam)
{
	return g_windows_menu_manager->KeyboardHook(code, wparam, lparam);
}

LRESULT CALLBACK OUIMenuManager::KeyboardHook(int code, WPARAM wparam, LPARAM lparam)
{
	if (code == HC_ACTION)
	{
		BOOL repeated	= lparam & (1<<30);
		BOOL up			= lparam & (1<<31);
		BOOL down		= !repeated && !up;

		switch (wparam)
		{
			case VK_MENU:
			{
				// don't pop up another menu if this alt keystroke is to close a menu, fix DSK-258216
				static BOOL closing_menu = FALSE;

				if (down)
				{
					m_opening_menu = TRUE;

					// menu is scheduled to open/focus on keyup unless it is open already
					if (g_application->GetMenuHandler()->IsShowingMenu())
					{
						closing_menu = TRUE;
						m_opening_menu = FALSE;
					}
				}

				if(up && m_alt_key_down)
				{
					if(closing_menu)
					{
						closing_menu = FALSE;
					}
					else if (!m_opening_menu)
					{
						// something stopped menu opening (for example mousedown event in WindowsOpWindow::HandleMouseEvents)
						// we must cancel, otherwise standard menubar can't be prevented from gaining focus
						CancelAltKey();
					}
					else
					{
						// if the menu bar is hidden, m_menu_bars will contain no items. If that is the case, check for the prefs setting
						// and fire the action
						if(!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu))
						{
							// fire action to show just the dropdown anyhow as we pressed alt
							g_input_manager->InvokeAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_MAIN_MENU)));
						}
					}
					// we have either opened or cancelled menu so reset state
					m_opening_menu = FALSE;
				}
				if ((up || down) && SetAltKey(down))
				{
					return TRUE;
				}
				break;
			}

			default:
			{
				m_alt_key_down = FALSE;
				if (up)
				{
					CancelAltKey();
				}
				else
				{
					BYTE lpKeyState[256];
					char scancode = lparam<<8>>24;
					uni_char uni_ch = wparam;

					if(m_alt_down_bar)
					{
						if (   wparam >= 0x30 && wparam <= 0x5A // VK_0 - VK_Z, alphanumeric 
							|| wparam >= VK_NUMPAD0 && wparam <= VK_NUMPAD9 
							|| wparam >= VK_OEM_1 && wparam <= VK_OEM_8 // alphabet of some languages has more than 26 chracters. they use OEM key for extra chracters.
							) // fix for DSK-256039, calling ToUnicode with only those keys which could be shortcut keys. otherwise it'll cause some wired consequences 
						{
							if(GetKeyboardState(lpKeyState))
							{
								INT32 ret = ToUnicode(wparam,scancode,lpKeyState,(LPWSTR)&uni_ch,1,0);
								if(ret == 1)
								{
									uni_ch = uni_toupper(uni_ch);
								}
								else
								{
									uni_ch = wparam;
								}
							}
						}
						
						if (SetKeyDown(uni_ch))
						{
							return TRUE;
						}
					}
				}
				break;
			}
		}
	}
	return CallNextHookEx(m_keyboard_hook, code, wparam, lparam);
}

/***********************************************************************************
**
**	OUIMenuBar
**
***********************************************************************************/

OUIMenuBar::OUIMenuBar(OUIMenuManager* menu_manager, HWND parent, const char* menu_bar_name)
{
	m_skin = NULL;
	m_menu_manager			= menu_manager;
	m_alt_key_down			= FALSE;
	m_border_type			= 0;
	m_hover_item			= NULL;
	m_focus					= NULL;
	m_old_focus				= NULL;
	m_mdi_item				= NULL;
	m_mdi_minimize			= NULL;
	m_mdi_restore			= NULL;
	m_mdi_close				= NULL;

	PrefsSection *section = NULL;
	
	TRAPD(err, section = g_setup_manager->GetSectionL(menu_bar_name, OPMENU_SETUP));
	
	if (section)
	{
		MenuShortcutLetterFinder letter_finder;
		for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
			OpLineParser line(entry->Key());

			OpString item_type;
			OpString item_name;
			OpString sub_menu_name;
			INT32 sub_menu_value = 0;

			RETURN_VOID_IF_ERROR(line.GetNextToken(item_type));

			if( uni_stristr(item_type.CStr(), UNI_L("Platform")) != NULL )
			{
				if( uni_stristr(&item_type.CStr()[8], UNI_L("Windows")) == NULL )
				{
					continue;
				}

				RETURN_VOID_IF_ERROR(line.GetNextToken(item_type));
			}

			if( uni_stristr(item_type.CStr(), UNI_L("Feature")) != NULL )
			{
				BOOL skip = TRUE;

#if defined(M2_SUPPORT)
				if( uni_stristr(&item_type.CStr()[8], UNI_L("Mail")) != NULL )
				{
					if (g_application->HasMail())
					{
						skip = FALSE;
					}
				}
				if( uni_stristr(&item_type.CStr()[8], UNI_L("Chat")) != NULL )
				{
					if (g_application->HasChat())
					{
						skip = FALSE;
					}
				}
				if( uni_stristr(&item_type.CStr()[8], UNI_L("Feeds")) != NULL )
				{
					if (g_application->HasFeeds())
					{
						skip = FALSE;
					}
				}
#endif
				if( uni_stristr(&item_type.CStr()[8], UNI_L("Tabbed")) != NULL )
				{
					if (!g_application->IsSDI())
					{
						skip = FALSE;
					}
				}
				if( uni_stristr(&item_type.CStr()[8], UNI_L("SDI")) != NULL )
				{
					if (g_application->IsSDI())
					{
						skip = FALSE;
					}
				}
				if( uni_stristr(&item_type.CStr()[8], UNI_L("MDI")) != NULL )
				{
					if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowWindowMenu)) // previously tagged as "MDI" in menu setups
					{
						skip = FALSE;
					}
				}
				if (skip)
					continue;

				RETURN_VOID_IF_ERROR(line.GetNextToken(item_type));
			}

			BOOL submenu = uni_stristr(item_type, UNI_L("Submenu")) != NULL;

			if (!submenu)
				continue;

			Str::LocaleString id(0);
			
			line.GetNextLanguageString(item_name, &id);
			RETURN_VOID_IF_ERROR(line.GetNextToken(sub_menu_name));
			line.GetNextValue(sub_menu_value);

			OpString default_language_item_name;
			
			if (g_application->GetDefaultLanguageManager())
			{
				g_application->GetDefaultLanguageManager()->GetString(id, default_language_item_name);
			}

			OpString item_name_new;
			uni_char shortcut = letter_finder.CreateMenuString(item_name, item_name_new, default_language_item_name.CStr());

			OUIMenuBarItem* item = OP_NEW(OUIMenuBarItem, (this, item_name_new, sub_menu_name, sub_menu_value, shortcut));
			if (item)
				item->Into(&m_items);
		}
		OP_DELETE(section);
	}

	Create(NULL, WS_CHILD|WS_CLIPSIBLINGS, WS_EX_CONTROLPARENT, 0, 0, 0, 0, parent, NULL);
}

OUIMenuBar::~OUIMenuBar()
{
	m_items.Clear();
	Destroy();
}

/**********************************************************************************/

void MoveWindowRect(HWND hwnd, RECT* rect)
{
	MoveWindow(hwnd, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, TRUE);
}

void OUIMenuBar::ArrangeMenubar(RECT *rect)
{
	RECT menubar_rect = *rect;

	SIZE size = GetSize(rect);

	menubar_rect.bottom = menubar_rect.top + size.cy;
	rect->top += size.cy;

	MoveWindowRect(m_HWnd, &menubar_rect);
	LayoutItems();
	ShowWindow();
}

/**********************************************************************************/

SIZE OUIMenuBar::GetSize(RECT *rect)
{
	SIZE size;

	size.cx = 0;

	OpButton* button;
	RETURN_VALUE_IF_ERROR(OpButton::Construct(&button), size);

	VisualDevice vd;

	vd.painter = m_parent_opwindow->m_mde_screen->GetVegaPainter();

	button->SetVisualDevice(&vd);
	button->string.SetConvertAmpersand(TRUE);

	button->SetButtonTypeAndStyle(OpButton::TYPE_MENU, OpButton::STYLE_IMAGE);
	button->GetForegroundSkin()->SetImage("Window Document Icon");

	INT32 height;
	INT32 width;

	button->GetRequiredSize(width, height);

	INT32 left, right, top, bottom;

	// ensure correct skin is used
	SetButtonSkin(button);

	button->GetBorderSkin()->GetMargin(&left, &top, &right, &bottom);

	height = height + top + bottom;
	width = width + left + right;

	button->SetButtonTypeAndStyle(OpButton::TYPE_MENU, OpButton::STYLE_TEXT);
	button->SetText(UNI_L("Dummy"));

	INT32 text_height;
	INT32 text_width;

	button->GetRequiredSize(text_width, text_height);

	size.cy = max(text_height, height);

	vd.painter = NULL;

	button->Delete();
	return size;
}

/**********************************************************************************/

BOOL OUIMenuBar::SetAltKey(BOOL down)
{
	m_menu_manager->m_alt_down_bar = down ? this : NULL;

	if (m_alt_key_down != down)
	{
		m_alt_key_down = down;

		if (m_HWnd)
		{
			if (!down)
			{
				if (!m_focus)
				{
					SetFocus();
				}
				else if(m_old_focus)
				{
					RestoreFocus();
				}
				return TRUE;
			}
			else if (m_focus && m_old_focus)
			{
				// Tentative fix for Japanese IME requiring Alt+other button for activation (johan)
				m_alt_key_down = FALSE;
				RestoreFocus();
				RedrawWindow();
			}
			else
			{
				RedrawWindow();
			}
		}
	}

	return FALSE;
}

/**********************************************************************************/

void OUIMenuBar::CancelAltKey()
{
	m_menu_manager->m_alt_down_bar = NULL;
	m_alt_key_down = FALSE;
	RestoreFocus();
	RedrawWindow();
}

/**********************************************************************************/

BOOL OUIMenuBar::SetKeyDown(int key)
{
	OUIMenuBarItem* item = (OUIMenuBarItem*) m_items.First();

	while (item && item->m_accel != key)
	{
		item = (OUIMenuBarItem*) item->Suc();
	}

	if (!item && m_mdi_item && key == 0xbd /*VK_OEM_MINUS*/)
	{
		item = m_mdi_item;
	}

	if (item)
	{
		::PostMessage(GetHWnd(), WM_LBUTTONDOWN, MK_SHIFT, (LPARAM) item);
		CancelAltKey();
		return TRUE;
	}
	else
	{
		CancelAltKey();
		return FALSE;
	}
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnKeyDown(WPARAM wparam, LPARAM lparam)
{
	switch (wparam)
	{
		case VK_ESCAPE:
		{
			if (m_old_focus)
			{
				RestoreFocus();
			}
			break;
		}
	}

	BOOL cycle = FALSE;
	BOOL cycle_forward = TRUE;
	BOOL click = FALSE;

	switch (wparam)
	{
		case VK_UP:	break;
		case VK_LEFT:	cycle = TRUE; cycle_forward = FALSE; break;
		case VK_DOWN:	click = TRUE; break;
		case VK_RIGHT:	cycle = TRUE; break;
	}

	if (cycle)
	{
	    CycleFocus(cycle_forward);
		return 0;
	}
	else if (click && m_focus)
	{
		m_focus->Click(TRUE);
		return 0;
	}

	OUIMenuBarItem* item = (OUIMenuBarItem*) m_items.First();

	while (item && item->m_accel != wparam)
	{
		item = (OUIMenuBarItem*) item->Suc();
	}

	if (!item && m_mdi_item && wparam == 0xbd /*VK_OEM_MINUS*/)
	{
		item = m_mdi_item;
	}

	if (item)
	{
		::PostMessage(GetHWnd(), WM_LBUTTONDOWN, MK_SHIFT, (LPARAM) item);
	}
	else if (wparam == ' ')
	{
		::SendMessage(GetParentHWnd(), WM_SYSCOMMAND, SC_KEYMENU, 0);
	}

	return 0;
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnChar(WPARAM wparam, LPARAM lparam)
{
	switch (wparam)
	{
		case VK_RETURN:
		{
			if (m_focus)
			{
				m_focus->Click(TRUE);
			}
			break;
		}
	}

	return 0;
}

/**********************************************************************************/

BOOL old_openstatus = FALSE;
BOOL old_openstatus_valid = FALSE;

LRESULT OUIMenuBar::OnSetFocus(WPARAM	wparam, LPARAM lparam)
{
	RedrawWindow();
	m_old_focus = (HWND) wparam;
	HIMC hIMC = ImmGetContext(m_HWnd);
	if (hIMC)
	{
		old_openstatus_valid = TRUE;
		old_openstatus = ImmGetOpenStatus(hIMC);
		ImmSetOpenStatus(hIMC, FALSE);
		ImmReleaseContext(m_HWnd, hIMC);
	}
	return 0;
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnKillFocus(WPARAM	wparam, LPARAM lparam)
{
	m_focus = NULL;
	m_old_focus = NULL;

	RedrawWindow();
	if (old_openstatus_valid)
	{
		HIMC hIMC = ImmGetContext(m_HWnd);
		if (hIMC)
		{
			ImmSetOpenStatus(hIMC, old_openstatus);
			ImmReleaseContext(m_HWnd, hIMC);
			old_openstatus_valid = FALSE;
		}
	}
	return 0;
}

/**********************************************************************************/

void OUIMenuBar::RestoreFocus()
{
	if (m_old_focus)
	{
		::SetFocus(m_old_focus);
		m_old_focus = NULL;
	}
}

/**********************************************************************************/

void OUIMenuBar::SetFocus()
{
	OUIMenuBarItem* item = (OUIMenuBarItem*) m_items.First();

	if (item && item == m_mdi_item)
	{
		item = (OUIMenuBarItem*) item->Suc();
	}

	if (item)
	{
		item->SetFocus();
	}
}

/**********************************************************************************/

void OUIMenuBar::CycleFocus(BOOL forward)
{
	forward ^= (UiDirection::Get() == UiDirection::RTL);

	if (this == m_menu_manager->GetActiveMenuBar())
	{
		OUIMenuBarItem* item = (OUIMenuBarItem*) m_items.First();

		while (item && item != m_menu_manager->GetActiveMenuBarItem())
		{
			item = (OUIMenuBarItem*) item->Suc();
		}

		if (item)
		{
			if (forward)
			{
				item = (OUIMenuBarItem*) item->Suc();
			}
			else
			{
				item = (OUIMenuBarItem*) item->Pred();
			}

			if (!item)
			{
				if (forward)
				{
					item = (OUIMenuBarItem*) m_items.First();
				}
				else
				{
					item = (OUIMenuBarItem*) m_items.Last();
				}
			}

			::PostMessage(GetHWnd(), WM_LBUTTONDOWN, MK_SHIFT, (LPARAM) item);

			if (m_focus)
			{
				OUIMenuBarItem* old_focus = m_focus;
				m_focus = item;
				old_focus->Redraw();
				m_focus->Redraw();
			}
		}
	}
	else
	{
		if (m_focus)
		{
			OUIMenuBarItem* old_focus = m_focus;

			if (forward)
			{
				m_focus = (OUIMenuBarItem*) m_focus->Suc();
			}
			else
			{
				m_focus = (OUIMenuBarItem*) m_focus->Pred();
			}

			old_focus->Redraw();
		}

		if (!m_focus)
		{
			if (forward)
			{
				m_focus = (OUIMenuBarItem*) m_items.First();
			}
			else
			{
				m_focus = (OUIMenuBarItem*) m_items.Last();
			}
		}

		if (m_focus)
		{
			m_focus->SetFocus();
		}
	}
}

/**********************************************************************************/

void OUIMenuBar::LayoutItems()
{
	RECT client_rect;
	GetClientRect(&client_rect);

	if (m_mdi_minimize)
	{
		int command = SW_HIDE;

		if (m_mdi_item)
		{
			command = SW_SHOWNA;

			int height = client_rect.bottom - client_rect.top;
			int mdi_width = GetSystemMetrics(SM_CXMENUSIZE);
			int mdi_height = GetSystemMetrics(SM_CYMENUSIZE);
			
			int x = client_rect.right - mdi_width;
			int y = client_rect.top + (height - mdi_height) / 2;

			::MoveWindow(m_mdi_close, x, y, mdi_width, mdi_height, TRUE);

			x -= mdi_width;

			::MoveWindow(m_mdi_restore, x, y, mdi_width, mdi_height, TRUE);

			x -= mdi_width;

			::MoveWindow(m_mdi_minimize, x, y, mdi_width, mdi_height, TRUE);
		}

		::ShowWindow(m_mdi_minimize, command);
		::ShowWindow(m_mdi_restore, command);
		::ShowWindow(m_mdi_close, command);
	}

	OUIMenuBarItem* item;

	OpButton* button;

	if (OpStatus::IsSuccess(OpButton::Construct(&button)))
	{
		VisualDevice vd;
		vd.painter = m_parent_opwindow->m_mde_screen->GetVegaPainter();

		button->SetVisualDevice(&vd);
		button->string.SetConvertAmpersand(TRUE);

		INT32 pos_x = 0;
		const int menu_bar_width = client_rect.right - client_rect.left;

		for (item = (OUIMenuBarItem*) m_items.First(); item; item = (OUIMenuBarItem*) item->Suc())
		{
			if(item->IsMDIItem())
			{
				button->SetButtonTypeAndStyle(OpButton::TYPE_MENU, OpButton::STYLE_IMAGE);
				button->GetForegroundSkin()->SetImage("Window Document Icon");
			}
			else
			{
				button->SetButtonTypeAndStyle(OpButton::TYPE_MENU, OpButton::STYLE_TEXT);
			}

			button->SetText(item->m_title);

			INT32 height;
			INT32 width;

			button->GetRequiredSize(width, height);

			INT32 left, right, top, bottom;

			SetButtonSkin(button);

			button->GetBorderSkin()->GetMargin(&left, &top, &right, &bottom);

			RECT rect;
			rect.left = pos_x + left;
			rect.top = top;
			rect.right = rect.left + width;
			rect.bottom = height;

			if (UiDirection::Get() == UiDirection::RTL)
			{
				int left = rect.left;
				rect.left = menu_bar_width - rect.right;
				rect.right = menu_bar_width - left;
			}

			item->SetPosition(&rect);

			pos_x += (width + right);
		}

		vd.painter = NULL;

		button->Delete();
	}

	InvalidateRect(m_HWnd, NULL, FALSE);
}

/**********************************************************************************/

LRESULT	OUIMenuBar::DefaultWindowProc
(
	UINT			in_Message,
	WPARAM			in_WParam,
	LPARAM			in_LParam
)
{
	if (in_Message == idMsgLostCursor)
	{
		SetHoverItem(NULL);
	}

	return OUIWindow::DefaultWindowProc(in_Message, in_WParam, in_LParam);
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnPaint
(
	WPARAM in_WParam,
	LPARAM in_LParam
)
{
	PAINTSTRUCT ps;
	RECT update_rect;
	RECT parent_rect;

	::GetClientRect(GetParentHWnd(), &parent_rect);

	::InvalidateRect(m_HWnd, &parent_rect, FALSE);

	// bail out quickly if no update region
	if (!GetUpdateRect(m_HWnd, &update_rect, 0))
	{
		return 0;
	}
	BeginPaint(m_HWnd, &ps);

	VisualDevice vd;
	OpBitmap* bmp;
	if (OpStatus::IsError(OpBitmap::Create(&bmp, ps.rcPaint.right-ps.rcPaint.left, ps.rcPaint.bottom-ps.rcPaint.top, FALSE, TRUE, 0, 0, TRUE)))
	{
		EndPaint(m_HWnd, &ps);
		return 0;
	}
	OpRect rect(0, 0, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
	vd.BeginPaint(bmp->GetPainter(), rect, rect);
	((VEGAOpPainter*)vd.painter)->SetVegaTranslation(-ps.rcPaint.left, -ps.rcPaint.top);

	OpRect clip_rect(&ps.rcPaint);
	OpRect skin_rect(&parent_rect);

	// check for transparent section
	BOOL has_transparent_skins = g_application->GetActiveBrowserDesktopWindow() ? g_application->GetActiveBrowserDesktopWindow()->HasTransparentSkins() : FALSE;

	// for compatibility, draw this skin first
	if (!has_transparent_skins)
		g_skin_manager->DrawElement(&vd, m_skin, skin_rect, 0, 0, &clip_rect);

	OpSkinElement *elm = g_skin_manager->GetCurrentSkin()->GetSkinElement(has_transparent_skins ? "Menu Transparent Skin" : "Menu Skin");
	if(has_transparent_skins && elm == NULL)
	{
		// we have no transparent section
		elm = g_skin_manager->GetCurrentSkin()->GetSkinElement("Menu Skin");
	}
	if(elm)
	{
		//  draw the background for the menu itself, but with a limited rect
		elm->Draw(&vd, clip_rect, 0);
	}
/*
#ifdef _DEBUG
	uni_char tmpbuf[128];

	uni_snprintf(tmpbuf, 128, UNI_L("update_rect: %d %d %d %d, clip_rect: %d %d %d %d\n"),
		update_rect.left, update_rect.top, update_rect.right - update_rect.left, update_rect.bottom - update_rect.top,
		clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height
		);

	OutputDebugString(tmpbuf);
#endif
*/
	OUIMenuBarItem* item;
	RECT dst = {0, 0, 0, 0};

	for (item = (OUIMenuBarItem*) m_items.First(); item; item = (OUIMenuBarItem*) item->Suc())
	{
		if (IntersectRect(&dst, &item->m_position, &ps.rcPaint))
		{
			item->Paint(&vd);
		}
	}
	vd.EndPaint();
	bmp->ReleasePainter();

	UINT32* opbmpdata = (UINT32*)bmp->GetPointer(OpBitmap::ACCESS_READONLY);
	if (opbmpdata)
	{
		BITMAPINFO bi;
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = bmp->Width();
		bi.bmiHeader.biHeight = -(int)bmp->Height();
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		bi.bmiHeader.biSizeImage = 0;
		bi.bmiHeader.biXPelsPerMeter = 0;
		bi.bmiHeader.biYPelsPerMeter = 0;
		bi.bmiHeader.biClrUsed = 0;
		bi.bmiHeader.biClrImportant = 0;
		HBITMAP img = CreateDIBitmap(ps.hdc, &bi.bmiHeader, CBM_INIT, opbmpdata, &bi, DIB_RGB_COLORS);

		HDC imgdc = CreateCompatibleDC(ps.hdc);
		HBITMAP oldbitmap = (HBITMAP)SelectObject(imgdc, img);
		BitBlt(ps.hdc, ps.rcPaint.left, ps.rcPaint.top, bmp->Width(), bmp->Height(), imgdc, 0, 0, SRCCOPY);
		SelectObject(imgdc, oldbitmap);
		DeleteDC(imgdc);

		DeleteObject(img);

		bmp->ReleasePointer(FALSE);
	}
	OP_DELETE(bmp);

	EndPaint(m_HWnd, &ps);

	return 0;
}

/**********************************************************************************/

OUIMenuBarItem*	OUIMenuBar::GetMenuBarItemFromPoint(int x, int y)
{
	POINT point = {x, y};

	OUIMenuBarItem* item;

	for (item = (OUIMenuBarItem*) m_items.First(); item; item = (OUIMenuBarItem*) item->Suc())
	{
		if (PtInRect(&item->m_position, point))
		{
			return item;
		}
	}

	return NULL;
}

/**********************************************************************************/

void OUIMenuBar::SetHoverItem(OUIMenuBarItem* item)
{
	if (item == m_hover_item)
		return;

	if (m_hover_item)
	{
		m_hover_item->ClearFlags(BUTTON_HIGHLIGHT);
	}

	m_hover_item = item;

	if (m_hover_item)
	{
		m_hover_item->SetFlags(BUTTON_HIGHLIGHT);
	}
}

/**********************************************************************************/

#define     SubclassWindow(hwnd, lpfn)       \
              ((WNDPROC)SetWindowLongPtr((hwnd), GWLP_WNDPROC, (LPARAM)(WNDPROC)(lpfn)))

void OUIMenuBar::UpdateMDIButtons(WindowsOpWindow* mdi_child)
{
	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseButtons) && mdi_child)
	{
		if (!m_mdi_item)
		{
			m_mdi_item = OP_NEW(OUIMenuBarItem, (this));
			if (m_mdi_item)
			{
				m_mdi_item->SetIsMDIItem(TRUE);
				m_mdi_item->IntoStart(&m_items);
			}
		}
		if (!m_mdi_minimize)
		{
			m_mdi_minimize			= ::CreateWindow(UNI_L("BUTTON"), NULL, WS_CHILD | WS_CLIPSIBLINGS | BS_OWNERDRAW, 0, 0, 0, 0, GetHWnd(), (HMENU) IDM_MENUBAR_MDI_MINIMIZE, s_Instance, NULL);
			m_mdi_restore			= ::CreateWindow(UNI_L("BUTTON"), NULL, WS_CHILD | WS_CLIPSIBLINGS | BS_OWNERDRAW, 0, 0, 0, 0, GetHWnd(), (HMENU) IDM_MENUBAR_MDI_RESTORE, s_Instance, NULL);
			m_mdi_close				= ::CreateWindow(UNI_L("BUTTON"), NULL, WS_CHILD | WS_CLIPSIBLINGS | BS_OWNERDRAW, 0, 0, 0, 0, GetHWnd(), (HMENU) IDM_MENUBAR_MDI_CLOSE, s_Instance, NULL);
			s_old_button_wndproc	= SubclassWindow(m_mdi_minimize, StaticSubclassedButtonWndProc);
			s_old_button_wndproc	= SubclassWindow(m_mdi_restore, StaticSubclassedButtonWndProc);
			s_old_button_wndproc	= SubclassWindow(m_mdi_close, StaticSubclassedButtonWndProc);
		}
		LayoutItems();
	}
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnMouseMove(WPARAM wparam, LPARAM lparam)
{
	int x = (short) LOWORD(lparam);
	int y = (short) HIWORD(lparam);

	OUIMenuBarItem* item = GetMenuBarItemFromPoint(x, y);

	if (m_menu_manager->GetActiveMenuBar() == this)
	{
		if (item && item != m_menu_manager->GetActiveMenuBarItem())
		{
			return TRUE;
		}
	}
	else
	{
		SetHoverItem(item);
	}
		
	return 0;
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnLButtonDblClk(WPARAM wparam, LPARAM lparam)
{
	int x = (short) LOWORD(lparam);
	int y = (short) HIWORD(lparam);

	OUIMenuBarItem* item = GetMenuBarItemFromPoint(x, y);

	if (m_mdi_item && item == m_mdi_item)
	{
		DesktopWindow* window = GetActiveDesktopWindow();
		if(window && window->IsClosableByUser())
		{
			window->Close(FALSE, TRUE);
		}
	}
	return 0;
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnLButtonDown(WPARAM wparam, LPARAM lparam)
{
	int x = (short) LOWORD(lparam);
	int y = (short) HIWORD(lparam);

	if (!(wparam & MK_SHIFT))
	{
		OnKillFocus(0,0);
	}

	OUIMenuBarItem* item;

	for (item = (OUIMenuBarItem*) m_items.First(); item; item = (OUIMenuBarItem*) item->Suc())
	{
		if (item == (OUIMenuBarItem*) lparam)
		{
			break;
		}
	}

	if (!item)
	{
		item = GetMenuBarItemFromPoint(x, y);
	}

	if(item && item != m_mdi_item)	// remove "item != m_mdi_item" when he MDI menu works
	{
		item->Click(wparam & MK_SHIFT ? TRUE : FALSE);
	}

	return 0;
}
/**********************************************************************************/
DesktopWindow* OUIMenuBar::GetActiveDesktopWindow()
{
	BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
	if(browser_window)
	{
		return browser_window->GetActiveDesktopWindow();
	}
	return NULL;
}

BOOL OUIMenuBar::GetMenubarInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out)
{
	if (OpStatus::IsError(out.GetMenuIdRef().SetMenuName(UNI_L(MENU_NAME_MAIN_MENU))))
		return FALSE;

	OUIMenuBarItem* menubar_item = 0;
	int row = 0;

	for (menubar_item = (OUIMenuBarItem*) m_items.First(); menubar_item; menubar_item = (OUIMenuBarItem*) menubar_item->Suc())
	{
		OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuItemInfo> item(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuItemInfo, ()));
		if (item.get() == NULL)
			return FALSE;

		RETURN_IF_ERROR(item.get()->SetText(menubar_item->m_title));
		item.get()->SetEnabled(TRUE);
		item.get()->SetChecked(FALSE);
		item.get()->SetBold(FALSE);

		OpString shortcut;
		shortcut.Set(&menubar_item->m_accel, 1);
		RETURN_IF_ERROR(item.get()->SetShortcutLetter(shortcut));
		item.get()->SetRow(row);

		OpString submenu;
		submenu.Set(menubar_item->m_sub_menu_name);
		RETURN_IF_ERROR(item.get()->SetSubmenu(submenu));
		
		RECT rect = menubar_item->m_position;
		ClientToScreenRect(GetHWnd(), &rect);
		item.get()->GetRectRef().SetX(rect.left);
		item.get()->GetRectRef().SetY(rect.top);
		item.get()->GetRectRef().SetWidth(rect.right - rect.left);
		item.get()->GetRectRef().SetHeight(rect.bottom - rect.top);

		//Add to info and release
		if (OpStatus::IsSuccess(out.GetMenuItemListRef().Add(item.get())))
			item.release();

		row++;
	}

	return TRUE;
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnCommand(WPARAM wparam, LPARAM lparam)
{
	int id = LOWORD(wparam);
	int notification = HIWORD(wparam);

	if (m_mdi_item && notification == BN_CLICKED)
	{
		if (m_old_focus)
		{
			RestoreFocus();
		}

		switch (id)
		{
			case IDM_MENUBAR_MDI_MINIMIZE:
				{
					DesktopWindow* window = GetActiveDesktopWindow();
					if(window)
					{
						window->Minimize();
					}
				}
				break;

			case IDM_MENUBAR_MDI_RESTORE:
				{
					DesktopWindow* window = GetActiveDesktopWindow();
					if(window)
					{
						window->Restore();
					}
				}
				break;

			case IDM_MENUBAR_MDI_CLOSE:
				{
					DesktopWindow* window = GetActiveDesktopWindow();
					if(window && window->IsClosableByUser())
					{
						window->Close(FALSE, TRUE);
					}
				}
				break;
		}
	}
	return 0;
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnDrawItem(WPARAM wparam, LPARAM lparam)
{
	LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT) lparam;

	if (di && (di->itemAction == ODA_DRAWENTIRE || di->itemAction == ODA_SELECT))
	{
		const char* full = "Caption Minimize Button Skin";

		switch (di->CtlID)
		{
			case IDM_MENUBAR_MDI_RESTORE:
				full = "Caption Restore Button Skin";
				break;

			case IDM_MENUBAR_MDI_CLOSE:
				full = "Caption Close Button Skin";
				break;
		}

		OpRect skin_rect(&di->rcItem);

		VisualDevice vd;
		OpBitmap* bmp;
		if (OpStatus::IsError(OpBitmap::Create(&bmp, skin_rect.width, skin_rect.height, FALSE, TRUE, 0, 0, TRUE)))
		{
			return 0;
		}
		vd.painter = bmp->GetPainter();
		((VEGAOpPainter*)vd.painter)->SetVegaTranslation(-skin_rect.x, -skin_rect.y);
		vd.ClearRect(skin_rect);

		g_skin_manager->DrawElement(&vd, full, skin_rect, di->itemState & ODS_SELECTED ? SKINSTATE_PRESSED : di->itemState & ODS_HOTLIGHT ? SKINSTATE_HOVER : 0);

		vd.painter = NULL;
		bmp->ReleasePainter();

		HBITMAP bm = CreateHBITMAPFromOpBitmap(bmp);
		if(bm)
		{
			HDC screen_dc = GetDC(NULL);
			HDC imgdc = CreateCompatibleDC(screen_dc);

			HBITMAP old_bm = (HBITMAP) SelectObject(imgdc, bm);

			AlphaBlt(di->hDC, skin_rect.x, skin_rect.y, skin_rect.width, skin_rect.height, imgdc, 0, 0, skin_rect.width, skin_rect.height);

			SelectObject(imgdc, old_bm);

			ReleaseDC(NULL, screen_dc);
			DeleteDC(imgdc);

			DeleteObject(bm);
		}
		OP_DELETE(bmp);
	}
	return 0;
}

/**********************************************************************************/

LRESULT OUIMenuBar::OnCtlColorBtn(WPARAM in_WParam, LPARAM in_LParam)
{
	return (LRESULT) GetStockObject(NULL_BRUSH);
}

/**********************************************************************************/

WNDPROC	OUIMenuBar::s_old_button_wndproc = NULL;

LRESULT CALLBACK OUIMenuBar::StaticSubclassedButtonWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	OUIMenuBar* menu_bar = (OUIMenuBar*) OUIWindow::StaticGetWindowObject(::GetParent(hwnd));

	if (menu_bar && message == WM_SETFOCUS)
	{
		HWND lost_focus = (HWND) wparam;

		if (lost_focus && lost_focus != hwnd)
		{
			menu_bar->m_old_focus = lost_focus;
		}
	}

	return CallWindowProc(s_old_button_wndproc, hwnd, message, wparam, lparam);	
}

/**********************************************************************************/

void OUIMenuBar::SetButtonSkin(OpButton *button)
{
	// Add a background color for the menu button when Aero is on to make text more readable
	BOOL has_transparent_skins = g_application->GetActiveBrowserDesktopWindow() ? g_application->GetActiveBrowserDesktopWindow()->HasTransparentSkins() : FALSE;
	
	BOOL composition = FALSE;
	OPDwmIsCompositionEnabled(&composition);

	button->GetBorderSkin()->SetImage(has_transparent_skins && composition ? "Menu Transparent Button Skin" : "Menu Button Skin", "Menu Button Skin");
}

/***********************************************************************************/
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OpWindow* OUIMenuBar::AccessibilityGetWindow() const
{
	return m_parent_opwindow;
}

int	OUIMenuBar::GetAccessibleChildrenCount()
{
	int count = 0;
	OUIMenuBarItem* item = (OUIMenuBarItem*)m_items.First();

	while (item)
	{
		count++;
		item=(OUIMenuBarItem*)item->Suc();
	}

	return count;
}

OP_STATUS OUIMenuBar::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	RECT pos;
	if (!GetWindowRect(GetHWnd(), &pos))
	{
		rect = OpRect(0,0,0,0);
		return OpStatus::ERR;
	}

	rect= OpRect(pos.left, pos.top, pos.right-pos.top, pos.bottom-pos.top);
	return OpStatus::OK;

}

OP_STATUS OUIMenuBar::AccessibilityGetText(OpString& str)
{
	return OpStatus::ERR;
}

OP_STATUS OUIMenuBar::AccessibilityGetDescription(OpString& str)
{
	str.Set("Application menu bar");
	return OpStatus::OK;
}

OP_STATUS OUIMenuBar::AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut)
{
	*shifts = SHIFTKEY_ALT;
	*kbdShortcut = 0;

	return OpStatus::OK;
}


Accessibility::ElementKind OUIMenuBar::AccessibilityGetRole() const
{
	return Accessibility::kElementKindMenuBar;
}

Accessibility::State OUIMenuBar::AccessibilityGetState()
{
	return Accessibility::kAccessibilityStateNone;
}

OpAccessibleItem* OUIMenuBar::GetAccessibleChild(int child_nr)
{
	int count = 0;
	OUIMenuBarItem* item = (OUIMenuBarItem*)m_items.First();

	while (item && count < child_nr)
	{
		count++;
		item=(OUIMenuBarItem*)item->Suc();
	}

	return item;
}

int OUIMenuBar::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int count = 0;
	OUIMenuBarItem* item = (OUIMenuBarItem*)m_items.First();

	while (item)
	{
		if (item == child)
			return count;
		count++;
		item=(OUIMenuBarItem*)item->Suc();
	}

	return Accessibility::NoSuchChild;
}

OpAccessibleItem* OUIMenuBar::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect position;
	AccessibilityGetAbsolutePosition(position);
	OUIMenuBarItem* item= GetMenuBarItemFromPoint(x-position.x, y-position.y);

	if (item)
		return item;
	else if (position.Contains(OpPoint(x,y)))
		return this;
	else
		return NULL;
}

OpAccessibleItem* OUIMenuBar::GetNextAccessibleSibling()
{
	return this->m_parent_opwindow->m_accessible_item->GetAccessibleChild(1);
}

OpAccessibleItem* OUIMenuBar::GetPreviousAccessibleSibling()
{
	return NULL;

}

OpAccessibleItem* OUIMenuBar::GetAccessibleFocusedChildOrSelf()
{
	if (m_focus)
		return m_focus;
	else
		return NULL;
}

OpAccessibleItem* OUIMenuBar::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* OUIMenuBar::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* OUIMenuBar::GetDownAccessibleObject()
{
	return GetNextAccessibleSibling();
}

OpAccessibleItem* OUIMenuBar::GetUpAccessibleObject()
{
	return NULL;
}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**	OUIMenuBarItem
**
***********************************************************************************/

OUIMenuBarItem::OUIMenuBarItem(OUIMenuBar *menu_bar, uni_char* title, const uni_char* sub_menu_name, INTPTR sub_menu_value, uni_char accel)
{
	InternalInit();

	m_menu_bar	= menu_bar;
	m_accel		= uni_toupper(accel);
	m_sub_menu_value = sub_menu_value;

	m_title.Set(title);
	m_sub_menu_name.Set(sub_menu_name);
}

/**********************************************************************************/

OUIMenuBarItem::OUIMenuBarItem(OUIMenuBar *menu_bar)
{
	InternalInit();

	m_menu_bar	= menu_bar;
}

void OUIMenuBarItem::InternalInit()
{
	m_menu_bar	= NULL;
	m_flags		= BUTTON_FLAT;
	m_accel		= 0;
	m_sub_menu_value = -1;
	m_button = NULL;
	m_is_mdi_item = FALSE;
}

/**********************************************************************************/

OUIMenuBarItem::~OUIMenuBarItem()
{
	if (this == m_menu_bar->m_mdi_item)
	{
		m_menu_bar->m_mdi_item = NULL;
	}
	if (this == m_menu_bar->m_focus)
	{
		m_menu_bar->m_focus = NULL;
	}
	if (this == m_menu_bar->m_hover_item)
	{
		m_menu_bar->m_hover_item = NULL;
	}
	if(m_button)
	{
		m_button->Delete();
	}
}

/**********************************************************************************/

void OUIMenuBarItem::Paint(VisualDevice* vd)
{
	if(!m_button)
	{
		RETURN_VOID_IF_ERROR(OpButton::Construct(&m_button));

		if (IsMDIItem())
		{
			m_button->SetButtonTypeAndStyle(OpButton::TYPE_MENU, OpButton::STYLE_IMAGE);
			m_button->GetForegroundSkin()->SetImage("Window Document Icon");
		}
		else
		{
			m_button->SetButtonTypeAndStyle(OpButton::TYPE_MENU, OpButton::STYLE_TEXT);
		}
		m_button->SetText(m_title);
		m_button->SetRTL(UiDirection::Get() == UiDirection::RTL);
	}
	
	// skin may change
	if (!IsMDIItem())
		m_menu_bar->SetButtonSkin(m_button);

	int flags = m_flags;
	// int bitmap_id = 0;
	// uni_char* text = m_title.CStr();

	if (m_menu_bar->m_alt_key_down || m_menu_bar->m_focus)
	{
		flags |= BUTTON_TEXT_PREFIX;
	}

	if (this == m_menu_bar->m_focus)
	{
		flags |= BUTTON_FOCUS;
	}

	OpRect rect(&m_position);
	m_button->SetRect(rect);
	m_button->string.SetConvertAmpersand(TRUE);

	BOOL always_show_underline = TRUE;

	SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &always_show_underline, 0);

	m_button->string.SetDrawAmpersandUnderline(flags & BUTTON_TEXT_PREFIX || always_show_underline ? TRUE : FALSE);

	m_button->packed2.is_pushed = m_button->packed2.is_down = flags & (BUTTON_DOWN|BUTTON_FOCUS)  ? TRUE : FALSE;
	m_button->packed2.is_outside =  flags & BUTTON_HIGHLIGHT ? FALSE : TRUE;

	m_button->Update();

	m_button->SetVisualDevice(vd);

	vd->Translate(rect.x, rect.y);
	m_button->GenerateOnPaint(m_button->GetBounds());
	vd->Translate(-rect.x, -rect.y);

	m_button->SetVisualDevice(NULL);
}

void OUIMenuBarItem::SetFlags(int set, int clear)
{
	int new_flags = m_flags | set;
	new_flags &= ~clear;

	if (m_flags !=  new_flags)
	{
		m_flags = new_flags;
		Redraw();
	}
}

void OUIMenuBarItem::Redraw()
{
	if (IsVisible())
	{
		InvalidateRect(m_menu_bar->GetHWnd(), &m_position, FALSE);
		::UpdateWindow(m_menu_bar->GetHWnd());
	}
}

void OUIMenuBarItem::SetFocus()
{
	if (IsVisible())
	{
		OUIMenuBarItem* old_focus = m_menu_bar->m_focus;
		m_menu_bar->m_focus = this;
		::SetFocus(m_menu_bar->GetHWnd());
		Redraw();
		
		if (old_focus)
		{
			old_focus->Redraw();
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
			AccessibilityManager::GetInstance()->SendEvent(old_focus, Accessibility::Event(Accessibility::kAccessibilityStateFocused));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
		}
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT


	}
}

void OUIMenuBarItem::Click(BOOL keyboard_activated)
{
	if (m_menu_bar->m_focus)
	{
		SetFocus();
	}

	SetFlags(BUTTON_DOWN);
	
	RECT rect = m_position;

	ClientToScreenRect(m_menu_bar->GetHWnd(), &rect);

	m_menu_bar->m_menu_manager->ShowMenuBarItemRect(m_menu_bar, this, &rect, m_menu_bar->GetParentHWnd(), keyboard_activated, FALSE);

	ClearFlags(BUTTON_DOWN);
}

/**********************************************************************************/

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OP_STATUS OUIMenuBarItem::AccessibilityClicked()
{
	Click(FALSE);
	return OpStatus::OK;
}

BOOL OUIMenuBarItem::AccessibilitySetFocus() 
{
	SetFocus();
	return OpStatus::OK;
}

int	OUIMenuBarItem::GetAccessibleChildrenCount()
{
	return 0;
}

OP_STATUS OUIMenuBarItem::AccessibilityGetAbsolutePosition(OpRect &rect)
{

	RECT pos;
	if (!GetWindowRect(m_menu_bar->GetHWnd(), &pos))
	{
		rect = OpRect(0,0,0,0);
		return OpStatus::ERR;
	}

	rect= OpRect(m_position.left+pos.left, m_position.top+pos.top, m_position.right-m_position.left, m_position.bottom-m_position.top);
	return OpStatus::OK;
}

OP_STATUS OUIMenuBarItem::AccessibilityGetText(OpString& str)
{
	str.Set(m_title);
	int amp = str.Find("&");
	if (amp != KNotFound)
		str.Delete(amp,1);
	return OpStatus::OK;
}

OP_STATUS OUIMenuBarItem::AccessibilityGetDescription(OpString& str)
{
	RETURN_IF_ERROR(AccessibilityGetText(str));
	str.Append(UNI_L(" menu"));
	return OpStatus::OK;
}

OP_STATUS OUIMenuBarItem::AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut)
{
	return OpStatus::ERR;
}

Accessibility::ElementKind OUIMenuBarItem::AccessibilityGetRole() const
{
	return Accessibility::kElementKindMenuItem;
}

Accessibility::State OUIMenuBarItem::AccessibilityGetState()
{
	if (m_menu_bar->m_focus == this)
		return Accessibility::kAccessibilityStateFocused;
	else
		return Accessibility::kAccessibilityStateNone;
}

OpAccessibleItem* OUIMenuBarItem::GetAccessibleChild(int child_nr)
{
	return NULL;
}

int OUIMenuBarItem::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	return Accessibility::NoSuchChild;
}

OpAccessibleItem* OUIMenuBarItem::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect position;
	AccessibilityGetAbsolutePosition(position);

	if (position.Contains(OpPoint(x,y)))
		return this;
	else
		return NULL;
}

OpAccessibleItem* OUIMenuBarItem::GetNextAccessibleSibling()
{
	return (OUIMenuBarItem*)Suc();
}

OpAccessibleItem* OUIMenuBarItem::GetPreviousAccessibleSibling()
{
	return (OUIMenuBarItem*)Pred();
}

OpAccessibleItem* OUIMenuBarItem::GetAccessibleFocusedChildOrSelf()
{
	if (m_menu_bar->m_focus == this)
		return this;
	else
		return NULL;
}


OpAccessibleItem* OUIMenuBarItem::GetLeftAccessibleObject()
{
	return (OUIMenuBarItem*)Pred();
}

OpAccessibleItem* OUIMenuBarItem::GetRightAccessibleObject()
{
	return (OUIMenuBarItem*)Suc();
}

OpAccessibleItem* OUIMenuBarItem::GetDownAccessibleObject()
{
	return m_menu_bar->GetNextAccessibleSibling();
}

OpAccessibleItem* OUIMenuBarItem::GetUpAccessibleObject()
{
	return NULL;
}


#endif //ACCESSIBILITY_EXTENSION_SUPPORT

/*wParam A handle to the drop-down menu or submenu. 
		lParam The low-order word specifies the zero-based relative position of the menu item that opens the drop-down menu or submenu. 
		The high-order word indicates whether the drop-down menu is the window menu. If the menu is the window menu, this parameter is TRUE; otherwise, it is FALSE.*/
void OUIMenuManager::OnMenuShown(WPARAM wParam, INT32 pos)
{
	OP_NEW_DBG("OUIMenuManager::OnMenuShown()", "desktop-scope");

	HMENU submenu = (HMENU) wParam;
	//Note, when menu A opens Menu B opens Menu C, and A and B are already opened, m_active_menu is Menu A
	if (submenu)    
	{
		MenuItemData* data = 0;
		UINT32 i = 0;
		for (i = 0; i < m_menu_stack.GetCount(); i++)
		{
			if (m_menu_stack.Get(i)->Contains(submenu))
			{
				data = GetMenuItemData(m_menu_stack.Get(i)->m_parent, pos, TRUE);
				
				break;
			}
		}

		if (data)
		{
			OP_DBG(("Pop-up submenu = %p", submenu));
			OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
			if (OpStatus::IsSuccess(SetQuickMenuInfo(submenu, m_active_menu_owner, data->m_sub_menu_name, info)))
			{
				g_application->GetMenuHandler()->OnMenuShown(TRUE, info);
			}
		}
		else
		{
			if (!m_active_menu_bar)
			{
				m_inpage_popup_menu = submenu;
				m_inpage_popup_menu_name.Set(m_popup_root_name);
				OP_DBG(("In-page pop-up menu = %p", submenu));
			}
			else
			{
				OP_DBG(("Submenu from main menu bar submenu = %p", submenu));
			}

			OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
			if (OpStatus::IsSuccess(SetQuickMenuInfo(submenu, m_active_menu_owner, m_popup_root_name, info)))
				g_application->GetMenuHandler()->OnMenuShown(TRUE, info);
		}
	}
}

void OUIMenuManager::OnMenuClosed(WPARAM wParam)
{
	OP_NEW_DBG("OUIMenuManager::OnMenuClosed()", "desktop-scope");
	OP_DBG(("m_active_menu = %p", m_active_menu));

	if (m_inpage_popup_menu == m_active_menu)
	{
		OP_DBG(("Closing in-page pop-up menu"));
		m_inpage_popup_menu = NULL;
		m_inpage_popup_menu_name.Empty();
	}

	OpScopeDesktopWindowManager_SI::QuickMenuInfo info;
	if (OpStatus::IsSuccess(SetQuickMenuInfo(m_active_menu, m_active_menu_owner, m_popup_root_name, info))) // NB: TODO: FIX: Cannot work with submenus
		g_application->GetMenuHandler()->OnMenuShown(FALSE, info);
}

OP_STATUS OUIMenuManager::SetQuickMenuInfo(HMENU menu, HWND menu_owner, const char* menu_name, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info) 
{
	if (menu)
	{
		OpString name;
		name.Set(menu_name);
		RETURN_IF_ERROR(info.GetMenuIdRef().SetMenuName(name.CStr()));

		for (INT32 i = 0; i < GetMenuItemCount(menu); i++)
		{
			MenuItemData* data = GetMenuItemData(menu, i, TRUE); 
			if (data)
			{
				if (!data->m_separator)
				{
					OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuItemInfo> item(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuItemInfo, ()));
					if (item.get() == NULL)
						return OpStatus::ERR;

					if (data->m_input_action) 
					{
						const char* action = OpInputAction::GetStringFromAction(data->m_input_action->GetAction());
						if (action)
						{
							OpString str;
							str.Set(action);
							item.get()->SetAction(str);

							//Param, if any
							const uni_char* param = data->m_input_action->GetActionDataString();
							if (param && *param) 
							{
								RETURN_IF_ERROR(item.get()->SetActionParam(param));
							}
						}
					}

					MENUITEMINFO item_info;
					item_info.cbSize = sizeof(MENUITEMINFO);
					item_info.fMask = MIIM_STATE;
					item_info.dwItemData = NULL;

					GetMenuItemInfo(menu, i, TRUE, &item_info);

					RETURN_IF_ERROR(item.get()->SetText(data->m_item_name));
					item.get()->SetEnabled(~item_info.fState & MFS_DISABLED);  
					item.get()->SetChecked(item_info.fState & MFS_CHECKED); 
					item.get()->SetBold(data->m_bold);

					// Set shortcutletter
					OpString parsed_name;
					int shortcut_pos = -1;
					RETURN_IF_ERROR(ParseMenuItemName(data->m_item_name, parsed_name, shortcut_pos));
					OpString shortcut;
					if (shortcut_pos != -1)
						RETURN_IF_ERROR(shortcut.Set(parsed_name.CStr() + shortcut_pos, 1));
					shortcut.MakeLower();
					RETURN_IF_ERROR(item.get()->SetShortcutLetter(shortcut));
					RETURN_IF_ERROR(item.get()->SetShortcut(data->m_shortcut));

					item.get()->SetRow(i);
					if (data->m_sub_menu_name) 
					{
						OpString str;
						str.Set(data->m_sub_menu_name);
						RETURN_IF_ERROR(item.get()->SetSubmenu(str.CStr()));
					}

					RECT r;
					LPRECT rect = &r;
					GetMenuItemRect(menu_owner, menu , i, rect);
					item.get()->GetRectRef().SetX(rect->left);
					item.get()->GetRectRef().SetY(rect->top);
					item.get()->GetRectRef().SetWidth(rect->right - rect->left);
					item.get()->GetRectRef().SetHeight(rect->bottom - rect->top);

					RETURN_IF_ERROR(info.GetMenuItemListRef().Add(item.get()));
					item.release(); 
				}
				else // Separator
				{
					OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickMenuItemInfo> item(OP_NEW(OpScopeDesktopWindowManager_SI::QuickMenuItemInfo, ()));
					if (item.get() == NULL)
						return OpStatus::ERR;

					item.get()->SetSeparator(TRUE);
					RETURN_IF_ERROR(item.get()->SetText(data->m_item_name));
					item.get()->SetEnabled(TRUE);  
					item.get()->SetChecked(FALSE); 
					item.get()->SetBold(FALSE);

					// Set shortcutletter
					OpString shortcut;
					RETURN_IF_ERROR(item.get()->SetShortcutLetter(shortcut));

					item.get()->SetRow(i);
					RECT r;
					LPRECT rect = &r;
					GetMenuItemRect(menu_owner, menu , i, rect);
					item.get()->GetRectRef().SetX(rect->left);
					item.get()->GetRectRef().SetY(rect->top);
					item.get()->GetRectRef().SetWidth(rect->right - rect->left);
					item.get()->GetRectRef().SetHeight(rect->bottom - rect->top);

					RETURN_IF_ERROR(info.GetMenuItemListRef().Add(item.get()));
					item.release();
				}
			}
		}
	}

	return OpStatus::OK;
}

void OUIMenuManager::DisableMenuAnimationParameter()
{
#ifdef VEGA_BACKEND_OPENGL
	OP_ASSERT(!m_menu_parameters_changed);
	if (GetWinType() <= WINXP &&
		g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D &&
		g_vegaGlobals.vega3dDevice->getDeviceType() == PrefsCollectionCore::OpenGL)
	{
		m_menu_parameters_changed = TRUE;
		SystemParametersInfo(SPI_GETMENUANIMATION, 0, &m_old_menu_animation, 0);
		SystemParametersInfo(SPI_GETSELECTIONFADE, 0, &m_old_menu_selection_fade, 0);
		SystemParametersInfo(SPI_SETMENUANIMATION, 0, FALSE, 0);
		SystemParametersInfo(SPI_SETSELECTIONFADE, 0, FALSE, 0);
	}
#endif // VEGA_BACKEND_OPENGL
}

void OUIMenuManager::RestoreMenuAnimationParameter()
{
	if (m_menu_parameters_changed)
	{
		m_menu_parameters_changed = FALSE;
		SystemParametersInfo(SPI_SETMENUANIMATION, 0, (PVOID)m_old_menu_animation, 0);
		SystemParametersInfo(SPI_SETSELECTIONFADE, 0, (PVOID)m_old_menu_selection_fade, 0);
	}
}
