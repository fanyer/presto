// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "core/pch.h"
#include "WindowsOpCommandLineArgumentHandler.h"
#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/installer/OperaInstaller.h"

extern void	MakeDefaultBrowser(BOOL force_HKLM = FALSE);
extern void	MakeDefaultMailClient(BOOL force_HKLM = FALSE);

OP_STATUS CommandLineArgumentHandler::Create(CommandLineArgumentHandler** handler)
{
	if ((*handler = new WindowsOpCommandLineArgumentHandler()) != 0)
		return OpStatus::OK;
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS WindowsOpCommandLineArgumentHandler::CheckArgPlatform(CommandLineManager::CLArgument arg_name, const char *arg)
{
	if ((arg_name == CommandLineManager::Settings
#ifdef _DEBUG
		|| arg_name == CommandLineManager::DebugSettings
#endif
		) && !PathFileExistsA(arg))
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS WindowsOpCommandLineArgumentHandler::HandlePlatform(CommandLineManager::CLArgument arg_name, CommandLineArgument *arg_value)
{
	switch (arg_name)
	{
		case CommandLineManager::ScreenHeight:
		case CommandLineManager::ScreenWidth:
#ifdef _DEBUG
		case CommandLineManager::DebugSettings:
#endif
		case CommandLineManager::MediaCenter:
		case CommandLineManager::Settings:
		case CommandLineManager::Fullscreen:
		case CommandLineManager::DelayShutdown:
		case CommandLineManager::ReinstallBrowser:
		case CommandLineManager::ReinstallMailer:
		case CommandLineManager::ReinstallNewsReader:
		case CommandLineManager::PersonalDirectory:
			//We use these later on
			break;
		case CommandLineManager::HideIconsCommand:
			//TODO: implement what is needed to remove Opera from view
			exit(0);
		case CommandLineManager::ShowIconsCommand:
			//TODO: implement what is needed to put Opera back in start menu,...
			exit(0);
		case CommandLineManager::NewWindow:
			return OpStatus::OK;
			break;

		case CommandLineManager::AddFirewallException:
		{
			uni_char exe_path[MAX_PATH];
			if(!GetModuleFileName(NULL, exe_path, MAX_PATH))
				break;
			OpStringC path_string(exe_path);
			OpStringC opera_string(UNI_L("Opera Internet Browser"));
 			WindowsOpSystemInfo::AddToWindowsFirewallException(path_string, opera_string);
			exit(0);
		}

		case CommandLineManager::CrashLog:
			g_desktop_bootstrap->DisableHWAcceleration();
			break;

		case CommandLineManager::OWIInstall:
		case CommandLineManager::OWIUninstall:
		case CommandLineManager::OWIAutoupdate:
			g_desktop_bootstrap->DisableHWAcceleration();
			// Fall through.
		case CommandLineManager::OWISilent:
		case CommandLineManager::OWIInstallFolder:
		case CommandLineManager::OWILanguage:
		case CommandLineManager::OWICopyOnly:
		case CommandLineManager::OWIAllUsers:
		case CommandLineManager::OWISingleProfile:
		case CommandLineManager::OWISetDefaultPref:
		case CommandLineManager::OWISetFixedPref:
		case CommandLineManager::OWISetDefaultBrowser:
		case CommandLineManager::OWIStartMenuShortcut:
		case CommandLineManager::OWIDesktopShortcut:
		case CommandLineManager::OWIQuickLaunchShortcut:
		case CommandLineManager::OWIPinToTaskbar:
		case CommandLineManager::OWILaunchOpera:
		case CommandLineManager::OWIGiveFolderWriteAccess:
		case CommandLineManager::OWIGiveWriteAccessTo:
		case CommandLineManager::OWIUpdateCountryCode:
		case CommandLineManager::OWICountryCode:
#if defined(_DEBUG) && defined(MEMORY_ELECTRIC_FENCE)
		case CommandLineManager::Fence:
#endif // MEMORY_ELECTRIC_FENCE
			//We use these later on
			break;

		default:
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

BOOL WindowsOpCommandLineArgumentHandler::HideHelp(CommandLineManager::CLArgument arg_name)
{
	switch (arg_name)
	{
 		case CommandLineManager::WatirTest:
 		case CommandLineManager::DebugProxy:
		case CommandLineManager::DelayShutdown:
#ifdef _DEBUG
		case CommandLineManager::DebugSettings:
#endif
		case CommandLineManager::HideIconsCommand:
		case CommandLineManager::ShowIconsCommand:
		case CommandLineManager::ReinstallBrowser:
		case CommandLineManager::ReinstallMailer:
		case CommandLineManager::ReinstallNewsReader:
		case CommandLineManager::OWIInstall:
		case CommandLineManager::OWIUninstall:
		case CommandLineManager::OWIAutoupdate:
		case CommandLineManager::OWISilent:
		case CommandLineManager::OWIInstallFolder:
		case CommandLineManager::OWILanguage:
		case CommandLineManager::OWICopyOnly:
		case CommandLineManager::OWIAllUsers:
		case CommandLineManager::OWISingleProfile:
		case CommandLineManager::OWISetDefaultPref:
		case CommandLineManager::OWISetFixedPref:
		case CommandLineManager::OWISetDefaultBrowser:
		case CommandLineManager::OWIStartMenuShortcut:
		case CommandLineManager::OWIDesktopShortcut:
		case CommandLineManager::OWIQuickLaunchShortcut:
		case CommandLineManager::OWIPinToTaskbar:
		case CommandLineManager::OWILaunchOpera:
		case CommandLineManager::OWIUpdateCountryCode:
		case CommandLineManager::OWICountryCode:
			return TRUE;
		default:
			return FALSE;
	}
}

