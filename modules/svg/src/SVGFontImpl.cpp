/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/svg/src/SVGFontImpl.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGManagerImpl.h"

#include "modules/unicode/unicode.h"

SVGFontImpl::SVGFontImpl(SVGFontData* fontdata, UINT32 size)
	: m_match_lang(NULL), m_size(size)
{
	SVGFontData::IncRef(m_fontdata = fontdata);

	m_scale = m_size / m_fontdata->GetUnitsPerEm();
}

SVGFontImpl::~SVGFontImpl()
{
	Out();
	SVGFontData::DecRef(m_fontdata);

	if (g_svg_manager_impl) // Can be NULL if shutting down
	{
		g_svg_manager_impl->UnregisterSVGFont(this);
	}
}

#ifdef OPFONT_GLYPH_PROPS
OP_STATUS SVGFontImpl::GetGlyphProps(const UnicodePoint up, GlyphProps* props)
{
	SVGGlyphData* current_glyph = NULL;

	// Convert UnicodePoint to a string we can use in FetchGlyphData
	uni_char in_str[2] = { up, 0 };
	UINT32 in_len = 1;
	if (up >= 0x10000 && up <= 0x10FFFF)
	{
		Unicode::MakeSurrogate(up, in_str[0], in_str[1]);
		in_len = 2;
	}
	UINT32 io_str_pos = 0;
	OpStatus::Ignore(m_fontdata->FetchGlyphData(in_str, in_len,
												io_str_pos, TRUE,
												m_match_lang, &current_glyph));

	// FetchGlyphData should never return a NULL current_glyph. If it
	// doesn't find an appropriate glyph, it will return a pointer to
	// the 'missing glyph' (which may or may not be empty). It should
	// also _always_ consume atleast one uni_char from in_str
	OP_ASSERT(current_glyph);

	if (OpBpath* the_outline = current_glyph->GetOutline())
	{
		OP_ASSERT(the_outline->Type() == SVGOBJECT_PATH);
		SVGBoundingBox bb = the_outline->GetBoundingBox();
		SVGNumber advance = current_glyph->HasAdvanceX() ? current_glyph->GetAdvanceX() : m_fontdata->GetSVGFontAttribute(SVGFontData::ADVANCE_X);
		props->advance = (advance * m_scale).GetIntegerValue();
		int iminx = (bb.minx * m_scale).GetIntegerValue();
		int iminy = (bb.miny * m_scale).GetIntegerValue();
		int imaxx = (bb.maxx * m_scale).ceil().GetIntegerValue();
		int imaxy = (bb.maxy * m_scale).ceil().GetIntegerValue();
		props->left = iminx;
		props->top = imaxy;
		props->width = imaxx - iminx;
		props->height = imaxy - iminy;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}
#endif // OPFONT_GLYPH_PROPS

OP_STATUS SVGFontImpl::GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,
								  BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph)
{
	OP_ASSERT(IsFontFullyCreated());

	if (!out_glyph)
		return OpStatus::OK;
	if (io_str_pos >= in_len || in_len == 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	OP_STATUS result = OpStatus::OK;

	SVGGlyphData* current_glyph = NULL;
	OpBpath* the_outline = NULL;

	OpStatus::Ignore(m_fontdata->FetchGlyphData(in_str, in_len,
												io_str_pos, in_writing_direction_horizontal,
												m_match_lang, &current_glyph));

	// FetchGlyphData should never return a NULL current_glyph. If it
	// doesn't find an appropriate glyph, it will return a pointer to
	// the 'missing glyph' (which may or may not be empty). It should
	// also _always_ consume atleast one uni_char from in_str
	OP_ASSERT(current_glyph);

	the_outline = current_glyph->GetOutline();

	if (in_writing_direction_horizontal)
	{
		// The reason why we don't do GetSVGFontAttribute(SVGFontData::ADVANCE_X) is that we want the unscaled value, otherwise we'll get scaled twice
		out_advance = current_glyph->HasAdvanceX() ? current_glyph->GetAdvanceX() : m_fontdata->GetSVGFontAttribute(SVGFontData::ADVANCE_X);
	}
	else
	{
		// The reason why we don't do GetSVGFontAttribute(SVGFontData::ADVANCE_Y) is that we want the unscaled value, otherwise we'll get scaled twice
		out_advance = current_glyph->HasAdvanceY() ? current_glyph->GetAdvanceY() : m_fontdata->GetSVGFontAttribute(SVGFontData::ADVANCE_Y);

		if (out_advance.Equal(0) && current_glyph->HasAdvanceX())
		{
			// Emulate vertical advance with ascent + |descent|
			out_advance =
				m_fontdata->GetSVGFontAttribute(SVGFontData::ASCENT) +
				m_fontdata->GetSVGFontAttribute(SVGFontData::DESCENT).abs();
		}
	}

	if (the_outline)
	{
		result = OpStatus::OK;
		OP_ASSERT(the_outline->Type() == SVGOBJECT_PATH);
		OpBpath* copy = static_cast<OpBpath *>(the_outline->Clone());
		if (!copy)
		{
			result = OpStatus::ERR;
		}
		else
		{
			if (OpStatus::IsSuccess(SVGUtils::RescaleOutline(copy, m_scale)))
				*out_glyph = copy;
			else
			{
				OP_DELETE(copy);
				result = OpStatus::ERR_NO_MEMORY;
			}
		}
	}
	else if (OpStatus::IsError(result))
	{
		result = OpStatus::ERR;
	}

	if (OpStatus::IsSuccess(result))
	{
		if (HasKerning() && io_str_pos < in_len)
		{
			OP_STATUS status;
			SVGNumber kerning;
			SVGGlyphData* next_glyph = NULL;
			UINT32 tmp_str_pos = io_str_pos;

			status = m_fontdata->FetchGlyphData(in_str, in_len, tmp_str_pos,
												in_writing_direction_horizontal,
												m_match_lang, &next_glyph);

			if(OpStatus::IsSuccess(status))
			{
				status = m_fontdata->GetKern(current_glyph, next_glyph,
											 in_writing_direction_horizontal, kerning);

				if(OpStatus::IsSuccess(status))
				{
					out_advance -= kerning;
				}
			}
		}
	}

	// Scale the advance to font size.
	out_advance *= m_scale;

	return result;
}

UINT32 SVGFontImpl::StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing
#ifdef PLATFORM_FONTSWITCHING
								, OpAsyncFontListener* listener
#endif
								)
{
	OP_ASSERT(IsFontFullyCreated());

	// FIXME: Take kerning into account
	if (!str || len == 0)
		return 0;

	SVGNumber stringVisualLen;
	SVGNumber maxVisualLen;
	SVGGlyphData* data = NULL;
	for (UINT32 io_str_pos = 0; io_str_pos < len;)
	{
		OpStatus::Ignore(m_fontdata->FetchGlyphData(str, len, io_str_pos, TRUE, m_match_lang, &data));

		OP_ASSERT(data);

		SVGNumber advance;
		if (data->HasAdvanceX())
			advance = data->GetAdvanceX() * m_scale;
		else
			advance = GetSVGFontAttribute(SVGFontData::ADVANCE_X);

		stringVisualLen += advance;

		stringVisualLen += extra_char_spacing;
		if (stringVisualLen < 0) // We never 'reverse' text with big negative letter spacings
		{
			stringVisualLen = 0;
		}

		maxVisualLen = SVGNumber::max_of(maxVisualLen, stringVisualLen);
	}

	return maxVisualLen.GetIntegerValue();
}

