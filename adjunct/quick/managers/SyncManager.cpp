
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Adam Minchinton (adamm)
 * @author Co-owner: Karianne Ekern  (karie)
 *
 */

#include "core/pch.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/SyncShutdownProgressDialog.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/FeatureManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/sync/SyncSpeedDialEntries.h"
#include "adjunct/quick/sync/view/SyncSetupWizard.h"
#include "adjunct/quick/sync/view/SyncStatusDialog.h"
#include "adjunct/quick/sync/view/SyncOldPasswordMissing.h"
#include "adjunct/quick/sync/view/SyncPasswordImprovementDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"


#include "modules/content_filter/content_filter.h"
#include "modules/history/direct_history.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_util.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"

#ifdef MSWIN
#include "platforms/windows/user_fun.h"
#endif

#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#elif defined(WIN_CE)
# include "platforms/win_ce/res/buildnum.h"
#else
# pragma error("include the file where VER_BUILD_NUMBER is defined")
#endif

#define STATUS_UPDATED_INTERVAL  (10*1000)      // let items updated status text stay for 10 seconds
#define SILENT_BUSY_STATUS_INTERVAL (3*1000)	// don't show busy status as long as it's below that time
#define SHUTDOWN_WAIT_INTERVAL	  (5*1000)      // Show the dialog after 5 seconds
#define DEBUG_SYNCED_ITEMS

#define SYNC_WELCOME_URL	UNI_L("http://www.opera.com/link/desktop/")

/***************************************************************************
 **
 ** SyncManager
 **
 ** Currently SyncManager controls whether bookmarksSyncer, speeddialSyncer
 ** and searchesSyncer listens to changes in the bookmarks, notes, speed dial
 ** and searches model.
 **
 ** Listening is enabled in SyncManager::Init if SyncState is not 0
 ** else it is enabled in SetLinkEnabled.
 **
 ** If supports is turned off for a data type, listening is
 ** not disabled. Items aren't synced by manual checking of the supports
 **
 **************************************************************************/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SyncManager::SyncManager() :
	m_inited(FALSE),
	m_comm_not_working(FALSE),
	m_is_syncing(FALSE),
	m_is_final_syncing(FALSE),
	m_final_sync_and_close(TRUE),
	m_sync_module_enabled(TRUE),
	m_bookmarks_syncer(NULL),
	m_notes_syncer(NULL)
	,m_speed_dial_syncer(NULL)
	,m_searches_syncer(NULL)
	,m_pref_initialized(FALSE)
	,m_current_error_message()
	,m_status(OperaAccountManager::Disabled)
	,m_msg_handler(NULL)
	,m_suggested_error_action(SyncStatusContext::ActionNone)
	,m_configure_style(NotConfiguring)
	,m_configure_dialog(NULL)
	,m_reencryption_current_context(NULL)
	,m_reencryption_recovery_status(OperaAccountManager::OK)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SyncManager::~SyncManager()
{
	if(g_sync_coordinator)
	{
		g_sync_coordinator->RemoveSyncUIListener(this);

		g_sync_coordinator->RemoveSyncDataClient(m_bookmarks_syncer, OpSyncDataItem::DATAITEM_BOOKMARK);
		g_sync_coordinator->RemoveSyncDataClient(m_notes_syncer, OpSyncDataItem::DATAITEM_NOTE);
		g_sync_coordinator->RemoveSyncDataClient(directHistory, OpSyncDataItem::DATAITEM_TYPED_HISTORY);
		g_sync_coordinator->RemoveSyncDataClient(g_urlfilter, OpSyncDataItem::DATAITEM_URLFILTER);
		g_sync_coordinator->RemoveSyncDataClient(m_searches_syncer, OpSyncDataItem::DATAITEM_SEARCH);
		g_sync_coordinator->RemoveSyncDataClient(m_speed_dial_syncer, OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS);
		g_sync_coordinator->RemoveSyncDataClient(m_speed_dial_syncer, OpSyncDataItem::DATAITEM_SPEEDDIAL_2);


		g_sync_coordinator->Cleanup();
	}

#ifdef DEBUG_LIVE_SYNC
	// Make sure the debug file is closed before attempting to clean up guest folders
	m_log_file.Close();
#endif // DEBUG_LIVE_SYNC

	OP_DELETE(m_bookmarks_syncer);
	m_bookmarks_syncer = 0;

	OP_DELETE(m_notes_syncer);
	m_notes_syncer = 0;

	OP_DELETE(m_speed_dial_syncer);
	m_speed_dial_syncer = 0;

	OP_DELETE(m_searches_syncer);
	m_searches_syncer = 0;

	OP_DELETE(m_msg_handler);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Init must be called before the SyncManager can be used. 
//  Currently done in the OperaAccountManager
//

OP_STATUS SyncManager::Init()
{
	// TODO shuais FIXME Only write queue to file if sync has been used
	int sync_used = FALSE;
	if (g_pcsync)
		sync_used = g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncUsed);
	if (sync_used)
	   	RETURN_IF_ERROR(g_sync_coordinator->Init(NULL, TRUE));
	else
		RETURN_IF_ERROR(g_sync_coordinator->Init(NULL, FALSE));
	m_state_modifier.Init();

	m_bookmarks_syncer = OP_NEW(SyncBookmarkItems, ());
	if(!m_bookmarks_syncer)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	m_notes_syncer = OP_NEW(SyncNoteItems, ());
	if(!m_notes_syncer)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	m_speed_dial_syncer = OP_NEW(SyncSpeedDialEntries, ());
	if (!m_speed_dial_syncer)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	m_searches_syncer = OP_NEW(SyncSearchEntries, ());
	if (!m_searches_syncer)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// TODO: Should this be done here??
	OpString buildno, opsys, product;

	buildno.Set(VER_BUILD_NUMBER_STR);
	opsys.Set(g_op_system_info->GetPlatformStr());

	opsys.MakeLower();

	OpStatus::Ignore(g_sync_coordinator->SetSystemInformation(SYNC_SYSTEMINFO_BUILD, buildno));
	OpStatus::Ignore(g_sync_coordinator->SetSystemInformation(SYNC_SYSTEMINFO_SYSTEM, opsys));
	
	RETURN_IF_ERROR(product.AppendFormat(UNI_L("Opera Desktop %d.%d"), VER_NUM_MAJOR, VER_NUM_MINOR));
	OpStatus::Ignore(g_sync_coordinator->SetSystemInformation(SYNC_SYSTEMINFO_PRODUCT, product));

// 	if (g_pcsync && !g_sync_manager->m_pref_initialized)
// 	{
// 		g_sync_manager->SetDoCompleteSync(g_pcsync->GetIntegerPref(PrefsCollectionSync::CompleteSync));
// 		g_sync_manager->m_pref_initialized = TRUE;
// 	}

	g_sync_coordinator->SetSyncUIListener(this);

	g_sync_coordinator->SetSyncDataClient(m_bookmarks_syncer, OpSyncDataItem::DATAITEM_BOOKMARK);
	g_sync_coordinator->SetSyncDataClient(m_searches_syncer, OpSyncDataItem::DATAITEM_SEARCH);
	g_sync_coordinator->SetSyncDataClient(g_urlfilter, OpSyncDataItem::DATAITEM_URLFILTER);
	g_sync_coordinator->SetSyncDataClient(m_speed_dial_syncer, OpSyncDataItem::DATAITEM_SPEEDDIAL_2);
	g_sync_coordinator->SetSyncDataClient(m_speed_dial_syncer, OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS);
	g_sync_coordinator->SetSyncDataClient(m_notes_syncer, OpSyncDataItem::DATAITEM_NOTE);
	g_sync_coordinator->SetSyncDataClient(directHistory, OpSyncDataItem::DATAITEM_TYPED_HISTORY);

	SetDefaultSettings();

	m_msg_handler = OP_NEW(MessageHandler, (NULL));
	if(!m_msg_handler)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(m_msg_handler->SetCallBack(this, MSG_QUICK_FEATURE_SETUP_COMPLETED, 0));
	RETURN_IF_ERROR(m_msg_handler->SetCallBack(this, MSG_QUICK_START_CLEAN_FEATURE_SETUP, 0));
	RETURN_IF_ERROR(m_msg_handler->SetCallBack(this, MSG_QUICK_PASSWORDS_RECOVERY_KNOWN, 0));

	// Once eveything is initalised then set the flag to TRUE
	m_inited = TRUE;

	return OpStatus::OK;
}

