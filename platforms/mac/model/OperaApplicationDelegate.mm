/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/OperaApplicationDelegate.h"
#include "platforms/mac/model/CocoaAppleEventHandler.h"
#include "platforms/mac/quick_support/CocoaInternalQuickMenu.h"
#include "platforms/mac/util/macutils.h"

#include "adjunct/quick/Application.h"
#include "adjunct/desktop_util/actions/delayed_action.h"

#define BOOL NSBOOL
#import <Foundation/NSArray.h>
#import <AppKit/NSApplication.h>
#undef BOOL

extern CocoaInternalQuickMenu* gDockMenu;

#ifdef WIDGET_RUNTIME_SUPPORT	
#include "adjunct/widgetruntime/GadgetStartup.h"
#endif //WIDGET_RUNTIME_SUPPORT

@interface OperaNSApplicationDelegate : NSObject <NSApplicationDelegate>
{
	CocoaAppleEventHandler *cocoa_apple_event_handler;
}
@end

@implementation OperaNSApplicationDelegate
- (NSBOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(NSBOOL)flag
{
	return YES;
}

- (NSBOOL)application:(NSApplication *)sender openFile:(NSString *)filename
{
#ifdef WIDGET_RUNTIME_SUPPORT	
	
	if (GadgetStartup::IsGadgetStartupRequested())
	{
		return NO;
	}
#endif //WIDGET_RUNTIME_SUPPORT	
	OpString str;
	SetOpString(str, (CFStringRef)filename);
	if (g_application->IsBrowserStarted())
		g_application->GoToPage(str, TRUE);
	return YES;
}
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
	unsigned int i;
	for (i=0; i<[filenames count]; i++)
		[self application:sender openFile:[filenames objectAtIndex:i]];
}
#endif

- (void)applicationDidHide:(NSNotification *)notification
{
}

- (void)applicationDidUnhide:(NSNotification *)notification
{
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
	cocoa_apple_event_handler = new CocoaAppleEventHandler();
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	delete cocoa_apple_event_handler;	
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	[NSApp stop:self];
	return NSTerminateCancel;
}

- (NSMenu *)applicationDockMenu:(NSApplication *)sender 
{ 
	if (gDockMenu)
	{	
		gDockMenu->RebuildMenu();
		return (NSMenu *)gDockMenu->GetMenu();
	}
	return nil;
} 

@end

OperaApplicationDelegate::OperaApplicationDelegate()
{
	[[NSApplication sharedApplication] setDelegate:[[OperaNSApplicationDelegate alloc] init]];
}

OperaApplicationDelegate::~OperaApplicationDelegate()
{
	id delegate = [[NSApplication sharedApplication] delegate];
	[[NSApplication sharedApplication] setDelegate:nil];
	[delegate release];
}

