/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef MACOPPLUGINLAYER_H__
#define MACOPPLUGINLAYER_H__


#if defined(_PLUGIN_SUPPORT_)

#include "platforms/mac/util/MachOCompatibility.h"

class MacOpPluginWindow;

#define BOOL NSBOOL
#import <Foundation/NSGeometry.h>
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAOpenGLLayer.h>
#import <AppKit/NSOpenGL.h>
#undef BOOL

#ifdef NS4P_COMPONENT_PLUGINS

// MacOpPluginLayer which is an CAOpenGLLayer which draws hardware accelerated plugins
@interface MacOpPluginLayer : CAOpenGLLayer
{
	GLuint					_surfaceTexture;
	IOSurfaceRef			_surface;
	GLsizei					_texWidth;
	GLsizei					_texHeight;
	IOSurfaceID				_surfaceId;
	bool					_newSurface;
	bool					_gLContextChanged;
	CGLContextObj			_glContext;
    CGRect                  _delayedLayerFrame;
}

- (id)init;

- (void)setSurfaceID: (IOSurfaceID)surfaceId;
- (IOSurfaceID)getSurfaceID;

- (void)setDelayedLayerFrame:(CGRect)rect;
- (void)setFrameFromDelayed;

@end

#else //  NS4P_COMPONENT_PLUGINS

@interface MacOpPluginLayer : CALayer
{
    CGRect _delayedLayerFrame;
}

- (void)setDelayedLayerFrame:(CGRect)rect;
- (void)setFrameFromDelayed;

@end

@interface MacOpCGPluginLayer : MacOpPluginLayer
{
	MacOpPluginWindow* _plugin_window;
}

- (id)initWithPluginWindow:(MacOpPluginWindow*)plugin_window;

@end

#endif // NS4P_COMPONENT_PLUGINS

#endif // _PLUGIN_SUPPORT_

#endif //  MACOPPLUGINLAYER_H__
