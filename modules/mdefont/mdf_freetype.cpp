/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef MDEFONT_MODULE

#ifdef MDF_FREETYPE_SUPPORT

#include "modules/mdefont/mdf_freetype.h"

#include "modules/util/OpHashTable.h"

#ifdef FT_INTERNAL_FREETYPE
# include "modules/libfreetype/include/ft2build.h"
# include "modules/libfreetype/include/freetype/tttables.h"
# include "modules/libfreetype/include/freetype/ftoutln.h"
# include "modules/libfreetype/include/freetype/ttnameid.h"
# include "modules/libfreetype/include/freetype/ftsnames.h"
#else // FT_INTERNAL_FREETYPE
# include <ft2build.h>
# include <freetype/tttables.h>
# include <freetype/ftoutln.h>
# include <freetype/ttnameid.h>
# include <freetype/ftsnames.h>
#endif // FT_INTERNAL_FREETYPE

#include FT_FREETYPE_H

#ifdef MDF_FREETYPE_PCF_FONT_SUPPORT
#include "modules/mdefont/mdf_ft_pcf_ext.h"
#endif

#if defined(FT_USE_SMOOTH_LCD_RENDERING) || defined(FT_USE_SMOOTH_LCDV_RENDERING)
# ifdef FT_INTERNAL_FREETYPE
#  include "modules/libfreetype/include/freetype/ftlcdfil.h"
# else // FT_INTERNAL_FREETYPE
#  include <freetype/ftlcdfil.h>
# endif // FT_INTERNAL_FREETYPE
#endif // FT_USE_SMOOTH_LCD_RENDERING || FT_USE_SMOOTH_LCDV_RENDERING

#include "modules/display/sfnt_base.h"

#ifdef MDF_DISABLE_FONT_ANTIALIASING
# define LOAD_FLAG_MONOCHROME FT_LOAD_MONOCHROME
#else
# define LOAD_FLAG_MONOCHROME 0
#endif
#ifdef FT_USE_SMOOTH_LCD_RENDERING
# define LOAD_FLAG_TARGET FT_LOAD_TARGET_LCD
#elif defined FT_USE_SMOOTH_LCDV_RENDERING
# define LOAD_FLAG_TARGET FT_LOAD_TARGET_LCD_V
#else
# define LOAD_FLAG_TARGET 0
#endif

#define DEFAULT_LOAD_FLAGS FT_LOAD_DEFAULT | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH | LOAD_FLAG_MONOCHROME | LOAD_FLAG_TARGET

#define IS_SYNTHESIZED(file_name_info) ((file_name_info)->bit_field & (MDF_FTFontEngine::MDF_FontFileNameInfo::BOLDIFY | MDF_FTFontEngine::MDF_FontFileNameInfo::APPLY_ITALICS))

class MDF_FTFontFace : public MDF_FontFace
{
public:
	MDF_FTFontFace() : m_font_name(NULL), m_face(NULL), m_last_wanted_size(-1)
	{}
	virtual ~MDF_FTFontFace()
	{
		if (m_font_name)
			op_free(m_font_name);
		if (m_face)
			FT_Done_Face(m_face);
	}

	OP_STATUS Set(const uni_char* font_name, FT_Face face, int index)
	{
		uni_char* new_font_name = uni_strdup(font_name);
		if (!new_font_name)
			return OpStatus::ERR_NO_MEMORY;
		if (m_font_name)
			op_free(m_font_name);
		if (m_face)
			FT_Done_Face(m_face);
		m_font_name = new_font_name;
		m_index = index;
		m_face = face;
		m_last_wanted_size = -1;
		return OpStatus::OK;
	}

	virtual int IsEqual(const uni_char* font_name, int index) const
	{
		return uni_strcmp(font_name, m_font_name) == 0 && m_index==index;
	}
	FT_Face GetFace() { return m_face; }
	OP_STATUS SetPixelHeight(int wanted_size)
	{
		// avoid calling FT_Set_Pixel_Sizes unless necessary as it can be expensive (e.g. if the font has hinting info)
		if (wanted_size != m_last_wanted_size)
		{
			int error = FT_Set_Pixel_Sizes(m_face, 0, wanted_size);
			if (error)
			{
				m_last_wanted_size = -1; // in case the failed call has left the face in a strange state
				return error == FT_Err_Out_Of_Memory?OpStatus::ERR_NO_MEMORY:OpStatus::ERR;
			}
			else
				m_last_wanted_size = wanted_size;
		}
		return OpStatus::OK;
	}
	uni_char* m_font_name; // public for easy debugging
	int m_index;
private:
	FT_Face m_face;
	int m_last_wanted_size;
};

#ifndef MDF_CUSTOM_FONTENGINE_CREATE
OP_STATUS MDF_FontEngine::Create(MDF_FontEngine** engine)
{
	MDF_FTFontEngine* e = OP_NEW(MDF_FTFontEngine, ());
	if (!e)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = e->Construct();
	if (OpStatus::IsError(err))
	{
		OP_DELETE(e);
		e = NULL;
	}
	*engine = e;
	return err;
}
#endif // !MDF_CUSTOM_FONTENGINE_CREATE

MDF_FTFontEngine::MDF_FTFontEngine()
	:
	face_cache(NULL)
	, ft_library(NULL)
#ifdef MDF_FONT_SMOOTHING
# if defined(FT_USE_SMOOTH_LCD_RENDERING)
	, smoothing(RGB)
# elif defined(FT_USE_SMOOTH_LCDV_RENDERING)
	, smoothing(VRGB)
# else
	, smoothing(NONE)
# endif
#endif // MDF_FONT_SMOOTHING
{}

static BOOL IsNonscalableFont(FT_Face face)
{
	return (face->face_flags & FT_FACE_FLAG_SCALABLE) == 0;
}

static int GetClosestSizeFromNonscalableFont(const FT_Face face, int wanted_size)
{
	// the font hasn't got a size (wtf?),so we might as well
	// fail fetching the one we want
	if (face->num_fixed_sizes <= 0)
		return wanted_size;
	// try to find the size closest to wanted
	FT_Bitmap_Size* bz = face->available_sizes;
	int i, new_size = face->available_sizes->y_ppem>>6;
	for (i=1; i < face->num_fixed_sizes; ++i)
		if (bz[i].y_ppem>>6 == wanted_size)
			return wanted_size;
		else if (op_abs((bz[i].y_ppem>>6)-wanted_size) < op_abs(new_size-wanted_size))
			new_size = bz[i].y_ppem>>6;
	return new_size;
}
MDF_FTFontEngine::MDF_FontFileNameInfo* MDF_FTFontEngine::GetBestNonscalableFontFileNameInfo(MDF_FontInformation* font_information, int wanted_size, BOOL bold, BOOL italic, int& actual_size)
{
	// accept different font size, but not different style

	int closest_size = INT_MIN;
	MDF_FontFileNameInfo* closest_file_info = 0;
	UINT8 mask = (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::IS_ITALIC);
	UINT8 wanted = 0;
	if (bold)
		wanted |= MDF_FontFileNameInfo::IS_BOLD;
	if (italic)
		wanted |= MDF_FontFileNameInfo::IS_ITALIC;
	for (MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
		file_name_info; file_name_info = file_name_info->next)
	{
		if (!file_name_info->num_fixed_sizes ||
			((file_name_info->bit_field & mask) != wanted))
			continue;

		for (int i = 0; i < file_name_info->num_fixed_sizes; ++i)
			// exact match
			if (file_name_info->fixed_sizes[i] == wanted_size)
			{
				actual_size = wanted_size;
				return file_name_info;
			}
			else if (op_abs(wanted_size-file_name_info->fixed_sizes[i]) < op_abs(wanted_size-closest_size))
			{
				closest_file_info = file_name_info;
				closest_size = file_name_info->fixed_sizes[i];
			}
	}
	if (!closest_file_info)
		return 0;

	actual_size = closest_size;
	return closest_file_info;
}

#ifdef MDE_MMAP_MANAGER

#include "modules/libgogi/mde_mmap.h"

static void MDF_Cleanup_FT_Face( void *_x )
{
    FT_Face x = (FT_Face)_x;
	if (g_mde_mmap_manager)
		g_mde_mmap_manager->UnmapFile((MDE_MMAP_File*)x->generic.data);
}
#endif // MDE_MMAP_MANAGER

OP_STATUS MDF_FTFontEngine::LoadFontFace(MDF_FontFace** face, const uni_char* font_name, int index)
{
	if (!*face)
	{
		*face = OP_NEW(MDF_FTFontFace, ());
		if (!*face)
			return OpStatus::ERR_NO_MEMORY;
	}
	FT_Face ftface;

	int error;
#ifdef MDE_MMAP_MANAGER
    MDE_MMAP_File *x;

	if (!g_mde_mmap_manager)
	{
		g_mde_mmap_manager = OP_NEW(MDE_MMapManager, ());
		if (!g_mde_mmap_manager)
			return OpStatus::ERR_NO_MEMORY;
	}
	OP_STATUS mmap_status = g_mde_mmap_manager->MapFile(&x, font_name);
	if (mmap_status != OpStatus::ERR_FILE_NOT_FOUND)
	{
		RETURN_IF_ERROR(mmap_status);
		if( (error=FT_New_Memory_Face( (FT_Library)ft_library, (const FT_Byte *)x->filemap->GetAddress(), x->filemap->GetSize(), index, &ftface )) )
		{
			g_mde_mmap_manager->UnmapFile(x);
		}
		else
		{
			ftface->generic.data = x;
			ftface->generic.finalizer = MDF_Cleanup_FT_Face;
		}
	}
	else
#endif // !MDE_MMAP_MANAGER
	{
		OpString8 font_name8;
		RETURN_IF_ERROR(font_name8.SetUTF8FromUTF16(font_name));
		error = FT_New_Face((FT_Library)ft_library, font_name8, index, &ftface);
	}


#ifdef MDF_FREETYPE_PCF_FONT_SUPPORT
	if (!error && IsPcfFont(ftface))
	{
		GetPcfMetrics(ftface);
	}
#endif //MDF_FREETYPE_PCF_FONT_SUPPORT

	if (error)
	{
		return error == FT_Err_Out_Of_Memory?OpStatus::ERR_NO_MEMORY:OpStatus::ERR;
	}
	OP_STATUS status = ((MDF_FTFontFace*)(*face))->Set(font_name, ftface, index);
	if (OpStatus::IsError(status))
		FT_Done_Face(ftface);
	return status;
}

OP_STATUS MDF_FTFontEngine::GetFontFace(FT_Face& face, const uni_char* font_name, int index)
{
	MDF_FontFace* mdf_face;
	RETURN_IF_ERROR(face_cache->GetFontFace(&mdf_face, font_name, index));
	face = ((MDF_FTFontFace*)mdf_face)->GetFace();
	return OpStatus::OK;
}
OP_STATUS MDF_FTFontEngine::GetFontFace(FT_Face& face, const MDF_FontFileNameInfo* info, int wanted_size)
{
	MDF_FontFace* mdf_face;
	RETURN_IF_ERROR(face_cache->GetFontFace(&mdf_face, info->file_name, info->font_index));
	face = ((MDF_FTFontFace*)mdf_face)->GetFace();
	if (IsNonscalableFont(face) || (info->bit_field & MDF_FontFileNameInfo::ONLY_BITMAP))
		wanted_size = GetClosestSizeFromNonscalableFont(face, wanted_size);
	return ((MDF_FTFontFace*)mdf_face)->SetPixelHeight(wanted_size);
}

