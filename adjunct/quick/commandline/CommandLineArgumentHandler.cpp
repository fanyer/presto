// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Created by Adam Minchinton on 6/19/07.
//

#include "core/pch.h"

#include "adjunct/quick/commandline/CommandLineArgumentHandler.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"

extern CLArgInfo g_cl_arg_info[];


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS CommandLineArgumentHandler::CheckArg(CommandLineManager::CLArgument arg_name, const char* arg)
{
	// First try to check the command line argument in the platform
	if (OpStatus::IsSuccess(CheckArgPlatform(arg_name, arg)))
		return OpStatus::OK;
		
	// If not checked by the platform then check it in a generic way now
	// (julienp)Uncomment when using, otherwise it generates warnings.
/*	switch (arg_name)
	{
	}*/
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS CommandLineArgumentHandler::Handle(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value)
{
	// First try to handle the command line argument in the platform
	if (OpStatus::IsSuccess(HandlePlatform(arg_name, arg_value)))
		return OpStatus::OK;
		
	// If not handled by the platform then handle it in a generic way now
	switch (arg_name)
	{
		case CommandLineManager::Help:
		case CommandLineManager::Question:
		{
			// Print out the help info

			OutputHelp("Usage: opera [options] [url]\n\n");
			
			OutputExtraHelp(arg_name, TRUE);

			OutputHelpEntries(OutputHelpEntries(-1));

			OutputExtraHelp(arg_name, FALSE);
			
			// Return an error so it doesn't start the application
			// This error is different and is only returned for help
			return CommandLineStatus::CL_HELP;
		}
		break;
	
		case CommandLineManager::KioskHelp:
			KioskManager::PrintCommandlineOptions();

			// Return an error so it doesn't start the application
			// This error is different and is only returned for help
			return CommandLineStatus::CL_HELP;
		break;

		case CommandLineManager::LIRCHelp:
			// Falltrough

		case CommandLineManager::DebugHelp:
			OutputExtraHelp(arg_name, TRUE);
			return CommandLineStatus::CL_HELP;
		break;

		case CommandLineManager::NoHWAccel:
			g_desktop_bootstrap->DisableHWAcceleration();
			break;

		case CommandLineManager::DisableCoreAnimation:
		case CommandLineManager::DisableInvalidatingCoreAnimation:
		case CommandLineManager::NoMail:
		case CommandLineManager::NewTab:
		case CommandLineManager::NewPrivateTab:
		case CommandLineManager::ProfilingLog:
		case CommandLineManager::NoWin:
		case CommandLineManager::NoMinMaxButtons:
		case CommandLineManager::StartMail:
		case CommandLineManager::UrlList:
		case CommandLineManager::UrlListLoadTimeout:
		case CommandLineManager::WatirTest:
		case CommandLineManager::DebugProxy:
		case CommandLineManager::DebugCamera:
		case CommandLineManager::DebugLayout:
		case CommandLineManager::DialogTest:
		case CommandLineManager::IniDialogTest:
		case CommandLineManager::DelayCustomizations:
		case CommandLineManager::GPUTest:
			// Nothing to do, just acknowledge it exists
			break;

#ifdef FEATURE_UI_TEST
		case CommandLineManager::UserInterfaceCrawl:
			// Nothing to do, just acknowledge it exists
			break;
#endif // FEATURE_UI_TEST

		case CommandLineManager::UIParserLog:
		case CommandLineManager::UIWidgetParserLog:
			// Nothing to do, just acknowledge it exists
			break;

#ifdef SELFTEST
		case CommandLineManager::SourceRootDir:
			// Nothing to do, just acknowledge it exists
			break;
#endif // SELFTEST

#ifdef WIDGET_RUNTIME_SUPPORT
		case CommandLineManager::Widget:
			// Nothing to do, just acknowledge it exists
			break;

#endif // WIDGET_RUNTIME_SUPPORT

		default:
			// This shouldn't happen so assert
			OP_ASSERT(FALSE);

			// Not handled
			return OpStatus::ERR;
		break;
	}
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CommandLineArgumentHandler::OutputText(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	int ret_val =  vprintf(format, args);
	va_end(args);
	
	return ret_val;
}


int CommandLineArgumentHandler::OutputHelp(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	int ret_val =  vprintf(format, args);
	va_end(args);
	
	return ret_val;
}


void CommandLineArgumentHandler::OutputExtraHelp( CommandLineManager::CLArgument arg_name, BOOL before_main_help )
{
}


int CommandLineArgumentHandler::OutputHelpEntries( int width )
{
	int w = 0;

	char format_buf[100];
	sprintf(format_buf, " -%%- %ds  %%s\n", width );

	for(int	cmd_num = 0; g_cl_arg_info[cmd_num].arg; cmd_num++ )
	{
		// Skip those args not on your platform
		
		if (!(g_cl_arg_info[cmd_num].arg_platform & GetPlatform()) || HideHelp((CommandLineManager::CLArgument)cmd_num))
		{
			continue;
		}
		

		OpString8 tmp;
		tmp.Append(g_cl_arg_info[cmd_num].arg);
		if( g_cl_arg_info[cmd_num].parameter )
		{
			tmp.Append(" ");
			tmp.Append(g_cl_arg_info[cmd_num].parameter);
		}
		
		if( width <= 0 )
		{
			// Just compute width
			int len = tmp.Length();
			if( w < len )
				w = len;
		}
		else
		{
			// Output text
			OutputHelp(format_buf, tmp.CStr(), g_cl_arg_info[cmd_num].arg_help);
		}
	}

	return width <= 0 ? w : width;
}


