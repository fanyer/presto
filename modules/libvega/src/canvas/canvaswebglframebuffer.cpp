/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/canvaswebglframebuffer.h"
#include "modules/dom/src/canvas/webglconstants.h"

CanvasWebGLFramebuffer::CanvasWebGLFramebuffer() : m_framebuffer(NULL),
	m_colorBuf(NULL), m_depthBuf(NULL), m_stencilBuf(NULL), m_depthStencilBuf(NULL),
	m_colorTex(NULL), m_depthTex(NULL), m_stencilTex(NULL), m_depthStencilTex(NULL),
	m_colorTexFace(0), m_depthTexFace(0), m_stencilTexFace(0), m_depthStencilTexFace(0),
	m_hasBeenBound(FALSE), m_bindingsChanged(TRUE)
{}

CanvasWebGLFramebuffer::~CanvasWebGLFramebuffer()
{
	VEGARefCount::DecRef(m_framebuffer);
}

OP_STATUS CanvasWebGLFramebuffer::Construct()
{
	VEGA3dFramebufferObject *ptr = NULL;
	OP_STATUS status = g_vegaGlobals.vega3dDevice->createFramebuffer(&ptr);
	m_framebuffer = ptr;
	return status;
}

void CanvasWebGLFramebuffer::destroyInternal()
{
	releaseHandles();
	VEGARefCount::DecRef(m_framebuffer);
	m_framebuffer = NULL;
}

unsigned int CanvasWebGLFramebuffer::checkStatus()
{
	unsigned int width = ~0u;
	unsigned int height = ~0u;
	if (m_colorTex)
	{
		VEGA3dTexture* tex = m_colorTex->getTexture();
		if (!tex || m_colorTex->isDestroyed())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		VEGA3dTexture::ColorFormat fmt = tex->getFormat();
		if (fmt != VEGA3dTexture::FORMAT_RGB565 && fmt != VEGA3dTexture::FORMAT_RGBA4444 && fmt != VEGA3dTexture::FORMAT_RGBA5551
			&& fmt != VEGA3dTexture::FORMAT_RGB888 && fmt != VEGA3dTexture::FORMAT_RGBA8888)
			return WEBGL_FRAMEBUFFER_UNSUPPORTED;
		width = tex->getWidth();
		height = tex->getHeight();
	}
	if (m_depthTex || m_stencilTex || m_depthStencilTex)
	{
		return WEBGL_FRAMEBUFFER_UNSUPPORTED;
	}

	if (m_colorBuf)
	{
		if (m_colorBuf->isZeroSized())
		{
			width = 0;
			height = 0;
		}
		else
		{
			VEGA3dRenderbufferObject* buf = m_colorBuf->getBuffer();
			if (!buf || m_colorBuf->isDestroyed())
				return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			VEGA3dRenderbufferObject::ColorFormat fmt = buf->getFormat();
			if (fmt != VEGA3dRenderbufferObject::FORMAT_RGB565 && fmt != VEGA3dRenderbufferObject::FORMAT_RGBA4 && fmt != VEGA3dRenderbufferObject::FORMAT_RGB5_A1)
				return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			width = buf->getWidth();
			height = buf->getHeight();
		}
	}

	if (m_depthBuf)
	{
		if (m_depthBuf->isZeroSized())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		if (m_stencilBuf || m_depthStencilBuf)
			return WEBGL_FRAMEBUFFER_UNSUPPORTED;

		VEGA3dRenderbufferObject* buf = m_depthBuf->getBuffer();
		if (!buf || m_depthBuf->isDestroyed())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		VEGA3dRenderbufferObject::ColorFormat fmt = buf->getFormat();
		if (fmt != VEGA3dRenderbufferObject::FORMAT_DEPTH16)
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		if (width == ~0u || height == ~0u)
		{
			width = buf->getWidth();
			height = buf->getHeight();
		}
		if (width != buf->getWidth() || height != buf->getHeight())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
	}

	if (m_stencilBuf)
	{
		if (m_stencilBuf->isZeroSized())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		if (m_depthBuf || m_depthStencilBuf)
			return WEBGL_FRAMEBUFFER_UNSUPPORTED;

		VEGA3dRenderbufferObject* buf = m_stencilBuf->getBuffer();
		if (!buf || m_stencilBuf->isDestroyed())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		VEGA3dRenderbufferObject::ColorFormat fmt = buf->getFormat();
		if (fmt != VEGA3dRenderbufferObject::FORMAT_STENCIL8)
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		if (width == ~0u || height == ~0u)
		{
			width = buf->getWidth();
			height = buf->getHeight();
		}
		if (width != buf->getWidth() || height != buf->getHeight())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
	}

	if (m_depthStencilBuf)
	{
		if (m_depthStencilBuf->isZeroSized())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		if (m_depthBuf || m_stencilBuf)
			return WEBGL_FRAMEBUFFER_UNSUPPORTED;

		VEGA3dRenderbufferObject* buf = m_depthStencilBuf->getBuffer();
		if (!buf || m_depthStencilBuf->isDestroyed())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		VEGA3dRenderbufferObject::ColorFormat fmt = buf->getFormat();
		if (fmt != VEGA3dRenderbufferObject::FORMAT_DEPTH16_STENCIL8)
			return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		if (width == ~0u || height == ~0u)
		{
			width = buf->getWidth();
			height = buf->getHeight();
		}
		if (width != buf->getWidth() || height != buf->getHeight())
			return WEBGL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
	}
	if (width == ~0u || height == ~0u)
		return WEBGL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
	if (width == 0 || height == 0)
		return WEBGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
	return WEBGL_FRAMEBUFFER_COMPLETE;
}

