//
//  AppleRemote.m
//  AppleRemote
//
//  Created by Martin Kahr on 11.03.06.
//  Copyright 2006 martinkahr.com. All rights reserved.
//

#import "platforms/mac/Remote/AppleRemote.h"

static AppleRemote* sharedInstance=nil;

@implementation AppleRemote

#pragma public interface

- (id) init
{
	if (sharedInstance) return sharedInstance;
	self = [super init];
	if ( self ) {
		openInExclusiveMode = YES;
		queue = NULL;
		hidDeviceInterface = NULL;
		cookies = NULL;
	}

	return self;
}

- (void) dealloc
{
	[self stopListening:self];
	[super dealloc];
}

+ (AppleRemote*) sharedRemote
{
	if (sharedInstance) return sharedInstance;
	sharedInstance = [[AppleRemote alloc] init];
	return sharedInstance;
}

- (BOOL) isRemoteAvailable
{
	io_object_t hidDevice = [self findAppleRemoteDevice];
	if (hidDevice != 0) {
		IOObjectRelease(hidDevice);
		return YES;
	} else {
		return NO;
	}
}

- (BOOL) isListeningToRemote
{
	return (hidDeviceInterface != NULL && cookies != NULL && queue != NULL);
}
- (void) setListeningToRemote: (BOOL) value
{
	if (value == NO) {
		[self stopListening:self];
	} else {
		[self startListening:self];
	}
}

- (void) setDelegate: (id) _delegate
{
	if ([_delegate respondsToSelector:@selector(appleRemoteButton:pressedDown:)]==NO) return;

	[_delegate retain];
	[delegate release];
	delegate = _delegate;
}
- (id) delegate {
	return delegate;
}

- (BOOL) isOpenInExclusiveMode
{
	return openInExclusiveMode;
}
- (void) setOpenInExclusiveMode: (BOOL) value
{
	openInExclusiveMode = value;
}

- (IBAction) startListening: (id) sender
{
	if ([self isListeningToRemote]) return;

	io_object_t hidDevice = [self findAppleRemoteDevice];
	if (hidDevice == 0) return;

	if ([self createInterfaceForDevice:hidDevice] == NULL) {
		goto error;
	}

	if ([self initializeCookies]==NO) {
		goto error;
	}

	if ([self openDevice]==NO) {
		goto error;
	}
	goto cleanup;

error:
	[self stopListening:self];

cleanup:
	IOObjectRelease(hidDevice);
}

- (IBAction) stopListening: (id) sender
{
	if (queue != NULL) {
		(*queue)->stop(queue);

		//dispose of queue
		(*queue)->dispose(queue);

		//release the queue we allocated
		(*queue)->Release(queue);

		queue = NULL;
	}

	if (cookies != NULL) {
		free(cookies);
		cookies = NULL;
	}

	if (hidDeviceInterface != NULL) {
		//close the device
		(*hidDeviceInterface)->close(hidDeviceInterface);

		//release the interface
		(*hidDeviceInterface)->Release(hidDeviceInterface);

		hidDeviceInterface = NULL;
	}
}

@end

@implementation AppleRemote (PrivateMethods)

- (IOHIDQueueInterface**) queue
{
	return queue;
}

- (IOHIDDeviceInterface**) hidDeviceInterface
{
	return hidDeviceInterface;
}

- (void) handleEvent: (IOHIDEventStruct) event
{
	AppleRemoteCookieIdentifier remoteId = -1;

	int i=0;
	for(i=0; i<NUMBER_OF_APPLE_REMOTE_ACTIONS; i++) {
		if (cookies[i] == event.elementCookie) {
			remoteId = i;
			break;
		}
	}
	if (delegate) {
		[delegate appleRemoteButton:remoteId pressedDown: (event.value==1)];
	}
	/*
	switch(remoteId) {
		case kRemoteButtonVolume_Plus:
			NSLog(@"Volume +");
			break;
		case kRemoteButtonVolume_Minus:
			NSLog(@"Volume -");
			break;
		case kRemoteButtonMenu:
			NSLog(@"Menu");
			break;
		case kRemoteButtonPlay:
			NSLog(@"Play");
			break;
		case kRemoteButtonRight:
			NSLog(@"Right");
			break;
		case kRemoteButtonLeft:
			NSLog(@"Left");
			break;
		case kRemoteButtonRight_Hold:
			NSLog(@"Right HOLD");
			break;
		case kRemoteButtonLeft_Hold:
			NSLog(@"Left HOLD");
			break;
		case kRemoteButtonPlay_Sleep:
			NSLog(@"Play SLEEP");
			break;
		default:
			NSLog(@"Unmapped event for cookie %d", event.elementCookie);
			break;
	}
	printf(" %d\n", event.value);*/
}

