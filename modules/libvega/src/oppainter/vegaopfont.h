/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAOPFONT_H
#define VEGAOPFONT_H

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)

#ifdef VEGA_NATIVE_FONT_SUPPORT
class VEGASWBuffer;
#ifdef VEGA_USE_HW
class VEGA3dRenderTarget;
#endif // VEGA_USE_HW
#endif
#ifdef VEGA_3DDEVICE
#include "modules/libvega/src/vegabackend_hw3d.h"
#endif // VEGA_3DDEVICE

class VEGAFont
#ifdef VEGA_3DDEVICE
	:public VEGA3dContextLostListener
#endif
{
public:
	VEGAFont();
	virtual ~VEGAFont();

	virtual OP_STATUS Construct();

	// Functions to be implemented by the font engine
	virtual UINT32 Ascent() = 0;
	virtual UINT32 Descent() = 0;
	virtual UINT32 InternalLeading() = 0;
	virtual UINT32 Height() = 0;
	virtual UINT32 Overhang() = 0;
	virtual UINT32 MaxAdvance() = 0;
	virtual UINT32 ExtraPadding() { return 0; }

#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len,
								 UINT32& io_str_pos, UINT32 in_last_str_pos,
								 BOOL in_writing_direction_horizontal,
								 SVGNumber& out_advance, SVGPath** out_glyph) = 0;
#endif // SVG_SUPPORT


#ifdef OPFONT_FONT_DATA
	virtual OP_STATUS GetFontData(UINT8*& font_data, UINT32& data_size) = 0;
	virtual OP_STATUS ReleaseFontData(UINT8* font_data) = 0;
#endif // OPFONT_FONT_DATA

	virtual const uni_char* getFontName()
	{
		return 0;
	}

	virtual BOOL isBold() = 0;
	virtual BOOL isItalic() = 0;

#ifdef VEGA_3DDEVICE
	virtual void OnContextLost();
	virtual void OnContextRestored();
	// Add two to get compensate for hinting in both directions
	virtual UINT32 MaxGlyphHeight(){return Height()+2+ExtraPadding();}
	virtual UINT32 MaxGlyphWidth(){return (MaxAdvance() ? (MaxAdvance() + ExtraPadding()) : Height()*2) + 2;}
	virtual int getBlurRadius() = 0;

	unsigned int getSlotWidth(){return slotWidth;}
	unsigned int getSlotHeight(){return slotHeight;}

	class VEGA3dTexture* getTexture(class VEGA3dDevice* dev, bool updateTexture
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
		, bool blackOnWhite = true
#endif
		);

// 	UINT8* getBuffer(){return bitmapCache;}
// 	unsigned int getBufferStride(){return xSlots*slotWidth;}
#endif // VEGA_3DDEVICE

#if defined(VEGA_3DDEVICE) || defined(VEGA_2DDEVICE)
	unsigned int getGlyphCacheSize(){return cacheSize;}
#endif // VEGA_3DDEVICE || VEGA_2DDEVICE

#ifdef SVG_PATH_TO_VEGAPATH
	/** used to fallback to outline path when font size is too big for
	  * using a texture as glyph cache */
	virtual bool forceOutlinePath()
	{
		return
# ifdef VEGA_NATIVE_FONT_SUPPORT
			!nativeRendering() &&
# endif // VEGA_NATIVE_FONT_SUPPORT
			!primaryCache;
	}
#endif // SVG_PATH_TO_VEGAPATH

	/**
	   Doubles the number of glyph entries in cache, if memory limit
	   is not reached yet.
	 */
	void GrowGlyphCacheIfNeeded();

	unsigned int GetGlyphCacheMemoryOverhead(int cacheSize) { return cacheSize * sizeof(*cachedGlyphs); }

	/**
	   process string: apply advance, kerning etc. the struct
	   ProcessedString is located in modules/mdefont/processedstring.h
	   but may be used by implementations not using the mdefont
	   backend. look there for further documentation.
	 */
	virtual OP_STATUS ProcessString(struct ProcessedString* processed_string,
									const uni_char* str, const size_t len,
									INT32 extra_char_spacing = 0, short word_width = -1, 
									bool use_glyph_indices = false) = 0;

	struct VEGAGlyph
	{
		UnicodePoint glyph;

		short top;
		short left;
		short advance;

		short width;
		short height;

		BOOL isIndex;

		short nextGlyph;
		/**
		   handle to per-glyph allocated buffer - should be zero if
		   hardware allocation is used
		*/
		void* m_handle;
	};
#ifdef VEGA_3DDEVICE
	OP_STATUS getGlyph(UnicodePoint c, VEGAGlyph& glyph, unsigned int& imgX, unsigned int& imgY, BOOL isIndex = FALSE);
