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

#ifdef AUTO_UPDATE_SUPPORT

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
#include "adjunct/autoupdate/updater/pi/aufileutils.h"
#include "adjunct/autoupdate/updater/audatafile.h"
#endif // AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "adjunct/quick/managers/LaunchManager.h"
#include "adjunct/quick/dialogs/UpdateAvailableDialog.h"
#include "adjunct/quick/dialogs/UpdateErrorDialogs.h"
#include "adjunct/quick/dialogs/AutoUpdateDialog.h"
#include "adjunct/quick/dialogs/AutoUpdateCheckDialog.h"

#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"

#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/resources/ResourceUtils.h"

#include "modules/inputmanager/inputaction.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/libssl/updaters.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#ifdef GADGET_UPDATE_SUPPORT
#include "adjunct/quick/ClassicApplication.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#endif 

#include "adjunct/autoupdate/updatablesetting.h"
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/global_storage.h"

using namespace opera_update_checker::global_storage;

/***********************************************************************************
**  Constructor. Initialize Values.
**
**	AutoUpdateManager::AutoUpdateManager
**
***********************************************************************************/
AutoUpdateManager::AutoUpdateManager() :
	m_internal_state(Inactive),
	m_inited(FALSE),
	m_minimized(AllMinimized),
	m_autoupdater(NULL),
	m_addition_checker(NULL),
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	m_file_utils(NULL),
#endif // AUTOUPDATE_PACKAGE_INSTALLATION
	m_update_context(NULL),
	m_progress_context(NULL),
	m_error_context(NULL),
	m_update_dialog(NULL),
	m_update_check_dialog(NULL),
	m_error_resuming_dialog(NULL),
	m_storage(NULL)
{
}


/***********************************************************************************
**
**	AutoUpdateManager::~AutoUpdateManager
**
***********************************************************************************/
AutoUpdateManager::~AutoUpdateManager()
{
	GlobalStorage::Destroy(m_storage);
	// Remove the prefs listener
	g_pcui->UnregisterListener(this);

	// Remove the listener if it was set
	if (m_autoupdater)
		m_autoupdater->RemoveListener(this);

	OP_DELETE(m_update_context);
	m_update_context = NULL;

	OP_DELETE(m_progress_context);
	m_progress_context = NULL;

	OP_DELETE(m_error_context);
	m_error_context = NULL;

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	OP_DELETE(m_file_utils);
	m_file_utils = NULL;
#endif // AUTOUPDATE_PACKAGE_INSTALLATION

	// Destroy the auto update object
	OP_DELETE(m_autoupdater);
	m_autoupdater = NULL;

	OP_DELETE(m_addition_checker);
	m_addition_checker = NULL;
}


