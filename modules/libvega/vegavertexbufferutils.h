/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGAVERTEXBUFFERUTILS_H
#define VEGAVERTEXBUFFERUTILS_H

#ifdef VEGA_SUPPORT

#include "modules/libvega/vega3ddevice.h"

#if defined(VEGA_3DDEVICE) || defined(CANVAS3D_SUPPORT)

/**
  * Returns the number of vertex attributes that fit in a buffer of
  * size 'size', the first vertex starting at byte offset 'offset', with stride
  * 'stride' between vertices, and where each vertex attributes has size
  * 'attributeSize'.
  *
  * Adds 'stride - attributeSize' to round correctly to the number of vertices.
  * To see why this works, consider the buffer as made up of n stride-sized
  * chunks plus a final, truncated chunk of length c (could be 0). c is large
  * enough to hold one additional attribute iff c >= attributeSize, which is
  * the same as c + (stride - attributeSize) >= stride. Therefore, the maximum
  * number of vertices that fit in the buffer is
  *
  *   n + (c + (stride - attributeSize))/stride
  * = (stride*n + c + (stride - attributeSize))/stride
  * = (size + (stride - attributeSize))/stride
  *
  * Example: xxxyyxxxyy
  * Array size is 10, offset to first y is 3, stride is 5, y vertex size is 2.
  * (10 - 3 + (5 - 2)) / 5 = 2.
  */
inline unsigned maxVerticesForBuffer(unsigned size, unsigned offset, unsigned stride, unsigned attributeSize)
{
	return (size - offset + (stride - attributeSize))/stride;
}

/** Copies and optionally converts and widens a single strided vertex component from the inbuffer to an unstrided
  * outbuffer. Note that this is only used in slow-case fallbacks, stick to floats and ubyte4's and it'll be fast.
  *
  * @param inPtr pointer to first input component.
  * @param inStride stride of the input component.
  * @param inType type of input component.
  * @param outPtr pointer to target buffer.
  * @param outStride stride (size) of out component.
  * @param outType type of output component.
  * @param maxVertices number of vertices to convert.
  * @param normalize true if the output is normalized.
  */
void CopyVertexComponent(void *inPtr, unsigned int inStride, VEGA3dVertexLayout::VertexType inType,
						 void *outPtr, unsigned int outStride, VEGA3dVertexLayout::VertexType outType,
						 unsigned int maxVertices, bool normalize);

#endif
#endif
#endif
