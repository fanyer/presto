/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/sync/view/SyncSetupWizard.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/sync/view/SyncPasswordImprovementDialog.h"
#include "adjunct/desktop_util/passwordstrength/PasswordStrength.h"

#include "adjunct/quick/controller/FeatureController.h"
#include "adjunct/quick/dialogs/SyncMainUserSwitchDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/widgets/OpButton.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
**  SyncDialog::SyncDialog
************************************************************************************/
SyncDialog::SyncDialog(SyncController* controller, SyncSettingsContext* settings_context, OperaAccountController * account_controller, OperaAccountContext* account_context, BOOL needs_setup) :
	FeatureDialog(account_controller, account_context, needs_setup),
	m_controller(controller),
	m_preset_settings(settings_context),
	m_current_settings(),
	m_passwd_checkbox_initial(FALSE),
	m_passwd_checkbox_changed(FALSE)
{
	OP_ASSERT(m_controller != NULL);
	OP_ASSERT(m_preset_settings != NULL);
	m_current_settings.SetIsFeatureEnabled(m_preset_settings->IsFeatureEnabled());
}

/***********************************************************************************
**  SyncDialog::GetHelpAnchor
************************************************************************************/
const char* 
SyncDialog::GetHelpAnchor()
{
	return "link.html";
}

/***********************************************************************************
**  SyncDialog::SwitchingUserWanted
************************************************************************************/
/*virtual*/ BOOL
SyncDialog::SwitchingUserWanted()
{
	// Warn like crazy if they are trying to change main users
	BOOL switching_user = FALSE;
	SyncMainUserSwitchDialog *dialog = OP_NEW(SyncMainUserSwitchDialog, (&switching_user));
	if (dialog)
		dialog->Init(this);
	return switching_user;
}

/***********************************************************************************
**  SyncDialog::OnChange
************************************************************************************/
void 
SyncDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget)
	{
		OpRadioButton* enable_radio = static_cast<OpRadioButton*>(GetWidgetByName("sync_enable_radio"));
		OpRadioButton* disable_radio = static_cast<OpRadioButton*>(GetWidgetByName("sync_disable_radio"));
		
		if (enable_radio && enable_radio == widget)
		{
			BOOL enabled = enable_radio->GetValue();

			m_current_settings.SetIsFeatureEnabled(enabled);
			SetInputFieldsEnabled(enabled);
		}
		else if (disable_radio && disable_radio == widget)
		{
			BOOL enabled = !disable_radio->GetValue();

			m_current_settings.SetIsFeatureEnabled(enabled);
			SetInputFieldsEnabled(enabled);
		}

		// Disable (grey out) personal bar support checkbox if bookmarks support is disabled
		OpCheckBox* checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_bookmarks_checkbox"));
		if (checkbox && (OpWidget*)checkbox == widget)
		{
			BOOL has_bookmarks = (BOOL)checkbox->GetValue();
			checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_personalbar_checkbox"));
			if (checkbox)
			{
				if (!has_bookmarks)
					checkbox->SetEnabled(FALSE);
				else
					checkbox->SetEnabled(TRUE);
			}
		}
		else
		{
			checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_personalbar_checkbox"));
			if (checkbox && (OpWidget*)checkbox == widget)
			{
				OpCheckBox* bm_checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_bookmarks_checkbox"));
				if (bm_checkbox && !((BOOL)bm_checkbox->GetValue()))
				{
					checkbox->SetEnabled(FALSE);
				}
			}
		}

		OpCheckBox* passwd_checkbox = 
				static_cast<OpCheckBox*>(GetWidgetByName("sync_password_checkbox"));
		if (passwd_checkbox)
		{
			BOOL new_value = static_cast<BOOL>(passwd_checkbox->GetValue());
			m_passwd_checkbox_changed = m_passwd_checkbox_initial != new_value;
		}
	}
	FeatureDialog::OnChange(widget, changed_by_mouse);
}

/***********************************************************************************
**  SyncDialog::OnInit
************************************************************************************/
void
SyncDialog::OnInit()
{
	FeatureDialog::OnInit();
	m_controller->SetConfiguring(TRUE);
}

