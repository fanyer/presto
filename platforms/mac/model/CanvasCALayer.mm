/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(CANVAS3D_HW_COMPOSITING)
#import "platforms/mac/model/CanvasCALayer.h"

#include "platforms/vega_backends/opengl/vegagldevice.h"

#import "platforms/mac/model/SharedOpenGLContext.h"

#define BOOL NSBOOL
#import <AppKit/NSOpenGL.h>
#undef BOOL
@implementation CanvasCALayer

+ (CanvasCALayer*) layer
{
	return [[CanvasCALayer alloc] init];
}

@synthesize texture;
@synthesize size;

- (id)init {
	self = [super init];
	texture = 0;
	size.width = size.height = 0;
	return self;
}

- (NSOpenGLPixelFormat *)openGLPixelFormatForDisplayMask:(uint32_t)mask
{
	return [SharedOpenGLContext defaultPixelFormat];
}
- (NSOpenGLContext *)openGLContextForPixelFormat:(NSOpenGLPixelFormat *)pixelFormat;
{
	return [SharedOpenGLContext sharedContext];
}


- (NSBOOL)canDrawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;
{
	return (context == [SharedOpenGLContext sharedContext]) && (texture != 0);
}

//#define T_BIND GL_TEXTURE_RECTANGLE_ARB
#define W 1.0
#define H 1.0
//#define W size.width
//#define H size.height

//
//- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts
- (void)drawInOpenGLContext:(NSOpenGLContext *)ctx pixelFormat:(NSOpenGLPixelFormat *)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;
{
//	glActiveTexture(GL_TEXTURE0);

if (glStringMarkerGREMEDY)
	glStringMarkerGREMEDY(0, "-drawInCGLContext");
	// Adjust the viewport so that the plugin clips correctly
//	CGLSetCurrentContext(ctx);
	[ctx makeCurrentContext];
	glViewport(0, 0, size.width, size.height);

	GLboolean resident=false;
	if (!glAreTexturesResident(1, &texture, &resident))
		printf("Not resident: %d, %d\n", texture, resident);

	if (!glIsTexture(texture))
		printf("Not a texture\n");


    ///////////// DEBUG
/*	GLuint mytex;
	glGenTextures(1, &mytex);
	if (mytex == texture)
		OP_ASSERT(!"Texture number reused. Something's wrong");
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, mytex);
	glTexImage2D (GL_TEXTURE_2D, 0, 4, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, (const GLvoid *)"\x00\xff\x00\xff" "\xff\xff\x00\xff" "\x00\x00\xff\x7f" "\x00\x00\x00\x7f");
//	glBindTexture(GL_TEXTURE_2D, 0);
/*	glDisable(GL_TEXTURE_2D);*/
    ///////////// DEBUG

//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, texture);
GLenum err = glGetError();
if (err) {
	printf("Bind failed %x\n", err);
}

/*
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);*/

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

/*    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
	glEnable(GL_STENCIL_TEST);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_TEXTURE_2D);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
*/
	glUseProgram(0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (texture)
	{
		glColor4f(0.0,0.0,0.0,1.0);
		glBegin(GL_QUADS);
//			glTexCoord2f(0, 0);
			glVertex2f(-1, -1);
//			glTexCoord2f(0, H);
			glVertex2f(-1,  500);
//			glTexCoord2f(W, H);
			glVertex2f( 500,  500);
//			glTexCoord2f(W, 0);
			glVertex2f( 2, -1);
		glEnd();
err = glGetError();
if (err) {
	printf("glEnd %x\n", err);
}

		////////
		
        glBindTexture(GL_TEXTURE_2D, 0);
	}
//	glDeleteTextures(1, &mytex);

	// Call super to finalize the drawing. By default all it does is call glFlush().
	[super drawInOpenGLContext:ctx pixelFormat:pf forLayerTime:t displayTime:ts];
}

@end

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && CANVAS3D_HW_COMPOSITING
