/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon / Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "adjunct/quick/application/OpBootManager.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/prefs/CorePrefsHelpers.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"
#include "modules/prefsfile/prefsfile.h"
#include "adjunct/quick/ClassicApplication.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/dialogs/PasswordDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/windows/PanelDesktopWindow.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/ManagerHolder.h"
#include "adjunct/m2_ui/dialogs/MailStoreUpdateDialog.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/quick/dialogs/CheckForUpgradeDialogs.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "adjunct/desktop_util/resources/ResourceSetup.h"
#include "adjunct/desktop_util/resources/ResourceUtils.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "modules/util/OpSharedPtr.h"

#ifdef SELFTEST
#include "modules/selftest/optestsuite.h"
#endif // SELFTEST

#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/quick/managers/AutoUpdateManager.h"
#endif // AUTO_UPDATE_SUPPORT

#ifdef ENABLE_USAGE_REPORT
# include "adjunct/quick/usagereport/UsageReport.h"
#endif // ENABLE_USAGE_REPORT

#ifdef LIBSSL_AUTO_UPDATE
#define LIBSSL_AUTO_UPDATE_TIMEOUT 604800 // 1 week
#define LIBSSL_AUTO_UPDATE_TASKNAME "SSL Autoupdate"
#endif // LIBSSL_AUTO_UPDATE

#ifdef _DDE_SUPPORT_
# include "platforms/windows/userdde.h" // DDEHandler
extern DDEHandler* ddeHandler;
#endif // _DDE_SUPPORT_

#ifdef _MACINTOSH_
#include "platforms/mac/File/FileUtils_Mac.h"
#include "modules/skin/OpSkinManager.h"
#endif

#define FULLSCREEN_TIMEOUT 100
#define OPERA_NEXT_PAGE UNI_L("http://redir.opera.com/www.opera.com/portal/next")
#define OPERA_UPGRADE_PAGE UNI_L("http://redir.opera.com/www.opera.com/upgrade/")
#define OPERA_FIRST_RUN_PAGE UNI_L("http://redir.opera.com/www.opera.com/firstrun/")

OpBootManager::OpBootManager(ClassicApplication& application,
		OpStartupSequence& startup_sequence)
	: m_autosave_manager(NULL)
	, m_default_language_manager(NULL)
#ifndef AUTO_UPDATE_SUPPORT
	, m_updates_checker(NULL)
#endif
	, m_exiting(FALSE)
	, m_startup_sequence(&startup_sequence)
	, m_ui_window_listener(NULL)
	, m_authentication_listener(NULL)
	, m_application(&application)
	, m_fullscreen_timer(NULL)
	, m_country_checker(NULL)
	, m_previous_ccs(CCS_DONE)
	, m_delay_customizations_timer(NULL)
{
	OP_PROFILE_METHOD("Constructed boot manager");

	m_autosave_manager = OP_NEW(SessionAutoSaveManager, ());
	if (!m_autosave_manager)
		return;

	CorePrefsHelpers::ApplyMemoryManagerPrefs();

	ConfigureWindowCommanderManager();

	ConfigureTranslations();
}

OpBootManager::~OpBootManager()
{
	g_windowCommanderManager->SetGadgetListener(NULL);

	g_windowCommanderManager->SetAuthenticationListener(NULL);
	OP_DELETE(m_authentication_listener);

	g_windowCommanderManager->SetUiWindowListener(NULL);
	OP_DELETE(m_ui_window_listener);

#ifndef AUTO_UPDATE_SUPPORT
	OP_DELETE(m_updates_checker);
#endif

	OP_DELETE(m_autosave_manager);

	// must be deleted after closing windows because of possible callbacks
	OP_DELETE(m_default_language_manager);

	OP_DELETE(m_fullscreen_timer);

	OP_DELETE(m_country_checker);
	OP_DELETE(m_delay_customizations_timer);
}

void OpBootManager::ConfigureWindowCommanderManager()
{
	m_ui_window_listener = m_application->CreateUiWindowListener();
	g_windowCommanderManager->SetUiWindowListener(m_ui_window_listener);

	m_authentication_listener = m_application->CreateAuthenticationListener();
	g_windowCommanderManager->SetAuthenticationListener(m_authentication_listener);
}

void OpBootManager::ConfigureTranslations()
{
	OpString lang;
	
	lang.Set(g_languageManager->GetLanguage());

	if(!lang.Compare(UNI_L("en")))
	{
		// no need to load a duplicate of the language we already use
		return;
	}
	if ((m_default_language_manager = OP_NEW(OpPrefsFileLanguageManager, ())) != NULL)
	{
		OpFile default_lngfile;
		OpString filename;

		filename.Set(g_desktop_op_system_info->GetLanguageFolder(UNI_L("en")).CStr());
		filename.Append(UNI_L(PATHSEP));
		filename.Append(DESKTOP_RES_DEFAULT_LANGUAGE);

		OP_STATUS err = default_lngfile.Construct(filename, OPFILE_LOCALE_FOLDER);
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

		PrefsFile* language_file = OP_NEW_L(PrefsFile, (PREFS_LNG));
		TRAP(err, language_file->ConstructL());
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
		TRAP(err, language_file->SetFileL(&default_lngfile));
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

		TRAP(err, ((OpPrefsFileLanguageManager*)m_default_language_manager)->LoadTranslationL(language_file));
		language_file = NULL; // Taken over by LM

		if (OpStatus::IsError(err))
		{
			// Smells like a memory leak
			m_default_language_manager = NULL;
		}
	}
}

