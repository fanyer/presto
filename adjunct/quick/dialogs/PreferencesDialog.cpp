/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file PreferencesDialog.cpp
 *
 * This implementation checks for existence of widgets before saving values to prefs.
 * This may not sound really needed, but it makes it possible to have multiple layouts
 * of the preferences dialog without recompiling anything, so we can have simple dialogs
 * on some platforms and more complete ones on others. *
 */

#include "core/pch.h"

#include "adjunct/quick/dialogs/PreferencesDialog.h"

#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES

#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/prefs/CorePrefsHelpers.h"
#include "adjunct/desktop_util/prefs/WidgetPrefs.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/controller/CustomWebFeedController.h"
#include "adjunct/quick/controller/NameCompletionController.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/dialogs/CertificateManagerDialog.h"
#include "adjunct/quick/dialogs/ChangeMasterPasswordDialog.h"
#include "adjunct/quick/dialogs/SecurityPasswordDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/dialogs/FileTypeDialog.h"
#include "adjunct/quick/dialogs/InputManagerDialog.h"
#include "adjunct/quick/dialogs/InternationalFontsDialog.h"
#include "adjunct/quick/dialogs/LinkStyleDialog.h"
#include "adjunct/quick/dialogs/ProxyServersDialog.h"
#include "adjunct/quick/dialogs/SecurityProtocolsDialog.h"
#include "adjunct/quick/dialogs/SelectFontDialog.h"
#include "adjunct/quick/dialogs/ServerManagerDialog.h"
#include "adjunct/quick/dialogs/ServerPropertiesDialog.h"
#include "adjunct/quick/dialogs/SetupApplyDialog.h"
#include "adjunct/quick/dialogs/TrustedProtocolDialog.h"
#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick_toolkit/widgets/DesktopOpDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/dialogs/ServerWhiteListDialog.h"
#include "adjunct/quick/managers/DownloadManagerManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"


#include "modules/cookies/url_cm.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/display/styl_man.h"
#include "modules/display/color.h"
#include "modules/history/direct_history.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#ifdef DOM_GEOLOCATION_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#endif // DOM_GEOLOCATION_SUPPORT

#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/accessors/lnglight.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/url/uamanager/ua.h"
#include "modules/viewers/viewers.h"
#include "modules/style/css.h"
#include "modules/util/filefun.h"
#include "modules/util/handy.h"
#include "modules/util/gen_math.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/database/opdatabasemanager.h"
#include "modules/database/opdatabase.h"

#include "adjunct/quick/data_types/MasterPasswordLifetime.h"

#ifdef M2_SUPPORT
# include "adjunct/m2_ui/dialogs/AccountManagerDialog.h"
#endif // M2_SUPPORT

#ifdef _SSL_SUPPORT_
# include "modules/libssl/ssldlg.h"
# include "modules/libssl/sslopt.h"
# include "modules/libssl/sslrand.h"
# include "modules/libssl/sslv3.h"
# include "modules/libssl/options/sslopt.h"
#endif

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
# include "modules/encodings/utility/charsetnames.h"
#endif

#ifdef MSWIN
# include "platforms/windows/windows_ui/winshell.h"
# include "platforms/windows/win_handy.h"
#endif

#ifdef _MACINTOSH_
extern Boolean MacGetDefaultApplicationPath(IN const uni_char* url, OUT OpString& appName, OUT OpString& appPath);
#endif

// Add more platforms here when they have implemented the needed functions.
// Ideally, this should probably be part of PrefsManager or similar.

#if defined(MSWIN)
extern BOOL IsSystemWin2000orXP();
#include "platforms/windows/installer/OperaInstaller.h"
#endif

#if defined(_UNIX_DESKTOP_)
# include "platforms/quix/dialogs/FileHandlerDialog.h"
# include "platforms/quix/dialogs/PluginPathDialog.h"
# include "platforms/quix/dialogs/PluginSetupDialog.h"
#endif // _UNIX_DESKTOP_

extern const float tsep_scale;
extern const float lsep_scale;

#ifdef WAND_SUPPORT
extern BOOL InputCorrectPassword(OpWindow *parent_window);
#endif // WAND_SUPPORT

const int	CUSTOM_KEY								= 0xff;

#ifdef _MACINTOSH_
# define POSITION_OF_NORMAL_FONT_IN_LIST 8
#else
# define POSITION_OF_NORMAL_FONT_IN_LIST 9
#endif // _MACINTOSH_

// We don't have language folder for every language code.
// these rules are used to choose a language folder for those language 
const struct langcodeMap
{
	const char* languageCode;
	const char* languageFileName;
} langcodeMap[] = 
{
	{"es","es-la"},
	{"zh","zh-cn"},
	{"zh-hk","zh-tw"},
	{"no-bok","nb"},
	{"no","nb"},
	{"no-nyn","nn"}
	
};

BOOL IsValidLanguageFileVersion(const OpString& path, DesktopWindow* parent_window, BOOL show_warning)
{
	OpFile lng_file;
	OpString db_version;
	OpStatus::Ignore(lng_file.Construct(path.CStr()));
	LangAccessorLight accessor;
	accessor.LoadL(&lng_file, NULL);
	accessor.GetDatabaseVersion(db_version);
	int value = -1;

	if (!db_version.IsEmpty())
	{
		uni_sscanf( db_version.CStr(), UNI_L("%d"), &value );
	}
	if (value < 811)
	{
		if (show_warning)
		{
			OpString message;
			g_languageManager->GetString(Str::D_LANGUAGE_FILE_INCOMPATIBLE, message);
			SimpleDialogController* controller = OP_NEW(SimpleDialogController,
						(SimpleDialogController::TYPE_CLOSE, SimpleDialogController::IMAGE_ERROR,
						 WINDOW_NAME_INCOMPATIBLE_LANGUAGE_FILE,message.CStr(), UNI_L("Opera")));
			
			ShowDialog(controller, g_global_ui_context,parent_window);
		}
		return FALSE;
	}

	return TRUE;
}

BOOL ParseRegionSuffix(const OpString& language_code,OpString& language_name) // convert "en-US" into "English (US)"
{
	LanguageCodeInfo language_info;
	// See if it has a country/region suffix
	if (language_code.Length() > 4 && language_code.CStr()[2] == '-')
	{
		OpString language_code_without_country;
		language_code_without_country.Set(language_code.CStr(), 2);
		language_info.GetNameByCode(language_code_without_country, language_name);
		if (!language_name.IsEmpty())
		{
			if (language_name.Compare(language_code_without_country) != 0)
			{
				language_name.AppendFormat(UNI_L(" (%s)"), language_code.CStr() + 3);
			}
			return TRUE;
		}
	}

	return FALSE;
}



int SplitAcceptLanguage(const uni_char **str_lang, OpString &language_code_only)
{
	const uni_char *str_lang_begin, *str_lang_end;
	str_lang_end = str_lang_begin = *str_lang;

	while (1)
	{
		if (!*str_lang_end)
		{
			*str_lang = NULL;
			break;
		}
		if (*str_lang_end == ',')
		{
			*str_lang = str_lang_end+1;
			break;
		}
		str_lang_end++;
	}
	const uni_char* str_semi = str_lang_end;

	// Remove any q-value from the string
	while (str_semi > str_lang_begin)
	{
		str_semi--;
		if (*str_semi == ';')
		{
			str_lang_end = str_semi;
			break;
		}
	}

	language_code_only.Set(str_lang_begin, str_lang_end - str_lang_begin);
	StrTrimChars(language_code_only.CStr(), UNI_L("\t\n "), OPSTR_TRIM_RIGHT_AND_LEFT);

	return language_code_only.Length();
}


class RemoveSearchDialogController : public SimpleDialogController
{
public:
	RemoveSearchDialogController()
		: SimpleDialogController(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_WARNING,
			WINDOW_NAME_DELETE_SEARCH_WARNING, Str::D_DELETE_SEARCH_WARNING, Str::D_DELETE_SEARCH_LABEL)
	{}

	OP_STATUS Init(SearchTemplate* item) { return m_guid.Set(item->GetUniqueGUID()); }

	virtual void OnOk()
	{
		SearchTemplate* item = g_searchEngineManager->GetByUniqueGUID(m_guid);
		if (item && OpStatus::IsSuccess(g_searchEngineManager->RemoveItem(item)))
		{
			item->SetPersonalbarPos(-1);
			OpStatus::Ignore(g_searchEngineManager->Write());
		}
	}

private:
	OpString m_guid;
};


/***********************************************************************************
**
**	NewPreferencesDialog
**
***********************************************************************************/

INT32 NewPreferencesDialog::s_last_selected_advanced_prefs = 0;
INT32 NewPreferencesDialog::s_last_selected_prefs_page = 0;

NewPreferencesDialog::NewPreferencesDialog() :
	m_sounds_model(2),
	m_fonts_model(2),
	m_file_types_model(2),
	m_toolbars_model(1),
	m_menus_model(1),
	m_mice_model(1),
	m_keyboards_model(1),
	m_trusted_protocol_model(2),
	m_local_storage_model(2),
	m_trusted_protocol_list(NULL),
	m_trusted_application_list(NULL),
	m_initial_advanced_page_id(-1),
	m_language_changed(FALSE),
	m_recreate_dialog(FALSE),
	m_chooser(0)
{
#ifdef WEB_TURBO_MODE
	TRAPD(err, g_pcui->RegisterListenerL(this));
#endif
}

NewPreferencesDialog::~NewPreferencesDialog()
{
	g_main_message_handler->UnsetCallBacks(this);

	OP_DELETE(m_trusted_application_list);
	OP_DELETE(m_trusted_protocol_list);

	OP_DELETE(m_chooser);
//	ServerManager::ReleaseServerManager();
#ifdef WEB_TURBO_MODE
	g_pcui->UnregisterListener(this);
#endif
	if (g_wand_manager)
		g_wand_manager->RemoveListener(this);
}

OP_STATUS NewPreferencesDialog::Init(DesktopWindow* parent_window, INT32 initial_page, BOOL hide_other_pages)
{
	if (initial_page <= 0 || initial_page > AdvancedPage)
	{
		if( initial_page >= FirstAdvancedPage && initial_page < AdvancedPageCount )
		{
			m_initial_advanced_page_id = initial_page; // Maps to actual tree index in AddGroup()
			initial_page = AdvancedPage;
		}
		else
		{
			initial_page = s_last_selected_prefs_page;
		}
	}

	m_hide_other_pages = hide_other_pages;
	//If only page should be shown, the initial page should be set as it is the only page that will show up
	OP_ASSERT(!m_hide_other_pages || (m_hide_other_pages && initial_page != -1));

	Dialog::Init(parent_window, initial_page, TRUE);

	return OpStatus::OK;
}

void NewPreferencesDialog::InitPage(INT32 index)
{
	Dialog::InitPage(index);
}

OP_STATUS NewPreferencesDialog::RecreateDialog()
{
	//Create a new prefences dialog
	NewPreferencesDialog* dialog = OP_NEW(NewPreferencesDialog, ());
	if (dialog)
	{
		// Find out which widget has focus in the old dialog
		OpInputContext* context = GetRecentKeyboardChildInputContext();
		if (context && context->GetType() >= WIDGET_TYPE && context->GetType() < WIDGET_TYPE_LAST)
		{
			//make sure that keyboard focus is the same in the new dialog
			RETURN_IF_ERROR(dialog->SetInitialFocus(static_cast<OpWidget*>(context)->GetName().CStr()));
		}

		//initialize the new dialog
		RETURN_IF_ERROR(dialog->Init(GetParentDesktopWindow(), GetCurrentPage(), m_hide_other_pages ));

		//Copy changes from the main tabs to the new dialog
		for (unsigned i = 0; i < PageCount; i++)
		{
			if (IsPageInited(i)) //don't copy if the page hasn't been opened, cannot contain changes
			{
				//make sure everything is initialize in the tab
				dialog->InitPage(i);
				RETURN_IF_ERROR(CopyWidgetChanges(GetPageByNumber(i), dialog));
			}
		}

		//Copy changes from the advanced sections
		for (unsigned i = FirstAdvancedPage; i < AdvancedPageCount; i++)
		{
			if (m_advanced_setup.Find(i) != -1) //only copy when the page has been opened
			{
				OpGroup* page = m_advanced_model.GetItemByID(i);
				if (page)	//ignore pages that are not added the treeview (they been disabled for some reason)
				{
					//initialize section
					dialog->InitAdvanced(i);
					//copy changes
					RETURN_IF_ERROR(CopyWidgetChanges(page, dialog));
				}
			}
		}

		//everything copied successfull
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS NewPreferencesDialog::CopyWidgetChanges(OpWidget* widget, NewPreferencesDialog* target_dialog)
{
	OP_ASSERT(widget != NULL && target_dialog != NULL); //need a source and target for copying
	if (widget != NULL)
	{
		//Loop through the children of the widget and search for widgets that can have changes
		OpWidget* child = static_cast<OpWidget*>(widget->childs.First());
		while(child)
		{
			//Get the name of the widget as specified in dialog.ini,
			//can be NULL or emtpy but GetText/GetValue support that
			const char* name = child->GetName().CStr();
			//this dialog and target_dialog should have the same widgets
			OP_ASSERT(child->GetName().IsEmpty() || target_dialog->GetWidgetByName(name) != NULL);
			switch (child->GetType())
			{
				case WIDGET_TYPE_DROPDOWN:
				case WIDGET_TYPE_CHECKBOX:
				case WIDGET_TYPE_RADIOBUTTON:
					{
						//use generic interfaces from OpWidget to copy the current state
						target_dialog->SetWidgetValue(name, child->GetValue());
						target_dialog->SetWidgetEnabled(name, child->IsEnabled());
					}
					break;
				case WIDGET_TYPE_EDIT:
				case WIDGET_TYPE_FILECHOOSER_EDIT:
					{
						//use generic GetText/SetText interface from OpWidget to copy the current text
						OpString text;
						RETURN_IF_ERROR(child->GetText(text));
						target_dialog->SetWidgetText(name, text.CStr());
					}
					break;
				case WIDGET_TYPE_GROUP:
					{
						// Traverse the children of a group to see if it contains more editable widgets
						RETURN_IF_ERROR(CopyWidgetChanges(child, target_dialog));
					}
				default:
					break;
			}
			child = (OpWidget*) child->Suc();
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR_NULL_POINTER;
}

const char* NewPreferencesDialog::GetDialogImage()
{
/*	switch (GetCurrentPage())
	{
	case 0:
		return "Go to homepage";
	case 1:
		return "Wand";
	case 2:
		return "Prefs Pages";
	default:
		break;
	}
*/
	return NULL;
}

const char* NewPreferencesDialog::GetHelpAnchor()
{
	INT32 page = GetCurrentPage();

	static const char* s_page_names[] =
	{
		"preferences.html",
		"preferences.html",
		"search.html",
		"preferences.html"
	};

	if (page<4)
		return s_page_names[page];
	else if (page!=4)
		return "";

	static const char* s_advanced_page_names[] =
	{
		"tabs.html#prefs",
		"preferences.html",
		"preferences.html",
		"",
		"preferences.html",
		"fonts.html",
		"downloads.html",
		"preferences.html",
		"",
		"history.html",
		"cookies.html",
		"preferences.html",
		"preferences.html",
		"",
		"customtoolbar.html",
		"preferences.html",
		"voiceprefs.html"
	};

	return s_last_selected_advanced_prefs > int(sizeof(s_advanced_page_names) / sizeof s_advanced_page_names[0]) - 1 ? "" : s_advanced_page_names[s_last_selected_advanced_prefs];
}

void NewPreferencesDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;

	switch (index)
	{
	case 0:
		action = GetOkAction(); text.Set(GetOkText());
		is_default = TRUE;
		name.Set(WIDGET_NAME_BUTTON_OK);
		break;
	case 1:
		action = GetCancelAction(); text.Set(GetCancelText());
		name.Set(WIDGET_NAME_BUTTON_CANCEL);
		break;
	case 2:
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_HELP); text.Set(GetHelpText()));
		name.Set(WIDGET_NAME_BUTTON_HELP);
		break;
	}
}

void NewPreferencesDialog::OnInit()
{
	m_selected_color_chooser.Set("Background_color_button");

	m_advanced_treeview = (OpTreeView*) GetWidgetByName("Advanced_treeview");

	if (m_advanced_treeview)
	{
		OpString str;
//		g_languageManager->GetString(Str::SI_IDLABEL_CATEGORY, str);
//		m_advanced_model.SetColumnData(0, str.CStr());

		ShowWidget("Ads_group", FALSE);

		g_languageManager->GetString(Str::D_NEW_PREFERENCES_PAGES, str);
		AddGroup(str.CStr(), "Tabs_group", TabsPage);

		g_languageManager->GetString(Str::D_NEW_PREFS_BROWSING, str);
		AddGroup(str.CStr(), "Browser_group", BrowserPage);

		g_languageManager->GetString(Str::D_NEW_PREFS_NOTIFICATIONS, str);
		AddGroup(str.CStr(), "Sounds_group", SoundPage);

		m_advanced_model.AddSeparator();

		g_languageManager->GetString(Str::D_NEW_PREFS_CONTENT, str);
		AddGroup(str.CStr(), "Content_group", ContentPage);
		g_languageManager->GetString(Str::D_NEW_PREFS_FONTS, str);
		AddGroup(str.CStr(), "Fonts_group", FontPage);
		g_languageManager->GetString(Str::D_NEW_PREFS_DOWNLOADS, str);
		AddGroup(str.CStr(), "Downloads_group", DownloadPage);
		g_languageManager->GetString(Str::D_NEW_PREFS_PROGRAMS, str);
		AddGroup(str.CStr(), "Programs_group", ProgramPage);

		m_advanced_model.AddSeparator();

		g_languageManager->GetString(Str::D_NEW_PREFS_HISTORY, str);
		AddGroup(str.CStr(), "History_group", HistoryPage);
		g_languageManager->GetString(Str::D_NEW_PREFS_COOKIES, str);
		AddGroup(str.CStr(), "Cookies_group", CookiePage);
		g_languageManager->GetString(Str::D_NEW_PREFS_SECURITY, str);
		AddGroup(str.CStr(), "Security_group", SecurityPage);
		g_languageManager->GetString(Str::D_NEW_PREFS_NETWORK, str);
		AddGroup(str.CStr(), "Network_group", NetworkPage);
		g_languageManager->GetString(Str::D_NEW_PREFS_STORAGE, str);
		AddGroup(str.CStr(), "LocalStorage_group", LocalStoragePage);
		m_advanced_model.AddSeparator();

		g_languageManager->GetString(Str::D_NEW_PREFS_TOOLBARS, str);
		AddGroup(str.CStr(), "Toolbars_group", ToolbarPage);
		g_languageManager->GetString(Str::D_NEW_PREFS_SHORTCUTS, str);
		AddGroup(str.CStr(), "Keyboard_group", ShortcutPage);

		m_advanced_treeview->SetDeselectable(FALSE);
		m_advanced_treeview->SetTreeModel(&m_advanced_model);
		m_advanced_treeview->SetSelectedItem(s_last_selected_advanced_prefs);
		m_advanced_treeview->SetShowColumnHeaders(FALSE);

		if (m_hide_other_pages)
		{
			// All other pages than the current selected one should be hidden
			// Let's remove the treeview listing the advanced pages, and center the current page
			m_advanced_treeview->SetOriginalRect(OpRect(0,0,0,0));
			OpGroup* group = m_advanced_model.GetItem(s_last_selected_advanced_prefs);
			OpRect rect(group->GetRect());
			group->SetOriginalRect(OpRect(0, 0, rect.width, rect.height));
			group->GetText(str);
			SetTitle(str.CStr());
		}
	}

	// Make sure we don't update more than needed
	m_initial_show_scrollbars = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars);
	m_initial_show_progress_dlg = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProgressDlg);
	m_initial_scale = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale);
	m_initial_show_win_size = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowWinSize);
	m_initial_rendering_mode = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::RenderingMode);

	// Advanced hasn't really changed yet
	SetPageChanged(AdvancedPage, FALSE);

	// Construct informative message releated to search for user
	ConstructSearchMsg();
}

void NewPreferencesDialog::OnSetFocus()
{
	if (m_initial_focused_widget.HasContent())
	{
		// A specific widget needs focus when the dialog is opened for the first time
		SetWidgetFocus(m_initial_focused_widget.CStr());
	}
}

void NewPreferencesDialog::OnRelayout()
{
	Dialog::OnRelayout();

	// Update search message/text with modified default search engine 
	if (GetCurrentPage() == SearchPage)
		ConstructSearchMsg();
}

void NewPreferencesDialog::AddGroup(const uni_char* title, const char* name, INT32 id)
{
	OpGroup* group = (OpGroup*) GetWidgetByName(name);

	if (!group)
		return;

	group->SetVisibility(FALSE);
	INT32 index = m_advanced_model.AddItem(title, NULL, 0, -1, group, id);

	if( id == m_initial_advanced_page_id )
	{
		s_last_selected_advanced_prefs = index; // Map id to index
	}
}

void NewPreferencesDialog::OnInitPage(INT32 page_number, BOOL first_time)
{
	if (first_time)
	{
		switch (page_number)
		{
		case 0:
			InitGeneral();
			break;
		case 1:
			InitWand();
			break;
		case 2:
			InitSearch();
			break;
		case 3:
            if (!IsPageChanged(AdvancedPage))
            {
				// Need to init the font list now
				OpStatus::Ignore(InitFontsModel());
				SetPageChanged(AdvancedPage, FALSE);
            }

			InitWebPages();
			break;
		case 4:
			InitAdvanced(-1);
			break;
		default:
			OP_ASSERT(0);
		}
	}
}

void NewPreferencesDialog::OnMasterPasswordChanged()
{
	BOOL has_master_password = g_libcrypto_master_password_handler->HasMasterPassword();
	SetWidgetEnabled("Master_password_checkbox", has_master_password);
	SetWidgetEnabled("Ask_for_password_dropdown", has_master_password);
}

void NewPreferencesDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_advanced_treeview)
	{
		OpGroup* group = m_advanced_model.GetItem(s_last_selected_advanced_prefs);
		group->SetVisibility(FALSE);

		INT32 new_modelpos = m_advanced_treeview->GetSelectedItemModelPos();
		SimpleTreeModelItem* item = m_advanced_model.GetItemByIndex(new_modelpos);
		InitAdvanced(item->GetID());
		group = m_advanced_model.GetItem(new_modelpos);
		group->SetVisibility(TRUE);
		s_last_selected_advanced_prefs = new_modelpos;
	}
	else if (widget->IsNamed("Email_dropdown"))
	{
		OpDropDown* dropdown = (OpDropDown*)widget;
		int selected_pos = dropdown->GetSelectedItem();

		int mail_handler = 0;
#if defined(_UNIX_DESKTOP_)
		switch (selected_pos)
		{
		case 0:
			mail_handler = MAILHANDLER_OPERA;
			break;
		case 1:
			mail_handler = MAILHANDLER_APPLICATION;
			break;
		}
#else
		switch (selected_pos)
		{
		case 0:
			mail_handler = MAILHANDLER_OPERA;
			break;
		case 1:
			mail_handler = MAILHANDLER_SYSTEM;
			break;
		case 2:
			mail_handler = MAILHANDLER_APPLICATION;
			break;
		}
#endif

		ShowWidget("Use_system_email_edit", FALSE);
		ShowWidget("Specific_email_chooser", FALSE);
		ShowWidget("Email_checkbox", FALSE);
		ShowWidget("Manage_accounts_button", FALSE);

		switch (mail_handler)
		{
		case MAILHANDLER_OPERA:
			{
				ShowWidget("Manage_accounts_button", TRUE);
				break;
			}
		case MAILHANDLER_SYSTEM:
			{
				ShowWidget("Use_system_email_edit", TRUE);
				break;
			}
		case MAILHANDLER_APPLICATION:
			{
				ShowWidget("Specific_email_chooser", TRUE);
#if defined (_UNIX_DESKTOP_)
				ShowWidget("Email_checkbox", TRUE);
#endif
				break;
			}
		}
	}
	else if (widget->IsNamed("Normal_cookies_dropdown"))
	{
		SetWidgetEnabled("Third_party_cookies_dropdown", ((OpDropDown*)widget)->GetSelectedItem() != 0);
	}
	else if (widget->IsNamed("Language_dropdown"))
	{
		m_language_changed = TRUE;
	}
	else if (widget->IsNamed("Protocol_treeview"))
	{
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Protocol_treeview");
		if (treeview && m_trusted_protocol_list)
		{
			INT32 pos = treeview->GetSelectedItemModelPos();
			TrustedProtocolEntry* entry = m_trusted_protocol_list->Get(pos);
			SetWidgetEnabled( "Delete_trusted_application", entry && entry->CanDelete() );
		}
	}
	else if (widget->IsNamed("No_cookies_radio"))
	{
		SetWidgetEnabled("Delete_cookies_on_exit_checkbox", widget->GetValue() ? FALSE : TRUE);
		SetWidgetEnabled("Display_cookies_checkbox",        widget->GetValue() ? FALSE : TRUE);
	}

	Dialog::OnChange(widget, changed_by_mouse);
}

void NewPreferencesDialog::OnClick(OpWidget *widget, UINT32 id)
{
/*
	if (widget->IsNamed("Open_in_tabs_checkbox"))
	{
		SetWidgetEnabled("Show_close_button_checkbox", GetWidgetValue("Open_in_tabs_checkbox"));
	}
*/
	if (widget->IsNamed("Show_all_file_types_checkbox"))
	{
		OpTreeView* treeview = (OpTreeView*) GetWidgetByName("File_types_treeview");
		if (treeview)
		{
			treeview->SetMatchType(widget->GetValue() ? MATCH_IMPORTANT : MATCH_STANDARD, TRUE);
		}
		return;
	}
	else if (widget->IsNamed("Mouse_gestures_checkbox"))
	{
		SetWidgetEnabled("Left_handed_checkbox", (BOOL)widget->GetValue());
		SetWidgetEnabled("Visual_mouse_gestures_checkbox", (BOOL)widget->GetValue());
	}
	else if (widget->IsNamed("Javascript_checkbox"))
	{
		g_input_manager->UpdateAllInputStates();
	}
	else if (widget->IsNamed("Plugins_checkbox"))
	{
		//The on demand plugins feature depends on plugins being enabled
		SetWidgetEnabled("On_demand_plugins_checkbox", (BOOL)widget->GetValue());
		g_input_manager->UpdateAllInputStates();
	}
	else if (widget->IsNamed("On_demand_plugins_checkbox"))
	{
		g_input_manager->UpdateAllInputStates();
	}
	else if (widget->IsNamed("Enable_frames_checkbox") || widget->IsNamed("Enable_inline_frames_checkbox"))
	{
		SetWidgetEnabled("Show_frame_border_checkbox", GetWidgetValue("Enable_frames_checkbox") || GetWidgetValue("Enable_inline_frames_checkbox"));
	}
/*	else if (widget->IsNamed("Enable_cookies_checkbox"))
	{
		BOOL enable_cookie_widgets = (BOOL)widget->GetValue();

		SetWidgetEnabled("Normal_cookies_dropdown", enable_cookie_widgets);
		SetWidgetEnabled("Third_party_cookies_dropdown", enable_cookie_widgets);
		SetWidgetEnabled("Throw_away_new_cookies_checkbox", enable_cookie_widgets);
		SetWidgetEnabled("Display_illegal_domain_warning_checkbox", enable_cookie_widgets);
		SetWidgetEnabled("Accept_illegal_path_checkbox", enable_cookie_widgets);
		SetWidgetEnabled("Display_illegal_path_warning_checkbox", enable_cookie_widgets);
	}*/
	else if (widget->IsNamed("Program_sounds_checkbox"))
	{
		SetWidgetEnabled("Program_sounds_treeview", widget->GetValue());
		g_input_manager->UpdateAllInputStates();
	}
	else if(widget->IsNamed("Source_external"))
	{
		SetWidgetEnabled("Source_viewer_chooser", TRUE);
	}
	else if(widget->IsNamed("Source_use_opera"))
	{
		SetWidgetEnabled("Source_viewer_chooser", FALSE);
	}
	else if (widget->IsNamed("Master_password_checkbox"))
	{
		// NOTE: This is a semi-nice, but only way to pass encryption request to WandManager
		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseParanoidMailpassword, GetWidgetValue("Master_password_checkbox")));

		// Setup for catching what happens in wand, which may be synchronous or asynchronous
		g_wand_manager->AddListener(this);

