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

OP_STATUS VEGASWBuffer::Create(unsigned w, unsigned h, VEGASWBufferType buftype, bool opaque)
{
	width = w;
	height = h;
	type = buftype;
	isOpaque = opaque;

	if (buftype == VSWBUF_COLOR)
	{
		bpp = VEGA_PIXEL_FORMAT_CLASS::BytesPerPixel;

		RETURN_IF_ERROR(VEGA_PIXEL_FORMAT_CLASS::CreateBuffer(ptr, w, h, opaque));
	}
	else // STENCIL / INDEXED_COLOR
	{
		bpp = 1;

		UINT8* buf = OP_NEWA_WH(UINT8, width, height);
		if (!buf)
			return OpStatus::ERR_NO_MEMORY;

		if (type == VSWBUF_INDEXED_COLOR)
		{
			VEGA_PIXEL* pal = OP_NEWA(VEGA_PIXEL, 256);
			if (!pal)
			{
				OP_DELETEA(buf);
				return OpStatus::ERR_NO_MEMORY;
			}
			op_memset(pal, 0, sizeof(VEGA_PIXEL) * 256);

			palette = pal;
		}

		raw_buffer = buf;
	}

	pix_stride = width;
	return OpStatus::OK;
}

OP_STATUS VEGASWBuffer::CreateFromData(void* data, unsigned datastride, unsigned w, unsigned h, bool opaque)
{
	RETURN_IF_ERROR(VEGA_PIXEL_FORMAT_CLASS::CreateBufferFromData(ptr, data, w, h, opaque));
	width = w;
	height = h;
	type = VSWBUF_COLOR;
	isOpaque = opaque;
	bpp = VEGA_PIXEL_FORMAT_CLASS::BytesPerPixel;
	pix_stride = datastride / bpp;
	return OpStatus::OK;
}

VEGASWBuffer VEGASWBuffer::CreateSubset(unsigned x, unsigned y, unsigned w, unsigned h) const
{
	OP_ASSERT(x < width && y < height);
	OP_ASSERT(x+w <= width && y+h <= height);

	unsigned ofs = y * GetPixelStride() + x;
	VEGASWBuffer tmp = *this;
	if (type == VSWBUF_COLOR)
	{
		tmp.ptr = ptr + ofs;
	}
	else // STENCIL / INDEXED_COLOR
	{
		tmp.raw_buffer = reinterpret_cast<UINT8*>(raw_buffer) + ofs;
	}
	tmp.width = w;
	tmp.height = h;
	return tmp;
}

OP_STATUS VEGASWBuffer::Bind(VEGAPixelStore* ps, bool opaque)
{
	if (VEGA_PIXEL_FORMAT_CLASS::IsCompatible(ps->format))
	{
		// Do simple bind swbuffer <-> pixelstore
		RETURN_IF_ERROR(VEGA_PIXEL_FORMAT_CLASS::Bind(ptr, ps, opaque));

		width = ps->width;
		height = ps->height;
		type = VSWBUF_COLOR;
		isOpaque = opaque;
		bpp = VEGA_PIXEL_FORMAT_CLASS::BytesPerPixel;
		pix_stride = ps->stride / bpp;
	}
	else
	{
		// Create a clone of the pixelstore
		RETURN_IF_ERROR(Create(ps->width, ps->height, VSWBUF_COLOR, opaque));

		CopyFromPixelStore(ps);
	}
	return OpStatus::OK;
}

void VEGASWBuffer::Sync(VEGAPixelStore* ps, const OpRect* update_rects, unsigned int num_rects)
{
	if (!VEGA_PIXEL_FORMAT_CLASS::IsCompatible(ps->format))
		CopyToPixelStore(ps, update_rects, num_rects);
}

void VEGASWBuffer::Reload(VEGAPixelStore* ps)
{
	if (!VEGA_PIXEL_FORMAT_CLASS::IsCompatible(ps->format))
		CopyFromPixelStore(ps);
}

void VEGASWBuffer::Unbind(VEGAPixelStore* ps)
{
	if (VEGA_PIXEL_FORMAT_CLASS::IsCompatible(ps->format))
	{
		VEGA_PIXEL_FORMAT_CLASS::Unbind(ptr, ps);
	}
	else
	{
		Destroy();
	}
}

void VEGASWBuffer::Destroy()
{
	if (!IsBacked())
		return;

	if (type != VSWBUF_COLOR)
	{
		UINT8* del_buffer = (UINT8*)raw_buffer;
		OP_DELETEA(del_buffer);
		OP_DELETEA(palette);
	}
	else
	{
		VEGA_PIXEL_FORMAT_CLASS::DeleteBuffer(ptr);
	}
}