void OpBootManager::PostSaveSessionRequest()
{
	m_autosave_manager->PostSaveRequest();
}

BOOL OpBootManager::IsSDI()
{
	return g_pcui->GetIntegerPref(PrefsCollectionUI::SDI) == 1;
}

#ifdef LIBSSL_AUTO_UPDATE
void OpBootManager::InitSLLAutoUpdate()
{
	m_ssl_autoupdate_task.SetListener(this);
	m_ssl_autoupdate_task.InitTask(LIBSSL_AUTO_UPDATE_TASKNAME);
	m_ssl_autoupdate_task.AddScheduledTask(LIBSSL_AUTO_UPDATE_TIMEOUT);
}
#endif // LIBSSL_AUTO_UPDATE

void OpBootManager::InitWindow()
{
	// Ensure we have a WINDOW_TYPE_BROWSER or WINDOW_TYPE_DOCUMENT window to process messages
	DesktopWindow* activewindow = g_application->GetActiveDesktopWindow();
	if( !activewindow || (activewindow->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER && activewindow->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT) )
	{
		// We will have a non-active window if we start iconified
		if( !g_application->GetDesktopWindowCollection().GetDesktopWindowByType(OpTypedObject::WINDOW_TYPE_BROWSER) )
		{
			g_application->GetBrowserDesktopWindow();
		}
	}

	// for SDI and tabbed, make sure there is no empty workspaces
	OpVector<DesktopWindow> browser_windows;
	g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, browser_windows);
	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace))
	{
		for(UINT32 i = 0; i < browser_windows.GetCount(); i++ )
		{
#ifdef DEVTOOLS_INTEGRATED_WINDOW
			if (!browser_windows.Get(i)->IsDevToolsOnly())
#endif // DEVTOOLS_INTEGRATED_WINDOW
			{
				OpWorkspace* workspace = browser_windows.Get(i)->GetWorkspace();

				if (!workspace->GetActiveDesktopWindow())
				{
					g_application->CreateDocumentDesktopWindow(workspace);
				}
			}
		}
	}
}

OP_STATUS OpBootManager::StartCountryCheckIfNeeded()
{
	// this function must be called before StartBrowser
	OP_ASSERT(!m_startup_sequence->IsBrowserStarted());

	int previous_ccs = g_pcui->GetIntegerPref(PrefsCollectionUI::CountryCheck);
	if (previous_ccs == CCS_IN_PROGRESS_FIRST_CLEAN_RUN || previous_ccs == CCS_IN_PROGRESS_FIRST_RUN)
	{
		// Opera decided in previous session that country check is needed, but
		// it was not finished nor timed out (session was closed or crashed).
		// Country check will be restarted in this session (unless this is
		// selftests or Watir run).
		// Save previous state as it contains information about original run type
		// which will be needed to execute pending upgrades.
		m_previous_ccs = static_cast<CountryCheckState>(previous_ccs);
	}
	else
	{
		// Start country check only if current region setting is based
		// on country code reported by OS (which we don't trust) and if
		// this is is first run with fresh profile or an upgrade from
		// version of Opera without region-based customizations.
		// Otherwise just return OK - customization files will be loaded
		// using current region setting.

		if (!g_region_info->m_from_os)
			return OpStatus::OK;

		Application::RunType runtype = DetermineFirstRunType();
		if (runtype != Application::RUNTYPE_FIRSTCLEAN)
		{
			if (runtype != Application::RUNTYPE_FIRST)
				return OpStatus::OK;
			OperaVersion elevensixty;
			RETURN_IF_ERROR(elevensixty.Set(UNI_L("11.60.01")));
			if (GetPreviousVersion() >= elevensixty)
				return OpStatus::OK;
		}
	}

	// Disable country check if Opera is running selftests or Watir to ensure
	// stable environment during tests. If you want to run tests with delayed
	// customizations use -delayedcustomizations <sec> command line option.
#ifdef SELFTEST
	if (g_selftest.suite->DoRun())
		return OpStatus::OK;
#endif // SELFTEST
	if (g_commandline_manager->GetArgument(CommandLineManager::WatirTest))
		return OpStatus::OK;

	OpAutoPtr<CountryChecker> country_checker(OP_NEW(CountryChecker, ()));
	RETURN_OOM_IF_NULL(country_checker.get());
	RETURN_IF_ERROR(country_checker->Init(this));
	RETURN_IF_ERROR(country_checker->CheckCountryCode(COUNTRY_CHECK_TIMEOUT));

	if (m_previous_ccs == CCS_DONE)
	{
		CountryCheckState ccs;
		if (DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN)
			ccs = CCS_IN_PROGRESS_FIRST_CLEAN_RUN;
		else
			ccs = CCS_IN_PROGRESS_FIRST_RUN;
		RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::CountryCheck, static_cast<int>(ccs)));
		RETURN_IF_LEAVE(g_prefsManager->CommitL());
	}

	m_country_checker = country_checker.release();

	OP_PROFILE_MSG("Country check started");

	return OpStatus::OK;
}