#ifdef WAND_CAP_OPS_WITH_OPSSLLISTENER // TODO: REMOVE WHEN INTEGRATING WITH patched WAND

		// Call wand to employ the changed paranoia setting with this dialog as ssl-dialog-listener (OpSSLListener)
		g_wand_manager->UpdateSecurityStateWithOpSSLListener(this);

#else

		g_wand_manager->UpdateSecurityStateWithoutWindow();

#endif // WAND_CAP_OPS_WITH_OPSSLLISTENER
    }

    Dialog::OnClick(widget, id);
}

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
void NewPreferencesDialog::EditSearchEntry()
{
	OpTreeView* searches = (OpTreeView*)GetWidgetByName("Web_search_treeview");
	if (searches)
	{
		SearchTemplate *item = (SearchTemplate *)searches->GetSelectedItem();

		if(item != NULL && item->IsDeletable())
		{
			OpString engine_name;
			item->GetEngineName(engine_name, FALSE);

            SearchTemplate* search_default = g_searchEngineManager->GetDefaultSearch();
            SearchTemplate* sd_default     = g_searchEngineManager->GetDefaultSpeedDialSearch();

            // is_default... = TRUE if selected search engine is the default (before editing)
            BOOL is_default_search   = search_default != 0 && (item->GetUniqueGUID().Compare(search_default->GetUniqueGUID()) == 0);
            BOOL is_speeddial_search = sd_default     != 0 && (item->GetUniqueGUID().Compare(sd_default->GetUniqueGUID()) == 0);

			OpString url;
			OpString key;
			OpString query;
			url.Set(item->GetUrl().CStr());
			key.Set(item->GetKey().CStr());
			query.Set(item->GetQuery().CStr());

			SearchEngineItem *search_engine = OP_NEW(SearchEngineItem, (engine_name, url, key, query, item->GetIsPost(), is_default_search, is_speeddial_search, item));

			if(search_engine != NULL)
			{
				EditSearchEngineDialog *dlg = OP_NEW(EditSearchEngineDialog, ());
				
				OpString charset; // todo: empty??
				if (dlg)
				{
					dlg->Init(this, search_engine, charset, item);
				}
				else
				{
					OP_DELETE(search_engine);
				}
			}
		}
	}
}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

void NewPreferencesDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if(widget->IsNamed("Web_search_treeview") && nclicks == 2)
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		EditSearchEntry();
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	}
}

void NewPreferencesDialog::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	if (widget)
	{
		if (widget->IsNamed("Toolbar_file_treeview"))
		{
			OpTreeView* toolbar_configurations = static_cast<OpTreeView*>(widget);
			m_toolbars_model.SetItemData(toolbar_configurations->GetModelPos(pos), column, text.CStr());
			TRAPD(err, g_setup_manager->RenameSetupPrefsFileL(text.CStr(), toolbar_configurations->GetModelPos(pos), OPTOOLBAR_SETUP));
		}
		else if (widget->IsNamed("Menu_file_treeview"))
		{
			OpTreeView* menu_configurations = static_cast<OpTreeView*>(widget);
			m_menus_model.SetItemData(menu_configurations->GetModelPos(pos), column, text.CStr());
			TRAPD(err, g_setup_manager->RenameSetupPrefsFileL(text.CStr(), menu_configurations->GetModelPos(pos), OPMENU_SETUP));
		}
		else if (widget->IsNamed("Keyboard_treeview"))
		{
			OpTreeView* keyboard_configurations = static_cast<OpTreeView*>(widget);
			m_keyboards_model.SetItemData(keyboard_configurations->GetModelPos(pos), column, text.CStr());
			TRAPD(err, g_setup_manager->RenameSetupPrefsFileL(text.CStr(), keyboard_configurations->GetModelPos(pos), OPKEYBOARD_SETUP));
		}
		else if (widget->IsNamed("Mouse_treeview"))
		{
			OpTreeView* mouse_configurations = static_cast<OpTreeView*>(widget);
			m_mice_model.SetItemData(mouse_configurations->GetModelPos(pos), column, text.CStr());
			TRAPD(err, g_setup_manager->RenameSetupPrefsFileL(text.CStr(), mouse_configurations->GetModelPos(pos), OPMOUSE_SETUP));
		}
	}
}


