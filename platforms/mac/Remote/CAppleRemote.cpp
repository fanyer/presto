/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/hid/IOHIDLib.h>
#import <IOKit/hid/IOHIDKeys.h>
#include "platforms/mac/Remote/CAppleRemote.h"
#include "modules/inputmanager/inputmanager.h"

#define AppleRemoteDeviceName "AppleIRController"

static void QueueCallbackFunction(void* target,  IOReturn result, void* refcon, void* sender)
{
	CAppleRemote* remote = (CAppleRemote*)target;
//	CAppleRemoteListener* listener = (CAppleRemoteListener*)refcon;

	IOHIDEventStruct event;
	AbsoluteTime 	 zeroTime = {0,0};

	while (result == kIOReturnSuccess)
	{
		result = (*remote->m_queue)->getNextEvent(remote->m_queue, &event, zeroTime, 0);
		if ( result != kIOReturnSuccess )
			continue;

		AppleRemoteButton remoteId = kAppleRemoteActionCount;

		int i=0;
		for(i=0; i<kAppleRemoteActionCount; i++) {
			if (remote->m_cookies[i] == event.elementCookie) {
				remoteId = (AppleRemoteButton)i;
				break;
			}
		}
		if (remoteId < kAppleRemoteActionCount) 
		{
			if (event.value==1)
			{
				switch (remoteId)
				{
					case kAppleRemoteButtonVolume_Plus:
						g_input_manager->InvokeKeyPressed(OP_KEY_VOLUMEUP, 0);
						break;
					case kAppleRemoteButtonVolume_Minus:
						g_input_manager->InvokeKeyPressed(OP_KEY_VOLUMEDOWN, 0);
						break;
					case kAppleRemoteButtonMenu:
						g_input_manager->InvokeKeyPressed(OP_KEY_MENU, 0);
						break;
					case kAppleRemoteButtonPlay:
						g_input_manager->InvokeKeyPressed(OP_KEY_PLAY, 0);
						break;
					case kAppleRemoteButtonRight:
						g_input_manager->InvokeKeyPressed(OP_KEY_NEXT, 0);
						break;
					case kAppleRemoteButtonLeft:
						g_input_manager->InvokeKeyPressed(OP_KEY_PREVIOUS, 0);
						break;
					case kAppleRemoteButtonRight_Hold:
						g_input_manager->InvokeKeyPressed(OP_KEY_FASTFORWARD, 0);
						break;
					case kAppleRemoteButtonLeft_Hold:
						g_input_manager->InvokeKeyPressed(OP_KEY_REWIND, 0);
						break;
					case kAppleRemoteButtonPlay_Sleep:
						g_input_manager->InvokeKeyPressed(OP_KEY_STOP, 0);
						break;
				}
			}
		}
	}
}

static IOHIDDeviceInterface** CreateInterfaceForDevice(io_object_t hidDevice)
{
	IOHIDDeviceInterface**	hidDeviceInterface = NULL;
	io_name_t				className;
	IOCFPlugInInterface**	plugInInterface = NULL;
	HRESULT					plugInResult = S_OK;
	SInt32					score = 0;
	IOReturn				ioReturnValue = kIOReturnSuccess;

	hidDeviceInterface = NULL;

	ioReturnValue = IOObjectGetClass(hidDevice, className);

	if (ioReturnValue != kIOReturnSuccess) {
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
		plugInResult = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), (void**)&hidDeviceInterface);

		if (plugInResult != S_OK) {
//			NSLog(@"Error: Couldn't create HID class device interface");
		}
		// Release
		if (plugInInterface) (*plugInInterface)->Release(plugInInterface);
	}
	return hidDeviceInterface;
}

static io_object_t FindAppleRemoteDevice()
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

static IOHIDElementCookie * InitializeCookies(IOHIDDeviceInterface** hidDeviceInterface)
{
	IOHIDDeviceInterface122** handle = (IOHIDDeviceInterface122**)hidDeviceInterface;
	IOHIDElementCookie *	cookies = NULL;
	IOHIDElementCookie		cookie;
	long					cookie_long;
	long					usage;
	long					usagePage;
	CFTypeRef				object;
	CFArrayRef				elements = NULL;
	CFDictionaryRef			element;
	IOReturn success;

	if (!handle || !(*handle)) return NULL;

	// Copy all elements, since we're grabbing most of the elements
	// for this device anyway, and thus, it's faster to iterate them
	// ourselves. When grabbing only one or two elements, a matching
	// dictionary should be passed in here instead of NULL.
	success = (*handle)->copyMatchingElements(handle, NULL, &elements);

	if (success == kIOReturnSuccess) {

		cookies = (IOHIDElementCookie *)calloc(kAppleRemoteActionCount, sizeof(IOHIDElementCookie));

		CFIndex i;
		for (i=0; i< CFArrayGetCount(elements); i++)
		{
			element = reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(elements, i));

			//Get cookie
			object = reinterpret_cast<CFTypeRef>(CFDictionaryGetValue(element, CFSTR(kIOHIDElementCookieKey)));
			if (object == NULL || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
			CFNumberGetValue((CFNumberRef)object, kCFNumberLongType, &cookie_long);
			cookie = (IOHIDElementCookie) cookie_long;

			//Get usage
			object = reinterpret_cast<CFTypeRef>(CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsageKey)));
			if (object == NULL || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
			CFNumberGetValue((CFNumberRef)object, kCFNumberLongType, &usage);

			//Get usage page
			object = reinterpret_cast<CFTypeRef>(CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsagePageKey)));
			if (object == NULL || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
			CFNumberGetValue((CFNumberRef)object, kCFNumberLongType, &usagePage);

			AppleRemoteButton cid = kAppleRemoteActionCount;
			switch(usage) {
				case 140:
					cid = kAppleRemoteButtonVolume_Plus;
					break;
				case 141:
					cid = kAppleRemoteButtonVolume_Minus;
					break;
				case 134:
					cid = kAppleRemoteButtonMenu;
					break;
				case 137:
					cid = kAppleRemoteButtonPlay;
					break;
				case 138:
					cid = kAppleRemoteButtonRight;
					break;
				case 139:
					cid = kAppleRemoteButtonLeft;
					break;
				case 179:
					cid = kAppleRemoteButtonRight_Hold;
					break;
				case 180:
					cid = kAppleRemoteButtonLeft_Hold;
					break;
				case 35:
					cid = kAppleRemoteButtonPlay_Sleep;
					break;
				default:
					break;
			}

			if (cid < kAppleRemoteActionCount) {
				cookies[cid] = cookie;
			}
		}
		CFRelease(elements);
	}
	return cookies;
}

