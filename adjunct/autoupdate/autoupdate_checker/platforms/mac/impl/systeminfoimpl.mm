/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "../../../adaptation_layer/system_info.h"

#import <Foundation/Foundation.h>
#import <AppKit/NSRunningApplication.h>

using namespace opera_update_checker::system_info;

/* virtual */ const char* SystemInfo::GetOsVersion()
{
  NSDictionary* plistData = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
  if (plistData != nil)
  {
    NSString* versionStr = [plistData objectForKey:@"ProductVersion"];
    return [versionStr UTF8String];
  }

  //default value
  return "0.0.0";
}

/* virtual */const char* SystemInfo::GetOsName()
{
  NSDictionary* plistData = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
  if (plistData != nil)
  {
    NSString* versionStr = [plistData objectForKey:@"ProductName"];
    return [versionStr UTF8String];
  }

  return "Mac OS X";
}
