/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/quick_support/MacApplicationListener.h"
#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"
#include "platforms/mac/quick_support/MacHostQuickMenu.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/url/loadhandler/url_lh.h"
#include "platforms/mac/model/MacServices.h"
#include "platforms/mac/model/DockManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_util/settings/DesktopSettings.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#include "platforms/mac/quick_support/MacGadgetApplicationListener.h"
#include "adjunct/widgetruntime/GadgetStartup.h"
#endif

extern Boolean gIsRebuildingMenu;
extern long gVendorDataID;

extern EmBrowserAppNotification *gApplicationProcs;

CocoaInternalQuickMenu* gDockMenu = NULL;
bool gHandlingContextMenu = false;


OP_STATUS StripAmpersand(OpString& s)
{
//	"Ne&w..."
//  "New... (&w)"	// F.ex japanese
//  "Reload\tF5"	// Windows keyboard shortcut

	const uni_char* cstr = s.CStr();
	uni_char* buf = SetNewStr(cstr);

	if (cstr == NULL || buf == NULL)
		return OpStatus::ERR;

	uni_char* jps = uni_strstr(buf, UNI_L("(&"));

	if (jps && jps[3] == ')')
	{
		memmove((void *)jps, (void *)(jps + 4), sizeof(uni_char) * (uni_strlen(jps + 4) + 1));
	}

	uni_char* t = buf;
	uni_char* a = buf;
	int len = uni_strlen(buf);
	while (len-- >= 0)
	{
		if ((*a == '&') && (*(a+1) != '&'))
			a++;
		else
			*t++ = *a++;
	}
	uni_char* tab = uni_strchr(buf, '\t');
	if (tab)
		*tab = 0;	// Clear everything after TAB

	OP_STATUS result = s.Set(buf);
	delete [] buf;

	return result;
}

OP_STATUS DesktopGlobalApplication::Create(DesktopGlobalApplication** desktop_application)
{
#ifdef WIDGET_RUNTIME_SUPPORT
    if (g_application && ((GadgetStartup::IsGadgetStartupRequested())))
    {
		*desktop_application = OP_NEW(MacGadgetApplicationListener, ());
    }
    else
#endif // WIDGET_RUNTIME_SUPPORT ;-)	
	*desktop_application = OP_NEW(MacApplicationListener, ());
	if(!*desktop_application)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS DesktopPopupMenuHandler::Create(DesktopPopupMenuHandler** menu_handler)
{
	*menu_handler = OP_NEW(MacApplicationMenuListener, ());
	if(!*menu_handler)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

void MacApplicationListener::OnStart()
{
	// Part of fix for bug 210821, Opera window doesn't activate completely
	if(KioskManager::GetInstance()->GetEnabled())
	{
		ProcessSerialNumber	psnOpera = {0, kCurrentProcess};
		::SetFrontProcess(&psnOpera);
	}

#ifdef SUPPORT_OSX_SERVICES
	MacOpServices::InstallServicesHandler();
#endif

	if (gVendorDataID == 'OPRA')
	{
		gMenuBar = new MacHostQuickMenuBar();
		if (gMenuBar)
		{
			gIsRebuildingMenu = true;
			if (!g_input_manager->GetKeyboardInputContext())
				g_input_manager->SetKeyboardInputContext(g_application->GetInputContext(), FOCUS_REASON_OTHER);
			g_application->GetMenuHandler()->AddMenuItems(gMenuBar, "Browser Menu Bar", 0, 0, g_input_manager->GetKeyboardInputContext());
			LocaliseOperaMenu();
			gDockMenu = new CocoaInternalQuickMenu(FALSE, "Dock Menu", 0, 0);
			g_application->GetMenuHandler()->AddMenuItems(gDockMenu, "Dock Menu", 1, 0, g_input_manager->GetKeyboardInputContext());
			gIsRebuildingMenu = false;
			g_dock_manager->Init();
		}
	}
}

#ifndef NO_CARBON

void MacApplicationListener::OnStarted()
{
	// Ref. DSK-293741
	if (KioskManager::GetInstance()->GetEnabled())
	{
		SystemUIOptions options = 0;
		if (KioskManager::GetInstance()->GetNoExit()) {
			options |= kUIOptionDisableAppleMenu | kUIOptionDisableProcessSwitch | kUIOptionDisableForceQuit | kUIOptionDisableSessionTerminate;
		}
		::SetSystemUIMode(kUIModeAllHidden, options);
	}
}

#endif // !NO_CARBON

void MacApplicationListener::Exit()
{
	delete gDockMenu;
	gDockMenu = NULL;
	if (gApplicationProcs && gApplicationProcs->notification)
	{
		gApplicationProcs->notification(NULL, emBrowser_app_msg_request_quit, 0);
	}
}

void MacApplicationListener::ExitStarted()
{

}

void MacApplicationListener::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_MENU_SETUP))
	{
		if (gVendorDataID == 'OPRA')
		{
			if (gMenuBar)
			{
				gMenuBar->RemoveAllMenus();
				gIsRebuildingMenu = true;
				if (!g_input_manager->GetKeyboardInputContext())
					g_input_manager->SetKeyboardInputContext(g_application->GetInputContext(), FOCUS_REASON_OTHER);
				g_application->GetMenuHandler()->AddMenuItems(gMenuBar, "Browser Menu Bar", 0, 0, g_input_manager->GetKeyboardInputContext());
				LocaliseOperaMenu();
				gIsRebuildingMenu = false;
			}
		}
	}
}

void MacApplicationMenuListener::OnPopupMenu(const char* popup_name, const PopupPlacement& placement, BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context)
{
	if (!gHandlingContextMenu) {
		gHandlingContextMenu = true;
		CocoaInternalQuickMenu* menu = new CocoaInternalQuickMenu(use_keyboard_context, popup_name, popup_value, 0);
		g_application->GetMenuHandler()->AddMenuItems(menu, popup_name, popup_value, 0, use_keyboard_context ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext());
		menu->Show(placement);
		gHandlingContextMenu = false;
	}
}

BOOL MacApplicationMenuListener::OnAddMenuItem(void* menu, BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, BOOL checked, BOOL selected, BOOL disabled, BOOL bold, OpInputAction* input_action)
{
	if (menu)
	{
		if(!item_name && !seperator)
		{
			return FALSE;
		}

		OpString str;
		MacQuickMenu *submenu = 0;
		str.Set(item_name);
		OpStatus::Ignore(StripAmpersand(str));
		if (sub_menu_name && *sub_menu_name)
		{
			Boolean keyboard = true;
			if (((MacQuickMenu*)menu)->IsInternal()) {
				keyboard = ((CocoaInternalQuickMenu*)menu)->IsKeyboardContext();
				submenu = new CocoaInternalQuickMenu(keyboard, sub_menu_name, sub_menu_value, sub_menu_start);
			}
			else
			{
				submenu = new MacHostQuickMenu(str, sub_menu_name, sub_menu_value);
			}
			if (submenu)
			{
				/**
				* If we're starting up then generate all menus since we need them for cmdkey-matching.
				*/
				if(gIsRebuildingMenu && op_strcmp(sub_menu_name, "Browser Bookmarks Menu"))
				{
					g_application->GetMenuHandler()->AddMenuItems(submenu, sub_menu_name, sub_menu_value, sub_menu_start, keyboard ? g_input_manager->GetKeyboardInputContext() : g_input_manager->GetMouseInputContext());
				}
			}
		}
		if (GetOSVersion() < 0x1070 && input_action && input_action->GetAction()==OpInputAction::ACTION_TOGGLE_SYSTEM_FULLSCREEN)
			return FALSE;

		((MacQuickMenu*)menu)->AddItem(seperator, str, image, shortcut, submenu, checked, selected, disabled, input_action);
	}
	return FALSE;
}


