/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/mac/component/MacComponentPlatform.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/macutils_mincore.h"
#include "modules/pi/OpThreadTools.h"

#include "platforms/posix_ipc/posix_ipc.h"

#define BOOL NSBOOL
#import <AppKit/NSApplication.h>
#import <Foundation/NSTimer.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSAutoreleasePool.h>
#undef BOOL

extern OpThreadTools* g_thread_tools;

@interface ComponentTimerCallback : NSObject {
	NSTimer *timer;
}

- (id)init;
- (void)removeFromRunLoop;
- (id)reschedulingNeeded:(int)nextEvent;
- (void)timeout:(NSTimer*)theTimer;
@end

@implementation ComponentTimerCallback

- (id)init
{
	timer = [NSTimer scheduledTimerWithTimeInterval:10000000.0 target:self selector:@selector(timeout:) userInfo:nil repeats:YES];
	[self reschedulingNeeded:0];
	return self;
}

- (void)removeFromRunLoop
{
	[timer invalidate];
}

- (id)reschedulingNeeded:(int)nextEvent
{
	if (nextEvent > -1)
	{
		NSDate *date = [NSDate dateWithTimeIntervalSinceNow:((float)nextEvent)/1000.0];
		[timer setFireDate:date];
	}
	return self;
}

- (void)timeout:(NSTimer*)theTimer
{
	int nextEvent = g_component_manager->RunSlice();
	[self reschedulingNeeded:nextEvent];
}

@end

/////////////////////////////////////////////////////////////////////////////

