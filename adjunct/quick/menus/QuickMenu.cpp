/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/handlers/DownloadItem.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/quick/application/ClassicGlobalUiContext.h"
#include "adjunct/quick/application/QuickContextualUI.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/models/DesktopWindowCollectionItem.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#ifdef M2_SUPPORT
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2_ui/models/accessmodel.h" 
#include "adjunct/m2_ui/models/groupsmodel.h"
#endif // M2_SUPPORT
#include "modules/img/src/imagecontent.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/skin/OpSkinManager.h"
#ifdef INTERNAL_SPELLCHECK_SUPPORT
#include "modules/spellchecker/opspellcheckerapi.h"
#endif
#include "modules/util/OpLineParser.h"
#ifdef MSWIN
#include "platforms/windows/win_handy.h"
#endif // MSWIN


namespace
{
// For sorting CSS entries in style meny alphabetically.
struct StyleFileEntry
{
	const uni_char* name;
	INT32 index;

	static int Compare( const void *item1, const void *item2 )
	{
		StyleFileEntry* e1 = (StyleFileEntry*)item1;
		StyleFileEntry* e2 = (StyleFileEntry*)item2;

		if( !e1->name )
		{
			return 1;
		}
		else if( !e2->name )
		{
			return -1;
		}
		else
		{
			int ans;
			TRAPD(err, ans = g_oplocale->CompareStringsL(e1->name, e2->name));
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: handle error
			return ans;
		}
	}
};

/***********************************************************************************
**
**	TruncateStringToMaxWidth
**
***********************************************************************************/

const uni_char* TruncateStringToMaxWidth(const uni_char* inStr)
{
	INT32 			max_width;
	FontAtt 		menu_font_att;
	OpFont*			menu_font;
	INT32			title_width;
	INT32			title_len;
	static UINT32	ellipsis_width = 0;
	INT32			pos;
	INT32			approx_num_chars_to_remove;
	static const size_t kTruncateStringBufferSize = 256;
	static uni_char	buf[kTruncateStringBufferSize];

	if(!inStr)
	{
		return NULL;
	}

	max_width = g_pcui->GetIntegerPref(PrefsCollectionUI::MaxWidthForBookmarksInMenu);
	if(max_width == -1)
		return inStr;

	g_op_ui_info->GetFont(OP_SYSTEM_FONT_UI_MENU, menu_font_att, FALSE);

	menu_font = g_font_cache ? g_font_cache->GetFont(menu_font_att, 100) : NULL;
	if(menu_font)
	{
		title_len = uni_strlen(inStr);
		title_width = menu_font->StringWidth(inStr, title_len);
		if(title_width > max_width)
		{
			if(!ellipsis_width)
			{
				ellipsis_width = menu_font->StringWidth(UNI_L("..."), 3);
			}

			approx_num_chars_to_remove = menu_font->AveCharWidth() > 0 ? (title_width + ellipsis_width - max_width)/menu_font->AveCharWidth() : 0;

			pos = (title_len - approx_num_chars_to_remove);
			// Serious problem here or maybe lots of skinny characters
			// Regardless of what happens 40 chars is the hardlimit
			if (pos < 40)
				pos = 40;
			if(pos > (int)kTruncateStringBufferSize-4)		// 1 for null + 3 for ellipsis = 4
				pos = (kTruncateStringBufferSize-4); 	// 1 for null + 3 for ellipsis = 4

			uni_strncpy(buf, inStr, pos);				// NULL-termination OK
			uni_strncpy(&buf[pos], UNI_L("..."), 3);	// NULL-termination OK
			buf[pos + 3] = 0;

			return buf;
		}
	}

	return inStr;
}

} // end of anonymous namespace


int QuickMenu::s_recursion_breaker = 0;

const char DOC_CONTEXT_MENU_NAME_PREFIX[] = "document-context-menu";
const int DOC_CONTEXT_MENU_NAME_PREFIX_SIZE = sizeof(DOC_CONTEXT_MENU_NAME_PREFIX);

BOOL QuickMenu::IsMenuEnabled(ClassicGlobalUiContext* global_context,
		const OpStringC8& menu_name, INTPTR menu_value,
		OpInputContext* menu_input_context)
{
	if (menu_name.IsEmpty())
	{
		return FALSE;
	}

	// extension-generated submenus are always "enabled"
	if (StringUtils::StartsWith<DOC_CONTEXT_MENU_NAME_PREFIX_SIZE>(menu_name, DOC_CONTEXT_MENU_NAME_PREFIX))
		return TRUE;

	/* A menu is enabled if it has an element which is enabled.  If a
	 * menu includes itself as a submenu (possibly after some
	 * intervening menus), we may end up with an infinite recursion
	 * (see DSK-311302).  So, break recursion at a reasonable depth,
	 * and if we don't yet know if the menu should be enabled, enable
	 * it.
	 */
	if (s_recursion_breaker > 10)
		return TRUE;
	s_recursion_breaker += 1;
	QuickMenu menu(global_context, NULL, menu_name, menu_value, 0,
			menu_input_context);

	menu.AddMenuItems(menu_name, menu_value);
	s_recursion_breaker -= 1;

	return menu.m_enabled_item_count > 0;
}


BOOL QuickMenu::ParseMenuEntry(OpLineParser* line, OpString& item_name,
		OpString& default_item_name, OpString8 &sub_menu_name,
		INTPTR& sub_menu_value, INTPTR& menu_value, MenuType& menu_type,
		BOOL& ghost_item, OpWidgetImage& widget_image)
{
	return ParseMenuEntry(*g_application, line, item_name,
			default_item_name, sub_menu_name, sub_menu_value, menu_value,
			menu_type, ghost_item, widget_image);
}

BOOL QuickMenu::ParseMenuEntry(Application& application,
		OpLineParser* line,
		OpString& item_name, OpString& default_item_name,
		OpString8 &sub_menu_name, INTPTR& sub_menu_value,
		INTPTR& menu_value, MenuType& menu_type,
		BOOL& ghost_item, OpWidgetImage& widget_image)
{
	OpString item_type;
	sub_menu_value = -2;

	if (SkipEntry(*line, item_type))
		return FALSE;
	
	ghost_item = FALSE;
	if (uni_stristr(item_type.CStr(), UNI_L("---")))
		menu_type = MENU_SEPARATOR;
	else if (uni_stristr(item_type.CStr(), UNI_L("Submenu")))
		menu_type = MENU_SUBMENU;
	else if (uni_stristr(item_type.CStr(), UNI_L("BreakItem")))
		menu_type = MENU_BREAK;
	else if (uni_stristr(item_type.CStr(), UNI_L("GhostItem")))
	{
		menu_type = MENU_ITEM;
		ghost_item = TRUE;
	}
	else if (uni_stristr(item_type.CStr(), UNI_L("Item")))
		menu_type = MENU_ITEM;
	else if (uni_stristr(item_type.CStr(), UNI_L("Include")))
		menu_type = MENU_INCLUDE;
	else
		return FALSE;

	// Parse other values on line
	if (menu_type == MENU_ITEM
			|| menu_type == MENU_SUBMENU)
	{
		Str::LocaleString id = Str::NOT_A_STRING;
		line->GetNextLanguageString(item_name, &id);

		OpLanguageManager *langman = application.GetDefaultLanguageManager();
		if (langman)
			RETURN_VALUE_IF_ERROR(langman->GetString(id, default_item_name), FALSE);
	}

	if(menu_type == MENU_SUBMENU
			|| menu_type == MENU_INCLUDE)
	{
		RETURN_VALUE_IF_ERROR(line->GetNextToken8(sub_menu_name), FALSE);
		INT32 tmp_value = static_cast<INT32>(sub_menu_value);
		line->GetNextValue(tmp_value);
		sub_menu_value = tmp_value;
	}
	if(menu_type == MENU_SUBMENU)
	{
		OpString tmp_image;

		RETURN_VALUE_IF_ERROR(line->GetNextString(tmp_image), FALSE);

		if(tmp_image.HasContent())
		{
			OpString8 tmp;

			tmp.Set(tmp_image.CStr());

			widget_image.SetImage(tmp.CStr());
		}
		else
		{
			widget_image.Empty();
		}
	}
	return TRUE;
}


PrefsSection* QuickMenu::ReadMenuSection(const char* menu_name)
{
	PrefsSection *section = NULL;
	TRAPD(err, section = g_setup_manager->GetSectionL(menu_name, OPMENU_SETUP));
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
	return section;
}


QuickMenu::QuickMenu(ClassicGlobalUiContext* global_context,
		void* platform_handle, const char* menu_name, INTPTR menu_value,
		INT32 start_at_index, OpInputContext* menu_input_context)
{
	Init(*g_application->GetMenuHandler(), *g_application, global_context,
			platform_handle, menu_name, menu_value, start_at_index,
			menu_input_context);
}

QuickMenu::QuickMenu(DesktopMenuHandler& handler, Application& application,
		ClassicGlobalUiContext* global_context,	void* platform_handle,
		const char* menu_name, INTPTR menu_value, INT32 start_at_index,
		OpInputContext* menu_input_context)
{
	Init(handler, application, global_context, platform_handle, menu_name,
			menu_value, start_at_index, menu_input_context);
}

void QuickMenu::Init(DesktopMenuHandler& menu_handler, Application& application,
		ClassicGlobalUiContext* global_context, void* platform_handle,
		const char* menu_name, INTPTR menu_value, INT32 start_at_index,
		OpInputContext* menu_input_context)
{
	m_initial_menu_name = menu_name;
	m_initial_menu_value = menu_value;
	m_start_at_index = start_at_index;
	m_current_index = 0;
	m_enabled_item_count = 0;
	m_grown_too_large = NO;
	m_menu_handler = &menu_handler;
	m_application = &application;
	m_global_context = global_context;
	m_platform_handle = platform_handle;
	m_menu_input_context = menu_input_context;
}


/***********************************************************************************
**
**	AddMenuItems
**
***********************************************************************************/

void QuickMenu::AddMenuItems(const char* menu_name, INTPTR menu_value)
{
	if (op_strnicmp(menu_name, "Internal ", 9))
	{
		AddExternalMenuItems(menu_name, menu_value);
		return;
	}

	const char* internal_menu_name = menu_name + 9;


	if (op_stricmp(internal_menu_name, "Reload Menu") == 0)
	{
		OpInputAction* input_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_DISABLE_AUTOMATIC_RELOAD));
		if (input_action)
		{
			int timeout = 0;

			// The menu opens from speed dials and regular web pages. Only speed dials
			// send along a pointer to the speed dial entry and we must test if it is valid.
			if (g_speeddial_manager->FindSpeedDial(reinterpret_cast<DesktopSpeedDial*>(menu_value)) != -1)
				timeout = reinterpret_cast<DesktopSpeedDial*>(menu_value)->GetPreviewRefreshTimeout();
			else
			{
				DocumentDesktopWindow* dw = g_application->GetActiveDocumentDesktopWindow();
				if (dw)
				{
					OpWindowCommander* window_commander = dw->GetWindowCommander();
					if (window_commander)
						timeout = WindowCommanderProxy::GetUserAutoReload(window_commander)->GetOptSecUserSetting();
				}
			}

			OpString message;
			if (timeout > 0)
			{
				OpString name;
				g_languageManager->GetString(Str::M_RELOAD_MENU_DISABLE, name);

				int hours   = (timeout / 3600);
				int minutes = (timeout - hours*3600) / 60;
				int seconds = (timeout - hours*3600 - minutes*60);
				OpString duration;
				duration.AppendFormat(UNI_L("%02d:%02d:%02d"), hours, minutes, seconds);

				OpString msg;
				g_languageManager->GetString(Str::S_PAGE_RELOAD_INTERVAL, msg);
				message.AppendFormat(msg, duration.CStr());
			}
			else
				g_languageManager->GetString(Str::S_PAGE_NO_RELOAD_INTERVAL, message);

			input_action->GetActionInfo().SetStatusText(message);
			AddMenuSeparator();

			OpString name;
			g_languageManager->GetString(Str::M_RELOAD_MENU_DISABLE, name);
			AddMenuItem(name.CStr(), NULL, input_action);
		}
	}

#ifdef WEBSERVER_SUPPORT
	if (op_stricmp(internal_menu_name, "Webserver Menu") == 0)
	{
		OpString webserver_string;
		g_languageManager->GetString(Str::D_WEBSERVER_NAME, webserver_string);

		OpWidgetImage widget_image;
		widget_image.SetImage("Menu Opera Unite");

		if (g_webserver_manager)
		{
			if (g_webserver_manager->IsFeatureEnabled())
			{
				AddSubMenu(webserver_string.CStr(), &widget_image, "Webserver Popup Menu", 0); // Settings / configuration
			}
			else if (g_webserver_manager->IsFeatureAllowed())
			{
				AddSubMenu(webserver_string.CStr(), &widget_image, "Webserver Setup Popup Menu", 0); // Shows menu to enable / login
			}
		}
	} else
#endif // WEBSERVER_SUPPORT
#ifdef SUPPORT_DATA_SYNC
	if (op_stricmp(internal_menu_name, "Sync Menu") == 0)
	{
		OpString sync_string;
		g_languageManager->GetString(Str::M_SYNCHRONIZE_OPERA, sync_string);

		OpWidgetImage widget_image;
		widget_image.SetImage("Menu Synchronize");

		if (g_sync_manager)
		{
			if (g_sync_manager->IsLinkEnabled())
			{
				AddSubMenu(sync_string.CStr(), &widget_image, "Sync Popup Menu", 0);
			}
			else
			{
				AddSubMenu(sync_string.CStr(), &widget_image, "Sync Setup Popup Menu", 0);
			}
		}
	} else
