/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/libgogi/pi_impl/mde_opfont.h"
#include "modules/libgogi/mde.h"

#ifdef MDEFONT_MODULE
#include "modules/mdefont/mdefont.h"
#endif // MDEFONT_MODULE

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_module.h"
# include "modules/svg/svg_path.h"
# include "modules/svg/svg_number.h"
# if defined(MDF_FREETYPE_SUPPORT) && defined(MDEFONT_MODULE)
#  ifdef FT_INTERNAL_FREETYPE
#   include "modules/libfreetype/include/ft2build.h"
#   include "modules/libfreetype/include/freetype/ftglyph.h"
#   include "modules/libfreetype/include/freetype/ftoutln.h"
#  else // FT_INTERNAL_FREETYPE
#   include <ft2build.h>
#   include <freetype/ftglyph.h>
#   include <freetype/ftoutln.h>
#  endif // FT_INTERNAL_FREETYPE
# endif // MDF_FREETYPE_SUPPORT && MDEFONT_MODULE
# ifdef MDF_AGFA_SUPPORT
#  include "modules/iType/product/source/common/fs_api.h"
# endif // MDF_AGFA_SUPPORT
#endif // SVG_SUPPORT

MDE_OpFont::MDE_OpFont() :
#ifdef MDEFONT_MODULE
		m_mdefont(NULL),
#endif // MDEFONT_MODULE
		m_ave_char_width(0),
		m_type(OpFontInfo::PLATFORM)
{
}

/*virtual*/
MDE_OpFont::~MDE_OpFont()
{
#ifdef MDEFONT_MODULE
	MDF_ReleaseFont(m_mdefont);
#endif // MDEFONT_MODULE
}

#ifdef MDEFONT_MODULE
void
MDE_OpFont::Init(MDE_FONT* mdefont)
{
	m_mdefont = mdefont;
}
#endif // MDEFONT_MODULE

/*virtual*/
UINT32
MDE_OpFont::Ascent()
{
#ifdef MDEFONT_MODULE
	return m_mdefont->ascent;
#else
	OP_ASSERT(!"MDE_Font module required");
	return 0;
#endif // MDEFONT_MODULE
}

/*virtual*/
UINT32
MDE_OpFont::Descent()
{
#ifdef MDEFONT_MODULE
	return m_mdefont->descent;
#else
	OP_ASSERT(!"MDE_Font module required");
	return 0;
#endif // MDEFONT_MODULE
}

/*virtual*/
UINT32
MDE_OpFont::InternalLeading()
{
#ifdef MDEFONT_MODULE
	return m_mdefont->internal_leading;
#else
	OP_ASSERT(!"MDE_Font module required");
	return 0;
#endif // MDEFONT_MODULE
}

/*virtual*/
UINT32
MDE_OpFont::Height()
{
#ifdef MDEFONT_MODULE
	return m_mdefont->height;
#else
	OP_ASSERT(!"MDE_Font module required");
	return 0;
#endif // MDEFONT_MODULE
}

/*virtual*/
UINT32
MDE_OpFont::AveCharWidth()
{
	if (m_ave_char_width == 0)
	{
#ifdef MDEFONT_MODULE
		int w;
		// FIXME:OOM
		if (OpStatus::IsSuccess(MDF_StringWidth(w, m_mdefont, UNI_L("x"), 1, 0)))
			m_ave_char_width = w;
#endif // MDEFONT_MODULE
	}
	return m_ave_char_width;
}

/*virtual*/
UINT32
MDE_OpFont::Overhang()
{
	// FIXME
//	OP_ASSERT(FALSE);
	return 0;
}

/*virtual*/
UINT32
MDE_OpFont::StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing)
{
#ifdef MDEFONT_MODULE
	int w=0;
	// FIXME:OOM
	if (OpStatus::IsSuccess(MDF_StringWidth(w, m_mdefont, str, len, extra_char_spacing)))
		if (w < 0 && extra_char_spacing < 0)
			// FIXME:OOM
			OpStatus::Ignore(MDF_StringWidth(w, m_mdefont, str, len, 0));
	return w >= 0 ? (UINT32)w : 0;
#else
	OP_ASSERT(!"MDE_Font module required");
	return 0;
#endif // MDEFONT_MODULE
}

/*virtual*/
UINT32
MDE_OpFont::StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing)
{
	return StringWidth(str, len, extra_char_spacing);
}

#ifdef SVG_SUPPORT

/*virtual*/
OP_STATUS 
MDE_OpFont::GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,
						BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph)
{
#ifdef MDEFONT_MODULE
	return MDF_GetOutline(m_mdefont, in_str, in_len, io_str_pos, in_last_str_pos, in_writing_direction_horizontal, out_advance, out_glyph);
#else
	OP_ASSERT(!"MDE_Font module required");
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // MDEFONT_MODULE
}

#endif // SVG_SUPPORT

#ifdef OPFONT_GLYPH_PROPS
/*virtual*/
OP_STATUS
MDE_OpFont::GetGlyphProps(const UnicodePoint up, GlyphProps* props)
{
#ifdef MDEFONT_MODULE
	return MDF_GetGlyphProps(m_mdefont, up, props);
#else // MDEFONT_MODULE
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // MDEFONT_MODULE
}
#endif // OPFONT_GLYPH_PROPS
