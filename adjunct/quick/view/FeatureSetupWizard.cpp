/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 *
 * TESTCASES: featuredialog.ot
 */

#include "core/pch.h"

#include "adjunct/quick/view/FeatureSetupWizard.h"

#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/controller/FeatureController.h"
#include "adjunct/quick/controller/FeatureSettingsContext.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpPasswordStrength.h"
#include "adjunct/quick/view/FeatureLicenseDialog.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/opera_auth/opera_auth_myopera.h"

#ifdef FORMATS_URI_ESCAPE_SUPPORT
	#include "modules/formats/uri_escape.h"
#endif // FORMATS_URI_ESCAPE_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"

/***********************************************************************************
**  FeatureDialog::FeatureDialog
************************************************************************************/
FeatureDialog::FeatureDialog(OperaAccountController * controller, OperaAccountContext* context, BOOL needs_setup) :
	m_account_controller(controller),
	m_account_context(context),
	m_needs_setup(needs_setup),
	m_in_progress(FALSE),
	m_login_only(FALSE),
	m_blocking(FALSE),
	m_retval(NULL),
	m_scheduled_page_idx(-1),
	m_previous_page_idx(-1),
	m_incorrect_input_field()
{
#ifdef PREFS_CAP_ACCOUNT_SAVE_PASSWORD
	m_password_saved = g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::SavePassword);
#else
	m_password_saved = TRUE;
#endif // PREFS_CAP_ACCOUNT_SAVE_PASSWORD
}

/***********************************************************************************
**  FeatureDialog::InitLoginDialog
************************************************************************************/
/*virtual*/ OP_STATUS
FeatureDialog::InitLoginDialog(BOOL * logged_in, DesktopWindow* parent_window)
{
	SetBlockingForReturnValue(logged_in);

	m_login_only = TRUE;
	return Dialog::Init(parent_window);
}

/***********************************************************************************
**  FeatureDialog::SetBlockingForReturnValue
************************************************************************************/
void
FeatureDialog::SetBlockingForReturnValue(BOOL * retval)
{
	m_retval = retval;
	if (m_retval)
	{
		m_blocking = TRUE;
		*m_retval = FALSE;
	}
}

/***********************************************************************************
**  FeatureDialog::GetType
************************************************************************************/
/*virtual*/ OpTypedObject::Type
FeatureDialog::GetType()
{
	return DIALOG_TYPE_FEATURE_SETUP_WIZARD;
}

/***********************************************************************************
**  FeatureDialog::OnInputAction
************************************************************************************/
/*virtual*/ BOOL
FeatureDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_BACK:
				{
					if (m_in_progress)
					{			
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					else
						break;
				}
			case OpInputAction::ACTION_OK:
			case OpInputAction::ACTION_FORWARD:
				{
						child_action->SetEnabled(!m_in_progress && (m_scheduled_page_idx != -1 || HasValidInput(GetCurrentPage())));
					return TRUE;
				}
			case OpInputAction::ACTION_ACCEPT_FEATURE_LICENSE:
			case OpInputAction::ACTION_DECLINE_FEATURE_LICENSE:
			case OpInputAction::ACTION_VIEW_FEATURE_LICENSE:
			case OpInputAction::ACTION_FEATURE_SETUP_SKIP_ACCOUNT:
				{
					// allow action if "create account" page is shown
					child_action->SetEnabled(IsCreateAccountPage(GetCurrentPage()));
					return TRUE;
				}
			case OpInputAction::ACTION_FEATURE_SETUP_NEW_ACCOUNT:
				{
					// allow action if "login" page is shown
					child_action->SetEnabled(IsLoginPage(GetCurrentPage()));
					return TRUE;
				}
			}
			break;
		}
	case OpInputAction::ACTION_OK:
		{
			if (m_login_only) // login page
			{
				HandleLoginAction();
			}
			else
			{
				ChangeFeatureSettings();
			}
			return TRUE;
		}
	case OpInputAction::ACTION_VIEW_FEATURE_LICENSE:
		{
			ShowLicenseDialog();
			return TRUE;
		}
	case OpInputAction::ACTION_ACCEPT_FEATURE_LICENSE:
		{
			SetLicenseAccepted(TRUE);
			return TRUE;
		}
	case OpInputAction::ACTION_DECLINE_FEATURE_LICENSE:
		{
			SetLicenseAccepted(FALSE);
			return TRUE;
		}
	case OpInputAction::ACTION_FEATURE_SETUP_SKIP_ACCOUNT:
		{
			INT32 target_page = GetPageByName(GetLoginPageName());
			GoToTargetPage(target_page);
			return TRUE;
		}
	case OpInputAction::ACTION_FEATURE_SETUP_NEW_ACCOUNT:
		{
			if (m_login_only)
			{
				GetFeatureController()->InvokeMessage(MSG_QUICK_START_CLEAN_FEATURE_SETUP, (MH_PARAM_1)GetPresetFeatureSettings()->GetFeatureType(), (MH_PARAM_2)0);

				OP_ASSERT(*m_retval == FALSE);
				CloseDialog(TRUE);
			}
			else
			{
				INT32 target_page = GetPageByName(GetCreateAccountPageName());
				GoToTargetPage(target_page);
			}
			return TRUE;
		}
		break;
	}

	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**  FeatureDialog::GetDialogType
************************************************************************************/
/*virtual*/ Dialog::DialogType
FeatureDialog::GetDialogType()
{
	if (!m_login_only && (m_needs_setup || !IsLoggedIn()))
		return TYPE_WIZARD;
	else
		return TYPE_OK_CANCEL;
}