OP_STATUS MDF_FTFontEngine::BlitGlyph(const FT_Bitmap & source_bitmap, const MDF_GLYPH_BUFFER* dest_buffer, UINT32 dest_stride, BOOL scalable)
{
	/* IMPORTANT: If smoothing is not NONE, 'dest_buffer' has 4
	 * channels per pixel, enabling us to do subpixel rendering.  I
	 * believe the format is UINT32-based ARGB8 (that matches what I
	 * see on x86_64, at least).
	 *
	 * If smoothing is NONE, dest_buffer is a 1 byte per pixel
	 * graymap/alphamap.
	 *
	 * It is unknown which combinations of smoothing and
	 * source_bitmap.pixel_mode can occur.  I expect that
	 * FT_PIXEL_MODE_LCD and FT_PIXEL_MODE_LCD_V will never happen
	 * when smoothing is NONE.  However, FT_PIXEL_MODE_MONO has been
	 * spotted with smoothing != NONE (see e.g. DSK-347303).
	 */

#ifdef MDF_FONT_SMOOTHING
	OP_ASSERT(smoothing == RGB || smoothing == BGR ||
			  smoothing  == VRGB || smoothing == VBGR ||
			  smoothing == NONE);
	if (smoothing == RGB || smoothing == BGR)
		OP_ASSERT(source_bitmap.pixel_mode == FT_PIXEL_MODE_LCD ||
				  source_bitmap.pixel_mode == FT_PIXEL_MODE_GRAY ||
				  source_bitmap.pixel_mode == FT_PIXEL_MODE_MONO);
	else if (smoothing == VRGB || smoothing == VBGR)
		OP_ASSERT(source_bitmap.pixel_mode == FT_PIXEL_MODE_LCD_V ||
				  source_bitmap.pixel_mode == FT_PIXEL_MODE_GRAY ||
				  source_bitmap.pixel_mode == FT_PIXEL_MODE_MONO);
	else
#endif // MDF_FONT_SMOOTHING
		OP_ASSERT(source_bitmap.pixel_mode == FT_PIXEL_MODE_GRAY ||
				  source_bitmap.pixel_mode == FT_PIXEL_MODE_MONO);
	if (source_bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
		OP_ASSERT(source_bitmap.num_grays == 256);
	if (
#ifdef MDF_FONT_SMOOTHING
		source_bitmap.pixel_mode != FT_PIXEL_MODE_LCD &&
		source_bitmap.pixel_mode != FT_PIXEL_MODE_LCD_V &&
#endif // MDF_FONT_SMOOTHING
		source_bitmap.pixel_mode != FT_PIXEL_MODE_GRAY &&
		source_bitmap.pixel_mode != FT_PIXEL_MODE_MONO)
	{
		OP_ASSERT(!"Unknown input format.  Giving up to avoid potential buffer overruns.");
		return OpStatus::ERR;
	}

	if (source_bitmap.pitch < 0)
	{
		OP_ASSERT(!"Bottom-to-top glyph images are not implemented");
		return OpStatus::ERR;
	}

#ifdef MDF_FONT_SMOOTHING
	if (smoothing == NONE && (source_bitmap.pixel_mode == FT_PIXEL_MODE_MONO ||
							  source_bitmap.pixel_mode == FT_PIXEL_MODE_GRAY))
	{
		return BlitGlyphNoSubpixels(source_bitmap, dest_buffer, dest_stride, scalable);
	}
	if (smoothing == NONE)
	{
		OP_ASSERT(!"Unexpected format for non-subpixel rendering.");
		return OpStatus::ERR;
	}

	/* That dealt with all the cases where we render into a
	 * non-subpixel buffer.  So from here on, we're sure dest_buffer
	 * is 4 bytes per pixel.
	 */
	OP_ASSERT(smoothing != NONE);

	if (source_bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		unsigned char * dest_data = static_cast<unsigned char*>(dest_buffer->data);
		unsigned char * source_data = static_cast<unsigned char*>(source_bitmap.buffer);
		int source_stride = source_bitmap.pitch;
		for (unsigned int y = 0; y < dest_buffer->h; y++)
			for (unsigned int x = 0; x < dest_buffer->w; x++)
			{
				unsigned char v = source_data[y * source_stride + (x/8)];
				v = ((v >> (7-x%8)) & 1) * 255;
				unsigned int base = y * dest_stride + x * 4;
				dest_data[base] = v;
				dest_data[base+1] = v;
				dest_data[base+2] = v;
				dest_data[base+3] = v;
			}
		return OpStatus::OK;
	}
	else if (source_bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
	{
		unsigned char * dest_data = static_cast<unsigned char*>(dest_buffer->data);
		unsigned char * source_data = static_cast<unsigned char*>(source_bitmap.buffer);
		int source_stride = source_bitmap.pitch;
		for (unsigned int y = 0; y < dest_buffer->h; y++)
			for (unsigned int x = 0; x < dest_buffer->w; x++)
			{
				unsigned char v = source_data[y * source_stride + x];
				unsigned int base = y * dest_stride + x * 4;
				dest_data[base] = v;
				dest_data[base+1] = v;
				dest_data[base+2] = v;
				dest_data[base+3] = v;
			}
		return OpStatus::OK;
	}

	// Only one case left: subpixel source and subpixel target

	UINT32 new_w, new_h;
	UINT32 overflow_w = 0;
	UINT32 overflow_h = 0;

	UINT32 pixel_stride;
	UINT32 extra_row_stride;
	UINT32 component_stride;

	if ((smoothing == RGB || smoothing == BGR) && source_bitmap.pixel_mode == FT_PIXEL_MODE_LCD)
	{
		overflow_w = source_bitmap.width % 3;
		new_w = (source_bitmap.width + 2) / 3;
		new_h = source_bitmap.rows;
		pixel_stride = 3;

		if (new_w > dest_buffer->w)
		{
			overflow_w = 0;
			new_w = dest_buffer->w;
		}
		if (new_h > dest_buffer->h)
			new_h = dest_buffer->h;

		extra_row_stride = source_bitmap.pitch - pixel_stride * new_w;
		component_stride = 1;
	}
	else if ((smoothing == VRGB || smoothing == VBGR) && source_bitmap.pixel_mode == FT_PIXEL_MODE_LCD_V)
	{
		overflow_h = source_bitmap.rows % 3;
		new_w = source_bitmap.width;
		new_h = (source_bitmap.rows + 2) / 3;
		pixel_stride = 1;

		if (new_w > dest_buffer->w)
			new_w = dest_buffer->w;
		if (new_h > dest_buffer->h)
		{
			overflow_h = 0;
			new_h = dest_buffer->h;
		}

		// three lines of sub-pixels means 3*pitch to reach next pixel line
		extra_row_stride = source_bitmap.pitch * 3 - pixel_stride * new_w;
		// sub-pixels are vertical, pitch is distance between lines of sub-pixels
		component_stride = source_bitmap.pitch;
	}
	else
	{
		OP_ASSERT(!"Unexpected combination of values.");
		return OpStatus::ERR;
	}

	UINT32 extra_dest_stride = dest_stride / sizeof(UINT32) - new_w;

	OP_ASSERT(dest_stride % sizeof(UINT32) == 0);
	OP_ASSERT(new_w <= dest_buffer->w);
	OP_ASSERT(new_h <= dest_buffer->h);
	OP_ASSERT(dest_buffer->w * sizeof(UINT32) <= dest_stride);

	const unsigned char* source = static_cast<unsigned char*>(source_bitmap.buffer);
	UINT32* dest = static_cast<UINT32*>(dest_buffer->data);
	unsigned char osrc[3]; // ARRAY OK 2010-11-10 wonko
	osrc[2] = 0;
	unsigned int loop_height = overflow_h ? new_h - 1 : new_h;
	unsigned int loop_width = overflow_w ? new_w - 1 : new_w;
	for (UINT32 y = 0; y < loop_height; ++y)
	{
		for (UINT32 x = 0; x < loop_width; ++x)
		{
			LCDToRGB(source, dest, component_stride);
			source += pixel_stride;
			++dest;
		}
		if (overflow_w)
		{
			OP_ASSERT(pixel_stride == 3);
			osrc[0] = source[0];
			osrc[1] = (overflow_w>1)?source[component_stride]:0;
			LCDToRGB(osrc, dest, 1);
			source += overflow_w;
			++dest;
		}
		source += extra_row_stride;
		dest += extra_dest_stride;
	}
	if (overflow_h)
	{
		OP_ASSERT(overflow_w == 0);
		OP_ASSERT(pixel_stride == 1);
		unsigned char osrc[3]; // ARRAY OK 2010-11-10 wonko
		osrc[2] = 0;
		for (UINT32 x = 0; x < new_w; ++x)
		{
			osrc[0] = source[0];
			osrc[1] = (overflow_h>1)?source[component_stride]:0;
			LCDToRGB(osrc, dest, 1);
			++source;
			++dest;
		}
	}
	return OpStatus::OK;
}

void MDF_FTFontEngine::LCDToRGB(const unsigned char* source, UINT32* dest, UINT32 component_stride)
{
	// left, middle and right sub-sample
	unsigned char first = source[0];
	unsigned char second = source[component_stride];
	unsigned char third = source[component_stride<<1];

	// output order depends of order of elements in display - output format is always average + BGR24
	if ( smoothing == RGB || smoothing == VRGB )
	{
		UINT32 avg = (first+second+third)/3;
		*dest = (avg<<24)|(first<<16)|(second<<8)|third;
	}
	else
	{
		UINT32 avg = (first+second+third)/3;
		*dest = (avg<<24)|(third<<16)|(second<<8)|first;
	}
}

OP_STATUS MDF_FTFontEngine::BlitGlyphNoSubpixels(const FT_Bitmap & source_bitmap, const MDF_GLYPH_BUFFER* dest_buffer, UINT32 dest_stride, BOOL scalable)
{
	if (scalable && smoothing != NONE)
	{
		/* In this case, we are doing subpixel rendering, and
		 * 'dest_buffer' is 4 channels.  But this method should only
		 * be called for non-subpixel rendering.  So this method
		 * assumes it is writing to a 1-channel grayscale
		 * 'dest_buffer'.
		 */
		OP_ASSERT(!"Mismatch in font setup.  Output format is multi-channel.");
		return OpStatus::ERR;
	}
#endif // MDF_FONT_SMOOTHING

	int height = dest_buffer->h;
	if (height > source_bitmap.rows)
		height = source_bitmap.rows;
	int width = dest_buffer->w;
	if (width > source_bitmap.width)
		width = source_bitmap.width;
	unsigned char * dest_data = static_cast<unsigned char*>(dest_buffer->data);
	unsigned char * source_data = static_cast<unsigned char*>(source_bitmap.buffer);
	int source_stride = source_bitmap.pitch;
	if (source_bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
	{
		// OPTIMIZE: Though it might not be worth it...
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				dest_data[dest_stride * y + x] = source_data[source_stride * y + x];
	}
	else if (source_bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		// OPTIMIZE: Though it might not be worth it...
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				unsigned char pixel = source_data[source_stride * y + x/8];
				if ((pixel << (x % 8)) & 0x80)
					dest_data[dest_stride * y + x] = 0xff;
				else
					dest_data[dest_stride * y + x] = 0;
			}
		}
	}
	else
	{
		OP_ASSERT(!"Unknown bitmap pixel mode");
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

#ifdef MDF_FONT_GLYPH_CACHE
OP_STATUS MDF_FTFontEngine::LoadFontGlyph(MDF_FontGlyph* cglyph, MDE_FONT* font, BOOL mustRender)
{
	BOOL isIndex = cglyph->IsIndex();
	MDF_GLYPH glyph;
	RETURN_IF_ERROR(LoadFontGlyph(glyph, cglyph->c, font, mustRender, isIndex));
	cglyph->buffer_data = glyph.buffer_data;
	cglyph->bitmap_left = glyph.bitmap_left;
	cglyph->bitmap_top = glyph.bitmap_top;
	cglyph->advance = glyph.advance;
	if (mustRender)
		cglyph->flags |= MDF_FontGlyph::GlyphRendered;
	return OpStatus::OK;
}
void MDF_FTFontEngine::FreeFontGlyph(MDF_FontGlyph* glyph)
{
	op_free(glyph->buffer_data);
	glyph->buffer_data = NULL;
}
OP_STATUS MDF_FTFontEngine::ResizeGlyphCache(MDE_FONT* font, int new_size)
{
	MDF_FT_FONT* ft_font = (MDF_FT_FONT*)font->private_data;
	MDF_FontGlyphCache* new_glyph_cache;
	// Create new cache of right size.
	OP_STATUS status = MDF_FontGlyphCache::Create(&new_glyph_cache, new_size, this);
	if ( OpStatus::IsError(status) )
	{
		return status;
	}
	// Copy slots from old to new cache.
	MDF_FontGlyphCache::MoveCache(new_glyph_cache, ft_font->glyph_cache);
	OP_DELETE(ft_font->glyph_cache);
	ft_font->glyph_cache = new_glyph_cache;
	return OpStatus::OK;
}
unsigned short MDF_FTFontEngine::GetGlyphCacheSize(MDE_FONT* font) const
{
	return ((MDF_FT_FONT*)font->private_data)->glyph_cache->GetSize();
}
BOOL MDF_FTFontEngine::GlyphCacheFull(MDE_FONT* font) const
{
	return ((MDF_FT_FONT*)font->private_data)->glyph_cache->IsFull();
}
unsigned short MDF_FTFontEngine::GetGlyphCacheFill(const MDE_FONT* font) const
{
	return ((MDF_FT_FONT*)font->private_data)->glyph_cache->GetFill();
}
#endif // MDF_FONT_GLYPH_CACHE

// skew the outline of the loaded glyph
static inline
void ApplyItalics(FT_Face& face)
{
	FT_Matrix transform;
	transform.xx = 1 << 16;
	transform.xy = MDF_FT_SLANT_FACTOR;
	transform.yx = 0;
	transform.yy = 1 << 16;
	FT_Set_Transform( face, &transform, 0 );
}

// apply boldification to the outline of the loaded glyph
static
OP_STATUS ApplyBoldification(FT_Face& face, int size)
{
#if FREETYPE_MAJOR > 2 || FREETYPE_MINOR > 1
	// apply boldification to outline
	// FIXME: maybe amount should be tweakable somehow
	const FT_Pos amount = (size << 6) / 21; // 26.6 fixpoint representation
	int error = FT_Outline_Embolden(&face->glyph->outline, amount);
	if ( error == FT_Err_Out_Of_Memory )
		return OpStatus::ERR_NO_MEMORY;
	else if (error)
		return OpStatus::ERR;
#endif // FREETYPE_MAJOR > 2 || FREETYPE_MINOR > 1
	return OpStatus::OK;
}


OP_STATUS MDF_FTFontEngine::LoadFontGlyph(MDF_GLYPH& glyph, const UnicodePoint c, MDE_FONT* font, BOOL& render, BOOL isIndex, const UINT32 stride)
{
	if (stride && !(glyph.buffer_data && glyph.buffer_data->data))
		return OpStatus::ERR;

    // No cache
	op_yield();
	FT_Face face;
	MDF_FT_FONT* font_data = (MDF_FT_FONT*)font->private_data;
	RETURN_IF_ERROR(GetFontFace(face, font_data->file_name_info, font_data->font_size));

	int error = 0;

#ifdef FT_INTERNAL_FREETYPE
#ifndef TT_CONFIG_OPTION_BYTECODE_INTERPRETER
	BOOL isAhem = (face->family_name && !op_strcmp(face->family_name, "Ahem"));
#endif // !TT_CONFIG_OPTION_BYTECODE_INTERPRETER
#endif // FT_INTERNAL_FREETYPE

	// apply italics
	if (font_data->file_name_info->bit_field & MDF_FontFileNameInfo::APPLY_ITALICS)
	{
		ApplyItalics(face);
	}
	else
	{
		FT_Set_Transform( face, 0, 0 );
	}

	BOOL boldify = (font_data->file_name_info->bit_field & MDF_FontFileNameInfo::BOLDIFY) != 0;
	FT_Int32 loadflags = font_data->load_flags;
	if (boldify && !IsNonscalableFont(face))
		loadflags |= FT_LOAD_NO_BITMAP;

#ifdef MDF_FT_USE_LIGHT_AUTOHINTER
    loadflags |= FT_LOAD_TARGET_LIGHT;
#endif // MDF_FT_USE_LIGHT_AUTOHINTER

#ifdef FT_INTERNAL_FREETYPE
#ifndef TT_CONFIG_OPTION_BYTECODE_INTERPRETER
	// if the bytecode interpreter is disabled, the hinting changes. this causes problems
	// with tests using the ahem font. in order to render glyphs from the ahem font
	// correclty, autohinting is disabled and glyphs rendered in monochrome.
	if (isAhem)
	{
		loadflags &= ~(FT_LOAD_TARGET_LCD | FT_LOAD_TARGET_LCD_V | FT_LOAD_TARGET_LIGHT | FT_LOAD_TARGET_NORMAL);
		loadflags |= FT_LOAD_TARGET_MONO;
	}
#endif // TT_CONFIG_OPTION_BYTECODE_INTERPRETER
#endif // FT_INTERNAL_FREETYPE

	const FT_UInt gid = isIndex ? c : FT_Get_Char_Index(face,c );
	// load glyph without rendering
	error = FT_Load_Glyph(face, gid, loadflags);
	glyph.advance = error?0:(short)(face->glyph->advance.x>>6);
	if (!stride)
		glyph.buffer_data = NULL;

	BOOL isOutline = face->glyph->format == FT_GLYPH_FORMAT_OUTLINE;
	// must render at once when boldifying, or advance adjustment cannot be done
	if (!render && (boldify && isOutline))
	{
		render = TRUE;
	}

	OP_STATUS status =
		(error == 0 ?
		 OpStatus::OK :
		 (error == FT_Err_Out_Of_Memory ?
		  OpStatus::ERR_NO_MEMORY :
		  OpStatus::ERR));

#ifdef MDF_FREETYPE_ALWAYS_RENDER
	RETURN_IF_ERROR( status );
	render = TRUE;
#else
	if (!render || OpStatus::IsError(status))
	{
#ifdef MDF_FONT_ADVANCE_CACHE
		if (OpStatus::IsSuccess(status) && !isIndex)
		{
			OP_ASSERT(font->m_advance_cache);
			font->m_advance_cache->Set(c, glyph.advance);
		}
#endif // MDF_FONT_ADVANCE_CACHE
		return status;
	}
#endif

	// render the glyph
	FT_Render_Mode mode;
#ifdef MDF_FONT_SMOOTHING
	switch (smoothing)
	{
		case RGB: /* fallthrough */
		case BGR:
			mode = FT_RENDER_MODE_LCD;
			break;
		case VRGB: /* fallthrough */
		case VBGR:
			mode = FT_RENDER_MODE_LCD_V;
			break;
		case NONE: /* fallthrough */
		default:
			mode = FT_LOAD_TARGET_MODE( loadflags );
			break;
	}
#else
	mode = FT_LOAD_TARGET_MODE( loadflags );
#endif
	error = FT_Render_Glyph( face->glyph, mode );


	INT32 extra_advance = 0;
	// boldify
	if (!error && boldify && isOutline)
	{
		// get width of unboldened glyph
		const UINT32 unboldened_width = face->glyph->bitmap.width;
		const int size = font->ascent + font->descent - font->internal_leading;

		// re-load glyph without rendering
		error = FT_Load_Glyph(face, gid, loadflags);
		if (error == FT_Err_Out_Of_Memory)
			return OpStatus::ERR_NO_MEMORY;
		OP_ASSERT(!error);
		OP_ASSERT(face->glyph->format == FT_GLYPH_FORMAT_OUTLINE);

		RETURN_IF_ERROR(ApplyBoldification(face, size));

		// re-rasterize
		error = FT_Render_Glyph( face->glyph, mode );
		// add difference in advance to extra_advance
		if (!error)
		{
			extra_advance = face->glyph->bitmap.width - unboldened_width;
#ifdef MDF_FONT_SMOOTHING
			if (smoothing == RGB || smoothing == BGR)
				extra_advance /= 3;
#endif // MDF_FONT_SMOOTHING
		}
	}

	if (error || font->ascent - face->glyph->bitmap_top + face->glyph->bitmap.rows < 0 ||
	    face->glyph->bitmap.width <= 0 || face->glyph->bitmap.rows <= 0)
	{
		if (!stride)
		{
			glyph.buffer_data = static_cast<MDF_GLYPH_BUFFER*>(op_malloc(sizeof(MDF_GLYPH_BUFFER)));
			RETURN_OOM_IF_NULL(glyph.buffer_data);
		}

		if (!error)
		{
			// Fix for Bug 235448. S_Ygo400.ttf seem to contain unsupported or corrupt data.
			glyph.bitmap_left = 0;
			glyph.bitmap_top = 0;
			glyph.advance = (short)(face->glyph->advance.x >> 6);
		}
		glyph.buffer_data->w = 0;
		glyph.buffer_data->h = 0;
		glyph.buffer_data->data = 0;
	}
	else
	{
		glyph.bitmap_left = face->glyph->bitmap_left;
		glyph.bitmap_top = face->glyph->bitmap_top;
		glyph.advance = (short)(face->glyph->advance.x >> 6);

		unsigned int buf_stride; // stride used in call to BlitGlyph
		unsigned int width = face->glyph->bitmap.width;
		unsigned int height = face->glyph->bitmap.rows;
		OP_ASSERT(face->glyph->bitmap.width >= 0);
#ifdef MDF_FONT_SMOOTHING
		if (smoothing != NONE && isOutline)
		{
			/* Input data is 3 sub-pixels per pixel.  Output data
			 * is 4 colour channels per pixel.  Adjust parameters
			 * accordingly.
			 */
			if (smoothing == RGB || smoothing == BGR)
			{
				// Each line has 3 sub-pixels
				OP_ASSERT(width % 3 == 0);
				width = width / 3;
			}
			else if (smoothing == VRGB || smoothing == VBGR)
			{
				// 3 rows of subpixels -> one row of pixels
				OP_ASSERT(height % 3 == 0);
				height = height / 3;
			}
			else
			{
				OP_ASSERT(!"Unexpected smoothing value.  Expect crashes.");
			}
		}
#endif // MDF_FONT_SMOOTHING
		if (!stride)
		{
			buf_stride = width;
#ifdef MDF_FONT_SMOOTHING
			if (smoothing != NONE) // i.e. sub-pixel rendering
				buf_stride *= 4;
#endif // MDF_FONT_SMOOTHING
			int sz = sizeof(MDF_GLYPH_BUFFER) + buf_stride * height;
			glyph.buffer_data = (MDF_GLYPH_BUFFER*)op_malloc(sz);
			RETURN_OOM_IF_NULL(glyph.buffer_data);
			glyph.buffer_data->data = ((char *)glyph.buffer_data) + sizeof(MDF_GLYPH_BUFFER);
			glyph.buffer_data->w = width;
			glyph.buffer_data->h = height;
		}
		else
		{
			buf_stride = stride;
			// dimensions of pre-allocated buffers may only be shrunk, never grown
			if (width < glyph.buffer_data->w)
				glyph.buffer_data->w = width;
			if (height < glyph.buffer_data->h)
				glyph.buffer_data->h = height;
		}

		RETURN_IF_ERROR(BlitGlyph(face->glyph->bitmap, glyph.buffer_data, buf_stride, isOutline));

		// stupid, but necessary - check if boldification caused empty last column and adjust if necessary
		if (boldify && glyph.buffer_data->w > 0 && glyph.buffer_data->h > 0)
		{
			unsigned vertical = 1;
			unsigned horizontal = 1;

#ifdef MDF_FONT_SMOOTHING
			if (smoothing == RGB || smoothing == BGR)
				horizontal = 4;
			else if (smoothing == VRGB || smoothing == VBGR)
				vertical = 4;
#endif // MDF_FONT_SMOOTHING

			unsigned tstride = stride ? stride : horizontal * glyph.buffer_data->w;
			unsigned height = glyph.buffer_data->h * vertical;
			unsigned char* p = (unsigned char*)glyph.buffer_data->data + (glyph.buffer_data->w - 1) * horizontal;
			unsigned i;

			for (i = 0; i < height; ++i)
			{
				for (unsigned j = 0; j < horizontal; ++j)
				{
					if (p[j])
					{
						p = 0;
						break;
					}
				}
				if (!p)
					break;

				p += tstride;
			}

			if (i == height)
				--extra_advance;
		}
	}

	glyph.advance += extra_advance;

#ifdef MDF_FONT_ADVANCE_CACHE
	if (!isIndex)
	{
		OP_ASSERT(font->m_advance_cache);
		font->m_advance_cache->Set(c, glyph.advance);
	}
#endif // MDF_FONT_ADVANCE_CACHE

	return OpStatus::OK;
}

/**
   Compare if str1 and str2 are equal.
   str2 has the length len, and is in upper case.
   str1 might be upper case, but must not be upper case.
*/
static BOOL equal_strings_upr_case(const char* str1, const char* str2, int len)
{
	for (int i = 0; i < len; i++)
    {
		int c1 = str1[i];
		if (c1 == 0)
		{
			return false;
		}
		if (c1 > 'Z') // FIXME:maybe reversed.
		{
			if (c1 - ('a' - 'A') != str2[i])
			{
				return false;
			}
		}
		else
		{
			if (c1 != str2[i])
			{
				return false;
			}
		}
    }
	return true;
}

/**
   Try to find the string find_str inside the string str.
   The length of find_str is find_len.
   find_str is all upper case, str can be any case, and
   lower case will match upper case.
*/
static BOOL find_string_upr_case(const char* str, const char* find_str, int find_len)
{
	char* curr_str = (char*)str;
	while (*curr_str != 0)
    {
		if (equal_strings_upr_case(curr_str, find_str, find_len))
		{
			return true;
		}
		curr_str++;
    }
	return false;
}

/**
   Get information from the style about if this
   style is bold, italic, both or normal.
*/
void MDF_FTFontEngine::GetBoldItalic(const char* style, UINT8& bit_field)
{
	if (find_string_upr_case(style, "BOLD", 4))
    {
		bit_field |= MDF_FontFileNameInfo::IS_BOLD;
    }
	else
    {
		bit_field &= ~MDF_FontFileNameInfo::IS_BOLD;
    }

	if (find_string_upr_case(style, "ITALIC", 6) || find_string_upr_case(style, "OBLIQUE", 7))
    {
		bit_field |= MDF_FontFileNameInfo::IS_ITALIC;
    }
	else
    {
		bit_field &= ~MDF_FontFileNameInfo::IS_ITALIC;
    }
}

/**
   Find the font information belonging to a specific font name
   font_name.  return OpStatus::OK if found, OpStatus::ERR if that
   information is not yet in the gogi font system and
   OpStatus::ERR_NO_MEMORY on OOM.
*/
OP_STATUS MDF_FTFontEngine::FindFontInformation(const char* name, MDF_FontInformation*& fi, int* font_array_index /*= NULL */)
{
	OpString16 s;
	RETURN_IF_ERROR(s.SetFromUTF8(name));
	const uni_char* font_name = s.CStr();

	OpHashIterator* iter = font_array.GetIterator();
	if(!iter)
		return OpStatus::ERR_NO_MEMORY;

	for (OP_STATUS status = iter->First();
		 OpStatus::IsSuccess(status); status = iter->Next())
	{
		MDF_FontInformation* font_information = (MDF_FontInformation*)iter->GetData();
		if (uni_strcmp(font_information->font_name, font_name) == 0)
		{
			fi = font_information;

			if(font_array_index)
				*font_array_index = (int)(INTPTR)iter->GetKey();

			OP_DELETE(iter);
			return OpStatus::OK;
		}
	}

	OP_DELETE(iter);
	return OpStatus::ERR;
}

/**
   Create MDF_FontFileNameInfo for a specific face, with
   a specified file_name.
*/
MDF_FTFontEngine::MDF_FontFileNameInfo* MDF_FTFontEngine::CreateFontFileNameInfo(FT_Face face, const uni_char* file_name, int font_index)
{
	MDF_FontFileNameInfo* file_name_info = OP_NEW(MDF_FontFileNameInfo, ());
	if (file_name_info == NULL)
    {
		return NULL;
    }

	file_name_info->bit_field = 0;
	file_name_info->next = NULL;
	file_name_info->file_name = uni_strdup(file_name);
	if (file_name_info->file_name == NULL)
    {
		OP_DELETE(file_name_info);
		return NULL;
    }

	if (face->style_name && face->style_name[0] != 0)
    {
		GetBoldItalic(face->style_name, file_name_info->bit_field);
    }
	if (IsNonscalableFont(face) && face->num_fixed_sizes)
	{
		file_name_info->num_fixed_sizes = face->num_fixed_sizes;
		file_name_info->fixed_sizes = OP_NEWA(int, file_name_info->num_fixed_sizes);
		if (!file_name_info->fixed_sizes)
		{
			op_free(file_name_info->file_name);
			OP_DELETE(file_name_info);
			return NULL;
		}
		int i;
		FT_Bitmap_Size** bz = &face->available_sizes;
		for (i = 0; i < file_name_info->num_fixed_sizes; ++i)
			file_name_info->fixed_sizes[i] = bz[i]->height;
	}
	else
	{
		file_name_info->num_fixed_sizes = 0;
		file_name_info->fixed_sizes = 0;
	}

	file_name_info->font_index = font_index;
	return file_name_info;
}

/**
   Frees a MDF_FontFileNameInfo object.
*/
void MDF_FTFontEngine::FreeFontFileNameInfo(MDF_FontFileNameInfo* file_name_info)
{
	op_free(file_name_info->file_name);
	OP_DELETEA(file_name_info->fixed_sizes);
	OP_DELETE(file_name_info);
}

/**
   Check if the MDF_FontInformation already has the vold and italic
   combination that we now want to add (so that we will not add two
   font styles with the same characteristics).
*/
BOOL MDF_FTFontEngine::FontInformationHasBoldItalic(MDF_FontInformation* font_information, MDF_FontFileNameInfo* file_name_info)
{
	UINT8 mask = (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::IS_ITALIC);
	UINT8 wanted_field = (file_name_info->bit_field & mask);
	MDF_FontFileNameInfo* file_info = font_information->file_name_list;
	while (file_info != NULL)
    {
		if ((file_info->bit_field & mask) == wanted_field
			// check if the same sizes exist
			&& file_info->num_fixed_sizes == file_name_info->num_fixed_sizes
			&& (file_info->num_fixed_sizes == 0 ||
				!op_strncmp((char*)file_info->fixed_sizes, (char*)file_name_info->fixed_sizes, sizeof(int)*(size_t)file_info->fixed_sizes)))
		{
			return true;
		}
		file_info = file_info->next;
    }
	return false;
}

/**
   Creates font information.
   A lot of FIXME:s.
*/

void MDF_FTFontEngine::GetFontInfo(FT_Face face, MDF_FontInformation& fi)
{
	fi.bit_field = 0;
	fi.has_serif = MDF_SERIF_UNDEFINED;

	void* tab = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
	if (tab && (UINTPTR)tab != ~0u)
	{
		TT_OS2* os2 = (TT_OS2*)tab;

		// use IBM font class to determine serifs
		const UINT8 classID = (os2->sFamilyClass & 0xff00) >> 8;
		// const UINT8 subclassID = os2->sFamilyClass & 0xff;
		switch (classID)
		{
		case  0: // no clasification
		case  6: // reserved
		case  9: // ornamentals
		case 11: // reserved
		case 13: // reserved
		case 14: // reserved
		case 12: // symbolic
				fi.has_serif = MDF_SERIF_UNDEFINED; // unknown
				break;

		case  1: // oldstyle serifs
		case  2: // transitional serifs
		case  3: // modern serifs
		case  4: // clarendon serifs
		case  5: // slab serifs
		case  7: // freeform serifs
				fi.has_serif = MDF_SERIF_SERIF; // serif class
				break;

		case  8: // sans serif
		case 10: // scripts
				fi.has_serif = MDF_SERIF_SANS; // sans-serif class
				break;

			default:
				OP_ASSERT(!"IBM font class out of bounds");
				break;
		}

#if 0 // currently not used, since IBM way is less messy - could be used to resolve unknown ...
		// use panose classification to determine serifs
		int serif_idx = 1; // index to serif entry - 1 for latin text, 5 for latin decoratives
		switch (os2.panose[0])
		{
		case 4: // latin decoratives
			serif_idx += 4;
			// fall through
		case 2: // latin text
			switch (os2.panose[serif_idx])
			{
			case  0: // any
			case  1: // no fit - no idea what it means
			case 16: // script
				fi.has_serif = MDF_SERIF_UNDEFINED; // unknown
				break;

			case  2: // cove
			case  3: // obtuse cove
			case  4: // square cove
			case  5: // obtuse square cove
			case  6: // square
			case  7: // thin
			case  8: // oval
			case  9; // exagerated
			case 10: // triangle
				fi.has_serif = MDF_SERIF_SERIF; // serif class
				break;

			case 11: // normal sans
			case 12: // obyuse sans
			case 13: // perpendicular sans
			case 14: // flared
			case 15: // rounded
				fi.has_serif = MDF_SERIF_SANS; // sans-serif class
				break;

			default:
				OP_ASSERT(!"panose latin text serif style out of bounds");
				break;
			}
			break;

		case 3: // latin handwritten
			fi.has_serif = MDF_SERIF_SANS;
			break;
		}
#endif
	}

	if (FT_IS_FIXED_WIDTH(face))
		fi.bit_field |= MDF_FontInformation::IS_MONOSPACE;

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
	fi.m_codepages[0] = fi.m_codepages[1] = 0;
#endif // MDF_CODEPAGES_FROM_OS2_TABLE
	// FIXME: find out if font is smallcaps and set if appropriate

	/* unicode information */
	TT_OS2* os2_table;
	os2_table = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2);

	if (os2_table != NULL && os2_table->version != 0xffff)
	{
		fi.ranges[0] = os2_table->ulUnicodeRange1;
		fi.ranges[1] = os2_table->ulUnicodeRange2;
		fi.ranges[2] = os2_table->ulUnicodeRange3;
		fi.ranges[3] = os2_table->ulUnicodeRange4;

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
		fi.m_codepages[0] = os2_table->ulCodePageRange1;
		fi.m_codepages[1] = os2_table->ulCodePageRange2;
#endif // MDF_CODEPAGES_FROM_OS2_TABLE
	}

	// Fallback to iso-latin-1 if no unicode ranges are set.
	// This is true for the Ahem font.
	UINT32* r = fi.ranges;
	if (!os2_table || os2_table->version == 0xffff || (!r[0] && !r[1] && !r[2] && !r[3]))
	{
		fi.ranges[0] = 3; // set to iso-latin-1-ish
		fi.ranges[1] = 0;
		fi.ranges[2] = 0;
		fi.ranges[3] = 0;
	}

#ifdef MDF_FREETYPE_PCF_FONT_SUPPORT
	if (IsPcfFont(face))
	{
		UINT32 ranges[4] = {0,0,0,0};
		BOOL unicode_ranges_retreived = GetPcfUnicodeRanges(face, ranges);
		if (unicode_ranges_retreived)
		{
			fi.ranges[0] = ranges[0];
			fi.ranges[1] = ranges[1];
			fi.ranges[2] = ranges[2];
			fi.ranges[3] = ranges[3];
		}
	}
#endif
}
MDF_FTFontEngine::MDF_FontInformation* MDF_FTFontEngine::CreateFontInformation(FT_Face face, const char* name, int name_len)
{
	OpString16 s;
	if (OpStatus::IsError(s.SetFromUTF8(name, name_len)))
		return 0;
	const uni_char* font_name = s.CStr();

	MDF_FontInformation* font_info = OP_NEW(MDF_FontInformation, ());
	if (font_info == NULL)
    {
		return NULL;
    }

	font_info->font_name = uni_strdup(font_name);
	if (font_info->font_name == NULL)
    {
		OP_DELETE(font_info);
		return NULL;
    }
	font_info->file_name_list = NULL;

	GetFontInfo(face, *font_info);
	return font_info;
}

/**
   Frees the font information.
   FIXME: free all included types, too.
*/
void MDF_FTFontEngine::FreeFontInformation(MDF_FontInformation* font_information)
{
	MDF_FontFileNameInfo* ni;
	for (MDF_FontFileNameInfo* fi = font_information->file_name_list; fi; fi = ni)
	{
		ni = fi->next;
		FreeFontFileNameInfo(fi);
	}
	op_free(font_information->font_name);

	OP_DELETE(font_information);
}

/**
   Adds a MDF_FontInformation object to the font_array.
*/
OP_STATUS MDF_FTFontEngine::AddFontInformation(unsigned int font_number, MDF_FontInformation* font_information)
{
	return font_array.Add(font_number, font_information);
}

OP_STATUS MDF_FTFontEngine::RemoveFont(MDF_WebFontBase* webfont)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;
	OP_ASSERT(wfi);

	if(!wfi)
	{
		return OpStatus::ERR;
	}

	OP_ASSERT(wfi->family);
	OP_ASSERT(wfi->face);
	OP_ASSERT(wfi->font_array_index >= 0);

	MDF_FontInformation* font_information = wfi->family;

	OP_STATUS err = OpStatus::OK;

	// local fonts should not be removed
	if (font_information->bit_field & MDF_FontInformation::IS_WEBFONT)
	{
		MDF_FontFileNameInfo* font_file_info = wfi->face;
		int font_array_index = wfi->font_array_index;

		// first face is a specialcase
		if(font_information->file_name_list == font_file_info)
		{
			if(font_file_info->next)
			{
				font_information->file_name_list = font_file_info->next;
				FreeFontFileNameInfo(font_file_info);
			}
			else
			{
				err = font_array.Remove(font_array_index, &font_information);
				if(OpStatus::IsSuccess(err))
				{
					FreeFontInformation(font_information);
					font_information = 0;
				}
			}
		}
		else
		{
			MDF_FontFileNameInfo* prev_fi = font_information->file_name_list;
			for (MDF_FontFileNameInfo* fi = font_information->file_name_list; fi; fi = fi->next)
			{
				if(fi == font_file_info)
				{
					prev_fi->next = font_file_info->next;
					FreeFontFileNameInfo(font_file_info);
					break;
				}
				else
				{
					prev_fi = fi;
				}
			}
		}

		// stupid hack: need to check if all remaining
		// MDF_FontFileNameInfo:s are synthesized - if so the
		// MDF_FontInformation should be deleted
		if (font_information)
		{
			MDF_FontFileNameInfo* i = font_information->file_name_list;
			for ( ; i; i = i->next)
				// break on non-synthesized font
				if (!IS_SYNTHESIZED(i))
					break;

			// all fonts were synthesized
			if (!i)
			{
				err = font_array.Remove(font_array_index, &font_information);
				if(OpStatus::IsSuccess(err))
				{
					FreeFontInformation(font_information);
					font_information = 0;
				}
			}
		}
	}

	OP_DELETE(wfi);

	return err;
}

OP_STATUS GetTTName(FT_SfntName* rec, uni_char*& name)
{
	OP_ASSERT(rec->platform_id == TT_PLATFORM_MICROSOFT && rec->encoding_id == TT_MS_ID_UNICODE_CS);
	// string length is in bytes
	OP_ASSERT( rec->string_len % 2 == 0 );
	UINT32 len = rec->string_len >> 1;
	UINT32 lenb = len << 1;
	name = OP_NEWA(uni_char, len + 1);
	if ( !name )
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(name, rec->string, lenb);
	name[len] = 0;

	// change endian
	char* cname = (char*)name;
	for (unsigned int i = 0; i < lenb; i+=2)
	{
		char tmp = cname[i];
		cname[i] = cname[i+1];
		cname[i+1] = tmp;
	}

	return OpStatus::OK;
}

#ifdef COMMIT_LOCALIZED_FONT_NAMES
OP_STATUS MDF_FTFontEngine::CommitLocalizedNames(FT_Face face, const uni_char* western_name)
{
	OP_STATUS status = OpStatus::OK;

	int num_names = FT_Get_Sfnt_Name_Count(face);
	for (int n = 0; n < num_names; ++n)
	{
		FT_SfntName rec;
		int error = FT_Get_Sfnt_Name(face, n, &rec);
		if (error == FT_Err_Out_Of_Memory)
    	{
			return OpStatus::ERR_NO_MEMORY;
		}
		else if (error)
			continue;

		// family name
		if (rec.name_id == TT_NAME_ID_FONT_FAMILY)
		{
			// FIXME: maybe add MAC encodings as well? and what about other MS encodings?

			 // microsoft encoding, unicode BMP
			if (rec.platform_id == TT_PLATFORM_MICROSOFT && rec.encoding_id == TT_MS_ID_UNICODE_CS)
			{
				// FIXME: what languages are interesting? now we add
				// all names, but there are possibly _hundreds_

				uni_char* name;
				OP_STATUS s = GetTTName(&rec, name);
				if (OpStatus::IsError(s))
				{
					if (OpStatus::IsMemoryError(s))
						return s;
					status = s;
					continue;
				}

				// no point in adding the western name to the hash
				if (!uni_strcmp(name, western_name))
				{
					OP_DELETEA(name);
					continue;
				}

				s = g_display_module.AddLocalizedFontName(name, western_name);

				if (OpStatus::IsError(s))
				{
					OP_DELETEA(name);
					if ( OpStatus::IsMemoryError(s) )
						return s;
				}
			} // microsoft unicode BMP
		} // if family name
	} // for all entries in name table

	return status;
}
#endif // COMMIT_LOCALIZED_FONT_NAMES

FT_ULong GetFontVersion(FT_Face face)
{
	// loop over all entries in name table
	int num_names = FT_Get_Sfnt_Name_Count(face);
	for (int n = 0; n < num_names; ++n)
	{
		FT_SfntName rec;
		int error = FT_Get_Sfnt_Name(face, n, &rec);
		if (error == FT_Err_Out_Of_Memory)
    	{
			return 0; // FIXME:OOM
		}
		else if (error)
			continue;
		// version string
		if (rec.name_id == TT_NAME_ID_VERSION_STRING)
		{
			// FIXME: maybe add MAC encodings as well? and what about other MS encodings?
			if (rec.platform_id == TT_PLATFORM_MICROSOFT && rec.encoding_id == TT_MS_ID_UNICODE_CS)
			{
				uni_char* name;
				OP_STATUS s = GetTTName(&rec, name);
				if (OpStatus::IsError(s))
				{
					if (OpStatus::IsMemoryError(s))
						return 0; // FIXME:OOM
					continue;
				}

				if (uni_strnicmp(name, "version ", 8))
				{
					OP_DELETEA(name);
					return 0;
				}

				uni_char* num = name + 8;
				uni_char* p = uni_strchr(num, '.');
				if (!p)
				{
					OP_DELETEA(name);
					return 0;
				}
				*p = 0;
				++p;
				UINT32 maj = uni_atoi(num);
				OP_ASSERT(maj < 0xffff);
				UINT32 min = uni_atoi(p);
				OP_ASSERT(min < 0xffff);
				OP_DELETEA(name);
				return (maj << 16) + min;
			}
		}
	}
	return 0;
}

typedef enum {
	NO_RESTRICTIONS = 0,
	BITMAP_ONLY,
	DONT_USE
} FontRestrictions;
static inline FontRestrictions GetRestrictions(FT_Face face)
{
	const char* name = face->family_name;
	if (!name)
		return NO_RESTRICTIONS;

#ifdef FT_INTERNAL_FREETYPE
#ifndef TT_CONFIG_OPTION_BYTECODE_INTERPRETER
	// MingLiU version 3 renders poorly with hinting disabled
	if (!op_stricmp(name + (*name == 'P' ? 1 : 0), "MingLiU"))
	{
		FT_ULong ver = GetFontVersion(face);
		if (ver >> 16 == 3)
			return BITMAP_ONLY;
		return NO_RESTRICTIONS;
	}
#endif // !TT_CONFIG_OPTION_BYTECODE_INTERPRETER
#endif // FT_INTERNAL_FREETYPE

	return NO_RESTRICTIONS;
}

static
OP_STATUS GetFamilyName(FT_Face face, const uni_char* filename, const char*& family_name, BOOL& allocated_name)
{
	allocated_name = FALSE;
	OP_STATUS status = OpStatus::OK;
	const int num_names = FT_Get_Sfnt_Name_Count(face);
	for (int n = 0; n < num_names; ++n)
	{
		FT_SfntName rec;
		int error = FT_Get_Sfnt_Name(face, n, &rec);
		if (error == FT_Err_Out_Of_Memory)
    	{
			status = OpStatus::ERR_NO_MEMORY;
			break;
		}
		else if (error)
			continue;

		if (rec.platform_id == TT_PLATFORM_MICROSOFT &&
			rec.encoding_id == TT_MS_ID_UNICODE_CS &&
			(rec.language_id & 0x3ff) == 0x009 && // any english
			rec.name_id == TT_NAME_ID_FONT_FAMILY)
		{
			OP_ASSERT((rec.string_len & 1) == 0);
			int family_name_len = rec.string_len >> 1;
			char* f = OP_NEWA(char, family_name_len+1);
			if (!f)
			{
				status = OpStatus::ERR_NO_MEMORY;
				break;
			}
			allocated_name = TRUE;
			family_name = static_cast<const char*>(f);
			f[family_name_len] = 0;
			for (int i = 0; i < family_name_len; ++i)
				f[i] = rec.string[2*i + 1];
			return OpStatus::OK;
		}
	}

	family_name = face->family_name;
	// if we didn't get a name, use filename
	if (!family_name)
	{
		const uni_char* s = filename;
		const uni_char* x = s;
		while( *x++ )
			if( *x == '/' )
				s = x+1;
		OP_ASSERT(s);
		const uni_char* e = uni_strchr(s, '.');

		OpString8 filename_utf8;
		RETURN_IF_ERROR(filename_utf8.SetUTF8FromUTF16(s, e ? e - s : (int)KAll));

		char* f = NULL;
		RETURN_IF_ERROR(SetStr(f, filename_utf8.CStr()));

		allocated_name = TRUE;
		family_name = f;
	}
	return status;
}

OP_STATUS MDF_FTFontEngine::AddFontFileInternal(const uni_char* filename, MDF_WebFontBase** webfont /*= NULL*/)
{
	OP_NEW_DBG("MDF_FTFontEngine::AddFontFileInternal", "webfonts");
	MDF_WebFontImpl* mwfi = NULL;

	if(webfont)
	{
		mwfi = OP_NEW(MDF_WebFontImpl, ());
		if(!mwfi)
			return OpStatus::ERR_NO_MEMORY;

		mwfi->font_array_index = -1;
		mwfi->family = NULL;
		mwfi->face = NULL;
	}

	int font_array_index = -1;

	FT_Face face;
	// Check existance and basic font info, dummy size

	OP_STATUS status = OpStatus::OK;
	int num_faces = 0;
	int i=0;
	do
	{
		// Get font face from a font (collection) file
		OP_STATUS error = GetFontFace(face, filename, i);
		if (OpStatus::IsError(error))
		{
			if (status != OpStatus::ERR_NO_MEMORY)
				status = error;
			if (i == 0)
			{
				// Failed to open correctly a font (collection) file.
				OP_DELETE(mwfi);
				return status;
			}
			else
				// There is something wrong with the current font in the collection -
				// continue to the next font.
				continue;
		}
		else
			if (i == 0)
			{
				// for collections used as webfonts we don't know what
				// to do, but probably it's best to use the first font
				// until it's possible to decide what font in the
				// collection to use
				if (webfont)
					num_faces = 1;
				else
				// Opening a font (collection) file is successful,
				// let's get the number of faces.
				num_faces = face->num_faces;
			}

		FontRestrictions restrictions = GetRestrictions(face);

		if (restrictions == DONT_USE)
			continue;

		const char* family_name = 0;
		OpAutoArray<char> _family_name;
		BOOL allocated_name;
		error = GetFamilyName(face, filename, family_name, allocated_name);
		if (OpStatus::IsSuccess(error))
		{
			if (allocated_name)
				_family_name.reset(const_cast<char*>(family_name));
		}
		else
		{
			OP_ASSERT(OpStatus::IsMemoryError(error));
			status = error;
			if (!family_name)
				continue;
		}

		MDF_FontInformation* font_information = 0;
		if (family_name)
		{
			error = FindFontInformation(family_name, font_information, &font_array_index);
			if (OpStatus::IsMemoryError(error))
			{
				status = error;
				continue;
			}
		}

		int assigned_font_number = -1;
		MDF_FontFileNameInfo* file_name_info = 0;
		if (font_information != NULL && (!mwfi || font_information->bit_field & MDF_FontInformation::IS_WEBFONT))
		{
			file_name_info = CreateFontFileNameInfo(face, filename, i);
			if (file_name_info == NULL || (!mwfi && FontInformationHasBoldItalic(font_information, file_name_info)))
			{
				if (file_name_info)
					FreeFontFileNameInfo(file_name_info);
				else
					status = OpStatus::ERR_NO_MEMORY;
				continue;
			}

			file_name_info->next = font_information->file_name_list;
			font_information->file_name_list = file_name_info;
			assigned_font_number = font_array_index;
		}
		else
		{
			font_information = CreateFontInformation(face, family_name, op_strlen(family_name));
			if (font_information == NULL)
			{
				status = OpStatus::ERR_NO_MEMORY;
				continue;
			}

			if(mwfi)
				font_information->bit_field |= MDF_FontInformation::IS_WEBFONT;

			file_name_info = CreateFontFileNameInfo(face, filename, i);
			if (file_name_info == NULL)
			{
				FreeFontInformation(font_information);
				status = OpStatus::ERR_NO_MEMORY;
				continue;
			}
			font_information->file_name_list = file_name_info;

			assigned_font_number = font_array.GetCount();
			// assigned_font_number might already be in use, need to look for one that's not
			MDF_FontInformation* dummy;
			while (OpStatus::IsSuccess(font_array.GetData(assigned_font_number, &dummy)))
				++ assigned_font_number;
			error = AddFontInformation(assigned_font_number, font_information);
			if (OpStatus::IsError(error))
			{
				FreeFontInformation(font_information);
				// assume oom is most important
				if (!OpStatus::IsMemoryError(status))
					status =  error;
				continue;
			}
		}

		OP_ASSERT(assigned_font_number >= 0);
		OP_ASSERT(file_name_info);

		if (face->face_flags & FT_FACE_FLAG_SCALABLE)
			font_information->bit_field |= MDF_FontInformation::HAS_SCALABLE;
		if (face->face_flags & FT_FACE_FLAG_FIXED_SIZES)
			font_information->bit_field |= MDF_FontInformation::HAS_NONSCALABLE;

		if (restrictions == BITMAP_ONLY)
		{
			OP_ASSERT(font_information->bit_field & MDF_FontInformation::HAS_NONSCALABLE);
			file_name_info->bit_field |= MDF_FontFileNameInfo::ONLY_BITMAP;
		}

		if(mwfi)
		{
			OP_ASSERT(mwfi->font_array_index == -1);
			mwfi->family = font_information;
			mwfi->face = font_information->file_name_list;
			mwfi->font_array_index = assigned_font_number;

			OP_DBG(("Added face: %p (font_array_index: %d)",
					mwfi, font_array_index));
		}

#ifdef COMMIT_LOCALIZED_FONT_NAMES
		error =	CommitLocalizedNames(face, font_information->font_name);
		if (OpStatus::IsMemoryError(error))
			status = error;
#endif // COMMIT_LOCALIZED_FONT_NAMES
	} while( ++i < num_faces );

	if(mwfi)
	{
		if(OpStatus::IsSuccess(status))
		{
			OP_ASSERT(mwfi->font_array_index >= 0);
			OP_ASSERT(mwfi->family);
			OP_ASSERT(mwfi->face);
			*webfont = mwfi;
		}
		else
		{
			OP_DELETE(mwfi);
		}
	}

	return status;
}

OP_STATUS MDF_FTFontEngine::Construct()
{
	int error = FT_Init_FreeType((FT_Library*)&ft_library);
	if (error)
    {
		return error == FT_Err_Out_Of_Memory?OpStatus::ERR_NO_MEMORY:OpStatus::ERR;
    }

#if defined(FT_USE_SMOOTH_LCD_RENDERING) || defined(FT_USE_SMOOTH_LCDV_RENDERING)
	// FT_LCD_FILTER_NONE, FT_LCD_FILTER_DEFAULT, FT_LCD_FILTER_LIGHT, FT_LCD_FILTER_LEGACY
 	FT_Library_SetLcdFilter(ft_library, FT_LCD_FILTER_DEFAULT);
#endif // FT_USE_SMOOTH_LCD_RENDERING

	face_cache = OP_NEW(MDF_FontFaceCache, (this));
	if (!face_cache)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	system_font_count = 0;

	return OpStatus::OK;
}

void MDF_FTFontEngine::FreeFontInformationFunc(const void *item, void *o)
{
	MDF_FontInformation* font_information = (MDF_FontInformation*)o;
	if(font_information)
		FreeFontInformation(font_information);
}

MDF_FTFontEngine::~MDF_FTFontEngine()
{
	OP_DELETE(face_cache);

	FT_Done_FreeType((FT_Library)ft_library);
	ft_library = NULL;

	font_array.ForEach(FreeFontInformationFunc);
	font_array.RemoveAll();
}

OP_STATUS MDF_FTFontEngine::BeginFontEnumeration()
{
	return OpStatus::OK;
}

void MDF_FTFontEngine::EndFontEnumeration()
{
	system_font_count = CountFonts();
}

int MDF_FTFontEngine::CountFonts()
{
	return font_array.GetCount();
}

BOOL MDF_FTFontEngine::HasMask(MDF_FontInformation* font_information, const UINT8 mask, const UINT8 val)
{
	MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
	while (file_name_info != NULL)
    {
		if ((file_name_info->bit_field & mask) == val)
		{
			return true;
		}
		file_name_info = file_name_info->next;
    }

	return false;
}
/**
   Has this font normal support?
*/
BOOL MDF_FTFontEngine::HasNormal(MDF_FontInformation* font_information)
{
	return HasMask(font_information, (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::IS_ITALIC), 0);
}

/**
   Has this font bold support?
*/
BOOL MDF_FTFontEngine::HasBold(MDF_FontInformation* font_information)
{
	return HasMask(font_information, (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::IS_ITALIC), MDF_FontFileNameInfo::IS_BOLD);
}

/**
   Has this font italic support?
*/
BOOL MDF_FTFontEngine::HasItalic(MDF_FontInformation* font_information)
{
	return HasMask(font_information, (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::IS_ITALIC), MDF_FontFileNameInfo::IS_ITALIC);
}

/**
   Has this font bold italic support?
*/
BOOL MDF_FTFontEngine::HasBoldItalic(MDF_FontInformation* font_information)
{
	UINT8 mask = (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::IS_ITALIC);
	return HasMask(font_information, mask, mask);
}

OP_STATUS MDF_FTFontEngine::GetFontInfoInternal(MDF_FontInformation* font_information, MDF_FONTINFO* font_info)
{
	font_info->font_name = font_information->font_name;
    font_info->has_serif = font_information->has_serif;
    font_info->has_normal = HasNormal(font_information);
    font_info->has_bold = HasBold(font_information);
    font_info->has_italic = HasItalic(font_information);
    font_info->has_bold_italic = HasBoldItalic(font_information);

	font_info->ranges[0] = font_information->ranges[0];
	font_info->ranges[1] = font_information->ranges[1];
	font_info->ranges[2] = font_information->ranges[2];
	font_info->ranges[3] = font_information->ranges[3];

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
	font_info->m_codepages[0] = font_information->m_codepages[0];
	font_info->m_codepages[1] = font_information->m_codepages[1];
#endif // MDF_CODEPAGES_FROM_OS2_TABLE

	font_info->is_monospace = (font_information->bit_field & MDF_FontInformation::IS_MONOSPACE) != 0;
    return OpStatus::OK;
}

OP_STATUS MDF_FTFontEngine::GetFontInfo(int font_nr, MDF_FONTINFO* font_info)
{
	MDF_FontInformation* font_information = NULL;
	RETURN_IF_ERROR(font_array.GetData(font_nr, &font_information));
	return GetFontInfoInternal(font_information, font_info);
}

OP_STATUS MDF_FTFontEngine::GetWebFontInfo(MDF_WebFontBase* webfont, MDF_FONTINFO* font_info)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;
	if (!wfi)
		return OpStatus::ERR;

	const OP_STATUS status = GetFontInfoInternal(wfi->family, font_info);

	// there's one ref for each face, font_info should contain data
	// only for the face in question
	font_info->has_normal = font_info->has_bold = font_info->has_italic = font_info->has_bold_italic = FALSE;

	const BOOL bold = (wfi->face->bit_field & (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::BOLDIFY)) != 0;
	const BOOL italic = (wfi->face->bit_field & (MDF_FontFileNameInfo::IS_ITALIC | MDF_FontFileNameInfo::APPLY_ITALICS)) != 0;

	if (bold)
	{
		if (italic)
			font_info->has_bold_italic = TRUE;
		else
			font_info->has_bold = TRUE;
	}
	else if (italic)
		font_info->has_italic = TRUE;
	else
		font_info->has_normal = TRUE;

	return status;
}

