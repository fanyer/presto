/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton, Michal Zajaczkowski
 */

#ifndef _AUTOUPDATER_H_INCLUDED_
#define _AUTOUPDATER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT

#define g_autoupdater	(&AutoUpdater::GetInstance())

#include "adjunct/autoupdate/autoupdatexml.h"
#include "adjunct/autoupdate/platformupdater.h"
#include "adjunct/autoupdate/file_downloader.h"
#include "adjunct/autoupdate/updatableresource.h"
#include "adjunct/autoupdate/statusxmldownloader.h"
#include "adjunct/desktop_util/version/operaversion.h"
#include "adjunct/autoupdate/scheduler/optaskscheduler.h"

#include "modules/util/adt/oplisteners.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif

class AutoUpdateXML;
class UpdatableResource;
class StatusXMLDownloader;
class StatusXMLDownloaderListener;

#ifndef _MACINTOSH_
namespace opera_update_checker { namespace ipc {
class Channel;
}}
#endif // _MACINTOSH_

enum AutoUpdateError
{
	AUNoError,			//< No error
	AUInternalError,	//< wrong initializations of objects etc.
	AUInProgressError,	//< check for updates while an update is in progress
	AUConnectionError,	//< no network connection, server not reachable, failed ssl handshake etc.
	AUSaveError,		//< no permission, not enough disk space
	AUValidationError,	//< extraction failed, wrong checksum, corrupt/invalid package, wrong format/file, wrong Opera version, XML parsing error
	AUUpdateError		//< error when updating resources
};

/**
 * Enum for the automation level
 */
enum LevelOfAutomation
{
	NoChecking = 0,		///< Opera shall never automatically connect to opera.com to retrieve updates or information about them.
	CheckForUpdates,	///< Opera shall periodically connect to opera.com to get updates to resources, as well as notify the user about available updates to the application.
	AutoInstallUpdates	///< Opera shall periodically connect to opera.com to get updates to resources, as well as automatically download and install available security (minor) updates to the application, and notify the user that an update has taken place.
};