#endif // SUPPORT_DATA_SYNC
	if (op_stricmp(internal_menu_name, "Bookmark Folder") == 0)
	{
		if( !KioskManager::GetInstance()->GetNoHotlist() )
			AddHotlistMenuItems(HotlistModel::BookmarkRoot, OpInputAction::ACTION_OPEN_LINK, "Bookmark Folder Menu", menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Add Contact") == 0)
	{
		AddHotlistMenuItems(HotlistModel::ContactRoot, OpInputAction::ACTION_UNKNOWN, "Contact Folder Menu", menu_value);
	}
#ifdef M2_SUPPORT
	else if (op_stricmp(internal_menu_name, "Add Nick To Contact") == 0)
	{
		AddHotlistMenuItems(HotlistModel::ContactRoot, OpInputAction::ACTION_ADD_NICK_TO_CONTACT, "Add Nick to Contact Menu", menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Compose TO Contact") == 0)
	{
		AddHotlistMenuItems(HotlistModel::ContactRoot, OpInputAction::ACTION_COMPOSE_TO_CONTACT, "Compose To Contact Menu", menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Compose CC Contact") == 0)
	{
		AddHotlistMenuItems(HotlistModel::ContactRoot, OpInputAction::ACTION_COMPOSE_CC_CONTACT, "Compose CC Contact Menu", menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Compose BCC Contact") == 0)
	{
		AddHotlistMenuItems(HotlistModel::ContactRoot, OpInputAction::ACTION_COMPOSE_BCC_CONTACT, "Compose BCC Contact Menu", menu_value);
	}
#endif // M2_SUPPORT
	else if (op_stricmp(internal_menu_name, "Note List") == 0)
	{
		AddHotlistMenuItems(HotlistModel::NoteRoot, OpInputAction::ACTION_INSERT, menu_name, menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Window List") == 0)
	{
		AddWindowListMenuItems();
	}
	else if (op_stricmp(internal_menu_name, "Closed Window List") == 0)
	{
		AddClosedWindowListMenuItems();
	}
	else if (op_stricmp(internal_menu_name, "Blocked Popup List") == 0)
	{
		AddBlockedPopupListMenuItems();
	}
	else if (op_stricmp(internal_menu_name, "Mail View") == 0)
	{
		AddMailViewMenuItems(menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Popular List") == 0)
	{
		AddPopularListItems();
	}
	else if (op_stricmp(internal_menu_name, "Page History") == 0)
	{
		DocumentDesktopWindow* document_window = m_application->GetActiveDocumentDesktopWindow();

		if (document_window)
		{
			AddHistoryPopupItems(document_window, true, false);

			if (document_window->HasDocument())
				AddMenuItem(document_window->GetTitle(TRUE), NULL, OpInputAction::ACTION_GO_TO_HISTORY, -1);

			AddHistoryPopupItems(document_window, false, false);
		}
	}
	else if (op_stricmp(internal_menu_name, "Back History") == 0)
	{
		AddHistoryPopupItems(m_application->GetActiveDocumentDesktopWindow(), false, false);
	}
	else if (op_stricmp(internal_menu_name, "Forward History") == 0)
	{
		AddHistoryPopupItems(m_application->GetActiveDocumentDesktopWindow(), true, false);
	}
	else if (op_stricmp(internal_menu_name, "Rewind History") == 0)
	{
		AddHistoryPopupItems(m_application->GetActiveDocumentDesktopWindow(), false, true);
	}
	else if (op_stricmp(internal_menu_name, "Fast Forward History") == 0)
	{
		AddHistoryPopupItems(m_application->GetActiveDocumentDesktopWindow(), true, true);
	}
	else if (op_stricmp(internal_menu_name, "Toolbar Extender List")==0 )
	{
		AddToolbarExtenderMenuItems();
	}
	else if (op_stricmp(internal_menu_name, "Panels") == 0)
	{
	 AddPanelListMenuItems();
	}
	else if (op_stricmp(internal_menu_name, "Session List")==0 )
	{
		OpWidgetImage widget_image;

		widget_image.SetImage("Session");

		OP_MEMORY_VAR OP_STATUS err;

		TRAP(err, g_session_manager->ScanSessionFolderL());
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ? Fails when unable to create folder.

		for (UINT32 i = 0; i < g_session_manager->GetSessionCount(); i++)
		{
			const uni_char* name = g_session_manager->GetSessionNameL(i).CStr();

			// don't add autosave to list
			if (uni_stricmp(name, UNI_L("autosave")) != 0)
			{
				AddMenuItem(name, &widget_image, OpInputAction::ACTION_SELECT_SESSION, i);
			}
		}
	}
	else if (op_stricmp(internal_menu_name, "Search List")==0)
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		for (UINT32 i = 0; i < g_searchEngineManager->GetSearchEnginesCount(); i++)
		{
			SearchTemplate* search = g_searchEngineManager->GetSearchEngine(i);

			OpString menu_item_text;
			search->GetEngineName(menu_item_text, FALSE);

			if (search->GetKey().HasContent() && menu_item_text.HasContent())
			{
				OpInputAction* action = OpInputAction::CreateToggleAction(OpInputAction::ACTION_SHOW_SEARCH, OpInputAction::ACTION_HIDE_SEARCH, search->GetID());

				if (search->GetKey().Length() >= 1 && menu_item_text.FindI(search->GetKey()) == KNotFound)
				{
					// add shortcuts for CJK languages and similar, bug #145902
					menu_item_text.Append(UNI_L("(&"));
					menu_item_text.Append(search->GetKey());
					menu_item_text.Append(UNI_L(")"));
				}
				Image image = search->GetIcon();
				OpWidgetImage widget_image;

				if(!image.IsEmpty())
				{
					widget_image.SetBitmapImage(image);
				}
				else
				{
					widget_image.SetImage("Search Web");
				}
				AddMenuItem(FALSE, menu_item_text.CStr(), &widget_image, NULL, 0, action);

				if (search->GetSeparatorAfter())
				{
					AddMenuItem(TRUE, NULL, NULL, NULL, 0, NULL);
				}
			}
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	}
	else if (op_stricmp(internal_menu_name, "Search With")==0)
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		for (UINT32 i = 0; i < g_searchEngineManager->GetSearchEnginesCount(); i++)
		{
			SearchTemplate* search = g_searchEngineManager->GetSearchEngine(i);

			OpString menu_item_text;
			search->GetEngineName(menu_item_text, FALSE);

			if (search->GetKey().HasContent() && menu_item_text.HasContent() && search->GetSearchType() != SEARCH_TYPE_INPAGE)
			{
				OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_HOTCLICK_SEARCH));
				if (!action)
					return;
				action->SetActionDataString(search->GetUniqueGUID().CStr());

				if (search->GetKey().Length() == 1 && menu_item_text.FindI(search->GetKey()) == KNotFound)
				{
					// add shortcuts for CJK languages and similar, bug #145902
					menu_item_text.Append(UNI_L("(&"));
					menu_item_text.Append(search->GetKey());
					menu_item_text.Append(UNI_L(")"));
				}
				Image image = search->GetIcon();
				OpWidgetImage widget_image;

				if(!image.IsEmpty())
				{
					widget_image.SetBitmapImage(image);
				}
				else
				{
					widget_image.SetImage("Search Web");
				}
				AddMenuItem(FALSE, menu_item_text.CStr(), &widget_image, NULL, 0, action);

				if (search->GetSeparatorAfter())
				{
					AddMenuItem(TRUE, NULL, NULL, NULL, 0, NULL);
				}
			}
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	}
	else if (op_stricmp(internal_menu_name, "Skin List")==0)
	{
		for (INT32 i = 0; i < g_skin_manager->GetSkinCount(); i++)
		{
			const uni_char* name = g_skin_manager->GetSkinName(i);

			AddMenuItem(name, NULL, OpInputAction::ACTION_SELECT_SKIN, i);
		}
	}
	else if (op_stricmp(internal_menu_name, "Style List") == 0)
	{
		DocumentDesktopWindow* document_window = m_application->GetActiveDocumentDesktopWindow();
		if (!document_window)
			return;

		DocumentView* doc_view = document_window->GetActiveDocumentView();
		if (!doc_view || doc_view->GetDocumentType() != DocumentView::DOCUMENT_TYPE_BROWSER)
			return;

		UINT32 total = g_pcfiles->GetLocalCSSCount();
		UINT32 i;

		// add items
		StyleFileEntry *list = OP_NEWA(StyleFileEntry, total);
		if( list )
		{
			UINT32 num_entry = 0;

			for (i = 0; i < total; i++)
			{
				if (g_pcfiles->GetLocalCSSName(i))
				{
					list[num_entry].name  = g_pcfiles->GetLocalCSSName(i);
					list[num_entry].index = i;
					num_entry ++;
				}
			}
			qsort(list, num_entry, sizeof(StyleFileEntry), StyleFileEntry::Compare );

			for (i = 0; i < (UINT32)num_entry; i++)
			{
				OpInputAction* action = OpInputAction::CreateToggleAction(OpInputAction::ACTION_SELECT_USER_CSS_FILE, OpInputAction::ACTION_DESELECT_USER_CSS_FILE, list[i].index);
				AddMenuItem(list[i].name, NULL, action);
			}
			OP_DELETEA(list);
		}
		else
		{
			for (i = 0; i < total; i++)
			{
				const uni_char *name = g_pcfiles->GetLocalCSSName(i);
				if (name)
				{
					OpInputAction* action = OpInputAction::CreateToggleAction(OpInputAction::ACTION_SELECT_USER_CSS_FILE, OpInputAction::ACTION_DESELECT_USER_CSS_FILE, i);
					AddMenuItem(name, NULL, action);
				}
			}
		}
		if (WindowCommanderProxy::GetActiveWindowCommander())
		{
			OpWindowCommander* commander = WindowCommanderProxy::GetActiveWindowCommander();
			if (total)
			{
				AddMenuSeparator();
			}

			for (i = 0; ; i++)
			{
				CssInformation information;

				if (!commander->GetAlternateCssInformation(i, &information))
					break;

				OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SELECT_ALTERNATE_CSS_FILE));
				if (!action)
					return;
				action->SetActionDataString(information.title);

				AddMenuItem(FALSE, information.title, NULL, NULL, 0, action);
			}
		}
	}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	else if (op_stricmp(internal_menu_name, "Spellcheck Language") == 0)
	{
		int spellsession_id = m_menu_handler->GetSpellSessionID();
		OpSpellUiSessionImpl session(spellsession_id);
		if(session.IsValid() && session.GetInstalledLanguageCount())
		{
			int i, count = session.GetInstalledLanguageCount();
			for(i=0;i<count;i++)
			{
#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
				AddMenuItem(session.GetInstalledLanguageNameAt(i), NULL, OpInputAction::ACTION_INTERNAL_SPELLCHECK_LANGUAGE, i);
#else
				AddMenuItem(session.GetInstalledLanguageStringAt(i), NULL, OpInputAction::ACTION_INTERNAL_SPELLCHECK_LANGUAGE, i);
#endif // WIC_SPELL_DICTIONARY_FULL_NAME
			}
			OpString item_name;
			g_languageManager->GetString(Str::M_INTERNAL_SPELLCHECK_ADD_LANGUAGES, item_name);
			AddMenuItem(item_name.CStr(), NULL, OpInputAction::ACTION_INSTALL_SPELLCHECK_LANGUAGE, FALSE);
		}
	}
	else if (op_stricmp(internal_menu_name, "Spellcheck Suggestions") == 0)
	{
		int spellsession_id = m_menu_handler->GetSpellSessionID();
		OpSpellUiSessionImpl session(spellsession_id);
		if(session.GetReplacementStringCount())
		{
			OpString replacement;
			int i, count = session.GetReplacementStringCount();
			for(i=0;i<count;i++)
			{
				session.GetReplacementString(i, replacement);
				AddMenuItem(replacement.CStr(), NULL, OpInputAction::ACTION_INTERNAL_SPELLCHECK_REPLACEMENT, i, TRUE);
			}
		}
	}
#endif // INTERNAL_SPELLCHECK_SUPPORT
	else if (op_stricmp(internal_menu_name, "Shell") == 0)
	{
		m_enabled_item_count ++;
		if (menu_value != 0 && m_platform_handle)
		{
			OpString filename;
			DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(menu_value);
			OpDocumentContext * ctx = menu_context->GetDocumentContext();
			URLInformation * url_info = NULL;

			BOOL privacy_mode = FALSE;
			if (menu_context->GetDesktopWindow())
				privacy_mode = menu_context->GetDesktopWindow()->PrivacyMode();
			else
				privacy_mode = g_application->GetActiveDesktopWindow() ? g_application->GetActiveDesktopWindow()->PrivacyMode() : FALSE; 

			if (ctx)
			{
				if (ctx->HasImage())
					ctx->CreateImageURLInformation(&url_info);
				else if (ctx->HasLink())
					ctx->CreateLinkURLInformation(&url_info);
				else
					ctx->CreateDocumentURLInformation(&url_info);

				if (url_info)
				{
					DownloadItem download_item(url_info, 0, ctx->HasImage(),privacy_mode); // DownloadItem should now own the url_info

					download_item.Init();
					download_item.GetMimeType();

					//Get the filename :
					if (ctx->HasImage())
					{
						download_item.GetFileName(filename);
						if (filename.IsEmpty() && url_info)
						{
							filename.Set(url_info->URLName());
							if (filename.Compare(UNI_L("file://localhost"), 16) == 0)
							{
								ConvertURLtoFullPath(filename);
							}
						}
					}
				}
				else if (ctx->HasLink())
				{
					URLInformation* url_info;
					if (OpStatus::IsSuccess(ctx->CreateLinkURLInformation(&url_info)))
					{
						filename.Set(url_info->URLName());
						OP_DELETE(url_info);
					}
				}
				else
				{
					URLInformation* url_info;
					if (OpStatus::IsSuccess(ctx->CreateDocumentURLInformation(&url_info)))
					{
						filename.Set(url_info->URLName());
						OP_DELETE(url_info);
					}
				}
			}
			else if (menu_context->GetFileUrlInfo())
			{
				DownloadItem download_item(menu_context->GetFileUrlInfo(), 0, FALSE, privacy_mode); // DownloadItem should now own the url_info

				download_item.Init();
				download_item.GetMimeType();

				download_item.GetFileName(filename);
				if (filename.IsEmpty() && menu_context->GetFileUrlInfo())
				{
					filename.Set(menu_context->GetFileUrlInfo()->URLName());
					if (filename.Compare(UNI_L("file://localhost"), 16) == 0)
					{
						ConvertURLtoFullPath(filename);
					}
				}
			}
			g_desktop_popup_menu_handler->OnAddShellMenuItems(m_platform_handle, filename.CStr());
		}
	}
#if defined (M2_SUPPORT)
	else if ((op_stricmp(internal_menu_name, "Get Mail") == 0 ||
			  op_stricmp(internal_menu_name, "Send Mail") == 0 ||
			  op_stricmp(internal_menu_name, "Chat Account") == 0 ||
			  op_stricmp(internal_menu_name, "Chat Connect") == 0 ||
			  op_stricmp(internal_menu_name, "Mail Account") == 0) &&
			  m_application->ShowM2() )
	{
		AccountManager* account_manager = g_m2_engine->GetAccountManager();
		Account* account = NULL;

		int i;

		// first populate account categories
		if (op_stricmp(internal_menu_name, "Mail Account") == 0)
		{
			OpString categories;

			for (i = 0; i < account_manager->GetAccountCount(); i++)
			{
				account = account_manager->GetAccountByIndex(i);

				if (account)
				{
					OpString category;
					account->GetAccountCategory(category);

					if (category.HasContent())
					{
						if (categories.FindI(category) == KNotFound)
						{
							categories.Append(category);
							OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ACCOUNT));
							if (action)
							{
								action->SetActionDataString(category.CStr());
								action->SetActionData(-1);
							}

							AddMenuItem(category.CStr(), NULL, action);
						}
					}
				}
			}
			if (categories.HasContent())
			{
				AddMenuSeparator();
			}
		}

		bool has_imap_account = false;

		for (i = 0; i < account_manager->GetAccountCount(); i++)
		{
			account = account_manager->GetAccountByIndex(i);

			if (account)
			{
				AccountTypes::AccountType type = account->GetIncomingProtocol();
				int id = account->GetAccountId();

				switch (type)
				{
					case AccountTypes::IRC:
					case AccountTypes::MSN:
				#ifdef JABBER_SUPPORT
					case AccountTypes::JABBER:
				#endif
					{
						if (op_stricmp(internal_menu_name, "Chat Account") == 0)
						{
							AddSubMenu(account->GetAccountName().CStr(), NULL, "Chat Account Menu", id);
						}
						else if (op_stricmp(internal_menu_name, "Chat Connect") == 0)
						{
							OpInputAction* input_action1 = OP_NEW(OpInputAction, (OpInputAction::ACTION_SET_CHAT_STATUS));
							if (input_action1)
							{
								input_action1->SetActionData(id);
								input_action1->SetActionDataString(UNI_L("online-any"));
								input_action1->SetActionOperator(OpInputAction::OPERATOR_OR);
							
								OpInputAction* input_action2 = OP_NEW(OpInputAction, (OpInputAction::ACTION_SET_CHAT_STATUS));
								if (input_action2)
								{
									input_action2->SetActionData(id);
									input_action2->SetActionDataString(UNI_L("offline"));
								}

								input_action1->SetNextInputAction(input_action2);
							}

							AddMenuItem(account->GetAccountName().CStr(), NULL, input_action1);
						}
						break;
					}
					case AccountTypes::IMAP:
						has_imap_account = true; // fallthrough
					case AccountTypes::POP:
					case AccountTypes::NEWS:
					{
						if (op_stricmp(internal_menu_name, "Get Mail") == 0)
						{
							AddMenuItem(account->GetAccountName().CStr(), NULL, OpInputAction::ACTION_GET_MAIL, id);
						}
						else if (op_stricmp(internal_menu_name, "Mail Account") == 0)
						{
							AddMenuItem(account->GetAccountName().CStr(), NULL, OpInputAction::ACTION_SHOW_ACCOUNT, id);
						}
/*						else if (op_stricmp(internal_menu_name, "Send Mail") == 0)
						{
	comment out until we support sending only from specific account
							AddMenuItem(name.CStr(), NULL, OpInputAction::ACTION_SEND_QUEUED_MAIL, id);
						}*/
						break;
					}
				}
			}
		}
		if (has_imap_account && op_stricmp(internal_menu_name, "Get Mail") == 0)
		{
			OpString full_sync;
			if (OpStatus::IsSuccess(g_languageManager->GetString(Str::M_MAIL_FULL_SYNC, full_sync)))
			{
				AddMenuSeparator();
				AddMenuItem(full_sync.CStr(), NULL, OpInputAction::ACTION_FULL_SYNC, 0);
			}
		}
	}
	else if (op_stricmp(internal_menu_name, "Mail Folder") == 0)
	{
		if (!m_application->HasMail() && !m_application->HasFeeds())
			return;
		g_desktop_popup_menu_handler->OnForceShortcutUnderline();
		AddMailFolderMenuItems(menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Mail Category Menu") == 0)
	{
		if (!m_application->HasMail() && !m_application->HasFeeds())
			return;
		AddMailCategoryMenuItems(menu_value);
	}
	else if (op_stricmp(internal_menu_name, "Mail Search In") == 0)
	{
		if (!m_application->HasMail() && !m_application->HasFeeds())
			return;

		AddAccessPointsMenuItems(menu_value, OpInputAction::ACTION_SEARCH_IN_INDEX);
	}
	else if (op_stricmp(internal_menu_name, "Mail Category Customize Menu") == 0)
	{
		if (!m_application->HasMail() && !m_application->HasFeeds())
			return;
		OpString text, full_text;
		OpInputAction::Action action = OpInputAction::ACTION_MAIL_HIDE_INDEX;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::M_MAIL_HIDE_THIS_SECTION, text));
		RETURN_VOID_IF_ERROR(full_text.AppendFormat(text.CStr(), g_m2_engine->GetIndexById(menu_value)->GetName().CStr()));
		AddMenuItem(full_text.CStr(), NULL, action, menu_value);
		AddMenuSeparator();
		AddAccessPointsMenuItems(menu_value, action);
	}
	else if (op_stricmp(internal_menu_name, "Mail Category Submenu") == 0)
	{
		if (!m_application->HasMail() && !m_application->HasFeeds())
			return;
		AddAccessPointsMenuItems(menu_value, OpInputAction::ACTION_MAIL_HIDE_INDEX);
	}
	else if (op_stricmp(internal_menu_name, "Mail Panel Configuration Menu") == 0)
	{
		if (!m_application->HasMail() && !m_application->HasFeeds())
			return;

		OpINT32Vector categories;
		g_m2_engine->GetIndexer()->GetCategories(categories);
		OpWidgetImage widget_image;
		widget_image.SetImage("Checkmark");
		for (UINT32 idx = 0; idx < categories.GetCount(); idx++)
		{
			Index* category = g_m2_engine->GetIndexer()->GetIndexById(categories.Get(idx));
			if (category)
			{
				// Hide index, index_id | Show index, index_id
				OpInputAction* action = OP_NEW(OpInputAction,(OpInputAction::ACTION_MAIL_HIDE_INDEX));
				if (action)
				{
					action->SetActionData(category->GetId());
					action->SetActionOperator(OpInputAction::OPERATOR_OR);
					OpInputAction* action2 = OP_NEW(OpInputAction,(OpInputAction::ACTION_MAIL_SHOW_INDEX));
					if (action2)
					{
						action2->SetActionData(category->GetId());
						action->SetNextInputAction(action2);
					}
				}
				
				AddMenuItem(category->GetName(), &widget_image, action, FALSE);
			}
		}
		AddMenuSeparator();
		OpString reset_string;
		g_languageManager->GetString(Str::M_RESET_MAIL_PANEL, reset_string);
		AddMenuItem(reset_string.CStr(), NULL, OpInputAction::ACTION_RESET_MAIL_PANEL, 0);
	}
	else if (op_stricmp(internal_menu_name, "Access Points") == 0)
	{
		if (!m_application->HasMail() && !m_application->HasFeeds())
			return;

		AddAccessPointsMenuItems(menu_value, OpInputAction::ACTION_READ_MAIL);
	}
	else if (op_stricmp(internal_menu_name, "RSS List") == 0)
	{
		if (m_application->HasFeeds())
		{
			INT32 id = 0;
			g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE, &id);

			if (id)
				AddAccessPointsMenuItems(id, OpInputAction::ACTION_READ_MAIL, FALSE);
		}
	}
	else if (op_stricmp(internal_menu_name, "Personal Information") == 0)
	{
		AddPersonalInformationMenuItems();
	}
	else if (op_stricmp(internal_menu_name, "Column List") == 0)
	{
		AddColumnMenuItems();
	}
	else if (op_stricmp(internal_menu_name, "Frame") == 0)
	{
		OpInputAction action(OpInputAction::ACTION_MAXIMIZE_FRAME);
		INT32 state = g_input_manager->GetActionState(&action, m_menu_input_context);
		BOOL disabled = state & OpInputAction::STATE_DISABLED && !(state & OpInputAction::STATE_ENABLED);
		if (!disabled)
		{
			OpString item_name;
			g_languageManager->GetString(Str::MI_IDM_POPUP_VIEW_FRAME_SOURCE_PARENT, item_name);
			AddSubMenu(item_name.CStr(), NULL, "Frame Menu", 0, NULL);
		}
	}
	else if (op_stricmp(internal_menu_name, "Document Background") == 0)
	{
		OpInputAction action(OpInputAction::ACTION_OPEN_BACKGROUND_IMAGE);
		INT32 state = g_input_manager->GetActionState(&action, m_menu_input_context);
		BOOL disabled = state & OpInputAction::STATE_DISABLED && !(state & OpInputAction::STATE_ENABLED);
		if (!disabled)
		{
			OpString item_name;
			g_languageManager->GetString(Str::MI_IDM_BACKGROUND_RELOAD2_PARENT, item_name);
			AddSubMenu(item_name.CStr(), NULL, "Document Background Menu", menu_value, NULL);
		}
	}
	else if (op_stricmp(internal_menu_name, "Select Subscribe Feed") == 0)
	{
		// The menu_value is a pointer to a vector containing all the feed
		// items (alternate pairs of title and href).
		OpAutoVector<OpString>* newsfeed_links = (OpAutoVector<OpString> *)(menu_value);
		OP_ASSERT(newsfeed_links != 0);

		OpString buf;
		if (buf.Reserve(512))
		{
			uni_char *menu_title = buf.CStr();

			// Loop the items and add them to the menu.
			for (UINT32 index = 0, count = newsfeed_links->GetCount(); index + 1 < count; index += 2)
			{
				const OpStringC& title_string = *newsfeed_links->Get(index);
				const OpStringC& href_string = *newsfeed_links->Get(index + 1);

				if (href_string.HasContent())
				{
					EscapeAmpersand(menu_title, 512, title_string.HasContent() ? title_string.CStr() : href_string.CStr() );

					OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SUBSCRIBE_NEWSFEED));
					if (!action)
						return;
					action->SetActionDataString(href_string.CStr());
					AddMenuItem(menu_title, 0, action);
				}
			}
		}
	}
//#if defined(GADGET_SUPPORT)
	else if (op_stricmp(internal_menu_name, "Select Install Widget") == 0)
	{
		// The menu_value is a pointer to a vector containing all the feed
		// items (alternate pairs of title and href).
		OpAutoVector<OpString>* widget_links = (OpAutoVector<OpString> *)(menu_value);
		OP_ASSERT(widget_links != 0);

		OpString buf;
		if (buf.Reserve(512))
		{
			uni_char *menu_title = buf.CStr();

			// Loop the items and add them to the menu.
			for (UINT32 index = 0, count = widget_links->GetCount(); index + 1 < count; index += 2)
			{
				const OpStringC& title_string = *widget_links->Get(index);
				const OpStringC& href_string = *widget_links->Get(index + 1);

				if (href_string.HasContent())
				{
					EscapeAmpersand(menu_title, 512, title_string.HasContent() ? title_string.CStr() : href_string.CStr() );
					OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_INSTALL_WIDGET));
					if (action)
					{
						action->SetActionDataString(href_string.CStr());
						action->SetActionImage("_"); // We want no image/icon (don't want document/gotopage icon)
					}
					AddMenuItem(menu_title, 0, action);
				}
			}
		}
	}
//#endif // GADGET_SUPPORT

	else if (op_stricmp(internal_menu_name, "Attachments List") == 0)
	{
		OpAutoVector<URL>* attachments = (OpAutoVector<URL>* )(menu_value);
		for (UINT32 index = 0, count = attachments->GetCount(); index < count; index ++)
		{
			OpWidgetImage widget_image;

			Image iconimage;

			OpBitmap* iconbitmap = NULL;

			OpString fileuniname;

			attachments->Get(index)->GetAttribute(URL::KUniName, fileuniname);

			OpString filepathname;

			attachments->Get(index)->GetAttribute(URL::KFilePathName_L, filepathname, TRUE);

			DesktopPiUtil::CreateIconBitmapForFile(&iconbitmap, filepathname.CStr());

			if(iconbitmap)
			{
				iconimage = imgManager->GetImage(iconbitmap);
			}

			widget_image.SetBitmapImage( iconimage, FALSE );

			OpString filename;
			TRAPD(err, attachments->Get(index)->GetAttributeL(URL::KSuggestedFileName_L, filename, TRUE));
			RETURN_VOID_IF_ERROR(err);

			OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_GO_TO_PAGE));
			if (!action)
				return;
			action->SetActionData(1);
			action->SetActionDataString(fileuniname.CStr());

			AddMenuItem(filename.CStr() , &widget_image, action);
		}

		AddMenuSeparator();
		OpInputAction* save_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SAVE_ATTACHMENT_LIST_TO_FOLDER));

		OpString item_name;
		g_languageManager->GetString(Str::M_MAIL_SAVE_ATTACHMENTS, item_name);
		AddMenuItem(item_name.CStr(), NULL, save_action);
	}
	else if (op_stricmp(internal_menu_name, "OpenIn Menu") == 0)
	{
		AddOpenInMenuItems(reinterpret_cast<DesktopMenuContext*>(menu_value));
	}