void OpBootManager::CountryCheckFinished()
{
	OP_PROFILE_MSG("Country check finished");

	if (IsExiting())
		return; // too late, but CountryCheck pref is set, so country check will be restarted in the next session

	if (m_country_checker->GetStatus() == CountryChecker::CheckSucceded)
	{
		OpStringC new_cc = m_country_checker->GetCountryCode();
		OP_ASSERT(new_cc.HasContent()); // should not be empty if country check succeeded

		OpStringC au_cc = g_pcui->GetStringPref(PrefsCollectionUI::AuCountryCode);
		if (new_cc != au_cc)
		{
			// This will trigger update of path to turbosettings.xml file (in AutoUpdateManager::PrefChanged)
			TRAPD(status, g_pcui->WriteStringL(PrefsCollectionUI::AuCountryCode, new_cc));
		}

		if (OpStatus::IsSuccess(ResourceUtils::UpdateRegionInfo(new_cc)) &&
			g_region_info->m_changed)
		{
			OpStatus::Ignore(FileUtils::UpdateDesktopLocaleFolders(true));
		}
	}

	// no longer needed
	OP_DELETE(m_country_checker);
	m_country_checker = NULL;

	// If already started then load customizations. Otherwise LoadCustomizations will be called from StartBrowser.
	if (m_startup_sequence->IsBrowserStarted())
	{
		OpStatus::Ignore(LoadCustomizations());
	}
}

/***********************************************************************************
 **
 **	StartBrowser
 **
 **
 **
 ***********************************************************************************/

OP_STATUS OpBootManager::StartBrowser()
{
	OP_ASSERT(m_startup_sequence->HasBrowserStartSetting());	// Can only start the browser once, check with IsBrowserStarted();
	if (!m_startup_sequence->HasBrowserStartSetting())
		return OpStatus::ERR;

	if (!m_country_checker)
		RETURN_IF_ERROR(LoadCustomizations());

	if (DetermineFirstRunType() != Application::RUNTYPE_NORMAL)
	{

#ifdef WEBSERVER_SUPPORT
		if (g_webserver_manager && g_webserver_manager->IsFeatureAllowed())
		{
			// install the root service on first run, ignore errors
			OpStatus::Ignore(g_desktop_gadget_manager->InstallRootService());

#ifdef USE_COMMON_RESOURCES
			ResourceSetup::InstallUniteServices();	// install services from custom builds
#endif // USE_COMMON_RESOURCES
			g_webserver_manager->LoadDefaultServices();	// install our regular services
		}
#endif // WEBSERVER_SUPPORT
	}

	// This call MUST be before any DesktopWindow instances are opened to allow listeners to be established
	g_desktop_global_application->OnStart();

	// See DSK-340253.
	if (DetermineFirstRunType() == Application::RUNTYPE_FIRST)
	{
		OperaVersion elevenfifty;
		RETURN_IF_ERROR(elevenfifty.Set(UNI_L("11.50.01")));
		if (GetPreviousVersion() < elevenfifty)
		{
 			PrivacyManager::Flags flags;
 			flags.Set(g_pcui->GetIntegerPref(PrefsCollectionUI::ClearPrivateDataDialog_CheckFlags));

			bool wand_sync = flags.IsSet(PrivacyManager::WAND_PASSWORDS_SYNC);
			bool wand_dont_sync = flags.IsSet(PrivacyManager::WAND_PASSWORDS_DONT_SYNC);
			bool bookmarks_vis_time = flags.IsSet(PrivacyManager::BOOKMARK_VISITED_TIME);
			bool all_windows = flags.IsSet(PrivacyManager::ALL_WINDOWS);
			bool certs = flags.IsSet(PrivacyManager::CERTIFICATES);
			bool searchfield = flags.IsSet(PrivacyManager::SEARCHFIELD_HISTORY);

			flags.Set(PrivacyManager::BOOKMARK_VISITED_TIME, wand_dont_sync);
			flags.Set(PrivacyManager::ALL_WINDOWS, bookmarks_vis_time);
			flags.Set(PrivacyManager::CERTIFICATES, all_windows);
			flags.Set(PrivacyManager::SEARCHFIELD_HISTORY, certs);
			flags.Set(PrivacyManager::WEBSTORAGE_DATA, searchfield);
			flags.Set(PrivacyManager::WAND_PASSWORDS_DONT_SYNC, wand_sync);

 			TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::ClearPrivateDataDialog_CheckFlags, flags.Get()));
 			TRAP(rc, g_prefsManager->CommitL());
		}
	}

/*//FIXME DUNNO if this really is needed... huibk
// make sure we have a BROWSER window or WINDOW_TYPE_DOCUMENT to process messages
	BOOL isSDIandCommandlineURLs = IsSDI() && (commandline_urls && commandline_urls->GetCount() > 0);
	DesktopWindow* activewindow = GetActiveDesktopWindow();
	if (!isSDIandCommandlineURLs && (!activewindow || (activewindow->GetType() != WINDOW_TYPE_BROWSER && activewindow->GetType() != WINDOW_TYPE_DOCUMENT)))
	{
#ifdef _MACINTOSH_
			GetBrowserDesktopWindow(TRUE, FALSE, TRUE);
#else
			GetBrowserDesktopWindow(TRUE, FALSE, IsSDI());
#endif // _MACINTOSH_
	}*/

#ifdef _MACINTOSH_

	// Possibly make a link to the new preferences folder location, if this is not an AppStore build (see DSK-334964)
	if (VER_NUM_MAJOR < 12 && !g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate))
		OpFileUtils::LinkOldPreferencesFolder();

    // Color Schemes no longer supported on Mac, except through Personas. Otherwise turn off.
    if (!g_skin_manager->HasPersonaSkin())
    {
        g_pccore->WriteIntegerL(PrefsCollectionCore::ColorSchemeMode, 0);
        g_skin_manager->SetColorSchemeMode(OpSkin::COLOR_SCHEME_MODE_NONE);
    }

