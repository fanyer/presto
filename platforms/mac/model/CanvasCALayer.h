/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CANVAS_CA_LAYER_H
#define CANVAS_CA_LAYER_H

#include "platforms/vega_backends/opengl/vegaglapi.h"
#define BOOL NSBOOL
#import <AppKit/NSOpenGLLayer.h>
#undef BOOL

@interface CanvasCALayer : NSOpenGLLayer
{
	GLuint texture;
	CGSize size;
}

+ (CanvasCALayer*) layer;
@property (assign) GLuint texture;
@property (assign) CGSize size;
@end

#endif // CANVAS_CA_LAYER_H
