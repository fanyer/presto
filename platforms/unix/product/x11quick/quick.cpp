/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "modules/display/color.h"
#include "modules/display/styl_man.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/libvega/src/oppainter/vegamdefont.h"
#include "modules/mdefont/mdefont.h"
#include "modules/pi/OpThreadTools.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/url/protocols/commcleaner.h"

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/desktop_util/resources/ResourceFolders.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/CrashLoggingDialog.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/widgetruntime/GadgetStartup.h"

#include "platforms/crashlog/crashlog.h" // InstallCrashSignalHandler()
#include "platforms/crashlog/gpu_info.h"
#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/posix/posix_logger.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/quix/skin/UnixWidgetPainter.h"
#include "platforms/quix/toolkits/ToolkitLoader.h"
#include "platforms/utilix/x11_all.h"
#include "platforms/unix/base/common/lirc.h"
#include "platforms/unix/base/common/sound/oss_audioplayer.h"
#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/common/unix_gadgetutils.h"
#include "platforms/unix/base/common/unix_opsysteminfo.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_ipcmanager.h"
#include "platforms/unix/base/x11/x11_nativedialog.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11prefhandler.h"
#include "platforms/unix/base/x11/x11utils.h"

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

#ifndef POSSIBLY_UNUSED
// I'm hoping this will be added to core some day (e.g. hardcore/base/deprecate.h)...
#if defined(__GNUC__)
#define POSSIBLY_UNUSED(var) var __attribute__((unused))
#else
#define POSSIBLY_UNUSED(var) var
#endif
#endif

#ifdef CRASHLOG_CRASHLOGGER
extern GpuInfo* g_gpu_info;
GpuInfo the_gpu_info;
#endif

// Some flags set from command line used throughout UNIX code
StartupSettings g_startup_settings;
BOOL g_log_plugin = FALSE;
// Forced DPI value as set by user in opera6.ini
INT32 g_forced_dpi = 0;
// Change to  g_plugin_wrapper_path
OpString8 g_motif_wrapper_path;
// Global for the memtools module
const char * g_executable;
/* Jump point for X11 error handling */
jmp_buf g_x11_error_recover;

// Indicate that crash dialog is active
BOOL g_is_crashdialog_active = FALSE;

#if defined(CDK_MODE_SUPPORT)
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
CDKSettings g_cdk_settings;
#endif

#ifdef SELFTEST
# include "modules/selftest/optestsuite.h"
#endif

/**
 * Set up signal handlers
 */
static void InitSignalHandling();

/**
 * Handle graceful exit
 */
static void ExitSignal(int sig);

/**
 * Handle X11 IO error
 */
static int X11IOErrorHandler(X11Types::Display* dpy);

/**
 * Makes sure a SUDO user to not overwrite the 
 * normal ~/.opera directory
 */
static void ReassignSudoHOME();

/**
 * Starts or stops POSIX logging
 */
static void StartPosixLogging();
static void StopPosixLogging();
static void DestroyPosixLogging();

/**
 * Activates an existing Opera instance. Returns
 * TRUE if opera should terminate after this call.
 */
static BOOL ScanForOperaInstance();

/**
 * Returns the path and filename of the opera executable
 */
static OP_STATUS GetExecutablePath(const OpStringC8& candidate, OpString8& full_path);

/**
 * Sets up wm class name and icon for the active opera mode 
 */
static OP_STATUS PrepareWMClassInfo(X11Globals::WMClassInfo& info);


#ifdef CRASHLOG_CRASHLOGGER
class RecoveryParameters
{
public:
	RecoveryParameters(): m_count(0), m_data(0), m_restart_index(-1) {}
	~RecoveryParameters() { FreeData(); }

	void FreeData()
	{
		if (m_data)
		{
			for(int i = 0; i < m_count; i++)
				op_free(m_data[i]);
			OP_DELETEA(m_data);
			m_data = 0;
			m_count = 0;
			m_restart_index = -1;
		}
	}

	void SetRestartFlag(BOOL set)
	{
		if (m_restart_index != -1)
		{
			char* text = op_strdup(set ? "continue" : "exit");
			if (text)
			{
				op_free(m_data[m_restart_index]);
				m_data[m_restart_index] = text;
			}
		}
	}