#endif // M2_SUPPORT
#ifdef SUPPORT_DATA_SYNC
	else if(op_stricmp(internal_menu_name, "Sync Status Menu") == 0)
	{
		OpString item_name;
		g_languageManager->GetString(Str::MI_SYNC_SHOW_STATUS, item_name);

		OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_SYNC_STATUS));
		if(action)
		{
			WidgetStateModifier* modifier = g_sync_manager->GetWidgetStateModifier();
			if(modifier && modifier->GetCurrentWidgetState())
			{
				OpWidgetImage image;

				image.SetImage(modifier->GetCurrentWidgetState()->GetStatusImage());
//				image.SetRestrictImageSize(FALSE);

				AddMenuItem(item_name.CStr(), &image, action);
			}
		}
	}
#endif // SUPPORT_DATA_SYNC
#ifdef WEBSERVER_SUPPORT
	else if(op_stricmp(internal_menu_name, "Webserver Status Menu") == 0)
	{
		if (g_webserver_manager->IsFeatureAllowed())
		{
			// MI_WEBSERVER_SHOW_STATUS
			OpString item_name;
			g_languageManager->GetString(Str::MI_WEBSERVER_SHOW_STATUS, item_name);

			OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_WEBSERVER_STATUS));
			if(action)
			{
				WidgetStateModifier* modifier = g_webserver_manager->GetWidgetStateModifier();
				if(modifier && modifier->GetCurrentWidgetState())
				{
					OpWidgetImage image;

					image.SetImage(modifier->GetCurrentWidgetState()->GetStatusImage());
//					image.SetRestrictImageSize(FALSE);

					AddMenuItem(item_name.CStr(), &image, action);
				}
			}
		}
	}
