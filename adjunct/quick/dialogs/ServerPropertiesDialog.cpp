/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/ServerPropertiesDialog.h"

#include "adjunct/desktop_util/prefs/WidgetPrefs.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/dialogs/CookieEditDialog.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "modules/cookies/url_cm.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/url/uamanager/ua.h"
#include "modules/url/url_man.h"
#include "modules/util/OpLineParser.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/application/BrowserApplicationCacheListener.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpNumberEdit.h"
#include "modules/widgets/OpListBox.h"


////////////////////////////////////////////////////////////////////////////

ServerPropertiesDialog::ServerPropertiesDialog(OpStringC& real_server_name, BOOL new_server, BOOL readonly, const uni_char *start_tab)
: m_loading_index(-1)
, m_new_server(new_server)
, m_default_settings(FALSE)
, m_cookies_model(1)
, m_readonly(readonly)
, m_proxy_setting_changed(FALSE)
{
	m_real_server_name.Set(real_server_name); 
	m_start_tab.Set(start_tab); 
}

ServerPropertiesDialog::~ServerPropertiesDialog()
{
	OpDropDown* encoding_dropdown_lang = (OpDropDown*) GetWidgetByName("Encoding_dropdown_lang");
	OpDropDown* encoding_dropdown_details	= (OpDropDown*) GetWidgetByName("Encoding_dropdown_details");

	ResetEncodingDropDown(encoding_dropdown_lang);
	ResetEncodingDropDown(encoding_dropdown_details);
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/
static const uni_char *tab_sections[] =
{
	UNI_L("general"),
	UNI_L("cookies"),
	UNI_L("content"),
	UNI_L("display"),
	UNI_L("script"),
	UNI_L("network"),
	NULL
};

void ServerPropertiesDialog::OnInit()
{
	if (!m_new_server && m_real_server_name.CompareI("*.*") == 0) // not returning TRUE at all
	{
		m_default_settings = TRUE;
		m_real_server_name.Set("");
	}

	InitGeneral();
	InitCookies();
	InitContent();
	InitStyle();
	InitJavaScript();
	InitNetwork();
	InitWebHandler();

	UpdateJavaScriptWidgets();

	// make all editable widgets readonly in privacy mode
	if(m_readonly)
	{
		SetWidgetEnabled("General_group",FALSE);
		SetWidgetEnabled("Cookie_group",FALSE);
		SetWidgetEnabled("Content_group",FALSE);
		SetWidgetEnabled("Style_group",FALSE);
		SetWidgetEnabled("Script_group",FALSE);
		SetWidgetEnabled("Network_group",FALSE);
		SetWidgetEnabled("Handler_group",FALSE);
	}

	// if a start is defined, make that tab the active one
	if(m_start_tab.HasContent())
	{
		int i;

		for(i = 0; tab_sections[i] != NULL; i++)
		{
			if(m_start_tab.CompareI(tab_sections[i]) == 0)
			{
				SetCurrentPage(i);
			}
		}
	}
}

void ServerPropertiesDialog::UpdateJavaScriptWidgets()
{
	BOOL enable = GetWidgetValue("Javascript_checkbox");

	SetWidgetEnabled("Allow_move_checkbox", enable);
	SetWidgetEnabled("Allow_status_checkbox", enable);
	SetWidgetEnabled("Allow_clicks_checkbox", enable);
	SetWidgetEnabled("Allow_hide_address_checkbox", enable);
	SetWidgetEnabled("Javascript_console_checkbox", enable);
	SetWidgetEnabled("My_user_javascript_chooser", enable);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL ServerPropertiesDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					child_action->SetEnabled(!m_readonly);
					return TRUE;
				}
			case OpInputAction::ACTION_DELETE:
				{
					child_action->SetEnabled(!m_readonly);
					return TRUE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			Cookie* cookie = GetSelectedCookie(FALSE);
			if (cookie)
			{
				CookieEditDialog* dialog = OP_NEW(CookieEditDialog, (m_real_server_name.CStr(), cookie, m_readonly));
				if (dialog)
					dialog->Init(this);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_DELETE:
		{
			switch(GetCurrentPage())
			{
				case CookiesPage:
				{
					Cookie* cookie = GetSelectedCookie(TRUE);

					if (cookie)
					{
						cookie->Out();
						OP_DELETE(cookie);
					}
				}
				break;

				case WebHandlerPage:
				{
					OpListBox* listbox = static_cast<OpListBox*>(GetWidgetByName("Handler_listbox"));
					if (listbox)
					{
						INT32 index = listbox->GetSelectedItem();
						if (index >= 0 && index < listbox->CountItems())
						{
							HandlerItem* item = reinterpret_cast<HandlerItem*>(listbox->GetItemUserData(index));
							if (item)
							{
								item->m_deleted = TRUE;
								listbox->RemoveItem(index);
								if (index >= listbox->CountItems())
									index = listbox->CountItems()-1;
								if (index >= 0)
									listbox->SelectItem(index, TRUE);
								listbox->InvalidateAll();
							}
						}
					}
				}
				break;
			}
			return TRUE;
		}

	}
	return Dialog::OnInputAction(action);
}


/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void ServerPropertiesDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	Dialog::OnChange(widget, changed_by_mouse);

	if(widget->IsNamed("Enable_frames_checkbox") || widget->IsNamed("Enable_inline_frames_checkbox"))
	{
		SetWidgetEnabled("Show_frame_border_checkbox", GetWidgetValue("Enable_frames_checkbox") || GetWidgetValue("Enable_inline_frames_checkbox"));
	}
	else if (widget->IsNamed("No_cookies_radio"))
	{
		SetWidgetEnabled("Delete_cookies_on_exit_checkbox", widget->GetValue() ? FALSE : TRUE);

		// only ever enable if the global option "show cookies" is enabled
		SetWidgetEnabled("Display_cookies_checkbox", !widget->GetValue() && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisplayReceivedCookies) ? TRUE : FALSE);
	}
	else if (widget->IsNamed("Plugins_checkbox"))
	{
		SetWidgetEnabled("On_demand_plugins_checkbox", widget->GetValue());
	}
	else if (widget->IsNamed("Encoding_dropdown_lang"))
	{
		// Fill out the 2nd drop down based on the selection in the first
		OpDropDown* encoding_dropdown_lang		= (OpDropDown*) GetWidgetByName("Encoding_dropdown_lang");
		OpDropDown* encoding_dropdown_details	= (OpDropDown*) GetWidgetByName("Encoding_dropdown_details");

		OpString8 *menu_to_load = (OpString8 *)encoding_dropdown_lang->GetItemUserData(encoding_dropdown_lang->GetSelectedItem());

		if (encoding_dropdown_details)
		{
			// Reset the drop down :P
			ResetEncodingDropDown(encoding_dropdown_details);

			if (menu_to_load->IsEmpty())
			{
				encoding_dropdown_details->SetEnabled(FALSE);

				OpString automatic_encoding;
				if(OpStatus::IsSuccess(g_languageManager->GetString(Str::MI_IDM_ENCODING_AUTOMATIC, automatic_encoding)))
				{
					encoding_dropdown_details->AddItem(automatic_encoding.CStr(), 0, NULL);
					encoding_dropdown_details->SetValue(0);
				}
			}
			else
			{
				encoding_dropdown_details->SetEnabled(TRUE);

				// Fill it with the same as the View/Encoding menu
				FillEncodingDropDown(encoding_dropdown_details, menu_to_load->CStr(), FALSE, NULL);

				// If loading then set the correct item
				if (m_loading_index > -1)
				{
					encoding_dropdown_details->SelectItem(m_loading_index, TRUE);
					m_loading_index = -1;
				}

				// Make sure the new items are displayed!
				encoding_dropdown_details->InvalidateAll();
			}
		}
	}
	else if (widget->IsNamed("Proxy_dropdown"))
	{
		m_proxy_setting_changed = TRUE;
	}
}


