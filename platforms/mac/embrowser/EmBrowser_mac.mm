/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "platforms/mac/util/systemcapabilities.h"

#define BOOL NSBOOL
#include <AppKit/NSApplication.h>
#include <AppKit/NSEvent.h>
#undef BOOL

bool IsEventInvokedFromMenu()
{
	return [[NSApp currentEvent] type] == NSLeftMouseUp;
}

void TearDownCocoaSpecificElements()
{
	if (GetOSVersion() >= 0x1080)
	{
		id sharing_delegate = NSClassFromString(@"SharingContentDelegate");
		if (sharing_delegate != nil && [sharing_delegate respondsToSelector:@selector(getInstance)])
			[[sharing_delegate performSelector:@selector(getInstance)] dealloc];
	}
}