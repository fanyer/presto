/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef NO_CORE_COMPONENTS

#include "platforms/mac/pluginwrapper/PluginWrapperNSApplicationDelegate.h"
#include "platforms/mac/pi/plugins/MacOpPluginTranslator.h"
#include "platforms/mac/util/macutils_mincore.h"

#define BOOL NSBOOL
#import <AppKit/NSApplication.h>
#import <Appkit/NSWindow.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#undef BOOL

@interface PluginWrapperNSApplicationDelegate : NSObject
{
	int old_num_windows;
}
@end

@implementation PluginWrapperNSApplicationDelegate

- (id)init
{
	self = [super init];

	old_num_windows = 0;

	return self;
}

- (void)applicationDidUpdate:(NSNotification *)notification
{
	NSArray *appWindows = [[NSApplication sharedApplication] windows];
	bool visibleWindows = false;
	int num_windows = 0;
	
	if (appWindows)
	{
		// Try and find a visible window in the PluginWrapper
		for (unsigned long i = 0; i < (unsigned long)[appWindows count]; i++)
		{
			NSWindow *win = [appWindows objectAtIndex:i];
			
			if ([win isVisible])
			{
				visibleWindows = true;
				num_windows++;
			}
		}
	}
	
	// Only send this message below when the number of windows changes!!!
	// This fixes all the strange focus problems like bugs DSK-360204, DSK-359996
	// The super plugin focus testcase is: http://t/imported/karolp/mac_focus_flash/focus.html to
	// check what you have broken after you play with this code
	if (old_num_windows != num_windows)
	{
		old_num_windows = num_windows;

		// Find the currently active MacOpPluginTranslator and send the message via it's channel
		MacOpPluginTranslator *translator = MacOpPluginTranslator::GetActivePluginTranslator();
		if (translator)
			translator->SendBrowserWindowShown(visibleWindows);
	}
}

@end


PluginApplicationDelegate::PluginApplicationDelegate()
{
	OperaNSReleasePool pool;

	[[NSApplication sharedApplication] setDelegate:[[PluginWrapperNSApplicationDelegate alloc] init]];
}

PluginApplicationDelegate::~PluginApplicationDelegate()
{
	OperaNSReleasePool pool;

	id delegate = [[NSApplication sharedApplication] delegate];
	[[NSApplication sharedApplication] setDelegate:nil];
	[delegate release];
}


#endif // NO_CORE_COMPONENTS