/***********************************************************************************
**
**	OnClick
**
***********************************************************************************/

void ServerPropertiesDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->IsNamed("Javascript_checkbox"))
	{
		UpdateJavaScriptWidgets();
	}
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 ServerPropertiesDialog::OnOk()
{
	if (!m_default_settings)
	{
		GetWidgetText("Server_name_edit", m_real_server_name);

		if(m_real_server_name.IsEmpty())
		{
			return 0; // not much to do without a server name
		}
	}

	// don't save anything in privacy mode
	if(m_readonly)
		return 0;

	SaveGeneral();
	SaveCookies();
	SaveContent();
	SaveStyle();
	SaveJavaScript();
	SaveNetwork();
	SaveWebHandler();

	// add to the list if it's not already there
	if (!m_default_settings && g_server_manager)
	{
		g_server_manager->OnServerAdded(m_real_server_name.CStr());
	}

	TRAPD(rc, g_prefsManager->CommitL());

	return 0;
}

/***********************************************************************************
**
**	Util functions
**
***********************************************************************************/

Cookie* ServerPropertiesDialog::GetSelectedCookie(BOOL remove_from_treemodel)
{
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Cookies_treeview");
	if (treeview)
	{

		int pos = treeview->GetSelectedItemPos();

		if (pos != -1)
		{
			// build up the cookie list

			Cookie** cookie_array = NULL;
			int size_returned = 0;
			uni_char* path = NULL;

			OpString cookie_domain;
			GetCookieDomain(m_real_server_name, cookie_domain);
			uni_char* domain = cookie_domain.CStr();

			OpStatus::Ignore(g_url_api->BuildCookieList(cookie_array, &size_returned, domain, path));
			if (size_returned > 0)
			{
				cookie_array = OP_NEWA(Cookie*, size_returned);
				if (!cookie_array)
					return NULL;
				OpStatus::Ignore(g_url_api->BuildCookieList(cookie_array, &size_returned, domain, path));
			}

			if (pos < size_returned)
			{
				if (remove_from_treemodel)
				{
					m_cookies_model.Delete(pos);
				}
				return cookie_array[pos];
			}
		}
	}
	return NULL;
}


/***********************************************************************************
**
**	Init functions
**
***********************************************************************************/

