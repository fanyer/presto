/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGASWBUFFER_H
#define VEGASWBUFFER_H

#include "modules/libvega/src/vegapixelformat.h"
#include "modules/libvega/vegapixelstore.h"

class OpBitmap;

enum VEGASWBufferType
{
	VSWBUF_COLOR,
	VSWBUF_STENCIL,
	VSWBUF_INDEXED_COLOR
};

class VEGASWBuffer
{
public:
	OP_STATUS Create(unsigned width, unsigned height, VEGASWBufferType buftype = VSWBUF_COLOR, bool opaque = false);
	OP_STATUS CreateFromData(void* data, unsigned datastride, unsigned w, unsigned h, bool opaque = false);
	OP_STATUS CreateFromBitmap(OpBitmap* bmp, bool& using_pointer);

	OP_STATUS Bind(VEGAPixelStore* ps, bool opaque = false);
	void Sync(VEGAPixelStore* ps, const OpRect* update_rects = NULL, unsigned int num_rects = 0);
	void Reload(VEGAPixelStore* ps);
	void Unbind(VEGAPixelStore* ps);

	void Reset() { VEGA_PIXEL_FORMAT_CLASS::ClearBuffer(ptr); palette = NULL; pix_stride = width = height = 0; }

	bool IsBacked() const { return raw_buffer != NULL; }
	bool IsShared(const VEGASWBuffer& other) const { return raw_buffer == other.raw_buffer; }
	bool IsIndexed() const { return type == VSWBUF_INDEXED_COLOR; }

	VEGASWBuffer CreateSubset(unsigned x, unsigned y, unsigned width, unsigned height) const;
	OP_STATUS Clone(VEGASWBuffer& dst) const;

	void Destroy();

	unsigned GetPixelStride() const { return pix_stride; }

	void* GetRawBufferPtr() const { return raw_buffer; }
	UINT8* GetStencilPtr(int x, int y) const
	{
		OP_ASSERT(type == VSWBUF_STENCIL);
		return (UINT8*)raw_buffer + y * (int)pix_stride + x;
	}
	UINT8* GetIndexedPtr(int x, int y) const
	{
		OP_ASSERT(type == VSWBUF_INDEXED_COLOR);
		return (UINT8*)raw_buffer + y * (int)pix_stride + x;
	}

	VEGA_PIXEL* GetPalette() const { return palette; }

	VEGAConstPixelAccessor GetConstAccessor(int x, int y) const
	{
		return VEGAConstPixelAccessor(ptr + y * GetPixelStride() + x);
	}
	VEGAPixelAccessor GetAccessor(int x, int y) const
	{
		return VEGAPixelAccessor(ptr + y * GetPixelStride() + x);
	}

	OP_STATUS CopyToBitmap(OpBitmap* bmp, unsigned int srcx, unsigned int srcy,
						   unsigned int dstx, unsigned int dsty, unsigned int w = 0, unsigned int h = 0);

	void CopyToPixelStore(VEGAPixelStore* store, const OpRect* update_rects = NULL, unsigned int num_rects = 0) const;
	void CopyToPixelStore(VEGAPixelStore* store, bool flipY, bool premultiplyAlpha = false, VEGAPixelStoreFormat requestedFormat = VPSF_SAME) const
	{
		CopyRectToPixelStore(store, OpRect(0, 0, width, height), flipY,
							 premultiplyAlpha, requestedFormat);

	}
	void CopyFromPixelStore(VEGAPixelStore* store);

	void FillRect(unsigned x, unsigned y, unsigned width, unsigned height,
				  VEGA_PIXEL color);
	void MoveRect(int x, int y, unsigned int width, unsigned int height,
				  int dx, int dy);

	void PremultiplyRect(const VEGASWBuffer& src,
						 unsigned left, unsigned top, unsigned right, unsigned bottom);

	// Helpers
	void Premultiply(const VEGASWBuffer& src)
	{
		PremultiplyRect(src, 0, 0, src.width-1, src.height-1);
	}
	void Premultiply() { Premultiply(*this); }

	void UnPremultiplyRect(const VEGASWBuffer& src, unsigned left, unsigned top, unsigned right, unsigned bottom);
	void UnPremultiply(const VEGASWBuffer& src)
	{
		UnPremultiplyRect(src, 0, 0, src.width-1, src.height-1);
	}
	void UnPremultiply() { UnPremultiply(*this); }

	VEGASWBufferType GetBufferType() const {return type;}

private:
	void CopyRectToPixelStore(VEGAPixelStore* store, const OpRect& rect,
							  bool flipY = false, bool premultiplyAlpha = false,
							  VEGAPixelStoreFormat requestedFormat = VPSF_SAME) const;

	VEGASWBufferType type;
	union
	{
		VEGA_PIXEL_FORMAT_CLASS::Pointer ptr;
		void* raw_buffer;
	};
	VEGA_PIXEL* palette;
	bool isOpaque;

public:
	unsigned pix_stride;
	unsigned bpp;

	unsigned width;
	unsigned height;
};

#endif // VEGASWBUFFER_H
