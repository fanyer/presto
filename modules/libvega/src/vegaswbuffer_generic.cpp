/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT

#include "modules/libvega/src/vegaswbuffer.h"

#ifdef VEGA_USE_GENERIC_SWBUFFER

#include "modules/pi/OpBitmap.h"

OP_STATUS VEGASWBuffer::CreateFromBitmap(OpBitmap* bmp, bool& using_pointer)
{
	RETURN_IF_ERROR(Create(bmp->Width(), bmp->Height(), VSWBUF_COLOR, !(bmp->HasAlpha() || bmp->IsTransparent())));

	UINT32* line = OP_NEWA(UINT32, width);
	if (!line)
	{
		Destroy();
		return OpStatus::ERR_NO_MEMORY;
	}

	VEGAPixelAccessor dst = GetAccessor(0, 0);
	unsigned dstPixelStride = GetPixelStride();

	for (unsigned int y = 0; y < height; ++y)
	{
		bmp->GetLineData(line, y);

		for (unsigned x = 0; x < width; ++x)
		{
			dst.StoreARGB(line[x]>>24, (line[x]>>16)&0xff,
						  (line[x]>>8)&0xff, line[x]&0xff);

			dst++;
		}

		dst += dstPixelStride - width;
	}

	OP_DELETEA(line);
	using_pointer = false;
	return OpStatus::OK;
}

static void CopyToBGRA8888(UINT32* dst,
						   const VEGASWBuffer& src, unsigned sx, unsigned sy,
						   unsigned len)
{
	VEGAConstPixelAccessor srca = src.GetConstAccessor(sx, sy);

	while (len-- > 0)
	{
		unsigned a, r, g, b;
		srca.LoadUnpack(a, r, g, b);
		srca++;

		*dst++ = (a<<24) | (r<<16) | (g<<8) | b;
	}
}

OP_STATUS VEGASWBuffer::CopyToBitmap(OpBitmap* bmp, unsigned int srcx, unsigned int srcy,
									 unsigned int dstx, unsigned int dsty, unsigned int w, unsigned int h)
{
	if (!w)
		w = width;
	if (!h)
		h = height;
	if (srcx + w > width || srcy + h > height)
		return OpStatus::ERR;

	unsigned bmp_width = bmp->Width();
	if (dstx + w > bmp_width || dsty + h > bmp->Height())
		return OpStatus::ERR;

	UINT32* line = OP_NEWA(UINT32, bmp_width);
	if (!line)
		return OpStatus::ERR_NO_MEMORY;

	if (dstx == 0 && w == bmp_width)
	{
		for (unsigned int i = 0; i < h; ++i)
		{
			CopyToBGRA8888(line, *this, srcx, i + srcy, w);

			bmp->AddLine(line, i + dsty);
		}
	}
	else
	{
		for (unsigned int i = 0; i < h; ++i)
		{
			bmp->GetLineData(line, dsty + i);

			CopyToBGRA8888(line + dstx, *this, srcx, i + srcy, w);

			bmp->AddLine(line, i + dsty);
		}
	}

	OP_DELETEA(line);
	return OpStatus::OK;
}

#endif // VEGA_USE_GENERIC_SWBUFFER
#endif // VEGA_SUPPORT