OP_STATUS ServerPropertiesDialog::InitGeneral()
{
	OpEdit* server_edit = GetWidgetByName<OpEdit>("Server_name_edit", WIDGET_TYPE_EDIT);
	if (server_edit)
	{
		server_edit->SetForceTextLTR(TRUE);

		if (!m_default_settings)
		{
			server_edit->SetText(m_real_server_name.CStr());
		}
		else
		{
			// Should normally not be visible from UI
			server_edit->SetText(UNI_L("Default settings"));
			server_edit->SetEnabled(FALSE);
		}
	}

	OpDropDown* popups_dropdown = (OpDropDown*) GetWidgetByName("Popups_dropdown");

	if (popups_dropdown)
	{
		OpString popupstrategy;
		int got_index;

		static const Str::LocaleString popup_stringid[4] =
		{
			(Str::LocaleString)Str::SI_IDSTR_POPUP_ALWAYS,
			(Str::LocaleString)Str::SI_IDSTR_POPUP_BACKGROUND,
			(Str::LocaleString)Str::SI_IDSTR_POPUP_REQUESTED,
			(Str::LocaleString)Str::SI_IDSTR_POPUP_NEVER
		};

		for (OP_MEMORY_VAR int i = 0; i < 4; i++)
		{
			g_languageManager->GetString(popup_stringid[i], popupstrategy);
			if (popupstrategy.HasContent())
				popups_dropdown->AddItem(popupstrategy.CStr(), -1, &got_index);
		}

		int selected_pos = 0;
		if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget, m_real_server_name.CStr()))
		{
			selected_pos = 3;
		}
		else if (g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination, m_real_server_name.CStr()) == POPUP_WIN_BACKGROUND)
		{
			selected_pos = 1;
		}
		if (g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups, m_real_server_name.CStr()))
		{
			selected_pos = 2;
		}

		popups_dropdown->SelectItem(selected_pos, TRUE);
	}

	OpDropDown* encoding_dropdown_lang = (OpDropDown*) GetWidgetByName("Encoding_dropdown_lang");

	if (encoding_dropdown_lang)
	{
		FillEncodingDropDown(encoding_dropdown_lang, "Encoding Menu", TRUE, NULL);

		const OpStringC force_encoding(g_pcdisplay->GetStringPref(PrefsCollectionDisplay::ForceEncoding, m_real_server_name.CStr()));
		if (force_encoding.IsEmpty())
		{
			// This means automatic which is always first
			encoding_dropdown_lang->SetValue(0);
			// Force OnChange
			if(encoding_dropdown_lang->GetListener())
			{
				encoding_dropdown_lang->GetListener()->OnChange(encoding_dropdown_lang);
			}
		}
		else {
			// Select the correct encoding!
			OpString8 force_enc;
			force_enc.Set(force_encoding.CStr());
			int num_items = encoding_dropdown_lang->CountItems();
			for (int i = 0; i < num_items; i++)
			{
				OpString8 *menu_to_load = (OpString8 *)encoding_dropdown_lang->GetItemUserData(i);
				if (menu_to_load && !menu_to_load->IsEmpty())
				{
					// Try and find the correct charset
					m_loading_index = FillEncodingDropDown(NULL, menu_to_load->CStr(), FALSE, force_enc.CStr());

					if (m_loading_index > -1)
					{
						// Found the selection!
						encoding_dropdown_lang->SelectItemAndInvoke(i, TRUE, FALSE);
						break;
					}
				}
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::InitCookies()
{
	// need to initialize cookies
	TRAPD(op_err, g_url_api->CheckCookiesReadL());
	OpStatus::Ignore(op_err);

	BOOL accept_cookies=FALSE, accept_third_party=FALSE, use_default=TRUE, delete_on_exit=FALSE, ask_cookie=FALSE;

	/*ServerName* server_name = g_url_api->GetServerName(m_real_server_name.CStr(), FALSE);
	if (server_name)
	{
		accept_cookies = (server_name->GetAcceptCookies() == COOKIE_ALL);

		accept_third_party = (server_name->GetAcceptThirdPartyCookies() == COOKIE_ACCEPT_THIRD_PARTY);

		use_default = (server_name->GetAcceptCookies() == COOKIE_DEFAULT);

		delete_on_exit = (server_name->GetDeleteCookieOnExit() == COOKIE_DELETE_ON_EXIT);
	}*/

	COOKIE_MODES cookie_mode = COOKIE_MODES(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled, m_real_server_name.CStr()));
	
	accept_cookies = (cookie_mode == COOKIE_ALL || cookie_mode == COOKIE_NO_THIRD_PARTY || cookie_mode == COOKIE_SEND_NOT_ACCEPT_3P);
	
	accept_third_party = (cookie_mode == COOKIE_ALL);

	delete_on_exit = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AcceptCookiesSessionOnly, m_real_server_name.CStr());

/*	if (use_default)
	{
		COOKIE_MODES cookie_mode = COOKIE_MODES(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled));

		accept_cookies = (cookie_mode == COOKIE_ALL || cookie_mode == COOKIE_NO_THIRD_PARTY || cookie_mode == COOKIE_SEND_NOT_ACCEPT_3P);
		accept_third_party = (cookie_mode == COOKIE_ALL);

		delete_on_exit = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AcceptCookiesSessionOnly);
	}*/

	// show "ask cookie" checkbox when global option is checked
	ask_cookie = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisplayReceivedCookies);
	SetWidgetEnabled("Display_cookies_checkbox", ask_cookie);
	SetWidgetValue("Display_cookies_checkbox", use_default);

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

	if (delete_on_exit)
	{
		SetWidgetValue("Delete_cookies_on_exit_checkbox", TRUE);
	}
	//WidgetPrefs::GetIntegerPref(GetWidgetByName("Delete_cookies_on_exit_checkbox"), PrefsCollectionNetwork::AcceptCookiesSessionOnly, m_real_server_name.CStr());

	if (m_default_settings) // never actually called
	{
		ShowWidget("Cookies_treeview", FALSE);
		ShowWidget("Display_cookies_checkbox", TRUE);

		WidgetPrefs::GetIntegerPref(GetWidgetByName("Display_cookies_checkbox"), PrefsCollectionNetwork::DisplayReceivedCookies, m_real_server_name.CStr());

		return OpStatus::OK;
	}

	ShowWidget("Cookies_treeview", TRUE);

	// build up the cookie treeview

	Cookie** OP_MEMORY_VAR cookie_array = NULL;
	int size_returned = 0;
	uni_char* path = NULL;
	
	OpString cookie_domain;
	GetCookieDomain(m_real_server_name, cookie_domain);
	uni_char* domain = cookie_domain.CStr();

	if (domain) // don't fill for empty domains (happens when creating new site prefs)
	{
		RETURN_IF_ERROR(g_url_api->BuildCookieList(cookie_array, &size_returned, domain, path));
		if (size_returned > 0)
		{
			cookie_array = OP_NEWA(Cookie*, size_returned);
			if (!cookie_array)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(g_url_api->BuildCookieList(cookie_array, &size_returned, domain, path));
		}

		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Cookies_treeview");

		if (treeview)
		{
			treeview->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_EDIT_PROPERTIES)));

			OpString cookies_text;
			g_languageManager->GetString(Str::D_SERVER_MANAGER_COOKIES, cookies_text);
			m_cookies_model.SetColumnData(0, cookies_text.CStr());

			for (int i = 0; i < size_returned; i++)
			{
				OpString content;

				content.Set(cookie_array[i]->Name());
				content.Append(" ");
				content.Append(cookie_array[i]->Value());
				m_cookies_model.AddItem(content.CStr());
			}

			treeview->SetTreeModel(&m_cookies_model);
			treeview->SetSelectedItem(0);
		}

		OP_DELETEA(cookie_array);
	}
	SetWidgetEnabled("Delete_cookies_on_exit_checkbox", GetWidgetValue("No_cookies_radio") ? FALSE : TRUE);

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::InitContent()
{
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Gif_animation_checkbox"), PrefsCollectionDoc::ShowAnimation, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Plugins_checkbox"), PrefsCollectionDisplay::PluginsEnabled, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("On_demand_plugins_checkbox"), PrefsCollectionDisplay::EnableOnDemandPlugin, m_real_server_name.CStr());
	SetWidgetEnabled("On_demand_plugins_checkbox", (BOOL)GetWidgetValue("Plugins_checkbox"));
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Contentblocker_checkbox"), PrefsCollectionNetwork::EnableContentBlocker, m_real_server_name.CStr());

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Default_zoom_dropdown");
	if (dropdown)
	{
		int scale = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale, m_real_server_name.CStr());

		static const int scale_values[] =
		{
			20, 30, 50, 70, 80, 90, 100, 110, 120, 150, 180, 200, 250, 300, 400, 500, 600, 700, 800, 900, 1000
		};

		OpString buffer;
		buffer.Reserve(30);

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

	dropdown = (OpDropDown*)GetWidgetByName("Page_image_dropdown");
	if( dropdown )
	{
		OpString loc_str;
		g_languageManager->GetString(Str::M_IMAGE_MENU_SHOW, loc_str);
		dropdown->AddItem(loc_str.CStr());
		g_languageManager->GetString(Str::M_IMAGE_MENU_CACHED, loc_str);
		dropdown->AddItem(loc_str.CStr());
		g_languageManager->GetString(Str::M_IMAGE_MENU_NO, loc_str);
		dropdown->AddItem(loc_str.CStr());

		// dropdown->SelectItem(3 - g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowImageState), TRUE);
		switch (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowImageState, m_real_server_name.CStr()))
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

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::InitStyle()
{
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_frames_checkbox"), PrefsCollectionDisplay::FramesEnabled, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_inline_frames_checkbox"), PrefsCollectionDisplay::IFramesEnabled, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Show_frame_border_checkbox"), PrefsCollectionDisplay::ShowActiveFrame, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Forms_styling_checkbox"), PrefsCollectionDisplay::EnableStylingOnForms, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Scrollbars_styling_checkbox"), PrefsCollectionDisplay::EnableScrollbarColors, m_real_server_name.CStr());

	DesktopFileChooserEdit* chooser = (DesktopFileChooserEdit*) GetWidgetByName("My_style_sheet_chooser");
	if (chooser)
	{
		OpFile css_file;
		TRAPD(rc1, g_pcfiles->GetFileL(PrefsCollectionFiles::LocalCSSFile, css_file, m_real_server_name.CStr()));
		if (OpStatus::IsSuccess(rc1))
		{
			chooser->SetText(css_file.GetFullPath());
		}

		OpString loc_str;
		g_languageManager->GetString(Str:: D_SELECT_STYLE_SHEET_FILE, loc_str);

		chooser->SetTitle(loc_str.CStr());

		g_languageManager->GetString(Str::SI_CSS_TYPES, loc_str);

		chooser->SetFilterString(loc_str.CStr());
	}

	SetWidgetEnabled("Show_frame_border_checkbox", GetWidgetValue("Enable_frames_checkbox") || GetWidgetValue("Enable_inline_frames_checkbox"));

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::InitJavaScript()
{
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Javascript_checkbox"), PrefsCollectionJS::EcmaScriptEnabled, m_real_server_name.CStr());

	// check only move, set move, resize, lower and raise on OK
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Allow_move_checkbox"), PrefsCollectionDisplay::AllowScriptToMoveWindow, m_real_server_name.CStr());

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Allow_status_checkbox"), PrefsCollectionDisplay::AllowScriptToChangeStatus, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Allow_clicks_checkbox"), PrefsCollectionDisplay::AllowScriptToReceiveRightClicks, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Allow_hide_address_checkbox"), PrefsCollectionDisplay::AllowScriptToHideURL, m_real_server_name.CStr());
#ifdef OPERA_CONSOLE
	OpCheckBox* checkbox = (OpCheckBox*)GetWidgetByName("Javascript_console_checkbox");
	if (NULL != checkbox && NULL != g_message_console_manager)
	{
		checkbox->SetValue(
				g_message_console_manager->GetJavaScriptConsoleSettingSite(
							m_real_server_name));
	}
