/*
 *  MacCommandLineArgumentHandler.cpp
 *  Opera
 *
 *  Created by Adam Minchinton on 6/19/07.
 *  Copyright 2007 Opera All rights reserved.
 *
 */

#include "core/pch.h"

#include "modules/util/opfile/opfile.h"

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"

#include "platforms/mac/subclasses/MacCommandLineArgumentHandler.h"
#include "platforms/mac/File/FileUtils_Mac.h"

// Suffix to add to the end of the Opera Preferences folders when -pd option is used
OpString g_pref_folder_suffix;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS CommandLineArgumentHandler::Create(CommandLineArgumentHandler** handler)
{
	*handler = new MacCommandLineArgumentHandler();

	return *handler ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacCommandLineArgumentHandler::CheckArgPlatform(CommandLineManager::CLArgument arg_name, const char* arg)
{
	// Check the param if needed

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacCommandLineArgumentHandler::HandlePlatform(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value)
{
	// Only add the arguments that need to be handled in a specific way for this patform here
	switch (arg_name)
	{
		case CommandLineManager::PrefsSuffix:
			g_pref_folder_suffix.Set(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PrefsSuffix)->m_string_value);
		break;

		// Now supported on Mac as well. See DSK-339862
		case CommandLineManager::PersonalDirectory:
		break;

		case CommandLineManager::ScreenHeight:
		case CommandLineManager::ScreenWidth:
		{
			CommandLineArgument *cl_height, *cl_width;

			// If both of these have be gotten then we can change resolution
			if ((cl_height = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ScreenHeight)) != NULL &&
				(cl_width = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::ScreenWidth)) != NULL)
			{
				// This code needs to be refined so that it doesn't mess up the desktop, tests if it can chage to the mode
				// and most likely only change the main display? Should also use the same colour depth as it was using
				//CGDirectDisplayID dspy;

				CGDirectDisplayID display[5];
				CGDisplayCount numDisplays;
				CGDisplayCount i;
				CGDisplayErr err;
				boolean_t exactMatch;

				err = CGGetActiveDisplayList(5, display, &numDisplays);

				if ( err == CGDisplayNoErr )
				{
					for ( i = 0; i < numDisplays; ++i )
					{
						CFDictionaryRef mode;

						// Perhaps this should change to other resolutions?

						mode = CGDisplayBestModeForParameters(display[i],
															  24,
															  800,
															  600,
															  &exactMatch);

						if ((err = CGDisplaySwitchToMode(display[i], mode)) != CGDisplayNoErr)
							return OpStatus::ERR;
					}
				}
				else
					return OpStatus::ERR;
			}
		}
		break;
			
		case CommandLineManager::CrashLog:
			// Crash recovery requires disabling HW acceleration
			g_desktop_bootstrap->DisableHWAcceleration();
			break;

		default:
			// Not handled
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL MacCommandLineArgumentHandler::HideHelp(CommandLineManager::CLArgument arg_name)
{
	switch (arg_name)
	{
		case CommandLineManager::DisableCoreAnimation:
		case CommandLineManager::DisableInvalidatingCoreAnimation:
 		case CommandLineManager::WatirTest:
 		case CommandLineManager::DebugProxy:
			return TRUE;
		default:
			return FALSE;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int MacCommandLineArgumentHandler::OutputText(const char *format, ...)
{
	char buf[256] = {0};
	
	va_list args;

	va_start(args, format);
	int ret_val =  vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	// Dont output unknown argument -psn message, the psn argument is added by the system
	if(!strstr(buf, "Unknown argument: -psn"))
	{
		printf("%s", buf);
	}
	
	return ret_val;
}
