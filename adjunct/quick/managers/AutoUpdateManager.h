/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifdef AUTO_UPDATE_SUPPORT

#ifndef AUTOUPDATE_MANAGER_H
#define AUTOUPDATE_MANAGER_H

#include "adjunct/autoupdate/autoupdater.h"
#include "adjunct/autoupdate/additioncheckerlistener.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "adjunct/autoupdate/autoupdatexml.h"
#include "adjunct/autoupdate/autoupdateserverurl.h"
#include "adjunct/autoupdate/updatablesetting.h" 
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

/* Redefine for a more Opera like name */
#define g_autoupdate_manager	(AutoUpdateManager::GetInstance())
/*
**  Define value for no resuming timer
**  @see	AutoUpdateManager::AUM_Listener::OnError
*/
#define NO_RETRY_TIMER			-1


class AutoUpdateDialog;
class AutoUpdateCheckDialog;
class UpdateErrorResumingDialog;
class OpInputAction;
class AUFileUtils;
class AdditionCheckerListener;
class AutoUpdateXML;

/*
**  @struct	:AvailableUpdateContext
**  @brief	The information needed in UI elements to present 'Update Available' information.
*/
struct AvailableUpdateContext
{
	OpFileLength	update_size;		//< Download size of the update(s) - accumulated
	OpString		update_info_url;	//< URL to info about the to-be-installed Opera version
};

/*
**  @struct	UpdateProgressContext
**  @brief	The information needed in UI elements to present download information.
*/
struct UpdateProgressContext
{
	// Type of the update, from a UI perspective.
	enum UpdateType
	{
		BrowserUpdate,		//< package or patch
		ResourceUpdate,		//< everything else: BrowserJS, ..
		DictionaryUpdate	//< spell(ing)-checker dictionary - only downloaded together with package
	};

	BOOL			is_preparing;		//< Updates are downloaded, resources are being installed and update is prepared
	BOOL			is_ready;			//< Updates are prepared/deployed, it's ready to install

	UpdateType		type;				//< Type of update downloaded (package, ..)
	OpFileLength	total_size;			//< Total download size
	OpFileLength	downloaded_size;	//< Size of downloaded part
	double			kbps;				//< Download speed
	unsigned long	time_estimate;		//< Estimated remaining download time
	OpString		filename;			//< Path of downloaded file
};

/*
**  @struct	UpdateErrorContext
**  @brief	The information needed in UI elements to error/resuming info.
*/
struct UpdateErrorContext
{
	AutoUpdateError	error;
	INT32			seconds_until_retry;
};

namespace opera_update_checker {
namespace global_storage {
class GlobalStorage;
}
}

/***********************************************************************************
 *
 *  @class AutoUpdateManager
 *
 *	@brief Connection between AutoUpdate UI and the AutoUpdate module
 *  @author Manuela Hutter
 *
 *
 ***********************************************************************************/
