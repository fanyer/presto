/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/vega_backends/opengl/vegaglapi.h"

#define __gl_h_

#include "platforms/mac/pi/CocoaMDEScreen.h"
#include "platforms/mac/pi/display/MacOpGLDevice.h"

// Last due to YES/NO type redefining.
#include "platforms/mac/model/VegaLayer.h"

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKEND_OPENGL)

#define BOOL NSBOOL
# include "platforms/mac/model/SharedOpenGLContext.h"
#undef BOOL

@implementation VegaCGLLayer
@synthesize screen=_screen, background=_background;

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
	CGLPixelFormatObj pf = static_cast<CGLPixelFormatObj>([[SharedOpenGLContext defaultPixelFormat] CGLPixelFormatObj]);
	CGLRetainPixelFormat(pf);
	return pf;
}

- (void)releaseCGLPixelFormat:(CGLPixelFormatObj)pf
{
	OP_ASSERT(pf == [[SharedOpenGLContext defaultPixelFormat] CGLPixelFormatObj]);
	CGLReleasePixelFormat(pf);
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pf
{
	OP_ASSERT(pf == [[SharedOpenGLContext defaultPixelFormat] CGLPixelFormatObj]);
	CGLContextObj ctx = static_cast<CGLContextObj>([[SharedOpenGLContext sharedContext] CGLContextObj]);
	CGLRetainContext(ctx);
	return ctx;
}

- (void)releaseCGLContext:(CGLContextObj)ctx
{
	OP_ASSERT(ctx == [[SharedOpenGLContext sharedContext] CGLContextObj]);
	CGLReleaseContext(ctx);
}

- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
			forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts
{
	NSRect frame = NSRectFromCGRect([self frame]);
	OP_ASSERT(ctx==[[SharedOpenGLContext sharedContext] CGLContextObj]);
	if (_screen && _screen->GetVegaWindow() && _screen->GetVegaWindow()->GetVEGA3dWindow() && frame.size.width && frame.size.height)
	{
		VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;
		if (_background && !_screen->GetVegaWindow()->getImage(_background))
		{
			// When there is no background to paint, just clear (this fixes flickering when hovering in the speed dial)
			device->setRenderTarget(NULL);
			device->setRenderState(device->getDefault2dRenderState());
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}
		else
		{
			_screen->UpdateImageAndResizeIfNeeded(frame.size.width, frame.size.height, _background);
			device->setRenderTarget(NULL);
			device->setRenderState(device->getDefault2dRenderState());
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			((MacGlWindow*)_screen->GetVegaWindow()->GetVEGA3dWindow())->present(NULL, 0, _background);
		}

		if ([SharedOpenGLContext doubleBuffered])
			[[SharedOpenGLContext sharedContext] flushBuffer];
		else
			glFlush();
	}
}

@end


@implementation VegaNSOpenGLLayer
@synthesize screen=_screen, background=_background;

- (NSOpenGLPixelFormat *)openGLPixelFormatForDisplayMask:(uint32_t)mask
{
	return [[SharedOpenGLContext defaultPixelFormat] retain];
}

- (NSOpenGLContext *)openGLContextForPixelFormat:(NSOpenGLPixelFormat *)pixelFormat
{
	OP_ASSERT(pixelFormat == [SharedOpenGLContext defaultPixelFormat]);
	return [[SharedOpenGLContext sharedContext] retain];
}

- (void)setOpenGLPixelFormat:(NSOpenGLPixelFormat *)fmt
{
	[super setOpenGLPixelFormat:[SharedOpenGLContext defaultPixelFormat]];
}

- (NSOpenGLPixelFormat *)openGLPixelFormat
{
	return [SharedOpenGLContext defaultPixelFormat];
}

- (void)setOpenGLContext:(NSOpenGLContext *)ctx
{
	[ctx makeCurrentContext];
	[super setOpenGLContext:[[SharedOpenGLContext sharedContext] retain]];
}

- (NSOpenGLContext *)openGLContext
{
	return [SharedOpenGLContext sharedContext];
}

- (void)drawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts
{
	RESET_OPENGL_CONTEXT
	
	NSRect frame = NSRectFromCGRect([self frame]);
	OP_ASSERT(context==[SharedOpenGLContext sharedContext]);
	if (_screen)
	{
		_screen->UpdateImageAndResizeIfNeeded(frame.size.width, frame.size.height, _background);
		VEGA3dDevice* device = g_opera->libvega_module.vega3dDevice;
		device->setRenderTarget(NULL);
		device->setRenderState(device->getDefault2dRenderState());
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		
		if (_screen->GetVegaWindow() && _screen->GetVegaWindow()->GetVEGA3dWindow())
			_screen->GetVegaWindow()->GetVEGA3dWindow()->present(NULL, 0);
		
		if ([SharedOpenGLContext doubleBuffered])
			[context flushBuffer];
		else
			glFlush();
	}
}

@end

#endif // VEGA_3DDEVICE && VEGA_BACKEND_OPENGL


@implementation VegaSoftwareLayer
@synthesize screen=_screen, background=_background;

#ifdef VEGALAYER_DISPLAY_WORKAROUND

- (void)drawInContext:(CGContextRef)theContext
{
	CGRect frame = [self frame];
	if (_screen)
	{
		_screen->UpdateImageAndResizeIfNeeded(frame.size.width, frame.size.height, _background);
		CocoaVEGAWindow* vega = _screen->GetVegaWindow();
		CGImageRef img = vega->createImage(_background);
		if (img) 
		{
			CGContextDrawImage(theContext, frame, img);
			CGImageRelease(img);
		}
	}
}

#else	// !VEGALAYER_DISPLAY_WORKAROUND

- (void)display
{
	NSRect frame = NSRectFromCGRect([self frame]);
	if (_screen)
	{
		_screen->UpdateImageAndResizeIfNeeded(frame.size.width, frame.size.height, _background);
		CocoaVEGAWindow* vega = _screen->GetVegaWindow();
		CGImageRef img = vega->createImage(_background);
		if (img)
		{
			[CATransaction begin];
			// Disable implicit animations.
			[CATransaction setValue:[NSNumber numberWithFloat:0.0f] forKey:kCATransactionAnimationDuration];
			[self setContents:(id)img];
			[CATransaction commit];
			CGImageRelease(img);
		}
	}
}

#endif	// VEGALAYER_DISPLAY_WORKAROUND

@end