#endif // VEGA_3DDEVICE
	/**
	   to be used when per-glyph buffer allocation is used only
	   @param buffer a pointer to the beginning of this glyph's buffer data
	   @param stride the stride of the buffer, in bytes (normally same as glyph width)
	 */
	OP_STATUS getGlyph(UnicodePoint c, VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride, BOOL isIndex = FALSE);

	virtual OP_STATUS getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex = FALSE);

#ifdef VEGA_NATIVE_FONT_SUPPORT
	virtual BOOL nativeRendering() = 0;
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing) = 0;
	/** Draw a string to the specified destination window.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGAWindow* dest) = 0;
	/** Draw a string to the specified destination buffer.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGASWBuffer* dest) = 0;
#ifdef VEGA_3DDEVICE
	/** Draw a string to the specified destination 3d render target.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGA3dRenderTarget* dest, bool isWindow) = 0;
#endif // VEGA_3DDEVICE
	/** Draw a string to an 8 bit alpha mask owned by the font.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL GetAlphaMask(const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, const UINT8** mask, OpPoint* size, unsigned int* rowStride, unsigned int* pixelStride) = 0;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	/** Set the glyph scale before DrawString is called.
	 * @param scale the new scale, in percentage. */
	virtual int SetGlyphScale(int scale) = 0;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
#endif // VEGA_NATIVE_FONT_SUPPORT
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	virtual bool UseSubpixelRendering(){return true;}
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
	virtual bool UseGlyphInterpolation(){return false;}
#endif
#endif // VEGA_SUBPIXEL_FONT_BLENDING
#ifdef VEGA_3DDEVICE
	void setBatchingBackend(VEGABackend_HW3D* backend);
#endif // VEGA_3DDEVICE
protected:
	/**
	   loads a glyph. if data and stride are both 0, implementation
	   should allocate buffer storage for the glyph.
	   VEGAGlyph::m_handle can be used as a handle. cleanup should be
	   done in unloadGlyph.
	   NOTE: implementations that need to do per-glyph cleanup are
	   responsible for calling clearCaches from destructor. since
	   clearCache calls unloadGlyph, which is virtual, this cannot be
	   done from base destructor.

	   @param blackOnWhite only relevant when UseGlyphInterpolation
		      returns true, used to load 2 subtly different bitmaps
		      that will be blended using the target text color as
		      coefficient. This is used on Windows to improve cleartype.
	 */
	virtual OP_STATUS loadGlyph(VEGAGlyph& glyph, UINT8* data, unsigned int stride, BOOL isIndex = FALSE
#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
		, BOOL blackOnWhite = TRUE
#endif
		) = 0;
	/**
	   called when a glyph is no longer used. implemenation should
	   free any memory allocated when loading glyph here.
	 */
	virtual void unloadGlyph(VEGAGlyph& glyph) = 0;
	/**
	   used by loadGlyph, buffer and stride should be set up as
	   documented in loadGlyph.
	 */
	virtual void getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride) = 0;
	/**
	   should be called from destructor of inheriting class if
	   per-glyph buffer allocations has been done - see note in
	   loadGlyph.
	 */
	void clearCaches()
	{
#ifdef VEGA_3DDEVICE
		if (batchingBackend)
		{
			batchingBackend->flushBatches();
		}
#endif // VEGA_3DDEVICE
		if (!cachedGlyphs)
			return; // There are no glyphs to free

#ifdef SVG_PATH_TO_VEGAPATH
		if (!primaryCache)
		{
			cachedGlyphs[0].glyph = 0;
			return;
		}
#endif // SVG_PATH_TO_VEGAPATH

		clearCache(secondaryCache);
		clearCache(primaryCache);
	}
private:
	void clearCache(short* cache);
	OP_STATUS getGlyph(UnicodePoint c, short &gl, BOOL isIndex = FALSE);
#ifdef VEGA_3DDEVICE
	// texture for cache slots
	UINT8* bitmapCache;

	unsigned int slotWidth;
	unsigned int slotHeight;
	unsigned int xSlots;
	unsigned int ySlots;
#endif // VEGA_3DDEVICE
	// cache to keep track of which glyph is in which cache slot
	VEGAGlyph* cachedGlyphs;

	short* primaryCache;
	unsigned int primaryCacheSize;
	// Current amount of memory used by primary glyph cache, including array overhead
	unsigned int primaryCacheMemSize;
	short* secondaryCache;
	short availableGlyphs;

	unsigned int cacheSize;

#ifdef VEGA_3DDEVICE
	class VEGA3dTexture* texture;

#ifdef VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
	UINT8* bitmapCache2;
	class VEGA3dTexture* texture2;
#endif

	OP_STATUS syncHWCache();

	bool hwDataDirty;

	VEGABackend_HW3D* batchingBackend;
	unsigned int remainingBatchFlushes;