/**
 * This class is the base class for the auto update functionallity.
 *
 * It should contain all code to do any platform/product independent 
 * functionallity such as version comparisons, xml parsing, 
 * downloading of files etc.
 *
 * @see platformupdater.h For the interface used to manage updates on the platform.
 *
*/
class AutoUpdater:
	public FileDownloadListener,
	public MessageObject,
	public OpTimerListener,
	public StatusXMLDownloaderListener,
	public OpPrefsListener
{
public:
	enum ScheduleCheckType
	{
		// Used when scheduling next update check on browser startup
		ScheduleStartup,
		// Used when scheduling next update check after previous check completed
		ScheduleRunning
	};

	enum AutoUpdateState
	{
		AUSUpToDate,
		AUSChecking,
		AUSUpdateAvailable,
		AUSDownloading,
		AUSReadyToUpdate,
		AUSUpdating,
		AUSUnpacking,
		AUSReadyToInstall,
		AUSErrorDownloading,
		AUSError
	};

	enum CheckerState
	{
		CheckerOk,
		CheckerCouldntLaunch
	};

	class AutoUpdateListener
	{
	public:
		virtual ~AutoUpdateListener() {}

		virtual void OnUpToDate(BOOL silent) = 0;
		virtual void OnChecking(BOOL silent) = 0;
		virtual void OnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL silent) = 0;

		virtual void OnDownloading(UpdatableResource::UpdatableResourceType type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate, BOOL silent) = 0;
		virtual void OnDownloadingDone(UpdatableResource::UpdatableResourceType type, const OpString& filename, OpFileLength total_size, BOOL silent) = 0;
		virtual void OnDownloadingFailed(BOOL silent) = 0;
		virtual void OnRestartingDownload(INT32 seconds_until_restart, AutoUpdateError last_error, BOOL silent) = 0;
		virtual void OnReadyToUpdate() = 0;
		virtual void OnUpdating() = 0;
		virtual void OnFinishedUpdating() = 0;
		virtual void OnUnpacking() = 0;
		virtual void OnReadyToInstallNewVersion(const OpString& version, BOOL silent) = 0;
		virtual void OnError(AutoUpdateError error, BOOL silent) = 0;
	};

	/**
	 * Initialize the updater. Also call Activate to run it, as this
	 * does not start the auto updater! Call Activate() when this function
	 * returns OK.
	 *
	 * @return OK if the updater is correctly initialized and ready to go, 
	 * err if there was some problem.
	 */
	virtual OP_STATUS Init(BOOL check_increased_update_check_interval = TRUE);

	/**
	 * Add an autoupdate listener
	 */
	OP_STATUS AddListener(AutoUpdateListener* listener) { return m_listeners.Add(listener); }
	/**
	 * Remove an autoupdate listener
	 */
	OP_STATUS RemoveListener(AutoUpdateListener* listener) { return m_listeners.Remove(listener); }

	virtual void StatusXMLDownloaded(StatusXMLDownloader* downloader);
	virtual void StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus);

	/** Signals a change in an integer preference.
	  * Function from OpPrefsListener interface.
	  *
	  * @param id       Identity of the collection that has changed the value.
	  * @param pref     Identity of the preference.
	  * @param newvalue The new value of the preference.
	  */
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	/**
	 * Activate the updater. AutoUpdater won't check for new updates
	 * until this function is called. If a new update is due, 
	 * this function will initate it. If it's not due, this 
	 * function will schedule it.
	 */
	OP_STATUS Activate();
	/**
	 * Check for updates right now
	 */
	OP_STATUS CheckForUpdate();
	/**
	 * Called from FileDownloader when file download done
	 */
	virtual void OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size);
	/**
	 * Called from FileDownloader when file download failed
	 */
	virtual void OnFileDownloadFailed(FileDownloader* file_downloader);
	/**
	 * Called from FileDownloader when file download was aborted
	 */
	virtual void OnFileDownloadAborted(FileDownloader* file_downloader);
	/**
	 * Called from FileDownloader when file download progresses
	 */
	virtual void OnFileDownloadProgress(FileDownloader* file_downloader, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate);
	/**
	 * Callback used to signal that the
	 * user has requested to be reminded later of some upgrade
	 *
	 * @param wait Interval to wait before notifying again.
	 */
	void DeferUpdate();
	/**
	 * Callback used to signal that the
	 * user has requested that an update download should begin.
	 */
	OP_STATUS DownloadUpdate();
	/**
	 * Get download page url (www.opera.com/download)
	 */
	OP_STATUS GetDownloadPageURL(OpString& url);
	/**
	 * Get the Opera version
	 */
	static OperaVersion& GetOperaVersion() { return m_opera_version; }

	/**
	 * Stores the given int preference value and commits the preference store. Does leave trapping and converts it to OP_STATUS.
	 *
	 * @param which - The int preference ID to store, i.e. PrefsCollectionUI::TimeOfLastUpdateCheck
	 * @param val - The new preference value to be stored
	 *
	 * @returns - OpStatus::OK saving and commiting the change went well, error code otherwise.
	 */
	OP_STATUS WritePref(PrefsCollectionUI::integerpref which, int val);

	AutoUpdater();

	/**
	 * Destructor.
	 */
	virtual ~AutoUpdater();

	/** Sets the user initiated flag. @see m_user_initiated. */
	void SetIsUserInitited(BOOL val) { m_user_initiated = val; }