BOOL NewPreferencesDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_MANAGE_SCRIPT_OPTIONS:
				{
					child_action->SetEnabled(GetWidgetValue("Javascript_checkbox"));
					return TRUE;
				}

			case OpInputAction::ACTION_MANAGE_PLUGINS:
				{
					child_action->SetEnabled(GetWidgetValue("Plugins_checkbox"));
					return TRUE;
				}
			case OpInputAction::ACTION_DELETE:
				{
					if (IsCurrentAdvancedPage(SoundPage))
					{
						child_action->SetEnabled(GetWidgetValue("Program_sounds_checkbox"));
						return TRUE;
					}
					else if (IsCurrentAdvancedPage(LocalStoragePage))
					{
						OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Storage_treeview");
						child_action->SetEnabled(treeview && treeview->GetSelectedItemCount());
						return TRUE;
					}
					else if (IsCurrentAdvancedPage(ProgramPage))
					{
						if (m_trusted_protocol_list)
						{
							OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Protocol_treeview");
							INT32 pos = treeview ? treeview->GetSelectedItemModelPos() : -1;
							TrustedProtocolEntry* entry = m_trusted_protocol_list->Get(pos);
							child_action->SetEnabled(entry && entry->CanDelete());
						}
						return TRUE;
					}
					break;
				}

			case OpInputAction::ACTION_MANAGE_TRUSTED_APPLICATION:
				if (IsCurrentAdvancedPage(ProgramPage))
				{
					if (m_trusted_protocol_list)
					{
						OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Protocol_treeview");
						INT32 pos = treeview ? treeview->GetSelectedItemModelPos() : -1;
						TrustedProtocolEntry* entry = m_trusted_protocol_list->Get(pos);
						child_action->SetEnabled(!!entry);
					}
					return TRUE;
				}
			break;

			case OpInputAction::ACTION_CHOOSE_SOUND:
			case OpInputAction::ACTION_TEST_SOUND:
				{
					child_action->SetEnabled(GetWidgetValue("Program_sounds_checkbox"));
					return TRUE;
				}
			case OpInputAction::ACTION_CHOOSE_CURRENT_PAGE:
				{
					child_action->SetEnabled(g_application->GetActiveDocumentDesktopWindow() != NULL);
					return TRUE;
				}
			case OpInputAction::ACTION_EDIT_SITE_PREFERENCES:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}

			case OpInputAction::ACTION_ADD_USER_SEARCH:
				{
					// DSK-351304: disable until searches are loaded
					child_action->SetEnabled(g_searchEngineManager->HasLoadedConfig());
					return TRUE;
				}

			case OpInputAction::ACTION_EDIT_USER_SEARCH:
			case OpInputAction::ACTION_DELETE_USER_SEARCH:
				{
					BOOL enabled = FALSE;
					OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Web_search_treeview");
					if (treeview)
					{
						SearchTemplate* item = (SearchTemplate *)treeview->GetSelectedItem();
						enabled = item && item->IsDeletable();

						// There must be at least one search engine in the list
						if (child_action->GetAction() == OpInputAction::ACTION_DELETE_USER_SEARCH && treeview->GetItemCount() <= 1)
							enabled = FALSE;
					}
					child_action->SetEnabled(enabled);
					return TRUE;
				}

			case OpInputAction::ACTION_DELETE_TOOLBAR_SETUP:
				{
					OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Toolbar_file_treeview");
					if(toolbar_configurations)
					{
						INT32 pos = toolbar_configurations->GetSelectedItemModelPos();
						child_action->SetEnabled(g_setup_manager->IsEditable(pos, OPTOOLBAR_SETUP));
					}
					return TRUE;
				}
			case OpInputAction::ACTION_DELETE_MOUSE_SETUP:
				{
					OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Mouse_treeview");
					if(toolbar_configurations)
					{
						INT32 pos = toolbar_configurations->GetSelectedItemModelPos();
						child_action->SetEnabled(g_setup_manager->IsEditable(pos, OPMOUSE_SETUP));
					}
					return TRUE;
				}
			case OpInputAction::ACTION_DELETE_KEYBOARD_SETUP:
				{
					OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Keyboard_treeview");
					if(toolbar_configurations)
					{
						INT32 pos = toolbar_configurations->GetSelectedItemModelPos();
						child_action->SetEnabled(g_setup_manager->IsEditable(pos, OPKEYBOARD_SETUP));
					}
					return TRUE;
				}
			case OpInputAction::ACTION_DELETE_MENU_SETUP:
				{
					OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Menu_file_treeview");
					if(toolbar_configurations)
					{
						INT32 pos = toolbar_configurations->GetSelectedItemModelPos();
						child_action->SetEnabled(g_setup_manager->IsEditable(pos, OPMENU_SETUP));
					}
					return TRUE;
				}
			case OpInputAction::ACTION_DELETE_LOCAL_STORAGE:
				{
					OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Storage_treeview");
					child_action->SetEnabled(treeview && treeview->GetItemCount());
					return TRUE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_SHOW_CLASSIC_TAB_OPTIONS:
		{
			TabClassicOptionsDialog *dialog = OP_NEW(TabClassicOptionsDialog, ());
			if (dialog)
				dialog->Init(this);

			return TRUE;
		}
		case OpInputAction::ACTION_MANAGE_WAND:
		{
			ServerManagerDialog* dialog = OP_NEW(ServerManagerDialog, (FALSE, TRUE));
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}

		case OpInputAction::ACTION_CHOOSE_CURRENT_PAGE:
		{
			BOOL permanent_homepage = FALSE;
#ifdef PERMANENT_HOMEPAGE_SUPPORT
			permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
#endif
			if (!permanent_homepage)
			{
				DocumentDesktopWindow* ddw = g_application->GetActiveDocumentDesktopWindow();
				if (ddw)
				{
					URL curr_url = WindowCommanderProxy::GetCurrentURL(ddw->GetWindowCommander());
					OpString homepage;
					curr_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, homepage, TRUE);
					SetWidgetText("Startpage_edit", homepage.CStr());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_CHOOSE_LINK_COLOR:
		{
			COLORREF color = 0;

			OpColorChooser* chooser = NULL;

			m_selected_color_chooser.Set(action->GetActionDataString());
			OpWidget * color_widget = GetWidgetByName(m_selected_color_chooser);
			if (color_widget)
				color = color_widget->GetBackgroundColor(color);

			if (OpStatus::IsSuccess(OpColorChooser::Create(&chooser)))
			{
				chooser->Show(color, this, this);
				OP_DELETE(chooser);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_CHOOSE_BACKGROUND_COLOR:
		{
			COLORREF color =
				g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND);

			OpColorChooser* chooser = NULL;

			m_selected_color_chooser.Set(action->GetActionDataString());

			if (OpStatus::IsSuccess(OpColorChooser::Create(&chooser)))
			{
				chooser->Show(color, this, this);
				OP_DELETE(chooser);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_CHOOSE_FONT:
		{

			m_selected_font_chooser.Set(action->GetActionDataString());

			OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Fonts_treeview");

			if (treeview)
			{
				int pos = treeview->GetSelectedItemModelPos();

				if (m_selected_font_chooser.CompareI("Normal_font_button") == 0)
				{
					pos = POSITION_OF_NORMAL_FONT_IN_LIST;
				}
				else if (m_selected_font_chooser.CompareI("Monospace_button") == 0)
				{
					pos = POSITION_OF_NORMAL_FONT_IN_LIST + 1;
				}

				if (pos != -1)
				{
					COLORREF color = 0;
					FontAtt font;
					BOOL fontname_only = FALSE;

					FontListItem* font_item = m_font_list.Get(pos);
					if( font_item )
					{
						GetFontAttAndColorFromFontListItem(font_item, font, color, fontname_only);

						SelectFontDialog *dialog = 0;
						if( fontname_only )
						{
							dialog = OP_NEW(SelectCSSFontDialog, (font, this));
						}
						else
						{
							dialog = OP_NEW(SelectFontDialog, (font, color, FALSE, m_font_list.Get(pos)->GetFontId(), this));
						}

						if (dialog)
							dialog->Init(this);

						m_selected_font_chooser.Empty();
					}
				}
				else
				{
					OP_ASSERT(!"No font item entry. Wrong index");
				}

			}
			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_INTERNATIONAL_FONTS:
		{
			InternationalFontsDialog* dialog = OP_NEW(InternationalFontsDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}

		case OpInputAction::ACTION_APPLY:
		{
			OnOk();
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_PREFERENCES:
		{
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_LANGUAGE_PREFERENCES:
		{
			LanguagePreferencesDialog* dialog = OP_NEW(LanguagePreferencesDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}

		case OpInputAction::ACTION_OPEN_OPERA_WEB_TURBO_DIALOG:
		{
			g_opera_turbo_manager->ShowNotificationDialog(this);
			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_NAME_COMPLETION:
		{
			OpStatus::Ignore(
				ShowDialog(
					OP_NEW(NameCompletionController, ()),
					g_global_ui_context,
					this));
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_PROXY_SERVERS:
		{
			ProxyServersDialog* dialog = OP_NEW(ProxyServersDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
		case OpInputAction::ACTION_MANAGE_COOKIES:
		{
			ServerManagerDialog* dialog = OP_NEW(ServerManagerDialog, (TRUE, FALSE));
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
		case OpInputAction::ACTION_MANAGE_SITES:
		{
			ServerManagerDialog* dialog = OP_NEW(ServerManagerDialog, (FALSE, FALSE));
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
		case OpInputAction::ACTION_CHANGE_MASTERPASSWORD:
		{
			ChangeMasterPasswordDialog* dialog = OP_NEW(ChangeMasterPasswordDialog, ());
			if (dialog)
			{
				dialog->AddListener(this);
				dialog->Init(this);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_SECURITY_PROTOCOLS:
		{
			SecurityProtocolsDialog* dialog = OP_NEW(SecurityProtocolsDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
		case OpInputAction::ACTION_MANAGE_CERTIFICATES:
		{
			CertificateManagerDialog* dialog = OP_NEW(CertificateManagerDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
		case OpInputAction::ACTION_MANAGE_WHITELIST:
		{
			ServerWhiteListDialog* dialog = OP_NEW(ServerWhiteListDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
#if defined(M2_SUPPORT)
		case OpInputAction::ACTION_MANAGE_ACCOUNTS:
		{
			if (g_application->ShowM2())
			{
				AccountManagerDialog* dialog = OP_NEW(AccountManagerDialog, ());
				if (dialog)
					dialog->Init(this);
			}
			return TRUE;
		}
#endif

#if defined(_UNIX_DESKTOP_)
		case OpInputAction::ACTION_MANAGE_PLUGINS:
			PluginSetupDialog::Create(this);
			if (m_advanced_setup.Find(DownloadPage) != -1)
				PopulateFileTypes();
			return TRUE;

	    case OpInputAction::ACTION_SHOW_FILE_HANDLERS:
		{
			FileHandlerDialog::Create(this);
			return TRUE;
		}

#endif
		case OpInputAction::ACTION_EDIT_FILE_TYPE:
		{
			OpTreeView* treeview = (OpTreeView*)GetWidgetByName("File_types_treeview");

			if (treeview)
			{
				SimpleTreeModelItem* item = (SimpleTreeModelItem*)treeview->GetSelectedItem();
				if (item)
				{
					Viewer * viewer = static_cast<Viewer*>(item->GetUserData());
					FiletypeDialog* dialog = OP_NEW(FiletypeDialog, (viewer, FALSE));
					if (dialog)
						dialog->Init(this);
					g_viewers->OnViewerChanged(viewer);
				}
			}

			return TRUE;
		}
		case OpInputAction::ACTION_NEW_FILE_TYPE:
		{
			OpTreeView* treeview = (OpTreeView*)GetWidgetByName("File_types_treeview");

			if (treeview)
			{
				Viewer* newviewer = OP_NEW(Viewer, ());
				if (newviewer)
				{
					FiletypeDialog* dialog = OP_NEW(FiletypeDialog, (newviewer, TRUE));
					if (dialog)
						dialog->Init(this);
					else
						OP_DELETE(newviewer);
				}
			}

			return TRUE;
		}
		case OpInputAction::ACTION_NEW_SERVER:
		{
			OpString empty;
			ServerPropertiesDialog* dialog = OP_NEW(ServerPropertiesDialog, (empty, TRUE));
			if (dialog)
				dialog->Init(this);

			return TRUE;
		}
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Site_treeview");
			if (treeview)
			{
				ServerItem* item = (ServerItem*) treeview->GetSelectedItem();

				if (item && item->m_real_server_name.HasContent() && item->m_type == OpTypedObject::SERVER_TYPE )
				{
					ServerPropertiesDialog* dialog = OP_NEW(ServerPropertiesDialog, (item->m_real_server_name, FALSE));
					if (dialog)
						dialog->Init(this);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_CHOOSE_SOUND:
		{
			OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Program_sounds_treeview");
			if (!treeview)
				return TRUE;

			INT32 pos = treeview->GetSelectedItemModelPos();
			if (pos == -1)
				return TRUE;

			if (!m_chooser)
				RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

			m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
			m_request.initial_path.Set(m_sounds_model.GetItemString(pos, 1));
			// Help the user a little bit if the file is empty
			if( m_request.initial_path.IsEmpty() )
			{
				for (int i = 0; i < 6; i++)
				{
					m_request.initial_path.Set(m_sounds_model.GetItemString(i, 1));
					if( m_request.initial_path.HasContent() )
						break;
				}
			}
			OpString filter;
			g_languageManager->GetString(Str::D_SELECT_SOUND_FILE, m_request.caption);
			g_languageManager->GetString(Str::SI_SOUND_TYPES, filter);

			if (filter.HasContent())
			{
				StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters);
			}
			OpStatus::Ignore(m_chooser->Execute(GetOpWindow(), this, m_request));

			return TRUE;
		}
		case OpInputAction::ACTION_TEST_SOUND:
		{
			OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Program_sounds_treeview");

			if (treeview)
			{
				UINT32 pos = treeview->GetSelectedItemModelPos();
				if ((INT32)pos != -1)
				{
					OpString sound;
					sound.Set(m_sounds_model.GetItemString(pos, 1));

					if (sound.HasContent())
					{
						SoundUtils::SoundIt(sound, TRUE, TRUE);
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_ADD_TRUSTED_APPLICATION:
		{
			if( m_trusted_protocol_list )
			{
				UINT32 count = m_trusted_protocol_list->GetCount();

				TrustedProtocolDialog *dialog = OP_NEW(TrustedProtocolDialog, (m_trusted_protocol_list,-1));
				if (dialog)
					dialog->Init(this); // blocking

				if( m_trusted_protocol_list->GetCount() == count+1 )
				{
					// One item has been added (at end of list)
					TrustedProtocolEntry* entry = m_trusted_protocol_list->Get(count);
					INT32 index = m_trusted_protocol_model.AddItem(entry->GetDisplayText());
					OpString application;
					BOOL is_formatted = FALSE;
					entry->GetActiveApplication(application, &is_formatted);
					m_trusted_protocol_model.SetItemData(index, 1, application);
					m_trusted_protocol_model.GetItemByIndex(index)->SetHasFormattedText(is_formatted);
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_MANAGE_INSTALLED_WEB_FEEDS:
		{
			CustomWebFeedController* dialog = OP_NEW(CustomWebFeedController,());
			if (dialog)
				ShowDialog(dialog, g_global_ui_context, this);
		}
		break;

		case OpInputAction::ACTION_EXECUTE_TRANSFERITEM:
		{
			if( m_trusted_application_list )
			{
				TrustedProtocolDialog *dialog = OP_NEW(TrustedProtocolDialog, (m_trusted_application_list,0));
				if (dialog)
					dialog->Init(this); // blocking
				if (m_trusted_application_list->GetCount() > 0)
				{
					OpString application;
					m_trusted_application_list->Get(0)->GetActiveApplication(application);
					SetWidgetText("Source_program", application);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_MANAGE_TRUSTED_APPLICATION:
		{
			OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Protocol_treeview");
			if (treeview && m_trusted_protocol_list)
			{
				INT32 pos = treeview->GetSelectedItemModelPos();
				if( pos != -1 )
				{
					TrustedProtocolDialog *dialog = OP_NEW(TrustedProtocolDialog, (m_trusted_protocol_list,pos));
					if (dialog)
						dialog->Init(this); // blocking
					TrustedProtocolEntry* entry = m_trusted_protocol_list->Get(pos);
					{
						m_trusted_protocol_model.SetItemData(pos, 0, entry->GetDisplayText());

						OpString application;
						BOOL is_formatted = FALSE;
						entry->GetActiveApplication(application, &is_formatted);
						m_trusted_protocol_model.SetItemData(pos, 1, application);
						m_trusted_protocol_model.GetItemByIndex(pos)->SetHasFormattedText(is_formatted);

						// The action can be trigged by a double click in the list. That execution path
						// will not flag the page as changed (thus data not saved in OnOK()) unless
						// we do that manually
						OpTreeView* treeview = static_cast<OpTreeView*>(GetWidgetByName("Protocol_treeview"));
						if (treeview)
							OnChange(treeview, FALSE);
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_DELETE:
		{
			OpTreeView* treeview = NULL;
			OpButton* button = NULL;

			if (IsCurrentAdvancedPage(ProgramPage))
			{
				treeview = (OpTreeView*)GetWidgetByName("Protocol_treeview");
				button = (OpButton*) GetWidgetByName("Delete_trusted_application");
				if (treeview && button /*&& (treeview->IsFocused() || button->IsFocused())*/)
				{
					INT32 pos = treeview->GetSelectedItemModelPos();
					if( pos != -1 && m_trusted_protocol_list )
					{
						if( m_trusted_protocol_list->CanDelete(pos) )
						{
							m_trusted_protocol_list->Delete(pos);
							m_trusted_protocol_model.Delete(pos);
						}
					}
					return TRUE;
				}
			}
			else if (IsCurrentAdvancedPage(DownloadPage))
			{
				treeview = (OpTreeView*) GetWidgetByName("File_types_treeview");
				button = (OpButton*) GetWidgetByName("File_types_delete_button");

				if (treeview && button /*&& (treeview->IsFocused() || button->IsFocused())*/)
				{
					SimpleTreeModelItem * item = (SimpleTreeModelItem*)treeview->GetSelectedItem();

					if(item)
					{
						Viewer * selected_viewer = static_cast<Viewer*>(item->GetUserData());
						if(!g_viewers->IsNativeViewerType(selected_viewer->GetContentTypeString8()))
						{
							g_viewers->DeleteViewer(selected_viewer);
						}
					}
					return TRUE;
				}
			}
			else if (IsCurrentAdvancedPage(SoundPage))
			{
				treeview = (OpTreeView*) GetWidgetByName("Program_sounds_treeview");
				button = (OpButton*) GetWidgetByName("Clear_sound_button");

				if (treeview && button && (treeview->IsFocused() || button->IsFocused()))
				{
					UINT32 pos = treeview->GetSelectedItemModelPos();
					if ((INT32)pos != -1)
					{
						m_sounds_model.SetItemData(pos, 1, NULL);
					}
					return TRUE;
				}
			}
			else if (IsCurrentAdvancedPage(LocalStoragePage))
			{
				treeview = (OpTreeView*) GetWidgetByName("Storage_treeview");
				if (treeview)
				{
					OpINT32Vector list;
					treeview->GetSelectedItems(list, FALSE);
					
					for (UINT32 i=0; i<list.GetCount(); i++)
					{
						INT32 pos = list.Get(list.GetCount() - i - 1);

						if (pos != -1)
							pos = treeview->GetModelPos(pos);
						if (pos != -1)
						{
							if (OpStatus::IsSuccess(DeleteLocalStorage(m_local_storage_model.GetItemString(pos))))
								m_local_storage_model.Delete(pos);
						}
					}
				}
			}
/*
			treeview = (OpTreeView*) GetWidgetByName("Site_treeview");
			button = (OpButton*) GetWidgetByName("Site_delete_button");

			if (treeview && button && (treeview->IsFocused() || button->IsFocused()))
			{
				ServerItem* item = (ServerItem*) treeview->GetSelectedItem();

				if (item && item->m_type == OpTypedObject::SERVER_TYPE)
				{
					g_server_manager->HandleServerDeleteAction(treeview, TRUE, TRUE);
				}
				return TRUE;
			}
*/
			break;
		}

		case OpInputAction::ACTION_MANAGE_MOUSE:
		{
			OpTreeView* mouse_configurations = (OpTreeView*)GetWidgetByName("Mouse_treeview");

			if (mouse_configurations && mouse_configurations->GetSelectedItemModelPos() != -1)
			{
				InputManagerDialog* dialog = OP_NEW(InputManagerDialog, (mouse_configurations->GetSelectedItemModelPos() , OPMOUSE_SETUP));
				if (dialog)
					dialog->Init(this);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_MANAGE_KEYBOARD:
		{
			OpTreeView* keyboard_configurations = (OpTreeView*)GetWidgetByName("Keyboard_treeview");

			if (keyboard_configurations && keyboard_configurations->GetSelectedItemModelPos() != -1)
			{
				InputManagerDialog* dialog = OP_NEW(InputManagerDialog, (keyboard_configurations->GetSelectedItemModelPos() , OPKEYBOARD_SETUP));
				if (dialog)
					dialog->Init(this);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_RENAME_TOOLBAR_SETUP:
		{
			OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Toolbar_file_treeview");

			if(toolbar_configurations)
			{
				INT32 pos = toolbar_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					toolbar_configurations->EditItem(pos, 0, TRUE);
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DUPLICATE_TOOLBAR_SETUP:
		{
			OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Toolbar_file_treeview");
			if(toolbar_configurations)
			{
				INT32 pos = toolbar_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DuplicateSetupL(pos, OPTOOLBAR_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						OpString filename;
						OpStatus::Ignore(g_setup_manager->GetSetupName(&filename, g_setup_manager->GetToolbarConfigurationCount()-1, OPTOOLBAR_SETUP));
						m_toolbars_model.AddItem(filename.CStr());
					}
					toolbar_configurations->SetSelectedItem(g_setup_manager->GetIndexOfToolbarSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DELETE_TOOLBAR_SETUP:
		{
			OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Toolbar_file_treeview");
			if(toolbar_configurations)
			{
				INT32 pos = toolbar_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DeleteSetupL(pos, OPTOOLBAR_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						m_toolbars_model.Delete(pos);
					}
					toolbar_configurations->SetSelectedItem(g_setup_manager->GetIndexOfToolbarSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_RENAME_MENU_SETUP:
		{
			OpTreeView* menu_configurations = (OpTreeView*)GetWidgetByName("Menu_file_treeview");

			if(menu_configurations)
			{
				INT32 pos = menu_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					menu_configurations->EditItem(pos, 0, TRUE);
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DUPLICATE_MENU_SETUP:
		{
			OpTreeView* menu_configurations = (OpTreeView*)GetWidgetByName("Menu_file_treeview");
			if(menu_configurations)
			{
				INT32 pos = menu_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DuplicateSetupL(pos, OPMENU_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						OpString filename;
						OpStatus::Ignore(g_setup_manager->GetSetupName(&filename, g_setup_manager->GetMenuConfigurationCount()-1, OPMENU_SETUP));
						m_menus_model.AddItem(filename.CStr());
					}
					menu_configurations->SetSelectedItem(g_setup_manager->GetIndexOfMenuSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DELETE_MENU_SETUP:
		{
			OpTreeView* menu_configurations = (OpTreeView*)GetWidgetByName("Menu_file_treeview");
			if(menu_configurations)
			{
				INT32 pos = menu_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DeleteSetupL(pos, OPMENU_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						m_menus_model.Delete(pos);
					}
					menu_configurations->SetSelectedItem(g_setup_manager->GetIndexOfMenuSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DUPLICATE_MOUSE_SETUP:
		{
			OpTreeView* mouse_configurations = (OpTreeView*)GetWidgetByName("Mouse_treeview");
			if(mouse_configurations)
			{
				INT32 pos = mouse_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DuplicateSetupL(pos, OPMOUSE_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						OpString filename;
						OpStatus::Ignore(g_setup_manager->GetSetupName(&filename, g_setup_manager->GetMouseConfigurationCount()-1, OPMOUSE_SETUP));
						m_mice_model.AddItem(filename.CStr());
					}
					mouse_configurations->SetSelectedItem(g_setup_manager->GetIndexOfMouseSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_RENAME_MOUSE_SETUP:
		{
			OpTreeView* mouse_configurations = (OpTreeView*)GetWidgetByName("Mouse_treeview");

			if(mouse_configurations)
			{
				INT32 pos = mouse_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					mouse_configurations->EditItem(pos, 0, TRUE);
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DELETE_MOUSE_SETUP:
		{
			OpTreeView* mouse_configurations = (OpTreeView*)GetWidgetByName("Mouse_treeview");
			if(mouse_configurations)
			{
				INT32 pos = mouse_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DeleteSetupL(pos, OPMOUSE_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						m_mice_model.Delete(pos);
					}
					mouse_configurations->SetSelectedItem(g_setup_manager->GetIndexOfMouseSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DUPLICATE_KEYBOARD_SETUP:
		{
			OpTreeView* keyboard_configurations = (OpTreeView*)GetWidgetByName("Keyboard_treeview");
			if(keyboard_configurations)
			{
				INT32 pos = keyboard_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DuplicateSetupL(pos, OPKEYBOARD_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						OpString filename;
						OpStatus::Ignore(g_setup_manager->GetSetupName(&filename, g_setup_manager->GetKeyboardConfigurationCount()-1, OPKEYBOARD_SETUP));
						m_keyboards_model.AddItem(filename.CStr());
					}
					keyboard_configurations->SetSelectedItem(g_setup_manager->GetIndexOfKeyboardSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_DELETE_KEYBOARD_SETUP:
		{
			OpTreeView* keyboard_configurations = (OpTreeView*)GetWidgetByName("Keyboard_treeview");
			if(keyboard_configurations)
			{
				INT32 pos = keyboard_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					TRAPD(err, g_setup_manager->DeleteSetupL(pos, OPKEYBOARD_SETUP));
					if (OpStatus::IsSuccess(err))
					{
						m_keyboards_model.Delete(pos);
					}
					keyboard_configurations->SetSelectedItem(g_setup_manager->GetIndexOfKeyboardSetup());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_RENAME_KEYBOARD_SETUP:
		{
			OpTreeView* keyboard_configurations = (OpTreeView*)GetWidgetByName("Keyboard_treeview");

			if(keyboard_configurations)
			{
				INT32 pos = keyboard_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					keyboard_configurations->EditItem(pos, 0, TRUE);
				}
			}			return TRUE;
		}

#ifdef MSWIN
		case OpInputAction::ACTION_SHOW_DEFAULT_APPLICATION:
		{
			OperaInstaller installer;
			if (!installer.AssociationsAPIReady())
			{
				//TODO: Display an error message here. Will have to be taken care of as part of WP9, because
				//it requires new strings. Just returning for now to make sure we at least have a consistent behavior
				return TRUE;
			}
			if (installer.UseSystemDefaultProgramsDialog())
			{
				IApplicationAssociationRegistrationUI* pAAR;

				HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI,
											  NULL,
											  CLSCTX_INPROC,
											  __uuidof(IApplicationAssociationRegistrationUI),
											  (void**)&pAAR);
				if (SUCCEEDED(hr))
				{
					hr = pAAR->LaunchAdvancedAssociationUI(UNI_L("Opera Internet Browser"));

					pAAR->Release();
				}

				return TRUE;
			}
			DefaultApplicationDialog* dialog = OP_NEW(DefaultApplicationDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
#endif

		case OpInputAction::ACTION_EDIT_SITE_PREFERENCES:
		{
			OpStringC default_site_name(UNI_L("*.*"));

			ServerPropertiesDialog* dialog = OP_NEW(ServerPropertiesDialog, (default_site_name, FALSE));
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}

		case OpInputAction::ACTION_ADD_USER_SEARCH:
		{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			OpTreeView* searches = (OpTreeView*)GetWidgetByName("Web_search_treeview");
			if (searches)
			{
				SearchEngineItem *search_engine = OP_NEW(SearchEngineItem, ());

				if(search_engine != NULL)
				{
					OpString charset;
					charset.Set("utf-8");
					EditSearchEngineDialog *dlg = OP_NEW(EditSearchEngineDialog, ());

					if (dlg)
						dlg->Init(this, search_engine, charset, NULL);
					else
					{
						OP_DELETE(search_engine);
					}
				}
			}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			return TRUE;
		}

		case OpInputAction::ACTION_EDIT_USER_SEARCH:
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			EditSearchEntry();
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			return TRUE;

		case OpInputAction::ACTION_DELETE_USER_SEARCH:
		{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			OpTreeView* searches = (OpTreeView*)GetWidgetByName("Web_search_treeview");
			if (searches)
			{
				SearchTemplate *item = (SearchTemplate *)searches->GetSelectedItem();

				if(item != NULL && item->IsDeletable())
				{
					RemoveSearchDialogController* controller = OP_NEW(RemoveSearchDialogController, ());
					if (controller && OpStatus::IsSuccess(controller->Init(item)))
					{
						ShowDialog(controller, g_global_ui_context, this);
					}
					else
					{
						OP_DELETE(controller);
					}
				}
			}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			return TRUE;
		}

#ifdef SUPPORT_VISUAL_ADBLOCK
		case OpInputAction::ACTION_CONTENTBLOCK_DETAILS:
		{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
			OpVector<ContentFilterItem> content_to_block;
#else
			ContentBlockTreeModel content_to_block;
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

			BrowserDesktopWindow::OpenContentBlockDetails(this, action, content_to_block);
			return TRUE;
		}
		break;
#endif // SUPPORT_VISUAL_ADBLOCK

		case OpInputAction::ACTION_MOVE_ITEM_UP:
		case OpInputAction::ACTION_MOVE_ITEM_DOWN:
			{
				OpTreeView* treeview = NULL;
				OpButton* button_up = NULL;
				OpButton* button_down = NULL;

				treeview = (OpTreeView*) GetWidgetByName("Web_search_treeview");
				button_up = (OpButton*) GetWidgetByName("Search_up");
				button_down = (OpButton*) GetWidgetByName("Search_down");

				if (treeview && button_up && button_down && (treeview->IsFocused() || button_up->IsFocused() || button_down->IsFocused()))
				{
					return TRUE;
				}
				break;
			}
		case OpInputAction::ACTION_DELETE_LOCAL_STORAGE:
			{
				m_local_storage_model.RemoveAll();
				// fall through, handled in GlobalUIContext
			}

		default:
			break;
	}
	return Dialog::OnInputAction(action);
}


void NewPreferencesDialog::OnSettingsChanged(DesktopSettings* settings)
{
	BOOL advanced_changed = IsPageChanged(AdvancedPage);

	Dialog::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_KEYBOARD_SETUP) || settings->IsChanged(SETTINGS_MOUSE_SETUP))
	{
		TRAPD(err, g_setup_manager->ScanSetupFoldersL());
		// Only update models, or we can lose checkbox settings
		InitMouse();
		InitKeyboard();
	}
	if (settings->IsChanged(SETTINGS_LANGUAGE))
	{
		// If the UI lanuage changes the whole dialog has to reinitialize.
		// Close the dialog and recreate
		m_recreate_dialog = TRUE;
		CloseDialog(TRUE, FALSE);
	}

	// Advanced hasn't really changed yet
	SetPageChanged(AdvancedPage, advanced_changed);
}


UINT32 NewPreferencesDialog::OnOk()
{
	s_last_selected_prefs_page = GetCurrentPage();

	if (IsPageChanged(GeneralPage))
		SaveGeneral();
	if (IsPageChanged(WandPage))
		SaveWand();
	if (IsPageChanged(SearchPage))
		SaveSearch();
	if (IsPageChanged(WebPagesPage))
		SaveWebPages();
	if (IsPageChanged(AdvancedPage))
		SaveAdvanced();

	if (!IsPageChanged(GeneralPage) &&
		!IsPageChanged(WandPage) &&
		!IsPageChanged(SearchPage) &&
		!IsPageChanged(WebPagesPage) &&
		!IsPageChanged(AdvancedPage))
	{
		return 0;
	}

	BOOL update_windows = TRUE; // FIXME FOR SPEED
	BOOL update_windows_defaults = TRUE; // FIXME FOR SPEED

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars) == m_initial_show_scrollbars &&
		g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProgressDlg) == m_initial_show_progress_dlg &&
		g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale) == m_initial_scale &&
		g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowWinSize) == m_initial_show_win_size &&
		g_pcdoc->GetIntegerPref(PrefsCollectionDoc::RenderingMode) == m_initial_rendering_mode)
	{
		update_windows_defaults = FALSE;
	}

	// The following should be optimized: FIXME FOR SPEED
	g_application->SettingsChanged(SETTINGS_MENU_SETUP);
	g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
	g_application->SettingsChanged(SETTINGS_KEYBOARD_SETUP);
	g_application->SettingsChanged(SETTINGS_MOUSE_SETUP);

	// needed both for multimedia, fonts&colors and language:
	if( update_windows )
	{
		windowManager->UpdateWindows(TRUE);
	}

	if (update_windows_defaults)
	{
		// CLEANMEUP: The following for loop should really be part of UpdateAllWindowDefaults
	    for (Window *w = windowManager->FirstWindow(); w; w = w->Suc())
		{
			if (w->HasFeature(WIN_FEATURE_NAVIGABLE)) // document windows
			{
				w->GetWindowCommander()->SetLayoutMode(g_pcdoc->GetIntegerPref(PrefsCollectionDoc::RenderingMode) == 0 ? OpWindowCommander::NORMAL : OpWindowCommander::ERA);
			}
		}

		// Update zoom of open windows etc.
		windowManager->UpdateAllWindowDefaults(
			g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars),
			g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProgressDlg),
			FALSE,
			g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale),
			g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowWinSize));

		// Prevents a "..." title on tabs in speed dialog mode caused by Window::UpdateWindowDefaults(). Bug #268508. [espen 2007-06-20]
		g_application->SettingsChanged(SETTINGS_LANGUAGE);
	}

	TRAPD(err, g_prefsManager->CommitL());

	return 0;
}

void NewPreferencesDialog::OnCancel()
{
	s_last_selected_prefs_page = GetCurrentPage();
}

void NewPreferencesDialog::OnClose(BOOL user_initiated)
{
	if (m_recreate_dialog)
	{
		// Create a new dialog with settings
		OP_STATUS err = RecreateDialog();
		OP_ASSERT(OpStatus::IsSuccess(err));//Should only fail in case of OOM!
		OpStatus::Ignore(err);
	}
	Dialog::OnClose(user_initiated);
}

void NewPreferencesDialog::OnInitVisibility()
{
#if defined(_MACINTOSH_)
	ShowWidget("Language_dropdown", FALSE);
	ShowWidget("Smooth_scrolling_checkbox", FALSE);
#else
	ShowWidget("Language_mac_label", FALSE);
#endif
}

void NewPreferencesDialog::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
#ifdef WEB_TURBO_MODE
	if(id == OpPrefsCollection::UI && pref == PrefsCollectionUI::OperaTurboMode)
	{
		OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("turbo_dropdown");
		if(dropdown)
		{
			dropdown->SelectItem(newvalue, TRUE);
		}
	}
#endif
}

static const Str::LocaleString popup_stringid[4] =
{
	Str::SI_IDSTR_POPUP_ALWAYS,
	Str::SI_IDSTR_POPUP_BACKGROUND,
	Str::SI_IDSTR_POPUP_REQUESTED,
	Str::SI_IDSTR_POPUP_NEVER
};

OP_STATUS NewPreferencesDialog::InitGeneral()
{
	OpDropDown* startup_dropdown = (OpDropDown*) GetWidgetByName("Startup_mode_dropdown");

	if (startup_dropdown)
	{
		OpString entry;

		g_languageManager->GetString(Str::D_STARTUP_LAST_TIME, entry);
		startup_dropdown->AddItem(entry.CStr());

		g_languageManager->GetString(Str::D_STARTUP_SAVED_SESSIONS, entry);
		startup_dropdown->AddItem(entry.CStr());

		g_languageManager->GetString(Str::D_STARTUP_WITH_HOMEPAGE, entry);
		startup_dropdown->AddItem(entry.CStr());

		Str::LocaleString startup_str(Str::D_STARTUP_WITH_NOPAGE);
		if (!g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace))
			startup_str = Str::S_START_BLANK_PAGE;
		g_languageManager->GetString(startup_str, entry);

		startup_dropdown->AddItem(entry.CStr());

		g_languageManager->GetString(Str::DI_IDM_SHOW_STARTUP_DIALOG, entry);
		startup_dropdown->AddItem(entry.CStr());

		STARTUPTYPE startupType = STARTUPTYPE(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType));

		switch(startupType)
		{
		case STARTUP_HISTORY:
			startup_dropdown->SelectItem(1, TRUE);
			break;
		case STARTUP_WINHOMES:
		case STARTUP_HOME:
			startup_dropdown->SelectItem(2, TRUE);
			break;
		case STARTUP_CONTINUE:
			startup_dropdown->SelectItem(0, TRUE);
			break;
		case STARTUP_NOWIN:
		default:
			startup_dropdown->SelectItem(3, TRUE);
		}

		BOOL show_startup_dialog = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowStartupDialog);

		if (show_startup_dialog)
		{
			startup_dropdown->SelectItem(4, TRUE);
		}
	}

	OpStringC homepage = g_pccore->GetStringPref(PrefsCollectionCore::HomeURL);
	OpEdit* homepage_edit = GetWidgetByName<OpEdit>("Startpage_edit", WIDGET_TYPE_EDIT);
	if (homepage_edit)
	{
		homepage_edit->SetText(homepage.CStr());
		homepage_edit->SetForceTextLTR(TRUE);
	}

#ifdef PERMANENT_HOMEPAGE_SUPPORT
	BOOL permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
	if (permanent_homepage)
	{
		if (startup_dropdown)
		{
			startup_dropdown->SetEnabled(FALSE);
		}
		if (homepage_edit)
		{
			homepage_edit->SetEnabled(FALSE);
		}
		OpButton* current_button = (OpButton*)GetWidgetByName("Startpage_use_current_button");
		if (current_button)
		{
			current_button->SetVisibility(FALSE);
		}
	}
#endif


/*
	OpDropDown* language_dropdown = (OpDropDown*) GetWidgetByName("Language_dropdown");

	if (language_dropdown)
	{
		language_dropdown->AddItem(UNI_L("English"));
	}
*/
/*
	int window_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::SDI);

	switch (window_mode)
	{
	case 0:
		SetWidgetValue("Open_in_tabs_checkbox", TRUE);
		SetWidgetValue("Show_close_button_checkbox", TRUE);
		break;
	case 1:
		SetWidgetValue("Show_close_button_checkbox", TRUE);
		SetWidgetEnabled("Show_close_button_checkbox", FALSE);
		break;
	case 2:
		SetWidgetValue("Open_in_tabs_checkbox", TRUE);
		SetWidgetValue("Show_close_button_checkbox", FALSE);
		break;
	}
*/
	OpDropDown* popups_dropdown = (OpDropDown*) GetWidgetByName("Popups_dropdown");

	if (popups_dropdown)
	{
		OpString popupstrategy;
		int got_index;

		for (int i = 0; i < 4; i++)
		{
			g_languageManager->GetString(popup_stringid[i], popupstrategy);
			if(!popupstrategy.IsEmpty())
				popups_dropdown->AddItem(popupstrategy.CStr(), -1, &got_index);
		}

		int selected_pos = 0;
		if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget))
		{
			selected_pos = 3;
		}
		else if (g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination) == POPUP_WIN_BACKGROUND)
		{
			selected_pos = 1;
		}
		if (g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups))
		{
			selected_pos = 2;
		}

		popups_dropdown->SelectItem(selected_pos, TRUE);
	}

	RETURN_IF_ERROR(InitLanguages());

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveGeneral()
{
	OpDropDown* startup_dropdown = (OpDropDown*) GetWidgetByName("Startup_mode_dropdown");

	if (startup_dropdown)
	{

		int selected = startup_dropdown->GetSelectedItem();

		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowStartupDialog, selected == 4));

		BOOL permanent_homepage = FALSE;
#ifdef PERMANENT_HOMEPAGE_SUPPORT
		permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
#endif
		if (!permanent_homepage)
		{
			switch (selected)
			{
			case 0:
				TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, STARTUP_CONTINUE));
				break;
			case 1:
				TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, STARTUP_HISTORY));
				break;
			case 2:
				TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, STARTUP_HOME));
				break;
			case 3:
				TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, STARTUP_NOWIN));
				break;
			}
		}
	}

	OpWidget* homepage_edit = GetWidgetByName("Startpage_edit");
	if (homepage_edit)
	{
		OpString homepage;
		homepage_edit->GetText(homepage);

		// We only want to write the new homepage if the user has actually changed it (related to DSK-325446)
		OpStringC homepage_on_init = g_pccore->GetStringPref(PrefsCollectionCore::HomeURL);
		if (homepage.Compare(homepage_on_init))
		{
			TRAPD(err, g_pccore->WriteStringL(PrefsCollectionCore::HomeURL, homepage));
		}
	}

	OpDropDown* popups_dropdown = (OpDropDown*) GetWidgetByName("Popups_dropdown");

	if (popups_dropdown)
	{
		int selected_pos = popups_dropdown->GetSelectedItem();
		if (selected_pos == 3)
		{
			TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, TRUE));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_IGNORE));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, FALSE));
		}
		else if (selected_pos == 2)
		{
			TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, FALSE));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_NEW));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, TRUE));
		}
		else if (selected_pos == 1)
		{
			TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, FALSE));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_BACKGROUND));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, FALSE));
		}
		else
		{
			TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, FALSE));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_NEW));
			TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, FALSE));
		}
	}

/*
	int sdi = !GetWidgetValue("Open_in_tabs_checkbox");
	int close_button = GetWidgetValue("Show_close_button_checkbox");
	int method = sdi ? 1 : close_button ? 0 : 2;

	g_pcui->WriteIntegerL(PrefsCollectionUI::SDI, method);
*/
	if (m_language_changed)
	{
		RETURN_IF_ERROR(SaveLanguages());
	}
	return OpStatus::OK;
}


static void GetCurrentLanguageFile( OpString &path )
{
	OpFile current_lng_file;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::LanguageFile, current_lng_file));
	path.Set(current_lng_file.GetFullPath());
}


OP_STATUS NewPreferencesDialog::InitLanguages()
{
	// find the accept languages

	LanguageCodeInfo language_info;

	OpStringC accept_languages = g_pcnet->GetStringPref(PrefsCollectionNetwork::AcceptLanguage);

	OpString current_language_file_path;
	GetCurrentLanguageFile( current_language_file_path );

	OpFile lng_file;
	OpStatus::Ignore(lng_file.Construct(current_language_file_path.CStr()));

	OpString lang_country_name, lang_code_in_file;

	{
		OpString file_build;
		LangAccessorLight accessor;
		accessor.LoadL(&lng_file, NULL);
		accessor.GetFileInfo(lang_code_in_file, lang_country_name, file_build);

		if (lang_country_name.HasContent())
			lang_country_name.AppendFormat(UNI_L(" [%s]"), lang_code_in_file.CStr());
	}

	OpDropDown* language_dropdown = (OpDropDown*)GetWidgetByName("Language_dropdown");

	if(language_dropdown)
	{
		language_dropdown->Clear();

		if (accept_languages.HasContent())
		{
			OpString userdefined, language_code_only;
			g_languageManager->GetString(Str::SI_IDSTR_USERDEF_LANGUAGE, userdefined);

			BOOL file_lang_added = FALSE;
			const uni_char* str_lang = accept_languages.CStr();
			do
			{
				// Ignore empty or single-letter strings
				if (SplitAcceptLanguage(&str_lang, language_code_only) < 2)
					continue;

				if (language_code_only.CompareI(lang_code_in_file) != 0)
				{
					// Retrieve language name from language codes list
					OpString langname;
					language_info.GetNameByCode(language_code_only, langname);

					if (langname.Compare(language_code_only) == 0)
					{
						if (!ParseRegionSuffix(language_code_only,langname))
							langname.Set(userdefined);
					}

					// Tack the ISO 639 to the end two columns, one with code, one with name
					langname.AppendFormat(UNI_L(" [%s]"), language_code_only.CStr());

					language_dropdown->AddItem(langname.CStr());
				}
				if (!file_lang_added)
				{
					// add language file as second (or first, depending if it's the same as accept language) language
					if (lang_country_name.HasContent())
					{
						language_dropdown->AddItem(lang_country_name.CStr());
					}
					file_lang_added = TRUE;
				}
			}
			while (str_lang);
		}
		else if (lang_country_name.HasContent())
		{
			language_dropdown->AddItem(lang_country_name.CStr());
		}

		for (UINT32 i = 0; i < language_info.GetCount(); i++)
		{
			LanguageCodeInfoItem* item = language_info.GetItem(i);
			if (item)
			{
				OpString name, code;
				item->GetName(name);
				item->GetCode(code);
				name.AppendFormat(UNI_L(" [%s]"), code.CStr());

				// if (code.CompareI(lang_code_in_file) != 0)
				{
					language_dropdown->AddItem(name.CStr());
				}
			}
		}
#ifdef _MACINTOSH_
		INT32 selected_item = language_dropdown->GetSelectedItem();
		const uni_char* selected_language = language_dropdown->GetItemText(selected_item);
		if (selected_language)
		{
			SetWidgetText("Language_mac_label", selected_language);
		}
#endif
	}
	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::SaveLanguages()
{
	BOOL is_ui_language_saved = FALSE, is_web_language_saved = FALSE;

	OpStringC accept_languages_c =
		g_pcnet->GetStringPref(PrefsCollectionNetwork::AcceptLanguage);
	OpString accept_languages; // web languages
	accept_languages.Set(accept_languages_c);

	OpString main_language, main_code;
	OpDropDown* language_dropdown = (OpDropDown*)GetWidgetByName("Language_dropdown");

	if (language_dropdown)
	{
		main_language.Set(language_dropdown->GetItemText(language_dropdown->GetSelectedItem()));
	}

	int bracket_pos = main_language.Find("[");
	if (bracket_pos != KNotFound)
	{
		main_code.Set(main_language.CStr()+bracket_pos+1);

		if (main_code.HasContent())
		{
			bracket_pos = main_code.Find("]");
			if (bracket_pos != KNotFound)
			{
				main_code[bracket_pos] = 0;
				{
					OpString insert;
					insert.Set(main_code);

					if (accept_languages.HasContent())
					{
						insert.Append(",");
					}
					accept_languages.Insert(0, insert);
				}
			}
		}
	}

	// SET WEB LANGUAGES
	if (accept_languages.HasContent())
	{
		OpString new_accept_languages, language_code_only;
		double q_value = 1.0;
		int langcode_count = 0;
		const uni_char* str_lang = accept_languages.CStr();
		do
		{
			// Ignore empty or single-letter strings
			if (SplitAcceptLanguage(&str_lang, language_code_only) < 2)
				continue;

			if (langcode_count > 0)
			{
				if (main_code.CompareI(language_code_only) == 0)
				{
					// don't add same language twice
					continue;
				}
				new_accept_languages.Append(",");
			}

			new_accept_languages.Append(language_code_only);

			if (langcode_count > 0)
				new_accept_languages.AppendFormat(UNI_L(";q=%3.1f"), q_value);

			if(q_value >= 0.2)
				q_value -= 0.1;

			langcode_count++;
		}
		while (str_lang && langcode_count < 10);

		TRAPD(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::AcceptLanguage, new_accept_languages));
		is_web_language_saved = TRUE;
	}

	// SET UI LANGUAGE
	OpString current_language_file_path;
	GetCurrentLanguageFile( current_language_file_path );

	OpFile lng_file;
	lng_file.Construct(current_language_file_path.CStr());

	OpString lang_code_in_file, new_lang_file;
	OpString lngfolder;
	BOOL language_code_has_changed = FALSE;

	{
		OpString lang_country_name, file_build;
		LangAccessorLight accessor;
		accessor.LoadL(&lng_file, NULL);
		accessor.GetFileInfo(lang_code_in_file, lang_country_name, file_build);
	}

	new_lang_file.Set(current_language_file_path);
	if (m_language_changed && main_code.Compare(lang_code_in_file) != 0)
	{
		language_code_has_changed = TRUE;

		// try to find a corresponding language file on disk, set value of

#ifdef DIRECTORY_SEARCH_SUPPORT

		OpString lang_code;
		lang_code.Set(main_code);

		// fix for DSK-288727
		//1 use the language code as file path, go to 2 if fails 
		//2 try to match in a lookup table, go to 3 if fails 
		//3 remove the region suffix and go to 1
		// e.g. for zh-hk search path is "zh-hk -> zh-tw", for es-** it's "es-** -> es -> es-la"
		BOOL lookup_table = FALSE;
		BOOL removed_country_code = FALSE;
		while (TRUE)
		{
			OpAutoVector<OpString> lng_file_candidate_locations;
			PrepareLanguageFolders(lang_code, lng_file_candidate_locations);
			SearchLanguageFileInDirectory(lng_file_candidate_locations,lang_code,new_lang_file,lngfolder);

			if (lngfolder.HasContent())
				break;

			// Search the table for a fallback
			BOOL found_alternative = FALSE;
			if (!lookup_table)
			{
				lookup_table = TRUE;
				for (UINT32 i=0; i<ARRAY_SIZE(langcodeMap); i++)
				{
					if (lang_code.CompareI(langcodeMap[i].languageCode) == 0)
					{
						lang_code.Set(langcodeMap[i].languageFileName);
						found_alternative = TRUE;
						break;
					}
				}
			}
			if (found_alternative)
				continue;

			// remove the region code and try again
			if (!removed_country_code && main_code.Length() > 4 && main_code.CStr()[2] == UNI_L('-'))
			{
				lang_code.Set(main_code.CStr(),2);
				removed_country_code = TRUE;
			}
			else
			{
				break;
			}
		}

		if (lang_code.CompareI(lang_code_in_file) == 0)
			language_code_has_changed = FALSE;
#endif // DIRECTORY_SEARCH_SUPPORT
	}

	if ( !current_language_file_path.IsEmpty() )
	{
		if( current_language_file_path.CompareI(new_lang_file) == 0)
		{
			if( !language_code_has_changed )
			{
				// This will happen if we change to a country code that does not have an lng-file and back again.
				// Switching back would otherwise trigger the error message when it should not.
				is_ui_language_saved = TRUE;
			}
		}
		else
		{
			if( IsValidLanguageFileVersion(new_lang_file, this, TRUE) )
			{
				OpFile languagefile;
				OP_STATUS err = languagefile.Construct(new_lang_file.CStr());
				BOOL exists;
				if (OpStatus::IsSuccess(err) && OpStatus::IsSuccess(languagefile.Exists(exists)) && exists)
				{
					TRAP(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::LanguageFile, &languagefile));
					TRAP(err, g_pcfiles->WriteDirectoryL(OPFILE_LANGUAGE_FOLDER, lngfolder.CStr()));

					is_ui_language_saved = TRUE;

					OpTreeView* searches = (OpTreeView*)GetWidgetByName("Web_search_treeview");
					if (searches)
						searches->SetTreeModel(NULL);

					OpStatus::Ignore(FileUtils::UpdateDesktopLocaleFolders(true));

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
					if (g_searchEngineManager->HasLoadedConfig())
					{
						TRAP(err, g_searchEngineManager->LoadSearchesL()); // Search engines can be localized
					}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

					g_application->SettingsChanged(SETTINGS_LANGUAGE);
					g_application->SettingsChanged(SETTINGS_MENU_SETUP);
					g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
				}
			}
		}
	}


	if (is_web_language_saved && !is_ui_language_saved) // notify users about changes that didn't happen
	{
		OpString title, message, format_str;

		g_languageManager->GetString(Str::S_LANGUAGE_CHANGE_NOTIFICATION_TITLE, title);
		g_languageManager->GetString(Str::S_LANGUAGE_CHANGE_NOTIFICATION_MESSAGE, format_str);
		message.AppendFormat(format_str.CStr(), main_language.CStr());

		SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
		if (dialog)
		{
			return dialog->Init(WINDOW_NAME_LANGUAGE_CHANGE_NOTIFICATION, title, message, GetParentDesktopWindow(), TYPE_OK);
		}
	}
	return OpStatus::OK;
}

void NewPreferencesDialog::PrepareLanguageFolders(const OpString& main_code, OpVector<OpString>& lng_file_candidate_locations)
{
	OpString tmp_storage1;
	OpString tmp_storage2;
	const OpStringC resourcedirectory = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_RESOURCES_FOLDER, tmp_storage1);
	const OpStringC profiledirectory = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage2);

#if defined (_UNIX_DESKTOP_)
	OpString* resource_locale_folder = OP_NEW(OpString, ()); // Deleted by the OpAutoVector
	if (resource_locale_folder)
	{
		OpString tmp_storage;
		resource_locale_folder->Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_LOCALE_FOLDER, tmp_storage));
		lng_file_candidate_locations.Add(resource_locale_folder);
	}

	OpString* resource_locale_code_folder = OP_NEW(OpString, ()); // Deleted by the OpAutoVector
	if (resource_locale_code_folder)
	{
		resource_locale_code_folder->Set(resource_locale_folder->CStr());
		resource_locale_code_folder->Append(main_code);
		lng_file_candidate_locations.Add(resource_locale_code_folder);
	}

	// Match directory using upper case letters at end of country code
	int pos = main_code.FindFirstOf('-');
	if( pos != KNotFound )
	{
		resource_locale_code_folder = OP_NEW(OpString, ()); // Deleted by the OpAutoVector
		if (resource_locale_code_folder)
		{
			resource_locale_code_folder->Set(resource_locale_folder->CStr());
			resource_locale_code_folder->Append(PATHSEP);
			OpString tmp;
			tmp.Set(main_code);
			for( int i=0; i<tmp.Length(); i++ )
			{
				if( tmp.CStr()[i] == '-' )
				{
					for( i++; i<tmp.Length(); i++ )
					{
						tmp.CStr()[i] = uni_toupper( tmp.CStr()[i] );
					}
				}
			}
			resource_locale_code_folder->Append(tmp);
			lng_file_candidate_locations.Add(resource_locale_code_folder);
		}
	}

#else
	OpString* resourcedirectory_vector = OP_NEW(OpString, ()); // Deleted by the OpAutoVector
	if (resourcedirectory_vector)
	{
		resourcedirectory_vector->Set(resourcedirectory);
		lng_file_candidate_locations.Add(resourcedirectory_vector);
	}
#endif

#if defined MSWIN
	OpString* exe_locale_code_folder = OP_NEW(OpString, ());
	if (exe_locale_code_folder)
	{
		exe_locale_code_folder->Set(resourcedirectory);
		exe_locale_code_folder->Append("locale");
		exe_locale_code_folder->Append(PATHSEP);
		exe_locale_code_folder->Append(main_code);
		lng_file_candidate_locations.Add(exe_locale_code_folder);
	}
#endif

	// All platforms
	OpString* profile_locale_lng_folder = OP_NEW(OpString, ()); // Deleted by the OpAutoVector
	if (profile_locale_lng_folder)
	{
		profile_locale_lng_folder->Set(profiledirectory);
		profile_locale_lng_folder->Append(UNI_L("locale"));
		profile_locale_lng_folder->Append(PATHSEP);
		profile_locale_lng_folder->Append(main_code);
		lng_file_candidate_locations.Add(profile_locale_lng_folder);
	}
}

void NewPreferencesDialog::SearchLanguageFileInDirectory(OpVector<OpString>& lng_file_candidate_locations, const OpString& main_code, OpString& language_file_path, OpString& language_file_folder)
{
	OpString lngfilefolder;
	OpString searchtemplate;

	for (UINT folder_no = 0; folder_no < lng_file_candidate_locations.GetCount(); folder_no++)
	{
		OpString* lngptr = lng_file_candidate_locations.Get(folder_no);
		if (!lngptr)
		{
			continue;
		}
		lngfilefolder.SetL(lngptr->CStr());

		searchtemplate.Set("*.lng");

		if( lngfilefolder.HasContent())
		{
			OpFolderLister* lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, searchtemplate.CStr(), lngfilefolder.CStr());
			if( !lister )
			{
				// Will happen if directory does not exists
				continue;
			}

			BOOL more = lister->Next();

			while( more )
			{
				if( !lister->IsFolder() )
				{
					const uni_char *candidate = lister->GetFullPath();
					if( candidate )
					{
						OpFile lng_file;
						lng_file.Construct(candidate);

						OpString lang_code_in_file, lang_country_name, file_build;

						LangAccessorLight accessor;
						accessor.LoadL(&lng_file, NULL);
						accessor.GetFileInfo(lang_code_in_file, lang_country_name, file_build);

						if ( uni_stricmp(lang_code_in_file.CStr(), main_code.CStr()) == 0)
						{
							language_file_path.Set(candidate);
							language_file_folder.Set(lngfilefolder);
						}
					}
				}
				more = lister->Next();
			}
			OP_DELETE(lister);
			lister = NULL;
		}
	}
}


static const struct personal_fields_struct
{
	const char *widget_name;
	PrefsCollectionCore::stringpref pref;
} s_personal_fields[] =
{
	{"Firstname_edit", PrefsCollectionCore::Firstname},
	{"Lastname_edit", PrefsCollectionCore::Surname},
	{"Address_edit", PrefsCollectionCore::Address},
	{"City_edit", PrefsCollectionCore::City},
	{"Region_edit", PrefsCollectionCore::State},
	{"Postal_edit", PrefsCollectionCore::Zip},
	{"Country_edit", PrefsCollectionCore::Country},
	{"Telephone_edit", PrefsCollectionCore::Telephone},
	{"Mobile_edit", PrefsCollectionCore::Telefax},
	{"Email_edit", PrefsCollectionCore::EMail},
	{"Homepage_edit", PrefsCollectionCore::Home},
	{"Special1_edit", PrefsCollectionCore::Special1},
	{"Special2_edit", PrefsCollectionCore::Special2},
	{"Special3_edit", PrefsCollectionCore::Special3}
};

OP_STATUS NewPreferencesDialog::InitWand()
{
	for (size_t i = 0; i < ARRAY_SIZE(s_personal_fields); i++)
	{
		OpWidget* widget = GetWidgetByName(s_personal_fields[i].widget_name);
		RETURN_IF_ERROR(WidgetPrefs::GetStringPref(widget, s_personal_fields[i].pref));
	}
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_wand_checkbox"), PrefsCollectionCore::EnableWand);

	OpWidget* image = GetWidgetByName("Wand_image");
	if (image)
	{
		image->SetSkinned(TRUE);
		image->GetBorderSkin()->SetImage("Wand");
	}

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveWand()
{
	for (size_t i = 0; i < ARRAY_SIZE(s_personal_fields); i++)
	{
		OpWidget* widget = GetWidgetByName(s_personal_fields[i].widget_name);
		RETURN_IF_ERROR(WidgetPrefs::SetStringPref(widget, s_personal_fields[i].pref));
	}

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_wand_checkbox"), PrefsCollectionCore::EnableWand);
	g_wand_manager->SetActive(g_pccore->GetIntegerPref(PrefsCollectionCore::EnableWand));

	return OpStatus::OK;
}

static const int scale_values[] =
{
	20, 30, 50, 70, 80, 90, 100, 110, 120, 150, 180, 200, 250, 300, 400, 500, 600, 700, 800, 900, 1000
};

OP_STATUS NewPreferencesDialog::InitWebPages()
{
    UpdateWidgetFont("Normal_font_button", HE_DOC_ROOT);
    UpdateWidgetFont("Monospace_button", HE_PRE);

    COLORREF background_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND);
    COLORREF link_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_LINK);
    COLORREF visited_link_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_VISITED_LINK);

    OnColorSelected("Background_color_button", background_color);
    OnColorSelected("Color_link_button", link_color);
    OnColorSelected("Color_visited_button", visited_link_color);

    OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Default_zoom_dropdown");
    if (dropdown)
	{
        int scale = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale);

        OpString buffer;
        if (buffer.Reserve(30))
		{
			for (size_t i = 0; i < ARRAY_SIZE(scale_values); i++)
			{
				uni_ltoa(scale_values[i], buffer.CStr(), 10);
				dropdown->AddItem(buffer.CStr());

				if (scale == scale_values[i])
				{
					dropdown->SelectItem(i, TRUE);
				}
			}
		}
	}

    dropdown = (OpDropDown*)GetWidgetByName("Page_image_dropdown");
    if( dropdown )
	{
		OpString str;
		g_languageManager->GetString(Str::M_IMAGE_MENU_SHOW, str);
		dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::M_IMAGE_MENU_CACHED, str);
		dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::M_IMAGE_MENU_NO, str);
		dropdown->AddItem(str.CStr());

        // dropdown->SelectItem(3 - g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowImageState), TRUE);
        switch (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowImageState))
        {
        case 1: // no images
            dropdown->SelectItem(2, TRUE);
            break;
        case 2: // show cached
            dropdown->SelectItem(1, TRUE);
            break;
        case 3: // all images
            dropdown->SelectItem(0, TRUE);
            break;
        }

	}

    WidgetPrefs::GetIntegerPref(GetWidgetByName("Fit_to_width_checkbox"), PrefsCollectionDoc::RenderingMode);
    WidgetPrefs::GetIntegerPref(GetWidgetByName("Underline_link_checkbox"), PrefsCollectionDisplay::LinkHasUnderline);
    WidgetPrefs::GetIntegerPref(GetWidgetByName("Underline_visited_checkbox"), PrefsCollectionDisplay::VisitedLinkHasUnderline);
#ifdef WEB_TURBO_MODE
	dropdown = (OpDropDown*)GetWidgetByName("turbo_dropdown");
	if(dropdown)
	{
		OpString str;
		g_languageManager->GetString(Str::D_OPERA_TURBO_AUTO, str);
		dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::D_OPERA_TURBO_ON, str);
		dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::D_OPERA_TURBO_OFF, str);
		dropdown->AddItem(str.CStr());

		int turbo_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode);
		dropdown->SelectItem(turbo_mode, TRUE);
	}
#endif

    return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::SaveWebPages()
{
    OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Default_zoom_dropdown");
    int scale =
        g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale);

	if (dropdown)
    {
        scale = scale_values[dropdown->GetSelectedItem()];

        TRAPD(err, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::Scale, scale));
    }

	dropdown = (OpDropDown*) GetWidgetByName("Page_image_dropdown");
    if (dropdown)
    {
        int selected_pos = dropdown->GetSelectedItem();
        switch (selected_pos)
        {
			case 0:
			{
				TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::ShowImageState, 3));
				break;
			}
			case 1:
			{
				TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::ShowImageState, 2));
				break;
			}
			case 2:
			{
				TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::ShowImageState, 1));
				break;
			}
        }
    }

	TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::RenderingMode, GetWidgetValue("Fit_to_width_checkbox", 0) ? -1 : 0));

    RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Underline_link_checkbox"), PrefsCollectionDisplay::LinkHasUnderline));
    RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Underline_visited_checkbox"), PrefsCollectionDisplay::VisitedLinkHasUnderline));

