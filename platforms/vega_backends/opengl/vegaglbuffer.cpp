/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKEND_OPENGL
#include "platforms/vega_backends/opengl/vegaglapi.h"

#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/vega_backends/opengl/vegaglbuffer.h"

VEGAGlBuffer::VEGAGlBuffer() : size(0), vbo(0)
{}

VEGAGlBuffer::~VEGAGlBuffer()
{
	clearDevice();
}

OP_STATUS VEGAGlBuffer::Construct(unsigned int size, Usage usage, bool indexBuffer)
{
	this->size = size;
	this->usage = usage;
	this->indexBuffer = indexBuffer;
	glGenBuffers(1, (GLuint*)&vbo);
	if (!vbo)
		return OpStatus::ERR;
	vboInitialized = false;
#ifndef VEGA_OPENGLES
	bufferUsed = 0;
#endif
	return OpStatus::OK;
}

OP_STATUS VEGAGlBuffer::writeAnywhere(unsigned int num, unsigned int size, void* data, unsigned int& index)
{
	/* There is nothing wrong in itself with calling this
	 * method on a buffer which is not DISCARD.  However, it
	 * is believed that there is no sensible use case where
	 * this method would be called on a non-DISCARD buffer.
	 * So if that happens, it indicates that there probably is
	 * a sub-optimality somewhere.
	 *
	 * If you trigger this assert and believe that the call to
	 * this method is sensible, please tell the owner of this
	 * code about it.  He can probably be persuaded by a good
	 * example.
	 */
	OP_ASSERT(usage == STREAM_DISCARD);

	if (!vbo)
		return OpStatus::ERR;

#ifndef VEGA_OPENGLES
	if (usage == STREAM_DISCARD && glMapBufferRange)
	{
		bind();

		unsigned int target = indexBuffer ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
		unsigned int dataSize = num * size;

		/* calculate starting offset (into 'index').  This should be
		 * the smallest value that is larger than 'bufferUsed' and
		 * meets the alignment restrictions ('size' and 4).
		 */
		index = (bufferUsed + size - 1) / size;
		index = (index + 3) & (~3u);

		if (!vboInitialized || index * size + dataSize > this->size)
		{
			if (dataSize > this->size)
				return OpStatus::ERR;
			vboInitialized = true;
			glBufferData(target, this->size, NULL, GL_STREAM_DRAW);
			bufferUsed = 0;
			index = 0;
		}
		void * buffer = glMapBufferRange(target, index * size, dataSize, non_gles_GL_MAP_WRITE_BIT | non_gles_GL_MAP_INVALIDATE_RANGE_BIT | non_gles_GL_MAP_UNSYNCHRONIZED_BIT);
		if (buffer)
		{
			op_memcpy(buffer, data, dataSize);
			bufferUsed = index * size + dataSize;
			if (glUnmapBuffer(target))
				return OpStatus::OK;
			/* If glUnmapBuffer fails, that means that some event
			 * outside of program control has caused the buffer to
			 * become invalid.  The content of the buffer is now
			 * undefined, so let's just fall back to the other
			 * update().
			 */
		}
		/* If the MapBufferRange strategy fails, it should be safe to
		 * fall back to the other udpate().
		 */
	}
#endif // !VEGA_OPENGLES

	index = 0;
	return writeAtOffset(0, num * size, data);
}