OP_STATUS CanvasWebGLFramebuffer::updateBindings(unsigned int& status)
{
	status = checkStatus();
	if (status != WEBGL_FRAMEBUFFER_COMPLETE)
		return OpStatus::OK;
	if (!m_framebuffer)
		return OpStatus::ERR;
	bool invalid = m_colorBuf.hasBeenInvalidated() || m_depthBuf.hasBeenInvalidated() ||
	               m_stencilBuf.hasBeenInvalidated() || m_depthStencilBuf.hasBeenInvalidated();
	if (!m_bindingsChanged && !invalid)
		return OpStatus::OK;
	m_bindingsChanged = FALSE;
	if (m_colorTex)
	{
		VEGA3dTexture::CubeSide cubeSide;
		switch (m_colorTexFace)
		{
		case WEBGL_TEXTURE_CUBE_MAP_POSITIVE_X:
			cubeSide = VEGA3dTexture::CUBE_SIDE_POS_X;
			break;
		case WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
			cubeSide = VEGA3dTexture::CUBE_SIDE_NEG_X;
			break;
		case WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
			cubeSide = VEGA3dTexture::CUBE_SIDE_POS_Y;
			break;
		case WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
			cubeSide = VEGA3dTexture::CUBE_SIDE_NEG_Y;
			break;
		case WEBGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
			cubeSide = VEGA3dTexture::CUBE_SIDE_POS_Z;
			break;
		case WEBGL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			cubeSide = VEGA3dTexture::CUBE_SIDE_NEG_Z;
			break;
		case WEBGL_TEXTURE_2D:
		default:
			cubeSide = VEGA3dTexture::CUBE_SIDE_NONE;
			break;
		}
		RETURN_IF_ERROR(m_framebuffer->attachColor(m_colorTex->getTexture(), cubeSide));
	}
	else if (m_colorBuf)
	{
		RETURN_IF_ERROR(m_framebuffer->attachColor(m_colorBuf->getBuffer()));
		m_colorBuf.validate();
	}
	else
		m_framebuffer->attachColor((VEGA3dTexture*)NULL);
	if (!m_depthBuf)
		m_framebuffer->attachDepth(NULL);
	if (!m_stencilBuf)
		m_framebuffer->attachStencil(NULL);
	if (!m_depthStencilBuf)
		m_framebuffer->attachDepthStencil(NULL);

	if (m_depthBuf)
	{
		RETURN_IF_ERROR(m_framebuffer->attachDepth(m_depthBuf->getBuffer()));
		m_depthBuf.validate();
	}
	if (m_stencilBuf)
	{
		RETURN_IF_ERROR(m_framebuffer->attachStencil(m_stencilBuf->getBuffer()));
		m_stencilBuf.validate();
	}
	if (m_depthStencilBuf)
	{
		RETURN_IF_ERROR(m_framebuffer->attachDepthStencil(m_depthStencilBuf->getBuffer()));
		m_depthStencilBuf.validate();
	}
	return OpStatus::OK;
}

void CanvasWebGLFramebuffer::releaseHandles()
{
	m_colorBuf = NULL;
	m_depthBuf = NULL;
	m_stencilBuf = NULL;
	m_depthStencilBuf = NULL;
	m_colorTex = NULL;
	m_depthTex = NULL;
	m_stencilTex = NULL;
	m_depthStencilTex = NULL;
}

#endif // CANVAS3D_SUPPORT

