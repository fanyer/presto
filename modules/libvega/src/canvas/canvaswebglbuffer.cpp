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

#include "modules/libvega/src/canvas/canvaswebglbuffer.h"
#include "modules/dom/src/canvas/webglconstants.h"

CanvasWebGLBuffer::CanvasWebGLBuffer() : m_buffer(NULL), m_size(0), m_data(NULL), m_usage(VEGA3dBuffer::STATIC), m_binding(0), m_hasBeenBound(FALSE), m_maxIndexUByte(-1), m_maxIndexUShort(-1)
{
}

CanvasWebGLBuffer::~CanvasWebGLBuffer()
{
	VEGARefCount::DecRef(m_buffer);
	OP_DELETEA(m_data);
}

OP_STATUS CanvasWebGLBuffer::create(VEGA3dBuffer::Usage usage, unsigned int size)
{
	reset();
	OP_ASSERT(m_binding != 0);
	if (m_binding == WEBGL_ELEMENT_ARRAY_BUFFER && size)
	{
		m_data = OP_NEWA(unsigned char, size);
		RETURN_OOM_IF_NULL(m_data);
	}
	RETURN_IF_ERROR(g_vegaGlobals.vega3dDevice->createBuffer(&m_buffer, size, usage, (m_binding == WEBGL_ELEMENT_ARRAY_BUFFER), true));
	m_size = size;
	m_usage = usage;
	return OpStatus::OK;
}

void CanvasWebGLBuffer::destroyInternal()
{
	reset();
}

void CanvasWebGLBuffer::reset()
{
	VEGARefCount::DecRef(m_buffer);
	m_buffer = NULL;
	OP_DELETEA(m_data);
	m_data = NULL;
	m_maxIndexUByte = -1;
	m_maxIndexUShort = -1;
	m_size = 0;
	m_usage = VEGA3dBuffer::STATIC;
}

unsigned int CanvasWebGLBuffer::getMaxIndex(unsigned int type)
{
	if (!m_data || !m_size)
		return 0;
	if (type == WEBGL_UNSIGNED_BYTE)
	{
		if (m_maxIndexUByte < 0)
		{
			// Find the maximum index
			for (unsigned int i = 0; i < m_size; ++i)
				if (m_data[i] > m_maxIndexUByte)
					m_maxIndexUByte = m_data[i];
		}
		return (unsigned int)m_maxIndexUByte;
	}
	else if (type == WEBGL_UNSIGNED_SHORT)
	{
		if (m_maxIndexUShort < 0)
		{
			// Find the maximum index
			unsigned short* data = (unsigned short*)m_data;
			for (unsigned int i = 0; i < m_size/2; ++i)
				if (data[i] > m_maxIndexUShort)
					m_maxIndexUShort = data[i];
		}
		return (unsigned int)m_maxIndexUShort;
	}
	return 0;
}


#endif // CANVAS3D_SUPPORT