protected:
	StatusXMLDownloader*						m_xml_downloader;		///< Holds the downloader that manages downloading the status xml.
	AutoUpdateXML*								m_autoupdate_xml;
	AutoUpdateServerURL*						m_autoupdate_server_url;

	/**
	 * OpTimerListener implementation
	 */
	virtual void OnTimeOut( OpTimer *timer );
	
	/**
	 * Get download status
	 */	
	void GetDownloadStatus(INT32* total_file_count, INT32* downloaded_file_count, INT32* failed_download_count);

	/**
	 * Check all resources
	 */		
	BOOL CheckAllResources();

	/**
	 * Get the type of the (possibly) available update
	 */
	UpdatableResource::UpdatableResourceType GetAvailableUpdateType() const;
	
	/**
	 * Get the version of the (possibly) available update
	 */
	BOOL GetAvailablePackageInfo(OpString* version, OpString* info_url, OpFileLength* size) const;

	/**
	 * Get the total size of the update
	 */
	OpFileLength GetTotalUpdateSize() const;

	/**
	 * Check current update state, and decide on next action
	 */	
	virtual OP_STATUS CheckAndUpdateState();

	/**
	 * MessageObject override
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	/*
	 * The autoupdate automatic check scheduling works by the following rules. All time values are given in seconds.
	 *
	 * Preferences:
	 *
	 * PrefsCollectionUI::TimeOfLastUpdateCheck			- The timestamp of the last update check, saved after each automatic autoupdate check request is made with success;
	 * PrefsCollectionUI::UpdateCheckInterval			- Number of seconds between automatic update checks. If changed by the autoupdate server in response to an autoupdate 
	 *													  request, will cause to drop the package update, should any be received with the autoupdate response;
	 * PrefsCollectionUI::DelayedUpdateCheckInterval	- Number of seconds added to PrefsCollectionUI::UpdateCheckInterval while scheduling an autoupdate check, will be 
	 *													  increased by the autoupdate mechanism when an error occurs during automatic update check, will be returned back to
	 *													  0 after a succesful download of the autoupdate XML response;
	 * 
	 * Constants:
	 * These values are mainly used to guard the settings read from preferences.
	 *
	 * MAX_UPDATE_CHECK_INTERVAL_SEC			- Maximum interval between two automatic checks;
	 * MIN_UPDATE_CHECK_INTERVAL_SEC			- Minimum interval between two automatic checks, used when scheduling a check after the browser has started;
	 * MIN_UPDATE_CHECK_INTERVAL_RUNNING_SEC	- Minimum interval between two automatic checks, used when scheduling a check after a previous check during one browser session;
	 * UPDATE_CHECK_INTERVAL_DELTA				- The number of seconds that will be added to the DelayedUpdateCheckInterval after an unsuccesful autoupdate check;
	 * AUTOUPDATE_RECHECK_TIMEOUT				- The number of seconds that need to pass in order to redownload the autoupdate XML after the browser exited duting downloading of an
	 *											  update, most likely as a result of a crash. If the browser is run again before this time elapses, the already downloaded XML response
	 *											  is used if possible.
	 *
	 * Scheduling an update check:
	 *
	 * 1. Update check is scheduled:
	 *	a) at browser startup - AutoUpdater::Activate();
	 *	b) after a succeful check - AutoUpdater::CheckAndUpdateState() for state AUSUpToDate;
	 *	c) after a check error - AutoUpdater::CheckAndUpdateState() for state AUSError.
	 * 2. The number of seconds to next update check is calculated ad follows:
	 *	The time of last check and time between checks is read from the preferences, see above. The time between checks is the sum of the UpdateCheckInterval and DelayedUpdateCheckInterval values.
	 *	The sum is then normalized so that is falls into the range of [MIN_UPDATE_CHECK_INTERVAL_RUNNING_SEC, MAX_UPDATE_CHECK_INTERVAL_SEC] during normal operation (see 1b and 1c) and 
	 *	[MIN_UPDATE_CHECK_INTERVAL_SEC, MAX_UPDATE_CHECK_INTERVAL_SEC] at browser startup (1a). The time of next scheduled check is then calculated by adding the time between checks to the time 
	 *	of last check read from preferences (TimeOfLastUpdateCheck). If the time of next scheduled check is in the past, the check is scheduled immediately, otherwise the calculated time of next check
	 *	is used. The minimum scheduled time is 1 second for debug builds and 5 seconds for release builds.
	 *
	 */

	/**
	 * Maximal time between two automatic updates
	 */
	static const int MAX_UPDATE_CHECK_INTERVAL_SEC;

	/**
	 * Minimal time between two automatic checks, used when scheduling an update check on browser startup
	 */
	static const int MIN_UPDATE_CHECK_INTERVAL_SEC;

	/**
	 * Minimal time between two automatic checks happening during one browser session
	 */
	static const int MIN_UPDATE_CHECK_INTERVAL_RUNNING_SEC;

	/**
	 * The update check interval will be increased with this value on each update check failure
	 */
	static const int UPDATE_CHECK_INTERVAL_DELTA_SEC;

	/**
	 * Timeout to determine if we should use an already downloaded xml on disk, or check with the server again,
	 * when e.g. downloading was aborted last time we ran Opera.
	 * If the current time minus the time of last check is greater than this timeout, we recheck with the server.  
	 */
	static const int AUTOUPDATE_RECHECK_TIMEOUT_SEC;

	/**
	 * Retry period in case the core SSL updaters are busy at the moment of autoupdate check. The AutoUpdater class will
	 * retry the update check attempt within the given time in such a case.
	 * Value given in seconds.
	 */
	static const int AUTOUPDATE_SSL_UPDATERS_BUSY_RECHECK_TIMEOUT_SEC;

	/**
	 * The base timeout for resource checking, see DSK-336588. During activation of the autoupdate module, we check the
	 * resource timestamps, see AutpUpdateXML::NeedsResourceCheck(). In case we want to update the resources only (i.e.
	 * in case any of the resources checked have a zero timestamp), we trigger a resource update check "right after"
	 * activation of the module.
	 * The "right after" time is determined by this base time with the DelayedUpdateCheckInterval added to it.
	 */
	static const int AUTOUPDATE_RESOURCE_CHECK_BASE_TIMEOUT_SEC;

	/**
	 * Function that starts the download of the status XML document supplying
	 * the necessary parameters. When the document is done downloading, the 
	 * StatusXMLDownloaded will be called. The caller of this function can
	 * just return control and do nothing until this callback arrives.
	 *
	 * @return OK if the download is correctly initiated, err otherwise.
	 */
	OP_STATUS DownloadStatusXML();

	/**
	 * Schedules an immediate resource update check. Sets the m_resource_check_only flag to TRUE. If an update
	 * check is already scheduled, the schedule timer will be deleted and the resource check will be scheduled.
	 * The update check schedule will be restored after the resource check finishes.
	 *
	 * @return OK if the check was scheduled correctly, ERR_NO_ACCESS if a check is currently in progress,
	 *         ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS ScheduleResourceCheck();

	/**
	 * Method encapsulating starting of the timer that triggers an update check.
	 * Sets the m_resource_check_only flag to FALSE.
	 *
	 * @param timeout Timeout in sections until update check
	 *
	 * @return OK if the check was scheduled correctly, ERR otherwise.
	 */
	OP_STATUS ScheduleUpdateCheck(ScheduleCheckType schedule_type);

	/**
	 * This method returns the number of seconds to the next update check at the moment
	 * of the call.
	 *
	 * This method is overriden by selftests in order to schedule an immediate check.
	 *
	 * @param timeout Timeout in sections until update check
	 *
	 * @return The number of seconds that need to pass until the next update check occurs.
	 */
	virtual int CalculateTimeOfNextUpdateCheck(ScheduleCheckType schedule_type);

	/**
	 * This method returns the number of seconds for the initial resource check, if any is
	 * needed. More information can be found in DSK-336588.
	 *
	 * @return The number of seconds that need to pass before we do the resource check.
	 */
	int CalculateTimeOfResourceCheck();

	/**
	 * Function starting downloading of specified types
	 * 
	 * @param include_resources_requiring_restart TRUE if add types that would require restart to complete update
	 *
	 * @return OK if the download was started, ERR otherwise.
	 */
	OP_STATUS DownloadUpdate(BOOL include_resources_requiring_restart);

	/**
	 * Function starting next download
	 * 
	 * @param previous_download Previous finished download
	 *
	 * @return OK if the download started, ERR on failure, ERR_NO_SUCH_RESOURCE if no more files to download.
	 */	
	OP_STATUS StartNextDownload(FileDownloader* previous_download);

	/**
	 * Function stopping downloading
	 * 
	 * @return OK if the downloads were stopped, ERR otherwise.
	 */	
	OP_STATUS StopDownloads();

	/**
	 * Function encapsulating starting of the timer that triggers a new download.
	 * The function will work out how long the timer needs to be set for itself.
	 *
	 * @return OK if the check was scheduled correctly, ERR otherwise.
	 */
	OP_STATUS ScheduleDownload();

	/**
	 * Function to start updating the implied types.
	 *
	 * @param include_resources_requiring_restart TRUE if add types that would require restart to complete update
	 *
	 * @return OK if updating succeeded, ERR otherwise.
	 */	
	OP_STATUS StartUpdate(BOOL include_resources_requiring_restart);

	/**
	 * Function to clean up all resources.
	 */
	OP_STATUS CleanupAllResources();

