/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/img/src/imagecontent.h"
#include "modules/img/src/imagemanagerimp.h"
#include "modules/display/colconv.h"

#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpBitmap.h"

#ifdef EMBEDDED_ICC_SUPPORT
#include "modules/img/imagecolormanager.h"
#endif // EMBEDDED_ICC_SUPPORT

#if defined(VEGA_OPPAINTER_SUPPORT) && defined(USE_PREMULTIPLIED_ALPHA)
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#endif

#include "modules/probetools/probepoints.h"

#define COL_OFS_R 2
#define COL_OFS_G 1
#define COL_OFS_B 0
#define COL_OFS_A 3

static inline void SetPixelWithDestAlpha_BGRA32( unsigned char *dst8, unsigned char *src8, unsigned char srca )
{
	if (dst8[COL_OFS_A] == 0)
	{
		dst8[COL_OFS_B] = src8[COL_OFS_B];
		dst8[COL_OFS_G] = src8[COL_OFS_G];
		dst8[COL_OFS_R] = src8[COL_OFS_R];
		dst8[COL_OFS_A] = srca;
	}
	else
	{
#ifdef USE_PREMULTIPLIED_ALPHA
		dst8[COL_OFS_B] = ((dst8[COL_OFS_B]*(256-srca))>>8)+src8[COL_OFS_B];
		dst8[COL_OFS_G] = ((dst8[COL_OFS_G]*(256-srca))>>8)+src8[COL_OFS_G];
		dst8[COL_OFS_R] = ((dst8[COL_OFS_R]*(256-srca))>>8)+src8[COL_OFS_R];
		dst8[COL_OFS_A] = ((dst8[COL_OFS_A]*(256-srca))>>8)+src8[COL_OFS_A];
#else // !USE_PREMULTIPLIED_ALPHA
		int total_alpha = MIN(dst8[COL_OFS_A] + srca, 255);
		int a = (srca<<8) / (total_alpha + 1);
		dst8[COL_OFS_B] += a * (src8[COL_OFS_B] - dst8[COL_OFS_B]) >> 8;
		dst8[COL_OFS_G] += a * (src8[COL_OFS_G] - dst8[COL_OFS_G]) >> 8;
		dst8[COL_OFS_R] += a * (src8[COL_OFS_R] - dst8[COL_OFS_R]) >> 8;
		dst8[COL_OFS_A] += (unsigned char) (long((srca) * (256 - dst8[COL_OFS_A])) >> 8);
#endif // !USE_PREMULTIPLIED_ALPHA
	}
}


FrameElm::~FrameElm()
{
	OP_ASSERT(ref_count == 0);
	OP_ASSERT(bitmap_buffer == NULL);
	OP_DELETE(bitmap);
}

void FrameElm::IncMemUsed(AnimatedImageContent* animated_image_content, INT32 size)
{
	OP_ASSERT(mem_used == 0);
	mem_used = size;
	((ImageManagerImp*)imgManager)->IncAnimMemUsed(animated_image_content, mem_used);
}

void FrameElm::DecRefCount(AnimatedImageContent* animated_image_content)
{
	OP_NEW_DBG("FrameElm::DecRefCount", "animation");
	OP_DBG(("this: %p, ref_count: %d", this, ref_count));

	ref_count--;
	if (ref_count <= 0)
	{
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
		if (animated_image_content->IsLarge() || animated_image_content->NrOfRepeats() == 1)
#else
		if (TRUE)
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
		{
			OP_ASSERT(ref_count == 0);
			((ImageManagerImp*)imgManager)->DecAnimMemUsed(mem_used);
			mem_used = 0;
			OP_DELETE(bitmap_buffer);
			bitmap_buffer = NULL;
		}
	}
}

void FrameElm::ClearBuffer()
{
	((ImageManagerImp*)imgManager)->DecAnimMemUsed(mem_used);
	mem_used = 0;
	OP_DELETE(bitmap_buffer);
	bitmap_buffer = NULL;
}

ImageContentType NullImageContent::Type()
{
	return NULL_IMAGE_CONTENT;
}

OpBitmap* NullImageContent::GetBitmap(ImageListener* image_listener)
{
	return NULL;
}

INT32 NullImageContent::Width()
{
	return 0;
}

INT32 NullImageContent::Height()
{
	return 0;
}

BOOL NullImageContent::IsTransparent()
{
	return TRUE;
}

BOOL NullImageContent::IsInterlaced()
{
	return TRUE;
}

INT32 NullImageContent::GetFrameCount()
{
	return 0;
}

INT32 NullImageContent::GetBitsPerPixel()
{
	return 0;
}

INT32 NullImageContent::GetLastDecodedLine()
{
	return 0;
}

EmptyImageContent::EmptyImageContent(INT32 width, INT32 height) : width(width), height(height)
{
	OP_ASSERT(width > 0);
	OP_ASSERT(height > 0);
}

ImageContentType EmptyImageContent::Type()
{
	return EMPTY_IMAGE_CONTENT;
}

INT32 EmptyImageContent::Width()
{
	return width;
}

INT32 EmptyImageContent::Height()
{
	return height;
}

OpBitmap* GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value); // imgblt.cpp

BitmapImageContent::BitmapImageContent(OpBitmap* bitmap) : bitmap(bitmap), bitmap_effect(NULL)
{
}

BitmapImageContent::~BitmapImageContent()
{
	OP_DELETE(bitmap);
}

ImageContentType BitmapImageContent::Type()
{
	return BITMAP_IMAGE_CONTENT;
}

OpBitmap* BitmapImageContent::GetBitmap(ImageListener* image_listener)
{
	return bitmap;
}

INT32 BitmapImageContent::Width()
{
	return bitmap->Width();
}

INT32 BitmapImageContent::Height()
{
	return bitmap->Height();
}

BOOL BitmapImageContent::IsTransparent()
{
	return TRUE;
}

BOOL BitmapImageContent::IsInterlaced()
{
	return FALSE;
}

INT32 BitmapImageContent::GetFrameCount()
{
	return 0;
}

INT32 BitmapImageContent::GetBitsPerPixel()
{
	return 0;
}

INT32 BitmapImageContent::GetLastDecodedLine()
{
	return bitmap->Height();
}

OpBitmap* BitmapImageContent::GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value, ImageListener* image_listener)
{
	OP_ASSERT(bitmap_effect == NULL); // We should not try to get a new effectbitmap without having released the previous one.

	OpBitmap* tmp = ::GetEffectBitmap(bitmap, effect, effect_value);
	if (tmp != bitmap)
		bitmap_effect = tmp;

	return bitmap_effect ? bitmap_effect : bitmap;
}

void BitmapImageContent::ReleaseEffectBitmap()
{
	OP_DELETE(bitmap_effect);
	bitmap_effect = NULL;
}

StaticImageContent::StaticImageContent(OpBitmap* bitmap, INT32 width, INT32 height, const OpRect& rect,
									   BOOL transparent, BOOL alpha, BOOL interlaced, INT32 bits_per_pixel,
									   BOOL bottom_to_top)
									   : bitmap(bitmap), bitmap_tile(NULL), bitmap_effect(NULL),
										 rect(rect), total_width(width), total_height(height),
										 bits_per_pixel(bits_per_pixel), last_decoded_line(0),
										 is_transparent(transparent), is_alpha(alpha), is_interlaced(interlaced),
										 bottom_to_top(bottom_to_top), lowest_decoded_line(0)
{
	OP_ASSERT(width > 0);
	OP_ASSERT(height > 0);

	if (bottom_to_top)
	{
		lowest_decoded_line = rect.height;
	}
}