/***********************************************************************************
**  FeatureDialog::OnForward
************************************************************************************/
/*virtual*/ UINT32
FeatureDialog::OnForward(UINT32 page_number)
{
	if (m_scheduled_page_idx != -1)
	{
		m_previous_page_idx = page_number;
		INT32 retval = m_scheduled_page_idx;
		m_scheduled_page_idx = -1;
		return retval;
	}

	if (IsCreateAccountPage(page_number) || IsLoginPage(page_number))
	{
		return GetPageByName(GetFeatureSettingsPageName());
	}
	else if (IsFeatureIntroPage(page_number))
	{
		if (IsLoggedIn())
		{
			if (m_account_context && (m_account_context->IsValidUsername() || !m_account_context->IsAccountUsed()))
			{
				return GetPageByName(GetFeatureSettingsPageName());
			}
			else // old usernames that were valid for link but aren't any more for Unite
			{
				INT32 login_page = GetPageByName(GetLoginPageName());

				// make sure error is set
				const char* error_label_name = GetErrorLabelName(login_page);
				OpString error_text;
				g_languageManager->GetString(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_USERNAME, error_text);
				SetWidgetText(error_label_name, error_text.CStr());

				m_incorrect_input_field.Set("login_username_edit");
				OpWidget * widget = GetWidgetByName(m_incorrect_input_field.CStr());
				if (widget)
				{
					widget->GetBorderSkin()->SetState(SKINSTATE_ATTENTION);
				}

				return login_page;
			}
		}
		else if (IsUserCredentialsSet())
			return GetPageByName(GetLoginPageName());
		else
			return GetPageByName(GetCreateAccountPageName());
	}
	else if (IsFeatureSettingsPage(page_number))
	{
		OP_ASSERT(!"this shouldn't happen, there's no next page");
	}

	OP_ASSERT(!"you're on a page in this dialog that shouldn't exist!");
	return 0;
}

/***********************************************************************************
**  FeatureDialog::OnValidatePage
************************************************************************************/
/*virtual*/ BOOL
FeatureDialog::OnValidatePage(INT32 page_number)
{
	if (m_scheduled_page_idx != -1)
		return TRUE;

	if (IsFeatureIntroPage(page_number))
	{
		return TRUE;
	}

	if (!m_in_progress)
	{
		if (IsCreateAccountPage(page_number))
		{
			HandleCreateAccountAction();
		}
		else if (IsLoginPage(page_number))
		{
			HandleLoginAction();
		}
		else if (IsFeatureSettingsPage(page_number))
		{
			if (HasValidSettings())
			{
				m_in_progress = TRUE;
				if (m_needs_setup)
					EnableFeature();
				else
					ChangeFeatureSettings();
			}
		}
	}
	else // called internally
	{
		m_in_progress = FALSE;
		return TRUE;
	}
	return FALSE; // don't go to next page while in progress
}

/***********************************************************************************
**  FeatureDialog::IsLastPage
************************************************************************************/
/*virtual*/ BOOL
FeatureDialog::IsLastPage(INT32 page_number)
{
	OpWidget* page = GetPageByNumber(page_number);
	if (page->GetName().Compare(GetFeatureSettingsPageName()) == 0)
		return TRUE;
	else
		return FALSE;
}

/***********************************************************************************
**  FeatureDialog::OnInit
************************************************************************************/
/*virtual*/ void 
FeatureDialog::OnInit()
{
	InitTitle();
	
	// decide what page to show first
	if (m_needs_setup)
		SetCurrentPage(GetPageByName(GetFeatureIntroPageName()));
	else
	{
		if (!IsLoggedIn())
			SetCurrentPage(GetPageByName(GetLoginPageName()));
		else
		{
			if (m_account_context && (m_account_context->IsValidUsername() || !m_account_context->IsAccountUsed()))
			{
				SetCurrentPage(GetPageByName(GetFeatureSettingsPageName()));
			}
			else // old usernames that were valid for link but aren't any more for Unite
			{
				SetCurrentPage(GetPageByName(GetLoginPageName()));
				HandleError(AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS);
			}
		}
	}
	// listen to callbacks
	m_account_controller->AddListener(this);
	GetFeatureController()->AddListener(this);
}

/***********************************************************************************
**  FeatureDialog::OnInitPage
************************************************************************************/
/*virtual*/ void
FeatureDialog::OnInitPage(INT32 page_number, BOOL first_time)
{
	if (first_time)
	{
		if (IsFeatureIntroPage(page_number))
			InitIntroPage();
		else if (IsCreateAccountPage(page_number))
			InitCreateAccountPage();
		else if (IsLoginPage(page_number))
			InitLoginPage();
		else if (IsFeatureSettingsPage(page_number))
			InitSettingsPage();
		else
			OP_ASSERT(!"there should be no other page than the above");

		ResetDialog(); // needed for some reason to show/invalidate all texts set
	}
}

/***********************************************************************************
**  FeatureDialog::OnCancel
************************************************************************************/
/*virtual*/ void 
FeatureDialog::OnCancel()
{
	if (HasToRevertUserCredentials())
	{
		m_account_controller->SetLoggedIn(FALSE);
		m_account_controller->StopServices();
	}
}

/***********************************************************************************
**  FeatureDialog::OnChange
************************************************************************************/
/*virtual*/ void
FeatureDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT || 
		widget->GetType() == OpTypedObject::WIDGET_TYPE_CHECKBOX)
	{
		// Force state update of the refresh button.
		g_input_manager->UpdateAllInputStates();	
	}
	else
	{
		Dialog::OnChange(widget, changed_by_mouse);
	}
}

/***********************************************************************************
**  FeatureDialog::GetWindowName
************************************************************************************/
/*virtual*/ const char*
FeatureDialog::GetWindowName()
{
	return "Feature Dialog";
}

/***********************************************************************************
**  FeatureDialog::OnClose
************************************************************************************/
/*virtual*/ void
FeatureDialog::OnClose(BOOL user_initiated)
{
	// make sure that an outstanding password request isn't returned after the dialog is closed
	PrivacyManager::GetInstance()->RemoveCallback(this);
	
	m_account_controller->RemoveListener(this);
	GetFeatureController()->RemoveListener(this);
	
	Dialog::OnClose(user_initiated);
}

/***********************************************************************************
**  FeatureDialog::GetIsBlocking
************************************************************************************/
/*virtual*/ BOOL
FeatureDialog::GetIsBlocking()
{
	return m_blocking;
}

