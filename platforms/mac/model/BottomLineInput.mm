/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/BottomLineInput.h"
#include "platforms/mac/util/macutils_mincore.h"
#ifndef NS4P_COMPONENT_PLUGINS
#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#endif // !NS4P_COMPONENT_PLUGINS

#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"
#include "platforms/mac/src/generated/g_message_mac_messages.h"

#define BOOL NSBOOL
#import <AppKit/NSScreen.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSTextInputContext.h>
#undef BOOL

@implementation BottomLineInput

+ (BottomLineInput *)bottomLineInput
{
	static BottomLineInput *sBottomLineInput;
	if (sBottomLineInput == nil)
		sBottomLineInput = [[BottomLineInput alloc] init];
	
	return sBottomLineInput;
}

- (id)init
{
	// 0x61f is some Apple magic hidden style
	self = [super initWithContentRect:NSZeroRect styleMask:0x61f backing:NSBackingStoreBuffered defer:YES];
	if (!self)
		return nil;
	
	// Set the frame size.
	NSRect visibleFrame = [[NSScreen mainScreen] visibleFrame];
	NSRect frame = NSMakeRect(visibleFrame.origin.x, visibleFrame.origin.y, visibleFrame.size.width, 20);
	
	[self setFrame:frame display:NO];
	
	_nsTextView = [[NSTextView alloc] initWithFrame:[self.contentView frame]];        
	_nsTextView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable | NSViewMaxXMargin | NSViewMinXMargin | NSViewMaxYMargin | NSViewMinYMargin;
	
	NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:[self.contentView frame]];
	scrollView.documentView = _nsTextView;
	self.contentView = scrollView;
	[scrollView release];
	
	_visible = FALSE;
	_shouldBeShown = FALSE;
	_operaNSView = nil;
#ifndef NS4P_COMPONENT_PLUGINS
    _macPluginWindow = nil;
#endif // !NS4P_COMPONENT_PLUGINS
	
	[self setFloatingPanel:YES];

/* Not sure if we need this yet
	[[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_keyboardInputSourceChanged:)
                                                 name:NSTextInputContextKeyboardSelectionDidChangeNotification
                                               object:nil];
*/
    return self;
}

/*
- (void)_keyboardInputSourceChanged:(NSNotification *)notification
{
	[self show:FALSE];
}
 */

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[_nsTextView release];
	
	[super dealloc];
}

#ifdef NS4P_COMPONENT_PLUGINS
- (void)setDestMessageAddress:(const OpMessageAddress &)addr
{
	_destMessageAddress = addr;
}
#else // NS4P_COMPONENT_PLUGINS
- (void)setPluginWindow:(void *)macPluginWindow
{
    _macPluginWindow = macPluginWindow;
}
#endif // NS4P_COMPONENT_PLUGINS

- (void)sendText:(NSString *)string
{
	if ([string length] <= 0)
		return;
	
	UniString composed_string;
	
	if (SetUniString(composed_string, (void *)string))
    {
#ifdef NS4P_COMPONENT_PLUGINS
		g_component_manager->SendMessage(OpMacPluginIMETextMessage::Create(composed_string, _destMessageAddress, _destMessageAddress));
#else // NS4P_COMPONENT_PLUGINS
        if (_macPluginWindow)
            static_cast<MacOpPluginWindow*>(_macPluginWindow)->SendIMEText(composed_string);
#endif // NS4P_COMPONENT_PLUGINS
    }
}

- (void)setMarkedText:(id)aString selectedRange:(NSRange)selRange
{
	[_nsTextView setMarkedText:aString selectedRange:selRange];
}

- (void)shouldShow
{
	_shouldBeShown = TRUE;
}

- (BOOL)shouldBeShown
{
	return _shouldBeShown;
}

- (void)setOperaNSView:(OperaNSView*)view
{
	_operaNSView = view;
}

- (NSRange)getMarkedRange;
{
	return [_nsTextView markedRange];
}

- (void)show:(BOOL)show 
{
	if (show)
		[self orderFront:nil];
	else
	{
		[self orderOut:nil];
		if (_operaNSView)
			[_operaNSView setMarkedText:@"" selectedRange:NSMakeRange(0,0)];
	}
	_shouldBeShown = FALSE;
	_visible = show;
}

- (BOOL)isVisible
{
	return _visible;
}

@end