#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
	else if(op_stricmp(internal_menu_name, "Webserver Discovered Services Menu") == 0)
	{
#ifdef UPNP_SUPPORT
		if (g_webserver_manager->IsFeatureAllowed() && g_webserver_manager->IsLocalDiscoveryNotificationsEnabled())
		{
			const OpVector<KnownDeviceMenuEntry> & list = g_webserver_manager->GetKnownDeviceList();
			if (list.GetCount() == 0)
			{
				OpString not_found;
				g_languageManager->GetString(Str::S_WEBSERVER_DISCOVERY_NO_DEVICES_FOUND, not_found);
				AddMenuItem(FALSE, not_found, NULL, NULL, 0, NULL, FALSE, NULL, FALSE, TRUE);
			}
			else
			{
				for (UINT32 i = 0; i < list.GetCount(); i++)
				{
					KnownDeviceMenuEntry * entry = list.Get(i);

					//OP_ASSERT(entry->m_url.HasContent());
					OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE));
					if (action && entry->m_description_string.HasContent())
					{
						action->SetActionDataString(entry->m_url.CStr());
						AddMenuItem(entry->m_description_string.CStr(), 0, action);
					}
					else
					{
						OP_ASSERT(FALSE); // description string or action not valid. check creation of known device list
					}
				}
			}
		}
#endif // UPNP_SUPPORT
	}
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT
#endif // WEBSERVER_SUPPORT
	else if (op_stricmp(internal_menu_name, "Content Sharing") == 0)
	{
		g_desktop_popup_menu_handler->BuildPlatformCustomMenuByName(m_platform_handle, internal_menu_name);
	}
}

/***********************************************************************************
**
**	AddOpenInMenuItems
**
***********************************************************************************/

void QuickMenu::AddOpenInMenuItems(DesktopMenuContext* menu_context)
{
	if(!menu_context)
	{
		// If we have no menu_context, this could mean that this was invoked
		// from a toolbar instead of a context menu. In order to support this
		// we need to know if there is a current doc, and then get the url-info
		// for that.
		OP_ASSERT(FALSE); //Without an info there is no context for this menu
		return;
	}
	OpDocumentContext * context = menu_context ? menu_context->GetDocumentContext() : NULL;
	URLInformation * url_info = NULL;
	if (context)
	{
		if (context->HasImage())
		{
			if (OpStatus::IsSuccess(context->CreateImageURLInformation(&url_info)))
			{
				OP_ASSERT(url_info);
			}
			else
			{
				OP_ASSERT(!url_info); // No URL or OOM. Same same in this case
			}
		}
		else if (context->HasBGImage())
		{
			if (OpStatus::IsSuccess(context->CreateBGImageURLInformation(&url_info)))
			{
				OP_ASSERT(url_info);
			}
			else
			{
				OP_ASSERT(!url_info); // No URL or OOM. Same same in this case
			}
		}
		else
		{
			if (OpStatus::IsSuccess(context->CreateDocumentURLInformation(&url_info)))
			{
				OP_ASSERT(url_info);
			}
			else
			{
				OP_ASSERT(!url_info); // No URL or OOM. Same same in this case
			}
		}
	}

	BOOL is_download = FALSE;
	if (context && (context->HasImage() || context->HasBGImage()))
		is_download = TRUE;
	if (menu_context->GetFileUrlInfo())
		is_download = TRUE;

	if (!context)
	{
		OP_DELETE(url_info);
		url_info = menu_context->GetFileUrlInfo();
	};

	if (url_info)
	{
		BOOL privacy_mode = FALSE;
		if (menu_context->GetDesktopWindow())
			privacy_mode = menu_context->GetDesktopWindow()->PrivacyMode();
		else
			privacy_mode = g_application->GetActiveDesktopWindow() ? g_application->GetActiveDesktopWindow()->PrivacyMode() : FALSE; 

		DownloadItem download_item(url_info, 0, is_download, privacy_mode);
		download_item.Init();
		download_item.GetMimeType();

		OpString filename;
		if (!download_item.GetFile().IsExecutable())
		{
			//Get the filename :
			if (is_download)
			{
				download_item.GetFileName(filename);
				if (filename.IsEmpty())
				{

					filename.Set(url_info->URLName());
					if (filename.Compare(UNI_L("file://localhost"), 16) == 0)
					{
						ConvertURLtoFullPath(filename);
					}
				}
			}
			else if (context && context->HasLink())
			{
				OP_DELETE(url_info);
				url_info = NULL;
				if (OpStatus::IsSuccess(context->CreateLinkURLInformation(&url_info)))
				{
					OP_ASSERT(url_info);
					filename.Set(url_info->URLName());
					OP_DELETE(url_info);
					url_info = NULL;
				}
				else
				{
					OP_ASSERT(!url_info); // No URL or OOM. Same same in this case
				}
			}
			else if (context)
			{
				OP_DELETE(url_info);
				url_info = NULL;
				if (OpStatus::IsSuccess(context->CreateDocumentURLInformation(&url_info)))
				{
					OP_ASSERT(url_info);
					filename.Set(url_info->URLName());
					OP_DELETE(url_info);
					url_info = NULL;
				}
				else
				{
					OP_ASSERT(!url_info); // No URL or OOM. Same same in this case
				}
			}

			//Get the handlers :
			OpVector<HandlerContainer> handlers;
			download_item.GetFileHandlers(handlers);

			//Add them to the menu :
			for(UINT32 i = 0; i < handlers.GetCount(); i++)
			{
				HandlerContainer * handler = handlers.Get(i);

				const uni_char * handler_command = handler->GetCommand().CStr();
				const uni_char * handler_name    = handler->GetName().CStr();
				OpWidgetImage widget_image;
				widget_image.SetWidgetImage(&handler->GetWidgetImageIcon());

				OpInputAction* action;
#ifdef WIC_CAP_URLINFO_OPENIN
				if (context->HasImage())
					 action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_IMAGE_IN, reinterpret_cast<INTPTR>(context), handler_command));
				else
#endif
				//Make the action :
					action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_FILE_IN));
					if (action)
					{
						action->SetActionData(1);
						action->SetActionDataString(handler_command);
						action->SetActionDataStringParameter(filename.CStr());
					}

				//Add it with the icon :
				AddMenuItem(handler_name ? handler_name : handler_command, &widget_image, action);

				//Add seperator after first one (the default one) :
				if(i == 0 && handlers.GetCount() > 1)
					AddMenuItem(TRUE, NULL, NULL, NULL, 0, NULL);
			}
		}
	}
}


/***********************************************************************************
**
**	AddHotlistMenuItems
**
***********************************************************************************/

void QuickMenu::AddHotlistMenuItems(INT32 type, OpInputAction::Action item_action, const char* folder_menu_name, INT32 id)
{
#define EMAIL_ADDRESSES_IN_SUBMENU 1

// The widget runtime doesnt initialize HotListManager, hence we need this check to make sure there are no crashes when showing Context Menu's
if(g_hotlist_manager == NULL)
	return;

#if EMAIL_ADDRESSES_IN_SUBMENU
	BOOL keep_adding = TRUE;

	if (id != 0)
	{
		HotlistModelItem* parent_item = g_hotlist_manager->GetItemByID(id);
		if (parent_item != 0 && parent_item->IsContact())
		{
			// Add email addresses under the contact submenu.
			OpAutoVector<OpString> mail_addresses;
			parent_item->GetMailAddresses(mail_addresses);

			for (INT32 index = 0, address_count = mail_addresses.GetCount();
				index < address_count && keep_adding; ++index)
			{
				const OpStringC& current_address = *mail_addresses.Get(index);
				OpInputAction* input_action = OP_NEW(OpInputAction, (item_action));
				if (!input_action)
					return;
				input_action->SetActionData(parent_item->GetID());
				input_action->SetActionDataString(current_address.CStr());
				keep_adding = AddMenuItem(TruncateStringToMaxWidth(current_address.CStr()), 0, input_action, index == 0);
			}
			return;
		}
	}

	OpVector<HotlistModelItem> items;

	/* OBS: This gives back a list sorted in the order set in preferences, which will be the last one set,
	   meaning if you have several windows open with different sortings, the menu in the all of them
	   will have its hotlist items sorted as in the last one opened */
	g_hotlist_manager->GetModelItemList(type, id, TRUE, items);

	for( UINT32 i=0; i < items.GetCount() && keep_adding; i++ )
	{
		HotlistModelItem *hli = items.Get(i);

		if( hli->IsFolder() )
		{
			if (folder_menu_name)
			{
				OpWidgetImage widget_image;
				widget_image.SetImage( hli->GetImage() );
				OpInputAction* input_action = OP_NEW(OpInputAction, ( OpInputAction::ACTION_MENU_FOLDER ));
				if (!input_action)
					return;
				input_action->SetActionData(hli->GetID());

				// make sure that Ctrl+Shift+T is displayed for active bookmark folder

				BOOL is_active_folder = FALSE;

				if (op_stricmp(folder_menu_name, "Bookmark Folder Menu") == 0)
				{
					BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();
					if (browser && browser->GetSelectedHotlistItemId(OpTypedObject::PANEL_TYPE_BOOKMARKS) == hli->GetID())
					{
						is_active_folder = TRUE;
					}
				}


				keep_adding = AddSubMenu(TruncateStringToMaxWidth(hli->GetMenuName().CStr()), &widget_image, is_active_folder ? "Active Bookmark Folder Menu" : folder_menu_name, hli->GetID(), input_action );

			}
		}
		else
		{
			if (item_action != OpInputAction::ACTION_UNKNOWN)
			{
				if (hli->IsSeparator())
				{
					keep_adding = AddMenuSeparator();
				}
				else
				{
					OpWidgetImage widget_image;
					widget_image.SetImage( hli->GetImage() );
					Image img = hli->GetIcon();
					widget_image.SetBitmapImage( img, FALSE );
#ifdef M2_SUPPORT
					//Adding a contact
					if (hli->IsContact())
					{

						// Only add a submenu if the contact has more than one e-mail addresses.
						OpAutoVector<OpString> mail_addresses;
						hli->GetMailAddresses(mail_addresses);

						if (mail_addresses.GetCount() == 1)
						{
							const OpStringC& mail_address = *mail_addresses.Get(0);

							OpInputAction* input_action = OP_NEW(OpInputAction, (item_action));
							if (!input_action)
								return;
							input_action->SetActionData(hli->GetID());
							input_action->SetActionDataString(mail_address.CStr());
							keep_adding = AddMenuItem(TruncateStringToMaxWidth(hli->GetMenuName().CStr()), &widget_image, input_action);
						}
						else if (mail_addresses.GetCount() > 1)
						{
							OpInputAction* input_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_MENU_FOLDER));
							if (input_action)
							{
								input_action->SetActionData(hli->GetID());
								hli->GetInfoText(input_action->GetActionInfo());
							}
							keep_adding = AddSubMenu(TruncateStringToMaxWidth(hli->GetName().CStr()), &widget_image, folder_menu_name, hli->GetID(), input_action);
						}
					}
					else
#endif // M2_SUPPORT
					//default method of adding hotlist items
					{
						//make the menu entry bold if needed
						BOOL bold =
#ifdef GADGET_SUPPORT
							hli->IsGadget() ? static_cast<GadgetModelItem*>(hli)->IsGadgetRunning() :
#endif // GADGET_SUPPORT
							 FALSE;

						OpInputAction* input_action = OP_NEW(OpInputAction, (item_action));
						if (input_action)
						{
							input_action->SetActionData(hli->GetID());
							input_action->SetActionDataString(hli->GetName().CStr());
							hli->GetInfoText(input_action->GetActionInfo());
						}
						keep_adding = AddMenuItem(TruncateStringToMaxWidth(hli->GetMenuName().CStr()), &widget_image, input_action, bold);
					}
				}
			}
		}
	}