#ifdef WEB_TURBO_MODE
	dropdown = (OpDropDown*)GetWidgetByName("turbo_dropdown");
	if (dropdown)
	{
		int turbo_mode = dropdown->GetSelectedItem();
		g_opera_turbo_manager->SetOperaTurboMode(turbo_mode);
	}
#endif

    OpWidget* widget = NULL;

    widget = GetWidgetByName("Color_visited_button");
    if (widget)
    {
        COLORREF color = 0;
        color = widget->GetBackgroundColor(color);
        TRAPD(err, g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_VISITED_LINK, color));
    }

    widget = GetWidgetByName("Color_link_button");
    if (widget)
    {
        COLORREF color = 0;
        color = widget->GetBackgroundColor(color);
        TRAPD(err, g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_LINK, color));
    }

    widget = GetWidgetByName("Background_color_button");
    if (widget)
    {
        COLORREF color = 0;
        color = widget->GetBackgroundColor(color);
        TRAPD(err, g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND, color));
		colorManager->SetBackgroundColor(color);
    }

    // why do we need this block?

#ifdef DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
    LayoutStyle *styl;
#else // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
    Style *styl;
#endif // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
    styl = styleManager->GetStyle(HE_A);
    PresentationAttr pres = styl->GetPresentationAttr();
    pres.Color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_LINK);
    pres.Bold = FALSE;
    pres.Italic = FALSE;
    pres.Underline = GetWidgetValue("Underline_link_checkbox");
    pres.StrikeOut = FALSE;
    //pres.Frame = button;
    styl->SetPresentationAttr(pres);


	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitSites()
{
/*	OpTreeView* server_treeview = (OpTreeView*) GetWidgetByName("Site_treeview");

	if (server_treeview == NULL)
		return OpStatus::ERR;

	server_treeview->SetAutoMatch(FALSE);
	server_treeview->SetShowThreadImage(TRUE);
	server_treeview->SetShowColumnHeaders(FALSE);
	server_treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_EDIT_PROPERTIES)));

	// ?
	TRAPD(op_err, g_url_api->CheckCookiesReadL());
	OpStatus::Ignore(op_err);

	ServerManager::RequireServerManager();

	server_treeview->SetTreeModel(g_server_manager, 0);
	server_treeview->SetMatchType(MATCH_FOLDERS, TRUE);
*/
	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::SaveSites()
{
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitAdvanced(int id)
{
	if( m_advanced_setup.GetCount() == 0 )
	{
		// needed for Toolbars, Menus, Keyboard, Mouse
		TRAPD(err, g_setup_manager->ScanSetupFoldersL());
	}

	if( m_advanced_setup.Find(id) == -1 )
	{
		if( id == ToolbarPage )
		{
			RETURN_IF_ERROR(InitToolbars());
			RETURN_IF_ERROR(InitMenus());
		}
		else if( id == ShortcutPage )
		{
			RETURN_IF_ERROR(InitKeyboard());
			RETURN_IF_ERROR(InitMouse());
		}
		else if( id == SoundPage )
			RETURN_IF_ERROR(InitSounds());
		else if( id == ContentPage )
			RETURN_IF_ERROR(InitDisplay());
		else if( id == FontPage )
			RETURN_IF_ERROR(InitFonts());
		else if( id == TabsPage )
			RETURN_IF_ERROR(InitTabs());
		else if( id == BrowserPage )
			RETURN_IF_ERROR(InitBrowsing());
		else if( id == DownloadPage )
			RETURN_IF_ERROR(InitFiles());
		else if( id == ProgramPage )
			RETURN_IF_ERROR(InitPrograms());
		else if( id == HistoryPage )
			RETURN_IF_ERROR(InitHistory());
		else if( id == SecurityPage )
			RETURN_IF_ERROR(InitSecurity());
		else if( id == NetworkPage )
			RETURN_IF_ERROR(InitNetwork());
		else if( id == CookiePage )
			RETURN_IF_ERROR(InitCookies());
		else if( id == LocalStoragePage)
			RETURN_IF_ERROR(InitLocalStorage());

		m_advanced_setup.Add(id);
	}

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveAdvanced()
{
	if( m_advanced_setup.Find(ToolbarPage) != -1 )
	{
		RETURN_IF_ERROR(SaveToolbars());
		RETURN_IF_ERROR(SaveMenus());
	}
	if( m_advanced_setup.Find(ShortcutPage) != -1 )
	{
		RETURN_IF_ERROR(SaveKeyboard());
		RETURN_IF_ERROR(SaveMouse());
	}
	if( m_advanced_setup.Find(SoundPage) != -1 )
		RETURN_IF_ERROR(SaveSounds());
	if( m_advanced_setup.Find(ContentPage) != -1 )
		RETURN_IF_ERROR(SaveDisplay());
	if( m_advanced_setup.Find(FontPage) != -1 )
		RETURN_IF_ERROR(SaveFonts());
	if( m_advanced_setup.Find(TabsPage) != -1 )
		RETURN_IF_ERROR(SaveTabs());
	if( m_advanced_setup.Find(BrowserPage) != -1 )
		RETURN_IF_ERROR(SaveBrowsing());
	if( m_advanced_setup.Find(DownloadPage) != -1 )
		RETURN_IF_ERROR(SaveFiles());
	if( m_advanced_setup.Find(ProgramPage) != -1 )
		RETURN_IF_ERROR(SavePrograms());
	if( m_advanced_setup.Find(HistoryPage) != -1 )
		RETURN_IF_ERROR(SaveHistory());
	if( m_advanced_setup.Find(SecurityPage) != -1 )
		RETURN_IF_ERROR(SaveSecurity());
	if( m_advanced_setup.Find(NetworkPage) != -1 )
		RETURN_IF_ERROR(SaveNetwork());
	if( m_advanced_setup.Find(CookiePage) != -1 )
		RETURN_IF_ERROR(SaveCookies());
	if( m_advanced_setup.Find(LocalStoragePage) != -1 )
		RETURN_IF_ERROR(SaveLocalStorage());

	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::InitSearch()
{
	OpTreeView* searches = (OpTreeView*)GetWidgetByName("Web_search_treeview");
	if (searches)
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		searches->SetTreeModel(g_searchEngineManager->GetSearchModel());
		searches->SetColumnFixedWidth(0,24); //place only for icon needed
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	}

	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Serches_enabled_checkbox"),
			PrefsCollectionUI::ShowSearchesInAddressfieldAutocompletion));

	return WidgetPrefs::GetIntegerPref(GetWidgetByName("Inline_find_checkbox"), PrefsCollectionUI::UseIntegratedSearch);
}

OP_STATUS NewPreferencesDialog::ConstructSearchMsg()
{
	OpString info_msg;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_PREFERENCES_SEARCH_ENGINE_KEYWORD_HINT, info_msg));

	SearchTemplate* search = g_searchEngineManager->GetDefaultSearch();
	if (search)
	{
		OpString search_name;
		RETURN_IF_ERROR(search->GetEngineName(search_name));
			
		OpString tmp_str;
		int len = info_msg.Length() + search_name.Length() + search->GetKey().Length() + 1;
		if (NULL == tmp_str.Reserve(len))
			return OpStatus::ERR_NO_MEMORY;

		uni_snprintf_ss(tmp_str.CStr(), tmp_str.Capacity(), info_msg.CStr(), search->GetKey().CStr(), search_name.CStr());
		RETURN_IF_ERROR(info_msg.Set(tmp_str));
	
		OpLabel *widget = static_cast<OpLabel*>(GetWidgetByName("Web_search_details"));
		if (widget)
		{
			widget->SetWrap(TRUE);
			widget->SetText(info_msg.CStr());
		}
	}

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveSearch()
{
	return WidgetPrefs::SetIntegerPref(GetWidgetByName("Serches_enabled_checkbox"),
			PrefsCollectionUI::ShowSearchesInAddressfieldAutocompletion);
}


OP_STATUS NewPreferencesDialog::InitToolbars()
{
	OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Toolbar_file_treeview");

	if (toolbar_configurations)
	{
		toolbar_configurations->SetDeselectable(FALSE);

		OpString str;
		g_languageManager->GetString(Str::S_TOOLBAR_SETUP, str);

		m_toolbars_model.SetColumnData(0, str.CStr());

		for (UINT32 i = 0; i < g_setup_manager->GetToolbarConfigurationCount(); i++)
		{
			RETURN_IF_ERROR(g_setup_manager->GetSetupName(&str, i, OPTOOLBAR_SETUP));
			m_toolbars_model.AddItem(str.CStr());
		}
		toolbar_configurations->SetTreeModel(&m_toolbars_model);
		toolbar_configurations->SetSelectedItem(g_setup_manager->GetIndexOfToolbarSetup());
	}
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveToolbars()
{
	INT32 selectedidx = -1;
	OpTreeView* toolbar_configurations = (OpTreeView*)GetWidgetByName("Toolbar_file_treeview");

	if(toolbar_configurations)
	{
		selectedidx = toolbar_configurations->GetSelectedItemModelPos();
	}

	if(selectedidx != -1)
	{
		g_setup_manager->SelectSetupFile(selectedidx, OPTOOLBAR_SETUP);
	}

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitMenus()
{

	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Hotclick_checkbox"), PrefsCollectionDisplay::AutomaticSelectMenu));

	OpTreeView* menu_configurations = (OpTreeView*)GetWidgetByName("Menu_file_treeview");

	if (menu_configurations)
	{
		menu_configurations->SetDeselectable(FALSE);

		OpString str;
		g_languageManager->GetString(Str::S_MENU_SETUP, str);

		m_menus_model.SetColumnData(0, str.CStr());

		for (UINT32 i = 0; i < g_setup_manager->GetMenuConfigurationCount(); i++)
		{
			g_setup_manager->GetSetupName(&str, i, OPMENU_SETUP);
			m_menus_model.AddItem(str.CStr());
		}
		menu_configurations->SetTreeModel(&m_menus_model);
		menu_configurations->SetSelectedItem(g_setup_manager->GetIndexOfMenuSetup());
	}
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveMenus()
{
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Hotclick_checkbox"), PrefsCollectionDisplay::AutomaticSelectMenu));

	INT32 selectedidx = -1;
	OpTreeView* menu_configurations = (OpTreeView*)GetWidgetByName("Menu_file_treeview");
	if(menu_configurations)
	{
		selectedidx = menu_configurations->GetSelectedItemModelPos();
	}

	if(selectedidx != -1)
	{
		g_setup_manager->SelectSetupFile(selectedidx, OPMENU_SETUP);
	}
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitKeyboard()
{
	m_keyboards_model.DeleteAll();

	BOOL models_only = FALSE;

	if( !models_only )
	{
#if defined (_DIRECT_URL_WINDOW_)
		RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Autocompletion_checkbox"), PrefsCollectionDisplay::AutoDropDown));
#endif
	}

	OpTreeView* keyboard_configurations = (OpTreeView*)GetWidgetByName("Keyboard_treeview");

	if (keyboard_configurations)
	{
		keyboard_configurations->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_MANAGE_KEYBOARD)));
		keyboard_configurations->SetDeselectable(FALSE);

		OpString str;
		g_languageManager->GetString(Str::S_KEYBOARD_SETUP, str);

		m_keyboards_model.SetColumnData(0, str.CStr());

		for (UINT32 i = 0; i < g_setup_manager->GetKeyboardConfigurationCount(); i++)
		{
			g_setup_manager->GetSetupName(&str, i, OPKEYBOARD_SETUP);
			m_keyboards_model.AddItem(str.CStr());
		}
		keyboard_configurations->SetTreeModel(&m_keyboards_model);
		keyboard_configurations->SetSelectedItem(g_setup_manager->GetIndexOfKeyboardSetup());
	}

	//initialize checkbox for enabling single key shortcuts
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Extended_shortcuts_checkbox"), PrefsCollectionUI::ExtendedKeyboardShortcuts));

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveKeyboard()
{
#if defined (_DIRECT_URL_WINDOW_)
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Autocompletion_checkbox"), PrefsCollectionDisplay::AutoDropDown));
#endif

	OpTreeView* keyboard_configurations = (OpTreeView*)GetWidgetByName("Keyboard_treeview");
	INT32 selected_keyboard_idx = keyboard_configurations ? keyboard_configurations->GetSelectedItemModelPos() : -1;

	if( selected_keyboard_idx != -1 )
	{
		g_setup_manager->SelectSetupFile(selected_keyboard_idx, OPKEYBOARD_SETUP);
	}

	//save single key shortcuts setting
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Extended_shortcuts_checkbox"), PrefsCollectionUI::ExtendedKeyboardShortcuts));

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitMouse()
{
	m_mice_model.DeleteAll();

	BOOL models_only = FALSE;

	if( !models_only )
	{
		RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Mouse_gestures_checkbox"), PrefsCollectionCore::EnableGesture));
		RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Left_handed_checkbox"), PrefsCollectionCore::ReverseButtonFlipping));
		RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Visual_mouse_gestures_checkbox"), PrefsCollectionUI::ShowGestureUI));
		SetWidgetEnabled("Left_handed_checkbox", GetWidgetValue("Mouse_gestures_checkbox"));
		SetWidgetEnabled("Visual_mouse_gestures_checkbox", GetWidgetValue("Mouse_gestures_checkbox"));
	}

	OpTreeView* mouse_configurations = (OpTreeView*)GetWidgetByName("Mouse_treeview");

	if (mouse_configurations)
	{
		mouse_configurations->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_MANAGE_MOUSE)));
		mouse_configurations->SetDeselectable(FALSE);

		OpString str;
		g_languageManager->GetString(Str::S_MOUSE_SETUP, str);

		m_mice_model.SetColumnData(0, str.CStr());

		for (UINT32 i = 0; i < g_setup_manager->GetMouseConfigurationCount(); i++)
		{
			g_setup_manager->GetSetupName(&str, i, OPMOUSE_SETUP);
			m_mice_model.AddItem(str.CStr());
		}
		mouse_configurations->SetTreeModel(&m_mice_model);
		mouse_configurations->SetSelectedItem(g_setup_manager->GetIndexOfMouseSetup());
	}
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveMouse()
{
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Mouse_gestures_checkbox"), PrefsCollectionCore::EnableGesture));
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Left_handed_checkbox"), PrefsCollectionCore::ReverseButtonFlipping));
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Visual_mouse_gestures_checkbox"), PrefsCollectionUI::ShowGestureUI));

	// SelectSetupFile(OPMOUSE_SETUP) below will reset selected_keyboard_idx to 0 (OnSettingsChanged() is called)
	OpTreeView* mouse_configurations = (OpTreeView*)GetWidgetByName("Mouse_treeview");
	INT32 selected_mouse_idx = mouse_configurations ? mouse_configurations->GetSelectedItemModelPos() : -1;

	if( selected_mouse_idx != -1 )
	{
		g_setup_manager->SelectSetupFile(selected_mouse_idx, OPMOUSE_SETUP);
	}
	return OpStatus::OK;
}


static const PrefsCollectionUI::stringpref sound_prefs[] =
{
	PrefsCollectionUI::StartSound,
	PrefsCollectionUI::EndSound,
	PrefsCollectionUI::LoadedSound,
	PrefsCollectionUI::FailureSound,
	PrefsCollectionUI::ClickedSound,
	PrefsCollectionUI::TransferDoneSound
};

static const Str::LocaleString sound_stringid[] =
{
	Str::SI_START_SOUND_STRING,
	Str::SI_END_SOUND_STRING,
	Str::SI_LOADED_SOUND_STRING,
	Str::SI_FAILURE_SOUND_STRING,
	Str::SI_CLICKED_SOUND_STRING,
	Str::SI_TRANSFER_DONE_SOUND_STRING
};

OP_STATUS NewPreferencesDialog::InitSounds()
{
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Program_sounds_treeview");

	if (treeview)
	{
		treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_CHOOSE_SOUND)));

		OpString str;
		g_languageManager->GetString(Str::SI_IDSTR_SNDTRIGGER_HEADER, str);
		m_sounds_model.SetColumnData(0, str.CStr());
		g_languageManager->GetString(Str::SI_IDSTR_SNDFILE_HEADER, str);
		m_sounds_model.SetColumnData(1, str.CStr());

		for (size_t i = 0; i < ARRAY_SIZE(sound_prefs); i++)
		{
			g_languageManager->GetString(sound_stringid[i], str);
			m_sounds_model.AddItem(str.CStr());
			m_sounds_model.SetItemData(i, 1, g_pcui->GetStringPref(sound_prefs[i]).CStr());
		}

		treeview->SetTreeModel(&m_sounds_model);
		treeview->SetColumnWeight(0,60);
		treeview->SetSelectedItem(0);
	}

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Program_sounds_checkbox"), PrefsCollectionUI::SoundsEnabled);

	SetWidgetEnabled("Program_sounds_treeview", GetWidgetValue("Program_sounds_checkbox"));