OP_STATUS VEGASWBuffer::Clone(VEGASWBuffer& dst) const
{
	RETURN_IF_ERROR(dst.Create(width, height, type, isOpaque));

	VEGAPixelAccessor dstp = dst.GetAccessor(0, 0);
	unsigned dstPixelStride = dst.GetPixelStride();

	VEGAConstPixelAccessor srcp = GetConstAccessor(0, 0);
	unsigned srcPixelStride = GetPixelStride();

	for (unsigned int yp = 0; yp < dst.height; ++yp)
	{
		dstp.CopyFrom(srcp.Ptr(), dst.width);

		dstp += dstPixelStride;
		srcp += srcPixelStride;
	}
	return OpStatus::OK;
}

void VEGASWBuffer::FillRect(unsigned x, unsigned y, unsigned w, unsigned h, VEGA_PIXEL color)
{
	OP_ASSERT(x < width && y < height);
	OP_ASSERT(x+w <= width && y+h <= height);

	unsigned dstPixelStride = GetPixelStride();

	if (type != VSWBUF_COLOR)
	{
		UINT8* dst = GetStencilPtr(x, y);

		for (unsigned i = 0; i < h; i++)
		{
			op_memset(dst, (UINT8)color, w);

			dst += dstPixelStride;
		}
	}
	else
	{
		VEGAPixelAccessor dst = GetAccessor(x, y);

		for (unsigned i = 0; i < h; ++i)
		{
			dst.Store(color, w);

			dst += dstPixelStride;
		}
	}
}

void VEGASWBuffer::MoveRect(int x, int y, unsigned int w, unsigned int h, int dx, int dy)
{
	OP_ASSERT(x >= 0 && y >= 0 && x+dx >= 0 && y+dy >= 0);
	OP_ASSERT(x+w <= width && x+dx+w <= width);
	OP_ASSERT(y+h <= height && y+dy+h <= height);

	VEGAConstPixelAccessor src = GetConstAccessor(x, y);
	VEGAPixelAccessor dst = GetAccessor(x + dx, y + dy);
	unsigned pixStride = GetPixelStride();

	// Move data line-by-line. Direction is important since we can have
	// source/destination overlap.
	if (dy > 0)
	{
		src += h * pixStride;
		dst += h * pixStride;
		for (unsigned int yp = 0; yp < h; ++yp)
		{
			src -= pixStride;
			dst -= pixStride;
			dst.CopyFrom(src.Ptr(), w);
		}
	}
	else if (dy < 0)
	{
		for (unsigned int yp = 0; yp < h; ++yp)
		{
			dst.CopyFrom(src.Ptr(), w);

			src += pixStride;
			dst += pixStride;
		}
	}
	else
	{
		for (unsigned int yp = 0; yp < h; ++yp)
		{
			dst.MoveFrom(src.Ptr(), w);

			src += pixStride;
			dst += pixStride;
		}
	}
}

void VEGASWBuffer::PremultiplyRect(const VEGASWBuffer& src,
								   unsigned left, unsigned top, unsigned right, unsigned bottom)
{
	OP_ASSERT(left < width && right < width);
	OP_ASSERT(top < height && bottom < height);
	OP_ASSERT(left < src.width && right < src.width);
	OP_ASSERT(top < src.height && bottom < src.height);

	OP_ASSERT(!IsIndexed() && !src.IsIndexed());

	VEGAConstPixelAccessor srcpa = src.GetConstAccessor(left, top);
	unsigned srcPixelStride = src.GetPixelStride();

	VEGAPixelAccessor dstpa = GetAccessor(left, top);
	unsigned dstPixelStride = GetPixelStride();

	unsigned width = right - left + 1;

	srcPixelStride -= width;
	dstPixelStride -= width;

	for (unsigned y = top; y <= bottom; ++y)
	{
		unsigned cnt = width;
		while (cnt-- > 0)
		{
			dstpa.Store(VEGAPixelPremultiplyFast(srcpa.Load()));

			++srcpa;
			++dstpa;
		}

		dstpa += dstPixelStride;
		srcpa += srcPixelStride;
	}
}

void VEGASWBuffer::UnPremultiplyRect(const VEGASWBuffer& src, unsigned left, unsigned top, unsigned right, unsigned bottom)
{
	OP_ASSERT(left < width && right < width);
	OP_ASSERT(top < height && bottom < height);
	OP_ASSERT(left < src.width && right < src.width);
	OP_ASSERT(top < src.height && bottom < src.height);

	OP_ASSERT(!IsIndexed() && !src.IsIndexed());

	VEGAConstPixelAccessor srcpa = src.GetConstAccessor(left, top);
	unsigned srcPixelStride = src.GetPixelStride();

	VEGAPixelAccessor dstpa = GetAccessor(left, top);
	unsigned dstPixelStride = GetPixelStride();

	unsigned width = right - left + 1;

	srcPixelStride -= width;
	dstPixelStride -= width;

	for (unsigned y = top; y <= bottom; ++y)
	{
		unsigned cnt = width;
		while (cnt-- > 0)
		{
			dstpa.Store(VEGAPixelUnpremultiplyFast(srcpa.Load()));

			++srcpa;
			++dstpa;
		}

		dstpa += dstPixelStride;
		srcpa += srcPixelStride;
	}
}

#endif // VEGA_SUPPORT