/***********************************************************************************
**  FeatureDialog::OnOperaAccountCreate
************************************************************************************/
/*virtual*/ void
FeatureDialog::OnOperaAccountCreate(OperaAuthError error, OperaRegistrationInformation& reg_info)
{
	if (error == AUTH_OK)
	{
		Login();
		return;
	}
	else
	{
		HandleError(error);
	}
}

/***********************************************************************************
**  FeatureDialog::OnOperaAccountAuth
************************************************************************************/
/*virtual*/ void
FeatureDialog::OnOperaAccountAuth(OperaAuthError error, const OpStringC &shared_secret)
{
	if (error == AUTH_OK)
	{
		OpString uname;
		OpString pword;

		BOOL remember_password = TRUE;
		BOOL save_credentials = TRUE;

		if (IsLoginPage(GetCurrentPage()))
		{
			GetWidgetText("login_username_edit", uname);
			GetWidgetText("login_password_edit", pword);

			// only remember password if checkbox is checked
			remember_password = (GetWidgetValue("login_remember_password") == TRUE);

			// only save credentials if they've changed
			OpString user_name;
			GetUsername(user_name);
			save_credentials = (user_name.Compare(uname) != 0 || (m_account_context && m_account_context->GetPassword().Compare(pword) != 0));
		}
		else if (IsCreateAccountPage(GetCurrentPage()))
		{
			GetWidgetText("new_username_edit", uname);
			GetWidgetText("new_password_edit", pword);
		}

		if(remember_password)
		{
			TRAPD(err, g_pcopera_account->WriteIntegerL(PrefsCollectionOperaAccount::SavePassword, TRUE));

			if (save_credentials)
			{
				m_account_controller->SaveCredentials(uname, pword);
			}
		}
		else
		{
			OpString empty;
			m_account_controller->SaveCredentials(uname, empty);
		}

		// make account manager aware of succeeded login
		m_account_controller->SetUsernameAndPassword(uname.CStr(), pword.CStr());
		m_account_controller->SetLoggedIn(TRUE);

		if (m_login_only)
		{
			if (m_retval)
				*m_retval = TRUE;

			CloseDialog(FALSE);
		}
		else
		{
			m_to_revert.login_uname.Set(uname.CStr());
			m_to_revert.login_pword.Set(pword.CStr());

			HideProgressInfo();
			// don't set m_in_progress here, you'll end up in a loop otherwise

			OpInputAction forward_action(OpInputAction::ACTION_FORWARD);
			OnInputAction(&forward_action);

			g_input_manager->UpdateAllInputStates();
		}
		return;
	}
	else
	{
		HandleError(error);
	}
}

/***********************************************************************************
**  FeatureDialog::OnFeatureEnablingSucceeded
************************************************************************************/
/*virtual*/ void
FeatureDialog::OnFeatureEnablingSucceeded()
{
	if (m_in_progress) // only listen to the event if you're waiting for it (see ALIEN-851)
	{
		if (m_needs_setup && !m_blocking)
		{
			GetFeatureController()->InvokeMessage(MSG_QUICK_FEATURE_SETUP_COMPLETED, (MH_PARAM_1)GetPresetFeatureSettings()->GetFeatureType(), (MH_PARAM_2)0);
		}

		if (m_blocking && m_retval)
			*m_retval = TRUE;
		CloseDialog(FALSE);
	}
}

/***********************************************************************************
**  FeatureDialog::OnFeatureSettingsChangeSucceeded
************************************************************************************/
/*virtual*/ void
FeatureDialog::OnFeatureSettingsChangeSucceeded()
{
	if (m_in_progress) // only listen to the event if you're waiting for it (see ALIEN-851)
	{
		if (m_blocking && m_retval)
			*m_retval = TRUE;
		CloseDialog(FALSE);
	}
}


/***********************************************************************************
**  FeatureDialog::InitTitle
************************************************************************************/
/*virtual*/ void
FeatureDialog::InitTitle()
{
	OpString feature_string;
	g_languageManager->GetString(GetPresetFeatureSettings()->GetFeatureStringID(), feature_string);

	OpString title_string;
	if (m_needs_setup)
		g_languageManager->GetString(Str::D_FEATURE_SETUP_TITLE, title_string);
	else
		g_languageManager->GetString(Str::D_FEATURE_SETTINGS_TITLE, title_string);

	OpString combined_string;
	combined_string.AppendFormat(title_string.CStr(), feature_string.CStr());

	SetTitle(combined_string.CStr());

}

/***********************************************************************************
**  FeatureDialog::InitCreateAccountPage
************************************************************************************/
/*virtual*/ void
FeatureDialog::InitCreateAccountPage()
{
	SetWidgetFocus("new_username_label", TRUE);

	OpLabel* title_label = static_cast<OpLabel*>(GetWidgetByName("account_title_label"));
	if (title_label)
	{
		title_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		title_label->SetRelativeSystemFontSize(150);
	}

	// put focus on username field
	OpEdit *username_edit = static_cast<OpEdit*>(GetWidgetByName("new_username_edit"));
	if (username_edit)
	{
		username_edit->SetMaxLength(OperaAccountContext::GetUsernameMaxLength());
		username_edit->SetFocus(FOCUS_REASON_OTHER);
	}
	// set up password fields
	OpEdit *password_edit = static_cast<OpEdit*>(GetWidgetByName("new_password_edit"));
	if (password_edit)
		password_edit->SetPasswordMode(TRUE);

	OpEdit *passwordretype_edit = static_cast<OpEdit*>(GetWidgetByName("password_retype_edit"));
	if (passwordretype_edit)
		passwordretype_edit->SetPasswordMode(TRUE);
	
	OpPasswordStrength* password_strength = static_cast<OpPasswordStrength*> (GetWidgetByName("password_strength_control"));
	if (password_strength && password_edit)
	{
		password_strength->AttachToEdit(password_edit);
	}
	
	// make error field multi-line, make it red
	OpLabel *error_label = static_cast<OpLabel*>(GetWidgetByName("account_error_label"));
	if (error_label)
	{
		error_label->SetWrap(TRUE);
		error_label->SetForegroundColor(OP_RGB(255, 0, 0));
	}

	// init progress widgets
	OpProgressBar * account_progress_spinner = static_cast<OpProgressBar *>(GetWidgetByName("account_progress_spinner"));
	if (account_progress_spinner)
	{
		account_progress_spinner->SetType(OpProgressBar::Spinner);
		account_progress_spinner->SetVisibility(FALSE);
	}
	OpLabel* progress_label = static_cast<OpLabel*>(GetWidgetByName("account_progress_label"));
	if (progress_label)
		progress_label->SetWrap(TRUE);

	OpEdit* email_edit = GetWidgetByName<OpEdit>("email_edit", WIDGET_TYPE_EDIT);
	if (email_edit)
		email_edit->SetForceTextLTR(TRUE);

	const char* star_label_names[] =
	{
		"new_username_slabel",
		"new_password_slabel",
		"password_retype_slabel",
		"email_slabel",
		"license_checkbox_slabel",
		"account_required_slabel",
	};

	// make stars red and right-aligned
	for (unsigned i = 0; i < ARRAY_SIZE(star_label_names); i++)
	{
		OpLabel* star_label = GetWidgetByName<OpLabel>(star_label_names[i], WIDGET_TYPE_LABEL);
		if (star_label)
		{
			star_label->SetJustify(star_label->GetRTL() ? JUSTIFY_LEFT : JUSTIFY_RIGHT, FALSE);
			star_label->SetForegroundColor(OP_RGB(255, 0, 0)); // todo: put color in skin
		}
	}
}

