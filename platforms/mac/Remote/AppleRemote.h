//
//  AppleRemote.h
//  AppleRemote
//
//  Created by Martin Kahr on 11.03.06.
//  Copyright 2006 martinkahr.com. All rights reserved.
//

#define BOOL NSBOOL
#import <Cocoa/Cocoa.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/hid/IOHIDLib.h>
#import <IOKit/hid/IOHIDKeys.h>
#undef BOOL

#define AppleRemoteDeviceName "AppleIRController"
#define NUMBER_OF_APPLE_REMOTE_ACTIONS 9
enum AppleRemoteCookieIdentifier
{
	kRemoteButtonVolume_Plus=0,
	kRemoteButtonVolume_Minus,
	kRemoteButtonMenu,
	kRemoteButtonPlay,
	kRemoteButtonRight,
	kRemoteButtonLeft,
	kRemoteButtonRight_Hold,
	kRemoteButtonLeft_Hold,
	kRemoteButtonPlay_Sleep
};
typedef enum AppleRemoteCookieIdentifier AppleRemoteCookieIdentifier;

/*	Encapsulates usage of the apple remote control
	This class is implemented as a singleton as there is exactly one remote per machine (until now)
	The class is not thread safe
*/
@interface AppleRemote : NSObject {
	IOHIDDeviceInterface** hidDeviceInterface;
	IOHIDQueueInterface**  queue;
	IOHIDElementCookie*    cookies;

	BOOL openInExclusiveMode;

	IBOutlet id delegate;
}

- (BOOL) isRemoteAvailable;

- (BOOL) isListeningToRemote;
- (void) setListeningToRemote: (BOOL) value;

- (BOOL) isOpenInExclusiveMode;
- (void) setOpenInExclusiveMode: (BOOL) value;

- (void) setDelegate: (id) delegate;
- (id) delegate;

- (IBAction) startListening: (id) sender;
- (IBAction) stopListening: (id) sender;

+ (AppleRemote*) sharedRemote;
@end

/*	Method definitions for the delegate of the AppleRemote class
*/
@interface NSObject(NSAppleRemoteDelegate)

- (void) appleRemoteButton: (AppleRemoteCookieIdentifier)buttonIdentifier pressedDown: (BOOL) pressedDown;

@end

@interface AppleRemote (PrivateMethods)
- (IOHIDQueueInterface**) queue;
- (IOHIDDeviceInterface**) hidDeviceInterface;
- (void) handleEvent: (IOHIDEventStruct) event;
@end

@interface AppleRemote (IOKitMethods)
- (io_object_t) findAppleRemoteDevice;
- (IOHIDDeviceInterface**) createInterfaceForDevice: (io_object_t) hidDevice;
- (BOOL) initializeCookies;
- (BOOL) openDevice;
@end
