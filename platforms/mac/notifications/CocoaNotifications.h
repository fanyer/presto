/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_GROWL_H
#define COCOA_GROWL_H

#include "platforms/mac/pi/MacOpNotifier.h"

#define KEY_NOTIFIER_ID @"KEY_NOTIFIER_ID"

#if !defined(MAC_OS_X_VERSION_10_8) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8

@protocol NSUserNotificationCenterDelegate
@end

#endif  // MAC_OS_X_VERSION_10_8

@class NSUserNotificationCenter;

@protocol OpUserNotification <NSObject>
@property(copy) NSString* title;
@property(copy) NSString* subtitle;
@property(copy) NSString* informativeText;
@property(copy) NSString* actionButtonTitle;
@property(copy) NSDictionary* userInfo;
@property(copy) NSDate* deliveryDate;
@property(copy) NSTimeZone* deliveryTimeZone;
@property(copy) NSDateComponents* deliveryRepeatInterval;
@property(readonly) NSDate* actualDeliveryDate;
@property(readonly, getter=isPresented) BOOL presented;
@property(readonly, getter=isRemote) BOOL remote;
@property(copy) NSString* soundName;
@property BOOL hasActionButton;
@end

@protocol OpUserNotificationCenter
+ (NSUserNotificationCenter*)defaultUserNotificationCenter;
@property(assign) id<NSUserNotificationCenterDelegate> delegate;
@property(copy) NSArray* scheduledNotifications;
- (void)scheduleNotification:(id<OpUserNotification>)notification;
- (void)removeScheduledNotification:(id<OpUserNotification>)notification;
@property(readonly) NSArray* deliveredNotifications;
- (void)deliverNotification:(id<OpUserNotification>)notification;
- (void)removeDeliveredNotification:(id<OpUserNotification>)notification;
- (void)removeAllDeliveredNotifications;
@end

void InitNotifications();

void TearDownNotifications();

class IdNotifierPair {
public:
	IdNotifierPair(NSInteger identifier, MacOpNotifier* notifier) : identifier_(identifier), notifier_(notifier) {}
	~IdNotifierPair() { OP_DELETE(notifier_); }

	bool operator==(const NSInteger i) const { return identifier_ == i; }

	MacOpNotifier* getNotifier() { return notifier_; }
private:
	NSInteger identifier_;
	MacOpNotifier* notifier_;
};

@interface OperaNotifierDelegate : NSObject <GrowlApplicationBridgeDelegate, NSUserNotificationCenterDelegate>
{
	MacOpNotifier* notifier_;
	NSInteger next_id_;
	OpVector<IdNotifierPair> notifier_list_;
}

+ (OperaNotifierDelegate*) getNotifierDelegate;

- (void) dealloc;
- (void) growlNotificationWasClicked:(id)clickContext;
- (void) growlNotificationTimedOut:(id)clickContext;
- (MacOpNotifier*) getNotifier:(int)notifier_id;
- (NSInteger) setNotifier:(MacOpNotifier*)notifier;
- (NSInteger) getNextNotifierId;
- (void) deleteNotifier:(NSInteger)notifier_id;
@end


#endif // COCOA_GROWL_H