#endif // VEGA_3DDEVICE
};

class VEGAOpFont : public OpFont
{
public:
	VEGAOpFont(VEGAFont* fnt, OpFontInfo::FontType type = OpFontInfo::PLATFORM);
	virtual ~VEGAOpFont();

	virtual OpFontInfo::FontType Type() { return type; }
	virtual UINT32 Ascent(){return fontImpl->Ascent();}
	virtual UINT32 Descent(){return fontImpl->Descent();}
	virtual UINT32 InternalLeading(){return fontImpl->InternalLeading();}
	virtual UINT32 Height(){return fontImpl->Height();}
	virtual UINT32 Overhang(){return fontImpl->Overhang();}
	virtual VEGAFont* getVegaFont(){return fontImpl;}

	virtual UINT32 AveCharWidth();

	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing);
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing);
#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len,
								 UINT32& io_str_pos, UINT32 in_last_str_pos,
								 BOOL in_writing_direction_horizontal,
								 SVGNumber& out_advance, SVGPath** out_glyph)
	{
		return fontImpl->GetOutline(in_str, in_len, io_str_pos, in_last_str_pos,
									in_writing_direction_horizontal, out_advance, out_glyph);
	}
#endif // SVG_SUPPORT

#ifdef OPFONT_FONT_DATA
	virtual OP_STATUS GetFontData(UINT8*& font_data, UINT32& data_size)
	{
		return fontImpl->GetFontData(font_data, data_size);
	}

	virtual OP_STATUS ReleaseFontData(UINT8* font_data)
	{
		return fontImpl->ReleaseFontData(font_data);
	}
#endif // OPFONT_FONT_DATA

#ifdef OPFONT_GLYPH_PROPS
	OP_STATUS GetGlyphProps(const UnicodePoint up, GlyphProps* props);
#endif // OPFONT_GLYPH_PROPS

protected:
	VEGAFont* fontImpl;

private:
	int ave_char_width;
	OpFontInfo::FontType type;
};

class VEGAOpFontManager : public OpFontManager
{
public:

	struct DefaultFonts
	{
		uni_char* default_font;
		uni_char* serif_font;
		uni_char* sans_serif_font;
		uni_char* cursive_font;
		uni_char* fantasy_font;
		uni_char* monospace_font;
	};

	VEGAOpFontManager();
	virtual ~VEGAOpFontManager();

	OP_STATUS SetDefaultFonts(DefaultFonts* fnts);

	virtual OpFont* CreateFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius);
	virtual VEGAFont* GetVegaFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius) = 0;

	virtual OpFont* CreateFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius);
	virtual OpFont* CreateFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius);
	virtual VEGAFont* GetVegaFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius) = 0;
	virtual VEGAFont* GetVegaFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius) = 0;
	virtual OP_STATUS GetLocalFont(OpWebFontRef& localfont, const uni_char* facename) = 0;
	virtual OpFontInfo::FontType GetWebFontType(OpWebFontRef webfont) = 0;
	virtual BOOL SupportsFormat(int format) = 0;

	virtual UINT32 CountFonts() = 0;
	virtual OP_STATUS GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo) = 0;

	virtual OP_STATUS BeginEnumeration() = 0;
	virtual OP_STATUS EndEnumeration() = 0;

#ifdef PERSCRIPT_GENERIC_FONT
	virtual const uni_char* GetGenericFontName(GenericFont generic_font, WritingSystem::Script script) = 0;
#else // PERSCRIPT_GENERIC_FONT
	virtual const uni_char* GetGenericFontName(GenericFont generic_font);
#endif // PERSCRIPT_GENERIC_FONT

#ifdef HAS_FONT_FOUNDRY
	virtual OP_STATUS FindBestFontName(const uni_char *name, OpString &full_name);
	virtual OP_STATUS GetFoundry(const uni_char *font, OpString &foundry);
	virtual OP_STATUS GetFamily(const uni_char *font, OpString &family);
	virtual OP_STATUS SetPreferredFoundry(const uni_char *foundry);
#endif // HAS_FONT_FOUNDRY
#ifdef PLATFORM_FONTSWITCHING
	virtual void SetPreferredFont(UnicodePoint ch, bool monospace, const uni_char *font, OpFontInfo::CodePage codepage=OpFontInfo::OP_DEFAULT_CODEPAGE);
#endif // PLATFORM_FONTSWITCHING
#ifdef _GLYPHTESTING_SUPPORT_
	virtual void UpdateGlyphMask(OpFontInfo *fontinfo) = 0;
#endif // _GLYPHTESTING_SUPPORT_
private:
	DefaultFonts m_default_fonts;
};

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT
#endif // !VEGAOPFONT_H

