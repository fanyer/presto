/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/libvega/vega3ddevice.h"
#include "modules/libvega/vega2ddevice.h"
#include "modules/mdefont/processedstring.h"

#if defined(MDF_FONT_ADVANCE_CACHE) || defined(MDF_OPENTYPE_SUPPORT)
# include "modules/libvega/src/oppainter/vegamdefont.h"
#endif // MDF_FONT_ADVANCE_CACHE || MDF_OPENTYPE_SUPPORT

#ifdef VEGA_MDEFONT_SUPPORT
#include "modules/mdefont/mdefont.h"
#endif // VEGA_MDEFONT_SUPPORT

#if defined(VEGA_3DDEVICE) && defined(VEGA_SUBPIXEL_FONT_BLENDING)
#include "modules/libvega/src/vegaswbuffer.h"
#endif

// Glyph cache will grow as needed unless it hits memory limit.
// TWEAK_VEGA_GLYPH_CACHE_MEM_LIMIT is defined in kb and covers
// both caches, and so the limit for primary cache in bytes is:
#define PRIMARY_GLYPH_CACHE_MEM_LIMIT (VEGA_GLYPH_CACHE_MEM_LIMIT * 1024 / 2)

VEGAFont::VEGAFont() :
#ifdef VEGA_3DDEVICE
			bitmapCache(NULL), slotWidth(0), slotHeight(0),
			xSlots(0), ySlots(0),
#endif // VEGA_3DDEVICE
			cachedGlyphs(NULL),
			primaryCache(NULL), primaryCacheSize(0), primaryCacheMemSize(0),
			secondaryCache(NULL), availableGlyphs(0)
#ifdef VEGA_3DDEVICE
					 , texture(NULL)
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
					 , bitmapCache2(NULL)
					 , texture2(NULL)
#endif
					 , hwDataDirty(false), batchingBackend(NULL)
#endif // VEGA_3DDEVICE
{}

VEGAFont::~VEGAFont()
{
#ifdef VEGA_3DDEVICE
	// see DSK-329510: must make sure no pending batch uses this font after deletion
	if (batchingBackend)
		batchingBackend->onFontDeleted(this);
#endif // VEGA_3DDEVICE

	OP_DELETEA(cachedGlyphs);
	OP_DELETEA(primaryCache);
	OP_DELETEA(secondaryCache);
#ifdef VEGA_3DDEVICE
	OP_DELETEA(bitmapCache);
	if (texture)
	{
		g_vegaGlobals.vega3dDevice->UnregisterContextLostListener(this);
		VEGARefCount::DecRef(texture);
	}
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
	if (texture2)
	{
		VEGARefCount::DecRef(texture2);
	}
	OP_DELETEA(bitmapCache2);
#endif
#endif // VEGA_3DDEVICE
}

