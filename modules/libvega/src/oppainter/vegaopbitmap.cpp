/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)

#ifdef EMBEDDED_ICC_SUPPORT
#include "modules/img/imagecolormanager.h"
#endif // EMBEDDED_ICC_SUPPORT

#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/libvega/src/oppainter/vegaopbitmap_tiled.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/vegarendertarget.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/src/vegaswbuffer.h"
#include "modules/libvega/src/vegabackend_sw.h"

/* static */
OP_STATUS OpBitmap::Create(OpBitmap **bitmap,
						   UINT32 width, UINT32 height,
						   BOOL transparent, BOOL alpha,
						   UINT32 transpcolor, INT32 indexed,
						   BOOL must_support_painter)
{
	OP_ASSERT(bitmap);

    if (!width  || !height)
        return OpStatus::ERR;

	VEGAOpBitmap* new_bitmap = OP_NEW(VEGAOpBitmap, (width, height, transparent, alpha,
													 transpcolor, indexed));
	if (new_bitmap == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = new_bitmap->Construct(must_support_painter, indexed != 0);
	if (OpStatus::IsSuccess(ret))
	{
		*bitmap = new_bitmap;
	}
	else
	{
		OP_DELETE(new_bitmap);
		*bitmap = 0;
	}
	return ret;
}

/* static */
OP_STATUS OpBitmap::CreateFromIcon(OpBitmap **bitmap, const uni_char* icon_path)
{
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}

/* static */
UINT32 VEGAOpBitmap::GetAllocationSize(UINT32 width, UINT32 height, BOOL transparent, BOOL alpha, INT32 indexed)
{
#ifdef SUPPORT_INDEXED_OPBITMAP
	if (indexed)
		return width*height+256*sizeof(VEGA_PIXEL)+sizeof(VEGASWBuffer);
#endif
#if defined(VEGA_INTERNAL_FORMAT_BGRA8888) || defined(VEGA_INTERNAL_FORMAT_RGBA8888)
	return width*height*4+sizeof(VEGASWBuffer);
#elif defined(VEGA_INTERNAL_FORMAT_RGB565A8)
	if (alpha)
		return width*height*3+sizeof(VEGASWBuffer);
	return width*height*2+sizeof(VEGASWBuffer);
#else
#error "No supported vega format specified"
#endif
}


VEGAOpBitmap::VEGAOpBitmap(UINT32 width, UINT32 height,
						   BOOL transparent, BOOL alpha,
						   UINT32 transpcolor, INT32 indexed) :
	backingstore(NULL), width(width), height(height),
	alpha(alpha!=0), transparent(transparent!=0),
	transColor(transpcolor), palette(NULL), paletteSize(0),
	painter(NULL), fill(NULL)
#ifdef VEGA_LIMIT_BITMAP_SIZE
	, bitmap_tiles(NULL)
#endif // VEGA_LIMIT_BITMAP_SIZE
{
	for (unsigned int i = 0; i < 9; ++i)
		subBitmapCache[i].bmp = NULL;
}

VEGAOpBitmap::~VEGAOpBitmap()
{
	for (unsigned int i = 0; i < 9; ++i)
		OP_DELETE(subBitmapCache[i].bmp);
	OP_DELETE(fill);
	OP_DELETEA(palette);
	OP_DELETE(painter);

	VEGARefCount::DecRef(backingstore);

#ifdef VEGA_LIMIT_BITMAP_SIZE
	OP_DELETE(bitmap_tiles);
#endif // VEGA_LIMIT_BITMAP_SIZE
}

OP_STATUS VEGAOpBitmap::Construct(BOOL support_painter, BOOL is_indexed)
{
	if (support_painter)
	{
#ifdef VEGA_LIMIT_BITMAP_SIZE
		OP_ASSERT(!needsTiling() && "requiring painter circumvents max bitmap size");
#endif // VEGA_LIMIT_BITMAP_SIZE

		painter = OP_NEW(VEGAOpPainter, ());
		if (!painter)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(painter->Construct(width, height, NULL));

		backingstore = painter->GetRenderTarget()->GetBackingStore();
		VEGARefCount::IncRef(backingstore);
		return OpStatus::OK;
	}

#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (needsTiling())
		// bitmap exceeds maximum allowed dimensions, split into tiles
		return ConstructTiles(VEGARendererBackend::maxBitmapSide(), is_indexed);
#endif // VEGA_LIMIT_BITMAP_SIZE

	return VEGARendererBackend::createBitmapStore(&backingstore, width, height, !!is_indexed, !(alpha||transparent));
}

#ifdef VEGA_USE_HW
OP_STATUS VEGAOpBitmap::Migrate()
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		OP_ASSERT(!"this operation is not allowed on tiled bitmaps, check calling code");
		return OpStatus::ERR; // better than crashing
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	OP_ASSERT(painter == NULL); // Shouldn't need to migrate these

	unsigned src_w = backingstore->GetWidth();
	unsigned src_h = backingstore->GetHeight();

	VEGABackingStore* local_clone;
	RETURN_IF_ERROR(VEGARendererBackend::createBitmapStore(&local_clone, src_w, src_h,
														   !!backingstore->IsIndexed(), !(alpha||transparent)));

	OP_STATUS status = local_clone->CopyRect(OpPoint(0, 0),
											 OpRect(0, 0, src_w, src_h),
											 backingstore);
	if (OpStatus::IsError(status))
	{
		VEGARefCount::DecRef(local_clone);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Copy the palette if any
	if (VEGA_PIXEL* srcpal = backingstore->GetPalette())
		if (VEGA_PIXEL* dstpal = local_clone->GetPalette())
			op_memcpy(dstpal, srcpal, sizeof(VEGA_PIXEL) * 256);

	// If the fill has been created (unlikely), then it points to the
	// old store
	OP_DELETE(fill);

	VEGARefCount::DecRef(backingstore);
	backingstore = local_clone;

	return OpStatus::OK;
}
#endif // VEGA_USE_HW

BOOL VEGAOpBitmap::Supports(SUPPORTS supports) const
{
	switch (supports)
	{
	case SUPPORTS_POINTER:
#ifdef VEGA_LIMIT_BITMAP_SIZE
		// possible, but would require allocating a huge buffer
		if (isTiled())
			return FALSE;
#endif // VEGA_LIMIT_BITMAP_SIZE
#ifdef VEGA_INTERNAL_FORMAT_BGRA8888
		return backingstore->IsIndexed() || backingstore->IsA(VEGABackingStore::SURFACE) ? FALSE : TRUE;
#else
	return FALSE;
#endif
	case SUPPORTS_PAINTER:
		return painter ? TRUE : FALSE;
	case SUPPORTS_INDEXED:
		return TRUE;
	case SUPPORTS_CREATETILE:
	case SUPPORTS_EFFECT:
	case SUPPORTS_OPACITY:
	default:
		break;
	}
	return FALSE;
}

UINT8 VEGAOpBitmap::GetBpp() const
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
		return bitmap_tiles->First()->First()->bmp->GetBpp();
#endif // VEGA_LIMIT_BITMAP_SIZE
	return backingstore->IsIndexed() ? 8 : backingstore->GetBytesPerPixel()*8;
}