OpFontInfo::FontType MDF_FTFontEngine::GetWebFontType(MDF_WebFontBase* webfont)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;
	if (!wfi)
		return OpFontInfo::PLATFORM;

	if (wfi->family->bit_field & MDF_FontInformation::IS_WEBFONT)
		return OpFontInfo::PLATFORM_WEBFONT;
	else
		return OpFontInfo::PLATFORM_LOCAL_WEBFONT;
}

OP_STATUS MDF_FTFontEngine::GetLocalFont(MDF_WebFontBase*& localfont, const uni_char* facename)
{
#ifdef MDF_SFNT_TABLES
	const unsigned long tag = (((unsigned long)'n'<<24)|((unsigned long)'a'<<16)|((unsigned long)'m'<<8)|'e');
	const int dummy_size = 4;
	FT_Face face;
	BYTE* tab;
	UINT32 size;

	OpHashIterator* it = font_array.GetIterator();
	if (!it)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoPtr<OpHashIterator> _it(it);
	for (OP_STATUS status = it->First(); OpStatus::IsSuccess(status); status = it->Next())
	{
		MDF_FontInformation* fontinfo = (MDF_FontInformation*)it->GetData();
		OP_ASSERT(fontinfo);
		if (fontinfo->bit_field & MDF_FontInformation::IS_WEBFONT)
			continue;
		for (MDF_FontFileNameInfo* fileinfo = fontinfo->file_name_list; fileinfo; fileinfo = fileinfo->next)
		{
			if (IS_SYNTHESIZED(fileinfo)) // don't match synthesized fonts
				continue;
#ifdef PI_WEBFONT_OPAQUE
			// hash is empty - fill with names from name table
			if (!fileinfo->local_names.GetCount())
			{
				// load name table
				RETURN_IF_ERROR(GetFontFace(face, fileinfo, dummy_size));
				RETURN_IF_ERROR(GetTableInt(face, tag, tab, size));
				// fetch names
				const OP_STATUS status = MDF_GetFaceNames(tab, size,
				                                          fileinfo->local_names);
				// free name table
				FreeTable(tab);

				if (OpStatus::IsError(status))
				{
					// empty hash - code assumes all valid names are contained
					fileinfo->local_names.DeleteAll();
					return status;
				}
			}

			// facename exists in hash
			if (fileinfo->local_names.Contains(facename))
			{
				MDF_WebFontImpl* mwfi = OP_NEW(MDF_WebFontImpl, ());
				if (!mwfi)
					return OpStatus::ERR_NO_MEMORY;
				mwfi->font_array_index = (int)(INTPTR)it->GetKey();
				mwfi->family = fontinfo;
				mwfi->face = fileinfo;
				localfont = mwfi;
				return OpStatus::OK;
			}
#endif // PI_WEBFONT_OPAQUE
		}
	}
	return OpStatus::OK;
#else // !MDF_SFNT_TABLES
	localfont = NULL;
	return OpStatus::OK;
#endif // !MDF_SFNT_TABLES
}

