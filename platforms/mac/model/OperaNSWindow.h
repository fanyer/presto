/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPERA_NS_WINDOW_H
#define OPERA_NS_WINDOW_H

#include "modules/pi/OpWindow.h"

#define BOOL NSBOOL
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#undef BOOL

class CAppleRemote;

@interface OperaNSWindow : NSWindow
{
	OpWindow::Style _opera_style;
	INT32			_unified_toolbar_height;
	INT32			_unified_toolbar_left;
	INT32			_unified_toolbar_right;
	INT32			_unified_toolbar_bottom;
	BOOL			_full_screen;
	BOOL			_sys_full_screen;
	CAppleRemote	*remote;
	BOOL			_invisible_titlebar;
	NSRect			_restore_frame;
	CGImageRef		_persona_image;
	OpBitmap*		_persona_image_opbitmap;
}
- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)aStyle backing:(NSBackingStoreType)bufferingType defer:(NSBOOL)flag;
- (void)dealloc;

- (void)setOperaStyle:(OpWindow::Style)opera_style;
- (OpWindow::Style)getOperaStyle;
- (void)setFullscreen:(BOOL)full_screen;
- (BOOL)fullscreen;
- (void)setSystemFullscreen:(BOOL)sys_full_screen;
- (BOOL)systemFullscreen;
+ (id)systemFullscreenWindow;
- (void)setUnifiedToolbarHeight:(INT32)height;
- (INT32)getUnifiedToolbarHeight;
- (CGImageRef)getPersonaCGImageForWidth:(UINT32)width andHeight:(UINT32)height;
- (OpBitmap *)getPersonaOpBitmapForWidth:(UINT32)width andHeight:(UINT32)height;
- (void)setPersonaChanged;

- (void)setUnifiedToolbarLeft:(INT32)left;
- (void)setUnifiedToolbarRight:(INT32)right;
- (void)setUnifiedToolbarBottom:(INT32)bottom;

- (INT32)unifiedToolbarLeft;
- (INT32)unifiedToolbarRight;
- (INT32)unifiedToolbarBottom;

- (void)setInvisibleTitlebar:(BOOL)invisible;
- (BOOL)isInvisibleTitlebar;
- (NSRect)restoreFrame;

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pboard types:(NSArray *)types;
- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType;

#if defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)
+ (void)InstallServicesHandler;

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pboard types:(NSArray *)types;
- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType;
#endif // defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)
@end

#if !defined(MAC_OS_X_VERSION_10_7) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
@interface NSWindow(dummy_missing_funcs)
- (CGFloat)backingScaleFactor;
- (void)toggleFullScreen:(id)sender;
@end
@interface NSScreen(dummy_missing_funcs)
- (CGFloat)backingScaleFactor;
@end
#endif // !defined(MAC_OS_X_VERSION_10_7) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

#endif // OPERA_NS_WINDOW_H
