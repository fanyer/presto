/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

//#include "core/pch.h"
#include "platforms/mac/system.h"

#include "platforms/mac/util/systemcapabilities.h"

#define BOOL NSBOOL
#import <Foundation/Foundation.h>
#undef BOOL

#ifdef __cplusplus
extern "C" {
#endif

SInt32	gMacOSVersion;
SInt32	g_major, g_minor, g_bugfix;

SInt32	GetOSVersion()
{
	return gMacOSVersion;
}

void GetDetailedOSVersion(int &major, int &minor, int &bugfix)
{
	major = g_major;
	minor = g_minor;
	bugfix = g_bugfix;
}

void InitOperatingSystemVersion()
{
	NSDictionary* plistData = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
	if (plistData != nil) {
		NSString* versionStr = [plistData objectForKey:@"ProductVersion"];
		NSArray* verArr = [versionStr componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"."]];
		SInt32* ver_arr[] = {&g_major, &g_minor, &g_bugfix};

		for (NSUInteger i = 0; i < [verArr count] && i < ARRAY_SIZE(ver_arr); ++i)
			*ver_arr[i] = [[verArr objectAtIndex:i] integerValue];
	}
	SInt32 major_high = g_major/10, major_low = g_major % 10;

	gMacOSVersion = ((major_high << 12) | (major_low << 8) | (MIN(g_minor, 15) << 4) | MIN(g_bugfix, 15));
}

#ifdef __cplusplus
}
#endif
