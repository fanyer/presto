/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/File/FileUtils_Mac.h"
#include "modules/util/adt/opvector.h"
#include "platforms/mac/util/systemcapabilities.h"

// Last due to YES/NO type redefining.
#include "platforms/mac/notifications/CocoaNotifications.h"

void InitNotifications()
{
	// we dont need to setup any third party notifications
	// system for Leopard and older cats
	if (GetOSVersion() < 0x1060) return;
	if (GetOSVersion() >= 0x1080) {
		id<OpUserNotificationCenter> center = [NSClassFromString(@"NSUserNotificationCenter") performSelector:@selector(defaultUserNotificationCenter)];
		center.delegate = [OperaNotifierDelegate getNotifierDelegate];
	} else {
		char path[PATH_MAX];
		NSString* oppath = nil;
		if (CFURLGetFileSystemRepresentation(OpFileUtils::GetOperaBundle(), true, (UInt8*)path, PATH_MAX))
			oppath = [NSString stringWithUTF8String:path];

		NSBundle *bundle = oppath ? [NSBundle bundleWithPath:oppath] : nil;
		if (!bundle)
			bundle = [NSBundle mainBundle];
		NSString *growlPath = [[bundle privateFrameworksPath] stringByAppendingPathComponent:@"Growl.framework"];
		NSBundle *growlBundle = [NSBundle bundleWithPath:growlPath];

		if (growlBundle != nil && [growlBundle load])
			[GrowlApplicationBridge setGrowlDelegate:[OperaNotifierDelegate getNotifierDelegate]];
    }
}

void TearDownNotifications()
{
	if (GetOSVersion() >= 0x1080)
		[[OperaNotifierDelegate getNotifierDelegate] dealloc];
}

@implementation OperaNotifierDelegate

-(id) init {
	if (self = [super init]) {
		next_id_ = 0;
	}
	return self;
}

- (void) dealloc {
	id<OpUserNotificationCenter> center = [NSClassFromString(@"NSUserNotificationCenter") performSelector:@selector(defaultUserNotificationCenter)];
	[center removeAllDeliveredNotifications];
	[super dealloc];
}

+ (OperaNotifierDelegate*) getNotifierDelegate {
	static OperaNotifierDelegate* instance = [[OperaNotifierDelegate alloc] init];
	return instance;
}

-(void) growlNotificationWasClicked:(id)clickContext {
	int notifier_id = *(int*)[(NSData*)clickContext bytes];

	MacOpNotifier *notifier = [self getNotifier:notifier_id];

	if (notifier)
		notifier->InvokeAction();

	[self deleteNotifier:notifier_id];
}

-(void) growlNotificationTimedOut:(id)clickContext
{
	int notifier_id = *(int*)[(NSData*)clickContext bytes];

	[self deleteNotifier:notifier_id];
}

-(MacOpNotifier*) getNotifier:(NSInteger)notifier_id
{
	for (UINT32 i = 0; i < notifier_list_.GetCount(); ++i)
		if (*notifier_list_.Get(i) == notifier_id)
			return notifier_list_.Get(i)->getNotifier();
	return NULL;
}

-(NSInteger) setNotifier:(MacOpNotifier*)notifier
{
	NSInteger notifier_id = [self getNextNotifierId];
	IdNotifierPair* e = OP_NEW(IdNotifierPair, (notifier_id, notifier));
	if (e == NULL) return -1;
	notifier_list_.Add(e);
	return notifier_id;
}

-(NSInteger) getNextNotifierId
{
	return next_id_++;
}

- (void) deleteNotifier:(NSInteger)notifier_id
{
	for (UINT32 i = 0; i < notifier_list_.GetCount(); ++i)
		if (*notifier_list_.Get(i) == notifier_id)
		{
			notifier_list_.Delete(notifier_list_.Get(i--));
			break;
		}
}

// Sent to the delegate when a notification delivery date has arrived. At this time, the notification has either been presented to the user or the notification center has decided not to present it because your application was already frontmost.
- (void)userNotificationCenter:(NSUserNotificationCenter *)center didDeliverNotification:(id<OpUserNotification>)notification
{
}

// Sent to the delegate when a user clicks on a notification in the notification center. This would be a good time to take action in response to user interacting with a specific notification.
// Important: If want to take an action when your application is launched as a result of a user clicking on a notification, be sure to implement the applicationDidFinishLaunching: method on your NSApplicationDelegate. The notification parameter to that method has a userInfo dictionary, and that dictionary has the NSApplicationLaunchUserNotificationKey key. The value of that key is the NSUserNotification that caused the application to launch. The NSUserNotification is delivered to the NSApplication delegate because that message will be sent before your application has a chance to set a delegate for the NSUserNotificationCenter.
- (void)userNotificationCenter:(NSUserNotificationCenter *)center didActivateNotification:(id<OpUserNotification>)notification
{
	NSDictionary* dict = notification.userInfo;
	int notifier_id = *(int*)[(NSData*)[dict objectForKey:KEY_NOTIFIER_ID] bytes];
	MacOpNotifier* op_notifier = [self getNotifier:notifier_id];
	if (op_notifier)
		op_notifier->InvokeAction();
	[self deleteNotifier:notifier_id];
	[center removeDeliveredNotification:notification];
}

// Sent to the delegate when the Notification Center has decided not to present your notification, for example when your application is front most. If you want the notification to be displayed anyway, return YES.
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center shouldPresentNotification:(id<OpUserNotification>)notification
{
	return TRUE;
}

@end
