/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_STARTUP_SEQUENCE_H
#define OP_STARTUP_SEQUENCE_H

#include "adjunct/quick/Application.h" // RunType, StartupSetting

class OpSession;

class OpStartupSequence
{
public:
	typedef DesktopBootstrap::Listener Listener;

	OpStartupSequence();
	~OpStartupSequence();

	/**
	 * Check whether this is the first run of a clean install,
	 * the first run of an upgrade to a previously installed
	 * instance, or a normal run.
	 *
	 * This function will return the same value every time it's
	 * called in a run, even if the preference that it uses is changed.
	 *
	 * @return FIRST, FIRSTCLEAN or NORMAL based on run type.
	 */
	Application::RunType DetermineFirstRunType();

	void DestroyBrowserStartSetting();

	BOOL HasBrowserStartSetting() const { return  m_browser_start_setting != 0; }

	Application::RunType GetBrowserStartSettingRunType() const { return m_browser_start_setting->m_run_type; }

	WindowRecoveryStrategy GetWindowRecoveryStrategy() const { return m_browser_start_setting->m_recover_strategy; }
	OpINT32Vector* GetSessions() const { return m_browser_start_setting->m_sessions; }
	OpSession* GetDefaultSession() const { return m_browser_start_setting->m_default_session; }
	void SetDefaultSession(OpSession* session);

	BOOL IsBrowserStarted() const { return m_is_browser_started; }

	BOOL HasCrashed() const { return m_has_crashed; }

	/**
	 * Holds the MaxVersionSeen preference before it is overwritten on startup
	 * Can be used on first runs to add special upgrade behaviour
	 *
	 * @return version eg. 950, 1001 
	 */
	OperaVersion GetPreviousVersion() { return m_previous_version; }

	/* Choose recovery strategy based on settings
	 * @param force_no_windows		If TRUE, this function will return Restore_NoWindows;
	 * @return						WindowRecoveryStrategy that matches the current preferences
	 */
	WindowRecoveryStrategy GetRecoverStrategy( const BOOL force_no_windows );

	/* Misc initialization of a new profile
	 */
	void SetBrowserStarted() { m_is_browser_started = TRUE; }

	/* Wrapper functions for the StartupSetting container. It is used to stored commandline parameters,
	 * the settings are used during Opera::Start. Appearantly only used by UNIX.
	 */
	void SetStartupSetting( const Application::StartupSetting &setting );
	BOOL GetStartupSetting( BOOL& show, OpRect*& rect, OpWindow::State& state );
	BOOL HasStartupSetting() const { return m_startup_setting != 0; }
	BOOL HasFullscreenStartupSetting() const { return m_startup_setting && m_startup_setting->fullscreen; }
	BOOL HasGeometryStartupSetting() const { return m_startup_setting && m_startup_setting->HasGeometry(); }
	void DestroyStartupSetting() { OP_DELETE(m_startup_setting); m_startup_setting = 0;}

	Listener::StartupStepResult StepStartupSequence(
			MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	enum StartupSequenceStates
	{
		STARTUP_SEQUENCE_LICENSE_DIALOG,
		STARTUP_SEQUENCE_DEFAULT_BROWSER_DIALOG,
		STARTUP_SEQUENCE_TEST_DIALOG,
		STARTUP_SEQUENCE_TEST_INI_DIALOG,
		STARTUP_SEQUENCE_STARTUP_DIALOG,
		STARTUP_SEQUENCE_START_SETTINGS,
		STARTUP_SEQUENCE_NORMAL_START,
		STARTUP_SEQUENCE_SELFTEST,
		STARTUP_SEQUENCE_WATIRTEST,
		STARTUP_SEQUENCE_DONE
	};

	struct BrowserStartSetting
	{
		BrowserStartSetting(Application::RunType run_type);

		~BrowserStartSetting();

		WindowRecoveryStrategy  m_recover_strategy;
		Application::RunType    m_run_type;
		OpINT32Vector*	        m_sessions;         ///< identifiers of selected sessions
		OpSession*              m_default_session;
	};

	OP_STATUS InitBrowserStartSetting(BOOL force_no_windows,
									  OpSession* default_session,
									  OpINT32Vector* sessions);

	void StorePreviousVersion();

	void SetupFirstRunOfOpera();

	/**
	 * Some tasks need to be done between calling g_opera->InitL() and starting the message loop.
	 * One of such tasks is setting the Watir debug proxy preferences basing on the command line input,
	 * doing that after the application is up and running poses a risk that autoconnect will happen
	 * with the prefs not set correctly.
	 * This method is called just before the platform message loop starts.
	 */
	void ApplicationIsAboutToStart();

	void UnsetFullscreenStartupSetting() { if(m_startup_setting) m_startup_setting->fullscreen = FALSE; }

	StartupSequenceStates m_startup_sequence_state;
	BrowserStartSetting* m_browser_start_setting;

	Application::RunType m_runtype;

	BOOL m_is_browser_started;
	BOOL m_has_crashed;

	OperaVersion	m_previous_version;

	Application::StartupSetting* m_startup_setting;
};



#endif // OP_STARTUP_SEQUENCE_H