/***********************************************************************************
**  SyncDialog::ShouldShowPasswordImprovementDialog
************************************************************************************/
BOOL
SyncDialog::ShouldShowPasswordImprovementDialog()
{
	BOOL settings_page = IsFeatureSettingsPage(GetCurrentPage());
	BOOL login_page = IsLoginPage(GetCurrentPage()) && IsUserCredentialsSet();

	OpString password;
	RETURN_VALUE_IF_ERROR(password.Set(g_desktop_account_manager->GetPassword()), FALSE);
	if (password.IsEmpty())
		return FALSE;

	BOOL is_weak = FALSE;
	if (IsLoggedIn() && m_current_settings.SupportsPasswordManager() && m_passwd_checkbox_changed &&
		(settings_page || login_page))
	{
		PasswordStrength::Level strength = PasswordStrength::NONE;
		INT32 passwd_min_length = OperaAccountContext::GetPasswordMinLength();
		strength = PasswordStrength::Check(password, passwd_min_length);
		is_weak = strength == PasswordStrength::VERY_WEAK || strength == PasswordStrength::WEAK;
	}

	return is_weak;
}

/***********************************************************************************
**  SyncDialog::OnClose
************************************************************************************/
void
SyncDialog::OnClose(BOOL user_initiated)
{
	FeatureDialog::OnClose(user_initiated);
	m_controller->SetConfiguring(FALSE);

	if (ShouldShowPasswordImprovementDialog())
	{
		GetFeatureController()->InvokeMessage(MSG_QUICK_PASSWORDS_RECOVERY_KNOWN, (MH_PARAM_1)GetPresetFeatureSettings()->GetFeatureType(), (MH_PARAM_2)0);
	}
}

/***********************************************************************************
**  SyncDialog::OnCancel
************************************************************************************/
void
SyncDialog::OnCancel()
{
	FeatureDialog::OnCancel();
	m_passwd_checkbox_changed = FALSE;
}

/***********************************************************************************
**  SyncDialog::GetFeatureController
************************************************************************************/
FeatureController* 
SyncDialog::GetFeatureController()
{
	return m_controller;
}

/***********************************************************************************
**  SyncDialog::GetPresetFeatureSettings
************************************************************************************/
const FeatureSettingsContext*
SyncDialog::GetPresetFeatureSettings() const
{
	return m_preset_settings;
}

/***********************************************************************************
**  SyncDialog::ReadCurrentFeatureSettings
************************************************************************************/
FeatureSettingsContext*
SyncDialog::ReadCurrentFeatureSettings()
{
	ReadSettings(m_current_settings);

	return &m_current_settings;
}

/***********************************************************************************
**  SyncDialog::IsFeatureEnabled
************************************************************************************/
/*virtual*/ BOOL
SyncDialog::IsFeatureEnabled() const
{
	return m_current_settings.IsFeatureEnabled();
}

/***********************************************************************************
**  SyncDialog::InitIntroPage
************************************************************************************/
void 
SyncDialog::InitIntroPage()
{
	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("sync_intro_service_icon"));
	if (service_icon)
		service_icon->GetBorderSkin()->SetImage("Link Logo");

	// make title bold
	OpLabel* title_label = static_cast<OpLabel*>(GetWidgetByName("sync_intro_title_label"));
	if (title_label)
	{
		title_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		title_label->SetRelativeSystemFontSize(150);
	}

	// wrap text
	OpLabel* text_label = static_cast<OpLabel*>(GetWidgetByName("sync_intro_text_label"));
	if (text_label)
		text_label->SetWrap(TRUE);

	// disable checkboxes that aren't supposed to be there

#ifndef SUPPORT_SYNC_NOTES
	OpCheckBox* notes_checkbox;
	notes_checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_notes_checkbox"));
	if (notes_checkbox)
		notes_checkbox->SetVisibility(FALSE);
#endif // !SUPPORT_SYNC_NOTES

#ifndef SYNC_TYPED_HISTORY
	OpCheckBox* history_checkbox;
	history_checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_typedhistory_checkbox"));
	if (history_checkbox)
		history_checkbox->SetVisibility(FALSE);
#endif // !SYNC_TYPED_HISTORY

#ifndef SUPPORT_SYNC_SEARCHES
	OpCheckBox* searches_checkbox;
	searches_checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_searches_checkbox"));
	if (searches_checkbox)
		searches_checkbox->SetVisibility(FALSE);
#endif // !SUPPORT_SYNC_SEARCHES
#ifndef SYNC_CONTENT_FILTERS
	OpCheckBox* urlfilter_checkbox;
	urlfilter_checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_urlfilter_checkbox"));
	if (urlfilter_checkbox)
		urlfilter_checkbox->SetVisibility(FALSE);
#endif // !SYNC_CONTENT_FILTERS
}

