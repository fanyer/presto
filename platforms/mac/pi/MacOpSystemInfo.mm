/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpSystemInfo.h"
#include "platforms/mac/model/OperaNSWindow.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "platforms/mac/util/systemcapabilities.h"
#include <pwd.h>

#define BOOL NSBOOL
#import <Foundation/NSUserDefaults.h>
#import <AppKit/AppKit.h>
#undef BOOL

#ifdef DPI_CAP_DESKTOP_SYSTEMINFO

#ifdef NO_CARBON

int MacOpSystemInfo::GetDoubleClickInterval()
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    float doubleClickTime = [defaults floatForKey:@"com.apple.mouse.doubleClickThreshold"];
	if (doubleClickTime == 0.0) doubleClickTime = 1.0;
	return doubleClickTime * 1000;
}

BOOL MacOpSystemInfo::IsForceTabMode()
{
	return !KioskManager::GetInstance()->GetEnabled() &&
		![OperaNSWindow systemFullscreenWindow];
}

#endif // NO_CARBON

BOOL MacOpSystemInfo::IsSandboxed()
{
	static BOOL is_sandboxed = strcmp(getpwuid(getuid())->pw_dir, [NSHomeDirectory() UTF8String]) != 0;
	return is_sandboxed;
}

BOOL MacOpSystemInfo::SupportsContentSharing()
{
	return GetOSVersion() >= 0x1080;
}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
INT32 MacOpSystemInfo::GetMaximumHiresScaleFactor()
{
	CGFloat max_scale = 1.0;

	if (GetOSVersion() >= 0x1070)
	{
		NSArray* screens = [NSScreen screens];
		for (unsigned int i = 0; i < [screens count]; i++)
		{
			NSScreen* screen = [screens objectAtIndex:i];
			CGFloat scale = [screen backingScaleFactor];
			if (scale > max_scale)
				max_scale = scale;
		}
	}

	return max_scale * 100;
}
#endif // PIXEL_SCALE_RENDERING_SUPPORT

OpFileFolder MacOpSystemInfo::GetDefaultSpeeddialPictureFolder()
{
	return IsSandboxed() ? OPFILE_USERPREFS_FOLDER : OPFILE_PICTURES_FOLDER;
}

#endif // DPI_CAP_DESKTOP_SYSTEMINFO
