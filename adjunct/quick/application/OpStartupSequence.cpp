/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon / Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "adjunct/quick/application/OpStartupSequence.h"

#include "adjunct/quick/application/BrowserUpgrade.h"
#include "adjunct/quick/controller/StartupController.h"
#include "adjunct/quick/dialogs/LicenseDialog.h"
#include "adjunct/quick/dialogs/TestIniDialog.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick_toolkit/contexts/DialogContext.h"
#include "modules/content_filter/content_filter.h" 
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"
#ifdef SELFTEST
#include "modules/selftest/optestsuite.h"
#endif
#include "modules/url/uamanager/ua.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/viewers/viewers.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "modules/util/opstrlst.h"

#ifdef ENABLE_USAGE_REPORT
# include "adjunct/quick/usagereport/UsageReport.h"
#endif

#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/autoupdate/updater/auupdater.h"
#endif



#ifndef USE_COMMON_RESOURCES
extern BOOL g_first_run_after_install; // Must be declared in platform code. Set to true if this is the first run of a clean install, false otherwise.
#endif // USE_COMMON_RESOURCES


OpStartupSequence::BrowserStartSetting::BrowserStartSetting(Application::RunType run_type)
{
	m_run_type = run_type;
	m_recover_strategy = Restore_RegularStartup;
	m_sessions = NULL;
	m_default_session = NULL;
}

OpStartupSequence::BrowserStartSetting::~BrowserStartSetting()
{
	if (m_sessions)
		OP_DELETE(m_sessions);
	if (m_default_session)
		OP_DELETE(m_default_session);
}

OpStartupSequence::OpStartupSequence() :
	m_startup_sequence_state(STARTUP_SEQUENCE_LICENSE_DIALOG),
	m_browser_start_setting(NULL),
	m_runtype(Application::RUNTYPE_NOT_SET),
	m_is_browser_started(FALSE),
	m_has_crashed(FALSE),
	m_previous_version(),
	m_startup_setting(NULL)
{
	// This should not override selftests
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
		m_startup_sequence_state = STARTUP_SEQUENCE_WATIRTEST;
	else if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DialogTest))
		m_startup_sequence_state = STARTUP_SEQUENCE_TEST_DIALOG;
	else if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::IniDialogTest))
		m_startup_sequence_state = STARTUP_SEQUENCE_TEST_INI_DIALOG;
#ifdef SELFTEST
	if (g_selftest.suite->DoRun())
		m_startup_sequence_state = STARTUP_SEQUENCE_SELFTEST;
#endif // SELFTEST

	// This is the last chance to setup the Watir prefs, see DSK-348653.
	ApplicationIsAboutToStart();
}

void OpStartupSequence::ApplicationIsAboutToStart()
{
	// This is the last moment when we can set the debug proxy prefs, i.e. ProxyHost and ProxyPort.
	// According to the comment in OpScopeDefaultManager::Construct() we can only safely set the
	// values before the message loop starts.
	// Setting the values too late caused DSK-348653, the browser was trying to connect to the Watir
	// driver *before* it had the chance to update the prefs with what was passed from the command line.
	CommandLineArgument* arg = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DebugProxy);
	if (arg && arg->m_string_value.HasContent())
	{
		OpString host;
		const uni_char* port = StringUtils::SplitWord(arg->m_string_value.CStr(), ':', host);
		if (port && *port && host.HasContent())
		{
			TRAPD(err, g_pctools->WriteStringL(PrefsCollectionTools::ProxyHost, host.CStr()));
			TRAP(err, g_pctools->WriteIntegerL(PrefsCollectionTools::ProxyPort, uni_atoi(port)));

			OpStatus::Ignore(err);
		}
	}
}

OpStartupSequence::~OpStartupSequence()
{
	OP_DELETE(m_startup_setting);
}