/***********************************************************************************
**  FeatureDialog::InitLoginPage
************************************************************************************/
/*virtual*/ void
FeatureDialog::InitLoginPage()
{
	OpLabel* title_label = static_cast<OpLabel*>(GetWidgetByName("login_account_title_label"));
	if (title_label)
	{
		title_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		title_label->SetRelativeSystemFontSize(150);
	}

	OpEdit *username_edit = static_cast<OpEdit*>(GetWidgetByName("login_username_edit"));
	if (username_edit)
	{
		username_edit->SetMaxLength(OperaAccountContext::GetUsernameMaxLength());
		username_edit->SetFocus(FOCUS_REASON_OTHER);
	}

	// set password mode
	OpEdit *password_edit = static_cast<OpEdit*>(GetWidgetByName("login_password_edit"));
	if (password_edit)
		password_edit->SetPasswordMode(TRUE);



	// make error field multi-line, make it red
	OpLabel *error_label = static_cast<OpLabel*>(GetWidgetByName("login_error_label"));
	if (error_label)
	{
		error_label->SetWrap(TRUE);
		error_label->SetForegroundColor(OP_RGB(255, 0, 0)); // todo: put color in skin
	}

	// init progress widgets
	OpProgressBar * login_progress_spinner = static_cast<OpProgressBar *>(GetWidgetByName("login_progress_spinner"));
	if (login_progress_spinner)
	{
		login_progress_spinner->SetType(OpProgressBar::Spinner);
		login_progress_spinner->SetVisibility(FALSE);
	}
	OpLabel* progress_label = static_cast<OpLabel*>(GetWidgetByName("login_progress_label"));
	if (progress_label)
		progress_label->SetWrap(TRUE);

	SetWidgetValue("login_remember_password", m_password_saved);

	// pre-fill form: always fill in username, if present

	OpString username;
	GetUsername(username);
	if (username.HasContent())
	{
		OpString password;
		if (m_password_saved)
		{
			if (m_account_context != NULL && !m_account_context->GetPassword().IsEmpty())
			{
				SetUsernamePasswordNoOnchange(username, m_account_context->GetPassword());
			}
			else
			{
				// Read from Wand.
				g_privacy_manager->GetPassword(WAND_OPERA_ACCOUNT, UNI_L(""), username, this);
			}
		}
		else
		{
			// set username only
			SetUsernamePasswordNoOnchange(username, UNI_L(""));
		}
	}

	const char* s_star_label_names[] =
	{
		"login_username_slabel",
		"login_password_slabel",
		"login_required_slabel",
		NULL
	};

	// make stars red and right-aligned
	OpLabel* star_label;
	for (UINT32 i = 0; s_star_label_names[i]; i++)
	{
		star_label = static_cast<OpLabel*>(GetWidgetByName(s_star_label_names[i]));
		if (star_label)
		{
			star_label->SetJustify(JUSTIFY_RIGHT, FALSE);
			star_label->SetForegroundColor(OP_RGB(255, 0, 0)); // todo: put color in skin
		}
	}
	
	ShowWidget("login_change_username", FALSE);
}

/***********************************************************************************
**  FeatureDialog::IsLoggedIn
************************************************************************************/
BOOL FeatureDialog::IsLoggedIn() const
{
	return (m_account_context != NULL && (m_account_context->IsLoggedIn()));
}

/***********************************************************************************
**  FeatureDialog::IsUserCredentialsSet
************************************************************************************/
BOOL
FeatureDialog::IsUserCredentialsSet() const
{
	// don't care about the password, it might come later from Wand
	return (m_account_context != NULL && m_account_context->GetUsername().HasContent());
}