#else //EMAIL_ADDRESSES_IN_SUBMENU
	OpVector<HotlistModelItem> items;

	/* OBS: This gives back a list sorted in the order set in preferences, which will be the last one set,
	   meaning if you have several windows open with different sortings, the menu in the all of them
	   will have its hotlist items sorted as in the last one opened */
	g_hotlist_manager->GetModelItemList(type, id, TRUE, items);
	BOOL keep_adding = TRUE;

	for( UINT32 i=0; i < items.GetCount() && keep_adding; i++ )
	{
		HotlistModelItem *hli = items.Get(i);

		if( hli->IsFolder() )
		{
			if (folder_menu_name)
			{
				OpWidgetImage widget_image;
				widget_image.SetImage( hli->GetImage() );
				OpInputAction* input_action = OP_NEW(OpInputAction, ( OpInputAction::ACTION_MENU_FOLDER, hli->GetID() ));

				if (input_action)
				{
					hli->GetInfoText(input_action->GetActionInfo());
				}
				keep_adding = AddSubMenu(TruncateStringToMaxWidth(hli->GetMenuName().CStr()), &widget_image, folder_menu_name, hli->GetID(), input_action );
			}
		}
		else
		{
			if (item_action != OpInputAction::ACTION_UNKNOWN)
			{
				if (hli->IsSeparator())
				{
					keep_adding = AddMenuSeparator();
				}
				else
				{
					OpWidgetImage widget_image;
					widget_image.SetImage( hli->GetImage() );
				widget_image.SetBitmapImage( hli->GetIcon(), FALSE );

					//make the menu entry bold if needed
					BOOL bold = hli->IsGadget() ? static_cast<GadgetModelItem*>(hli)->IsGadgetRunning() : FALSE;

					OpInputAction* input_action = OP_NEW(OpInputAction, (item_action, hli->GetID(), hli->GetName().CStr()));

					if (input_action)
					{
						hli->GetInfoText(input_action->GetActionInfo());
					}
					keep_adding = AddMenuItem(TruncateStringToMaxWidth(hli->GetMenuName().CStr()), &widget_image, input_action, bold);
				}
			}
		}
	}
#endif //EMAIL_ADDRESSES_IN_SUBMENU
}

/***********************************************************************************
**
**	AddPersonalInformationMenuItems
**
***********************************************************************************/

void QuickMenu::AddPersonalInformationMenuItems()
{
	const int personal_info_fields = 14;
	const int max_menu_item_length = 45;

	PrefsCollectionCore::stringpref personalinfo[personal_info_fields] =
	{
		PrefsCollectionCore::Firstname,
		PrefsCollectionCore::Surname,
		PrefsCollectionCore::Address,
		PrefsCollectionCore::City,
		PrefsCollectionCore::State,
		PrefsCollectionCore::Zip,
		PrefsCollectionCore::Country,
		PrefsCollectionCore::Telephone,
		PrefsCollectionCore::Telefax,
		PrefsCollectionCore::EMail,
		PrefsCollectionCore::Home,
		PrefsCollectionCore::Special1,
		PrefsCollectionCore::Special2,
		PrefsCollectionCore::Special3
	};

	// special first item that combines firstname and surname

	OpString tmp;
	tmp.Set(g_pccore->GetStringPref(PrefsCollectionCore::Firstname));
	if( !tmp.IsEmpty() )
	{
		OpString fullName;
		fullName.Set(tmp);
		tmp.Set(g_pccore->GetStringPref(PrefsCollectionCore::Surname));

		if( !tmp.IsEmpty() )
		{
			fullName.Append(" ");
			fullName.Append(tmp);
			OpString full_name_cutoff;
			full_name_cutoff.Set(fullName.CStr(), MIN(max_menu_item_length, fullName.Length()));

			// Insert into list as first element
			OpInputAction* menu_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_INSERT));
			if (!menu_action)
				return;
			menu_action->SetActionDataString(fullName.CStr());

			AddMenuItem(full_name_cutoff, NULL, menu_action);
		}
	}

	OpString data;
	int i;
	for (i = 0; i < personal_info_fields; i ++)
	{
		data.Set(g_pccore->GetStringPref(personalinfo[i]));
		if (data.HasContent())
			// the entry 0 is reserved above
		{
			OpString data_cutoff;
			data_cutoff.Set(data.CStr(), MIN(max_menu_item_length, data.Length()));

			OpInputAction* menu_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_INSERT));
			if (!menu_action)
				return;
			menu_action->SetActionDataString(data.CStr());

			AddMenuItem(data_cutoff.CStr(), NULL, menu_action);
		}
	}
}

/***********************************************************************************
**
**	AddMailViewMenuItems
**
***********************************************************************************/

void QuickMenu::AddMailViewMenuItems(INT32 index_id)
{
#if defined(M2_SUPPORT)
	// add types

	if (index_id >= IndexTypes::FIRST_CONTACT && index_id < IndexTypes::LAST_CONTACT && g_m2_engine && !g_m2_engine->IsIndexMailingList(index_id))
	{
		AddMenuItems("Mail Contact View Menu", index_id);
	}
	else if (index_id >= IndexTypes::DOC_ATTACHMENT && index_id < IndexTypes::ZIP_ATTACHMENT)
	{
		AddMenuItems("Mail Attachment View Menu", index_id);
	}
	else
	{
		AddMenuItems("Mail Standard View Menu", index_id);
	}
#endif
}

/***********************************************************************************
**
**	AddWindowListMenuItems
**
***********************************************************************************/

void QuickMenu::AddWindowListMenuItems()
{
	INT32 i;
	INT32 parent_count = 0;
	
    if (NULL == m_application->GetWindowList())
    {
		return;
	}
	OpTreeModel& window_tree = *m_application->GetWindowList();

	for (i = 0; i < window_tree.GetItemCount(); i++)
	{
		if (window_tree.GetItemParent(i) == -1)
		{
			parent_count++;
		}
	}

	INT32 number = 1;


	OpString escaped;
	escaped.Reserve(512);

	for (i = 0; i < window_tree.GetItemCount(); i++)
	{
		DesktopWindow* window = static_cast<DesktopWindowCollectionItem*>(
					window_tree.GetItemByPosition(i))->GetDesktopWindow();

		if (!window || (window_tree.GetItemParent(i) == -1 && parent_count < 2))
			continue;

		OpString name;
		INT32 parent_index;

		if ((parent_index = window_tree.GetItemParent(i)) != -1
				&& parent_count > 1)
		{
			// If there is only 1 child then don't add it!
			if (static_cast<DesktopWindowCollectionItem*>(
						window_tree.GetItemByPosition(parent_index))
					->GetSubtreeSize() == 1)
				continue;

			name.Set(UNI_L("   "));
		}

		if (number <= 10)
		{
			uni_char number_string[16];

			uni_itoa(number, number_string, 10);

			name.Append(number_string);
			name.Append(UNI_L(" "));
			number++;
		}

		name.Append(window->GetTitle());

		if( escaped.Capacity() > 0 && name.CStr())
		{
			EscapeAmpersand( escaped.CStr(), escaped.Capacity(), name.CStr() );
			name.Set(escaped);
		}

		AddMenuItem(TruncateStringToMaxWidth(name.CStr()), const_cast<OpWidgetImage*>(window->GetWidgetImage()), OpInputAction::ACTION_ACTIVATE_WINDOW, window->GetID(), window == m_application->GetActiveDesktopWindow(FALSE) );
	}
}

/***********************************************************************************
**
**	AddClosedWindowListMenuItems
**
***********************************************************************************/

void QuickMenu::AddClosedWindowListMenuItems()
{
	for(UINT32 i=0; i < g_session_manager->GetClosedSessionCount(); i++)
	{
		OpString name;
		OP_STATUS rc = g_languageManager->GetString(Str::S_WINDOW_HEADER, name);

		if(OpStatus::IsSuccess(rc))
		{
			OpString title;
			OpSession *session = g_session_manager->GetClosedSession(i);

			if(session)
			{
				UINT32 window_count = 0;
				TRAP(rc, window_count = session->GetWindowCountL());
				for(UINT32 j=0; j < window_count; j++)
				{
					OpSessionWindow *window = NULL;
					TRAP(rc, window = session->GetWindowL(j));
					if(window)
					{
						INT32 active = 0;
						TRAP(rc, active = window->GetValueL("active"));
						if(active)
						{
							OpVector<OpString> array;
							UINT32 history_pos = 0;
							TRAP(rc, window->GetStringArrayL("history title", array));
							TRAP(rc, history_pos = window->GetValueL("current history"));
							if(history_pos > 0 && array.GetCount() >= history_pos)
							{
								OpString *s = array.Get(history_pos-1);
								if(s && s->CStr())
								{
									title.Set(s->CStr());
								}
							}
							break;
						}
					}
				}
			}

			name.AppendFormat(UNI_L(" %d"), i+1);
			if(title.HasContent())
			{
				name.AppendFormat(UNI_L(" - %s"), title.CStr());
			}
			AddMenuItem(name.CStr(), NULL, OpInputAction::ACTION_REOPEN_WINDOW, i);
		}
	}

	BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();

	if (!browser)
		return;

	if(browser->GetUndoSession() && browser->GetUndoSession()->GetWindowCountL() > 0 && g_session_manager->GetClosedSessionCount() > 0)
	{
		AddMenuSeparator();
	}

	AddSessionMenuItems(browser->GetUndoSession(), OpInputAction::ACTION_REOPEN_PAGE);
}


/***********************************************************************************
**
**	AddBlockedPopupListMenuItems
**
***********************************************************************************/

void QuickMenu::AddBlockedPopupListMenuItems()
{
	BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();

	if (!browser)
		return;

	INT32 count = browser->GetBlockedPopupCount();

	for (INT32 i = 0; i < count; i++)
	{
		BlockedPopup* popup = browser->GetBlockedPopup(i);

		OpWidgetImage widget_image;

		widget_image.SetImage("Window Document Icon");

		Image img = g_favicon_manager->Get(popup->GetURL());
		widget_image.SetBitmapImage(img, FALSE);

		if (!AddMenuItem(popup->GetTitle(), &widget_image, OpInputAction::ACTION_OPEN_BLOCKED_POPUP, i, popup->IsFrom(browser->GetActiveDocumentDesktopWindow())))
			break;
	}
}

/***********************************************************************************
**
**	AddSessionMenuItems
**
***********************************************************************************/

void QuickMenu::AddSessionMenuItems(OpSession* session, OpInputAction::Action item_action)
{
	if (!session)
		return;

	INT32 count = session->GetWindowCountL();
	INT32 number = 1;
	INT32 i;

	for (i = 0; i < count; i++)
	{
		OpSessionWindow* session_window = session->GetWindowL(count - 1 - i);

		OpString name;

		if (number <= 10)
		{
			uni_char number_string[16];

			uni_itoa(number, number_string, 10);

			name.Append(number_string);
			name.Append(UNI_L(" "));
			number++;
		}

		name.Append(session_window->GetStringL("title"));

		if (!AddMenuItem(name.CStr(), session_window->GetWidgetImage(), item_action, i))
			break;
	}
}

/***********************************************************************************
**
**	AddMailFolderMenuItems
**
***********************************************************************************/

