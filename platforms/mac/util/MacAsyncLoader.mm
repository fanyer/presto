/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/MacAsyncLoader.h"
#include "platforms/mac/util/systemcapabilities.h"

#define BOOL NSBOOL
#import <Foundation/Foundation.h>
#import <Foundation/NSString.h>
#undef BOOL

// static
void* MacAsyncLoader::LoadElements(void*) {
	// TODO(bprzybylski):
	// this autorelease pool should be replaced with
	// @autoreleasepool { } keyword however our
	// current compiler (GCC4.2) doesn't support
	// that keyword. The reason for using
	// @autoreleasepool instead of NSAutoreleasePool
	// is that @autoreleasepool is working in
	// all environments: ref counting, GC and ARC
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	LoadSharingServices();
	[pool release];
	return NULL;
}

// static
void MacAsyncLoader::LoadSharingServices() {
	if (GetOSVersion() >= 0x1080) {
		id sharing_service = NSClassFromString(@"NSSharingService");
		if (sharing_service != nil && [sharing_service respondsToSelector:@selector(sharingServiceNamed:)]) {
			[sharing_service performSelector:@selector(sharingServiceNamed:) withObject:@"com.apple.share.Twitter.post"];
			if ([sharing_service respondsToSelector:@selector(canPerformWithItems:)])
				[sharing_service performSelector:@selector(canPerformWithItems:) withObject: nil];
		}
	}
}
