/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef MDEFONT_MODULE

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_MDEFONT_SUPPORT)

#include "modules/libvega/src/oppainter/vegamdefont.h"
#include "modules/style/css_webfont.h"
#include "modules/mdefont/mdefont.h"
#include "modules/libvega/src/vegafiltergaussian.h"


/*********************************************************************/
/*                             VEGAMDEFont                           */
/*********************************************************************/

VEGAMDEFont::VEGAMDEFont(MDE_FONT* mdffont, INT32 blurRadius) : m_mdefont(mdffont), m_blurRadius(blurRadius), m_blurKernel(NULL), m_blurTemp(NULL), m_blurTempSize(0)
{
	OP_ASSERT(m_mdefont);
}
VEGAMDEFont::~VEGAMDEFont()
{
	// call has to be made from inheriting class since clearCache
	// calls unloadGlyph, which is virtual ...
	clearCaches();
	MDF_ReleaseFont(m_mdefont);
	OP_DELETEA(m_blurKernel);
	OP_DELETEA(m_blurTemp);
}
UINT32 VEGAMDEFont::Ascent()
{
	return m_mdefont->ascent;
}
UINT32 VEGAMDEFont::Descent()
{
	return m_mdefont->descent;
}
UINT32 VEGAMDEFont::InternalLeading()
{
	return m_mdefont->internal_leading;
}
UINT32 VEGAMDEFont::Height()
{
	return m_mdefont->height;
}
UINT32 VEGAMDEFont::Overhang()
{
	return 0;
}
UINT32 VEGAMDEFont::MaxAdvance()
{
	return m_mdefont->max_advance;
}
UINT32 VEGAMDEFont::ExtraPadding()
{
	return m_mdefont->extra_padding;
}

#ifdef SVG_SUPPORT
OP_STATUS VEGAMDEFont::GetOutline(const uni_char* in_str, UINT32 in_len,
								  UINT32& io_str_pos, UINT32 in_last_str_pos,
								  BOOL in_writing_direction_horizontal,
								  SVGNumber& out_advance, SVGPath** out_glyph)
{
	return MDF_GetOutline(m_mdefont, in_str, in_len, io_str_pos, in_last_str_pos,
						  in_writing_direction_horizontal, out_advance, out_glyph);
}
#endif // SVG_SUPPORT

#ifdef OPFONT_FONT_DATA
OP_STATUS VEGAMDEFont::GetFontData(UINT8*& font_data, UINT32& data_size)
{
	// FIXME: Must check if it is portable that table 0 means complete font data
	return MDF_GetTable(m_mdefont, 0, (BYTE*&)font_data, data_size);
}

OP_STATUS VEGAMDEFont::ReleaseFontData(UINT8* font_data)
{
	MDF_FreeTable((BYTE*&)font_data);
	return OpStatus::OK;
}
#endif // OPFONT_FONT_DATA