OP_STATUS VEGAFont::Construct()
{
#ifdef VEGA_NATIVE_FONT_SUPPORT
	// Don't create a cache for native font rendering since it should not be used anyway
	if (nativeRendering())
		return OpStatus::OK;
#endif // VEGA_NATIVE_FONT_SUPPORT
	unsigned int i;

	cacheSize = VEGA_GLYPH_CACHE_INITIAL_SIZE;

#ifdef VEGA_3DDEVICE
	// FIXME: Allow 2D-backends to use this cache type
	if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D)
	{
		cacheSize = VEGA_GLYPH_CACHE_SIZE;

		slotWidth = MaxGlyphWidth()+getBlurRadius()*2;
		slotHeight = MaxGlyphHeight()+getBlurRadius()*2;
		// find the texture resolution wasting the least amount of
		// texture when storing in a power of two texture
		int curMin = -1;
		cacheSize*=2; // will be divided by 2 at the start of the loop
		while (curMin < 0)
		{
			cacheSize /= 2;
			OP_ASSERT(cacheSize > 0);
			if (cacheSize <= 0)
				return OpStatus::ERR;
			xSlots = cacheSize*2;
			ySlots = 1;
			int cur = 0;
			int minWaste = INT_MAX;
			while (xSlots > 0)
			{
				int w = slotWidth*xSlots;
				int h = slotHeight*ySlots;
				int w2 = VEGA3dDevice::pow2_ceil(w);
				int h2 = VEGA3dDevice::pow2_ceil(h);
				int waste = (w2-w)*h2+(h2-h)*w;
				if (waste < minWaste)
				{
					// FIXME: make sure this is a valid texture size, this just keeps cache textures small
					if ((w2 <= 512 && h2 <= 512) || cacheSize == 1)
					{
						minWaste = waste;
						curMin = cur;
					}
				}
				xSlots /= 2;
				ySlots *= 2;
				++cur;
			}
		}
		xSlots = (cacheSize*2)>>curMin;
		ySlots = 1<<curMin;

		// would exceed max texture size
		if (xSlots*slotWidth  > VEGARendererBackend::maxBitmapSide() ||
		    ySlots*slotHeight > VEGARendererBackend::maxBitmapSide())
#ifdef SVG_PATH_TO_VEGAPATH
		{
			// will fall back to draw using outline - see forceOutlinePath
			// there's no glyph cache, but getGlyph must still work
			cachedGlyphs = OP_NEWA(VEGAGlyph, 1);
			RETURN_OOM_IF_NULL(cachedGlyphs);
			cachedGlyphs[0].glyph = 0;
			return OpStatus::OK;
		}
#else  // SVG_PATH_TO_VEGAPATH
			// no fallback available
			return OpStatus::ERR;
#endif // SVG_PATH_TO_VEGAPATH

		UINT32 bufSize = slotWidth*slotHeight*xSlots*ySlots;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
		if (UseSubpixelRendering())
			bufSize *= 4;
#endif
		bitmapCache = OP_NEWA(UINT8, bufSize);
		if (!bitmapCache)
			return OpStatus::ERR_NO_MEMORY;

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
		if (UseGlyphInterpolation())
		{
			bitmapCache2 = OP_NEWA(UINT8, bufSize);
			if (!bitmapCache2)
				return OpStatus::ERR_NO_MEMORY;
		}
#endif
	}
#endif // VEGA_3DDEVICE

	// try to find an initial cache size that doesn't exceed limit by too much
	const unsigned h2 = Height() * Height(); // approximation of glyph bitmap
	while (cacheSize && GetGlyphCacheMemoryOverhead(cacheSize) + cacheSize * h2 > PRIMARY_GLYPH_CACHE_MEM_LIMIT)
		cacheSize /= 2;

	if (!cacheSize)
	{
#ifdef SVG_PATH_TO_VEGAPATH
		// will fall back to draw using outline - see forceOutlinePath
		// there's no glyph cache, but getGlyph must still work
		cachedGlyphs = OP_NEWA(VEGAGlyph, 1);
		RETURN_OOM_IF_NULL(cachedGlyphs);
		cachedGlyphs[0].glyph = 0;
		return OpStatus::OK;
#else  // SVG_PATH_TO_VEGAPATH
		cacheSize = 1;
#endif // SVG_PATH_TO_VEGAPATH
	}

	primaryCache = OP_NEWA(short, VEGA_GLYPH_CACHE_HASH_SIZE);
	secondaryCache = OP_NEWA(short, VEGA_GLYPH_CACHE_HASH_SIZE);
	if (!primaryCache || !secondaryCache)
		return OpStatus::ERR_NO_MEMORY;

	cachedGlyphs = OP_NEWA(VEGAGlyph, cacheSize*2);
	if (!cachedGlyphs)
		return OpStatus::ERR_NO_MEMORY;

	for (i = 0; i < cacheSize*2-1; ++i)
		cachedGlyphs[i].nextGlyph = i+1;
	cachedGlyphs[cacheSize*2-1].nextGlyph = -1;
	availableGlyphs = 0;
	primaryCacheSize = 0;
	primaryCacheMemSize = GetGlyphCacheMemoryOverhead(cacheSize);
	for (i = 0; i < VEGA_GLYPH_CACHE_HASH_SIZE; ++i)
	{
		primaryCache[i] = -1;
		secondaryCache[i] = -1;
	}

	return OpStatus::OK;
}

void VEGAFont::GrowGlyphCacheIfNeeded()
{
#ifdef VEGA_3DDEVICE
	// TODO: Have to recreate bitmap cache as well in order to support this
	if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D)
		return;