const uni_char* MDF_FTFontEngine::GetFontName(MDE_FONT* font)
{
	MDF_FT_FONT* ftfont = (MDF_FT_FONT*)font->private_data;
	return ftfont->font_name;
}

const uni_char* MDF_FTFontEngine::GetFontFileName(MDE_FONT* font)
{
	MDF_FT_FONT* ftfont = (MDF_FT_FONT*)font->private_data;
	return ftfont->file_name_info->file_name;
}

BOOL MDF_FTFontEngine::HasGlyph(MDE_FONT* font, uni_char c)
{
	FT_Face face;
	MDF_FT_FONT* font_data = (MDF_FT_FONT*)font->private_data;
	OP_STATUS error = GetFontFace(face, font_data->file_name_info, font_data->font_size);
    return !OpStatus::IsError(error) && (FT_Get_Char_Index(face, c) != 0);
}

BOOL MDF_FTFontEngine::IsScalable(MDE_FONT* font)
{
	FT_Face face = (FT_Face) GetFontImplementation(font);
	return face && FT_IS_SCALABLE(face);
}

BOOL MDF_FTFontEngine::IsBold(MDE_FONT* font)
{
	MDF_FT_FONT* font_data = (MDF_FT_FONT*)font->private_data;
	return font_data->file_name_info->bit_field & MDF_FontFileNameInfo::IS_BOLD ? TRUE : FALSE;
}

