// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton, Karianne Ekern
//
#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/webserver/view/WebServerSetupWizard.h"
#include "adjunct/quick/webserver/view/WebServerStatusDialog.h"

#include "modules/opera_auth/opera_auth_myopera.h"
#include "modules/console/opconsoleengine.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#if defined(MSWIN)
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////
void
WebServerUIConnector::SetConfiguring(BOOL configuring)
{
	if (!configuring) {
		m_configure_style = NotConfiguring;
		m_configure_dialog = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
BOOL WebServerUIConnector::IsConfiguring()
{
	return m_configure_style != NotConfiguring;
}

/***********************************************************************************
 *
 *  WebServerUIConnector::ShowSetupWizard
 *
 ************************************************************************************/
OP_STATUS
WebServerUIConnector::ShowSetupWizard(WebServerManager * manager, BOOL * webserver_enabled, const WebServerSettingsContext & settings_context,
						OperaAccountController * account_controller, OperaAccountContext* account_context)
{
	if (m_configure_style == SetupWizard) {
		m_configure_dialog->Activate();
		return OpStatus::OK;
	}

	if (account_context && !UserWantsInvalidUsernamesChange(account_context))
	{
		return OpStatus::OK;
	}

	WebServerDialog *wizard = OP_NEW(WebServerDialog, (manager,
		&settings_context,
		account_controller,
		account_context,
		TRUE));

	if (wizard)
	{
		if (webserver_enabled)
			wizard->SetBlockingForReturnValue(webserver_enabled);

		m_configure_dialog = wizard;
		m_configure_style = SetupWizard;
		// blocking dialog, so don't init m_configure_dialog/m_configure_style after that
		RETURN_IF_ERROR(wizard->Init(g_application->GetActiveDesktopWindow()));

		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
 *  WebServerUIConnector::ShowSettingsDialog
 ************************************************************************************/
OP_STATUS
WebServerUIConnector::ShowSettingsDialog(WebServerManager * manager, const WebServerSettingsContext & settings_context,
						OperaAccountController * account_controller, OperaAccountContext* account_context)
{
	if (m_configure_style == SettingsDialog) {
		m_configure_dialog->Activate();
		return OpStatus::OK;
	}

	if (account_context && !UserWantsInvalidUsernamesChange(account_context))
	{
		return OpStatus::OK;
	}

	WebServerDialog* dialog = OP_NEW(WebServerDialog, (manager,
		&settings_context,
		account_controller,
		account_context,
		FALSE));

	if (dialog)
	{
		m_configure_dialog = dialog;
		m_configure_style = SettingsDialog;
		// blocking dialog, so don't init m_configure_dialog/m_configure_style after that
		dialog->Init(g_application->GetActiveDesktopWindow());
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
 *  WebServerUIConnector::ShowStatus
 ************************************************************************************/
OP_STATUS
WebServerUIConnector::ShowStatus(WebServerManager * manager)
{
	WebServerStatusDialog* dialog = OP_NEW(WebServerStatusDialog, (*manager));

	if (dialog)
	{
		dialog->Init(g_application->GetActiveDesktopWindow());
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
 *  WebServerUIConnector::ShowLoginDialog
 ************************************************************************************/
OP_STATUS
WebServerUIConnector::ShowLoginDialog(WebServerManager * manager, const WebServerSettingsContext & settings_context,
						OperaAccountController * account_controller, OperaAccountContext* account_context, BOOL * logged_in)
{
	if (m_configure_style == LoginDialog) {
		m_configure_dialog->Activate();
		return OpStatus::OK;
	}
	WebServerDialog* wizard = OP_NEW(WebServerDialog, (manager,
		&settings_context,
		account_controller,
		account_context,
		FALSE));

	if (wizard)
	{
		m_configure_dialog = wizard;
		m_configure_style = LoginDialog;
		// blocking dialog, so don't init m_configure_dialog/m_configure_style after that
		RETURN_IF_ERROR(wizard->InitLoginDialog(logged_in, g_application->GetActiveDesktopWindow()));
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*virtual*/ BOOL
WebServerUIConnector::UserWantsFeatureEnabling(DesktopWindow * parent_window)
{
	OpString dialog_text;
	g_languageManager->GetString(Str::D_WEBSERVER_ENABLE_QUESTION, dialog_text);

	OpString dialog_title;
	g_languageManager->GetString(Str::D_WEBSERVER_ENABLE_QUESTION_TITLE, dialog_title);

	return DialogResult(dialog_title, dialog_text, parent_window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*virtual*/ BOOL
WebServerUIConnector::UserWantsInvalidUsernamesChange(OperaAccountContext * account_context, DesktopWindow * parent_window)
{
	if (!account_context || !account_context->IsAccountUsed())
	{
		return TRUE;
	}

	if (account_context->IsLoggedIn() && !account_context->IsValidUsername())
	{
		OpString dialog_text;
		g_languageManager->GetString(Str::D_INVALID_USER_SWITCH_QUESTION, dialog_text);

		OpString dialog_title;
		g_languageManager->GetString(Str::D_INVALID_USER_SWITCH_QUESTION_TITLE, dialog_title);

		if (!DialogResult(dialog_title, dialog_text, parent_window))
		{
			return FALSE;
		}
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ BOOL
WebServerUIConnector::UserWantsDeviceTakeover(const OpStringC & device_name, DesktopWindow * parent_window)
{
	OpString release_text, dialog_text;
	g_languageManager->GetString(Str::D_WEBSERVER_RELEASE_DEVICE_NAME_IN_USE, release_text);
	dialog_text.AppendFormat(release_text.CStr(), device_name.CStr());

	OpString dialog_title;
	g_languageManager->GetString(Str::D_WEBSERVER_RELEASE_DEVICE_TITLE, dialog_title);

	return DialogResult(dialog_title, dialog_text, parent_window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ BOOL
WebServerUIConnector::UserWantsReconnectAfterTakeover(DesktopWindow * parent_window)
{
	OpString dialog_text;
	g_languageManager->GetString(Str::D_WEBSERVER_DISCONNECTED_TEXT, dialog_text);

	OpString dialog_title;
	g_languageManager->GetString(Str::D_WEBSERVER_DISCONNECTED_TITLE, dialog_title);

	return DialogResult(dialog_title, dialog_text, parent_window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerUIConnector::OnWebServerIsInsecure(DesktopWindow * parent_window)
{
#ifdef AUTO_UPDATE_SUPPORT
	// add listener, so that "no update available" can be handled
	// (if the webserver has been disabled without the user knowing)
	if (!g_webserver_manager->IsFeatureEnabled())
	{
		OpStatus::Ignore(g_autoupdate_manager->AddListener(this));
	}
#endif // AUTO_UPDATE_SUPPORT

	OnWebServerIsOutdated(parent_window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerUIConnector::OnWebServerIsOutdated(DesktopWindow * parent_window)
{
#ifdef AUTO_UPDATE_SUPPORT
	// if the webserver is outdated, just trigger the update
	if (OpStatus::IsError(g_autoupdate_manager->CheckForUpdate(FALSE)))
#endif // AUTO_UPDATE_SUPPORT
	{
		if (IsConfiguring() && m_configure_dialog) // close any other Unite setup/settings dialog
		{
			m_configure_dialog->Close();
			SetConfiguring(FALSE);
		}
		// show that the webserver is outdated
		ShowStatus(g_webserver_manager);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
WebServerUIConnector::HandleErrorInDialog(OperaAuthError error)
{
	if (m_configure_dialog)
	{
		m_configure_dialog->HandleError(error);
		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(!"there is no dialog to show the error in!");
		return OpStatus::ERR;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*virtual*/ void
WebServerUIConnector::LogError(WebserverStatus status, BOOL show_as_warning)
{
	OpString context;
	INT32 error_code = OAM_WEBSERVER_ERROR;

	m_current_error_descr =
		WebServerErrorDescr::FindByWebServerStatus(status);
	if (NULL != m_current_error_descr)
	{
		context.Set(m_current_error_descr->m_context);
		error_code |= m_current_error_descr->m_probable_cause;
	}
	else
	{
		// Unhandled so do not log to console
		return;
	}

	// Convert from enum to error_code
	error_code |= (INT32)status;

	// Log the error to the console
	OpString message;
	g_languageManager->GetString(m_current_error_descr->m_msg_id, message);

#ifdef OPERA_CONSOLE
	// Build a console error message
#ifdef CON_CAP_LINK_UNITE
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaUnite, OpConsoleEngine::Error);
#else
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Error);
#endif // CON_CAP_LINK_UNITE

	if (show_as_warning)
	{
		// OpConsoleEngine::Information == Str::D_CONSOLE_LEVEL_WARNING in MessageConsoleDialog
		log_message.severity = OpConsoleEngine::Information;
	}

	// Set the error information
	OpStatus::Ignore(log_message.context.Set(context));
	OpStatus::Ignore(log_message.message.Set(message));
	log_message.error_code = error_code;

	// Send the error to the console
	g_console->PostMessageL(&log_message);
#endif // OPERA_CONSOLE
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerUIConnector::LogError(UploadServiceStatus status, BOOL show_as_warning)
{
	OpString context;
	INT32 error_code = OAM_WEBSERVER_UPLOAD_ERROR;

	m_current_error_descr =
			WebServerErrorDescr::FindByUploadServiceStatus(status);
	if (NULL != m_current_error_descr)
	{
		context.Set(m_current_error_descr->m_context);
		error_code |= m_current_error_descr->m_probable_cause;
	}
	else
	{
		// Unhandled so do not log to console
		return;
	}

	// Convert from enum to error_code
	error_code |= (INT32)status;

	// Log the error to the console
	OpString message;
	g_languageManager->GetString(m_current_error_descr->m_msg_id, message);

#ifdef OPERA_CONSOLE
	// Build a console error message
#ifdef CON_CAP_LINK_UNITE
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaUnite, OpConsoleEngine::Error);
#else
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Error);
#endif // CON_CAP_LINK_UNITE

	if (show_as_warning)
	{
		// OpConsoleEngine::Information == Str::D_CONSOLE_LEVEL_WARNING in MessageConsoleDialog
		log_message.severity = OpConsoleEngine::Information;
	}

	// Set the error information
	OpStatus::Ignore(log_message.context.Set(context));
	OpStatus::Ignore(log_message.message.Set(message));
	log_message.error_code = error_code;

	// Send the error to the console
	g_console->PostMessageL(&log_message);
#endif // OPERA_CONSOLE
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef AUTO_UPDATE_SUPPORT

/*virtual*/ void
WebServerUIConnector::OnUpToDate()
{
	// if there's no update, let the users know that there's something wrong with Unite
	ShowStatus(g_webserver_manager);
	OpStatus::Ignore(g_autoupdate_manager->RemoveListener(this));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*virtual*/ void
WebServerUIConnector::OnUpdateAvailable(AvailableUpdateContext* context)
{
	OpStatus::Ignore(g_autoupdate_manager->RemoveListener(this));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*virtual*/ void
WebServerUIConnector::OnError(UpdateErrorContext* context)
{
	OpStatus::Ignore(g_autoupdate_manager->RemoveListener(this));
}
#endif // AUTO_UPDATE_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL
WebServerUIConnector::DialogResult(const OpStringC& title, const OpStringC& message, DesktopWindow* parent_window)
{
	if (!parent_window)
	{
		parent_window = g_application->GetActiveDesktopWindow();
	}

	INT32 result = 0;

	SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
	if (dialog)
	{
		dialog->SetProtectAgainstDoubleClick(SimpleDialog::ForceOff);
		dialog->Init(WINDOW_NAME_WEBSERVER_UI_CONNECTOR, title, message, parent_window, Dialog::TYPE_NO_YES, Dialog::IMAGE_WARNING, TRUE, &result);
		return (result == Dialog::DIALOG_RESULT_YES);
	}
	else
	{
		return FALSE;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

WebServerManager::WebServerManager() :
	m_opera_account_manager(NULL),
	m_desktop_gadget_manager(NULL),
	m_hotlist_manager(NULL),
	m_webserver(NULL),
	m_webserver_prefs(NULL),
	m_webserver_ui(NULL),
	m_inited(FALSE),
	m_running(FALSE),
	m_enabling(FALSE),
	m_shared_secret(),
	m_root_service_address(),
	m_force_open_home_service(FALSE),
	m_status(OperaAccountManager::Disabled),
	m_upload_status(OperaAccountManager::Disabled),
	m_state_modifier(),
	m_settings_context(),
	m_schedule_feature_enabling(FALSE),
	m_schedule_feature_change(FALSE),
	m_delay_home_page(NoDelay),
	m_service_to_open(NULL),
	m_msg_handler(NULL),
	m_user_wants_device_release(-1),
	m_direct_access_address()
#ifdef UPNP_SUPPORT
	, m_upnp_listener(this, g_upnp)
#endif // UPNP_SUPPORT
{
	// Register Prefs listener
	g_pcui->RegisterListenerL(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

WebServerManager::~WebServerManager()
{
	Shutdown();

	// Remove the prefs listener
	g_pcui->UnregisterListener(this);

	// Remove the listener
	if(m_webserver)
	{
		m_webserver->RemoveWebserverListener(this);
	}
	if(m_opera_account_manager)
	{
		m_opera_account_manager->RemoveListener(this);
	}

	OP_DELETE(m_webserver);
	OP_DELETE(m_webserver_prefs);
	OP_DELETE(m_webserver_ui);

	OP_DELETE(m_msg_handler);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
WebServerManager::Init()
{
	if(IsFeatureAllowed())
	{
		if (g_desktop_account_manager && g_desktop_gadget_manager && g_hotlist_manager)
		{
			return Init(g_desktop_account_manager, g_desktop_gadget_manager, g_hotlist_manager);
		}
		else
		{
			OP_ASSERT(!"Wrong initialization order of managers!");
			return OpStatus::ERR_NULL_POINTER;
		}
	}
	else
	{
		// Disable webserver if unite is disabled
		TRAPD(err, g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, 0));
		return OpStatus::OK;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS WebServerManager::Init(OperaAccountManager * opera_account_manager,
								 DesktopGadgetManager * desktop_gadget_manager,
								 HotlistManager * hotlist_manager)
{
	if (!m_webserver)
	{
		SetWebServerConnector(OP_NEW(WebServerConnector, ()));
		OP_ASSERT(m_webserver);
	}
	if (!m_webserver_prefs)
	{
		SetWebServerPrefsConnector(OP_NEW(WebServerPrefsConnector, ()));
		OP_ASSERT(m_webserver_prefs);
	}
	if (!m_webserver_ui)
	{
		SetWebServerUIConnector(OP_NEW(WebServerUIConnector, ()));
		OP_ASSERT(m_webserver_ui);
	}

	m_opera_account_manager = opera_account_manager;
	OP_ASSERT(m_opera_account_manager);
	m_desktop_gadget_manager = desktop_gadget_manager;
	OP_ASSERT(m_desktop_gadget_manager);
	m_hotlist_manager = hotlist_manager;

	if (!m_webserver || !m_webserver_prefs || !m_webserver_ui ||
		!m_opera_account_manager || !m_desktop_gadget_manager || !m_hotlist_manager)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	m_state_modifier.Init();

	// Add the listener
	m_webserver->AddWebserverListener(this);
	m_opera_account_manager->AddListener(this);

	// Read the current shared secret
	OpStringC shared_secret = m_webserver_prefs->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword);

	if (shared_secret.HasContent())
	{
		m_shared_secret.Set(shared_secret.CStr());
	}

	// If the do not use Opera Account is set then try to start the webserver straight away.
	if (m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable) &&
		!m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
	{
		EnableFeature();
	}
	m_msg_handler = OP_NEW(MessageHandler, (NULL));
	if(!m_msg_handler)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(m_msg_handler->SetCallBack(this, MSG_QUICK_FEATURE_SETUP_COMPLETED, 0));
	RETURN_IF_ERROR(m_msg_handler->SetCallBack(this, MSG_QUICK_START_CLEAN_FEATURE_SETUP, 0));
	RETURN_IF_ERROR(m_msg_handler->SetCallBack(this, MSG_QUICK_SHOW_WEBSERVER_INVALID_USER, 0));

	// Register Prefs listener
	g_pcui->RegisterListenerL(this);

#ifdef UPNP_SUPPORT
	if (IsLocalDiscoveryNotificationsEnabled())
	{
		RETURN_IF_ERROR(g_upnp->AddLogic(&m_upnp_listener, TRUE));
	}
	OP_STATUS s = g_upnp->StartUPnPDiscovery();
	OP_ASSERT(OpStatus::IsSuccess(s));
	OpStatus::Ignore(s);	// will fail if no interfaces are installed, don't make it a critical failure for module init

#endif // UPNP_SUPPORT

	m_inited = TRUE;
	return OpStatus::OK;
}


/***********************************************************************************
**  WebServerManager::WebServerConnector
**
************************************************************************************/
OP_STATUS
WebServerManager::SetWebServerConnector(WebServerConnector * webserver_connector)
{
	if (webserver_connector)
	{
		if (m_webserver)
		{
			OP_DELETE(m_webserver);
		}
		m_webserver = webserver_connector;
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR_NULL_POINTER;
	}
}


/***********************************************************************************
**  WebServerManager::WebServerPrefsConnector
**
************************************************************************************/
OP_STATUS
WebServerManager::SetWebServerPrefsConnector(WebServerPrefsConnector * prefs_connector)
{
	if (prefs_connector)
	{
		if (m_webserver_prefs)
		{
			OP_DELETE(m_webserver_prefs);
		}
		m_webserver_prefs = prefs_connector;
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR_NULL_POINTER;
	}
}

/***********************************************************************************
**  WebServerManager::SetWebServerUIConnector
**
************************************************************************************/
OP_STATUS
WebServerManager::SetWebServerUIConnector(WebServerUIConnector * ui_connector)
{
	if (ui_connector)
	{
		if (m_webserver_ui)
		{
			OP_DELETE(m_webserver_ui);
		}
		m_webserver_ui = ui_connector;
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR_NULL_POINTER;
	}
}

/***********************************************************************************
**  WebServerManager::HandleCallback
**
************************************************************************************/
void WebServerManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_QUICK_SHOW_WEBSERVER_INVALID_USER)
	{
		// this also diables link, which is ugly.
		// but if logged in, the dialog will jump over login part if already logged in
		m_opera_account_manager->SetLoggedIn(FALSE);
		m_opera_account_manager->StopServices();

		// TODO: show dialog with invalid user message
		if (HasUsedWebServer())
		{
			ShowSettingsDialog();
		}
		else
		{
			ShowSetupWizard();
		}
		m_webserver_ui->HandleErrorInDialog(AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS);
	}
	else if (static_cast<FeatureType>(par1) == FeatureTypeWebserver)
	{
		if (msg == MSG_QUICK_FEATURE_SETUP_COMPLETED)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL, 0, UNI_L("Unite"));
		}
		else if (msg == MSG_QUICK_START_CLEAN_FEATURE_SETUP)
		{
			m_webserver_ui->ShowSetupWizard(this, NULL, GetWebserverSettings(), m_opera_account_manager, NULL);
		}
	}
}


/***********************************************************************************
 *
 *  WebServerManager::InvokeMessage - called from dialogs to perform other dialog actions
 *
 ************************************************************************************/
void WebServerManager::InvokeMessage(OpMessage msg, MH_PARAM_1 param_1, MH_PARAM_2 param_2)
{
	m_msg_handler->PostMessage(msg, param_1, param_2, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS WebServerManager::SetWebServerEnabled(BOOL enabled, BOOL write_pref, BOOL shutdown)
{
	if (enabled)
	{
		// Start the server if needed
		OP_STATUS stat = Start();
		if (OpStatus::IsError(stat))
		{
			// If this doesn't start show an error in the message console
			OpString error_msg;
			g_languageManager->GetString(Str::S_WEBSERVER_ERROR_UNABLE_TO_START, error_msg);
			// Build a console error message
			OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Error);
			OpStatus::Ignore(log_message.message.Set(error_msg));

			// Send the error to the console
			g_console->PostMessageL(&log_message);

			return stat;
		}
	}
	else
	{
		// Stop (all services and) the webserver always here
		Stop(shutdown);
	}

	if (write_pref && m_webserver_prefs)
	{
		// Set the preference
		TRAPD(err, m_webserver_prefs->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, enabled));
		TRAPD(rc, m_webserver_prefs->CommitL());
	}

	// Autostart any gadgets if required
	if (enabled)
	{
		m_desktop_gadget_manager->StartAutoStartServices();
	}

	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
WebServerManager::ReleaseDevice(const OpStringC & device_name)
{
	OP_ASSERT(device_name.HasContent());
	if (device_name.HasContent())
	{
		return m_opera_account_manager->ReleaseDevice(device_name);
	}
	return OpStatus::ERR;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
WebServerManager::RegisterDevice(const OpStringC & device_name, BOOL force)
{
	if (IsValidDeviceName(device_name))
	{
		m_settings_context.SetDeviceName(device_name);

		return m_opera_account_manager->RegisterDevice(device_name, force);
	}
	return OpStatus::ERR;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
OP_STATUS
WebServerManager::EnableIfRequired(BOOL& has_started, BOOL always_ask, BOOL delay_for_service)
{
	if (m_running || m_enabling)
	{
		has_started = TRUE;
		return OpStatus::OK;
	}
	else
	{
		has_started = FALSE;

		// check offline mode
		if (g_desktop_account_manager->GetIsOffline())
		{
			if( !g_application->AskEnterOnlineMode(TRUE) )
			{
				return OpStatus::ERR;
			}
		}

		BOOL enable = FALSE;
		if (!HasUsedWebServer())
		{
			if (delay_for_service)
			{
				m_delay_home_page = DelayRequested;
			}
			OP_STATUS stat = ShowSetupWizard(&has_started);
			if (delay_for_service && (OpStatus::IsError(stat) || !has_started))
			{
				m_delay_home_page = NoDelay;
			}
			return stat;
		}
		else if (m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount) && !m_opera_account_manager->GetLoggedIn())
		{
			RETURN_IF_ERROR(m_webserver_ui->ShowLoginDialog(this, GetWebserverSettings(), m_opera_account_manager, m_opera_account_manager->GetAccountContext(), &has_started));
			if (has_started)
			{
				enable = TRUE;
			}
		}
		else if (!always_ask || m_webserver_ui->UserWantsFeatureEnabling())
		{
			has_started = TRUE;
			enable = TRUE;
		}

		if (enable)
		{
			EnableFeature(); // todo: handle device already taken failure
			return OpStatus::OK;
		}

	}
	return OpStatus::ERR;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
OP_STATUS
WebServerManager::OpenAndEnableIfRequired(BOOL& has_started, BOOL always_ask, BOOL delay_for_service)
{
	if (m_running)
	{
		OpenRootService();
		has_started = TRUE;
		return OpStatus::OK;
	}
	if (HasUsedWebServer())
	{
		m_force_open_home_service = TRUE;
	}
	OP_STATUS stat = EnableIfRequired(has_started, always_ask, delay_for_service);
	if (!has_started)
	{
		m_force_open_home_service = FALSE; // reset
	}
	return stat;
}

/***************************************************************************
 *
 * WebServerManager::ChangeAccount
 *
 *
 ***************************************************************************/
OP_STATUS
WebServerManager::ChangeAccount()
{
	m_opera_account_manager->SetLoggedIn(FALSE);
	m_opera_account_manager->StopServices();

	BOOL is_enabled;
	return EnableIfRequired(is_enabled, FALSE, FALSE);
}

/***************************************************************************
 *
 * Start
 *
 *
 *
 ***************************************************************************/

OP_STATUS WebServerManager::Start()
{
	if (!m_running)
	{
		// AJMTO Need to check everywhere this function is called to check the error return
		OP_STATUS err = OpStatus::OK;

		WebserverListeningMode listening_mode = GetWebServerListeningMode();

		// Startup the webserver.
		if (OpStatus::IsError(err = m_webserver->Start(listening_mode
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
														, m_shared_secret.CStr()
#endif
														)))
			return err;

		SetIsRunning(TRUE);
		BOOL open_home_page = !HasUsedWebServer() || m_force_open_home_service;
		if (m_force_open_home_service) m_force_open_home_service = FALSE; // reset force

		TRAP(err, m_webserver_prefs->WriteIntegerL(PrefsCollectionWebserver::WebserverUsed, TRUE));
		if (OpStatus::IsError(err))
			return err;

		// Start the root service widget
		if (OpStatus::IsError(m_desktop_gadget_manager->OpenRootService(open_home_page)))
		{
			// If this doesn't start show an error in the message console
			OpString error_msg;
			g_languageManager->GetString(Str::S_WEBSERVER_ERROR_UNABLE_TO_START_HOME_SERVICE, error_msg);
			// Build a console error message
				OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Error);
				OpStatus::Ignore(log_message.message.Set(error_msg));

				// Send the error to the console
				g_console->PostMessageL(&log_message);
		}

#ifdef WEB_UPLOAD_SERVICE_LIST
		BOOL start_asd;
#ifdef PREFS_CAP_UI_SERVICE_DISCOVERY_NOTIFICATIONS
		start_asd = m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::ServiceDiscoveryEnabled);
#else
		start_asd = TRUE;
#endif // PREFS_CAP_UI_SERVICE_DISCOVERY_NOTIFICATIONS

		// Don't start  the service discovery for unless the proxy is being used
		if (start_asd && ((listening_mode & WEBSERVER_LISTEN_PROXY_LOCAL) == WEBSERVER_LISTEN_PROXY_LOCAL))
		{
			// Start service discovery
			// AJMTODO add preference to change the service discovery server
			OpStringC discovery_server = m_webserver_prefs->GetStringPref(PrefsCollectionWebserver::ServiceDiscoveryServer);

			// if this asserts, then the token from auth has not been stored properly
			OP_ASSERT(m_opera_account_manager->GetToken() && *m_opera_account_manager->GetToken());

			if (OpStatus::IsError(m_webserver->StartServiceDiscovery(discovery_server, m_opera_account_manager->GetToken())))
			{
				// If this doesn't start show an error in the message console
				OpString error_msg;
				g_languageManager->GetString(Str::S_WEBSERVER_ERROR_UNABLE_TO_START_SERVICE_DISCOVERY, error_msg);
					// Build a console error message
				OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Error);
				OpStatus::Ignore(log_message.message.Set(error_msg));

					// Send the error to the console
				g_console->PostMessageL(&log_message);
			}
		}
#endif // WEB_UPLOAD_SERVICE_LIST

		if (m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount)
			&& !m_opera_account_manager->GetLoggedIn()) // needed because account manager might have automatically logged out
		{
			m_opera_account_manager->SetLoggedIn(TRUE);
		}

		// Set the status to connected
		SetFeatureEnabling(FALSE);
		m_status = OperaAccountManager::OK;
		RETURN_IF_ERROR(m_state_modifier.SetWidgetStateToEnabled());
	}

	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 *
 * Stop
 *
 *
 **********************************************************************************/
void WebServerManager::CloseAllServices()
{
	// Close all of the unite services that are running
	// but add them to the autostart list so that they will restart if you
	// re-enable the webserver
	if (g_main_message_handler)
		g_main_message_handler->PostMessage(MSG_CLOSE_ALL_UNITE_SERVICES, (MH_PARAM_1)m_desktop_gadget_manager, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::Stop(BOOL shutdown)
{
	if (!m_running) return;

	CloseAllServices();

#ifdef WEB_UPLOAD_SERVICE_LIST
	// Stop service discovery
	m_webserver->StopServiceDiscovery();
#endif // WEB_UPLOAD_SERVICE_LIST

	// close the root service
	if (!shutdown)
	{
		m_desktop_gadget_manager->CloseRootServiceWindow();
	}

	m_webserver->Stop();

	SetIsRunning(FALSE);

	// Set the status to disabled
	m_status = OperaAccountManager::Disabled;
	OpStatus::Ignore(m_state_modifier.SetWidgetStateToDisabled());
}

/***********************************************************************************
**  WebServerManager::IsUserRequestingDeviceRelease
************************************************************************************/
BOOL
WebServerManager::IsUserRequestingDeviceRelease(DesktopWindow * parent_window)
{
	/*
	 * We cannot trust auth to return the correct status for a device already being
	 * taken. i.e in a wireless config, moving in and out of coverage might make auth 
	 * and/or the proxy think that the device name is taken. 
	 *
	 * So showing a dialog is more confusing than not showint it at all and just 
	 * taking over in any case.
	 */
	return TRUE;
/*
if (m_user_wants_device_release != -1)
	{
		return m_user_wants_device_release;
	}
	else
	{
		m_user_wants_device_release = m_webserver_ui->UserWantsDeviceTakeover(m_settings_context.GetDeviceName(), parent_window);
		return m_user_wants_device_release;
	}
*/
}

///////////////////////////////////////////////////////////////////////////////////////////////////

WebserverListeningMode WebServerManager::GetWebServerListeningMode()
{
	if (!m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
		return WEBSERVER_LISTEN_LOCAL;
	else
	{
		WebserverListeningMode mode = WEBSERVER_LISTEN_PROXY_LOCAL;

#ifdef UPNP_SUPPORT
		BOOL upnp_enabled;
		BOOL upnp_discovery_enabled;
#ifdef WEBSERVER_CAP_LISTEN_ROBOTS_TXT
		BOOL robots_enabled;
#endif // WEBSERVER_CAP_LISTEN_ROBOTS_TXT

#ifdef PREFS_CAP_UPNP_ENABLED
		upnp_enabled = m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UPnPEnabled);
#else
		upnp_enabled = TRUE;
#endif // PREFS_CAP_UPNP_ENABLED

#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
		upnp_discovery_enabled = m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled);
#else
		upnp_discovery_enabled = FALSE;
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED

#ifdef WEBSERVER_CAP_LISTEN_ROBOTS_TXT
#ifdef PREFS_CAP_ROBOTS_TXT_ENABLED
		robots_enabled = m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::RobotsTxtEnabled);
#else
		robots_enabled = TRUE;
#endif // PREFS_CAP_ROBOTS_TXT_ENABLED
#endif // WEBSERVER_CAP_LISTEN_ROBOTS_TXT

		if (upnp_enabled)
		{
			mode |= WEBSERVER_LISTEN_OPEN_PORT;
		}
		if (upnp_discovery_enabled)
		{
			mode |= WEBSERVER_LISTEN_UPNP_DISCOVERY;
		}

#ifdef WEBSERVER_CAP_LISTEN_ROBOTS_TXT
		if (robots_enabled)
		{
			mode |= WEBSERVER_LISTEN_ROBOTS_TXT;
		}
#endif // WEBSERVER_CAP_LISTEN_ROBOTS_TXT

#endif // UPNP_SUPPORT
		return mode;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOL WebServerManager::HasUsedWebServer()
{
	return m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::WebserverUsed);
}

BOOL
WebServerManager::IsValidDeviceName(const OpStringC & device_name)
{
	// don't allow IDNA strings (for now)
	if (device_name.HasContent() && device_name.Find("xn--") == KNotFound)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::SaveDeviceName()
{
	if (m_settings_context.GetDeviceName().HasContent())
	{
		m_webserver_prefs->WriteStringL(PrefsCollectionWebserver::WebserverDevice, m_settings_context.GetDeviceName());
		m_settings_context.ResetDeviceName();
	}
	else
	{
		OP_ASSERT(!"device name hasn't been stored properly");
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::UpdateOffline(BOOL offline)
{
	if ( IsInited() && m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable) &&
		m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
	{
		if (offline)
		{
			// don't use SetWebServerEnabled(FALSE) here, as it sets WebserverEnable pref
			// the pref should only be set if the user chooses to disable
			Stop(); // Stop all services and webserver
		}
		else
		{
			OpStatus::Ignore(SetWebServerEnabled(TRUE)); // starts webserver and starts services again
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
WebServerManager::GetURLForComputerName(OpString & url, const OpStringC & computername, BOOL admin_url)
{
	url.Empty();

	// Read the username from prefs
	OpString username;
	username.Set(m_webserver_prefs->GetStringPref(PrefsCollectionWebserver::WebserverUser));
	OpStringC proxy = m_webserver_prefs->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost);

	if (!username.HasContent())
	{
		// Check if OperaAccountUser set - if user has used Opera Account (Link) but not Unite
		m_opera_account_manager->GetUsername(username);
		if (username.HasContent())
		{
			TRAPD(err, m_webserver_prefs->WriteStringL(PrefsCollectionWebserver::WebserverUser, username));
		}
	}

	// Build the path to the root service
	// allow empty computer name (for preview URL in setup/settings dialog)
	if (/*computername.HasContent() && */username.HasContent() && proxy.HasContent())
	{
		OpString prefix;
		if (admin_url)
		{
#ifdef WEBSERVER_CAP_ADMIN_SUPPORT
			prefix.Set(UNI_L("http://admin."));
#else
			prefix.Set(UNI_L("unite://"));
#endif // WEBSERVER_CAP_ADMIN_SUPPORT
		}
		else
		{
			prefix.Set(UNI_L("http://"));
		}
		return url.AppendFormat(UNI_L("%s%s.%s.%s/"),
			prefix.CStr(),
			computername.HasContent() ? computername.CStr() : UNI_L(""),
			username.CStr(),
			proxy.CStr());
	}
	return OpStatus::ERR;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::SetSharedSecret(const OpStringC& shared_secret)
{
	// If we have a shared secret set in the member and preference
	// ONLY EVER save the shared secret if we have one
	if (shared_secret.HasContent())
	{
		m_shared_secret.Set(shared_secret.CStr());

		// Save the shared secret
		TRAPD(err, m_webserver_prefs->WriteStringL(PrefsCollectionWebserver::WebserverHashedPassword, shared_secret));
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////


void WebServerManager::OpenRootService()
{
	if (m_running)
	{
		HotlistGenericModel* unite_model = m_hotlist_manager->GetUniteServicesModel();
		UniteServiceModelItem* root_service = NULL;
		if (unite_model && unite_model->GetRootService())
		{
			root_service = static_cast<UniteServiceModelItem*>(unite_model->GetRootService());
			if (root_service)
			{
				root_service->OpenOrFocusServicePage();
			}
		}
	}
	else
	{
		OP_ASSERT(FALSE);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const uni_char *WebServerManager::GetRootServiceAddress(BOOL admin_url)
{
	m_root_service_address.Empty();

	if (!m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
	{
		INT32 port = m_webserver->GetLocalListeningPort();
		m_root_service_address.AppendFormat(UNI_L("http://localhost:%d/"), port);
	}
	else
	{
		// Read the device name from the pref
		OpStringC device_name = m_webserver_prefs->GetStringPref(PrefsCollectionWebserver::WebserverDevice);
		if (device_name.HasContent())
		{
			RETURN_VALUE_IF_ERROR(GetURLForComputerName(m_root_service_address, device_name, admin_url), NULL);
		}
	}	
	return m_root_service_address.CStr();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	// initialize if EnableUnite switched on
	if(!m_inited)
	{
		if(id == OpPrefsCollection::UI &&
			PrefsCollectionUI::EnableUnite == PrefsCollectionUI::integerpref(pref) &&
			newvalue)
		{
			Init();
		}
		return;
	}


	// Checking for the enable pref for the webserver
	if (id == OpPrefsCollection::Webserver &&
		PrefsCollectionWebserver::WebserverEnable == PrefsCollectionWebserver::integerpref(pref))
	{
		// If this is trying to be turned on we MUST be logged in otherwise reset it
		if (/*!m_opera_account_manager->GetLoggedIn() &&*/ newvalue)
		{
			SetWebServerEnabled(FALSE);
		}
	}
#if defined(SHOW_DISCOVERED_DEVICES_SUPPORT) && defined(UPNP_SUPPORT)
	// checking if notifications about known devices were turned off
	if (id == OpPrefsCollection::UI &&
		PrefsCollectionUI::EnableServiceDiscoveryNotifications == PrefsCollectionUI::integerpref(pref))
	{
		if (newvalue == FALSE)
		{
			g_upnp->RemoveLogic(&m_upnp_listener);
			m_upnp_listener.ClearDevicesList();

			if (m_state_modifier.HasAttention())
			{
				m_state_modifier.SetAttention(FALSE);
			}
		}
		else
		{
			OpStatus::Ignore(g_upnp->AddLogic(&m_upnp_listener, TRUE));
			OpStatus::Ignore(g_upnp->StartUPnPDiscovery());
			m_state_modifier.SetAttention(m_upnp_listener.GetKnownDeviceList().GetCount() > 0);
		}
	}
#endif // defined(SHOW_DISCOVERED_DEVICES_SUPPORT) && defined(UPNP_SUPPORT)
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void WebServerManager::SetIsRunning(BOOL is_running)
{
	m_running = is_running;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
WebServerManager::GetServiceAddressFromPath(const OpStringC & url_str, OpString & service_address)
{
	URL url = g_url_api->GetURL(url_str.CStr());
	OpString path;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniPath, path));
	if (path.HasContent())
	{
		path.Delete(0, 1); // strip first '/'
		if (path.IsEmpty() || path.Compare(UNI_L("_root/"), 5) == 0)
		{
			return service_address.Set(UNI_L("_root"));
		}
		else
		{
			return service_address.Set(path.CStr(), path.FindFirstOf('/'));
		}
	}
	return OpStatus::ERR;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
WebServerManager::OpenCachedPagesForService(const OpStringC & service_name)
{
	// Open cached pages
	ServiceWindowMap * map;
	OP_STATUS stat = m_auto_load_windows.Remove(service_name.CStr(), &map);
	if (OpStatus::IsSuccess(stat) && map)
	{
		while(map->window_ids.GetCount() > 0)
		{
			g_main_message_handler->PostMessage(MSG_GETPAGE, (MH_PARAM_1)map->window_ids.Remove(0), STARTUP_HISTORY);
		}
		OP_DELETE(map);
	}
	return stat;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::HandleWebserverError(WebserverStatus status)
{
	// Update the status of the webserver
	if (m_status != OperaAccountManager::Disabled)
	{
		OperaAccountManager::OAM_Status old_status = m_status;

		if (status == WEBSERVER_OK)
		{
			// If we get an ok then go back to the connected state
			m_status = OperaAccountManager::OK;
			OpStatus::Ignore(m_state_modifier.SetWidgetStateToEnabled());
		}
		else
		{
			// don't show an error if listening socket is taken [DSK-247526]
			if (status != WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN)
			{
				// If we get an error then go to the error state
				m_status = OperaAccountManager::Error;
				OpStatus::Ignore(m_state_modifier.SetWidgetStateToError());
			}
		}

		// Update the input states if the status changes
		if (old_status != m_status)
			g_input_manager->UpdateAllInputStates();
	}

	// Log the error (LogError will handle when to send the error to the console)
	m_webserver_ui->LogError(status, status == WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN ? TRUE : FALSE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::HandleWebserverUploadServiceError(UploadServiceStatus status)
{
	// Update the status of the webserver
	if (m_upload_status != OperaAccountManager::Disabled)
	{
		OperaAccountManager::OAM_Status old_status = m_upload_status;

		if (status == UPLOAD_SERVICE_OK)
		{
			// If we get an ok then go back to the connected state
			m_upload_status = OperaAccountManager::OK;
		}
		else
		{
			// If we get an error then go to the error state
			m_upload_status = OperaAccountManager::Error;
		}

		// Update the input states if the status changes
		if (old_status != m_upload_status)
			g_input_manager->UpdateAllInputStates();
	}

	// Log the error (LogError will handle when to send the error to the console)
	m_webserver_ui->LogError(status);
}

BOOL
WebServerManager::IsFeatureAllowed() const
{
	return g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUnite);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOL WebServerManager::IsFeatureEnabled() const
{
	return m_running;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::EnableFeature(const FeatureSettingsContext* settings)
{
	EnableFeature(settings, FALSE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerManager::EnableFeature(const FeatureSettingsContext* settings, BOOL force_device_release)
{
	if (IsFeatureEnabled())
		return;

	const WebServerSettingsContext * webserver_settings = static_cast<const WebServerSettingsContext*>(settings);

	OpString device_name;
	if (webserver_settings)
	{
		BOOL needs_restart = FALSE;
		OpStatus::Ignore(SaveAdvancedSettings(webserver_settings, needs_restart));

		device_name.Set(webserver_settings->GetDeviceName());
	}

	if (device_name.IsEmpty())
	{
		if (!m_settings_context.GetDeviceName().IsEmpty())
		{
			device_name.Set(m_settings_context.GetDeviceName());
		}
		else
		{
			m_webserver_prefs->GetStringPrefL(PrefsCollectionWebserver::WebserverDevice, device_name);
		}
	}

	OP_ASSERT(device_name.HasContent());  // webserver used without devicename set should not happen
	if (IsValidDeviceName(device_name))
	{
		if (!m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
		{
			// skip device creation
			m_settings_context.SetDeviceName(device_name);
			OpString empty_str;
			OnOperaAccountDeviceCreate(AUTH_OK, empty_str, empty_str);
		}
		else
		{
			m_user_wants_device_release = -1;
			m_status = OperaAccountManager::Busy;
			OpStatus::Ignore(m_state_modifier.SetWidgetStateToEnabling());
			SetFeatureEnabling(TRUE);

			RegisterDevice(device_name, force_device_release);
		}
	}
	else if (IsConfiguring())
	{
		m_webserver_ui->HandleErrorInDialog(AUTH_ACCOUNT_CREATE_DEVICE_INVALID);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
void
WebServerManager::SetHasDiscoveredServices(BOOL discovered_services)
{
	if (discovered_services == TRUE)
	{
		if (IsLocalDiscoveryNotificationsEnabled() && !m_state_modifier.HasAttention())
		{
			OpString users_found_str; 
			g_languageManager->GetString(Str::S_WEBSERVER_DISCOVERY_TOOLTIP, users_found_str);
			m_state_modifier.SetAttentionText(users_found_str);
			m_state_modifier.SetAttention(TRUE);
		}
	}
	else
	{
		m_state_modifier.SetAttention(FALSE);
	}
}
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT


///////////////////////////////////////////////////////////////////////////////////////////////////

void
WebServerManager::SetFeatureEnabling(BOOL enabling)
{
	if (m_enabling != enabling)
	{
		m_enabling = enabling;

		// make sure the status text in the panel changes
		HotlistModel* unite_model = m_hotlist_manager->GetUniteServicesModel();
		UniteServiceModelItem* root_service = NULL;
		if (unite_model && unite_model->GetRootService())
		{
			root_service = static_cast<UniteServiceModelItem*>(unite_model->GetRootService());
			if (root_service)
			{
				root_service->Change();
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::DisableFeature(BOOL write_pref, BOOL shutdown)
{
	if (IsFeatureEnabled())
	{
		// start device release
		OpString device_name;
		m_webserver_prefs->GetStringPrefL(PrefsCollectionWebserver::WebserverDevice, device_name);
		ReleaseDevice(device_name);
	}

	// no matter unite is running or not we need to call this function to write pref!
	// as it's possible unite is not running due to some reasons(device name in use for example)
	// but the pref is TRUE
	SetWebServerEnabled(FALSE, write_pref, shutdown);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void
WebServerManager::Shutdown()
{
	if (IsInited())
	{
		DisableFeature(FALSE, TRUE);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::SetFeatureSettings(const FeatureSettingsContext* settings)
{
	OP_ASSERT(settings);
	if (settings)
	{
		const WebServerSettingsContext * webserver_settings = static_cast<const WebServerSettingsContext*>(settings);

		BOOL needs_restart = FALSE;

		if (!settings->IsFeatureEnabled())
		{
			// device name isn't saved on disabling, advanced settings are, though..
			OpStatus::Ignore(SaveAdvancedSettings(webserver_settings, needs_restart));
			DisableFeature();
		}
		else
		{
			if (!IsFeatureEnabled())
			{
				// settings are saved inside EnableFeature
				EnableFeature(settings);
			}
			else
			{
				OpStatus::Ignore(SaveAdvancedSettings(webserver_settings, needs_restart));

				// device name
				OpString current_device_name;
				m_webserver_prefs->GetStringPrefL(PrefsCollectionWebserver::WebserverDevice, current_device_name);
				BOOL device_name_changed = (current_device_name.Compare(webserver_settings->GetDeviceName()) != 0);
				needs_restart = needs_restart || device_name_changed;


				if (needs_restart)
				{
					m_schedule_feature_change = TRUE;
					m_settings_context.SetDeviceName(webserver_settings->GetDeviceName());
					DisableFeature(FALSE, FALSE);
				}
				else
				{
					BroadcastOnFeatureSettingsChangeSucceeded();
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS WebServerManager::SaveAdvancedSettings(const WebServerSettingsContext* webserver_settings, BOOL & needs_restart)
{
	OP_ASSERT(webserver_settings);
	if (webserver_settings)
	{
#if defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_ENABLED)
		BOOL upnp_setting_changed = (m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UPnPEnabled) != webserver_settings->IsUPnPEnabled());
		needs_restart = needs_restart || upnp_setting_changed;
#endif // defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_ENABLED)

		BOOL port_changed = (m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::WebserverPort) != webserver_settings->GetPort());
		needs_restart = needs_restart || port_changed;

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
		BOOL asd_changed = (m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::ServiceDiscoveryEnabled) != webserver_settings->IsASDEnabled());
		needs_restart = needs_restart || asd_changed;
#endif //  PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#if defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED)
		BOOL upnp_visibility_changed = (m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled) != webserver_settings->IsUPnPServiceDiscoveryEnabled());
		needs_restart = needs_restart || upnp_visibility_changed;
#endif // defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED)

		// settings to set without restarting webserver
		RETURN_IF_ERROR(m_webserver_prefs->WriteIntegerL(PrefsCollectionWebserver::WebserverUploadRate, webserver_settings->GetUploadSpeed()));

#if defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_ENABLED)
		if (upnp_setting_changed)
			RETURN_IF_ERROR(m_webserver_prefs->WriteIntegerL(PrefsCollectionWebserver::UPnPEnabled, webserver_settings->IsUPnPEnabled()));
#endif // defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_ENABLED)

		if (port_changed)
			RETURN_IF_ERROR(m_webserver_prefs->WriteIntegerL(PrefsCollectionWebserver::WebserverPort, webserver_settings->GetPort()));

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
		if (asd_changed)
			RETURN_IF_ERROR(m_webserver_prefs->WriteIntegerL(PrefsCollectionWebserver::ServiceDiscoveryEnabled, webserver_settings->IsASDEnabled()));
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#if defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED)
		if (upnp_visibility_changed)
			RETURN_IF_ERROR(m_webserver_prefs->WriteIntegerL(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled, webserver_settings->IsUPnPServiceDiscoveryEnabled()));
#endif // defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED)

		OP_STATUS stat = OpStatus::ERR;
#if defined(WEBSERVER_CAP_SET_VISIBILITY)
		HotlistModel* unite_model = m_hotlist_manager->GetUniteServicesModel();
		UniteServiceModelItem* root_service = NULL;
		if (unite_model && unite_model->GetRootService())
		{
			root_service = static_cast<UniteServiceModelItem*>(unite_model->GetRootService());
			if (root_service)
			{
				OpGadget * service = root_service->GetOpGadget();
				if (service)
				{
					service->SetVisibleRobots(webserver_settings->IsVisibleRobotsOnHome());
					stat = OpStatus::OK;
				}
			}
		}
#else
		stat = OpStatus::OK;
#endif // WEBSERVER_CAP_SET_VISIBILITY

		return stat;
	}
	else
	{
		return OpStatus::ERR;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::OnProxyConnected()
{
	if (m_status != OperaAccountManager::OK)
	{
		m_status = OperaAccountManager::OK;
		OpStatus::Ignore(m_state_modifier.SetWidgetStateToEnabled());
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void WebServerManager::OnProxyConnectionFailed(WebserverStatus status, BOOL retry)
{
	HandleWebserverError(status);
}

void WebServerManager::OnProxyConnectionProblem(WebserverStatus status,  BOOL retry)
{
	HandleWebserverError(status);
}

// WebserverEventListener interface
void WebServerManager::OnWebserverStopped(WebserverStatus status)
{
	HandleWebserverError(status);
}

void WebServerManager::OnWebserverServiceStarted(const uni_char *service_name,
												 const uni_char *service_path, BOOL is_root_service)
{
}

void WebServerManager::OnWebserverServiceStopped(const uni_char *service_name,
												 const uni_char *service_path, BOOL is_root_service)
{

}

void WebServerManager::OnWebserverListenLocalStarted(unsigned int port)
{
}

void WebServerManager::OnWebserverListenLocalStopped()
{
}

void WebServerManager::OnWebserverListenLocalFailed(WebserverStatus status)
{
	HandleWebserverError(status);
}

#ifdef WEB_UPLOAD_SERVICE_LIST
void WebServerManager::OnWebserverUploadServiceStatus(UploadServiceStatus status)
{
	HandleWebserverUploadServiceError(status);
}
#endif // WEB_UPLOAD_SERVICE_LIST

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
void WebServerManager::OnProxyDisconnected(WebserverStatus status, BOOL retry, int code)
{
	switch (status)
	{
	case PROXY_CONNECTION_LOGGED_OUT:
		{
			SetWebServerEnabled(FALSE); // don't use DisableFeature, the device has been already been released by another computer
			HandleWebserverError(status);

			if (m_webserver_ui->UserWantsReconnectAfterTakeover())
			{
				EnableFeature(NULL, TRUE);
			}
		}
		break;
	case PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION:
		{
			SetWebServerEnabled(FALSE);
			HandleWebserverError(status);
			m_webserver_ui->OnWebServerIsInsecure();
		}
		break;
	case PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION:
		{
			m_webserver_ui->OnWebServerIsOutdated();
		}
	}
}
#endif

void WebServerManager::OnNewDOMEventListener(const uni_char *service_name, const uni_char *evt, const uni_char *virtual_path)
{
	if (evt && *evt && uni_stri_eq(evt, UNI_L("_request")) || uni_stri_eq(evt, UNI_L("_index")))
	{

		if (!virtual_path || !(*virtual_path))
		{
			return;
		}

		if (uni_stri_eq(virtual_path, UNI_L("_root"))) //
		{
			HotlistGenericModel* unite_model = m_hotlist_manager->GetUniteServicesModel();
			UniteServiceModelItem* root_service = 0;
			if (unite_model && unite_model->GetRootService())
			{
				root_service = static_cast<UniteServiceModelItem*>(unite_model->GetRootService());
				if (root_service)
				{
					root_service->SetCanOpenServicePage(TRUE);
					if(!root_service->GetOpenServicePage())
					{
						OpStringC root(UNI_L("_root"));
						OpStatus::Ignore(OpenCachedPagesForService(root));
						return;
					}
					root_service->SetOpenServicePage(FALSE);

					if (m_delay_home_page == NoDelay)
					{
						root_service->OpenOrFocusServicePage();

						// if the service callback came before the one for the root service:
						if (m_service_to_open)
						{
							m_service_to_open->OpenOrFocusServicePage();
							m_service_to_open = NULL;
							g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL, 0, UNI_L("Unite"));
						}
					}
					else
					{
						m_delay_home_page = DelayTriggered;
					}
				}
			}
		}
		else
		{

			HotlistModel* unite_model = m_hotlist_manager->GetUniteServicesModel();
			UniteServiceModelItem * item = NULL;

			if (unite_model)
			{
				for (INT32 i = 0; i < unite_model->GetCount(); i++)
				{
					HotlistModelItem* hmi = static_cast<HotlistModelItem*>(unite_model->GetItemByIndex(i));
					if (hmi && hmi->IsUniteService())
					{
						item = static_cast<UniteServiceModelItem*>(hmi);
						if (!item) continue;
						OpGadget* gadget = item->GetOpGadget();
						if (gadget)
						{
							const uni_char* servicename = const_cast<uni_char*>(gadget->GetUIData(UNI_L("serviceName")));
							if (servicename && uni_str_eq(virtual_path, servicename))
							{
								item->SetCanOpenServicePage(TRUE);
								if (!item->GetOpenServicePage())
								{
									OpStringC svc(servicename);
									OpStatus::Ignore(OpenCachedPagesForService(svc));
									return;
								}
								else
								{
									item->SetOpenServicePage(FALSE);
									break;
								}
							}
						}
					}
				}
			}
			OP_ASSERT(item);
			if (item)
			{
				// if the root service callback has already come in, but wasn't handled because of the delay,
				// handle it now
				if (m_delay_home_page == DelayTriggered)
				{
					OpenRootService();
					m_delay_home_page = NoDelay;
				}

				// if the root service delay has been requested, but the callback hasn't come yet for the root service
				// wait with opening this page until the home service page is opened
				if (m_delay_home_page == DelayRequested)
				{
					m_delay_home_page = NoDelay;
					m_service_to_open = item;
				}
				else
				{
					item->OpenOrFocusServicePage();
					g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL, 0, UNI_L("Unite"));
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void	
WebServerManager::OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address)
{
	if (direct_access)
	{
		m_direct_access_address.Set(direct_access_address);
	}
	else
	{
		m_direct_access_address.Empty();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerManager::OnOperaAccountAuth(OperaAuthError error, const OpStringC &shared_secret)
{
	if (error == AUTH_ERROR_COMM_TIMEOUT)
	{
		if (m_schedule_feature_change || m_schedule_feature_enabling)
		{
			OP_ASSERT(!"Inform the user here?");

			// Set the status to disabled
			m_status = OperaAccountManager::Disabled;
			OpStatus::Ignore(m_state_modifier.SetWidgetStateToDisabled());

			m_schedule_feature_change = FALSE;
			m_schedule_feature_enabling = FALSE;
			SetFeatureEnabling(FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerManager::OnOperaAccountReleaseDevice(OperaAuthError error)
{
	switch (error)
	{
	case AUTH_ERROR_PARSER: // code 400. missing argument/malformed URL. this REALLY shouuldn't happen
	case AUTH_ACCOUNT_AUTH_FAILURE: // code 403/404. seems to happen when another computer took over the computer name and this computer is trying to disable
	case AUTH_ACCOUNT_AUTH_INVALID_KEY: // code 411. invalid device name
		{
			OP_ASSERT(FALSE); // there shouldn't really be an error here. check why it happened.
			// continue
		}
	case AUTH_OK: // even if the releasing didn't work on auth, disable the device
		{

			// then do whatever else is sheduled
			if (m_schedule_feature_enabling || m_schedule_feature_change)
			{
				EnableFeature(&GetWebserverSettings());
			}
			// don't call BroadcastOnFeatureSettingsChangeSucceeded() here (see DSK-261172)
			// if you need a callback, create a new BroadcastOnFeatureDisablingSucceeded() or similar
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerManager::OnOperaAccountDeviceCreate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message)
{
	BOOL handle_error = FALSE;

	// allow login failure to go through (needed on startup, see DSK-269778)
	if(error == AUTH_OK || error == AUTH_ERROR_COMM_FAIL)
	{
		SaveDeviceName();
		if(shared_secret.HasContent())
		{
			SetSharedSecret(shared_secret.CStr());
		}
		if (OpStatus::IsSuccess(SetWebServerEnabled(TRUE)))
		{
			if (m_schedule_feature_change)
			{
				m_schedule_feature_change = FALSE;
				BroadcastOnFeatureSettingsChangeSucceeded();
			}
			else
			{
				m_schedule_feature_enabling = FALSE;
				BroadcastOnFeatureEnablingSucceeded();
			}
		}
		else
		{
			OP_ASSERT(!"Webserver couldn't be started, handle this properly!");
			handle_error = TRUE;
		}
	}
	else
	{
		handle_error = TRUE;
	}

	if (handle_error)
	{
		// Set the status to disabled
		m_status = OperaAccountManager::Disabled;
		OpStatus::Ignore(m_state_modifier.SetWidgetStateToDisabled());

		SetFeatureEnabling(FALSE);
		m_schedule_feature_change = FALSE;
		m_schedule_feature_enabling = FALSE;

		if (!IsConfiguring()) // if configuring, the dialog will handle the errors
		{
			switch(error)
			{
			case AUTH_ACCOUNT_CREATE_DEVICE_EXISTS:
				{
					if (IsUserRequestingDeviceRelease())
					{
						EnableFeature(NULL, TRUE);
						break;
					}
				}
				// else: fall through
			case AUTH_ACCOUNT_CREATE_DEVICE_INVALID:
			default: // do we really want to do that in all cases??
				{
					m_settings_context.ResetDeviceName();
					m_webserver_ui->ShowSettingsDialog(this, GetWebserverSettings(TRUE), m_opera_account_manager, m_opera_account_manager->GetAccountContext());
					m_webserver_ui->HandleErrorInDialog(error);
				}
			}
		}
		else
		{
			// don't reset the device name for those errors that use it in dialog
			if (error != AUTH_ACCOUNT_CREATE_DEVICE_EXISTS)
			{
				m_settings_context.ResetDeviceName();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
WebServerManager::OnPasswordMissing()
{
	if (IsFeatureEnabling())
	{
		m_status = OperaAccountManager::Disabled;
		OpStatus::Ignore(m_state_modifier.SetWidgetStateToDisabled());

		SetFeatureEnabling(FALSE);
		m_schedule_feature_change = FALSE;
		m_schedule_feature_enabling = FALSE;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
BOOL
WebServerManager::IsLocalDiscoveryNotificationsEnabled()
{
#ifdef DU_CAP_PREFS
	return (BOOL) g_pcui->GetIntegerPref(PrefsCollectionUI::EnableServiceDiscoveryNotifications);
#else
	return TRUE;
#endif // DU_CAP_PREFS
}
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
WebServerManager::Restart()
{
	m_schedule_feature_enabling = TRUE;
	DisableFeature();
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
WebServerManager::GetWebServerStatusContext(WebServerStatusContext & context)
{
	if (m_webserver_ui->GetCurrentErrorDescr())
	{
		context.m_suggested_action = m_webserver_ui->GetCurrentErrorDescr()->m_suggested_action;

		OpString error_msg;
		g_languageManager->GetString(m_webserver_ui->GetCurrentErrorDescr()->m_msg_id, error_msg);
		RETURN_IF_ERROR(context.m_status_text.Set(error_msg.CStr()));
	}

	switch (m_status)
	{
	case OperaAccountManager::Error:
		{
			RETURN_IF_ERROR(context.m_status_icon_name.Set("Unite Failed Large"));
			OP_ASSERT(m_webserver_ui->GetCurrentErrorDescr());
			break;
		}
	case OperaAccountManager::OK:
		{
			context.m_suggested_action = WebServerErrorDescr::ActionNone;
			RETURN_IF_ERROR(context.m_status_icon_name.Set("Unite Enabled Large"));
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_RUNNING, context.m_status_text);
			break;
		}
	case OperaAccountManager::Disabled:
		{
			if (m_webserver_ui->GetCurrentErrorDescr())
			{
				RETURN_IF_ERROR(context.m_status_icon_name.Set("Unite Failed Large")); // todo: use disabled error icon instead
				break;
			}
			// else: fall through
		}
	case OperaAccountManager::Busy: // should we treat busy state differently here?
	case OperaAccountManager::Offline: // todo: decide what to do with these states
	default:
		{
			context.m_suggested_action = WebServerErrorDescr::ActionNone;
			RETURN_IF_ERROR(context.m_status_icon_name.Set("Unite Disabled Large"));
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_DISABLED, context.m_status_text);
		}
	}

	context.m_services_running = GetRunningServicesCount();

#ifdef WEBSERVER_CAP_STATS_TRANSFER
	context.m_size_uploaded = GetBytesUploaded();
	context.m_size_downloaded = GetBytesDownloaded();
#endif // WEBSERVER_CAP_STATS_TRANSFER

#ifdef WEBSERVER_CAP_STATS_NUM_USERS
	context.m_people_connected = GetLastUsersCount(60 * WebServerStatusDialog::S_NUM_MIN);
#endif // WEBSERVER_CAP_STATS_NUM_USERS

	context.m_direct_access_address.Set(m_direct_access_address);

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
WebServerManager::ShowSetupWizard(BOOL * webserver_enabled)
{
	OperaAccountContext * context = m_opera_account_manager->GetAccountContext();
	if (!m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
	{
		context->SetAccountUsed(FALSE);
	}

	return m_webserver_ui->ShowSetupWizard(this, webserver_enabled, GetWebserverSettings(), m_opera_account_manager, context);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
WebServerManager::ShowSettingsDialog()
{
	OperaAccountContext * context = m_opera_account_manager->GetAccountContext();
	if (!m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
	{
		context->SetAccountUsed(FALSE);
	}

	return m_webserver_ui->ShowSettingsDialog(this, GetWebserverSettings(), m_opera_account_manager, context);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ INT32
WebServerManager::GetRunningServicesCount()
{
	return g_desktop_gadget_manager->GetRunningServicesCount();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS WebServerManager::LoadCustomDefaultServices()
{
	return LoadDefaultServicesFromFolder(OPFILE_UNITE_CUSTOM_PACKAGE_FOLDER);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS WebServerManager::LoadDefaultServices()
{
	return LoadDefaultServicesFromFolder(OPFILE_UNITE_PACKAGE_FOLDER);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
WebServerManager::URLNeedsDelayForWebServer(const OpStringC & url_str)
{
	if (!g_webserver_manager->IsFeatureAllowed())
		return FALSE;

	if (m_enabling || // either the webserver is currently enabling
		(!g_webserver_manager->IsFeatureEnabled() && m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable))) // or it's supposed to be enabled any second
	{
		URL url = g_url_api->GetURL(url_str.CStr());
		const ServerName * url_name = static_cast<const ServerName *>(url.GetAttribute(URL::KServerName, NULL));

		if (url_name && url_name->GetUniName().HasContent())
		{
			OpStringC unite_url(url_name->GetUniName());

			// check if it's an admin URL
			const uni_char * admin_home_address = GetRootServiceAddress(TRUE);
			if (admin_home_address && unite_url.Find(admin_home_address) == 0)
			{
				return TRUE;
			}

			// check if it's an visitor URL
			const uni_char * home_address = GetRootServiceAddress(FALSE);
			if (home_address && unite_url.Find(home_address) == 0)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
WebServerManager::AddAutoLoadWindow(unsigned long window_id, const OpStringC & url_str)
{
	OpString service_address;
	OP_STATUS stat = GetServiceAddressFromPath(url_str, service_address);
	if (OpStatus::IsSuccess(stat))
	{
		ServiceWindowMap * window_map;
		stat = m_auto_load_windows.GetData(service_address.CStr(), &window_map);

		if (OpStatus::IsSuccess(stat))
		{
			OP_ASSERT(window_map);
			RETURN_IF_ERROR(window_map->window_ids.Add(window_id));
		}
		else
		{
			ServiceWindowMap * data = OP_NEW(ServiceWindowMap, ());
			RETURN_IF_ERROR(data->service_id.Set(service_address.CStr()));
			RETURN_IF_ERROR(data->window_ids.Add(window_id));
			RETURN_IF_ERROR(m_auto_load_windows.Add(data->service_id.CStr(), data));
		}
		return OpStatus::OK;
	}
	return stat;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS WebServerManager::LoadDefaultServicesFromFolder(OpFileFolder opfolder)
{
	if (!IsInited())
	{
		// seems that you tried to call an un-initialized webserver manager
		// might be that the ui-pref "PrefsCollectionUI::EnableUnite" is set to false, but you're still trying to do something with the manager anyway
		OP_ASSERT(FALSE);
	}

	if (OpFolderLister* folder_lister = OpFile::GetFolderLister(opfolder, UNI_L("*.ua"))) // Huh?? Shouldn't this be opfolder too????
	{
		BOOL more = folder_lister->Next();
		while (more)
		{
			OpString service_path;
			service_path.Set(folder_lister->GetFileName());

			if (service_path.IsEmpty() || (opfolder == OPFILE_UNITE_PACKAGE_FOLDER &&
										   service_path.Compare(UNI_L("home.ua")) == 0))
				// we don't want to add the home service here, it's done another place
			{
				more = folder_lister->Next();
				continue;
			}

			// insert the path before the filename so that it's a valid path to the service
			OpString tmp_storage;
			service_path.Insert(0, g_folder_manager->GetFolderPathIgnoreErrors(opfolder, tmp_storage));

			// we have a unite service we want to add
			INT32 item_id;
			OpString urlstring; // orig URL is empty for default services
			m_hotlist_manager->NewUniteService(service_path,urlstring,&item_id,FALSE,FALSE,NULL,NULL,NULL, NULL);

			HotlistModelItem * item =  m_hotlist_manager->GetItemByID(item_id);
			if (item)
				(static_cast<UniteServiceModelItem *>(item))->SetNeedsConfiguration(TRUE);

			more = folder_lister->Next();
		}

		OP_DELETE(folder_lister);

	}
	return OpStatus::OK;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
const WebServerSettingsContext &
WebServerManager::GetWebserverSettings(BOOL force_enable)
{
	if (force_enable)
	{
		m_settings_context.SetIsFeatureEnabled(TRUE);
	}
	else
	{
		m_settings_context.SetIsFeatureEnabled(IsFeatureEnabled());
	}

	if (m_settings_context.GetDeviceName().IsEmpty())
	{
		OpString device_name;
		m_webserver_prefs->GetStringPrefL(PrefsCollectionWebserver::WebserverDevice, device_name);
		if(device_name.HasContent())
		{
			m_settings_context.SetDeviceName(OpStringC(device_name.CStr()));
		}
	}

#if defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_ENABLED)
	m_settings_context.SetUPnPEnabled(m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UPnPEnabled));
#endif // defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_ENABLED)

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	m_settings_context.SetASDEnabled(m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::ServiceDiscoveryEnabled));
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#if defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED)
	m_settings_context.SetUPnPServiceDiscoveryEnabled(m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled));
#endif // defined(UPNP_SUPPORT) && defined(PREFS_CAP_UPNP_DISCOVERY_ENABLED)

#if defined(WEBSERVER_CAP_SET_VISIBILITY)
	HotlistModel* unite_model = m_hotlist_manager->GetUniteServicesModel();
	UniteServiceModelItem* root_service = 0;
	if (unite_model && unite_model->GetRootService())
	{
		root_service = static_cast<UniteServiceModelItem*>(unite_model->GetRootService());
		if (root_service)
		{
			OpGadget * service = root_service->GetOpGadget();
			if (service)
			{
				m_settings_context.SetVisibleRobotsOnHome(service->IsVisibleRobots());
			}
		}
	}
#endif // WEBSERVER_CAP_SET_VISIBILITY

	m_settings_context.SetUploadSpeed(m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::WebserverUploadRate));
	m_settings_context.SetPort(m_webserver_prefs->GetIntegerPref(PrefsCollectionWebserver::WebserverPort));

	if (m_settings_context.GetDeviceNameSuggestions()->GetCount() == 0)
	{
		OpString *suggestion = OP_NEW(OpString, ());
		suggestion->Set(UNI_L("home"));
		m_settings_context.AddDeviceNameSuggestion(suggestion);

		suggestion = OP_NEW(OpString, ());
		suggestion->Set(UNI_L("work"));
		m_settings_context.AddDeviceNameSuggestion(suggestion);

		suggestion = OP_NEW(OpString, ());
		suggestion->Set(UNI_L("office"));
		m_settings_context.AddDeviceNameSuggestion(suggestion);

		suggestion = OP_NEW(OpString, ());
		suggestion->Set(UNI_L("notebook"));
		m_settings_context.AddDeviceNameSuggestion(suggestion);

		suggestion = OP_NEW(OpString, ());
		suggestion->Set(UNI_L("school"));
		m_settings_context.AddDeviceNameSuggestion(suggestion);
	}

	return m_settings_context;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef UPNP_SUPPORT

DesktopUPnPListener::DesktopUPnPListener(WebServerManager * webserver_manager, UPnP *upnp_father)
   : UPnPLogic(upnp_father, UPNP_DISCOVERY_OPERA_SEARCH_TYPE)
   , m_webserver_manager(webserver_manager)
{
	OP_ASSERT(m_webserver_manager);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
DesktopUPnPListener::HandlerFailed(UPnPXH_Auto *child, const char *mex)
{
	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
DesktopUPnPListener::OnNewDevice(UPnPDevice * known_device)
{
	if (known_device && m_webserver_manager)
	{
		UPnPServicesProvider * prov = known_device->GetProvider();

		if (prov)
		{
			KnownDeviceMenuEntry * entry = NULL;
			BOOL device_found = FALSE;

			const uni_char * dev_type = prov->GetAttributeValueUni(UPnPServicesProvider::DEVICE_TYPE);
			if (dev_type && uni_strcmp(UNI_L(UPNP_DISCOVERY_OPERA_SEARCH_TYPE), dev_type) == 0)
			{
				// set device info (update device info for existing devices)
				const uni_char * device_name = prov->GetAttributeValueUni(UPnPServicesProvider::UNITE_DEVICE);
				const uni_char * user_name = prov->GetAttributeValueUni(UPnPServicesProvider::UNITE_USER);

				OpString desc_str;
				desc_str.AppendFormat(UNI_L("%s @ %s"), user_name, device_name);

				// check if we already have an entry for this device
				for (UINT32 i = 0; i < m_known_devices.GetCount(); i++)
				{
					KnownDeviceMenuEntry * tmp_entry = m_known_devices.Get(i);
					if (tmp_entry->m_device == known_device ||
						tmp_entry->m_description_string.Compare(desc_str) == 0)
					{
						entry = m_known_devices.Get(i);
						device_found = TRUE;
						break;
					}
				}
				const uni_char * presentation_url = prov->GetAttributeValueUni(UPnPServicesProvider::PRESENTATION_URL);
				if (presentation_url != NULL && uni_strlen(presentation_url) > 0)
				{
					if (!device_found)
					{
						OP_ASSERT(!entry);
						entry = OP_NEW(KnownDeviceMenuEntry, ());
					}

					OP_ASSERT(entry);
					entry->m_description_string.Set(desc_str.CStr());
					entry->m_url.Set(presentation_url);
					entry->m_device = known_device;

					if (!device_found)
					{
						m_known_devices.Add(entry);

#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
						if (m_webserver_manager->IsLocalDiscoveryNotificationsEnabled())
						{
							m_webserver_manager->SetHasDiscoveredServices(TRUE);
						}
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT
					}
				}
				// remove devices that were updated with empty presentation URL
				// is that the expected behavior?
				else if (device_found && entry)
				{
					OpStatus::Ignore(m_known_devices.RemoveByItem(entry));
					OP_DELETE(entry);
					entry = NULL;
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
DesktopUPnPListener::OnRemoveDevice(UPnPDevice * known_device)
{
	if (m_webserver_manager)
	{
		for (UINT32 i = 0; i < m_known_devices.GetCount(); i++)
		{
			if (m_known_devices.Get(i)->m_device == known_device)
			{
				m_known_devices.Delete(i);
				if (m_known_devices.GetCount() == 0)
				{
					m_webserver_manager->SetHasDiscoveredServices(FALSE);
				}
				break;
			}
		}
	}
}

#endif // UPNP_SUPPORT

#endif // WEBSERVER_SUPPORT
