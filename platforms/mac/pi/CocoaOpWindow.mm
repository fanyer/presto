/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/pi/OpView.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/skin/opskinmanager.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_pi/DesktopOpUiInfo.h"
#include "adjunct/quick/Application.h"

#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/CocoaMDEScreen.h"
#include "platforms/mac/model/OperaNSWindow.h"
#include "platforms/mac/model/OperaNSView.h"
#include "platforms/mac/model/OperaWindowDelegate.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/subclasses/cursor_mac.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/model/CocoaOperaListener.h"
#include "platforms/mac/pi/MacOpScreenInfo.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/pi/CocoaOpDragManager.h"

#define BOOL NSBOOL
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSMenu.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#undef BOOL

OpPointerHashTable<void, CocoaOpWindow> CocoaOpWindow::s_adopted_windows;
NSModalSession _modalsession = NULL;
CocoaOpWindow* CocoaOpWindow::s_current_popup = NULL;
CocoaOpWindow* CocoaOpWindow::s_current_extension_popup = NULL;

OpPoint CocoaOpWindow::s_last_mouse_down = OpPoint(0,0);
void* CocoaOpWindow::s_last_mouse_down_event = nil;
float CocoaOpWindow::s_alpha = 1.0f;
BOOL CocoaOpWindow::s_force_first_responder = FALSE;

static NSMutableArray *f_blockingWindowsStack = nil;

OP_STATUS DesktopOpWindow::Create(DesktopOpWindow** new_window)
{
	*new_window = new CocoaOpWindow();
	return *new_window ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#if !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7)
enum {
	NSFullScreenWindowMask = 1 << 14,
	NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,
	NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8
};
#endif // MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

CocoaOpWindow::CocoaOpWindow()
	: m_screen(0),
	  m_vega(0),
	  m_parent_view(0),
	  m_parent_window(0),
	  m_data(0),
	  m_temp_data(0),
	  m_style(STYLE_DESKTOP),
	  m_type(OpTypedObject::WINDOW_TYPE_UNKNOWN),
	  m_effect(0),
	  m_auto_resize_window(FALSE),
	  m_animated_setframe(nil)
{
	if (f_blockingWindowsStack == nil)
	{
		f_blockingWindowsStack = [NSMutableArray new];
	}
	else
	{
		[f_blockingWindowsStack retain];
	}

}

CocoaOpWindow::~CocoaOpWindow()
{
	if (this == s_current_popup)
		s_current_popup = NULL;

	if (this == s_current_extension_popup)
		s_current_extension_popup = NULL;

	if(m_style == STYLE_DESKTOP)
	{
		g_skin_manager->RemoveSkinNotficationListener(this);
		g_skin_manager->RemoveTransparentBackgroundListener(this);
	}
	if (m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		OperaWindowDelegate *delegate = [win delegate];
		if (delegate) {
			[win setDelegate:nil];
			[delegate release];
		}
		for(unsigned int i=0; i<m_tooltips.GetCount(); i++)
		{
			CocoaOpWindow *tooltip = m_tooltips.Get(i);
			if (tooltip)
			{
				[win removeChildWindow:(NSWindow*)(tooltip->m_data)];
				tooltip->TooltipParentDeleted();
			}
		}
		if (m_style == OpWindow::STYLE_TOOLTIP || m_style == OpWindow::STYLE_NOTIFIER)
		{
			if (m_parent_window)
				((CocoaOpWindow*)m_parent_window)->RemoveTooltipWindow(this);
			m_parent_window = NULL;
		}

		// If we were a blocking window we need to remove ourselves from the list
		[f_blockingWindowsStack removeObject:win];

		// If this window has a parent we need to remember to remove it
		// otherwise the window won't close
		if (m_parent_window)
		{
			CocoaOpWindow *cocoa_parent = (CocoaOpWindow *)m_parent_window;
			if (cocoa_parent->GetNativeWindow())
				[cocoa_parent->GetNativeWindow() removeChildWindow:win];
		}
		OperaNSView* content = [win contentView];
		if(content)
		{
			[content setScreen:NULL];
		}
	}
	if(m_screen)
	{
		if(mdeWidget && mdeWidget->m_parent)
		{
			mdeWidget->m_parent->RemoveChild(mdeWidget);
		}
		delete m_screen;
	}
	delete m_vega;
	if ([f_blockingWindowsStack retainCount] == 1)
	{
		[f_blockingWindowsStack release];
		f_blockingWindowsStack = nil;
	}
	else {
		[f_blockingWindowsStack release];
	}
	[m_animated_setframe release];
}

void CocoaOpWindow::Invalidate(MDE_Region *update_region)
{
	if (m_data && update_region)
	{
		NSWindow *win = (NSWindow *)m_data;
		NSView *view = [win contentView];
		for (int i = 0; i<update_region->num_rects; i++)
		{
			MDE_RECT mderect = update_region->rects[i];
			NSRect rect = {{mderect.x, [view frame].size.height-(mderect.y+mderect.h)},{mderect.w, mderect.h}};
			[view setNeedsDisplayInRect:rect];
		}
	}
}

WindowRef CocoaOpWindow::GetCarbonWindow()
{
	if (m_data) {
		NSWindow *win = (NSWindow *)m_data;
		return (WindowRef)[win windowRef];
	}
	return NULL;
}

void CocoaOpWindow::GetContentPos(INT32* x, INT32* y)
{
	if (m_data) {
		NSWindow *win = (NSWindow *)m_data;
		float windowHeight = [win frame].size.height;
		NSView *contentView = [win contentView];
		NSRect contentRect = [contentView convertRect:[contentView frame] toView:nil]; // convert to window-relative coordinates
		*x = contentRect.origin.x;
		*y = windowHeight - NSMaxY(contentRect);
	}
}

NSView* CocoaOpWindow::GetContentView()
{
	if (m_data) {
		NSWindow *win = (NSWindow *)m_data;
		return [win contentView];
	}
	return NULL;
}

void CocoaOpWindow::ResetRightMouseCount()
{
	if (m_data) {
		NSWindow *win = (NSWindow *)m_data;
		[[win contentView] resetRightMouseCount];
	}
}

void CocoaOpWindow::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	if (GetDesktopWindow() && GetDesktopWindow()->GetWidgetContainer() && GetDesktopWindow()->GetSkinImage())
		GetDesktopWindow()->GetSkinImage()->GetPadding(left, top, right, bottom);
}