OP_STATUS VEGAMDEFont::blurGlyph(UINT8* src, UINT8* dst, unsigned int srcstride, unsigned int dststride, unsigned int srcw, unsigned int srch)
{
	unsigned int dstw = srcw+m_blurRadius*2;
	unsigned int dsth = srch+m_blurRadius*2;

	if (!m_blurKernel)
	{
		int kern_size = 1+2*m_blurRadius;
		m_blurKernel = OP_NEWA(VEGA_FIX, kern_size);
		if (!m_blurKernel)
			return OpStatus::ERR_NO_MEMORY;
		VEGA_FIX stdDev = VEGA_FIXDIV2(VEGA_INTTOFIX(m_blurRadius));

		VEGAFilterGaussian::initKernel(stdDev, m_blurKernel, m_blurRadius, kern_size);
	}

	int src_bpp = 1;

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
	// If the underlying font engine uses subpixel rendering we'll
	// always get a 4-bpp target. Reconfiguing the font engine would
	// be another option but that could on the other hand affect other
	// properties of the rendering, so just use the "averaged" value
	// of the components below.
	if (g_mdf_engine->HasComponentRendering())
		src_bpp = 4;
#endif // VEGA_SUBPIXEL_FONT_BLENDING

	unsigned int tempSize = dstw*srch;
	if (tempSize / dstw != srch)
		return OpStatus::ERR;
	if (tempSize > m_blurTempSize)
	{
		VEGA_FIX* temp = OP_NEWA_WH(VEGA_FIX, dstw, srch);
		if (!temp)
			return OpStatus::ERR_NO_MEMORY;
		OP_DELETEA(m_blurTemp);
		m_blurTemp = temp;
		m_blurTempSize = tempSize;
	}
	unsigned int tempstride = dstw;
	int x, y;
	for (y = 0; y < (int)srch; ++y)
	{
		UINT8* horizSrc = src+y*srcstride*src_bpp;
		VEGA_FIX* tempLine = m_blurTemp+y*tempstride;
		for (x = 0; x < (int)dstw; ++x)
		{
			int srcx = x - m_blurRadius;
			VEGA_FIX sum = 0;
			int brMin = MAX(-m_blurRadius, -srcx);
			int brMax = MIN(m_blurRadius, (int)srcw-1-srcx);
			UINT8* srcCol = horizSrc + (srcx+brMin)*src_bpp;
			VEGA_FIX* kern = m_blurKernel + (m_blurRadius+brMin);
			for (int k_ofs = brMin; k_ofs <= brMax; ++k_ofs)
			{
				unsigned src_alpha;
				if (src_bpp == 4)
					src_alpha = *(UINT32*)srcCol >> 24;
				else
					src_alpha = *srcCol;

				sum += (*kern) * src_alpha;
				srcCol += src_bpp;
				++kern;
			}
			*tempLine = sum;
			++tempLine;
		}
	}
	for (y = 0; y < (int)dsth; ++y)
	{
		int srcy = y - m_blurRadius;
		int brMin = MAX(-m_blurRadius, -srcy);
		int brMax = MIN(m_blurRadius, (int)srch-1-srcy);
		UINT8* dstLine = dst+y*dststride;
		for (x = 0; x < (int)dstw; ++x)
		{
			VEGA_FIX sum = 0;
			VEGA_FIX* tempRow = m_blurTemp + (srcy+brMin)*tempstride + x;
			VEGA_FIX* kern = m_blurKernel + (m_blurRadius+brMin);
			for (int k_ofs = brMin; k_ofs <= brMax; ++k_ofs)
			{
				sum += VEGA_FIXMUL(*kern, *tempRow);
				tempRow += tempstride;
				++kern;
			}
			*dstLine = VEGA_FIXTOINT(sum);
			++dstLine;
		}
	}
	return OpStatus::OK;
}