#ifdef M2_SUPPORT
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Chat_notification_checkbox"), PrefsCollectionUI::LimitAttentionToPersonalChatMessages);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Message_notification_checkbox"), PrefsCollectionUI::ShowNotificationForNewMessages);
#endif

#if !defined(WIDGET_RUNTIME_SUPPORT) && defined(WEBSERVER_SUPPORT)
	SetWidgetText("Widget_notification_checkbox", Str::D_WIDGET_SERVICE_NOTIFIER);
#endif // !defined(WIDGET_RUNTIME_SUPPORT) && defined(WEBSERVER_SUPPORT)

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Widget_notification_checkbox"), PrefsCollectionUI::ShowNotificationsForWidgets);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Popup_notification_checkbox"), PrefsCollectionUI::ShowNotificationForBlockedPopups);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Transfer_notification_checkbox"), PrefsCollectionUI::ShowNotificationForFinishedTransfers);

	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::SaveSounds()
{
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Program_sounds_treeview");

	if (treeview)
	{
		for (size_t i = 0; i < ARRAY_SIZE(sound_prefs); i++)
		{
			OpStringC str(m_sounds_model.GetItemString(i, 1));
			TRAPD(res, g_pcui->WriteStringL(sound_prefs[i], str));
		}
	}

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Program_sounds_checkbox"), PrefsCollectionUI::SoundsEnabled);

#ifdef M2_SUPPORT
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Chat_notification_checkbox"), PrefsCollectionUI::LimitAttentionToPersonalChatMessages);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Message_notification_checkbox"), PrefsCollectionUI::ShowNotificationForNewMessages);
#endif
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Widget_notification_checkbox"), PrefsCollectionUI::ShowNotificationsForWidgets);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Popup_notification_checkbox"), PrefsCollectionUI::ShowNotificationForBlockedPopups);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Transfer_notification_checkbox"), PrefsCollectionUI::ShowNotificationForFinishedTransfers);

	return OpStatus::OK;
}

static struct fli_struct
{
	short html_type;
	int font_id;
	int color_id;
} fli[] =
{
/*	IMPORTANT: If you add a font before OP_SYSTEM_FONT_DOCUMENT_NORMAL, make sure to update POSITION_OF_NORMAL_FONT_IN_LIST
	at the top of this file. POSITION_OF_NORMAL_FONT_IN_LIST should reflect the position of OP_SYSTEM_FONT_DOCUMENT_NORMAL.
	Do not add any font inbetween OP_SYSTEM_FONT_DOCUMENT_NORMAL and OP_SYSTEM_FONT_DOCUMENT_PRE
	When changing anything in this list, make sure to update the font_stringid list below too!*/

	{0,							OP_SYSTEM_FONT_EMAIL_COMPOSE, -1},
	{0,							OP_SYSTEM_FONT_EMAIL_DISPLAY, -1},
	{0,							OP_SYSTEM_FONT_HTML_COMPOSE, OP_SYSTEM_COLOR_HTML_COMPOSE_TEXT},
#ifndef _MACINTOSH_
	{0,							OP_SYSTEM_FONT_UI_MENU, -1},
#endif
	{0,							OP_SYSTEM_FONT_UI_TOOLBAR, -1},
	{0,							OP_SYSTEM_FONT_UI_DIALOG, -1},
	{0,							OP_SYSTEM_FONT_UI_PANEL, -1},
	{0,							OP_SYSTEM_FONT_UI_TOOLTIP, -1},
	{0,							OP_SYSTEM_FONT_UI_HEADER, -1},
	{HE_DOC_ROOT,				OP_SYSTEM_FONT_DOCUMENT_NORMAL, OP_SYSTEM_COLOR_DOCUMENT_NORMAL},
	{HE_PRE,					OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE},
	{STYLE_EX_FORM_TEXTAREA,	OP_SYSTEM_FONT_FORM_TEXT, OP_SYSTEM_COLOR_TEXT},
	{STYLE_EX_FORM_TEXTINPUT,	OP_SYSTEM_FONT_FORM_TEXT_INPUT, OP_SYSTEM_COLOR_TEXT_INPUT},
	{STYLE_EX_FORM_BUTTON,		OP_SYSTEM_FONT_FORM_BUTTON, OP_SYSTEM_COLOR_BUTTON},
	{0,							OP_SYSTEM_FONT_CSS_SERIF, -1},
	{0,							OP_SYSTEM_FONT_CSS_SANS_SERIF, -1},
	{0,							OP_SYSTEM_FONT_CSS_CURSIVE, -1},
	{0,							OP_SYSTEM_FONT_CSS_FANTASY, -1},
	{0,							OP_SYSTEM_FONT_CSS_MONOSPACE, -1},
	{HE_H1,						OP_SYSTEM_FONT_DOCUMENT_HEADER1, OP_SYSTEM_COLOR_DOCUMENT_HEADER1},
	{HE_H2,						OP_SYSTEM_FONT_DOCUMENT_HEADER2, OP_SYSTEM_COLOR_DOCUMENT_HEADER2},
	{HE_H3,						OP_SYSTEM_FONT_DOCUMENT_HEADER3, OP_SYSTEM_COLOR_DOCUMENT_HEADER3},
	{HE_H4,						OP_SYSTEM_FONT_DOCUMENT_HEADER4, OP_SYSTEM_COLOR_DOCUMENT_HEADER4},
	{HE_H5,						OP_SYSTEM_FONT_DOCUMENT_HEADER5, OP_SYSTEM_COLOR_DOCUMENT_HEADER5},
	{HE_H6,						OP_SYSTEM_FONT_DOCUMENT_HEADER6, OP_SYSTEM_COLOR_DOCUMENT_HEADER6}

};

// these have to be in a separate array, as the struct above would not allow an initialiser list
// with Str::LocaleString in it, and resorting to int involves expensive inlined conversions
static const Str::LocaleString font_stringid[] =
{
	Str::S_EMAIL_COMPOSE_FONT_TEXT,
	Str::S_EMAIL_DISPLAY_FONT_TEXT,
	Str::S_HTML_COMPOSE_FONT_TEXT,
#ifndef _MACINTOSH_
	Str::S_MENU_TEXT_FONT,
#endif
	Str::S_TOOLBAR_TEXT_FONT,
	Str::S_DIALOG_FONT_TEXT,
	Str::S_PANEL_FONT_TEXT,
	Str::S_TOOLTIP_FONT_TEXT,
	Str::S_UI_HEADER_FONT_TEXT,
	Str::SI_FONT_TEXT_NORMAL,
	Str::SI_FONT_TEXT_PRE,
	Str::SI_FONT_TEXT_FORM_TEXTAREA,
	Str::SI_FONT_TEXT_FORM_TEXTINPUT,
	Str::SI_FONT_TEXT_FORM_BUTTON,
	Str::SI_FONT_TEXT_CSS_SERIF,
	Str::SI_FONT_TEXT_CSS_SANS_SERIF,
	Str::SI_FONT_TEXT_CSS_CURSIVE,
	Str::SI_FONT_TEXT_CSS_FANTASY,
	Str::SI_FONT_TEXT_CSS_MONOSPACE,
	Str::SI_FONT_TEXT_H1,
	Str::SI_FONT_TEXT_H2,
	Str::SI_FONT_TEXT_H3,
	Str::SI_FONT_TEXT_H4,
	Str::SI_FONT_TEXT_H5,
	Str::SI_FONT_TEXT_H6
};

OP_STATUS NewPreferencesDialog::InitFontsModel()
{
	if (m_fonts_model.GetItemCount())
		return OpStatus::OK; // already initialized

	OpString str;

	g_languageManager->GetString(Str::S_FONT_TYPE, str);
	m_fonts_model.SetColumnData(0, str.CStr());
	g_languageManager->GetString(Str::S_FONT, str);
	m_fonts_model.SetColumnData(1, str.CStr());

	int min_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize);
	if (str.Reserve(30))
	{
		uni_ltoa(min_size, str.CStr(), 10);

		SetWidgetText("Minimum_font_size_edit", str.CStr());
	}

	for (size_t i = 0; i < ARRAY_SIZE(fli); i++)
	{
		FontListItem *fli_new = OP_NEW(FontListItem, (font_stringid[i], fli[i].html_type, fli[i].font_id, fli[i].color_id));
		if (fli_new)
		{
			m_font_list.Add(fli_new);

			g_languageManager->GetString(fli_new->GetStringId(), str);

			INT32 got_index = m_fonts_model.AddItem(str.CStr());

			COLORREF color = USE_DEFAULT_COLOR;
			FontAtt font;
			BOOL fontname_only = FALSE;

			GetFontAttAndColorFromFontListItem(fli_new, font, color, fontname_only);

			m_fonts_model.SetItemData(got_index, 1, font.GetFaceName());

			if (!fontname_only && color != USE_DEFAULT_COLOR)
			{
				m_fonts_model.SetItemColor(got_index, 1, color);
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::InitFonts()
{
	RETURN_IF_ERROR(InitFontsModel());

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Fonts_treeview");
	if (treeview)
	{
		treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_CHOOSE_FONT)));
		treeview->SetTreeModel(&m_fonts_model);
		treeview->SetSelectedItem(0);
	}
	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::SaveFonts()
{
	OpString8 num;

	GetWidgetText("Minimum_font_size_edit", num);
	if (num.HasContent())
	{
		int min_font_size = atoi(num.CStr());
		//LimitRangeInt( &min_font_size, 1, 200);
		min_font_size = min_font_size < 1 ? 1 : min_font_size;
		min_font_size = min_font_size > 200 ? 200 : min_font_size;
		TRAPD(err, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::MinFontSize, min_font_size));
	}

	g_application->SettingsChanged(SETTINGS_UI_FONTS);
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitTabs()
{
	OpDropDown* cycle_pages_dropdown = (OpDropDown*) GetWidgetByName("Cycle_pages_dropdown");
	OpString str;

	if (cycle_pages_dropdown)
	{
		int got_index;

		g_languageManager->GetString(Str::SI_IDM_ACCESSIBILITY_CYCLE_STACK, str);
		cycle_pages_dropdown->AddItem(str.CStr(), -1, &got_index);
		g_languageManager->GetString(Str::SI_IDM_ACCESSIBILITY_CYCLE_LIST, str);
		cycle_pages_dropdown->AddItem(str.CStr(), -1, &got_index);
		g_languageManager->GetString(Str::SI_IDM_ACCESSIBILITY_CYCLE_MDI, str);
		cycle_pages_dropdown->AddItem(str.CStr(), -1, &got_index);

		int selected = g_pcui->GetIntegerPref(PrefsCollectionUI::WindowCycleType);
		cycle_pages_dropdown->SelectItem(selected, TRUE);
	}

	OpDropDown* close_tab_dropdown = (OpDropDown*) GetWidgetByName("Close_tab_dropdown");

	if (close_tab_dropdown)
	{
		int policy = g_pcui->GetIntegerPref(PrefsCollectionUI::ActivateTabOnClose);

		g_languageManager->GetString(Str::S_CLOSE_TAB_ACTIVATE_LAST_ACTIVE, str);
		close_tab_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::S_CLOSE_TAB_ACTIVATE_RIGHT, str);
		close_tab_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::S_CLOSE_TAB_ACTIVATE_FIRST_OPENED_FROM_CURRENT, str);
		close_tab_dropdown->AddItem(str.CStr());

		close_tab_dropdown->SelectItem(policy, TRUE);
	}

	SetWidgetValue("Reuse_page_checkbox", !g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow));
	SetWidgetValue("Thumbnails_in_tab_cycle", g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInWindowCycle));
	SetWidgetValue("Thumbnails_on_tab_hover", g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInTabTooltips));
	SetWidgetValue("Thumbnails_inside_tabs", g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInsideTabs));

	OpStatus::Ignore(WidgetPrefs::GetIntegerPref(GetWidgetByName("Open_next_to_current_checkbox"), PrefsCollectionUI::OpenPageNextToCurrent));

	OpString shortcut_string;

	OpInputAction cycle_pages_action(OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE);
	g_input_manager->GetShortcutStringFromAction(&cycle_pages_action, shortcut_string, this);

	g_languageManager->GetString(Str::D_CYCLE_THROUGH_TABS_WITH, str);

	if (shortcut_string.IsEmpty())
	{
		shortcut_string.Set("Ctrl+Tab");
	}

	str.Append(shortcut_string);

	SetWidgetText("label_for_Cycle_pages_dropdown", str.CStr());

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveTabs()
{
	OpDropDown* cycle_pages_dropdown = (OpDropDown*) GetWidgetByName("Cycle_pages_dropdown");

	if (cycle_pages_dropdown)
	{
		int type = cycle_pages_dropdown->GetSelectedItem();
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::WindowCycleType, type));
	}
	OpWidget* thumbnails = GetWidgetByName("Thumbnails_in_tab_cycle");
	if (thumbnails)
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::UseThumbnailsInWindowCycle, thumbnails->GetValue()));
	}
	thumbnails = GetWidgetByName("Thumbnails_on_tab_hover");
	if (thumbnails)
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::UseThumbnailsInTabTooltips, thumbnails->GetValue()));
	}
	thumbnails = GetWidgetByName("Thumbnails_inside_tabs");
	if (thumbnails)
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::UseThumbnailsInsideTabs, thumbnails->GetValue()));
	}
	OpWidget* reuse_windows = GetWidgetByName("Reuse_page_checkbox");

	if (reuse_windows && g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow) == reuse_windows->GetValue())
	{
		TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::NewWindow, !reuse_windows->GetValue()));
	}
	OpDropDown* close_tab_dropdown = (OpDropDown*) GetWidgetByName("Close_tab_dropdown");

	if (close_tab_dropdown)
	{
		int policy = close_tab_dropdown->GetSelectedItem();
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ActivateTabOnClose, policy));
	}

	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Open_next_to_current_checkbox"), PrefsCollectionUI::OpenPageNextToCurrent));

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitBrowsing()
{
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Show_scrollbars_checkbox"), PrefsCollectionDoc::ShowScrollbars));
#ifdef FEATURE_SCROLL_MARKER
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Show_scrollmarker_checkbox"), PrefsCollectionUI::EnableScrollMarker));
#endif
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Smooth_scrolling_checkbox"), PrefsCollectionDisplay::SmoothScrolling));
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_spellchecker"), PrefsCollectionCore::SpellcheckEnabledByDefault));
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Show_window_size_checkbox"), PrefsCollectionDoc::ShowWinSize));
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Window_menu_checkbox"), PrefsCollectionUI::ShowWindowMenu));
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Confirm_exit_checkbox"), PrefsCollectionUI::ShowExitDialog));
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Tooltips_checkbox"), PrefsCollectionUI::PopupButtonHelp));
	RETURN_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Show_full_url"), PrefsCollectionUI::ShowFullURL));

	OpString str;
	OpDropDown* loading_dropdown = (OpDropDown*) GetWidgetByName("Loading_dropdown");

	if (loading_dropdown)
	{
		INT32 got_index;
		INT32 delay = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FirstUpdateDelay);

		if (str.Reserve(256))
		{
			g_languageManager->GetString(Str::S_REDRAW_INSTANTLY, str);
			loading_dropdown->AddItem(str.CStr(), -1, &got_index, 0);
			if (delay == 0)	loading_dropdown->SelectItem(got_index, TRUE);

			g_languageManager->GetString(Str::S_REDRAW_AFTER_ONE_SECOND, str);
			loading_dropdown->AddItem(str.CStr(), -1, &got_index, 1000);
			if (delay == 1000) loading_dropdown->SelectItem(got_index, TRUE);

			OpString seconds;
			g_languageManager->GetString(Str::S_REDRAW_AFTER_X_SECONDS, seconds);
			if (seconds.CStr())
			{
				uni_sprintf(str.CStr(), seconds.CStr(), 2);
				loading_dropdown->AddItem(str.CStr(), -1, &got_index, 2000);
				if (delay == 2000)	loading_dropdown->SelectItem(got_index, TRUE);

				uni_sprintf(str.CStr(), seconds.CStr(), 3);
				loading_dropdown->AddItem(str.CStr(), -1, &got_index, 3000);
				if (delay == 3000)	loading_dropdown->SelectItem(got_index, TRUE);

				uni_sprintf(str.CStr(), seconds.CStr(), 5);
				loading_dropdown->AddItem(str.CStr(), -1, &got_index, 5000);
				if (delay == 5000)	loading_dropdown->SelectItem(got_index, TRUE);

				uni_sprintf(str.CStr(), seconds.CStr(), 10);
				loading_dropdown->AddItem(str.CStr(), -1, &got_index, 10000);
				if (delay == 10000)	loading_dropdown->SelectItem(got_index, TRUE);

				uni_sprintf(str.CStr(), seconds.CStr(), 20);
				loading_dropdown->AddItem(str.CStr(), -1, &got_index, 20000);
				if (delay == 20000)	loading_dropdown->SelectItem(got_index, TRUE);
			}

			g_languageManager->GetString(Str::S_REDRAW_WHEN_LOADED, str);
			loading_dropdown->AddItem(str.CStr(), -1, &got_index, INT_MAX);
			if (delay == INT_MAX) loading_dropdown->SelectItem(got_index, TRUE);
		}
	}

	OpDropDown* page_icon_dropdown = (OpDropDown*)GetWidgetByName("Page_icon_dropdown");
	if( page_icon_dropdown )
	{
		g_languageManager->GetString(Str::D_PREFERENCES_SHOW_ALL_PAGE_ICONS, str);
		page_icon_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::D_PREFERENCES_SHOW_EMBEDDED_PAGE_ICONS, str);
		page_icon_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::D_PREFERENCES_SHOW_NO_PAGE_ICONS, str);
		page_icon_dropdown->AddItem(str.CStr());
		if(g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon) == 0)
			page_icon_dropdown->SelectItem(2, TRUE);
		else if(g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon) == 1)
			page_icon_dropdown->SelectItem(0, TRUE);
		else if(g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon) == 2)
			page_icon_dropdown->SelectItem(1, TRUE);
		else
			page_icon_dropdown->SelectItem(0, TRUE);
	}

#ifdef _MACINTOSH_
	ShowWidget("Window_menu_checkbox", FALSE);
#endif // _MACINTOSH_
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveBrowsing()
{
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Show_scrollbars_checkbox"), PrefsCollectionDoc::ShowScrollbars));
#ifdef FEATURE_SCROLL_MARKER 
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Show_scrollmarker_checkbox"), PrefsCollectionUI::EnableScrollMarker));
#endif
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Smooth_scrolling_checkbox"), PrefsCollectionDisplay::SmoothScrolling));
 	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_spellchecker"), PrefsCollectionCore::SpellcheckEnabledByDefault));
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Show_window_size_checkbox"), PrefsCollectionDoc::ShowWinSize));
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Window_menu_checkbox"), PrefsCollectionUI::ShowWindowMenu));
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Confirm_exit_checkbox"), PrefsCollectionUI::ShowExitDialog));
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Tooltips_checkbox"), PrefsCollectionUI::PopupButtonHelp));
	RETURN_IF_ERROR(WidgetPrefs::SetIntegerPref(GetWidgetByName("Show_full_url"), PrefsCollectionUI::ShowFullURL));

	OpDropDown* page_icon_dropdown = (OpDropDown*) GetWidgetByName("Page_icon_dropdown");
	if (page_icon_dropdown)
	{
		int selected_pos = page_icon_dropdown->GetSelectedItem();
		switch(selected_pos)
		{
			case 0:
			{
				TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::AlwaysLoadFavIcon, 1));
				break;
			}
			case 1:
			{
				TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::AlwaysLoadFavIcon, 2));
				break;
			}
			case 2:
			{
				TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::AlwaysLoadFavIcon, 0));
				break;
			}
		}
	}

	OpDropDown* loading_dropdown = (OpDropDown*) GetWidgetByName("Loading_dropdown");

	if (loading_dropdown)
	{
		INT32 delay = (INTPTR)loading_dropdown->GetItemUserData(loading_dropdown->GetSelectedItem());

		TRAPD(err, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::FirstUpdateDelay, delay));
	}

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitDisplay()
{
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Gif_animation_checkbox"), PrefsCollectionDoc::ShowAnimation);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Javascript_checkbox"), PrefsCollectionJS::EcmaScriptEnabled);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Plugins_checkbox"), PrefsCollectionDisplay::PluginsEnabled);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("On_demand_plugins_checkbox"), PrefsCollectionDisplay::EnableOnDemandPlugin);
	// enable on demand plugins checkbox only when plugins are enabled
	SetWidgetEnabled("On_demand_plugins_checkbox", (BOOL)GetWidgetValue("Plugins_checkbox"));

#ifndef _UNIX_DESKTOP_
	ShowWidget("Configure_plugins_button", FALSE);
#endif // !_UNIX_DESKTOP_
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveDisplay()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Gif_animation_checkbox"), PrefsCollectionDoc::ShowAnimation);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Javascript_checkbox"), PrefsCollectionJS::EcmaScriptEnabled);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Plugins_checkbox"), PrefsCollectionDisplay::PluginsEnabled);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("On_demand_plugins_checkbox"), PrefsCollectionDisplay::EnableOnDemandPlugin);

	g_application->SettingsChanged(SETTINGS_MULTIMEDIA);

	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::InitFiles()
{
	SetWidgetValue("Show_all_file_types_checkbox", TRUE);
	SetWidgetEnabled("Show_all_file_types_checkbox", TRUE);

	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("File_types_treeview");

	OpString str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_MIME_HEADER, str));
	m_file_types_model.SetColumnData(0, str.CStr(), NULL, FALSE, TRUE);
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_EXTENSION_HEADER, str));
	m_file_types_model.SetColumnData(1, str.CStr(), NULL, FALSE, TRUE);

	if (treeview)
	{
		treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_EDIT_FILE_TYPE)));

		PopulateFileTypes();

		treeview->SetTreeModel(&m_file_types_model, 0);

		OpQuickFind* quickfind = (OpQuickFind*) GetWidgetByName("Filetypes_quickfind");

		treeview->SetMatchType(MATCH_IMPORTANT, TRUE);

		if (quickfind)
			quickfind->SetTarget(treeview);
	}

	g_main_message_handler->SetCallBack(this, MSG_VIEWERS_ADDED, 0);
	g_main_message_handler->SetCallBack(this, MSG_VIEWERS_CHANGED, 0);
	g_main_message_handler->SetCallBack(this, MSG_VIEWERS_DELETED, 0);

#if !defined(_UNIX_DESKTOP_)
	ShowWidget("Saved_files_button", FALSE);
#endif

	OpString tmp_storage;
	SetWidgetText("Download_directory_chooser", g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DOWNLOAD_FOLDER, tmp_storage));
	
	OpDropDown* download_managers = (OpDropDown*)GetWidgetByName("Download_manager");
	g_download_manager_manager->PopulateDropdown(download_managers);
	return OpStatus::OK;
}

void NewPreferencesDialog::PopulateFileTypes()
{
	m_file_types_model.BeginChange();

	m_file_types_model.DeleteAll();

	ChainedHashIterator * iter;
	g_viewers->CreateIterator(iter);
	if (iter) {
		for (Viewer * v = g_viewers->GetNextViewer(iter); v; v = g_viewers->GetNextViewer(iter)) {
			OnViewerAdded(v);
		}
	}
	OP_DELETE(iter);

	m_file_types_model.EndChange();
}