#ifndef _MACINTOSH_
  /**
   * Tries to run the checker.
   */
  OP_STATUS TryRunningChecker();
#endif // _MACINTOSH_

	/**
	 * Broadcasts autoupdate events to all the listeners
	 *
	 */
	void BroadcastOnUpToDate(BOOL silent);
	void BroadcastOnChecking(BOOL silent);
	void BroadcastOnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL silent);
	void BroadcastOnDownloading(UpdatableResource::UpdatableResourceType type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate, BOOL silent);
	void BroadcastOnDownloadingDone(UpdatableResource::UpdatableResourceType type, const OpString& filename, OpFileLength total_size, BOOL silent);
	void BroadcastOnDownloadingFailed(BOOL silent);
	void BroadcastOnRestartingDownload(INT32 seconds_until_restart, AutoUpdateError last_error, BOOL silent);
	void BroadcastOnReadyToUpdate();
	void BroadcastOnUpdating();
	void BroadcastOnFinishedUpdating();
	void BroadcastOnUnpacking();
	void BroadcastOnReadyToInstallNewVersion(const OpString& version, BOOL silent);
	void BroadcastOnError(AutoUpdateError error, BOOL silent);

	/**
	 * Set autoupdate set
	 */
	OP_STATUS	SetUpdateState(AutoUpdateState update_state);

	/**
	 * Set last error
	 */
	void	SetLastError(AutoUpdateError error) { m_last_error = error; }
	
	/**
	 * Indicates if it is a silently running update.
	 *
	 * If the update is triggered by the user (ie using CheckForUpdates() or DownloadUpdate()),
	 * the update is not silent. Otherwise, it is silent. Exception: if update level is CheckForUpdates,
	 * automatic updates are silent only until an update is available.
	 */
	BOOL	IsSilent() const	{ return m_silent_update; }
	
	/**
	 * Reset internal members.
	 */
	void	Reset();