UINT32 VEGAOpBitmap::GetBytesPerLine() const
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		UINT32 b = 0;
		for (VEGABitmapTile* t = bitmap_tiles->First()->First(); t; t = t->Suc())
			b += t->bmp->GetBytesPerLine();
		return b;
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	return backingstore->GetBytesPerLine();
}

OP_STATUS VEGAOpBitmap::CopyRect(const OpPoint& dest_point, const OpRect& src_rect, OpBitmap* srcbmp)
{
	VEGAOpBitmap* vsrc = static_cast<VEGAOpBitmap*>(srcbmp);

	// Validate source and destination rectangles
	OpRect src_area(0, 0, vsrc->width, vsrc->height);
	if (!src_area.Contains(src_rect))
		return OpStatus::ERR;

	OpRect dst_area(0, 0, width, height);
	OpRect dst_rect(dest_point.x, dest_point.y, src_rect.width, src_rect.height);
	if (!dst_area.Contains(dst_rect))
		return OpStatus::ERR;

#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
		return copyTiled(dest_point, src_rect, vsrc);

	if (vsrc->isTiled())
		return vsrc->copyFromTiled(src_rect, dest_point, this);
#endif // VEGA_LIMIT_BITMAP_SIZE

	return backingstore->CopyRect(dest_point, src_rect, vsrc->backingstore);
}