/***********************************************************************************
**
**	AutoUpdateManager::OnUpToDate
**  @param silent	Silent update or not
**
***********************************************************************************/
void AutoUpdateManager::OnUpToDate(BOOL silent)
{
	m_internal_state = Inactive;
	GenerateOnUpToDate();

	if (!silent && m_minimized != AvailableUpdateOnly) // don't show dialog for silent updates
	{
		SimpleDialogController* controller = OP_NEW(SimpleDialogController,
			(SimpleDialogController::TYPE_OK, SimpleDialogController::IMAGE_INFO,
			 WINDOW_NAME_UPDATE_CHECK,Str::D_NO_NEW_VERSION_AVAILABLE, Str::D_NO_NEW_VERSION_TITLE));

		SimpleDialogController::DialogResultCode dummy;
		if (controller)
			controller->SetBlocking(dummy);
		ShowDialog(controller, g_global_ui_context,g_application->GetActiveDesktopWindow());

#ifdef GADGET_UPDATE_SUPPORT
		//FixMe: GetGadgetUpdateController should go to Application and be 
		//implemented in a valid way in other types of applications
		GadgetUpdateController* upd_controller = ((ClassicApplication*)g_application)->GetGadgetUpdateController();
		if (upd_controller)
			upd_controller->OnTaskTimeOut(NULL);
#endif
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::OnChecking
**  @param	silent
**
***********************************************************************************/
void AutoUpdateManager::OnChecking(BOOL silent)
{
	m_internal_state = Checking;
	GenerateOnChecking();

	if(!silent && m_minimized != AvailableUpdateOnly)
	{
		ShowCheckForUpdateDialog();
	}
}

/***********************************************************************************
**
**	AutoUpdateManager::OnUpdateAvailable
**  @param	type
**  @param	update_size
**  @param	update_info_url
**  @param	silent
**
***********************************************************************************/
void AutoUpdateManager::OnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL silent)
{
	if (type & UpdatableResource::RTPackage)
	{
		m_internal_state = UpdateAvailable;

		if (m_update_context)
		{
			OP_DELETE(m_update_context);
		}
		m_update_context = OP_NEW(AvailableUpdateContext, ());
		m_update_context->update_info_url.Set(update_info_url);
		m_update_context->update_size = update_size;

		GenerateOnUpdateAvailable(m_update_context);

		if (m_minimized == AvailableUpdateOnly)
		{
			LevelOfAutomation level = (LevelOfAutomation)g_pcui->GetIntegerPref(PrefsCollectionUI::LevelOfUpdateAutomation);
			if (AutoInstallUpdates == level)
			{
				SetUpdateMinimized(TRUE);
				DownloadUpdate();
			}
			else
			{
				SetUpdateMinimized(FALSE); // shows update available dialog
			}
		}
		else
		{
			SetUpdateMinimized(silent);
		}
	}
	else
	{
		OnUpToDate(silent);
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::OnDownloading
**  @param	type
**  @param	total_size
**  @param	downloaded_size
**  @param	kbps
**  @param	time_estimate
**  @param  silent
**
***********************************************************************************/
void AutoUpdateManager::OnDownloading(UpdatableResource::UpdatableResourceType type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate, BOOL silent)
{
	m_internal_state = InProgress;

	if (!m_progress_context)
	{
		m_progress_context = OP_NEW(UpdateProgressContext, ());
	}
	if (type & UpdatableResource::RTPackage || type & UpdatableResource::RTPatch)
	{
		m_progress_context->type = UpdateProgressContext::BrowserUpdate;
	}
	else // TODO: add dictionary update type
	{
		m_progress_context->type = UpdateProgressContext::ResourceUpdate;
	}
	m_progress_context->is_preparing = FALSE;
	m_progress_context->is_ready = FALSE;
	m_progress_context->total_size = total_size;
	m_progress_context->downloaded_size = downloaded_size;
	m_progress_context->kbps = kbps;
	m_progress_context->time_estimate = time_estimate;

	GenerateOnDownloading(m_progress_context);	// the AU dialog doesn't need to get the first callback

	if( m_progress_context->type == UpdateProgressContext::BrowserUpdate)
	{
		if (!silent)	// manual
		{
			// Close the resuming dialog if it's open
			if(m_error_resuming_dialog)
			{
				m_error_resuming_dialog->CloseDialog(FALSE);
			}

			if(!IsUpdateMinimized())
			{
				ShowAutoUpdateDialog(m_progress_context);
			}
		}
	}
}


/***********************************************************************************
 **
 **	AutoUpdateManager::OnDownloadingDone
 **  @param	type
 **  @param	filename
 **  @param	total_size
 **  @param  silent
 **
 ***********************************************************************************/
void AutoUpdateManager::OnDownloadingDone(UpdatableResource::UpdatableResourceType type, const OpString& filename, OpFileLength total_size, BOOL silent)
{
	if(m_progress_context)
	{
		m_progress_context->total_size = total_size;
		m_progress_context->downloaded_size = total_size;
		m_progress_context->filename.Set(filename);

		GenerateOnDownloadingDone(m_progress_context);
	}
}


/***********************************************************************************
 **
 **	AutoUpdateManager::OnDownloadingFailed
 **  @param  silent
 **
 ***********************************************************************************/
void AutoUpdateManager::OnDownloadingFailed(BOOL silent)
{
	if(!silent)
	{
		OpString title;
		OpString error_type;
		OpString error_info;
		OpString message;

		g_languageManager->GetString(Str::D_AUTO_UPDATE_TITLE, title);

		g_languageManager->GetString(Str::D_UPDATE_ERROR_TEXT, error_info);
		g_languageManager->GetString(Str::D_UPDATE_ERROR_TYPE_CONNECTION, error_type);

		message.AppendFormat(error_info.CStr(), error_type.CStr());
		
		SimpleDialogController* controller = OP_NEW(SimpleDialogController,(SimpleDialogController::TYPE_OK, SimpleDialogController::IMAGE_INFO,
			WINDOW_NAME_AUTOUPDATE_DOWNLOADING_FAILED,message, title));
		
		ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow());	

		m_internal_state = UpdateAvailable;
	}
	GenerateOnDownloadingFailed();
}


/***********************************************************************************
**
**	AutoUpdateManager::OnRestartingDownload
**  @param	seconds_until_restart
**  @param	last_error
**
***********************************************************************************/
void AutoUpdateManager::OnRestartingDownload(INT32 seconds_until_restart, AutoUpdateError last_error, BOOL silent)
{
	m_internal_state = Resuming;
	if (!m_error_context)
	{
		m_error_context = OP_NEW(UpdateErrorContext, ());
		m_error_context->seconds_until_retry = NO_RETRY_TIMER;
	}
	m_error_context->error = last_error;
	m_error_context->seconds_until_retry = seconds_until_restart;

	if (!silent)
	{
		if(!m_error_resuming_dialog)
		{
			m_error_resuming_dialog = OP_NEW(UpdateErrorResumingDialog, (m_error_context));
			m_error_resuming_dialog->Init(g_application->GetActiveDesktopWindow());
			m_error_resuming_dialog->AddListener(this);
		}
	}
	GenerateOnError(m_error_context);
}


/***********************************************************************************
**
**	AutoUpdateManager::OnReadyToInstall
**  @param	type
**  @param	version
**  @param	silent
**
***********************************************************************************/
void AutoUpdateManager::OnReadyToInstallNewVersion(const OpString& version, BOOL silent)
{
	m_version.Set(version.CStr());

	m_internal_state = Ready;
	if (!m_progress_context)
	{
		m_progress_context = OP_NEW(UpdateProgressContext, ());
	}
	if (m_progress_context)
	{
		m_progress_context->is_ready = TRUE;

		SetUpdateMinimized(silent);
		GenerateOnReadyToInstall(m_progress_context);
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::OnReadyToUpdate
**
***********************************************************************************/
void AutoUpdateManager::OnReadyToUpdate()
{
	// do nothing here
}


/***********************************************************************************
**
**	AutoUpdateManager::OnUpdating
**
***********************************************************************************/
void AutoUpdateManager::OnUpdating()
{
	if (m_progress_context)
	{
		m_progress_context->is_preparing = TRUE;
		GenerateOnPreparing(m_progress_context);
	}
}


/***********************************************************************************
 **
 **	AutoUpdateManager::OnFinishedUpdating
 **
 ***********************************************************************************/
void AutoUpdateManager::OnFinishedUpdating()
{
	GenerateOnFinishedPreparing();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void AutoUpdateManager::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue)
{
#ifdef DU_CAP_PREFS
	if(OpPrefsCollection::UI != id)
		return;

	switch(PrefsCollectionUI::stringpref(pref))
	{
	case PrefsCollectionUI::AuCountryCode:
		// Update path to the turbo configuration file.
		OpStatus::Ignore(FileUtils::SetTurboSettingsPath());
		break;
	default:
		break;
	}
#endif // DU_CAP_PREFS
}

/***********************************************************************************
**
**	AutoUpdateManager::OnError
**  @param	error
**  @param	silent
**
***********************************************************************************/
void AutoUpdateManager::OnError(AutoUpdateError error, BOOL silent)
{
	if (m_internal_state == Checking)
	{
		m_internal_state = Inactive;
		if (m_update_check_dialog)
		{
			m_update_check_dialog->CloseDialog(FALSE);
		}
		if (!silent && m_minimized != AvailableUpdateOnly)
		{

			SimpleDialogController* controller = OP_NEW(SimpleDialogController,	(SimpleDialogController::TYPE_OK, SimpleDialogController::IMAGE_INFO,
				WINDOW_NAME_UPDATE_CHECK_ERROR,Str::D_NEW_UPGRADE_ERROR_TEXT, Str::D_NEW_UPGRADE_ERROR_TITLE));

			ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow());	

		}

		if (!m_error_context)
		{
			m_error_context = OP_NEW(UpdateErrorContext, ());
			if(!m_error_context)
				return;
			m_error_context->seconds_until_retry = NO_RETRY_TIMER;
		}
		m_error_context->error = error;

		GenerateOnError(m_error_context);
	}
	else
	{
		if (!m_error_context)
		{
			m_error_context = OP_NEW(UpdateErrorContext, ());
			if(!m_error_context)
				return;
			m_error_context->seconds_until_retry = NO_RETRY_TIMER;
		}
		m_error_context->error = error;

		m_internal_state = Inactive;
		if (m_update_dialog)
		{
			m_update_dialog->CloseDialog(FALSE); // call-cancel would set update minimized
		}
		if (m_error_resuming_dialog)
		{
			m_error_resuming_dialog->CloseDialog(FALSE);
		}
		// show error dialog
		if(!silent && m_minimized != AvailableUpdateOnly)
		{
			UpdateErrorDialog* error_dialog = OP_NEW(UpdateErrorDialog, (m_error_context->error));
			if(!error_dialog)
				return;
			error_dialog->Init(g_application->GetActiveDesktopWindow());

			SetUpdateMinimized(FALSE);	// error will always be maximized (if not resuming)
		}
		GenerateOnError(m_error_context);
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GetVersionString
**  @param	version
**  @return
**
***********************************************************************************/
OP_STATUS AutoUpdateManager::GetVersionString(OpString& version)
{
	RETURN_IF_ERROR(version.Set(m_version));
	return OpStatus::OK;
}

/***********************************************************************************
**
**	AutoUpdateManager::OnDesktopWindowClosing
**  @param	desktop_window
**  @param	user_inititated
**
***********************************************************************************/
void AutoUpdateManager::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if(desktop_window == m_update_dialog)
	{
		m_update_dialog = NULL;
		return;
	}
	if(desktop_window == m_error_resuming_dialog)
	{
		m_error_resuming_dialog = NULL;
		return;
	}
	if(desktop_window == m_update_check_dialog)
	{
		m_update_check_dialog = NULL;
		return;
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::Init
**  @return
**
***********************************************************************************/
OP_STATUS AutoUpdateManager::Init()
{
	m_storage = GlobalStorage::Create();
	RETURN_OOM_IF_NULL(m_storage);
	m_autoupdater = OP_NEW(AutoUpdater, ());
	RETURN_OOM_IF_NULL(m_autoupdater);
	RETURN_IF_ERROR(m_autoupdater->Init());
	RETURN_IF_ERROR(m_autoupdater->AddListener(this));

	m_addition_checker = OP_NEW(AdditionChecker, ());
	RETURN_OOM_IF_NULL(m_addition_checker);
	RETURN_IF_ERROR(m_addition_checker->Init());

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	// Create the aufile utils object
	m_file_utils  = AUFileUtils::Create();
	if (!m_file_utils)
		return OpStatus::ERR_NO_MEMORY;
#endif // AUTOUPDATE_PACKAGE_INSTALLATION

	// Register Prefs listener
	TRAPD(err, g_pcui->RegisterListenerL(this));
	return OpStatus::OK;
}

OP_STATUS AutoUpdateManager::CheckForUpdate(BOOL user_initiated)
{
	if (!user_initiated)
	{
		m_minimized = AvailableUpdateOnly;
	}
	else
	{
		SetUpdateMinimized(FALSE);
	}

	if (m_internal_state == Inactive)
	{
		if(user_initiated && !g_ssl_auto_updaters->Active())
		{
			// Check for SSL autoupdates (Certificates and EV)
			g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
		}
		// Set the user initiated flag for this check.
		m_autoupdater->SetIsUserInitited(user_initiated);
		OP_STATUS status = m_autoupdater->CheckForUpdate();
		if (user_initiated)
			m_autoupdater->SetIsUserInitited(FALSE); // Reset the user initiated flag.
		return status;
	}
	else
	{
		return OpStatus::OK;
	}
}

OP_STATUS AutoUpdateManager::CheckForAddition(UpdatableResource::UpdatableResourceType type, const OpStringC& key)
{
	return m_addition_checker->CheckForAddition(type, key);
}

/***********************************************************************************
**
**	AutoUpdateManager::DownloadUpdate
**  @return
**
***********************************************************************************/
OP_STATUS AutoUpdateManager::DownloadUpdate()
{
	return m_autoupdater->DownloadUpdate();
}


/***********************************************************************************
**
**	AutoUpdateManager::DeferUpdate
**
***********************************************************************************/
void AutoUpdateManager::DeferUpdate()
{
	SetUpdateMinimized();
	m_autoupdater->DeferUpdate();
}


/***********************************************************************************
**
**	AutoUpdateManager::CancelUpdate
**
***********************************************************************************/
void AutoUpdateManager::CancelUpdate()
{
	DeferUpdate();

	if (m_progress_context)
	{
		OP_DELETE(m_progress_context);
		m_progress_context = NULL;
	}
	if (m_update_context)
	{
		m_internal_state = UpdateAvailable;
		GenerateOnUpdateAvailable(m_update_context);
	}
}


/***********************************************************************************
 **
 **	AutoUpdateManager::GoToDownloadPage
 **
 ***********************************************************************************/
void AutoUpdateManager::GoToDownloadPage()
{
	OpString downloadURL;
	OP_STATUS ret = m_autoupdater->GetDownloadPageURL(downloadURL);
	if(OpStatus::IsError(ret) || !downloadURL.HasContent())
	{
		if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
			downloadURL.Set("www.opera.com/browser/next/");
		else
			downloadURL.Set("www.opera.com/download");
	}
	g_application->OpenURL(downloadURL, NO, YES, NO);
}


/***********************************************************************************
**
**	AutoUpdateManager::SetUpdateMinimized
**  @param	minimized
**
***********************************************************************************/
void AutoUpdateManager::SetUpdateMinimized(BOOL minimized)
{
	m_minimized = minimized ? AllMinimized : AllVisible;
	GenerateOnMinimizedStateChanged(minimized);

	if (!minimized)
	{
		switch (m_internal_state)
		{
		case UpdateAvailable:
			{
				if (m_update_context != NULL)
				{
					ShowUpdateAvailableDialog(m_update_context);
				}
				break;
			}
		case InProgress:
		case Ready:
			{
				if (m_progress_context != NULL)
				{
					ShowAutoUpdateDialog(m_progress_context);
				}
				break;
			}
		case Resuming:
			{
				if (m_error_context != NULL)
				{
					// todo: save error and seconds until restart
					UpdateErrorResumingDialog* error_dialog = OP_NEW(UpdateErrorResumingDialog, (m_error_context));
					error_dialog->Init(g_application->GetActiveDesktopWindow());
				}
				break;
			}
		};


	}
}

OP_STATUS AutoUpdateManager::SetLevelOfAutomation(LevelOfAutomation level_of_automation)
{
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::LevelOfUpdateAutomation, level_of_automation));
	if(OpStatus::IsSuccess(err))
		TRAP(err, g_prefsManager->CommitL());
	return err;
}

LevelOfAutomation AutoUpdateManager::GetLevelOfAutomation()
{
	return (LevelOfAutomation)g_pcui->GetIntegerPref(PrefsCollectionUI::LevelOfUpdateAutomation);
}

/***********************************************************************************
**
**	AutoUpdateManager::OnInputAction
**  @param	action
**  @return
**
***********************************************************************************/
BOOL AutoUpdateManager::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_RESTORE_AUTO_UPDATE_DIALOG:
				{
					if (!IsUpdateMinimized())
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_RESTART_OPERA:
					{
						return TRUE;
					}
			}
			break;
		}
	case OpInputAction::ACTION_RESTORE_AUTO_UPDATE_DIALOG:
		{
			SetUpdateMinimized(FALSE);
			return TRUE;
		}
	case OpInputAction::ACTION_RESTART_OPERA:
		{
			RestartOpera();
			return TRUE;
		}
	}
	return FALSE;
}


/***********************************************************************************
**
**	AutoUpdateManager::RestartOpera
**
***********************************************************************************/
void AutoUpdateManager::RestartOpera()
{
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	if (m_file_utils)
	{
		uni_char* path = NULL;
		if (m_file_utils->GetUpdatePath(&path) == AUFileUtils::OK)
		{
			AUDataFile dataFile;
			if(!OpStatus::IsError(dataFile.Init()))
			{
				if(!OpStatus::IsError(dataFile.LoadValuesFromExistingFile()))
				{
					dataFile.SetShowInformation(FALSE);
					dataFile.Write();
				}
			}

			g_launch_manager->LaunchAutoupdateOnExit(path);

			OP_DELETEA(path);

			g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
		}
	}
// TODO: do something if AUFileUtils isn't defined
#endif // AUTOUPDATE_PACKAGE_INSTALLATION
}

/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnUpToDate
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnUpToDate()
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnUpToDate();
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnChecking
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnChecking()
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnChecking();
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnUpdateAvailable
**  @param	context
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnUpdateAvailable(AvailableUpdateContext* context)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnUpdateAvailable(context);
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnDownloading
**  @param	context
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnDownloading(UpdateProgressContext* context)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDownloading(context);
	}
}