OP_STATUS CocoaOpWindow::Init(Style style, OpTypedObject::Type type, OpWindow* parent_window, OpView* parent_view, void* native_handle, UINT32 effect)
{
	m_style = style;
	m_type = type;
	m_parent_window = parent_window;
	m_parent_view = parent_view;

#ifndef VEGAOPPAINTER_MDI
	if (!native_handle && (parent_view || parent_window) &&
		(style == STYLE_DESKTOP || style == STYLE_CHILD || style == STYLE_EMBEDDED || style == STYLE_WORKSPACE || style == STYLE_TRANSPARENT))
	{
		m_data = NULL;
		return MDE_OpWindow::Init(style, type, parent_window, parent_view, NULL, effect);
	}
#endif // VEGAOPPAINTER_MDI

	if (!native_handle)
	{
		// Set the default attributes
		unsigned int	win_style = NSBorderlessWindowMask;
		int				win_level = NSNormalWindowLevel;
		NSBOOL			ignore_mouse_events = NO;
		NSBOOL			has_shadow = YES;
		float			alpha_value = 1.0f;
		NSColor			*win_colour = [NSColor windowBackgroundColor];
		NSBOOL			win_opaque = YES;
		BOOL			unified_toolbar = FALSE;
		BOOL			invisible_titlebar = FALSE;
		BOOL			is_blocking = FALSE;
		switch(style)
		{
			case STYLE_DESKTOP:
				{
					win_style |= NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;
					unified_toolbar = TRUE;
					g_skin_manager->SetTransparentBackgroundColor(0);
					g_skin_manager->AddSkinNotficationListener(this);

					// Make a windows Menu, this is stupid but it doesn't work if this is called in the NSApplication
					// ApplicationRun, only seems to work here, but the menu is freed in ApplicationTerminate
					// This is needed to make the open windows appear in the Dock Menu
					if ([[NSApplication sharedApplication] windowsMenu] == nil)
						[[NSApplication sharedApplication] setWindowsMenu:[[NSMenu alloc] init]];
				}
				break;

			case STYLE_TOOLTIP:
				ignore_mouse_events = TRUE;
				// This fall though is correct so that notifiers can use the mouse
				// to click them but this isn't needed for tooltips
			case STYLE_NOTIFIER:
				if (m_parent_window == NULL && g_application->GetActiveDesktopWindow() != NULL)
				{
//					m_parent_window = g_application->GetActiveDesktopWindow()->GetOpWindow();
				}
				if (m_parent_window)
					((CocoaOpWindow*)m_parent_window)->AddTooltipWindow(this);
				break;

			case STYLE_OVERLAY:
				ignore_mouse_events = YES;
				has_shadow = NO;
				break;

			case STYLE_GADGET:
			case STYLE_HIDDEN_GADGET:
				win_style |= NSTitledWindowMask;
				if (parent_window && parent_window->GetStyle() == STYLE_DESKTOP) win_level = NSModalPanelWindowLevel;
				invisible_titlebar = TRUE;
				has_shadow = NO;
				win_opaque = NO;
				win_colour = [NSColor clearColor];
				break;

#ifdef DECORATED_GADGET_SUPPORT
			case STYLE_DECORATED_GADGET:

				win_style |= NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;
				unified_toolbar = TRUE;
				win_colour = [NSColor blackColor];
				break;
#endif // DECORATED_GADGET_SUPPORT

			case STYLE_EXTENSION_POPUP:
				if (s_current_extension_popup)
				{
					s_current_extension_popup->Close(TRUE);
					OP_ASSERT(!s_current_extension_popup);
				}
				s_current_extension_popup = this;
				win_level = NSModalPanelWindowLevel;

				break;

			case STYLE_POPUP:
				if (s_current_popup) {
					s_current_popup->Close(TRUE);
					OP_ASSERT(!s_current_popup);
				}
				s_current_popup = this;
				break;

			case STYLE_MODAL_DIALOG:
				if (parent_window && [((NSWindow*)((CocoaOpWindow*)parent_window)->m_data) isMiniaturized])
					[((NSWindow*)((CocoaOpWindow*)parent_window)->m_data) deminiaturize:nil];
				win_style |= NSTitledWindowMask | NSClosableWindowMask;
				break;

			case STYLE_MODELESS_DIALOG:
				if (parent_window && [((NSWindow*)((CocoaOpWindow*)parent_window)->m_data) isMiniaturized])
					[((NSWindow*)((CocoaOpWindow*)parent_window)->m_data) deminiaturize:nil];
				win_style |= NSTitledWindowMask | NSClosableWindowMask;
				win_level = NSModalPanelWindowLevel;
				break;

			case STYLE_BLOCKING_DIALOG:
				if (parent_window && [((NSWindow*)((CocoaOpWindow*)parent_window)->m_data) isMiniaturized])
					[((NSWindow*)((CocoaOpWindow*)parent_window)->m_data) deminiaturize:nil];
				win_style |= NSTitledWindowMask;
				win_level = NSModalPanelWindowLevel;
				is_blocking = TRUE;
				break;

			case STYLE_CONSOLE_DIALOG:
				win_style |= NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;
				break;

			case STYLE_CHILD:
			case STYLE_EMBEDDED:
				break;

			case STYLE_WORKSPACE:
				if (parent_window && parent_window->GetStyle() == STYLE_WORKSPACE)
					m_auto_resize_window = TRUE;
				break;

			default	:
				OP_ASSERT(!"Do we have a new style now? If so it is not implemented yet.");
				break;
		}

		if (effect & OpWindow::EFFECT_TRANSPARENT)
		{
			win_style |= NSTitledWindowMask;
			invisible_titlebar = TRUE;
			win_opaque = NO;
			has_shadow = NO;
			win_colour = [NSColor clearColor];
		}

		NSRect rect = NSMakeRect(0,0,0,0);
		[NSApplication sharedApplication];
		OperaNSWindow *win = [[OperaNSWindow alloc] initWithContentRect:rect styleMask:win_style backing:NSBackingStoreBuffered defer:NO];
		if (is_blocking)
		{
			[f_blockingWindowsStack addObject:win];
		}

		[win setUnifiedToolbarHeight:(win_style & NSTitledWindowMask) ? MAC_TITLE_BAR_HEIGHT : 0];
		[win setOperaStyle:style];
		[win setAlphaValue:(s_alpha * alpha_value)];
		[win setOpaque:win_opaque];
		if (GetOSVersion() >= 0x1070) {
			if (m_parent_window || m_parent_view)
				[win setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary];
			else if (style == STYLE_DESKTOP)
				[win setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
		}
		rect.origin.x = rect.origin.y = 0;
		OperaNSView* imageView = [[OperaNSView alloc] initWithFrame:rect operaStyle:style unifiedToolbar:unified_toolbar];
		if (style == STYLE_DESKTOP && GetDesktopWindow())
			[imageView setPrivacyMode:GetDesktopWindow()->PrivacyMode()];
		[win setContentView:[imageView autorelease]];
		[imageView becomeFirstResponder];
		[imageView initDrawFrame];
		[win setAcceptsMouseMovedEvents:YES];
		// This makes NO sense but you can't call setIgnoresMouseEvents:YES otherwise the transparent
		// areas on gagdets cannot be clicked through
		if (ignore_mouse_events == YES)
			[win setIgnoresMouseEvents:ignore_mouse_events];
		[win setLevel:win_level];
		[win setHasShadow:has_shadow];
		[win setBackgroundColor:win_colour];
		[win setDelegate:[[OperaWindowDelegate alloc] init]];
		[win useOptimizedDrawing:NO];
		[win setInvisibleTitlebar:invisible_titlebar];

		m_data = win;
		m_vega = new CocoaVEGAWindow(this);
		m_screen = new CocoaMDEScreen(this, m_vega);
		RETURN_IF_ERROR(m_screen->Init());
		[imageView setScreen:m_screen];

		if (style == STYLE_EXTENSION_POPUP)
			m_screen->BypassUpdateLock();

		RETURN_IF_ERROR(MDE_OpWindow::Init(style, type, NULL, NULL, m_screen));

		if(style == STYLE_DESKTOP)
			g_skin_manager->AddTransparentBackgroundListener(this);
	}
    else // if native_handle
    {
	    m_data = native_handle;

		if (style == STYLE_ADOPT)
			return s_adopted_windows.Add(m_data, this);

		return OpStatus::OK;
	}

	return OpStatus::OK;
}

void CocoaOpWindow::SetParent(OpWindow* parent_window, OpView* parent_view, BOOL behind)
{
	m_parent_window = parent_window;
	m_parent_view = parent_view;
	MDE_OpWindow::SetParent(parent_window, parent_view, behind);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
void CocoaOpWindow::SetAccessibleItem(OpAccessibleItem* accessible_item)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		[[win contentView] setAccessibleItem:accessible_item];
	}
}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

