/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton, Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/autoupdater.h"
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/autoupdate/updatablesetting.h"

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
# include "adjunct/autoupdate/updater/pi/aufileutils.h"
# include "adjunct/autoupdate/updater/audatafile_reader.h"
#endif

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/opfile/opfile.h"
#ifdef AUTOUPDATE_ENABLE_AUTOUPDATE_INI
#include "modules/prefsfile/prefsfile.h"
#endif // AUTOUPDATE_ENABLE_AUTOUPDATE_INI
#include "modules/libssl/updaters.h"

#include "adjunct/quick/managers/LaunchManager.h"
#ifndef _MACINTOSH_
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/ipc.h"
#endif // _MACINTOSH_
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#ifndef _MACINTOSH_
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/global_storage.h"
#endif // _MACINTOSH_
#include "modules/util/path.h"

const uni_char* autoupdate_error_strings[] =
{
	UNI_L("No error"),
	UNI_L("Internal error"),
	UNI_L("In progress error"),
	UNI_L("Connection error"),
	UNI_L("Save error"),
	UNI_L("Validation error"),
	UNI_L("Update error")
};

OperaVersion AutoUpdater::m_opera_version;

/**
 * See autoupdater.h for description of the values below.
 */
const int AutoUpdater::MAX_UPDATE_CHECK_INTERVAL_SEC = (7*24*60*60);
const int AutoUpdater::MIN_UPDATE_CHECK_INTERVAL_SEC = (5*60);
const int AutoUpdater::MIN_UPDATE_CHECK_INTERVAL_RUNNING_SEC = (24*60*60);
const int AutoUpdater::UPDATE_CHECK_INTERVAL_DELTA_SEC = (24*60*60);
const int AutoUpdater::AUTOUPDATE_RECHECK_TIMEOUT_SEC = (24*60*60);
const int AutoUpdater::AUTOUPDATE_SSL_UPDATERS_BUSY_RECHECK_TIMEOUT_SEC = 10;
const int AutoUpdater::AUTOUPDATE_RESOURCE_CHECK_BASE_TIMEOUT_SEC = 60;

#ifndef _MACINTOSH_
using namespace opera_update_checker::ipc;
using namespace opera_update_checker::global_storage;
using namespace opera_update_checker::status;
#endif // _MACINTOSH_

AutoUpdater::AutoUpdater():
	m_xml_downloader(NULL),
	m_autoupdate_xml(NULL),
	m_autoupdate_server_url(NULL),
	m_update_state(AUSUpToDate),
	m_silent_update(TRUE),
	m_last_error(AUNoError),
	m_update_check_timer(NULL),
	m_download_timer(NULL),
	m_download_countdown(0),
	m_download_retries(0),
	m_activated(FALSE),
	m_resources_check_only(FALSE),
	m_check_increased_update_check_interval(TRUE),
	m_time_of_last_update_check(0),
	m_update_check_interval(86400),
	m_update_check_delay(0),
	m_user_initiated(FALSE)
#ifndef _MACINTOSH_
	,m_channel_to_checker(NULL)
	,m_channel_id(0)
	,m_checker_launch_retry_timer(NULL)
	,m_checker_launch_retry_count(0)
	,m_checker_state(CheckerOk)
#endif //_MACINTOSH_
{
}

AutoUpdater::~AutoUpdater()
{
	g_pcui->UnregisterListener(this);

	OP_DELETE(m_xml_downloader);
	OP_DELETE(m_autoupdate_xml);
	OP_DELETE(m_autoupdate_server_url);
	OP_DELETE(m_update_check_timer);
    OP_DELETE(m_download_timer);

#ifndef _MACINTOSH_
	OP_DELETE(m_checker_launch_retry_timer);
	if (m_channel_to_checker)
		m_channel_to_checker->Disconnect();
	Channel::Destroy(m_channel_to_checker);
#endif // _MACINTOSH_
}

OP_STATUS AutoUpdater::Init(BOOL check_increased_update_check_interval)
{
	OpDesktopResources* resources;
	RETURN_IF_ERROR(OpDesktopResources::Create(&resources));
	OpAutoPtr<OpDesktopResources> auto_resources(resources);
    RETURN_IF_ERROR(g_pcui->RegisterListener(this));
    
#ifndef _MACINTOSH_
	RETURN_IF_ERROR(resources->GetUpdateCheckerPath(m_checker_path));
	m_channel_id = opera_update_checker::ipc::GetCurrentProcessId();
	op_srand(m_channel_id);
	m_channel_to_checker = Channel::Create(true, m_channel_id, Channel::CHANNEL_MODE_READ_WRITE, Channel::BIDIRECTIONAL);
	RETURN_OOM_IF_NULL(m_channel_to_checker);
#endif // _MACINTOSH_

	m_check_increased_update_check_interval = check_increased_update_check_interval;
	m_level_of_automation = static_cast<LevelOfAutomation>(g_pcui->GetIntegerPref(PrefsCollectionUI::LevelOfUpdateAutomation));
	int saved_state = g_pcui->GetIntegerPref(PrefsCollectionUI::AutoUpdateState);
	m_update_state = static_cast<AutoUpdateState>(saved_state & 0xFFFF); // The lower word is the update state.
#ifndef _MACINTOSH_
	m_checker_state = static_cast<CheckerState>(saved_state >> 16);  // The higher word is the checker state.
	// If couldn't launch the checker last time schedule the new attempt.
	if (m_checker_state == CheckerCouldntLaunch)
	{
		m_checker_launch_retry_timer = OP_NEW(OpTimer, ());
		if (m_checker_launch_retry_timer)
		{
			m_checker_launch_retry_timer->SetTimerListener(this);
			m_checker_launch_retry_timer->Start(CHECKER_LAUNCH_RETRY_TIME_MS);
		}
	}
#endif // _MACINTOSH_
	m_time_of_last_update_check = static_cast<time_t>(g_pcui->GetIntegerPref(PrefsCollectionUI::TimeOfLastUpdateCheck));

	const int DELTA_PERIOD_MINUTES = 4 * 60;
	const int DELTA_RESOLUTION_MINUTES = 10;
	/* Random delta needed by the user counting system to detect collisions.
	   It's required to be wihin +- 4h with 10min. resolution.
	   Since rand operates on ranges starting from 0 only in order to achieve [-4h; 4h] range
	   rand from [0h; 7h50min] range is done and 4h is substracted from the result
	   (+1 below is needed due to how % operator works and -10 min is so that after
	    rounding the result is still within the expected range).
	   To achieve 10 minutes resolution the result delta is rounded to the nearest number
	   being a multiplication of 10. */
	m_update_check_random_delta = (op_rand() % (2 * DELTA_PERIOD_MINUTES - DELTA_RESOLUTION_MINUTES + 1)) - DELTA_PERIOD_MINUTES; // In minutes.
	// Rounding.
	m_update_check_random_delta += m_update_check_random_delta > 0 ? (DELTA_RESOLUTION_MINUTES - 1) : -(DELTA_RESOLUTION_MINUTES - 1);
	m_update_check_random_delta = m_update_check_random_delta / DELTA_RESOLUTION_MINUTES * DELTA_RESOLUTION_MINUTES;
	m_update_check_random_delta = m_update_check_random_delta == 0 ? DELTA_RESOLUTION_MINUTES : m_update_check_random_delta;
	m_update_check_random_delta *= 60; // In seconds.
	m_update_check_interval = g_pcui->GetIntegerPref(PrefsCollectionUI::UpdateCheckInterval);
	m_update_check_delay = g_pcui->GetIntegerPref(PrefsCollectionUI::DelayedUpdateCheckInterval);

	// Allow the version object to read the faked version from the autoupdate.ini file, should there be one.
	// This helps testing the autoupdate.
	m_opera_version.AllowAutoupdateIniOverride();

	if ( (m_update_state < AUSUpToDate) || (m_update_state > AUSError) )
	{
		// Force up to date state
		RETURN_IF_ERROR(SetUpdateState(AUSUpToDate));
	}

	OpAutoPtr<AutoUpdateXML> autoupdate_xml_guard(OP_NEW(AutoUpdateXML, ()));
	RETURN_OOM_IF_NULL(autoupdate_xml_guard.get());
	RETURN_IF_ERROR(autoupdate_xml_guard->Init());

	OpAutoPtr<AutoUpdateServerURL> autoupdate_server_url_guard(OP_NEW(AutoUpdateServerURL, ()));
	RETURN_OOM_IF_NULL(autoupdate_server_url_guard.get());
	RETURN_IF_ERROR(autoupdate_server_url_guard->Init());

	OpAutoPtr<StatusXMLDownloader> xml_downloader_guard(OP_NEW(StatusXMLDownloader, ()));
	RETURN_OOM_IF_NULL(xml_downloader_guard.get());
	RETURN_IF_ERROR(xml_downloader_guard->Init(StatusXMLDownloader::CheckTypeUpdate, this));

	m_autoupdate_xml = autoupdate_xml_guard.release();
	m_autoupdate_server_url = autoupdate_server_url_guard.release();
	m_xml_downloader = xml_downloader_guard.release();

	return OpStatus::OK;
}

