/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef BROWSER_MENU_HANDLER_H
#define BROWSER_MENU_HANDLER_H

#include "adjunct/desktop_util/settings/SettingsListener.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "modules/util/OpHashTable.h"

class ClassicApplication;
class ClassicGlobalUiContext;
class SpellCheckContext;
class DesktopMenuContext;


class BrowserMenuHandler
	: public DesktopMenuHandler
	, public SettingsListener
{
public:
	BrowserMenuHandler(ClassicApplication& application,
			ClassicGlobalUiContext& global_context,
			SpellCheckContext& spell_check_context);

	//
	// DesktopMenuHandler
	//
	virtual PrefsSection* GetMenuSection(const char* menu_name);

	/**
	 * @override
	 */
	virtual void ShowPopupMenu(const char* popup_name, const PopupPlacement& placement, INTPTR popup_value = 0, BOOL use_keyboard_context = FALSE, DesktopMenuContext* context = NULL);

	virtual BOOL IsShowingMenu() const;

	virtual void OnMenuShown(BOOL shown, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info);

	/**
	 * @override
	 */
	virtual void AddMenuItems(void* platform_handle, const char* menu_name,
			INTPTR menu_value, INT32 start_at_index,
			OpInputContext* menu_input_context, BOOL is_sub_menu = FALSE);

	virtual void SetSpellSessionID(int id);
	virtual int GetSpellSessionID() const;
	
#ifdef EMBROWSER_SUPPORT
	virtual void SetContextMenuWasBlocked(BOOL blocked);
	virtual BOOL GetContextMenuWasBlocked();
#endif // EMBROWSER_SUPPORT

	//
	// SettingsListener
	//
	void OnSettingsChanged(DesktopSettings* settings);

private:
	class MenuSection : public OpString8
	{
	public:
		MenuSection(const char* name, PrefsSection* section);
		~MenuSection();

		PrefsSection* GetSection();

	private:
		PrefsSection* m_section;
	};

	// Test if the point is in a OpBrwoserView
	static BOOL IsPointInPage(const OpPoint& pt);

	ClassicApplication* m_application;
	ClassicGlobalUiContext* m_global_context;
	OpAutoString8HashTable<MenuSection> m_menu_sections;
	SpellCheckContext* m_spell_check_context;
	BOOL m_showing_menu;
#ifdef EMBROWSER_SUPPORT
	BOOL m_menu_blocked;
#endif
	DesktopMenuContext* m_popup_menu_context;
};

#endif // BROWSER_MENU_HANDLER_H