OpWindow* CocoaOpWindow::GetParentWindow()
{
	return m_parent_window;
}

OpView* CocoaOpWindow::GetParentView()
{
	return m_parent_view;
}

OpWindow* CocoaOpWindow::GetRootWindow()
{
	if (GetParentView())
		return GetParentView()->GetRootWindow();
	else if (GetParentWindow() && !IsNativeWindow())
		return GetParentWindow()->GetRootWindow();
	return this;
}

void CocoaOpWindow::SetWindowListener(OpWindowListener *windowlistener)
{
	if (m_data) {
		NSWindow *win = (NSWindow *)m_data;
		[(OperaWindowDelegate*) [win delegate] setListener:windowlistener];
	}
	MDE_OpWindow::SetWindowListener(windowlistener);
}

#ifdef OPWINDOW_DESKTOP_FUNCTIONS
void CocoaOpWindow::SetDesktopPlacement(const OpRect& rect, State state, BOOL inner, BOOL behind, BOOL center)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		
		// DSK-343908
		if ([win styleMask] & NSFullScreenWindowMask)
			return;

		if (center)
		{
			if (inner)
				SetInnerSize(rect.width, rect.height);
			else
				SetOuterSize(rect.width, rect.height);

			[win center];
		}
		else
		{
			if (inner)
				SetInnerPosAndSize(rect.x, rect.y, rect.width, rect.height);
			else
				SetOuterPosAndSize(rect.x, rect.y, rect.width, rect.height);
		}

		NSWindow* currentWindow = [NSApp mainWindow];
		Show(TRUE);
		if (behind) [currentWindow makeKeyAndOrderFront:nil];

#ifdef RESTORE_MAC_FULLSCREEN_ON_STARTUP
		if (state == OpWindow::FULLSCREEN && GetOSVersion() >= 0x1070)
			[win toggleFullScreen:nil];
#endif //RESTORE_MAC_FULLSCREEN_ON_STARTUP

		if (m_style == STYLE_BLOCKING_DIALOG)
		{
			if (_modalsession)	// opening multiple modal sessions is NOT supported, but don't crash or deadlock anyway...
			{
				[NSApp stopModal];
				[NSApp endModalSession:(NSModalSession)_modalsession];
			}
			_modalsession = (NSModalSession)[NSApp beginModalSessionForWindow:win];
			int result = NSRunContinuesResponse;

			CocoaOperaListener tempListener;
			// Loop until some result other than continues:
			while (result == NSRunContinuesResponse)
			{
				// Run the window modally until there are no events to process:
				if(_modalsession)
					result = [NSApp runModalSession:(NSModalSession)_modalsession];
				else
					result = NSRunStoppedResponse;

				// Give the main loop some time:
				if(result == NSRunContinuesResponse)
				{
					[[NSRunLoop currentRunLoop] limitDateForMode:NSDefaultRunLoopMode];
				}

				usleep(17000);
			}
		}
	}
	else
	{
		// Hack to not use the crazy new MDI windows on Mac, just never pass the restored state to MDE_OpWindow
		if (m_style == STYLE_DESKTOP && state == OpWindow::RESTORED)
			state = OpWindow::MAXIMIZED;

		MDE_OpWindow::SetDesktopPlacement(rect, state, inner, behind, center);
	}
}

void CocoaOpWindow::GetDesktopPlacement(OpRect& rect, State& state)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		if([win isMiniaturized])
			state = OpWindow::MINIMIZED;
#ifdef RESTORE_MAC_FULLSCREEN_ON_STARTUP
		else if (GetOSVersion() >= 0x1070 && ([win styleMask] & NSFullScreenWindowMask) && [win systemFullscreen])
			state = OpWindow::FULLSCREEN;
#endif //RESTORE_MAC_FULLSCREEN_ON_STARTUP
		else if (([win styleMask] & NSFullScreenWindowMask) && ![win fullscreen]) // NSFullScreenWindowMask)
			state = OpWindow::MAXIMIZED;
		else
			state = OpWindow::RESTORED;
		GetOuterPos(&rect.x, &rect.y);
		GetOuterSize((UINT32*)&rect.width, (UINT32*)&rect.height);
	}
	else
		MDE_OpWindow::GetDesktopPlacement(rect, state);

	// Trying to prevent the effects of DSK-300752, do not let shit happen
	rect.width = MAX(100,rect.width);
	rect.height = MAX(100,rect.height);
}

OpWindow::State CocoaOpWindow::GetRestoreState() const
{
	if (m_data)
	{
		NSWindow* win = (NSWindow*) m_data;
		if ([win isZoomed])
			return OpWindow::MAXIMIZED;
		if ([win isMiniaturized])
			return OpWindow::MINIMIZED;
		if (GetOSVersion() >= 0x1070 && ([win styleMask] & NSFullScreenWindowMask))
			return OpWindow::FULLSCREEN;
		return OpWindow::RESTORED;
	}

	return MDE_OpWindow::GetRestoreState();
}

void CocoaOpWindow::SetRestoreState(State state)
{
	if (state == OpWindow::MINIMIZED)
		Minimize();
	else if (state == OpWindow::MAXIMIZED)
		Maximize();
	else if (GetOSVersion() >= 0x1070 && state == OpWindow::FULLSCREEN)
		Fullscreen();
	else
		MDE_OpWindow::SetRestoreState(state);
}

void CocoaOpWindow::Restore()
{
	if (m_data)
	{
		OperaNSWindow* win = static_cast<OperaNSWindow*>(m_data);
		if (GetOSVersion() >= 0x1070 && ([win styleMask] & NSFullScreenWindowMask))
			SetFullscreenMode(FALSE);
		else if ([win isZoomed])
			[win performZoom:nil];
		else if ([win isMiniaturized])
			[win deminiaturize:nil];
	}
	else
		MDE_OpWindow::Restore();
}

void CocoaOpWindow::Minimize()
{
	if (m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		[win miniaturize:nil];
	}
	else
	{
		// This is a hack, but sometimes minimize action gets to wrong window
		// in gadget so we need to check if this window is transparent
		// and parent window is gadget to call it properly
		if (m_style == OpWindow::STYLE_TRANSPARENT &&
			m_parent_window && m_parent_window->GetStyle() == OpWindow::STYLE_GADGET)
		{
			m_parent_window->Minimize();
		}
		else
			MDE_OpWindow::Minimize();
	}
}

