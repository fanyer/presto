/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "../includes/globalstorageimpl.h"

#import <Foundation/Foundation.h>

const char* OAUCGlobalStorageImpl::GetPrefsFile()
{
  NSString* path = [NSSearchPathForDirectoriesInDomains(
                      NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];

  NSString *prefsPath = [NSString stringWithFormat:@"%s/%s/%s", [path UTF8String], opera_product_type, OAUC_GLOBAL_STORAGE_FILE];
  return [prefsPath UTF8String];
}

const char* OAUCGlobalStorageImpl::GetPrefsFolder()
{
  NSString* path = [NSSearchPathForDirectoriesInDomains(
                      NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];

  NSString *prefsPath = [NSString stringWithFormat:@"%s/%s", [path UTF8String], opera_product_type];
  return [prefsPath UTF8String];
}

/* virtual */ opera_update_checker::status::Status OAUCGlobalStorageImpl::SetData(const char *key, const char* data, unsigned long data_len)
{
  if (!opera_product_type)
  {
    return opera_update_checker::status::StatusCode::FAILED;
  }
  if (key == NULL)
  {
    return opera_update_checker::status::StatusCode::FAILED;
  }

  NSString* key_ = [NSString stringWithFormat:@"%s", key];
  NSString* data_ = [NSString stringWithFormat:@"%s", data];
  NSString* prefsFile = [NSString stringWithFormat:@"%s", GetPrefsFile()];

  NSMutableDictionary* plistData = [NSMutableDictionary dictionaryWithContentsOfFile:prefsFile];
  if (plistData == nil)
  {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString* prefsFolder = [NSString stringWithFormat:@"%s", GetPrefsFolder()];
    if (![fileManager fileExistsAtPath:prefsFolder])
    {
      //Create prefs folder
      if ([fileManager createDirectoryAtPath:prefsFolder withIntermediateDirectories:YES attributes:nil error:nil] == NO)
      {
        return opera_update_checker::status::StatusCode::FAILED;
      }
    }
    //dont save anything
    if (data == NULLPTR)
    {
      return opera_update_checker::status::StatusCode::OK;
    }
    //Create plist file if does not exists, and fill key/data
    plistData = [[NSMutableDictionary alloc] initWithObjectsAndKeys:data_, key_ , nil];
    if (plistData == nil)
    {
      return opera_update_checker::status::StatusCode::FAILED;
    }
  }
  else
  {
    //Edit data for key
    if (data != NULLPTR)
    {
      [plistData setObject:data_ forKey:key_];
    }
    else
    {
      //remove key
      [plistData removeObjectForKey:key_];
    }
  }

  [plistData writeToFile:prefsFile atomically:YES];

  return opera_update_checker::status::StatusCode::OK;
}

/* virtual */ opera_update_checker::status::Status OAUCGlobalStorageImpl::GetData(const char *key, const char*& data, unsigned long& data_len)
{
  data_len = 0;
  if (!opera_product_type)
  {
    return opera_update_checker::status::StatusCode::FAILED;
  }
  NSString* prefsFile = [NSString stringWithFormat:@"%s", GetPrefsFile()];
  NSDictionary* plistData = [NSDictionary dictionaryWithContentsOfFile:prefsFile];
  if (plistData == nil)
  {
    data_len = 0;
    data = NULLPTR;
    return opera_update_checker::status::StatusCode::OK;
  }

  NSString* key_ = [NSString stringWithFormat:@"%s", key];
  NSString* value = [plistData objectForKey:key_];
  if (value != nil)
  {
    data_len = [value length] + 1;
  }
  else
  {
    data = NULLPTR;
    data_len = 0;
    return opera_update_checker::status::StatusCode::OK;
  }

  char* ret_val = OAUC_NEWA(char, data_len);
  memset(ret_val, 0, data_len);
  strcpy(ret_val, [value UTF8String]);
  data = ret_val;

  return opera_update_checker::status::StatusCode::OK;
}

void OAUCGlobalStorageImpl::SetOperaProductType(const char* product_type)
{
  memset(opera_product_type, 0, MAX_OAUC_PRODUCT_LEN);
  strncpy(opera_product_type, product_type, MAX_OAUC_PRODUCT_LEN);
}