#endif // VEGA_3DDEVICE

	// Double the glyph cache entry size
	int newCacheSize = cacheSize*2;

	// Only grow glyph cache if memory limit is not reached yet,
	// when the memory overhead of increased cache size is taken into account.
	if (primaryCacheMemSize + GetGlyphCacheMemoryOverhead(newCacheSize - cacheSize) > PRIMARY_GLYPH_CACHE_MEM_LIMIT)
		return;

	VEGAGlyph* newCachedGlyphs = OP_NEWA(VEGAGlyph, newCacheSize*2);
	if (!newCachedGlyphs)
		return;

	// Copy old glyph cache into new cache
	op_memcpy(newCachedGlyphs, cachedGlyphs, GetGlyphCacheMemoryOverhead(cacheSize)*2);

	// Initialize linked list of newly allocated part of cache
	for (int i = cacheSize*2; i < newCacheSize*2-1; ++i)
		newCachedGlyphs[i].nextGlyph = i+1;

	// "availableGlyphs" points to the first glyph in a linked list of
	// available glyphs. The newly allocated glyph entries should be
	// included in this list, and the fastest way to do this 
	// is to prepend the new entries to the existing list. 

	// Connect the last of the new glyph entries to the current list of available glyphs
	newCachedGlyphs[newCacheSize*2-1].nextGlyph = availableGlyphs;

	// Update the availableGlyphs pointer to point at the first new glyph entry.
	availableGlyphs = cacheSize*2;

	OP_DELETEA(cachedGlyphs);
	cacheSize = newCacheSize;
	cachedGlyphs = newCachedGlyphs;
}

#ifdef VEGA_3DDEVICE
void VEGAFont::OnContextLost()
{}

void VEGAFont::OnContextRestored()
{
	hwDataDirty = true;
}

VEGA3dTexture* VEGAFont::getTexture(VEGA3dDevice* dev, bool updateTexture
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
	, bool blackOnWhite
#endif
	)
{
	if (!texture)
	{
		VEGA3dTexture::ColorFormat fmt = VEGA3dTexture::FORMAT_ALPHA8;
		bool asRendertarget = false;

		// FIXME: set some hint that these change frequently
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
		if (UseSubpixelRendering())
		{
			fmt = VEGA3dTexture::FORMAT_RGBA8888;
			asRendertarget = true;
		}
#endif
		if (OpStatus::IsError(dev->createTexture(&texture, slotWidth*xSlots, slotHeight*ySlots, fmt, false, asRendertarget)))
			return NULL;

		// prevent odd pixels caused by adaptive AA
		RETURN_VALUE_IF_ERROR(texture->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST), NULL);

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
		OP_ASSERT(!texture2);
		if (UseGlyphInterpolation())
		{
			if (OpStatus::IsSuccess(dev->createTexture(&texture2, slotWidth*xSlots, slotHeight*ySlots, fmt, false, asRendertarget)))
				if (OpStatus::IsError(texture2->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST)))
				{
					VEGARefCount::DecRef(texture2);
					texture2 = NULL;
				}
		}
#endif

		dev->RegisterContextLostListener(this);
		hwDataDirty = true;
	}

	if (hwDataDirty && updateTexture)
	{
		RETURN_VALUE_IF_ERROR(syncHWCache(), NULL);
		hwDataDirty = false;
	}

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
	return blackOnWhite ? texture : texture2;
#else
	return texture;
#endif
}

OP_STATUS VEGAFont::syncHWCache()
{
	if (texture)
	{
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
		if (UseSubpixelRendering())
		{
			VEGASWBuffer buf;
			VEGAPixelStore ps;
			ps.buffer = bitmapCache;
			ps.width = slotWidth*xSlots;
			ps.height = slotHeight*ySlots;
#ifdef VEGA_INTERNAL_FORMAT_RGBA8888
			ps.format = VPSF_RGBA8888;
#else
			ps.format = VPSF_BGRA8888;
#endif
			ps.stride = ps.width*4;
			RETURN_IF_ERROR(buf.Bind(&ps));
			texture->update(0, 0, &buf);
			buf.Unbind(&ps);

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
			if (texture2)
			{
				ps.buffer = bitmapCache2;
				RETURN_IF_ERROR(buf.Bind(&ps));
				texture2->update(0, 0, &buf);
				buf.Unbind(&ps);
			}
#endif
		}
		else
#endif
			texture->update(0, 0, slotWidth*xSlots, slotHeight*ySlots, bitmapCache);
	}
	return OpStatus::OK;
}