#ifdef VEGA_LIMIT_BITMAP_SIZE
class AddLineIterator : public VEGALineIterator
{
public:
	AddLineIterator(INT32 line, void* data, OP_STATUS (VEGAOpBitmap::*function)(void* data, INT32 line))
	: VEGALineIterator(line), data((char*)data), function(function) {}
	OP_STATUS action(VEGABitmapTile& tile, INT32 line)
	{
		const OP_STATUS status = (tile.bmp->*function)(data, line);
		data += tile.bmp->GetBytesPerLine();
		return status;
	}
private:
	char* data;
	OP_STATUS (VEGAOpBitmap::*function)(void* data, INT32 line);
};
#endif // VEGA_LIMIT_BITMAP_SIZE

OP_STATUS VEGAOpBitmap::AddLine(void* data, INT32 line)
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		AddLineIterator it(line, data, &VEGAOpBitmap::AddLine);
		return it.iterate(*bitmap_tiles);
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	if (!backingstore || line >= (INT32)height)
		return OpStatus::ERR;

	OP_ASSERT(!backingstore->IsIndexed());

	VEGASWBuffer* buf = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
	if (!buf)
		return OpStatus::ERR_NO_MEMORY;

	VEGAPixelAccessor dst = buf->GetAccessor(0, line);
	const UINT32* src = (const UINT32*)data;

	unsigned cnt = buf->width;
	while (cnt-- > 0)
	{
		UINT32 s = *src++;
		dst.StoreARGB(s >> 24, (s >> 16) & 0xff, (s >> 8) & 0xff, s & 0xff);

		++dst;
	}

	backingstore->EndTransaction(TRUE /* commit */);

	return OpStatus::OK;
}

#ifdef USE_PREMULTIPLIED_ALPHA
OP_STATUS VEGAOpBitmap::AddLineAndPremultiply(void* data, INT32 line)
{
	// FIXME: lots of code dup here
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		AddLineIterator it(line, data, &VEGAOpBitmap::AddLineAndPremultiply);
		return it.iterate(*bitmap_tiles);
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	if (!backingstore || line >= (INT32)height)
		return OpStatus::ERR;

	OP_ASSERT(!backingstore->IsIndexed());

	VEGASWBuffer* buf = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
	if (!buf)
		return OpStatus::ERR_NO_MEMORY;

	VEGAPixelAccessor dst = buf->GetAccessor(0, line);
	const UINT32* src = (const UINT32*)data;

	unsigned cnt = buf->width;
	while (cnt-- > 0)
	{
		UINT32 s = *src++;
		VEGA_PIXEL packed_color = VEGA_PACK_ARGB(s>>24, (s>>16)&0xff, (s>>8)&0xff, s&0xff);
		dst.Store(VEGAPixelPremultiplyFast(packed_color));

		++dst;
	}

	backingstore->EndTransaction(TRUE /* commit */);

	return OpStatus::OK;
}
#endif // USE_PREMULTIPLIED_ALPHA

#ifdef EMBEDDED_ICC_SUPPORT

#ifdef VEGA_LIMIT_BITMAP_SIZE
class AddLineWithTransformIterator : public VEGALineIterator
{
public:
	typedef OP_STATUS (VEGAOpBitmap::*pfn)(void* data, INT32 line, ImageColorTransform* color_transform);

	AddLineWithTransformIterator(INT32 line, void* data, ImageColorTransform* color_transform, pfn function)
		: VEGALineIterator(line), data((char*)data), transform(color_transform), function(function) {}
	OP_STATUS action(VEGABitmapTile& tile, INT32 line)
	{
		const OP_STATUS status = (tile.bmp->*function)(data, line, transform);
		data += tile.bmp->GetBytesPerLine();
		return status;
	}
private:
	char* data;
	ImageColorTransform* transform;
	pfn function;
};
#endif // VEGA_LIMIT_BITMAP_SIZE