#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	/**
	 * Check if a newer version has been downloaded
	 *
	 * Check if we have the autoupdate.txt file
	 * This means the user have downloaded a newer version, but not installed it yet.
	 * Dont ask for upgrades in this case.
	 */
	virtual BOOL HasDownloadedNewerVersion();
	
	/**
	 * Delete the upgrade folder if there is no autoupdate.txt there.
	 *
	 */
	void DeleteUpgradeFolderIfNeeded();
#endif
	
	/**
	 * Increases the value stored in the PrefsCollectionUI::DelayedUpdateCheckInterval preference. The value is used
	 * to increase the interval to the next update check in case of error. 
	 * See more information about calculating the time of next update check above.
	 *
	 */
	void IncreaseDelayedUpdateCheckInterval();

	LevelOfAutomation							m_level_of_automation;	///< Holds the level of automation
	AutoUpdateState								m_update_state;			///< Holds the state of updating (whether an update has been discovered)
	OpListeners<AutoUpdateListener>				m_listeners;			///< Listener for autoupdate events for external classes
	BOOL										m_silent_update;		///< Indicates if it is a silently running update 
	AutoUpdateError								m_last_error;			///< Last occurred error
	OpTimer*									m_update_check_timer;	///< Timer for rescheduling update check
	OpTimer*									m_download_timer;		///< Timer for rescheduling download
	INT32										m_download_countdown;	///< Holds the remaining seconds till restarting download
	INT32										m_download_retries;		///< Holds the number retried downloads
	BOOL										m_activated;			///< Set if the autoupdater has been activated
	BOOL										m_check_increased_update_check_interval; ///< Set if autoupdater should check if the server is increasing the update check interval. In case it does, any packages should not be downloaded to avoid overloading the server.
	BOOL										m_include_resources_requiring_restart; ///< When updating include resources that would need restart for update completion 
	BOOL										m_resources_check_only; ///< Determines what update level will be used for the update check, resources only or an full update check.
	static OperaVersion							m_opera_version;		///< Holds the current Opera version
	AutoUpdateState								m_state_after_unpacking; ///<Which state to move to once unpacking is done

	// AutoUpdater keeps time of last update check, interval, and delay in member variables,
	// because prefs, where these values are also stored, may be read only (DSK-353750)
	time_t										m_time_of_last_update_check; ///< Time of last update check in seconds
	INT32										m_update_check_interval;     ///< Interval between update checks in seconds
	INT32										m_update_check_random_delta; ///< A random time delta (in seconds) added to the check interval in order to have slightly different check time. It's needed by the server to detect UUID/LUT collisions.
	INT32										m_update_check_delay;        ///< Additional delay for update check in seconds, used when server is not responding
    BOOL										m_user_initiated; ///< TRUE if the current check is initiated by a user. It should be cleared after the check is done.

#ifndef _MACINTOSH_
	opera_update_checker::ipc::Channel*			m_channel_to_checker; ///< IPC channel to the checker process.
	INT64										m_channel_id; ///< Id of the communication channel to the checker.
	OpString									m_checker_path; ///< The path to the checker executable.
	OpTimer*									m_checker_launch_retry_timer; ///< The checker launch retry timer.
	unsigned 									m_checker_launch_retry_count; ///< The counter of the checker launch reties.
	CheckerState								m_checker_state; ///< The current checker state.
	static const unsigned						MAX_CHECKER_LAUNCH_RETRIES = 5;
	static const unsigned						CHECKER_LAUNCH_RETRY_TIME_MS = 10000;
#endif // _MACINTOSH_
};

namespace Console
{
	/* Write message to console */
	void WriteMessage(const uni_char* message);

	/* Write error to console */
	void WriteError(const uni_char* error);
};

#endif // AUTO_UPDATE_SUPPORT

#endif // _AUTOUPDATER_H_INCLUDED_
