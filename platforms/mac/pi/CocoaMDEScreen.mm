/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/vega_backends/opengl/vegaglapi.h"

#define __gl_h_

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
#include "modules/display/pixelscaler.h"
#include "modules/display/pixelscalepainter.h"
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#ifdef MDE_NATIVE_WINDOW_SUPPORT
#include "modules/libgogi/pi_impl/mde_native_window.h"
#endif // MDE_NATIVE_WINDOW_SUPPORT

#include "adjunct/quick/managers/AnimationManager.h"

#include "platforms/mac/model/OperaNSWindow.h"
#include "platforms/mac/model/OperaNSView.h"
#include "platforms/mac/model/SharedOpenGLContext.h"
#include "platforms/mac/pi/CocoaMDEScreen.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/pi/display/MacOpGlDevice.h"
#include "platforms/mac/pi/MacOpPrinterController.h"

#define BOOL NSBOOL
#import <Foundation/NSTimer.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSRunLoop.h>
#import <QuartzCore/CALayer.h>
#undef BOOL

#ifdef USE_TIME_SINCE_VALIDATE
// Since m_last_valid was set in the past (presumably), timeIntervalSinceNow is negative.
// Don't validate more than 60 times per second
#define TIMER_DELAY MAX(0.003,(0.016+[m_last_valid timeIntervalSinceNow]))
#else
#define TIMER_DELAY 0.015
#endif

@interface RedrawTimerCallback : NSObject {
	CocoaMDEScreen* _screen;
}
- (id)initWithScreen:(CocoaMDEScreen*)screen delay:(NSTimeInterval)delay;
- (void)timeout:(id)object;
@end

@implementation RedrawTimerCallback
- (id)initWithScreen:(CocoaMDEScreen*)screen delay:(NSTimeInterval)delay
{
	self = [super init];
	_screen = screen;
	// Set the timeout to 1/60th of a second, to prevent core from over-flushing.
	[self performSelector:@selector(timeout:) withObject:nil afterDelay:delay];
	return self;
}
- (void)timeout:(id)object
{
	_screen->OnTimeOut();
}
@end

CocoaMDEScreen::CocoaMDEScreen(CocoaOpWindow* w, CocoaVEGAWindow*v)
	: MDE_GenericScreen(0, 0, 0, MDE_FORMAT_BGRA32, v), m_window(w), m_vega(v), m_timer(NULL), m_bypass_update_lock(FALSE), m_dont_display(FALSE), m_gradient_height(0), m_dirty(false), m_mutex(NULL)
{
#ifdef USE_TIME_SINCE_VALIDATE
	m_last_valid = [[NSDate date] retain];
#endif
}

CocoaMDEScreen::~CocoaMDEScreen()
{
	if (m_timer) {
		[NSObject cancelPreviousPerformRequestsWithTarget:(id)m_timer];
		[(id)m_timer release];
	}
#ifdef USE_TIME_SINCE_VALIDATE
	[m_last_valid autorelease];
#endif
#ifdef MUTEX_LOCK_CALAYER_UPDATES
	for (unsigned i=0; i<refs.GetCount(); i++)
	{
		CocoaMDEScreen** refAddr = refs.Get(i);
		*refAddr = NULL;
	}
#endif // MUTEX_LOCK_CALAYER_UPDATES
}

OP_STATUS CocoaMDEScreen::Init()
{
	OP_STATUS ret = MDE_GenericScreen::Init();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	if (!GetVegaPainter())
		CreateVegaPainter();
	UpdatePainterScaleFactor();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	return ret;
}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
void CocoaMDEScreen::UpdatePainterScaleFactor()
{
	PixelScalePainter* oppainter = static_cast<PixelScalePainter*>(GetVegaPainter());
	if (oppainter)
	{
		INT32 new_scale = m_window->GetPixelScale();
		INT32 old_scale = oppainter->SetScale(new_scale);
		if (new_scale != old_scale)
		{
			PixelScaler scaler(new_scale);
			Invalidate(MDE_MakeRect(0, 0, FROM_DEVICE_PIXEL(scaler, m_vega->width()), FROM_DEVICE_PIXEL(scaler, m_vega->height())), true);
		}
	}
}
#endif // PIXEL_SCALE_RENDERING_SUPPORT