OP_STATUS VEGAOpBitmap::AddLineWithColorTransform(void* data, INT32 line, ImageColorTransform* color_transform)
{
	OP_ASSERT(color_transform);

	// FIXME: lots of code dup here
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		AddLineWithTransformIterator it(line, data, color_transform, &VEGAOpBitmap::AddLineWithColorTransform);
		return it.iterate(*bitmap_tiles);
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	if (!backingstore || line >= (INT32)height)
		return OpStatus::ERR;

	OP_ASSERT(!backingstore->IsIndexed());

	VEGASWBuffer* buf = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
	if (!buf)
		return OpStatus::ERR_NO_MEMORY;

	VEGAPixelAccessor dst = buf->GetAccessor(0, line);
	const UINT32* src = static_cast<const UINT32*>(data);

	if (VEGA_PIXEL_FORMAT_CLASS::IsCompatible(VPSF_BGRA8888))
	{
		color_transform->Apply(dst.Ptr().rgba, src, buf->width);
	}
	else
	{
		if (UINT32* tmp = color_transform->Apply(src, buf->width))
		{
			src = tmp;
		}

		unsigned cnt = buf->width;
		while (cnt-- > 0)
		{
			UINT32 s = *src++;
			dst.StoreARGB(s >> 24, (s >> 16) & 0xff, (s >> 8) & 0xff, s & 0xff);

			++dst;
		}
	}

	backingstore->EndTransaction(TRUE /* commit */);

	return OpStatus::OK;
}
#endif // EMBEDDED_ICC_SUPPORT

#ifdef SUPPORT_INDEXED_OPBITMAP
OP_STATUS VEGAOpBitmap::AddIndexedLine(void* data,  INT32 line)
{
	// FIXME: lots of code dup here
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		AddLineIterator it(line, data, &VEGAOpBitmap::AddIndexedLine);
		return it.iterate(*bitmap_tiles);
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	if (!backingstore || line >= (INT32)height)
		return OpStatus::ERR;

	if (!backingstore->IsIndexed() && !palette)
		return OpStatus::ERR;

	VEGASWBuffer* buf = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
	if (!buf)
		return OpStatus::ERR_NO_MEMORY;

	if (!buf->IsIndexed())
	{
		VEGAPixelAccessor dst = buf->GetAccessor(0, line);
		const UINT8* src8 = (const UINT8*)data;

		unsigned cnt = buf->width;
		while (cnt-- > 0)
		{
			if (*src8 < paletteSize)
			{
				UINT32 s = palette[*src8];
				dst.StoreARGB(s >> 24, (s >> 16) & 0xff, (s >> 8) & 0xff, s & 0xff);
			}
			else
			{
				dst.Store(0);
			}

			++dst;
			++src8;
		}
	}
	else
	{
		UINT8* dst = buf->GetIndexedPtr(0, line);

		op_memcpy(dst, data, buf->width);
	}

	backingstore->EndTransaction(TRUE /* commit */);

	return OpStatus::OK;
}

BOOL VEGAOpBitmap::GetIndexedLineData(void* data, UINT32 line) const
{
	OP_ASSERT(!"Vega does not support GetIndexedLineData");
	return FALSE;
}

#ifdef VEGA_LIMIT_BITMAP_SIZE
class PaletteIterator : public VEGAEachIterator
{
public:
	PaletteIterator(UINT8* pal, UINT32 num_colors)
	: pal(pal), num_colors(num_colors) {}
	OP_STATUS action(VEGABitmapTile& tile)
	{
		return tile.bmp->SetPalette(pal, num_colors) ? OpStatus::OK : OpStatus::ERR;
	}
private:
	UINT8* pal;
	UINT32 num_colors;
};
#endif // VEGA_LIMIT_BITMAP_SIZE

BOOL VEGAOpBitmap::SetPalette(UINT8* pal, UINT32 num_colors)
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		PaletteIterator it(pal, num_colors);
		return OpStatus::IsSuccess(it.iterate(*bitmap_tiles));
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	if (!backingstore->IsIndexed())
	{
		if (num_colors > paletteSize)
		{
			UINT32* newpal = OP_NEWA(UINT32, num_colors);
			if (!newpal)
				return FALSE;

			OP_DELETEA(palette);
			palette = newpal;
			paletteSize = num_colors;
		}

		for (unsigned int c = 0; c < num_colors; ++c)
		{
			palette[c] = (0xffu<<24) | (pal[c*3]<<16) | (pal[c*3+1]<<8) | (pal[c*3+2]);
		}

		if (transparent && transColor < paletteSize)
		{
			palette[transColor] = 0;
		}
	}
	else
	{
		VEGA_PIXEL* bmp_pal = backingstore->GetPalette();
		for (unsigned int c = 0; c < num_colors; ++c)
		{
			bmp_pal[c] = VEGA_PACK_RGB(pal[c*3], pal[c*3+1], pal[c*3+2]);
		}

		if (transparent && transColor < 256)
		{
			bmp_pal[transColor] = 0;
		}
	}

	return TRUE;
}

