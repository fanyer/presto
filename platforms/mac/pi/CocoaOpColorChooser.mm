/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NO_CARBON
#include "platforms/mac/pi/CocoaOpColorChooser.h"
#include "platforms/mac/model/CocoaOperaListener.h"

#define BOOL NSBOOL
#import <AppKit/NSColor.h>
#import <AppKit/NSColorPanel.h>
#import <AppKit/NSApplication.h>
#import <Foundation/NSRunLoop.h>
#undef BOOL

@interface ColorPanelDelegate : NSObject
{
	OpColorChooserListener* _listener;
}
- (id) initWithListener:(OpColorChooserListener*)listener;
- (void)changeColor:(id)sender;
@end

@implementation ColorPanelDelegate
- (id) initWithListener:(OpColorChooserListener*)listener
{
	self = [super init];
	_listener = listener;
	return self;
}
- (void)changeColor:(id)sender
{
	NSColor* color = [[sender color] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
	if (color)
	{
		UINT8 r = [color redComponent]*255.0;
		UINT8 g = [color greenComponent]*255.0;
		UINT8 b = [color blueComponent]*255.0;
		_listener->OnColorSelected(OP_RGBA(r,g,b,0));
	}
	else
	{
		// Should colorUsingColorSpaceName ever fail, use black
		_listener->OnColorSelected(OP_RGBA(0,0,0,0));
	}
}
@end


OP_STATUS OpColorChooser::Create(OpColorChooser** new_object)
{
	*new_object = new CocoaOpColorChooser;
	if (*new_object == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

CocoaOpColorChooser::CocoaOpColorChooser()
	:responder(nil)
{
}

CocoaOpColorChooser::~CocoaOpColorChooser()
{
}

OP_STATUS CocoaOpColorChooser::Show(COLORREF initial_color, OpColorChooserListener* listener, DesktopWindow* parent/*=NULL*/)
{
	id panel = [NSColorPanel sharedColorPanel];
	float r = (float)(GetRValue(initial_color))/255.0, g = (float)(GetGValue(initial_color))/255.0, b =(float)(GetBValue(initial_color))/255.0;
	responder = [[ColorPanelDelegate alloc] initWithListener:listener];
	[panel setDelegate:responder];
	NSColor* color = [NSColor colorWithDeviceRed:r green:g blue:b alpha:1.0];
	[panel setColor:color];
	[[NSApplication sharedApplication] orderFrontColorPanel:panel];
	[panel makeKeyWindow];
	NSModalSession modalsession = (NSModalSession)[NSApp beginModalSessionForWindow:panel];
	int result = NSRunContinuesResponse;

	CocoaOperaListener tempListener;
	// Loop until some result other than continues:
	while (result == NSRunContinuesResponse)
	{
		// Run the window modally until there are no events to process:
		if ([panel isVisible])
			result = [NSApp runModalSession:modalsession];
		else
		{
			result = NSRunStoppedResponse;
			[NSApp endModalSession:modalsession];
			[panel setDelegate:nil];
			[responder release];
		}

		// Give the main loop some time:
		if(result == NSRunContinuesResponse)
		{
			[[NSRunLoop currentRunLoop] limitDateForMode:NSDefaultRunLoopMode];
		}
	}		
	return OpStatus::OK;
}

#endif // NO_CARBON

