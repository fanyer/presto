/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAPIXELSTORE_H
#define VEGAPIXELSTORE_H

#if defined(VEGA_SUPPORT)

/** Pixel store formats supported by Vega.
 * Be very careful when using RGB565 and BGR565 as they do not use the same
 * component ordering as the others in order to be compatible with the public
 * gogi API. For 16bit RGB/BGR the naming is MSB->LSB, for the other formats
 * it is LSB->MSB. */
enum VEGAPixelStoreFormat
{
	VPSF_BGRA8888,
	VPSF_RGBA8888,
	VPSF_ABGR8888,
	VPSF_ARGB8888,
	VPSF_BGRX8888,
	VPSF_RGBA4444,
	VPSF_ABGR4444,
	VPSF_BGRA4444,
	VPSF_RGB565,
	VPSF_BGR565,
	VPSF_RGB888,
	VPSF_RGBA5551,
	VPSF_BGRA5551,
	VPSF_ALPHA8,
	VPSF_LUMINANCE8_ALPHA8,
	VPSF_SAME  // Used to signal that the used format is what was originally requested.
};

static inline unsigned int VPSF_BytesPerPixel(VEGAPixelStoreFormat fmt)
{
	switch (fmt)
	{
	case VPSF_BGRX8888:
	case VPSF_BGRA8888:
	case VPSF_RGBA8888:
	case VPSF_ABGR8888:
	case VPSF_ARGB8888:
		return 4;

	case VPSF_RGB888:
		return 3;

	case VPSF_LUMINANCE8_ALPHA8:
	case VPSF_BGRA5551:
	case VPSF_RGBA4444:
	case VPSF_BGRA4444:
	case VPSF_ABGR4444:
	case VPSF_RGB565:
	case VPSF_BGR565:
	case VPSF_RGBA5551:
		return 2;

	default:
	case VPSF_ALPHA8:
		return 1;
	}
}

struct VEGAPixelStore
{
	void*                buffer;
	unsigned int         stride;
	VEGAPixelStoreFormat format;

	unsigned int width;
	unsigned int height;
};

class VEGAPixelStoreWrapper
{
public:
	VEGAPixelStoreWrapper() { }
	VEGAPixelStoreWrapper(unsigned int w, unsigned int h, VEGAPixelStoreFormat f, unsigned int rowSize, void *buf)
	{
		ps.buffer = buf;
		ps.stride = rowSize;
		ps.format = f;
		ps.width = w;
		ps.height = h;
	}

	OP_STATUS Allocate(VEGAPixelStoreFormat fmt, unsigned int w, unsigned int h, unsigned rowAlignment)
	{
		ps.format = fmt;
		ps.width = w;
		ps.height = h;

		unsigned int rowLen = ps.width * VPSF_BytesPerPixel(fmt);
		ps.stride = rowLen;
		if (rowLen % rowAlignment)
			ps.stride = (rowLen / rowAlignment + 1) * rowAlignment;
		ps.buffer = OP_NEWA(UINT8, ps.stride * ps.height);
		return ps.buffer ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}

	void Free()
	{
		OP_DELETEA(static_cast<UINT8*>(ps.buffer));
	}

	VEGAPixelStore ps;
};

#endif // VEGA_SUPPORT
#endif // !VEGAPIXELSTORE_H
