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

#include "modules/libvega/src/canvas/canvaswebglrenderbuffer.h"

CanvasWebGLRenderbuffer::CanvasWebGLRenderbuffer() : m_renderbuffer(NULL), m_initialized(false), m_hasBeenBound(false), m_zeroSizedBuffer(false), m_initId(0)
{
}

CanvasWebGLRenderbuffer::~CanvasWebGLRenderbuffer()
{
	VEGARefCount::DecRef(m_renderbuffer);
}

OP_STATUS CanvasWebGLRenderbuffer::create(VEGA3dRenderbufferObject::ColorFormat fmt, unsigned int width, unsigned int height)
{
	VEGARefCount::DecRef(m_renderbuffer);
	++m_initId; // Increment the id so any fbo using it can know it has changed.
	m_renderbuffer = NULL;
	m_zeroSizedBuffer = width == 0 || height == 0;
	return g_vegaGlobals.vega3dDevice->createRenderbuffer(&m_renderbuffer, width, height, fmt);
}

void CanvasWebGLRenderbuffer::destroyInternal()
{
	VEGARefCount::DecRef(m_renderbuffer);
	m_renderbuffer = NULL;
}

bool CanvasWebGLRenderbufferPtr::hasBeenInvalidated() const
{
	return m_initId == 0 || !GetPointer() || GetPointer()->getInitId() != m_initId;
}

void CanvasWebGLRenderbufferPtr::validate()
{
	m_initId = GetPointer() ? GetPointer()->getInitId() : 0;
}

#endif // CANVAS3D_SUPPORT
