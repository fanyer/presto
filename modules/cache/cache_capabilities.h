/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CACHE_CAPABILITIES_H
#define CACHE_CAPABILITIES_H

#define CACHE_MODULE_NAME cache
#define CACHE_MODULE_VERSION 1
#define CACHE_MODULE_FINAL_VERSION 1

// The Cache module is included
#define CACHE_CAP_MODULE

// This module has datadescriptors that inludes information about content-type and charset, with possibility of overriding without affecting parent URL
#define URL_CAP_DESCRIPTOR_HAS_CONTENTTYPE

// This module permits Local_File_Storage to be created with a folder location
#define CACHE_CAP_LOCAL_FILE_FOLDER

// This module can return a specific URL_Rep object based on it's ID
#define CACHE_CAP_LOCATE_URL

// This module supports URL_DataDescriptor::GetFirstInvalidCharacterOffset().
#define CACHE_CAP_FIRST_INVALID_CHARACTER_OFFSET

// This module contain the CacheStorage::StoreDataEncode function
#define CACHE_CAP_STORE_ENCODE

// This module contains the Cache_Storage::SetIgnoreWarnings function
#define CACHE_CAP_HAVE_IGNORE_WARNINGS

// This module contains the URL_DataDescriptor::SetPosition function
#define CACHE_CAP_SET_POSITION

// This module can independently adjust the Cache Size of URL contexts
#define CACHE_CAP_HAVE_CONTEXT_CACHE_SIZE

// This module supports the generational cache (embedded files, directories and containers)
#define CACHE_CAP_GENERATIONAL

// The cache define the CACHE_VIEW_SOURCE_FOLDER constant
#define CACHE_CAP_VIEW_SOURCE_FOLDER

// Support for "plain files", that for example are not stored on containers or embedded in the index
#define CACHE_CAP_PLAIN_FILES

// Support for dynamic attributes
#define CACHE_CAP_DYNAMIC_ATTRIBUTES2

// URL_DataDescriptor::SetPosition has a return value
#define CACHE_CAP_SET_POSITION_RETVAL

// Cache_Storage::AccessReadOnly() is defined
#define ACCESS_READ_ONLY

#endif // CACHE_CAPABILITIES_H