	void SetData(const char* executable_name, const OpVector<OpString8> & restart_arguments)
	{
		FreeData();

		m_count = restart_arguments.GetCount() + 8;
		m_data = OP_NEWA(char*, m_count);
		if (!m_data)
			return;

		int i = 0;
		m_data[i++] = op_strdup(executable_name);
		m_data[i++] = op_strdup("-crashlog");
		char buf[32];
		int POSSIBLY_UNUSED(bufused);
		bufused = snprintf(buf, sizeof(buf), "%d", getpid());
		OP_ASSERT(bufused < sizeof(buf));
		m_data[i++] = op_strdup(buf);
		m_data[i++] = op_strdup("-crashaction");
		m_restart_index = i;
		m_data[i++] = op_strdup("continue");
		m_data[i++] = op_strdup("-crashlog-gpuinfo");
		OP_ASSERT(sizeof(UINTPTR) <= sizeof(unsigned long));
		bufused = snprintf(buf, sizeof(buf), "%#lx", static_cast<unsigned long>(reinterpret_cast<UINTPTR>(g_gpu_info)));
		OP_ASSERT(bufused < sizeof(buf));
		m_data[i++] = op_strdup(buf);

		for (UINT32 j = 0; j < restart_arguments.GetCount(); j++)
			m_data[i++] = op_strdup(restart_arguments.Get(j)->CStr());

		m_data[i++] = 0;
	}

	char** data() const { return m_data; }

private:
	int m_count;
	char** m_data;
	int m_restart_index;
};

RecoveryParameters recovery_parameters;

#endif // CRASHLOG_CRASHLOGGER

// Global function. Avoid if-defs elsewhere in code
void SetCrashlogRestart(BOOL state)
{
#ifdef CRASHLOG_CRASHLOGGER
	recovery_parameters.SetRestartFlag(state);
#endif
}


int main_contentL(int argc, char **argv)
{
#ifdef POSIX_CAP_LOCALE_INIT
	PosixModule::InitLocale();
#else
	setlocale(LC_CTYPE, "");
#endif

	g_executable = argv[0]; // TODO - Get rid of this
	GetExecutablePath(argv[0], g_startup_settings.our_binary);

	// Prevent a SUDO version use a regular user's ~/.opera directory 
	ReassignSudoHOME();

	// Get startup settings from environment variables (will be overriden by the command line)
	OpStatus::Ignore(g_startup_settings.share_dir.Set(op_getenv("OPERA_DIR")));
	OpStatus::Ignore(g_startup_settings.binary_dir.Set(op_getenv("OPERA_BINARYDIR")));
	OpStatus::Ignore(g_startup_settings.personal_dir.Set(op_getenv("OPERA_PERSONALDIR")));
	OpStatus::Ignore(g_startup_settings.temp_dir.Set(op_getenv("TMPDIR")));

	// Do argument processing
	if (OpStatus::IsError(CommandLineManager::GetInstance()->Init()))
		return 1;

	if (OpStatus::IsError(CommandLineManager::GetInstance()->ParseArguments(argc, argv)))
		return 0;

	// Show command line help 
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::Version))
	{
		X11Globals::ShowOperaVersion(FALSE, FALSE, 0);
		return 0;
	}

	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
	{
		CommandLineArgument* cmd_arg = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory);
		// OPERA_PERSONALDIR is ignored in WatirTest mode, so if personal_dir was not set from -pd, we disregard it
		if (!cmd_arg)
			g_startup_settings.personal_dir.Empty(); 
	}

	OP_PROFILE_INIT(); // Call OP_PROFILE_EXIT() on each return or at end

#ifdef CRASHLOG_CRASHLOGGER
	// Must be set before calling recovery_parameters.SetData()
	g_gpu_info = &the_gpu_info;
	// Set restart arguments for crash recovery
	recovery_parameters.SetData(g_startup_settings.our_binary, CommandLineManager::GetInstance()->GetRestartArguments());
	g_crash_recovery_parameters = recovery_parameters.data();
	// And disable HW acceleration if we are currently doing crash recovery
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog))
		g_desktop_bootstrap->DisableHWAcceleration();