OP_STATUS AutoUpdater::WritePref(PrefsCollectionUI::integerpref which, int val)
{
	TRAPD(st, g_pcui->WriteIntegerL(which, val));
	RETURN_IF_ERROR(st);
	TRAP(st, g_prefsManager->CommitL());
	return st;
}

OP_STATUS AutoUpdater::Activate()
{
	OP_ASSERT(m_xml_downloader);

	if(m_activated)
	{
		Console::WriteError(UNI_L("AutoUpdate already activated."));
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	m_activated = TRUE;

	if (m_level_of_automation < NoChecking || m_level_of_automation > AutoInstallUpdates)
		return OpStatus::OK;

	// If the user is interested in updates or update notifications, get on with that.
	switch(m_update_state)
	{
		case AUSChecking:
		{
			// Opera probably exited or crashed while checking
			// Set initial state, and schedule new update check.
			break;
		}
		case AUSDownloading:
		{
			// Opera probably exited or crashed while downloading. If the last check was done less than 1 day ago,
			// try parsing the autoupdate xml we already downloaded, if not, schedule another check
			if(g_timecache->CurrentTime() - m_time_of_last_update_check < AUTOUPDATE_RECHECK_TIMEOUT_SEC)
			{
				// This method is to only be run once after the object has been created, expect the XML downloader 
				// to not exist yet.
				SetUpdateState(AUSChecking);
				StatusXMLDownloader::DownloadStatus status = m_xml_downloader->ParseDownloadedXML();

				if(status == StatusXMLDownloader::SUCCESS)
				{
					StatusXMLDownloaded(m_xml_downloader);
					return OpStatus::OK;
				}					
			}
			break;
		}
		case AUSReadyToInstall:
		{
			// Illegal state when activated
			// Set initial state, and schedule new check
			break;
		}
		case AUSError:
		case AUSUpToDate:
		case AUSUpdating:
		case AUSUnpacking:
		case AUSReadyToUpdate:
		case AUSUpdateAvailable:
		case AUSErrorDownloading:
			break;
		default:
			OP_ASSERT(!"What about this state?");
			break;
	}

	SetUpdateState(AUSUpToDate);

	OP_STATUS ret = OpStatus::OK;

	if (m_autoupdate_xml->NeedsResourceCheck())
	{
		// DSK-336588
		// If any of the resource timestamps are set to 0, we need to trigger a resource check NOW.
		// The browserjs timestamp will be set 0 to browser upgrade.
		ret = ScheduleResourceCheck();
	}
	else
	{
		// Schedule the next update check according to the schedule.
		ret = ScheduleUpdateCheck(ScheduleStartup);
	}

	if (OpStatus::IsSuccess(ret))
		Console::WriteMessage(UNI_L("The autoupdate mechanism activated."));
	else
		Console::WriteError(UNI_L("Failed to activate the autoupdate mechanism."));

	return ret;
}

OP_STATUS AutoUpdater::CheckAndUpdateState()
{
	switch(m_update_state)
	{
		case AUSUpToDate:
		{
			BroadcastOnUpToDate(IsSilent());

			Console::WriteMessage(UNI_L("Up to date."));

			Reset();
			
			return ScheduleUpdateCheck(ScheduleRunning);
		}

		case AUSChecking:
		{
			BroadcastOnChecking(IsSilent());

			Console::WriteMessage(UNI_L("Checking for updates..."));
			break;
		}

		case AUSDownloading:
		{
			UpdatableResource::UpdatableResourceType update_type = GetAvailableUpdateType();
			OpFileLength total_size = GetTotalUpdateSize();
			BroadcastOnDownloading(update_type, total_size, 0, 0.0, 0, IsSilent());

			Console::WriteMessage(UNI_L("Downloading updates..."));
			break;
		}

		case AUSReadyToUpdate:
		{
			BroadcastOnReadyToUpdate();

			Console::WriteMessage(UNI_L("Ready to update."));

			if(m_level_of_automation > NoChecking || !IsSilent())
			{
				/** Update all resources */
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
				StartUpdate(TRUE);
#else
				StartUpdate(FALSE);
#endif
			} else if(m_level_of_automation == NoChecking)
			{
				/** Only update spoof and browerjs file when in NoChecking mode. */
				StartUpdate(FALSE);
			}
			break;
		}

		case AUSUpdating:
		{
			BroadcastOnUpdating();

			Console::WriteMessage(UNI_L("Updating..."));
			break;
		}

		case AUSUnpacking:
		{
			BroadcastOnUnpacking();

			Console::WriteMessage(UNI_L("Extracting from downloaded package..."));
			break;
		}

		case AUSReadyToInstall:
		{
			Console::WriteMessage(UNI_L("Ready to install new version."));

			// Call listener
			OpString version;
			GetAvailablePackageInfo(&version, NULL, NULL);
			BroadcastOnReadyToInstallNewVersion(version, IsSilent());
			break;
		}

		case AUSError:
		{
			// Call listener
			BroadcastOnError(m_last_error, IsSilent());

			if(m_last_error < (int)ARRAY_SIZE(autoupdate_error_strings))
			{
				Console::WriteError(autoupdate_error_strings[m_last_error]);
			}

			// Set initial state
			SetUpdateState(AUSUpToDate);
			Reset();
			// Schedule new update check
			return ScheduleUpdateCheck(ScheduleRunning);
		}

		case AUSErrorDownloading:
		{
			// Call listener
			BroadcastOnDownloadingFailed(IsSilent());

			Console::WriteError(UNI_L("Downloading update failed."));

			// Increase the time to next update check
			IncreaseDelayedUpdateCheckInterval();

			// Schedule new download
			if(IsSilent())
			{
				// Go back to up to date state
				SetUpdateState(AUSUpToDate);
				CheckAndUpdateState();
			}
			else
			{
				m_silent_update = TRUE;
				SetUpdateState(AUSUpdateAvailable);
				//CheckAndUpdateState();
			}
			break;
		}

		case AUSUpdateAvailable:
		{
			Console::WriteMessage(UNI_L("An update is available."));

			// Call listener
			OpString info_url;
			OpFileLength size = 0;
			GetAvailablePackageInfo(NULL, &info_url, &size);
			UpdatableResource::UpdatableResourceType update_type = GetAvailableUpdateType();
			BroadcastOnUpdateAvailable(update_type, size, info_url.CStr(), IsSilent());
			BOOL has_package = update_type & UpdatableResource::RTPackage;
			if(!has_package)
			{
				// Update is silent if we dont have a package
				m_silent_update = TRUE;
			}

			if(IsSilent())
			{
				switch(m_level_of_automation)
				{
					case NoChecking:
					{
						/** Only download spoof and browerjs file when in NoChecking mode. */
						DownloadUpdate(FALSE);
						break;
					}
					case CheckForUpdates:
					{
						/** If there is no package, continue updating other resources */
						DownloadUpdate(FALSE);
						break;
					}
					case AutoInstallUpdates:
					{
						/** Get on with downloading the update */
						DownloadUpdate(TRUE);
						break;
					}
				}
			}
			break;
		}
			
		default:
		{
			OP_ASSERT(!"Should never reach this.");
		}
	}
	return OpStatus::OK;
}

OP_STATUS AutoUpdater::CheckForUpdate()
{
	if (m_update_state == AUSUpToDate || m_update_state == AUSUpdateAvailable)
	{
		m_silent_update = FALSE;
		return DownloadStatusXML();
	}
	else
	{
		BroadcastOnError(AUInProgressError, IsSilent());
		return OpStatus::OK;
	}
}

UpdatableResource::UpdatableResourceType AutoUpdater::GetAvailableUpdateType() const
{
	OP_ASSERT(m_xml_downloader);

	int res_type = 0;
	UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();

	while (dl_elm)
	{
		int cur_type = dl_elm->GetType();
		res_type |= cur_type;
		dl_elm = m_xml_downloader->GetNextResource();
	}

	return static_cast<UpdatableResource::UpdatableResourceType>(res_type);
}

BOOL AutoUpdater::GetAvailablePackageInfo(OpString* version, OpString* info_url, OpFileLength* size) const
{
	OP_ASSERT(m_xml_downloader);

	UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
	while (dl_elm)
	{
		if(dl_elm->GetType() == UpdatableResource::RTPackage)
		{
			UpdatablePackage* up = static_cast<UpdatablePackage*>(dl_elm);
			if(version)
				RETURN_VALUE_IF_ERROR(up->GetAttrValue(URA_VERSION, *version), FALSE);
			if(info_url)
				RETURN_VALUE_IF_ERROR(up->GetAttrValue(URA_INFOURL, *info_url), FALSE);
			if(size)
			{
				int size_int;
				RETURN_VALUE_IF_ERROR(up->GetAttrValue(URA_SIZE, size_int), FALSE);
				*size = size_int;
			}
			return TRUE;
		}
		dl_elm = m_xml_downloader->GetNextResource();
	}

	return FALSE;
}

OpFileLength AutoUpdater::GetTotalUpdateSize() const
{
	OP_ASSERT(m_xml_downloader);

	OpFileLength size = 0;
	UpdatableResource* resource = m_xml_downloader->GetFirstResource();
	while (resource)
	{
		int size_int;
		if (OpStatus::IsSuccess(resource->GetAttrValue(URA_SIZE, size_int)))
			size += size_int;
		resource = m_xml_downloader->GetNextResource();
	}

	return size;
}

void AutoUpdater::StatusXMLDownloaded(StatusXMLDownloader* downloader)
{
	OP_ASSERT(m_autoupdate_xml);
	OP_ASSERT(downloader);
	OP_ASSERT(downloader == m_xml_downloader);
	OP_ASSERT(m_update_state == AUSChecking);

	// Better safe than sorry
	if (NULL == downloader)
		return;

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	DeleteUpgradeFolderIfNeeded();
#endif

	// Success for a response from the Autoupdate server so set the flag to say we have
	// spoken to this server at least once. Used for stats to detect first runs and upgrades
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::AutoUpdateResponded, 1));

	// On a successful check, set delayed update check interval back to default
	m_update_check_delay = 0;
	TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::DelayedUpdateCheckInterval, 0));
	TRAP(err, g_prefsManager->CommitL());

	// Not much we can do about it now anyway
	OpStatus::Ignore(err);

	// Check if the server is increasing the update check interval.
	// In that case, dont download any packages, to avoid overloading the server
	BOOL increased_update_check_interval = FALSE;
	
	UpdatableResource* current_resource = m_xml_downloader->GetFirstResource();
	UpdatablePackage* package = NULL;

	while (current_resource)
	{
		UpdatableResource::UpdatableResourceType type = current_resource->GetType();

		switch(type)
		{
		case UpdatableResource::RTSetting:
			{
				UpdatableSetting* setting = static_cast<UpdatableSetting*>(current_resource);
				if (setting->IsUpdateCheckInterval())
				{
					int new_interval = 0;
					if (OpStatus::IsSuccess(setting->GetAttrValue(URA_DATA, new_interval)))
					{
						if (new_interval > m_update_check_interval)
							if(m_check_increased_update_check_interval)
								increased_update_check_interval = TRUE;
						// we could just wait for PrefsChanged, but it won't be called if operaprefs.ini
						// is read-only
						m_update_check_interval = new_interval;
						// PrefsCollectionUI::UpdateCheckInterval will be updated by UpdateResource
					}
				}
				if (!(setting->CheckResource() && OpStatus::IsSuccess(setting->UpdateResource())))
					OP_ASSERT(!"Could not update setting resource!");

				m_xml_downloader->RemoveResource(current_resource);
				OP_DELETE(current_resource);
				current_resource = NULL;
			}
			break;
		case UpdatableResource::RTPackage:
			{
				if (package)
					OP_ASSERT(!"Got more than one package from the server!");

				package = static_cast<UpdatablePackage*>(current_resource);
				if(AutoInstallUpdates == m_level_of_automation && package->GetShowNotification())
					m_silent_update = FALSE;
			}
			break;
		case UpdatableResource::RTPatch:
			// Do we know what to do with a patch? Carry on anyway.
			OP_ASSERT(!"A patch...?!");
		case UpdatableResource::RTSpoofFile:
		case UpdatableResource::RTBrowserJSFile:
		case UpdatableResource::RTDictionary:
		case UpdatableResource::RTHardwareBlocklist:
		case UpdatableResource::RTHandlersIgnore:
			// A dictionary may be updated with a regular autoupdate check.
			// Carry on
			break;
		case UpdatableResource::RTPlugin:
			// We don't expect any plugins with an update check.
			OP_ASSERT(!"Didn't expect a plugin with autoupdate check!");
			m_xml_downloader->RemoveResource(current_resource);
			OP_DELETE(current_resource);
			current_resource = NULL;
			break;
		default:
			OP_ASSERT(!"Uknown resource type");
			break;
		}

		current_resource = m_xml_downloader->GetNextResource();
	}

	if (package)
	{
		if ((NoChecking == m_level_of_automation && IsSilent()) || increased_update_check_interval)
		{
			if(!increased_update_check_interval)
				Console::WriteError(UNI_L("Did not expect package from server with current autoupdate level."));
			else
				Console::WriteMessage(UNI_L("Increased update check interval."));
			m_xml_downloader->RemoveResource(package);
			OP_DELETE(package);
			package = NULL;
		}
	}

	if (m_xml_downloader->GetResourceCount() > 0)
	{
		// Server returned some resources
		UpdatableResource::UpdatableResourceType update_type = GetAvailableUpdateType();
		if (CheckForUpdates == m_level_of_automation  && (update_type & UpdatableResource::RTPackage))
			m_silent_update = FALSE;

		SetUpdateState(AUSUpdateAvailable);
	}
	else
		SetUpdateState(AUSUpToDate);

	CheckAndUpdateState();
}

