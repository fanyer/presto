/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MAC_COCOA_OPENGL_CONTEXT_H__
#define MAC_COCOA_OPENGL_CONTEXT_H__

#define BOOL NSBOOL
#import <AppKit/NSOpenGL.h>
#undef BOOL

@interface SharedOpenGLContext : NSObject
{
}

+ (NSOpenGLContext*)sharedContext;
+ (NSOpenGLPixelFormat*)defaultPixelFormat;
+ (void)clearSharedContext;
+ (bool)doubleBuffered;

@end

#endif