#endif // CRASHLOG_CRASHLOGGER

	// Save profile directory (the personal directory) to $OPERA_HOME if set on the command line
	// or by $OPERA_PERSONALDIR. The resource code will later read $OPERA_HOME or use a fallback 
	// (~/.opera) if it has not been set.
	OpString opera_home_path;
	CommandLineArgument* cmd_arg = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory);
	if( cmd_arg && cmd_arg->m_string_value.HasContent() )
	{
		opera_home_path.SetL(cmd_arg->m_string_value);
	}
	else
	{
		opera_home_path.SetL(op_getenv("OPERA_PERSONALDIR"));
	}
	opera_home_path.Strip();
	if (opera_home_path.HasContent())
	{
		char tmp[_MAX_PATH+1];
		if (OpStatus::IsError(PosixFileUtil::FullPath(opera_home_path.CStr(), tmp)) ||
			OpStatus::IsError(PosixNativeUtil::FromNative(tmp, &opera_home_path)))
		{
			PosixNativeUtil::NativeString path(opera_home_path.CStr());
			printf("opera: Failed to set up profile (personal) directory : %s\n", path.get() ? path.get() : "<NULL>");
			OP_PROFILE_EXIT();
			return 1;
		}

		g_env.Set("OPERA_HOME", opera_home_path.CStr());
	}
	
	if (OpStatus::IsError(X11Globals::Create()))
	{
		OP_PROFILE_EXIT();
		return -1;
	}

	// Catch signals
	InitSignalHandling();

	{
		OP_PROFILE_METHOD("Preferences pre-setup");

		if (OpStatus::IsError(g_desktop_bootstrap->Init()))
		{
			OP_PROFILE_EXIT();
			return -1;
		}
	}

	// Change permission for personal folder (~/.opera) to 0700 (accessible only for owner
	// The directory will be created from g_desktop_bootstrap->Init() above if it does not
	// already exists.
	if (!access(g_startup_settings.personal_dir.CStr(), F_OK))
		chmod(g_startup_settings.personal_dir.CStr(), 0700);

    // Directories specified through the environment rather than on the command line need to be stored for correct restarting
    if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ShareDirectory) && op_getenv("OPERA_DIR"))
		RETURN_VALUE_IF_ERROR(CommandLineManager::GetInstance()->AddRestartArgument(CommandLineManager::ShareDirectory, g_startup_settings.share_dir.CStr()), -1);
    if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::BinaryDirectory) && op_getenv("OPERA_BINARYDIR"))
		RETURN_VALUE_IF_ERROR(CommandLineManager::GetInstance()->AddRestartArgument(CommandLineManager::BinaryDirectory, g_startup_settings.binary_dir.CStr()), -1);
    if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory) && op_getenv("OPERA_PERSONALDIR"))
		RETURN_VALUE_IF_ERROR(CommandLineManager::GetInstance()->AddRestartArgument(CommandLineManager::PersonalDirectory, g_startup_settings.personal_dir.CStr()), -1);

	// Set environment variables for prefsfile substitutions and child processes
	g_env.Set("OPERA_DIR", g_startup_settings.share_dir.CStr());
	g_env.Set("OPERA_BINARYDIR", g_startup_settings.binary_dir.CStr());
	g_env.Set("OPERA_PERSONALDIR", g_startup_settings.personal_dir.CStr());
	g_env.Set("OPERA_HOME", g_startup_settings.personal_dir.CStr()); // For compatibility

	// Create and/or clean up temp dir
	OpString temp_dir;
	RETURN_VALUE_IF_ERROR(PosixNativeUtil::FromNative(g_startup_settings.temp_dir, &temp_dir), -1);
	RETURN_VALUE_IF_ERROR(UnixUtils::CleanUpFolder(temp_dir), -1);

#if defined(_DEBUG)
	// Make life simpler for developers #1: Set toolkit automatially using the personal dir path
	// Examples: -pd <dir>/x11 -pd kde 
	{
		struct
		{
			const char* name;
			int toolkit;
		} probe[]=
		{
			{"gtk/", 2},
			{"kde/", 3},
			{"x11/", 4},
			{0, 0}
		};
		OpString8 tmp;
		tmp.Set(g_startup_settings.personal_dir);
		int length = tmp.Length();
		for (int i=0; probe[i].name; i++)
		{
			int l = strlen(probe[i].name);
			if (l <= length)
			{
				int pos = tmp.Find(probe[i].name, length-l);
				if (pos != KNotFound)
				{
					PrefsFile* pf = g_desktop_bootstrap->GetInitInfo()->prefs_reader;
					pf->WriteIntL("File Selector", "Dialog Toolkit", probe[i].toolkit);
					break;
				}
			}
		}
	}
# if defined(__linux__)
	if (!g_startup_settings.do_grab)
	{
		// Make life simpler for developers #2: Automatically set -nograb if gdb is controlling the process
		OpString path;
		OpFile file;
		if (OpStatus::IsSuccess(path.AppendFormat(UNI_L("/proc/%d/cmdline"), getppid())) &&
			OpStatus::IsSuccess(file.Construct(path)) &&
			OpStatus::IsSuccess(file.Open(OPFILE_READ)))
		{
			char val;
			OpString8 buf;
			OpFileLength num_read;
			// We are only interested in the first argument.  Each argument in the command line is zero-terminated,
			// so read until the first 0 byte.
			while (OpStatus::IsSuccess(file.Read(&val, 1, &num_read)) && num_read > 0 && val)
			{
				if (val == '/')
					buf.Empty();
				else
					buf.Append(&val, 1);
			}
			if (!buf.Compare("gdb") )
			{
				X11Widget::DisableGrab(true);
				printf("opera: '-nograb' automatically applied. Use '-dograb' to override\n");
			}
		}
	}