void AutoUpdater::StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus status)
{
	OP_ASSERT(m_autoupdate_xml);
	OP_ASSERT(downloader);
	OP_ASSERT(downloader == m_xml_downloader);
	OP_ASSERT(m_update_state == AUSChecking);

	AutoUpdateError error = AUInternalError;
	switch(status)
	{
		case StatusXMLDownloader::NO_TRANSFERITEM:
		case StatusXMLDownloader::NO_URL:
		case StatusXMLDownloader::LOAD_FAILED:
		case StatusXMLDownloader::DOWNLOAD_FAILED:
		case StatusXMLDownloader::DOWNLOAD_ABORTED:
		{
			error = AUConnectionError;
			break;
		}
		case StatusXMLDownloader::PARSE_ERROR:
		case StatusXMLDownloader::WRONG_XML:
		{
			error = AUValidationError;
			break;
		}
	}

	// Increase delayed update check interval when the request fails.
	if(error == AUConnectionError)
	{
		// Go through the server list
		OP_ASSERT(m_autoupdate_server_url);

		if (OpStatus::IsSuccess(m_autoupdate_server_url->IncrementURLNo(AutoUpdateServerURL::NoWrap)))
		{
			// There is more servers to check so increment to the next server and check right away
			RETURN_VOID_IF_ERROR(SetUpdateState(AUSUpToDate));
			RETURN_VOID_IF_ERROR(DownloadStatusXML());
		}
		else
		{
			// Last server in the list so reset back to the start and set a delayed called
			RETURN_VOID_IF_ERROR(m_autoupdate_server_url->ResetURLNo());
			IncreaseDelayedUpdateCheckInterval();
		}
	}

	SetLastError(error);
	SetUpdateState(AUSError);
	CheckAndUpdateState();
}