void NewPreferencesDialog::OnViewerAdded(Viewer *v)
{
	// hide default plugin and default action
	if (uni_strcmp(v->GetContentTypeString(), UNI_L("*/*")) != 0
		&& uni_strcmp(v->GetContentTypeString(), UNI_L("...")) != 0)
	{
		INT32 pos = m_file_types_model.AddItem(v->GetContentTypeString(), NULL, 0, -1, v);
		m_file_types_model.SetItemData(pos, 1, v->GetExtensions());
		m_file_types_model.GetItemByIndex(pos)->SetIsImportant(v->GetAction() != VIEWER_OPERA && v->GetAction() != VIEWER_ASK_USER);
	}
}


void NewPreferencesDialog::OnViewerChanged(Viewer *v)
{
	INT32 pos = m_file_types_model.FindItemUserData(v);
	if (pos != -1)
	{
		m_file_types_model.SetItemData(pos, 0, v->GetContentTypeString());
		m_file_types_model.SetItemData(pos, 1, v->GetExtensions());
		m_file_types_model.GetItemByIndex(pos)->SetIsImportant(v->GetAction() != VIEWER_OPERA && v->GetAction() != VIEWER_ASK_USER);
	}
}


void NewPreferencesDialog::OnViewerDeleted(Viewer *v)
{
	INT32 pos = m_file_types_model.FindItemUserData(v);
	if (pos != -1)
	{
		m_file_types_model.Delete(pos);
	}
}


OP_STATUS NewPreferencesDialog::SaveFiles()
{
	TRAPD(err, g_viewers->WriteViewersL());

	if (GetWidgetByName("Download_directory_chooser"))
	{
		OpString download_directory;
		GetWidgetText("Download_directory_chooser", download_directory);

		TRAPD(err, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, download_directory.CStr()));
		TRAP(err, g_pcfiles->WriteDirectoryL(OPFILE_DOWNLOAD_FOLDER, download_directory.CStr()));
	}

	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Download_manager");
	if (dropdown)
	{
		ExternalDownloadManager* selected = (ExternalDownloadManager*)dropdown->GetItemUserData(dropdown->GetSelectedItem());
		g_download_manager_manager->SetDefaultManager(selected);
	}

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitPrograms()
{
	UINT i;

	m_trusted_application_list = OP_NEW(TrustedProtocolList, ());

	if (!m_trusted_application_list)
		return OpStatus::ERR_NO_MEMORY;

	m_trusted_application_list->Read(TrustedProtocolList::APPLICATIONS);

	for( i = 0; i < m_trusted_application_list->GetCount(); i++ )
	{
		TrustedProtocolEntry* entry = m_trusted_application_list->Get(i);
		if (entry->GetEntryType() == TrustedProtocolEntry::SOURCE_VIEWER_ENTRY)
		{
			OpString application;
			entry->GetActiveApplication(application);
			SetWidgetText("Source_program", application);
			break;
		}
	}

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Protocol_treeview");
	if (treeview)
	{
		treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_MANAGE_TRUSTED_APPLICATION)));

		OpString str;

		g_languageManager->GetString(Str::S_PROTOCOL, str);
		m_trusted_protocol_model.SetColumnData(0, str.CStr() );
		g_languageManager->GetString(Str::S_PROGRAM_OR_WEBPAGE, str);
		m_trusted_protocol_model.SetColumnData(1, str.CStr() );

		m_trusted_protocol_list = OP_NEW(TrustedProtocolList, ());
		if( m_trusted_protocol_list )
		{
			m_trusted_protocol_list->Read(TrustedProtocolList::PROTOCOLS);
			for( i=0; i<m_trusted_protocol_list->GetCount(); i++ )
			{
				TrustedProtocolEntry* entry = m_trusted_protocol_list->Get(i);
				INT32 index = m_trusted_protocol_model.AddItem(entry->GetDisplayText());
				OpString application;
				BOOL is_formatted = FALSE;
				entry->GetActiveApplication(application, &is_formatted);
				m_trusted_protocol_model.SetItemData(i, 1, application);
				m_trusted_protocol_model.GetItemByIndex(index)->SetHasFormattedText(is_formatted);
			}
		}

		treeview->SetTreeModel(&m_trusted_protocol_model);
		treeview->SetColumnWeight(0,40);
		treeview->SetSelectedItem(0);
	}

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Default_check_on_startup_checkbox"), PrefsCollectionUI::ShowDefaultBrowserDialog);

#if !defined(MSWIN)
	ShowWidget("Default_check_on_startup_checkbox", FALSE);
	ShowWidget("Default_check_on_startup_button", FALSE);
#else
	if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_LABS)
	{
		ShowWidget("Default_check_on_startup_checkbox", FALSE);
		ShowWidget("Default_check_on_startup_button", FALSE);
	}
#endif

#if !defined(_UNIX_DESKTOP_)
	ShowWidget("Configure_plugins_button", FALSE);
#endif

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SavePrograms()
{
	if (m_trusted_application_list)
		m_trusted_application_list->Save(TrustedProtocolList::APPLICATIONS);
	if (m_trusted_protocol_list)
		m_trusted_protocol_list->Save(TrustedProtocolList::PROTOCOLS);

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Default_check_on_startup_checkbox"), PrefsCollectionUI::ShowDefaultBrowserDialog);
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitHistory()
{
	// abs(prefsManager->GetMaxWindowHistorySetting()));

	int max_direct_history = g_pccore->GetIntegerPref(PrefsCollectionCore::MaxDirectHistory);
	if (max_direct_history < 0)
		max_direct_history = 0;

	int max_global_history = g_pccore->GetIntegerPref(PrefsCollectionCore::MaxGlobalHistory);
	if (max_global_history < 0)
		max_global_history = 0;

	OpString str;
	if (!str.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Typed_in_history_dropdown");
	if (dropdown)
	{
		dropdown->AddItem(UNI_L("0"),   -1, NULL, 0);
		dropdown->AddItem(UNI_L("10"),  -1, NULL, 10);
		dropdown->AddItem(UNI_L("50"),  -1, NULL, 50);
		dropdown->AddItem(UNI_L("100"), -1, NULL, 100);
		dropdown->AddItem(UNI_L("200"), -1, NULL, 200);
		dropdown->AddItem(UNI_L("500"), -1, NULL, 500);

		switch (max_direct_history)
		{
		case 0:
			dropdown->SelectItem(0, TRUE);
			break;
		case 10:
			dropdown->SelectItem(1, TRUE);
			break;
		case 50:
			dropdown->SelectItem(2, TRUE);
			break;
		case 100:
			dropdown->SelectItem(3, TRUE);
			break;
		case 200:
			dropdown->SelectItem(4, TRUE);
			break;
		case 500:
			dropdown->SelectItem(5, TRUE);
			break;
		default:
			uni_ltoa(max_direct_history, str.CStr(), 10);
			dropdown->AddItem(str.CStr(), -1, NULL, max_direct_history);
			dropdown->SelectItem(6, TRUE);
			break;
		}
	}

	dropdown = (OpDropDown*) GetWidgetByName("Visited_history_dropdown");
	if (dropdown)
	{
		dropdown->AddItem(UNI_L("0"),     -1, NULL, 0);
		dropdown->AddItem(UNI_L("1000"),  -1, NULL, 1000);
		dropdown->AddItem(UNI_L("5000"),  -1, NULL, 5000);
		dropdown->AddItem(UNI_L("10000"), -1, NULL, 10000);
		dropdown->AddItem(UNI_L("20000"), -1, NULL, 20000);
		dropdown->AddItem(UNI_L("50000"), -1, NULL, 50000);

		switch (max_global_history)
		{
		case 0:
			dropdown->SelectItem(0, TRUE);
			break;
		case 1000:
			dropdown->SelectItem(1, TRUE);
			break;
		case 5000:
			dropdown->SelectItem(2, TRUE);
			break;
		case 10000:
			dropdown->SelectItem(3, TRUE);
			break;
		case 20000:
			dropdown->SelectItem(4, TRUE);
			break;
		case 50000:
			dropdown->SelectItem(5, TRUE);
			break;
		default:
			uni_ltoa(max_global_history, str.CStr(), 10);
			dropdown->AddItem(str.CStr(), -1, NULL, max_global_history);
			dropdown->SelectItem(6, TRUE);
			break;
		}
	}

	int ram_cache_size = g_pccore->GetIntegerPref(PrefsCollectionCore::DocumentCacheSize);
	// prefsManager->GetFigureCacheSize();
	// PrefsManager::RamCacheDocs);
	// PrefsManager::RamCacheFigs);

	OpString mb;
	g_languageManager->GetString(Str::SI_IDSTR_MEGABYTE, mb);
	mb.Insert(0, UNI_L("%d "));

	dropdown = (OpDropDown*) GetWidgetByName("Memory_cache_dropdown");
	if (dropdown)
	{
		g_languageManager->GetString(Str::S_AUTOMATIC_CACHE_SIZE, str);
		dropdown->AddItem(str.CStr(), -1, NULL, 0);

		uni_sprintf(str.CStr(), mb.CStr(), 4);
		dropdown->AddItem(str.CStr(), -1, NULL, 2000);
		uni_sprintf(str.CStr(), mb.CStr(), 10);
		dropdown->AddItem(str.CStr(), -1, NULL, 5000);
		uni_sprintf(str.CStr(), mb.CStr(), 20);
		dropdown->AddItem(str.CStr(), -1, NULL, 10000);
		uni_sprintf(str.CStr(), mb.CStr(), 40);
		dropdown->AddItem(str.CStr(), -1, NULL, 20000);
		uni_sprintf(str.CStr(), mb.CStr(), 60);
		dropdown->AddItem(str.CStr(), -1, NULL, 30000);
		uni_sprintf(str.CStr(), mb.CStr(), 100);
		dropdown->AddItem(str.CStr(), -1, NULL, 50000);
		uni_sprintf(str.CStr(), mb.CStr(), 150);
		dropdown->AddItem(str.CStr(), -1, NULL, 75000);
		uni_sprintf(str.CStr(), mb.CStr(), 200);
		dropdown->AddItem(str.CStr(), -1, NULL, 100000);
		uni_sprintf(str.CStr(), mb.CStr(), 400);
		dropdown->AddItem(str.CStr(), -1, NULL, 200000);

		g_languageManager->GetString(Str::S_FILTER_OFF, str);
		dropdown->AddItem(str.CStr(), -1, NULL, 0);

		if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticRamCache))
		{
			dropdown->SelectItem(0, TRUE);
		}
		else
		{
			switch (ram_cache_size)
			{
			case 2000:
				dropdown->SelectItem(1, TRUE);
				break;
			case 5000:
				dropdown->SelectItem(2, TRUE);
				break;
			case 10000:
				dropdown->SelectItem(3, TRUE);
				break;
			case 20000:
				dropdown->SelectItem(4, TRUE);
				break;
			case 30000:
				dropdown->SelectItem(5, TRUE);
				break;
			case 50000:
				dropdown->SelectItem(6, TRUE);
				break;
			case 75000:
				dropdown->SelectItem(7, TRUE);
				break;
			case 100000:
				dropdown->SelectItem(8, TRUE);
				break;
			case 200000:
				dropdown->SelectItem(9, TRUE);
				break;
			case 0:
				dropdown->SelectItem(10, TRUE);
				break;
			default:
				uni_sprintf(str.CStr(), mb.CStr(), ram_cache_size / 500);
				dropdown->AddItem(str.CStr(), -1, NULL, ram_cache_size);
				dropdown->SelectItem(11, TRUE);
				break;
			}
		}
	}

	int disk_cache_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize);

	dropdown = (OpDropDown*) GetWidgetByName("Disk_cache_dropdown");
	if (dropdown)
	{
		g_languageManager->GetString(Str::S_FILTER_OFF, str);
		dropdown->AddItem(str.CStr(), -1, NULL, 0);
		uni_sprintf(str.CStr(), mb.CStr(), 2);
		dropdown->AddItem(str.CStr(), -1, NULL, 2000);
		uni_sprintf(str.CStr(), mb.CStr(), 5);
		dropdown->AddItem(str.CStr(), -1, NULL, 5000);
		uni_sprintf(str.CStr(), mb.CStr(), 10);
		dropdown->AddItem(str.CStr(), -1, NULL, 10000);
		uni_sprintf(str.CStr(), mb.CStr(), 20);
		dropdown->AddItem(str.CStr(), -1, NULL, 20000);
		uni_sprintf(str.CStr(), mb.CStr(), 50);
		dropdown->AddItem(str.CStr(), -1, NULL, 50000);
		uni_sprintf(str.CStr(), mb.CStr(), 100);
		dropdown->AddItem(str.CStr(), -1, NULL, 100000);
		uni_sprintf(str.CStr(), mb.CStr(), 200);
		dropdown->AddItem(str.CStr(), -1, NULL, 200000);
		uni_sprintf(str.CStr(), mb.CStr(), 400);
		dropdown->AddItem(str.CStr(), -1, NULL, 400000);

		switch (disk_cache_size)
		{
		case 0:
			dropdown->SelectItem(0, TRUE);
			break;
		case 2000:
			dropdown->SelectItem(1, TRUE);
			break;
		case 5000:
			dropdown->SelectItem(2, TRUE);
			break;
		case 10000:
			dropdown->SelectItem(3, TRUE);
			break;
		case 20000:
			dropdown->SelectItem(4, TRUE);
			break;
		case 50000:
			dropdown->SelectItem(5, TRUE);
			break;
		case 100000:
			dropdown->SelectItem(6, TRUE);
			break;
		case 200000:
			dropdown->SelectItem(7, TRUE);
			break;
		case 400000:
			dropdown->SelectItem(8, TRUE);
			break;
		default:
			uni_sprintf(str.CStr(), mb.CStr(), disk_cache_size / 1000);
			dropdown->AddItem(str.CStr(), -1, NULL, disk_cache_size);
			dropdown->SelectItem(9, TRUE);
			break;
		}
	}

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Cache_documents_checkbox"), PrefsCollectionNetwork::CacheDocs);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Cache_images_checkbox"), PrefsCollectionNetwork::CacheFigs);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Cache_other_checkbox"), PrefsCollectionNetwork::CacheOther);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Empty_cache_on_exit_checkbox"), PrefsCollectionNetwork::EmptyCacheOnExit);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_vps_checkbox"), PrefsCollectionCore::VisitedPages);

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Always_check_redirect_images_checkbox"), PrefsCollectionNetwork::AlwaysCheckRedirectChangedImages);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Always_check_redirect_checkbox"), PrefsCollectionNetwork::AlwaysCheckRedirectChanged);

	CHMOD chmod;
	int chtime;

	chtime = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DocExpiry);
	chmod = CHMOD(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckDocModification));

	InitCheckModification("Document_check_dropdown", chmod, chtime);

	chtime = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::FigsExpiry);
	chmod = CHMOD(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckFigModification));

	InitCheckModification("Image_check_dropdown", chmod, chtime);

	chtime = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OtherExpiry);
	chmod = CHMOD(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckOtherModification));

	InitCheckModification("Other_check_dropdown", chmod, chtime);
	return OpStatus::OK;
}


void NewPreferencesDialog::InitCheckModification(const char* widget_name, CHMOD chmod, int chtime)
{
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName(widget_name);

	if (dropdown)
	{
		OpString buffer;
		if (buffer.Reserve(128))
		{
			g_languageManager->GetString(Str::DI_IDM_CMOD_DOCS_NEVER, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, UINT_MAX);

			g_languageManager->GetString(Str::DI_IDM_CMOD_DOCS_ALWAYS, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 0);

			g_languageManager->GetString(Str::S_EVERY_5_MINUTES, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 300);

			g_languageManager->GetString(Str::S_EVERY_10_MINUTES, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 600);

			g_languageManager->GetString(Str::S_EVERY_HOUR, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 3600);

			g_languageManager->GetString(Str::S_EVERY_2_HOURS, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 7200);

			g_languageManager->GetString(Str::S_EVERY_3_HOURS, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 10800);

			g_languageManager->GetString(Str::S_EVERY_5_HOURS, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 18000);

			g_languageManager->GetString(Str::S_EVERY_10_HOURS, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 36000);

			g_languageManager->GetString(Str::S_EVERY_24_HOURS, buffer);
			dropdown->AddItem(buffer.CStr(), -1, NULL, 86400);

			switch (chmod)
			{
			case NEVER:
				dropdown->SelectItem(0, TRUE);
				break;
			case ALWAYS:
				dropdown->SelectItem(1, TRUE);
				break;
			default:
				switch (chtime)
				{
				case 300: // 5 min
					dropdown->SelectItem(2, TRUE);
					break;
				case 600: // 10 min
					dropdown->SelectItem(3, TRUE);
					break;
				case 3600: // 1h
					dropdown->SelectItem(4, TRUE);
					break;
				case 7200: // 2 h
					dropdown->SelectItem(5, TRUE);
					break;
				case 10800: // 3 h
					dropdown->SelectItem(6, TRUE);
					break;
				case 36000: // 10 h
					dropdown->SelectItem(8, TRUE);
					break;
				case 86400: // 24 h
					dropdown->SelectItem(9, TRUE);
					break;
				case 604800: // week
					dropdown->SelectItem(10, TRUE);
					break;
				case 18000: // 5h
				default:
					dropdown->SelectItem(7, TRUE);
					break;
				}
				break;
			}
		}
	}
}


OP_STATUS NewPreferencesDialog::SaveHistory()
{
	int old_max_direct_history = g_pccore->GetIntegerPref(PrefsCollectionCore::MaxDirectHistory);
	if (old_max_direct_history < 0)
		old_max_direct_history = 0;

	int old_max_global_history = g_pccore->GetIntegerPref(PrefsCollectionCore::MaxGlobalHistory);
	if (old_max_global_history < 0)
		old_max_global_history = 0;

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Typed_in_history_dropdown");
	if (dropdown)
	{
		int hist_lines = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());
		TRAPD(err, g_pccore->WriteIntegerL(PrefsCollectionCore::MaxDirectHistory, hist_lines));
		directHistory->Size(hist_lines);
	}

	dropdown = (OpDropDown*) GetWidgetByName("Visited_history_dropdown");
	if (dropdown)
	{
		int hist_lines = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		if (0 == hist_lines)
		{
			// mimic the Clear button
			g_input_manager->InvokeAction(OpInputAction::ACTION_CLEAR_VISITED_HISTORY);
			g_input_manager->InvokeAction(OpInputAction::ACTION_CLEAR_TYPED_IN_HISTORY);

			// set direct history to 0 as well
			TRAPD(err, g_pccore->WriteIntegerL(PrefsCollectionCore::MaxDirectHistory, 0));
		}
		else if (0 == old_max_global_history && 0 == old_max_direct_history)
		{
			// reset direct history to the default value
			TRAPD(err, g_pccore->ResetIntegerL(PrefsCollectionCore::MaxDirectHistory));
		}

		TRAPD(err, g_pccore->WriteIntegerL(PrefsCollectionCore::MaxGlobalHistory, hist_lines));

		DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
		if (history_model)
		{
			if (0 == hist_lines)
				history_model->DeleteAllItems();

			history_model->Resize(hist_lines);
		}
	}

	// prefsManager->WriteMaxWindowHistoryL(hist_lines);
	// windowManager->SetMaxWindowHistory(hist_lines);

	dropdown = (OpDropDown*) GetWidgetByName("Memory_cache_dropdown");
	if (dropdown)
	{
		long doc_cache_size, fig_cache_size;

		doc_cache_size = fig_cache_size = (long)(INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		BOOL auto_cache = dropdown->GetSelectedItem() == 0; // auto always at first

		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AutomaticRamCache, auto_cache));

		if (auto_cache)
		{
			doc_cache_size = fig_cache_size = 2000;
		}

		TRAP(err, g_pccore->WriteIntegerL(PrefsCollectionCore::DocumentCacheSize, doc_cache_size));
		TRAP(err, g_pccore->WriteIntegerL(PrefsCollectionCore::ImageRAMCacheSize, fig_cache_size));

		CorePrefsHelpers::ApplyMemoryManagerPrefs();

		TRAP(err, g_pccore->WriteIntegerL(PrefsCollectionCore::RamCacheDocs, g_memory_manager->MaxDocMemory() > 0));
		TRAP(err, g_pccore->WriteIntegerL(PrefsCollectionCore::RamCacheFigs, g_memory_manager->GetMaxImgMemory() > 0));
	}

	dropdown = (OpDropDown*) GetWidgetByName("Disk_cache_dropdown");
	if (dropdown)
	{
		long disk_cache_size = (long)(INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::DiskCacheSize, disk_cache_size));
		TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CacheToDisk, disk_cache_size == 0 ? 0 : 1));
	}

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Cache_documents_checkbox"), PrefsCollectionNetwork::CacheDocs);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Cache_images_checkbox"), PrefsCollectionNetwork::CacheFigs);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Cache_other_checkbox"), PrefsCollectionNetwork::CacheOther);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Empty_cache_on_exit_checkbox"), PrefsCollectionNetwork::EmptyCacheOnExit);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_vps_checkbox"), PrefsCollectionCore::VisitedPages);

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Always_check_redirect_images_checkbox"), PrefsCollectionNetwork::AlwaysCheckRedirectChangedImages);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Always_check_redirect_checkbox"), PrefsCollectionNetwork::AlwaysCheckRedirectChanged);

	CHMOD chmod_docs = CHMOD(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckDocModification));
	CHMOD chmod_figs = CHMOD(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckFigModification));
	CHMOD chmod_other = CHMOD(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckOtherModification));

	dropdown = (OpDropDown*) GetWidgetByName("Document_check_dropdown");

	if (dropdown)
	{
		long check_modification = (long)(INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		switch (check_modification)
		{
		case 0:
			chmod_docs = ALWAYS;
			break;
		case UINT_MAX:
			chmod_docs = NEVER;
			break;
		default:
			{
				chmod_docs = TIME;

				TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::DocExpiry, check_modification));
			}
		}

		chmod_figs = chmod_other = chmod_docs;
	}

	dropdown = (OpDropDown*) GetWidgetByName("Image_check_dropdown");

	if (dropdown)
	{
		long check_modification = (long)(INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		switch (check_modification)
		{
		case 0:
			chmod_figs = ALWAYS;
			break;
		case UINT_MAX:
			chmod_figs = NEVER;
			break;
		default:
			{
				chmod_figs = TIME;

				TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::FigsExpiry, check_modification));
				// writing other now in case the other dropdown is not available:
				TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::OtherExpiry, check_modification));
			}
		}
		chmod_other = chmod_figs;
	}

	dropdown = (OpDropDown*) GetWidgetByName("Other_check_dropdown");

	if (dropdown)
	{
		long check_modification = dropdown->GetItemUserData(dropdown->GetSelectedItem());

		switch (check_modification)
		{
		case 0:
			chmod_other = ALWAYS;
			break;
		case UINT_MAX:
			chmod_other = NEVER;
			break;
		default:
			{
				chmod_other = TIME;

				TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::OtherExpiry, check_modification));
			}
		}
	}

	TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CheckDocModification, chmod_docs));
	TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CheckFigModification, chmod_figs));
	TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CheckOtherModification, chmod_other));
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitCookies()
{
	BOOL accept_cookies=FALSE, accept_third_party=FALSE;

	COOKIE_MODES cookie_mode = COOKIE_MODES(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled));

	accept_cookies = (cookie_mode == COOKIE_ALL || cookie_mode == COOKIE_NO_THIRD_PARTY || cookie_mode == COOKIE_SEND_NOT_ACCEPT_3P);
	accept_third_party = (cookie_mode == COOKIE_ALL);

	if (accept_cookies && accept_third_party)
	{
		SetWidgetValue("Accept_cookies_radio", TRUE);
	}
	else if (accept_cookies)
	{
		SetWidgetValue("No_third_party_radio", TRUE);
	}
	else
	{
		SetWidgetValue("No_cookies_radio", TRUE);
	}

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Delete_cookies_on_exit_checkbox"), PrefsCollectionNetwork::AcceptCookiesSessionOnly);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Display_cookies_checkbox"), PrefsCollectionNetwork::DisplayReceivedCookies);

	SetWidgetEnabled("Delete_cookies_on_exit_checkbox", GetWidgetValue("No_cookies_radio") ? FALSE : TRUE);
	SetWidgetEnabled("Display_cookies_checkbox", GetWidgetValue("No_cookies_radio") ? FALSE : TRUE);

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveCookies()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Delete_cookies_on_exit_checkbox"), PrefsCollectionNetwork::AcceptCookiesSessionOnly);

	BOOL def_accept_cookies = GetWidgetValue("Accept_cookies_radio") || GetWidgetValue("No_third_party_radio");
	BOOL def_accept_third_party = GetWidgetValue("Accept_cookies_radio");

	OP_MEMORY_VAR COOKIE_MODES cookie_mode
		= def_accept_cookies && def_accept_third_party ? COOKIE_ALL :
	def_accept_cookies ? COOKIE_SEND_NOT_ACCEPT_3P : COOKIE_NONE;

	TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CookiesEnabled, cookie_mode));

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Display_cookies_checkbox"), PrefsCollectionNetwork::DisplayReceivedCookies);

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitSecurity()
{
	m_previous_paranoid_pref = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword);

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Ask_insecure_form_checkbox"), PrefsCollectionNetwork::WarnInsecureFormSubmit);
#if defined(_VALIDATION_SUPPORT_)
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Ask_validation_checkbox"), PrefsCollectionUI::ShowValidationDialog);
#endif
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Master_password_checkbox"), PrefsCollectionNetwork::UseParanoidMailpassword);
#ifdef TRUST_RATING
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Trust_info_checkbox"), PrefsCollectionNetwork::EnableTrustRating);
#endif // TRUST_RATING
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Do_not_track_checkbox"), PrefsCollectionNetwork::DoNotTrack);

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Ask_for_password_dropdown");

	if (dropdown)
	{
		MasterPasswordLifetime lifetime;
		OpString item;

		MasterPasswordLifetime::Values values[] = 
			{ MasterPasswordLifetime::ALWAYS,
			  MasterPasswordLifetime::ONCE,
			  MasterPasswordLifetime::SHORT_TIME,
			  MasterPasswordLifetime::LONGER_TIME,
			  MasterPasswordLifetime::FAIR_TIME,
			  MasterPasswordLifetime::LONG_TIME };

		MasterPasswordLifetime loaded_lifetime;
		loaded_lifetime.Read();

		for (unsigned i = 0; i < ARRAY_SIZE(values); ++i)
		{
			lifetime.SetValue(values[i]);
			RETURN_IF_ERROR(lifetime.ToString(item));
			RETURN_IF_ERROR(dropdown->AddItem(item.CStr(), -1, NULL, lifetime.GetValue()));

			if (loaded_lifetime == lifetime)
				dropdown->SelectItem(i, TRUE);
		}
	}

	{
		BOOL has_master_password = g_libcrypto_master_password_handler->HasMasterPassword();
		SetWidgetEnabled("Master_password_checkbox", has_master_password);
		SetWidgetEnabled("Ask_for_password_dropdown", has_master_password);
	}

	dropdown = (OpDropDown*)GetWidgetByName("auto_update_level_dropdown");
    if( dropdown )
	{
#ifdef AUTO_UPDATE_SUPPORT
		OpString str;

		g_languageManager->GetString(Str::D_OPERA_UPDATE_LEVEL_NO_CHECK, str);
		dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::D_OPERA_UPDATE_LEVEL_CHECK, str);
		dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::D_OPERA_UPDATE_LEVEL_INSTALL, str);
		dropdown->AddItem(str.CStr());

		dropdown->SelectItem(g_autoupdate_manager->GetLevelOfAutomation(), TRUE);

		// Hide the control is the Package update is disabled
		if (g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate))
			dropdown->SetVisibility(FALSE);