BOOL MDF_FTFontEngine::IsItalic(MDE_FONT* font)
{
	MDF_FT_FONT* font_data = (MDF_FT_FONT*)font->private_data;
	return font_data->file_name_info->bit_field & MDF_FontFileNameInfo::IS_ITALIC? TRUE : FALSE;
}

#ifdef MDF_KERNING
OP_STATUS MDF_FTFontEngine::ApplyKerning(MDE_FONT* font, ProcessedString& processed_string)
{
	OP_ASSERT(processed_string.m_is_glyph_indices);

	FT_Face face = (FT_Face)GetFontImplementation(font);
	if (!face)
		return OpStatus::ERR;

	const FT_UInt kernMode = FT_KERNING_DEFAULT;
	FT_Vector kerning;
	INT32 acc_x = 0;
	for (size_t i = 1; i < processed_string.m_length; ++i)
	{
		if (!FT_Get_Kerning(face,
							processed_string.m_processed_glyphs[i-1].m_id,
							processed_string.m_processed_glyphs[i].m_id,
							kernMode,
							&kerning))
			acc_x += kerning.x >> 6;
		processed_string.m_processed_glyphs[i].m_pos.x += acc_x;
	}

	processed_string.m_advance += acc_x;
	return OpStatus::OK;
}
#endif // MDF_KERNING