OP_STATUS VEGAGlBuffer::writeAtOffset(unsigned int offset, unsigned int size, void* data)
{
	if (!vbo)
		return OpStatus::ERR;
	unsigned int target = indexBuffer ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
	unsigned int glmode = GL_STREAM_DRAW;
	switch (usage)
	{
	case STATIC:
		glmode = GL_STATIC_DRAW;
		break;
	case STREAM:
	case STREAM_DISCARD:
		glmode = GL_STREAM_DRAW;
		break;
	case DYNAMIC:
		glmode = GL_DYNAMIC_DRAW;
		break;
	default:
		return OpStatus::ERR;
	}

	bind();

	/* Some Linux NVIDIA drivers (at least) generate GL errors when
	   glMapBufferRange() is called with size = 0. To avoid that, and as there
	   isn't anything left to do in practice in that case once we've checked
	   for errors, return here. */
	if (size == 0)
		return OpStatus::OK;

#ifndef VEGA_OPENGLES
	/* Make sure glMapBufferRange() is not used to append to the
	 * buffer (since the buffer may not be set up correctly for that).
	 */
	bufferUsed = this->size;
#endif  // !VEGA_OPENGLES
	if (usage == STREAM_DISCARD)
	{
		/* Since the buffer may end up being smaller than this->size,
		 * mark the buffer as "uninitialized" to ensure we don't try
		 * to access beyond the actual size of the buffer.
		 */
		vboInitialized = false;
		OP_ASSERT(offset==0);
#ifndef VEGA_OPENGLES
		if (glMapBufferRange)
		{
			/* Under some circumstances (seen while testing
			 * painter_bench_suite/blit_text.html), glBufferData()
			 * with data (80kB) can be quite slow.  In contrast,
			 * glBufferData() without data followed by
			 * MapBufferRange() and memcpy() is a consistently fast
			 * way of doing the same thing.  The difference can easily
			 * be on the order of 10 times as fast.  (ATI Radeon HD
			 * 5450, ubuntu 11.10 with fglrx 4.1.11005 (probably
			 * catalyst 11.8) on a 2.3 GHz Core 2 Duo).
			 *
			 * This is probably driver-specific, as it should be
			 * semantically equivalent.
			 */
			OP_ASSERT(offset + size <= this->size);
			glBufferData(target, this->size, NULL, glmode);
			void * buffer = glMapBufferRange(target, offset, size, non_gles_GL_MAP_WRITE_BIT | non_gles_GL_MAP_INVALIDATE_RANGE_BIT | non_gles_GL_MAP_UNSYNCHRONIZED_BIT);
			if (buffer)
			{
				op_memcpy(buffer, data, size);
				if (glUnmapBuffer(target))
				{
					bufferUsed = offset + size;
					vboInitialized = true;
					return OpStatus::OK;
				}
			}
		}
#endif // !VEGA_OPENGLES
		glBufferData(target, size, data, glmode);
		return OpStatus::OK;
	}
	else if (!vboInitialized)
	{
		vboInitialized = true;
#ifndef VEGA_OPENGLES
		if (glMapBufferRange)
		{
			/* Under some circumstances, glMapBufferRange is much
			 * faster than glBufferData.  See comment at similar code
			 * above.
			 */
			glBufferData(target, this->size, NULL, glmode);
			void * buffer = glMapBufferRange(target, offset, size, non_gles_GL_MAP_WRITE_BIT | non_gles_GL_MAP_INVALIDATE_RANGE_BIT | non_gles_GL_MAP_UNSYNCHRONIZED_BIT);
			if (buffer)
			{
				op_memcpy(buffer, data, size);
				if (glUnmapBuffer(target))
					return OpStatus::OK;
			}
		}
#endif // !VEGA_OPENGLES
		if (offset == 0 && size == this->size)
		{
			glBufferData(target, size, data, glmode);
			return OpStatus::OK;
		}
		glBufferData(target, this->size, NULL, glmode);
	}
	glBufferSubData(target, offset, size, data);
	return OpStatus::OK;
}

void VEGAGlBuffer::bind()
{
	((VEGAGlDevice*)g_vegaGlobals.vega3dDevice)->bindBuffer(indexBuffer, vbo);
}

void VEGAGlBuffer::unbind()
{
	((VEGAGlDevice*)g_vegaGlobals.vega3dDevice)->bindBuffer(indexBuffer, 0);
}

void VEGAGlBuffer::clearDevice()
{
	Out();
	if (vbo)
	{
		((VEGAGlDevice*)g_vegaGlobals.vega3dDevice)->deleteBuffers(indexBuffer, 1, (const GLuint*)&vbo);
		vbo = 0;
	}
}

VEGAGlVertexLayout::~VEGAGlVertexLayout()
{
#ifndef VEGA_OPENGLES
	if (attribZeroBuffer)
		((VEGAGlDevice*)g_vegaGlobals.vega3dDevice)->deleteBuffers(false, 1, (const GLuint*)&attribZeroBuffer);
#endif // !VEGA_OPENGLES
	while (bindings)
	{
		BindPoint* nb = bindings->next;
		OP_DELETE(bindings);
		bindings = nb;
	}
}

OP_STATUS VEGAGlVertexLayout::addComponent(VEGA3dBuffer* buffer, unsigned int index, unsigned int offset, unsigned int stride, VertexType type, bool normalize)
{
	BindPoint** nextp = &bindings;
	while (*nextp)
		nextp = &((*nextp)->next);
	BindPoint* b = OP_NEW(BindPoint, ());
	RETURN_OOM_IF_NULL(b);
	b->next = NULL;
	b->buffer = buffer;
	b->index = index;
	b->offset = offset;
	b->stride = stride;
	switch (type)
	{
	case FLOAT4:
		b->type = GL_FLOAT;
		b->typeSize = 4;
		break;
	case FLOAT3:
		b->type = GL_FLOAT;
		b->typeSize = 3;
		break;
	case FLOAT2:
		b->type = GL_FLOAT;
		b->typeSize = 2;
		break;
	case FLOAT1:
		b->type = GL_FLOAT;
		b->typeSize = 1;
		break;
	case USHORT4:
		b->type = GL_UNSIGNED_SHORT;
		b->typeSize = 4;
		break;
	case USHORT3:
		b->type = GL_UNSIGNED_SHORT;
		b->typeSize = 3;
		break;
	case USHORT2:
		b->type = GL_UNSIGNED_SHORT;
		b->typeSize = 2;
		break;
	case USHORT1:
		b->type = GL_UNSIGNED_SHORT;
		b->typeSize = 1;
		break;
	case SHORT4:
		b->type = GL_SHORT;
		b->typeSize = 4;
		break;
	case SHORT3:
		b->type = GL_SHORT;
		b->typeSize = 3;
		break;
	case SHORT2:
		b->type = GL_SHORT;
		b->typeSize = 2;
		break;
	case SHORT1:
		b->type = GL_SHORT;
		b->typeSize = 1;
		break;
	case UBYTE4:
		b->type = GL_UNSIGNED_BYTE;
		b->typeSize = 4;
		break;
	case UBYTE3:
		b->type = GL_UNSIGNED_BYTE;
		b->typeSize = 3;
		break;
	case UBYTE2:
		b->type = GL_UNSIGNED_BYTE;
		b->typeSize = 2;
		break;
	case UBYTE1:
		b->type = GL_UNSIGNED_BYTE;
		b->typeSize = 1;
		break;
	case BYTE4:
		b->type = GL_BYTE;
		b->typeSize = 4;
		break;
	case BYTE3:
		b->type = GL_BYTE;
		b->typeSize = 3;
		break;
	case BYTE2:
		b->type = GL_BYTE;
		b->typeSize = 2;
		break;
	case BYTE1:
		b->type = GL_BYTE;
		b->typeSize = 1;
		break;
	default:
		return OpStatus::ERR;
	}
	b->normalize = normalize;
	*nextp = b;
	return OpStatus::OK;
}

OP_STATUS VEGAGlVertexLayout::addConstComponent(unsigned int index, float* values)
{
	BindPoint** nextp = &bindings;
	while (*nextp)
		nextp = &((*nextp)->next);
	BindPoint* b = OP_NEW(BindPoint, ());
	RETURN_OOM_IF_NULL(b);
	b->next = NULL;
	b->buffer = NULL;
	b->index = index;
	b->value[0] = values[0];
	b->value[1] = values[1];
	b->value[2] = values[2];
	b->value[3] = values[3];
	*nextp = b;
	return OpStatus::OK;
}