/***********************************************************************************
**  FeatureDialog::Login
************************************************************************************/
void
FeatureDialog::Login()
{
	OpString username;
	OpString password;

	INT32 page_number = GetCurrentPage();

	if (IsLoginPage(page_number))
	{
		GetWidgetText("login_username_edit", username);
		GetWidgetText("login_password_edit", password);

		if(GetWidgetValue("login_remember_password") == FALSE)
		{
#ifdef PREFS_CAP_ACCOUNT_SAVE_PASSWORD
			TRAPD(err, g_pcopera_account->WriteIntegerL(PrefsCollectionOperaAccount::SavePassword, FALSE));	
#endif // PREFS_CAP_ACCOUNT_SAVE_PASSWORD
			if (m_account_context && m_account_context->GetUsername().HasContent())
			{
				// forget the password
				OpString empty;
				m_account_controller->SaveCredentials(m_account_context->GetUsername(), empty);
			}
		}
	}
	else if (IsCreateAccountPage(page_number))
	{
		GetWidgetText("new_username_edit", username);
		GetWidgetText("new_password_edit", password);
	}
	else
	{
		OP_ASSERT(!"can't login when not on login or create account page");
	}
	 // don't log in if already logged in with same user
	if (HasToRevertUserCredentials() &&
		m_to_revert.login_uname.Compare(username) == 0 &&
		m_to_revert.login_pword.Compare(password) == 0)
	{
		OnOperaAccountAuth(AUTH_OK, NULL);
		return;
	}

	// check if logging in with different username
	if (m_account_context && m_account_context->GetUsername().HasContent() && m_account_context->GetUsername().Compare(username) != 0)
	{
		if (!SwitchingUserWanted())
		{
			SetInProgress(FALSE);
			return;
		}
	}

	ShowProgressInfo(Str::D_FEATURE_SETUP_LOGGING_IN);

	// if different login, logout beforehand
	if (IsLoggedIn() || 
		(HasToRevertUserCredentials() && (m_to_revert.login_uname != username || m_to_revert.login_pword != password)))
	{
		m_account_controller->SetLoggedIn(FALSE);
		m_account_controller->StopServices();
	}
	// reset fields
	m_to_revert.login_uname.Empty();
	m_to_revert.login_pword.Empty();

	// login
	if (m_account_controller)
	{
		m_account_controller->Authenticate(username, password);
	}
}

/***********************************************************************************
**  FeatureDialog::CreateAccount
************************************************************************************/
void
FeatureDialog::CreateAccount()
{
	ShowProgressInfo(Str::D_FEATURE_SETUP_CREATING_ACCOUNT);

	OperaRegistrationInformation reg_info;
	GetWidgetText("new_username_edit", reg_info.m_username);
	GetWidgetText("new_password_edit", reg_info.m_password);
	GetWidgetText("email_edit", reg_info.m_email);
	reg_info. m_registration_type = OperaRegNormal;

	m_account_controller->CreateAccount(reg_info);
}

/***********************************************************************************
**  FeatureDialog::EnableFeature
************************************************************************************/
void
FeatureDialog::EnableFeature()
{
	OpString enabling_string;
	OpString feature_string;
	OpString merged_string;

	g_languageManager->GetString(Str::D_FEATURE_SETUP_ENABLING_FEATURE, enabling_string);
	g_languageManager->GetString(GetPresetFeatureSettings()->GetFeatureStringID(), feature_string);
	merged_string.AppendFormat(enabling_string.CStr(), feature_string.CStr());

	ShowProgressInfo(merged_string);
	GetFeatureController()->EnableFeature(ReadCurrentFeatureSettings());
}

/***********************************************************************************
**  FeatureDialog::ChangeFeatureSettings
************************************************************************************/
void
FeatureDialog::ChangeFeatureSettings()
{
	FeatureSettingsContext * current_settings = ReadCurrentFeatureSettings();

	// if feature is about to be disabled, close dialog immediately, don't show progress text
	if (current_settings->IsFeatureEnabled())
	{
		// show progress
		OpString enabling_string;
		OpString feature_string;
		OpString merged_string;

		g_languageManager->GetString(Str::D_FEATURE_SETTINGS_CHANGING, enabling_string);
		g_languageManager->GetString(GetPresetFeatureSettings()->GetFeatureStringID(), feature_string);
		merged_string.AppendFormat(enabling_string.CStr(), feature_string.CStr());
		ShowProgressInfo(merged_string);
	}

	GetFeatureController()->SetFeatureSettings(current_settings);
	
	// if feature is about to be disabled, close dialog immediately, don't wait for response
	if (!current_settings->IsFeatureEnabled())
	{
		CloseDialog(FALSE);
	}
}

/***********************************************************************************
**  FeatureDialog::HasValidNewAccountInfo
************************************************************************************/
BOOL
FeatureDialog::HasValidInput(INT32 page_number)
{
	if (IsFeatureIntroPage(page_number))
		return TRUE;

	else if (IsCreateAccountPage(page_number))
		return HasValidNewAccountInfo();

	else if (IsLoginPage(page_number))
		return HasValidLoginInfo();

	else if (IsFeatureSettingsPage(page_number))
		return HasValidSettings();

	else
	{
		OP_ASSERT(!"this page shouldn't be used");
		return FALSE;
	}
}

/***********************************************************************************
**  FeatureDialog::HasValidNewAccountInfo
************************************************************************************/
/*virtual*/ BOOL
FeatureDialog::HasValidNewAccountInfo() // only checking what needs to be valid for enabling 'Next' button
{
	const char * required_fields[] = { "new_username_edit", "new_password_edit", "password_retype_edit", "email_edit"};
	UINT32 required_fields_count = ARRAY_SIZE(required_fields);

	OpString edit_text;
	for (UINT32 i = 0; i < required_fields_count; i++)
	{
		GetWidgetText(required_fields[i], edit_text, TRUE);
		if (edit_text.IsEmpty()) // must have content
		{
			return FALSE;
		}
	}
	return GetWidgetValue("license_checkbox", 0); // must be checked
}

/***********************************************************************************
**  FeatureDialog::HasValidLoginInfo
************************************************************************************/
/*virtual*/ BOOL
FeatureDialog::HasValidLoginInfo()
{
	OpString edit_text;

	GetWidgetText("login_username_edit", edit_text, TRUE);
	if (edit_text.IsEmpty()) // must have content
	{
		return FALSE;
	}

	GetWidgetText("login_password_edit", edit_text, TRUE);
	if (edit_text.IsEmpty()) // must have content
	{
		return FALSE;
	}

	return TRUE;
}