void AutoUpdater::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id == OpPrefsCollection::UI)
	{
		switch (pref)
		{
		case PrefsCollectionUI::TimeOfLastUpdateCheck:
			m_time_of_last_update_check = static_cast<time_t>(newvalue);
			break;
		case PrefsCollectionUI::UpdateCheckInterval:
			m_update_check_interval = newvalue;
			break;
		case PrefsCollectionUI::DelayedUpdateCheckInterval:
			m_update_check_delay = newvalue;
			break;
		default:
			break;
		}
	}
}

void AutoUpdater::OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size)
{
	OP_ASSERT(m_update_state == AUSDownloading);

	UpdatableFile* file = static_cast<UpdatableFile*>(file_downloader);
	if (file)
	{
		UpdatableResource::UpdatableResourceType type = file->GetType();
		OpString target_filename;
		file->GetTargetFilename(target_filename);
		BroadcastOnDownloadingDone(type, target_filename, total_size, IsSilent());
	}
	
	OP_STATUS ret = StartNextDownload(file_downloader);
	
	if (OpStatus::IsSuccess(ret))
		return;

	// Last file
	if (ret == OpStatus::ERR_NO_SUCH_RESOURCE)
	{
		// All files are downloaded, check all
		SetUpdateState(AUSReadyToUpdate);
		CheckAndUpdateState();
	}
	else
	{
		// One or more file downloads failed
		SetLastError(AUConnectionError);
		SetUpdateState(AUSErrorDownloading);
		CheckAndUpdateState();
	}
}

void AutoUpdater::OnFileDownloadFailed(FileDownloader* file_downloader)
{
	OP_ASSERT(m_update_state == AUSDownloading);

	// Set error downloading state
	SetLastError(AUConnectionError);
	SetUpdateState(AUSErrorDownloading);
	CheckAndUpdateState();
}

void AutoUpdater::OnFileDownloadAborted(FileDownloader* file_downloader)
{
}

void AutoUpdater::OnFileDownloadProgress(FileDownloader* file_downloader, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate)
{
	OP_ASSERT(m_update_state == AUSDownloading);

	UpdatableFile* file = static_cast<UpdatableFile*>(file_downloader);
	UpdatableResource::UpdatableResourceType type = UpdatableResource::RTEmpty;
	if(file)
	{
		type = file->GetType();
	}
	BroadcastOnDownloading(type, total_size, downloaded_size, kbps, time_estimate, IsSilent());
}

void AutoUpdater::DeferUpdate()
{
	// Destroy the retry download timer if we have one.
	if(m_download_timer)
	{
		OP_DELETE(m_download_timer);
		m_download_timer = NULL;
	}

	switch(m_update_state)
	{
		case AUSUpdateAvailable:
		{
			break;
		}
			
		case AUSDownloading:
		{
			StopDownloads();
			SetUpdateState(AUSUpdateAvailable);
			//CheckAndUpdateState();
			break;
		}

		default:
		{
			m_silent_update = TRUE;
			SetUpdateState(AUSUpToDate);
			CheckAndUpdateState();
		}
	}
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::BrowserJSTime, 0));
	OpStatus::Ignore(ScheduleResourceCheck());
}

OP_STATUS AutoUpdater::DownloadUpdate()
{
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	m_silent_update = FALSE;
	m_download_retries++;
	return DownloadUpdate(TRUE);
#else
	m_silent_update = TRUE;
	m_download_retries++;
	return DownloadUpdate(FALSE);	
#endif
}

OP_STATUS AutoUpdater::GetDownloadPageURL(OpString& url)
{
	OP_ASSERT(m_xml_downloader);
	return m_xml_downloader->GetDownloadURL(url);
}

OP_STATUS AutoUpdater::DownloadUpdate(BOOL include_resources_requiring_restart)
{
	OP_ASSERT(m_xml_downloader);

	if(m_update_state != AUSUpdateAvailable && m_update_state != AUSErrorDownloading)
	{
		BroadcastOnError(AUInternalError, IsSilent());
		return OpStatus::ERR;
	}

	m_include_resources_requiring_restart = include_resources_requiring_restart;
	
	// Destroy the retry download timer if we have one.
	if(m_download_timer)
	{
		OP_DELETE(m_download_timer);
		m_download_timer = NULL;
	}
	
	OP_STATUS ret = StartNextDownload(NULL);
	
	if(OpStatus::IsSuccess(ret))
	{
		SetUpdateState(AUSDownloading);
		CheckAndUpdateState();
	}
	else
	{
		if(ret == OpStatus::ERR_NO_SUCH_RESOURCE)
		{
			// No files to download, but continue updating other resources
			SetUpdateState(AUSReadyToUpdate);
			CheckAndUpdateState();				
		}
		else
		{
			StopDownloads();
			SetLastError(AUConnectionError);
			SetUpdateState(AUSErrorDownloading);
			CheckAndUpdateState();
		}
	}
	
	return OpStatus::OK;
}