# endif // linux
#endif

	// Test for lock file
	OpString lock_file_name;
	ResourceFolders::GetFolder(OPFILE_USERPREFS_FOLDER, g_desktop_bootstrap->GetInitInfo(), lock_file_name);
	lock_file_name.AppendL(UNI_L("lock"));
	BOOL found_locked_file = UnixUtils::LockFile(lock_file_name, 0) == 0;

	if (found_locked_file)
	{
		// Look for a running instance. If found, we will activate it and exit 
		if (ScanForOperaInstance())
		{
			OP_PROFILE_EXIT();
			return 0;
		}

		// Ask user what to do. There might be a process that is about to exit at this
		// stage, so perhaps we want to test for that and give user some information

		OpString8 tmp;
		tmp.Set(lock_file_name);
		OpString8 message;
		message.AppendFormat(
			"It appears another Opera instance is using the same configuration directory because its "
			"lock file is active:\n\n%s\n\nDo you want "
			"to start Opera anyway?", tmp.CStr());

		X11NativeDialog::DialogResult res = X11NativeDialog::ShowTwoButtonDialog("Opera", message.CStr(), "Yes", "No");
		if (res == X11NativeDialog::REP_NO)
		{
			OP_PROFILE_EXIT();
			return 0;
		}
	}

	PrefsFile* pf = g_desktop_bootstrap->GetInitInfo()->prefs_reader;
	/* This value is used in UnixDefaultStyle::GetSystemFont(), which is called 
	   before the UNIX prefs are read. */
	g_forced_dpi = pf->ReadIntL("User Prefs", "Force DPI", 0);

	// Bug #DSK-249843 [espen 2009-03-26]
	// Common resources code will set OPFILE_LANGUAGE_FOLDER to the language 
	// prefix directory (ie. "en") and parent folder to the locale directory that
	// contains all the prefix directories. 
	// If "Language Files Directory" contains a string, then the prefs code that
	// reads it assumes it contains the full path to the correct prefix directory
	// (including the prefix directory) and fill OPFILE_LANGUAGE_FOLDER with it
	// overriding common resources.
	// In OpPrefsFileLanguageManager::LoadL() this path is the fallback if the 
	// "Language File" string has not been set (only set from Prefs dialog)
	//
	// Older versions of opera sat "Language Files Directory" to point to 
	// the locale directory (not including the prefix). That setting combined
	// with common resources do not match, and we do not really need it. Opera will
	// exit if the fallback is wrong and "Language File" is not set so we 
	// reset the "Language Files Directory" here and let common resources code decide.	
	pf->WriteStringL("User Prefs", "Language Files Directory", UNI_L(""));


	// Preferences loaded. Now we test if we are in a crash recovery mode and do not want to use it.
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog))
	{
		BOOL show_dialog = pf->ReadIntL("User Prefs", "Show Crash Log Upload Dialog", TRUE);
		if (!show_dialog)
		{
			OP_PROFILE_EXIT();
			return 0;
		}
	}

	{
		OP_PROFILE_METHOD("Font initialization");
		if (OpStatus::IsError(MDF_InitFont()))
		{
			fprintf(stderr, "opera: Failed to set up fonts.\n");
			return 0;
		}
	}
	
	g_desktop_bootstrap->GetInitInfo()->prefs_reader = pf;

	{
		OP_PROFILE_MSG("Initializing core starting");
		OP_PROFILE_METHOD("Init core completed");

		TRAPD(core_err, g_opera->InitL(*g_desktop_bootstrap->GetInitInfo()));
		if (OpStatus::IsError(core_err))
		{
			fprintf(stderr, "opera: Failed to set up core.\n");
			OP_PROFILE_EXIT();
			return -1;
		}
	}

	// Initialize the message loop (must happen after initializing core)
	if (OpStatus::IsError(X11OpMessageLoop::Self()->Init()))
	{
		fprintf(stderr, "opera: Failed to set up the message loop.\n");
		OP_PROFILE_EXIT();
		return -1;
	}

	// Let skin manager erase to toolkit base color instead of default color (0xFFC0C0C0)
	UINT32 bg = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_BACKGROUND);
	UINT32 c = 0xFF000000 | (OP_GET_R_VALUE(bg) << 16) | (OP_GET_G_VALUE(bg) << 8) | OP_GET_B_VALUE(bg);
	g_skin_manager->SetTransparentBackgroundColor(c);

	// Prepare window class. Should be available for all sort of windows
	X11Globals::WMClassInfo info;
	if (OpStatus::IsError(PrepareWMClassInfo(info)) || OpStatus::IsError(g_x11->SetWMClassInfo(info)))
	{
		OP_PROFILE_EXIT();
		return -1;
	}


	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::FullVersion))
	{
		X11Globals::ShowOperaVersion(TRUE, FALSE, 0);
		OP_PROFILE_EXIT();
		return 0;
	}

	// Some window managers will provide startup feedback.
	g_x11->NotifyStartupBegin(g_x11->GetDefaultScreen());

	// Setup widget painter, has to be done after startup because quix starts before skin
	// Note: The widget painter manager takes ownership (see QuixModule::Destroy())
	if (g_unix_widget_painter)
		g_widgetpaintermanager->SetPrimaryWidgetPainter(g_unix_widget_painter);

	// Start logging in posix code
	StartPosixLogging();