StaticImageContent::~StaticImageContent()
{
	OP_DELETE(bitmap);
	OP_DELETE(bitmap_tile);
	OP_DELETE(bitmap_effect);
}

ImageContentType StaticImageContent::Type()
{
	return STATIC_IMAGE_CONTENT;
}

OpPoint StaticImageContent::GetBitmapOffset()
{
	return OpPoint(rect.x, rect.y);
}

OpBitmap* StaticImageContent::GetBitmap(ImageListener* image_listener)
{
	return bitmap;
}

INT32 StaticImageContent::Width()
{
	return total_width;
}

INT32 StaticImageContent::Height()
{
	return total_height;
}

BOOL StaticImageContent::IsTransparent()
{
	return is_transparent || is_alpha;
}

BOOL StaticImageContent::IsInterlaced()
{
	return is_interlaced;
}

INT32 StaticImageContent::GetFrameCount()
{
	return 1;
}

INT32 StaticImageContent::GetBitsPerPixel()
{
	return bits_per_pixel;
}

INT32 StaticImageContent::GetLastDecodedLine()
{
	if (last_decoded_line < rect.height)
		return last_decoded_line + rect.y;
	else
		return total_height;
}

void StaticImageContent::ReplaceBitmap(OpBitmap* new_bitmap)
{
	OP_DELETE(bitmap);
	bitmap = new_bitmap;
	total_width = bitmap->Width();
	total_height = bitmap->Height();
	rect.width = total_width;
	rect.height = total_height;
	OP_DELETE(bitmap_tile);
	bitmap_tile = NULL;
	last_decoded_line = total_height;
}

#define	TILE_SQUARE_LIMIT	64

/*static */OpBitmap* StaticImageContent::UpdateTileBitmapIfNeeded(OpBitmap* bitmap, OpBitmap*& bitmap_tile, int desired_width, int desired_height)
{
	// Don't make larger tilebitmaps of anything larger than TILE_SQUARE_LIMIT * TILE_SQUARE_LIMIT (for memory reasons)
	// F.ex. a bitmap size of 10000x1 could be sent in, so if we created a 10000 * TILE_SQUARE_LIMIT of that, it would use a lot memory.
	if(bitmap->Width() * bitmap->Height() < TILE_SQUARE_LIMIT * TILE_SQUARE_LIMIT)
	{
		desired_width = MIN(desired_width, TILE_SQUARE_LIMIT);
		desired_height = MIN(desired_height, TILE_SQUARE_LIMIT);
		UINT32 tile_width = ((desired_width + bitmap->Width() - 1) / bitmap->Width()) * bitmap->Width();
		UINT32 tile_height = ((desired_height + bitmap->Height() - 1) / bitmap->Height()) * bitmap->Height();

		if (bitmap_tile)
		{
			// We already have a optimized tilebitmap, but is it large enough for this desired width and height?
			if (bitmap_tile->Width() >= tile_width && bitmap_tile->Height() >= tile_height)
				return bitmap_tile;

			// We need a larger bitmap_tile so delete the old one first.
			OP_DELETE(bitmap_tile);
			bitmap_tile = NULL;
		}

		if (tile_width != bitmap->Width() || tile_height != bitmap->Height())
		{
/*#ifdef STRICT_CACHE_LIMIT
			mem_used_by_tile = g_op_screen_info->GetBitmapAllocationSize(tile_width, tile_height,
																		 TRUE, TRUE, FALSE, FALSE);
			if (!IncImgMemoryCount(mem_used_by_tile))
			{
				return bitmap;
			}
#endif // STRICT_CACHE_LIMIT*/
			if(bitmap->Supports(OpBitmap::SUPPORTS_CREATETILE))
			{
				bitmap_tile = bitmap->CreateTile(tile_width, tile_height);
			}
			else
			{
				OpBitmap* CreateTileOptimizedBitmap(OpBitmap* srcbitmap, INT32 new_width, INT32 new_height);
				bitmap_tile = CreateTileOptimizedBitmap(bitmap, tile_width, tile_height);
				if (bitmap_tile == bitmap)
				{
					// CreateTileOptimizedBitmap returns the source bitmap on error.
					// It is a very dangerous practice since it easily leads to double frees.
					// Setting bitmap_tile to NULL here solves the problem.
					bitmap_tile = NULL;
				}
			}

/*#ifndef STRICT_CACHE_LIMIT
			if (bitmap_tile && mem_used_by_tile)
			{
				mem_used_by_tile = g_op_screen_info->GetBitmapAllocationSize(bitmap_tile->Width(), bitmap_tile->Height(),
									bitmap_tile->IsTransparent(), bitmap_tile->HasAlpha(), bitmap_tile->IsIndexed(), FALSE);
				IncImgMemoryCount(mem_used_by_tile);
			}
#endif // STRICT_CACHE_LIMIT*/
		}
	}

	return bitmap_tile ? bitmap_tile : bitmap;
}

OpBitmap* StaticImageContent::GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height)
{
	BOOL tileable = (rect.x == 0 && rect.y == 0 && 
			total_width == rect.width && 
			total_height == rect.height);	
	if (!tileable)
	{
		// Create a new bitmap which covers the entire size
		OpBitmap* bmp;
		if (OpStatus::IsSuccess(OpBitmap::Create(&bmp, 
						total_width, total_height, 
						FALSE, TRUE)))
		{
			UINT32* data = OP_NEWA(UINT32, total_width);
			if (data)
			{
				int line;
				op_memset(data, 0, total_width*4);
				for (line = 0; line < rect.y; ++line)
				{
					bmp->AddLine(data, line);
				}
				for (;line < rect.y+rect.height; ++line)
				{
					bitmap->GetLineData(data+rect.x, line-rect.y);
					bmp->AddLine(data, line);
				}
				op_memset(data, 0, total_width*4);
				for (; line < total_height; ++line)
				{
					bmp->AddLine(data, line);
				}
				OpBitmap* tmp = bmp;
				bmp = bitmap;
				bitmap = tmp;
				if (last_decoded_line < rect.height)
					last_decoded_line += rect.y;
				else
					last_decoded_line = total_height;
				rect = OpRect(0, 0, total_width, total_height);
				is_transparent = FALSE;
				is_alpha = TRUE;
				bits_per_pixel = 32;
				OP_DELETE(bitmap_tile);
				bitmap_tile = NULL;
				OP_DELETE(bitmap_effect);
				bitmap_effect = NULL;

			}
			OP_DELETEA(data);
			OP_DELETE(bmp);
		}
	}
	return UpdateTileBitmapIfNeeded(bitmap, bitmap_tile, desired_width, desired_height);
}

void StaticImageContent::ReleaseTileBitmap()
{
}

OpBitmap* StaticImageContent::GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value, ImageListener* image_listener)
{
	OP_ASSERT(bitmap_effect == NULL); // We should not try to get a new effectbitmap without having released the previous one.

	OpBitmap* tmp = ::GetEffectBitmap(bitmap, effect, effect_value);
	if (tmp != bitmap)
		bitmap_effect = tmp;

	return bitmap_effect ? bitmap_effect : bitmap;
}

void StaticImageContent::ReleaseEffectBitmap()
{
	OP_DELETE(bitmap_effect);
	bitmap_effect = NULL;
}

