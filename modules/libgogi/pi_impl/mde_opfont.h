/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_OPFONT_H
#define MDE_OPFONT_H

#include "modules/pi/OpFont.h"

#ifdef MDEFONT_MODULE
struct MDE_FONT;
#endif // MDEFONT_MODULE

class MDE_OpFont : public OpFont
{
public:
	MDE_OpFont();
	virtual ~MDE_OpFont();
    virtual OpFontInfo::FontType Type() { return m_type; }
#ifdef MDEFONT_MODULE
	void Init(MDE_FONT* mdefont);
#endif // MDEFONT_MODULE
	virtual UINT32 Ascent();
	virtual UINT32 Descent();
	virtual UINT32 InternalLeading();
	virtual UINT32 Height();
	virtual UINT32 AveCharWidth();
	virtual UINT32 Overhang();
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing = 0);
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing = 0);

#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,
								 BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph);
#endif // SVG_SUPPORT

	virtual OP_STATUS GetFontData(UINT8*& font_data, UINT32& data_size) { return OpStatus::ERR; }
	virtual OP_STATUS ReleaseFontData(UINT8* font_data) { return OpStatus::ERR; }

#ifdef OPFONT_GLYPH_PROPS
	OP_STATUS GetGlyphProps(const UnicodePoint up, GlyphProps* props);
#endif // OPFONT_GLYPH_PROPS

// private-ish:
#ifdef MDEFONT_MODULE
	MDE_FONT* m_mdefont;
#endif // MDEFONT_MODULE
	int m_ave_char_width;
	OpFontInfo::FontType m_type;
};

#endif // MDE_OPFONT_H
