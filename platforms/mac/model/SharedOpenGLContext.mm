/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "platforms/mac/model/SharedOpenGLContext.h"

#define BOOL NSBOOL
#import <AppKit/NSOpenGLView.h>
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLContext.h>
#undef BOOL

@implementation SharedOpenGLContext

static NSOpenGLContext* shared_context = nil;
static bool is_double_buffered = true;

+ (NSOpenGLPixelFormat*)defaultPixelFormat
{
	static NSOpenGLPixelFormat* result;
	NSOpenGLPixelFormatAttribute attr[] = {
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAAccelerated,
		/*NSOpenGLPFABackingStore,*/
		NSOpenGLPFAWindow,
		NSOpenGLPFAMultiScreen,
		NSOpenGLPFANoRecovery,
		NSOpenGLPFASingleRenderer,
		//(NSOpenGLPixelFormatAttribute)NSOpenGLPFACompliant,		
		
		//(NSOpenGLPixelFormatAttribute)NSOpenGLPFAAuxBuffers, (NSOpenGLPixelFormatAttribute)0,
		NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAStencilSize, (NSOpenGLPixelFormatAttribute)8,
		//NSOpenGLPFAAccumSize, (NSOpenGLPixelFormatAttribute)0,
		(NSOpenGLPixelFormatAttribute)0
	};
	if (!result)
		result = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
	return result;
}

+ (NSOpenGLContext*)sharedContext
{
	if (shared_context)
		return shared_context;

	NSOpenGLPixelFormat* deffmt = [SharedOpenGLContext defaultPixelFormat];
	if (!deffmt)
		return nil;

	shared_context = [[NSOpenGLContext alloc] initWithFormat:deffmt shareContext:nil];
	if (shared_context)
	{
		GLint zeroOpacity = 0;
		[shared_context setValues:&zeroOpacity forParameter:NSOpenGLCPSurfaceOpacity];

		NSOpenGLPixelFormat* pf = [SharedOpenGLContext defaultPixelFormat];
		const int numvs = [pf numberOfVirtualScreens];
		for (int i = 0; i < numvs; ++i)
		{
			GLint val;
			[pf getValues:&val forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:i];
			is_double_buffered &= val == 1 || val == NSOpenGLPFADoubleBuffer;
		}
	}
	
	return shared_context;
}

+ (void)clearSharedContext
{
	[shared_context release];
	shared_context = nil;
}

+ (bool)doubleBuffered
{
	return is_double_buffered;
}

@end