void QuickMenu::AddMailFolderMenuItems(UINT32 parent_id)
{
#if defined(M2_SUPPORT)
	if( !m_application->ShowM2() )
	{
		return;
	}

	if (!m_platform_handle)
	{
		m_enabled_item_count++;
		return;
	}

	OpTreeView* mail_window_tree_view = (OpTreeView*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_TREEVIEW, m_menu_input_context);

	UINT32 selected_message = 0;
	if (mail_window_tree_view)
	{
		OpTreeModelItem* item = mail_window_tree_view->GetSelectedItem();
		if (item)
			selected_message = item->GetID();
	}

	if (parent_id != 0 && parent_id < IndexTypes::FIRST_ACCOUNT)
	{
		Index* parent_index = g_m2_engine->GetIndexer()->GetIndexById(parent_id);
		if (parent_index)
		{
			OpString label_as, label_as_index_name;
			OpWidgetImage widget_image;
			const char* image = NULL;
			Image bitmap_image;

			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_LABEL_AS_PARENT_LABEL, label_as));
			RETURN_VOID_IF_ERROR(label_as_index_name.AppendFormat(label_as.CStr(), parent_index->GetName().CStr()));
			
			RETURN_VOID_IF_ERROR(parent_index->GetImages(image, bitmap_image));
			widget_image.SetImage(image);
			widget_image.SetBitmapImage(bitmap_image, FALSE);

			OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_ADD_TO_VIEW));
			if (!action)
				return;
			action->SetActionData(parent_id);
			action->SetActionOperator(OpInputAction::OPERATOR_OR);

			OpInputAction* next_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_REMOVE_FROM_VIEW));
			if (!next_action)
			{
				OP_DELETE(action);
				return;
			}
			next_action->SetActionData(parent_id);
			action->SetNextInputAction(next_action);
				
			AddMenuItem(label_as_index_name.CStr(), &widget_image, action, parent_index->Contains(selected_message));
			AddMenuSeparator();
		}
	}

	AccessModel access_model (parent_id ? parent_id : (UINT32)IndexTypes::CATEGORY_LABELS, g_m2_engine->GetIndexer());
	RETURN_VOID_IF_ERROR(access_model.Init());

	OpTreeView* treeview;
	RETURN_VOID_IF_ERROR(OpTreeView::Construct(&treeview));

	treeview->SetTreeModel(&access_model, 0);
	treeview->OpenAllItems(TRUE);

	int count = treeview->GetLineCount();
	BOOL keep_adding = TRUE;

	for (int i = 0; i < count && keep_adding; i++)
	{
		INT32 item_pos = treeview->GetItemByLine(i);

		OpTreeModelItem* item = treeview->GetItemByPosition(item_pos);

		if (item)
		{
			INT32 parent = 0;

			OpTreeModelItem* parent_item = treeview->GetParentByPosition(item_pos);

			if (parent_item)
			{
				parent = parent_item->GetID();

				if (parent == 0)
				{
					parent -= treeview->GetParent(item_pos) + 2;
				}
			}

			if (parent && parent_id != (UINT32) parent)
			{
				continue;
			}

			INT32 item_id = item->GetID();

			if (item_id == 0)
			{
				item_id -= item_pos + 2;
			}

			OpString name;
			OpWidgetImage widget_image;

			if (item->GetType() == OpTypedObject::INDEX_TYPE)
			{
				Index* index = g_m2_engine->GetIndexById(item->GetID());
				if (index)
				{
					const char* image = NULL;
					Image bitmap_image;
					RETURN_VOID_IF_ERROR(index->GetImages(image, bitmap_image));

					widget_image.SetImage(image);
					widget_image.SetBitmapImage(bitmap_image, FALSE);

					// update unread count if needed

					index->PreFetch();

					OpTreeModelItem::ItemData item_data(OpTreeModelItem::COLUMN_QUERY);
					item_data.column_query_data.column_text = &name;
					item_data.column_query_data.column = 5;
					RETURN_VOID_IF_ERROR(item->GetItemData(&item_data));

					BOOL bold = index->Contains(selected_message);
					OpINT32Vector child_vector;
					RETURN_VOID_IF_ERROR(g_m2_engine->GetIndexer()->GetChildren(index->GetId(), child_vector, TRUE));

					if (child_vector.GetCount() == 0)
					{
						OpInputAction* action = OP_NEW(OpInputAction,(OpInputAction::ACTION_ADD_TO_VIEW));
						if (!action)
							return;
						action->SetActionData(index->GetId());
						action->SetActionOperator(OpInputAction::OPERATOR_OR);

						OpInputAction* next_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_REMOVE_FROM_VIEW));
						if (!next_action)
						{
							OP_DELETE(action);
							return;
						}
						next_action->SetActionData(index->GetId());
						action->SetNextInputAction(next_action);
				
						AddMenuItem(index->GetName().CStr(), &widget_image, action, bold);
					}
					else
					{
						keep_adding = AddSubMenu(index->GetName().CStr(), &widget_image, "Internal Mail Folder", item_id, NULL, bold);
					}
				}
			}
		}
	}

	treeview->Delete();

#endif // M2_SUPPORT
}

/***********************************************************************************
**
**	AddMailCategoryMenuItems
**
***********************************************************************************/

void QuickMenu::AddMailCategoryMenuItems(UINT32 index_id)
{
#if defined(M2_SUPPORT)

	// Add these items: 
	// Read mail: for all items except "All messages"
	// Properties...: for all accounts except the feed account + filters
	// Subscribe to groups: for feeds, news, imap
	// ------ Separator
	OpString locale_text;
	if (index_id != IndexTypes::CATEGORY_MY_MAIL)
	{
		OpString action_text;
		OpWidgetImage action_image;
		g_languageManager->GetString(Str::M_MAIL_READ_CATEGORY, locale_text);
		action_text.AppendFormat(locale_text.CStr(), g_m2_engine->GetIndexById(index_id)->GetName().CStr());
		AddMenuItem(action_text.CStr(), NULL, OpInputAction::ACTION_READ_MAIL, index_id);

		UINT32 account_type = 0;
		
		if (index_id >= IndexTypes::FIRST_ACCOUNT && index_id <= IndexTypes::LAST_ACCOUNT)
			account_type = g_m2_engine->GetAccountById(index_id-IndexTypes::FIRST_ACCOUNT)->GetIncomingProtocol();


		if (account_type == AccountTypes::NEWS || account_type == AccountTypes::IMAP)
		{
			OpString parameter;
		
			switch (account_type)
			{
				case AccountTypes::NEWS:
					g_languageManager->GetString(Str::M_BROWSER_MAIL_MENU_NEWSGROUPS, action_text);
					action_image.SetImage("Account News");
					parameter.Set("nntp");
					break;
				case AccountTypes::IMAP:
					g_languageManager->GetString(Str::M_BROWSER_MAIL_MENU_IMAPFOLDERS, action_text);
					action_image.SetImage("Folder");
					parameter.Set("imap");
					break;
			}
			OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SUBSCRIBE_TO_GROUPS));
			if (!action)
				return;
			action->SetActionData(index_id-IndexTypes::FIRST_ACCOUNT);
			action->SetActionDataString(parameter.CStr());
			AddMenuItem(action_text.CStr(), &action_image, action);
		}
		else if (account_type == AccountTypes::RSS)
		{
			action_image.SetImage("Mail Newsfeeds");
			g_languageManager->GetString(Str::S_ADD_FEED, action_text);
			AddMenuItem(action_text.CStr(), &action_image, OpInputAction::ACTION_NEW_NEWSFEED, index_id);
		}
		if (index_id == IndexTypes::CATEGORY_MAILING_LISTS || index_id == IndexTypes::CATEGORY_LABELS || account_type == AccountTypes::RSS)
		{
			action_image.SetImage("New Folder");
			if (index_id == IndexTypes::CATEGORY_LABELS)
			{
				action_image.SetImage("Set label");
				g_languageManager->GetString(Str::M_INDEX_ITEM_POPUP_MENU_NEW_FILTER, action_text);
			}
			else 
			{
				g_languageManager->GetString(Str::SI_NEW_FOLDER_BUTTON_TEXT, action_text);
			}
			OpInputAction* action = OP_NEW(OpInputAction,(OpInputAction::ACTION_NEW_FOLDER));
			if (!action)
				return;
			action->SetActionData(index_id);
			if (index_id != IndexTypes::CATEGORY_LABELS)
				action->SetActionDataString(UNI_L("union"));

			AddMenuItem(action_text.CStr(), &action_image, action);
		}
		if (index_id == IndexTypes::CATEGORY_LABELS || (account_type != 0 && account_type != AccountTypes::RSS))
		{
			g_languageManager->GetString(Str::MI_IDM_HLITEM_PROPERTIES, action_text);
			action_image.SetImage("Edit properties");
			AddMenuItem(action_text.CStr(), &action_image, OpInputAction::ACTION_EDIT_PROPERTIES, index_id);
		}

		AddMenuSeparator();
	}
	g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_DIALOG_HEADER, locale_text);
	AddSubMenu(locale_text, NULL, "Internal Mail Category Customize Menu", index_id);
#endif // M2_SUPPORT
}

/***********************************************************************************
**
**	AddAccessPointsMenuItems
**
***********************************************************************************/

void QuickMenu::AddAccessPointsMenuItems(INT32 parent_id, OpInputAction::Action menu_action, BOOL include_parent_item)
{
#if defined(M2_SUPPORT)

	if( !m_application->ShowM2() )
	{
		return;
	}

	if (!m_platform_handle)
	{
		m_enabled_item_count++;
		return;
	}

	const char* submenu;
	switch (menu_action)
	{
	case OpInputAction::ACTION_MAIL_HIDE_INDEX:
		submenu = "Internal Mail Category Submenu";
		break;
	case OpInputAction::ACTION_READ_MAIL:
		submenu = "Internal Access Points";
		break;
	case OpInputAction::ACTION_SEARCH_IN_INDEX:
		submenu = "Internal Mail Search In";
		break;
	default:
			OP_ASSERT(FALSE);
			return;
	}

	if (parent_id == 0)
	{
		OpINT32Vector categories;
		OpStatus::Ignore(g_m2_engine->GetIndexer()->GetCategories(categories));

		for (UINT32 i=0; i < categories.GetCount(); i++)
		{
			OpINT32Vector children;
			const char* image = NULL;
			Image bitmap_image;
			OpString label_text;
			OpWidgetImage widget_image;

			Index* m2index = g_m2_engine->GetIndexById(categories.Get(i));
			if (m2index)
			{
				m2index->GetName(label_text);
				m2index->GetImages(image, bitmap_image);
				widget_image.SetImage(image);
				widget_image.SetBitmapImage(bitmap_image, FALSE);

				// avoid adding categories without children
				if (OpStatus::IsSuccess(g_m2_engine->GetIndexer()->GetChildren(categories.Get(i), children)) && children.GetCount() > 0)
				{
					AddSubMenu(label_text.CStr(), &widget_image, submenu, categories.Get(i), NULL, m2index->UnreadCount());
				}
			}
		}
		return;
	}

	if (include_parent_item && ((parent_id == IndexTypes::CATEGORY_MY_MAIL && menu_action == OpInputAction::ACTION_SEARCH_IN_INDEX) ||
		(menu_action != OpInputAction::ACTION_MAIL_HIDE_INDEX && parent_id != IndexTypes::CATEGORY_MY_MAIL) || 
		!g_m2_engine->GetIndexer()->IsCategory(parent_id)))
	{
		OpString this_view;
		Index* parent_index = g_m2_engine->GetIndexById(parent_id);

		const char* image = NULL;
		Image bitmap_image;
		OpWidgetImage widget_image;
		parent_index->GetImages(image, bitmap_image);

		widget_image.SetImage(image);
		widget_image.SetBitmapImage(bitmap_image, FALSE);

		if (menu_action == OpInputAction::ACTION_MAIL_HIDE_INDEX)
			g_languageManager->GetString(Str::M_MAIL_THIS_FOLDER, this_view);
		else
			parent_index->GetName(this_view);

		if (menu_action != OpInputAction::ACTION_MAIL_HIDE_INDEX)
		{
			AddMenuItem(this_view.CStr(), &widget_image, menu_action, parent_id);
		}
		else
		{
			OpInputAction* action = OP_NEW(OpInputAction,(OpInputAction::ACTION_MAIL_HIDE_INDEX));
			if (action)
			{
				action->SetActionData(parent_id);
				action->SetActionOperator(OpInputAction::OPERATOR_OR);

				OpInputAction* next_action = OP_NEW(OpInputAction,(OpInputAction::ACTION_MAIL_SHOW_INDEX));
				if (!next_action)
					return;
				next_action->SetActionData(parent_id);
				action->SetNextInputAction(next_action);
			}
			AddMenuItem(this_view.CStr(), &widget_image, action, FALSE);
		}

		AddMenuSeparator();
	}
	
	AccessModel access_model (parent_id ? parent_id : IndexTypes::CATEGORY_LABELS, g_m2_engine->GetIndexer());
	access_model.ShowHiddenIndexes();
	access_model.Init();

	OpTreeView* treeview;
	if (OpStatus::IsError(OpTreeView::Construct(&treeview)))
		return;

	treeview->SetTreeModel(&access_model, 0);
	treeview->OpenAllItems(TRUE);

	int count = treeview->GetLineCount();
	BOOL keep_adding = TRUE;

	for (int i = 0; i < count && keep_adding; i++)
	{
		INT32 item_pos = treeview->GetItemByLine(i);

		OpTreeModelItem* item = treeview->GetItemByPosition(item_pos);

		if (item)
		{
			INT32 parent = 0;

			OpTreeModelItem* parent_item = treeview->GetParentByPosition(item_pos);

			if (parent_item)
			{
				parent = parent_item->GetID();

				if (parent == 0)
				{
					parent -= treeview->GetParent(item_pos) + 2;
				}
			}

			if (parent && (parent_id != parent &&
				(parent_id != 0 || (parent >= IndexTypes::FIRST_FOLDER && parent < IndexTypes::LAST_FOLDER))))
			{
				continue;
			}

			INT32 item_id = item->GetID();

			if (item_id == 0)
			{
				item_id -= item_pos + 2;
			}

			BOOL has_children = FALSE;
			OpString name;
			OpWidgetImage widget_image;

			if (item->GetType() == OpTypedObject::INDEX_TYPE)
			{
				Index* index = g_m2_engine->GetIndexById(item->GetID());
				if (index)
				{
					const char* image = NULL;
					Image bitmap_image;
					index->GetImages(image, bitmap_image);

					widget_image.SetImage(image);
					widget_image.SetBitmapImage(bitmap_image, FALSE);

					OpINT32Vector grand_child_vector;
					RETURN_VOID_IF_ERROR(g_m2_engine->GetIndexer()->GetChildren(index->GetId(), grand_child_vector, TRUE));

					has_children = grand_child_vector.GetCount() > 0;
					// update unread count if needed

					index->PreFetch();
				}
			}

			OpTreeModelItem::ItemData item_data(OpTreeModelItem::COLUMN_QUERY);
			item_data.column_query_data.column_text = &name;
			item_data.column_query_data.column = 5;
			item->GetItemData(&item_data);

			BOOL bold = item_data.flags & OpTreeModelItem::FLAG_BOLD;

			if (menu_action != OpInputAction::ACTION_MAIL_HIDE_INDEX)
			{
				if (has_children)
				{
					keep_adding = AddSubMenu(name.CStr(), &widget_image, submenu, item->GetID(), NULL, bold);
				}
				else
				{
					keep_adding = AddMenuItem(name.CStr(), &widget_image, menu_action, item->GetID(), bold);
				}
			}
			else
			{
				widget_image.SetImage("Checkmark");
				// Hide index, index_id, NULL | Show index, index_id, NULL
				OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_MAIL_HIDE_INDEX));
				if (action)
				{
					action->SetActionData(item->GetID());
					action->SetActionText(name.CStr());
					action->SetActionOperator(OpInputAction::OPERATOR_OR);

					OpInputAction* next_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_MAIL_SHOW_INDEX));
					if (!next_action)
						return;
					next_action->SetActionData(item->GetID());
					next_action->SetActionText(name.CStr());

					action->SetNextInputAction(next_action);
				}
				
				if (has_children)
				{
					keep_adding = AddSubMenu(name.CStr(), &widget_image, submenu, item->GetID(), action, bold);
				}
				else
				{
					keep_adding = AddMenuItem(name.CStr(), &widget_image, action, bold);
				}
			}
		}
	}
	OP_DELETE(treeview);
