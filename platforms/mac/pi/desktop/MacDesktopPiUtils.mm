/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/systemcapabilities.h"

#define BOOL NSBOOL
#import <Foundation/NSString.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/AppKit.h>
#undef BOOL

OP_STATUS DesktopPiUtil::SetDesktopBackgroundImage(URL& url)
{
	if (GetOSVersion() < 0x1060) {
		return SetBackgroundUsingAppleEvents(url);
	}
	OpString strurl;
	RETURN_IF_ERROR(url.GetAttribute(URL::KFilePathName_L, strurl));
	RETURN_IF_ERROR(strurl.Insert(0, "file://", 7));

	NSError* err;
	NSString* s = [NSString stringWithCharacters:(const unichar*)strurl.CStr() length:strurl.Length()];
	[s retain];
	s = [s stringByAddingPercentEscapesUsingEncoding:[NSString defaultCStringEncoding]];
	NSURL* imgurl = [[NSURL alloc] initWithString:s];
	[imgurl retain];

	NSDictionary* options = [[[NSWorkspace sharedWorkspace] desktopImageOptionsForScreen:[NSScreen mainScreen]] mutableCopy];

	if (NO == [[NSWorkspace sharedWorkspace] setDesktopImageURL:imgurl forScreen:[[NSApp keyWindow] screen] options:options error: &err])
	{
		NSLog(@"Error: %s", [[err localizedDescription] UTF8String]);
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS DesktopPiUtil::CreateIconBitmapForFile(OpBitmap **bitmap, const uni_char* icon_path)
{
	*bitmap = NULL;//GetAttachmentIconOpBitmap(icon_path, FALSE);
	if (*bitmap)
		return OpStatus::OK;
	else
		return OpStatus::ERR;
}
