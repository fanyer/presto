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

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/view/WebServerSetupWizard.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/webserver/controller/WebServerServiceDownloadContext.h"
#include "adjunct/quick/webserver/controller/WebServerServiceSettingsContext.h"
#include "adjunct/quick/webserver/view/WebServerAdvancedSettingsDialog.h"

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/widgets/OpButton.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"

#include "adjunct/quick/managers/WebServerManager.h"

#include "adjunct/quick/managers/DesktopGadgetManager.h"

#define WEBSERVER_DEFAULT_NAME	UNI_L("home")

const INT32 DEVICENAME_MAX_LENGTH = 63;
const INT32 NEXT_BUTTON_INDEX = 1;

const char WEBSERVER_UPD_PROGRESS_LABEL[] = "webserver_upd_progress_label";
const char WEBSERVER_UPD_PROGRESS_SPINNER[] = "webserver_upd_progress_spinner";

/***********************************************************************************
 *
 *  WebServerDialog::WebServerDialog
 *
 ************************************************************************************/
WebServerDialog::WebServerDialog(WebServerController * controller, 
								 const WebServerSettingsContext* settings_context, 
								 OperaAccountController * account_controller, 
								 OperaAccountContext* account_context, 
								 BOOL needs_setup) :
	FeatureDialog(account_controller, account_context, needs_setup),
	m_controller(controller),
	m_preset_settings(settings_context),
	m_current_settings(),
	m_in_progress(FALSE)
#ifdef  GADGET_UPDATE_SUPPORT
	,m_update_lstnr_registered(FALSE)
#endif //GADGET_UPDATE_SUPPORT
{
	OP_ASSERT(m_preset_settings != NULL);
	m_current_settings.SetIsFeatureEnabled(m_preset_settings->IsFeatureEnabled());
}

/***********************************************************************************
**  WebServerDialog::GetHelpAnchor
************************************************************************************/
const char*
WebServerDialog::GetHelpAnchor()
{
	return "unite.html";
}

/***********************************************************************************
** WebServerDialog::OnInit
************************************************************************************/
void
WebServerDialog::OnInit()
{
	FeatureDialog::OnInit();
	m_controller->SetConfiguring(TRUE);
}

/***********************************************************************************
**  WebServerDialog::OnInit
************************************************************************************/
void
WebServerDialog::OnCancel()
{
	FeatureDialog::OnCancel();
}

/***********************************************************************************
**  WebServerDialog::OnClose
************************************************************************************/
void
WebServerDialog::OnClose(BOOL user_initiated)
{
	FeatureDialog::OnClose(user_initiated);
	m_controller->SetConfiguring(FALSE);
	//m_controller->RemoveWebServerListener(this);

}

/***********************************************************************************
**  WebServerDialog::OnOperaAccountReleaseDevice
************************************************************************************/
/*virtual*/ void
WebServerDialog::OnOperaAccountReleaseDevice(OperaAuthError error)
{
	switch(error)
	{
	case AUTH_ERROR_PARSER:						// code 400: missing argument or malformed XML
	case AUTH_ACCOUNT_AUTH_FAILURE:				// code 403: authentication error (error in password or user doesn't exist)
	case AUTH_ACCOUNT_CREATE_DEVICE_INVALID:	// code 411: invalid device name
		{
			HideProgressInfo();
			SetInProgress(FALSE);
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_DEFAULT); // todo: specific error message here?
			return;	
		}
	case AUTH_OK:
		break;

	default:
		{
			OP_ASSERT(!"no other error than the above should happen on device release");
		}
	};	
}