OpBitmap* StaticImageContent::GetTileEffectBitmap(INT32 effect, INT32 effect_value, int horizontal_count, int vertical_count)
{
	OpBitmap* tile = UpdateTileBitmapIfNeeded(bitmap, bitmap_tile, bitmap->Width() * horizontal_count, bitmap->Height() * vertical_count);
	return GetEffectBitmap(tile, effect, effect_value);
}

void StaticImageContent::ReleaseTileEffectBitmap()
{
	ReleaseEffectBitmap();
}

BOOL StaticImageContent::IsBottomToTop()
{
	return bottom_to_top;
}

UINT32 StaticImageContent::GetLowestDecodedLine()
{
	if (lowest_decoded_line > 0)
		return lowest_decoded_line + rect.y;
	else
		return 0;
}

///////////////////////
// ImageFrameBitmap ///
///////////////////////

ImageFrameBitmap::ImageFrameBitmap(OpBitmap* bmp, BOOL del)
{
	m_delete_bitmap = del;
	m_bitmap = bmp;
	m_data32 = NULL;
#ifdef SUPPORT_INDEXED_OPBITMAP
	m_data8 = NULL;
	m_palette = NULL;
	m_palette_size = 0;
#endif // SUPPORT_INDEXED_OPBITMAP
}

ImageFrameBitmap::ImageFrameBitmap(UINT32* data, unsigned int width, unsigned int height, BOOL transparent, BOOL alpha, UINT32 transpcolor)
{
	m_delete_bitmap = FALSE;
	m_bitmap = NULL;
	m_data32 = data;
#ifdef SUPPORT_INDEXED_OPBITMAP
	m_data8 = NULL;
#endif // SUPPORT_INDEXED_OPBITMAP
	m_width = width;
	m_height = height;
#ifdef SUPPORT_INDEXED_OPBITMAP
	m_palette = NULL;
	m_palette_size = 0;
#endif // SUPPORT_INDEXED_OPBITMAP
	m_transparent = transparent;
	m_alpha = alpha;
	m_transpcolor = transpcolor;
}

ImageFrameBitmap::ImageFrameBitmap(UINT8* data, unsigned int width, unsigned int height, BOOL transparent, BOOL alpha, UINT32 transpcolor)
{
	m_delete_bitmap = FALSE;
	m_bitmap = NULL;
	m_data32 = NULL;
#ifdef SUPPORT_INDEXED_OPBITMAP
	m_data8 = data;
#endif // SUPPORT_INDEXED_OPBITMAP
	m_width = width;
	m_height = height;
#ifdef SUPPORT_INDEXED_OPBITMAP
	m_palette = NULL;
	m_palette_size = 0;
#endif // SUPPORT_INDEXED_OPBITMAP
	m_transparent = transparent;
	m_alpha = alpha;
	m_transpcolor = transpcolor;
}

ImageFrameBitmap::~ImageFrameBitmap()
{
	if (m_delete_bitmap)
		OP_DELETE(m_bitmap);
	OP_DELETEA(m_data32);
#ifdef SUPPORT_INDEXED_OPBITMAP
	OP_DELETEA(m_data8);
	OP_DELETEA(m_palette);
#endif // SUPPORT_INDEXED_OPBITMAP
}

OP_STATUS ImageFrameBitmap::AddLine(void* data, unsigned int line)
{
	if (m_bitmap)
		m_bitmap->AddLine(data, line);
	else if (m_data32)
	{
		op_memcpy(m_data32+m_width*line, data, m_width*4);
	}
	else
	{
		OP_ASSERT(FALSE);
	}
	return OpStatus::OK;
}
#ifdef USE_PREMULTIPLIED_ALPHA
OP_STATUS ImageFrameBitmap::AddLineAndPremultiply(void* data, unsigned int line)
{
	UINT32* premul = NULL;
	int bitmap_width = Width();
#ifdef VEGA_OPPAINTER_SUPPORT
	if (m_bitmap)
	{
		return ((VEGAOpBitmap*)m_bitmap)->AddLineAndPremultiply(data, line);
	}
#else
	if (m_bitmap)
	{
		premul = ((ImageManagerImp*)imgManager)->GetScratchBuffer(bitmap_width);
		if (!premul)
			return OpStatus::ERR_NO_MEMORY;
	}
	else
#endif
	if (m_data32)
	{
		premul = m_data32+m_width*line;
	}
	else
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}


	for (int xp = 0; xp < bitmap_width; ++xp)
	{
		int a = (((UINT32*)data)[xp]>>24);
		if (a == 255)
			premul[xp] = ((UINT32*)data)[xp];
		else if (a == 0)
			premul[xp] = 0;
		else
		{
			int r = (((UINT32*)data)[xp]>>16)&0xff;
			int g = (((UINT32*)data)[xp]>>8)&0xff;
			int b = (((UINT32*)data)[xp])&0xff;
			r = (r*(a+1))>>8;
			g = (g*(a+1))>>8;
			b = (b*(a+1))>>8;
			premul[xp] = (a<<24) | (r<<16) | (g<<8) | b;
		}
	}

#ifndef VEGA_OPPAINTER_SUPPORT
	if (m_bitmap)
		return m_bitmap->AddLine(premul, line);
#endif
	return OpStatus::OK;
}
#endif // USE_PREMULTIPLIED_ALPHA

#ifdef EMBEDDED_ICC_SUPPORT
OP_STATUS ImageFrameBitmap::AddLineWithColorTransform(void* data, unsigned int line, ImageColorTransform* color_transform)
{
	OP_ASSERT(color_transform);

#ifdef VEGA_OPPAINTER_SUPPORT
	if (m_bitmap)
	{
		return  ((VEGAOpBitmap*)m_bitmap)->AddLineWithColorTransform(data, line, color_transform);
	}
	else
#endif //VEGA_OPPAINTER_SUPPORT
	{
		if (UINT32* tmp = color_transform->Apply(static_cast<UINT32*>(data), m_width))
		{
			data = tmp;
		}

		return AddLine(data, line);
	}
}
#endif // EMBEDDED_ICC_SUPPORT

#ifdef SUPPORT_INDEXED_OPBITMAP
OP_STATUS ImageFrameBitmap::AddIndexedLine(void* data, unsigned int line)
{
	if (m_bitmap)
		m_bitmap->AddIndexedLine(data, line);
	else if (m_data8)
	{
		op_memcpy(m_data8+m_width*line, data, m_width);
	}
	else
	{
		OP_ASSERT(FALSE);
	}
	return OpStatus::OK;
}

OP_STATUS ImageFrameBitmap::SetPalette(UINT8* pal, unsigned int size)
{
	OP_DELETEA(m_palette);
	m_palette_size = 0;
	m_palette = OP_NEWA(UINT8, 256*3);
	if (!m_palette)
		return OpStatus::ERR_NO_MEMORY;
	op_memcpy(m_palette, pal, size*3);
	m_palette_size = size;
	for (int pe = size*3; pe < 256*3; ++pe)
		m_palette[pe] = 255;
	return OpStatus::OK;
}
#endif // SUPPORT_INDEXED_OPBITMAP

// Stuff from OpBitmap
unsigned int ImageFrameBitmap::Width()
{
	return m_bitmap?m_bitmap->Width():m_width;
}
unsigned int ImageFrameBitmap::Height()
{
	return m_bitmap?m_bitmap->Height():m_height;
}
BOOL ImageFrameBitmap::IsTransparent()
{
	return m_bitmap?m_bitmap->IsTransparent():m_transparent;
}
BOOL ImageFrameBitmap::HasAlpha()
{
	return m_bitmap?m_bitmap->HasAlpha():m_alpha;
}
#ifdef SUPPORT_INDEXED_OPBITMAP
int ImageFrameBitmap::GetTransparentColorIndex()
{
	return m_bitmap?m_bitmap->GetTransparentColorIndex():m_transpcolor;
}
#endif // SUPPORT_INDEXED_OPBITMAP

