/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef VEGA_LAYER_H
#define VEGA_LAYER_H

class CocoaMDEScreen;

#define BOOL NSBOOL
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKEND_OPENGL)
# import <AppKit/NSOpenGLLayer.h>
# import <QuartzCore/CAOpenGLLayer.h>
#endif

#if !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7)
@interface CALayer (LION_ONLY)
- (void)setContentsScale:(CGFloat)scale;
@end
#endif // MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
#undef BOOL

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKEND_OPENGL)

// CGL implementation:

@interface VegaCGLLayer : CAOpenGLLayer {
	CocoaMDEScreen* _screen;
	BOOL _background;
}
@property(assign) CocoaMDEScreen* screen;
@property(assign) BOOL background;
@end


// NSOpenGL implementation (10.6 and newer):

@interface VegaNSOpenGLLayer : NSOpenGLLayer {
	CocoaMDEScreen* _screen;
	BOOL _background;
}
@property(assign) CocoaMDEScreen* screen;
@property(assign) BOOL background;
@end
#endif // VEGA_3DDEVICE && VEGA_BACKEND_OPENGL


// Software rendering / CALayer content

@interface VegaSoftwareLayer : CALayer {
	CocoaMDEScreen* _screen;
	BOOL _background;
}
@property(assign) CocoaMDEScreen* screen;
@property(assign) BOOL background;
@end


#endif // VEGA_LAYER_H