MDE_BUFFER *CocoaMDEScreen::LockBuffer()
{
	return MDE_GenericScreen::LockBuffer();
}
void CocoaMDEScreen::UnlockBuffer(MDE_Region *update_region)
{
	[(OperaNSView*) m_window->GetContentView() setNeedsDisplayForLayers:!m_dont_display];
	MDE_GenericScreen::UnlockBuffer(update_region);
}

void CocoaMDEScreen::OnValidate()
{
	if (m_bypass_update_lock)
	{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		PixelScaler scaler(m_window->GetPixelScale());
		Invalidate(MDE_MakeRect(0, 0, FROM_DEVICE_PIXEL(scaler, m_vega->width()), FROM_DEVICE_PIXEL(scaler, m_vega->height())), true, false, false, true);
#else
		Invalidate(MDE_MakeRect(0, 0, m_vega->width(), m_vega->height()), true, false, false, true);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	}

	// Advance animations here. We are going to paint anyway.
	if (g_animation_manager)
		g_animation_manager->advanceAnimations();
}

void CocoaMDEScreen::OnInvalid()
{
	if (!m_timer && !m_dont_display)
		m_timer = [[RedrawTimerCallback alloc] initWithScreen:this delay:TIMER_DELAY];
}

#ifdef _MAC_DEBUG
// filename must be a URL, like file:///Users/ismailp/some/thing.png
void ExportImage(CGImageRef image, const char* filename)
{
	CFStringEncoding encoding = kCFStringEncodingMacRoman; // =  0;
	CFAllocatorRef alloc_default = kCFAllocatorDefault;  // = NULL;
	CFStringRef imagename = CFStringCreateWithCString(alloc_default, filename, encoding);
	
	CFURLRef url = CFURLCreateWithString( NULL, imagename, NULL);
	
	CFStringRef type = kUTTypePNG;  //public.png
	size_t count = 1; 
	CFDictionaryRef options = NULL;
	CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, type, count, options);
	
	CGImageDestinationAddImage(dest, image, NULL);
	
	CGImageDestinationFinalize(dest);
	CFRelease(url);
	CFRelease(dest);
	CFRelease(imagename);
}
#endif

void CocoaMDEScreen::UpdateImageAndResizeIfNeeded(int w, int h, BOOL background)
{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	PixelScaler scaler(m_window->GetPixelScale());
	int width_point = FROM_DEVICE_PIXEL(scaler, m_vega->width());
	int height_point = FROM_DEVICE_PIXEL(scaler, m_vega->height());
#else
	int width_point = m_vega->width();
	int height_point = m_vega->height();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if ((background && m_gradient_height < [(OperaNSWindow*)m_window->GetNativeWindow() getUnifiedToolbarHeight]) || w != width_point || h != height_point) {
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		m_vega->resize(TO_DEVICE_PIXEL(scaler, w), TO_DEVICE_PIXEL(scaler, h), background);
#else
		img = m_vega->resize(w, h, background);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		// Calling Resize() or Invalidate() may result in
		// calls to OnInvalid(), which are pointless, because we will validate anyway.
		// Calling Validate() may result in calls to UnlockBuffer
		// Mark that we don't need to care about it here, because when this
		// function returns the image will contain the most recent data.
		m_dont_display = TRUE;
		Resize(w, h, m_vega->getBytesPerLine(), m_vega);
		m_window->MDE_OpWindow::SetInnerSize(w, h);
#ifdef GADGET_SUPPORT
		if (m_window->HasInvisibleTitlebar())
			m_vega->ClearBuffer();
#endif // GADGET_SUPPORT
		Invalidate(MDE_MakeRect(0,0,w,h), true);
		DoValidate(FALSE);
		m_dont_display = FALSE;
	}

	if (GetInvalidFlag())
	{
#ifdef MUTEX_LOCK_CALAYER_UPDATES
		if (!m_mutex || !pthread_mutex_trylock(m_mutex))
		{
#endif
			DoValidate(TRUE);
#ifdef MUTEX_LOCK_CALAYER_UPDATES
		}
		else
		{
			m_dirty = true;
		}
#endif
	}
	if (GetInvalidFlag() && !m_timer)
		m_timer = [[RedrawTimerCallback alloc] initWithScreen:this delay:TIMER_DELAY];

	if (background)
		m_gradient_height = [(OperaNSWindow*)m_window->GetNativeWindow() getUnifiedToolbarHeight];
}