/***********************************************************************************
**  FeatureDialog::HandleError
************************************************************************************/
void
FeatureDialog::HandleError(OperaAuthError error)
{
	OP_ASSERT(error != AUTH_OK);

	HideProgressInfo();
	SetInProgress(FALSE);
	
	if (m_incorrect_input_field.HasContent())
	{
		OpWidget * widget = GetWidgetByName(m_incorrect_input_field.CStr());
		if (widget)
		{
			widget->GetBorderSkin()->SetState(0);
		}
		m_incorrect_input_field.Empty();
	}
	BOOL is_login = IsLoginPage(GetCurrentPage());

	switch (error)
	{
		case AUTH_ACCOUNT_CREATE_USER_EMAIL_ERROR:
			m_incorrect_input_field.Set("email_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_EMAIL);
			break;

        case AUTH_ACCOUNT_CREATE_USER_EMAIL_EXISTS:
			m_incorrect_input_field.Set("email_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_EMAIL_EXISTS);
			break;

        case AUTH_ACCOUNT_CREATE_USER_TOO_SHORT:
			m_incorrect_input_field.Set("new_username_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_USERNAME_TOO_SHORT);
			break;
			
        case AUTH_ACCOUNT_CREATE_USER_TOO_LONG:
			m_incorrect_input_field.Set("new_username_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_USERNAME_TOO_LONG);
			break;
			
        case AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS:
			if (is_login)
			{
				m_incorrect_input_field.Set("login_username_edit");
			}
			else
			{
				m_incorrect_input_field.Set("new_username_edit");
			}
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_USERNAME);
			break;
			
		case AUTH_ACCOUNT_USER_UNAVAILABLE:
			if (is_login)
			{
				m_incorrect_input_field.Set("login_username_edit");
			}
			else
			{
				m_incorrect_input_field.Set("new_username_edit");
			}
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_USER_UNAVAILABLE);
			break;

		case AUTH_ACCOUNT_USER_BANNED:
			m_incorrect_input_field.Set("login_username_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_USER_BANNED);
			break;

		case AUTH_ACCOUNT_AUTH_FAILURE:
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_CREDENTIALS);
			break;

		case AUTH_ACCOUNT_CREATE_USER_EXISTS:
			m_incorrect_input_field.Set("new_username_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_USER_EXISTS); 
			break;

		case AUTH_ERROR_COMM_TIMEOUT:	// connection timed out
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_CONNECTION_TIMEOUT);
			break;

		case AUTH_ERROR_COMM_ABORTED:	// connection/transfer aborted
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_CONNECTION_ABORTED);
			break;

	    case AUTH_ERROR_COMM_FAIL:	// transfer is done, but connection failed
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_CONNECTION_FAILED);
			break;

		case AUTH_ERROR_MEMORY:
			ShowError(Str::SI_ERR_OUT_OF_MEMORY);
			break;

		case AUTH_ERROR_PARSER:		// server xml can't be parsed
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_RESPONSE);
			break;

       case AUTH_ACCOUNT_AUTH_INVALID_KEY:	// we sent the wrong key
       case AUTH_ERROR:			// default error returned
       case AUTH_ERROR_SERVER:	// never called, it seems
		default:
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_DEFAULT);
			break;
	}
	if (m_incorrect_input_field.HasContent())
	{
		OpWidget * widget = GetWidgetByName(m_incorrect_input_field.CStr());
		if (widget)
		{
			widget->GetBorderSkin()->SetState(SKINSTATE_ATTENTION);
		}
	}
}

/***********************************************************************************
**  FeatureDialog::ShowError
************************************************************************************/
void
FeatureDialog::ShowError(Str::LocaleString error_string_ID)
{
	const char* error_label_name = GetErrorLabelName(GetCurrentPage());

	OpString error_text;
	g_languageManager->GetString(error_string_ID, error_text);

	OpString text;
	GetWidgetText(error_label_name, text);
	
	if (text.Compare(error_text) != 0)
	{
		SetWidgetText(error_label_name, error_text.CStr());
		ResetDialog();
	}
}

/***********************************************************************************
**  FeatureDialog::HideError
************************************************************************************/
void 
FeatureDialog::HideError()
{
	const char* error_label_name = GetErrorLabelName(GetCurrentPage());

	OpString text;
	GetWidgetText(error_label_name, text);
	
	if (!text.IsEmpty())
	{
		SetWidgetText(error_label_name, UNI_L(""));
		ShowWidget("login_change_username", FALSE);
		ResetDialog();
	}
}

/***********************************************************************************
**  FeatureDialog::SetInProgress
************************************************************************************/
void
FeatureDialog::SetInProgress(BOOL in_progress)
{
	if (m_in_progress != in_progress)
	{
		m_in_progress = in_progress;
		g_input_manager->UpdateAllInputStates();
	}
}

/***********************************************************************************
**  FeatureDialog::ShowProgressInfo
************************************************************************************/
void
FeatureDialog::ShowProgressInfo(Str::LocaleString progress_info_ID)
{
	OpString process_text;
	g_languageManager->GetString(progress_info_ID, process_text);
	ShowProgressInfo(process_text);
}

/***********************************************************************************
**  Shows a progress bar in text-only mode. The way it works now, it doesn't work
**  with a full progress bar, because it can't be hidden.
**
**  FeatureDialog::ShowProgressInfo
************************************************************************************/
void
FeatureDialog::ShowProgressInfo(const OpStringC & progress_info_string)
{
	SetInProgress(TRUE);

	HideError();
	ShowWidget(GetProgressSpinnerName(GetCurrentPage()), TRUE);

	const char* progress_label_name = GetProgressLabelName(GetCurrentPage());

	OpString text;
	GetWidgetText(progress_label_name, text);
	
	if (text.Compare(progress_info_string) != 0)
	{
		SetWidgetText(progress_label_name, progress_info_string.CStr());
		ResetDialog();
	}
}

/***********************************************************************************
**  Clears text on progress bar. Hiding it doesn't work otherwise.
**
**  FeatureDialog::HideProgressInfo
************************************************************************************/
void
FeatureDialog::HideProgressInfo()
{
	ShowWidget(GetProgressSpinnerName(GetCurrentPage()), FALSE);

	const char* progress_label_name = GetProgressLabelName(GetCurrentPage());

	OpString text;
	GetWidgetText(progress_label_name, text);
	
	if (!text.IsEmpty())
	{
		SetWidgetText(progress_label_name, UNI_L(""));
		ResetDialog();
	}
}