@end

/*	Callback method for the device queue
Will be called for any event of any type (cookie) to which we subscribe
*/
static void QueueCallbackFunction(void* target,  IOReturn result, void* refcon, void* sender)
{
	AppleRemote* remote = (AppleRemote*)target;

	IOHIDEventStruct event;
	AbsoluteTime 	 zeroTime = {0,0};

	while (result == kIOReturnSuccess)
	{
		result = (*[remote queue])->getNextEvent([remote queue], &event, zeroTime, 0);
		if ( result != kIOReturnSuccess )
			continue;

		[remote handleEvent: event];
	}
}

@implementation AppleRemote (IOKitMethods)

- (IOHIDDeviceInterface**) createInterfaceForDevice: (io_object_t) hidDevice
{
	io_name_t				className;
	IOCFPlugInInterface**   plugInInterface = NULL;
	HRESULT					plugInResult = S_OK;
	SInt32					score = 0;
	IOReturn				ioReturnValue = kIOReturnSuccess;

	hidDeviceInterface = NULL;

	ioReturnValue = IOObjectGetClass(hidDevice, className);

	if (ioReturnValue != kIOReturnSuccess) {
		NSLog(@"Error: Failed to get class name.");
		return NULL;
	}

	ioReturnValue = IOCreatePlugInInterfaceForService(hidDevice,
													  kIOHIDDeviceUserClientTypeID,
													  kIOCFPlugInInterfaceID,
													  &plugInInterface,
													  &score);
	if (ioReturnValue == kIOReturnSuccess)
	{
		//Call a method of the intermediate plug-in to create the device interface
		plugInResult = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), (LPVOID) &hidDeviceInterface);

		if (plugInResult != S_OK) {
			NSLog(@"Error: Couldn't create HID class device interface");
		}
		// Release
		if (plugInInterface) (*plugInInterface)->Release(plugInInterface);
	}
	return hidDeviceInterface;
}

- (io_object_t) findAppleRemoteDevice
{
	CFMutableDictionaryRef hidMatchDictionary = NULL;
	IOReturn ioReturnValue = kIOReturnSuccess;
	io_iterator_t hidObjectIterator = 0;
	io_object_t	hidDevice = 0;

	// Set up a matching dictionary to search the I/O Registry by class
	// name for all HID class devices
	hidMatchDictionary = IOServiceMatching(AppleRemoteDeviceName);

	// Now search I/O Registry for matching devices.
	ioReturnValue = IOServiceGetMatchingServices(kIOMasterPortDefault, hidMatchDictionary, &hidObjectIterator);

	if ((ioReturnValue == kIOReturnSuccess) && (hidObjectIterator != 0)) {
		hidDevice = IOIteratorNext(hidObjectIterator);
	}

	// release the iterator
	IOObjectRelease(hidObjectIterator);

	return hidDevice;
}