/***********************************************************************************
**  WebServerDialog::OnOperaAccountReleaseDevice
************************************************************************************/
/*virtual*/ void
WebServerDialog::OnOperaAccountDeviceCreate(OperaAuthError error, const OpStringC& shared_secret, 
		const OpStringC& server_message)
{
	switch(error)
	{
	case AUTH_OK:
		{
			return;
		}
	case AUTH_ACCOUNT_CREATE_DEVICE_EXISTS: // code 405
		{
			if (m_controller->IsUserRequestingDeviceRelease(this))
			{
				OpString enabling_string;
				OpString feature_string;
				OpString merged_string;

				g_languageManager->GetString(Str::D_FEATURE_SETUP_ENABLING_FEATURE, enabling_string);
				g_languageManager->GetString(GetPresetFeatureSettings()->GetFeatureStringID(), feature_string);
				merged_string.AppendFormat(enabling_string.CStr(), feature_string.CStr());

				ShowProgressInfo(merged_string);
				m_controller->EnableFeature(&m_current_settings, TRUE); // force device release
			}
			else
			{
				HideProgressInfo();
				SetInProgress(FALSE);
				ShowError(Str::D_WEBSERVER_DEVICE_NAME_EXISTS);
			}
			return;
		}
	case AUTH_ACCOUNT_CREATE_DEVICE_INVALID: // code 411
		{
			HideProgressInfo();
			SetInProgress(FALSE);
			ShowError(Str::D_WEBSERVER_DEVICE_NAME_INVALID);
			return;
		}
	// code 407: invalid username, might still be able to authenticate on MyOpera or using Link (if username has been valid at some point)
	case AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS: 
		{
			INT32 current_page = GetCurrentPage();
			if (IsFeatureIntroPage(current_page))
			{
				OpInputAction forward_action(OpInputAction::ACTION_FORWARD); // go to login page
				OnInputAction(&forward_action);
				HandleError(error);
			}
			else if (IsLoginPage(current_page) || IsCreateAccountPage(current_page))
			{
				HandleError(error);
			}
			else if (IsFeatureSettingsPage(current_page))
			{
				if (HasToRevertUserCredentials())
				{
					HideProgressInfo(); // needed so that the progress-text is cleared when coming back to this page later

					// go back to previous (login or account page)
					OpInputAction back_action(OpInputAction::ACTION_BACK);
					OnInputAction(&back_action);
					HandleError(error); // show error on the page that shows account credentials
				}
				else
				{
					GetFeatureController()->InvokeMessage(MSG_QUICK_SHOW_WEBSERVER_INVALID_USER, (MH_PARAM_1)0, (MH_PARAM_2)0);
					CloseDialog(FALSE, TRUE); // close immediately (don't call cancel)
				}
			}
			return;
		}
	case AUTH_ACCOUNT_AUTH_FAILURE: // code 403: error in password or user doesn't exist - shouldn't happen because user is authenticated at this point
	case AUTH_ERROR_PARSER: // code 400
	case AUTH_ACCOUNT_AUTH_INVALID_KEY: // code 411
	default:
		{
			OP_ASSERT(FALSE); // these errors shouldn't be happening here
			HandleError(error);
			return;
		}
	}
}

/***********************************************************************************
**  WebServerDialog::GetFeatureController
************************************************************************************/
/*virtual*/ void
WebServerDialog::HandleError(OperaAuthError error)
{
	switch(error)
	{
	case AUTH_ACCOUNT_CREATE_DEVICE_EXISTS:
		{
			ShowError(Str::D_WEBSERVER_DEVICE_NAME_EXISTS);
			break;
		}
	case AUTH_ACCOUNT_CREATE_DEVICE_INVALID:
		{
			HideProgressInfo();
			SetInProgress(FALSE);
			ShowError(Str::D_WEBSERVER_DEVICE_NAME_INVALID);
			break;
		}
	default:
		FeatureDialog::HandleError(error);
	}
}

/***********************************************************************************
**  WebServerDialog::GetFeatureController
************************************************************************************/
FeatureController*
WebServerDialog::GetFeatureController()
{
	return m_controller;
}

/***********************************************************************************
**  WebServerDialog::GetPresetFeatureSettings
************************************************************************************/
const FeatureSettingsContext*
WebServerDialog::GetPresetFeatureSettings() const
{
	return m_preset_settings;
}

/***********************************************************************************
**  WebServerDialog::ReadCurrentFeatureSettings
************************************************************************************/
FeatureSettingsContext*
WebServerDialog::ReadCurrentFeatureSettings()
{
	ReadSettings(m_current_settings);

	return &m_current_settings;
}

/***********************************************************************************
**  WebServerDialog::IsFeatureEnabled
************************************************************************************/
/*virtual*/ BOOL
WebServerDialog::IsFeatureEnabled() const
{
	return m_current_settings.IsFeatureEnabled();
}