void CocoaMDEScreen::OnTimeOut()
{
	RESET_OPENGL_CONTEXT

	[NSObject cancelPreviousPerformRequestsWithTarget:(id)m_timer];
	[(id)m_timer release];
	m_timer = NULL;

    if (MacOpPrinterController::IsPrinting())
	{
		m_timer = [[RedrawTimerCallback alloc] initWithScreen:this delay:TIMER_DELAY];
        return;
	}

#ifdef GADGET_SUPPORT
	if (m_window->HasInvisibleTitlebar())
	{
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		PixelScaler scaler(m_window->GetPixelScale());
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		m_vega->ClearBuffer();

		// Calling Invalidate may result in an OnInvalid call
		// Mark that we don't need to care about it. We don't need to
		// set a new timer, because we will handle the drawing right here.
		m_dont_display = TRUE;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		Invalidate(MDE_MakeRect(0, 0, FROM_DEVICE_PIXEL(scaler, m_vega->width()), FROM_DEVICE_PIXEL(scaler, m_vega->height())), true);
#else
		Invalidate(MDE_MakeRect(0, 0, m_vega->width(), m_vega->height()), true);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		m_dont_display = FALSE;
	}
#endif // GADGET_SUPPORT
	// If we are currently validating then don't call Validate directly but reset
	// the timer so it is called later
	if (m_is_validating)
		OnInvalid();
	else
	{
#ifdef MUTEX_LOCK_CALAYER_UPDATES
		if (!m_mutex || !pthread_mutex_trylock(m_mutex))
		{
#endif
			DoValidate(FALSE);
#ifdef MUTEX_LOCK_CALAYER_UPDATES
		}
		else
		{
			m_dirty = true;
		}
#endif
	}
}

void CocoaMDEScreen::OnBeforeRectPaint(const MDE_RECT &rect_point)
{
	MDE_RECT rect;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	PixelScaler scaler(GetPixelScale());
	rect.x = TO_DEVICE_PIXEL(scaler, rect_point.x);
	rect.y = TO_DEVICE_PIXEL(scaler, rect_point.y);
	rect.w = TO_DEVICE_PIXEL(scaler, rect_point.x + rect_point.w) - rect.x;
	rect.h = TO_DEVICE_PIXEL(scaler, rect_point.y + rect_point.h) - rect.y;
#else
	rect = rect_point;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	m_vega->ClearBufferRect(rect);
#ifdef VEGA_USE_HW
	if (m_vega->GetVEGA3dWindow())
	{
		static_cast<MacGlWindow*>(m_vega->GetVEGA3dWindow())->Erase(rect);
	}
#endif
}

void CocoaMDEScreen::DoValidate(BOOL dont_display)
{
#ifdef USE_TIME_SINCE_VALIDATE
	[m_last_valid autorelease];
	m_last_valid = [[NSDate date] retain];
#endif

	if (dont_display)
		m_dont_display = TRUE;
	OperaNSView *view = (OperaNSView *)m_window->GetContentView();
	[view setValidateStarted:TRUE];
#ifdef ASYNC_MERGE
	m_vega->BeginValidate();
#endif
	Validate(true);
#ifdef ASYNC_MERGE
	m_vega->EndValidate();
#endif
	[view setValidateStarted:FALSE];
	if (dont_display)
		m_dont_display = FALSE;
}

bool CocoaMDEScreen::UseTransparentBackground()
{
	return (m_window && m_window->GetStyle() == OpWindow::STYLE_EXTENSION_POPUP);
}

void CocoaMDEScreen::ScrollBackground(const MDE_RECT &rect, int dx, int dy)
{
	MDE_RECT scaled_rect = rect;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	PixelScaler scaler(GetPixelScale());
	scaled_rect.x = TO_DEVICE_PIXEL(scaler, rect.x);
	scaled_rect.y = TO_DEVICE_PIXEL(scaler, rect.y);
	scaled_rect.w = TO_DEVICE_PIXEL(scaler, rect.w);
	scaled_rect.h = TO_DEVICE_PIXEL(scaler, rect.h);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	m_vega->ScrollBackground(scaled_rect, dx, dy);
}

void CocoaMDEScreen::MoveToBackground(OpRect rect, bool transparent)
{
	m_vega->MoveToBackground(rect, transparent);
}

CGImageRef CocoaMDEScreen::CreateImageForRect(OpRect rect)
{
	return m_vega->CreateImageForRect(rect);
}

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
int CocoaMDEScreen::GetPixelScale() const
{
	return m_window ? m_window->GetPixelScale() : 100;
}
#endif // PIXEL_SCALE_RENDERING_SUPPORT