UINT32 SVGFontImpl::Ascent()
{
	if (!IsFontFullyCreated())
	{
		// This happens because the font is used while traversing
		// the tree to create the outline. It will be read into
		// LayoutProperties but not used.
		return 0; // dummy value
	}

	return GetSVGFontAttribute(SVGFontData::ASCENT).GetIntegerValue();
}

UINT32 SVGFontImpl::Descent()
{
	if (!IsFontFullyCreated())
	{
		// This happens because the font is used while traversing
		// the tree to create the outline. It will be read into
		// LayoutProperties but not used.
		return 0; // dummy value
	}

	// People writing SVG:s can't agree if the descent
	// is negative or positive and the specification is fuzzy.
	return op_abs(GetSVGFontAttribute(SVGFontData::DESCENT).GetIntegerValue());
}

UINT32 SVGFontImpl::InternalLeading()
{
	if (!IsFontFullyCreated())
	{
		// This happens because the font is used while traversing
		// the tree to create the outline. It will be read into
		// LayoutProperties but not used.
		return 0; // dummy value
	}

	// Approximation, or is this correct?
	int height = Ascent() + Descent();
	int int_size = m_size.GetIntegerValue();
	if (height > int_size)
	{
		return height - int_size;
	}
	return 0;
}

UINT32 SVGFontImpl::Height()
{
	OP_ASSERT(IsFontFullyCreated());

	return Ascent() + Descent();
}

#endif // SVG_SUPPORT