#ifdef OPFONT_GLYPH_PROPS
OP_STATUS MDF_FTFontEngine::GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props)
{
#ifdef MDF_FONT_GLYPH_CACHE
	MDF_GlyphHandle gh(font);
	RETURN_IF_ERROR(gh.Get(up, TRUE, FALSE));
	MDF_GLYPH& g = gh.Glyph();
	OP_ASSERT(g.buffer_data);

	props->left = g.bitmap_left;
	props->top = g.bitmap_top;
	props->width = g.buffer_data->w;
	props->height = g.buffer_data->h;
	props->advance = g.advance;
	return OpStatus::OK;
#else
	OP_ASSERT(!"Do we ever come here?");
	return OpStatus::ERR;
#endif // MDF_FONT_GLYPH_CACHE
}
#endif // OPFONT_GLYPH_PROPS

/**
   Finds the file name for the font_nr and specified
   bold and italic.
*/
MDF_FTFontEngine::MDF_FontFileNameInfo* MDF_FTFontEngine::FindFileNameInfo(int font_nr, int size, BOOL bold, BOOL italic)
{
	MDF_FontInformation* font_information = NULL;
	if(OpStatus::IsError(font_array.GetData(font_nr, &font_information)))
		return NULL;

	if (font_information->bit_field & MDF_FontInformation::HAS_NONSCALABLE)
	{
		int actual_size;
		MDF_FontFileNameInfo* info = GetBestNonscalableFontFileNameInfo(font_information, size, bold, italic, actual_size);
		// if it's a prefect match or there isn't a scalable version, use this one
		if (info && (actual_size == size || !(font_information->bit_field & MDF_FontInformation::HAS_SCALABLE)))
			return info;
	}

	UINT8 mask = (MDF_FontFileNameInfo::IS_BOLD | MDF_FontFileNameInfo::IS_ITALIC);
	UINT8 wanted = 0;
	if (bold)
		wanted |= MDF_FontFileNameInfo::IS_BOLD;
	if (italic)
		wanted |= MDF_FontFileNameInfo::IS_ITALIC;
    MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
	while (file_name_info != NULL)
    {
		if (!file_name_info->num_fixed_sizes && // ignore non-scalable fonts this turn
			(file_name_info->bit_field & mask) == wanted)
		{
			return file_name_info;
		}
		file_name_info = file_name_info->next;
    }

    return NULL;
}

