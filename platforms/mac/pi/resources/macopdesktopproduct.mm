/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "platforms/mac/bundledefines.h"
#include "platforms/mac/util/macutils.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL


#ifdef NO_CARBON

OP_STATUS OpDesktopProduct::GetProductInfo(OperaInitInfo*, PrefsFile*, OpDesktopProduct::ProductInfo& product_info)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[NSApplication sharedApplication];
	NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];

	product_info.m_product_type = PRODUCT_TYPE_OPERA;
	if ([[infoDict objectForKey:(NSString *)kCFBundleIdentifierKey] isEqualToString:@OPERA_NEXT_BUNDLE_ID_STRING])
	{
		product_info.m_product_type = PRODUCT_TYPE_OPERA_NEXT;
	}
	else if ([[infoDict objectForKey:(NSString *)kCFBundleIdentifierKey] hasPrefix:@OPERA_LABS_BUNDLE_ID_PREFIX_STRING])
	{
		product_info.m_product_type = PRODUCT_TYPE_OPERA_LABS;
	}
	
	product_info.m_package_type.Empty();
	
	OP_STATUS status = OpStatus::OK;
	product_info.m_labs_product_name.Empty();
	if (product_info.m_product_type == PRODUCT_TYPE_OPERA_LABS && [infoDict objectForKey:@"OperaLabsName"])
	{
		if (!SetOpString(product_info.m_labs_product_name, (CFStringRef)[infoDict objectForKey:@"OperaLabsName"]))
		{
			status = OpStatus::ERR_NO_MEMORY;
		}
	}

	[pool release];

	return status;
}

#endif // NO_CARBON
