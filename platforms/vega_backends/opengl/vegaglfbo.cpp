/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_OPENGL
#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegaglfbo.h"
#include "platforms/vega_backends/opengl/vegagltexture.h"

VEGAGlRenderbufferObject::VEGAGlRenderbufferObject(unsigned int width, unsigned int height, ColorFormat fmt, int samples) 
	: width(width)
	, height(height)
	, fmt(fmt)
	, samples(samples)
{
	renderbuffer[0] = 0;
	renderbuffer[1] = 0;
}

VEGAGlRenderbufferObject::~VEGAGlRenderbufferObject()
{
	clearDevice();
}

OP_STATUS VEGAGlRenderbufferObject::Construct()
{
	VEGAGlDevice *glDevice = static_cast<VEGAGlDevice*>(g_vegaGlobals.vega3dDevice);
	if (width > glDevice->stateVariables.maxRenderBufferSize ||
	    height > glDevice->stateVariables.maxRenderBufferSize)
		return OpStatus::ERR;
	int ifmt;
#ifdef VEGA_OPENGLES
	int ifmt2 = 0;
#endif // VEGA_OPENGLES
	unsigned int numBuffers = 1;
	switch (fmt)
	{
#ifndef VEGA_OPENGLES
	case FORMAT_RGBA8:
		ifmt = GL_RGBA;
		break;
#endif // VEGA_OPENGLES
	case FORMAT_RGB565:
		ifmt = GL_RGB565;
		break;
	case FORMAT_RGB5_A1:
#ifdef VEGA_OPENGLES
		ifmt = GL_RGB5_A1;
#else
		ifmt = glDevice && glDevice->supportRGB5_A1_fbo ? GL_RGB5_A1 : GL_RGBA;
#endif
		break;
	case FORMAT_RGBA4:
#ifdef VEGA_OPENGLES
		ifmt = GL_RGBA4;
#else
		ifmt = glDevice && glDevice->supportRGBA4_fbo ? GL_RGBA4 : GL_RGBA;
#endif
		break;
	case FORMAT_STENCIL8:
#ifdef VEGA_OPENGLES
		ifmt = GL_STENCIL_INDEX8;
		// Use packed depth stencil if stencil only is not supported,
		// like for PowerVR OpenGL ES emulator.
		if (!glDevice->supportStencilAttachment && glDevice->supportPackedDepthStencil())
			ifmt = GL_DEPTH24_STENCIL8_OES;
#else
		ifmt = non_gles_GL_DEPTH24_STENCIL8;
#endif
		break;
	case FORMAT_DEPTH16:
#ifdef VEGA_OPENGLES
		ifmt = GL_DEPTH_COMPONENT16;
#else
		ifmt = non_gles_GL_DEPTH_COMPONENT24;
#endif
		break;
	case FORMAT_DEPTH16_STENCIL8:
#ifdef VEGA_OPENGLES
		if (glDevice && glDevice->supportPackedDepthStencil())
		{
			ifmt = GL_DEPTH24_STENCIL8_OES;
		}
		else
		{
			numBuffers = 2;
			ifmt = GL_DEPTH_COMPONENT16;
			ifmt2 = GL_STENCIL_INDEX8;
		}
#else
		ifmt = non_gles_GL_DEPTH24_STENCIL8;
#endif
		break;
	default:
		return OpStatus::ERR;
	}

	// Consume any previous errors so that error handling below only
	// deals with calls made here.
	GLenum ret;
	while ((ret = glGetError()) != GL_NO_ERROR)
	{
		if (ret != GL_OUT_OF_MEMORY)
			OP_ASSERT(!"GL error encountered. something's broken somewhere. it might be possible to get more details by enabling TWEAK_VEGA_BACKENDS_GL_DEBUG_CONTEXT");
	}

	glGenRenderbuffers(numBuffers, renderbuffer);
	if (!renderbuffer[0] || (numBuffers > 1 && !renderbuffer[1]))
		return OpStatus::ERR;
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer[0]);


#ifndef VEGA_OPENGLES
	// zero width or height crashes intel driver either immediately or later after bound
	// to an FBO
	if (width && height)
		if (samples > 1)
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, ifmt, width, height);
		else
