/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_BOOT_MANAGER_H
#define OP_BOOT_MANAGER_H

#include "adjunct/quick/Application.h" // RunType
#include "adjunct/quick/application/OpUrlPlayer.h"
#include "adjunct/quick/application/OpStartupSequence.h"
#include "adjunct/quick/application/DesktopOpAuthenticationListener.h"
#include "adjunct/autoupdate/country_checker.h"

#if defined(LIBSSL_AUTO_UPDATE)
# include "adjunct/autoupdate/scheduler/optaskscheduler.h" // OpScheduledTaskListener
#endif

class ClassicApplication;
class SessionAutoSaveManager;
class OpLanguageManager;
class OpAuthenticationListener;
class OpUiWindowListener;
class Dialog;
class OpSession;
class DesktopWindow;
#ifndef AUTO_UPDATE_SUPPORT
class NewUpdatesChecker;
#endif

class OpBootManager :
	public OpTimerListener
#ifdef LIBSSL_AUTO_UPDATE
	, public OpScheduledTaskListener
#endif
	, public CountryCheckerListener
{
public:

	OpBootManager(ClassicApplication& application, OpStartupSequence& startup_sequence);
	~OpBootManager();

	/*
	 * Application::StartupSetting handling
	 */

	void SetStartupSetting( const Application::StartupSetting &setting )
		{ m_startup_sequence->SetStartupSetting(setting); }

	BOOL GetStartupSetting( BOOL& show, OpRect*& rect, OpWindow::State& state )
		{ return m_startup_sequence->GetStartupSetting(show, rect, state); }

	BOOL HasStartupSetting()
		{ return m_startup_sequence->HasStartupSetting(); }

	BOOL HasGeometryStartupSetting() const
		{ return m_startup_sequence->HasGeometryStartupSetting(); }

	/*
	 * Startup state
	 */

	Application::RunType DetermineFirstRunType()
		{ return m_startup_sequence->DetermineFirstRunType(); }

	BOOL IsBrowserStarted() const
		{ return m_startup_sequence->IsBrowserStarted(); }

	BOOL HasCrashed() const
		{ return m_startup_sequence->HasCrashed(); }

	OperaVersion GetPreviousVersion()
		{ return m_startup_sequence->GetPreviousVersion(); }

	WindowRecoveryStrategy GetRecoverStrategy( const BOOL force_no_windows )
		{ return m_startup_sequence->GetRecoverStrategy(force_no_windows); }

	/*
	 * Exit state
	 */

	BOOL IsExiting() const { return m_exiting; }

	void SetIsExiting() { m_exiting = TRUE; }

	/*
	 * Default language manager is used to get the English strings in a translated version if needed
	 */

	OpLanguageManager* GetDefaultLanguageManager()
		{ return m_default_language_manager; }

	/* Set a flag in preferences (PrefsCollectionUI::Running) that Opera is running. This flag is used
	 * to determine whether Opera crashed. The Setting is directly committed to disk.
	 * @param is_running Value to be set for PrefsCollectionUI::Running
	 * @return			 OpStatus::OK on success, errors are usually caused by a inaccessible opera6.ini file
	 */
	OP_STATUS SetOperaRunning(BOOL is_running);

	/** Send request for IP-based country code if this is first run and Opera
	 * only has country code from OS (which is considered unreliable).
	 * If country check is started loading of customization files will be
	 * delayed until Opera gets the answer or request times out.
	 * This function must NOT be called after #StartBrowser.
	 *
	 * @return OpStatus::OK if request was sent successfully or was not needed
	 */
	OP_STATUS StartCountryCheckIfNeeded();

	// CountryCheckerListener
	virtual void CountryCheckFinished();

	OP_STATUS StartBrowser();

	/* Initiate shutdown sequence
	 * @return			number of objects that needs the message pump to continue to run in order to
	 * 					gracefully close, if number == 0, continue with exit, if > 0, ACTION_EXIT will
	 * 					triggered and TryExit shall be called again.
	 */
	int TryExit();

	DesktopOpAuthenticationListener* GetAuthListener() { return m_authentication_listener; }

private:
	BOOL HasFullscreenStartupSetting() const { return m_startup_sequence->HasFullscreenStartupSetting(); }
	void DestroyStartupSetting() { m_startup_sequence->DestroyStartupSetting(); }

	/* Determine whether it is time to check for a new version of Opera
	 * @return TRUE the Opera server should be queried for a new version
	 */
	BOOL IsUpgradeCheckNeeded();

#ifndef AUTO_UPDATE_SUPPORT
	void PerformUpgradesCheck(BOOL informIfNoUpdate);
#endif //! AUTO_UPDATE_SUPPORT

#ifdef LIBSSL_AUTO_UPDATE
	void InitSLLAutoUpdate();
#endif // LIBSSL_AUTO_UPDATE

	/* Open URLs that are specified through the command line, uses m_startup_setting
	 * @param commandline_urls		List of URLs that are launched from the command line, cannot be NULL
	 */
	void ExecuteCommandlineArg( OpVector<OpString>* commandline_urls);

	void ConfigureWindowCommanderManager();
	void ConfigureTranslations();

	void PostSaveSessionRequest();

	/**
	 * Recover Opera from a previous session and setting
	 */
	void RecoverOpera(WindowRecoveryStrategy strategy);
	void RecoverSession(OpSession* session_ptr);

	/** Load customization files (search engines, speed dials, bookmarks) and perform any pending
	 * initialization steps that depend on these files.
	 * This function is usually called from StartBrowser, but it may be also delayed for a few
	 * seconds if Opera needs to contact the Autoupdate server to get country code, which is
	 * used to select customization files.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS LoadCustomizations();

	/** Convert search type prefs to options in search.ini file.
	 * Should be called when upgrading from versions < 10.60 (see DSK-304850).
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR_NULL_POINTER if PrefsReader for operaprefs.ini is not initialized
	 */
	OP_STATUS UpgradeSearchTypePrefs();

	BOOL IsSDI();
	void CreateToplevelWindow(OpWindowCommander* windowCommander, OpenURLSetting & settings, UINT32 width, UINT32 height, BOOL toolbars, BOOL focus_document, BOOL open_background);

	/*
	 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked and the function
	 *								returns FALSE.
	 * @return TRUE if a url should be opened in the background
	 */
	void ShowFirstRunPage( const BOOL upgrade );

	/** Goes to Homepage if PermanentHomepage preference is enabled.
	 *
	 * @return true if GoToPage() has been called, otherwise false is returned.
	 */
	bool GoToPermanentHomepage();

#ifdef M2_MERLIN_COMPATIBILITY
	/* Reindex/Update mail if needed
	 * @return TRUE if reindexing was needed
	 */
	BOOL UpdateMail();
#endif //M2_MERLIN_COMPATIBILITY

	void InitWindow();

	void OnTimeOut(OpTimer* timer);

	SessionAutoSaveManager*	m_autosave_manager;
	OpLanguageManager* m_default_language_manager;
#ifndef AUTO_UPDATE_SUPPORT
	NewUpdatesChecker* m_updates_checker;
#endif

#if defined(LIBSSL_AUTO_UPDATE)
	// Implementing OpScheduledTaskListener interface
	virtual void OnTaskTimeOut(OpScheduledTask* task);
	OpScheduledTask m_ssl_autoupdate_task;
#endif // LIBSSL_AUTO_UPDATE

	BOOL m_exiting;

	OpUrlPlayer m_url_player;

	OpStartupSequence* m_startup_sequence;

	OpUiWindowListener* m_ui_window_listener;
	DesktopOpAuthenticationListener* m_authentication_listener;

	ClassicApplication* m_application;

	// Ref. DSK-293741
	OpTimer* m_fullscreen_timer;

	CountryChecker* m_country_checker;             //< Performs request for IP-based country code
	static const int COUNTRY_CHECK_TIMEOUT = 7000; //< Timeout for country code request in milliseconds

	/**
	 * State of country check.
	 * These values are stored as PrefsCollectionUI::CountryCheck so that Opera can detect
	 * that country check was not finished in previous session (browser was closed or crashed)
	 * and has to be restarted.
	 */
	enum CountryCheckState
	{
		CCS_DONE,  //< Finished or not started
		CCS_IN_PROGRESS_FIRST_RUN,  //< Started when run type was RUNTYPE_FIRST
		CCS_IN_PROGRESS_FIRST_CLEAN_RUN //< Started when run type was RUNTYPE_FIRSTCLEAN
	};

	CountryCheckState m_previous_ccs; //< State of country check at the end of previous session

	OpTimer* m_delay_customizations_timer;         //< Timer for additional delay when loading customization files (for testing)
};

#endif // OP_BOOT_MANAGER_H