PosixIpcComponentPlatform* PosixIpcComponentPlatform::CreateComponent(OpComponentType type)
{
	if (type == COMPONENT_PLUGIN) // This is the only component we know how to create
		return OP_NEW(MacComponentPlatform, ());
	
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////

MacComponentPlatform::MacComponentPlatform() 
{
	m_socket[READ] = NULL;
	m_socket[WRITE] = NULL;
	m_run_loop_source[READ] = NULL;
	m_run_loop_source[WRITE] = NULL;
}

/////////////////////////////////////////////////////////////////////////////

MacComponentPlatform::~MacComponentPlatform()
{
	DestroySocket(READ);
	DestroySocket(WRITE);
}

/////////////////////////////////////////////////////////////////////////////

int MacComponentPlatform::Run()
{
	OperaNSReleasePool pool;
	
	// Place any initialisation code here

	// Needed for the MachOCompatibility layer
	InitializeCoreGraphics();
	InitializeIOSurface();
	InitializeLaunchServices();
	
	RETURN_VALUE_IF_ERROR(OpThreadTools::Create(&g_thread_tools), -1);
	
	// Text converter needed for loading of old resource style plug-ins
	gTextConverter = OP_NEW(CTextConverter, ());
	if (!gTextConverter)
		return -1;

	// Init the sockets for reading and writing
	if (!InitSocket(READ, m_ipc->GetReadPipe()))
		return -1;
	if (!InitSocket(WRITE, m_ipc->GetWritePipe()))
		return -1;
	
	m_timer = (ComponentTimerCallback *)[[ComponentTimerCallback alloc] init];

	[[NSApplication sharedApplication] run];
	
	// Nothing will be called after the "run" call above, since we are using 
	// "terminate" to shutdown the PluginWrapper, so DO NOT place any
	// code below it and expect it to be called EVER!
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////

void MacComponentPlatform::Exit()
{
	OperaNSReleasePool pool;

    PosixIpcComponentPlatform::Exit();

	// Place any clean up code here
	

	// Use "terminate" to shutdown the PluginWrapper since this will
	// exit right away, and we need no cleanup in the PluginWrapper.
	// Note: "stop" doesn't work here because we are using the CFRunLoop
	//		 internally when Processing Events
	[[NSApplication sharedApplication] terminate:0];
	
	// Nothing will be called after the "terminate" function above,
	// the app will just exit so DO NOT place any code here and
	// expect it to work EVER!
}

/////////////////////////////////////////////////////////////////////////////

BOOL MacComponentPlatform::InitSocket(SocketDir dir, int fd)
{
	CFSocketContext ctx;
	ctx.version = 0;
	ctx.info = this;
	ctx.retain = NULL;
	ctx.release = NULL;
	ctx.copyDescription = NULL;
	
	// All of the mode flags need to be turned on in the create even it they aren't in the current mode because calls to 
	// CFSocketEnableCallBacks will only temporarily enable the mode, so if it isn't set at the begining you can't
	// ever enable it.
	CFOptionFlags option_flags = kCFSocketNoCallBack;
	if (dir == READ)
		option_flags |= kCFSocketReadCallBack;
	if (dir == WRITE)
		option_flags |= kCFSocketWriteCallBack;

	m_socket[dir] = CFSocketCreateWithNative(kCFAllocatorDefault, fd, kCFSocketConnectCallBack | option_flags, SocketCallback, &ctx);
	CFSocketSetSocketFlags(m_socket[dir], CFSocketGetSocketFlags(m_socket[dir]) &~ kCFSocketAutomaticallyReenableReadCallBack);
	CFSocketDisableCallBacks(m_socket[dir], option_flags);
	m_run_loop_source[dir] = CFSocketCreateRunLoopSource(NULL, m_socket[dir], 0);
	if (m_run_loop_source[dir])
	{
		CFRunLoopRef run_loop = [[NSRunLoop mainRunLoop] getCFRunLoop];
		CFRunLoopAddSource(run_loop, m_run_loop_source[dir], kCFRunLoopCommonModes);
		CFRunLoopSourceSignal(m_run_loop_source[dir]);
		CFRunLoopWakeUp(run_loop);
	}
	// we don't want our recv() calls to block
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);	

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

void MacComponentPlatform::DestroySocket(SocketDir dir)
{
	if (CFSocketIsValid(m_socket[dir]))
		CFSocketInvalidate(m_socket[dir]);
	CFRelease(m_socket[dir]);
	if(m_run_loop_source[dir])
		CFRelease(m_run_loop_source[dir]);
}

/////////////////////////////////////////////////////////////////////////////

void MacComponentPlatform::Read()
{
	if (OpStatus::IsError(m_ipc->Receive()))
		return Exit();

	while (m_ipc->MessageAvailable())
	{
		OpTypedMessage* msg = NULL;
		if (OpStatus::IsSuccess(m_ipc->ReceiveMessage(msg)))
			g_component_manager->DeliverMessage(msg);
		else
			Exit();
	}
	
	int nextEvent = g_component_manager->RunSlice();
	[(ComponentTimerCallback *)m_timer reschedulingNeeded:nextEvent];
}

/////////////////////////////////////////////////////////////////////////////

void MacComponentPlatform::Write()
{
	if (OpStatus::IsError(m_ipc->Send()))
		return Exit();

	int nextEvent = g_component_manager->RunSlice();
	[(ComponentTimerCallback *)m_timer reschedulingNeeded:nextEvent];
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacComponentPlatform::SendMessage(OpTypedMessage* message)
{
	RETURN_IF_ERROR(PosixIpcComponentPlatform::SendMessage(message));
	if (m_ipc->WantSend() && m_socket[WRITE])
		CFSocketEnableCallBacks(m_socket[WRITE], kCFSocketWriteCallBack);
	
	return OpStatus::OK;
}


/////////////////////////////////////////////////////////////////////////////

void MacComponentPlatform::SocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info)
{
	MacComponentPlatform* self = (MacComponentPlatform*) info;

	if (self)
	{
		switch (type)
		{
			case kCFSocketReadCallBack:
				{
					self->Read();
					CFSocketEnableCallBacks(self->m_socket[READ], kCFSocketReadCallBack);
				}
				break;
			case kCFSocketWriteCallBack:
				{
					self->Write();
				}
				break;
		}
	}
}