#endif // _MACINTOSH_

	if (m_startup_sequence->GetBrowserStartSettingRunType() == Application::RUNTYPE_FIRSTCLEAN)
	{
		// Open a welcome to opera page
		ShowFirstRunPage(FALSE);
	}
	else
	{
		// Recover from a previous session
		OP_PROFILE_METHOD("Loaded and initialized previous session");

		WindowRecoveryStrategy strategy = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoSession) ? 
			Restore_NoWindows : m_startup_sequence->GetWindowRecoveryStrategy();
		RecoverOpera(strategy);
	}
	if (m_startup_sequence->GetBrowserStartSettingRunType() == Application::RUNTYPE_FIRST)
	{
		// Open a thank you for upgrading page
		ShowFirstRunPage(TRUE);
	}

	// open crash feedback page
	OpStringC url = g_pcui->GetStringPref(PrefsCollectionUI::CrashFeedbackPage);
	if (url.HasContent())
	{ 
		g_application->OpenURL(url,MAYBE,YES,NO);
		TRAPD(err,g_pcui->WriteStringL(PrefsCollectionUI::CrashFeedbackPage,UNI_L("")));
	}

	m_startup_sequence->SetBrowserStarted();

	m_startup_sequence->DestroyBrowserStartSetting();

 	InitWindow();

#ifdef M2_MERLIN_COMPATIBILITY
	UpdateMail();
#endif // M2_MERLIN_COMPATIBILITY

#ifdef _DDE_SUPPORT_
	if(ddeHandler) // In StartupSeq?
	{
		ddeHandler->Start();		//start accepting requests
	}
#endif // _DDE_SUPPORT_

	if(!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoWin))
	{
		// ok to send in an empty collection here
		ExecuteCommandlineArg(CommandLineManager::GetInstance()->GetUrls());
	}

	OpStatus::Ignore(SetOperaRunning(TRUE));

	if( (HasFullscreenStartupSetting())
		|| CommandLineManager::GetInstance()->GetArgument(CommandLineManager::MediaCenter)
        || CommandLineManager::GetInstance()->GetArgument(CommandLineManager::Fullscreen)
		||(KioskManager::GetInstance()->GetEnabled() && !KioskManager::GetInstance()->GetKioskNormalScreen()) )
		{
			KioskManager::GetInstance()->SetEnableFilter(FALSE);
			BOOL handled = g_input_manager->InvokeAction(OpInputAction::ACTION_ENTER_FULLSCREEN);
			if (handled)
			{
				g_desktop_global_application->OnStarted();
			}
			else
			{
				m_fullscreen_timer = OP_NEW(OpTimer, ());
				m_fullscreen_timer->SetTimerListener(this);
				m_fullscreen_timer->Start(FULLSCREEN_TIMEOUT);
			}
			KioskManager::GetInstance()->SetEnableFilter(TRUE);
		}

	if(KioskManager::GetInstance()->GetEnabled())
		g_application->PrepareKioskMode();

	// This setting is read in CreateSessionWindows() but should
	// be available on startup.
	DestroyStartupSetting();

#ifdef LIBSSL_AUTO_UPDATE
	InitSLLAutoUpdate();
#endif // LIBSSL_AUTO_UPDATE

#ifdef AUTO_UPDATE_SUPPORT
	g_autoupdate_manager->Activate();
#else
	if (IsUpgradeCheckNeeded())
	{
		PerformUpgradesCheck(FALSE);
	}
#endif //!AUTO_UPDATE_SUPPORT

//	g_secman_instance->InitializeTables(); // Needed by the SecurityInformationDialog to check if a url is in the intranet range.

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	const OpStringC filter = g_pcnet->GetStringPref(PrefsCollectionNetwork::NeverFlushTrustedServers);
	if (filter.IsEmpty())
    {
		OpString trusted;
		trusted.Set("help.opera.com");
		TRAPD(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::NeverFlushTrustedServers, trusted));
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
	}
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT

	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::StartMail))
		g_application->GoToMailView(IndexTypes::UNREAD_UI);

	m_url_player.Play();

	// When running watir tests always load opera:debug
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
	{
		DocumentDesktopWindow *ddw = g_application->GetActiveDocumentDesktopWindow();
		if (ddw)
		{
			if (!ddw->HasDocumentInHistory())
			{
				// DSK-361642: Make sure we ignore the modifier keys when opening opera:debug
				g_application->GoToPage(UNI_L("opera:debug"), FALSE, FALSE, FALSE, 0, static_cast<URL_CONTEXT_ID>(-1), TRUE);
			}
		}
	}

	// If customizations are already loaded then we should start Account Manager,
	// otherwise it will be started in LoadCustomizations.
	if (!m_country_checker && !m_delay_customizations_timer)
		RETURN_IF_ERROR(g_desktop_account_manager->Start());

	return OpStatus::OK;	//FIXME don't check any status actually
}

