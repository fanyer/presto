/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetMenuHandler.h"

#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/spellcheck/SpellCheckContext.h"
#include "modules/inputmanager/inputmanager.h"


GadgetMenuHandler::GadgetMenuHandler(SpellCheckContext& spell_check_context)
	: m_spell_check_context(&spell_check_context)
	, m_showing_menu(FALSE)
{
}


PrefsSection* GadgetMenuHandler::GetMenuSection(const char* menu_name)
{
	return QuickMenu::ReadMenuSection(menu_name);
}


void GadgetMenuHandler::ShowPopupMenu(const char* popup_name, const PopupPlacement& placement, INTPTR popup_value, BOOL use_keyboard_context, DesktopMenuContext* context)
{
	m_showing_menu = TRUE;
	g_input_manager->LockMouseInputContext(TRUE);
	g_desktop_popup_menu_handler->OnPopupMenu(popup_name, placement, FALSE, popup_value, use_keyboard_context);
	g_input_manager->LockMouseInputContext(FALSE);
	m_showing_menu = FALSE;
}


BOOL GadgetMenuHandler::IsShowingMenu() const
{
	return m_showing_menu;
}


void GadgetMenuHandler::AddMenuItems(void* platform_handle,
		const char* menu_name, INTPTR menu_value, INT32 start_at_index,
		OpInputContext* menu_input_context, BOOL is_sub_menu)
{
	if (!menu_name || !*menu_name)
		return;

	QuickMenu menu(NULL, platform_handle, menu_name, menu_value, start_at_index,
			menu_input_context);
	menu.AddMenuItems(menu_name, menu_value);
}


void GadgetMenuHandler::SetSpellSessionID(int id)
{
	m_spell_check_context->SetSpellSessionID(id);
}

int GadgetMenuHandler::GetSpellSessionID() const
{
	OP_ASSERT(NULL != m_spell_check_context);
	return m_spell_check_context->GetSpellSessionID();
}

#endif // WIDGET_RUNTIME_SUPPORT