#else // AUTO_UPDATE_SUPPORT
		dropdown->SetVisibility(FALSE);
#endif // AUTO_UPDATE_SUPPORT
	}
	OpWidget* widget = GetWidgetByName("auto_update_level_label");
	if (widget)
	{
#ifdef AUTO_UPDATE_SUPPORT
		// Hide the control is the Package update is disabled
		if (g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate))
			widget->SetVisibility(FALSE);
#else
		widget->SetVisibility(FALSE);
#endif // AUTO_UPDATE_SUPPORT
	}
	
	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveSecurity()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Ask_insecure_form_checkbox"), PrefsCollectionNetwork::WarnInsecureFormSubmit);
# if defined(_VALIDATION_SUPPORT_)
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Ask_validation_checkbox"), PrefsCollectionUI::ShowValidationDialog);
# endif
#ifdef TRUST_RATING
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Trust_info_checkbox"), PrefsCollectionNetwork::EnableTrustRating);
#endif // TRUST_RATING
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Do_not_track_checkbox"), PrefsCollectionNetwork::DoNotTrack);

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Ask_for_password_dropdown");

	if (dropdown)
	{
		MasterPasswordLifetime lifetime;

		lifetime.SetValue(dropdown->GetItemUserData(dropdown->GetSelectedItem()));

		lifetime.Write();
	}

#ifdef AUTO_UPDATE_SUPPORT
	dropdown = (OpDropDown*)GetWidgetByName("auto_update_level_dropdown");
	if (dropdown)
	{
		g_autoupdate_manager->SetLevelOfAutomation((LevelOfAutomation)dropdown->GetSelectedItem());
	}
#endif // AUTO_UPDATE_SUPPORT

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::InitNetwork()
{
    static const int connections[] =
    {
        1, 2, 4, 8, 16, 20, 32, 64, 128
    };

	OpString standard;
	g_languageManager->GetString(Str::D_DEFAULT_PREFS_COMMENT, standard);

    int max_connections_to_server = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer);

    OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Max_connections_to_server_dropdown");
    if (dropdown)
    {
        OpString buffer;
        if (buffer.Reserve(30))
		{
			for (size_t i = 0; i < ARRAY_SIZE(connections); i++)
			{
				uni_ltoa(connections[i], buffer.CStr(), 10);

				if (connections[i] == DEFAULT_MAX_CONNECTIONS_SERVER)
				{
					buffer.Append(standard.CStr());
				}

				dropdown->AddItem(buffer.CStr(), -1, NULL, connections[i]);

				if (max_connections_to_server == connections[i])
				{
					dropdown->SelectItem(i, TRUE);
				}
			}
		}
    }

    int max_connections_total = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal);

    dropdown = (OpDropDown*) GetWidgetByName("Max_connections_total_dropdown");
    if (dropdown)
    {
        OpString buffer;
        if (buffer.Reserve(30))
		{
			for (size_t i = 0; i < ARRAY_SIZE(connections); i++)
			{
				uni_ltoa(connections[i], buffer.CStr(), 10);

				if (connections[i] == DEFAULT_MAX_CONNECTIONS_TOTAL)
				{
					buffer.Append(standard.CStr());
				}

				dropdown->AddItem(buffer.CStr(), -1, NULL, connections[i]);

				if (max_connections_total == connections[i])
				{
					dropdown->SelectItem(i, TRUE);
				}
			}
		}
    }

	WidgetPrefs::GetIntegerPref(GetWidgetByName("International_address_checkbox"), PrefsCollectionNetwork::UseUTF8Urls);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_referrer_logging_checkbox"), PrefsCollectionNetwork::ReferrerEnabled);

	// both these control redirect
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientRefresh);
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientPull);
#ifdef DOM_GEOLOCATION_SUPPORT
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_geolocation_checkbox"), PrefsCollectionGeolocation::EnableGeolocation);
#endif // DOM_GEOLOCATION_SUPPORT

	return OpStatus::OK;
}


OP_STATUS NewPreferencesDialog::SaveNetwork()
{
    int max_connections_server = 0;
    int max_connections_total = 0;

    OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Max_connections_to_server_dropdown");
    if (dropdown)
    {
        max_connections_server = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());
        max_connections_server = max_connections_server < 1 ? 1 : max_connections_server;
        max_connections_server = max_connections_server > MAX_CONNECTIONS_SERVER ? MAX_CONNECTIONS_SERVER : max_connections_server;
        TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::MaxConnectionsServer, max_connections_server));
    }

    dropdown = (OpDropDown*)GetWidgetByName("Max_connections_total_dropdown");
    if (dropdown)
    {
        max_connections_total = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());
        if (max_connections_total > MAX_CONNECTIONS_TOTAL)
        {
            max_connections_total = 1;
        }
        TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::MaxConnectionsTotal, max_connections_total));
    }

	WidgetPrefs::SetIntegerPref(GetWidgetByName("International_address_checkbox"), PrefsCollectionNetwork::UseUTF8Urls);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_referrer_logging_checkbox"), PrefsCollectionNetwork::ReferrerEnabled);

	// both these control redirect
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientRefresh);
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientPull);

#ifdef DOM_GEOLOCATION_SUPPORT
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_geolocation_checkbox"), PrefsCollectionGeolocation::EnableGeolocation);
#endif // DOM_GEOLOCATION_SUPPORT

	return OpStatus::OK;
}

/********************************************************************************
 *
 * Local Storage
 *
 *******************************************************************************/
OP_STATUS NewPreferencesDialog::InitStorageModel()
{
	OpStringHashTable<INTPTR> storage;

	// Offline application cache
	OpAutoVector<OpWindowCommanderManager::OpApplicationCacheEntry> caches;
	g_windowCommanderManager->GetAllApplicationCacheEntries(caches);
	for (UINT32 i=0; i<caches.GetCount(); i++)
	{
		const uni_char* domain = caches.Get(i)->GetApplicationCacheDomain();
		INTPTR size = caches.Get(i)->GetApplicationCacheCurrentDiskUse();
		if (domain)
		{
			if (storage.Contains(domain))
			{
				INTPTR* val = NULL;
				storage.GetData(domain, &val);
				storage.Remove(domain, &val);
				size += (INTPTR)val;
			}

			storage.Add(domain, (INTPTR*)size);
		}
	}

	// Web storage/database
	PS_IndexIterator* it;
	RETURN_IF_LEAVE(it = g_database_manager->GetIteratorL(0, PS_ObjectTypes::KDBTypeStart, FALSE, PS_IndexIterator::ORDERED_ASCENDING));
	OpStackAutoPtr<PS_IndexIterator> it_ptr(it);

	PS_IndexEntry *key;

	RETURN_IF_LEAVE(key = it->GetItemL());

	OpFileLength size;
	while (key)
	{
		const uni_char *domain = key->GetDomain();
		if (!key->WillBeDeleted() && key->IsPersistent() && domain)
		{
			RETURN_IF_ERROR(key->GetDataFileSize(&size));
			if (storage.Contains(domain))
			{
				INTPTR* val = NULL;
				storage.GetData(domain, &val);
				storage.Remove(domain, &val);
				size += (INTPTR)val;
			}

			storage.Add(domain, (INTPTR*)size);
		}

		RETURN_IF_LEAVE(it->MoveNextL());
		RETURN_IF_LEAVE(key = it->GetItemL());
	}

	// Build tree model
	OpString str;
	RETURN_IF_LEAVE(str.ReserveL(256));
	g_languageManager->GetString(Str::S_CACHE_DOMAIN, str);
	m_local_storage_model.SetColumnData(0, str.CStr());
	g_languageManager->GetString(Str::SI_SIZE_TEXT, str);
	m_local_storage_model.SetColumnData(1, str.CStr());

	m_local_storage_model.RemoveAll();
	OpHashIterator *iter = storage.GetIterator();
	if(!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = iter->First();
	while(ret == OpStatus::OK)
	{
		UINT32 value = (UINT32)(INTPTR)iter->GetData();

		uni_char* key = (uni_char *)iter->GetKey();
		StrFormatByteSize(str, value, 0);
		
		int index = m_local_storage_model.AddItem(key);

		m_local_storage_model.SetItemData(index, 1, str.CStr(), 0, value);

		ret = iter->Next();
	}
	OP_DELETE(iter);

	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::InitLocalStorage()
{
	// Tree
	InitStorageModel();
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Storage_treeview");
	if (treeview)
	{
		treeview->SetTreeModel(&m_local_storage_model);
		treeview->SetColumnWeight(0,200);
		treeview->SetSelectedItem(0);
		treeview->SetMultiselectable(TRUE);
	}

	// Strategy
	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Application_cache_dropdown");
	if (dropdown)
	{
		OpString str;
		g_languageManager->GetString(Str::D_GEOLOCATION_YES, str);
		dropdown->AddItem(str.CStr(), -1, NULL, Accept);

		g_languageManager->GetString(Str::D_GEOLOCATION_NO, str);
		dropdown->AddItem(str.CStr(), -1, NULL, Reject);

		g_languageManager->GetString(Str::D_GEOLOCATION_ASK, str);
		dropdown->AddItem(str.CStr(), -1, NULL, Ask);

		int val = g_pcui->GetIntegerPref(PrefsCollectionUI::StrategyOnApplicationCache);
		if (val == Accept)
			dropdown->SelectItem(0, TRUE);
		else if (val == Reject)
			dropdown->SelectItem(1, TRUE);
		else
			dropdown->SelectItem(2, TRUE);
	}

	return OpStatus::OK;
}

OP_STATUS NewPreferencesDialog::DeleteLocalStorage(const uni_char* domain)
{
	OP_STATUS ret = OpStatus::OK;

	if (domain)
	{
		// Web storage/database
		PS_IndexIterator* it;
		RETURN_IF_LEAVE(it = g_database_manager->GetIteratorL(0, PS_ObjectTypes::KDBTypeStart, FALSE));
		OpStackAutoPtr<PS_IndexIterator> it_ptr(it);

		PS_IndexEntry *key;
		RETURN_IF_LEAVE(key = it->GetItemL());

		while (key)
		{
			const uni_char* entry_domain = key->GetDomain();
			if (entry_domain && uni_stricmp(domain, entry_domain) == 0)
				key->Delete();

			RETURN_IF_LEAVE(it->MoveNextL());
			RETURN_IF_LEAVE(key = it->GetItemL());
		}

		// Offline application cache
		OpAutoVector<OpWindowCommanderManager::OpApplicationCacheEntry> caches;
		g_windowCommanderManager->GetAllApplicationCacheEntries(caches);
		for (UINT32 i=0; i<caches.GetCount(); i++)
		{
			const uni_char* d = caches.Get(i)->GetApplicationCacheDomain();
			if (d && uni_stricmp(domain,d) == 0)
			{
				if (OpStatus::IsError(g_windowCommanderManager->DeleteApplicationCache(caches.Get(i)->GetApplicationCacheManifestURL())))
					ret = OpStatus::ERR;

				// also reset the site prefs for application cache strategy
				TRAPD(err, g_pcui->RemoveOverridesL(domain));
				if (OpStatus::IsError(err))
					ret = OpStatus::ERR;
			}
		}
	}

	return ret;
}

OP_STATUS NewPreferencesDialog::SaveLocalStorage()
{
	// dropdown
	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Application_cache_dropdown");
	if (dropdown)
	{
		INTPTR selected = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StrategyOnApplicationCache, selected));
	}
	return OpStatus::OK;
}

void NewPreferencesDialog::GetFontAttAndColorFromFontListItem(FontListItem* font_item, FontAtt& font, COLORREF& color, BOOL& fontname_only)
{
	switch (font_item->GetFontId())
	{
	case OP_SYSTEM_FONT_CSS_SERIF:
	case OP_SYSTEM_FONT_CSS_SANS_SERIF:
	case OP_SYSTEM_FONT_CSS_CURSIVE:
	case OP_SYSTEM_FONT_CSS_FANTASY:
	case OP_SYSTEM_FONT_CSS_MONOSPACE:
		{
			fontname_only = TRUE;
			font.SetHeight(0);

			PrefsCollectionDisplay::stringpref family = PrefsCollectionDisplay::DummyLastStringPref;
			switch (font_item->GetFontId())
			{
			case OP_SYSTEM_FONT_CSS_SERIF:
				family = PrefsCollectionDisplay::CSSFamilySerif;
				break;
			case OP_SYSTEM_FONT_CSS_SANS_SERIF:
				family = PrefsCollectionDisplay::CSSFamilySansserif;
				break;
			case OP_SYSTEM_FONT_CSS_CURSIVE:
				family = PrefsCollectionDisplay::CSSFamilyCursive;
				break;
			case OP_SYSTEM_FONT_CSS_FANTASY:
				family = PrefsCollectionDisplay::CSSFamilyFantasy;
				break;
			case OP_SYSTEM_FONT_CSS_MONOSPACE:
				family = PrefsCollectionDisplay::CSSFamilyMonospace;
				break;
			}

			OpStringC fontname = g_pcdisplay->GetStringPref(family);

			if (fontname.HasContent())
				font.SetFaceName(fontname.CStr());
		}
		break;

	case OP_SYSTEM_FONT_FORM_TEXT:
	case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
	case OP_SYSTEM_FONT_FORM_BUTTON:
		{
#ifdef DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			LayoutStyle *styl = styleManager->GetStyleEx(font_item->GetHTMLType());
#else // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			Style *styl = styleManager->GetStyleEx(font_item->GetHTMLType());
#endif // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			PresentationAttr pres = styl->GetPresentationAttr();
			font = *(pres.GetPresentationFont(WritingSystem::Unknown).Font);
			color = pres.Color;
		}
		break;

	case OP_SYSTEM_FONT_EMAIL_COMPOSE:
	case OP_SYSTEM_FONT_HTML_COMPOSE:
	case OP_SYSTEM_FONT_EMAIL_DISPLAY:
	case OP_SYSTEM_FONT_UI_MENU:
	case OP_SYSTEM_FONT_UI_TOOLBAR:
	case OP_SYSTEM_FONT_UI_DIALOG:
	case OP_SYSTEM_FONT_UI_PANEL:
	case OP_SYSTEM_FONT_UI_TOOLTIP:
	case OP_SYSTEM_FONT_UI_HEADER:
		g_pcfontscolors->GetFont(OP_SYSTEM_FONT(font_item->GetFontId()), font);
		break;

	default:
		{
#ifdef DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			LayoutStyle *styl = styleManager->GetStyle((HTML_ElementType)font_item->GetHTMLType());
#else // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			Style *styl = styleManager->GetStyle((HTML_ElementType)font_item->GetHTMLType());
#endif // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			PresentationAttr pres = styl->GetPresentationAttr();
			font = *(pres.GetPresentationFont(WritingSystem::Unknown).Font);
			color = pres.Color;
		}
	}
}


void NewPreferencesDialog::UpdateWidgetFont(const char* widget_name, HTML_ElementType style)
{
	OpWidget* widget = GetWidgetByName(widget_name);

	if (widget == NULL)
		return;

	FontAtt dialog_font;
	g_pcfontscolors->GetFont(OP_SYSTEM_FONT_UI_DIALOG, dialog_font);

#ifdef DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
    LayoutStyle *s = styleManager->GetStyle(style);
#else // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
    Style *s = styleManager->GetStyle(style);
#endif // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
    PresentationAttr p = s->GetPresentationAttr();
	const FontAtt& font = *(p.GetPresentationFont(WritingSystem::Unknown).Font);
	UINT32 fontnr = font.GetFontNumber();

	INT32 font_size = min(dialog_font.GetSize()*15/10, font.GetSize());

	const OpFontInfo* fontinfo = styleManager->GetFontInfo(fontnr);

	if (fontinfo)
		widget->SetText(fontinfo->GetFace());

	widget->SetFontInfo(fontinfo, font_size, font.GetItalic(), font.GetWeight(), JUSTIFY_CENTER);
}


void NewPreferencesDialog::OnColorSelected(const char* widget_name, COLORREF color)
{
	OpWidget* widget;
	widget = GetWidgetByName(widget_name);
	if (widget)
	{
		widget->SetBackgroundColor(color);
	}
}


void NewPreferencesDialog::OnColorSelected(COLORREF color)
{
	UINT32 new_color = OP_RGB(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color));

	OnColorSelected(m_selected_color_chooser.CStr(), new_color);
}


void NewPreferencesDialog::OnFontSelected(FontAtt& font, COLORREF color)
{
	// Insert your code here
#ifdef DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
	LayoutStyle *styl;
#else // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
	Style *styl;
#endif // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
	PresentationAttr pres;
	LayoutAttr lattr;

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Fonts_treeview");

	if (treeview)
	{
		int pos = treeview->GetSelectedItemModelPos();

		if (m_selected_font_chooser.CompareI("Normal_font_button") == 0)
		{
			pos = POSITION_OF_NORMAL_FONT_IN_LIST;
		}
		else if (m_selected_font_chooser.CompareI("Monospace_button") == 0)
		{
			pos = POSITION_OF_NORMAL_FONT_IN_LIST + 1;
		}

		if (pos != -1)
		{
			m_fonts_model.SetItemData(pos, 1, font.GetFaceName());

			switch (m_font_list.Get(pos)->GetFontId())
			{
				case OP_SYSTEM_FONT_CSS_SERIF:
				{
					TRAPD(err, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::CSSFamilySerif, OpStringC(font.GetFaceName())));
					return;
				}

				case OP_SYSTEM_FONT_CSS_SANS_SERIF:
				{
					TRAPD(err, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::CSSFamilySansserif, OpStringC(font.GetFaceName())));
					return;
				}

				case OP_SYSTEM_FONT_CSS_CURSIVE:
				{
					TRAPD(err, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::CSSFamilyCursive, OpStringC(font.GetFaceName())));
					return;
				}
				case OP_SYSTEM_FONT_CSS_FANTASY:
				{
					TRAPD(err, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::CSSFamilyFantasy, OpStringC(font.GetFaceName())));
					return;
				}
				case OP_SYSTEM_FONT_CSS_MONOSPACE:
				{
					TRAPD(err, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::CSSFamilyMonospace, OpStringC(font.GetFaceName())));
					return;
				}
			case OP_SYSTEM_FONT_EMAIL_COMPOSE:
			case OP_SYSTEM_FONT_HTML_COMPOSE:
			case OP_SYSTEM_FONT_EMAIL_DISPLAY:
			case OP_SYSTEM_FONT_UI_MENU:
			case OP_SYSTEM_FONT_UI_TOOLBAR:
			case OP_SYSTEM_FONT_UI_DIALOG:
			case OP_SYSTEM_FONT_UI_PANEL:
			case OP_SYSTEM_FONT_UI_TOOLTIP:
			case OP_SYSTEM_FONT_UI_HEADER:
			{
				g_pcfontscolors->WriteFontL(OP_SYSTEM_FONT(m_font_list.Get(pos)->GetFontId()), font);
				return;
			}

			case OP_SYSTEM_FONT_FORM_TEXT:
			case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
			case OP_SYSTEM_FONT_FORM_BUTTON:
				styl = styleManager->GetStyleEx(m_font_list.Get(pos)->GetHTMLType());
				break;

			default:
				styl = styleManager->GetStyle((HTML_ElementType)m_font_list.Get(pos)->GetHTMLType());
				break;
			}

			m_fonts_model.SetItemColor(pos, 1, color);

    		pres = styl->GetPresentationAttr();
    		lattr = styl->GetLayoutAttr();
			WritingSystem::Script def_script = WritingSystem::Unknown;
    		*(pres.GetPresentationFont(def_script).Font) = font;
			pres.Color = color;
			short lsep = (short) (abs(pres.GetPresentationFont(def_script).Font->GetHeight()) * lsep_scale);
			short tsep = (short) (abs(pres.GetPresentationFont(def_script).Font->GetHeight()) * tsep_scale);
			if ((HTML_ElementType)m_font_list.Get(pos)->GetHTMLType() != HE_DOC_ROOT)
			{
				lattr.LeadingSeparation = lsep;
				lattr.TrailingSeparation = tsep;
			}
			styl->SetPresentationAttr(pres);
			styl->SetLayoutAttr(lattr);

			g_pcfontscolors->WriteFontL(OP_SYSTEM_FONT(m_font_list.Get(pos)->GetFontId()), *(pres.GetPresentationFont(def_script).Font));
			if (m_font_list.Get(pos)->GetColorId() != -1)
			{
				g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR(m_font_list.Get(pos)->GetColorId()), pres.Color);
			}

			switch (m_font_list.Get(pos)->GetHTMLType())
			{
#ifdef HAS_FONT_FOUNDRY
			case HE_DOC_ROOT:
			{
				OpFontManager *fontman = styleManager->GetFontManager();
				OpString preferred_foundry;
				fontman->GetFoundry(font.GetFaceName(), preferred_foundry);
				fontman->SetPreferredFoundry(preferred_foundry.CStr());
				break;
			}
#endif // HAS_FONT_FOUNDRY
			case HE_PRE:
				styl = styleManager->GetStyle(HE_PRE);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);

				styl = styleManager->GetStyle(HE_LISTING);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);
				styl = styleManager->GetStyle(HE_PLAINTEXT);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);
				styl = styleManager->GetStyle(HE_XMP);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);
				styl = styleManager->GetStyle(HE_CODE);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);
				styl = styleManager->GetStyle(HE_SAMP);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);
				styl = styleManager->GetStyle(HE_TT);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);

				styl = styleManager->GetStyle(HE_KBD);
				styl->SetPresentationAttr(pres);
				styl->SetLayoutAttr(lattr);

				break;

			case STYLE_EX_FORM_TEXTINPUT:
				styl = styleManager->GetStyleEx(STYLE_EX_FORM_TEXTINPUT);
				styl->SetPresentationAttr(pres);
				break;

			case STYLE_EX_FORM_TEXTAREA:
				styl = styleManager->GetStyleEx(STYLE_EX_FORM_TEXTAREA);
				styl->SetPresentationAttr(pres);
				break;

			case STYLE_EX_FORM_BUTTON:
				styl = styleManager->GetStyleEx(STYLE_EX_FORM_SELECT);
				styl->SetPresentationAttr(pres);
				styl = styleManager->GetStyleEx(STYLE_EX_FORM_BUTTON);
				styl->SetPresentationAttr(pres);

				break;
			}
		}
	}

	UpdateWidgetFont("Normal_font_button", HE_DOC_ROOT);
	UpdateWidgetFont("Monospace_button", HE_PRE);
}

void NewPreferencesDialog::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Program_sounds_treeview");

	if (result.files.GetCount() && treeview && treeview->GetSelectedItemModelPos() != -1)
		m_sounds_model.SetItemData(treeview->GetSelectedItemModelPos(), 1, result.files.Get(0)->CStr());
}


void NewPreferencesDialog::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg) {
	case MSG_VIEWERS_ADDED:
		OnViewerAdded(reinterpret_cast<Viewer*>(par1));
		break;
	case MSG_VIEWERS_CHANGED:
		OnViewerChanged(reinterpret_cast<Viewer*>(par1));
		break;
	case MSG_VIEWERS_DELETED:
		OnViewerDeleted(reinterpret_cast<Viewer*>(par1));
		break;
	default:
		Dialog::HandleCallback(msg, par1, par2);
	}
}

void NewPreferencesDialog::OnSecurityStateChange(BOOL successful, BOOL changed, BOOL is_now_security_encrypted)
{
	// If the change was synchronous or asynchronous, and the user canceled/or the action was otherwise unsuccessful
	// we need to revert the change in prefs UseParanoidMailpassword:
	m_previous_paranoid_pref = is_now_security_encrypted;

	SetWidgetValue("Master_password_checkbox", is_now_security_encrypted);

	g_wand_manager->RemoveListener(this);
}

void NewPreferencesDialog::OnSecurityPasswordNeeded(OpWindowCommander*, SSLSecurityPasswordCallback* callback)
{
	SecurityPasswordDialog* dialog = OP_NEW(SecurityPasswordDialog, (callback));
	if (dialog)
	{
		dialog->Init(this);
	}
}

BOOL NewPreferencesDialog::IsCurrentAdvancedPage(AdvancedPageId id)
{
	return (GetCurrentPage() == AdvancedPage 
		&& m_advanced_treeview
		&& m_advanced_treeview->GetSelectedItemPos() == id - FirstAdvancedPage);
}

/*
 * Helper class LanguageCodeInfo
 *
 */

LanguageCodeInfo::LanguageCodeInfo()
{
	OpString languagecodes_file_name;
	languagecodes_file_name.Set(g_pcui->GetStringPref(PrefsCollectionUI::LanguageCodesFileName));
	if (languagecodes_file_name.IsEmpty())
	{
		OpString tmp_storage;
		languagecodes_file_name.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_RESOURCES_FOLDER, tmp_storage));
		languagecodes_file_name.Append("lngcode.txt");
	}

	OpFile codefile;
	if (OpStatus::IsSuccess(codefile.Construct(languagecodes_file_name.CStr())))
	{
		PrefsFile datafile(PREFS_LNG);
		PrefsSection *section = NULL;

		TRAPD(err, datafile.ConstructL(); \
			datafile.SetFileL(&codefile); \
			datafile.LoadAllL(); \
			section = datafile.ReadSectionL(UNI_L("Languages")));

		// Copy data into our structure
		if (section)
		{
			const PrefsEntry *entry = section->Entries();
			while (entry)
			{
				LanguageCodeInfoItem* item = OP_NEW(LanguageCodeInfoItem, (entry->Key(), entry->Value()));
				if (item)
					m_languages.Add(item);

				entry = (const PrefsEntry *) entry->Suc();
			}
			OP_DELETE(section);
		}
	}

	if (m_languages.GetCount() == 0)
	{
		LanguageCodeInfoItem* item = OP_NEW(LanguageCodeInfoItem, (UNI_L("en"), UNI_L("English")));
		if (item)
			m_languages.Add(item);
	}
}

OP_STATUS LanguageCodeInfo::GetNameByCode(OpString& code, OpString& name)
{
	for (UINT i = 0; i < m_languages.GetCount(); i++)
	{
		LanguageCodeInfoItem* item = m_languages.Get(i);
		if (item && *item == code)
		{
			return item->GetName(name);
		}
	}

	return name.Set(code);
}


AddWebLanguageDialog::~AddWebLanguageDialog()
{

}

OP_STATUS AddWebLanguageDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
	return OpStatus::OK;
}

void AddWebLanguageDialog::OnInit()
{
	OpStringC accept_languages = g_pcnet->GetStringPref(PrefsCollectionNetwork::AcceptLanguage);

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Add_Web_language_treeview");
	if (treeview)
	{
		treeview->SetTreeModel(&m_web_language_model);
		treeview->SetShowColumnHeaders(FALSE);

		for (UINT32 i = 0; i < m_language_info.GetCount(); i++)
		{
			LanguageCodeInfoItem* item = m_language_info.GetItem(i);
			if (item)
			{
				OpString name, code;
				item->GetName(name);
				item->GetCode(code);
				name.AppendFormat(UNI_L(", [%s]"), code.CStr());
				m_web_language_model.AddItem(name.CStr());
			}
		}
		treeview->Sort( 0, TRUE);
	}
}