// Usually called from StartBrowser, i.e. before UI is displayed, but may be also called
// some time after start sequence completes if Opera needs to contact autoupdate server
// to get country code, which is then used to initialize paths to customization files.
// Aborts only on OOMs. Step that fail with other error codes are just ignored (there is
// not much that can be done esp. if this function is called when UI is already displayed).
OP_STATUS OpBootManager::LoadCustomizations()
{
	// this function must not be called after customizations are loaded
	OP_ASSERT(!g_searchEngineManager->HasLoadedConfig());
	OP_ASSERT(!g_speeddial_manager->HasLoadedConfig());
	OP_ASSERT(!g_desktop_bookmark_manager->GetBookmarkModel()->Loaded());

	if (m_delay_customizations_timer == NULL)
	{
		// Apply additional delay if requested. This is only used to test browser's
		// behaviour without customizations.
		CommandLineArgument* delay_arg = g_commandline_manager->GetArgument(CommandLineManager::DelayCustomizations);
		if (delay_arg && delay_arg->m_int_value > 0)
		{
			m_delay_customizations_timer = OP_NEW(OpTimer, ());
			if (m_delay_customizations_timer)
			{
				m_delay_customizations_timer->SetTimerListener(this);
				m_delay_customizations_timer->Start(delay_arg->m_int_value * 1000);
				return OpStatus::OK;
			}
		}
	}
	else
	{
		OP_DELETE(m_delay_customizations_timer);
		m_delay_customizations_timer = NULL;
	}

	OP_PROFILE_METHOD("Customization files loaded");

	Application::RunType run_type;

	// If Opera restarted country check that did not finish in previous session then
	// we need previous session's run type to trigger pending upgrades.
	if (m_previous_ccs == CCS_IN_PROGRESS_FIRST_CLEAN_RUN)
		run_type = Application::RUNTYPE_FIRSTCLEAN;
	else if (m_previous_ccs == CCS_IN_PROGRESS_FIRST_RUN)
		run_type = Application::RUNTYPE_FIRST;
	else
		run_type = DetermineFirstRunType();

	if (m_previous_ccs == CCS_IN_PROGRESS_FIRST_CLEAN_RUN || m_previous_ccs == CCS_IN_PROGRESS_FIRST_RUN)
	{
		// This should trigger pending upgrades in functions that themselves check
		// current run type (e.g. DesktopBookmarkManager::UpgradeDefaultBookmarks).
		// Usually they also check this flag.
		g_region_info->m_changed = true;
	}

	TRAPD(err, g_searchEngineManager->LoadSearchesL());
	RETURN_IF_MEMORY_ERROR(err);

	// Need to check if the pref was downloaded on the last run
	BOOL tld_downloaded = g_pcui->GetIntegerPref(PrefsCollectionUI::GoogleTLDDownloaded) ? TRUE : FALSE;

	// Update the google TLD on updates
	if (!tld_downloaded ||
		run_type == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER ||
		run_type == Application::RUNTYPE_FIRST ||
		run_type == Application::RUNTYPE_FIRSTCLEAN)
	{
		// First reset the pref so a new one is retrieved on upgrade/installation
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::GoogleTLDDownloaded, 0));
		RETURN_IF_MEMORY_ERROR(err);

		// Now get the new TLD google server
		RETURN_IF_MEMORY_ERROR(g_searchEngineManager->UpdateGoogleTLD());
	}

	// Convert old SearchType prefs when upgrading from version < 10.60 (DSK-304850)
	if (run_type == Application::RUNTYPE_FIRST)
	{
		OperaVersion tensixty;
		RETURN_IF_ERROR(tensixty.Set(UNI_L("10.60.01")));
		if (GetPreviousVersion() < tensixty)
		{
			RETURN_IF_MEMORY_ERROR(UpgradeSearchTypePrefs());
		}
	}

	if (m_startup_sequence->IsBrowserStarted())
	{
		// If already started then there can be some search widgets with empty
		// search GUIDs - let them know that search engines are loaded
		g_application->SettingsChanged(SETTINGS_SEARCH);
		// Trigger update of search buttons in toolbars and panels (DSK-359163)
		g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
		// Update search engines in all open document windows (DSK-362425)
		SearchTemplate* default_search = g_searchEngineManager->GetDefaultSearch();
		if (default_search && default_search->GetUniqueGUID().HasContent())
		{
			OpVector<DesktopWindow> browser_windows;
			g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_DOCUMENT, browser_windows);
			for (UINT32 i = 0; i < browser_windows.GetCount(); ++i)
			{
				OpBrowserView* browser_view = browser_windows.Get(i)->GetBrowserView();
				if (browser_view && browser_view->GetSearchGUID().IsEmpty())
				{
					browser_view->SetSearchGUID(default_search->GetUniqueGUID());
				}
			}
		}
	}

	g_desktop_extensions_manager->StartAutoStartServices();

	RETURN_IF_MEMORY_ERROR(g_speeddial_manager->Load(run_type == Application::RUNTYPE_FIRSTCLEAN));

	if (run_type == Application::RUNTYPE_FIRST)
	{
		OperaVersion elevenfifty;
		RETURN_IF_ERROR(elevenfifty.Set(UNI_L("11.50.01")));
		if (GetPreviousVersion() < elevenfifty)
		{
			/* We can't do it in OpStartupSequence::UpgradeBrowserSettings like the rest of
			 * update stuff as it has to be done after SpeedDialManager::Load is called.
			 */
			OpVector<OpGadget> speed_dial_extensions;
			if (OpStatus::IsSuccess(g_desktop_extensions_manager->GetAllSpeedDialExtensions(speed_dial_extensions)))
			{
				for (unsigned i = 0, count = speed_dial_extensions.GetCount(); i < count; ++i)
				{
					OpGadget* extension = speed_dial_extensions.Get(i);
					if (g_speeddial_manager->FindSpeedDialByWuid(extension->GetIdentifier()) < 0)
					{
						OpStatus::Ignore(g_desktop_extensions_manager->AddSpeedDial(*extension));
					}
				}
			}
		}
	}

	if (g_desktop_bookmark_manager)
		RETURN_IF_MEMORY_ERROR(g_desktop_bookmark_manager->Load(run_type == Application::RUNTYPE_FIRSTCLEAN));

	if (g_favicon_manager)
		RETURN_IF_MEMORY_ERROR(g_favicon_manager->InitSpecialIcons());

	// Setup path to regional web turbo settings if this is the first run. It will be updated automatically when Opera
	// receives country code from the Autoupdate server.
	if (run_type == Application::RUNTYPE_FIRSTCLEAN)
	{
		RETURN_IF_MEMORY_ERROR(FileUtils::SetTurboSettingsPath());
	}