OP_STATUS AutoUpdater::StartNextDownload(FileDownloader* previous_download)
{
	OP_ASSERT(m_xml_downloader);

	UpdatableResource* resource = m_xml_downloader->GetFirstResource();
	BOOL found_previous_download = previous_download == NULL ? TRUE : FALSE;
	for(; resource; resource = m_xml_downloader->GetNextResource())
	{
		if (	resource->GetResourceClass() == UpdatableResource::File
			&& (m_include_resources_requiring_restart || !resource->UpdateRequiresRestart()))
		{
			UpdatableFile* file = static_cast<UpdatableFile*>(resource);

			// Start with the first download after previous_download
			if(!found_previous_download)
			{
				if(file == previous_download)
				{
					found_previous_download = TRUE;
				}
			}
			else
			{
				return file->StartDownloading(this);
			}
		}
	}
	
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS AutoUpdater::StopDownloads()
{
	OP_ASSERT(m_xml_downloader);

	UpdatableResource* resource = m_xml_downloader->GetFirstResource();
	while(resource)
	{
		if(resource->GetResourceClass() == UpdatableResource::File)
		{
			UpdatableFile* file = static_cast<UpdatableFile*>(resource);
			file->StopDownload();
		}
		resource = m_xml_downloader->GetNextResource();
	}
	return OpStatus::OK;
}

OP_STATUS AutoUpdater::StartUpdate(BOOL include_resources_requiring_restart)
{
	OP_ASSERT(m_xml_downloader);
	OP_ASSERT(m_update_state == AUSReadyToUpdate);

	if(m_update_state != AUSReadyToUpdate)
	{
		BroadcastOnError(AUInternalError, IsSilent());
		return OpStatus::ERR;
	}	

	SetUpdateState(AUSUpdating);
	CheckAndUpdateState(); // broadcast updating state

	BOOL call_ready_to_install = FALSE;
	BOOL waiting_for_unpacking = FALSE;
	BOOL update_error = FALSE;
	
	UpdatableResource* dl_elm = m_xml_downloader->GetFirstResource();
	for (; dl_elm; dl_elm = m_xml_downloader->GetNextResource())
	{
		if (	include_resources_requiring_restart
			||	!dl_elm->UpdateRequiresRestart())
		{
			if(dl_elm->CheckResource())
			{
				if(OpStatus::IsSuccess(dl_elm->UpdateResource()))
				{
					if(dl_elm->UpdateRequiresUnpacking())
					{
						waiting_for_unpacking = TRUE;
						g_main_message_handler->SetCallBack(this, MSG_AUTOUPDATE_UNPACKING_COMPLETE, 0);
					}
					if(dl_elm->UpdateRequiresRestart())
					{
						call_ready_to_install = TRUE;
					}


					
					OpString message;
					message.AppendFormat(UNI_L("Updated %s resource."), dl_elm->GetResourceName());
					Console::WriteMessage(message.CStr());
				}
				else
				{
					OpString error;
					error.AppendFormat(UNI_L("Failed to update %s resource."), dl_elm->GetResourceName());
					Console::WriteError(error.CStr());
					
					SetLastError(AUUpdateError);											

					if(dl_elm->UpdateRequiresRestart())
					{
						update_error = TRUE;
					}
				}
			}
			else
			{
				OpString error;
				error.AppendFormat(UNI_L("Resource check failed for %s resource."), dl_elm->GetResourceName());
				Console::WriteError(error.CStr());

				SetLastError(AUValidationError);
				
				if(dl_elm->UpdateRequiresRestart())
				{
					update_error = TRUE;
				}
			}
		}
	}

	// Clean up resources
	OpStatus::Ignore(CleanupAllResources());
	
	if (waiting_for_unpacking)
	{
		SetUpdateState(AUSUnpacking);
		if(call_ready_to_install)
			m_state_after_unpacking = AUSReadyToInstall;
		else if(update_error)
			m_state_after_unpacking = AUSUpdateAvailable;
		else
			m_state_after_unpacking = AUSUpToDate;
	}
	else if(call_ready_to_install)
	{
		SetUpdateState(AUSReadyToInstall);
	}
	else if(update_error)
	{
		BroadcastOnError(m_last_error, IsSilent());
		SetUpdateState(AUSUpdateAvailable);
		return OpStatus::ERR;
	}
	else
	{
		BroadcastOnFinishedUpdating();
		SetUpdateState(AUSUpToDate);
	}
	CheckAndUpdateState();
	return OpStatus::OK;
}

void AutoUpdater::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_AUTOUPDATE_UNPACKING_COMPLETE)
	{
		g_main_message_handler->UnsetCallBack(this, MSG_AUTOUPDATE_UNPACKING_COMPLETE);

		if (m_state_after_unpacking == AUSUpdateAvailable)
		{
			BroadcastOnError(m_last_error, IsSilent());
		}
		else if (m_state_after_unpacking == AUSUpToDate)
		{
			BroadcastOnFinishedUpdating();
		}

		SetUpdateState(m_state_after_unpacking);
		CheckAndUpdateState();
	}
}

OP_STATUS AutoUpdater::CleanupAllResources()
{
	OP_ASSERT(m_xml_downloader);

	OP_STATUS ret = OpStatus::OK;
	UpdatableResource* resource = m_xml_downloader->GetFirstResource();
	while (resource)
	{
		ret |= resource->Cleanup();
		resource = m_xml_downloader->GetNextResource();
	}

	return ret;
}

void AutoUpdater::OnTimeOut(OpTimer *timer)
{
	if (timer == m_download_timer)
	{
		m_download_countdown--;
		if(m_download_countdown > 0)
		{
			m_download_timer->Start(1000);
			BroadcastOnRestartingDownload(m_download_countdown, m_last_error, IsSilent());
		}
		else
		{
			OP_DELETE(m_download_timer);
			m_download_timer = NULL;
			m_download_retries++;
			DownloadUpdate(TRUE);
		}
		return;
	}

	if (timer == m_update_check_timer)
	{
		if(!g_ssl_auto_updaters->Active())
		{
			OP_DELETE(m_update_check_timer);
			m_update_check_timer = NULL;
			m_silent_update = TRUE;

			RETURN_VOID_IF_ERROR(DownloadStatusXML());
		}
		else
		{
			// If the SSL auto update is active, schedule auto update check later
			m_update_check_timer->Start(AUTOUPDATE_SSL_UPDATERS_BUSY_RECHECK_TIMEOUT_SEC * 1000);
		}
		return;
	}
#ifndef _MACINTOSH_
	if (timer == m_checker_launch_retry_timer)
	{
		TryRunningChecker();
		return;
	}
#endif // _MACINTOSH_

	OP_ASSERT("Unknown timer!");
}

void AutoUpdater::GetDownloadStatus(INT32* total_file_count, INT32* downloaded_file_count, INT32* failed_download_count)
{
	OP_ASSERT(m_xml_downloader);

	if(total_file_count)
		*total_file_count = 0;

	if(downloaded_file_count)
		*downloaded_file_count = 0;

	if(failed_download_count)
		*failed_download_count = 0;

	UpdatableResource* resource = m_xml_downloader->GetFirstResource();
	while (resource)
	{
		if(resource->GetResourceClass() == UpdatableResource::File)
		{
			UpdatableFile* file = static_cast<UpdatableFile*>(resource);
			if(file->DownloadStarted())
			{
				if(downloaded_file_count && file->Downloaded())
				{
					(*downloaded_file_count)++;
				}
				if(failed_download_count && file->DownloadFailed())
				{
					(*failed_download_count)++;
				}
				if(total_file_count)
				{
					(*total_file_count)++;
				}
			}
		}
		resource = m_xml_downloader->GetNextResource();
	}
}

BOOL AutoUpdater::CheckAllResources()
{
	OP_ASSERT(m_xml_downloader);

	UpdatableResource* resource = m_xml_downloader->GetFirstResource();
	int resource_checked_count = 0;
	while (resource)
	{
		if(!resource->CheckResource())
		{
			OpString error;
			error.AppendFormat(UNI_L("Resource check failed for %s resource."), resource->GetResourceName());
			Console::WriteError(error.CStr());
			return FALSE;
		}
		resource_checked_count++;
		resource = m_xml_downloader->GetNextResource();
	}
	return (resource_checked_count > 0) ? TRUE : FALSE;
}