/***********************************************************************************
**  SyncDialog::InitCreateAccountPage
************************************************************************************/
/*virtual*/ void
SyncDialog::InitCreateAccountPage()
{
	FeatureDialog::InitCreateAccountPage();

	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("account_service_icon"));
	if (service_icon)
		service_icon->GetBorderSkin()->SetImage("Link Logo");

	OpLabel* label_account = static_cast<OpLabel*>(GetWidgetByName("signup_intro_text_label"));
 	if (label_account)
	{
		// make introduction multi-line
 		label_account->SetWrap(TRUE);

		// compose intro label
		OpString info_text;
		g_languageManager->GetString(Str::D_SYNC_ACCOUNT_INTRO, info_text);
		label_account->SetText(info_text.CStr());
	}
}

/***********************************************************************************
**  SyncDialog::InitLoginPage
************************************************************************************/
/*virtual*/ void
SyncDialog::InitLoginPage()
{
	FeatureDialog::InitLoginPage();

	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("login_service_icon"));
	if (service_icon)
		service_icon->GetBorderSkin()->SetImage("Link Logo");

	OpLabel* label_account = static_cast<OpLabel*>(GetWidgetByName("login_intro_text_label"));
 	if (label_account)
	{
		// make introduction multi-line
 		label_account->SetWrap(TRUE);

		OpString info_text;
		g_languageManager->GetString(Str::D_SYNC_LOGIN_INTRO, info_text);
		label_account->SetText(info_text.CStr());
	}
}

/***********************************************************************************
**  SyncDialog::InitSettingsPage
************************************************************************************/
void
SyncDialog::InitSettingsPage()
{
	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("sync_settings_service_icon"));
	if (service_icon)
		service_icon->GetBorderSkin()->SetImage("Link Logo");

	// make title bold
	OpLabel* title_label = static_cast<OpLabel*>(GetWidgetByName("sync_settings_title_label"));
	if (title_label)
	{
		title_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		title_label->SetRelativeSystemFontSize(150);
	}

	if (!NeedsSetup())
	{
		BOOL enabled = m_preset_settings->IsFeatureEnabled();
		OpRadioButton* state_radio;

		if (enabled)
			state_radio = (OpRadioButton*)GetWidgetByName("sync_enable_radio");
		else
			state_radio = (OpRadioButton*)GetWidgetByName("sync_disable_radio");

		if (state_radio)
			state_radio->SetValue(TRUE);

		SetInputFieldsEnabled(enabled);
	}
	else
	{
		ShowWidget("sync_enable_radio", FALSE);
		ShowWidget("sync_disable_radio", FALSE);
	}

	OpCheckBox* checkbox;
	
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_bookmarks_checkbox"));
	if (checkbox)
		checkbox->SetValue(m_preset_settings->SupportsBookmarks());

	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_speeddial_checkbox"));
	if (checkbox)
		checkbox->SetValue(m_preset_settings->SupportsSpeedDial());

	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_personalbar_checkbox"));
	if (checkbox)
		checkbox->SetValue(m_preset_settings->SupportsPersonalbar());

#ifdef SUPPORT_SYNC_NOTES
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_notes_checkbox"));
	if (checkbox)
		checkbox->SetValue(m_preset_settings->SupportsNotes());
#endif // SUPPORT_SYNC_NOTES

#ifdef SYNC_TYPED_HISTORY
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_typedhistory_checkbox"));
	if (checkbox)
		checkbox->SetValue(m_preset_settings->SupportsTypedHistory());
#endif // SYNC_TYPED_HISTORY

#ifdef SUPPORT_SYNC_SEARCHES
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_searches_checkbox"));
	if (checkbox)
		checkbox->SetValue(m_preset_settings->SupportsSearches());
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_CONTENT_FILTERS
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_urlfilter_checkbox"));
	if (checkbox)
		checkbox->SetValue(m_preset_settings->SupportsURLFilter());
#endif // !SYNC_CONTENT_FILTERS
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_password_checkbox"));
	if (checkbox)
	{
		checkbox->SetValue(m_preset_settings->SupportsPasswordManager());
		m_passwd_checkbox_initial = static_cast<BOOL>(checkbox->GetValue());
		m_passwd_checkbox_changed = FALSE;
	}
}

/***********************************************************************************
**  SyncDialog::GetFeatureIntroPageName
************************************************************************************/
const char*
SyncDialog::GetFeatureIntroPageName() const
{
	return "sync_intro_page";
}

/***********************************************************************************
**  SyncDialog::GetFeatureSettingsPageName
************************************************************************************/
const char*
SyncDialog::GetFeatureSettingsPageName() const
{
	return "sync_settings_page";
}

/***********************************************************************************
**  SyncDialog::GetIntroSettingsProgressSpinnerName
************************************************************************************/
const char*
SyncDialog::GetFeatureIntroProgressSpinnerName() const
{
       return ""; // currently not needed, enabling Sync is syncronous
}