#ifdef SELFTEST
	// Note: Might modify argc and argv
	SELFTEST_MAIN( &argc, argv );
#endif

	UpdateX11FromPrefs();

	if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoLIRC))
		Lirc::Create();

	g_audio_player = OP_NEW(OssAudioPlayer, ());
	if (g_audio_player)
	{
		g_audio_player->SetAutoCloseDevice(true);
	}

	// This creates the Application object
	if (OpStatus::IsError(g_desktop_bootstrap->Boot()))
	{
		fprintf(stderr, "opera: Failed to set up the application.\n");
		OP_PROFILE_EXIT();
		return -1;
	}

#ifdef CRASHLOG_CRASHLOGGER
	// Show crash dialog we we got a crashlog option on startup
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog))
	{
		BOOL restart = FALSE;
		BOOL minimal_restart = FALSE;
		OpStringC filename(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CrashLog)->m_string_value);

		g_is_crashdialog_active = TRUE;
		CrashLoggingDialog* dialog = new CrashLoggingDialog(filename, restart, minimal_restart);
		if (dialog)
		{
			dialog->Init(0);
		}
		if (!restart)
		{
			g_desktop_bootstrap->ShutDown();
			return 0;
		}
		if (!minimal_restart)
		{
			/* This depends on -crashlog causing hardware acceleration
			 * to be disabled, as well as the crashlog dialog setting
			 * up the rest of a minimal restart.  In that case, we can
			 * just continue here in order to have our minimal
			 * restart.
			 *
			 * On the other hand, if we don't want a minimal restart,
			 * we'll just shut down this copy of opera and start a new
			 * one without the -crashlog arguments.  That should give
			 * us a "normal" startup, which is surely exactly what we
			 * want.
			 */
			g_desktop_bootstrap->ShutDown();

			// Skip the crashlog arguments for the restart.
			OP_ASSERT(op_strcmp(g_crash_recovery_parameters[1], "-crashlog") == 0);
			OP_ASSERT(op_strcmp(g_crash_recovery_parameters[3], "-crashaction") == 0);
			int drop = 4;
			if (op_strcmp(g_crash_recovery_parameters[5], "-crashlog-gpuinfo") == 0)
				drop += 2;
			g_crash_recovery_parameters[drop] = g_crash_recovery_parameters[0];
			execv(g_crash_recovery_parameters[0], g_crash_recovery_parameters + drop);
			return 0;
		}

		/* We have shown the dialog. Now enable crash logging */

		InstallCrashSignalHandler();
	}
#endif // CRASHLOG_CRASHLOGGER
	g_is_crashdialog_active = FALSE;


	Application::StartupSetting setting;
	CommandLineManager* clm = CommandLineManager::GetInstance();
	setting.open_in_background = clm->GetArgument(CommandLineManager::BackgroundTab) != 0;
	setting.open_in_new_page   = clm->GetArgument(CommandLineManager::NewTab) != 0;
	setting.open_in_new_window = clm->GetArgument(CommandLineManager::NewWindow) != 0;
	setting.fullscreen         = clm->GetArgument(CommandLineManager::Fullscreen) != 0;
	setting.iconic             = FALSE;
	CommandLineArgument* arg = clm->GetArgument(CommandLineManager::Geometry);
	if (arg)
	{
		OpString8 tmp;
		tmp.Set(arg->m_string_value);
		setting.geometry = X11Utils::ParseGeometry(tmp);
	}
	g_application->SetStartupSetting(setting);


	g_main_message_handler->PostMessage(MSG_QUICK_APPLICATION_START, 0, 0);

	CommCleaner *cleaner = OP_NEW(CommCleaner, ());
	if( !cleaner )
	{
		fputs("opera: Could not allocate comm. cleaner.\n", stderr);
		OP_PROFILE_EXIT();
		return -1;
	}
	cleaner->ConstructL();

	if (setjmp(g_x11_error_recover) == 0)
	{
		XSetIOErrorHandler(X11IOErrorHandler);
		X11OpMessageLoop::Self()->MainLoop();
	}

	// Stop logging in posix code
	StopPosixLogging();

	OP_DELETE(cleaner);
	cleaner = 0;

	g_desktop_bootstrap->ShutDown();

	Lirc::Destroy();

	{
		OP_PROFILE_MSG("Destroying core starting");
		OP_PROFILE_METHOD("Destroying core completed");

		g_opera->Destroy();
	}

	MDF_ShutdownFont();

	OP_DELETE(g_audio_player);
	g_audio_player = 0;

	// Destroy loggers
	DestroyPosixLogging();

	X11Globals::Destroy();
	ToolkitLoader::Destroy();

	// Remove temp dir
	RETURN_VALUE_IF_ERROR(UnixUtils::CleanUpFolder(temp_dir), -1);

	OP_PROFILE_EXIT();

	return 0;
}