#endif // M2_SUPPORT
}

/***********************************************************************************
**
**	AddColumnMenuItems
**
***********************************************************************************/

void QuickMenu::AddColumnMenuItems()
{
	OpTreeView* tree_view = (OpTreeView*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_TREEVIEW, m_menu_input_context);

	if (!tree_view)
		return;

	INT32 column_count = tree_view->GetColumnCount();

	for (INT32 i = 0; i < column_count; i++)
	{
		if (tree_view->GetColumnVisibility(i))
		{
			OpString name;

			tree_view->GetColumnName(i, name);

			OpInputAction* action = OpInputAction::CreateToggleAction(OpInputAction::ACTION_SHOW_COLUMN, OpInputAction::ACTION_HIDE_COLUMN, i);

			AddMenuItem(name.CStr(), NULL, action);
		}
	}
}

/***********************************************************************************
**
**	AddToolbarExtenderMenuItems
**
***********************************************************************************/

void QuickMenu::AddToolbarExtenderMenuItems()
{
	Hotlist* hotlist = (Hotlist*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_HOTLIST, m_menu_input_context);
	HotlistSelector* panel_selector = (HotlistSelector*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_PANEL_SELECTOR, m_menu_input_context);
	OpToolbar* toolbar = (OpToolbar*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_TOOLBAR, m_menu_input_context);

	if (!toolbar)
		return;

	INT32 widget_count = toolbar->GetWidgetCount();

	for (INT32 i = 0; i < widget_count; i++)
	{
		OpWidget* widget = toolbar->GetWidget(i);

		if (!widget || !widget->IsAllowedInExtenderMenu() || (widget->IsVisible()))
			continue;

		if (panel_selector && hotlist)
		{
			OpString name;
			hotlist->GetPanelText(i, name, Hotlist::PANEL_TEXT_NAME);
			AddMenuItem(name.CStr(), hotlist->GetPanelImage(i), OpInputAction::ACTION_FOCUS_PANEL, i);
			continue;
		}

		OpInputAction* action = widget->GetAction();

		if (!action)
			continue;

		OpString name;
		widget->GetText(name);

		if (action->GetAction() == OpInputAction::ACTION_SHOW_POPUP_MENU)
		{
			OpString8 menu;
			menu.Set(action->GetActionDataString());
			AddSubMenu(name.CStr(), widget->GetForegroundSkin(), menu.CStr(), action->GetActionData());
		}
		else
		{
			AddMenuItem(name.CStr(), widget->GetForegroundSkin(), OpInputAction::CopyInputActions(action));
		}
	}
}


/***********************************************************************************
**
**  AddPanelListMenuItems
**
***********************************************************************************/

void QuickMenu::AddPanelListMenuItems()
{
	Hotlist* hotlist = (Hotlist*) g_input_manager->GetTypedObject(OpTypedObject::WIDGET_TYPE_HOTLIST, m_menu_input_context);

	if (!hotlist)
	return;

	INT32 count = hotlist->GetPanelCount();

	for (INT32 i = 0; i < count; i++)
	{
		OpString name;
		hotlist->GetPanelText(i, name, Hotlist::PANEL_TEXT_NAME);

		AddMenuItem(name.CStr(), hotlist->GetPanelImage(i) ,OpInputAction::ACTION_FOCUS_PANEL, i);
	}
}


/***********************************************************************************
**
**	AddPopularListItems
**
***********************************************************************************/

void QuickMenu::AddPopularListItems()
{
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

	if (!m_platform_handle && history_model)
	{
		if (history_model->GetNumItems() > 0)
			m_enabled_item_count++;
		return;
	}

	OpVector<HistoryModelPage> topTen;

	if(history_model) history_model->GetTopItems(topTen, 10);

	INT32 count = topTen.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		OpString address;
		OpString title;

		HistoryModelPage* item = topTen.Get(i);
		item->GetAddress(address);
		item->GetTitle(title);

		OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_GO_TO_PAGE));
		if (!action)
			return;
		action->SetActionData(1);
		action->SetActionDataString(address.CStr());

		OpString name;

		uni_char number_string[16];
		uni_itoa((i+1), number_string, 10);
		name.Append(number_string);
		name.Append(UNI_L(". "));

		name.Append(title.CStr());

		OpWidgetImage widget_image;

		widget_image.SetImage("Window Document Icon");

		Image img = g_favicon_manager->Get(address.CStr());
		widget_image.SetBitmapImage(img, FALSE);

		AddMenuItem(name.CStr(), &widget_image, action);
	}
}

/***********************************************************************************
**
**	AddHistoryPopupItems
**
***********************************************************************************/

void QuickMenu::AddHistoryPopupItems(DocumentDesktopWindow* desktop_window, bool forward, bool fast)
{
	if (!desktop_window)
		return;

	OpString buf;
	if (!buf.Reserve(512))
		return;

	uni_char *menu_title = buf.CStr();

	OpAutoVector<DocumentHistoryInformation> history_list;
	RETURN_VOID_IF_ERROR(desktop_window->GetHistoryList(history_list, forward, fast));

	for (UINT32 i = 0 ; i < history_list.GetCount(); i++)
	{
		DocumentHistoryInformation* history_info = history_list.Get(i);

		OpWidgetImage widget_image;
		Image img = g_favicon_manager->Get(history_info->url, TRUE);
		if (!img.IsEmpty())
			widget_image.SetBitmapImage(img, FALSE);

		EscapeAmpersand(menu_title, 512, history_info->title.HasContent() ? history_info->title.CStr() : history_info->url.CStr());

		INTPTR pos = fast && forward ? -1 - (int)i : history_info->number;
		AddMenuItem(menu_title, &widget_image, OpInputAction::ACTION_GO_TO_HISTORY, pos);
	}
}

/***********************************************************************************
**
**	AddExternalMenuItems
**
***********************************************************************************/

void QuickMenu::AddExternalMenuItems(const char* menu_name, INTPTR menu_value)
{
	PrefsSection *section = m_menu_handler->GetMenuSection(menu_name);
	if (NULL == section)
	{
		section = ReadMenuSection(menu_name);
	}

	if (!section)
		return;

	//TODO similar parsing is done in three places (toolbar, opinputaction and here).
	//It probably needs some common code (huibk)
	for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
	{
		OpWidgetImage	widget_image;
		MenuType		menu_type;
		OpString		item_name, default_language_item_name;
		OpString8		sub_menu_name;
		INTPTR			sub_menu_value = 0;
		BOOL			ghost_item;

		OpLineParser	line(entry->Key());

		if (ParseMenuEntry(*m_application, &line, item_name,
				default_language_item_name, sub_menu_name, sub_menu_value,
				menu_value, menu_type, ghost_item, widget_image))
		{
			if (menu_type == MENU_INCLUDE)
			{
				AddMenuItems(sub_menu_name.CStr(), sub_menu_value == -2 ? menu_value : sub_menu_value);
				continue;
			}

			OpAutoPtr<OpInputAction> first_input_action;
			OpInputAction* input_action = 0;
			if (menu_type == MENU_ITEM || menu_type == MENU_SUBMENU)
			{
				if (OpStatus::IsError(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action, menu_value)))
				{
					OP_DELETE(input_action);
					input_action = 0;
				}

				// Save the first item
				first_input_action.reset(input_action);
				OpString first_item_name;
				if (OpStatus::IsError(first_item_name.Set(item_name)))
					return;

				OpInputAction *prev_input_action = NULL;

				// Find the first enabled action in the action list
				// This currently won't work for toggle menu items with a checkmark next to them in the menu
				while (input_action)
				{
					// No more actions so exit the loop or an action we don't handle in this loop, break out (we only handle OR actions here)
					if (input_action->GetActionOperator() == OpInputAction::OPERATOR_NONE || input_action->GetActionOperator() != OpInputAction::OPERATOR_OR)
						break;

					INT32 state = g_input_manager->GetActionState(input_action, m_menu_input_context);

					// Only look for the next action if this one is specifically disabled
					if (!(state & OpInputAction::STATE_DISABLED))
						break;

					prev_input_action = input_action;

					// Get the next item
					input_action = input_action->GetActionOperator() != OpInputAction::OPERATOR_PLUS ? input_action->GetNextInputAction() : NULL;
					// Update the menu item string to the new action's data, if there is a new string
					if (input_action && input_action->GetActionText() && input_action->GetActionText()[0] &&
						OpStatus::IsError(item_name.Set(input_action->GetActionText())))
						return;
				}
				// If the above parsing managed to set the action to NULL then re-choose the
				// original action as the action for this menu item
				if (input_action == NULL)
				{
					input_action = first_input_action.get();
					if (OpStatus::IsError(item_name.Set(first_item_name)))
						return;
				}
				else if (prev_input_action)
				{
					prev_input_action->SetNextInputAction(NULL);
					first_input_action.reset();
				}
			}

			first_input_action.release();

			if (!AddMenuItem(menu_type == MENU_SEPARATOR, item_name.CStr(), widget_image.HasContent() ? &widget_image : NULL, sub_menu_name.CStr(), sub_menu_value == -2 ? menu_value : sub_menu_value, input_action, FALSE, default_language_item_name.CStr(), menu_type == MENU_BREAK, FALSE, ghost_item))
				break;
		}
	}

}

/***********************************************************************************
**
**	AddMenuItem
**
***********************************************************************************/