- (BOOL) initializeCookies
{
	IOHIDDeviceInterface122** handle = (IOHIDDeviceInterface122**)hidDeviceInterface;
	IOHIDElementCookie		cookie;
	long					usage;
	long					usagePage;
	id						object;
	NSArray*				elements = nil;
	NSDictionary*			element;
	IOReturn success;

	if (!handle || !(*handle)) return NO;

	// Copy all elements, since we're grabbing most of the elements
	// for this device anyway, and thus, it's faster to iterate them
	// ourselves. When grabbing only one or two elements, a matching
	// dictionary should be passed in here instead of NULL.
	success = (*handle)->copyMatchingElements(handle, NULL, (CFArrayRef*)&elements);

	if (success == kIOReturnSuccess) {

		[elements autorelease];

		cookies = calloc(NUMBER_OF_APPLE_REMOTE_ACTIONS, sizeof(IOHIDElementCookie));
		memset(cookies, 0, sizeof(IOHIDElementCookie) * NUMBER_OF_APPLE_REMOTE_ACTIONS);

		unsigned int i;
		for (i=0; i< [elements count]; i++) {
			element = [elements objectAtIndex:i];

			//Get cookie
			object = [element valueForKey: (NSString*)CFSTR(kIOHIDElementCookieKey) ];
			if (object == nil || ![object isKindOfClass:[NSNumber class]]) continue;
			if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
			cookie = (IOHIDElementCookie) [object longValue];

			//Get usage
			object = [element valueForKey: (NSString*)CFSTR(kIOHIDElementUsageKey) ];
			if (object == nil || ![object isKindOfClass:[NSNumber class]]) continue;
			usage = [object longValue];

			//Get usage page
			object = [element valueForKey: (NSString*)CFSTR(kIOHIDElementUsagePageKey) ];
			if (object == nil || ![object isKindOfClass:[NSNumber class]]) continue;
			usagePage = [object longValue];

			AppleRemoteCookieIdentifier cid = -1;
			switch(usage) {
				case 140:
					cid = kRemoteButtonVolume_Plus;
					break;
				case 141:
					cid = kRemoteButtonVolume_Minus;
					break;
				case 134:
					cid = kRemoteButtonMenu;
					break;
				case 137:
					cid = kRemoteButtonPlay;
					break;
				case 138:
					cid = kRemoteButtonRight;
					break;
				case 139:
					cid = kRemoteButtonLeft;
					break;
				case 179:
					cid = kRemoteButtonRight_Hold;
					break;
				case 180:
					cid = kRemoteButtonLeft_Hold;
					break;
				case 35:
					cid = kRemoteButtonPlay_Sleep;
					break;
				default:
					//NSLog(@"Usage %d will not be used", usage);
					break;
			}

			if (cid != -1) {
				if (cid < NUMBER_OF_APPLE_REMOTE_ACTIONS) {
					cookies[cid] = cookie;
				} else {
					NSLog(@"Invalid index %d for cookie. No slot to store the cookie.", cid);
				}
			}
			//NSLog(@"%d: usage = %d and page = %d", cookie, usage, usagePage);
		}
	} else {
		return NO;
	}

	return YES;
}

- (BOOL) openDevice
{
	HRESULT  result;

	IOHIDOptionsType openMode = kIOHIDOptionsTypeNone;
	if ([self isOpenInExclusiveMode]) openMode = kIOHIDOptionsTypeSeizeDevice;
	IOReturn ioReturnValue = (*hidDeviceInterface)->open(hidDeviceInterface, openMode);

	if (ioReturnValue == KERN_SUCCESS) {
		queue = (*hidDeviceInterface)->allocQueue(hidDeviceInterface);
		if (queue) {
			result = (*queue)->create(queue, 0,
									  8);	//depth: maximum number of elements in queue before oldest elements in queue begin to be lost.

			int i=0;
			for(i=0; i<NUMBER_OF_APPLE_REMOTE_ACTIONS; i++) {
				if (cookies[i] != 0) {
					(*queue)->addElement(queue, cookies[i], 0);
				}
			}

			// add callback for async events
			CFRunLoopSourceRef eventSource;
			ioReturnValue = (*queue)->createAsyncEventSource(queue, &eventSource);
			if (ioReturnValue == KERN_SUCCESS) {
				ioReturnValue = (*queue)->setEventCallout(queue,QueueCallbackFunction, self, NULL);
				if (ioReturnValue == KERN_SUCCESS) {
					CFRunLoopAddSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopDefaultMode);
					//start data delivery to queue
					(*queue)->start(queue);
					return YES;
				} else {
					NSLog(@"Error when setting event callout");
				}
			} else {
				NSLog(@"Error when creating async event source");
			}
		} else {
			NSLog(@"Error when opening device");
		}
	}
	return NO;
}

@end