OP_STATUS ImageFrameBitmap::CopyToTransparent(OpBitmap* dst, const OpPoint& pnt, BOOL dont_blend_prev)
{
	OP_PROBE6(OP_PROBE_IMG_COPYTOTRANSPARENT);
	UINT8 *fromBuf, *toBuf, *pal;

	BOOL fromLineAlloc = FALSE;
#ifdef SUPPORT_INDEXED_OPBITMAP
	int pal_size = 0;
	BOOL indexed = (dst->GetBpp() <= 8 && (m_data8 || (m_bitmap && m_bitmap->GetBpp() <= 8)));
	if (indexed)
	{
		if (m_data8)
		{
			fromBuf = m_data8;
			pal = m_palette;
			pal_size = m_palette_size;
		}
		else
		{
            const UINT32 width = Width();
			fromBuf = OP_NEWA(UINT8, width);
			pal = OP_NEWA(UINT8, 768);
			pal_size = 256;
			fromLineAlloc = TRUE;
		}
        const UINT32 bpl = dst->GetBytesPerLine();
		toBuf = OP_NEWA(UINT8, bpl);
	}
	else
#endif
	{
		if (m_data32)
			fromBuf = (UINT8*)m_data32;
		else
		{
            const UINT32 width = Width();
			fromBuf = OP_NEWA(UINT8, width * 4);
			fromLineAlloc = TRUE;
		}
        const UINT32 width = dst->Width();
		toBuf = OP_NEWA(UINT8, width * 4);
		pal = NULL;
	}

	if (fromBuf == NULL || toBuf == NULL 
#ifdef SUPPORT_INDEXED_OPBITMAP
		|| (indexed && pal == NULL)
#endif
		)
	{
		if (fromLineAlloc)
		{
			OP_DELETEA(fromBuf);
			OP_DELETEA(pal);
		}
		OP_DELETEA(toBuf);
		return OpStatus::ERR_NO_MEMORY;
	}

	INT32 fromHeight = Height();
	INT32 fromWidth = Width();

	INT32 x = pnt.x;
	INT32 y = pnt.y;

	if (x + fromWidth > (INT32)dst->Width())
		fromWidth = dst->Width() - x;
	if (y + fromHeight > (INT32)dst->Height())
		fromHeight = dst->Height() - y;

	if (fromWidth > 0 && fromHeight > 0)
	{
		BOOL combine_alpha = HasAlpha() && !dont_blend_prev;
		BOOL combine_transparent = IsTransparent() && !dont_blend_prev;
		for (INT32 i = 0; i < fromHeight; i++)
		{
			OP_ASSERT((INT32)dst->Height() >= i + y);

#ifdef SUPPORT_INDEXED_OPBITMAP
			if (indexed)
			{
				// Indexed version

				if (fromLineAlloc)
				{
					OP_ASSERT(m_bitmap);
					m_bitmap->GetIndexedLineData(fromBuf, i);
					m_bitmap->GetPalette(pal);
				}

				dst->GetIndexedLineData(toBuf, i + y);
				dst->SetPalette(pal, pal_size);

				OP_ASSERT(dst->GetBpp() == 8);
				UINT8* toBufStart = toBuf + x;
				UINT8* fromBufStart = fromBuf;

				if (combine_transparent)
				{
					UINT32 transp_index = GetTransparentColorIndex();

					for (int j = 0; j < fromWidth; j++)
						if (fromBufStart[j] != transp_index)
							toBufStart[j] = fromBufStart[j];
				}
				else
				{
					op_memcpy(toBufStart, fromBufStart, fromWidth);
				}

				OpStatus::Ignore(dst->AddIndexedLine(toBuf, i + y));

				if (!fromLineAlloc)
					fromBuf += Width();

				continue;
			}
#endif // SUPPORT_INDEXED_OPBITMAP

			if (fromLineAlloc)
			{
				if (m_bitmap)
					m_bitmap->GetLineData(fromBuf, i);
#ifdef SUPPORT_INDEXED_OPBITMAP
				else if (m_data8)
				{
					for (unsigned int xp = 0; xp < Width(); ++xp)
					{
						int idx = m_data8[Width()*i+xp];
						if (m_transparent && idx == GetTransparentColorIndex())
							((UINT32*)fromBuf)[xp] = 0;
						else
						{
							((UINT32*)fromBuf)[xp] = 0xff000000 | (m_palette[idx*3]<<16) | (m_palette[idx*3+1]<<8) | m_palette[idx*3+2];
						}
					}
				}
#endif // SUPPORT_INDEXED_OPBITMAP
				else
				{
					OP_ASSERT(FALSE);
				}
			}
			dst->GetLineData(toBuf, i + y);

			UINT8* toBufStart = toBuf + x * 4;
			UINT8* fromBufStart = fromBuf;

			if(combine_alpha)
			{
				for (INT32 j = 0; j < fromWidth; j++, fromBufStart += 4, toBufStart += 4)
				{
					UINT8 alpha = fromBufStart[COL_OFS_A];
					if(alpha == 0xFF)
					{
						*((UINT32*)toBufStart) = *((UINT32*)fromBufStart);
					}
					else if(alpha)
					{
						SetPixelWithDestAlpha_BGRA32(toBufStart, fromBufStart, alpha);
					}
				}
			}
			else if(combine_transparent)
			{
				for (INT32 j = 0; j < fromWidth; j++, fromBufStart += 4, toBufStart += 4)
				{
					if (*((UINT32*)fromBufStart) >> 24)
					{
						*((UINT32*)toBufStart) = *((UINT32*)fromBufStart);
					}
				}
			}
			else
			{
				op_memcpy(toBufStart, fromBufStart, fromWidth*4);
			}

			OpStatus::Ignore(dst->AddLine(toBuf, i + y)); // FIXME:OOM-KILSMO

			if (!fromLineAlloc)
				fromBuf += Width()*4;
		}
	}
	if (fromLineAlloc)
	{
		OP_DELETEA(fromBuf);
		OP_DELETEA(pal);
	}
	OP_DELETEA(toBuf);
	return OpStatus::OK;
}

OP_STATUS ImageFrameBitmap::CopyTo(OpBitmap* dst)
{
	OP_PROBE6(OP_PROBE_IMG_COPYTO);
	OP_ASSERT(dst->Width() == Width() && dst->Height() == Height());

	if (m_bitmap)
		return AnimatedImageContent::CopyBitmap(dst, m_bitmap);
	if (m_data32)
	{
		for (unsigned int i = 0; i < Height(); ++i)
			dst->AddLine(m_data32+i*Width(), i);
	}
#ifdef SUPPORT_INDEXED_OPBITMAP
	else if (m_data8)
	{
		if (dst->GetBpp() == 8)
		{
			dst->SetPalette(m_palette, m_palette_size);
			for (unsigned int i = 0; i < Height(); ++i)
				dst->AddIndexedLine(m_data8+i*Width(), i);
		}
		else
		{
            const UINT32 width = dst->Width();
			UINT32* tmp = OP_NEWA(UINT32, width);
			if (!tmp)
				return OpStatus::ERR_NO_MEMORY;
			for (unsigned int i = 0; i < Height(); ++i)
			{
				for (unsigned int x = 0; x < Width(); ++x)
				{
					int idx = m_data8[dst->Width()*i+x];
					if (m_transparent && idx == GetTransparentColorIndex())
						tmp[x] = 0;
					else
					{
						tmp[x] = 0xff000000 | (m_palette[idx*3]<<16) | (m_palette[idx*3+1]<<8) | m_palette[idx*3+2];
					}
				}
				dst->AddLine(tmp, i);
			}
			OP_DELETEA(tmp);
		}
	}
#endif // SUPPORT_INDEXED_OPBITMAP
	else
	{
		OP_ASSERT(FALSE);
	}
	return OpStatus::OK;
}

