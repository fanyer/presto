/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef GDIOPFONT_H
#define GDIOPFONT_H

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include <usp10.h>

class GdiOpFontManager;


class GdiOpFont : 
# ifdef VEGA_GDIFONT_SUPPORT
	public VEGAFont
# else
	public VEGAOpFont
# endif
{

public:

	friend GdiOpFontManager;

	GdiOpFont() {};
	GdiOpFont(GdiOpFontManager* font_manager, const uni_char *face, UINT32 size, UINT8 weight, BOOL italic, INT32 blur_radius);
	virtual ~GdiOpFont();

	// Returns the loading status of the font. Can return OK, ERR, or ERR_NO_MEMORY depending on how it went loading it.
	OP_STATUS GetStatus() const { return m_status; };

	// Sets font on HDC. Remember to save the return value, and set it back on the HDC when done.
	HFONT SetFontOnHDC(HDC hdc) const { return (HFONT)SelectObject( hdc, m_fnt); };

	UINT32 Ascent() { return m_ascent; }

	UINT32 Descent() { return m_descent; }

	UINT32 InternalLeading() { return m_internal_leading; }

	UINT32 Height() { return m_height; }

	UINT32 AveCharWidth() { return m_ave_charwidth; }

	UINT32 Overhang() { return m_overhang; }

	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing);

#ifdef VEGA_GDIFONT_SUPPORT
# ifdef OPFONT_GLYPH_PROPS
	virtual OP_STATUS getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex = FALSE);
# endif // OPFONT_GLYPH_PROPS
#endif // VEGA_GDIFONT_SUPPORT

#ifdef SVG_SUPPORT
	virtual OP_STATUS GdiOpFont::GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,	
												 BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph);
#endif // SVG_SUPPORT

	BOOL IsVectorFont() { return m_vectorfont; }

# ifdef VEGA_GDIFONT_SUPPORT
	virtual UINT32 MaxAdvance(){return m_max_advance;}

	virtual BOOL StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_space, UINT32* strlen);

#ifdef MDF_CAP_PROCESSEDSTRING
	virtual OP_STATUS ProcessString(struct ProcessedString* processed_string,
									const uni_char* str, const size_t len,
									INT32 extra_char_spacing = 0, short word_width = -1,
									bool use_glyph_indices = false);
#endif // MDF_CAP_PROCESSEDSTRING

	virtual BOOL isBold() { return m_weight >= 6; }
	virtual BOOL isItalic() { return m_italic; }
	virtual int getBlurRadius() { return m_blur_radius; }

protected:
	virtual OP_STATUS loadGlyph(VEGAGlyph& glyph, UINT8* data, unsigned int stride, BOOL isIndex = FALSE, BOOL blackOnWhite = TRUE);
	virtual void unloadGlyph(VEGAGlyph& glyph);
	virtual void getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride);
	virtual bool UseGlyphInterpolation(){return !!IsVectorFont();}
private:
	int m_max_advance;
# endif
	virtual BOOL nativeRendering();
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGAWindow* dest);
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGASWBuffer* dest);
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGA3dRenderTarget* dest, bool isWindow);
	virtual BOOL GetAlphaMask(const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, const UINT8** mask, OpPoint* size, unsigned int* rowStride, unsigned int* pixelStride);

private:

	// variables to make a font use only one HDC
	static GdiOpFont* s_font_on_dc;
	static HDC s_fonthdc;

	static HBITMAP s_text_bitmap;
	static HBITMAP s_old_text_bitmap;
	static HDC s_text_bitmap_dc;
	static UINT8* s_text_bitmap_data;
	static LONG s_text_bitmap_width;
	static LONG s_text_bitmap_height;

	OP_STATUS m_status;
	HFONT m_fnt;

	HBITMAP m_bmp;
	UINT32* m_bmp_data;

	HGDIOBJ m_oldGdiObj;
	SCRIPT_CACHE vega_script_cache;

	UINT32 m_ascent;
	UINT32 m_descent;
	UINT32 m_internal_leading;
	UINT32 m_height;
	UINT32 m_ave_charwidth;
	UINT32 m_overhang;

	BOOL m_italic;
	INT32  m_blur_radius;
	BOOL m_smallcaps;
	UINT8 m_weight;
	BOOL m_vectorfont;

	GdiOpFontManager* m_font_manager;
};

#endif // GDIOPFONT_H