/***********************************************************************************
 *
 *  SyncManager::EnableIfrequired
 *
 ***********************************************************************************/
// Note: This is exactly the same as a function in WebServerManager -> FIXME 
// (See also ShowLoginDialog and ShowSetupWizard)
OP_STATUS SyncManager::EnableIfRequired(BOOL& has_started)
{

	if (IsLinkEnabled())
	{
		has_started = TRUE;
		return OpStatus::OK;
	}
	else
	{
		has_started = FALSE;

		if (!HasUsedSync())
		{
			return ShowSetupWizard(&has_started);
		}
		else if (!g_desktop_account_manager->GetLoggedIn())
		{
			RETURN_IF_ERROR(ShowLoginDialogAndEnableLink());
		}
		else
		{
			has_started = TRUE;
			SetLinkEnabled(TRUE);
			return TRUE;
		}

	}
	return OpStatus::ERR;
}

/***********************************************************************************
 *  SyncManager::ShowLoginDialogAndEnableLink
 ************************************************************************************/
OP_STATUS
SyncManager::ShowLoginDialogAndEnableLink(DesktopWindow* parent)
{
	BOOL logged_in = FALSE;
	RETURN_IF_ERROR(ShowLoginDialog(&logged_in, parent));
	if (logged_in)
	{
		SetLinkEnabled(TRUE);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 *  SyncManager::ShowLoginDialog
 ************************************************************************************/
OP_STATUS
SyncManager::ShowLoginDialog(BOOL * logged_in, DesktopWindow* parent)
{
	if (m_configure_style == LoginDialog) {
		m_configure_dialog->Activate();
		return OpStatus::OK;
	}
	SyncDialog* wizard = OP_NEW(SyncDialog, (this,
		GetSavedSettings(), 
		g_desktop_account_manager,
		g_desktop_account_manager->GetAccountContext(),
		FALSE));

	if (wizard)
	{
		m_configure_dialog = wizard;
		m_configure_style = LoginDialog;
		DesktopWindow* win = parent ? parent : g_application->GetActiveDesktopWindow();
		RETURN_IF_ERROR(wizard->InitLoginDialog(logged_in, win));
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
 *  SyncManager::ShowSetupWizard
 ************************************************************************************/
OP_STATUS
SyncManager::ShowSetupWizard(BOOL * sync_enabled, SyncSettingsContext * settings_context, OperaAccountContext* account_context)
{
	if (m_configure_style == SetupWizard) {
		m_configure_dialog->Activate();
		return OpStatus::OK;
	}
	SyncDialog *wizard = OP_NEW(SyncDialog, (this,
		settings_context, 
		g_desktop_account_manager,
		account_context,
		TRUE));

	if (wizard)
	{
		if (sync_enabled)
			wizard->SetBlockingForReturnValue(sync_enabled);

		m_configure_dialog = wizard;
		m_configure_style = SetupWizard;

		RETURN_IF_ERROR(wizard->Init(g_application->GetActiveDesktopWindow()));
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
 *  SyncManager::ShowSettingsDialog
 ************************************************************************************/
OP_STATUS
SyncManager::ShowSettingsDialog()
{
	if (m_configure_style == SettingsDialog) {
		m_configure_dialog->Activate();
		return OpStatus::OK;
	}
	SyncDialog* dialog = OP_NEW(SyncDialog, (this,
		GetSavedSettings(), 
		g_desktop_account_manager,
		g_desktop_account_manager->GetAccountContext(),
		FALSE));

	if (dialog)
	{
		m_configure_dialog = dialog;
		m_configure_style = SettingsDialog;

		dialog->Init(g_application->GetActiveDesktopWindow());
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
 *  SyncManager::ShowStatus
 ************************************************************************************/
OP_STATUS
SyncManager::ShowStatus()
{
	SyncStatusContext context;
	switch (m_status)
	{
	case OperaAccountManager::Error:
		{
			if (m_reencryption_current_context)
			{
				SyncOldPasswordMissingDialog* dialog = OP_NEW(SyncOldPasswordMissingDialog, (*this));
				
				if (dialog)
				{
					dialog->Init(g_application->GetActiveDesktopWindow());
					return OpStatus::OK;
				}
				else
					return OpStatus::ERR_NO_MEMORY;
			}

			context.m_suggested_action = (SyncStatusContext::SuggestedAction)m_suggested_error_action;
			context.m_status_icon_name.Set("Link Failed Large");
			context.m_status_text.Set(m_current_error_message.CStr());
			break;
		}
	case OperaAccountManager::Busy: // should we treat busy state differently here?
	case OperaAccountManager::OK:
		{
			context.m_suggested_action = SyncStatusContext::ActionNone;
			context.m_status_icon_name.Set("Link Enabled Large");
			g_languageManager->GetString(Str::D_SYNC_STATUS_DIALOG_RUNNING, context.m_status_text);
			break;
		}
	case OperaAccountManager::Offline: // todo: decide what to do with these states
	case OperaAccountManager::Disabled:
	default:
		{
			context.m_suggested_action = SyncStatusContext::ActionNone;
			context.m_status_icon_name.Set("Link Disabled Large");
			g_languageManager->GetString(Str::D_SYNC_STATUS_DIALOG_DISABLED, context.m_status_text);
		}
	}

	SyncStatusDialog* dialog = OP_NEW(SyncStatusDialog, (*this, context));

	if (dialog)
	{
		dialog->Init(g_application->GetActiveDesktopWindow());
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
**  SyncManager::IsFeatureEnabled
************************************************************************************/
BOOL
SyncManager::IsFeatureEnabled() const
{
	return IsLinkEnabled();
}


/***********************************************************************************
**  SyncManager::EnableFeature
************************************************************************************/
void
SyncManager::EnableFeature(const FeatureSettingsContext* settings)
{
	SetFeatureSettingsWithoutEnabling(settings);
	SetLinkEnabled(TRUE, TRUE);
	BroadcastOnFeatureEnablingSucceeded(); // enabling sync is synchronous
}


/***********************************************************************************
**  SyncManager::DisableFeature
************************************************************************************/
void
SyncManager::DisableFeature()
{
	SetLinkEnabled(FALSE);
}


/***********************************************************************************
**  SyncManager::SetFeatureSettings
************************************************************************************/
void
SyncManager::SetFeatureSettings(const FeatureSettingsContext* context)
{
	SetFeatureSettingsWithoutEnabling(context);

	// Enabled status changed
	if (IsLinkEnabled() != context->IsFeatureEnabled())
	{
	        BOOL was_enabled = IsLinkEnabled();

		SetLinkEnabled(context->IsFeatureEnabled());

		// if Link is being disabled; reset state for all types
		if (was_enabled)
		{
		  // Reset state for each type, but don't reset supports for each type when disabling Link 
		  OpStatus::Ignore(g_sync_coordinator->ResetSupportsState(SYNC_SUPPORTS_MAX, FALSE));
		}
	}

	BroadcastOnFeatureSettingsChangeSucceeded();
}

/***********************************************************************************
**  SyncManager::InvokeMessage
************************************************************************************/
void
SyncManager::InvokeMessage(OpMessage msg, MH_PARAM_1 param_1, MH_PARAM_2 param_2)
{
	m_msg_handler->PostMessage(msg, param_1, param_2);
}

/***********************************************************************************
**  SyncManager::HandleCallback
************************************************************************************/
void SyncManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (static_cast<FeatureType>(par1) == FeatureTypeSync)
	{
		if (msg == MSG_QUICK_FEATURE_SETUP_COMPLETED)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE, 0, SYNC_WELCOME_URL);
		}
		else if (msg == MSG_QUICK_START_CLEAN_FEATURE_SETUP)
		{
			ShowSetupWizard(NULL, GetSavedSettings(), NULL);
		}
		else if (msg == MSG_QUICK_PASSWORDS_RECOVERY_KNOWN)
		{
			SyncPasswordImprovementDialog* dialog = OP_NEW(SyncPasswordImprovementDialog, ());
			if (dialog && OpStatus::IsError(dialog->Init(g_application->GetActiveDesktopWindow(TRUE))))
			{
				OP_DELETE(dialog);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SyncManager::SupportsType(SyncDataType data_type)
{
	switch(data_type)
	{
	case SYNC_PERSONALBAR:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncPersonalbar);
	case SYNC_BOOKMARKS:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncBookmarks);
	case SYNC_SPEEDDIAL:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncSpeeddial);
	case SYNC_NOTES:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncNotes);
#ifdef SUPPORT_SYNC_SEARCHES
	case SYNC_SEARCHES:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncSearches);
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_TYPED_HISTORY
	case SYNC_TYPEDHISTORY:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncTypedHistory);
#endif // SYNC_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case SYNC_URLFILTERS:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncURLFilter);
#endif // SYNC_CONTENT_FILTERS
	case SYNC_PASSWORD_MANAGER:
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncPasswordManager);
	default:
		OP_ASSERT(FALSE);
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


OP_STATUS SyncManager::SetSupportsType(SyncDataType data_type, BOOL setting)
{
	OP_STATUS ret = OpStatus::ERR;

	switch(data_type)
	{
	case SYNC_PERSONALBAR:
		m_settings_context.SetSupportsPersonalbar(setting);
		TRAP(ret, g_pcsync->WriteIntegerL(PrefsCollectionSync::SyncPersonalbar, setting));
		break;
	default:
	  // Reset state if we're disabling a type 
	  if (m_settings_context.GetSupportsType((OpSyncSupports)data_type) && !setting)
	  {
	    OpStatus::Ignore(g_sync_coordinator->ResetSupportsState((OpSyncSupports)data_type, TRUE));
	  }

	  m_settings_context.SetSupportsType((OpSyncSupports)data_type, setting);
	  ret = g_sync_coordinator->SetSupports((OpSyncSupports)data_type, setting);
	}

	return ret;
}

/***********************************************************************************
**  SyncManager::SetFeatureSettingsWithoutEnabling
************************************************************************************/
void
SyncManager::SetFeatureSettingsWithoutEnabling(const FeatureSettingsContext* context)
{
	const SyncSettingsContext* sync_context;
	sync_context = static_cast<const SyncSettingsContext*>(context);
	OP_ASSERT(sync_context);

	if(sync_context)
	{
		SetSupportsType(SYNC_BOOKMARKS, sync_context->SupportsBookmarks());
		SetSupportsType(SYNC_SPEEDDIAL, sync_context->SupportsSpeedDial());
		SetSupportsType(SYNC_PERSONALBAR, sync_context->SupportsPersonalbar());

#ifdef SUPPORT_SYNC_NOTES
		SetSupportsType(SYNC_NOTES, sync_context->SupportsNotes());
#endif // SUPPORT_SYNC_NOTES

#ifdef SYNC_TYPED_HISTORY
		SetSupportsType(SYNC_TYPEDHISTORY, sync_context->SupportsTypedHistory());
#endif // SYNC_TYPED_HISTORY

#ifdef SUPPORT_SYNC_SEARCHES
		SetSupportsType(SYNC_SEARCHES, sync_context->SupportsSearches());
#endif // SUPPORT_SYNC_SEARCHES

#ifdef SYNC_CONTENT_FILTERS
		SetSupportsType(SYNC_URLFILTERS, sync_context->SupportsURLFilter());
#endif // SYNC_CONTENT_FILTERS
		SetSupportsType(SYNC_PASSWORD_MANAGER, sync_context->SupportsPasswordManager());
	}
}

void SyncManager::SetDefaultSettings()
{
	m_settings_context.SetIsFeatureEnabled(IsLinkEnabled());

	m_settings_context.SetSupportsBookmarks(SupportsType(SYNC_BOOKMARKS));
	m_settings_context.SetSupportsSpeedDial(SupportsType(SYNC_SPEEDDIAL));
	m_settings_context.SetSupportsPersonalbar(SupportsType(SYNC_PERSONALBAR));

#ifdef SUPPORT_SYNC_NOTES
	m_settings_context.SetSupportsNotes(SupportsType(SYNC_NOTES));
#endif // SUPPORT_SYNC_NOTES

#ifdef SYNC_TYPED_HISTORY
	m_settings_context.SetSupportsTypedHistory(SupportsType(SYNC_TYPEDHISTORY));
#endif // SYNC_TYPED_HISTORY

#ifdef SUPPORT_SYNC_SEARCHES
	m_settings_context.SetSupportsSearches(SupportsType(SYNC_SEARCHES));
#endif // SUPPORT_SYNC_SEARCHES

#ifdef SYNC_CONTENT_FILTERS
	m_settings_context.SetSupportsURLFilter(SupportsType(SYNC_URLFILTERS));
#endif // SYNC_CONTENT_FILTERS

	m_settings_context.SetSupportsPasswordManager(SupportsType(SYNC_PASSWORD_MANAGER));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SyncManager::LogError(OpSyncError error)
{
	OpString context;
	INT32 error_code = OAM_SYNC_ERROR;

	m_current_error_message.Empty();

	switch (error)
	{
		case SYNC_ERROR:
			context.Set(UNI_L("SYNC_ERROR"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ERROR_PARSER:
			context.Set(UNI_L("SYNC_ERROR_PARSER"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_PARSER, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ERROR_MEMORY:
			context.Set(UNI_L("SYNC_ERROR_MEMORY"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_MEMORY, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ERROR_SERVER:
			context.Set(UNI_L("SYNC_ERROR_SERVER"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_SERVER, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ERROR_COMM_FAIL:
			context.Set(UNI_L("SYNC_ERROR_COMM_FAIL"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_COMM_FAIL, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_NETWORK;
			m_suggested_error_action = SyncStatusContext::ActionNone;
			break;
		
		case SYNC_ERROR_COMM_ABORTED:
			context.Set(UNI_L("SYNC_ERROR_COMM_ABORTED"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_COMM_ABORTED, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_NETWORK;
			m_suggested_error_action = SyncStatusContext::ActionNone;
			break;
		
		case SYNC_ERROR_COMM_TIMEOUT:
			context.Set(UNI_L("SYNC_ERROR_COMM_TIMEOUT"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_COMM_TIMEOUT, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_NETWORK;
			m_suggested_error_action = SyncStatusContext::ActionNone;
			break;
		
		case SYNC_ERROR_INVALID_REQUEST:
			context.Set(UNI_L("SYNC_ERROR_INVALID_REQUEST"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_REQUEST, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
			break;
		
		case SYNC_ERROR_PROTOCOL_VERSION:
			context.Set(UNI_L("SYNC_ERROR_PROTOCOL_VERSION"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_PROTOCOL_VERSION, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ERROR_CLIENT_VERSION:
			context.Set(UNI_L("SYNC_ERROR_CLIENT_VERSION"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_CLIENT_VERSION, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ERROR_INVALID_STATUS:
			context.Set(UNI_L("SYNC_ERROR_INVALID_STATUS"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_STATUS, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
		break;
		
		case SYNC_ERROR_INVALID_BOOKMARK:
			context.Set(UNI_L("SYNC_ERROR_INVALID_BOOKMARK"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_BOOKMARK, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
		break;
		
#ifdef SUPPORT_SYNC_SPEED_DIAL
		case SYNC_ERROR_INVALID_SPEEDDIAL:
			context.Set(UNI_L("SYNC_ERROR_INVALID_SPEEDDIAL"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_SPEEDDIAL, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
			break;
#endif // SUPPORT_SYNC_SPEED_DIAL
		
#ifdef SUPPORT_SYNC_NOTES
		case SYNC_ERROR_INVALID_NOTE:
			context.Set(UNI_L("SYNC_ERROR_INVALID_NOTE"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_NOTE, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
		break;
#endif // SUPPORT_SYNC_NOTES

// #ifdef SUPPORT_SYNC_TYPED_HISTORY
// 		case SYNC_ERROR_INVALID_TYPED_HISTORY:
// 			context.Set(UNI_L("SYNC_ERROR_INVALID_TYPED_HISTORY"));
// 			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_TYPED_HISTORY, m_current_error_message);
//			m_suggested_error_action = SyncStatusContext::ActionNone;
// 		break;
// #endif // SUPPORT_SYNC_SPEED_DIAL

#ifdef SUPPORT_SYNC_SEARCHES
#if 0
		// AJMTODO: Add error / Sync module get error
	    case SYNC_ERROR_INVALID_SEARCH:
			context.Set(UNI_L("SYNC_ERROR_INVALID_SEARCH"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_NOTE, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
//			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
#endif
#endif // SUPPORT_SYNC_SEARCHES

		
		case SYNC_ERROR_INVALID_CONTACT:
			context.Set(UNI_L("SYNC_ERROR_INVALID_CONTACT"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_INVALID_CONTACT, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
		break;
		
		case SYNC_ERROR_SYNC_DISABLED:
			context.Set(UNI_L("SYNC_ERROR_SYNC_DISABLED"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_SYNC_DISABLED, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
		break;
		
		case SYNC_ERROR_SYNC_IN_PROGRESS:
			context.Set(UNI_L("SYNC_ERROR_SYNC_IN_PROGRESS"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_SYNC_IN_PROGRESS, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_OTHER;
			m_suggested_error_action = SyncStatusContext::ActionSyncNow;
		break;
		
		case SYNC_ACCOUNT_AUTH_FAILURE:
			context.Set(UNI_L("SYNC_ACCOUNT_AUTH_FAILURE"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_AUTH_FAILURE, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_AUTH;
			m_suggested_error_action = SyncStatusContext::ActionChangeUser;
		break;
		
		case SYNC_ACCOUNT_USER_UNAVAILABLE:
			context.Set(UNI_L("SYNC_ACCOUNT_USER_UNAVAILABLE"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_USER_UNAVAILABLE, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_AUTH;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ACCOUNT_AUTH_INVALID_KEY:
			context.Set(UNI_L("SYNC_ACCOUNT_AUTH_INVALID_KEY"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_AUTH_INVALID_KEY, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_AUTH;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		case SYNC_ACCOUNT_USER_BANNED:
			context.Set(UNI_L("SYNC_ACCOUNT_USER_BANNED"));
			g_languageManager->GetString(Str::S_OPERA_LINK_ERROR_USER_BANNED, m_current_error_message);
			error_code |= OAM_ERROR_TYPE_AUTH;
			m_suggested_error_action = SyncStatusContext::ActionNone;
		break;
		
		default:
			// Unhandled so do not log to console
			return;
		break;
	}

	// Convert from enum to error_code
	error_code |= (INT32)error;

#ifdef OPERA_CONSOLE
	// Build a console error message
#ifdef CON_CAP_LINK_UNITE
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaLink, OpConsoleEngine::Error);
#else
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Error);
#endif // CON_CAP_LINK_UNITE

	// Set the error information
	OpStatus::Ignore(log_message.context.Set(context));
	OpStatus::Ignore(log_message.message.Set(m_current_error_message));
	log_message.error_code = error_code;

	// Send the error to the console
	g_console->PostMessageL(&log_message);
#endif // OPERA_CONSOLE
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SyncManager::SetSyncActive(BOOL active)
{
	BOOL sync_active = CanSync() && active;

	OpStatus::Ignore(g_sync_coordinator->SetSyncActive(sync_active));
	UpdateStatus(sync_active ? OperaAccountManager::OK : OperaAccountManager::Disabled);
	sync_active ?  
		m_state_modifier.SetWidgetStateToEnabled() :
		m_state_modifier.SetWidgetStateToDisabled();
}

/******************************************************************
 **
 ** BackupFile
 **
 ** Backup file filename to make rollback possible
 **
 ******************************************************************/

OP_STATUS SyncManager::BackupFile(OpFile& file, const uni_char* suffix)
{
	OpString backupFilename;
	RETURN_IF_ERROR(backupFilename.Set(file.GetFullPath()));

	if (!suffix || !*suffix)
		suffix = UNI_L(".pre_sync");

	//	backupFilename.
	backupFilename.Append(suffix);

	BOOL exists = FALSE;
	RETURN_IF_ERROR(file.Exists(exists));
	if (!exists)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	exists = FALSE;
	OpFile backupFile;

	RETURN_IF_ERROR(backupFile.Construct(backupFilename.CStr()));
	RETURN_IF_ERROR(backupFile.Exists(exists));

	if (exists)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	// TODO: Add handle file exists 
	OP_STATUS status = backupFile.CopyContents(&file, TRUE );
	if (OpStatus::IsError(status))
	{
		OP_ASSERT(FALSE);
		return status;
	}

	return OpStatus::OK;
}

/*********************************************************************
 **
 ** SetConfiguring
 **
 *********************************************************************/

void SyncManager::SetConfiguring(BOOL configuring)
{
	if (!configuring) {
		m_configure_style = NotConfiguring;
		m_configure_dialog = NULL;
	}
}

/*********************************************************************
 **
 ** SetConfiguring
 **
 *********************************************************************/

BOOL SyncManager::IsConfiguring()
{
	return m_configure_style != NotConfiguring;
}

/*********************************************************************
 **
 ** HasUsedSync
 **
 ** @return TRUE if user has used sync, i.e. if sync state != 0
 **
 *********************************************************************/

BOOL SyncManager::HasUsedSync()
{
	if (g_pcsync)
	{
		return g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncUsed);
	}
	return FALSE; 
}

////////////////////////////////////////////////////////////////////////////////////////

void SyncManager::SetLinkEnabled(BOOL enabled, BOOL auto_login, BOOL save_pref)
{
	m_settings_context.SetIsFeatureEnabled(enabled);

// todo: offline, kiosk mode

	g_sync_coordinator->SetUseDiskQueue(TRUE);
	
	if (IsLinkEnabled())
	{
		OpString state;
		TRAPD(err, g_pcsync->GetStringPrefL(PrefsCollectionSync::SyncClientState, state));
		TRAP(err, g_pcsync->WriteIntegerL(PrefsCollectionSync::SyncUsed, TRUE));
	}
	
	// Set the preference
	if (save_pref)
	{
		TRAPD(err, g_pcsync->WriteIntegerL(PrefsCollectionSync::SyncEnabled, IsLinkEnabled()));
	}

	// We set active after sending the data to the module so that the module sends the items 
	// to the server straight away.
	SetSyncActive(enabled);

	// Send the status event
	UpdateStatus(auto_login ? OperaAccountManager::OK : IsLinkEnabled() ? OperaAccountManager::Busy : OperaAccountManager::Disabled);
}

///////////////////////////////////////////////////////////////////////////

void
SyncManager::UpdateOffline(BOOL offline)
{
	if (g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncEnabled) == TRUE)
	{
		SetSyncActive(!offline);
	}
}

///////////////////////////////////////////////////////////////////////////

BOOL SyncManager::CanSync()
{
	// No syncing in kiosk mode or while Work Offline is enabled
	if (KioskManager::GetInstance()->GetEnabled() ||
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		{
			return FALSE;
		}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////

void SyncManager::UpdateStatus(OperaAccountManager::OAM_Status status, BOOL force_now)
{
	OperaAccountManager::OAM_Status old_status = m_status;
	
	if (m_reencryption_current_context)
	{
		m_reencryption_recovery_status = status;
		if (status == OperaAccountManager::OK)
			status = OperaAccountManager::Error;
	}

	// Must do this first do the internal member has been updated
	m_status = status;
	
	// Update the input states if the status changes
	if (old_status != m_status || force_now)
	{
		switch (status)
		{
		case OperaAccountManager::Error:
			{
				m_state_modifier.SetWidgetStateToError();
			}
			break;

		case OperaAccountManager::Busy:
			{
				if (!force_now && old_status == OperaAccountManager::OK)
				{
					// don't show busy state immediately, wait a while
					if (!m_status_timer.get())
					{
						// Create the timer.
						OpTimer *timer = OP_NEW(OpTimer, ());
						OP_ASSERT(timer != NULL);

						m_status_timer = timer;
						m_status_timer->SetTimerListener(this);
					}
					m_status_timer->Start(SILENT_BUSY_STATUS_INTERVAL);
				}
				else
				{
					m_state_modifier.SetWidgetStateToBusy();
				}
			}
			break;

		case OperaAccountManager::OK:
			{
				m_state_modifier.SetWidgetStateToEnabled();
			}
			break;

		case OperaAccountManager::Offline:
		case OperaAccountManager::Disabled:
			{
				m_state_modifier.SetWidgetStateToDisabled();
			}
			break;

		}
		g_input_manager->UpdateAllInputStates();
	}
}


// OpTimerListener interface implementation


/*************************************************************************************
 **
 ** OnTimeOut
 **
 ** @param timer - m_timer:				Start synchronization and start new timer interval
 **                m_status_timer:		Set status text back to m_current_permanent_status
 **				   m_shutdown_timer:	Set for shutting down the application
 **
 **
 *************************************************************************************/

void SyncManager::OnTimeOut(OpTimer* timer)
{
	if (timer == m_status_timer.get())
	{
		timer->Stop();
		if (m_reencryption_current_context)
			UpdateStatus(m_reencryption_recovery_status, TRUE);
		else
			UpdateStatus(m_status, TRUE);
	}
	if (timer == m_shutdown_timer.get())
	{
		if(g_application)
		{
#ifdef SY_CAP_HASQUEUEDITEMS
			if(g_sync_coordinator && g_sync_coordinator->HasQueuedItems())
#endif // SY_CAP_HASQUEUEDITEMS
			{	
			  if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
			    return;

			  // Show the dialog
				SyncShutdownProgressDialog *dialog = OP_NEW(SyncShutdownProgressDialog, ());
				if (dialog)
					dialog->Init(g_application->GetActiveDesktopWindow(), 0, TRUE);
			}
		}
	}
}

/***************************************************************************
 **
 ** OnSyncStarted
 **
 **
 **
 **************************************************************************/
// Status text update etc. will move in here for the new world
void SyncManager::OnSyncStarted(BOOL items_sent)
{
	// If syncing is done in the module, 
	// Ensure we won't start a sync from the client 
	m_is_syncing = TRUE;

	// Send the status event on if items are sent
	if (items_sent)
		UpdateStatus(OperaAccountManager::Busy);
}

/***************************************************************************
 **
 ** OnSyncError
 **
 ** Sets status text
 **
 **************************************************************************/
// TODO: Need to handle auth_filaure, user_unavail, user_banned, invalid_key too?
void SyncManager::OnSyncError(OpSyncError error, const OpStringC& error_msg )
{
	// Set the flag to say that we had a first successful reply from the sync module
	m_is_syncing = FALSE;

	// Stop the shutdown timer if it's running
	if (m_shutdown_timer.get())
		m_shutdown_timer->Stop();

	// Log the error (LogError will handle when to send the error to the console)
	LogError(error);

	// Set the commuication working flag
	switch (error)
	{
		// These are all the errors that cause the communication not working flag to be set to true
		case SYNC_ERROR_COMM_FAIL:
		case SYNC_ERROR_COMM_ABORTED:
		case SYNC_ERROR_COMM_TIMEOUT:
			m_comm_not_working = TRUE;
		break;

		default:
			// Otherwise reset the commuication to working
			m_comm_not_working = FALSE;
		break;
	}

	// If this was a final sync and guest mode then we need to confirm before exiting
	if (m_is_final_syncing)
	{
		m_is_final_syncing = FALSE;

		// Logout so that the EXIT will work this time (this won't effect the autologin as it's not saved to the preference file)
		SetLinkEnabled(FALSE, FALSE, FALSE);

		if (m_final_sync_and_close)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
			return;
		}
	}

	// Send the status event making sure that the user is still logged in
	UpdateStatus(IsLinkEnabled() ? OperaAccountManager::Error : OperaAccountManager::Disabled);
}



/**********************************************************************************
 *
 * OnSyncFinished
 *
 *
 *
 **********************************************************************************/

// OpSyncUIListener
void SyncManager::OnSyncFinished(OpSyncState& sync_state)
{
	// Reset the communication to working
	m_comm_not_working = FALSE;

	// Mark syncing as complete
	m_is_syncing = FALSE;

	// If this was a final sync we don't start again we send the message to Opera to exit!
	if (m_is_final_syncing)
	{
		m_is_final_syncing = FALSE;

		// Logout so that the EXIT will work this time (this won't effect the autologin as it's not saved to the preference file)
		SetLinkEnabled(FALSE, FALSE, FALSE);

		// Exit if required
		if (m_final_sync_and_close)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
			return;
		}
	}

	// Make sure that the user is still logged in
	if (IsLinkEnabled())
	{
		// Send the status event
		UpdateStatus(OperaAccountManager::OK);
	}
	else
	{
		// Send the status event disabled if the user is now logged out
		UpdateStatus(OperaAccountManager::Disabled);
	}

}

// OpSyncUIListener
void SyncManager::OnSyncReencryptEncryptionKeyFailed(ReencryptEncryptionKeyContext* context)
{
	BOOL retry = m_reencryption_current_context != NULL;

	if (!retry)
	{
		m_reencryption_recovery_status = m_status;
		if (IsLinkEnabled()) 
			UpdateStatus(OperaAccountManager::Error);
	}
	m_reencryption_current_context = context;
	if (retry)
	{
		g_main_message_handler->PostMessage(MSG_QUICK_PASSWORDS_RECOVERY_KNOWN, 0, FALSE);
	}
}

// OpSyncUIListener
void SyncManager::OnSyncReencryptEncryptionKeyCancel(ReencryptEncryptionKeyContext* context)
{
	OP_ASSERT(m_reencryption_current_context == context || NULL == context);
	if ((m_reencryption_current_context != context) && (NULL != context)) return;
	m_reencryption_current_context = NULL;
	UpdateStatus(IsLinkEnabled() ? m_reencryption_recovery_status : OperaAccountManager::Disabled);
	if (context == NULL)
		g_main_message_handler->PostMessage(MSG_QUICK_PASSWORDS_RECOVERY_KNOWN, 0, TRUE);
}

void SyncManager::RetryPasswordRecovery(const OpStringC& old_pass)
{
	if (m_reencryption_current_context)
		m_reencryption_current_context->DecryptWithPassword(old_pass);
}

void SyncManager::AbandonPasswordRecovery()
{
	if (m_reencryption_current_context)
	{
		ReencryptEncryptionKeyContext* tmp = m_reencryption_current_context;
		m_reencryption_current_context = NULL;
		UpdateStatus(IsLinkEnabled() ? m_reencryption_recovery_status : OperaAccountManager::Disabled);

		tmp->CreateNewEncryptionKey();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SyncManager::SyncNow(SyncManager::SyncType sync_type, BOOL force_complete_sync)
{
	// Don't sync if not logged in, can't sync or you've already started a sync
	if (!IsLinkEnabled() || !CanSync() || (m_is_syncing && sync_type == Now))
	{
		return OpStatus::OK;
	}

	if (force_complete_sync)
	{
		TRAPD(err, g_pcsync->WriteIntegerL(PrefsCollectionSync::CompleteSync, TRUE));
	}

	// Set the final syncing and closing flags based on the sync requested
	switch (sync_type)
	{
		case Now:
			m_is_final_syncing = FALSE;
			m_final_sync_and_close = FALSE;
		break;

		case Exit:
			m_is_final_syncing = TRUE;
			m_final_sync_and_close = TRUE;
		break;

		case Logout:
			m_is_final_syncing = TRUE;
			m_final_sync_and_close = FALSE;
		break;
	};

	// Start the timer for the shutdown progress dialog to show only if shutting down
	if (m_final_sync_and_close)
	{
		if(!m_shutdown_timer.get())
		{
			// Create the timer.
			OpTimer *timer = OP_NEW(OpTimer, ());
			if (timer == NULL)
				OP_ASSERT(FALSE);
			m_shutdown_timer = timer;
			m_shutdown_timer->SetTimerListener(this);
		}
		m_shutdown_timer->Start(SHUTDOWN_WAIT_INTERVAL);
	}

	// Make sure notes are sent off, for SyncNow, Exit and Logout
	m_notes_syncer->SendModifiedNotes();

	// If this is one of the final syncs and we are already syncing exit now.
	// We exit late since this turns an existing sync into a final sync
	// and starts the dialog timer
	if (m_is_syncing && sync_type != Now)
		return OpStatus::OK;

	// Send the status event
	UpdateStatus(OperaAccountManager::Busy, TRUE);
	
	// Set the syncing flag
	m_is_syncing = TRUE;

	// Call the module to sync now
	return g_sync_coordinator->SyncNow();
}


#ifdef DEBUG_LIVE_SYNC

void SyncManager::SyncLog(const uni_char *format, ...)
{
	OpString out;
	va_list args;

	if(!m_log_file.IsOpen())
	{
		OpString logfile_name;

		OpString tmp_storage;
		logfile_name.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFS_FOLDER, tmp_storage));
		logfile_name.Append(UNI_L("link"));
		logfile_name.Append(PATHSEP);
		logfile_name.Append(UNI_L("link.log"));

		if(OpStatus::IsSuccess(m_log_file.Construct(logfile_name.CStr())))
		{
			OpStatus::Ignore(m_log_file.Open(OPFILE_APPEND | OPFILE_TEXT));
		}
	}

	va_start(args, format);

	if(m_log_file.IsOpen())
	{
		OpString strdate;
		time_t date = g_timecache->CurrentTime();
		SyncUtil::CreateRFC3339Date(date, strdate);

		out.AppendFormat(UNI_L("%s - "), strdate.CStr());

		out.AppendVFormat(format, args);

		OpString8 filestr;

		filestr.Set(out.CStr());
		m_log_file.Write(filestr.CStr(), filestr.Length());
		m_log_file.Flush();
	}
	va_end(args);
}

#endif // DEBUG_LIVE_SYNC