OP_STATUS ImageFrameBitmap::CopyTo(OpBitmap* dst, const OpRect& rect)
{
	// DOES NOT HANDLE CLIPPED BITMAPS
	OP_PROBE6(OP_PROBE_IMG_COPYTO2);

	if (rect.width == (INT32)dst->Width() && rect.height == (INT32)dst->Height())
	{
		return CopyTo(dst);
	}
	INT32 width_bytes = dst->Width() * 4;
	INT32 copy_offset = rect.x * 4;
	INT32 copy_size = rect.width * 4;
	INT32 start_line = rect.y;
	INT32 last_line = start_line + rect.height;
	if (m_bitmap)
	{
		unsigned char* buf = OP_NEWA(unsigned char, width_bytes);
		unsigned char* srcbuf = OP_NEWA(unsigned char, width_bytes);
		if (buf == NULL || srcbuf == NULL)
		{
			OP_DELETEA(buf);
			OP_DELETEA(srcbuf);
			return OpStatus::ERR_NO_MEMORY;
		}

		unsigned char* copy_dest = buf + copy_offset;
		unsigned char* copy_src = srcbuf + copy_offset;

		for (INT32 y = start_line; y < last_line; y++)
		{
			m_bitmap->GetLineData(srcbuf, y);
			dst->GetLineData(buf, y);
			op_memcpy(copy_dest, copy_src, copy_size);
			OpStatus::Ignore(dst->AddLine(buf, y)); // FIXME:OOM-KILSMO
		}

		OP_DELETEA(buf);
		OP_DELETEA(srcbuf);
	}
	else if (m_data32)
	{
		unsigned char* buf = OP_NEWA(unsigned char, width_bytes);
		if (buf == NULL)
		{
			OP_DELETEA(buf);
			return OpStatus::ERR_NO_MEMORY;
		}

		unsigned char* copy_dest = buf + copy_offset;
		unsigned char* copy_src = ((unsigned char*)m_data32) + copy_offset;

		for (INT32 y = start_line; y < last_line; y++)
		{
			dst->GetLineData(buf, y);
			op_memcpy(copy_dest, copy_src, copy_size);
			OpStatus::Ignore(dst->AddLine(buf, y)); // FIXME:OOM-KILSMO
			copy_src += Width()*4;
		}

		OP_DELETEA(buf);
	}
#ifdef SUPPORT_INDEXED_OPBITMAP
	else if (m_data8)
	{
		unsigned char* buf = OP_NEWA(unsigned char, width_bytes);
		if (buf == NULL)
		{
			OP_DELETEA(buf);
			return OpStatus::ERR_NO_MEMORY;
		}

		unsigned char* copy_src = m_data8;

		for (INT32 y = start_line; y < last_line; y++)
		{
			dst->GetLineData(buf, y);
			for (int x = rect.x; x < rect.x+rect.width; ++x)
			{
				int idx = copy_src[x];
				if (m_transparent && idx == GetTransparentColorIndex())
					((UINT32*)buf)[x] = 0;
				else
				{
					((UINT32*)buf)[x] = 0xff000000 | (m_palette[idx*3]<<16) | (m_palette[idx*3+1]<<8) | m_palette[idx*3+2];
				}
			}
			OpStatus::Ignore(dst->AddLine(buf, y)); // FIXME:OOM-KILSMO
			copy_src += Width();
		}

		OP_DELETEA(buf);
	}
#endif // SUPPORT_INDEXED_OPBITMAP
	return OpStatus::OK;
}



///////////////////////////
// AnimatedImageContent ///
///////////////////////////