#ifdef USE_ONLY_LIMITED_FONT_SIZE_SET
int GetClosestSize(INT32 size)
{
	if (!g_opera->mdefont_module.FixedSizeTable || !g_opera->mdefont_module.NumFixedSizes)
		return size;
	int delta = INT_MAX;
	int cand = -1;
	for (int i = 0; i < g_opera->mdefont_module.NumFixedSizes; ++i)
		if (op_abs(size-g_opera->mdefont_module.FixedSizeTable[i]) < delta)
		{
			delta = op_abs(size-g_opera->mdefont_module.FixedSizeTable[i]);
			cand = i;
		}
	OP_ASSERT(cand >= 0 && cand < g_opera->mdefont_module.NumFixedSizes);
	return g_opera->mdefont_module.FixedSizeTable[cand];
}
#endif // USE_ONLY_LIMITED_FONT_SIZE_SET

MDE_FONT* MDF_FTFontEngine::GetWebFont(MDF_WebFontBase* webfont, int size)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;
	return GetFontInternal(wfi->face, wfi->family, size);
}

MDE_FONT* MDF_FTFontEngine::GetWebFont(MDF_WebFontBase* webfont, UINT8 weight, BOOL italic, int size)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;

	const BOOL boldify =
		weight > 5 &&
		!(wfi->face->bit_field & MDF_FontFileNameInfo::IS_BOLD);
	const BOOL italicize =
		italic &&
		!(wfi->face->bit_field & MDF_FontFileNameInfo::IS_ITALIC);

	MDF_FontFileNameInfo* synthesized = GetSynthesizedFileInfo(wfi->family, wfi->face, boldify, italicize);
	return GetFontInternal(synthesized, wfi->family, size);
}

MDF_FTFontEngine::MDF_FontFileNameInfo* MDF_FTFontEngine::GetSynthesizedFileInfo(MDF_FontInformation* font_information, MDF_FontFileNameInfo* base_file_info, BOOL boldify, BOOL italicize)
{
	if (!base_file_info || !font_information)
		return 0;

	// attempt to find an already created entry
	const UINT8 mask =
		MDF_FontFileNameInfo::BOLDIFY | MDF_FontFileNameInfo::APPLY_ITALICS;
	const UINT8 wanted =
		(boldify   ? MDF_FontFileNameInfo::BOLDIFY       : 0) |
		(italicize ? MDF_FontFileNameInfo::APPLY_ITALICS : 0);
	for (MDF_FontFileNameInfo* i = font_information->file_name_list; i; i = i->next)
		if ((i->bit_field & mask) == wanted)
			return i;

	// shouldn't use outlines for this font
	if (base_file_info->bit_field & MDF_FontFileNameInfo::ONLY_BITMAP)
		return 0;

	FT_Face face;
	if (OpStatus::IsError(GetFontFace(face, base_file_info->file_name, base_file_info->font_index)))
		return 0;

	// transformations and boldification only work for scalable fonts
	if (IsNonscalableFont(face))
		return 0;

	MDF_FontFileNameInfo* synthesized_name_info = CreateFontFileNameInfo(face, base_file_info->file_name, base_file_info->font_index);

	// FIXME:OOM
	if (!synthesized_name_info)
		return 0;

	if (italicize)
	{
		synthesized_name_info->bit_field |= MDF_FontFileNameInfo::IS_ITALIC;
		synthesized_name_info->bit_field |= MDF_FontFileNameInfo::APPLY_ITALICS;
	}
	if (boldify)
	{
		synthesized_name_info->bit_field |= MDF_FontFileNameInfo::IS_BOLD;
		synthesized_name_info->bit_field |= MDF_FontFileNameInfo::BOLDIFY;
	}

	synthesized_name_info->next = font_information->file_name_list;
	font_information->file_name_list = synthesized_name_info;
	font_information->bit_field |= MDF_FontInformation::HAS_SCALABLE;

	return synthesized_name_info;
}

MDE_FONT* MDF_FTFontEngine::GetFont(int font_nr, int size, BOOL bold, BOOL italic)
{
#ifdef USE_ONLY_LIMITED_FONT_SIZE_SET
	size = GetClosestSize(size);
#endif // USE_ONLY_LIMITED_FONT_SIZE_SET

    // Does not share fonts yet (no ref counting).
    // We will also use a cache later.
    MDF_FontFileNameInfo* file_name_info = FindFileNameInfo(font_nr, size, bold, italic);

	// if italic is requested but we havn't got a font for it, see if the font
	// is scalable, and in case it is create a new entry that is tagged to be
	// slanted.
	// conversely, boldify non-bold scalable fonts.
	if (!file_name_info && (bold || italic))
	{
		// always use plain font as base, since we don't know what modifications we've done already
		MDF_FontFileNameInfo* base_file_info = FindFileNameInfo(font_nr, size, false, false);

		OP_ASSERT(font_nr >= 0);
		MDF_FontInformation* font_information;
		OP_STATUS s = font_array.GetData(font_nr, &font_information);
		OP_ASSERT(OpStatus::IsSuccess(s) && font_information);
		if (OpStatus::IsError(s)) // suppress compiler warning
			return 0;
		file_name_info = GetSynthesizedFileInfo(font_information, base_file_info, bold, italic);
	}

	// try to find any version remotely resembling what's wanted
	if (!file_name_info)
	{
		MDF_FontInformation* font_information = NULL;
		if(OpStatus::IsSuccess(font_array.GetData(font_nr, &font_information)))
		{
			int score = -1;
			for (MDF_FontFileNameInfo* f = font_information->file_name_list; f; f = f->next)
			{
				const BOOL b = (f->bit_field & MDF_FontFileNameInfo::IS_BOLD)   != 0;
				const BOOL i = (f->bit_field & MDF_FontFileNameInfo::IS_ITALIC) != 0;
				// no difference in score here - maybe one is better than the other?
				int s = (b == bold ? 1 : 0) + (i == italic ? 1 : 0);
				if (s > score)
				{
					score = s;
					file_name_info = f;
				}
			}
		}
	}

    return GetFontInternal(file_name_info, font_nr, size);
}

MDE_FONT* MDF_FTFontEngine::GetFontInternal(MDF_FontFileNameInfo* file_name_info, MDF_FontInformation* font_information, int size)
{
    if (file_name_info != NULL)
    {
		FT_Face face;
		if (OpStatus::IsError(GetFontFace( face, file_name_info, size )))
			return NULL;

		MDF_FT_FONT* font_data = OP_NEW(MDF_FT_FONT, ());

		// FIXME:OOM
		if (!font_data)
			return NULL;

		OP_ASSERT(font_information);
		font_data->font_name = font_information->font_name;

		font_data->font_size = size;
		font_data->file_name_info = file_name_info;
		font_data->load_flags = DEFAULT_LOAD_FLAGS;
#ifdef MDF_FONT_GLYPH_CACHE
		if ( OpStatus::IsError(MDF_FontGlyphCache::Create(&font_data->glyph_cache, this)) )
		{
			OP_DELETE(font_data);
			return NULL;
		}
#endif // MDF_FONT_GLYPH_CACHE

		MDE_FONT* font = OP_NEW(MDE_FONT, ());
		if (font == NULL)
		{
			OP_DELETE(font_data);
			return NULL;
		}

		font->private_data = (void*)font_data;
		FT_Size_Metrics metrics = face->size->metrics;
		font->ascent = metrics.ascender >> 6;
		font->descent = (-metrics.descender) >> 6;
		font->height = font->ascent + font->descent;//metrics.height >> 6;
		font->max_advance = (metrics.max_advance + (1 << 6) - 1) >> 6;
		font->extra_padding = 0;

		// synthesized fonts will be bigger than what the font reports - see CT-1911
		if (IS_SYNTHESIZED(file_name_info))
		{
			unsigned extra = 0;
			if (file_name_info->bit_field & MDF_FTFontEngine::MDF_FontFileNameInfo::BOLDIFY)
				extra += font->height / 6; // wild guess
			if (file_name_info->bit_field & MDF_FTFontEngine::MDF_FontFileNameInfo::APPLY_ITALICS)
				extra += (font->height * MDF_FT_SLANT_FACTOR + (1<<15)) >> 16;
			// a minimum amount is likely needed
			if (extra < 4)
				extra = 4;

			font->extra_padding += extra;
		}

		// Guess a internal leading based on the assumption that normal lineheight is 1.2 and
		// freetype include the internal leading in ascent+descent. That means internal leading
		// should be around 0.2 of that.
		// Seems to work perfect in my tests /emil
// 		font->internal_leading = (int) (font->height * 0.2f + 0.5f);
		font->internal_leading = font->height - size;

		// if size is the same as font->height (font->ascent+font->descent)
		// leading is not included and we (probably) don't want it
		// because of rounding (we might not get exactly the size we ask for) we
		// must still use special check for ahem, which shouldn't have internal leading.
		BOOL isAhem = (face->family_name && !op_strcmp(face->family_name, "Ahem"));
		if (!IsNonscalableFont(face) && (size == font->height || isAhem))
		{
			font->internal_leading = 0;
			// We also have to calculate ascent and descent from the original information
			// in the face, instead of using the metrics which seem to have rounding errors for ahem.
			// I haven't found a generic calculation which works for all fonts.
			font->height = metrics.height >> 6;
			font->ascent = (font->height * face->ascender + face->height / 2) / face->height;
			font->descent = font->height - font->ascent;
		}

		return font;
    }

    return NULL;
}

MDE_FONT* MDF_FTFontEngine::GetFontInternal(MDF_FontFileNameInfo* file_name_info, int font_nr, int size)
{
	MDF_FontInformation* font_information = NULL;
	OP_STATUS err = font_array.GetData(font_nr, &font_information);
	if(OpStatus::IsError(err))
		return NULL;
	return GetFontInternal(file_name_info, font_information, size);
}

void MDF_FTFontEngine::ReleaseFont(MDE_FONT* font)
{
    // Does not share fonts yet.
	if (font)
	{
		MDF_FT_FONT *ptr = (MDF_FT_FONT *)font->private_data;
		OP_DELETE(ptr);
		OP_DELETE(font);
	}
}