/***********************************************************************************
**  FeatureDialog::HasToRevertUserCredentials
************************************************************************************/
BOOL
FeatureDialog::HasToRevertUserCredentials()
{
	return (m_to_revert.login_uname.HasContent() && m_to_revert.login_pword.HasContent());
}

/***********************************************************************************
**  FeatureDialog::ShowLicenseDialog
************************************************************************************/
void
FeatureDialog::ShowLicenseDialog()
{
	FeatureLicenseDialog* dialog = OP_NEW(FeatureLicenseDialog, ());
	if (dialog)
		dialog->Init(this);
}

/***********************************************************************************
**  FeatureDialog::GetPageByName
************************************************************************************/
INT32
FeatureDialog::GetPageByName(const OpStringC8 & name)
{
	for (UINT32 i = 0; i < GetPageCount(); i++)
	{
		OpWidget* page = GetPageByNumber(i);
		if (page->GetName().Compare(name) == 0)
			return i;
	}
	return 0;
}

/***********************************************************************************
**  FeatureDialog::GetCreateAccountPageName
************************************************************************************/
/*virtual*/ const char*
FeatureDialog::GetCreateAccountPageName() const
{
	return "create_account_page";
}

/***********************************************************************************
**  FeatureDialog::GetLoginPageName
************************************************************************************/
/*virtual*/ const char*
FeatureDialog::GetLoginPageName() const
{
	return "login_page";
}

/***********************************************************************************
**	FeatureDialog::IsCreateAccountPage
***********************************************************************************/
BOOL
FeatureDialog::IsCreateAccountPage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetCreateAccountPageName()) == 0);
}

/***********************************************************************************
**	FeatureDialog::IsLoginPage
***********************************************************************************/
BOOL
FeatureDialog::IsLoginPage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetLoginPageName()) == 0);
}

/***********************************************************************************
**	FeatureDialog::IsFeatureIntroPage
***********************************************************************************/
BOOL
FeatureDialog::IsFeatureIntroPage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetFeatureIntroPageName()) == 0);
}

/***********************************************************************************
**	FeatureDialog::IsFeatureSettingsPage
***********************************************************************************/
BOOL
FeatureDialog::IsFeatureSettingsPage(INT32 page_number) const
{
	return (GetPageByNumber(page_number)->GetName().Compare(GetFeatureSettingsPageName()) == 0);
}

/***********************************************************************************
**	FeatureDialog::SetLicenseAccepted
***********************************************************************************/
void 
FeatureDialog::SetLicenseAccepted(BOOL is_accepted)
{
	OpCheckBox* license_checkbox = static_cast<OpCheckBox*>(GetWidgetByName("license_checkbox"));
	if (license_checkbox)
		license_checkbox->SetValue(is_accepted);
}

/***********************************************************************************
**	FeatureDialog::GoToTargetPage
***********************************************************************************/
void
FeatureDialog::GoToTargetPage(INT32 target_page)
{
	if (m_previous_page_idx != -1 && m_previous_page_idx == target_page)
	{
		OpInputAction back_action(OpInputAction::ACTION_BACK);
		OnInputAction(&back_action);
	}
	else
	{
		m_scheduled_page_idx = target_page;
		OpInputAction forward_action(OpInputAction::ACTION_FORWARD);
		OnInputAction(&forward_action);
	}
}

/***********************************************************************************
**	FeatureDialog::GetErrorLabelName
***********************************************************************************/
const char*
FeatureDialog::GetErrorLabelName(INT32 page_number) const
{
	if (IsCreateAccountPage(page_number))
		return "account_error_label";
	else if (IsLoginPage(page_number))
		return "login_error_label";
	else if (IsFeatureSettingsPage(page_number))
		return GetFeatureSettingsErrorLabelName();
	else
	{
		OP_ASSERT(!"incorrect page");
		return "";
	}
}

/***********************************************************************************
**	FeatureDialog::GetProgressSpinnerName
***********************************************************************************/
const char*
FeatureDialog::GetProgressSpinnerName(INT32 page_number) const
{
	if (IsCreateAccountPage(page_number))
		return "account_progress_spinner";
	else if (IsLoginPage(page_number))
		return "login_progress_spinner";
	else if (IsFeatureIntroPage(page_number))
		return GetFeatureIntroProgressSpinnerName();
	else if (IsFeatureSettingsPage(page_number))
		return GetFeatureSettingsProgressSpinnerName();
	else
	{
		OP_ASSERT(!"incorrect page");
		return "";
	}
}

/***********************************************************************************
**	FeatureDialog::GetProgressLabelName
***********************************************************************************/
const char*
FeatureDialog::GetProgressLabelName(INT32 page_number) const
{
	if (IsCreateAccountPage(page_number))
		return "account_progress_label";
	else if (IsLoginPage(page_number))
		return "login_progress_label";
	else if (IsFeatureIntroPage(page_number))
		return GetFeatureIntroProgressLabelName();
	else if (IsFeatureSettingsPage(page_number))
		return GetFeatureSettingsProgressLabelName();
	else
	{
		OP_ASSERT(!"incorrect page");
		return "";
	}
}