static IOHIDQueueInterface** OpenDevice(IOHIDDeviceInterface** hidDeviceInterface, IOHIDElementCookie * cookies, Boolean exclusive, void * target, void * refcon)
{
	HRESULT result;
	IOHIDQueueInterface** queue;
	IOHIDOptionsType openMode = kIOHIDOptionsTypeNone;
	if (exclusive)
		openMode = kIOHIDOptionsTypeSeizeDevice;
	IOReturn ioReturnValue = (*hidDeviceInterface)->open(hidDeviceInterface, openMode);

	if (ioReturnValue == KERN_SUCCESS) {
		queue = (*hidDeviceInterface)->allocQueue(hidDeviceInterface);
		if (queue) {
			result = (*queue)->create(queue, 0,
									  8);	//depth: maximum number of elements in queue before oldest elements in queue begin to be lost.

			int i=0;
			for(i=0; i<kAppleRemoteActionCount; i++) {
				if (cookies[i] != 0) {
					(*queue)->addElement(queue, cookies[i], 0);
				}
			}

			// add callback for async events
			CFRunLoopSourceRef eventSource;
			ioReturnValue = (*queue)->createAsyncEventSource(queue, &eventSource);
			if (ioReturnValue == KERN_SUCCESS) {
				ioReturnValue = (*queue)->setEventCallout(queue,QueueCallbackFunction, target, refcon);
				if (ioReturnValue == KERN_SUCCESS) {
					CFRunLoopAddSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopDefaultMode);
					//start data delivery to queue
					(*queue)->start(queue);
					return queue;
				}
			}
		}
	}
	return NULL;
}

CAppleRemoteListener::CAppleRemoteListener()
{
}

CAppleRemoteListener::~CAppleRemoteListener()
{
}

CAppleRemote::CAppleRemote()
	: m_exclusive(true)
	, m_cookies(NULL)
	, m_interface(NULL)
	, m_queue(NULL)
	, m_listener(NULL)
{
}

CAppleRemote::~CAppleRemote()
{
	SetListeningToRemote(false);
}

Boolean CAppleRemote::IsRemoteAvailable()
{
	io_object_t hidDevice = FindAppleRemoteDevice();
	if (hidDevice != 0) {
		IOObjectRelease(hidDevice);
		return true;
	}
	return false;
}

Boolean CAppleRemote::IsListeningToRemote()
{
	return (m_interface != NULL && m_cookies != NULL && m_queue != NULL);
}

void CAppleRemote::SetListeningToRemote(Boolean value)
{
	if (value != IsListeningToRemote())
	{
		if (value)
		{
			io_object_t hidDevice = FindAppleRemoteDevice();
			if (hidDevice)
			{
				m_interface = CreateInterfaceForDevice(hidDevice);
				if (m_interface)
				{
					m_cookies = InitializeCookies(m_interface);
					if (m_cookies)
					{
						m_queue = OpenDevice(m_interface, m_cookies, m_exclusive, this, m_listener);
						if (m_queue)
						{
							IOObjectRelease(hidDevice);
							return; // All done!
						}
					}
				}
				// Something went wrong
				IOObjectRelease(hidDevice);
			}
		}
		if (m_queue != NULL) {
			(*m_queue)->stop(m_queue);

			//dispose of queue
			(*m_queue)->dispose(m_queue);

			//release the queue we allocated
			(*m_queue)->Release(m_queue);

			m_queue = NULL;
		}

		if (m_cookies != NULL) {
			free(m_cookies);
			m_cookies = NULL;
		}

		if (m_interface != NULL) {
			//close the device
			(*m_interface)->close(m_interface);

			//release the interface
			(*m_interface)->Release(m_interface);

			m_interface = NULL;
		}
	}
}

Boolean CAppleRemote::IsOpenInExclusiveMode()
{
	return m_exclusive;
}

void CAppleRemote::SetOpenInExclusiveMode(Boolean value)
{
	m_exclusive = value;
}

CAppleRemoteListener * CAppleRemote::GetListener()
{
	return m_listener;
}

void CAppleRemote::SetListener(CAppleRemoteListener * listener)
{
	m_listener = listener;
}