#ifndef _MACINTOSH_
namespace
{
BOOL LaunchTheChecker(const uni_char* path, int channel_id)
{
	const int argc = 18;
	char* argv[argc];
	OperaVersion version;
	version.AllowAutoupdateIniOverride();
	OpString8 tmp_str;
	tmp_str.Empty();
	tmp_str.AppendFormat("%d.%02d.%04d", version.GetMajor(), version.GetMinor(), version.GetBuild());
	argv[0] = op_strdup("-version");
	argv[1] = op_strdup(tmp_str.CStr());
	argv[2] = op_strdup("-host");
	tmp_str.SetUTF8FromUTF16(g_pcui->GetStringPref(PrefsCollectionUI::AutoUpdateGeoServer));
	argv[3] = op_strdup(tmp_str.CStr());
	argv[4] = op_strdup("-firstrunver");
	tmp_str.SetUTF8FromUTF16(g_pcui->GetStringPref(PrefsCollectionUI::FirstVersionRun));
	argv[5] = op_strdup(tmp_str.CStr());
	argv[6] = op_strdup("-firstrunts");
	tmp_str.Empty();
	tmp_str.AppendFormat("%d", g_pcui->GetIntegerPref(PrefsCollectionUI::FirstRunTimestamp));
	argv[7] = op_strdup(tmp_str.CStr());
	argv[8] = op_strdup("-loc");
	tmp_str.Empty();
	OpStringC user = g_pcui->GetStringPref(PrefsCollectionUI::CountryCode);
	OpStringC detected = g_pcui->GetStringPref(PrefsCollectionUI::DetectedCountryCode);
	OpString tmp_loc;
	tmp_loc.AppendFormat("%s;%s;%s;%s", user.HasContent() ? user.CStr() : UNI_L("empty"),
	                     detected.HasContent() ? detected.CStr() : UNI_L("empty"),
	                     g_region_info->m_country.HasContent() ? g_region_info->m_country.CStr() : UNI_L("empty"),
	                     g_region_info->m_region.HasContent() ? g_region_info->m_region.CStr() : UNI_L("empty"));
	tmp_str.SetUTF8FromUTF16(tmp_loc.CStr());
	argv[9] = op_strdup(tmp_str.CStr());
	argv[10] = op_strdup("-lang");
	tmp_str.SetUTF8FromUTF16(g_languageManager->GetLanguage().CStr());
	argv[11] = op_strdup(tmp_str.CStr());
	argv[12] = op_strdup("-pipeid");
	tmp_str.Empty();
	tmp_str.AppendFormat("%d", channel_id);
	argv[13] = op_strdup(tmp_str.CStr());
	argv[14] = op_strdup("-producttype");
#ifdef _DEBUG
	argv[15] = op_strdup("Debug");
#else
	switch (g_desktop_product->GetProductType())
	{
		case PRODUCT_TYPE_OPERA:
			argv[15] = op_strdup(""); break;
		case PRODUCT_TYPE_OPERA_NEXT:
			argv[15] = op_strdup("Next"); break;
		case PRODUCT_TYPE_OPERA_LABS:
			argv[15] = op_strdup("Labs");break;
		default:
			argv[15] = op_strdup(""); break;
	}
#endif // _DEBUG

	/* Send path to SSL certificate file. This is irrelevant on platforms that
	 * use OpenSSL and supply the certificate file from memory. The standalone
	 * checkers on these platforms will ignore this argument. The checkers that
	 * rely only on CURL and need to supply the certificate in a file will
	 * expect a -certfile argument with the path. It should be:
	 * OPFILE_RESOURCES_FOLDER/cert.pem */
	OpString cert_dir;
	OP_STATUS status = g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, cert_dir);
	OpString8 cert_file8;
	if (OpStatus::IsSuccess(status))
	{
		OpString cert_file;
		status += OpPathDirFileCombine(cert_file,
									   cert_dir,
									   OpStringC(UNI_L("cert.pem")));
		status += cert_file8.SetUTF8FromUTF16(cert_file);
	}
	if (OpStatus::IsSuccess(status))
	{
		argv[16] = op_strdup("-certfile");
		argv[17] = op_strdup(cert_file8.CStr());
	}
	else
	{
		argv[16] = NULL; // Likely an OOM in GetFolderPath or string operations
		argv[17] = NULL; // This will fail the launch
	}

	BOOL valid = TRUE;
	for (int i = 0; i < argc; ++i)
	{
		if (!argv[i])
		{
			valid = FALSE;
			break;
		}
	}
	if (valid)
		valid = g_launch_manager->Launch(path, argc, argv);

	for (int i = 0; i < argc; ++i)
		op_free(argv[i]);

	return valid;
}
}

OP_STATUS AutoUpdater::TryRunningChecker()
{
	OP_STATUS status = OpStatus::OK;
	OP_DELETE(m_checker_launch_retry_timer);
	m_checker_launch_retry_timer = NULL;
	m_checker_state = CheckerOk;
	BOOL ok = LaunchTheChecker(m_checker_path.CStr(), m_channel_id);
	if (!ok)
	{
		m_checker_state = CheckerCouldntLaunch;
		if (++m_checker_launch_retry_count <= MAX_CHECKER_LAUNCH_RETRIES)
		{
			m_checker_launch_retry_timer = OP_NEW(OpTimer, ());
			if (m_checker_launch_retry_timer)
			{
				m_checker_launch_retry_timer->SetTimerListener(this);
				m_checker_launch_retry_timer->Start(CHECKER_LAUNCH_RETRY_TIME_MS);
			}
			else
			{
				m_checker_launch_retry_count = 0;
				status = OpStatus::ERR_NO_MEMORY;
			}
		}
		else // give up.
			m_checker_launch_retry_count = 0;
	}
	else
	{
		m_checker_launch_retry_count = 0;
		if (m_channel_to_checker)
			m_channel_to_checker->Connect(25); // This is only so the updater is connected and it does not retry. We won't be using its results here.
	}

	SetUpdateState(m_update_state); // Force asving the state.
	return status;
}
#endif // _MACINTOSH_