/***********************************************************************************
**	FeatureDialog::HandleCreateAccountAction
***********************************************************************************/
void
FeatureDialog::HandleCreateAccountAction()
{
	if (m_incorrect_input_field.HasContent())
	{
		OpWidget * widget = GetWidgetByName(m_incorrect_input_field.CStr());
		if (widget)
		{
			widget->GetBorderSkin()->SetState(0);
		}
	}

	OperaAccountContext account_context; // temp account context used for validation
	OpString	text;
	BOOL		error_found = FALSE;
	OpEdit*		edit;
	
	if (!error_found)
	{
		edit = static_cast<OpEdit*>(GetWidgetByName("new_username_edit"));
		edit->GetText(text);
		account_context.SetUsername(text);
		if (account_context.IsUsernameTooShort())
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("new_username_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_USERNAME_TOO_SHORT);
		}
		else if (!account_context.IsValidUsername())
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("new_username_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_USERNAME);
		}
	}
	if (!error_found)
	{
		edit = static_cast<OpEdit*>(GetWidgetByName("new_password_edit"));
		edit->GetText(text);
		account_context.SetPassword(text);
		if (!account_context.IsValidPassword())
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("new_password_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_PASSWORD);
		}
	}
	if (!error_found)
	{
		edit = static_cast<OpEdit*>(GetWidgetByName("password_retype_edit"));
		OpString password_text;
		GetWidgetText("new_password_edit", password_text);
		edit->GetText(text);
		if (password_text.Compare(text) != 0)
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("password_retype_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_PASSWORD_RETYPE);
		}
	}
	if (!error_found)
	{
		edit = static_cast<OpEdit*>(GetWidgetByName("email_edit"));
		edit->GetText(text);
		account_context.SetEmailAddress(text);
		if (!account_context.IsValidEmailAddress())
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("email_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_EMAIL);
		}
	}	

	if (error_found)
	{
		OP_ASSERT(m_incorrect_input_field.HasContent());

		OpWidget * widget = GetWidgetByName(m_incorrect_input_field.CStr());
		if (widget)
		{
			widget->GetBorderSkin()->SetState(SKINSTATE_ATTENTION);
		}
	}
	else
	{
		m_in_progress = TRUE;
		CreateAccount();
	}
}

/***********************************************************************************
**	FeatureDialog::HandleLoginAction
***********************************************************************************/
void 
FeatureDialog::HandleLoginAction()
{
	if (m_incorrect_input_field.HasContent())
	{
		OpWidget * widget = GetWidgetByName(m_incorrect_input_field.CStr());
		if (widget)
		{
			widget->GetBorderSkin()->SetState(0);
		}
	}

	OperaAccountContext account_context; // temp account context used for validation
	OpString	text;
	BOOL		error_found = FALSE;
	OpEdit*		edit;
	
	if (!error_found)
	{
		edit = static_cast<OpEdit*>(GetWidgetByName("login_username_edit"));
		if (edit)
		{
			edit->GetText(text);
			account_context.SetUsername(text);
		}
		if (account_context.IsUsernameTooShort())
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("new_username_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_USERNAME_TOO_SHORT);
		}
		else if (!account_context.IsValidUsername())
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("login_username_edit");

			OpButton* button = (OpButton*)GetWidgetByName("login_change_username");
			if(button)
			{
				OpString address;
				if(GetPresetFeatureSettings())
				{
					if(GetPresetFeatureSettings()->GetFeatureType() == FeatureTypeWebserver)
					{
						address.Set(UNI_L("http://redir.opera.com/rename-user/?service=OperaUnite&username="));
					}
					else if(GetPresetFeatureSettings()->GetFeatureType() == FeatureTypeSync)
					{
						address.Set(UNI_L("http://redir.opera.com/rename-user/?service=OperaLink&username="));
					}
				}
				if(address.HasContent())
				{
#ifdef FORMATS_URI_ESCAPE_SUPPORT
					UriEscape::AppendEscaped(address, text.CStr(), UriEscape::AllUnsafe);
#else
					address.Append(text);
#endif // FORMATS_URI_ESCAPE_SUPPORT
					OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE));
					if(action)
					{
						action->SetActionDataString(address.CStr());
						button->SetAction(action);
						ShowWidget("login_change_username", TRUE);
					}
				}
			}
			
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_USERNAME);
		}
	}
	if (!error_found)
	{
		edit = static_cast<OpEdit*>(GetWidgetByName("login_password_edit"));
		edit->GetText(text);
		account_context.SetPassword(text);
		if (!account_context.IsValidPassword())
		{
			error_found = TRUE;
			m_incorrect_input_field.Set("login_password_edit");
			ShowError(Str::D_FEATURE_ACCOUNT_ERROR_INVALID_PASSWORD);
		}
	}

	if (error_found)
	{
		OP_ASSERT(m_incorrect_input_field.HasContent());

		OpWidget * widget = GetWidgetByName(m_incorrect_input_field.CStr());
		if (widget)
		{
			widget->GetBorderSkin()->SetState(SKINSTATE_ATTENTION);
		}
	}
	else
	{
		m_in_progress = TRUE;
		Login();
	}
}

OP_STATUS
FeatureDialog::GetUsername(OpString & username)
{
	if (m_account_context != NULL && !m_account_context->GetUsername().IsEmpty())
	{
		// username is in memory
		RETURN_IF_ERROR(username.Set(m_account_context->GetUsername()));
	}
	else
	{
		RETURN_IF_ERROR(username.Set(g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::Username)));
	}
	return OpStatus::OK;
}


void FeatureDialog::SetUsernamePasswordNoOnchange(const OpStringC& username, const OpStringC& password)
{
	// force no onchange,since onchange of username/password will invalidate saved crendential

	OpEdit* username_edit = (OpEdit*) GetWidgetByName("login_username_edit");
	OpEdit* pass_edit = (OpEdit*) GetWidgetByName("login_password_edit");

	if(username_edit && pass_edit)
	{
		if (username.HasContent())
		{
			username_edit->SetText(username.CStr(), TRUE);
		}
		if (password.HasContent())
		{
			pass_edit->SetText(password.CStr(), TRUE);
		}
		// Force state update of the buttons
		g_input_manager->UpdateAllInputStates();
	}	

}

void FeatureDialog::OnPasswordRetrieved(const OpStringC& password)
{
	if (m_account_context)
	{
		m_account_context->SetPassword(password);
	}
	OpStringC username = g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::Username);
	if (username.HasContent())
	{
		SetUsernamePasswordNoOnchange(username, password);
	}
}

void FeatureDialog::OnPasswordFailed()
{
	OpStringC username = g_pcopera_account->GetStringPref(PrefsCollectionOperaAccount::Username);
	if (username.HasContent())
	{
		SetUsernamePasswordNoOnchange(username, UNI_L(""));
	}
}