void CocoaOpWindow::Maximize()
{
	if (m_data)
	{
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		if (!([win styleMask] & NSFullScreenWindowMask) && ![win isZoomed])
		{
			[win performZoom:nil];
		}
	}
	else
		MDE_OpWindow::Maximize();
}

void CocoaOpWindow::Fullscreen()
{
	if (m_data)
		SetFullscreenMode(TRUE);
	else
		MDE_OpWindow::Fullscreen();
}

void CocoaOpWindow::SetFullscreenMode(BOOL fullscreen)
{
	if (m_data)
	{
		if (GetOSVersion() >= 0x1070) {
			OperaNSWindow *win = (OperaNSWindow *)m_data;
			[win setFullscreen:fullscreen];
			if (fullscreen ^ ([win styleMask] & NSFullScreenWindowMask))
				[win toggleFullScreen:nil];
		}
		else if (fullscreen && !m_temp_data)
		{
			if (s_current_extension_popup)
				s_current_extension_popup->Close(TRUE);
			Show(FALSE);

			// Save out the current window
			OperaNSWindow *win = (OperaNSWindow *)m_data;
			m_temp_data = win;

			// Create a new window
			OperaNSWindow *fswin = [[OperaNSWindow alloc] initWithContentRect:[[win screen] frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
			m_data = fswin;

			// Set the fullscreen mode
			[fswin setFullscreen:YES];

			// Swap over the contentView and delegate
			OperaNSView* content = [win contentView];
			[content retain];
			[content removeFromSuperview];
			[fswin setContentView: content];
			[content release];
			[fswin setOperaStyle:STYLE_DESKTOP];
			[fswin setDelegate:[win delegate]];
			[win setDelegate:nil];
			[fswin setAcceptsMouseMovedEvents:YES];

			// Hide the menu and dock
			if (GetOSVersion() >= 0x1060) {
				@try {
					[NSApp setPresentationOptions:(NSApplicationPresentationAutoHideDock)|(NSApplicationPresentationAutoHideMenuBar)];
				} @catch (NSException *exception) {
					[NSMenu setMenuBarVisible:NO];
				}
			}
			else
				[NSMenu setMenuBarVisible:NO];

			Show(TRUE);
		}
		else if (!fullscreen && m_temp_data)
		{
			Show(FALSE);

			// Move back in the real window
			OperaNSWindow *win = (OperaNSWindow *)m_temp_data;
			OperaNSWindow *fswin = (OperaNSWindow *)m_data;

			// Swap over the contentView and delegate
			[win setContentView: [fswin contentView]];
			[win setDelegate:[fswin delegate]];
			[fswin setDelegate:nil];

			// Save back into the data pointer
			m_data = win;

			// Kill the window
			[fswin release];
			m_temp_data = NULL;

			// Show the menu and dock
			if (GetOSVersion() >= 0x1060)
				[NSApp setPresentationOptions:0]; // NSApplicationPresentationAutoHideMenuBar, NSApplicationPresentationFullScreen, NSApplicationPresentationAutoHideToolbar
			else
				[NSMenu setMenuBarVisible:YES];

			Show(TRUE);
		}
	}
}

void CocoaOpWindow::LockUpdate(BOOL lock)
{


}

void CocoaOpWindow::SetFloating(BOOL floating)
{
	if (m_data)
	{
		NSWindow *win = (NSWindow *)m_data;

		if (floating)
		{
			[[win standardWindowButton:NSWindowMiniaturizeButton] setEnabled:NO];
			[win setLevel:NSModalPanelWindowLevel];
		}
		else
		{
			[[win standardWindowButton:NSWindowMiniaturizeButton] setEnabled:YES];
			[win setLevel:NSNormalWindowLevel];
		}
	}
}

void CocoaOpWindow::SetAlwaysBelow(BOOL below)
{
	if (m_data)
	{
		NSWindow *win = (NSWindow *)m_data;

		if (below)
		{
			[[win standardWindowButton:NSWindowMiniaturizeButton] setEnabled:NO];
			[win setLevel:kCGDesktopIconWindowLevel];
		}
		else
		{
			[[win standardWindowButton:NSWindowMiniaturizeButton] setEnabled:YES];
			[win setLevel:NSNormalWindowLevel];
		}
	}
}

void CocoaOpWindow::SetShowInWindowList(BOOL in_window_list)
{
}

#endif // OPWINDOW_DESKTOP_FUNCTIONS

void CocoaOpWindow::AddTooltipWindow(CocoaOpWindow* tooltip)
{
	if(m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		if(OpStatus::IsSuccess(m_tooltips.Add(tooltip)))
		{
			[win addChildWindow:(NSWindow *)(tooltip->m_data) ordered:NSWindowAbove];
			return;
		}
	}
	OP_ASSERT(!"Should never happen");
}

void CocoaOpWindow::RemoveTooltipWindow(CocoaOpWindow* tooltip)
{
	if(m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		if(OpStatus::IsSuccess(m_tooltips.RemoveByItem(tooltip)))
		{
			[win removeChildWindow:(NSWindow *)(tooltip->m_data)];
			return;
		}
	}
	OP_ASSERT(!"Should never happen");
}

void CocoaOpWindow::TooltipParentDeleted()
{
	if (m_data && (m_style == OpWindow::STYLE_TOOLTIP || m_style == OpWindow::STYLE_NOTIFIER))
	{
		m_parent_window = NULL;
	}
}

void CocoaOpWindow::Show(BOOL show)
{
	if (m_data)
	{
		NSWindow *win = (NSWindow *)m_data;

		SetWidgetVisibility(show);

		// Super hack but then NSWindow is so dumb it doesn't tell us when it is shown
		[(OperaWindowDelegate*)[win delegate] getListener]->OnShow(show);

		if (show) {
			// If this window has a parent then we need to make this window a child of it to
			// get the window leveling correct
			if (m_style == OpWindow::STYLE_TOOLTIP || m_style == OpWindow::STYLE_NOTIFIER)
			{
				if (m_parent_window)
					((CocoaOpWindow*)m_parent_window)->RemoveTooltipWindow(this);
				m_parent_window = g_application && g_application->GetActiveDesktopWindow() ? ((CocoaOpWindow*)g_application->GetActiveDesktopWindow()->GetOpWindow())->GetRootWindow() : NULL;
				if (m_parent_window)
					((CocoaOpWindow*)m_parent_window)->AddTooltipWindow(this);
			}
			else if (m_parent_window && ![win parentWindow])
			{
				s_force_first_responder = TRUE;
				CocoaOpWindow *cocoa_parent = (CocoaOpWindow *)m_parent_window;
				[cocoa_parent->GetNativeWindow() addChildWindow:win ordered:NSWindowAbove];
				s_force_first_responder = FALSE;
			}
			// Don't let tooltips or notifiers steal focus (this "fix" should go when we redo the modal dialog weirdness)
			if (m_style != OpWindow::STYLE_TOOLTIP && m_style != OpWindow::STYLE_NOTIFIER)
			{
				[win makeKeyAndOrderFront:nil];
			}
		}
		else
		{
			if (m_parent_window && [win parentWindow])
			{
				CocoaOpWindow *cocoa_parent = (CocoaOpWindow *)m_parent_window;
				[cocoa_parent->GetNativeWindow() removeChildWindow:win];
			}

			[win orderOut:nil];
		}
	}
	else
	{
		MDE_OpWindow::Show(show);
		SetWidgetVisibility(show);
	}

}

void CocoaOpWindow::SetFocus(BOOL focus)
{
	MDE_OpWindow::SetFocus(focus);
}

void CocoaOpWindow::Close(BOOL user_initiated)
{
	NSWindow *win = NULL;
	OpWindow::Style style = m_style;
	if(m_data)
	{
		win = (NSWindow *)m_data;
	}


	// Call the base class first so that when the listeners are called to collect data like
	// window sizes etc, the window still exists
	MDE_OpWindow::Close(user_initiated);

	if (win)
	{
		if (style == STYLE_BLOCKING_DIALOG && _modalsession)
		{
			[NSApp stopModal];
			[NSApp endModalSession:(NSModalSession)_modalsession];
			_modalsession = NULL;
		}
		[win setDelegate:nil];
		[win close];
	}
}

void CocoaOpWindow::GetOuterSize(UINT32* width, UINT32* height)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		NSRect frame = (([win styleMask] & NSFullScreenWindowMask) && ![win fullscreen]) ? [win restoreFrame] : [win frame];

		*width = frame.size.width;
		*height = frame.size.height;

		if ([win isInvisibleTitlebar])
			*height -= MAC_TITLE_BAR_HEIGHT;

		return;
	}
	MDE_OpWindow::GetOuterSize(width, height);
}