BOOL QuickMenu::AddMenuItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const char* sub_menu_name, INTPTR sub_menu_value, OpInputAction* input_action, BOOL bold, const uni_char* default_language_item_name, BOOL menu_break, BOOL force_disabled, BOOL ghost_item)
{
	OpAutoPtr<OpInputAction> input_action_holder(input_action);
	m_current_index++;

	if (m_current_index <= m_start_at_index)
		return TRUE;

	if (m_grown_too_large == YES)
		return FALSE;

	if (m_platform_handle && m_grown_too_large == MAYBE)
	{
		g_desktop_popup_menu_handler->OnAddMenuItem(m_platform_handle, TRUE, NULL, NULL, NULL, NULL, 0, 0, FALSE, FALSE, FALSE, FALSE, NULL);

		OpString more_items;
		g_languageManager->GetString(Str::M_MORE_ITEMS, more_items);

		OpString more_items_new;
		m_shortcut_finder.CreateMenuString(more_items.CStr(), more_items_new, default_language_item_name);

		g_desktop_popup_menu_handler->OnAddMenuItem(m_platform_handle, FALSE, more_items_new.CStr(), NULL, NULL, m_initial_menu_name, m_initial_menu_value, m_current_index - 1, FALSE, FALSE, FALSE, FALSE, NULL);

		m_grown_too_large = YES;

		return FALSE;
	}

	BOOL need_to_wrap_soon = FALSE;

	if (seperator)
	{
		if (m_platform_handle)
		{
			need_to_wrap_soon = g_desktop_popup_menu_handler->OnAddMenuItem(m_platform_handle, TRUE, NULL, NULL, NULL, NULL, 0, 0, FALSE, FALSE, FALSE, FALSE, NULL);
		}
	}
	else if(menu_break)
	{
		if (m_platform_handle)
		{
			need_to_wrap_soon = g_desktop_popup_menu_handler->OnAddBreakMenuItem();
		}
	}
	else
	{
		OpWidgetImage widget_image;
		OpString shortcut_string;
		BOOL disabled = force_disabled;
		BOOL checked = FALSE;
		BOOL selected = FALSE;

		if (image)
		{
			widget_image.SetWidgetImage(image);
		}

		if (input_action)
		{
			INT32 state;

			state = g_input_manager->GetActionState(input_action, m_menu_input_context);

			if (sub_menu_name)
			{
				// Introduced because of DSK-142522
				//
				// Special handling for submenu entries that have one or more actions. These actions are only there
				// to provide visual effects in the parent menu entry that triggers the submenu. Shortcuts, bullets etc.
				// There are some limitations. 
				// 1. We only show bullets, no checkboxes.
				// 2. Some sub menu items can be disabled and others enabled. One enabled item will overrule all disabled

				selected = state & OpInputAction::STATE_SELECTED;
				if ((state&OpInputAction::STATE_ENABLED) && (state&OpInputAction::STATE_DISABLED))
					state &= ~OpInputAction::STATE_DISABLED;
			}
			else
			{
				checked = state & OpInputAction::STATE_SELECTED && input_action->GetActionOperator() != OpInputAction::OPERATOR_AND && input_action->GetNextInputAction();
				selected = state & OpInputAction::STATE_SELECTED && !input_action->GetNextInputAction();
			}

			OpInputAction* current_action = input_action;
			if (!force_disabled)
			{
				disabled = state & OpInputAction::STATE_DISABLED && !(state & OpInputAction::STATE_ENABLED);

				while (current_action->GetNextInputAction() && disabled)
				{
					current_action = current_action->GetNextInputAction();
					state = g_input_manager->GetActionState(current_action, m_menu_input_context);
					disabled = state & OpInputAction::STATE_DISABLED && !(state & OpInputAction::STATE_ENABLED);
				}
			}

			if( !KioskManager::GetInstance()->GetNoKeys() )
			{
				// OpInputAction* action_copy = OpInputAction::CopyInputActions(current_action, OpInputAction::OPERATOR_PLUS | OpInputAction::OPERATOR_NEXT | OpInputAction::OPERATOR_OR);

				g_input_manager->GetShortcutStringFromAction(current_action, shortcut_string, m_menu_input_context);
				// OpInputAction::DeleteInputActions(action_copy);
			}

			if (selected)
			{
				if (image)
					bold = TRUE;
				else
					widget_image.SetImage("Bullet");
			}
			else if (checked)
			{
				widget_image.SetImage("Checkmark");
			}
			else if (!image)
			{
				if (input_action->GetActionData() == SEARCH_TYPE_DEFAULT &&
					input_action->GetAction() == OpInputAction::ACTION_HOTCLICK_SEARCH)
				{
					SearchTemplate* search = NULL;
					const uni_char* search_guid = input_action->GetActionDataString();

					if (search_guid)
						search = g_searchEngineManager->GetByUniqueGUID(search_guid);

					if (search)
					{
						Image search_icon = search->GetIcon();

						if (search_icon.IsEmpty())
							widget_image.SetImage(current_action->GetActionImage());
						else
							widget_image.SetBitmapImage(search_icon);
					}
				} else
					widget_image.SetImage(current_action->GetActionImage());
			}
		}

		// make copy of item name and strip at newline

		OpString item_name_copy;

		m_shortcut_finder.CreateMenuString(item_name, item_name_copy, default_language_item_name);

		uni_char* c = item_name_copy.CStr();

		while (c && *c)
		{
			if (*c == '\n' || *c == '\r')
			{
				*c = 0;
				item_name_copy.Append(UNI_L("..."));
				break;
			}
			c++;
		}

		// if this is a submenu, we should check if it has any enabled sub

		if (sub_menu_name && !force_disabled)
		{
			disabled = !IsMenuEnabled(m_global_context, sub_menu_name,
					sub_menu_value, m_menu_input_context);
		}

		if (m_platform_handle && !(ghost_item && disabled) ) // don't add ghost item if disabled
		{
			need_to_wrap_soon = g_desktop_popup_menu_handler->OnAddMenuItem(m_platform_handle, FALSE, item_name_copy.CStr(), &widget_image, shortcut_string.CStr(), sub_menu_name, sub_menu_value, 0, checked, selected, disabled, bold, input_action_holder.release());
		}

		if (!disabled)
		{
			m_enabled_item_count++;
		}
	}

	if (need_to_wrap_soon)
	{
		m_grown_too_large = MAYBE;
	}

	if (!m_platform_handle && m_enabled_item_count > 0)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL QuickMenu::AddMenuItem(const uni_char* item_name, OpWidgetImage* image, OpInputAction* input_action, BOOL bold, BOOL menu_break)
{
	return AddMenuItem(FALSE, item_name, image, NULL, 0, input_action, bold, NULL, menu_break);
}

BOOL QuickMenu::AddMenuItem(const uni_char* item_name, OpWidgetImage* image, OpInputAction::Action action, INTPTR action_data, BOOL bold, BOOL menu_break)
{
	OpInputAction* input_action = OP_NEW(OpInputAction, (action));
	if (!input_action)
		return FALSE;
	input_action->SetActionData(action_data);
	return AddMenuItem(FALSE, item_name, image, NULL, 0, input_action, bold, NULL, menu_break);
}

BOOL QuickMenu::AddSubMenu(const uni_char* item_name, OpWidgetImage* image, const char* sub_menu_name, INTPTR sub_menu_value, OpInputAction* input_action, BOOL bold)
{
	return AddMenuItem(FALSE, item_name, image, sub_menu_name, sub_menu_value, input_action, bold, NULL);
}

BOOL QuickMenu::AddMenuSeparator()
{
	return AddMenuItem(TRUE, NULL, NULL, NULL, 0, NULL);
}

void QuickMenu::OnPopupMessageMenuItemClick(OpInputAction* action)
{
	OP_ASSERT(action);
	DesktopMenuContext* context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
	OP_ASSERT(context);
	UINT32 menu_item_id = static_cast<UINT32>(uni_atoi(action->GetActionDataString()));
	context->GetWindowCommander()->OnMenuItemClicked(context->GetDocumentContext(), menu_item_id);
}

OP_STATUS QuickMenu::AddPopupMessageMenuItems(DesktopMenuContext* context, const char* menu_name, INTPTR menu_value, BOOL is_sub_menu)
{
	OP_ASSERT(context);
	const OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItemList* menu_items_list = NULL;
	if (!is_sub_menu)
		menu_items_list = &context->GetPopupMessage()->GetDocumentMenuItemList();
	else if (menu_value && menu_name && StringUtils::StartsWith<DOC_CONTEXT_MENU_NAME_PREFIX_SIZE>(menu_name, DOC_CONTEXT_MENU_NAME_PREFIX))
		menu_items_list = reinterpret_cast<OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItemList*>(menu_value);
	else
		return OpStatus::OK;
	return AddPopupMessageMenuItems(context, menu_items_list);
}

OP_STATUS QuickMenu::AddPopupMessageMenuItems(DesktopMenuContext* context, const OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItemList* menu_items_list)
{
	OP_ASSERT(context);
	OP_ASSERT(menu_items_list);
	const OpProtobufMessageVector<OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItem> & menu_items = menu_items_list->GetMenuItems();
	for (UINT32 i = 0; i < menu_items.GetCount(); i++)
	{
		OpWindowcommanderMessages_MessageSet::PopupMenuRequest_MenuItem* menu_item = menu_items.Get(i);

		OpAutoPtr<OpWidgetImage> menu_item_icon_widget;
		OpBitmap* menu_item_icon_bitmap_from_core;

		// This method will always return OK if the resource is in the extension, even if it's not a valid bitmap, so make sure
		// we also check if a bitmap is returned. 
		if (OpStatus::IsSuccess(context->GetWindowCommander()->GetMenuItemIcon(menu_item->GetIconId(), menu_item_icon_bitmap_from_core)) 
			&& menu_item_icon_bitmap_from_core)
		{
			// copy bitmap
			OpBitmap* menu_item_icon_bitmap;
			RETURN_IF_ERROR(OpBitmap::Create(&menu_item_icon_bitmap,
				menu_item_icon_bitmap_from_core->Width(), menu_item_icon_bitmap_from_core->Height()));
			OpAutoPtr<OpBitmap> menu_item_icon_bitmap_auto_ptr(menu_item_icon_bitmap);
			RETURN_IF_ERROR(AnimatedImageContent::CopyBitmap(menu_item_icon_bitmap, menu_item_icon_bitmap_from_core));

			menu_item_icon_widget = OP_NEW(OpWidgetImage, ());
			RETURN_OOM_IF_NULL(menu_item_icon_widget.get());

			Image menu_item_icon_image = imgManager->GetImage(menu_item_icon_bitmap);
			menu_item_icon_bitmap_auto_ptr.release();
			menu_item_icon_widget->SetBitmapImage(menu_item_icon_image);
			menu_item_icon_image.DecVisible(NULL);
		}

		const uni_char* menu_item_name = menu_item->GetLabel().Data(true);

		if (!menu_item->HasSubMenuItemList() && (menu_item->GetId() == 0))
		{
			AddMenuSeparator();
		}
		else if (!menu_item->HasSubMenuItemList())
		{
			OpAutoPtr<OpInputAction> menu_item_click_action(OP_NEW(OpInputAction, (OpInputAction::ACTION_HANDLE_DOC_CONTEXT_MENU_CLICK)));
			RETURN_OOM_IF_NULL(menu_item_click_action.get());
			menu_item_click_action->SetActionData(reinterpret_cast<INTPTR>(context));
			OpString menu_item_id_str;
			RETURN_IF_ERROR(menu_item_id_str.AppendFormat("%d", menu_item->GetId()));
			menu_item_click_action->SetActionDataString(menu_item_id_str.CStr());

			AddMenuItem(menu_item_name, menu_item_icon_widget.get(), menu_item_click_action.release());
		}
		else
		{
			OpString8 sub_menu_name;
			RETURN_IF_ERROR(sub_menu_name.AppendFormat("%s-%d", DOC_CONTEXT_MENU_NAME_PREFIX, menu_item->GetId()));
			INTPTR sub_menu_value = reinterpret_cast<INTPTR>(menu_item->GetSubMenuItemList());

			AddSubMenu(menu_item_name, menu_item_icon_widget.get(), sub_menu_name.CStr(), sub_menu_value);
		}

	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

BOOL BookmarkMenuContext::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_GET_ACTION_STATE)
	{
		OpInputAction* child_action = action->GetChildAction();

		HotlistModelItem* item = g_hotlist_manager->GetItemByID(m_bookmark_id);
		switch (child_action->GetAction())
		{
			case OpInputAction::ACTION_DELETE:
			case OpInputAction::ACTION_CUT:
			{
				child_action->SetEnabled(item && item->GetIsDeletable());
				return TRUE;
			}
			case OpInputAction::ACTION_COPY:
			{
				child_action->SetEnabled(item && !item->GetIsTrashFolder());
				return TRUE;
			}
			case OpInputAction::ACTION_PASTE:
			{
				child_action->SetEnabled(g_desktop_bookmark_manager->GetClipboard()->GetCount());
				return TRUE;
			}
			case OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR:
			case OpInputAction::ACTION_HIDE_FROM_PERSONAL_BAR:
			{
				HotlistModelItem* item = g_hotlist_manager->GetItemByID(m_bookmark_id);
				if (item && !item->IsSeparator())
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR, g_hotlist_manager->IsOnPersonalBar(item));
				else
					child_action->SetEnabled(FALSE);

				return TRUE;
			}
		}

		while (child_action)
		{
			if (NeedActionData(child_action->GetAction()))
				child_action->SetActionData(m_bookmark_id);
			child_action = child_action->GetNextInputAction();
		}
		return FALSE;
	}
	else
	{
		OpINT32Vector list;
		list.Add(m_bookmark_id);	
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_DELETE:
			{
				g_desktop_bookmark_manager->Delete(list);
				return TRUE;
			}
			case OpInputAction::ACTION_CUT:
			{
				g_desktop_bookmark_manager->CutItems(list);
				return TRUE;
			}
			case OpInputAction::ACTION_COPY:
			{
				g_desktop_bookmark_manager->CopyItems(list);	
				return TRUE;
			}
			case OpInputAction::ACTION_PASTE:
			{
				HotlistModelItem* item = g_hotlist_manager->GetItemByID(m_bookmark_id);
				g_desktop_bookmark_manager->PasteItems(item);
				return TRUE;
			}
			case OpInputAction::ACTION_EDIT_PROPERTIES:
			{
				g_desktop_bookmark_manager->EditItem(m_bookmark_id, NULL);
				return TRUE;
			}
			case OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR:
			case OpInputAction::ACTION_HIDE_FROM_PERSONAL_BAR:
			{
				return g_hotlist_manager->ShowOnPersonalBar( m_bookmark_id, action->GetAction() == OpInputAction::ACTION_SHOW_ON_PERSONAL_BAR );
			}
		}

		while (action)
		{
			if (NeedActionData(action->GetAction()))
				action->SetActionData(m_bookmark_id);
			action = action->GetNextInputAction();
		}
	}

	return FALSE; 
}

BOOL BookmarkMenuContext::NeedActionData(OpInputAction::Action action)
{
	return TRUE;
}