BOOL VEGAOpBitmap::GetPalette(UINT8* pal) const
{
	OP_ASSERT(!"Vega does not support GetPalette");
	return FALSE;
}
#endif // SUPPORT_INDEXED_OPBITMAP

#ifdef VEGA_LIMIT_BITMAP_SIZE
class SetColorEachIterator : public VEGAEachIterator
{
public:
	SetColorEachIterator(UINT8* color, BOOL all_transparent)
	: color(color), all_transparent(all_transparent) {}
	OP_STATUS action(VEGABitmapTile& tile) { return tile.bmp->SetColor(color, all_transparent, NULL); }
private:
	UINT8* color;
	BOOL all_transparent;
};

class SetColorRectIterator : public VEGARectIterator
{
public:
	SetColorRectIterator(const OpRect& rect, UINT8* color, BOOL all_transparent)
	: VEGARectIterator(rect), color(color), all_transparent(all_transparent) {}
	OP_STATUS action(VEGABitmapTile& tile, const OpRect& rect, const unsigned src_offs_x, const unsigned src_offs_y)
	{
		return tile.bmp->SetColor(color, all_transparent, &rect);
	}
private:
	UINT8* color;
	BOOL all_transparent;
};
#endif // VEGA_LIMIT_BITMAP_SIZE

BOOL VEGAOpBitmap::SetColor(UINT8* color, BOOL all_transparent, const OpRect* rect)
{
	const UINT32 col = all_transparent ? 0 : *(const UINT32*)color;
	const OpRect all(0, 0, Width(), Height());

#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		if (!rect || rect->Equals(all))
		{
			SetColorEachIterator it(color, all_transparent);
			return OpStatus::IsSuccess(it.iterate(*bitmap_tiles));
		}
		else
		{
			SetColorRectIterator it(*rect, color, all_transparent);
			return OpStatus::IsSuccess(it.iterate(*bitmap_tiles));
		}
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	backingstore->SetColor(rect ? *rect : all, col);
	return TRUE;
}

#ifdef VEGA_LIMIT_BITMAP_SIZE
class GetLineIterator : public VEGALineIterator
{
public:
	GetLineIterator(INT32 line, void* data) : VEGALineIterator(line), data((char*)data) {}
	OP_STATUS action(VEGABitmapTile& tile, INT32 line)
	{
		const OP_STATUS status = tile.bmp->GetLineData(data, line) ? OpStatus::OK : OpStatus::ERR;
		data += tile.bmp->GetBytesPerLine();
		return status;
	}
private:
	char* data;
};
#endif // VEGA_LIMIT_BITMAP_SIZE