/***********************************************************************************
 **
 **	AutoUpdateManager::GenerateOnDownloadingDone
 **  @param	context
 **
 ***********************************************************************************/
void AutoUpdateManager::GenerateOnDownloadingDone(UpdateProgressContext* context)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDownloadingDone(context);
	}
}

/***********************************************************************************
 **
 **	AutoUpdateManager::GenerateOnDownloadingFailed
 **  @param	context
 **
 ***********************************************************************************/
void AutoUpdateManager::GenerateOnDownloadingFailed()
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDownloadingFailed();
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnPreparing
**  @param	context
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnPreparing(UpdateProgressContext* context)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPreparing(context);
	}
}


/***********************************************************************************
 **
 **	AutoUpdateManager::GenerateOnFinishedPreparing
 **
 ***********************************************************************************/
void AutoUpdateManager::GenerateOnFinishedPreparing()
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFinishedPreparing();
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnReadyToInstall
**  @param	context
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnReadyToInstall(UpdateProgressContext* context)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnReadyToInstall(context);
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnError
**  @param	error
**  @param	seconds_until_retry
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnError(UpdateErrorContext* context)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnError(context);
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::GenerateOnMinimizedStateChanged
**  @param	minimized
**
***********************************************************************************/
void AutoUpdateManager::GenerateOnMinimizedStateChanged(BOOL minimized)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnMinimizedStateChanged(minimized);
	}
}


