/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include <sys/wait.h>
#include <dirent.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

#include "platforms/mac/QuickOperaApp/macauinstaller.h"
#include "platforms/mac/QuickOperaApp/QuickWidgetUnicodeUtils.h"
#include "platforms/mac/pi/desktop/mac_launch_pi.h"
#include "platforms/mac/bundledefines.h"
#include "adjunct/desktop_util/string/string_convert.h"
#include "adjunct/autoupdate/updater/austringutils.h"
#include "adjunct/desktop_util/string/OpBasicString.h"

#ifdef NO_CARBON
#define BOOL NSBOOL
#import <AppKit/NSAlert.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSString.h>
#import <Foundation/NSAutoreleasePool.h>
#undef BOOL
#endif // NO_CARBON

/////////////////////////////////////////////////////////////////////////////////////////

#define AUTO_UPDATE_MAC_UNZIP_CMD			"unzip -o \"%s\" -d \"%s\""

#define AUTO_UPDATE_MAC_MOVE_CMD			"/bin/mv"
#define AUTO_UPDATE_MAC_MOVE_FORCE_CMD		"/bin/mv -f \"%s\" \"%s\""
#define AUTO_UPDATE_MAC_MOVE_ARGS			"-f"

#define AUTO_UPDATE_MAC_CHOWN_CMD			"/usr/sbin/chown"
#define AUTO_UPDATE_MAC_CHOWN_ARGS			"-fR"

#define AUTO_UPDATE_MAC_CHMOD_CMD			"/bin/chmod"
#define AUTO_UPDATE_MAC_CHMOD_ARG1			"-R" 
#define AUTO_UPDATE_MAC_CHMOD_ARG2			"g+w"

#define AUTO_UPDATE_MAC_DELETE_CMD			"/bin/rm"
#define AUTO_UPDATE_MAC_DELETE_ARGS			"-fR"

#define AUTO_UPDATE_MAC_GROUP				":admin"

#define AUTO_UPDATE_MAC_OPERA_BACKUP_NAME	"Opera1.app"

/////////////////////////////////////////////////////////////////////////////////////////

AUInstaller* AUInstaller::Create()
{
	// Can't use OP_NEW as this is used outside of core
	return new MacAUInstaller();
}

//////////////////////////////////////////////////////////////////////////////////////////

