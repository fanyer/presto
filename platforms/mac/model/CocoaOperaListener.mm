/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/CocoaOperaListener.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"
#include "platforms/mac/util/OpenGLContextWrapper.h"
#define BOOL NSBOOL
#import <Foundation/NSTimer.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSAutoreleasePool.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSApplication.h>
#undef BOOL

@interface TimerCallback : NSObject {
	NSTimer *timer;
	BOOL _tracking;
}

- (id)init:(BOOL)tracking;

- (void)setTracking:(BOOL)tracking;

- (void)removeFromRunLoop;

- (NSDate *)fireDate;
- (void)setFireDate:(NSDate *)date;
- (id)reschedulingNeeded:(int)nextEvent;

- (void)exitRunLoop:(NSTimer*)theTimer;
- (void)timeout:(NSTimer*)theTimer;

- (NSDictionary *)userInfo;

@end

@implementation TimerCallback

- (id)init:(BOOL)tracking
{
	_tracking = FALSE;
	timer = [NSTimer scheduledTimerWithTimeInterval:10000000.0 target:self selector:@selector(timeout:) userInfo:[self userInfo] repeats:YES];
	[self setTracking:tracking];
	[self reschedulingNeeded:0];
	return self;
}

- (void)setTracking:(BOOL)tracking
{
	if (_tracking != tracking)
	{
		_tracking = tracking;
		if (_tracking)
		{
			[[NSRunLoop currentRunLoop] addTimer:timer forMode:NSEventTrackingRunLoopMode];
		}
		else
		{
			[self removeFromRunLoop];
			timer = [NSTimer scheduledTimerWithTimeInterval:10000000.0 target:self selector:@selector(timeout:) userInfo:[self userInfo] repeats:YES];
			[self reschedulingNeeded:0];
		}
	}
}

- (void)removeFromRunLoop
{
	[timer invalidate];
}

- (NSDate *)fireDate
{
	return [timer fireDate];
}

- (void)setFireDate:(NSDate *)date
{
	[timer setFireDate:date];
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

- (void)exitRunLoop:(NSTimer*)theTimer
{
	CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void)timeout:(NSTimer*)theTimer
{
	RESET_OPENGL_CONTEXT

	const int min_system_runslice_tracking = 3;
	const int max_system_runslice_processing_events = 100;

	int nextEvent = g_component_manager->RunSlice();

	// Hack to stop delays, just pump the RunSlices faster to get the mouse/keyboards events working
	if (CocoaOperaListener::GetListener()->IsInsideProcessEvents())
		nextEvent = MIN(nextEvent,max_system_runslice_processing_events);

	// Workaround for DSK-358054 and similar: Do not deplete system processing time for menu tracking
	if (_tracking)
		nextEvent = MAX(nextEvent,min_system_runslice_tracking);

	[self reschedulingNeeded:nextEvent];
	if (_tracking || CocoaOperaListener::GetListener()->IsInsideProcessEvents())
	{
		NSUInteger mask = CocoaOperaListener::GetListener()->IsInsideProcessEvents() ? NSAnyEventMask : NSAppKitDefinedMask|NSSystemDefinedMask|NSApplicationDefinedMask|NSPeriodicMask;

		NSEvent* event = [NSApp nextEventMatchingMask:mask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
		if (event)
			[NSApp sendEvent:event];
	}
}

- (NSDictionary *)userInfo {
    return [NSDictionary dictionaryWithObject:[NSDate date] forKey:@"StartDate"];
}

@end

CocoaOperaListener* CocoaOperaListener::s_current_opera_listener = NULL;
PosixIpcProcessManager CocoaOperaListener::s_process_manager;

CocoaOperaListener::CocoaOperaListener(BOOL tracking, void* killDate)
{
	m_data = [[TimerCallback alloc] init:tracking];
	m_insideProcessEvents = false;
	g_component_manager->SetPlatform(this);
    m_previous_listener = s_current_opera_listener;
	if (m_previous_listener)
		[(id)m_data setFireDate:[(id)m_previous_listener->m_data fireDate]];
    s_current_opera_listener = this;
}

CocoaOperaListener::~CocoaOperaListener()
{
	if (m_previous_listener)
		[(id)m_data setFireDate:[(id)m_previous_listener->m_data fireDate]];
	s_current_opera_listener = m_previous_listener;

	if (g_component_manager)
		g_component_manager->SetPlatform(s_current_opera_listener);
	TimerCallback* timer = (TimerCallback*)m_data;
	[timer removeFromRunLoop];
	[timer release];
}

void CocoaOperaListener::SetTracking(BOOL tracking)
{
	TimerCallback* timer = (TimerCallback*)m_data;
	[timer setTracking:tracking];
}

void CocoaOperaListener::RequestRunSlice(unsigned int limit)
{
    s_process_manager.RequestRunSlice(limit);
	[(id)m_data reschedulingNeeded:limit];
	if (m_insideProcessEvents)
		CFRunLoopStop(CFRunLoopGetCurrent());
}

OP_STATUS CocoaOperaListener::RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type)
{
	switch (type)
	{
		case COMPONENT_TEST:
			return s_process_manager.CreateComponent(peer, requester, type);
		case COMPONENT_PLUGIN_MAC_32:
		case COMPONENT_PLUGIN:
			return s_process_manager.CreateComponent(peer, requester, type);
	}
	
	return g_component_manager->HandlePeerRequest(requester, type);
}

OP_STATUS CocoaOperaListener::SendMessage(OpTypedMessage* message)
{
	return s_process_manager.SendMessage(message);
}

OP_STATUS CocoaOperaListener::ProcessEvents(unsigned int timeout, EventFlags flags)
{
    if (flags == PROCESS_IPC_MESSAGES)
        return s_process_manager.ProcessEvents(timeout);

	m_insideProcessEvents = true;
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, (CFTimeInterval)timeout/1000.0, FALSE);
	m_insideProcessEvents = false;

	// if we are inside a nested loop, exit our parent loop as well.
	CFRunLoopStop(CFRunLoopGetCurrent());

	return OpStatus::OK;
}