#endif // OPERA_CONSOLE

	WidgetPrefs::GetIntegerPref(GetWidgetByName("Clipboardaccess_checkbox"), PrefsCollectionJS::LetSiteAccessClipboard, m_real_server_name.CStr());

	DesktopFileChooserEdit* chooser = (DesktopFileChooserEdit*) GetWidgetByName("My_user_javascript_chooser");
	if (chooser)
	{
		chooser->SetText(g_pcjs->GetStringPref(PrefsCollectionJS::UserJSFiles, m_real_server_name.CStr()).CStr() );
	}

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::InitNetwork()
{
	WidgetPrefs::GetIntegerPref(GetWidgetByName("International_address_checkbox"), PrefsCollectionNetwork::UseUTF8Urls, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_referrer_logging_checkbox"), PrefsCollectionNetwork::ReferrerEnabled, m_real_server_name.CStr());

	// both these control redirect
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientRefresh, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientPull, m_real_server_name.CStr());
	WidgetPrefs::GetIntegerPref(GetWidgetByName("Do_no_track_checkbox"), PrefsCollectionNetwork::DoNotTrack, m_real_server_name.CStr());

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Browser_id_dropdown");

	if (dropdown)
	{
		OP_MEMORY_VAR int selected_pos = 0;
		OpString messagestring;
		OpString item_title;

		item_title.Reserve(70);

		int pos;
		OP_MEMORY_VAR int j;
		UA_BaseStringId global_id = g_uaManager->GetUABaseId();
		OP_MEMORY_VAR UA_BaseStringId current_id = global_id;

		ServerName *servername = g_url_api->GetServerName(m_real_server_name.CStr(), TRUE);
		if (servername)
		{
			current_id = g_uaManager->OverrideUserAgent(servername);
		}
		// servername can be NULL if new site preferences are added manually

		if (current_id == UA_NotOverridden)
		{
			current_id = global_id;
		}

		for(j=0;j<5;j++)
		{
			Str::LocaleString id = Str::NOT_A_STRING;
			UA_BaseStringId uaid = UA_PolicyUnknown;

			switch(j)
			{
			case 0:
				uaid = UA_Opera;
				id = Str::SI_IDSTR_USE_OPERA_UA;
				break;
			case 1:
				uaid = UA_Mozilla;
				id = Str::M_IDAS_MENU_MOZILLA; // SI_IDSTR_MOZILLA_SPOOF1
				break;
			case 2:
				uaid = UA_MSIE;
				id = Str::M_IDAS_MENU_INTERNET_EXPLORER; // SI_IDSTR_MSIE_SPOOF
				break;
			case 3:
				uaid = UA_MozillaOnly;
				id = Str::S_MASK_AS_MOZILLA;
				break;
			case 4:
				uaid = UA_MSIE_Only;
				id = Str::S_MASK_AS_IE;
				break;
			}

			g_languageManager->GetString(id, messagestring);

			OpString8 spoofversion;

			if (OpStatus::IsSuccess(g_uaManager->PickSpoofVersionString(uaid, 0, spoofversion)))
			{
				const uni_char *spoof = make_doublebyte_in_tempbuffer(spoofversion.CStr(), spoofversion.Length());
				uni_snprintf(item_title.CStr(), 70, messagestring.CStr(), spoof);
				dropdown->AddItem(item_title.CStr(), -1, &pos, uaid);

				if(uaid == current_id)
					selected_pos = pos;
			}
		}

		if (m_default_settings)
		{
			dropdown->SelectItem(0, TRUE);
			dropdown->SetEnabled(FALSE);
		}
		else
		{
			dropdown->SelectItem(selected_pos, TRUE);
		}
	}

	dropdown = (OpDropDown*) GetWidgetByName("Proxy_dropdown");
	if (dropdown)
	{
		OpString str;
		g_languageManager->GetString(Str::DI_IDYES, str);
		dropdown->AddItem(str.CStr(), -1, NULL, YES);

		g_languageManager->GetString(Str::DI_IDNO, str);
		dropdown->AddItem(str.CStr(), -1, NULL, NO);

		int val = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableProxy, m_real_server_name.CStr());
		if (val)
			dropdown->SelectItem(0, TRUE);
		else
			dropdown->SelectItem(1, TRUE);
	}

	dropdown = (OpDropDown*) GetWidgetByName("Application_cache_dropdown");
	if (dropdown)
	{
		OpString str;
		g_languageManager->GetString(Str::D_GEOLOCATION_YES, str);
		dropdown->AddItem(str.CStr(), -1, NULL, Accept);

		g_languageManager->GetString(Str::D_GEOLOCATION_NO, str);
		dropdown->AddItem(str.CStr(), -1, NULL, Reject);

		g_languageManager->GetString(Str::D_GEOLOCATION_ASK, str);
		dropdown->AddItem(str.CStr(), -1, NULL, Ask);

		int val = g_pcui->GetIntegerPref(PrefsCollectionUI::StrategyOnApplicationCache, m_real_server_name.CStr());
		if (val == Accept)
			dropdown->SelectItem(0, TRUE);
		else if (val == Reject)
			dropdown->SelectItem(1, TRUE);
		else
			dropdown->SelectItem(2, TRUE);
	}

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::InitWebHandler()
{
	OpStringC handler_csv_list = g_pcjs->GetStringPref(PrefsCollectionJS::DisallowedWebHandlers, m_real_server_name.CStr());
	if (handler_csv_list.HasContent())
	{
		OpAutoVector<OpString> list;
		RETURN_IF_ERROR(StringUtils::SplitString( list, handler_csv_list, ','));
		for (UINT32 i=0; i<list.GetCount(); i++)
		{
			OpAutoPtr<HandlerItem> item(OP_NEW(HandlerItem,()));
			RETURN_OOM_IF_NULL(item.get());
			RETURN_IF_ERROR(item->m_name.Set(*list.Get(i)));
			RETURN_IF_ERROR(m_handler_list.Add(item.get()));
			item.release();
		}
	}

	OpListBox* listbox = static_cast<OpListBox*>(GetWidgetByName("Handler_listbox"));
	if (listbox)
	{
		for (UINT32 i=0; i<m_handler_list.GetCount(); i++)
			RETURN_IF_ERROR(listbox->AddItem(m_handler_list.Get(i)->m_name, -1, 0, reinterpret_cast<INTPTR>(m_handler_list.Get(i))));
	}

	return OpStatus::OK;
}