OpStartupSequence::Listener::StartupStepResult
		OpStartupSequence::StepStartupSequence(MH_PARAM_1 par1,	MH_PARAM_2 par2)
{
	if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
		g_uaManager->AddComponent("Edition Next");
	else if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_LABS)
	{
		OpString8 labs_ua_string;

		labs_ua_string.Set(g_desktop_product->GetLabsProductName());
		labs_ua_string.Insert(0, "Edition Labs ");

		g_uaManager->AddComponent(labs_ua_string.CStr());
	}

	switch (m_startup_sequence_state)
	{
	case STARTUP_SEQUENCE_LICENSE_DIALOG:
	{
		INT32 flag = g_pcui->GetIntegerPref(PrefsCollectionUI::AcceptLicense);
		// Value for 7.50
		INT32 mask = 0x01;
		// Will be extended whenever it is required

		BOOL show_dialog = (flag & mask) ? FALSE : TRUE;

#if defined AUTO_UPDATE_SUPPORT && defined AUTOUPDATE_PACKAGE_INSTALLATION
		show_dialog = show_dialog && ! AUUpdater::WaitFileWasPresent();
#endif	// AUTO_UPDATE_SUPPORT && AUTOUPDATE_PACKAGE_INSTALLATION
		if(show_dialog && !g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate))
		{
			m_startup_sequence_state = STARTUP_SEQUENCE_DEFAULT_BROWSER_DIALOG;
			LicenseDialog * dialog = OP_NEW(LicenseDialog, ());
			if (dialog && OpStatus::IsSuccess(dialog->Init(NULL)))
				return Listener::CONTINUE;
			else
				g_desktop_global_application->Exit();
		}
		// else - fall through
	}
	case STARTUP_SEQUENCE_DEFAULT_BROWSER_DIALOG:
	{
#ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
		Application::RunType firstRun = DetermineFirstRunType();
		if (firstRun != Application::RUNTYPE_FIRSTCLEAN
			&&	static_cast<DesktopOpSystemInfo*>(g_op_system_info)->ShallWeTryToSetOperaAsDefaultBrowser()
			&&	g_pcui->GetIntegerPref(PrefsCollectionUI::ShowDefaultBrowserDialog)
			&&	g_desktop_product->GetProductType() != PRODUCT_TYPE_OPERA_LABS)
		{
			DefaultBrowserDialog* dialog = OP_NEW(DefaultBrowserDialog, ());
			if (dialog && OpStatus::IsSuccess(dialog->Init(NULL)))
			{
				m_startup_sequence_state = STARTUP_SEQUENCE_STARTUP_DIALOG;
				return Listener::CONTINUE;
			}
		}
#endif // QUICK_USE_DEFAULT_BROWSER_DIALOG
		// else - fall through
	}
	case STARTUP_SEQUENCE_STARTUP_DIALOG:
	{
		CommandLineArgument *crash_log = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog);
		// crash upload dialog will have set WindowRecoveryStrategy after a crash,skip startup dialog in that case
		if(!crash_log)
		{
			Application::RunType firstRun = DetermineFirstRunType();
			if (firstRun != Application::RUNTYPE_FIRSTCLEAN
				&&	(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowStartupDialog)
					 ||	(g_pcui->GetIntegerPref(PrefsCollectionUI::Running)
						 &&	g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProblemDlg)
						 && !CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoSession))))
			{
				StartupController* controller = OP_NEW(StartupController, ());
				if (OpStatus::IsSuccess(ShowDialog(controller, g_global_ui_context, NULL)))
				{
					m_startup_sequence_state = STARTUP_SEQUENCE_START_SETTINGS;
					return Listener::CONTINUE;
				}
			}
			else
			{
				OpSession* tempdefsession = OP_NEW(OpSession, ());
				if(tempdefsession)
				{
					TRAPD(sts, tempdefsession->InitL(NULL));
					if(OpStatus::IsError(sts))
					{
						// Mark all sessions as read-only to avoid accidental overwrites of a incomplete session
						g_session_manager->SetReadOnly(TRUE);
					}
					par2 = (MH_PARAM_2)tempdefsession;
				}
			}
		}

		// OBS: Or do we need to do the else partof StartupDialogStart??
		// No break here, fallthrough to the next case
		//  if we don't do the StartupDialog
	}
	case STARTUP_SEQUENCE_TEST_DIALOG:
		if (m_startup_sequence_state == STARTUP_SEQUENCE_TEST_DIALOG)
		{
			CommandLineArgument* testdialog = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::DialogTest);
			OpAutoPtr<TestDialogContext> controller(OP_NEW(TestDialogContext, ()));
			if (controller.get())
			{
				OpString8 dialog_name;
				if (OpStatus::IsSuccess(dialog_name.Set(testdialog->m_string_value.CStr())) &&
					OpStatus::IsSuccess(controller->SetDialog(dialog_name)))
				{
					ShowDialog(controller.release(), g_global_ui_context, NULL);
				}
			}
			// 'controller' destructor initiates shutdown of Opera
			break;
		}
		// Fallthrough
	case STARTUP_SEQUENCE_TEST_INI_DIALOG:
		if (m_startup_sequence_state == STARTUP_SEQUENCE_TEST_INI_DIALOG)
		{
			CommandLineArgument* testinidialog = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::IniDialogTest);
			OpAutoPtr<TestIniDialog> dialog(OP_NEW(TestIniDialog, ()));
			if (dialog.get() && OpStatus::IsSuccess(dialog->SetDialogName(testinidialog->m_string_value)))
				dialog.release()->Init(NULL);
			// 'dialog' destructor initiates shutdown of Opera
			break;
		}
		// Fallthrough
	case STARTUP_SEQUENCE_WATIRTEST:
		// Starting here skips all the dialogs above
		// Special options just opera watir start
		if (m_startup_sequence_state == STARTUP_SEQUENCE_WATIRTEST)
		{
			// If autoproxy isn't on then kick it in anyway
			if (!g_pctools->GetIntegerPref(PrefsCollectionTools::ProxyAutoConnect))
			{
				OP_STATUS err;
				// Alter the pref to catch restarts
				TRAP(err, g_pctools->WriteIntegerL(PrefsCollectionTools::ProxyAutoConnect, 1));

				// Disable AutoUpdate (dialog)
				if (!g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate))
					TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::DisableOperaPackageAutoUpdate, 1));

				// We'd rather want to avoid showing the crash dialog when run in autotest mode, that doesn't work well with
				// the launcher.
				TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowCrashLogUploadDlg, 0));
				OpStatus::Ignore(err);

				// Fire the message so the auto connect happens now
				g_main_message_handler->PostDelayedMessage(MSG_SCOPE_CREATE_CONNECTION, 1, 0, 1000);
			}
		}
		// Fallthrough
	case STARTUP_SEQUENCE_SELFTEST:
		// Fallthrough
	case STARTUP_SEQUENCE_START_SETTINGS:
		m_startup_sequence_state = STARTUP_SEQUENCE_NORMAL_START;
		if (g_pcui->GetIntegerPref(PrefsCollectionUI::Running))
			m_has_crashed = TRUE;

		InitBrowserStartSetting((CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoWin) != NULL),
					reinterpret_cast<OpSession*>(par2),
					reinterpret_cast<OpINT32Vector*>(par1));

		if ((DetermineFirstRunType() == Application::RUNTYPE_FIRST) && g_pcui)
		{
			OP_STATUS status = BrowserUpgrade::UpgradeFrom(m_previous_version);
			OP_ASSERT(OpStatus::IsSuccess(status));	//why did upgrading settings fail???
			OpStatus::Ignore(status);
		}

		// We must save the previous run version before we update it
		StorePreviousVersion();

		if (DetermineFirstRunType() != Application::RUNTYPE_NORMAL)
		{
			OpString vs;
			OP_STATUS err = vs.Set(VER_NUM_INT_STR);
			if (OpStatus::IsSuccess(err))
			{
				OperaVersion current_version;
				TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::MaxVersionRun,current_version.GetFullString()));