void CocoaOpWindow::SetOuterSize(UINT32 width, UINT32 height)
{
	if (m_data) {
		if(width == 0 && height == 0)
		{
			// Dont allow empty window
			return;
		}
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		int x = [win frame].origin.x;
		int y = [win frame].origin.y + [win frame].size.height - height;

		if ([win isInvisibleTitlebar])
		{
			y -= MAC_TITLE_BAR_HEIGHT;
			height += MAC_TITLE_BAR_HEIGHT;
		}

		int dy = [win frame].size.height - [[win contentView] frame].size.height;

		NSSize min_size = [win contentMinSize];
		width = MAX(width, min_size.width);
		height = MAX(height, min_size.height + dy);

		NSSize max_size = [win contentMaxSize];
		width = MIN(width, max_size.width);
		height = MIN(height, max_size.height + dy);

		INT32 left_padding = 0;
		INT32 top_padding = 0;
		INT32 right_padding = 0;
		INT32 bottom_padding = 0;
		GetPadding(&left_padding, &top_padding, &right_padding, &bottom_padding);
		[(OperaNSView*)[win contentView] setPaddingLeft:left_padding andTop:top_padding andRight:right_padding andBottom:bottom_padding];

		[win setFrame:NSMakeRect(x, y, width, height) display:(GetDesktopWindow()!=NULL?YES:NO)];
		return;
	}
	MDE_OpWindow::SetOuterSize(width, height);
}

void CocoaOpWindow::GetInnerSize(UINT32* width, UINT32* height)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		*width = [[win contentView] frame].size.width;
		*height = [[win contentView] frame].size.height;
		return;
	}
	MDE_OpWindow::GetInnerSize(width, height);
}

void CocoaOpWindow::SetOuterPosAndSize(INT32 x, INT32 y, UINT32 width, UINT32 height)
{
	if (m_data) {
		if ((width == 0 && height == 0) ||
			((height > 1000000 && width > 1000000) &&
			  (height != DEFAULT_SIZEPOS || width != DEFAULT_SIZEPOS)))
		{
			// Dont allow empty or oversized window, unless measurements are default sizepos
			return;
		}

		OperaNSWindow *win = (OperaNSWindow *)m_data;

		// use cocoa default size if given height is default sizepos
		height = ((height != DEFAULT_SIZEPOS)?height:[win frame].size.height);
		width =  ((width != DEFAULT_SIZEPOS) ? width:[win frame].size.width);

		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		INT32 v = [menu_screen frame].size.height - y -  height;

		if ([win isInvisibleTitlebar])
		{
			v += MAC_TITLE_BAR_HEIGHT;
			height += MAC_TITLE_BAR_HEIGHT;
		}

		int dy = [win frame].size.height - [[win contentView] frame].size.height;

		NSSize min_size = [win contentMinSize];
		width = MAX(width, min_size.width);
		height = MAX(height, min_size.height + dy);

		NSSize max_size = [win contentMaxSize];
		width = MIN(width, max_size.width);
		height = MIN(height, max_size.height + dy);

		[win setFrame:NSMakeRect(x, v, width, height) display:YES];
		return;
	}
	MDE_OpWindow::SetOuterPos(x, y);
	MDE_OpWindow::SetOuterSize(width, height);
}

void CocoaOpWindow::SetInnerPosAndSize(INT32 x, INT32 y, UINT32 width, UINT32 height)
{
	if (m_data) {
		[m_animated_setframe stopAnimation];
		[m_animated_setframe release];
		m_animated_setframe = nil;

		OperaNSWindow *win = (OperaNSWindow *)m_data;
		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		INT32 v = [menu_screen frame].size.height - y - height;
		INT32 dy = [win frame].size.height - [[win contentView] frame].size.height;
		INT32 h = height + dy;

		NSSize min_size = [win contentMinSize];
		width = MAX(width, min_size.width);
		h = MAX(h, min_size.height + dy);

		NSSize max_size = [win contentMaxSize];
		width = MIN(width, max_size.width);
		h = MIN(h, max_size.height + dy);

		if (![win frame].size.width || ![win frame].size.height)
		{
			// This is what we really want to do always,
			// now we limit it to cases where the window has no size yet
			[win setFrame:NSMakeRect(x,v,width,h) display:YES];
		}
		else
		{
			// Due to the slowness of the core animiation layers to update
			// we use an NSViewAnimation object to overlook the frame size change.
			// We then try to animate to the new size as fast as possible.
			NSDictionary *animation_dictionary = [[[NSDictionary alloc] initWithObjectsAndKeys:win, NSViewAnimationTargetKey, [NSValue valueWithRect:NSMakeRect(x,v,width,h)], NSViewAnimationEndFrameKey, nil] autorelease];
			NSArray *view_animations = [NSArray arrayWithObject:animation_dictionary];
			m_animated_setframe = [[NSViewAnimation alloc] initWithViewAnimations:view_animations];
			[m_animated_setframe setDuration:0.0];
			[m_animated_setframe startAnimation];
		}

		return;
	}
	MDE_OpWindow::SetInnerPos(x, y);
	MDE_OpWindow::SetInnerSize(width, height);
}

void CocoaOpWindow::SetInnerSize(UINT32 width, UINT32 height)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		int x = [win frame].origin.x;
		int y = [win frame].origin.y + [[win contentView] frame].size.height - height;
		int dy = [win frame].size.height - [[win contentView] frame].size.height;
		int h = height + dy;

		NSSize min_size = [win contentMinSize];
		width = MAX(width, min_size.width);
		h = MAX(h, min_size.height + dy);

		NSSize max_size = [win contentMaxSize];
		width = MIN(width, max_size.width);
		h = MIN(h, max_size.height + dy);

		[win setFrame:NSMakeRect(x,y,width,h) display:YES];
		return;
	}
	MDE_OpWindow::SetInnerSize(width, height);
}

