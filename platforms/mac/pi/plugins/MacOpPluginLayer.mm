/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#include "platforms/mac/pi/plugins/MacOpPluginLayer.h"

#define BOOL NSBOOL
#import <AppKit/NSEvent.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSGraphicsContext.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <QuartzCore/CAOpenGLLayer.h>
#import <AppKit/NSOpenGLView.h>
#undef BOOL

// Set to turn on all the printfs to debug the PluginWrapper
//#define MAC_PLUGIN_WRAPPER_DEBUG

#ifdef NS4P_COMPONENT_PLUGINS

////////////////////////////////////////////////////////////////////////////////////

@implementation MacOpPluginLayer

- (id)init
{
	self = [super init];
	
	// Init everything
	_surface = NULL;
	_surfaceTexture = 0;
	_surfaceId = 0;
	_texWidth = 0;
	_texHeight = 0;
	_newSurface = false;
	_gLContextChanged = true;
	_glContext = NULL;
	
	return self;
}

- (void)dealloc
{
	if (_surface) 
		CFRelease(_surface);
	
	[super dealloc];
}

- (void)releaseCGLPixelFormat:(CGLPixelFormatObj)pixelFormat
{
}

-(CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	printf("copyCGLPixelFormatForDisplayMask\n");
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	CGLPixelFormatAttribute attributes[] = {
		kCGLPFAWindow,
		kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
		kCGLPFANoRecovery,
        kCGLPFAColorSize, (CGLPixelFormatAttribute)32,
        kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
        kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
        (CGLPixelFormatAttribute)0
    };

    GLint numPixelFormats;
	CGLPixelFormatObj pixelFormat;
    CGLChoosePixelFormat(attributes, &pixelFormat, &numPixelFormats);

	return pixelFormat;
}

// 2)	[Optional] This message is sent prior to rendering to create a context to render to.
//		You would typically override this method if you needed to specify a share context to share OpenGL resources.
//		This is also an ideal location to do any initialization that is necessary for the context returned
-(CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
	printf("copyCGLContextForPixelFormat surface id:%d\n", _surfaceId);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
	
	// Reset the previous everytime it gets a new context for the pixel format
	_gLContextChanged = true;
	
	// Default handling is fine for this demonstration.
	_glContext = [super copyCGLContextForPixelFormat:pixelFormat];
	return _glContext;
}

// 4)	[Required] Implement this message in order to actually draw anything.
//		Typically you will do the following when you recieve this message:
//		1. Draw your OpenGL content! (the current context has already been set)
//		2. call [super drawInContext:pixelFormat:forLayerTime:displayTime:] to finalize the layer content, or call glFlush().
//		NOTE: The viewport has already been set correctly by the time this message is sent, so you do not need to set it yourself.
//		The viewport is automatically updated whenever the layer is displayed (that is, when the -display message is sent).
//		This is arranged for when you send the -setNeedsDisplay message, or when the needsDisplayOnBoundsChange property is set to YES
//		and the layer's size changes.
-(void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
	if (!_surfaceId)
		return;

	if (_newSurface)
	{
		if (_surfaceTexture)
			glDeleteTextures(1, &_surfaceTexture);
		glGenTextures(1, &_surfaceTexture);
	}
	
	if (_newSurface || _gLContextChanged)
	{
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _surfaceTexture);
		glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   		CGLTexImageIOSurface2D(glContext, GL_TEXTURE_RECTANGLE_ARB, GL_RGBA,
                               _texWidth, _texHeight,
                               GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _surface, 0);

		_newSurface = false;
		_gLContextChanged = false;
	}


	// Adjust the viewport so that the plugin clips correctly
	glViewport(0, 0, _texWidth, _texHeight);
	
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, _texWidth, _texHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //////
	
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
    /////////////
    
    if (_surfaceTexture) 
	{
        // Draw the color channels from the incoming texture.
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _surfaceTexture);
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBegin(GL_TRIANGLE_STRIP);
        
		glTexCoord2f(0, _texHeight);
		glVertex2f(0, 0);
		glTexCoord2f(_texWidth, _texHeight);
		glVertex2f(_texWidth, 0);
		glTexCoord2f(0, 0);
		glVertex2f(0, _texHeight);
		glTexCoord2f(_texWidth, 0);
		glVertex2f(_texWidth, _texHeight);
		
		glEnd();
		glDisable(GL_TEXTURE_RECTANGLE_ARB);
		
		////////
		
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	}

#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
//	printf("drawInCGLContext surface id:%d context:%p\n", _surfaceId, glContext);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		

	// Call super to finalize the drawing. By default all it does is call glFlush().
	[super drawInCGLContext:glContext pixelFormat:pixelFormat forLayerTime:timeInterval displayTime:timeStamp];
}

- (void)_bindSurfaceToTexture: (IOSurfaceID)surfaceId
{
	if (_surface) 
		CFRelease(_surface);
	
	_surfaceId = surfaceId;
	_surface = IOSurfaceLookup(surfaceId);
	
	if (_surface != nil) {
		_texWidth   = IOSurfaceGetWidth(_surface);
		_texHeight  = IOSurfaceGetHeight(_surface);
		_newSurface = true;
		
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
		printf("_bindSurfaceToTexture, w:%d, h:%d\n", _texWidth, _texHeight);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
	}
}

- (void)setSurfaceID: (IOSurfaceID)surfaceId
{
#ifdef MAC_PLUGIN_WRAPPER_DEBUG		
    printf("setSurfaceID id: %d\n", surfaceId);
#endif // MAC_PLUGIN_WRAPPER_DEBUG		
	if (surfaceId) 
		[self _bindSurfaceToTexture: surfaceId];
}

- (IOSurfaceID)getSurfaceID
{
	return _surfaceId;
}

- (void)setDelayedLayerFrame:(CGRect)rect
{
    _delayedLayerFrame = rect;
}

- (void)setFrameFromDelayed
{
    [super setFrame:_delayedLayerFrame];
}

@end

#else // NS4P_COMPONENT_PLUGINS

@implementation MacOpPluginLayer

- (void)setDelayedLayerFrame:(CGRect)rect
{
    _delayedLayerFrame = rect;
}

- (void)setFrameFromDelayed
{
    [super setFrame:_delayedLayerFrame];
}

@end

@implementation MacOpCGPluginLayer

- (id)initWithPluginWindow:(MacOpPluginWindow*)plugin_window
{
	self = [super init];
	_plugin_window = plugin_window;
	return self;
}

- (void)drawInContext:(CGContextRef)theContext
{
	CGContextSaveGState(theContext);
	CGContextScaleCTM(theContext, 1.0, -1.0);
	CGContextTranslateCTM(theContext, 0, -_plugin_window->GetHeight());
	NPCocoaEvent* event_record = OP_NEW(NPCocoaEvent, ());
	if (event_record)
	{
		event_record->type = NPCocoaEventDrawRect;
		event_record->version = 0;
		event_record->data.draw.context = theContext;
		event_record->data.draw.x = 0;
		event_record->data.draw.y = 0;
		event_record->data.draw.width = _plugin_window->GetWidth();
		event_record->data.draw.height = _plugin_window->GetHeight();
		MacOpPlatformEvent<NPCocoaEvent> evt;
		evt.m_events.Add(event_record);
		_plugin_window->GetPlugin()->HandleEvent(&evt, PLUGIN_PAINT_EVENT);
	}
	CGContextRestoreGState(theContext);
}

@end

#endif // NS4P_COMPONENT_PLUGINS
