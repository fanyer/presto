/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGA3DCOMPATINDEXBUFFER_H
#define VEGA3DCOMPATINDEXBUFFER_H

# ifdef VEGA_SUPPORT

/** Class that can generate a temporary indexbuffer for backends that cannot
  * render all the primitives required by the interface. E.g. line-loops and
  * triangle-fans. */
class VEGA3dCompatIndexBuffer
{
public:
	// The type of primitive that indices should be generated for.
	enum CompatType
	{
		LINE_LOOP, // Should be drawn as a line list
		TRI_FAN    // Should be drawn as a triangle list
	};

	VEGA3dCompatIndexBuffer() : m_currLength(0), m_currOffset(0), m_state(CACHE_INVALID), m_buffer(NULL), m_cache(NULL) { };
	~VEGA3dCompatIndexBuffer();
	void Destroy();

	/** Update the internal cache if needed and return an index buffer that  can be used to render the
	  * unsupported primitive type. */
	OP_STATUS GetIndexBuffer(VEGA3dDevice *device, const VEGA3dBuffer *&buffer, CompatType type, unsigned int len, unsigned int &numIndices, const unsigned char *indices = NULL, unsigned int bytesPerIndex = 2, unsigned int offset = 0);
private:
	/** State of the internal buffer. */
	enum CacheState
	{
		CACHE_INVALID,   // The buffer content cannot be reused.
		CACHE_LINE_LOOP, // The buffer holds line loop indices.
		CACHE_FAN        // The buffer holds fan indices.
	};

	unsigned int m_currLength;
	unsigned int m_currOffset;
	CacheState m_state;
	VEGA3dBuffer *m_buffer;
	UINT16 *m_cache;
};

# endif // VEGA_SUPPORT

#endif // VEGA3DCOMPATINDEXBUFFER