void CocoaOpWindow::GetOuterPos(INT32* x, INT32* y)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		NSRect frame = (([win styleMask] & NSFullScreenWindowMask) && ![win fullscreen]) ? [win restoreFrame] : [win frame];
		*x = frame.origin.x;
		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		*y = [menu_screen frame].size.height-frame.origin.y-frame.size.height;

		if ([win isInvisibleTitlebar])
			*y += MAC_TITLE_BAR_HEIGHT;

		return;
	}
	MDE_OpWindow::GetOuterPos(x, y);
}

void CocoaOpWindow::SetOuterPos(INT32 x, INT32 y)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		INT32 v = [menu_screen frame].size.height - y - [win frame].size.height;

		if ([win isInvisibleTitlebar])
			v += MAC_TITLE_BAR_HEIGHT;

		[win setFrameOrigin:NSMakePoint(x, v)];
		return;
	}
	MDE_OpWindow::SetOuterPos(x, y);
}

void CocoaOpWindow::GetInnerPos(INT32* x, INT32* y)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		*x = [win frame].origin.x;
		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		*y = [menu_screen frame].size.height-[win frame].origin.y-[[win contentView] frame].size.height;
		return;
	}
	MDE_OpWindow::GetInnerPos(x, y);
}

void CocoaOpWindow::SetInnerPos(INT32 x, INT32 y)
{
	if (m_data) {
		OperaNSWindow *win = (OperaNSWindow *)m_data;
		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		INT32 v = [menu_screen frame].size.height - y - [[win contentView] frame].size.height;
		[win setFrameOrigin:NSMakePoint(x, v)];
		return;
	}
	MDE_OpWindow::SetInnerPos(x, y);
}


#ifdef VIEWPORTS_SUPPORT
void CocoaOpWindow::GetRenderingBufferSize(UINT32* width, UINT32* height)
{
	GetInnerSize(width, height);
}
#endif // VIEWPORTS_SUPPORT

void CocoaOpWindow::SetMinimumInnerSize(UINT32 width, UINT32 height)
{
	if (m_data) {
		NSWindow *win = (NSWindow *)m_data;
		NSSize size = {width, height};
		[win setContentMinSize:size];
	}
	else
		MDE_OpWindow::SetMinimumInnerSize(width, height);
}

void CocoaOpWindow::SetMaximumInnerSize(UINT32 width, UINT32 height)
{
	if (m_data) {
		NSWindow *win = (NSWindow *)m_data;
		NSSize size = {width, height};
		[win setContentMaxSize:size];
	}
	else
		MDE_OpWindow::SetMaximumInnerSize(width, height);
}

void CocoaOpWindow::GetMargin(INT32* left_margin, INT32* top_margin, INT32* right_margin, INT32* bottom_margin)
{
	MDE_OpWindow::GetMargin(left_margin, top_margin, right_margin, bottom_margin);
}

void CocoaOpWindow::SetTransparency(INT32 transparency)
{
	MDE_OpWindow::SetTransparency(transparency);
}

void CocoaOpWindow::SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility)
{

}

void CocoaOpWindow::SetMenuBarVisibility(BOOL visible)
{
	MDE_OpWindow::SetMenuBarVisibility(visible);
}

BOOL CocoaOpWindow::GetMenuBarVisibility()
{
	return MDE_OpWindow::GetMenuBarVisibility();
}

void CocoaOpWindow::UpdateFont()
{
	MDE_OpWindow::UpdateFont();
}

void CocoaOpWindow::UpdateLanguage()
{
	MDE_OpWindow::UpdateLanguage();
}

void CocoaOpWindow::UpdateMenuBar()
{
	MDE_OpWindow::UpdateMenuBar();
}

void CocoaOpWindow::SetSkin(const char* skin)
{

}

void CocoaOpWindow::Redraw()
{
	MDE_OpWindow::Redraw();
}

OpWindow::Style CocoaOpWindow::GetStyle()
{
#ifdef VEGA_USE_HW
	return m_style;
#else
	return MDE_OpWindow::GetStyle();
#endif
}

BOOL CocoaOpWindow::HasInvisibleTitlebar()
{
	if (m_data)
		return [(OperaNSWindow *)m_data isInvisibleTitlebar];
	return FALSE;
}

void CocoaOpWindow::Raise()
{
	if(m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		[win orderFront:nil];
		return;
	}
	MDE_OpWindow::Raise();
}

void CocoaOpWindow::Lower()
{
	if(m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		[win orderBack:nil];
		return;
	}
	MDE_OpWindow::Lower();
}

void CocoaOpWindow::Activate()
{
	if(m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		if (m_style != STYLE_BLOCKING_DIALOG)
			[win makeKeyAndOrderFront:nil];
		return;
	}
	CocoaOpWindow* win = (CocoaOpWindow*)GetRootWindow();
	if (win && win->IsNativeWindow())
	{
		OperaNSView* content = [win->GetNativeWindow() contentView];
		if (content)
		{
			[content stopScrollMomentum];
		}
	}
	MDE_OpWindow::Activate();
}

void CocoaOpWindow::Deactivate()
{
	MDE_OpWindow::Deactivate();
}

void CocoaOpWindow::Attention()
{
	MDE_OpWindow::Attention();
}

#ifndef MOUSELESS
void CocoaOpWindow::SetCursor(CursorType cursor)
{
	if (!g_cocoa_drag_mgr->IsDragging())
	{
		gMouseCursor->SetCursor(cursor);
		MDE_OpWindow::SetCursor(cursor);
	}
}

CursorType CocoaOpWindow::GetCursor()
{
	return MDE_OpWindow::GetCursor();
}

#endif // !MOUSELESS
const uni_char* CocoaOpWindow::GetTitle() const
{
	return MDE_OpWindow::GetTitle();
}

void CocoaOpWindow::SetTitle(const uni_char* title)
{
	if (m_data && title) {
		NSWindow *win = (NSWindow *)m_data;
		NSString *str = [[NSString alloc] initWithCharacters:(const unichar*)title length:uni_strlen(title)];
		[win setTitle:str];
		[str release];
	}
	if (!title)
		MDE_OpWindow::SetTitle(UNI_L(""));
	else
		MDE_OpWindow::SetTitle(title);
}

void CocoaOpWindow::SetIcon(class OpBitmap* bitmap)
{
	MDE_OpWindow::SetIcon(bitmap);
}

void CocoaOpWindow::SetOpenInBackground(BOOL state)
{
	MDE_OpWindow::SetOpenInBackground(state);
	(void)state;
}

void CocoaOpWindow::SetResourceName(const char *name)
{
	MDE_OpWindow::SetResourceName(name);
}

#ifdef OPWINDOW_TOPMOST_WINDOW
BOOL CocoaOpWindow::IsActiveTopmostWindow()
{
	if(m_data)
	{
		NSWindow *win = (NSWindow *)m_data;
		return [win isKeyWindow];
	}
	OpWindow *rootWindow = GetRootWindow();
	return rootWindow?rootWindow->IsActiveTopmostWindow():FALSE;
}
#endif // OPWINDOW_TOPMOST_WINDOW