#ifdef VER_BETA
				TRAP(err, g_pcui->WriteStringL(PrefsCollectionUI::NewestUsedBetaName, VER_BETA_NAME));
#else
        //Hack. If the pref is not set, setting it to "" will NOT set it to empty, because the prefs module will
        //not consider it as achange. This ensures that it does get set to an empty value.
				TRAP(err, g_pcui->WriteStringL(PrefsCollectionUI::NewestUsedBetaName, UNI_L("temp")));
				TRAP(err, g_pcui->WriteStringL(PrefsCollectionUI::NewestUsedBetaName, UNI_L("")));
#endif
			}
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
#ifdef AUTO_UPDATE_SUPPORT
			TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::BrowserJSTime, 0));
			TRAP(err, g_prefsManager->CommitL());
#endif // AUTO_UPDATE_SUPPORT
		}

		if (DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN)
		{
			OperaVersion current_version;
			TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::FirstVersionRun,current_version.GetFullString()));
			TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::FirstRunTimestamp, g_timecache->CurrentTime()));

			SetupFirstRunOfOpera();
		}
#ifdef ENABLE_USAGE_REPORT
		if(!g_application->IsEmBrowser() && !KioskManager::GetInstance()->GetEnabled() &&
		   ((DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN) || (DetermineFirstRunType() == Application::RUNTYPE_FIRST)))
		{
			// TODO: Not necessary in final version
			TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::EnableUsageStatistics, 0));

			// Ask a specified percentage of users
			if (rand() % 100 < g_pcui->GetIntegerPref(PrefsCollectionUI::AskForUsageStatsPercentage))
			{
				OpUsageReportManager::ShowInfoDialog();
				// FIXME rfz, make dialog non blocking and uncomment next line
				//return;
			}
		}
