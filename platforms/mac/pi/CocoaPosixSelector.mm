/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if !defined(POSIX_OK_SIMPLE_SELECTOR)
#include "platforms/mac/pi/CocoaPosixSelector.h"

@interface OperaMachPortDelegate : NSObject
{
	PosixSelectListener *m_listener;
}

-(id)initWithListener:(PosixSelectListener*)listener;
-(PosixSelectListener *)listener;
@end
@implementation OperaMachPortDelegate

-(id)initWithListener:(PosixSelectListener*)listener
{
	self = [super init];
	m_listener = listener;
	return self;
}
-(PosixSelectListener *)listener
{
	return m_listener;
}

- (void)handlePortMessage:(NSPortMessage *)message
{
}

- (void)handleMachMessage:(void *)msg
{
}
@end


OP_STATUS PosixSelector::Create(PosixSelector * &selector)
{
	selector = new CocoaPosixSelector;
	return (selector ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
}

CocoaPosixSelector::CocoaPosixSelector()
{
}

CocoaPosixSelector::~CocoaPosixSelector()
{
}

void CocoaPosixSelector::Detach(const PosixSelectListener *whom, int fd)
{
}

void CocoaPosixSelector::Detach(const PosixSelectListener *whom)
{
	
}

OP_STATUS CocoaPosixSelector::Watch(int fd, PosixSelector::Type mode, unsigned long interval, PosixSelectListener *listener, const class OpSocketAddress *connect)
{
	NSPort* port = [NSMachPort portWithMachPort:fd];
	[port setDelegate:[[OperaMachPortDelegate alloc] initWithListener:listener]];
	[port scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:[[NSRunLoop currentRunLoop] currentMode]];
	m_ports.Add(port);
	return OpStatus::OK;
}

#endif // !POSIX_OK_SIMPLE_SELECTOR