OP_STATUS VEGAFont::getGlyph(UnicodePoint c, VEGAGlyph& glyph, unsigned int& imgX, unsigned int& imgY, BOOL isIndex)
{
	short gnum;
	RETURN_IF_ERROR(getGlyph(c, gnum, isIndex));
	glyph = cachedGlyphs[gnum];
	imgX = (gnum%xSlots)*slotWidth;
	imgY = (gnum/xSlots)*slotHeight;

	return OpStatus::OK;
}

void VEGAFont::setBatchingBackend(VEGABackend_HW3D* backend)
{
	if (batchingBackend == backend)
		return;
	if (batchingBackend && backend)
		batchingBackend->flushBatches();
	batchingBackend = backend;
	remainingBatchFlushes = 1;
}

#endif // VEGA_3DDEVICE

OP_STATUS VEGAFont::getGlyph(UnicodePoint c, VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride, BOOL isIndex)
{
	short gnum;
	RETURN_IF_ERROR(getGlyph(c, gnum, isIndex));
	glyph = cachedGlyphs[gnum];

	getGlyphBuffer(glyph, buffer, stride);
	return OpStatus::OK;
}

OP_STATUS VEGAFont::getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex)
{
	short gnum;
	RETURN_IF_ERROR(getGlyph(c, gnum, isIndex));
	glyph = cachedGlyphs[gnum];

	return OpStatus::OK;
}

void VEGAFont::clearCache(short* cache)
{
#ifdef SVG_PATH_TO_VEGAPATH
	OP_ASSERT(primaryCache);
#endif // SVG_PATH_TO_VEGAPATH

	// Clear cache
	for (unsigned int slot = 0; slot < VEGA_GLYPH_CACHE_HASH_SIZE; ++slot)
	{
		if (cache[slot] >= 0)
		{
			short last = cache[slot];
			while (cachedGlyphs[last].nextGlyph >= 0)
			{
				unloadGlyph(cachedGlyphs[last]);
				last = cachedGlyphs[last].nextGlyph;
			}

			unloadGlyph(cachedGlyphs[last]);
			cachedGlyphs[last].nextGlyph = availableGlyphs;
			availableGlyphs = cache[slot];
			cache[slot] = -1;
		}
	}
}

OP_STATUS VEGAFont::getGlyph(UnicodePoint c, short &gl, BOOL isIndex)
{
	gl = 0;

#ifdef SVG_PATH_TO_VEGAPATH
	// there's no glyph cache, but getGlyph must still work
	if (!primaryCache)
	{
		// requested glyph already cached
		if (cachedGlyphs[0].glyph == c && cachedGlyphs[0].isIndex == isIndex)
			return OpStatus::OK;

		// this assumes the bitmap is never needed, which it shouldn't be
		VEGAGlyph tmp;
		tmp.glyph = c;
		tmp.isIndex = isIndex;
		RETURN_IF_ERROR(loadGlyph(tmp, 0, 0, isIndex));
		op_memcpy(cachedGlyphs, &tmp, sizeof(tmp));
		cachedGlyphs[0].m_handle = NULL;
		unloadGlyph(tmp);

		return OpStatus::OK;
	}
#endif // SVG_PATH_TO_VEGAPATH

	short glyphSlot;
	// Look for glyph in primary cache
	glyphSlot = primaryCache[c%VEGA_GLYPH_CACHE_HASH_SIZE];
	while (glyphSlot >= 0)
	{
		if (cachedGlyphs[glyphSlot].glyph == c && cachedGlyphs[glyphSlot].isIndex == isIndex)
		{
			gl = glyphSlot;
			return OpStatus::OK;
		}
		glyphSlot = cachedGlyphs[glyphSlot].nextGlyph;
	}
	// if failed, look in secondary
	glyphSlot = secondaryCache[c%VEGA_GLYPH_CACHE_HASH_SIZE];
	short* nextp = &secondaryCache[c%VEGA_GLYPH_CACHE_HASH_SIZE];
	while (glyphSlot >= 0)
	{
		// Just break since we need to move the glyph to the primary cache,
		// which might require swapping caches and such
		if (cachedGlyphs[glyphSlot].glyph == c && cachedGlyphs[glyphSlot].isIndex == isIndex)
		{
			// Remove the glyph from the secondary cache
			*nextp = cachedGlyphs[glyphSlot].nextGlyph;
			break;
		}
		nextp = &cachedGlyphs[glyphSlot].nextGlyph;
		glyphSlot = cachedGlyphs[glyphSlot].nextGlyph;
	}
	// when reaching this point it is clear that something needs to be added to the primary cache
	BOOL checkMemoryLimit = TRUE;
#ifdef VEGA_3DDEVICE
	if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D)
		checkMemoryLimit = FALSE;