#ifdef ENABLE_USAGE_REPORT
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUsageStatistics))
	{
		g_usage_report_manager = OP_NEW(OpUsageReportManager, (UNI_L("usagereport/report.xml"), OPFILE_HOME_FOLDER));
		RETURN_OOM_IF_NULL(g_usage_report_manager);
	}
#endif // ENABLE_USAGE_REPORT

	if (run_type == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER ||
		run_type == Application::RUNTYPE_FIRST ||
		run_type == Application::RUNTYPE_FIRSTCLEAN)
	{
		RETURN_IF_MEMORY_ERROR(g_desktop_extensions_manager->InstallCustomExtensions());
	}

	TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::CountryCheck, static_cast<int>(CCS_DONE)));
	RETURN_IF_MEMORY_ERROR(err);

	// Account Manager may trigger Master Password Dialog, so it should be initialized after
	// Opera displays browser window. If this function was delayed then browser window is
	// already displayed. Otherwise Account Manager will be started in StartBrowser.
	if (m_startup_sequence->IsBrowserStarted())
		RETURN_IF_MEMORY_ERROR(g_desktop_account_manager->Start());

	return OpStatus::OK;
}

OP_STATUS OpBootManager::UpgradeSearchTypePrefs()
{
	PrefsFile *reader = const_cast<PrefsFile *>(g_prefsManager->GetReader());
	RETURN_VALUE_IF_NULL(reader, OpStatus::ERR_NULL_POINTER);
	RETURN_IF_LEAVE(reader->LoadAllL());
	
	bool write_search = false;	// do we need to write the searches?

	// Convert the Search Type prefs from the old format to the new
	// First we need to determine if the DefaultSearchType and/or DefaultSpeeddialSearchType pref are located in the ini file.
	if(reader->IsKey("User Prefs", "Speed Dial Search Type"))
	{
		OpString search_value;
		RETURN_IF_LEAVE(reader->ReadStringL("User Prefs", "Speed Dial Search Type", search_value));
		if(search_value.HasContent() && g_searchEngineManager->GetByUniqueGUID(search_value))
		{
			SearchTemplate *search = g_searchEngineManager->GetDefaultSpeedDialSearch();
			if(search && search_value.Compare(search->GetUniqueGUID()))
			{
				// the search is different, update it in the search engine manager
				RETURN_IF_ERROR(g_searchEngineManager->SetDefaultSpeedDialSearch(search_value));
				write_search = true;
			}
		}
	}
	if(reader->IsKey("User Prefs", "Search Type"))
	{
		OpString search_value;
		RETURN_IF_LEAVE(reader->ReadStringL("User Prefs", "Search Type", search_value));
		if(search_value.HasContent() && g_searchEngineManager->GetByUniqueGUID(search_value))
		{
			SearchTemplate *search = g_searchEngineManager->GetDefaultSearch();
			if(search && search_value.Compare(search->GetUniqueGUID()))
			{
				// the search is different, update it in the search engine manager
				RETURN_IF_ERROR(g_searchEngineManager->SetDefaultSearch(search_value));
				write_search = true;
			}
		}
	}
	if(write_search)
	{
		RETURN_IF_MEMORY_ERROR(g_searchEngineManager->Write());
	}
	return OpStatus::OK;
}


/***********************************************************************************
**
**	ShowFirstRunPage
**
***********************************************************************************/

inline void OpBootManager::ShowFirstRunPage( const BOOL upgrade )
{
	// When running watir tests always load opera:debug
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
	{
		// DSK-361642: Make sure we ignore the modifier keys when opening opera:debug
		g_application->GoToPage(UNI_L("opera:debug"), FALSE, FALSE, FALSE, 0, static_cast<URL_CONTEXT_ID>(-1), TRUE);
	}
	else
	{
		if (GoToPermanentHomepage())
			return;
		if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
		{
			// Fix for DSK-348094 - opera.com/portal/next page not shown on Next upgrades
			g_application->GoToPage(OPERA_NEXT_PAGE, TRUE);
		}
		else if (upgrade)
		{
			// this is to show you a nice thank-you-for-upgrading page
			g_application->GoToPage(OPERA_UPGRADE_PAGE, TRUE);
		}
		else
		{
			// this is redirected to /startup/, but counting installs
			g_application->GoToPage(OPERA_FIRST_RUN_PAGE);
		}

	}
}

