/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/mac/pluginwrapper/pi/CocoaOpMainThread.h"

#define BOOL NSBOOL
#import <Foundation/NSObject.h>
#import <Foundation/NSLock.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSThread.h>
#undef BOOL

@interface CocoaOpTypedMessage : NSObject {
@private
	OpTypedMessage* m_typed_msg;
}

-(id)init:(OpTypedMessage *)message;
- (OpTypedMessage *)getMessage;
@end

@implementation CocoaOpTypedMessage

-(id)init:(OpTypedMessage *)message
{
	[super init];
	m_typed_msg = message;
	return self;
}
- (OpTypedMessage *)getMessage
{
	return m_typed_msg;
}

@end

@interface MainThreadCallback : NSObject {
@private
	NSLock	*m_lock;
	NSMutableArray *m_message_list;
}

- (id)init;
- (void)addSendMessage:(OpTypedMessage*)message;
- (void)runOnMainThread;
- (void)removeAllMessages;

@end

@implementation MainThreadCallback

- (id)init
{
	[super init];
	m_lock			= [[NSLock alloc] init];
	m_message_list	= [[NSMutableArray alloc] init];
	return self;
}

- (void)dealloc
{
	if (m_message_list)
		[m_message_list release];
	if (m_lock)
		[m_lock release];
	[super dealloc];
}

- (void)addSendMessage:(OpTypedMessage*)message
{
	CocoaOpTypedMessage *cocoa_message = [[CocoaOpTypedMessage alloc] init:message];
	[m_lock lock];
	// Add to the end of the array
	[m_message_list addObject:[cocoa_message autorelease]];
	[m_lock unlock];
}

- (void)runOnMainThread
{
	[m_lock lock];
	if (![m_message_list count]) 
	{
		[m_lock unlock];
		return;
	}	
	CocoaOpTypedMessage *cocoa_message = [m_message_list objectAtIndex:0];
	[cocoa_message retain];
	[m_message_list removeObjectAtIndex:0];
	[m_lock unlock];
	
	g_component_manager->SendMessage([cocoa_message getMessage]);
	[cocoa_message release];
}

- (void)removeAllMessages
{
	[m_lock lock];
	while ([m_message_list count])
	{
		CocoaOpTypedMessage *cocoa_message = [m_message_list objectAtIndex:0]; 
		OpTypedMessage *message = [cocoa_message getMessage];
		OP_DELETE(message);
		[m_message_list removeObjectAtIndex:0];
	}
	[m_lock unlock];
}

@end

OP_STATUS OpThreadTools::Create(OpThreadTools** new_main_thread)
{
	*new_main_thread = OP_NEW(CocoaOpMainThread, ());
	if (!*new_main_thread)
		return OpStatus::ERR_NO_MEMORY;
	
	if (OpStatus::IsError(static_cast<CocoaOpMainThread*>(*new_main_thread)->Init()))
	{
		OP_DELETE(*new_main_thread);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

CocoaOpMainThread::~CocoaOpMainThread() 
{
	[(MainThreadCallback *)m_main_thread_callback removeAllMessages];
	[(MainThreadCallback *)m_main_thread_callback release];
}

OP_STATUS CocoaOpMainThread::Init()
{
	m_main_thread_callback = (void *)[[MainThreadCallback alloc] init];
	if (!m_main_thread_callback)
		return OpStatus::ERR_NO_MEMORY;
	
	return OpStatus::OK;
}


void* CocoaOpMainThread::Allocate(size_t size)
{
	return malloc(size);
}

void CocoaOpMainThread::Free(void* memblock)
{
	free(memblock);
}

OP_STATUS CocoaOpMainThread::PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay)
{
	return OpStatus::ERR;
}

OP_STATUS CocoaOpMainThread::SendMessageToMainThread(OpTypedMessage* message)
{
	[(MainThreadCallback *)m_main_thread_callback addSendMessage:message];
	[(MainThreadCallback *)m_main_thread_callback performSelectorOnMainThread:@selector(runOnMainThread) withObject:(id)m_main_thread_callback waitUntilDone:NO];

	return OpStatus::OK;
}