#endif
			glRenderbufferStorage(GL_RENDERBUFFER, ifmt, width, height);

	OP_STATUS error = CheckGLError();
	if (OpStatus::IsError(error))
	{
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glDeleteRenderbuffers(numBuffers, renderbuffer);
		return OpStatus::ERR_NO_MEMORY;
	}

#ifdef VEGA_OPENGLES
	if (numBuffers > 1)
	{
		if (ifmt2 != GL_STENCIL_INDEX8)
			return OpStatus::ERR;
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer[1]);
		glRenderbufferStorage(GL_RENDERBUFFER, ifmt2, width, height);
	}
#endif

	glDevice->getRenderbufferParameters(ifmt, params);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	return OpStatus::OK;
}

void VEGAGlRenderbufferObject::clearDevice()
{
	Out();
	if (renderbuffer[0] > 0)
		glDeleteRenderbuffers(1, &renderbuffer[0]);
	if (renderbuffer[1] > 0)
		glDeleteRenderbuffers(1, &renderbuffer[1]);
	renderbuffer[0] = 0;
	renderbuffer[1] = 0;
}


VEGAGlFramebufferObject::VEGAGlFramebufferObject() : colorTexture(NULL), colorBuffer(NULL), depthStencilBuffer(NULL), fbo(0)
{}

VEGAGlFramebufferObject::~VEGAGlFramebufferObject()
{
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	if (dev && dev->getRenderTarget() == this)
		dev->setRenderTarget(NULL);
	VEGARefCount::DecRef(colorTexture);
	VEGARefCount::DecRef(colorBuffer);
	VEGARefCount::DecRef(depthStencilBuffer);
	clearDevice();
}

OP_STATUS VEGAGlFramebufferObject::Construct()
{
	glGenFramebuffers(1, &fbo);
	if (!fbo)
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS VEGAGlFramebufferObject::attachColor(VEGA3dTexture* color, VEGA3dTexture::CubeSide side)
{
	if (color)
	{
		RETURN_IF_ERROR(static_cast<VEGAGlTexture*>(color)->createTexture());
		VEGARefCount::IncRef(color);
	}
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	VEGA3dRenderTarget* oldrt = dev->getRenderTarget();
	dev->setRenderTarget(this, false);
	GLenum textarget;
	switch (side)
	{
	case VEGA3dTexture::CUBE_SIDE_NEG_X:
		textarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
		break;
	case VEGA3dTexture::CUBE_SIDE_POS_X:
		textarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
		break;
	case VEGA3dTexture::CUBE_SIDE_NEG_Y:
		textarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
		break;
	case VEGA3dTexture::CUBE_SIDE_POS_Y:
		textarget = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
		break;
	case VEGA3dTexture::CUBE_SIDE_NEG_Z:
		textarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
		break;
	case VEGA3dTexture::CUBE_SIDE_POS_Z:
		textarget = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
		break;
	default:
		textarget = GL_TEXTURE_2D;
		break;
	}
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textarget, color?static_cast<VEGAGlTexture*>(color)->getTextureId():0, 0);
	dev->setRenderTarget(oldrt, false);
	VEGARefCount::DecRef(colorBuffer);
	colorBuffer = NULL;
	VEGARefCount::DecRef(colorTexture);
	colorTexture = color;
	return OpStatus::OK;
}

OP_STATUS VEGAGlFramebufferObject::attachColor(VEGA3dRenderbufferObject* color)
{
	VEGARefCount::IncRef(color);
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	VEGA3dRenderTarget* oldrt = dev->getRenderTarget();
	dev->setRenderTarget(NULL);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color?static_cast<VEGAGlRenderbufferObject*>(color)->getRenderbufferId():0);
	dev->setRenderTarget(oldrt);
	VEGARefCount::DecRef(colorBuffer);
	colorBuffer = color;
	VEGARefCount::DecRef(colorTexture);
	colorTexture = NULL;
	return OpStatus::OK;
}

OP_STATUS VEGAGlFramebufferObject::attachStencil(VEGA3dRenderbufferObject* stencil)
{
	VEGARefCount::IncRef(stencil);
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	VEGA3dRenderTarget* oldrt = dev->getRenderTarget();
	dev->setRenderTarget(NULL);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
#ifdef VEGA_OPENGLES
	VEGAGlDevice *glDevice = static_cast<VEGAGlDevice*>(dev);
	if (!glDevice->supportStencilAttachment && glDevice->supportPackedDepthStencil())
	{
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, stencil?static_cast<VEGAGlRenderbufferObject*>(stencil)->getRenderbufferId():0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil?static_cast<VEGAGlRenderbufferObject*>(stencil)->getRenderbufferId():0);
	}
	else
	{
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil?static_cast<VEGAGlRenderbufferObject*>(stencil)->getRenderbufferId():0);
	}