AUInstaller::AUI_Status MacAUInstaller::Install(uni_char *install_path, uni_char *package_file)
{
	// Get the package string into an OpBasicString8 to save memory cleanup
	char *package = StringConvert::UTF8FromUTF16(package_file);
	OpBasicString8 package_full_path(package);
	OP_DELETEA(package);

	if (!package_full_path.CStr())
		return AUInstaller::ERR_MEM;

	// Save the path to the upgrade folder
	OpBasicString8 upgrade_folder_path(package_full_path.CStr());

	// upgrade_folder_path now has the path to the upgrade and package
	if (!upgrade_folder_path.DeleteAfterLastOf('/'))
		return AUInstaller::ERR;

	AUInstaller::AUI_Status err;

	// First thing we are going to do is unzip the zip package that was sent us
	if ((err = RunCmd(AUTO_UPDATE_MAC_UNZIP_CMD, package_full_path.CStr(), upgrade_folder_path.CStr())) != AUInstaller::OK)
		return err;
	
	OpBasicString8 unzipped_opera_app_name("");
	
	// Find the actual name of the Opera/Opera Next/Opera Labs build that was just unzipped onto disk
	struct dirent *dp;
	DIR *dir = opendir(upgrade_folder_path.CStr());
	while ((dp=readdir(dir)) != NULL) {
		OpBasicString8 filepath(upgrade_folder_path.CStr());
		filepath.Append(dp->d_name);
		
		int length = strlen(dp->d_name);
		if (length && strstr(dp->d_name, "Opera") && (dp->d_name[0] == 'O' || dp->d_name[0] == 'o') &&
			!strcasecmp(&dp->d_name[length - 4], ".app"))
		{
			unzipped_opera_app_name.Append(upgrade_folder_path.CStr());
			unzipped_opera_app_name.Append(dp->d_name);
			break;
		}
	}
	closedir(dir);
	
	if (strlen(unzipped_opera_app_name.CStr()) == 0)
		return AUInstaller::ERR;

	// We'll move the Original to the upgrade folder as a backup then we'll move the new version
	// to the correct folder as the installation. Opera itself can clean up all the mess in the
	// upgrade folder once it knows everything has worked

	// Make the 2 strings we need
	OpBasicString8 upgrade_folder_opera(unzipped_opera_app_name.CStr());
	OpBasicString8 upgrade_folder_opera_backup(upgrade_folder_path.CStr());

	if (!upgrade_folder_opera_backup.Append(AUTO_UPDATE_MAC_OPERA_BACKUP_NAME))
		return AUInstaller::ERR;

	// Get the install string into an OpBasicString8 to save memory cleanup
	char *install = StringConvert::UTF8FromUTF16(install_path);
	OpBasicString8 install_full_path(install);
	OP_DELETEA(install);

	if (!install_full_path.CStr())
		return AUInstaller::ERR_MEM;

	// Check signature of executable
	MacLaunchPI launch_pi;
	uni_char* executable = StringConvert::UTF16FromUTF8(upgrade_folder_opera.CStr());
	BOOL ok = launch_pi.VerifyExecutable(executable);
	delete [] executable;
	if(!ok)
	{
		return AUInstaller::ERR;
	}

	// Check if user has read and write privileges
	BOOL has_privileges = HasInstallPrivileges(install_path);

	if (has_privileges)
	{
		// Move the installed Opera to a backup in the upgrade folder
		if (RunCmd(AUTO_UPDATE_MAC_MOVE_FORCE_CMD, install_full_path.CStr(), upgrade_folder_opera_backup.CStr()) != AUInstaller::OK)
		{
			// If moving it out fails for any reason, assume it failed because a file inside didn't have privileges
			// Ask for authorization and try again
			has_privileges = FALSE;
		}
		else
		{
			// Move the new Opera to the installation folder
			if (RunCmd(AUTO_UPDATE_MAC_MOVE_FORCE_CMD, upgrade_folder_opera.CStr(), install_full_path.CStr()) != AUInstaller::OK)
			{
				// Move back the backup since the move failed
				RunCmd(AUTO_UPDATE_MAC_MOVE_FORCE_CMD, upgrade_folder_opera_backup.CStr(), install_full_path.CStr());
				return AUInstaller::ERR;
			}
			// Installation was a success
			return AUInstaller::OK;
		}
	}

	// We need to lift the system privileges here for those who cannot write to where the Opera.app is
	AuthorizationRef		authRef;
	AuthorizationItem		right =  { OPERA_UPDATE_BUNDLE_ID_STRING, 0, NULL, 0 };
	AuthorizationRights		rights = { 1, &right };
	OSStatus				status;
	AuthorizationFlags		flags = kAuthorizationFlagDefaults |
									kAuthorizationFlagInteractionAllowed |
									kAuthorizationFlagExtendRights;
	AuthorizationItemSet	*authinfo;

	// Create the authorisation object
	status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authRef);
	if (status != errAuthorizationSuccess)
		return AUInstaller::NO_ACCESS;

	// make user authenticate as an administrator
	status = AuthorizationCopyRights(authRef, &rights, kAuthorizationEmptyEnvironment, flags, NULL);
	if (status != errAuthorizationSuccess)
		return AUInstaller::NO_ACCESS;

	// Get the username from the current authorisation object so we can set the correct owner
	status = AuthorizationCopyInfo(authRef, kAuthorizationEnvironmentUsername,  &authinfo);
	if (status != errAuthorizationSuccess && authinfo->count == 1)
	{
		AuthorizationFreeItemSet(authinfo);
		return AUInstaller::NO_ACCESS;
	}

	// Grab the username
	OpBasicString8 chmod_user_group((const char *)authinfo->items[0].value);

	// Free the authorisation info as soon as possible
	AuthorizationFreeItemSet(authinfo);

	// Add on the group
	if (!chmod_user_group.Append(AUTO_UPDATE_MAC_GROUP))
	{
		AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
		return AUInstaller::ERR;
	}

	// Move the installed Opera to a backup in the upgrade folder
	if (RunPrivilegedCmd(authRef, AUTO_UPDATE_MAC_MOVE_CMD, AUTO_UPDATE_MAC_MOVE_ARGS, install_full_path.CStr(), upgrade_folder_opera_backup.CStr()) != AUInstaller::OK)
	{
		AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
		return AUInstaller::ERR;
	}

	// Move the new Opera to the installation folder
	if (RunPrivilegedCmd(authRef, AUTO_UPDATE_MAC_MOVE_CMD, AUTO_UPDATE_MAC_MOVE_ARGS, upgrade_folder_opera.CStr(), install_full_path.CStr()) != AUInstaller::OK)
	{
		// Move back the backup since the move failed
		RunPrivilegedCmd(authRef, AUTO_UPDATE_MAC_MOVE_CMD, AUTO_UPDATE_MAC_MOVE_ARGS, upgrade_folder_opera_backup.CStr(), install_full_path.CStr());
		AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
		return AUInstaller::ERR;
	}

	// Set the correct permissions on the file
	if (RunPrivilegedCmd(authRef, AUTO_UPDATE_MAC_CHOWN_CMD, AUTO_UPDATE_MAC_CHOWN_ARGS, chmod_user_group.CStr(), install_full_path.CStr()) != AUInstaller::OK)
	{
		AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
		return AUInstaller::ERR;
	}

	// Set the correct permissions on the file
	if (RunPrivilegedCmd(authRef, AUTO_UPDATE_MAC_CHMOD_CMD, AUTO_UPDATE_MAC_CHMOD_ARG1, AUTO_UPDATE_MAC_CHMOD_ARG2, install_full_path.CStr()) != AUInstaller::OK)
	{
		AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
		return AUInstaller::ERR;
	}

	// Since this user has no permissions we'll have to delete the original Opera installations otherwise it will never be deleted
	RunPrivilegedCmd(authRef, AUTO_UPDATE_MAC_DELETE_CMD, AUTO_UPDATE_MAC_DELETE_ARGS, upgrade_folder_opera_backup.CStr());

	// Free the authorisation object
	AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);

	return AUInstaller::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////