class AutoUpdateManager
	: public AutoUpdater::AutoUpdateListener
	, public DesktopWindowListener
	, public DesktopManager<AutoUpdateManager>
	, public OpPrefsListener
{
public:

	/*
	**  @class	AutoUpdateManager::AvailableUpdateContext
	**  @brief	The information needed in UI elements to present 'Update Available' information.
	*/
	class AUM_Listener
	{
	public:
		virtual ~AUM_Listener(){}

		/*
		**  Called if the UI needs to present that no new update is available. This is
		**  called when no update or no package/patch update is available (so it's not
		**  exactly the same as the autoupdate-callback).
		**  @see AutoUpdater::OnUpToDate
		*/
		virtual void OnUpToDate() = 0;

		virtual void OnChecking() = 0;
		/*
		**  Called if the UI needs to present 'update is available' information.
		**  @see AutoUpdater::OnUpdateAvailable
		*/
		virtual void OnUpdateAvailable(AvailableUpdateContext* context) = 0;
		virtual void OnDownloading(UpdateProgressContext* context) = 0;
		virtual void OnDownloadingDone(UpdateProgressContext* context) = 0;
		virtual void OnDownloadingFailed() = 0;
		virtual void OnPreparing(UpdateProgressContext* context) = 0;
		virtual void OnFinishedPreparing() = 0;
		virtual void OnReadyToInstall(UpdateProgressContext* context) = 0;
		virtual void OnError(UpdateErrorContext* context) = 0;
		virtual void OnMinimizedStateChanged(BOOL minimized) = 0;
	};

	AutoUpdateManager();
	~AutoUpdateManager();
	
	// All the functions below are OPTIONAL extras if needed.
	// Initialise everything the manager needs
	OP_STATUS		Init();
	BOOL			IsInited() { return m_inited; }

	/**
	* Activates the auto-update and checks for update if necessary.
	*/
	OP_STATUS	Activate()			{ return m_autoupdater->Activate(); }

	/**
	* Forces a check for update. If user_initiated is TRUE, the update check is visible in a dialog, 
	* as is the information about an available update or no update available.
	* 
	* If user_initiated is FALSE, the check is silent, as is the information that no update is available. 
	* Available updates will be shown or downloaded automatically, depending on the user's settings.
	*/
	OP_STATUS	CheckForUpdate(BOOL user_initiated = TRUE);

	OP_STATUS	CheckForAddition(UpdatableResource::UpdatableResourceType type, const OpStringC& key);

	OP_STATUS	DownloadUpdate();
	void		DeferUpdate();
	void		CancelUpdate();
	void		RestartOpera();
	void		GoToDownloadPage();

	void		SetUpdateMinimized(BOOL minimized = TRUE);
	BOOL		IsUpdateMinimized() const { return m_minimized != AllVisible; }

	OP_STATUS	SetLevelOfAutomation(LevelOfAutomation level_of_automation);
	LevelOfAutomation	GetLevelOfAutomation();

	// Listener functions for this classes internal listener
	OP_STATUS	AddListener(AUM_Listener* listener) { return m_listeners.Add(listener); }
	OP_STATUS	AddListener(AdditionCheckerListener* listener) { OP_ASSERT(m_addition_checker); return m_addition_checker->AddListener(listener); }
	OP_STATUS	RemoveListener(AUM_Listener* listener) { return m_listeners.Remove(listener); }
	OP_STATUS	RemoveListener(AdditionCheckerListener* listener) { OP_ASSERT(m_addition_checker); return m_addition_checker->RemoveListener(listener); }

	void		RequestCallback(AUM_Listener* listener);
	
	// handing actions forwarded by Application
	BOOL		OnInputAction(OpInputAction* action);

	// AutoUpdateListener implementation
	virtual void OnUpToDate(BOOL silent);
	virtual void OnChecking(BOOL silent);
	virtual void OnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL silent);
	virtual void OnDownloading(UpdatableResource::UpdatableResourceType type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate, BOOL silent);
	virtual void OnDownloadingDone(UpdatableResource::UpdatableResourceType type, const OpString& filename, OpFileLength total_size, BOOL silent);
	virtual void OnDownloadingFailed(BOOL silent);
	virtual void OnRestartingDownload(INT32 seconds_until_restart, AutoUpdateError last_error, BOOL silent);
	virtual void OnReadyToInstallNewVersion(const OpString& version, BOOL silent);
	virtual void OnError(AutoUpdateError error, BOOL silent);
	virtual void OnReadyToUpdate();
	virtual void OnUpdating();
	virtual void OnUnpacking() {}
	virtual void OnFinishedUpdating();

	// OpPrefsListener interface
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue);

	// others
	OP_STATUS	GetVersionString(OpString& version);

	/** Returns the storage autoupdate UUID and LUT (last update timestamp) may be read/written from/to. */
	opera_update_checker::global_storage::GlobalStorage* GetUpdateStorage() { return m_storage; }

protected:
	// DesktopWindowListener implementation
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

protected:
	OpListeners<AUM_Listener>			m_listeners;

private:
	void GenerateOnUpToDate();
	void GenerateOnChecking();
	void GenerateOnUpdateAvailable(AvailableUpdateContext* context);
	void GenerateOnDownloading(UpdateProgressContext* context);
	void GenerateOnDownloadingDone(UpdateProgressContext* context);
	void GenerateOnDownloadingFailed();
	void GenerateOnPreparing(UpdateProgressContext* context);
	void GenerateOnFinishedPreparing();
	void GenerateOnReadyToInstall(UpdateProgressContext* context);
	void GenerateOnError(UpdateErrorContext* context);
	void GenerateOnMinimizedStateChanged(BOOL minimized);

	void ShowCheckForUpdateDialog();
	void ShowUpdateAvailableDialog(AvailableUpdateContext* context);
	void ShowAutoUpdateDialog(UpdateProgressContext* context);

private:
	enum InternalState
	{
		Inactive,
		Checking,
		UpdateAvailable,
		InProgress,
		Resuming,
		Ready

	} m_internal_state;


	BOOL						m_inited;			// Set TRUE once the Init() call has succeeded

	enum Visibility
	{
		AllVisible,
		AllMinimized,
		AvailableUpdateOnly

	} m_minimized;

	AutoUpdater*				m_autoupdater;		// The Auto Update object
	AdditionChecker*			m_addition_checker;
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION
	AUFileUtils*				m_file_utils;		// AUFileUitls object 
#endif // AUTOUPDATE_PACKAGE_INSTALLATION

	AvailableUpdateContext*		m_update_context;
	UpdateProgressContext*		m_progress_context;
	UpdateErrorContext*			m_error_context;

	AutoUpdateDialog*			m_update_dialog;
	AutoUpdateCheckDialog*		m_update_check_dialog;
	UpdateErrorResumingDialog*	m_error_resuming_dialog;

	OpString					m_version;
	opera_update_checker::global_storage::GlobalStorage*
								m_storage;
};


#endif // AUTOUPDATE_MANAGER_H

#endif // AUTO_UPDATE_SUPPORT