OP_STATUS VEGAMDEFont::loadGlyph(VEGAGlyph& glyph, UINT8* data, unsigned int stride, BOOL isIndex)
{
	glyph.m_handle = 0;

	MDF_GLYPH mdfglyph;
	MDF_GLYPH_BUFFER mdfbuffer;
#ifdef VEGA_3DDEVICE
	if (data && m_blurRadius == 0)
	// rasterize glyph into buffer provided in data
	{
		OP_ASSERT(stride);
		mdfbuffer.data = data;
		// passed width and height are used for clipping, remember?
		mdfbuffer.w = getSlotWidth();
		mdfbuffer.h = getSlotHeight();
		mdfglyph.buffer_data = &mdfbuffer;
		RETURN_IF_ERROR(MDF_FillGlyph(mdfglyph, m_mdefont, glyph.glyph, stride, isIndex));
	}
	else
#endif // VEGA_3DDEVICE
	// let mdefont allocate a buffer big enough to encompass the glyph
	// clearCache calls unloadBuffer for cleanup
	{
		RETURN_IF_ERROR(MDF_GetGlyph(mdfglyph, m_mdefont, glyph.glyph, TRUE, isIndex));
		OP_ASSERT(mdfglyph.buffer_data);
		glyph.m_handle = mdfglyph.buffer_data;
		mdfbuffer = *mdfglyph.buffer_data;
	}

	glyph.top = mdfglyph.bitmap_top;
	glyph.left = mdfglyph.bitmap_left;
	glyph.advance = mdfglyph.advance;
	glyph.width = mdfbuffer.w;
	glyph.height = mdfbuffer.h;

	if (m_blurRadius)
	{
		UINT8* blurdest = NULL;
		// 3D backend will blur directly into the glyph cache surface
		// software need to allocate a buffer
		if (!data)
		{
			int neww = glyph.width+2*m_blurRadius;
			int newh = glyph.height+2*m_blurRadius;
			blurdest = OP_NEWA_WH(UINT8, neww, newh);
			stride = neww;
			if (!blurdest)
			{
				MDF_FreeGlyph(m_mdefont, mdfglyph);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		else
			blurdest = data;

		blurGlyph((UINT8*)static_cast<MDF_GLYPH_BUFFER*>(glyph.m_handle)->data, blurdest, glyph.width, stride, glyph.width, glyph.height);
		glyph.width += 2*m_blurRadius;
		glyph.height += 2*m_blurRadius;
		glyph.top += m_blurRadius;
		glyph.left -= m_blurRadius;

		// data is owned by someone else so don't keep reference
		if (data)
			glyph.m_handle = 0;
		else
			glyph.m_handle = blurdest;

		MDF_FreeGlyph(m_mdefont, mdfglyph);
	}

	return OpStatus::OK;
}
void VEGAMDEFont::unloadGlyph(VEGAGlyph& glyph)
{
	// assumes only thing accessed in MDF_FreeGlyph is glyph.buffer_data
	MDF_GLYPH g;
	if (m_blurRadius)
		OP_DELETEA((UINT8*)glyph.m_handle);
	else
	{
		g.buffer_data = (MDF_GLYPH_BUFFER*)glyph.m_handle;
		MDF_FreeGlyph(m_mdefont, g);
	}
}
void VEGAMDEFont::getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride)
{
	if (m_blurRadius)
	{
		buffer = (UINT8*)glyph.m_handle;
		stride = glyph.width;
		return;
	}
	MDF_GLYPH_BUFFER* mbuf = (MDF_GLYPH_BUFFER*)glyph.m_handle;
	OP_ASSERT(mbuf);
	buffer = (UINT8*)mbuf->data;
	stride = mbuf->w;
}

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
bool VEGAMDEFont::UseSubpixelRendering()
{
	return g_mdf_engine->HasComponentRendering() && !m_blurRadius;
}
#endif // VEGA_SUBPIXEL_FONT_BLENDING

const uni_char* VEGAMDEFont::getFontName()
{
	return g_mdf_engine->GetFontName(m_mdefont);
}

BOOL VEGAMDEFont::isBold()
{
	return g_mdf_engine->IsBold(m_mdefont);
}

BOOL VEGAMDEFont::isItalic()
{
	return g_mdf_engine->IsItalic(m_mdefont);
}


/*********************************************************************/
/*                        VEGAMDFOpFontManager                       */
/*********************************************************************/

#ifndef VEGA_NO_FONTMANAGER_CREATE
/*static */
OP_STATUS OpFontManager::Create(OpFontManager** new_fontmanager)
{
	OP_ASSERT(new_fontmanager);
	*new_fontmanager = OP_NEW(VEGAMDFOpFontManager, ());
	if (!(*new_fontmanager))
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = ((VEGAMDFOpFontManager*)*new_fontmanager)->Construct();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(*new_fontmanager);
		*new_fontmanager = NULL;
	}
	return status;
}
#endif  // !VEGA_NO_FONTMANAGER_CREATE

#ifdef GOGI
#include "platforms/gogi/src/gogi_opera.h"
#endif // GOGI

VEGAMDFOpFontManager::VEGAMDFOpFontManager()
	:VEGAOpFontManager()
#ifdef PERSCRIPT_GENERIC_FONT
	,m_serif_fonts(WritingSystem::NumScripts)
	,m_sansserif_fonts(WritingSystem::NumScripts)
	,m_cursive_fonts(WritingSystem::NumScripts)
	,m_fantasy_fonts(WritingSystem::NumScripts)
	,m_monospace_fonts(WritingSystem::NumScripts)
#endif // PERSCRIPT_GENERIC_FONT
{
}





OP_STATUS VEGAMDFOpFontManager::Construct()
{
	// FIXME: OOM, verify that the fonts exist
	VEGAOpFontManager::DefaultFonts def_fonts;
#ifdef GOGI
	def_fonts.default_font = uni_up_strdup(g_gogi_opera->GetFontStruct()->default_font);
	def_fonts.serif_font = uni_up_strdup(g_gogi_opera->GetFontStruct()->serif_font);
	def_fonts.sans_serif_font = uni_up_strdup(g_gogi_opera->GetFontStruct()->sans_serif_font);
	def_fonts.cursive_font = uni_up_strdup(g_gogi_opera->GetFontStruct()->cursive_font);
	def_fonts.fantasy_font = uni_up_strdup(g_gogi_opera->GetFontStruct()->fantasy_font);
	def_fonts.monospace_font = uni_up_strdup(g_gogi_opera->GetFontStruct()->monospace_font);
#else
	if (g_vegaGlobals.default_font)
		def_fonts.default_font = uni_strdup(g_vegaGlobals.default_font);
	else
		def_fonts.default_font = uni_strdup(UNI_L("Times New Roman"));
	if (g_vegaGlobals.default_serif_font)
		def_fonts.serif_font = uni_strdup(g_vegaGlobals.default_serif_font);
	else
		def_fonts.serif_font = uni_strdup(UNI_L("Times New Roman"));
	if (g_vegaGlobals.default_sans_serif_font)
		def_fonts.sans_serif_font = uni_strdup(g_vegaGlobals.default_sans_serif_font);
	else
		def_fonts.sans_serif_font = uni_strdup(UNI_L("Arial"));
	if (g_vegaGlobals.default_cursive_font)
		def_fonts.cursive_font = uni_strdup(g_vegaGlobals.default_cursive_font);
	else
		def_fonts.cursive_font = uni_strdup(UNI_L("Comic Sans MS"));
	if (g_vegaGlobals.default_fantasy_font)
		def_fonts.fantasy_font = uni_strdup(g_vegaGlobals.default_fantasy_font);
	else
		def_fonts.fantasy_font = uni_strdup(UNI_L("Arial"));
	if (g_vegaGlobals.default_monospace_font)
		def_fonts.monospace_font = uni_strdup(g_vegaGlobals.default_monospace_font);
	else
		def_fonts.monospace_font = uni_strdup(UNI_L("Courier New"));
#endif

	OP_STATUS status = SetDefaultFonts(&def_fonts);


#ifdef PERSCRIPT_GENERIC_FONT
	if (OpStatus::IsSuccess(status))
		status = InitGenericFont(def_fonts);
#endif

	op_free(def_fonts.default_font);
	op_free(def_fonts.serif_font);
	op_free(def_fonts.sans_serif_font);
	op_free(def_fonts.cursive_font);
	op_free(def_fonts.fantasy_font);
	op_free(def_fonts.monospace_font);

	return status;
}

UINT32 VEGAMDFOpFontManager::CountFonts()
{
	return MDF_CountFonts();
}