OP_STATUS AutoUpdater::DownloadStatusXML()
{
	OP_ASSERT(m_xml_downloader);
	OP_ASSERT(m_autoupdate_xml);
	OP_ASSERT(m_update_state == AUSUpToDate || m_update_state == AUSUpdateAvailable);
#ifndef _MACINTOSH_
	OP_ASSERT(m_checker_path.Length() > 0);
#endif // _MACINTOSH_

	if (m_update_state != AUSUpToDate && m_update_state != AUSUpdateAvailable)
	{
		BroadcastOnError(AUInternalError, IsSilent());
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	if (m_resources_check_only)
		Console::WriteMessage(UNI_L("Starting a resource check."));
	else
		Console::WriteMessage(UNI_L("Starting an update check."));

	// Stop the update check timer, if we have one
	OP_DELETE(m_update_check_timer);
	m_update_check_timer = NULL;

	SetUpdateState(AUSChecking);
	CheckAndUpdateState();

	AutoUpdateXML::AutoUpdateLevel autoupdate_level = AutoUpdateXML::UpdateLevelResourceCheck;

	if (!m_resources_check_only)
	{
		if (m_level_of_automation > NoChecking || !IsSilent())
		{
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
			if (!HasDownloadedNewerVersion())
#endif // AUTOUPDATE_PACKAGE_INSTALLATION
			{
				autoupdate_level = AutoUpdateXML::UpdateLevelDefaultCheck;
			}
		}
	}

	// In case of an error below, a new update check will be scheduled in CheckAndUpdateState().
	// Scheduling an update sets m_resource_check_only to FALSE, so we don't have any retry
	// mechanism for the initial resource check, but we schedule a full update check that
	// will happen when the time comes. A new resource check will be attempted if needed on the
	// next browser start.
	// In case of no error here, we will schedule a new update check after the resource check is
	// over, and that will also set m_resources_check_only to FALSE.
	// However, because we prefer to be rather safe than sorry, we set it explicitly here, so that
	// we don't get stuck in the resource-check-only state somehow.
	m_resources_check_only = FALSE;

	OpString url_string;
	OpString8 xml;

	OP_ASSERT(m_autoupdate_server_url);
	if (OpStatus::IsError(m_autoupdate_server_url->GetCurrentURL(url_string)))
	{
		SetLastError(AUInternalError);
		SetUpdateState(AUSError);
		CheckAndUpdateState();
		return OpStatus::ERR;
	}

	// No addition items are sent with an update request
	m_autoupdate_xml->ClearRequestItems();
	AutoUpdateXML::AutoUpdateLevel used_level;
	if (OpStatus::IsError(m_autoupdate_xml->GetRequestXML(xml, autoupdate_level, AutoUpdateXML::RT_Main, &used_level)))
	{
		SetLastError(AUInternalError);
		SetUpdateState(AUSError);
		CheckAndUpdateState();
		return OpStatus::ERR;
	}

#ifndef _MACINTOSH_
	// The checker needs to be run when launching Opera for the first time and every time the main update is asked for.
	// It also has to be launched if there is no uuid stored yet.
	BOOL run_checker =
			(used_level == AutoUpdateXML::UpdateLevelDefaultCheck
			 || used_level == AutoUpdateXML::UpdateLevelUpgradeCheck
			 || g_run_type->m_type == StartupType::RUNTYPE_FIRST
			 || g_run_type->m_type == StartupType::RUNTYPE_FIRSTCLEAN
			 || g_run_type->m_type == StartupType::RUNTYPE_FIRST_NEW_BUILD_NUMBER)
			&& !m_user_initiated;

	if (run_checker)
		if (OpStatus::IsMemoryError(TryRunningChecker()))
		{
			SetLastError(AUInternalError);
			SetUpdateState(AUSError);
			CheckAndUpdateState();
			return OpStatus::ERR_NO_MEMORY;
		}
#endif // _MACINTOSH_

	// Set time of last check
	m_time_of_last_update_check = g_timecache->CurrentTime();
	OP_STATUS st = WritePref(PrefsCollectionUI::TimeOfLastUpdateCheck, static_cast<int>(m_time_of_last_update_check));
	if (OpStatus::IsError(st))
	{
		SetLastError(AUInternalError);
		SetUpdateState(AUSError);
		CheckAndUpdateState();
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(m_xml_downloader->StartXMLRequest(url_string, xml)))
	{
		m_xml_downloader->StopRequest();
		SetLastError(AUConnectionError);
		SetUpdateState(AUSError);
		CheckAndUpdateState();
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS AutoUpdater::ScheduleResourceCheck()
{
	if (AUSChecking == m_update_state)
	{
		// A check is already in progress.
		// We hope to get the new resources along with the check.
		// Ignore this request.
		Console::WriteMessage(UNI_L("Ignoring resource check request."));
		return OpStatus::ERR_NO_ACCESS;
	}

	if (m_update_check_timer)
	{
		// An update check is already scheduled.
		// Forget that, schedule an immediate resource check, the update check
		// will be scheduled after we have finished.

		OP_DELETE(m_update_check_timer);
		m_update_check_timer = NULL;

		Console::WriteMessage(UNI_L("Suspending the update check schedule in order to make a resource check."));
	}

	m_update_check_timer = OP_NEW(OpTimer, ());
	if (!m_update_check_timer)
	{
		Console::WriteError(UNI_L("Could not schedule a resource check."));
		return OpStatus::ERR_NO_MEMORY;
	}

	// Schedule the resource check as required. Note, that this timeout should not be set "too high", since it is
	// responsible for downloading the new browserjs after upgrading Opera and having the old browserjs may cause
	// quite serious problems. It should not also be set "too low" since we might hit the browser startup phase
	// and slow it down.
	int seconds_to_resource_check = CalculateTimeOfResourceCheck();
	m_resources_check_only = TRUE;
	m_update_check_timer->SetTimerListener(this);
	m_update_check_timer->Start(seconds_to_resource_check * 1000);

	Console::WriteMessage(UNI_L("Scheduled an immediate resource check."));

	return OpStatus::OK;
}

OP_STATUS AutoUpdater::ScheduleUpdateCheck(ScheduleCheckType schedule_type)
{
	OP_ASSERT(m_update_state == AUSUpToDate);
	OP_ASSERT(!m_update_check_timer);

	if (m_update_state == AUSUpToDate && !m_update_check_timer)
	{
		m_update_check_timer = OP_NEW(OpTimer, ());

		if (m_update_check_timer)
		{
			int seconds_to_next_check = CalculateTimeOfNextUpdateCheck(schedule_type);

			m_update_check_timer->SetTimerListener(this);
			m_update_check_timer->Start(seconds_to_next_check * 1000);
			m_resources_check_only = FALSE;

			OpString message;
			message.AppendFormat(UNI_L("Scheduled new update check in %d seconds."), seconds_to_next_check);
			Console::WriteMessage(message.CStr());

			return OpStatus::OK;
		}
	}

	Console::WriteError(UNI_L("Failed to schedule new update check."));
	
	return OpStatus::ERR;
}

int AutoUpdater::CalculateTimeOfResourceCheck()
{
	OP_ASSERT(AUTOUPDATE_RESOURCE_CHECK_BASE_TIMEOUT_SEC > 0);
	OP_ASSERT(m_update_check_delay >= 0);

	int timeout = AUTOUPDATE_RESOURCE_CHECK_BASE_TIMEOUT_SEC + m_update_check_delay;
	return timeout;
}

int AutoUpdater::CalculateTimeOfNextUpdateCheck(ScheduleCheckType schedule_type)
{
	// Set the time the code below will normalize it if needed.
	int time_between_checks_sec = m_update_check_interval + m_update_check_delay + m_update_check_random_delta;
	int seconds_to_next_check = 0;

	// Make sure the time between checks is in range [MIN_UPDATE_CHECK_TIMEOUT, UPDATE_CHECK_INTERVAL_MAX]
	OP_ASSERT(MIN_UPDATE_CHECK_INTERVAL_SEC < MAX_UPDATE_CHECK_INTERVAL_SEC);
	OP_ASSERT(MIN_UPDATE_CHECK_INTERVAL_RUNNING_SEC < MAX_UPDATE_CHECK_INTERVAL_SEC);

	if (ScheduleRunning == schedule_type)
		time_between_checks_sec = MAX(time_between_checks_sec, MIN_UPDATE_CHECK_INTERVAL_RUNNING_SEC + m_update_check_random_delta);
	else if (ScheduleStartup == schedule_type)
		time_between_checks_sec = MAX(time_between_checks_sec, MIN_UPDATE_CHECK_INTERVAL_SEC);
	else
	{
		OP_ASSERT("Unknown ScheduleCheckType!");
	}

	time_between_checks_sec = MIN(time_between_checks_sec, MAX_UPDATE_CHECK_INTERVAL_SEC);

	time_t time_now_sec = g_timecache->CurrentTime();
	time_t time_of_next_scheduled_check = m_time_of_last_update_check + time_between_checks_sec;

	if(time_of_next_scheduled_check < time_now_sec)
		seconds_to_next_check = 0;
	else
		seconds_to_next_check = time_of_next_scheduled_check - time_now_sec;

	// To ensure that we dont check too often.
#ifdef _DEBUG
	seconds_to_next_check = MAX(seconds_to_next_check, 1);
#else
	seconds_to_next_check = MAX(seconds_to_next_check, 5);
#endif
	return seconds_to_next_check;
}

OP_STATUS AutoUpdater::ScheduleDownload()
{
	OP_ASSERT(m_update_state == AUSErrorDownloading);
	OP_ASSERT(!m_download_timer);
	
	if(m_update_state != AUSErrorDownloading)
		return OpStatus::ERR;
	if(m_download_timer)
		return OpStatus::ERR;

	m_download_timer = OP_NEW(OpTimer, ());

	if(!m_download_timer)
		return OpStatus::ERR_NO_MEMORY;

	m_download_timer->SetTimerListener(this);
	m_download_timer->Start(1000);
	m_download_countdown = 60;

	return OpStatus::OK;
}

void AutoUpdater::BroadcastOnUpToDate(BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnUpToDate(silent);
	}
}

void AutoUpdater::BroadcastOnChecking(BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnChecking(silent);
	}
}

void AutoUpdater::BroadcastOnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnUpdateAvailable(type, update_size, update_info_url, silent);
	}
}