/***********************************************************************************
 **
 **	AutoUpdateManager::ShowCheckForUpdateDialog
 **
 ***********************************************************************************/
void AutoUpdateManager::ShowCheckForUpdateDialog()
{
	if(!m_update_check_dialog)
	{
		m_update_check_dialog = OP_NEW(AutoUpdateCheckDialog, ());
		if (m_update_check_dialog)
		{
			OP_STATUS status = m_update_check_dialog->Init(g_application->GetActiveDesktopWindow(), 0, TRUE);
			if (OpStatus::IsError(status))
			{
				m_update_check_dialog = NULL;
				return;
			}
			m_update_check_dialog->AddListener(this);
		}
	}
}

/***********************************************************************************
**
**	AutoUpdateManager::ShowUpdateAvailableDialog
**  @param	context
**
***********************************************************************************/
void AutoUpdateManager::ShowUpdateAvailableDialog(AvailableUpdateContext* context)
{
	OP_ASSERT(m_progress_context == NULL);

	UpdateAvailableDialog *dialog = OP_NEW(UpdateAvailableDialog, (context));
	if (dialog)
	{
		OP_STATUS status = dialog->Init(g_application->GetActiveDesktopWindow(), 0, TRUE);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(dialog);
		}
	}
}


/***********************************************************************************
**
**	AutoUpdateManager::ShowAutoUpdateDialog
**  @param	context
**
***********************************************************************************/
void AutoUpdateManager::ShowAutoUpdateDialog(UpdateProgressContext* context)
{
	if(!m_update_dialog)
	{
		m_update_dialog = OP_NEW(AutoUpdateDialog, ());
		m_update_dialog->Init(g_application->GetActiveDesktopWindow());
		m_update_dialog->AddListener(this);
	}
	if (context != NULL)
	{
		m_update_dialog->SetContext(context);
	}
}

/***********************************************************************************
 **
 **	AutoUpdateManager::RequestCallback
 **  @param	listener
 **
 ***********************************************************************************/
void AutoUpdateManager::RequestCallback(AUM_Listener* listener)
{
	switch(m_internal_state)
	{
		case Inactive:
		{
			listener->OnUpToDate();
			break;
		}
		case UpdateAvailable:
		{
			listener->OnUpdateAvailable(m_update_context);
			break;
		}
		case InProgress:
		{
			listener->OnDownloading(m_progress_context);
			break;
		}
		case Ready:
		{
			listener->OnReadyToInstall(m_progress_context);
			break;
		}
		case Checking:
		{
			listener->OnChecking();
			break;
		}
	}
}

#endif // AUTO_UPDATE_SUPPORT