OP_STATUS VEGAMDFOpFontManager::GetFontInfoInternal(const MDF_FONTINFO& mdfinfo, OpFontInfo* fontinfo)
{
	OP_ASSERT(fontinfo);

	fontinfo->SetFace(mdfinfo.font_name);
	fontinfo->SetWeight(4, mdfinfo.has_normal);
	fontinfo->SetWeight(7, mdfinfo.has_bold);
	fontinfo->SetItalic(mdfinfo.has_italic);
	fontinfo->SetMonospace(mdfinfo.is_monospace);

	switch (mdfinfo.has_serif)
	{
	case MDF_SERIF_SANS:
		fontinfo->SetSerifs(OpFontInfo::WithoutSerifs);
		break;
	case MDF_SERIF_SERIF:
		fontinfo->SetSerifs(OpFontInfo::WithSerifs);
		break;
	case MDF_SERIF_UNDEFINED:
		fontinfo->SetSerifs(OpFontInfo::UnknownSerifs);
		break;
	default:
		OP_ASSERT(FALSE);
	}

	for (int i = 0 ; i < 4 ; i++)
		for (int j = 0 ; j < 32 ; j++)
			fontinfo->SetBlock(i * 32 + j, (mdfinfo.ranges[i] & (1<<j)) == 0 ? FALSE : TRUE);

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
	fontinfo->SetScriptsFromOS2CodePageRanges(mdfinfo.m_codepages, 1);
#endif // MDF_CODEPAGES_FROM_OS2_TABLE

	return OpStatus::OK;
}

OP_STATUS VEGAMDFOpFontManager::GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo)
{
	MDF_FONTINFO mdfinfo;
	RETURN_IF_ERROR(MDF_GetFontInfo(fontnr, &mdfinfo));
	fontinfo->SetFontNumber(fontnr);
	return GetFontInfoInternal(mdfinfo, fontinfo);
}

OpFontInfo::FontType VEGAMDFOpFontManager::GetWebFontType(OpWebFontRef webfont)
{
	return MDF_GetWebFontType((MDF_WebFontBase*)webfont);
}

OP_STATUS VEGAMDFOpFontManager::GetLocalFont(OpWebFontRef& localfont, const uni_char* facename)
{
	MDF_WebFontBase* local = 0;
	RETURN_IF_ERROR(MDF_GetLocalFont(local, facename));
	localfont = (OpWebFontRef)local;
	return OpStatus::OK;
}

BOOL VEGAMDFOpFontManager::SupportsFormat(int f)
{
	const CSS_WebFont::Format format = static_cast<CSS_WebFont::Format>(f);
	return (format & (CSS_WebFont::FORMAT_TRUETYPE | CSS_WebFont::FORMAT_TRUETYPE_AAT | CSS_WebFont::FORMAT_OPENTYPE)) != 0;
}

OP_STATUS VEGAMDFOpFontManager::BeginEnumeration()
{
	return MDF_BeginFontEnumeration();
}

OP_STATUS VEGAMDFOpFontManager::EndEnumeration()
{
	MDF_EndFontEnumeration();
	return OpStatus::OK;
}

OP_STATUS VEGAMDFOpFontManager::AddWebFont(OpWebFontRef& webfont, const uni_char* full_path_of_file)
{
	MDF_WebFontBase* ref;
	RETURN_IF_ERROR(MDF_AddFontFile(full_path_of_file, ref));
	webfont = (OpWebFontRef)ref;
	return OpStatus::OK;
}

OP_STATUS VEGAMDFOpFontManager::RemoveWebFont(OpWebFontRef webfont)
{
	return MDF_RemoveFont((MDF_WebFontBase*)webfont);
}

OP_STATUS VEGAMDFOpFontManager::GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo)
{
	MDF_FONTINFO mdfinfo;
	RETURN_IF_ERROR(MDF_GetWebFontInfo((MDF_WebFontBase*)webfont, &mdfinfo));
	return GetFontInfoInternal(mdfinfo, fontinfo);
}

VEGAFont* VEGAMDFOpFontManager::GetVegaFontInt(MDE_FONT* mdefont, UINT32 size, INT32 blur_radius)
{
	if (!mdefont)
	{
		return NULL;
	}

	VEGAMDEFont* font = OP_NEW(VEGAMDEFont, (mdefont, blur_radius));
	if (!font)
	{
		MDF_ReleaseFont(mdefont);
		return NULL;
	}

	OP_STATUS status = font->Construct();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(font);
		return NULL;
	}

	return font;
}

VEGAFont* VEGAMDFOpFontManager::GetVegaFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius)
{
	MDE_FONT* mdefont = MDF_GetWebFont((MDF_WebFontBase*)webfont, size);
	return GetVegaFontInt(mdefont, size, blur_radius);
}
VEGAFont* VEGAMDFOpFontManager::GetVegaFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius)
{
	MDE_FONT* mdefont = MDF_GetWebFont((MDF_WebFontBase*)webfont, weight, italic, size);
	return GetVegaFontInt(mdefont, size, blur_radius);
}

#ifdef PERSCRIPT_GENERIC_FONT


OP_STATUS VEGAMDFOpFontManager::InitGenericFont(const DefaultFonts& fonts)
{
	if (m_serif_fonts.GetCount())
		return OpStatus::OK;

	OpString *s;
	for (int i=0; i < (int)WritingSystem::NumScripts; ++i)
	{
		s = OP_NEW(OpString,());
		if (!s)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(m_serif_fonts.Add(s));
		s = OP_NEW(OpString,());
		if (!s)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(m_sansserif_fonts.Add(s));
		s = OP_NEW(OpString,());
		if (!s)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(m_cursive_fonts.Add(s));
		s = OP_NEW(OpString,());
		if (!s)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(m_fantasy_fonts.Add(s));
		s = OP_NEW(OpString,());
		if (!s)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(m_monospace_fonts.Add(s));
	}

	for (int i = 0; i < (int)WritingSystem::NumScripts; ++i)
	{
		if (!m_serif_fonts.Get(i)->HasContent())
			RETURN_IF_ERROR(m_serif_fonts.Get(i)->Set(fonts.serif_font));
		if (!m_sansserif_fonts.Get(i)->HasContent())
			RETURN_IF_ERROR(m_sansserif_fonts.Get(i)->Set(fonts.sans_serif_font));
		if (!m_cursive_fonts.Get(i)->HasContent())
			RETURN_IF_ERROR(m_cursive_fonts.Get(i)->Set(fonts.cursive_font));
		if (!m_fantasy_fonts.Get(i)->HasContent())
			RETURN_IF_ERROR(m_fantasy_fonts.Get(i)->Set(fonts.fantasy_font));
		if (!m_monospace_fonts.Get(i)->HasContent())
			RETURN_IF_ERROR(m_monospace_fonts.Get(i)->Set(fonts.monospace_font));
	}

	return OpStatus::OK;
}


OP_STATUS VEGAMDFOpFontManager::SetGenericFonts(const DefaultFonts& fonts, WritingSystem::Script script)
{
	if (fonts.serif_font)
		RETURN_IF_ERROR(m_serif_fonts.Get(script)->Set(fonts.serif_font));
	if (fonts.sans_serif_font)
		RETURN_IF_ERROR(m_sansserif_fonts.Get(script)->Set(fonts.sans_serif_font));
	if (fonts.cursive_font)
		RETURN_IF_ERROR(m_cursive_fonts.Get(script)->Set(fonts.cursive_font));
	if (fonts.fantasy_font)
		RETURN_IF_ERROR(m_fantasy_fonts.Get(script)->Set(fonts.fantasy_font));
	if (fonts.monospace_font)
		RETURN_IF_ERROR(m_monospace_fonts.Get(script)->Set(fonts.monospace_font));

	return OpStatus::OK;
}