/***********************************************************************************
**
**	Save functions
**
***********************************************************************************/

OP_STATUS ServerPropertiesDialog::SaveGeneral()
{
	OpDropDown* popups_dropdown = (OpDropDown*) GetWidgetByName("Popups_dropdown");

	if (popups_dropdown)
	{
		int selected_pos = popups_dropdown->GetSelectedItem();

		int ignore_target = FALSE;
		int target_destination = POPUP_WIN_NEW;
		int ignore_unrequested_popups = FALSE;

		if (selected_pos == 3)
		{
			ignore_target = TRUE;
			target_destination = POPUP_WIN_IGNORE;
			ignore_unrequested_popups = FALSE;
		}
		else if (selected_pos == 2)
		{
			ignore_target = FALSE;
			target_destination = POPUP_WIN_NEW;
			ignore_unrequested_popups = TRUE;
		}
		else if (selected_pos == 1)
		{
			ignore_target = FALSE;
			target_destination = POPUP_WIN_BACKGROUND;
			ignore_unrequested_popups = FALSE;
		}
		else
		{
			ignore_target = FALSE;
			target_destination = POPUP_WIN_NEW;
			ignore_unrequested_popups = FALSE;
		}

		if (m_default_settings)
		{
			g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, ignore_target);
			g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, target_destination);
			g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, ignore_unrequested_popups);
		}
		else
		{
			g_pcdoc->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionDoc::IgnoreTarget, ignore_target, TRUE);
			g_pcjs->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionJS::TargetDestination, target_destination, TRUE);
			g_pcjs->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionJS::IgnoreUnrequestedPopups, ignore_unrequested_popups, TRUE);
		}
	}

	OpDropDown* encoding_dropdown_details	= (OpDropDown*) GetWidgetByName("Encoding_dropdown_details");

	if (encoding_dropdown_details)
	{
		OpString8 *charset8 = (OpString8 *)encoding_dropdown_details->GetItemUserData(encoding_dropdown_details->GetSelectedItem());

		// The m_default_settings = TRUE is no longer in use so only this case is needed
		if (!m_default_settings)
		{
			OpString charset;

			// Set the charset if it's set. Automatic will have a null here and set a blank string
			if (charset8)
				charset.Set(*charset8);
				
			g_pcdisplay->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionDisplay::ForceEncoding, charset, TRUE);
		}
	}

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::SaveCookies()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Delete_cookies_on_exit_checkbox"), PrefsCollectionNetwork::AcceptCookiesSessionOnly, m_real_server_name.CStr());
	BOOL delete_on_exit = GetWidgetValue("Delete_cookies_on_exit_checkbox");

	if (m_default_settings) // not called
	{
		BOOL def_accept_cookies = GetWidgetValue("Accept_cookies_radio") || GetWidgetValue("No_third_party_radio");
		BOOL def_accept_third_party = GetWidgetValue("Accept_cookies_radio");

		OP_MEMORY_VAR COOKIE_MODES cookie_mode
			= def_accept_cookies && def_accept_third_party ? COOKIE_ALL :
			def_accept_cookies ? COOKIE_SEND_NOT_ACCEPT_3P : COOKIE_NONE;

		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CookiesEnabled, cookie_mode));

		WidgetPrefs::SetIntegerPref(GetWidgetByName("Display_cookies_checkbox"), PrefsCollectionNetwork::DisplayReceivedCookies, m_real_server_name.CStr());

		return OpStatus::OK;
	}

	if(m_real_server_name.IsEmpty() )
	{
		return 0;
	}

	BOOL accept_cookies = GetWidgetValue("Accept_cookies_radio") || GetWidgetValue("No_third_party_radio");
	BOOL accept_third_party = GetWidgetValue("Accept_cookies_radio");
	BOOL accept_illegal_paths = TRUE; // Changed in this code, to match site specific prefs

	ServerName *servername = g_url_api->GetServerName(m_real_server_name.CStr(), TRUE);

	if (servername)
	{
		// find out if the cookie is equal to default
		COOKIE_MODES cookie_mode;
		
		if (accept_third_party)
			cookie_mode = COOKIE_ALL;
		else if (accept_cookies)
			cookie_mode = COOKIE_SEND_NOT_ACCEPT_3P;
		else
			cookie_mode = COOKIE_NONE;

		COOKIE_MODES current_cookie_mode = COOKIE_MODES(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled, m_real_server_name.CStr()));
		
		if (current_cookie_mode != cookie_mode)
			RETURN_IF_LEAVE(g_pcnet->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionNetwork::CookiesEnabled, (int) cookie_mode, TRUE));

		g_url_api->CreateCookieDomain(servername);
		servername->SetAcceptThirdPartyCookies(accept_third_party ? COOKIE_ACCEPT_THIRD_PARTY : COOKIE_NO_THIRD_PARTY);
		servername->SetAcceptCookies(cookie_mode);
		servername->SetAcceptIllegalPaths(accept_illegal_paths ? COOKIE_ILLPATH_ACCEPT : COOKIE_ILLPATH_REFUSE);
		servername->SetDeleteCookieOnExit(delete_on_exit ? COOKIE_DELETE_ON_EXIT : COOKIE_NO_DELETE_ON_EXIT);

		// safe to call, since ServerManager checks for duplicates
		if (g_server_manager)
			g_server_manager->OnCookieFilterAdded(servername);

	}
	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::SaveContent()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Gif_animation_checkbox"), PrefsCollectionDoc::ShowAnimation, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Plugins_checkbox"), PrefsCollectionDisplay::PluginsEnabled, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("On_demand_plugins_checkbox"), PrefsCollectionDisplay::EnableOnDemandPlugin, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Contentblocker_checkbox"), PrefsCollectionNetwork::EnableContentBlocker, m_real_server_name.CStr());

	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Default_zoom_dropdown");
	int scale = 100;

	if (dropdown)
	{
		static const int scale_values[] =
		{
			20, 30, 50, 70, 80, 90, 100, 110, 120, 150, 180, 200, 250, 300, 400, 500, 600, 700, 800, 900, 1000
		};
		scale = scale_values[dropdown->GetSelectedItem()];

		if (m_default_settings)
		{
			TRAPD(err, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::Scale, scale));
		}
		else
		{
			g_pcdisplay->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionDisplay::Scale, scale, TRUE);
		}
	}

	dropdown = (OpDropDown*) GetWidgetByName("Page_image_dropdown");
	if (dropdown)
	{
		int selected_pos = dropdown->GetSelectedItem();
		int image_state = 3 - selected_pos;

		if (m_default_settings)
		{
			TRAPD(err, g_pcdoc->WriteIntegerL(PrefsCollectionDoc::ShowImageState, image_state));
		}
		else
		{
			g_pcdoc->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionDoc::ShowImageState, image_state, TRUE);
		}
	}

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::SaveStyle()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_frames_checkbox"), PrefsCollectionDisplay::FramesEnabled, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_inline_frames_checkbox"), PrefsCollectionDisplay::IFramesEnabled, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Show_frame_border_checkbox"), PrefsCollectionDisplay::ShowActiveFrame, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Forms_styling_checkbox"), PrefsCollectionDisplay::EnableStylingOnForms, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Scrollbars_styling_checkbox"), PrefsCollectionDisplay::EnableScrollbarColors, m_real_server_name.CStr());

	DesktopFileChooserEdit* chooser = (DesktopFileChooserEdit*)GetWidgetByName("My_style_sheet_chooser");

	if (chooser)
	{
		OpString filename;
		GetWidgetText("My_style_sheet_chooser", filename);

		OpFile file;
		OpStatus::Ignore(file.Construct(filename.CStr()));
		if (m_default_settings)
		{
			TRAPD(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::LocalCSSFile, &file));
		}
		else
		{
			TRAPD(err, g_pcfiles->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionFiles::LocalCSSFile, &file, TRUE));
		}
	}

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::SaveJavaScript()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Javascript_checkbox"), PrefsCollectionJS::EcmaScriptEnabled, m_real_server_name.CStr());

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_move_checkbox"), PrefsCollectionDisplay::AllowScriptToResizeWindow, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_move_checkbox"), PrefsCollectionDisplay::AllowScriptToMoveWindow, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_move_checkbox"), PrefsCollectionDisplay::AllowScriptToRaiseWindow, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_move_checkbox"), PrefsCollectionDisplay::AllowScriptToLowerWindow, m_real_server_name.CStr());

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_status_checkbox"), PrefsCollectionDisplay::AllowScriptToChangeStatus, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_clicks_checkbox"), PrefsCollectionDisplay::AllowScriptToReceiveRightClicks, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Allow_hide_address_checkbox"), PrefsCollectionDisplay::AllowScriptToHideURL, m_real_server_name.CStr());
#ifdef OPERA_CONSOLE
	OpCheckBox* checkbox = (OpCheckBox*)GetWidgetByName("Javascript_console_checkbox");
	if (NULL != checkbox && NULL != g_message_console_manager)
	{
		g_message_console_manager->SetJavaScriptConsoleLoggingSite(
				m_real_server_name, (BOOL)checkbox->GetValue());
	}