bool OpBootManager::GoToPermanentHomepage()
{
#ifdef PERMANENT_HOMEPAGE_SUPPORT
	if (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage))
	{
			g_application->GoToPage(g_pccore->GetStringPref(PrefsCollectionCore::HomeURL));
			return true;
	}
#endif //PERMANENT_HOMEPAGE_SUPPORT
	return false;
}

/***********************************************************************************
**
**	SetOperaRunning
**
***********************************************************************************/

OP_STATUS OpBootManager::SetOperaRunning(BOOL is_running)
{
	OP_STATUS err = OpStatus::ERR;
	if (g_prefsManager)
	{
		// always reset the save folder to the download directory on startup
		// TRAP(err, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, g_folder_manager->GetFolderPath(OPFILE_DOWNLOAD_FOLDER) ));

		time_t current_time = g_timecache->CurrentTime();
		if (is_running)
		{
			TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupTimestamp, current_time));
		}
		else
		{
			TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::TotalUptime, \
						g_pcui->GetIntegerPref(PrefsCollectionUI::TotalUptime) + \
						current_time - g_pcui->GetIntegerPref(PrefsCollectionUI::StartupTimestamp)); \
					  g_pcui->ResetIntegerL(PrefsCollectionUI::StartupTimestamp));
		}
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::Running, is_running); \
				  g_prefsManager->CommitL());
		OP_ASSERT(OpStatus::IsSuccess(err)); // Not possible to commit opera6.ini?
	}
	return err;
}

#ifdef M2_MERLIN_COMPATIBILITY
/***********************************************************************************
**
**	UpdateMail
**
***********************************************************************************/

inline BOOL OpBootManager::UpdateMail()
{
	if (g_m2_engine && StoreUpdater::NeedsUpdate())
	{
		DesktopWindow* desktop = g_application->GetDesktopWindowCollection().GetDesktopWindowByType(OpTypedObject::WINDOW_TYPE_BROWSER);

		// Ask user if they want to update now
		if (SimpleDialog::ShowDialog(WINDOW_NAME_ASK_UPDATE_MAIL, desktop, Str::D_MAIL_STORE_UPDATE_ASK_TEXT,
					   Str::D_MAIL_STORE_UPDATE_TITLE, Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION)
				  == Dialog::DIALOG_RESULT_YES)
		{
			MailStoreUpdateDialog* update_dialog = OP_NEW(MailStoreUpdateDialog, ());
			if (update_dialog)
			{
				update_dialog->Init(desktop);
				return TRUE;
			}
		}
	}

	return FALSE;
}
#endif // M2_MERLIN_COMPATIBILITY

/***********************************************************************************
 **
 **	IsUpdateCheckNeeded
 **
 ***********************************************************************************/

BOOL OpBootManager::IsUpgradeCheckNeeded()
{
	return ( (!g_application->IsEmBrowser()) &&
			(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode)) &&
			(g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera) > 0) &&
			((time_t)g_pcui->GetIntegerPref(PrefsCollectionUI::CheckForNewOpera) < g_timecache->CurrentTime()) );
}

/***********************************************************************************
**
**	PerformUpgradesCheck
**
***********************************************************************************/
#ifndef AUTO_UPDATE_SUPPORT
void OpBootManager::PerformUpgradesCheck(BOOL informIfNoUpdate)
{
	if (m_updates_checker)
	{
		// A checker already exists.
		if (m_updates_checker->CanSafelyBeDeleted())
		{
			// The check is done and the checker can be deleted so that a new checker can be instantiated.
			OP_DELETE(m_updates_checker);
			m_updates_checker = NULL;
		}
		else
		{
			// The check is still ongoing and the checker is busy. No need to interrupt, just let this check
			// run to completion.
			return;
		}
	}
	// After cleaning up, we are ready to make a new checker and start it.
	if ((m_updates_checker = OP_NEW(NewUpdatesChecker, (informIfNoUpdate))) != NULL) // TODO: Make sure that GetActiveDesktopWindow doesn't require any initalization that is not done at this stage.
		m_updates_checker->PerformNewUpdatesCheck();
	// We don't delete the checker here, because it needs to exist when the callbacks from the
	// download comes in. It can be destroyed later, when it has finished its work.
	// If there is no appropriate place for its deletion before the destruction of Application,
	// then it will just be wasted space. (The alternative is to have it delete itself, and the risk that
	// that involves seems like a bigger drawback than a few wasted bytes.)
	// Beware: You must check that the instance is done before you delete it, call
	// BOOL NewUpdatesChecker::CanSafelyBeDeleted() to check.
}
#endif //! AUTO_UPDATE_SUPPORT

#if defined(LIBSSL_AUTO_UPDATE)
void OpBootManager::OnTaskTimeOut(OpScheduledTask* task)
{
	if(task == &m_ssl_autoupdate_task)
	{
		// Check for SSL autoupdates (Certificates and EV)
		g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
	}
}
#endif // LIBSSL_AUTO_UPDATE

/***********************************************************************************
**
**	OpenCommandlineURLs
**
***********************************************************************************/