/***********************************************************************************
**  WebServerDialog::OnChange
************************************************************************************/
void 
WebServerDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget)
	{
		OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("webserver_computername_dropdown"));
		OpRadioButton* enable_radio = static_cast<OpRadioButton*>(GetWidgetByName("webserver_enable_radio"));
		OpRadioButton* disable_radio = static_cast<OpRadioButton*>(GetWidgetByName("webserver_disable_radio"));
		
		if (enable_radio == widget || disable_radio == widget)
		{
			BOOL enabled = enable_radio->GetValue();

			m_current_settings.SetIsFeatureEnabled(enabled);
			SetInputFieldsEnabled(enabled);
		}
		else if (dropdown && (dropdown == widget || dropdown->edit == widget))
		{
			OpString computername;
			dropdown->GetText(computername);
			UpdateURL(computername);
		}
	}
	FeatureDialog::OnChange(widget, changed_by_mouse);
}

/***********************************************************************************
**  WebServerDialog::OnInputAction
************************************************************************************/
/*virtual*/ BOOL
WebServerDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_OPEN_ADVANCED_WEBSERVER_SETTINGS: // disable button after clicking it once
				{
					child_action->SetEnabled(IsFeatureSettingsPage(GetCurrentPage()));
					return TRUE;
				}
			}
		}
		break;

	case OpInputAction::ACTION_OPEN_ADVANCED_WEBSERVER_SETTINGS:
		{
			WebServerAdvancedSettingsDialog * dialog = OP_NEW(WebServerAdvancedSettingsDialog, (&m_current_settings, &m_current_settings));
			if (dialog)
			{
				OpStatus::Ignore(dialog->Init(this));
			}
			return TRUE;
		}
	}
	return FeatureDialog::OnInputAction(action);
}

/***********************************************************************************
**  WebServerDialog::InitIntroPage
************************************************************************************/
void 
WebServerDialog::InitIntroPage()
{
	// make title bold
	OpLabel* title_label = static_cast<OpLabel*>(GetWidgetByName("webserver_intro_title_label"));
	if (title_label)
	{
		title_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		title_label->SetRelativeSystemFontSize(150);
	}
	// wrap text
	OpLabel* text_label = static_cast<OpLabel*>(GetWidgetByName("webserver_intro_text_label"));
	if (text_label)
		text_label->SetWrap(TRUE);

	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("webserver_intro_service_icon"));
	if (service_icon)
	{
		service_icon->GetBorderSkin()->SetImage("Unite Logo");
	}

	// init progress widgets
	OpProgressBar * progress_spinner = static_cast<OpProgressBar *>(GetWidgetByName("webserver_intro_progress_spinner"));
	if (progress_spinner)
	{
		progress_spinner->SetType(OpProgressBar::Spinner);
		progress_spinner->SetVisibility(FALSE);
	}
	OpLabel* progress_label = static_cast<OpLabel*>(GetWidgetByName("webserver_intro_progress_label"));
	if (progress_label)
	{
		progress_label->SetWrap(TRUE);
	}
	
#ifdef GADGET_UPDATE_SUPPORT
	EnableButton(NEXT_BUTTON_INDEX,FALSE);
#endif //GADGET_UPDATE_SUPPORT
}

/***********************************************************************************
**  WebServerDialog::InitCreateAccountPage
************************************************************************************/
/*virtual*/ void
WebServerDialog::InitCreateAccountPage()
{
	FeatureDialog::InitCreateAccountPage();

	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("account_service_icon"));
	if (service_icon)
		service_icon->GetBorderSkin()->SetImage("Unite Logo");

	OpLabel* label_account = static_cast<OpLabel*>(GetWidgetByName("signup_intro_text_label"));
 	if (label_account)
	{
		// make introduction multi-line
 		label_account->SetWrap(TRUE);

		// compose intro label
		OpString info_text;
		g_languageManager->GetString(Str::D_WEBSERVER_ACCOUNT_INTRO, info_text);
		label_account->SetText(info_text.CStr());
	}
}

/***********************************************************************************
**  WebServerDialog::InitLoginPage
************************************************************************************/
/*virtual*/ void
WebServerDialog::InitLoginPage()
{
	FeatureDialog::InitLoginPage();

	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("login_service_icon"));
	if (service_icon)
		service_icon->GetBorderSkin()->SetImage("Unite Logo");

	OpLabel* label_account = static_cast<OpLabel*>(GetWidgetByName("login_intro_text_label"));
 	if (label_account)
	{
		// make introduction multi-line
 		label_account->SetWrap(TRUE);

		OpString info_text;
		g_languageManager->GetString(Str::D_WEBSERVER_LOGIN_INTRO, info_text);
		label_account->SetText(info_text.CStr());
	}
}