#ifdef PI_ANIMATED_WINDOWS
void CocoaOpWindow::SetWindowAnimationListener(OpWindowAnimationListener *listener)
{
}

void CocoaOpWindow::AnimateWindow(const OpWindowAnimation& animation)
{
}

void CocoaOpWindow::CancelAnimation()
{
}

#endif // PI_ANIMATED_WINDOWS

// DesktopOpWindow:
void CocoaOpWindow::OnVisibilityChanged(BOOL visibility)
{
}

void CocoaOpWindow::SetLockedByUser(BOOL locked_by_user)
{
}

void CocoaOpWindow::OnTransparentAreaChanged(int top, int bottom, int left, int right)
{
	CocoaOpWindow *cocoa_window = (CocoaOpWindow *)GetRootWindow();

	if (cocoa_window &&  cocoa_window->m_data)
	{
		OperaNSWindow *win = (OperaNSWindow *)(cocoa_window->m_temp_data?cocoa_window->m_temp_data:cocoa_window->m_data);

		[win setUnifiedToolbarHeight:top + MAC_TITLE_BAR_HEIGHT];
		[win setUnifiedToolbarLeft:left];
		[win setUnifiedToolbarRight:right];
		[win setUnifiedToolbarBottom:bottom];
		[[win contentView] setFrameSize:((NSView*)[win contentView]).frame.size];
		[[[win contentView] superview] setNeedsDisplay:YES];
	}
}


/////////////////////////////////////////////////////////////////////////////////////

OpPoint CocoaOpWindow::IntersectsGrowRegion(const OpRect& rect)
{
	OpPoint result(0,0);

	if (m_data && rect.width > 0 && rect.height > 0)
	{
		NSWindow *win = (NSWindow *)m_data;
		if ([win showsResizeIndicator] == YES && ([win styleMask] & NSResizableWindowMask))
		{
			// If the size has already been calculated don't bother again
			if (m_grow_size.x <= 0 || m_grow_size.y <= 0)
			{
				HIRect hirect;
				HIPoint pt = { 0, 0 };
				HIThemeGrowBoxDrawInfo growboxinfo;

				// Calculate the size of a standard grow control
				growboxinfo.version = 0;
				growboxinfo.state = kThemeStateActive;
				growboxinfo.kind = kHIThemeGrowBoxKindNormal;
				growboxinfo.direction = kThemeGrowRight | kThemeGrowDown;
				growboxinfo.size = kHIThemeGrowBoxSizeNormal;

				if (HIThemeGetGrowBoxBounds(&pt, &growboxinfo, &hirect) == noErr)
				{
					// Save and cache the result
					m_grow_size.x = hirect.size.width;
					m_grow_size.y = hirect.size.height;
				}
			}

			INT32 x, y;
			UINT32 width, height;
			GetOuterPos(&x, &y);
			GetOuterSize(&width, &height);

			INT32 y_diff = y + (INT32)height - (rect.y + (INT32)rect.height);

			if (y_diff < m_grow_size.y)
			{
				result.y = MAX(0, m_grow_size.y - y_diff);

				if (result.y)
				{
					INT32 x_diff = x + (INT32)width - (rect.x + (INT32)rect.width);

					result.x = MAX(0, m_grow_size.x - x_diff);
				}
			}
		}
	}

	return result;
}

/* static */
BOOL CocoaOpWindow::CloseCurrentPopupIfNot(void* nswin)
{
	OperaNSWindow *wnd = (OperaNSWindow *)nswin;
	BOOL consume = FALSE;

	if (s_current_popup && s_current_popup->m_data != nswin) {
		s_current_popup->Close(TRUE);
		OP_ASSERT(!s_current_popup);
	}

	if (s_current_extension_popup)
	{
		if (((s_current_extension_popup->m_data != nswin) ||
			(s_current_popup && s_current_popup->m_data != nswin)) &&
			[wnd getOperaStyle] != OpWindow::STYLE_GADGET &&
			[wnd getOperaStyle] != OpWindow::STYLE_HIDDEN_GADGET &&
			[wnd getOperaStyle] != OpWindow::STYLE_NOTIFIER &&
			[wnd getOperaStyle] != OpWindow::STYLE_MODAL_DIALOG
			)
		{
			consume = TRUE;
			s_current_extension_popup->Close(TRUE);
		}
	}

	return consume;
}

/* static */
BOOL CocoaOpWindow::ShouldPassMouseEvents(void *nswin)
{
	if (g_application == NULL)
		return FALSE;

	NSWindow *wnd = (NSWindow *)nswin;
	BOOL should = TRUE;
	CustomizeToolbarDialog* dialog = g_application ? g_application->GetCustomizeToolbarsDialog() : NULL;

	if (!dialog && !s_force_first_responder && f_blockingWindowsStack != nil &&
		[f_blockingWindowsStack count] > 0 &&
		[f_blockingWindowsStack objectAtIndex:([f_blockingWindowsStack count] - 1)] != wnd &&
		[f_blockingWindowsStack objectAtIndex:([f_blockingWindowsStack count] - 1)] != [wnd parentWindow])
	{
		should = FALSE;
	}

	return should;
}

/* static */
BOOL CocoaOpWindow::ShouldPassMouseMovedEvents(void *nswin, int x, int y)
{
	BOOL should = ShouldPassMouseEvents(nswin);

	if (should && s_current_popup && nswin != s_current_popup->m_data)
	{
		NSWindow *win = (NSWindow *)s_current_popup->m_data;
		NSRect r;
		r = [win frame];
		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		r.origin.y = [menu_screen frame].size.height-[win frame].origin.y-[win frame].size.height+MAC_TITLE_BAR_HEIGHT;
		// Check that we are actually outside the popup window. Do not pass events for other windows than the one we are hovering!
		should = (x < r.origin.x || y < r.origin.y || x > r.origin.x+r.size.width || y > r.origin.y+r.size.height);
	}

	if (should && s_current_extension_popup && nswin != s_current_extension_popup->m_data)
	{
		NSWindow *win = (NSWindow *)s_current_extension_popup->m_data;
		NSRect r;
		r = [win frame];
		// Always adjust the coords from the menu screen where the origins are
		NSScreen *menu_screen = [[NSScreen screens] objectAtIndex:0];
		r.origin.y = [menu_screen frame].size.height-[win frame].origin.y-[win frame].size.height+MAC_TITLE_BAR_HEIGHT;
		// Check that we are actually outside the extension popup window. Do not pass events for other windows than the one we are hovering!
		should = (x < r.origin.x || y < r.origin.y || x > r.origin.x+r.size.width || y > r.origin.y+r.size.height);
	}

	return should;
}

/* static */
void CocoaOpWindow::SetLastMouseDown(int x, int y, void* evt)
{
	s_last_mouse_down = OpPoint(x,y);
	if (s_last_mouse_down_event)
		[(id)s_last_mouse_down_event release];
	s_last_mouse_down_event = [(id)evt retain];
}
/* static */
BOOL CocoaOpWindow::HandleEnterLeaveFullscreen()
{
	if (GetOSVersion() >= 0x1070) {
		if ([OperaNSWindow systemFullscreenWindow])
		{
			[[OperaNSWindow systemFullscreenWindow] toggleFullScreen:nil];
			return TRUE;
		}
	}
	return FALSE;
}

