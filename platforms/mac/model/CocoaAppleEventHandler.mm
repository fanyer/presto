/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/CocoaAppleEventHandler.h"
#include "platforms/mac/model/AppleEventManager.h"

#include "modules/inputmanager/inputmanager.h"

#define BOOL NSBOOL
#import <Foundation/NSAppleEventManager.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <AppKit/AppKit.h>
#undef BOOL

#include "adjunct/widgetruntime/GadgetStartup.h"

@interface CocoaAppleEventHandlerInternal : NSObject
{
}
- (void)handleReopenAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;
@end

@implementation CocoaAppleEventHandlerInternal
- (id)init
{
	NSAppleEventManager *appleEventManager = [NSAppleEventManager sharedAppleEventManager];
	self = [super init];

	[appleEventManager setEventHandler:self andSelector:@selector(handleQuitAppEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEQuitApplication];
	[appleEventManager setEventHandler:self andSelector:@selector(handleReopenAppEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEReopenApplication];
	[appleEventManager setEventHandler:self andSelector:@selector(handleOpenAppEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEOpen];
	[appleEventManager setEventHandler:self andSelector:@selector(handlePrintAppEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEPrint];
	[appleEventManager setEventHandler:self andSelector:@selector(handleCreateElmAppEvent:withReplyEvent:) forEventClass:kAECoreSuite andEventID:kAECreateElement];
	[appleEventManager setEventHandler:self andSelector:@selector(handleCloseAppEvent:withReplyEvent:) forEventClass:kAECoreSuite andEventID:kAEClose];
	[appleEventManager setEventHandler:self andSelector:@selector(handleGetDataAppEvent:withReplyEvent:) forEventClass:kAECoreSuite andEventID:kAEGetData];
	[appleEventManager setEventHandler:self andSelector:@selector(handleSetDataAppEvent:withReplyEvent:) forEventClass:kAECoreSuite andEventID:kAESetData];
	[appleEventManager setEventHandler:self andSelector:@selector(handleCountElmsAppEvent:withReplyEvent:) forEventClass:kAECoreSuite andEventID:kAECountElements];
	[appleEventManager setEventHandler:self andSelector:@selector(handleExistsAppEvent:withReplyEvent:) forEventClass:kAECoreSuite andEventID:kAEDoObjectsExist];
	[appleEventManager setEventHandler:self andSelector:@selector(handleSaveAppEvent:withReplyEvent:) forEventClass:kAECoreSuite andEventID:kAESave];

#ifdef WIDGET_RUNTIME_SUPPORT	
	if (!GadgetStartup::IsGadgetStartupRequested())
#endif // WIDGET_RUNTIME_SUPPORT		
	{
		[appleEventManager setEventHandler:self andSelector:@selector(handleOpenURLAppEvent:withReplyEvent:) forEventClass:kAEInternetSuite andEventID:kAEISGetURL];
		[appleEventManager setEventHandler:self andSelector:@selector(handleOpenURLAppEvent:withReplyEvent:) forEventClass:'GURL' andEventID:'GURL'];
		[appleEventManager setEventHandler:self andSelector:@selector(handleOpenURLAppEvent:withReplyEvent:) forEventClass:'WWW!' andEventID:'OURL'];
	}
	return self;
}
- (void)handleQuitAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	NSEvent* dummy = [NSEvent mouseEventWithType:NSMouseMoved
										location:[NSEvent mouseLocation]
								   modifierFlags:0 timestamp:[NSDate timeIntervalSinceReferenceDate]
									windowNumber:[[NSApp mainWindow] windowNumber]
										 context:[[NSApp mainWindow] graphicsContext]
									 eventNumber:0
									  clickCount:1
										pressure:0.0];
	
	g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT);

	// Workaround for DSK-298450: Need to kick both Opera and the App to finish processing of the action
	[NSApp postEvent:dummy atStart:NO];
	g_component_manager->RunSlice();
}

- (void)handleReopenAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::ReopenApp(evt, reply, 0);
}
- (void)handleOpenAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::Open(evt, reply, 0);
}
- (void)handlePrintAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::Print(evt, reply, 0);
}
- (void)handleCreateElmAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::Make(evt, reply, 0);
}
- (void)handleCloseAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::Close(evt, reply, 0);
}
- (void)handleGetDataAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::GetData(evt, reply, 0);
}

- (void)handleSetDataAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::SetData(evt, reply, 0);
}

- (void)handleCountElmsAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::Count(evt, reply, 0);
}

- (void)handleExistsAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::Exists(evt, reply, 0);
}

- (void)handleSaveAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::Save(evt, reply, 0);
}

- (void)handleOpenURLAppEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	NSLog(@"Requesting opening an URL");
	const AppleEvent* evt = [event aeDesc];
	AppleEvent* reply = const_cast<AppleEvent*>([replyEvent aeDesc]);
	AppleEventManager::OpenURL(evt, reply, 0);
}
@end


CocoaAppleEventHandler::CocoaAppleEventHandler()
{
	m_internal_handler = [[CocoaAppleEventHandlerInternal alloc] init];
}