/***********************************************************************************
**  WebServerDialog::InitSettingsPage
************************************************************************************/
void
WebServerDialog::InitSettingsPage()
{
	OpLabel* service_icon = static_cast<OpLabel*>(GetWidgetByName("webserver_service_icon"));
	if (service_icon)
		service_icon->GetBorderSkin()->SetImage("Unite Logo");
	
	// make title bold
	OpLabel* title_label = static_cast<OpLabel*>(GetWidgetByName("webserver_title_label"));
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
			state_radio = (OpRadioButton*)GetWidgetByName("webserver_enable_radio");
		else
			state_radio = (OpRadioButton*)GetWidgetByName("webserver_disable_radio");

		if (state_radio)
			state_radio->SetValue(TRUE);

		SetInputFieldsEnabled(enabled);
	}
	else
	{
		ShowWidget("webserver_enable_radio", FALSE);
		ShowWidget("webserver_disable_radio", FALSE);
	}

	// bold heading
	OpLabel* devicename_title = static_cast<OpLabel*>(GetWidgetByName("webserver_devicename_label"));
	if (devicename_title)
		devicename_title->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

	// wrap text
	OpLabel* text_label = static_cast<OpLabel*>(GetWidgetByName("webserver_settings_label"));
	if (text_label)
		text_label->SetWrap(TRUE);

	// set ellipsis for url
	OpLabel* url_label = static_cast<OpLabel*>(GetWidgetByName("webserver_url_label"));
	if (url_label)
		url_label->SetEllipsis(ELLIPSIS_CENTER);

	if (m_preset_settings)
	{
		OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("webserver_computername_dropdown"));
		if (dropdown)
		{
			dropdown->SetEditableText(TRUE);
			dropdown->edit->SetMaxLength(DEVICENAME_MAX_LENGTH);
			dropdown->edit->SetOnChangeOnEnter(FALSE); // make sure changes are reported back on all input

			INT32 selected_item = -1;

			const OpStringC device_name = m_preset_settings->GetDeviceName();
			const OpAutoVector<OpString>* suggestions = m_preset_settings->GetDeviceNameSuggestions();
			for (UINT idx = 0; idx < suggestions->GetCount(); idx++)
			{
				OpString* item = suggestions->Get(idx);
				dropdown->AddItem(item->CStr());
				if (selected_item == -1 && !device_name.IsEmpty() && item->Compare(device_name) == 0)
				{
					selected_item = idx;
				}
			}
			// add default item if not already in the list
			if (selected_item == -1 && !device_name.IsEmpty())
				dropdown->AddItem(device_name.CStr(), 0);

			// add one default item if there is none
			if (dropdown->CountItems() <= 0)
				dropdown->AddItem(WEBSERVER_DEFAULT_NAME);
			
			// set selected item if not already set
			if (selected_item == -1)
				selected_item = 0;

			// check if device name is part of the suggestions list
			dropdown->SetValue(selected_item);

			OpString computername;
			dropdown->GetText(computername);
			UpdateURL(computername);
		}
	}

	// init progress widgets
	OpProgressBar * progress_spinner = static_cast<OpProgressBar *>(GetWidgetByName("webserver_progress_spinner"));
	if (progress_spinner)
	{
		progress_spinner->SetType(OpProgressBar::Spinner);
		progress_spinner->SetVisibility(FALSE);
	}
	OpLabel* progress_label = static_cast<OpLabel*>(GetWidgetByName("webserver_progress_label"));
	if (progress_label)
		progress_label->SetWrap(TRUE);
	
	// make error field multi-line, make it red
	OpLabel *error_label = static_cast<OpLabel*>(GetWidgetByName("webserver_error_label"));
	if (error_label)
	{
		error_label->SetWrap(TRUE);
		error_label->SetForegroundColor(OP_RGB(255, 0, 0));
	}

	// advanced dialog settings
#ifdef PREFS_CAP_UPNP_ENABLED
	m_current_settings.SetUPnPEnabled(m_preset_settings->IsUPnPEnabled());
