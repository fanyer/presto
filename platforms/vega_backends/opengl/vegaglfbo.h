/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAGLFBO_H
#define VEGAGLFBO_H

#ifdef VEGA_BACKEND_OPENGL

#include "modules/libvega/vega3ddevice.h"
#include "platforms/vega_backends/opengl/vegaglapi.h"

class VEGAGlRenderbufferObject : public VEGA3dRenderbufferObject, public Link
{
public:
	VEGAGlRenderbufferObject(unsigned int width, unsigned int height, ColorFormat fmt, int samples);
	~VEGAGlRenderbufferObject();
	OP_STATUS Construct();

	virtual unsigned int getWidth(){return width;}
	virtual unsigned int getHeight(){return height;}

	virtual ColorFormat getFormat(){return fmt;}
	virtual unsigned int getNumSamples(){return samples;}


	virtual unsigned int getRedBits() { return params.redBits; }
	virtual unsigned int getGreenBits() { return params.greenBits; }
	virtual unsigned int getBlueBits() { return params.blueBits; }
	virtual unsigned int getAlphaBits() { return params.alphaBits; }
	virtual unsigned int getDepthBits() { return params.depthBits; }
	virtual unsigned int getStencilBits() { return params.stencilBits; }

	GLuint getRenderbufferId(unsigned int id = 0){return renderbuffer[id];}
	void clearDevice();
private:
	GLuint renderbuffer[2];
	unsigned int width;
	unsigned int height;
	ColorFormat fmt;
	int samples;
	VEGAGlRenderbufferParameters params;
};

class VEGAGlFramebufferObject : public VEGA3dFramebufferObject, public Link
{
public:
	VEGAGlFramebufferObject();
	~VEGAGlFramebufferObject();
	OP_STATUS Construct();

	virtual unsigned int getWidth(){return colorTexture?colorTexture->getWidth():colorBuffer?colorBuffer->getWidth():0;}
	virtual unsigned int getHeight(){return colorTexture?colorTexture->getHeight():colorBuffer?colorBuffer->getHeight():0;}

	virtual OP_STATUS attachColor(VEGA3dTexture* color, VEGA3dTexture::CubeSide side);
	virtual OP_STATUS attachColor(VEGA3dRenderbufferObject* color);
	virtual OP_STATUS attachStencil(VEGA3dRenderbufferObject* stencil);
	virtual OP_STATUS attachDepth(VEGA3dRenderbufferObject* depth);
	virtual OP_STATUS attachDepthStencil(VEGA3dRenderbufferObject* depthStencil);

	virtual unsigned int getRedBits();
	virtual unsigned int getGreenBits();
	virtual unsigned int getBlueBits();
	virtual unsigned int getAlphaBits();
	virtual unsigned int getDepthBits();
	virtual unsigned int getStencilBits();
	virtual unsigned int getSubpixelBits();

	virtual unsigned int getSampleBuffers();
	virtual unsigned int getSamples();

	virtual VEGA3dTexture* getAttachedColorTexture(){return colorTexture;}
	virtual VEGA3dRenderbufferObject* getAttachedColorBuffer() { return colorBuffer; }
	virtual VEGA3dRenderbufferObject* getAttachedDepthStencilBuffer() { return depthStencilBuffer; }

	void clearDevice();
private:
	VEGA3dTexture* colorTexture;
	VEGA3dRenderbufferObject* colorBuffer;
	VEGA3dRenderbufferObject* depthStencilBuffer;
public:
	GLuint fbo;
};

#endif // VEGA_BACKEND_OPENGL
#endif // VEGAGLFBO_H
