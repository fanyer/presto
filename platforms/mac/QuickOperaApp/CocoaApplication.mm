/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/QuickOperaApp/CocoaApplication.h"
#include "platforms/mac/QuickOperaApp/CocoaQuickWidgetMenu.h"
#include "platforms/mac/QuickOperaApp/OperaExternalNSMenuItem.h"
#include "platforms/mac/util/MachOCompatibility.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL
@class CocoaMenuResponder;

extern QuickWidgetMenu* gAppleQuickMenu;
extern CocoaMenuResponder* menuResponder;

Pool::Pool()
{
	pool = [[NSAutoreleasePool alloc] init];
}

Pool::~Pool()
{
	[((NSAutoreleasePool*)pool) release];
}

#ifdef WIDGET_RUNTIME_SUPPORT

BOOL IsBrowserStartup()
{
	BOOL ret = FALSE;
	NSString *executable = [[[NSBundle mainBundle] infoDictionary] valueForKey:@"CFBundleExecutable"];
	
	if (executable != nil && [executable compare:@"Opera"] == NSOrderedSame)
	{
		ret = TRUE;
	}
	return ret;
}

CFStringRef GetOperaBundleID()
{
	return (CFStringRef)[[[NSBundle mainBundle] infoDictionary] objectForKey:@"OperaBundleIdentifier"];
}

#endif // WIDGET_RUNTIME_SUPPORT


void BuildStandardMenu()
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	OperaExternalNSMenuItem *item = nil;
	[NSApplication sharedApplication];

	UniChar applemenuname[2];
	applemenuname[0] = kMenuAppleLogoFilledGlyph;
	applemenuname[1] = 0;
	gAppleQuickMenu = new QuickWidgetMenu(applemenuname, "Opera Menu", quickMenuKindApple);
	if (gAppleQuickMenu)
	{
		gAppleQuickMenu->InsertInMenubar(NULL);
	}
	NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
	NSString *appName = [infoDict valueForKey:@"CFBundleDisplayName"];
	if (appName == nil)
		appName = @"Opera";
	
	
	NSString *aboutStr = [NSString stringWithFormat:@"About %@", appName];
	NSString *hideStr = [NSString stringWithFormat:@"Hide %@", appName];
	NSString *quitStr = [NSString stringWithFormat:@"Quit %@", appName];	

	NSMenu *menu = [[[NSApp mainMenu] itemAtIndex:0] submenu];
	
	if(appName != @"Opera" && menu && ([[menu title] compare:@"Opera"]==NSOrderedSame || [[menu title] compare:[infoDict valueForKey:(NSString*)kCFBundleNameKey]]==NSOrderedSame))
	{
		[menu setTitle:appName];
	}

	// About will be added later
	item = [[OperaExternalNSMenuItem alloc] initWithTitle:aboutStr action:@selector(menuClicked:) keyEquivalent:@""];
	[item setTarget:menuResponder];
	[item setTag:kHICommandAbout];
	[menu addItem:item];
	if ([item respondsToSelector: @selector(setHidden:)])
		[item setHidden:YES];
	[item release];
	
	// Another separator
	[menu addItem:[OperaExternalNSMenuItem separatorItem]];
	
	// Preferences
	item = [[OperaExternalNSMenuItem alloc] initWithTitle:@"Preferences..." action:@selector(menuClicked:) keyEquivalent:@","];
	[item setTarget:menuResponder];
	[item setKeyEquivalentModifierMask:NSCommandKeyMask];
	[item setTag:kHICommandPreferences];
	[menu addItem:item];
	if ([item respondsToSelector: @selector(setHidden:)])
		[item setHidden:YES];	
	[item release];
	
	// Another separator
	[menu addItem:[OperaExternalNSMenuItem separatorItem]];
	
	// Service menu
	item = [[OperaExternalNSMenuItem alloc] initWithTitle:@"Services" action:NULL keyEquivalent:@""];
	NSMenu *sMenu = [[NSMenu alloc] initWithTitle:@"Services"];
	[item setSubmenu:sMenu];
	[NSApp setServicesMenu:sMenu];
	[item setTag:emOperaCommandServices];
	[menu addItem:item];
	[sMenu release];
	[item release];

	// Separator again
	[menu addItem:[OperaExternalNSMenuItem separatorItem]];
	
	// Hide opera click
	item = [[OperaExternalNSMenuItem alloc] initWithTitle:hideStr action:@selector(hide:) keyEquivalent:@"h"];
	[item setTarget:NSApp];
	[item setKeyEquivalentModifierMask:NSCommandKeyMask];
	[item setTag:kHICommandHide];
	[menu addItem:item];
	[item release];	

	// Hide others
	item = [[OperaExternalNSMenuItem alloc] initWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[item setTarget:NSApp];
	[item setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
	[item setTag:kHICommandHideOthers];
	[menu addItem:item];
	[item release];	
	
	// Add show all
	item = [[OperaExternalNSMenuItem alloc] initWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
	[item setTarget:NSApp];
	[item setTag:kHICommandShowAll];
	[menu addItem:item];
	[item release];	
	
	// Separator...
	[menu addItem:[OperaExternalNSMenuItem separatorItem]];
	
	// And application quit
 	item = [[OperaExternalNSMenuItem alloc] initWithTitle:quitStr action:@selector(terminate:) keyEquivalent:@"q"];
	[item setTarget:NSApp];
	[item setKeyEquivalentModifierMask:NSCommandKeyMask];
	[item setTag:kHICommandQuit];
	[menu addItem:item];
	[item release];

	[pool release];
}

void ApplicationRun()
{
	// Just before run, we will build the menus
	[NSApp run];
}

void ApplicationTerminate()
{
	// Release the windows menu
	id menu = [[NSApplication sharedApplication] windowsMenu];
	[[NSApplication sharedApplication] setWindowsMenu:nil];
	[menu release];

	// We need to stop and not terminate so all the cleanup is done
	[[NSApplication sharedApplication] stop:0];
}

void ApplicationActivate()
{
	[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

