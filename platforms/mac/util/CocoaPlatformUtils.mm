/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 */
#include "core/pch.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL
#include "platforms/mac/util/CocoaPlatformUtils.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/util/systemcapabilities.h"

BOOL CocoaPlatformUtils::IsGadgetInstallerStartup()
{
	BOOL ret = FALSE;
	NSString *executable = [[[NSBundle mainBundle] infoDictionary] valueForKey:@"CFBundleExecutable"];
	
	if ([executable compare:@"WidgetInstaller"] == NSOrderedSame)
	{
		ret = TRUE;
	}
	
	return ret;
}

OP_STATUS CocoaPlatformUtils::BounceDockIcon()
{
	[[NSApplication sharedApplication] requestUserAttention:NSInformationalRequest];
	
	return OpStatus::OK;
}

OP_STATUS CocoaPlatformUtils::HasAdminAccess(BOOL &has_admin_access)
{
	has_admin_access = FALSE;
	OpString application_path;
	
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	OP_STATUS err = OpFileUtils::FindFolder(kApplicationsFolderType, application_path);
	if (OpStatus::IsError(err))
	{
		[pool release];
		return err;
	}
	NSString *app_path = [NSString stringWithCharacters:(const UniChar *)application_path.CStr() length:application_path.Length()];
	NSFileManager *mgr = [NSFileManager defaultManager];
	
	if ([mgr isWritableFileAtPath:app_path])
	{
		has_admin_access = TRUE;
	}
	[pool release];
	
	return OpStatus::OK;
	
	
}

#ifdef NO_CARBON
BOOL MacPlaySound(const uni_char* fullfilepath, BOOL async)
{
	NSString* str = [NSString stringWithCharacters:(const unichar*)fullfilepath length:uni_strlen(fullfilepath)];
	NSSound* snd = [[NSSound alloc] initWithContentsOfFile:str byReference:FALSE];
	[snd play];
	[snd release];
	return TRUE;
}
#endif // NO_CARBON