BOOL MacAUInstaller::HasInstallPrivileges(uni_char *install_path)
{
	char* path = StringConvert::UTF8FromUTF16(install_path);

	// check if app folder has access
	int ret = access(path, R_OK | W_OK);
	if (ret == 0)
	{
		// yes, the app folder has access, but the parent folder also needs access
		char* sep = strrchr(path, '/');
		if (sep)
		{
			*sep = 0;
			ret = access(path, R_OK | W_OK);
		}
	}

	delete [] path;

	if(ret == 0)
		return TRUE;
	else
		return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////

BOOL MacAUInstaller::ShowStartDialog(uni_char *caption, uni_char *message, uni_char *yes_text, uni_char *no_text)
{
#ifdef NO_CARBON
	[NSApplication sharedApplication];

	NSAutoreleasePool *pool = [NSAutoreleasePool new];

	NSAlert *dialog = [[[NSAlert alloc] init] autorelease];
	int selected_button;

	// Set the caption and the message
	[dialog setMessageText:[NSString stringWithCharacters:(const unichar*)caption length:UniStrLen((const UniChar *)caption)]];
	[dialog setInformativeText:[NSString stringWithCharacters:(const unichar*)message length:UniStrLen((const UniChar *)message)]];
	
	// Add the yes button
	[dialog addButtonWithTitle:[NSString stringWithCharacters:(const unichar*)yes_text length:UniStrLen((const UniChar *)yes_text)]];

	// Add the No button and change the key to ESC
	NSButton *no_button  = [dialog addButtonWithTitle:[NSString stringWithCharacters:(const unichar*)no_text length:UniStrLen((const UniChar *)no_text)]];
	[no_button setKeyEquivalent:@"\033"]; 
	
	// Set the Yes button as the first responder
	[[dialog window] makeFirstResponder:[[dialog buttons] objectAtIndex:0]];
	
	// Set to a warning style
	[dialog setAlertStyle: NSCriticalAlertStyle];

	// Launch!
	selected_button = [dialog runModal];
	
	[pool release];

	// Check if Yes was pressed
	if (selected_button == NSAlertFirstButtonReturn)
		return TRUE;
	
	return FALSE;
#else	
	AlertStdCFStringAlertParamRec alertParams =
		{
			kStdCFStringAlertVersionOne,
			false,
			false,
			CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)yes_text, UniStrLen((const UniChar *)yes_text)),
			CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)no_text, UniStrLen((const UniChar *)no_text)),
			nil,
			kAlertStdAlertOKButton,
			kAlertStdAlertCancelButton,
			kWindowDefaultPosition,
			0
		};

	CFStringRef error = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)caption, UniStrLen((const UniChar *)caption));
	CFStringRef explanation = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)message, UniStrLen((const UniChar *)message));
	DialogRef dialog = NULL;
	SInt16 selectedButton = kAlertStdAlertCancelButton;

	OSStatus err = CreateStandardAlert(kAlertCautionAlert, error, explanation, &alertParams, &dialog);
	if (err == noErr && dialog) {
		RunStandardAlert(dialog, NULL, &selectedButton);
	}

	CFRelease(error);
	CFRelease(explanation);
	CFRelease(alertParams.defaultText);
	CFRelease(alertParams.cancelText);

	if (selectedButton == kAlertStdAlertOKButton)
		return TRUE;
	else
		return FALSE;