void VEGAGlVertexLayout::bind()
{
#ifndef VEGA_OPENGLES
	bool attribZeroEnabled = false;
	bool needsAttribZeroCurrentValueEmulation = false;
	float attribZeroValue[4] = { 0, 0, 0, 0 }; /* ARRAY OK 2012-05-23 emoller */
	unsigned int maxVerts = 0;
#endif // !VEGA_OPENGLES
	for (BindPoint* bp = bindings; bp; bp = bp->next)
	{
		if (bp->buffer)
		{
#ifndef VEGA_OPENGLES
			if (bp->index == 0)
				attribZeroEnabled = true;
			unsigned int curMax = (bp->buffer->getSize()-bp->offset)/bp->stride;
			maxVerts = MAX(maxVerts, curMax);
#endif // !VEGA_OPENGLES
			glEnableVertexAttribArray(bp->index);
			((VEGAGlBuffer*)bp->buffer)->bind();
			glVertexAttribPointer(bp->index, bp->typeSize, bp->type, bp->normalize?GL_TRUE:GL_FALSE, bp->stride, INT_TO_PTR(bp->offset));
		}
		else
		{
#ifndef VEGA_OPENGLES
			if (bp->index == 0)
			{
				OP_ASSERT(!attribZeroEnabled);
				needsAttribZeroCurrentValueEmulation = true;

				attribZeroValue[0] = bp->value[0];
				attribZeroValue[1] = bp->value[1];
				attribZeroValue[2] = bp->value[2];
				attribZeroValue[3] = bp->value[3];
			}
			else
#endif // !VEGA_OPENGLES
				glVertexAttrib4f(bp->index, bp->value[0], bp->value[1], bp->value[2], bp->value[3]);
		}
	}
#ifndef VEGA_OPENGLES
	/* Some non-ES drivers refuse to draw at all if attribute zero is disabled.
	   When attribute zero is not used by the current shader (should only
	   happen for user WebGL shaders) we bind a dummy VBO to it, since some
	   drivers crash when no VBO is bound even to unused enabled generic
	   attributes. As some drivers also dislike completely empty VBOs (e.g.,
	   ones on which bufferData() have never been called) being bound to
	   generic vertex attributes, the dummy VBO contains a small amount of
	   dummy data.

	   When attribute zero is disabled but used by the current shader, we
	   instead emulate a current value on on it via a different buffer. This is
	   slow, so we only want to do it when necessary.

	   Some other background:
	   https://developer.mozilla.org/en/WebGL/WebGL_best_practices
	   http://www.khronos.org/webgl/wiki/WebGL_and_OpenGL_Differences#Vertex_Attribute_0

	   The special semantics for attribute zero seem to have gone away between
	   OpenGL 3.0 and 3.1 (search the specs. for "generic attribute zero"), so
	   perhaps we wouldn't have to treat it specially if we get a high enough
	   version for the context (and the drivers are decent). */

	VEGAGlDevice *glDev = static_cast<VEGAGlDevice*>(g_vegaGlobals.vega3dDevice);

	if (!attribZeroEnabled)
	{
		if (!needsAttribZeroCurrentValueEmulation)
			glDev->dummyVBO->bind();
		else
		{
			// FIXME: not sure how to handle the case where no vertex attrib
			// arrays are present and can hold at least one vertex
			if (maxVerts == 0)
				return;
			// vertexAttrib 0 must be enabled as an array or nothing is drawn
			if (!attribZeroBuffer)
			{
				float* attrZeroData = OP_NEWA(float, maxVerts*4);
				if (!attrZeroData)
					return;
				for (unsigned int i = 0; i < maxVerts; ++i)
				{
						attrZeroData[i*4] = attribZeroValue[0];
						attrZeroData[i*4+1] = attribZeroValue[1];
						attrZeroData[i*4+2] = attribZeroValue[2];
						attrZeroData[i*4+3] = attribZeroValue[3];
				}
				glGenBuffers(1, &attribZeroBuffer);
				glDev->bindBuffer(false, attribZeroBuffer);
				glBufferData(GL_ARRAY_BUFFER, maxVerts*4*sizeof(float), attrZeroData, GL_STATIC_DRAW);
				OP_DELETEA(attrZeroData);
			}
			else
				glDev->bindBuffer(false, attribZeroBuffer);
		}
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	}
#endif // !VEGA_OPENGLES
	((VEGAGlDevice*)g_vegaGlobals.vega3dDevice)->bindBuffer(false, 0);
}

void VEGAGlVertexLayout::unbind()
{
#ifndef VEGA_OPENGLES
	bool attribZeroDisabled = false;
#endif // VEGA_OPENGLES

	for (BindPoint* bp = bindings; bp; bp = bp->next)
	{
		if (bp->buffer)
		{
			glDisableVertexAttribArray(bp->index);
#ifndef VEGA_OPENGLES
			if (bp->index == 0)
				attribZeroDisabled = true;
#endif // VEGA_OPENGLES
		}
		else
			glVertexAttrib4f(bp->index, 0.f, 0.f, 0.f, 1.f);
	}

#ifndef VEGA_OPENGLES
	if (!attribZeroDisabled)
		glDisableVertexAttribArray(0);
#endif // VEGA_OPENGLES
}


#endif // VEGA_BACKEND_OPENGL