#endif // OPERA_CONSOLE

	WidgetPrefs::SetIntegerPref(GetWidgetByName("Clipboardaccess_checkbox"), PrefsCollectionJS::LetSiteAccessClipboard, m_real_server_name.CStr());

	WidgetPrefs::SetStringPref(GetWidgetByName("My_user_javascript_chooser"), PrefsCollectionJS::UserJSFiles, m_real_server_name.CStr());

	OpString filename;
	GetWidgetText("My_user_javascript_chooser", filename);

	if (m_default_settings)
	{
		TRAPD(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::UserJSEnabled, filename.HasContent()));
		TRAP(err, g_pcjs->WriteIntegerL(PrefsCollectionJS::UserJSAlwaysLoad, filename.HasContent()));
	}
	else
	{
		TRAPD(err, g_pcjs->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionJS::UserJSEnabled, filename.HasContent(), TRUE));
		TRAP(err, g_pcjs->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionJS::UserJSAlwaysLoad, filename.HasContent(), TRUE));
	}
	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::SaveNetwork()
{
	WidgetPrefs::SetIntegerPref(GetWidgetByName("International_address_checkbox"), PrefsCollectionNetwork::UseUTF8Urls, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_referrer_logging_checkbox"), PrefsCollectionNetwork::ReferrerEnabled, m_real_server_name.CStr());
	
	// both these control redirect
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientRefresh, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Enable_redirection_checkbox"), PrefsCollectionNetwork::EnableClientPull, m_real_server_name.CStr());
	WidgetPrefs::SetIntegerPref(GetWidgetByName("Do_no_track_checkbox"), PrefsCollectionNetwork::DoNotTrack, m_real_server_name.CStr());

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Browser_id_dropdown");
	if (dropdown)
	{
		INTPTR uaid = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		if (m_default_settings)
		{
			TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UABaseId, uaid));
		}
		else
		{
			TRAPD(err, g_pcnet->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionNetwork::UABaseId, uaid, TRUE));
		}
	}

	dropdown = (OpDropDown*) GetWidgetByName("Proxy_dropdown");
	if (dropdown && m_proxy_setting_changed)
	{
		INTPTR selected = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());
		if (m_default_settings)
		{
			TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::EnableProxy, selected));
		}
		else
		{
			TRAPD(err, g_pcnet->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionNetwork::EnableProxy, selected, TRUE));
		}
	}

	dropdown = (OpDropDown*)GetWidgetByName("Application_cache_dropdown");
	if (dropdown)
	{
		INTPTR selected = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());
		if (m_default_settings)
		{
			TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StrategyOnApplicationCache, selected));
		}
		else
		{
			TRAPD(err, g_pcui->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionUI::StrategyOnApplicationCache, selected, TRUE));
		}
	}

	return OpStatus::OK;
}