static void ExitSignal(int sig)
{
	// Only do this once
	static BOOL been_here = FALSE;

	if (been_here || (g_application && g_application->IsExiting()) )
		return;

	been_here = TRUE;

	if (!g_thread_tools)
	{
		exit(0);
		return;
	}

	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EXIT));
	if (!action) return;
	action->SetActionData(1);
		g_thread_tools->PostMessageToMainThread(MSG_QUICK_DELAYED_ACTION, reinterpret_cast<MH_PARAM_1>(action), 0);
}


static void HandleSIGCHLD(int sig)
{
	X11OpMessageLoop::ReceivedSIGCHLD();
}


static void InitSignalHandling()
{
	BOOL failed = FALSE;

	/* Setting signal(SIGCHLD, SIG_IGN) is a special operation that makes sure
	 * child processes get reaped (by the system) when they die, avoiding
	 * zombie processes and the need for us to clean them up.
	 *
	 * Unfortunately, Some libraries install their own SIGCHLD
	 * handlers.  In particular, Qt needs to have its SIGCHLD handler
	 * triggered or it will hang when spawning subprocesses.  Which
	 * happens when KDE starts up its messaging subsystem (DSK-295272,
	 * DSK-370690).  Qt will chain-call the previously installed
	 * SIGCHLD handler, but Qt will not reap any zombies except for
	 * its own.
	 *
	 * So we must deal with SIGCHLD ourselves.
	 */
	/* TODO: We should do all signal handling through the posix
	 * module.  Global state sucks.
	 */
	if (signal(SIGCHLD, HandleSIGCHLD) == SIG_ERR) failed = TRUE;
	if (signal(SIGHUP, SIG_IGN) == SIG_ERR) failed = TRUE; // DSK-344521
	if (signal(SIGINT, ExitSignal) == SIG_ERR) failed = TRUE;
	if (signal(SIGTERM, ExitSignal) == SIG_ERR) failed = TRUE;
	if (failed)
	{
		printf("opera: Error when enabling signal handling.\n");
	}
}


static int X11IOErrorHandler(X11Types::Display* dpy)
{
	longjmp(g_x11_error_recover, 1);
	return 0;
}


static void ReassignSudoHOME()
{
	if( getuid() == 0 )
	{
		/* Fix for running opera with sudo.  The $HOME will not be changed and
		 * we risk overwriting files in ~/.opera/ with root rw-permissions
		 * making them unusable later.  Note: This will not fix the problem if
		 * the user is using a custom personaldir */

		setpwent();

		for( passwd* pw=getpwent(); pw; pw=getpwent() )
		{
			if( pw->pw_uid == 0 )
			{
				// Try to prevent to print message if not necessary.
				const char* env = op_getenv("HOME");
				if( !env || strncmp(env, pw->pw_dir, 5) )
				{
					printf("opera: $HOME set to %s. Use -personaldir if you do not want to use %s/.opera/\n",
						   pw->pw_dir, pw->pw_dir);
				}

				g_env.Set("HOME", pw->pw_dir);
				break;
			}
		}

		endpwent();
	}
}



static void StartPosixLogging()
{
	if (g_startup_settings.posix_dns_logger)
		g_posix_log_boss->Set(PosixLogger::DNS, g_startup_settings.posix_dns_logger);
	if (g_startup_settings.posix_socket_logger)
		g_posix_log_boss->Set(PosixLogger::SOCKET, g_startup_settings.posix_socket_logger);
	if (g_startup_settings.posix_file_logger)
		g_posix_log_boss->Set(PosixLogger::STREAM, g_startup_settings.posix_file_logger);
	if (g_startup_settings.posix_async_logger)
		g_posix_log_boss->Set(PosixLogger::ASYNC, g_startup_settings.posix_async_logger);
}
							  

static void StopPosixLogging()
{
	g_posix_log_boss->Set(PosixLogger::DNS, 0);
	g_posix_log_boss->Set(PosixLogger::SOCKET, 0);
	g_posix_log_boss->Set(PosixLogger::STREAM, 0);
	g_posix_log_boss->Set(PosixLogger::ASYNC, 0);
}


static void DestroyPosixLogging()
{
	OP_DELETE(g_startup_settings.posix_dns_logger);
	g_startup_settings.posix_dns_logger = 0;
	OP_DELETE(g_startup_settings.posix_socket_logger);
	g_startup_settings.posix_socket_logger = 0;
	OP_DELETE(g_startup_settings.posix_file_logger);
	g_startup_settings.posix_file_logger = 0;
	OP_DELETE(g_startup_settings.posix_async_logger);
	g_startup_settings.posix_async_logger = 0;
}


