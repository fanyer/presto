/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/Application.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/pi/OpBitmap.h"
#include "modules/display/vis_dev.h"

#include "platforms/mac/Remote/CAppleRemote.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/CocoaOpClipboard.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"

// Last due to YES/NO type redefining.
#include "platforms/mac/model/OperaNSWindow.h"

#define BOOL NSBOOL
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSApplication.h>
#undef BOOL

#if defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)
// Handlers for mac services
NSArray *g_services_types = nil;
BOOL	g_services_inited = FALSE;
//OperaNSServices *g_service_handler;
#endif // defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)
OperaNSWindow *g_last_fullscreen = nil;

@implementation OperaNSWindow

///////////////////////////////////////////////////////////////////////////////////////////

- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)aStyle backing:(NSBackingStoreType)bufferingType defer:(NSBOOL)flag
{
	self = [super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag];
	
	if (!self)
		return nil;
	
	_full_screen = FALSE;
	_sys_full_screen = FALSE;
	remote = NULL;
	_invisible_titlebar = FALSE;
	_restore_frame = contentRect;
	_persona_image = NULL;

	[self disableCursorRects];

    return self;	
}

- (void)dealloc
{
	CGImageRelease(_persona_image);
	OP_DELETE(remote);
	if (self == g_last_fullscreen)
		g_last_fullscreen = nil;
	[super dealloc];
}

- (void)enableCursorRects
{
}

- (void)discardCursorRects
{
}

- (void)invalidateCursorRectsForView:(NSView *)view
{
}

- (void)resetCursorRects
{
}

///////////////////////////////////////////////////////////////////////////////////////////

- (NSBOOL)canBecomeMainWindow
{
	if (_full_screen)
		return YES;
	
	// If we are using the transparency hack then you cannot be the main window
	// otherwise you will close if someone trys to click on you :)
	if (_invisible_titlebar && _opera_style != OpWindow::STYLE_GADGET && _opera_style != OpWindow::STYLE_HIDDEN_GADGET)
		return NO;

	return [super canBecomeMainWindow];
}

- (NSBOOL)canBecomeKeyWindow
{
	if (_invisible_titlebar && _opera_style != OpWindow::STYLE_GADGET && _opera_style != OpWindow::STYLE_HIDDEN_GADGET)
		return NO;

	if (!CocoaOpWindow::ShouldPassMouseEvents(self))
		return NO;
	
	if (_full_screen)
		return YES;

	return [super canBecomeKeyWindow];
}

- (void)setOperaStyle:(OpWindow::Style)opera_style
{
	_opera_style = opera_style;
}

- (OpWindow::Style)getOperaStyle
{
	return _opera_style;
}

- (void)setFullscreen:(BOOL)full_screen;
{
	_full_screen = full_screen;

	// Add the apple remote for full screen
	if (full_screen && !remote)
	{
		remote = new CAppleRemote();
		if (remote)
			remote->SetListeningToRemote(true);
	}
}

- (BOOL)fullscreen
{
	return _full_screen;
}

- (void)setSystemFullscreen:(BOOL)sys_full_screen
{
	if (sys_full_screen) {
		g_last_fullscreen = self;
		_restore_frame = [self frame];
	}
	else if (self == g_last_fullscreen)
		g_last_fullscreen = nil;

	_sys_full_screen = sys_full_screen;
}

- (BOOL)systemFullscreen
{
	return _sys_full_screen;
}

+ (id)systemFullscreenWindow
{
	return g_last_fullscreen;
}

- (void)setUnifiedToolbarHeight:(INT32)height
{
	_unified_toolbar_height = height;
}

- (void)setUnifiedToolbarLeft:(INT32)left
{
	_unified_toolbar_left = left;
}

- (void)setUnifiedToolbarRight:(INT32)right
{
	_unified_toolbar_right = right;
}

- (void)setUnifiedToolbarBottom:(INT32)bottom
{
	_unified_toolbar_bottom = bottom;
}

- (INT32)getUnifiedToolbarHeight
{
	return _unified_toolbar_height;
}

- (INT32)unifiedToolbarLeft
{
	return _unified_toolbar_left;
}

- (INT32)unifiedToolbarRight
{
	return _unified_toolbar_right;
}