OP_STATUS ServerPropertiesDialog::SaveWebHandler()
{
	BOOL has_deleted = FALSE;
	for (UINT32 i=0; i<m_handler_list.GetCount() && !has_deleted; i++)
		has_deleted = m_handler_list.Get(i)->m_deleted;
	if (has_deleted)
	{
		OpString csv_list;
		for (UINT32 i=0; i<m_handler_list.GetCount(); i++)
		{
			if (!m_handler_list.Get(i)->m_deleted)
			{
				if (csv_list.HasContent())
					RETURN_IF_ERROR(csv_list.Append(UNI_L(",")));
				RETURN_IF_ERROR(csv_list.Append(m_handler_list.Get(i)->m_name));
			}
		}
		if (csv_list.HasContent())
			RETURN_IF_LEAVE(g_pcjs->OverridePrefL(m_real_server_name.CStr(), PrefsCollectionJS::DisallowedWebHandlers, csv_list, TRUE));
		else
			RETURN_IF_LEAVE(g_pcjs->RemoveOverrideL(m_real_server_name.CStr(), PrefsCollectionJS::DisallowedWebHandlers, TRUE));
	}

	return OpStatus::OK;
}

int ServerPropertiesDialog::FillEncodingDropDown(OpDropDown *encoding_dropdown, const char *menu, BOOL lang_drop_down, const char *match)
{
	OP_MEMORY_VAR int current_index = -1, matched_index = -1;

	// Fill it with the same as the View/Encoding menu
	PrefsSection *section = g_application->GetMenuHandler()->GetMenuSection(menu);

	if (section)
	{
		OpWidgetImage widget_image;
		QuickMenu::MenuType menu_type;
		OpString	item_name, default_language_item_name;
		OpString8	sub_menu_name;
		INTPTR		sub_menu_value, menu_value;
		INT32		got_index;
		BOOL		ghost_item;

		for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
			OpLineParser	line(entry->Key());

			if (QuickMenu::ParseMenuEntry(&line, item_name, default_language_item_name, sub_menu_name, sub_menu_value, menu_value, menu_type, ghost_item, widget_image))
			{
				// Only add items and submenus
				if (menu_type == QuickMenu::MENU_ITEM || menu_type == QuickMenu::MENU_SUBMENU)
				{
					current_index++;

					OpString8 *menu_to_load = OP_NEW(OpString8, ());

					if (lang_drop_down)
					{
						menu_to_load->Set(sub_menu_name);
					}
					else
					{
						OpString8 charset;
						int      start;

						charset.Set(entry->Value());

						start = charset.FindFirstOf('"');
						if (start != KNotFound)
						{
							charset.Delete(0,start+1);
							start = charset.FindLastOf('"');
							if (start != KNotFound)
								charset.Delete(start);
						}

						menu_to_load->TakeOver(charset);

						// If matching save the index of the match
						if (match)
						{
							if (!op_strcmp(match, menu_to_load->CStr()))
								matched_index = current_index;
						}
					}

					if (encoding_dropdown)
						encoding_dropdown->AddItem(item_name.CStr(), -1, &got_index, reinterpret_cast<INTPTR>(menu_to_load));
					else
						OP_DELETE(menu_to_load);
				}
			}
		}
	}

	return match ? matched_index : -1;
}

void ServerPropertiesDialog::ResetEncodingDropDown(OpDropDown *encoding_dropdown)
{
	if (encoding_dropdown)
	{
		int num_items = encoding_dropdown->CountItems();
		for (int i = 0; i < num_items; i++)
		{
			OpString8 *d = (OpString8 *)encoding_dropdown->GetItemUserData(i);
			if (d)
				OP_DELETE(d);
		}
		encoding_dropdown->Clear();
	}
}

OP_STATUS
ServerPropertiesDialog::GetCookieDomain(const OpStringC & server_name, OpString & cookie_domain)
{
	if (server_name.Find(".") == KNotFound)
	{
		cookie_domain.Empty();
		return cookie_domain.AppendFormat(UNI_L("%s.local"), server_name.CStr());
	}
	else
	{
		return cookie_domain.Set(server_name.CStr());
	}
}