void AutoUpdater::BroadcastOnDownloading(UpdatableResource::UpdatableResourceType type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate, BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDownloading(type, total_size, downloaded_size, kbps, time_estimate, silent);
	}
}

void AutoUpdater::BroadcastOnDownloadingDone(UpdatableResource::UpdatableResourceType type, const OpString& filename, OpFileLength total_size, BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDownloadingDone(type, filename, total_size, silent);
	}
}

void AutoUpdater::BroadcastOnDownloadingFailed(BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDownloadingFailed(silent);
	}
}

void AutoUpdater::BroadcastOnRestartingDownload(INT32 seconds_until_restart, AutoUpdateError last_error, BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnRestartingDownload(seconds_until_restart, last_error, silent);
	}
}

void AutoUpdater::BroadcastOnReadyToUpdate()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnReadyToUpdate();
	}
}

void AutoUpdater::BroadcastOnUpdating()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnUpdating();
	}
}

void AutoUpdater::BroadcastOnFinishedUpdating()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFinishedUpdating();
	}
}

void AutoUpdater::BroadcastOnUnpacking()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnUnpacking();
	}
}

void AutoUpdater::BroadcastOnReadyToInstallNewVersion(const OpString& version, BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnReadyToInstallNewVersion(version, silent);
	}
}

void AutoUpdater::BroadcastOnError(AutoUpdateError error, BOOL silent)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnError(error, silent);
	}
}

OP_STATUS AutoUpdater::SetUpdateState(AutoUpdateState update_state)
{
#ifndef _MACINTOSH_
	int state = m_checker_state << 16 | update_state;
#else
    int state = update_state;
#endif // _MACINTOSH_
	RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::AutoUpdateState, state));
	TRAPD(err, g_prefsManager->CommitL());
	m_update_state = update_state;
	return OpStatus::OK;
}

void AutoUpdater::Reset()
{
	m_download_retries = 0;

	OP_DELETE(m_update_check_timer);
	m_update_check_timer = NULL;

	OP_DELETE(m_download_timer);
	m_download_timer = NULL;
}

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION			
BOOL AutoUpdater::HasDownloadedNewerVersion()
{
	AUDataFileReader reader;
	if(OpStatus::IsSuccess(reader.Init()) &&
	   OpStatus::IsSuccess(reader.LoadFromOpera()))
	{
		OperaVersion downloaded_version;

		uni_char* version = reader.GetVersion();
		uni_char* buildnum = reader.GetBuildNum();
		
		OpString version_string;
		
		if(OpStatus::IsSuccess(version_string.Set(version)) &&
		   OpStatus::IsSuccess(version_string.Append(".")) &&
		   OpStatus::IsSuccess(version_string.Append(buildnum)) &&
		   OpStatus::IsSuccess(downloaded_version.Set(version_string)))
		{
			if(m_opera_version < downloaded_version)
			{
				// Only resource check if we have downloaded a higher version
				return TRUE;
			}
		}
		OP_DELETEA(version);
		OP_DELETEA(buildnum);
	}
	return FALSE;
}
#endif // AUTOUPDATE_PACKAGE_INSTALLATION

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
void AutoUpdater::DeleteUpgradeFolderIfNeeded()
{
	// Create the aufile utils object
	AUFileUtils* file_utils  = AUFileUtils::Create();
	if (!file_utils)
		return;

	// If the last installation succedded then clean up the remains of the
	// special upgrade folder
	OpString autoupdate_textfile_path;
	uni_char* temp_folder = NULL;

	if (file_utils->GetUpgradeFolder(&temp_folder) == AUFileUtils::OK)
	{				
		// Build the path to the text file
		RETURN_VOID_IF_ERROR(autoupdate_textfile_path.Set(temp_folder));
		RETURN_VOID_IF_ERROR(autoupdate_textfile_path.Append(PATHSEP));
		RETURN_VOID_IF_ERROR(autoupdate_textfile_path.Append(AUTOUPDATE_UPDATE_TEXT_FILENAME));
		
		// Check if the upgrade folder even exists
		OpFile	upgrade_path;
		BOOL	exists = FALSE;
		
		// Build the OpFile
		RETURN_VOID_IF_ERROR(upgrade_path.Construct(temp_folder));
		
		// If the file doesn't exist kill the folder
		if (OpStatus::IsSuccess(upgrade_path.Exists(exists)) && exists)
		{
			// If the text file exists then don't clean up we can assume they want to do this next time
			// Otherwise kill the folder
			OpFile	autoupdate_textfile;
			
			// reset exists
			exists = FALSE;
			
			RETURN_VOID_IF_ERROR(autoupdate_textfile.Construct(autoupdate_textfile_path));
			
			// If the file doesn't exist kill the folder
			if (OpStatus::IsSuccess(autoupdate_textfile.Exists(exists)) && !exists)
			{
				// Delete this entire folder
				RETURN_VOID_IF_ERROR(upgrade_path.Delete(TRUE));
			}
		}
	}
	OP_DELETEA(temp_folder);
	OP_DELETE(file_utils);
}
#endif // AUTOUPDATE_PACKAGE_INSTALLATION

void AutoUpdater::IncreaseDelayedUpdateCheckInterval()
{
	m_update_check_delay = MIN(m_update_check_delay + UPDATE_CHECK_INTERVAL_DELTA_SEC, MAX_UPDATE_CHECK_INTERVAL_SEC);
	// Don't let increasing the upgrade check time to break any further operation
	OpStatus::Ignore(WritePref(PrefsCollectionUI::DelayedUpdateCheckInterval, m_update_check_delay));
}

void Console::WriteMessage(const uni_char* message)
{
#ifdef OPERA_CONSOLE
	OpConsoleEngine::Message cmessage(OpConsoleEngine::AutoUpdate, OpConsoleEngine::Verbose);
	cmessage.message.Set(message);
	TRAPD(err, g_console->PostMessageL(&cmessage));
#endif
}

void Console::WriteError(const uni_char* error)
{
#ifdef OPERA_CONSOLE
	OpConsoleEngine::Message cmessage(OpConsoleEngine::AutoUpdate, OpConsoleEngine::Error);
	cmessage.message.Set(error);
	TRAPD(err, g_console->PostMessageL(&cmessage));
#endif
}

#endif // AUTO_UPDATE_SUPPORT