- (INT32)unifiedToolbarBottom
{
	return _unified_toolbar_bottom;
}

- (void)setInvisibleTitlebar:(BOOL)invisible
{
	_invisible_titlebar = invisible;
}

- (BOOL)isInvisibleTitlebar
{
	return _invisible_titlebar;
}

- (OpBitmap *)getPersonaOpBitmapForWidth:(UINT32)width andHeight:(UINT32)height
{
	[self getPersonaCGImageForWidth:width andHeight:height];

	return _persona_image_opbitmap;
}

- (CGImageRef)getPersonaCGImageForWidth:(UINT32)width andHeight:(UINT32)height
{
#ifdef PERSONA_SKIN_SUPPORT
	if(_persona_image && (CGImageGetWidth(_persona_image) < width || CGImageGetHeight(_persona_image) < height))
	{
		// image needs to be in a different size now
		[self setPersonaChanged];
	}
	if(!_persona_image && g_skin_manager->HasPersonaSkin())
	{
		OpSkinElement *elm = g_skin_manager->GetPersona()->GetBackgroundImageSkinElement();
		if(elm)
		{
			Image img = elm->GetImage(0, SKINPART_TILE_CENTER);
			if(!img.IsEmpty())
			{
				// we need to create it 
				if(OpStatus::IsSuccess(OpBitmap::Create(&_persona_image_opbitmap, width, height, FALSE, TRUE, 0, 0, TRUE)))
				{
					// Clear the bitmap to transparent
					_persona_image_opbitmap->SetColor(NULL, TRUE, NULL);
					OpPainter* painter = _persona_image_opbitmap->GetPainter();
					if (painter)
					{
						OpRect draw_rect(0, 0, width, height);
						VisualDevice vd;
						vd.painter = painter;
						
						elm->Draw(&vd, draw_rect, 0);
						
						_persona_image_opbitmap->ReleasePainter();
						
						// Get an CGImageRef from the OpBitmap, OpBitmap is now owned by the CGImageRef
						_persona_image = CreateCGImageFromOpBitmap(_persona_image_opbitmap);
					}
					else 
					{
						delete _persona_image_opbitmap;
						_persona_image_opbitmap = NULL;
					}
				}
			}
		}
	}
#endif // PERSONA_SKIN_SUPPORT
	return _persona_image;
}

- (void)setPersonaChanged
{
	// Just clear it, it will be recreated with the new persona on the next paint
	if(_persona_image)
	{
		CGImageRelease(_persona_image);
		_persona_image = NULL;
		_persona_image_opbitmap = NULL;
	}
}

- (NSRect)restoreFrame
{
	//NSFullScreenWindowMask
	return [self styleMask] & (1 << 14) ? _restore_frame : [self frame];
}

#if defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)

////////////////////////////////////////////////////////////////////////

+ (void)InstallServicesHandler
{
	// Only install if we're not embedded
	if (g_application && g_application->IsEmBrowser())
		return;
	
	if (g_services_inited)
		return;	// already installed
	
	g_services_inited = TRUE;
	
    g_services_types = [[NSArray arrayWithObjects: NSStringPboardType, NSTIFFPboardType, nil] retain];
    [[NSApplication sharedApplication] registerServicesMenuSendTypes: g_services_types returnTypes: nil];
}

////////////////////////////////////////////////////////////////////////

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
	if([g_services_types containsObject: sendType])
		return self;
	
	return [super validRequestorForSendType: sendType returnType: returnType];
}

////////////////////////////////////////////////////////////////////////

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pboard types:(NSArray *)types
{
	CocoaOpClipboard* clip = CocoaOpClipboard::s_cocoa_op_clipboard;
	
	clip->SetServicePasteboard(pboard, types);
	OpInputAction* copy = new OpInputAction(OpInputAction::ACTION_COPY);
	if (copy)
	{
		OpInputContext * context = g_input_manager->GetKeyboardInputContext();
		if (!context) {
			context = g_application->GetInputContext();
		}
		g_input_manager->InvokeAction(copy, context);
	}
	clip->SetServicePasteboard(NULL, NULL);
	return YES;
}

#endif // defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)

@end