#else
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, non_gles_GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencil?static_cast<VEGAGlRenderbufferObject*>(stencil)->getRenderbufferId():0);
#endif
	dev->setRenderTarget(oldrt);
	VEGARefCount::DecRef(depthStencilBuffer);
	depthStencilBuffer = stencil;
	return OpStatus::OK;
}

OP_STATUS VEGAGlFramebufferObject::attachDepth(VEGA3dRenderbufferObject* depth)
{
	VEGARefCount::IncRef(depth);
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	VEGA3dRenderTarget* oldrt = dev->getRenderTarget();
	dev->setRenderTarget(NULL);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth?static_cast<VEGAGlRenderbufferObject*>(depth)->getRenderbufferId():0);
	dev->setRenderTarget(oldrt);
	VEGARefCount::DecRef(depthStencilBuffer);
	depthStencilBuffer = depth;
	return OpStatus::OK;
}

OP_STATUS VEGAGlFramebufferObject::attachDepthStencil(VEGA3dRenderbufferObject* depthStencil)
{
	VEGARefCount::IncRef(depthStencil);
	VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
	VEGA3dRenderTarget* oldrt = dev->getRenderTarget();
	dev->setRenderTarget(NULL);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
#ifdef VEGA_OPENGLES
	VEGAGlDevice *glDevice = static_cast<VEGAGlDevice*>(dev);
	GLuint depthId = depthStencil?static_cast<VEGAGlRenderbufferObject*>(depthStencil)->getRenderbufferId():0;
	GLuint stencilId;

	if (glDevice && glDevice->supportPackedDepthStencil())
		stencilId = depthId;
	else
		stencilId = depthStencil?static_cast<VEGAGlRenderbufferObject*>(depthStencil)->getRenderbufferId(1):0;

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthId);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilId);
#else
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, non_gles_GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencil?static_cast<VEGAGlRenderbufferObject*>(depthStencil)->getRenderbufferId():0);
#endif
	dev->setRenderTarget(oldrt);
	VEGARefCount::DecRef(depthStencilBuffer);
	depthStencilBuffer = depthStencil;
	return OpStatus::OK;
}

void VEGAGlFramebufferObject::clearDevice()
{
	Out();
	if (fbo > 0)
		glDeleteFramebuffers(1, &fbo);
	fbo = 0;
}

unsigned int VEGAGlFramebufferObject::getRedBits()
{
	GLint bits;
	glGetIntegerv(GL_RED_BITS, &bits);
	return bits;
}

unsigned int VEGAGlFramebufferObject::getGreenBits()
{
	GLint bits;
	glGetIntegerv(GL_GREEN_BITS, &bits);
	return bits;
}

unsigned int VEGAGlFramebufferObject::getBlueBits()
{
	GLint bits;
	glGetIntegerv(GL_BLUE_BITS, &bits);
	return bits;
}

unsigned int VEGAGlFramebufferObject::getAlphaBits()
{
	GLint bits;
	glGetIntegerv(GL_ALPHA_BITS, &bits);
	return bits;
}

unsigned int VEGAGlFramebufferObject::getDepthBits()
{
	GLint bits;
	glGetIntegerv(GL_DEPTH_BITS, &bits);
	return bits;
}

unsigned int VEGAGlFramebufferObject::getStencilBits()
{
	GLint bits;
	glGetIntegerv(GL_STENCIL_BITS, &bits);
	return bits;
}

unsigned int VEGAGlFramebufferObject::getSubpixelBits()
{
	GLint bits;
	glGetIntegerv(GL_SUBPIXEL_BITS, &bits);
	return bits;
}

unsigned int VEGAGlFramebufferObject::getSampleBuffers()
{
	GLint samp;
	glGetIntegerv(GL_SAMPLE_BUFFERS, &samp);
	return samp;
}

unsigned int VEGAGlFramebufferObject::getSamples()
{
	GLint samp;
	glGetIntegerv(GL_SAMPLES, &samp);
	return samp;
}


#endif // VEGA_BACKEND_OPENGL