UINT32 AddWebLanguageDialog::OnOk()
{
	OpString name, code;

	GetWidgetText("Add_Web_language_edit", code);
	if (code.HasContent())
	{
		if (code.Find(UNI_L("[")) == KNotFound || code.Find(UNI_L("]")) == KNotFound)
		{
			m_language_info.GetNameByCode(code, name);
			name.AppendFormat(UNI_L(", [%s]"), code.CStr());
		}
		else
		{
			name.Set(code);
		}

		((LanguagePreferencesDialog*)GetParentDesktopWindow())->AddWebLanguage(name);

		return 0;
	}

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Add_Web_language_treeview");
	if (treeview)
	{
		UINT32 pos = treeview->GetSelectedItemModelPos();
		if ((INT32)pos != -1)
		{
			LanguageCodeInfoItem* item = m_language_info.GetItem(pos);
			if (item)
			{
				item->GetName(name);
				item->GetCode(code);
				name.AppendFormat(UNI_L(", [%s]"), code.CStr());

				((LanguagePreferencesDialog*)GetParentDesktopWindow())->AddWebLanguage(name);

				return 0;
			}
		}
	}


	return 0;
}

void AddWebLanguageDialog::OnCancel()
{

}



LanguagePreferencesDialog::~LanguagePreferencesDialog()
{

}

OP_STATUS LanguagePreferencesDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
	return OpStatus::OK;
}

void LanguagePreferencesDialog::OnInit()
{
	// find the accept languages

	LanguageCodeInfo language_info;

	OpStringC accept_languages = g_pcnet->GetStringPref(PrefsCollectionNetwork::AcceptLanguage);

	DesktopFileChooserEdit* chooser = (DesktopFileChooserEdit*) GetWidgetByName("UI_language_chooser");
	if (chooser)
	{
		OpString str;
		GetCurrentLanguageFile(str);
		chooser->SetText(str.CStr());

		g_languageManager->GetString(Str::D_SELECT_LANGUAGE_FILE, str);
		chooser->SetTitle(str.CStr());

		g_languageManager->GetString(Str::D_LANGUAGE_TYPES, str);
		chooser->SetFilterString(str.CStr());
	}

	if(accept_languages.HasContent())
	{
		m_web_language_model.DeleteAll();

		OpString userdefined, language_code_only;
		g_languageManager->GetString(Str::SI_IDSTR_USERDEF_LANGUAGE, userdefined);

		const uni_char* str_lang = accept_languages.CStr();
		do
		{
			// Ignore empty or single-letter strings
			if (SplitAcceptLanguage(&str_lang, language_code_only) < 2)
				continue;

			// Locate language name
			OpString langname;

			// Retrieve language name from language codes list
			language_info.GetNameByCode(language_code_only, langname);
			if (langname.Compare(language_code_only) == 0)
			{
				if (!ParseRegionSuffix(language_code_only,langname))
					langname.Set(userdefined);
			}

			// Tack the ISO 639 to the end two columns, one with code, one with name
			langname.AppendFormat(UNI_L(", [%s]"), language_code_only.CStr());
			m_web_language_model.AddItem(langname.CStr());
		}
		while (str_lang);
	}

	// parse the string with language abbrevations and set the actual abbrevations to the array

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Web_language_treeview");

	if (treeview)
	{
		OpString preferred_language;
		g_languageManager->GetString(Str::DI_ID_TXT_PREFTXT, preferred_language);

		m_web_language_model.SetColumnData(0, preferred_language.CStr());
		treeview->SetTreeModel(&m_web_language_model);
		treeview->SetUserSortable(FALSE);
	}

	// Setup the default character set dialog

	OpDropDown* encoding_dropdown = (OpDropDown*) GetWidgetByName("Fallback_encoding_dropdown");

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	const char *const *charsets = g_charsetManager->GetCharsetNames();
	const char *htmlEncoding = g_pcdisplay->GetDefaultEncoding();

	if (charsets && encoding_dropdown)
	{
		int htmlidx = 0;
		for (int idx = 0; charsets[idx]; idx ++)
		{
			OpString charset;
			charset.Set(charsets[idx]);

			encoding_dropdown->AddItem(charset.CStr());
			if (!op_stricmp(charsets[idx], htmlEncoding)) htmlidx = idx;
		}

		encoding_dropdown->SelectItem(htmlidx, TRUE);
	}
	else
#endif
	{
		// If we do not have any character sets, disable the preferences
		// associated with it

		SetWidgetEnabled("Fallback_encoding_dropdown", FALSE);
	}
}

UINT32 LanguagePreferencesDialog::OnOk()
{
	int langcode_count = m_web_language_model.GetItemCount();
	OpString accept_languages;
	BOOL language_settings_changed = FALSE;
	double q_value = 1.0;

	for (int i = 0; i < langcode_count; i++)
	{
		if (i > 0)
			accept_languages.Append(",");

		const uni_char* tempstr = m_web_language_model.GetItemString(i);
		uni_char* first_brace = uni_strchr(tempstr, '[');
		uni_char* last_brace = uni_strrchr(tempstr, ']');
		if (first_brace && last_brace)
		{
			/* Limit to 10 characters (in practice longest language codes are
			   5 characters long, for example "en-EN"). */
			int copy_len = MIN(10, last_brace - first_brace - 1);
			RETURN_VALUE_IF_ERROR(accept_languages.Append(first_brace + 1, copy_len), 0);
			if (i > 0)
				RETURN_VALUE_IF_ERROR(accept_languages.AppendFormat(";q=%3.1f", q_value), 0);
			if (q_value >= 0.2)
				q_value -= 0.1;
		}
	}

	OpStringC existing_accept_languages = g_pcnet->GetStringPref(PrefsCollectionNetwork::AcceptLanguage);
	if (accept_languages.CompareI(existing_accept_languages) != 0)
	{
		TRAPD(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::AcceptLanguage, accept_languages));
		language_settings_changed = TRUE;
	}

	DesktopFileChooserEdit* language_chooser = (DesktopFileChooserEdit*)GetWidgetByName("UI_language_chooser");

	if (language_chooser)
	{
		OpString filename;
		language_chooser->GetText(filename);

		if(filename[0] == '\"')
		{
			if (filename[filename.Length()-1] == '\"')
			{
				filename.Delete(0, 1);
				filename.Delete(filename.Length() - 1, 1);
			}
		}

		OpString current_language_file_path;
		GetCurrentLanguageFile( current_language_file_path );

		if( !current_language_file_path.IsEmpty() && current_language_file_path.CompareI(filename) != 0 )
		{
			if( !IsValidLanguageFileVersion( filename, this->GetParentDesktopWindow(), TRUE ) )
			{
				return 0;
			}

#ifdef _MACINTOSH_
			// On Mac if there's no string we will assume that this means that we want to go back to
			// letting the system decide the language (i.e. Remove the preference from the file)
			if (filename.IsEmpty())
			{
				TRAPD(err, g_pcfiles->ResetFileL(PrefsCollectionFiles::LanguageFile));
			}
			else
			{
#endif
				OpFile languagefile;
				OP_STATUS err = languagefile.Construct(filename.CStr());
				BOOL exists;
				if (OpStatus::IsSuccess(err) && OpStatus::IsSuccess(languagefile.Exists(exists)) && exists)
				{
					TRAP(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::LanguageFile, &languagefile));

					OpTreeView* searches = (OpTreeView*)GetWidgetByName("Web_search_treeview");
					if (searches)
						searches->SetTreeModel(NULL);

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
					TRAP(err, g_searchEngineManager->LoadSearchesL()); // Search engines can be localized
#endif // DESKTOP_UTIL_SEARCH_ENGINES

					language_settings_changed = TRUE;
				}
#ifdef _MACINTOSH_
			}
#endif
		}
	}

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
	// Check selected fallback character encoding

	OpDropDown* encoding_dropdown = (OpDropDown*)GetWidgetByName("Fallback_encoding_dropdown");

	if (encoding_dropdown)
	{
		TRAPD(err, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::DefaultEncoding, encoding_dropdown->GetItemText(encoding_dropdown->GetSelectedItem())));
	}
#endif

	if (language_settings_changed)
	{
		g_application->SettingsChanged(SETTINGS_LANGUAGE);
		g_application->SettingsChanged(SETTINGS_MENU_SETUP);
		g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
	}
	return 0;
}

void LanguagePreferencesDialog::OnCancel()
{
}

BOOL LanguagePreferencesDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_ADD_WEB_LANGUAGE:
		{
			AddWebLanguageDialog* dialog = OP_NEW(AddWebLanguageDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
		case OpInputAction::ACTION_DELETE:
		{
			OpTreeView* treeview = NULL;
			OpButton* button = NULL;

			treeview = (OpTreeView*) GetWidgetByName("Web_language_treeview");
			button = (OpButton*) GetWidgetByName("Remove_language_button");

			if (treeview && button && (treeview->IsFocused() || button->IsFocused()))
			{
				UINT32 pos = treeview->GetSelectedItemModelPos();
				if ((INT32)pos != -1)
				{
					m_web_language_model.Delete(pos);
				}
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_MOVE_ITEM_UP:
		case OpInputAction::ACTION_MOVE_ITEM_DOWN:
		{
			OpTreeView* treeview = NULL;
			OpButton* button_up = NULL;
			OpButton* button_down = NULL;

			treeview = (OpTreeView*) GetWidgetByName("Web_language_treeview");
			button_up = (OpButton*) GetWidgetByName("Move_language_up_button");
			button_down = (OpButton*) GetWidgetByName("Move_language_down_button");

			if (treeview && button_up && button_down && (treeview->IsFocused() || button_up->IsFocused() || button_down->IsFocused()))
			{
				INT32 pos = treeview->GetSelectedItemModelPos();
				if (pos > 0 && action->GetAction() == OpInputAction::ACTION_MOVE_ITEM_UP)
				{
					OpString data;
					data.Set(m_web_language_model.GetItemString(pos));

					m_web_language_model.Delete(pos);
					m_web_language_model.InsertBefore(pos-1, data.CStr());

					treeview->SetSelectedItem(pos-1);
				}
				else if (pos != -1 && (INT32)pos+1 < m_web_language_model.GetItemCount() &&
					     action->GetAction() == OpInputAction::ACTION_MOVE_ITEM_DOWN)
				{
					OpString data;
					data.Set(m_web_language_model.GetItemString(pos));

					m_web_language_model.Delete(pos);
					m_web_language_model.InsertAfter(pos, data.CStr());

					treeview->SetSelectedItem(pos+1);
				}
				return TRUE;
			}
			break;
		}
	}
	return Dialog::OnInputAction(action);
}

void LanguagePreferencesDialog::OnInitVisibility()
{
#ifdef _MACINTOSH_
	ShowWidget("Web_language_treeview", FALSE);
	ShowWidget("Add_language_button", FALSE);
	ShowWidget("Remove_language_button", FALSE);
	ShowWidget("Move_language_up_button", FALSE);
	ShowWidget("Move_language_down_button", FALSE);
#endif
}

OP_STATUS LanguagePreferencesDialog::AddWebLanguage(OpString &language)
{
	m_web_language_model.AddItem(language.CStr());
	return OpStatus::OK;
}


#ifdef MSWIN
DefaultApplicationDialog::~DefaultApplicationDialog()
{

}

OP_STATUS DefaultApplicationDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
	return OpStatus::OK;
}

struct OperaAssociation
{
	char *widget_name;
	OperaInstaller::AssociationsSupported association;
};

const OperaAssociation s_def_apps[] =
{
	{"Default_html_checkbox", OperaInstaller::HTML},
	{"Default_html_checkbox", OperaInstaller::HTM},
	{"Default_html_checkbox", OperaInstaller::XHTML},
	{"Default_html_checkbox", OperaInstaller::XHTM},
	{"Default_html_checkbox", OperaInstaller::XHT},
	{"Default_html_checkbox", OperaInstaller::MHT},
	{"Default_html_checkbox", OperaInstaller::MHTML},

	{"Default_xml_checkbox", OperaInstaller::XML},
	{"Default_oex_checkbox", OperaInstaller::OEX},
	{"Default_gif_checkbox", OperaInstaller::GIF},

	{"Default_jpeg_checkbox", OperaInstaller::JPEG},
	{"Default_jpeg_checkbox", OperaInstaller::JPG},

	{"Default_png_checkbox", OperaInstaller::PNG},
	{"Default_bmp_checkbox", OperaInstaller::BMP},
	{"Default_xbm_checkbox", OperaInstaller::XBM},
	{"Default_svg_checkbox", OperaInstaller::SVG},

	{"Default_ogg_checkbox", OperaInstaller::OGA},
	{"Default_ogg_checkbox", OperaInstaller::OGV},
	{"Default_ogg_checkbox", OperaInstaller::OGM},
	{"Default_ogg_checkbox", OperaInstaller::OGG},
	{"Default_ogg_checkbox", OperaInstaller::WEBM},

	{"Default_torrent_checkbox", OperaInstaller::TORRENT},

	{"Default_http_checkbox", OperaInstaller::HTTP},
	{"Default_https_checkbox", OperaInstaller::HTTP},
	{"Default_ftp_checkbox", OperaInstaller::FTP},

	{"Default_news_checkbox", OperaInstaller::NEWS},
	{"Default_news_checkbox", OperaInstaller::SNEWS},
	{"Default_news_checkbox", OperaInstaller::NNTP},

	{"Default_mail_checkbox", OperaInstaller::MAILTO},
};

void DefaultApplicationDialog::OnInit()
{
	OperaInstaller installer;
	OpWidget* widget;

	for (size_t i = 0; i < ARRAY_SIZE(s_def_apps); i++)
	{
		widget = GetWidgetByName(s_def_apps[i].widget_name);
		INT32 new_value = 0;
		if (widget)
		{
			if (i == 0 || op_strcmp(s_def_apps[i].widget_name, s_def_apps[i-1].widget_name) != 0)
				new_value = installer.HasAssociation(s_def_apps[i].association);
			else
				new_value = widget->GetValue() && installer.HasAssociation(s_def_apps[i].association);

			widget->SetValue(new_value);
			widget->SetEnabled(!new_value);
		}

	}
	
	if (IsSystemWin7())
	{
		if ((widget = GetWidgetByName("Set_on_Start_menu")) != 0)
			widget->SetVisibility(FALSE);
		if ((widget = GetWidgetByName("Set_mailer_on_Start_menu")) != 0)
			widget->SetVisibility(FALSE);
		if ((widget = GetWidgetByName("Default_on_Start_menu_label")) != 0)
			widget->SetVisibility(FALSE);
	}
	else
	{
		if ((widget = GetWidgetByName("Set_on_Start_menu")) != 0)
		{
			widget->SetValue(installer.IsDefaultBrowser());
			widget->SetEnabled(!installer.IsDefaultBrowser());
		}
		if ((widget = GetWidgetByName("Set_mailer_on_Start_menu")) != 0)
		{
			widget->SetValue(installer.IsDefaultMailer());
			widget->SetEnabled(!installer.IsDefaultMailer());
		}
	}

}

UINT32 DefaultApplicationDialog::OnOk()
{
	OperaInstaller installer;
	OpWidget* widget;
	BOOL has_assoc = FALSE;

	for (size_t i = 0; i < ARRAY_SIZE(s_def_apps); i++)
	{
		widget = GetWidgetByName(s_def_apps[i].widget_name);

		BOOL enabled = (widget != NULL) && widget->IsEnabled() && widget->GetValue();
		has_assoc |= enabled;

		if (enabled)
			installer.AssociateType(s_def_apps[i].association);
	}

	if (has_assoc)
		installer.RestoreFileHandlers();

	if (IsSystemWin7())
	{
		widget = GetWidgetByName("Default_html_checkbox");
		if (widget != NULL && widget->GetValue())
			installer.BecomeDefaultBrowser();
	}
	else
	{
		widget = GetWidgetByName("Set_on_Start_menu");
		if (widget != NULL && widget->IsEnabled() && widget->GetValue())
			installer.BecomeDefaultBrowser();

		widget = GetWidgetByName("Set_mailer_on_Start_menu");
		if (widget != NULL && widget->IsEnabled() && widget->GetValue())
			installer.BecomeDefaultMailer();
	}

	return 0;
}

void DefaultApplicationDialog::OnCancel()
{
}

BOOL DefaultApplicationDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CHECK_ALL:
		{
			BOOL all_true = TRUE;
			size_t i;
			for (i = 0; i < ARRAY_SIZE(s_def_apps); i++)
			{
				if (!GetWidgetValue(s_def_apps[i].widget_name))
				{
					all_true = FALSE;
					break;
				}
			}

			if ((!GetWidgetValue("Set_on_Start_menu") ||!GetWidgetValue("Set_mailer_on_Start_menu")) && IsSystemWin7() )
					all_true = FALSE;

			if (all_true)
			{
				// undo "Select All"
				OnInit();
				break;
			}

			for (i = 0; i < ARRAY_SIZE(s_def_apps); i++)
				SetWidgetValue(s_def_apps[i].widget_name, TRUE);

			if (!IsSystemWin7())
			{
				SetWidgetValue("Set_on_Start_menu", TRUE);
				SetWidgetValue("Set_mailer_on_Start_menu", TRUE);
			}

			break;
		}
	}
	return Dialog::OnInputAction(action);
}

#endif //MSWIN

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
EditSearchEngineDialog::EditSearchEngineDialog()
	:	m_search_engine_item(NULL),
		m_char_set(),
		m_item_to_change(NULL),
		m_key_edit(NULL),
		m_key_label(NULL)
{
}

OP_STATUS EditSearchEngineDialog::Init(DesktopWindow* parent_window, SearchEngineItem *search_engine_item, const OpString & char_set, SearchTemplate * item_to_change)
{
	m_search_engine_item = search_engine_item;
	m_char_set.Set(char_set);
	m_item_to_change = item_to_change;

	return Dialog::Init(parent_window);
}

void EditSearchEngineDialog::OnInit()
{
	OpString name;
	OpEdit *edit;
	OpCheckBox *check;

	edit = GetWidgetByName<OpEdit>("Search_engine_name", WIDGET_TYPE_EDIT);
	if(edit != NULL)
	{
		m_search_engine_item->GetName(name);

		edit->SetText(name.CStr());
	}
	edit = GetWidgetByName<OpEdit>("Search_engine_url", WIDGET_TYPE_EDIT);
	if (edit != NULL)
	{
		m_search_engine_item->GetURL(name);
		edit->SetText(name.CStr());
		edit->SetForceTextLTR(TRUE);
	}
	m_key_label = GetWidgetByName<OpLabel>("Search_engine_status", WIDGET_TYPE_LABEL);
	m_key_edit = edit = GetWidgetByName<OpEdit>("Search_engine_key", WIDGET_TYPE_EDIT);
	if (edit != NULL)
	{
		m_search_engine_item->GetKey(name);

		edit->SetText(name.CStr());
	}
	edit = GetWidgetByName<OpEdit>("Search_engine_query", WIDGET_TYPE_EDIT);
	if (edit != NULL)
	{
		m_search_engine_item->GetQuery(name);

		edit->SetText(name.CStr());
		edit->SetEnabled(m_search_engine_item->IsPost());
	}
	check = GetWidgetByName<OpCheckBox>("Search_engine_use_post", WIDGET_TYPE_CHECKBOX);
	if (check)
	{
		check->SetValue(m_search_engine_item->IsPost() ? 1 : 0);
	}

	if (m_item_to_change == NULL)
	{
		ShowWidget("Advanced_group", TRUE);
	}
	else
	{
		ShowWidget("Advanced_group", FALSE);
	}

	SetWidgetValue("Default_search_checkbox", m_search_engine_item->IsDefaultEngine());
	SetWidgetValue("Default_speeddial_checkbox", m_search_engine_item->IsSpeeddialEngine());

	Dialog::OnInit();
}

UINT32 EditSearchEngineDialog::OnOk()
{
	OpString name;
	OpEdit *edit;
	OpCheckBox *check;

	edit = (OpEdit *)GetWidgetByName("Search_engine_name");
	if(edit != NULL)
	{
		edit->GetText(name);

		m_search_engine_item->SetName(name);

	}
	edit = (OpEdit *)GetWidgetByName("Search_engine_url");
	if(edit != NULL)
	{
		OpString resolved_name;

		edit->GetText(name);

		// Resolve the URL
		g_url_api->ResolveUrlNameL(name, resolved_name);

		m_search_engine_item->SetURL(resolved_name);
	}
	edit = (OpEdit *)GetWidgetByName("Search_engine_key");
	if(edit != NULL)
	{
		edit->GetText(name);

		m_search_engine_item->SetKey(name);
	}
	edit = (OpEdit *)GetWidgetByName("Search_engine_query");
	if(edit != NULL)
	{
		edit->GetText(name);

		m_search_engine_item->SetQuery(name);
	}
	check = (OpCheckBox *)GetWidgetByName("Search_engine_use_post");
	if(check)
	{
		m_search_engine_item->SetIsPost(check->GetValue() == 1);
	}

	BOOL default_search_changed;
	BOOL speeddial_search_changed;

	BOOL is_default = GetWidgetValue("Default_search_checkbox", 0);
	default_search_changed = (m_search_engine_item->IsDefaultEngine() != is_default);
	if (default_search_changed)
	{
		m_search_engine_item->SetIsDefaultEngine(is_default);
	}

	BOOL is_speeddial = GetWidgetValue("Default_speeddial_checkbox", 0);
	speeddial_search_changed = (m_search_engine_item->IsSpeeddialEngine() != is_speeddial);
	if (speeddial_search_changed)
	{
		m_search_engine_item->SetIsSpeeddialEngine(is_speeddial);
	}

	m_search_engine_item->SetValid(TRUE);

	if (m_item_to_change == NULL)
	{
		return g_searchEngineManager->AddSearch(m_search_engine_item, m_char_set);
	}
	else
	{
		return g_searchEngineManager->EditSearch(m_search_engine_item, m_char_set, m_item_to_change, default_search_changed, speeddial_search_changed);
	}
}

void EditSearchEngineDialog::OnCancel()
{
	m_search_engine_item->SetValid(FALSE);
}

void EditSearchEngineDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->IsNamed("Search_engine_use_post"))
	{
		INT32 state = ((OpCheckBox *)widget)->GetValue();

		OpDropDown *dlbw = (OpDropDown *)GetWidgetByName("Search_engine_query");
		if(dlbw != NULL)
		{
			dlbw->SetEnabled(state == 1);
		}
	}
}

void EditSearchEngineDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_key_edit)
	{
		OpString message;

		if(m_key_label)
		{
			if(IsValidShortcut())
			{
				m_key_label->SetForegroundColor(OP_RGB(0, 0, 0));
				message.Set(UNI_L(""));
			}
			else
			{
				g_languageManager->GetString(Str::D_KEYWORD_IN_USE, message);
				m_key_label->SetForegroundColor(OP_RGB(255, 0, 0));
			}
			m_key_label->SetText(message.CStr());
		}
		g_input_manager->UpdateAllInputStates();
		Relayout(); // seems to be needed to update buttons
	}
}

BOOL EditSearchEngineDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_OK:
					{
						OpString key;
						if (m_key_edit)
							m_key_edit->GetText(key);
						child_action->SetEnabled(IsValidShortcut() && key.HasContent());
					}
					return TRUE;
			}
			break;
		}
	}
	return Dialog::OnInputAction(action);
}


/**
 *
 * IsValidShortcut
 *
 * @return TRUE if the short cut is not already in use by another search engine
 *
 */
BOOL EditSearchEngineDialog::IsValidShortcut()
{
	OpString key;
	if (m_key_edit)
		m_key_edit->GetText(key);
	return !g_searchEngineManager->KeyInUse(key, m_search_engine_item->GetSearchTemplate());
}
#endif

TabClassicOptionsDialog::~TabClassicOptionsDialog()
{

}

OP_STATUS TabClassicOptionsDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
	return OpStatus::OK;
}

void TabClassicOptionsDialog::OnInit()
{
	OpString str;
	OpDropDown* new_pages_dropdown = (OpDropDown*) GetWidgetByName("New_pages_dropdown");

	if (new_pages_dropdown)
	{
		int policy = g_pcui->GetIntegerPref(PrefsCollectionUI::MaximizeNewWindowsWhenAppropriate);

		g_languageManager->GetString(Str::S_REMEMBER_LAST_SIZE, str);
		new_pages_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::S_ALWAYS_MAXIMIZE, str);
		new_pages_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::S_ALWAYS_MAXIMIZE_POPUPS, str);
		new_pages_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::S_ALWAYS_CASCADE, str);
		new_pages_dropdown->AddItem(str.CStr());
		g_languageManager->GetString(Str::S_TILE_ALL_AUTOMATICALLY, str);
		new_pages_dropdown->AddItem(str.CStr());

		new_pages_dropdown->SelectItem(policy, TRUE);
	}
	RETURN_VOID_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Allow_empty_workspace_checkbox"), PrefsCollectionUI::AllowEmptyWorkspace));
	RETURN_VOID_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Click_to_minimize_checkbox"), PrefsCollectionUI::ClickToMinimize));
	RETURN_VOID_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Close_buttons_checkbox"), PrefsCollectionUI::ShowCloseButtons));
	RETURN_VOID_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Separate_windows_checkbox"), PrefsCollectionUI::SDI));
	RETURN_VOID_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Dblclick_checkbox"), PrefsCollectionUI::DoubleclickToClose));

#ifdef MSWIN
	RETURN_VOID_IF_ERROR(WidgetPrefs::GetIntegerPref(GetWidgetByName("Taskbar_thumbnail"), PrefsCollectionUI::UseWindows7TaskbarThumbnails));
#endif

#ifdef _MACINTOSH_
	ShowWidget("label_for_New_pages_dropdown", FALSE);
	ShowWidget("New_pages_dropdown", FALSE);
	ShowWidget("Allow_empty_workspace_checkbox", FALSE);
	ShowWidget("Click_to_minimize_checkbox", FALSE);
#endif // _MACINTOSH_

}

UINT32 TabClassicOptionsDialog::OnOk()
{
	OpDropDown* new_pages_dropdown = (OpDropDown*) GetWidgetByName("New_pages_dropdown");

	if (new_pages_dropdown)
	{
		int policy = new_pages_dropdown->GetSelectedItem();
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::MaximizeNewWindowsWhenAppropriate, policy));
	}
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_empty_workspace_checkbox"), PrefsCollectionUI::AllowEmptyWorkspace));
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Click_to_minimize_checkbox"), PrefsCollectionUI::ClickToMinimize));
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Close_buttons_checkbox"), PrefsCollectionUI::ShowCloseButtons));
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Separate_windows_checkbox"), PrefsCollectionUI::SDI));
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Dblclick_checkbox"), PrefsCollectionUI::DoubleclickToClose));

#ifdef MSWIN
	OpStatus::Ignore(WidgetPrefs::SetIntegerPref(GetWidgetByName("Taskbar_thumbnail"), PrefsCollectionUI::UseWindows7TaskbarThumbnails));
#endif

	return 0;
}

void TabClassicOptionsDialog::OnCancel()
{

}