void CocoaOpWindow::SetAlpha(float alpha)
{
	CocoaOpWindow *cocoa_window = (CocoaOpWindow *)GetRootWindow();

	if (cocoa_window &&  cocoa_window->m_data)
	{
		OperaNSWindow *win = (OperaNSWindow *)cocoa_window->m_data;

		[win setAlphaValue:alpha];
	}
}

void CocoaOpWindow::AbortInputMethodComposing()
{
	if (m_data)
	{
		[[(NSWindow*)m_data contentView] abandonInput];
	}
}

class DelayedFirstResponder : public OpDelayedAction
{
public:
	DelayedFirstResponder(CocoaOpWindow* win) : mWin(win) {}
	void DoAction()
		{
			mWin->m_posted_actions.RemoveByItem(static_cast<OpDelayedAction*>(this));
			if (mWin->GetNativeWindow())
			{
				[mWin->GetNativeWindow() makeFirstResponder:mWin->GetContentView()];
			}
		}
private:
	CocoaOpWindow* mWin;
};

BOOL CocoaOpWindow::GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out)
{
	// Nothing to do here on Mac as the main menu is always a parent to whatever menu
	// is showing so ListPopupMenuInfo collects all the items from it

	return FALSE;
}

void CocoaOpWindow::ResetFirstResponder()
{
	if (m_data)
	{
		[(NSWindow*)m_data makeFirstResponder:nil];
		m_posted_actions.Add(static_cast<OpDelayedAction*>(OP_NEW(DelayedFirstResponder,(this))));
	}
}

void CocoaOpWindow::OnPersonaLoaded(OpPersona *loaded_persona)
{
	CocoaOpWindow *cocoa_window = (CocoaOpWindow *)GetRootWindow();

	if (cocoa_window && cocoa_window->m_data)
	{
		OperaNSWindow *win = (OperaNSWindow *)(cocoa_window->m_temp_data?cocoa_window->m_temp_data:cocoa_window->m_data);

		[win setPersonaChanged];
		[[[win contentView] superview] setNeedsDisplay:YES];
	}
}

void CocoaOpWindow::OnBackgroundCleared(OpPainter* op_painter, const OpRect& clear_rect)
{
	CocoaOpWindow *cocoa_window = (CocoaOpWindow *)GetRootWindow();

	VEGAOpPainter *painter = static_cast<VEGAOpPainter *>(op_painter);
	if(m_data && painter && painter == cocoa_window->m_screen->GetVegaPainter())
	{
		BOOL has_persona_skin = g_skin_manager->HasPersonaSkin();
		if(has_persona_skin)
		{
			OperaNSWindow *win = (OperaNSWindow *)m_data;

			CGFloat win_height = [[win contentView] frame].size.height;

			OpBitmap* bitmap = [win getPersonaOpBitmapForWidth:[win frame].size.width andHeight:[win frame].size.height];
			if(bitmap)
			{
				CGFloat top_height = [win frame].size.height - win_height;
				OpRect rect(0, 0, bitmap->Width(), bitmap->Height());

				// translate to view coordinations
				rect.x += painter->GetTranslationX();
				rect.width -= painter->GetTranslationX();
				rect.y += painter->GetTranslationY() + top_height;
				rect.height -= (painter->GetTranslationY() + (int)top_height);

                // Make a nice white background to draw on
                op_painter->SetColor(0xFF, 0xFF, 0xFF, 0xFF);
                painter->FillRect(OpRect(0, 0, bitmap->Width(), bitmap->Height()));

				// The clip rect has already been set in the painter
				painter->DrawBitmapClipped(bitmap, rect, OpPoint(0, 0));

				// paint the overlay for the inactive window, if needed
				if (![win isMainWindow])
				{
					UINT32 color = g_desktop_op_ui_info->GetPersonaDimColor();
					op_painter->SetColor(color&0xFF, (color>>8)&0xFF, (color>>16)&0xFF, (color>>24)&0xFF);
					painter->FillRect(OpRect(0, 0, bitmap->Width(), bitmap->Height()));
				}
			}
		}
		else
		{
			UINT32 bgc = g_skin_manager->GetTransparentBackgroundColor();
			op_painter->SetColor(bgc&0xFF, (bgc>>8)&0xFF, (bgc>>16)&0xFF, (bgc>>24)&0xFF);
			painter->ClearRect(clear_rect);
		}
	}
}

void CocoaOpWindow::DeminiaturizeAnyWindow()
{
	BOOL demin = TRUE;

	for (unsigned int i = 0; i < [[NSApp windows] count]; ++i)
	{
		NSWindow* w = [[NSApp windows] objectAtIndex:i];
		if (![w isMiniaturized] && [w respondsToSelector:@selector(getOperaStyle)] && ([(OperaNSWindow*)w getOperaStyle] == OpWindow::STYLE_DESKTOP))
		{
			demin = FALSE;
			break;
		}
	}
	if (demin)
		for (unsigned int i = 0; i < [[NSApp windows] count]; ++i)
		{
			NSWindow* w = [[NSApp windows] objectAtIndex:i];
			if ([w isMiniaturized] && [w respondsToSelector:@selector(getOperaStyle)] && ([(OperaNSWindow*)w getOperaStyle] == OpWindow::STYLE_DESKTOP))
			{
				[w deminiaturize:nil];
				break;
			}
		}
}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
int CocoaOpWindow::GetPixelScale()
{
	if (GetOSVersion() >= 0x1070)
	{
		if (m_data)
		{
			OperaNSWindow *win = (OperaNSWindow*)m_data;
			CGFloat pixelscale = [win backingScaleFactor];
			return pixelscale * 100;
		}

		OpWindow* rootwin = GetRootWindow();
		if (rootwin && rootwin != this)
		{
			return rootwin->GetPixelScale();
		}
		else
		{
			// This really, really shouldn't happen. But we need to return a plausible value.
			OP_ASSERT(FALSE);
			CGFloat pixelscale = [[NSScreen mainScreen] backingScaleFactor];
			return pixelscale * 100;
		}
	}
	else
	{
		return 100;
	}
}
#endif //PIXEL_SCALE_RENDERING_SUPPORT

#ifdef _MAC_DEBUG
// Creates an image file (PNG) from given raw bitmap data.
// fname is a filename, not url.
BOOL CreateImageFromData(const void* rgba, unsigned int width, unsigned int height, const CFStringRef fname) {
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGContextRef bitmapContext = CGBitmapContextCreate(
													   (void*)rgba,
													   width,
													   height,
													   8, // bitsPerComponent
													   4*width, // bytesPerRow
													   colorSpace,
													   kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast);

	CFRelease(colorSpace);

	CGImageRef cgImage = CGBitmapContextCreateImage(bitmapContext);
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, fname, kCFURLPOSIXPathStyle, false);

	CFStringRef type = kUTTypePNG; // or kUTTypeBMP if you like
	CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, type, 1, 0);

	CGImageDestinationAddImage(dest, cgImage, 0);
	CFRelease(cgImage);
	CFRelease(bitmapContext);
	CFRelease(url);
	CGImageDestinationFinalize(dest);
	CFRelease(dest);
	return TRUE;
}
#endif	// _MAC_DEBUG