BOOL VEGAOpBitmap::GetLineData(void* data, UINT32 line) const
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		GetLineIterator it(line, data);
		return OpStatus::IsSuccess(it.iterate(*bitmap_tiles));
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	if (!backingstore || line >= height)
		return FALSE;

	VEGASWBuffer* buf = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);
	if (!buf)
		return FALSE;

	if (!backingstore->IsIndexed())
	{
		UINT32* dst = (UINT32*)data;
		VEGAConstPixelAccessor src = buf->GetConstAccessor(0, line);

		unsigned cnt = buf->width;
		while (cnt-- > 0)
		{
			unsigned a, r, g, b;

			src.LoadUnpack(a, r, g, b);
			++src;

			*dst++ = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
	else
	{
		UINT32* dst = (UINT32*)data;
		const UINT8* isrc = buf->GetIndexedPtr(0, line);
		const VEGA_PIXEL* pal = buf->GetPalette();

		unsigned cnt = buf->width;
		while (cnt-- > 0)
		{
			VEGA_PIXEL px = pal[*isrc++];

			unsigned a = VEGA_UNPACK_A(px);
			unsigned r = VEGA_UNPACK_R(px);
			unsigned g = VEGA_UNPACK_G(px);
			unsigned b = VEGA_UNPACK_B(px);

			*dst++ = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}

	backingstore->EndTransaction(FALSE /* no need to commit */);

	return TRUE;
}

static inline VEGABackingStore::AccessType FromOpBitmapAccess(OpBitmap::AccessType acc)
{
	switch (acc)
	{
	case OpBitmap::ACCESS_READONLY:
		return VEGABackingStore::ACC_READ_ONLY;
	case OpBitmap::ACCESS_WRITEONLY:
		return VEGABackingStore::ACC_WRITE_ONLY;
	default:
	case OpBitmap::ACCESS_READWRITE:
		break;
	}
	return VEGABackingStore::ACC_READ_WRITE;
}

void* VEGAOpBitmap::GetPointer(AccessType access_type)
{
#if defined(VEGA_INTERNAL_FORMAT_BGRA8888) || defined(VEGA_INTERNAL_FORMAT_RGBA8888)
	if (painter)
		painter->present();

#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		OP_ASSERT(!"Supports(SUPPORTS_POINTER) should have returned FALSE for this bitmap");
		return NULL;
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	const VEGASWBuffer* dstbuf = backingstore->BeginTransaction(FromOpBitmapAccess(access_type));
	return dstbuf ? dstbuf->GetRawBufferPtr() : NULL;
#else
	OP_ASSERT(!"GetPointer() is only supported for BGRA32");
	return NULL;
#endif
}

void VEGAOpBitmap::ReleasePointer(BOOL changed)
{
#if defined(VEGA_INTERNAL_FORMAT_BGRA8888) || defined(VEGA_INTERNAL_FORMAT_RGBA8888)
	backingstore->EndTransaction(changed);
#endif
}

OpPainter* VEGAOpBitmap::GetPainter()
{
	return painter;
}

void VEGAOpBitmap::ReleasePainter()
{
}

UINT32 VEGAOpBitmap::Width() const
{
	return width;
}

UINT32 VEGAOpBitmap::Height() const
{
	return height;
}

BOOL VEGAOpBitmap::HasAlpha() const
{
	return alpha;
}

BOOL VEGAOpBitmap::IsTransparent() const
{
	return transparent;
}

UINT32 VEGAOpBitmap::GetTransparentColorIndex() const
{
	return 0;
}

OP_STATUS VEGAOpBitmap::getFill(VEGARenderer* rend, VEGAFill** outfill)
{
	OP_STATUS ret = OpStatus::OK;
	if (painter)
	{
		VEGAFill* f = NULL;
		ret = painter->GetRenderTarget()->getImage(&f);
		*outfill = f;
		return ret;
	}
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (isTiled())
	{
		OP_ASSERT(!"this operation is not allowed on tiled bitmaps, check calling code");
		return OpStatus::ERR; // better than crashing
	}
#endif // VEGA_LIMIT_BITMAP_SIZE
	if (!fill)
	{
		ret = rend->createImage(&fill, this);
	}
	*outfill = fill;
	return ret;
}

OpBitmap* VEGAOpBitmap::GetSubBitmap(const OpRect& rect)
{
	OpBitmap* sliced_bitmap;
	for (unsigned int i = 0; i < 9; ++i)
	{
		if (subBitmapCache[i].bmp && subBitmapCache[i].rect.Equals(rect))
			return subBitmapCache[i].bmp;
	}
	if (OpStatus::IsSuccess(OpBitmap::Create(&sliced_bitmap, rect.width, rect.height, IsTransparent(), HasAlpha())))
	{
		VEGAOpBitmap *vb = (VEGAOpBitmap *) sliced_bitmap;
		if (OpStatus::IsError(vb->CopyRect(OpPoint(0, 0), rect, this)))
			OP_DELETE(sliced_bitmap);
		else
		{
			OP_DELETE(subBitmapCache[8].bmp);
			for (unsigned int i = 8; i > 0; --i)
				subBitmapCache[i] = subBitmapCache[i-1];
			subBitmapCache[0].rect = rect;
			subBitmapCache[0].bmp = sliced_bitmap;
			return sliced_bitmap;
		}
	}
	return NULL;
}

void VEGAOpBitmap::ReleaseSubBitmap()
{
}

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT

