/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGA_MDF_FONT_H
#define VEGA_MDF_FONT_H

#ifdef MDEFONT_MODULE

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_MDEFONT_SUPPORT)

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/mdefont/mdefont.h"

struct MDE_FONT;

class VEGAMDEFont : public VEGAFont
{
	friend class VEGAMDFOpFontManager;
public:
	~VEGAMDEFont();
	static OP_STATUS Create(VEGAMDEFont** font, int font_nr, int size, BOOL bold, BOOL italic);

	// Functions to be implemented by the font engine
	virtual UINT32 Ascent();
	virtual UINT32 Descent();
	virtual UINT32 InternalLeading();
	virtual UINT32 Height();
	virtual UINT32 Overhang();
	virtual UINT32 MaxAdvance();
	virtual UINT32 ExtraPadding();
	virtual int getBlurRadius(){return m_blurRadius;}

#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len,
								 UINT32& io_str_pos, UINT32 in_last_str_pos,
								 BOOL in_writing_direction_horizontal,
								 SVGNumber& out_advance, SVGPath** out_glyph);
#endif // SVG_SUPPORT

#ifdef OPFONT_FONT_DATA
	virtual OP_STATUS GetFontData(UINT8*& font_data, UINT32& data_size);
	virtual OP_STATUS ReleaseFontData(UINT8* font_data);
#endif // OPFONT_FONT_DATA

	OP_STATUS ProcessString(ProcessedString* processed_string,
							const uni_char* str, const size_t len,
							INT32 extra_char_spacing, short word_width,
							bool use_glyph_indices);
	
	const uni_char* getFontName();

	virtual BOOL isBold();
	virtual BOOL isItalic();

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	virtual bool UseSubpixelRendering();
#endif // VEGA_SUBPIXEL_FONT_BLENDING
protected:
	virtual OP_STATUS loadGlyph(VEGAGlyph& glyph, UINT8* data, unsigned int stride, BOOL isIndex = FALSE);
	virtual void unloadGlyph(VEGAGlyph& glyph);
	virtual void getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride);

private:
	OP_STATUS blurGlyph(UINT8* src, UINT8* dst, unsigned int srcstride, unsigned int dststride, unsigned int srcw, unsigned int srch);
	VEGAMDEFont(MDE_FONT* mdefont, INT32 blurRadius);
	MDE_FONT* m_mdefont;
	INT32 m_blurRadius;
	VEGA_FIX* m_blurKernel;
	VEGA_FIX* m_blurTemp;
	unsigned int m_blurTempSize;
};
# ifdef MDF_FONT_ADVANCE_CACHE
inline OP_STATUS VEGAMDEFont::ProcessString(ProcessedString* processed_string,
									 const uni_char* str, const size_t len,
									 INT32 extra_char_spacing, short word_width, 
									 bool use_glyph_indices)
{
	OP_ASSERT(processed_string);
	return MDF_ProcessString(m_mdefont, *processed_string, str, len, extra_char_spacing, word_width,
				use_glyph_indices ? MDF_PROCESS_FLAG_USE_GLYPH_INDICES : MDF_PROCESS_FLAG_NONE);
}
# endif // MDF_FONT_ADVANCE_CACHE


class VEGAMDFOpFontManager : public VEGAOpFontManager
{
public:
	VEGAMDFOpFontManager();

	OP_STATUS Construct();

	virtual UINT32 CountFonts();
	virtual OP_STATUS GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo);

	virtual OP_STATUS GetLocalFont(OpWebFontRef& localfont, const uni_char* facename);
	virtual BOOL SupportsFormat(int format);
	virtual OP_STATUS AddWebFont(OpWebFontRef& webfont, const uni_char* full_path_of_file);
	virtual OP_STATUS RemoveWebFont(OpWebFontRef webfont);
	virtual OP_STATUS GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo);
	virtual VEGAFont* GetVegaFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius);
	virtual VEGAFont* GetVegaFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius);
	virtual OpFontInfo::FontType GetWebFontType(OpWebFontRef webfont);

#ifdef _GLYPHTESTING_SUPPORT_
	virtual void UpdateGlyphMask(OpFontInfo *fontinfo);
#endif // _GLYPHTESTING_SUPPORT_

	virtual OP_STATUS BeginEnumeration();
	virtual OP_STATUS EndEnumeration();

#ifdef PERSCRIPT_GENERIC_FONT
	OP_STATUS SetGenericFonts(const DefaultFonts& fonts, WritingSystem::Script script);
	virtual const uni_char* GetGenericFontName(GenericFont generic_font, WritingSystem::Script script);
#endif // PERSCRIPT_GENERIC_FONT

	virtual void BeforeStyleInit(class StyleManager* styl_man) {}

	virtual VEGAFont* GetVegaFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius);

private:
#ifdef PERSCRIPT_GENERIC_FONT
	OP_STATUS InitGenericFont(const DefaultFonts& fonts);
#endif
	VEGAFont* GetVegaFontInt(MDE_FONT* mdefont, UINT32 size, INT32 blur_radius);
	OP_STATUS GetFontInfoInternal(const MDF_FONTINFO& mdf_info, OpFontInfo* fontinfo);

private:
#ifdef PERSCRIPT_GENERIC_FONT
	OpAutoVector<OpString> m_serif_fonts;
	OpAutoVector<OpString> m_sansserif_fonts;
	OpAutoVector<OpString> m_cursive_fonts;
	OpAutoVector<OpString> m_fantasy_fonts;
	OpAutoVector<OpString> m_monospace_fonts;
#endif
};

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && VEGA_MDEFONT_SUPPORT

#endif // MDEFONT_MODULE

#endif // VEGA_MDF_FONT_H