#endif

		if (m_startup_sequence_state != STARTUP_SEQUENCE_DONE)
			g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START_CONTINUE, 0, 0);
		break;
	case STARTUP_SEQUENCE_NORMAL_START:
		m_startup_sequence_state = STARTUP_SEQUENCE_DONE;
		return Listener::DONE;
	}

	return Listener::CONTINUE;
}


Application::RunType OpStartupSequence::DetermineFirstRunType()
{
#ifdef USE_COMMON_RESOURCES
	// Clean this up and delete this function once all platforms have changed over
	m_runtype           = (Application::RunType)g_run_type->m_type;
	m_previous_version  = g_run_type->m_previous_version;
#else
	if (m_runtype == Application::RUNTYPE_NOT_SET)
	{
		OperaVersion current_version,newest_used_version;
		if (g_pcui)
		{
			if (OpStatus::IsError(newest_used_version.Set(g_pcui->GetStringPref(PrefsCollectionUI::MaxVersionRun))))
			{
				OpString old_version;
				old_version.Set(g_pcui->GetStringPref(PrefsCollectionUI::MaxVersionRun));
				m_previous_version = uni_atoi(old_version.CStr());
				m_runtype = RUNTYPE_FIRST;
				return m_runtype;
			}
		}

		// If the runtype is not already set, determine what it really is.
		if (g_first_run_after_install)
		{
			m_runtype = Application::RUNTYPE_FIRSTCLEAN; // This is set based on a global set when opera has to create a whole new profile.
		}
		else if (g_pcui && ( uni_atoi(g_pcui->GetStringPref(PrefsCollectionUI::MaxVersionRun).CStr()) < atoi(VER_NUM_INT_STR) ) )
		{
			m_runtype = Application::RUNTYPE_FIRST; // If the version is still less than the current Opera's version this must be an upgraded installation being run for the first time
		}
		else
		{
			m_runtype = Application::RUNTYPE_NORMAL; // None of the above applies, so this must be a normal run.
		}
	}
#endif // USE_COMMON_RESOURCES
	return m_runtype;
}

OP_STATUS OpStartupSequence::InitBrowserStartSetting(BOOL force_no_windows, OpSession* default_session, OpINT32Vector* sessions)
{
	m_browser_start_setting = OP_NEW(BrowserStartSetting, (DetermineFirstRunType()));

	if (!m_browser_start_setting)
		return OpStatus::ERR_NO_MEMORY;

	m_browser_start_setting->m_recover_strategy = GetRecoverStrategy(force_no_windows);
	m_browser_start_setting->m_default_session  = default_session;
	m_browser_start_setting->m_sessions         = sessions;

	return OpStatus::OK;
}

void OpStartupSequence::SetDefaultSession(OpSession* session)
{
	if (m_browser_start_setting)
		m_browser_start_setting->m_default_session = session;
}

void OpStartupSequence::DestroyBrowserStartSetting()
{
	OP_DELETE(m_browser_start_setting);
	m_browser_start_setting = NULL;
}

void OpStartupSequence::StorePreviousVersion()
{
	m_previous_version = g_run_type->m_previous_version;
}