#endif

	if (primaryCacheSize >= cacheSize || (checkMemoryLimit && primaryCacheMemSize > PRIMARY_GLYPH_CACHE_MEM_LIMIT))
	{
#ifdef VEGA_3DDEVICE
		if (batchingBackend)
		{
			if (remainingBatchFlushes-- == 0)
			{
				batchingBackend->flushBatches();
			}
		}
#endif // VEGA_3DDEVICE

		// Clear secondary cache
		clearCache(secondaryCache);
		// Swap primary and secondary cache
		short* temp = primaryCache;
		primaryCache = secondaryCache;
		secondaryCache = temp;
		primaryCacheSize = 0;
		GrowGlyphCacheIfNeeded();
		primaryCacheMemSize = GetGlyphCacheMemoryOverhead(cacheSize);
	}
	// if failed, load the glyph
	if (glyphSlot < 0)
	{
		// load the glyph
		OP_ASSERT(availableGlyphs >= 0);
		glyphSlot = availableGlyphs;
		if (glyphSlot < 0)
			return OpStatus::ERR;

		VEGAGlyph tmp;
		tmp.glyph = c;
		tmp.isIndex = isIndex;

#ifdef VEGA_3DDEVICE
		// FIXME: Allow 2D-backends to use this cache type
		if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D)
		{
			unsigned int ys = glyphSlot/xSlots;
			unsigned int xs = glyphSlot%xSlots;

			unsigned int stride = xSlots*slotWidth;
			unsigned int targetOffset = ys*slotWidth*slotHeight*xSlots + xs*slotWidth;
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
			if (UseSubpixelRendering())
			{
				targetOffset *= 4;
				stride *= 4;
			}
#endif
			UINT8* target = bitmapCache + targetOffset;

			OP_ASSERT((target == 0) == (stride == 0));
			RETURN_IF_ERROR(loadGlyph(tmp, target, stride, isIndex));

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
			if (UseGlyphInterpolation())
				RETURN_IF_ERROR(loadGlyph(tmp, bitmapCache2 + targetOffset, stride, isIndex, FALSE));
#endif
			hwDataDirty = true;
		}
		else
#endif // VEGA_3DDEVICE
		RETURN_IF_ERROR(loadGlyph(tmp, 0, 0, isIndex));

		availableGlyphs = cachedGlyphs[glyphSlot].nextGlyph;
		cachedGlyphs[glyphSlot] = tmp;
	}
	// Insert the glyph in the primary cache
	cachedGlyphs[glyphSlot].nextGlyph = primaryCache[c%VEGA_GLYPH_CACHE_HASH_SIZE];
	primaryCache[c%VEGA_GLYPH_CACHE_HASH_SIZE] = glyphSlot;
	++primaryCacheSize;

	primaryCacheMemSize += cachedGlyphs[glyphSlot].width * cachedGlyphs[glyphSlot].height;

	gl = glyphSlot;
	return OpStatus::OK;
}

VEGAOpFont::VEGAOpFont(VEGAFont* fnt, OpFontInfo::FontType type)
	: fontImpl(fnt), ave_char_width(0), type(type)
{}

VEGAOpFont::~VEGAOpFont()
{
	OP_DELETE(fontImpl);
}

UINT32 VEGAOpFont::AveCharWidth()
{
	if (!ave_char_width)
		ave_char_width = StringWidth(UNI_L("x"), 1, 0);
	return ave_char_width;
}