const uni_char* VEGAMDFOpFontManager::GetGenericFontName(GenericFont generic_font, WritingSystem::Script script)
{
	switch (generic_font)
	{
	case GENERIC_FONT_SERIF:
		return m_serif_fonts.Get(script)->CStr();
	case GENERIC_FONT_SANSSERIF:
		return m_sansserif_fonts.Get(script)->CStr();
	case GENERIC_FONT_CURSIVE:
		return m_cursive_fonts.Get(script)->CStr();
	case GENERIC_FONT_FANTASY:
		return m_fantasy_fonts.Get(script)->CStr();
	case GENERIC_FONT_MONOSPACE:
		return m_monospace_fonts.Get(script)->CStr();
	default:
		OP_ASSERT(FALSE);
		return NULL;
	}
}
#endif // PERSCRIPT_GENERIC_FONT




VEGAFont* VEGAMDFOpFontManager::GetVegaFont(const uni_char* face, UINT32 size, UINT8 weight,
											BOOL italic, BOOL must_have_getoutline, INT32 blur_radius)
{
	OP_ASSERT(face);
	int fnum = styleManager->GetFontNumber(face);
	if (fnum < 0)
		return NULL;

	MDE_FONT* mdefont = MDF_GetFont(fnum, size, weight > 5, italic ? true : false);

    if (must_have_getoutline && mdefont)
    {
        if (!MDF_IsScalable(mdefont))
        {
            MDF_ReleaseFont(mdefont);
            return NULL;
        }
    }

    if (!mdefont)
    {
        mdefont = MDF_GetFont(fnum, size, false, false);

        if (must_have_getoutline && mdefont)
        {
            if (!MDF_IsScalable(mdefont))
            {
                MDF_ReleaseFont(mdefont);
                return NULL;
            }
        }

        if (!mdefont)
        {
            return NULL;
        }
    }

    VEGAMDEFont* font = OP_NEW(VEGAMDEFont, (mdefont, blur_radius));
    if (!font)
    {
        MDF_ReleaseFont(mdefont);
        return NULL;
    }

    OP_STATUS status = font->Construct();
    if (OpStatus::IsError(status))
    {
        OP_DELETE(font);
        return NULL;
    }

    return font;
}

#ifdef _GLYPHTESTING_SUPPORT_
void VEGAMDFOpFontManager::UpdateGlyphMask(OpFontInfo *fontinfo)
{
#ifdef VEGA_MDEFONT_SUPPORT
	// FIXME: OOM
	OpStatus::Ignore(MDF_UpdateGlyphMask(fontinfo));
#endif // VEGA_MDEFONT_SUPPORT
}
#endif // _GLYPHTESTING_SUPPORT_

#ifndef MDF_FONT_ADVANCE_CACHE
OP_STATUS VEGAMDEFont::ProcessString(ProcessedString* processed_string,
									 const uni_char* str, const size_t len,
									 INT32 extra_char_spacing, short word_width, 
									 bool use_glyph_indices)
{
	OP_ASSERT(processed_string);
	// no advance cache means every glyph will be loaded in order to
	// fetch advance, since mdefont's glyph cache is disabled
	int flags = MDF_PROCESS_FLAG_NO_ADVANCE;
	if (use_glyph_indices)
		flags |= MDF_PROCESS_FLAG_USE_GLYPH_INDICES;
	RETURN_IF_ERROR(MDF_ProcessString(m_mdefont, *processed_string, str, len, extra_char_spacing, word_width, flags));
	int advance = 0;
	VEGAFont::VEGAGlyph glyph;
	for (size_t i = 0; i < processed_string->m_length; ++i)
	{
		processed_string->m_processed_glyphs[i].m_pos.x += advance;

		if (OpStatus::IsSuccess(getGlyph(processed_string->m_processed_glyphs[i].m_id, glyph, processed_string->m_is_glyph_indices)))
			advance += glyph.advance;
	}
	processed_string->m_advance += advance;
	if (word_width != -1 && word_width != processed_string->m_advance)
		AdjustToWidth(*processed_string, word_width);
	return OpStatus::OK;
}
#endif // !MDF_FONT_ADVANCE_CACHE

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && VEGA_MDEFONT_SUPPORT

#endif // MDEFONT_MODULE