void OpBootManager::ExecuteCommandlineArg(OpVector<OpString>* commandline_urls)
{
	if(commandline_urls && commandline_urls->GetCount())
	{
		OpenURLSetting setting;
		if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewWindow))
		{
			setting.m_new_window = YES;
		}
		else if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewTab))
		{
			setting.m_new_page = YES;
		}
		else if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::BackgroundTab))
		{
			setting.m_new_page = YES;
			setting.m_in_background = YES;
		}
		
		if (setting.m_new_window == NO && setting.m_new_page == NO && !CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ActiveTab))
		{
			setting.m_new_page = YES;
		}
		
		if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewPrivateTab))
		{
			setting.m_is_privacy_mode = TRUE;
		}

		for(unsigned int i=0; i < commandline_urls->GetCount(); i++)
		{
			if (!g_desktop_gadget_manager->OpenIfGadgetFolder(*(commandline_urls->Get(i)), TRUE))
			{
				// Make sure a at least one main window is running, so URL's can be launched in there
				BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow();
				if (!browser)
					browser = g_application->GetBrowserDesktopWindow(FALSE, FALSE, TRUE);	//don't force new window, not in background, but force new page

				if (i > 0)
				{
					setting.m_new_window = NO;
					setting.m_new_page = YES;
				}
				setting.m_address.Set( commandline_urls->Get(i)->CStr() );
				g_application->OpenURL( setting );
			}
		}
	}

	else if(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewPrivateTab))
	{
		// Make sure a at least one main window is running, so URL's can be launched in there
		BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow();
		if (!browser)
		{
			browser = g_application->GetBrowserDesktopWindow(FALSE, FALSE, TRUE);	//don't force new window, not in background, but force new page
		}

		if (browser)
		{
			OpenURLSetting setting;
			setting.m_is_privacy_mode = TRUE;
			setting.m_force_new_tab = TRUE;
			setting.m_new_page = YES;
			g_application->OpenURL( setting );
		}
	}
	
	else if(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NewTab))
	{
		// Make sure a at least one main window is running, so URL's can be launched in there
		BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow();
		if (!browser)
		{
			browser = g_application->GetBrowserDesktopWindow(FALSE, FALSE, TRUE);	//don't force new window, not in background, but force new page
		}
		
		if(browser)
		{
			OpenURLSetting setting;
			setting.m_force_new_tab = TRUE;
			setting.m_new_page		= MAYBE;
			g_application->OpenURL( setting );
		}
	}
}

/***********************************************************************************
**
**	RecoverOpera
**
***********************************************************************************/

inline void OpBootManager::RecoverOpera(WindowRecoveryStrategy strategy)
{
	OP_ASSERT(m_startup_sequence);

	if (Restore_AutoSavedWindows == strategy)
	{
		RecoverSession(g_session_manager->ReadSession(UNI_L("autosave")));
	}
	else if (Restore_RegularStartup == strategy)
	{
		OpINT32Vector* sessions = m_startup_sequence->GetSessions();
		OpSession* default_session = m_startup_sequence->GetDefaultSession();

		if (sessions)
		{
			for (unsigned i = 0; i < sessions->GetCount(); ++i)
			{
				OpSession* session = g_session_manager->ReadSession(sessions->Get(i));
				RecoverSession(session);
			}
		}
		else if (default_session)
		{
			// in this case we want to take over default_session object
			m_startup_sequence->SetDefaultSession(NULL);
			RecoverSession(default_session);
		}
	}
	else if (Restore_Homepage == strategy)
	{
		g_application->GoToPage(g_pccore->GetStringPref(PrefsCollectionCore::HomeURL));
	}
	else if (Restore_NoWindows == strategy && !IsSDI())
	{
		DocumentDesktopWindow* page = NULL;
#ifdef _MACINTOSH_
		g_application->GetBrowserDesktopWindow(TRUE, FALSE, TRUE, &page);
#else
		g_application->GetBrowserDesktopWindow(TRUE, FALSE, !g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace), &page);
#endif // _MACINTOSH_
	}
#ifdef WEBSERVER_SUPPORT
	if (g_webserver_manager->IsFeatureEnabled())
	{
		OP_PROFILE_METHOD("Auto started services");
		// Try and start them now in case the webserver is already running
		g_desktop_gadget_manager->StartAutoStartServices();
	}
#endif // WEBSERVER_SUPPORT
}

void OpBootManager::RecoverSession(OpSession* session_ptr)
{
	OP_PROFILE_METHOD("Created session windows");

	OpSharedPtr<OpSession> session(session_ptr);

	OP_STATUS err = m_application->CreateSessionWindows(session);

	if (OpStatus::IsError(err))
	{
		// don't allow overwriting the session if this failed
		g_session_manager->SetReadOnly(TRUE);
	}
}

int OpBootManager::TryExit()
{
	return 0;
}

void OpBootManager::OnTimeOut(OpTimer* timer)
{
	if (timer == m_fullscreen_timer)
	{
		KioskManager::GetInstance()->SetEnableFilter(FALSE);
		BOOL handled = g_input_manager->InvokeAction(OpInputAction::ACTION_ENTER_FULLSCREEN);
		KioskManager::GetInstance()->SetEnableFilter(TRUE);

		if (handled)
		{
			OP_DELETE(timer);
			m_fullscreen_timer = NULL;
			g_desktop_global_application->OnStarted();
		}
		else
		{
			timer->Start(FULLSCREEN_TIMEOUT);
		}
	}
	else if (timer == m_delay_customizations_timer)
	{
		OP_ASSERT(m_country_checker == NULL);
		LoadCustomizations();
	}
	else
	{
		OP_ASSERT(!"Unknown timer");
	}
}
