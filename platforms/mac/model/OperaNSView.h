/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPERA_NS_VIEW_H
#define OPERA_NS_VIEW_H

#include "modules/pi/OpWindow.h"
#include "modules/pi/OpInputMethod.h"

#define BOOL NSBOOL
#import <AppKit/NSView.h>
#import <QuartzCore/CALayer.h>
#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKEND_OPENGL)
#import <AppKit/NSOpenGLView.h>
#endif
#import <AppKit/NSInputManager.h>
#import <AppKit/NSDragging.h>
#import <AppKit/NSAccessibility.h>
#undef BOOL

// Ugly but nessecary to avoid warnings.
#undef YES
#undef NO

#define FIRST_OTHER_MOUSE_BUTTON 2
#define NUM_MOUSE_BUTTONS 9

class OpInputAction;
class OpInputContext;

struct action_handler {
	OpInputAction *action;
	OpInputContext *context;
};

class CocoaMDEScreen;
class DesktopDragObject;

@interface OperaNSView : NSView <NSTextInput>
{
	CocoaMDEScreen*		_screen;
	NSTrackingRectTag	trackingRect;
	OpWindow::Style		_opera_style;
	BOOL				_unified_toolbar;
	OpInputMethodString*_imstring;
	IM_WIDGETINFO*		_widgetinfo;
	OpAccessibleItem*	_accessible_item;
	BOOL				_fake_right_mouse;
	BOOL				_cycling;
	int					_right_mouse_count;
	BOOL				_privacy_mode;
	INT32				_scale_adjustments;
	INT32				_prev_scale;
	BOOL				_performing_gesture;
	BOOL				_handling_mouse_down;
	BOOL				_ime_sel_changed;
	BOOL				_ime_hide_enter;
	OpPoint				_last_drag;
	BOOL				_first_gesture_event;
	BOOL				_mouseButtonIsDown[NUM_MOUSE_BUTTONS];
	int					_mouse_down_x;
	int					_mouse_down_y;
	BOOL				_can_be_dragged;
	BOOL				_has_been_dropped;
	OpView*				_initial_drag_view;
	CALayer*			_comp_layer;
	CALayer*			_content_layer;
	CALayer*			_background_layer;
	CALayer*			_plugin_clip_layer;
	BOOL				_validate_started;
	int					_plugin_z_order;
	INT32				_left_padding;
	INT32				_top_padding;
	INT32				_right_padding;
	INT32				_bottom_padding;
	BOOL				_momentum_active;
}

- (id)initWithFrame:(NSRect)frameRect operaStyle:(OpWindow::Style)opera_style unifiedToolbar:(BOOL)unified_toolbar;
- (CocoaMDEScreen*)screen;
- (void)setScreen:(CocoaMDEScreen*)screen;
- (void)abandonInput;
- (BOOL)fake_right_mouse;
- (void)drawFrame:(NSRect)rect;
- (void)initDrawFrame;
- (BOOL)isUnifiedToolbar;
- (void)resetRightMouseCount;
- (void)setPrivacyMode:(BOOL)privacyMode;
- (BOOL)isPrivacyMode;
- (void)invalidateAll;
- (void)setAlpha:(float)alpha;
- (void)setAccessibleItem:(OpAccessibleItem*)item;
- (void)openUrl:(const uni_char*)urlString view:(OpView*)opView newTab:(BOOL)openInNewTab;
- (BOOL)tryToOpenWebloc:(const uni_char*)urlString view:(OpView*)opView;
- (void)releaseMouseCapture;
- (void)releaseOtherMouseButtons:(NSEvent *)theEvent;
- (void)setPaddingLeft:(INT32)left_padding andTop:(INT32)top_padding andRight:(INT32)right_padding andBottom:(INT32)bottom_padding;

- (void)addSublayer:(CALayer *)layer;

- (void)setValidateStarted:(BOOL)validateStarted;
- (void)updatePluginZPosition:(CALayer *)layer;

- (void)setNeedsDisplayForLayers:(BOOL)needsDisplay;

- (void)beginGestureWithEvent:(NSEvent *)theEvent;
- (void)endGestureWithEvent:(NSEvent *)theEvent;
- (void)stopScrollMomentum;
- (void)swipeWithEvent:(NSEvent *)theEvent;
- (void)rotateWithEvent:(NSEvent *)theEvent;
- (void)magnifyWithEvent:(NSEvent *)theEvent;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
- (void)viewDidChangeBackingProperties;
- (void)updateContentsScales;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
@end

@interface OperaNSView(NSDraggingDestination)
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender;
- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender;
- (void)draggingExited:(id <NSDraggingInfo>)sender;
- (void)draggingEnded:(id <NSDraggingInfo>)sender;
- (NSBOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender;
- (NSBOOL)performDragOperation:(id <NSDraggingInfo>)sender;
- (void)concludeDragOperation:(id <NSDraggingInfo>)sender;
@end

@interface OperaNSView(NSDraggingSource)
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination;
@end

@interface OperaNSView(NSDrag)
- (void)dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag;
@end

#endif // OPERA_NS_WINDOW_H