static BOOL ScanForOperaInstance()
{
	CommandLineManager* clm = CommandLineManager::GetInstance();
	CommandLineArgument* arg;

	X11Types::Window requested_window = None;
	arg = clm->GetArgument(CommandLineManager::WindowId);
	if (arg)
	{
		errno = 0;
		uni_char* endptr = 0;
		requested_window = uni_strtol(arg->m_string_value.CStr(), &endptr, 0);
		if( errno != 0 || (endptr && *endptr ) )
			requested_window = None;
	}

	arg = clm->GetArgument(CommandLineManager::WindowName);
	X11IPCManager::ErrorCode error;

	OpString name;
	name.Set("first");
	X11Types::Window win = X11IPCManager::FindOperaWindow(requested_window, arg ? arg->m_string_value.CStr() : name.CStr(), error);
	if (win == None)
	{
		// Terminate if we have requested a special window and could not find it
		return requested_window ? TRUE : FALSE;
	}

	BOOL ok = FALSE;

	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::FullVersion))
	{
		OpString cmd;
		cmd.Set("version()");
		ok = X11IPCManager::Set(win, cmd);
	}
	else if (clm->GetArgument(CommandLineManager::Remote))
	{
		arg = clm->GetArgument(CommandLineManager::Remote);
		ok = X11IPCManager::Set(win, arg->m_string_value);
	}
	else if (clm->GetUrls()->GetCount() > 0)
	{
		for (UINT32 i=0; i<clm->GetUrls()->GetCount(); i++)
		{
			OpString* url = clm->GetUrls()->Get(i);
			if (!url || url->Length() == 0)
			{
				continue;
			}

			OpString cmd;
			if (clm->GetArgument(CommandLineManager::ActiveTab))
			{
				cmd.AppendFormat(UNI_L("openURL(%s"), url->CStr());
			}
			else if (clm->GetArgument(CommandLineManager::NewTab))
			{
				cmd.AppendFormat(UNI_L("openURL(%s,new-tab"), url->CStr());
			}
			else if (clm->GetArgument(CommandLineManager::NewPrivateTab))
			{
				cmd.AppendFormat(UNI_L("openURL(%s,new-private-tab"), url->CStr());
			}
			else if (clm->GetArgument(CommandLineManager::NewWindow))
			{
				cmd.AppendFormat(UNI_L("openURL(%s,new-window"), url->CStr());
				
				arg = clm->GetArgument(CommandLineManager::WindowName);
				if (arg)
				{
					cmd.AppendFormat(UNI_L(",N=%s"), arg->m_string_value.CStr());
				}
				arg = clm->GetArgument(CommandLineManager::Geometry);
				if (arg)
				{
					cmd.AppendFormat(UNI_L(",G=%s"), arg->m_string_value.CStr());
				}
			}
			else if (clm->GetArgument(CommandLineManager::BackgroundTab))
			{
				cmd.AppendFormat(UNI_L("openURL(%s,background-tab"), url->CStr());
			}
			else
			{
				cmd.AppendFormat(UNI_L("openURL(%s,new-tab"), url->CStr());
			}

			if (clm->GetArgument(CommandLineManager::NoRaise))
			{
				cmd.AppendFormat(UNI_L(",noraise"));
			}
			cmd.Append(UNI_L(")"));
			
			ok = X11IPCManager::Set(win, cmd);
		}
	}
	else if (clm->GetArgument(CommandLineManager::NewTab) ||
			 clm->GetArgument(CommandLineManager::NewPrivateTab) ||
			 clm->GetArgument(CommandLineManager::NewWindow) || 
			 clm->GetArgument(CommandLineManager::BackgroundTab))
	{
		OpString cmd;
		if (clm->GetArgument(CommandLineManager::NewTab))
		{
			cmd.Set(UNI_L("newInstance(new-tab"));
		}
		else if (clm->GetArgument(CommandLineManager::NewPrivateTab))
		{
			cmd.Set(UNI_L("newInstance(new-private-tab"));
		}
		else if (clm->GetArgument(CommandLineManager::NewWindow))
		{
			cmd.Set(UNI_L("newInstance(new-window"));
			
			arg = clm->GetArgument(CommandLineManager::WindowName);
			if (arg)
			{
				cmd.AppendFormat(UNI_L(",N=%s"), arg->m_string_value.CStr());
			}
			
			arg = clm->GetArgument(CommandLineManager::Geometry);
			if (arg)
			{
				cmd.AppendFormat(UNI_L(",G=%s"), arg->m_string_value.CStr());
			}
		}
		else if (clm->GetArgument(CommandLineManager::BackgroundTab))
		{
			cmd.Set("newInstance(background-tab");
		}

		if (clm->GetArgument(CommandLineManager::NoRaise))
		{
			cmd.AppendFormat(UNI_L(",noraise"));
		}
		cmd.Append(UNI_L(")"));
		
		ok = X11IPCManager::Set(win, cmd);
	}
	else
	{
		OpString cmd;
#ifdef GADGET_SUPPORT
		if(!GadgetStartup::IsBrowserStartupRequested())
		{
			cmd.Set(UNI_L("raise()"));
		}
		else
#endif // GADGET_SUPPORT
		{
			int sdi;
			PrefsFile* pf = g_desktop_bootstrap->GetInitInfo()->prefs_reader;
			TRAPD(rc, sdi = pf->ReadIntL("User Prefs", "SDI", 0));
			if (OpStatus::IsSuccess(rc) && sdi)
				cmd.Set(UNI_L("newInstance(new-window"));
			else
				cmd.Set(UNI_L("newInstance(new-tab"));
			if (clm->GetArgument(CommandLineManager::NoRaise))
			{
				cmd.AppendFormat(UNI_L(",noraise"));
			}
			cmd.Append(UNI_L(")"));
		}
		ok = X11IPCManager::Set(win, cmd);
	}
	
	if (ok)
		printf("opera: Activated running instance\n");

	return ok;
}