OP_STATUS MDF_FTFontEngine::UCPToGID(MDE_FONT* font, ProcessedString& processed_string)
{
	OP_ASSERT(!processed_string.m_is_glyph_indices);
	MDF_FT_FONT* font_data=(MDF_FT_FONT*)font->private_data;
	FT_Face face;
	RETURN_IF_ERROR(GetFontFace(face, font_data->file_name_info, font_data->font_size));
	for (size_t i = 0; i < processed_string.m_length; ++i)
		processed_string.m_processed_glyphs[i].m_id = FT_Get_Char_Index(face, processed_string.m_processed_glyphs[i].m_id);
	processed_string.m_is_glyph_indices = TRUE;
	return OpStatus::OK;
}

UINT32 MDF_FTFontEngine::GetCharIndex(MDE_FONT* font, const UnicodePoint ch)
{
	MDF_FT_FONT* font_data=(MDF_FT_FONT*)font->private_data;
	FT_Face face;
	OP_STATUS error = GetFontFace(face, font_data->file_name_info, font_data->font_size);
	return OpStatus::IsError(error) ? 0 : FT_Get_Char_Index(face, ch);
}
UINT32 MDF_FTFontEngine::GetCharIndex(MDE_FONT* font, const uni_char* str, const UINT32 len, UINT32& pos)
{
	UINT32 ch = GetCharIndex(font, str[pos]);
	++pos;
	return ch;
}

void* MDF_FTFontEngine::GetFontImplementation(MDE_FONT* font)
{
	MDF_FT_FONT* font_data=(MDF_FT_FONT*)font->private_data;
	FT_Face face;
	OP_STATUS error = GetFontFace(face, font_data->file_name_info, font_data->font_size);
	return OpStatus::IsError(error) ? NULL : face;
}

#ifdef MDF_SFNT_TABLES

OP_STATUS MDF_FTFontEngine::GetTable(MDE_FONT* font, unsigned long tab_tag, BYTE*& tab, UINT32& size)
{
	tab = 0;
	size = 0;

	FT_Face face;
	MDF_FT_FONT* font_data = (MDF_FT_FONT*)font->private_data;
	RETURN_IF_ERROR(GetFontFace(face, font_data->file_name_info, font_data->font_size));
	return GetTableInt(face, tab_tag, tab, size);
}

OP_STATUS MDF_FTFontEngine::GetTableInt(FT_Face face, unsigned long tab_tag, BYTE*& tab, UINT32& size)
{
	OP_ASSERT(face);

	FT_ULong length = 0;

	FT_Error error = FT_Load_Sfnt_Table(face, tab_tag, 0, NULL, &length);

	if (error)
		return OpStatus::ERR;

	OP_ASSERT(length);
	FT_Byte* raw_table = OP_NEWA(FT_Byte, length);

	if (!raw_table)
		return OpStatus::ERR_NO_MEMORY;

	error = FT_Load_Sfnt_Table(face, tab_tag, 0, raw_table, &length);

	if (error)
	{
		OP_DELETEA(raw_table);
		return OpStatus::ERR;
	}

	tab = raw_table;
	size = (UINT32)length;
	return OpStatus::OK;
}

void MDF_FTFontEngine::FreeTable(BYTE* tab)
{
	OP_DELETEA(tab);
}
#endif // MDF_SFNT_TABLES

OP_STATUS MDF_FTFontEngine::GetGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender, BOOL isIndex)
{
#ifdef MDF_FONT_GLYPH_CACHE
	// fetch glyph from cache
	MDF_FontGlyphCache *cache = ((MDF_FT_FONT*)font->private_data)->glyph_cache;
	MDF_FontGlyph* cglyph;
	RETURN_IF_ERROR(cache->GetChar(cglyph, ch, font, mustRender, isIndex));
	// copy bitmap data
	MDF_GLYPH_BUFFER* buffer = 0;
	if (mustRender)
	{
		const UINT32 width = cglyph->buffer_data->w;
		const UINT32 height = cglyph->buffer_data->h;
		const size_t size = width*height;
		buffer = (MDF_GLYPH_BUFFER*)op_malloc(size + sizeof(MDF_GLYPH_BUFFER));
		if (!buffer)
			return OpStatus::ERR_NO_MEMORY;
		buffer->data = (char*)buffer + sizeof(MDF_GLYPH_BUFFER);
		op_memcpy(buffer->data, cglyph->buffer_data->data, size);
		buffer->w = width;
		buffer->h = height;
	}
	glyph.buffer_data = buffer;
	glyph.bitmap_left = cglyph->bitmap_left;
	glyph.bitmap_top = cglyph->bitmap_top;
	glyph.advance = cglyph->advance;
#else // MDF_FONT_GLYPH_CACHE
	BOOL render = mustRender;
	glyph.buffer_data = 0;
	RETURN_IF_ERROR(LoadFontGlyph(glyph, ch, font, render, isIndex));
	// glyph was rendered because we skewed or boldified it
	if (!mustRender && render)
	{
		FreeGlyph(glyph);
		glyph.buffer_data = 0;
	}
#endif // MDF_FONT_GLYPH_CACHE
	return OpStatus::OK;
}
void MDF_FTFontEngine::FreeGlyph(MDF_GLYPH& glyph)
{
	// buffer_data->data is contained in buffer_data
	op_free(glyph.buffer_data);
}
OP_STATUS MDF_FTFontEngine::FillGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, const UINT32 stride, BOOL isIndex)
{
	BOOL render = TRUE;
	return LoadFontGlyph(glyph, ch, font, render, isIndex, stride);
}
#ifdef MDF_FONT_GLYPH_CACHE
OP_STATUS MDF_FTFontEngine::BorrowGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender, BOOL isIndex)
{
	MDF_FontGlyph* cglyph;
	MDF_FontGlyphCache *cache = ((MDF_FT_FONT*)font->private_data)->glyph_cache;
	RETURN_IF_ERROR(cache->GetChar(cglyph, ch, font, mustRender, isIndex));
	glyph.buffer_data = cglyph->buffer_data;
	glyph.bitmap_left = cglyph->bitmap_left;
	glyph.bitmap_top = cglyph->bitmap_top;
	glyph.advance = cglyph->advance;

	return OpStatus::OK;
}
#endif // MDF_FONT_GLYPH_CACHE

#ifdef SVG_SUPPORT
// Freetype returns 26.6 fractional pixels if FT_LOAD_NO_SCALE is NOT
// set in the FT_Load_Char() call below.
# define FT_POS_TO_SVGNUMBER(x) (SVGNumber((x) / 64.0))
# define FT_FIXED_TO_SVGNUMBER(x) (SVGNumber((x) / 65536.0))
# if FREETYPE_MAJOR>=2 && FREETYPE_MINOR>=2
#  define FTCONST const
# else
#  define FTCONST
# endif // freetype version checking
static inline int MoveToProc(FTCONST FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	SVGPath* p = (SVGPath*)data;
	p->Close(); /* Does nothing if no other commands has been sent
				 * yet. */
	p->MoveTo(FT_POS_TO_SVGNUMBER(to->x),
			  FT_POS_TO_SVGNUMBER(to->y),
			  FALSE);
	return 0;
}
static inline int LineToProc(FTCONST FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	SVGPath* p = (SVGPath*)data;
	p->LineTo(FT_POS_TO_SVGNUMBER(to->x),
			  FT_POS_TO_SVGNUMBER(to->y),
			  FALSE);
	return 0;
}
static inline int QuadraticCurveToProc(FTCONST FT_Vector* control, FTCONST FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	SVGPath* p = (SVGPath*)data;
	p->QuadraticCurveTo(FT_POS_TO_SVGNUMBER(control->x),
						FT_POS_TO_SVGNUMBER(control->y),
						FT_POS_TO_SVGNUMBER(to->x),
						FT_POS_TO_SVGNUMBER(to->y), FALSE, FALSE);
	return 0;
}
static inline int CubicCurveToProc(FTCONST FT_Vector* control1, FTCONST FT_Vector* control2, FTCONST FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	SVGPath* p = (SVGPath*)data;
	p->CubicCurveTo(FT_POS_TO_SVGNUMBER(control1->x),
					FT_POS_TO_SVGNUMBER(control1->y),
					FT_POS_TO_SVGNUMBER(control2->x),
					FT_POS_TO_SVGNUMBER(control2->y),
					FT_POS_TO_SVGNUMBER(to->x),
					FT_POS_TO_SVGNUMBER(to->y),
					FALSE, FALSE);
	return 0;
}
OP_STATUS MDF_FTFontEngine::GetOutline(MDE_FONT* font, const UINT32 g_id, const UINT32 next_g_id, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath* out_glyph)
{
	MDF_FT_FONT* ftfont = (MDF_FT_FONT*)font->private_data;
	if (ftfont->file_name_info->bit_field & MDF_FontFileNameInfo::ONLY_BITMAP)
		return OpStatus::ERR;
	FT_Face face = (FT_Face) GetFontImplementation(font);
	if (!face)
		return OpStatus::ERR;

	if (ftfont->file_name_info->bit_field & MDF_FontFileNameInfo::APPLY_ITALICS)
		ApplyItalics(face);
	else
		FT_Set_Transform( face, 0, 0 );

	BOOL use_kerning = next_g_id != 0 && FT_HAS_KERNING(face);
	FT_Error err = FT_Load_Glyph(face, g_id, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
	if(err == 0)
	{
		if (!(ftfont->file_name_info->bit_field & MDF_FontFileNameInfo::BOLDIFY) ||
		    OpStatus::IsSuccess(ApplyBoldification(face, face->size->metrics.y_ppem)))
		{
			FT_GlyphSlot slot = face->glyph;
			FT_Outline outline = slot->outline;

			FT_Outline_Funcs funcs;
			funcs.move_to = &MoveToProc;
			funcs.line_to = &LineToProc;
			funcs.conic_to = &QuadraticCurveToProc;
			funcs.cubic_to = &CubicCurveToProc;
			funcs.shift = 0;
			funcs.delta = 0;

			FT_Outline_Decompose(&outline, &funcs, (void *)out_glyph);
			out_glyph->Close();
		}
	}

	if(face->glyph)
	{
		if(in_writing_direction_horizontal)
		{
			// face->glyph->advance.x is grid fitted and "inexact"
			out_advance = FT_FIXED_TO_SVGNUMBER(face->glyph->linearHoriAdvance);
			OP_ASSERT(out_advance.NotEqual(0) || !face->glyph->advance.x);
		}
		else
		{
			// Vertical advances are unreliable, thus the fallbacks
			out_advance = FT_FIXED_TO_SVGNUMBER(face->glyph->linearVertAdvance);
			// Older versions of freetype (maybe fixed spring 2006) contains
			// parser errors when working with certain fonts. See for instance
			// http://www.mail-archive.com/freetype@nongnu.org/msg00302.html
			//
			// We try to detact when numbers are extremely large and fallback
			// on something useful.
#define CORRECT_FREETYPE_BUG
#ifdef CORRECT_FREETYPE_BUG
			if (out_advance > 10*font->height)
			{
				// Absurd number.
				//OP_ASSERT(!"Either the font or the used freetype version is broken");
				out_advance = font->height;
			}
			else
#endif // CORRECT_FREETYPE_BUG
			if (out_advance.Equal(0) && face->glyph->advance.x)
			{
				// No vertical advance, but a horizontal one. Probably
				// an error in the font.
				// Use Ascent()+Descent() as fallback. This will cause errors, hopefully small
				out_advance = font->height;
			}
		}
	}

	// kerning
	if(use_kerning)
	{
		FT_Vector v;
		FT_Error err = FT_Get_Kerning(face, g_id, next_g_id, FT_KERNING_DEFAULT, &v);
		if(err == 0)
		{
			if(in_writing_direction_horizontal)
			{
				out_advance += FT_POS_TO_SVGNUMBER(v.x);
			}
#if 0 // This kerning is based on writing horizontally so we won't use it for vertical text
			else
			{
				out_advance += FT_POS_TO_SVGNUMBER(v.y);
			}
#endif //
		}
	}

	return OpStatus::OK;
}
#endif // SVG_SUPPORT


#ifdef _DEBUG
const uni_char* MDF_FTFontEngine::DebugGetFaceName(MDE_FONT* font)
{
	MDF_FT_FONT* ft_font = (MDF_FT_FONT*)font->private_data;
	return ft_font->font_name;
}
const uni_char* MDF_FTFontEngine::DebugGetFileName(MDE_FONT* font)
{
	MDF_FT_FONT* ft_font = (MDF_FT_FONT*)font->private_data;
	return ft_font->file_name_info->file_name;
}
#endif // _DEBUG

#endif // MDF_FREETYPE_SUPPORT

#endif // MDEFONT_MODULE