/***********************************************************************************
**  SyncDialog::GetIntroSettingsProgressLabelName
************************************************************************************/
const char*
SyncDialog::GetFeatureIntroProgressLabelName() const
{
       return ""; // currently not needed, enabling Sync is syncronous
}


/***********************************************************************************
**  SyncDialog::GetFeatureSettingsProgressSpinnerName
************************************************************************************/
const char*
SyncDialog::GetFeatureSettingsProgressSpinnerName() const
{
	return ""; // currently not needed, enabling Sync is syncronous
}

/***********************************************************************************
**  SyncDialog::GetFeatureSettingsProgressLabelName
************************************************************************************/
const char*
SyncDialog::GetFeatureSettingsProgressLabelName() const
{
	return ""; // currently not needed, enabling Sync is syncronous
}

/***********************************************************************************
**  SyncDialog::GetFeatureSettingsErrorLabelName
************************************************************************************/
const char*
SyncDialog::GetFeatureSettingsErrorLabelName() const
{
	return ""; // currently not needed, no errors reported back
}

/***********************************************************************************
**  SyncDialog::HasValidSettings
************************************************************************************/
BOOL
SyncDialog::HasValidSettings()
{
	return TRUE;
}

/***********************************************************************************
**  SyncDialog::ReadSettings
************************************************************************************/
void
SyncDialog::ReadSettings(SyncSettingsContext & settings)
{
	OpCheckBox* checkbox;

	BOOL  bookmarks_enabled = FALSE;
	
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_bookmarks_checkbox"));
	if (checkbox)
	{
		m_current_settings.SetSupportsBookmarks((BOOL)checkbox->GetValue());
		bookmarks_enabled = (BOOL)checkbox->GetValue();
		if (!bookmarks_enabled) // syncing personalbar requires syncing of bookmarks
		{
			m_current_settings.SetSupportsPersonalbar(FALSE);
		}
		else
		{
			checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_personalbar_checkbox"));
			if (checkbox)
			{
				m_current_settings.SetSupportsPersonalbar((BOOL)checkbox->GetValue());
			}
		}
	}

	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_speeddial_checkbox"));
	if (checkbox)
		m_current_settings.SetSupportsSpeedDial((BOOL)checkbox->GetValue());

#ifdef SUPPORT_SYNC_NOTES
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_notes_checkbox"));
	if (checkbox)
		m_current_settings.SetSupportsNotes((BOOL)checkbox->GetValue());
#endif // SUPPORT_SYNC_NOTES

#ifdef SYNC_TYPED_HISTORY
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_typedhistory_checkbox"));
	if (checkbox)
		m_current_settings.SetSupportsTypedHistory((BOOL)checkbox->GetValue());
#endif // SYNC_TYPED_HISTORY

#ifdef SUPPORT_SYNC_SEARCHES
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_searches_checkbox"));
	if (checkbox)
		m_current_settings.SetSupportsSearches((BOOL)checkbox->GetValue());
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_CONTENT_FILTERS
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_urlfilter_checkbox"));
	if (checkbox)
		m_current_settings.SetSupportsURLFilter((BOOL)checkbox->GetValue());
#endif // !SYNC_CONTENT_FILTERS
	checkbox = static_cast<OpCheckBox*>(GetWidgetByName("sync_password_checkbox"));
	if (checkbox)
		m_current_settings.SetSupportsPasswordManager((BOOL)checkbox->GetValue());
}

/***********************************************************************************
**  SyncDialog::SetInputFieldsEnabled
************************************************************************************/
void
SyncDialog::SetInputFieldsEnabled(BOOL enabled)
{
	SetWidgetEnabled("sync_bookmarks_checkbox", enabled);
	SetWidgetEnabled("sync_speeddial_checkbox", enabled);
	SetWidgetEnabled("sync_personalbar_checkbox", enabled);
	
#ifdef SUPPORT_SYNC_NOTES
	SetWidgetEnabled("sync_notes_checkbox", enabled);
#endif // SUPPORT_SYNC_NOTES

#ifdef SYNC_TYPED_HISTORY
	SetWidgetEnabled("sync_typedhistory_checkbox", enabled);
#endif // SYNC_TYPED_HISTORY

#ifdef SUPPORT_SYNC_SEARCHES
	SetWidgetEnabled("sync_searches_checkbox", enabled);
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_CONTENT_FILTERS
	SetWidgetEnabled("sync_urlfilter_checkbox", enabled);
#endif // SYNC_CONTENT_FILTERS

	SetWidgetEnabled("sync_password_checkbox", enabled);
}

#endif // SUPPORT_DATA_SYNC