static OP_STATUS GetExecutablePath(const OpStringC8& candidate, OpString8& full_path)
{
	if (candidate.IsEmpty())
		return OpStatus::ERR;

	OpString8 buf;

	int pos = candidate.Find("/");
	if (pos != KNotFound)
	{
		if (pos == 0)
			RETURN_IF_ERROR(buf.Set(candidate));
		else
		{
			if (!buf.Reserve(4096+candidate.Length()+1))
				return OpStatus::ERR_NO_MEMORY;

			if (!getcwd(buf.CStr(), 4096))
				return OpStatus::ERR;

			RETURN_IF_ERROR(buf.AppendFormat("/%s",candidate.CStr()));
		}
	}
	else
	{
		// Single name. Try to match it in the PATH
		OpString8 path;
		RETURN_IF_ERROR(path.Set(op_getenv("PATH")));
		if (path.HasContent())
		{
			char* p = strtok(path.CStr(), ":");
			while (p)
			{
				OpString8 tmp;
				RETURN_IF_ERROR(tmp.AppendFormat("%s/%s", p, candidate.CStr()));
				if (tmp.HasContent() && access(tmp.CStr(), X_OK) == 0)
				{
					RETURN_IF_ERROR(buf.Set(tmp));
					break;
				}
				p = strtok(0, ":");
			}
		}
	}

	if (buf.HasContent())
	{
		// UnixUtils::CanonicalPath() uses 16 bit so we have to convert to and from. 
		// We will not loose any data, but it takes more time than nesessary
		OpString tmp;
		RETURN_IF_ERROR(tmp.Set(buf));
		UnixUtils::CanonicalPath(tmp);
		return full_path.Set(tmp);
	}
	else
	{
		return OpStatus::OK; // Use candidate as is
	}
}

static OP_STATUS PrepareWMClassInfo(X11Globals::WMClassInfo& info)
{
	if (GadgetStartup::IsGadgetStartupRequested())
	{
		if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
		{
			info.id = X11Globals::OperaNextWidget;
			RETURN_IF_ERROR(info.name.Set("OperaNextWidget"));
			RETURN_IF_ERROR(info.icon.Set("opera-browser"));
		}
		else
		{
			info.id = X11Globals::OperaWidget;
			RETURN_IF_ERROR(info.name.Set("OperaWidget"));
			RETURN_IF_ERROR(info.icon.Set("opera-browser"));
		}

		// Append widget name
		OpString filename;
		GadgetStartup::GetRequestedGadgetFilePath(filename);
		if (filename.HasContent())
		{
			OpString name;
			if (OpStatus::IsSuccess(UnixGadgetUtils::GetWidgetName(filename, name)))
			{
				OpString8 buffer;
				int length = name.Length();
				if (length > 0 && buffer.Reserve(length+1))
				{
					char* p = buffer.CStr();
					for (int i=0; i<length; i++)
					{
						int ch = name[i];
						if(ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch >= '0' && ch <= '9')
							*p++ = ch;
					}
					*p = 0;
				}
				RETURN_IF_ERROR(info.name.Append(buffer));
			}
			// Clamp string. 64 is enough
			if (info.name.Length() > 64)
				info.name.Delete(64);
		}
	}
	else // Browser
	{
		if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
		{
			info.id = X11Globals::OperaNextBrowser;
			RETURN_IF_ERROR(info.name.Set("OperaNext"));
			RETURN_IF_ERROR(info.icon.Set("opera-next"));
		}
		else
		{
			info.id = X11Globals::OperaBrowser;
			RETURN_IF_ERROR(info.name.Set("Opera"));
			RETURN_IF_ERROR(info.icon.Set("opera-browser"));
		}
	}

	return OpStatus::OK;
}