#endif // PREFS_CAP_UPNP_ENABLED

	m_current_settings.SetUploadSpeed(m_preset_settings->GetUploadSpeed());
	m_current_settings.SetPort(m_preset_settings->GetPort());

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	m_current_settings.SetASDEnabled(m_preset_settings->IsASDEnabled());
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
	m_current_settings.SetUPnPServiceDiscoveryEnabled(m_preset_settings->IsUPnPServiceDiscoveryEnabled());
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	m_current_settings.SetVisibleRobotsOnHome(m_preset_settings->IsVisibleRobotsOnHome());
#endif // WEBSERVER_CAP_SET_VISIBILITY
}

/***********************************************************************************
**  WebServerDialog::GetFeatureIntroPageName
************************************************************************************/
const char*
WebServerDialog::GetFeatureIntroPageName() const
{
	return "webserver_intro_page";
}

/***********************************************************************************
**  WebServerDialog::GetFeatureSettingsPageName
************************************************************************************/
const char*
WebServerDialog::GetFeatureSettingsPageName() const
{
	return "webserver_settings_page";
}

/***********************************************************************************
**  WebServerDialog::GetIntroSettingsProgressSpinnerName
************************************************************************************/
const char*
WebServerDialog::GetFeatureIntroProgressSpinnerName() const
{
	return "webserver_intro_progress_spinner";
}

/***********************************************************************************
**  WebServerDialog::GetIntroSettingsProgressLabelName
************************************************************************************/
const char*
WebServerDialog::GetFeatureIntroProgressLabelName() const
{
	return "webserver_intro_progress_label";
}

/***********************************************************************************
**  WebServerDialog::GetFeatureSettingsProgressSpinnerName
************************************************************************************/
const char*
WebServerDialog::GetFeatureSettingsProgressSpinnerName() const
{
	return "webserver_progress_spinner";
}

/***********************************************************************************
**  WebServerDialog::GetFeatureSettingsProgressLabelName
************************************************************************************/
const char*
WebServerDialog::GetFeatureSettingsProgressLabelName() const
{
	return "webserver_progress_label";
}

/***********************************************************************************
**  WebServerDialog::GetFeatureSettingsErrorLabelName
************************************************************************************/
const char*
WebServerDialog::GetFeatureSettingsErrorLabelName() const
{
	return "webserver_error_label";
}


/***********************************************************************************
**  WebServerDialog::HasValidSettings
************************************************************************************/
BOOL
WebServerDialog::HasValidSettings()
{
	OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("webserver_computername_dropdown"));
	if (dropdown)
	{
		OpString text;
		if (OpStatus::IsSuccess(dropdown->GetText(text)))
			return text.HasContent(); // todo: check for valid string
	}
	return FALSE;
}

/***********************************************************************************
**  WebServerDialog::ReadSettings
************************************************************************************/
void
WebServerDialog::ReadSettings(WebServerSettingsContext & settings)
{
	OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName("webserver_computername_dropdown"));
	if (dropdown)
	{
		OpString text;
		if (OpStatus::IsSuccess(dropdown->GetText(text)))
			settings.SetDeviceName(text);
		// computername suggestions don't have to be set
	}
}

/***********************************************************************************
**  WebServerDialog::SetInputFieldsEnabled
************************************************************************************/
void
WebServerDialog::SetInputFieldsEnabled(BOOL enabled)
{
	OpString status_text, status_button_text;
	SetWidgetEnabled("webserver_computername_dropdown", enabled);
	SetWidgetEnabled("webserver_advanced_button", enabled);
}

/***********************************************************************************
**  WebServerDialog::UpdateURL
************************************************************************************/
void
WebServerDialog::UpdateURL(const OpStringC & computername)
{
	OpLabel* url_label = static_cast<OpLabel*>(GetWidgetByName("webserver_url_label"));
	if (url_label)
	{
		OpString url;
		if (OpStatus::IsSuccess(m_controller->GetURLForComputerName(url, computername)))
		{
			url_label->SetText(url.CStr());
		}
	}
}

#ifdef GADGET_UPDATE_SUPPORT
void	WebServerDialog::OnGadgetUpdateFinish(GadgetUpdateInfo* data,
									  GadgetUpdateController::GadgetUpdateResult result)
{
	if ( GadgetUpdateController::UPD_SUCCEDED == result)
	{
		FeatureDialog::Login();
	}
	else
	{
		HideProgressInfo();
		ShowError(Str::D_UNITE_UPDATE_HOME_DOWNLOAD_ERROR);
	}
	
}
#endif //GADGET_UPDATE_SUPPORT

#endif // WEBSERVER_SUPPORT
