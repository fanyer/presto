/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "platforms/quix/commandline/quix_commandlineargumenthandler.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/stdlib/include/opera_stdlib.h"
#include "platforms/posix/posix_logger.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/common/lirc.h"
#include "platforms/unix/base/common/unixutils.h"

#include <errno.h>


OP_STATUS CommandLineArgumentHandler::Create(CommandLineArgumentHandler** handler)
{
	*handler = OP_NEW(QuixCommandLineArgumentHandler, ());
	return *handler ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

QuixCommandLineArgumentHandler::~QuixCommandLineArgumentHandler()
{
}


BOOL QuixCommandLineArgumentHandler::IsHexIntValue(const char* src, long& value)
{
	errno = 0;
	char* endptr = 0;
	value = strtol(src, &endptr, 0);

	return errno == 0 && (!endptr || !*endptr);
}



OP_STATUS QuixCommandLineArgumentHandler::CheckArgPlatform(CommandLineManager::CLArgument arg_name, const char* arg)
{
	switch (arg_name)
	{
		case CommandLineManager::WindowId:
		{
			long value;
			if( !IsHexIntValue(arg, value) )
			{
				printf("opera: Can not parse window indentifier: %s\n", arg );
				return OpStatus::ERR;
			}
			break;
		}
		case CommandLineManager::DebugShape:
		{
			long value;
			if( !IsHexIntValue(arg, value) )
			{
				printf("opera: Can not parse alpha value: %s\n", arg );
				return OpStatus::ERR;
			}
			if( value < 0 || value > 255)
				value = 0;
			g_startup_settings.shape_alpha_level = (UINT32)value;
			break;
		}
	}

	return OpStatus::OK;
}


OP_STATUS QuixCommandLineArgumentHandler::HandlePlatform(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value)
{
	CommandLineArgument* arg = CommandLineManager::GetInstance()->GetArgument(arg_name);

	switch (arg_name)
	{
		// Commands that don't need special actions (fallthrough!)
		// Most (if not all) settings are available from the CommandLineManager
		case CommandLineManager::CrashLog:
			g_startup_settings.crashlog_mode = TRUE;
			break;

	    case CommandLineManager::WatirTest:
			g_startup_settings.watir_test = TRUE;
			break;
			
		case CommandLineManager::PersonalDirectory:
		{
			g_startup_settings.personal_dir.Empty();
			RETURN_IF_ERROR(PosixNativeUtil::ToNative(arg_value->m_string_value.CStr(), &g_startup_settings.personal_dir));
			break;
		}
		case CommandLineManager::ShareDirectory:
		{
			g_startup_settings.share_dir.Empty();
			RETURN_IF_ERROR(PosixNativeUtil::ToNative(arg_value->m_string_value.CStr(), &g_startup_settings.share_dir));
			break;
		}
		case CommandLineManager::BinaryDirectory:
		{
			g_startup_settings.binary_dir.Empty();
			RETURN_IF_ERROR(PosixNativeUtil::ToNative(arg_value->m_string_value.CStr(), &g_startup_settings.binary_dir));
			break;
		}

		// Commands that don't need special actions (fallthrough!)
		case CommandLineManager::NewWindow:
		case CommandLineManager::NewTab:
		case CommandLineManager::ActiveTab:
		case CommandLineManager::BackgroundTab:
		case CommandLineManager::Fullscreen:
		case CommandLineManager::Geometry:
		case CommandLineManager::Remote:
		case CommandLineManager::NoRaise:
		case CommandLineManager::WindowId:
		case CommandLineManager::WindowName:
		case CommandLineManager::NoSession:
			break;
		case CommandLineManager::NoARGB:
			g_startup_settings.no_argb = TRUE;
			break;
		case CommandLineManager::NoLIRC:
			break;
		case CommandLineManager::DisplayName:
			g_startup_settings.display_name.Set(arg->m_string_value.CStr());
			break;
		case CommandLineManager::DisableIME:
		case CommandLineManager::Version:
		case CommandLineManager::FullVersion:
			break;
		case CommandLineManager::DebugClipboard:
			g_startup_settings.debug_clipboard = TRUE;
			break;
		case CommandLineManager::DebugFont:
			g_startup_settings.debug_font = TRUE;
			break;
		case CommandLineManager::DebugFontFilter:
			g_startup_settings.debug_font_filter = TRUE;
			break;
		case CommandLineManager::DebugKeyboard:
			g_startup_settings.debug_keyboard = TRUE;
			break;
		case CommandLineManager::DebugLocale:
			g_startup_settings.debug_locale = TRUE;
			break;
		case CommandLineManager::DebugMouse:
			g_startup_settings.debug_mouse = TRUE;
			break;
		case CommandLineManager::DebugPlugin:
			g_startup_settings.debug_plugin = TRUE;
			break;
		case CommandLineManager::DebugAtom:
			g_startup_settings.debug_atom = TRUE;
			break;
		case CommandLineManager::DebugXError:
			g_startup_settings.debug_x11_error = TRUE;
			break;
		case CommandLineManager::DebugShape:
			g_startup_settings.debug_shape = TRUE;
			break;
		case CommandLineManager::DebugIme:
			g_startup_settings.debug_ime = TRUE;
			break;
		case CommandLineManager::DebugIwScan:
			g_startup_settings.debug_iwscan = TRUE;
			break;
		case CommandLineManager::DebugLibraries:
			g_startup_settings.debug_libraries = TRUE;
			break;
		case CommandLineManager::DebugCamera:
			g_startup_settings.debug_camera = TRUE;
			break;
		case CommandLineManager::DebugLIRC:
			g_startup_settings.debug_lirc = TRUE;
			break;
		case CommandLineManager::CrashLogGpuInfo:
			break;
		case CommandLineManager::CrashAction:
			break;
		case CommandLineManager::DebugAsyncActivity:
			g_startup_settings.posix_async_logger = ReadPosixDebugArg(arg_name);
			break;
		case CommandLineManager::DebugDNS:
			g_startup_settings.posix_dns_logger = ReadPosixDebugArg(arg_name);
			break;
		case CommandLineManager::DebugFile:
			g_startup_settings.posix_file_logger = ReadPosixDebugArg(arg_name);
			break;
		case CommandLineManager::DebugSocket:
			g_startup_settings.posix_socket_logger = ReadPosixDebugArg(arg_name);
			break;
		case CommandLineManager::Session:
		{           
			g_startup_settings.session_id.Set(arg_value->m_string_value);
			int pos = g_startup_settings.session_id.Find("_");
			if (pos != KNotFound)
			{
				g_startup_settings.session_key.Set(g_startup_settings.session_id.SubString(pos+1));
				g_startup_settings.session_id.Set(g_startup_settings.session_id.SubString(0),pos);
			}
			break;
		}
		case CommandLineManager::NoGrab:
			X11Widget::DisableGrab(true);
			break;
#ifdef _DEBUG
		case CommandLineManager::DoGrab:
			g_startup_settings.do_grab = TRUE;
			break;
#endif
		case CommandLineManager::SyncEvent:
			g_startup_settings.sync_event = TRUE;
			break;

		// Not handled
		default:
			return OpStatus::ERR;
			break;
	}

	return OpStatus::OK;
}

class PosixLogListener *QuixCommandLineArgumentHandler::ReadPosixDebugArg(CommandLineManager::CLArgument flag)
{
	PosixLogger::Loquacity level = PosixLogger::NORMAL;
	FILE *out = 0;
	CommandLineArgument *arg = CommandLineManager::GetInstance()->GetArgument(flag);
	if (arg->m_string_value.HasContent())
	{
		PosixNativeUtil::NativeString param(arg->m_string_value.CStr());
#ifdef TYPE_SYSTEMATIC
		const
#endif
		char *end = 0;
		switch (int val = op_strtol(param.get(), &end, 0))
		{
		case 5: level = PosixLogger::SPEW;		break;
		case 4: level = PosixLogger::VERBOSE;	break;
		case 3: level = PosixLogger::CHATTY;	break;
		case 2: level = PosixLogger::NORMAL;	break;
		case 1: level = PosixLogger::TERSE;		break;
		case 0:
			if (end && end > param.get() + 1)
			{
				level = PosixLogger::QUIET;
				break;
			}
			// Deliberate fall-through
		default:
			if (val > 5)
			{
				level = PosixLogger::SPEW;
				fprintf(stderr, "Clipping debug arg's logging level to 5 (max): %s\n", param.get());
			}
			else // Could return 0 to indicate error ?
				fprintf(stderr, "Malformed logging level (expected number, using 2) to debug arg: %s\n", param.get());
		}
		if (end && *end == ',') // [,file]
		{
			out = fopen(end+1, "w");
			if (out == 0)
				fprintf(stderr, "Failed to open '%s' - falling back on standard output for debug arg %s\n",
						end+1, param.get());
		}
	}

	if (out)
	{
		PosixLogListener *res = OP_NEW(PosixLogToStdIO, (out, level));
		if (res) return res;
		fclose(out);
		fputs("Failed to allocate debug listener, trying again using stdout.\n", stderr);
	}
	return OP_NEW(PosixLogToStdIO, (level));
}



// static
OP_STATUS QuixCommandLineArgumentHandler::DecodeURL(OpString& candidate)
{
	if (candidate.HasContent())
	{
		OpString8 candidate8;
		RETURN_IF_ERROR(PosixNativeUtil::ToNative(candidate.CStr(), &candidate8));

		OpString8 path;
		RETURN_IF_LEAVE(path.ReserveL(PATH_MAX));

		struct stat buf;
		if (stat(candidate8.CStr(), &buf) == 0 && realpath(candidate8.CStr(), path.DataPtr()))
		{
			RETURN_IF_ERROR(UnixUtils::FromNativeOrUTF8(path.CStr(), &candidate));
		}
	}
	return OpStatus::OK;
}



OP_STATUS QuixCommandLineArgumentHandler::CheckURL(OpString& candidate)
{
	return DecodeURL(candidate);
}


BOOL QuixCommandLineArgumentHandler::HideHelp(CommandLineManager::CLArgument arg_name)
{
	switch (arg_name)
	{
	case CommandLineManager::WatirTest:
	case CommandLineManager::DebugProxy:
	case CommandLineManager::DebugAsyncActivity:
	case CommandLineManager::DebugClipboard:
	case CommandLineManager::DebugDNS:
	case CommandLineManager::DebugFile:
	case CommandLineManager::DebugSocket:
	case CommandLineManager::DebugFont:
	case CommandLineManager::DebugFontFilter:
	case CommandLineManager::DebugKeyboard:
	case CommandLineManager::DebugLocale:
	case CommandLineManager::DebugMouse:
	case CommandLineManager::DebugPlugin:
	case CommandLineManager::DebugAtom:
	case CommandLineManager::DebugXError:
	case CommandLineManager::DebugShape:
	case CommandLineManager::DebugIme:
	case CommandLineManager::DebugIwScan:
	case CommandLineManager::DebugCamera:
	case CommandLineManager::DebugLIRC:
	case CommandLineManager::NoGrab:
#ifdef _DEBUG
	case CommandLineManager::DoGrab:
#endif
	case CommandLineManager::SyncEvent:
	case CommandLineManager::DisableIME:
	case CommandLineManager::CrashLog:
	case CommandLineManager::CrashAction:
	case CommandLineManager::Session:
		return TRUE;

	default:
		return FALSE;
	}
}


int QuixCommandLineArgumentHandler::OutputText(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	printf("opera: ");
	int ret_val =  vprintf(format, args);
	va_end(args);

	return ret_val;
}

void QuixCommandLineArgumentHandler::OutputExtraHelp( CommandLineManager::CLArgument arg_name, BOOL before_main_help )
{
	if( arg_name == CommandLineManager::Help || arg_name == CommandLineManager::Question )
	{
		if( before_main_help )
		{
			OutputHelp( " A url is by default opened in a new tab\n\n" );
		}
		else
		{
			OutputHelp(
				"\n"
				"Remote commands:\n\n"
				" openURL()                      open \"Go to\" dialog box prompting for input\n"
				" openURL(url[,noraise])         open url in active window\n"
				" openURL(url[,dest][,noraise])  open url in destination <W|T|B|P>\n"
				" openFile([dest])               open file selector in destination <W|T>\n"
				" openM2([dest][,noraise])       open M2 list view in destination <W>\n"
				" openComposer([dest][,noraise]) open M2 composer in destination <W>\n"
				" addBookmark(url)               add url to bookmark list\n"
				" raise()                        raises the opera window\n"
				" lower()                        lowers the opera window\n"
				"\n"
				" [dest] Replace W: 'new-window', T: 'new-tab', B: 'background-tab', P: 'new-private-tab'\n"
				" [noraise] prevents target window to be raised\n"
				"\n"
				" A standalone url argument or '-newwindow', '-newtab', '-backgroundtab',\n"
				" '-newprivatetab' or '-nowin' will disable '-remote' commands\n"
				"\n"
				"Notes:\n\n"
				" * <geometry> format is: WIDTHxHEIGHT+XOFF+YOFF\n"
				" * '-window' and '-windowname' work for '-remote' and '-newwindow' commands\n"
				" * '-window' accepts a hexadecimal or a decimal argument\n"
				" * '-fullscreen' works when a new browser is launched\n"
				" * '-nowin' disables any url argument\n"
				" * '-windowname' will override '-newwindow' if a named window is located\n"
				" * '-windowname' accepts \"first\", \"last\" and \"operaN\" where N=1 means\n"
				"   first window and so on\n\n");
		}
	}
	else if( arg_name == CommandLineManager::DebugHelp )
	{
		OutputHelp(
			"\n"
			"Usage: opera [debug options]\n"
			"Provides simple debugging information on:\n"
			"\n"
			" -debugasync [level[,file]]     asynchronous activity\n"
			" -debugatom                     X atoms\n"
			" -debugclipboard                clipboard handling\n"
			" -debugdns [level[,file]]       dns lookup\n"
			" -debugfile [level[,file]]      file activity\n"
			" -debugfont                     font handling\n"
			" -debugfontfilter               font filter scores\n"
			" -debugiwscan                   scanning for wireless networks\n"
#ifdef FEATURE_DOM_STREAM_API
			" -debugcamera                   web camera state\n"
#endif
			" -debugkeyboard                 keyboard events\n"
			" -debuglayout                   UI layout\n"
			" -debuglibraries                loading of optional libraries\n"
			" -debuglirc                     lirc transactions\n"
			" -debuglocale                   locale handling\n"
			" -debugmouse                    mouse presses\n"
			" -debugplugin                   plugin handling\n"
			" -debugshape <alpha>            shape 24-bit windows. Show pixels with equal or higher alpha level\n"
			" -debugsocket [level[,file]]    socket activity\n"
			" -debugxerror                   X errors\n"
			" -disableinputmethods           disable input methods (IME)\n"
			" -nograb                        do not let popup windows grab mouse or keyboard\n"
			" -sync                          process events synchronously\n\n"
			"Optional 'level' (where given) controls amount of output, from 0 (low)\n"
			"to 5 (high); defaults to 2 (normal); accompanying file names where to\n"
			"save logging output (default is stdout).\n\n"
		);
	}
	else if( arg_name == CommandLineManager::LIRCHelp )
	{
		Lirc::DumpLircrc();
	}
}