#ifdef OPFONT_GLYPH_PROPS
OP_STATUS VEGAOpFont::GetGlyphProps(const UnicodePoint up, GlyphProps* props)
{
	VEGAFont::VEGAGlyph glyph;
	RETURN_IF_ERROR(fontImpl->getGlyph(up, glyph));
	props->left = glyph.left;
	props->top = glyph.top;
	props->width = glyph.width;
	props->height = glyph.height;
	props->advance = glyph.advance;
	return OpStatus::OK;
}
#endif // OPFONT_GLYPH_PROPS

UINT32 VEGAOpFont::StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing)
{
#ifdef VEGA_NATIVE_FONT_SUPPORT
	if (fontImpl->nativeRendering())
		return fontImpl->StringWidth(str, len, NULL, extra_char_spacing);
#endif // VEGA_NATIVE_FONT_SUPPORT
	ProcessedString processed_string;
	// FIXME:OOM
	if (OpStatus::IsError(fontImpl->ProcessString(&processed_string, str, len, extra_char_spacing)))
		return 0;
	return processed_string.m_advance;
}

UINT32 VEGAOpFont::StringWidth(const uni_char* str, UINT32 len,
							   OpPainter* painter, INT32 extra_char_spacing)
{
#ifdef VEGA_NATIVE_FONT_SUPPORT
	if (fontImpl->nativeRendering())
		return fontImpl->StringWidth(str, len, painter, extra_char_spacing);
#endif // VEGA_NATIVE_FONT_SUPPORT
	return StringWidth(str, len, extra_char_spacing);
}

VEGAOpFontManager::VEGAOpFontManager()
{
	m_default_fonts.default_font = NULL;
	m_default_fonts.serif_font = NULL;
	m_default_fonts.sans_serif_font = NULL;
	m_default_fonts.cursive_font = NULL;
	m_default_fonts.fantasy_font = NULL;
	m_default_fonts.monospace_font = NULL;
}

VEGAOpFontManager::~VEGAOpFontManager()
{
	op_free(m_default_fonts.default_font);
	op_free(m_default_fonts.serif_font);
	op_free(m_default_fonts.sans_serif_font);
	op_free(m_default_fonts.cursive_font);
	op_free(m_default_fonts.fantasy_font);
	op_free(m_default_fonts.monospace_font);
}