OP_STATUS AnimatedImageContent::MakeBitmapTransparent(OpBitmap* bitmap)
{
	UINT32 color = 0;
	BOOL ret = bitmap->SetColor((UINT8*)&color, TRUE, NULL);
	if (ret)
		return OpStatus::OK;

#ifdef SUPPORT_INDEXED_OPBITMAP
	BOOL indexed = (bitmap->GetBpp() <= 8);
	if (indexed)
	{
		INT32 width = bitmap->Width();
		unsigned char* buf = OP_NEWA(unsigned char, width);
		if (buf == NULL)
			return OpStatus::ERR_NO_MEMORY;

		op_memset(buf, bitmap->GetTransparentColorIndex(), width);

		INT32 height = bitmap->Height();
		for (INT32 y = 0; y < height; y++)
			bitmap->AddIndexedLine(buf, y);

		OP_DELETEA(buf);
		return OpStatus::OK;
	}
#endif // SUPPORT_INDEXED_OPBITMAP

	INT32 width_bytes = bitmap->Width()* 4;
	unsigned char* buf = OP_NEWA(unsigned char, width_bytes);
	if (buf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	op_memset(buf, 0, width_bytes);
	INT32 height = bitmap->Height();
	for (INT32 y = 0; y < height; y++)
	{
		OpStatus::Ignore(bitmap->AddLine(buf, y)); // FIXME:OOM-KILSMO
	}
	OP_DELETEA(buf);
	return OpStatus::OK;
}

OP_STATUS AnimatedImageContent::MakeBitmapTransparent(OpBitmap* bitmap, const OpRect& rect)
{
	UINT32 color = 0;
	OpRect clipped_rect = rect;
	clipped_rect.IntersectWith(OpRect(0, 0, bitmap->Width(), bitmap->Height()));

	if (clipped_rect.IsEmpty())
	{
		return OpStatus::OK;
	}

	if (bitmap->SetColor((UINT8*)&color, TRUE, &clipped_rect))
	{
		return OpStatus::OK;
	}

    const UINT32 width = bitmap->Width();
	unsigned char* buf = OP_NEWA(unsigned char, width * 4);

	if (buf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	INT32 transp_offset = clipped_rect.x * 4;
	INT32 transp_size = clipped_rect.width * 4;
	unsigned char* transp_start = buf + transp_offset;
	INT32 start_line = clipped_rect.y;
	INT32 last_line = start_line + clipped_rect.height;

	for (INT32 y = start_line; y < last_line; y++)
	{
		bitmap->GetLineData(buf, y);
		op_memset(transp_start, 0, transp_size);
		OpStatus::Ignore(bitmap->AddLine(buf, y)); // FIXME:OOM-KILSMO
	}
	OP_DELETEA(buf);
	return OpStatus::OK;
}

OP_STATUS AnimatedImageContent::CopyBitmap(OpBitmap* dst, OpBitmap* src)
{
	OP_ASSERT(dst->Width() == src->Width() && dst->Height() == src->Height());
	if (src->Supports(OpBitmap::SUPPORTS_POINTER) && src->GetBpp() == 32)
	{
		UINT8* srcbuf = (UINT8*) src->GetPointer();
		UINT32 stride = src->GetBytesPerLine();
		for (UINT32 i = 0; i < src->Height(); i++)
		{
			dst->AddLine(&srcbuf[i * stride], i);
		}
		src->ReleasePointer();
		return OpStatus::OK;
	}
#ifdef SUPPORT_INDEXED_OPBITMAP
	if (src->GetBpp() <= 8 && dst->GetBpp() <= 8)
	{
        const UINT32 size = MAX(dst->GetBytesPerLine() + dst->GetBytesPerLine(), 768);
        unsigned char* buf = OP_NEWA(unsigned char, size);
		if (buf == NULL)
			return OpStatus::ERR_NO_MEMORY;

		unsigned char* buf1 = buf;
		unsigned char* buf2 = buf + dst->GetBytesPerLine();

		INT32 height = dst->Height();
		for (INT32 y = 0; y < height; y++)
		{
			src->GetIndexedLineData(buf1, y);
			ConvertColorIndexed(buf1, buf2, dst->Width(), src->GetBpp(), 8, NULL, 0, NULL);
			OpStatus::Ignore(dst->AddIndexedLine(buf2, y)); // FIXME:OOM-KILSMO
		}

		src->GetPalette(buf);
		dst->SetPalette(buf, 256);

		OP_DELETEA(buf);
		return OpStatus::OK;
	}
#endif // SUPPORT_INDEXED_OPBITMAP
    const UINT32 width = dst->Width();
	unsigned char* buf = OP_NEWA(unsigned char, width * 4);
	if (buf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	INT32 height = dst->Height();
	for (INT32 y = 0; y < height; y++)
	{
		src->GetLineData(buf, y);
		OpStatus::Ignore(dst->AddLine(buf, y)); // FIXME:OOM-KILSMO
	}
	OP_DELETEA(buf);
	return OpStatus::OK;
}

AnimatedImageContent::AnimatedImageContent(INT32 width, INT32 height, INT32 nr_of_repeats, BOOL pal_is_shared)
	: bitmap_tile(NULL), bitmap_effect(NULL), total_width(width), total_height(height), 
	  nr_of_repeats(nr_of_repeats), nr_of_frames(0), is_decoded(FALSE), pal_is_shared(pal_is_shared), tmp_combine_array(NULL)
{
	is_large = (width * height) > 200 * 100;
}

AnimatedImageContent::~AnimatedImageContent()
{
	OP_ASSERT(listener_hash.GetCount() == 0);
	listener_hash.DeleteAll();
	for (FrameElm* elm = (FrameElm*)frame_list.First(); elm != NULL; elm = (FrameElm*)elm->Suc())
	{
		elm->ClearBuffer();
	}
	frame_list.Clear();
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	((ImageManagerImp*)imgManager)->RemoveAnimatedImage(this);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	OP_DELETEA(tmp_combine_array);
}

OP_STATUS AnimatedImageContent::AddFrame(ImageFrameBitmap* bitmap, const OpRect& rect, INT32 duration, DisposalMethod disposal_method, BOOL dont_blend_prev)
{
	BOOL use_alpha = FALSE;
	BOOL is_transparent = FALSE;
	BOOL create_buffer = TRUE;
	BOOL first_frame = nr_of_frames == 0;
	BOOL covers_all = rect.Equals(OpRect(0,0,total_width,total_height));
	FrameElm* elm = OP_NEW(FrameElm, ());
	FrameElm *pred_elm = first_frame ? NULL : (FrameElm*)frame_list.Last();
	BOOL pred_clear_all = first_frame || pred_elm->flags & FrameElm::FRAME_FLAGS_CLEAR_ALL;

	if(elm == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	elm->ref_count = 0;
	elm->bitmap = bitmap;
	elm->rect = rect;
	elm->duration = duration;
	elm->disposal_method = disposal_method;
	elm->dont_blend_prev = dont_blend_prev;
	elm->bitmap_buffer = NULL;
	elm->mem_used = 0;
	elm->flags = 0;
	elm->pred_combine = NULL;

	// For the first frame, allocate 16 pointers, later 32, 64, 128,...
	if(!(nr_of_frames & (nr_of_frames-1)) && !(nr_of_frames > 0 && nr_of_frames < 16))
	{
        const UINT32 size = nr_of_frames ? nr_of_frames*2 : 16;
		FrameElm **new_tmp_combine_array = OP_NEWA(FrameElm*, size);
		if(!new_tmp_combine_array)
		{
			elm->bitmap = NULL; // avoid double delete in caller.
			OP_DELETE(elm);
			return OpStatus::ERR_NO_MEMORY;
		}
		OP_DELETEA(tmp_combine_array);
		tmp_combine_array = new_tmp_combine_array;
	}

	use_alpha = bitmap->HasAlpha();
	is_transparent = bitmap->IsTransparent();
	
	if((first_frame || (!use_alpha && !is_transparent) || dont_blend_prev) && covers_all)
	{
		elm->flags |= FrameElm::FRAME_FLAGS_USE_ORG_BITMAP;
		create_buffer = FALSE;
	}
	if(disposal_method == DISPOSAL_METHOD_RESTORE_PREVIOUS && pred_clear_all)
	{
		elm->disposal_method = DISPOSAL_METHOD_RESTORE_BACKGROUND;
	}
	if(elm->disposal_method == DISPOSAL_METHOD_RESTORE_BACKGROUND && (covers_all || pred_clear_all))
	{
		elm->flags |= FrameElm::FRAME_FLAGS_CLEAR_ALL;
	}

	if(create_buffer)
	{
		FrameElm *combine_elm = pred_elm;
		while(combine_elm && !(combine_elm->flags & FrameElm::FRAME_FLAGS_CLEAR_ALL))
		{
			if(combine_elm->disposal_method == DISPOSAL_METHOD_RESTORE_PREVIOUS)
				combine_elm = (FrameElm*)combine_elm->Pred();
			else
			{
				elm->pred_combine = combine_elm;
				use_alpha = use_alpha || combine_elm->flags & FrameElm::FRAME_FLAGS_COMBINED_ALPHA;
				is_transparent = is_transparent || combine_elm->flags & FrameElm::FRAME_FLAGS_COMBINED_TRANSPARENT;
				break;
			}
		}
	}
	
	if(use_alpha)
		elm->flags |= FrameElm::FRAME_FLAGS_COMBINED_ALPHA;
	if(is_transparent)
		elm->flags |= FrameElm::FRAME_FLAGS_COMBINED_TRANSPARENT;
	nr_of_frames++;
	elm->Into(&frame_list);
	return OpStatus::OK;
}

ImageContentType AnimatedImageContent::Type()
{
	return ANIMATED_IMAGE_CONTENT;
}

OpBitmap* AnimatedImageContent::GetBitmap(ImageListener* image_listener)
{
	OP_NEW_DBG("AnimatedImageContent::GetBitmap", "animation");
	OP_DBG(("this: %p, image_listener: %p", this, image_listener));
	
	if(image_listener == NULL)
	{
		FrameElm* frame_elm = GetFrameElm(0);
		OP_ASSERT(frame_elm);
		if(frame_elm == NULL)
			return NULL;
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
		((ImageManagerImp*)imgManager)->TouchAnimated(this);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
		return frame_elm->bitmap->GetOpBitmap();
	}
	
	AnimationListenerElm* elm = GetAnimationListenerElm(image_listener);

	if (elm == NULL)
		return NULL;

	FrameElm* frame_elm = GetFrameElm(elm->frame_nr);
	OP_ASSERT(frame_elm != NULL);

	BOOL use_org_bitmap = !!(frame_elm->flags & FrameElm::FRAME_FLAGS_USE_ORG_BITMAP) &&
		frame_elm->bitmap->GetOpBitmap();

	if(!use_org_bitmap && frame_elm->bitmap_buffer == NULL)
	{
		OP_STATUS ret_val = OpStatus::OK;

		if (frame_elm->pred_combine == NULL)
		{
			if(frame_elm->flags & FrameElm::FRAME_FLAGS_USE_ORG_BITMAP)
			{
				ret_val = CreateBitmapBuffer(frame_elm,frame_elm->bitmap->IsTransparent(),frame_elm->bitmap->HasAlpha());
				if(OpStatus::IsSuccess(ret_val))
					ret_val = frame_elm->bitmap->CopyTo(frame_elm->bitmap_buffer);
			}
			else
				ret_val = CreateFirstBitmap(frame_elm);
		}
		else
			ret_val = AnimateInternal(elm, frame_elm);

		if (OpStatus::IsError(ret_val))
		{
			OP_DELETE(frame_elm->bitmap_buffer);
			frame_elm->bitmap_buffer = NULL;
			return NULL; // return a bitmap?
		}
		INT32 memused = g_op_screen_info->GetBitmapAllocationSize(total_width, total_height,
																  TRUE, FALSE, 0);
		frame_elm->IncMemUsed(this, memused);
	}
	frame_elm->ref_count++;
	if (elm->last_frame)
		elm->last_frame->DecRefCount(this);
	elm->last_frame = frame_elm;

	elm->last_frame_painted_nr = elm->frame_nr;

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	((ImageManagerImp*)imgManager)->TouchAnimated(this);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

	return use_org_bitmap ? frame_elm->bitmap->GetOpBitmap() : frame_elm->bitmap_buffer;
}

OpBitmap* AnimatedImageContent::GetTileBitmap(ImageListener* image_listener, int desired_width, int desired_height)
{
	// Create a new tile if we don't have one
	if(bitmap_tile)
		return bitmap_tile;
	OpBitmap* bitmap = GetBitmap(image_listener);
	if (bitmap == NULL)
		return NULL;
	return StaticImageContent::UpdateTileBitmapIfNeeded(bitmap, bitmap_tile, desired_width, desired_height);
}

void AnimatedImageContent::ReleaseTileBitmap()
{
	OP_DELETE(bitmap_tile);
	bitmap_tile = NULL;
}

OpBitmap* AnimatedImageContent::GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value, ImageListener* image_listener)
{
	// Create a new effect if we don't have one
	if(bitmap_effect)
	{
		return bitmap_effect;
	}
	OpBitmap* tmp = GetBitmap(image_listener);
	if (tmp == NULL)
	{
		return NULL;
	}
	OpBitmap* tmp_effect = ::GetEffectBitmap(tmp, effect, effect_value);
	if (tmp_effect != tmp)
		bitmap_effect = tmp_effect;

	return bitmap_effect ? bitmap_effect : tmp;
}

void AnimatedImageContent::ReleaseEffectBitmap()
{
	OP_DELETE(bitmap_effect);
	bitmap_effect = NULL;
}

INT32 AnimatedImageContent::Width()
{
	return total_width;
}

INT32 AnimatedImageContent::Height()
{
	return total_height;
}

BOOL AnimatedImageContent::IsTransparent()
{
	return TRUE;
}

BOOL AnimatedImageContent::IsInterlaced()
{
	return FALSE;
}

INT32 AnimatedImageContent::GetFrameCount()
{
	return nr_of_frames;
}

void AnimatedImageContent::SyncronizeAnimation(ImageListener* dest_listener, ImageListener* source_listener)
{
	AnimationListenerElm* dst_elm = GetAnimationListenerElm(dest_listener);
	AnimationListenerElm* src_elm = GetAnimationListenerElm(source_listener);
	
	if (src_elm == NULL || dst_elm == NULL)
		return;

	OP_ASSERT(GetFrameElm(dst_elm->frame_nr) != NULL && GetFrameElm(src_elm->frame_nr) != NULL);

	if (dst_elm->last_frame)
		dst_elm->last_frame->DecRefCount(this);

	dst_elm->frame_nr = src_elm->frame_nr;
	dst_elm->loop_nr = src_elm->loop_nr;
	dst_elm->last_frame_painted_nr = src_elm->last_frame_painted_nr;
	dst_elm->last_frame = NULL;//new_frame_elm;
}

INT32 AnimatedImageContent::GetBitsPerPixel()
{
	FrameElm* elm = (FrameElm*)frame_list.First();
	OP_ASSERT(elm != NULL);
	// First frame is always a OpBitmap
	return elm->bitmap->GetOpBitmap()->GetBpp(); // FIXME:IMG
}

INT32 AnimatedImageContent::GetLastDecodedLine()
{
	return total_height;
}

OP_STATUS AnimatedImageContent::CreateBitmapBuffer(FrameElm *frame_elm, BOOL is_transparent, BOOL use_alpha)
{
#ifdef SUPPORT_INDEXED_OPBITMAP
// Disabled 8bit animation buffers for now. There are too many bugs both in platforms and various places in core where 8bit data isn't handled.
// Also, even when the palette is shared, many gif-animations have different transparent indexes for different frames which must be handled properly in all places.
//	INT32 indexed = pal_is_shared ? 8 : 0;
	INT32 indexed = 0;
	return OpBitmap::Create(&frame_elm->bitmap_buffer, total_width, total_height, is_transparent, use_alpha, indexed ? frame_elm->bitmap->GetTransparentColorIndex() : 0, indexed);
#else
	// We can't get indexed data back from OpBitmaps so we can't use indexed bitmaps.
	return OpBitmap::Create(&frame_elm->bitmap_buffer, total_width, total_height, is_transparent, use_alpha, 0, 0);
#endif
}

OP_STATUS AnimatedImageContent::CreateFirstBitmap(FrameElm* frame_elm)
{
	OP_STATUS ret_val = CreateFirstBitmapInternal(frame_elm);
	if (OpStatus::IsError(ret_val))
	{
		OP_DELETE(frame_elm->bitmap_buffer);
		frame_elm->bitmap_buffer = NULL;
	}
	return ret_val;
}

BOOL AnimatedImageContent::Animate(ImageListener* image_listener)
{
	OP_NEW_DBG("AnimatedImageContent::Animate", "animation");
	OP_DBG(("this: %p, image_listener: %p", this, image_listener));
	OP_ASSERT(image_listener != NULL);
	AnimationListenerElm* elm = GetAnimationListenerElm(image_listener);
	if (!elm)
		return FALSE;
	FrameElm* frame_elm = GetFrameElm(elm->frame_nr);
	OP_ASSERT(frame_elm != NULL);
	FrameElm* next_frame_elm = (FrameElm*)frame_elm->Suc();
	INT32 next_frame_nr = elm->frame_nr + 1;
	if (next_frame_elm == NULL)
	{
		if (!CanLoop(elm))
		{
			return FALSE;
		}
		next_frame_nr = 0;
	}
	if (next_frame_nr == 0)
	{
		elm->loop_nr++;
	}

	// Some useful debugprintouts..
	/*OpString str;
	int i = 0;
	frame_elm = GetFrameElm(0);
	str.Set("");
	while(frame_elm)
	{
		str.AppendFormat(UNI_L("%d"), frame_elm->ref_count);
		frame_elm = (FrameElm*) frame_elm->Suc();
	}
	OutputDebugString(str.CStr());
	OutputDebugString(UNI_L("   "));
	i = 0;
	frame_elm = GetFrameElm(0);
	str.Set("");
	while(frame_elm)
	{
		str.AppendFormat(UNI_L("%d"), frame_elm->bitmap_buffer ? 1 : 0);
		frame_elm = (FrameElm*) frame_elm->Suc();
	}
	OutputDebugString(str.CStr());
	OutputDebugString(UNI_L("\n"));*/

	elm->frame_nr = next_frame_nr;

	return TRUE;
}

void AnimatedImageContent::ResetAnimation(ImageListener* image_listener)
{
	AnimationListenerElm* elm = GetAnimationListenerElm(image_listener);
	if (elm && !CanLoop(elm))
	{
		if (elm->last_frame)
		{
			elm->last_frame->DecRefCount(this);
			elm->last_frame = NULL;
		}

		elm->frame_nr = 0;
		elm->last_frame_painted_nr = -1;
		elm->loop_nr = 0;
	}
}

OP_STATUS AnimatedImageContent::IncVisible(ImageListener* image_listener)
{
	OP_NEW_DBG("AnimatedImageContent::IncVisible", "animation");
	OP_DBG(("this: %p, image_listener: %p", this, image_listener));

	AnimationListenerElm* elm = GetAnimationListenerElm(image_listener);
	if (elm)
	{
		// Normally, there should be only one AnimationListenerElm per listener, but it is possible that some code call IncVisible twice with the same listener.
		// Since we can't add the same key to listener_hash twice, and want it to have the same frame number anyway, we reuse and refcount AnimationListenerElm.
		elm->ref_count++;
		return OpStatus::OK;
	}

	elm = OP_NEW(AnimationListenerElm, ());
	if (elm == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	if (OpStatus::IsError(listener_hash.Add((const void*) image_listener, (void *)elm)))
	{
		OP_DELETE(elm);
		return OpStatus::ERR_NO_MEMORY;
	}
	elm->listener = image_listener;
	elm->frame_nr = 0;
	elm->last_frame_painted_nr = -1;
	elm->loop_nr = 0;
	elm->last_frame = NULL;
	elm->ref_count = 1;

	FrameElm* frame_elm = (FrameElm*)frame_list.First();
	OP_ASSERT(frame_elm != NULL);

	if (frame_elm->bitmap_buffer == NULL)
	{
		OP_STATUS ret_val = CreateFirstBitmap(frame_elm);
		if (OpStatus::IsError(ret_val))
		{
			void *data = NULL;
			listener_hash.Remove((const void*) image_listener, &data);
			OP_DELETE(elm);
			return ret_val;
		}
		OP_ASSERT(frame_elm->bitmap_buffer != NULL);
	}

	return OpStatus::OK;
}

void AnimatedImageContent::DecVisible(ImageListener* image_listener)
{
	OP_NEW_DBG("AnimatedImageContent::DecVisible", "animation");
	OP_DBG(("this: %p, image_listener: %p", this, image_listener));
	AnimationListenerElm* elm = GetAnimationListenerElm(image_listener);

	if (!elm)
		return;

	elm->ref_count--;
	if (elm->ref_count > 0)
		return;

	if (elm->last_frame)
		elm->last_frame->DecRefCount(this);

	void *data = NULL;
	listener_hash.Remove((const void*) image_listener, &data);
	OP_ASSERT(data == elm);
	OP_DELETE(elm);

	if (listener_hash.GetCount() == 0)
	{
		ClearBuffers();
	}
}

void AnimatedImageContent::ClearBuffers()
{
	for (FrameElm* elm = (FrameElm*)frame_list.First(); elm != NULL; elm = (FrameElm*)elm->Suc())
	{
		elm->ClearBuffer();
	}
#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	((ImageManagerImp*)imgManager)->RemoveAnimatedImage(this);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
}

AnimationListenerElm* AnimatedImageContent::GetAnimationListenerElm(ImageListener* listener)
{
	void *data = NULL;
	if (OpStatus::IsSuccess(listener_hash.GetData((const void*) listener, &data)))
		return (AnimationListenerElm *) data;
	return NULL;
}

FrameElm* AnimatedImageContent::GetFrameElm(INT32 frame_nr)
{
	FrameElm* elm = (FrameElm*)frame_list.First();
	for (int i = 0; i < frame_nr; i++)
	{
		elm = (FrameElm*)elm->Suc();
	}
	return elm;
}

BOOL AnimatedImageContent::CanLoop(AnimationListenerElm* elm)
{
	return is_decoded && (nr_of_repeats == 0 || elm->loop_nr + 1 < nr_of_repeats);
}

OpRect AnimatedImageContent::GetCurrentFrameRect(ImageListener* image_listener)
{
	OP_NEW_DBG("AnimatedImageContent::GetCurrentFrameRect", "animation");
	OP_DBG(("this: %p, image_listener: %p", this, image_listener));
	OpRect rect;
	AnimationListenerElm* elm = GetAnimationListenerElm(image_listener);
	if (!elm)
	{
		rect.width = total_width;
		rect.height = total_height;
		return rect;
	}
	INT32 frame_nr = elm->frame_nr;
	FrameElm* frame_elm = GetFrameElm(frame_nr);
	OP_ASSERT(frame_elm != NULL);
	elm->last_frame_painted_nr = elm->frame_nr - 1; //image might have been animated (Animate called) but not repainted afterwards
													//so make sure last_frame_painted_nr is sane before using it
	if (frame_nr == 0 || elm->last_frame_painted_nr < 0)
	{
		elm->last_frame_painted_nr = -1;
		rect.width = total_width;
		rect.height = total_height;
	}
	else
	{
		OP_ASSERT(elm->last_frame_painted_nr >= 0);
		INT32 start_frame_nr = elm->last_frame_painted_nr;
		FrameElm* start_frame_elm = GetFrameElm(start_frame_nr);
		OP_ASSERT(start_frame_elm != NULL);
		while (start_frame_elm->disposal_method == DISPOSAL_METHOD_RESTORE_PREVIOUS)
		{
			start_frame_elm = (FrameElm*)start_frame_elm->Pred();
			OP_ASSERT(start_frame_elm != NULL);
		}
		OP_ASSERT(start_frame_elm != NULL);
		if (start_frame_elm->disposal_method != DISPOSAL_METHOD_DO_NOT_DISPOSE)
		{
			rect = start_frame_elm->rect;
		}
		FrameElm* curr_elm = (FrameElm*)start_frame_elm->Suc();
		while (curr_elm != frame_elm)
		{
			rect.UnionWith(curr_elm->rect);
			curr_elm = (FrameElm*)curr_elm->Suc();
		}
		rect.UnionWith(frame_elm->rect);
	}
	return rect;
}

INT32 AnimatedImageContent::GetCurrentFrameDuration(ImageListener* image_listener)
{
	OP_NEW_DBG("AnimatedImageContent::GetCurrentFrameDuration", "animation");
	OP_DBG(("this: %p, image_listener: %p", this, image_listener));
	AnimationListenerElm* elm = GetAnimationListenerElm(image_listener);
	if(!elm)
		return -1;

	FrameElm* frame_elm = GetFrameElm(elm->frame_nr);
	OP_ASSERT(frame_elm != NULL);

	if (frame_elm->Suc() == NULL && !CanLoop(elm)) // Last frame and no loop.
		return -1;

	return frame_elm->duration;
}

void AnimatedImageContent::SetDecoded()
{
	OP_ASSERT(!is_decoded);
	is_decoded = TRUE;
}
