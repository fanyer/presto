/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/application/BrowserMenuHandler.h"

#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/quick/ClassicApplication.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/spellcheck/SpellCheckContext.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/scope/src/scope_manager.h"


BrowserMenuHandler::MenuSection::MenuSection(const char* name,
		PrefsSection* section)
	: m_section(section)
{
	Set(name);
}

BrowserMenuHandler::MenuSection::~MenuSection()
{
	OP_DELETE(m_section);
}

PrefsSection* BrowserMenuHandler::MenuSection::GetSection()
{
	return m_section;
}



BrowserMenuHandler::BrowserMenuHandler(ClassicApplication& application,
		ClassicGlobalUiContext& global_context,
		SpellCheckContext& spell_check_context)
	: m_application(&application)
	, m_global_context(&global_context)
	, m_spell_check_context(&spell_check_context)
	, m_showing_menu(FALSE)
#ifdef EMBROWSER_SUPPORT
	, m_menu_blocked(FALSE)
#endif
{
}


PrefsSection* BrowserMenuHandler::GetMenuSection(const char* menu_name)
{
	MenuSection* menu_section = NULL;
	m_menu_sections.GetData(menu_name, &menu_section);

	if (!menu_section)
	{
		PrefsSection *section = QuickMenu::ReadMenuSection(menu_name);
		menu_section = OP_NEW(MenuSection, (menu_name, section));
		if (!menu_section)
			return NULL;
		if (menu_section->IsEmpty())
		{
			OP_DELETE(menu_section);
			return NULL;
		}

		m_menu_sections.Add(menu_section->CStr(), menu_section);
	}

	return menu_section->GetSection();
}


void BrowserMenuHandler::ShowPopupMenu(const char* popup_name, const PopupPlacement& placement, INTPTR popup_value,
	BOOL use_keyboard_context, DesktopMenuContext* context)
{
	if (KioskManager::GetInstance()->GetNoContextMenu() || !g_pcui->GetIntegerPref(PrefsCollectionUI::AllowContextMenus))
	{
		if (OpStringC8(popup_name) != "Bookmark Folder Menu" || KioskManager::GetInstance()->GetNoHotlist())
			return;
	}

	m_application->SetToolTipListener(NULL);
	m_showing_menu = TRUE;
	m_popup_menu_context = context;
	g_input_manager->LockMouseInputContext(TRUE);
	g_desktop_popup_menu_handler->OnPopupMenu(popup_name, placement, IsPointInPage(placement.rect.Center()), popup_value, use_keyboard_context);
	g_input_manager->LockMouseInputContext(FALSE);
	m_showing_menu = FALSE;
}

void BrowserMenuHandler::OnMenuShown(BOOL shown, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info) 
{
	g_scope_manager->desktop_window_manager->OnMenuShown(shown, info);
}

BOOL BrowserMenuHandler::IsShowingMenu() const
{
	return m_showing_menu;
}


void BrowserMenuHandler::AddMenuItems(void* platform_handle,
		const char* menu_name, INTPTR menu_value, INT32 start_at_index,
		OpInputContext* menu_input_context, BOOL is_sub_menu)
{
	if (!menu_name || !*menu_name)
		return;

	QuickMenu menu(m_global_context, platform_handle, menu_name, menu_value,
			start_at_index, menu_input_context);
	menu.AddMenuItems(menu_name, menu_value);

	if (m_showing_menu && m_popup_menu_context && m_popup_menu_context->GetPopupMessage())
		menu.AddPopupMessageMenuItems(m_popup_menu_context, menu_name, menu_value, is_sub_menu);
}


void BrowserMenuHandler::SetSpellSessionID(int id)
{
	m_spell_check_context->SetSpellSessionID(id);
}

int BrowserMenuHandler::GetSpellSessionID()	 const
{
	return m_spell_check_context->GetSpellSessionID();
}


#ifdef EMBROWSER_SUPPORT
void BrowserMenuHandler::SetContextMenuWasBlocked(BOOL blocked)
{
	m_menu_blocked = blocked;
}

BOOL BrowserMenuHandler::GetContextMenuWasBlocked()
{
	return m_menu_blocked;
}
#endif


void BrowserMenuHandler::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_MENU_SETUP))
	{
		m_menu_sections.DeleteAll();
	}
}


BOOL BrowserMenuHandler::IsPointInPage(const OpPoint& pt)
{
	DesktopWindow* active = g_application->GetActiveDesktopWindow(FALSE);
	if (active)
	{
		OpBrowserView* browser = active->GetBrowserView();
		if (browser)
			return browser->GetScreenRect().Contains(pt);
	}
	return FALSE;
}
