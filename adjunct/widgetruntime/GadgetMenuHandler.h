/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGET_MENU_HANDLER_H
#define GADGET_MENU_HANDLER_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/menus/DesktopMenuHandler.h"

class SpellCheckContext;


class GadgetMenuHandler : public DesktopMenuHandler
{
public:
	explicit GadgetMenuHandler(SpellCheckContext& spell_check_context);

	virtual PrefsSection* GetMenuSection(const char* menu_name);

	/**
	 * @override
	 */
	virtual void ShowPopupMenu(const char* popup_name, const PopupPlacement& placement, INTPTR popup_value = 0, BOOL use_keyboard_context = FALSE, DesktopMenuContext* context = NULL);

	virtual BOOL IsShowingMenu() const;

	virtual void OnMenuShown(BOOL shown, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info) {}

	/**
	 * @override
	 */
	virtual void AddMenuItems(void* platform_handle, const char* menu_name,
			INTPTR menu_value, INT32 start_at_index,
			OpInputContext* menu_input_context, BOOL is_sub_menu = FALSE);

	virtual void SetSpellSessionID(int id);
	virtual int GetSpellSessionID() const;  

#ifdef EMBROWSER_SUPPORT
	virtual void SetContextMenuWasBlocked(BOOL blocked)  {}
	virtual BOOL GetContextMenuWasBlocked()
		{ return FALSE; }
#endif // EMBROWSER_SUPPORT

	 
private:
	SpellCheckContext* m_spell_check_context;
	BOOL m_showing_menu;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_MENU_HANDLER_H