OP_STATUS VEGAOpFontManager::SetDefaultFonts(DefaultFonts* fnts)
{
	op_free(m_default_fonts.default_font);
	m_default_fonts.default_font = NULL;
	if (fnts->default_font)
	{
		m_default_fonts.default_font = uni_strdup(fnts->default_font);
		if (!m_default_fonts.default_font)
			return OpStatus::ERR_NO_MEMORY;
	}
	op_free(m_default_fonts.serif_font);
	m_default_fonts.serif_font = NULL;
	if (fnts->serif_font)
	{
		m_default_fonts.serif_font = uni_strdup(fnts->serif_font);
		if (!m_default_fonts.serif_font)
			return OpStatus::ERR_NO_MEMORY;
	}
	op_free(m_default_fonts.sans_serif_font);
	m_default_fonts.sans_serif_font = NULL;
	if (fnts->sans_serif_font)
	{
		m_default_fonts.sans_serif_font = uni_strdup(fnts->sans_serif_font);
		if (!m_default_fonts.sans_serif_font)
			return OpStatus::ERR_NO_MEMORY;
	}
	op_free(m_default_fonts.cursive_font);
	m_default_fonts.cursive_font = NULL;
	if (fnts->cursive_font)
	{
		m_default_fonts.cursive_font = uni_strdup(fnts->cursive_font);
		if (!m_default_fonts.cursive_font)
			return OpStatus::ERR_NO_MEMORY;
	}
	op_free(m_default_fonts.fantasy_font);
	m_default_fonts.fantasy_font = NULL;
	if (fnts->fantasy_font)
	{
		m_default_fonts.fantasy_font = uni_strdup(fnts->fantasy_font);
		if (!m_default_fonts.fantasy_font)
			return OpStatus::ERR_NO_MEMORY;
	}
	op_free(m_default_fonts.monospace_font);
	m_default_fonts.monospace_font = NULL;
	if (fnts->monospace_font)
	{
		m_default_fonts.monospace_font = uni_strdup(fnts->monospace_font);
		if (!m_default_fonts.monospace_font)
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpFont* VEGAOpFontManager::CreateFont(const uni_char* face, UINT32 size, UINT8 weight,
									  BOOL italic, BOOL must_have_getoutline, INT32 blur_radius)
{
	VEGAFont* fnt = GetVegaFont(face, size, weight, italic, must_have_getoutline, blur_radius);
	if (!fnt)
		return NULL;

	VEGAOpFont* opfnt  = OP_NEW(VEGAOpFont, (fnt));
	if (!opfnt)
	{
		OP_DELETE(fnt);
		return NULL;
	}
	return opfnt;
}

OpFont* VEGAOpFontManager::CreateFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius)
{
	VEGAFont* fnt = GetVegaFont(webfont, size, blur_radius);
	if (!fnt)
		return NULL;

	OpFontInfo::FontType type = GetWebFontType(webfont);
	
	VEGAOpFont* opfnt  = OP_NEW(VEGAOpFont, (fnt, type));
	if (!opfnt)
	{
		OP_DELETE(fnt);
		return NULL;
	}

	return opfnt;
}
OpFont* VEGAOpFontManager::CreateFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius)
{
	VEGAFont* fnt = GetVegaFont(webfont, weight, italic, size, blur_radius);
	if (!fnt)
		return NULL;

	OpFontInfo::FontType type = GetWebFontType(webfont);
	
	VEGAOpFont* opfnt  = OP_NEW(VEGAOpFont, (fnt, type));
	if (!opfnt)
	{
		OP_DELETE(fnt);
		return NULL;
	}

	return opfnt;
}

#ifndef PERSCRIPT_GENERIC_FONT
const uni_char* VEGAOpFontManager::GetGenericFontName(GenericFont generic_font)
{
	const uni_char* name;
	switch(generic_font)
	{
	case GENERIC_FONT_SERIF:
		name = m_default_fonts.serif_font;
		break;
	case GENERIC_FONT_SANSSERIF:
		name = m_default_fonts.sans_serif_font;
		break;
	case GENERIC_FONT_CURSIVE:
		name = m_default_fonts.cursive_font;
		break;
	case GENERIC_FONT_FANTASY:
		name = m_default_fonts.fantasy_font;
		break;
	case GENERIC_FONT_MONOSPACE:
		name = m_default_fonts.monospace_font;
		break;
	default:
		name = m_default_fonts.default_font;
	}
	// font doesn't exist - get fallback from StyleManager
	if (styleManager && styleManager->GetFontNumber(name) < 0)
	{
		StyleManager::GenericFont id = StyleManager::GetGenericFont(generic_font);
		if (id == StyleManager::UNKNOWN) // what does default mean? i guess serif's as good as anything ...
			id = StyleManager::SERIF;
		short num = styleManager->GetGenericFontNumber(id, WritingSystem::Unknown);
		OP_ASSERT(num >= 0);
		name = styleManager->GetFontFace(num);
	}
	return name;
}
#endif // !PERSCRIPT_GENERIC_FONT

#ifdef HAS_FONT_FOUNDRY
#error
OP_STATUS VEGAOpFontManager::FindBestFontName(const uni_char *name, OpString &full_name)
{
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}
OP_STATUS VEGAOpFontManager::GetFoundry(const uni_char *font, OpString &foundry)
{
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}
OP_STATUS VEGAOpFontManager::GetFamily(const uni_char *font, OpString &family)
{
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}
OP_STATUS VEGAOpFontManager::SetPreferredFoundry(const uni_char *foundry)
{
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}
#endif // HAS_FONT_FOUNDRY
#ifdef PLATFORM_FONTSWITCHING
#error
void VEGAOpFontManager::SetPreferredFont(UnicodePoint ch, bool monospace, const uni_char *font, OpFontInfo::CodePage codepage=OpFontInfo::OP_DEFAULT_CODEPAGE)
{
	OP_ASSERT(FALSE);
}
#endif // PLATFORM_FONTSWITCHING

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT

