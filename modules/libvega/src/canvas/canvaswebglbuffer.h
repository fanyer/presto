/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASWEBGLBUFFER_H
#define CANVASWEBGLBUFFER_H

#ifdef CANVAS3D_SUPPORT

#include "modules/libvega/src/canvas/webglutils.h"

class CanvasWebGLBuffer : public WebGLRefCounted
{
public:
	CanvasWebGLBuffer();
	~CanvasWebGLBuffer();

	OP_STATUS create(VEGA3dBuffer::Usage usage, unsigned int size);
	void destroyInternal();
	VEGA3dBuffer* getBuffer(){return m_buffer;}

	unsigned int getSize(){return m_size;}
	unsigned char* getData(){return m_data;}
	VEGA3dBuffer::Usage getUsage(){return m_usage;}

	unsigned int getBinding(){return m_binding;}
	void setBinding(unsigned int b){m_binding = b;  m_hasBeenBound |= (b!=0);}

	unsigned int getMaxIndex(unsigned int type);
	void resetMaxIndex(){m_maxIndexUByte = -1; m_maxIndexUShort = -1;}

	BOOL hasBeenBound() const     { return m_hasBeenBound; }
private:
	void reset();

	VEGA3dBuffer* m_buffer;

	unsigned int m_size;
	unsigned char* m_data;
	VEGA3dBuffer::Usage m_usage;

	unsigned int m_binding;
	BOOL m_hasBeenBound;

	int m_maxIndexUByte;
	int m_maxIndexUShort;
};

typedef WebGLSmartPointer<CanvasWebGLBuffer> CanvasWebGLBufferPtr;

#endif //CANVAS3D_SUPPORT
#endif //CANVASWEBGLBUFFER_H
