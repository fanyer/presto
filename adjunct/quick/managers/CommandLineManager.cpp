// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"

#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/commandline/CommandLineArgumentHandler.h"

#if defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_)
# include "platforms/posix/posix_native_util.h"
#endif

// Static member (a global really :))
OpAutoPtr<CommandLineManager> CommandLineManager::m_commandline_manager;

// If defs to clean up the code as windows wants case insentive compares and Unix and Mac don't
#ifdef MSWIN
 #define COMPAREFUNC	op_stricmp
 #define COMPARENFUNC	op_strnicmp
#else
 #define COMPAREFUNC	op_strcmp
 #define COMPARENFUNC	op_strncmp
#endif // MSWIN

#define ARGPREFIX			'-'
// Set an alternate prefix to use if one exisits on the platform
#ifdef MSWIN
 #define ARGPREFIX_ALT		'/'
#else
 #define ARGPREFIX_ALT		ARGPREFIX
#endif // MSWIN

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This CLArgInfo g_cl_arg_info[] MUST be in sync with the CLArgument enum in the header file
CLArgInfo g_cl_arg_info[] =
{
#ifdef WIDGET_RUNTIME_SUPPORT
	/*arg					parameter			arg_alias (old name)
	  arg_help																arg_type		arg_plaform*/
	{ "widget",				 "<file>",			0,
	  "open widget",														TRUE,	StringCLArg,	AllCLArg },
#endif // WIDGET_RUNTIME_SUPPORT
	{ "newwindow",			 0,					0,
	  "open url in new window",												FALSE,	FlagCLArg,		UnixCLArg | WinCLArg },
	{ "newtab",				 0,					"newpage",
	  "open url in new tab (default behavior)",								FALSE,	FlagCLArg,		AllCLArg },
	{ "newprivatetab",				 0,			"newprivatepage",
	  "open url in new private tab",										FALSE,	FlagCLArg,		AllCLArg },
	{ "activetab",			 0,					0,
	  "open url in current tab",											FALSE,	FlagCLArg,		UnixCLArg },
	{ "backgroundtab",		 0,					"backgroundpage",
	  "open url in background tab",											FALSE,	FlagCLArg,		UnixCLArg },
	{ "fullscreen",			 0,					0,
	  "start in full screen state",											FALSE,	FlagCLArg,		UnixCLArg | WinCLArg },
	{ "geometry",			 "<geometry>",		0,
	  "set geometry of toplevel window",									FALSE,	StringCLArg,	UnixCLArg },
	{ "remote",				 "<command>",		0,
	  "send command to another Opera window",								FALSE,	StringCLArg,	UnixCLArg },
	{ "window",				 "<window id>",		0,
	  "id of remote Opera window",											FALSE,	StringCLArg,	UnixCLArg },
	{ "windowname",			 "<window name>",	0,
	  "symbolic name of remote Opera window",								FALSE,	StringCLArg,	UnixCLArg },
	{ "noraise",			 0,					0,
	  "do not raise Opera window when sending a remote command",			TRUE,	FlagCLArg,		UnixCLArg },
	{ "nosession",			 0,					0,
	  "do not use saved sessions or homepage",								FALSE,	FlagCLArg,		UnixCLArg },
	{ "nomail",				 0,					0,
	  "start Opera without internal e-mail client",							TRUE,	FlagCLArg,		AllCLArg },
	{ "noargb",			     0,					0,
	  "do not use an ARBG (32-bit) visual",									TRUE,	FlagCLArg,		UnixCLArg },
	{ "nohwaccel",			 0,					0,
	  "do not use hardware acceleration",									TRUE,	FlagCLArg,		AllCLArg },
	{ "nolirc",				 0,					0,
	  "do not use LIRC (infra red control)",								TRUE,	FlagCLArg,		UnixCLArg },
	{ "nowin",				 0,					"nosession",
	  "do not use saved sessions or homepage",								FALSE,	FlagCLArg,		AllCLArg },
	{ "display",			 "<display name>",	0,
	  "set the X display",													TRUE,	StringCLArg,	UnixCLArg },
	{ "nograb",				0,					0,
	  "do not let popup windows grab mouse or keyboard",		   			TRUE,	FlagCLArg,		UnixCLArg },
#ifdef _DEBUG
	{ "dograb",				0,					0,
	  "grab mouse or keyboard in debug mode",		   			  			TRUE,	FlagCLArg,		UnixCLArg },
#endif
	{ "sync",				0,					0,
	  "process events synchronously",		   			                    TRUE,	FlagCLArg,		UnixCLArg },
	{ "disableinputmethods", 0,					0,
	  "disable input methods",												TRUE,	FlagCLArg,		UnixCLArg },
	{ "version",			 0,					0,
	  "show version data",													FALSE,	FlagCLArg,		UnixCLArg },
	{ "full-version",		 0,					0,
	  "show version data and build details",								FALSE,	FlagCLArg,		UnixCLArg },
	{ "debugasync",			 "[level[,file]]",	0,
	  "asynchronous activity",												TRUE,	OptCLArg,		UnixCLArg },
	{ "debugclipboard",		 0,					0,
	  "clipboard handling",													TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugdns",			 "[level[,file]]",	0,
	  "dns lookup",															TRUE,	OptCLArg,	    UnixCLArg },
	{ "debugfile",			 "[level[,file]]",	0,
	  "file activity",														TRUE,	OptCLArg,	    UnixCLArg },
	{ "debugfont",			 0,					0,
	  "font handling",														TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugfontfilter",	 0,					0,
	  "font filter handling",												TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugkeyboard",		 0,					0,
	  "keyboard events",													TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugcamera",		 0,					0,
	  "camera device logs",													TRUE,	FlagCLArg,		UnixCLArg },
	{ "debuglocale",		 0,					0,
	  "locale handling",													TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugmouse",			 0,					0,
	  "mouse presses",														TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugplugin",		 0,					0,
	  "plugin handling",													TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugsocket",		 "[level[,file]]",	0,
	  "socket activity",													TRUE,	OptCLArg,	    UnixCLArg },
	{ "debugatom",			 0,					0,
	  "X atoms",															TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugxerror",		 0,					0,
	  "X errors",															TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugshape",			 "<alpha level>",	0,
	  "shape 24-bit windows. Show pixels exceeding alpha level",		    TRUE,	StringCLArg,	UnixCLArg },
	{ "debugime",			 0,					0,
	  "input method handling",											    TRUE,	FlagCLArg,		UnixCLArg },
	{ "debugiwscan",		 0,					0,
	  "scanning for wireless networks",										TRUE,	FlagCLArg,		UnixCLArg },
	{ "debuglibraries",		 0,					0,
	  "loading of optional libraries",										TRUE,	FlagCLArg,		UnixCLArg },
	{ "debuglirc",			 0,					0,
	  "LIRC (infra red control) transactions",								TRUE,	FlagCLArg,		UnixCLArg },
	{ "debuglayout",		 0,					0,
	  "turn on UI layout debugging features",								FALSE,	FlagCLArg,		AllCLArg },
	{ "csp",				 0,					"createsingleprofile",
		"set to create a single user profile",								TRUE,	FlagCLArg,	MacCLArg },
	{ "ps",					"<suffix>",			"prefssuffix",
		"suffix to append to Opera preferences folders",					TRUE,	StringCLArg,	MacCLArg },
	{ "pd",					"<path>",			"personaldir",
	  "location of alternative Opera preferences folder",					TRUE,	StringCLArg,	UnixCLArg },
	{ "sd",					"<path>",			"sharedir",
	  "location of alternative Opera shared resource folder",				TRUE,	StringCLArg,	UnixCLArg },
	{ "bd",					"<path>",			"binarydir",
	  "location of version-specific binaries folder",						TRUE,	StringCLArg,	UnixCLArg },
	{ "screenheight",		"<height>",			0,
	  "screen height resolution in pixels",									TRUE,	IntCLArg,		MacCLArg | WinCLArg },
	{ "screenwidth",		"<width>",			0,
	  "screen width resolution in pixels",									TRUE,	IntCLArg,		MacCLArg | WinCLArg },
	{ "mail",				 0,					0,
	  "starts displaying unread mails",										FALSE,	FlagCLArg,		AllCLArg },
	{ "help",				 0,					0,
	  "displays command line help",											FALSE,	FlagCLArg,		AllCLArg },
	{ "?",					 0,					0,
	  "displays command line help",											FALSE,	FlagCLArg,		AllCLArg },
	{ "kioskhelp",			 0,					0,
		"displays kiosk mode command line help",							FALSE,	FlagCLArg,		AllCLArg },
	{ "disableca",			 0,					0,
		"disables CoreAnimation drawing mode for plugins",					FALSE,	FlagCLArg,		MacCLArg },
	{ "disableica",			 0,					0,
		"disables InvalidatingCoreAnimation drawing mode for plugins",		FALSE,	FlagCLArg,		MacCLArg },
#ifdef _DEBUG
	{ "debugsettings",		 0,					0,
	  "specify file for debug settings",									TRUE,	StringCLArg,	WinCLArg },
#endif
	{ "mediacenter",		 0,					0,
	  "run on windows media center",										TRUE,	FlagCLArg,		WinCLArg },
	{ "settings",			 0,					0,
	  "specify an alternate configuration file",							TRUE,	StringCLArg,	WinCLArg },
	{ "reinstallbrowser",	 0,					0,
	  "sets up Opera as default browser",									FALSE,	FlagCLArg,		WinCLArg },
	{ "reinstallmailer",	 0,					0,
	  "sets up Opera as default mail client",								FALSE,	FlagCLArg,		WinCLArg },
	{ "reinstallnewsreader", 0,					0,
	  "sets up Opera as default news reader",								FALSE,	FlagCLArg,		WinCLArg },
	{ "hideiconscommand"	,0,					0,
	  "hides Opera access points",											TRUE,	FlagCLArg,		WinCLArg },
	{ "showiconscommand",	 0,					0,
	  "makes Opera access points available",								TRUE,	FlagCLArg,		WinCLArg },
	{ "nominmaxbuttons",	0,					0,
	  "do not show minimize and maximize buttons in the title bar",			TRUE,	FlagCLArg,		WinCLArg },
	{ "debughelp",			0,					0,
	  "displays available debug options",									FALSE,	FlagCLArg,		UnixCLArg },
	{ "lirchelp",			0,					0,
	  "extra options for LIRC (infra red control)",							FALSE,	FlagCLArg,		UnixCLArg },
	{ "urllist",		 "<filepath>",			0,
	  "load each line in the given page in an automated run",				FALSE,	StringCLArg,	AllCLArg},
	{ "urllistloadtimeout",	   "<seconds>",		0,
	  "timeout for each page specified with 'urllist' argument",			FALSE,	IntCLArg,		AllCLArg},
#ifdef FEATURE_UI_TEST
	{ "uicrawl",			0,					0,
	  "start a UI crawl",													FALSE,	FlagCLArg,		AllCLArg},
#endif // FEATURE_UI_TEST
	{ "uiparserlog",		"<filename>",		0,
	  "UI parser log file for dialogs.yml",									FALSE,	StringCLArg,	AllCLArg},
	{ "uiwidgetsparserlog",		"<filename>",	0,
	  "UI parser log file for widgets.yml",									FALSE,	StringCLArg,	AllCLArg},
#ifdef SELFTEST
	{ "srcroot",			0,					0,
	  "root folder of the Opera source code",								TRUE,	StringCLArg,	AllCLArg},
#endif // SELFTEST
	{ "nocrashhandler",		0,					0,
	  "turn Opera crash logging off",										TRUE,	FlagCLArg,	    MacCLArg },
	{ "crashlog",			 0,					0,
	  "crashlog",															FALSE,	StringCLArg,	AllCLArg },
	{ "crashlog-gpuinfo",			 0,					0,
	  "crashlog-gpuinfo",															FALSE,	StringCLArg,	AllCLArg },
	{ "crashaction",		0,					0,
	  "crashaction",														FALSE,	StringCLArg,	UnixCLArg },
	{ "session",		 		0,				0,
	  "Session manager identifier ",									    FALSE,	StringCLArg,	UnixCLArg },
	{ "addfirewallexception",0,					0,
	  "add firewall exceptions",											FALSE,	FlagCLArg,		WinCLArg },
	{ "profilinglog",		"<filename>",		"profile",
		"startup and exit profiling log file",								FALSE,	StringCLArg,	AllCLArg },
	{ "delayshutdown",		"<seconds>",		0,
	  "delayshutdown",														TRUE,	IntCLArg,		WinCLArg },
	{ "autotestmode",								0,					0,
		"watir test settings",												TRUE,	FlagCLArg,		AllCLArg },
	{ "debugproxy",                             0,                          0,
	  "proxy for scope debugging" ,                                        TRUE,   StringCLArg, AllCLArg },
	{ "dialogtest",			"<dialogname>",		0,
	  "Test a dialog layout",												FALSE,  StringCLArg,	AllCLArg },
	{ "inidialogtest",		"<dialogname>",		0,
	  "Test an ini dialog layout",											FALSE,  StringCLArg,	AllCLArg },
	{ "gputest",			"<backendtype>",	0,
	  "Test gpu with specified backend",									FALSE,  IntCLArg,		AllCLArg },
//Opera installer for windows
	{ "install",			0,					"update",
	  "install",															TRUE,	FlagCLArg,		WinCLArg },
	{ "uninstall",			0,					0,
	  "uninstall",															TRUE,	FlagCLArg,		WinCLArg },
	{ "autoupdate",			0,					0,
	  "autoupdate",															TRUE,	FlagCLArg,		WinCLArg },
	{ "silent",				0,					0,
	  "silent",																TRUE,	FlagCLArg,		WinCLArg },
	{ "installfolder",		"<folder>",			0,
	  "installfolder",														TRUE,	StringCLArg,	WinCLArg },
	{ "language",			"<languagecode>",	0,
	  "language-code",														TRUE,	StringCLArg,	WinCLArg },
	{ "copyonly",			0,					0,
	  "copyonly",															TRUE,	IntCLArg,		WinCLArg },
	{ "allusers",			0,					0,
	  "allusers",															TRUE,	IntCLArg,		WinCLArg },
	{ "singleprofile",		0,					0,
	  "singleprofile",														TRUE,	IntCLArg,		WinCLArg },
	{ "setdefaultpref",		0,					0,
	  "setdefaultpref",														TRUE,	StringListCLArg,WinCLArg },
	{ "setfixedpref",		0,					0,
	  "setfixedpref",														TRUE,	StringListCLArg,WinCLArg },
	{ "setdefaultbrowser",	0,					0,
	  "setdefaultbrowser",													TRUE,	IntCLArg,		WinCLArg },
	{ "startmenushortcut",	0,					0,
	  "startmenushortcut",													TRUE,	IntCLArg,		WinCLArg },
	{ "desktopshortcut",	0,					0,
	  "desktopshortcut",													TRUE,	IntCLArg,		WinCLArg },
	{ "quicklaunchshortcut",0,					0,
	  "quicklaunchshortcut",												TRUE,	IntCLArg,		WinCLArg },
	{ "pintotaskbar",		0,					0,
	  "pintotaskbar",														TRUE,	IntCLArg,		WinCLArg },
	{ "launchopera",		0,					0,
	  "launchopera",														TRUE,	IntCLArg,		WinCLArg },
	{ "give_folder_write_access", 0,			0,
	  "give_folder_write_access",											TRUE,	StringCLArg,	WinCLArg },
	{ "give_write_access_to", 0,				0,
	  "give_write_access_to",												TRUE,	StringCLArg,	WinCLArg },
	{ "updatecountrycode",	0,					0,
	  "updatecountrycode",													TRUE,	FlagCLArg,		WinCLArg },
	{ "countrycode",		0,					0,
	  "countrycode",														TRUE,	StringCLArg,	WinCLArg },
	{ "delaycustomizations",	"<seconds>",	0,
	  "delay loading of speed dials, bookmarks, and search engines",		TRUE,	IntCLArg,		AllCLArg },
#if defined(_DEBUG) && defined(MEMORY_ELECTRIC_FENCE)
	{ "fence",					"<class list>",	0,
	"comma separated list of classes to protect after freeing their memory",	TRUE,	StringCLArg,		WinCLArg },
#endif // MEMORY_ELECTRIC_FENCE
	{ NULL, NULL, NULL, NULL, FALSE, InvalidCLArg, NoneCLArg }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandLineManager *CommandLineManager::GetInstance()
{
	if (!m_commandline_manager.get())
	{
		m_commandline_manager = OP_NEW(CommandLineManager, ());
	}
	return m_commandline_manager.get();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandLineManager::CommandLineManager() :
	m_handler(NULL)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandLineManager::~CommandLineManager()
{
	// Cleanup array of pointers (or what's left of them)
	for (int i = 0; i < ARGUMENT_COUNT; i++)
		OP_DELETE(m_args[i]);

	// Clean up the assigned handler
	OP_DELETE(m_handler);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS CommandLineManager::Init()
{
	// Start the array of arg as NULL pointers
	op_memset(m_args, 0, sizeof(m_args));

	// Create the handler
	return CommandLineArgumentHandler::Create(&m_handler);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS CommandLineManager::ParseArguments(int argc, const char* const* argv, const uni_char* const* wargv)
{
	OP_ASSERT(m_handler);
	BOOL found_url = FALSE;

	RETURN_IF_ERROR(m_binary_path.Set(argv[0]));

	// Loop with a switch to parse all the command line arguments
	for (int i = 1; i < argc; i++)
	{
		// Check for -, -- and /
		if (argv[i] && ((argv[i][0] == ARGPREFIX) || (argv[i][0] == ARGPREFIX_ALT)))
		{
			if (found_url)
				continue;

			int		cmd_num = 0, offset = 1;
			BOOL	unknown_arg = TRUE;

			// Add code to skip over the second '-' if it exists
			if ((argv[i][0] == ARGPREFIX) && argv[i][1] && (argv[i][1] == ARGPREFIX))
				offset++;

			// Save the next arg for easy of selection
			const char *next_arg = ((i + 1) < argc) ? argv[i + 1] : NULL;
			const uni_char *next_argw = NULL;
			if (wargv)
				next_argw = ((i + 1) < argc) ? wargv[i + 1] : NULL;

			// Start by checking for kiosk mode flags
			int flags_eaten = KioskManager::GetInstance()->ParseFlag(argv[i] + offset, next_arg);
			if (flags_eaten > 0)
			{
				// All kiosk mode flags should be kept over restart
				for (int j = 0; j < flags_eaten; j++)
					RETURN_IF_ERROR(AppendRestartArgument(argv[i + j]));
				i += flags_eaten - 1;
			}
			else
			{
				// Loop the array of known arguments and try to match the
				// argument currently processed.
				while (g_cl_arg_info[cmd_num].arg)
				{
					const char* candidate = argv[i] + offset;
					const char* arg = g_cl_arg_info[cmd_num].arg;
					const char* alias = g_cl_arg_info[cmd_num].arg_alias;
					const CLArgType arg_type = g_cl_arg_info[cmd_num].arg_type;

					if (!(candidate && !COMPAREFUNC(candidate, arg))
							&& !(alias && !COMPAREFUNC(candidate, alias)))
					{
						++cmd_num;
						continue;
					}

					if (alias && !COMPAREFUNC(candidate, alias))
					{
						// Print a warning if a deprecated alias is used.
						BOOL deprecated_alias = FALSE;
						if (!COMPAREFUNC(alias, "newpage"))
							deprecated_alias = TRUE;
						else if (!COMPAREFUNC(alias, "newprivatepage"))
							deprecated_alias = TRUE;
						else if (!COMPAREFUNC(alias, "backgroundpage"))
							deprecated_alias = TRUE;
						if (deprecated_alias)
							m_handler->OutputText("The argument '%c%s' is deprecated and may stop working in a future version of opera.  Use '%c%s' instead.\n", ARGPREFIX, alias, ARGPREFIX, arg);
					};

					unknown_arg = FALSE;

					if ((arg_type == StringCLArg || arg_type == StringListCLArg || arg_type == IntCLArg)
							&& !next_arg && !next_argw)
					{
						m_handler->OutputText("Parameter to %s is required\n",
								argv[i]);
						return OpStatus::ERR;
					}
					else if (arg_type == FlagCLArg || arg_type == OptCLArg
							|| next_arg || next_argw)
					{
						// First check what is on the line is valid
						if (arg_type != FlagCLArg)
							RETURN_IF_ERROR(m_handler->CheckArg((CLArgument)cmd_num, next_arg));

						// Create the argument
						if (!m_args[cmd_num])
							m_args[cmd_num] = OP_NEW(CommandLineArgument, ());
						if (!m_args[cmd_num])
							return OpStatus::ERR_NO_MEMORY;

						// Set the value based on the type
						switch (arg_type)
						{
							case StringCLArg:
								if (next_argw)
								{
									// This is what should happen on Windows
									m_args[cmd_num]->m_string_value.Set(next_argw);
								}
								else
								{
#if defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_)
									// This is what should happen on UNIX and Mac
									m_args[cmd_num]->m_string_value.Empty();
									RETURN_IF_ERROR(PosixNativeUtil::FromNative(next_arg, &m_args[cmd_num]->m_string_value));
#else
									// This should not happen
									OP_ASSERT(!"Cannot convert the string from the locale encoding correctly");
									// Too early for a portable use of OpLocale::ConvertFromLocaleEncoding()
									m_args[cmd_num]->m_string_value.Set(next_arg);
#endif
								}
								if (g_cl_arg_info[cmd_num].arg_restart)
									RETURN_IF_ERROR(AddRestartArgument(CLArgument(cmd_num), next_arg));
								i++;
							break;

							case StringListCLArg:
							{
								OpAutoPtr<OpString> str(OP_NEW(OpString, ()));
								RETURN_OOM_IF_NULL(str.get());
								if (next_argw)
								{
									// This is what should happen on Windows
									RETURN_IF_ERROR(str->Set(next_argw));
								}
								else
								{
#if defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_)
									// This is what should happen on UNIX and Mac
									m_args[cmd_num]->m_string_value.Empty();
									RETURN_IF_ERROR(PosixNativeUtil::FromNative(next_arg, str.get()));
#else
									// This should not happen
									OP_ASSERT(!"Cannot convert the string from the locale encoding correctly");
									// Too early for a portable use of OpLocale::ConvertFromLocaleEncoding()
									RETURN_IF_ERROR(str->Set(next_arg));
#endif
								}
								RETURN_IF_ERROR(m_args[cmd_num]->m_string_list_value.Add(str.get()));
								str.release();
								if (g_cl_arg_info[cmd_num].arg_restart)
									RETURN_IF_ERROR(AddRestartArgument(CLArgument(cmd_num), next_arg));
								i++;
							}
							break;

							case IntCLArg:
							{
								int value;
								int num_scanned = sscanf(next_arg, "%d", &value);
								if (num_scanned == 1)
									m_args[cmd_num]->m_int_value = value;
								else
								{
									// Output the unknown argument
									m_handler->OutputText("Illegal parameter to %s: %s\n", argv[i], next_arg);
									return OpStatus::ERR;
								}
								if (g_cl_arg_info[cmd_num].arg_restart)
									RETURN_IF_ERROR(AddRestartArgument(CLArgument(cmd_num), m_args[cmd_num]->m_int_value));
								i++;
							}
							break;

							case FlagCLArg:
							{
								if (g_cl_arg_info[cmd_num].arg_restart)
									RETURN_IF_ERROR(AddRestartArgument(CLArgument(cmd_num)));
							}
							break;

							case OptCLArg:
								if (next_arg && next_arg[0] != '-')
								{
									if (next_argw)
										m_args[cmd_num]->m_string_value.Set(next_argw);
									else
										m_args[cmd_num]->m_string_value.Set(next_arg);

									if (g_cl_arg_info[cmd_num].arg_restart)
										RETURN_IF_ERROR(AddRestartArgument(CLArgument(cmd_num), m_args[cmd_num]->m_int_value));

									i++;
								}
								else
								{
									if (g_cl_arg_info[cmd_num].arg_restart)
										RETURN_IF_ERROR(AddRestartArgument(CLArgument(cmd_num)));
								}

							break;
						}

						// Handle the argument
						RETURN_IF_ERROR(m_handler->Handle((CLArgument)cmd_num, m_args[cmd_num]));

						break;
					}

					cmd_num++;
				}

				// We need to save the unknown arg and/or write it to the screen
				if (unknown_arg)
				{
#ifdef SELFTEST
					/* If this unknown arg starts with "-test" we'll just
					 * completely ignore it as a selftest arg; likewise if it
					 * has a run- or exclude- prefix; keep in sync with
					 * modules/selftest/src/optestsuite.cpp's
					 * SelftestCLParser::Parse(). */
					if (COMPARENFUNC(argv[i] + offset, "run", 3) == 0)
					{
						offset += 3;
						while (argv[i][offset] == '-') offset++;
					}
					if (COMPARENFUNC(argv[i] + offset, "exclude", 7) == 0)
					{
						offset += 7;
						while (argv[i][offset] == '-') offset++;
					}

					if (argv[i][offset] &&
						COMPARENFUNC(argv[i] + offset, "test", 4) != 0)
#endif // SELFTEST
					{
						// Output the unknown argument
						m_handler->OutputText("Unknown argument: %s\n", argv[i]);
						return OpStatus::ERR;
					}
				}
			}
		}
#ifdef MSWIN
		// (julienp) Search the internet command: "? search query"
		// Meant to be used on Vista
		else if (argv[i] && (argv[i][0] == '?'))
		{
			if (found_url)
				break;

			OpString* searchstring = OP_NEW(OpString, ());
			searchstring->Set(UNI_L("?"));
			while (i+1 < argc)
			{
				searchstring->Append(" ");
				if (wargv)
					searchstring->Append(wargv[++i]);
				else
					searchstring->Append(argv[++i]);
			}
			m_commandline_urls.Add(searchstring);
		}
#endif
		else		//list of URLs to open
		{
			found_url = TRUE;

			OpString* newurlstring;
			newurlstring = OP_NEW(OpString, ());
			if (!newurlstring)
				return OpStatus::ERR_NO_MEMORY;

			if (wargv)
			{
				// This is what should happen on Windows
				newurlstring->Set(wargv[i]);
			}
			else
			{
#if defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_)
				// This is what should happen on UNIX and Mac
				RETURN_IF_ERROR(PosixNativeUtil::FromNative(argv[i], newurlstring));
#else
				// This should not happen
				OP_ASSERT(!"Cannot convert the string from the locale encoding correctly");
				// Too early for a portable use of OpLocale::ConvertFromLocaleEncoding()
				newurlstring->Set(argv[i]);
#endif
			}

			// A local file name may have to be modified
			m_handler->CheckURL(*newurlstring);

			m_commandline_urls.Add(newurlstring);
		}
	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS CommandLineManager::AddRestartArgument(CLArgument arg)
{
	const char prefix[] = { ARGPREFIX, 0 };
	OpString8 str;
	RETURN_IF_ERROR(str.Set(prefix));
	RETURN_IF_ERROR(str.Append(g_cl_arg_info[arg].arg));
	RETURN_IF_ERROR(AppendRestartArgument(str.CStr()));
	return OpStatus::OK;
}

OP_STATUS CommandLineManager::AddRestartArgument(CLArgument arg, int value)
{
	RETURN_IF_ERROR(AddRestartArgument(arg));
	OpString8 str;
	RETURN_IF_ERROR(str.AppendFormat("%d", value));
	RETURN_IF_ERROR(AppendRestartArgument(str.CStr()));
	return OpStatus::OK;
}

OP_STATUS CommandLineManager::AddRestartArgument(CLArgument arg, const char* value)
{
	RETURN_IF_ERROR(AddRestartArgument(arg));
	RETURN_IF_ERROR(AppendRestartArgument(value));
	return OpStatus::OK;
}

OP_STATUS CommandLineManager::AppendRestartArgument(const char* arg)
{
	OpAutoPtr<OpString8> str(OP_NEW(OpString8, ()));
	if (!str.get())
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(str->Set(arg));
	RETURN_IF_ERROR(m_restart_arguments.Add(str.get()));
	str.release();
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
