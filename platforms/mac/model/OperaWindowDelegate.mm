/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/CocoaOpWindow.h"
#include "modules/pi/OpWindow.h"
#include "platforms/mac/model/BottomLineInput.h"

// Last due to YES/NO type redefining.
#include "platforms/mac/model/OperaWindowDelegate.h"
#include "platforms/mac/model/OperaNSWindow.h"
#include "platforms/mac/Remote/AppleRemote.h"

#define BOOL NSBOOL
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#undef BOOL

@implementation OperaWindowDelegate
- (id)init {
	windowlistener = NULL;
	return [super init];
}
- (void)setListener:(OpWindowListener*)listener {
	windowlistener = listener;
}

- (OpWindowListener*)getListener {
	return windowlistener;
}
#pragma mark (NSWindowNotifications)
- (void)windowDidResize:(NSNotification *)notification {
	if (windowlistener) {
		NSRect frame = [[[notification object] contentView] frame];
		windowlistener->OnResize(frame.size.width, frame.size.height);
		[[[[notification object] contentView] superview] setNeedsDisplay:YES];
	}
}

// Note: This isn't called like "OnShow", stupid Apple, we dunno what it does really :P
- (void)windowDidExpose:(NSNotification *)notification {
}

- (void)windowDidMove:(NSNotification *)notification {
	if ([notification object] == [NSApp keyWindow])
		CocoaOpWindow::CloseCurrentPopupIfNot([notification object]);
	if (windowlistener) {
		windowlistener->OnMove();
	}
}
- (void)windowDidBecomeKey:(NSNotification *)notification {
	if ([[[notification object] contentView] respondsToSelector:@selector(invalidateAll)])
	{
		[[[notification object] contentView] invalidateAll];
	}
}
- (void)windowDidResignKey:(NSNotification *)notification {
	if ([[BottomLineInput bottomLineInput] shouldBeShown] || [[BottomLineInput bottomLineInput] isVisible])
		[[BottomLineInput bottomLineInput] show:FALSE];

	if ([[[notification object] contentView] respondsToSelector:@selector(invalidateAll)])
	{
		[[[notification object] contentView] invalidateAll];
	}
}
- (void)windowDidBecomeMain:(NSNotification *)notification {
	if ([[[notification object] contentView] respondsToSelector:@selector(invalidateAll)])
	{
		[[[notification object] contentView] invalidateAll];
	}
	if (windowlistener) {
		windowlistener->OnActivate(TRUE);
	}
}
- (void)windowDidResignMain:(NSNotification *)notification {
	if ([[BottomLineInput bottomLineInput] shouldBeShown] || [[BottomLineInput bottomLineInput] isVisible])
		[[BottomLineInput bottomLineInput] show:FALSE];
	
	if ([[[notification object] contentView] respondsToSelector:@selector(invalidateAll)])
	{
		[[[notification object] contentView] invalidateAll];
	}
	if (windowlistener) {
		windowlistener->OnActivate(FALSE);
	}
}
- (void)windowWillClose:(NSNotification *)notification {
	if (windowlistener) {
		windowlistener->OnClose(TRUE);
	}
}
- (void)windowDidDeminiaturize:(NSNotification *)notification
{
	if ([[[notification object] contentView] respondsToSelector:@selector(invalidateAll)])
	{
		[[[notification object] contentView] invalidateAll];
	}
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
	[[notification object] setSystemFullscreen:FALSE];
	if ([[notification object] fullscreen]) {
		g_input_manager->InvokeAction(OpInputAction::ACTION_LEAVE_FULLSCREEN);
	}
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
	[[notification object] setSystemFullscreen:TRUE];
}

#pragma mark (NSWindowDelegate)
- (NSRect)window:(NSWindow *)window willPositionSheet:(NSWindow *)sheet usingRect:(NSRect)rect
{
//	FIXME: use correct toolbar height for sheets
	NSRect bounds = rect;
	if ([window respondsToSelector:@selector(getUnifiedToolbarHeight)])
		bounds.origin.y = [window frame].size.height - [((OperaNSWindow*)window) getUnifiedToolbarHeight];
	bounds.size.height = 0;
	return bounds;
}

@end

