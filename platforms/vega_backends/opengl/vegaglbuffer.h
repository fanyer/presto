/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAGLBUFFER_H
#define VEGAGLBUFFER_H

#ifdef VEGA_BACKEND_OPENGL

#include "modules/libvega/vega3ddevice.h"

class VEGAGlBuffer : public VEGA3dBuffer, public Link
{
public:
	VEGAGlBuffer();
	~VEGAGlBuffer();
	OP_STATUS Construct(unsigned int size, Usage usage, bool indexBuffer);

	OP_STATUS writeAnywhere(unsigned int num, unsigned int size, void* data, unsigned int& index);
	OP_STATUS writeAtOffset(unsigned int offset, unsigned int size, void* data);

	unsigned int getSize(){return size;}

	/* Warning: Use the bind() and unbind() methods rather than calling
	   glBindBuffer() yourself, or you might cause internal state that
	   keeps track of buffer bindings to avoid redundant bind calls to
	   become unsynchronized. */

	void bind();

	/** Binds 0 to the target corresponding to the buffer (ARRAY_BUFFER or
	    ELEMENT_ARRAY_BUFFER). */
	void unbind();

	void clearDevice();
private:
	unsigned int size;
	Usage usage;
	bool indexBuffer;
	bool vboInitialized;
#ifndef VEGA_OPENGLES
	unsigned int bufferUsed;
#endif
public:
	unsigned int vbo;
};

class VEGAGlVertexLayout : public VEGA3dVertexLayout
{
public:
	VEGAGlVertexLayout() : bindings(NULL)
#ifndef VEGA_OPENGLES
		, attribZeroBuffer(0)
#endif // !VEGA_OPENGLES
	{}
	~VEGAGlVertexLayout();

	virtual OP_STATUS addComponent(VEGA3dBuffer* buffer, unsigned int index, unsigned int offset, unsigned int stride, VertexType type, bool normalize);
	virtual OP_STATUS addConstComponent(unsigned int index, float* values);

	void bind();
	void unbind();
private:
	struct BindPoint
	{
		VEGA3dBuffer* buffer;
		unsigned int index;
		unsigned int offset;
		unsigned int stride;
		unsigned int type;
		unsigned int typeSize;
		bool normalize;
		float value[4];
		BindPoint* next;
	};
	BindPoint* bindings;
#ifndef VEGA_OPENGLES
	GLuint attribZeroBuffer;
#endif // !VEGA_OPENGLES
};

#endif // VEGA_BACKEND_OPENGL
#endif // VEGAGLBUFFER_H
