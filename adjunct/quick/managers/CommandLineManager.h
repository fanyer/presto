// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __COMMANDLINE_MANAGER_H__
#define __COMMANDLINE_MANAGER_H__

#define g_commandline_manager (CommandLineManager::GetInstance())

class CommandLineArgumentHandler;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CommandLineArgument
{
	OpAutoVector<OpString>	m_string_list_value;//Holds a list of strings for a list of strings argument
	OpString				m_string_value;		// Holds the string value if it's a string argument
	int						m_int_value;		// Holds the int value if it's a int argument
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CommandLineStatus : public OpStatus
{
public:
    enum
    {
		/**
		 * The program needs to exit so just make this
		 * an error and the rest should take care of itself
		 */
        CL_HELP = USER_ERROR + 0,
    };
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CommandLineManager
{
public:
	// This enum MUST be in sync with the CLArgInfo g_cl_arg_info[] in the source file
	enum CLArgument
	{
#ifdef WIDGET_RUNTIME_SUPPORT
		Widget,
#endif // WIDGET_RUNTIME_SUPPORT
		NewWindow,
		NewTab,
		NewPrivateTab,
		ActiveTab,
		BackgroundTab,
		Fullscreen,
		Geometry,
		Remote,
		WindowId,
		WindowName,
		NoRaise,
		NoSession,
		NoMail,
		NoARGB,
		NoHWAccel,
		NoLIRC,
		NoWin,
		DisplayName,
		NoGrab,
#ifdef _DEBUG
		DoGrab,
#endif
		SyncEvent,
		DisableIME,
		Version,
		FullVersion,
		DebugAsyncActivity,
		DebugClipboard,
		DebugDNS,
		DebugFile,
		DebugFont,
		DebugFontFilter,
		DebugKeyboard,
		DebugCamera,
		DebugLocale,
		DebugMouse,
		DebugPlugin,
		DebugSocket,
		DebugAtom,
		DebugXError,
		DebugShape,
		DebugIme,
		DebugIwScan,
		DebugLibraries,
		DebugLIRC,
		DebugLayout,
		CreateSingleProfile,
		PrefsSuffix,
		PersonalDirectory,
		ShareDirectory,
		BinaryDirectory,
		ScreenHeight,
		ScreenWidth,
		StartMail,
		Help,
		Question,
		KioskHelp,
		DisableCoreAnimation,
		DisableInvalidatingCoreAnimation,
#ifdef _DEBUG
		DebugSettings,
#endif
		MediaCenter,
		Settings,
		ReinstallBrowser,
		ReinstallMailer,
		ReinstallNewsReader,
		HideIconsCommand,
		ShowIconsCommand,
		NoMinMaxButtons,
		DebugHelp, // Always on, not to be disabled by _DEBUG ifdef
		LIRCHelp,
		UrlList,
		UrlListLoadTimeout,
#ifdef FEATURE_UI_TEST
		UserInterfaceCrawl,
#endif // FEATURE_UI_TEST
		UIParserLog,
		UIWidgetParserLog,
#ifdef SELFTEST
		SourceRootDir,
#endif // SELFTEST
		NoCrashHandling,
		CrashLog,
		CrashLogGpuInfo,
		CrashAction,
		Session,
		AddFirewallException,
		ProfilingLog,
		DelayShutdown,
		WatirTest,
		DebugProxy,
		DialogTest,
		IniDialogTest,
		GPUTest,
		OWIInstall,
		OWIUninstall,
		OWIAutoupdate,
		OWISilent,
		OWIInstallFolder,
		OWILanguage,
		OWICopyOnly,
		OWIAllUsers,
		OWISingleProfile,
		OWISetDefaultPref,
		OWISetFixedPref,
		OWISetDefaultBrowser,
		OWIStartMenuShortcut,
		OWIDesktopShortcut,
		OWIQuickLaunchShortcut,
		OWIPinToTaskbar,
		OWILaunchOpera,
		OWIGiveFolderWriteAccess,
		OWIGiveWriteAccessTo,
		OWIUpdateCountryCode,
		OWICountryCode,
		DelayCustomizations,
#if defined(_DEBUG) && defined(MEMORY_ELECTRIC_FENCE)
		Fence,
#endif // MEMORY_ELECTRIC_FENCE
		ARGUMENT_COUNT
	};

public:

	virtual ~CommandLineManager();

	static CommandLineManager *GetInstance();

	/** Initialization function, only run other functions if this returns success
	  */
	OP_STATUS Init();

	/** Parse all arguments in argc/argv format so that they can later be retrieved
	  * @param argc Argument count as given to main()
	  * @param argv Arguments as given to main()
	  * @param wargv Like argv, but in wide character format
	  */
	OP_STATUS ParseArguments(int argc, const char* const* argv, const uni_char* const* wargv = NULL);

	/** Get the value of a given command line argument
	  * @param arg Argument to get
	  * @return Pointer to value, or NULL if this command line argument wasn't specified
	  */
	CommandLineArgument *GetArgument(CLArgument arg) const { return m_args[arg]; }

	OpVector<OpString> *GetUrls() { return &m_commandline_urls; }

	CommandLineArgumentHandler  *GetHandler() const { return m_handler;}

    // These functions allow the platform to add arguments to be used
    // when restarting, in addition to those we got on the command
    // line. Note that this doesn't get stored in m_args.
    OP_STATUS AddRestartArgument(CLArgument arg);
    OP_STATUS AddRestartArgument(CLArgument arg, int value);
    OP_STATUS AddRestartArgument(CLArgument arg, const char* value);

    // Return the list of arguments to use when restarting Opera
    const OpVector<OpString8> & GetRestartArguments() const { return m_restart_arguments; }

	// Return path to the Opera binary as used when starting Opera
	OpStringC8 GetBinaryPath() const { return m_binary_path; }

private:

	CommandLineManager();

	OP_STATUS AppendRestartArgument(const char* arg);

	CommandLineArgument			*m_args[ARGUMENT_COUNT];	// Pointer array for the command line arguments
	CommandLineArgumentHandler	*m_handler;					// Pointer to the argument handling class
	OpAutoVector<OpString>		m_commandline_urls;			// List of urls typed at the command line.
	OpAutoVector<OpString8>     m_restart_arguments;        // Arguments to pass when restarting Opera
	OpString8					m_binary_path;				// Path to the Opera binary as used when starting Opera

	static OpAutoPtr<CommandLineManager> m_commandline_manager;
};

#endif // __COMMANDLINE_MANAGER_H__