WindowRecoveryStrategy OpStartupSequence::GetRecoverStrategy(const BOOL force_no_windows)
{
	// If we are running operawatir tests we have to make it like we never crash so that
	// sessions come back correctly in most cases
        if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest)) 
        {
	     OP_STATUS err;
	     TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::Running, 0));

	     err = g_urlfilter->AddURLString(UNI_L("*://redir.opera.com/speeddials/*"), TRUE, NULL);
	     err = g_urlfilter->AddURLString(UNI_L("*"), FALSE, NULL);
	}

	// Use start setting from recovery dialog box if the
	// previous session crashed, otherwise use the regular
	// startup setting
	WindowRecoveryStrategy strategy = Restore_RegularStartup;
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::Running))
	{
		strategy = WindowRecoveryStrategy(g_pcui->GetIntegerPref(PrefsCollectionUI::WindowRecoveryStrategy));
	}
	else
	{
		if(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType) == STARTUP_CONTINUE)
			strategy = Restore_AutoSavedWindows;
		else if(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType) == STARTUP_HISTORY)
			strategy = Restore_RegularStartup;
		else if(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType) == STARTUP_HOME)
			strategy = Restore_Homepage;
		if(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType) == STARTUP_NOWIN)
			strategy = Restore_NoWindows;
	}

	if( force_no_windows )
	{
		strategy = Restore_NoWindows;
	}
	return strategy;
}

void OpStartupSequence::SetupFirstRunOfOpera()
{
	// default settings for korean version
	const OpStringC lngcode = g_languageManager->GetLanguage();

	if (lngcode.CompareI("ko") == 0)
	{
		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::EnableHostNameWebLookup, TRUE));
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
		TRAP(err, g_pcnet->WriteStringL(PrefsCollectionNetwork::HostNameWebLookupAddress, UNI_L("http://redir.opera.com/netpia/?q=%s&lang=ko")));
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
	}

	// Commit this straight away
	TRAPD(err, g_prefsManager->CommitL());
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

#ifndef USE_COMMON_RESOURCES
# ifdef GADGET_SUPPORT
	// Search the widgets folder under resources and add all the widgets in here
	OpFolderLister* lister = NULL;
	OpString 		gadget_path, gadget_file;
	OpFile 			gadget_file_src, gadget_file_dest;

	// Grab the gadget folder path to reuse
	OpString tmp_storage;
	gadget_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_GADGET_FOLDER, tmp_storage));
	// Get all the zip files in the package folder
	lister = OpFile::GetFolderLister(OPFILE_GADGET_PACKAGE_FOLDER, UNI_L("*.zip"));
	if (lister)
	{
		while (lister && lister->Next())
		{
			// Set the destination filename
			gadget_file.Set(gadget_path.CStr());
			gadget_file.Append(lister->GetFileName());

			// Create the opfiles and copy the file over
			if (OpStatus::IsSuccess(gadget_file_src.Construct(lister->GetFullPath())) &&
				OpStatus::IsSuccess(gadget_file_dest.Construct(gadget_file.CStr())) &&
				OpStatus::IsSuccess(gadget_file_dest.CopyContents(&gadget_file_src, FALSE)))
			{
				// Add the gadget
				g_hotlist_manager->NewGadget(gadget_file, OpStringC(), NULL, FALSE, FALSE);
			}
		}

		OP_DELETE(lister);
	}
# endif // GADGET_SUPPORT
#endif // !USE_COMMON_RESOURCES
}

/***********************************************************************************
**
**	SetStatupSetting
**
***********************************************************************************/

void OpStartupSequence::SetStartupSetting( const Application::StartupSetting &setting )
{
	OP_DELETE(m_startup_setting);
	m_startup_setting = OP_NEW(Application::StartupSetting, (setting));
}

/***********************************************************************************
**
**	GetStartupSetting
**
***********************************************************************************/

BOOL OpStartupSequence::GetStartupSetting( BOOL &show, OpRect*& rect, OpWindow::State& state )
{
	if( m_startup_setting )
	{
		show = TRUE;

		if( m_startup_setting->HasGeometry() )
		{
			rect = &m_startup_setting->geometry;
		}
		if( m_startup_setting->iconic )
		{
			state = OpWindow::MINIMIZED;
			UnsetFullscreenStartupSetting();
//			m_startup_setting->fullscreen = FALSE;
		}
		else if( HasFullscreenStartupSetting() )
		{
			state = OpWindow::FULLSCREEN;
		}

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
