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

#if defined VEGA_INTERNAL_FORMAT_BGRA8888 || defined VEGA_INTERNAL_FORMAT_RGBA8888

#ifdef VEGA_USE_GENERIC_SWBUFFER
#error "VEGA_USE_GENERIC_SWBUFFER should not be enabled for BGRA8888"
#endif // VEGA_USE_GENERIC_SWBUFFER

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/libvega/src/vegabackend.h"
#else
#include "modules/pi/OpBitmap.h"
#endif

OP_STATUS VEGASWBuffer::CreateFromBitmap(OpBitmap* bmp, bool& using_pointer)
{
	// Can we access the raw BGRA8888 bitmap data?
	if (bmp->Supports(OpBitmap::SUPPORTS_POINTER) && bmp->GetBpp() == 32)
	{
#ifdef VEGA_INTERNAL_FORMAT_BGRA8888
		// Fast case: OpBitmap uses BGRA8888, so we can just reference the
		// bitmap data directly.
		width = bmp->Width();
		height = bmp->Height();
		ptr.rgba = (UINT32*)bmp->GetPointer();
		pix_stride = bmp->GetBytesPerLine() / 4;
		bpp = 4;
		type = VSWBUF_COLOR;

		using_pointer = true;
#else // VEGA_INTERNAL_FORMAT_BGRA8888
		// We need to do a format conversion from BGRA8888 to the VEGA
		// internal format.
		RETURN_IF_ERROR(Create(bmp->Width(), bmp->Height()));
		VEGAPixelStoreWrapper src_store(width, height, VPSF_BGRA8888, bmp->GetBytesPerLine(), bmp->GetPointer());
		CopyFromPixelStore(&src_store.ps);

		using_pointer = false;
#endif // VEGA_INTERNAL_FORMAT_BGRA8888
	}
	else
	{
#ifdef VEGA_OPPAINTER_SUPPORT
		// With OPPAINTER support, we know that the bmp is in fact a VEGAOpBitmap,
		// and we can clone the corresponding SW buffer directly (it's essentially
		// a memcpy, since source & destination format is always the VEGA internal
		// format).
		VEGAOpBitmap* vega_bmp = static_cast<VEGAOpBitmap*>(bmp);
		VEGABackingStore* src_backingstore = vega_bmp->GetBackingStore();
		VEGASWBuffer* buf = src_backingstore->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);
		if (!buf)
			return OpStatus::ERR;
		OP_STATUS status = buf->Clone(*this);
		src_backingstore->EndTransaction(FALSE);
		using_pointer = false;
		return status;
#else
		// We need to do a copy (and possibly a format conversion).
		RETURN_IF_ERROR(Create(bmp->Width(), bmp->Height()));

		// Read back the data (in BGRA8888 format).
		UINT32* dstp = ptr.rgba;
		for (unsigned int line = 0; line < height; ++line)
		{
			bmp->GetLineData(dstp, line);
			dstp += width;
		}

#ifndef VEGA_INTERNAL_FORMAT_BGRA8888
		// We need to do a format conversion from BGRA8888 to the VEGA
		// internal format - we do it in place.
		VEGAPixelStoreWrapper src_store(width, height, VPSF_BGRA8888, width * 4, ptr.rgba);
		CopyFromPixelStore(&src_store.ps);
#endif // VEGA_INTERNAL_FORMAT_BGRA8888

		using_pointer = false;
#endif // VEGA_OPPAINTER_SUPPORT
	}

	return OpStatus::OK;
}

OP_STATUS VEGASWBuffer::CopyToBitmap(OpBitmap* bmp,
									 unsigned int srcx, unsigned int srcy,
									 unsigned int dstx, unsigned int dsty,
									 unsigned int w, unsigned int h)
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

	UINT8* src = (UINT8*)ptr.rgba;
	unsigned srcstride = pix_stride * bpp;
	src += srcy * srcstride + srcx * 4;

	if (bmp->GetBpp() == 32 && bmp->Supports(OpBitmap::SUPPORTS_POINTER))
	{
		UINT8* dst = (UINT8*)bmp->GetPointer();
		if (!dst)
			return OpStatus::ERR;

		unsigned dststride = bmp->GetBytesPerLine();
		dst += dststride * dsty + dstx * 4;
		for (unsigned int i = 0; i < h; ++i)
		{
			op_memcpy(dst, src, w*4);

			src += srcstride;
			dst += dststride;
		}
		bmp->ReleasePointer();
	}
	else
	{
		if (dstx == 0 && w == bmp_width)
		{
			for (unsigned int i = 0; i < h; ++i)
			{
				bmp->AddLine(src, i + dsty);

				src += srcstride;
			}
		}
		else
		{
			UINT32* line = OP_NEWA(UINT32, bmp_width);
			if (!line)
				return OpStatus::ERR_NO_MEMORY;

			for (unsigned int i = 0; i < h; ++i)
			{
				bmp->GetLineData(line, dsty + i);
				op_memcpy(line + dstx, src, w * 4);
				bmp->AddLine(line, i + dsty);

				src += srcstride;
			}

			OP_DELETEA(line);
		}
	}
	return OpStatus::OK;
}

#endif // VEGA_INTERNAL_FORMAT_BGRA8888 || VEGA_INTERNAL_FORMAT_RGBA8888
#endif // VEGA_SUPPORT