#endif // NO_CARBON
}

//////////////////////////////////////////////////////////////////////////////////////////

AUInstaller::AUI_Status MacAUInstaller::RunCmd(const char *cmd_format, const char *src, const char *dest)
{
	// Just make sure stuff was passed in is valid
	if (!cmd_format || !src || !dest)
		return AUInstaller::ERR;

	// Allocate the memory for the command
	char *cmd = OP_NEWA(char, strlen(cmd_format) + strlen(src) + strlen(dest) + 1);
	if (cmd == NULL)
		return AUInstaller::ERR_MEM;

	// Build the command string
	if (sprintf(cmd, cmd_format, src, dest) <= 0)
	{
		OP_DELETEA(cmd);
		return AUInstaller::ERR;
	}

	//printf(cmd);
	// Call the command
	int ret = system(cmd);

	OP_DELETEA(cmd);

	// Check the move command worked!
	if (ret != 0)
		return AUInstaller::ERR;

	return AUInstaller::OK;
}


//////////////////////////////////////////////////////////////////////////////////////////

AUInstaller::AUI_Status MacAUInstaller::RunPrivilegedCmd(AuthorizationRef authRef, const char *cmd, const char *args, const char *src, const char *dest)
{
	// Just make sure stuff was passed in is valid
	if (!cmd || !args)
		return AUInstaller::ERR;

	FILE*	pipe = NULL;
	char*	argv[4];
    int		wait_status;
    int		pid;

	// Assign the arguments
	memset(argv, NULL, sizeof(argv));
	argv[0] = const_cast<char *>(args);
	if (src)
		argv[1] = const_cast<char *>(src);
	if (src)
		argv[2] = const_cast<char *>(dest);

	OSStatus status =  AuthorizationExecuteWithPrivileges(authRef, cmd, kAuthorizationFlagDefaults, argv, &pipe);
	if (status != errAuthorizationSuccess)
		return AUInstaller::NO_ACCESS;

	// Wait for the command to finish
	pid = wait(&wait_status);

	return AUInstaller::OK;
}


//////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

