/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGTextRenderer.h"

#include "modules/display/vis_dev.h"
#include "modules/display/styl_man.h"

#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGFontImpl.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/probetools/probepoints.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGTextTraverse.h"
#include "modules/svg/src/SVGTextLayout.h"
#include "modules/svg/src/SVGTextElementStateContext.h"

#ifdef SVG_SUPPORT_EDITABLE
#include "modules/svg/src/SVGEditable.h"
#endif // SVG_SUPPORT_EDITABLE
#include "modules/pi/ui/OpUiInfo.h"

#include "modules/logdoc/htm_elm.h"

#include "modules/layout/layoutprops.h"
#include "modules/layout/cascade.h"

#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
# include "modules/display/textshaper.h"
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT

//#define SVG_DEBUG_TEXT_DETAILED
//#define SVG_DEBUG_TEXTSELECTION
// #define SVG_DEBUG_SYSTEMFONT_GLYPHCACHING

OP_STATUS FragmentTransform::Apply(const uni_char* txt, unsigned len, int format)
{
#ifdef SVG_TXTOUT_USE_TMPBUFFER
	if (len <= UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()))
	{
		m_output = static_cast<uni_char*>(g_memory_manager->GetTempBuf2k());
	}
	else
#endif
	{
		m_output = OP_NEWA(uni_char, len);
		m_allocated = m_output;
		if (!m_output)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	// FIXME: TEXT_FORMAT_BIDI_PRESERVE_ORDER flag, conditional/unconditional ?
	size_t slen = len;
 	m_output = VisualDevice::TransformText(txt, m_output, slen, format);
	m_output_len = (unsigned)slen;
	return OpStatus::OK;
}

SVGNumber SVGTextRenderer::GetExpansion() const
{
	if (m_canvas)
		return m_canvas->GetTransform().GetExpansionFactor();

	return SVGNumber(1);
}

static inline BOOL OpFontIsSVGFont(OpFont* font)
{
	return font->Type() == OpFontInfo::SVG_WEBFONT;
}

BOOL SVGTextRenderer::GetSVGFontAttribute(SVGFontData::ATTRIBUTE attribute,
										  SVGNumber fontheight, SVGNumber& out_value)
{
	OpFont* current_font = m_fontdesc->GetFont();
	if (OpFontIsSVGFont(current_font))
	{
		SVGFontImpl* svg_font = static_cast<SVGFontImpl*>(current_font);
		out_value = svg_font->GetSVGFontAttribute(attribute) * m_fontdesc->GetFontToUserScale();
		return TRUE;
	}

	// Set default values
	switch (attribute)
	{
	case SVGFontData::STRIKETHROUGH_THICKNESS:
	case SVGFontData::OVERLINE_THICKNESS:
	case SVGFontData::UNDERLINE_THICKNESS:
		out_value = fontheight / 12; // Good looking width.
		break;
	case SVGFontData::STRIKETHROUGH_POSITION:
		out_value = fontheight / 3; // Good looking position.
		break;
	case SVGFontData::UNDERLINE_POSITION:
		out_value = -fontheight / 8; // 1/8 of a font below the baseline. Good looking position.
		break;
	case SVGFontData::OVERLINE_POSITION:
		out_value = fontheight * SVGNumber(0.9); // Good looking pos.
		break;
	}
	return FALSE;
}

OP_STATUS SVGFontDescriptor::Check(SVGDocumentContext* doc_ctx, SVGNumber ctm_expansion,
								   const SVGNumber exact_font_size)
{
	if (m_fontinfo.GetChanged() || m_font == NULL || m_scale_might_have_changed)
	{
		OP_NEW_DBG("SVGFontDescriptor::Check", "svg_text");

		m_scale_might_have_changed = FALSE;

		m_fontinfo.SetHasGetOutline(TRUE);

		// We do the scaling instead of visual device to have full control
		// over rounding errors and font size. The end result is the same, but
		// with less code.
		int font_size_asked_for = (ctm_expansion * exact_font_size).GetRoundedIntegerValue();
		if (font_size_asked_for < 2)
		{
			// WindowsOpFont bugs if asking it for a font of size < 2 (returns
			// a larger font without telling anyone), this won't affect
			// vector drawn fonts but will make painter drawn
			// very small fonts too large.
			font_size_asked_for = 2;
		}

		OP_DBG(("Fontnumber: %d FontSize: %d. Asking for font size: %d doc_ctx: %p (%s)\n", m_fontinfo.GetFontNumber(), m_fontinfo.GetSize(), font_size_asked_for, doc_ctx, doc_ctx->IsExternal() ? "external" : "normal"));

		// Reuse the m_fontinfo struct with just the actual size modified
		int original_font_height = m_fontinfo.GetHeight();
		m_fontinfo.SetHeight(font_size_asked_for);

		// Lookup the font first in the font_doc_ctx and then fall back to parent doc_ctx
		OpFont* tmpFont = NULL;
		do {
			tmpFont = g_svg_manager_impl->GetFont(m_fontinfo, 100, doc_ctx);
			if (!tmpFont)
				doc_ctx = doc_ctx->GetParentDocumentContext();
		} while (!tmpFont && doc_ctx);

		m_fontinfo.SetHeight(original_font_height);
		m_fontinfo.ClearChanged();

		if (tmpFont)
		{
			OP_DBG((UNI_L("CurrentFont: %d face: %s\n"), m_fontinfo.GetFontNumber(), m_fontinfo.GetFaceName()));
			// Don't want the old font to be set in the renderer when calling ReleaseFont since
			// that might confuse some cleaning up code that then wants to release that font again
			// since it's going away.
			OpFont* font_to_release = m_font;
			m_font = tmpFont;
			if (font_to_release)
			{
				g_svg_manager_impl->ReleaseFont(font_to_release);
				// Now font_to_release might be deleted and the pointer pointing to dead memory
			}
			// Calculate correction factor between the retrieved font and user coordinates
			m_font_to_user_scale = exact_font_size / font_size_asked_for;
		}
		else
		{
			OP_ASSERT(!"Didn't get a font, what now?");
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::OK;
}

static BOOL NeedsVectorFonts(const SVGTextArguments& tparams)
{
	if (tparams.elm_needs_vector_fonts)
		return TRUE;

	if (tparams.rotatelist.Get(tparams.total_char_sum + tparams.current_char_idx, TRUE))
	{
		// We have rotates for single chars. DrawString can't do that.
		return TRUE;
	}

	// We think that DrawString would work and since DrawString is so very much faster
	// than vector fonts we can get some speedup
	return FALSE;
}

OP_STATUS GlyphRun::MapToGlyphs(OpFont* font, const uni_char* text, unsigned int textlen,
								const SVGGlyphLookupContext& lookup_cxt)
{
	m_cached_char_count = textlen;

	return g_svg_manager_impl->GetGlyphCache().GetGlyphsForWord(font, text, textlen,
																lookup_cxt, m_glyphs);
}

OP_STATUS GlyphRun::Append(OpVector<GlyphInfo>& glyphs)
{
	for (unsigned int i = 0; i < glyphs.GetCount(); ++i)
	{
		GlyphInfo* gi = glyphs.Get(i);
		OP_ASSERT(gi);

		RETURN_IF_ERROR(m_glyphs.Add(gi));
	}
	return OpStatus::OK;
}

unsigned int GlyphRun::CountChars()
{
	unsigned int char_count = 0;
	for (unsigned int i = 0; i < m_glyphs.GetCount(); i++)
	{
		const GlyphInfo* glyph_info = m_glyphs.Get(i);
		char_count += glyph_info->GetNumChars();
	}
	return char_count;
}

int GlyphRun::FindGlyphContaining(int char_idx)
{
	int current_idx = 0;
	for (unsigned int i = 0; i < m_glyphs.GetCount(); i++)
	{
		const GlyphInfo* glyph_info = m_glyphs.Get(i);

		current_idx += glyph_info->GetNumChars();
		if (current_idx > char_idx)
			return i;
	}
	return -1;
}

SVGNumber GlyphRun::GetExtent(unsigned int start_idx, unsigned int num_glyphs,
							  SVGNumber letter_spacing, SVGNumber font_to_user_scale,
							  BOOL skip_last_ls /* = TRUE */)
{
	OP_ASSERT(m_glyphs.GetCount() > 0);
	OP_ASSERT(num_glyphs <= m_glyphs.GetCount());
	OP_ASSERT(start_idx < m_glyphs.GetCount());
	OP_ASSERT(start_idx+num_glyphs <= m_glyphs.GetCount());

	SVGNumber total_width;
	if (num_glyphs > 0)
	{
		// extent = sum[i=si..li]{advance[i] + letter_spacing / compensation} * compensation
		// => extent = sum[i=si..li]{advance[i]) * compensation + letter_spacing * (li - si + 1)

		for (unsigned int i = 0; i < num_glyphs; i++)
		{
			const GlyphInfo* glyph_info = m_glyphs.Get(start_idx + i);
			total_width += glyph_info->GetAdvance();
		}

		total_width *= font_to_user_scale;

		unsigned int ls_count = num_glyphs;
		if (skip_last_ls)
			ls_count--; // Don't count the letter-spacing after the last glyph

		total_width += letter_spacing * ls_count;
	}
	return total_width;
}

static SVGGlyphLookupContext GetOrientationProperties(const HTMLayoutProperties& props, BOOL is_horizontal)
{
	// Extract and pack the interresting info into a struct
	SVGGlyphLookupContext lookup_cxt;

	lookup_cxt.horizontal = is_horizontal;

	const SvgProperties *svg_props = props.svg;

	if (is_horizontal)
	{
		lookup_cxt.is_auto = FALSE;
		lookup_cxt.orientation = svg_props->textinfo.glyph_orientation_horizontal;
	}
	else
	{
		if (svg_props->textinfo.glyph_orientation_vertical_is_auto)
		{
			lookup_cxt.is_auto = TRUE;
		}
		else
		{
			lookup_cxt.is_auto = FALSE;
			lookup_cxt.orientation = svg_props->textinfo.glyph_orientation_vertical;
		}
	}

	return lookup_cxt;
}

OP_STATUS SVGSmallCaps::ApplyTransform(uni_char* txt, int len, BOOL reverse_word)
{
	OP_STATUS err = OpStatus::OK;
	// handle "Word word wOrd WorD"
	while (len > 0 && GlyphsRemaining())
	{
		// find next place where uppercase switches to lowercase or vice versa
		unsigned segment_len = GetCapsSegmentLength(txt, len);

		if (uni_isupper(*txt))
		{
			// Check and set font
			RETURN_IF_ERROR(m_fontdesc->Check(m_font_doc_ctx, m_expansion, m_font_size));

			// draw last segment in BIG uppercase
			err = HandleSegment(txt, segment_len, reverse_word);
			if (OpStatus::IsError(err))
				break;
		}
		else
		{
			// draw last segment in small uppercase
			for (int j = segment_len; j; --j)
				txt[j - 1] = uni_toupper(txt[j - 1]);

			SVGNumber small_font_size = m_font_size * SVGNumber(0.800000);

			m_fontdesc->SetFontSize(small_font_size);
			RETURN_IF_ERROR(m_fontdesc->Check(m_font_doc_ctx, m_expansion, small_font_size));

			// FIXME: Fonts
			err = HandleSegment(txt, segment_len, reverse_word);
			if (OpStatus::IsError(err))
				break;

			m_fontdesc->SetFontSize(m_font_size);
			RETURN_IF_ERROR(m_fontdesc->Check(m_font_doc_ctx, m_expansion, m_font_size));
		}

		txt += segment_len;
		len -= segment_len;
	}
	return err;
}

class SVGSmallCapsTextRenderer : public SVGSmallCaps
{
public:
	SVGSmallCapsTextRenderer(SVGTextRenderer* text_renderer,
							 SVGFontDescriptor* fontdesc,
							 SVGDocumentContext* font_doc_ctx,
							 int glyph_count) :
		SVGSmallCaps(fontdesc, font_doc_ctx,
					 text_renderer->GetExpansion(),
					 text_renderer->GetFontSize(),
					 glyph_count),
		m_textrenderer(text_renderer),
		m_consumed_chars(0) {}

	int ConsumedChars() const { return m_consumed_chars; }

protected:
	SVGTextRenderer* m_textrenderer;
	int m_consumed_chars;
};

class SVGSmallCapsExtents : public SVGSmallCapsTextRenderer
{
public:
	SVGSmallCapsExtents(SVGTextRenderer* text_renderer,
						SVGFontDescriptor* fontdesc,
						SVGDocumentContext* font_doc_ctx,
						const SVGTextArguments& tparams,
						int glyph_count) :
		SVGSmallCapsTextRenderer(text_renderer, fontdesc, font_doc_ctx, glyph_count), m_tparams(tparams) {}

	SVGNumber GetExtents() const { return m_calculated_extent; }

protected:
	virtual OP_STATUS HandleSegment(const uni_char* txt, int len, BOOL reverse_word)
	{
		int consumed_chars = 1;
		int glyphcount = 0;
		m_calculated_extent += m_textrenderer->GetTxtExtent(txt, len, m_glyph_count,
															consumed_chars, glyphcount, m_tparams);
		m_consumed_chars += consumed_chars;
		return OpStatus::OK;
	}

	SVGNumber m_calculated_extent;
	const SVGTextArguments& m_tparams;
};

class SVGSmallCapsOutput : public SVGSmallCapsTextRenderer
{
public:
	SVGSmallCapsOutput(SVGTextRenderer* text_renderer,
					   SVGFontDescriptor* fontdesc,
					   SVGDocumentContext* font_doc_ctx,
					   SVGTextArguments& tparams,
					   int glyph_count) :
		SVGSmallCapsTextRenderer(text_renderer, fontdesc, font_doc_ctx, glyph_count), m_tparams(tparams) {}

protected:
	virtual OP_STATUS HandleSegment(const uni_char* txt, int len, BOOL reverse_word)
	{
		int consumed_chars = 1;
		OP_STATUS status = m_textrenderer->TxtOut(txt, len, m_tparams, reverse_word,
												  m_glyph_count, consumed_chars);
		m_consumed_chars += consumed_chars;
		return status;
	}

	SVGTextArguments& m_tparams;
};

OP_STATUS SVGTextRenderer::TxtOutEx(const uni_char* txt, unsigned len, int& consumed_chars,
									int max_glyph_count, int format, SVGTextArguments& tparams)
{
	FragmentTransform fragxfrm;
	RETURN_IF_ERROR(fragxfrm.Apply(txt, len, format));

	uni_char* txt_out = fragxfrm.GetTransformed();
	len = fragxfrm.GetTransformedLength();

	int local_max_glyph_count = MIN((int)len, max_glyph_count);

	OP_STATUS err = OpStatus::OK;
	if (txt_out && len)
	{
		BOOL reverse_word = (format & TEXT_FORMAT_REVERSE_WORD) != 0;

		if (format & TEXT_FORMAT_SMALL_CAPS)
		{
			SVGSmallCapsOutput sc_txtout(this, m_fontdesc, m_font_doc_ctx, tparams, local_max_glyph_count);
			err = sc_txtout.ApplyTransform(txt_out, len, reverse_word);

			consumed_chars = sc_txtout.ConsumedChars();
		}
		else
		{
			// Check and set font
			err = m_fontdesc->Check(m_font_doc_ctx, GetExpansion(), GetFontSize());

			if (OpStatus::IsSuccess(err))
				err = TxtOut(txt_out, len, tparams, reverse_word, local_max_glyph_count, consumed_chars);
		}
	}
	return err;
}

OP_STATUS SVGTextRenderer::TxtOut(const uni_char* txt, int len, SVGTextArguments& tparams,
								  BOOL reverse_word, int max_glyph_count, int& out_consumed_chars)
{
	OP_NEW_DBG("SVGTextRenderer::TxtOut", "svg_text_draw");
	OP_ASSERT(txt);
	OP_ASSERT(max_glyph_count <= len);
	OP_STATUS err = OpStatus::OK;

#ifdef _DEBUG
	{
		TmpSingleByteString tmpstr;
		tmpstr.Set(txt, len);
		OP_DBG(("(%g,%g): \'%s\'",
				tparams.ctp.x.GetFloatValue(), tparams.ctp.y.GetFloatValue(),
				tmpstr.GetString()));
	}
#endif // _DEBUG

	if (len <= 0 || txt == NULL)
		return err;

	OpFont* current_font = m_fontdesc->GetFont();
	OP_ASSERT(current_font);

	if (GetFontSize() <= 0)
		return OpStatus::OK;

	BOOL is_svgfont = FALSE;
	BOOL renderWithPainter = !NeedsVectorFonts(tparams);
	if(renderWithPainter)
	{
		/**
		 * Text-rendering is a hint, so we will respect it only if possible:
		 * Font must be a systemfont (be accepted by DrawString), otherwise we can't
		 * draw with the painter.
		 */
		is_svgfont = OpFontIsSVGFont(current_font);
		if (is_svgfont)
		{
			// This font is a SVG font and the painter doesn't work with
			// those
			renderWithPainter = FALSE;
		}
	}
	else
	{
		// Check if the current font supports outlines by asking with a NULL path pointer
		// Note: if the platform returns ERR_NOT_SUPPORTED then fallback to using the painter, and it will look broken in many cases.
		SVGNumber adv;
		UINT32 strpos = 0;
		renderWithPainter = (current_font->GetOutline(txt, len, strpos, strpos, TRUE, adv, NULL) == OpStatus::ERR_NOT_SUPPORTED);
	}

	if (renderWithPainter)
	{
		// Only get the OpPainter once
		if (!m_canvas->IsPainterActive() &&
			m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_DRAW)
		{
			err = m_canvas->GetPainter(current_font->Overhang());

			RETURN_IF_MEMORY_ERROR(err);

			if (OpStatus::IsError(err))
				// We failed to acquire a painter from the
				// bitmap, make one last try with vector fonts
				renderWithPainter = FALSE;
		}
	}

	if(renderWithPainter)
	{
		len = MIN(max_glyph_count, len);
		out_consumed_chars = len;
		err = RenderSystemTextRun(txt, len, tparams, reverse_word);
	}
	else
	{
		GlyphRun grun(len);

		if (is_svgfont)
			static_cast<SVGFontImpl*>(current_font)->SetMatchLang(tparams.GetCurrentLang());

		SVGGlyphLookupContext glyph_props = GetOrientationProperties(*tparams.props, !tparams.IsVertical());
		err = grun.MapToGlyphs(current_font, txt, len, glyph_props);

		if (is_svgfont)
			static_cast<SVGFontImpl*>(current_font)->SetMatchLang(NULL);

		RETURN_IF_ERROR(err);

		// Truncate based on max_glyph_count
		unsigned int glyph_count = grun.GetGlyphCount();
		if (glyph_count > (unsigned)max_glyph_count)
			grun.Truncate(max_glyph_count);

		if (reverse_word)
			tparams.current_char_idx += len; // ??? len vs. consumed?

		err = RenderGlyphRun(grun, tparams, reverse_word);

		out_consumed_chars = grun.GetCharCount();
	}

	return err;
}

/**
 * Returns the unscaled width of a string. That is, if the author has
 * requested font-size 10px and the string is "XXX" where X is 1em
 * wide, then it will return 30, even if there is a scale that will
 * make us have a (for instance) 20px font in m_current_font.
 */
SVGNumber SVGTextRenderer::DrawStringWidth(const uni_char* text, unsigned len, SVGNumber letter_spacing, SVGNumber pixel_to_userscale)
{
#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
	if (TextShaper::NeedsTransformation(text, len))
	{
		uni_char *converted;
		int converted_len;
		if (OpStatus::IsSuccess(TextShaper::Prepare(text, len, converted, converted_len))) // FIXME: OOM
		{
			text = converted;
			len = converted_len;
		}
	}
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT

	letter_spacing /= m_fontdesc->GetFontToUserScale(); // Make it pixels, as StringWidth expects
	SVGNumber scaled_font_width = m_fontdesc->GetFont()->StringWidth(text, len, letter_spacing.GetIntegerValue());
	return scaled_font_width / pixel_to_userscale;
}

SVGNumber SVGTextRenderer::GetGlyphRunExtent(GlyphRun& grun, const SVGTextArguments& tparams)
{
	OP_NEW_DBG("SVGTextRenderer::GetGlyphRunExtent", "svg_text");

	SVGNumber extent = grun.GetExtent(0, grun.GetGlyphCount(), tparams.letter_spacing,
									  m_fontdesc->GetFontToUserScale());

#ifdef SVG_DEBUG_TEXT_DETAILED
	OP_DBG(("Scalefactor: %g. Width: %g. FontHeight: %d.",
			m_fontdesc->GetFontToUserScale().GetFloatValue(),
			extent.GetFloatValue(), m_fontdesc->GetFont()->Height()));
#endif // SVG_DEBUG_TEXT_DETAILED

	SVGTextData* data = tparams.data;
	if (data && data->SetExtent() && !data->range.IsEmpty())
	{
		int char_len = grun.GetCharCount();
		SVGTextRange isect = data->range;
		isect.IntersectWith(tparams.current_char_idx, tparams.current_char_idx + char_len);

		if (isect.length == char_len)
		{
			data->extent += extent;
		}
		else if (isect.length > 0)
		{
			// Calculate extent for intersection
			int ofs = isect.index - tparams.current_char_idx;
			SVGNumber partial_extent;
			int start_glyph_idx = grun.FindGlyphContaining(ofs);
			if (start_glyph_idx >= 0)
			{
				int end_glyph_idx = grun.FindGlyphContaining(ofs + isect.length - 1);
				partial_extent = grun.GetExtent(start_glyph_idx, end_glyph_idx - start_glyph_idx + 1,
												tparams.letter_spacing, m_fontdesc->GetFontToUserScale());
			}

			data->extent += partial_extent;
		}
	}

	BOOL horizontal = !tparams.IsVertical();
	if (data && data->SetStartPosition())
	{
		int char_len = grun.GetCharCount();
		int global_char_idx = tparams.total_char_sum + tparams.current_char_idx;
		if (data->range.Intersecting(global_char_idx, char_len))
		{
			int idx = data->range.index - global_char_idx;
			OP_ASSERT(idx >= 0 && idx < (int)char_len);

			data->startpos = tparams.ctp; // CTP including dx/dy

			SVGNumber partial_extent;
			if (idx > 0)
			{
				// Calculate extent for prefix
				// Let's assume there are no 0:N mappings - mkay?
				int matched_glyph = grun.FindGlyphContaining(idx);
				if (matched_glyph > 0) /* the extent for 0 glyphs will be zero */
					partial_extent = grun.GetExtent(0, matched_glyph, tparams.letter_spacing,
													m_fontdesc->GetFontToUserScale(), FALSE);
			}

			if (horizontal)
				data->startpos.x += partial_extent;
			else
				data->startpos.y += partial_extent;
		}
	}

	return extent;
}

SVGNumber SVGTextRenderer::GetSystemTextExtent(const uni_char* text, unsigned len,
											   const SVGTextArguments& tparams, SVGNumber pixel_to_user)
{
	OP_NEW_DBG("SVGTextRenderer::GetSystemTextExtent", "svg_text");

	OP_ASSERT(!tparams.IsVertical());

	SVGNumber extent = DrawStringWidth(text, len, tparams.letter_spacing, pixel_to_user);

#ifdef SVG_DEBUG_TEXT_DETAILED
	OP_DBG(("Scalefactor: %g. Width: %g. FontHeight: %d.",
			m_fontdesc->GetFontToUserScale().GetFloatValue(),
			extent.GetFloatValue(), m_fontdesc->GetFont()->Height()));
#endif // SVG_DEBUG_TEXT_DETAILED

	SVGTextData* data = tparams.data;
	if (data && data->SetExtent() && !data->range.IsEmpty())
	{
		SVGTextRange isect = data->range;
		isect.IntersectWith(tparams.current_char_idx, (int)len);

		if (isect.length == (int)len)
		{
			data->extent += extent;
		}
		else if (isect.length > 0)
		{
			// Calculate extent for intersection
			int ofs = isect.index - tparams.current_char_idx;
			SVGNumber partial_extent = DrawStringWidth(text + ofs, isect.length,
													   tparams.letter_spacing, pixel_to_user);

			data->extent += partial_extent;
		}
	}

	if (data && data->SetStartPosition())
	{
		int global_char_idx = tparams.total_char_sum + tparams.current_char_idx;
		if (data->range.Intersecting(global_char_idx, (int)len))
		{
			int idx = data->range.index - global_char_idx;
			OP_ASSERT(idx >= 0 && idx < (int)len);

			data->startpos = tparams.ctp; // CTP including dx/dy

			SVGNumber partial_extent;
			if (idx > 0)
			{
				// Calculate extent for prefix
				// FIXME: This is borked
				partial_extent = DrawStringWidth(text, idx, tparams.letter_spacing, pixel_to_user);
			}

			data->startpos.x += partial_extent;
		}
	}

	return extent;
}

SVGNumber SVGTextRenderer::GetTxtExtentEx(const uni_char* txt, unsigned len, int format,
										  int max_glyph_count, int& out_consumed_chars, 
										  int& out_glyphcount, const SVGTextArguments& tparams)
{
	FragmentTransform fragxfrm;
	RETURN_IF_ERROR(fragxfrm.Apply(txt, len, format));

	uni_char* txt_out = fragxfrm.GetTransformed();
	len = fragxfrm.GetTransformedLength();

	int local_max_glyph_count = MIN((int)len, max_glyph_count);

	SVGNumber extent;

	if (txt_out && len)
	{
		BOOL reverse_word = (format & TEXT_FORMAT_REVERSE_WORD) != 0;

		if (format & TEXT_FORMAT_SMALL_CAPS)
		{
			SVGSmallCapsExtents sc_extents(this, m_fontdesc, m_font_doc_ctx, tparams, local_max_glyph_count);
			OpStatus::Ignore(sc_extents.ApplyTransform(txt_out, len, reverse_word));

			out_consumed_chars = sc_extents.ConsumedChars();
			extent = sc_extents.GetExtents();
		}
		else
		{
			// Check and set font
			if (OpStatus::IsSuccess(m_fontdesc->Check(m_font_doc_ctx, GetExpansion(), GetFontSize())))
				extent = GetTxtExtent(txt_out, len, local_max_glyph_count, out_consumed_chars, out_glyphcount, tparams);
		}
	}

	return extent;
}

SVGNumber SVGTextRenderer::GetTxtExtent(const uni_char* text, unsigned len, int max_glyph_count,
										int& out_consumed_chars, int& out_glyphcount, const SVGTextArguments& tparams)
{
	OP_NEW_DBG("SVGTextRenderer::GetTxtExtent", "svg_text");

	OpFont* current_font = m_fontdesc->GetFont();
	OP_ASSERT(current_font);

	if (GetFontSize() <= 0)
		return 0;

	SVGNumber pixel_to_user = GetExpansion();
	if (pixel_to_user.Close(0))
		return 0;

	BOOL is_svgfont = OpFontIsSVGFont(current_font);
	BOOL use_dsw = !NeedsVectorFonts(tparams) && !is_svgfont;

	SVGNumber extent;
	if (!use_dsw)
	{
		if (is_svgfont)
			static_cast<SVGFontImpl*>(current_font)->SetMatchLang(tparams.GetCurrentLang());

		SVGGlyphLookupContext glyph_props = GetOrientationProperties(*tparams.props, !tparams.IsVertical());

		GlyphRun grun(len);
		OP_STATUS err = grun.MapToGlyphs(current_font, text, len, glyph_props);
		out_glyphcount = grun.GetGlyphCount();

		if(OpStatus::IsSuccess(err))
		{
			if (is_svgfont)
				static_cast<SVGFontImpl*>(current_font)->SetMatchLang(NULL);

			// Truncate based on max_glyph_count
			if (out_glyphcount > max_glyph_count)
			{
				grun.Truncate(max_glyph_count);
				out_glyphcount = grun.GetGlyphCount();
			}

			extent = GetGlyphRunExtent(grun, tparams);

			out_consumed_chars = grun.GetCharCount();
		}
		else if(out_glyphcount == 0)
		{
			use_dsw = TRUE;
		}
	}
	else
	{
		out_glyphcount = 0;
	}

	if(use_dsw)
	{
		len = MIN((unsigned)max_glyph_count, len);

		extent = GetSystemTextExtent(text, len, tparams, pixel_to_user);

		out_glyphcount = len;
		out_consumed_chars = len;
	}

	return extent;
}

void SVGTextRenderer::DrawStrikeThrough(SVGTextArguments& tparams, SVGNumber start_x, SVGNumber width)
{
	SVGNumber fontheight = GetFontSize();

	SVGNumber thickness, position;
	GetSVGFontAttribute(SVGFontData::STRIKETHROUGH_THICKNESS, fontheight, thickness);
	GetSVGFontAttribute(SVGFontData::STRIKETHROUGH_POSITION, fontheight, position);

	SVGNumber line_pos = tparams.ctp.y - tparams.baseline_shift - position - thickness/2;

	RETURN_VOID_IF_ERROR(m_canvas->SaveState());
	RETURN_VOID_IF_ERROR(m_canvas->SetDecorationPaint());

	m_canvas->DrawRect(start_x, line_pos, width, thickness, 0, 0);

	m_canvas->ResetDecorationPaint();
	m_canvas->RestoreState();
}

void SVGTextRenderer::DrawTextDecorations(SVGTextArguments& tparams, SVGNumber width)
{
	// Markup::SVGA_UNDERLINE_THICKNESS/Markup::SVGA_OVERLINE_THICKNESS (a property of the font),
	// arbitrary good looking number
	SVGNumber fontheight = GetFontSize();
	const HTMLayoutProperties& props = *tparams.props;

	RETURN_VOID_IF_ERROR(m_canvas->SaveState());
	RETURN_VOID_IF_ERROR(m_canvas->SetDecorationPaint());

	if (props.underline_color != USE_DEFAULT_COLOR)
	{
		SVGNumber thickness, position;
		GetSVGFontAttribute(SVGFontData::UNDERLINE_THICKNESS, fontheight, thickness);
		GetSVGFontAttribute(SVGFontData::UNDERLINE_POSITION, fontheight, position);

		SVGNumber linepos = tparams.ctp.y + tparams.baseline_shift - position - thickness/2;
		m_canvas->DrawRect(tparams.ctp.x, linepos, width, thickness, 0, 0);
	}

	if (props.overline_color != USE_DEFAULT_COLOR)
	{
		SVGNumber thickness, position;
		GetSVGFontAttribute(SVGFontData::OVERLINE_THICKNESS, fontheight, thickness);
		GetSVGFontAttribute(SVGFontData::OVERLINE_POSITION, fontheight, position);

		SVGNumber linepos = tparams.ctp.y + tparams.baseline_shift - position - thickness/2;
		m_canvas->DrawRect(tparams.ctp.x, linepos, width, thickness, 0, 0);
	}

	m_canvas->ResetDecorationPaint();
	m_canvas->RestoreState();
}

#ifdef SVG_SUPPORT_EDITABLE
static void SetupAndDrawCaret(SVGTextArguments& tparams, SVGCanvas* canvas,
							  OpFont* font,
							  SVGNumber advance, SVGNumber fontsize,
							  SVGNumber font_to_user_scale)
{
	// Caret in glyph coordinates
	SVGRect glyph_caret(0,
						-SVGNumber(font->Descent()),
						0 /* determined by drawing code */,
						fontsize / font_to_user_scale);
	// Caret in userspace
	SVGRect us_caret(tparams.ctp.x,
					 tparams.ctp.y + font_to_user_scale * font->Descent(),
					 0,
					 fontsize);

	if (tparams.DrawCaretOnRightSide())
	{
		glyph_caret.x = advance / font_to_user_scale;
		us_caret.x += advance; // FIXME: Letter-spacing?
	}

	tparams.editable->m_caret.m_pos = us_caret;
	tparams.editable->m_caret.m_glyph_pos = glyph_caret;
	tparams.editable->Paint(canvas);

	tparams.packed.has_painted_caret = 1;

	OP_ASSERT(!tparams.area ||
			  (tparams.area->lineinfo &&
			   tparams.area->current_line < tparams.area->lineinfo->GetCount()));

	tparams.editable->m_caret.m_line = tparams.area ? tparams.area->current_line : 0;
}
#endif // SVG_SUPPORT_EDITABLE

#ifdef SVG_SUPPORT_TEXTSELECTION
OP_STATUS SVGTextRenderer::ProcessSelection(const uni_char* text, unsigned len, BOOL reverse_word, const uni_char* orig_text,
										   SVGTextArguments& tparams, OpPoint int_pos, INT32 text_height)
{
	OpFont* opfont = m_fontdesc->GetFont();
#ifdef _DEBUG
	OP_ASSERT(!OpFontIsSVGFont(opfont));
#endif // _DEBUG
	int logical_start = tparams.current_char_idx;
	int logical_end = reverse_word ? (logical_start - len) : (logical_start + len);
	INT32 pixel_extra_spacing = (tparams.letter_spacing / m_fontdesc->GetFontToUserScale()).GetIntegerValue();
	BOOL has_more_ranges = FALSE;
	BOOL had_more_ranges;
	BOOL done_with_selection = FALSE;
	do
	{
		done_with_selection = !tparams.SelectionActive() || tparams.SelectionEmpty();
		had_more_ranges = has_more_ranges;

		if (!done_with_selection)
		{
			int word_bound = reverse_word ? logical_end : logical_start;
			int word_start_idx = MAX(0, tparams.selected_min_idx - word_bound);
			int word_stop_idx = MIN((int)len, tparams.selected_max_idx - word_bound);
			int selected_len = word_stop_idx - word_start_idx;

			if (selected_len > 0)
			{
				int start_x, stop_x;
				if (reverse_word)
				{
					start_x = opfont->StringWidth(&orig_text[word_start_idx], len - word_start_idx);
					stop_x = opfont->StringWidth(&orig_text[word_stop_idx], len - word_stop_idx);
					if (start_x > stop_x)
					{
						int tmp = start_x;
						start_x = stop_x;
						stop_x = tmp;
					}
				}
				else
				{
					OP_ASSERT((unsigned)selected_len <= len);
					start_x = opfont->StringWidth(text, word_start_idx, pixel_extra_spacing);
					stop_x = opfont->StringWidth(text, word_stop_idx, pixel_extra_spacing);
				}

				OpRect selrect(int_pos.x + start_x, int_pos.y,
								stop_x - start_x, text_height);

				OpVector<SVGRect>* list = tparams.data->selection_rect_list;
				if (list)
					RETURN_IF_ERROR(tparams.AddSelectionRect(selrect));
				else
					m_canvas->DrawSelectedStringPainter(selrect, OP_RGB(0,0,0), OP_RGB(255,255,255), opfont, int_pos, (uni_char*)text, len, pixel_extra_spacing);
			}
		}

		has_more_ranges = tparams.GetNextSelectionForFragment();

	} while (had_more_ranges || has_more_ranges);

	return OpStatus::OK;
}
#endif // SVG_SUPPORT_TEXTSELECTION

OP_STATUS SVGTextRenderer::RenderSystemTextRun(const uni_char* text, unsigned len,
											   SVGTextArguments& tparams, BOOL reverse_word)
{
	OP_NEW_DBG("RenderSystemTextRun", "svg_systemfonts");

	if (GetExpansion().Close(0))
		// Don't render text that should be invisible
		return OpStatus::OK;

	OpFont* opfont = m_fontdesc->GetFont();
#ifdef _DEBUG
	OP_ASSERT(!OpFontIsSVGFont(opfont));
#endif // _DEBUG

	if (reverse_word)
		tparams.current_char_idx += len;

	SVGTextData* data = tparams.data;
	const HTMLayoutProperties& props = *tparams.props;

	int logical_start = tparams.current_char_idx;
	int logical_end = reverse_word ? (logical_start - len) : (logical_start + len);

	// If visibility is not visible, we still need to update the CTP
	BOOL really_draw = m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_DRAW;
	BOOL allow_draw = m_canvas->AllowDraw(really_draw);

	INT32 pixel_extra_spacing = (tparams.letter_spacing / m_fontdesc->GetFontToUserScale()).GetIntegerValue();

#if defined(SVG_SUPPORT_TEXTSELECTION) || defined(SVG_SUPPORT_EDITABLE)
	const uni_char* orig_str = text;
#endif // SVG_SUPPORT_TEXTSELECTION || SVG_SUPPORT_EDITABLE
	OpString reversed_text; // Must have the same scope as |text| since |text| might point into reversed_text's buffer.
	if (reverse_word)
	{
		RETURN_IF_ERROR(reversed_text.Set(text, len));
		size_t slen = len;
		VisualDevice::TransformText(text, reversed_text.CStr(), slen, TEXT_FORMAT_REVERSE_WORD);
		len = (unsigned)slen;
		text = reversed_text.CStr();
	}

#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
	if (TextShaper::NeedsTransformation(text, len))
	{
		uni_char *converted;
		int converted_len;
		if (OpStatus::IsSuccess(TextShaper::Prepare(text, len, converted, converted_len))) // FIXME: OOM
		{
			text = converted;
			len = converted_len;
		}
	}
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT

	// Temporarily adjust CTP with baseline-shift (use this for
	// everything related to positioning)
	SVGNumberPair bsl_adj_ctp = tparams.ctp;
	if (tparams.baseline_shift.NotEqual(0))
		bsl_adj_ctp.y -= tparams.baseline_shift;

	// Reset painter position
	SVGNumberPair pixel_pos = m_canvas->GetTransform().ApplyToCoordinate(bsl_adj_ctp);

	// Note that y position is negated because OpPainter works
	// from top-left corner, not lower-left
	OpPoint int_pos(pixel_pos.x.GetIntegerValue(),
					pixel_pos.y.GetIntegerValue() - opfont->Ascent());

	OpRect pixel_rect;
	pixel_rect.x = int_pos.x;
	pixel_rect.y = int_pos.y;
	pixel_rect.height = opfont->Height();
	pixel_rect.width = opfont->StringWidth(text, len, pixel_extra_spacing);

	const OpRect& text_rect = pixel_rect;

#ifdef _DEBUG
	{
		TmpSingleByteString tmpstr;
		tmpstr.Set(text, len);
		OP_DBG(("[%s] Has Painter: %d. Screenbox: %d %d %d %d.",
				tmpstr.GetString(), (m_canvas && m_canvas->IsPainterActive()),
				text_rect.x, text_rect.y, text_rect.width, text_rect.height));
	}
#endif // _DEBUG

	// Since we require ctm[0] == ctm[3] to take this codepath, we can
	// use one or the other here.
	SVGNumber recip_axis_scale = SVGNumber(1) / m_canvas->GetTransform()[0];
	// Userspace advance calculation
	SVGNumber user_width = recip_axis_scale * pixel_rect.width;

	if (tparams.is_layout || m_calculate_bbox)
	{
		// Layout / BBox - AKA: no real drawing
		if (tparams.is_layout)
		{
			m_canvas->AddToDirtyRect(text_rect);
		}
	}
	else if (m_canvas->IsPainterActive() && allow_draw)
	{
		// "All text decorations except line-through should be
		// drawn before the text is filled and stroked; thus,
		// the text is rendered on top of these decorations."
		if (props.underline_color != USE_DEFAULT_COLOR ||
			props.overline_color != USE_DEFAULT_COLOR)
			DrawTextDecorations(tparams, user_width);

		int* caret_x = NULL;
#ifdef SVG_SUPPORT_EDITABLE
		int caret_x_actual;

		if (tparams.DrawCaret((int)len) && tparams.packed.has_painted_caret == 0)
		{
			int caret_ofs = tparams.editable->m_caret.m_point.ofs;
			if(caret_ofs == tparams.current_char_idx)
			{
				caret_x_actual = 0;
			}
			else
			{
				int ofs = caret_ofs - tparams.current_char_idx;
				ofs = MIN((int)len, ofs);
				caret_x_actual = opfont->StringWidth(orig_str, ofs) - 1;
			}

			tparams.editable->m_caret.m_pos.x = bsl_adj_ctp.x;
			tparams.editable->m_caret.m_pos.x += recip_axis_scale * caret_x_actual;
			tparams.editable->m_caret.m_pos.y = bsl_adj_ctp.y;
			tparams.editable->m_caret.m_pos.y += recip_axis_scale * opfont->Descent();
			tparams.editable->m_caret.m_pos.height = GetFontSize();

			tparams.packed.has_painted_caret = 1;

			OP_ASSERT(!tparams.area ||
					  (tparams.area->lineinfo &&
					   tparams.area->current_line < tparams.area->lineinfo->GetCount()));

			tparams.editable->m_caret.m_line = tparams.area ? tparams.area->current_line : 0;
			if (tparams.editable->m_caret.m_on)
				caret_x = &caret_x_actual;
		}
#endif // SVG_SUPPORT_EDITABLE

		m_canvas->DrawStringPainter(opfont, int_pos, (uni_char*)text, len, pixel_extra_spacing, caret_x);

		// Draw line-through decoration if applicable
		if (props.linethrough_color != USE_DEFAULT_COLOR)
			DrawStrikeThrough(tparams, tparams.ctp.x, user_width);
#ifdef SVG_SUPPORT_TEXTSELECTION
		RETURN_IF_ERROR(ProcessSelection(text, len, reverse_word, orig_str, tparams, int_pos, text_rect.height));
#endif // SVG_SUPPORT_TEXTSELECTION
	}
#ifdef SVG_SUPPORT_TEXTSELECTION
	else if (data && data->SetSelectionRectList())
	{
		RETURN_IF_ERROR(ProcessSelection(text, len, reverse_word, orig_str, tparams, int_pos, text_rect.height));
	}
#endif // SVG_SUPPORT_TEXTSELECTION

	// Bounding box
	// Most of this is actually unnecessary if we only want to draw the text
	SVGRect user_rect(-m_canvas->GetTransform()[4] + pixel_rect.x,
					  -m_canvas->GetTransform()[5] + pixel_rect.y,
					  user_width,
					  recip_axis_scale * pixel_rect.height);

	user_rect.x *= recip_axis_scale;
	user_rect.y *= recip_axis_scale;

	tparams.bbox.Add(user_rect);

	if (data && data->SetBBox() && !data->range.IsEmpty())
	{
		SVGTextRange isect = data->range;
		isect.IntersectWith(tparams.current_char_idx + tparams.total_char_sum, (int)len);

		if (isect.length > 0)
		{
			// Calculate bbox for intersecting glyphs
			int ofs = isect.index - tparams.current_char_idx - tparams.total_char_sum;
			int width_pix = opfont->StringWidth(text + ofs, isect.length, pixel_extra_spacing);
			int x_ofs = ofs ? opfont->StringWidth(text, ofs, pixel_extra_spacing) : 0;

			SVGRect glyph_bbox(user_rect);
			glyph_bbox.width = recip_axis_scale * width_pix;
			glyph_bbox.x += recip_axis_scale * x_ofs;

			data->bbox.UnionWith(glyph_bbox);
		}
	}

	tparams.ctp.x += user_width;

	// Intersection testing
	if (allow_draw && m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_INTERSECT_POINT)
	{
		const OpPoint& point = m_canvas->GetIntersectionPointInt();

		OpRect screenbox(text_rect);
#ifdef SVG_SUPPORT_STENCIL
		m_canvas->ApplyClipToRegion(screenbox);
#endif // SVG_SUPPORT_STENCIL

		if (!screenbox.IsEmpty() && screenbox.Contains(point))
		{
			int x_pos = point.x - text_rect.x;
			if (reverse_word)
				x_pos = text_rect.width - x_pos;

			for (unsigned int i = 0; i < len; i++)
			{
				INT32 char_w = opfont->StringWidth(&text[i], 1, pixel_extra_spacing);
				INT32 x = opfont->StringWidth(text, i);
				if (x_pos <= x + char_w / 2)
				{
					logical_end = tparams.current_char_idx + i;
					break;
				}
			}

			m_canvas->SetCurrentElementIndex(logical_start, logical_end);
			m_canvas->SetLastIntersectedElement();
			m_canvas->SetLastIntersectedElementHitEndHalf(TRUE);

			if (data && data->SetCharNumAtPosition())
			{
				data->range.index = logical_end;
			}
		}
	}

	tparams.current_glyph_idx += len;

	if (!reverse_word)
		tparams.current_char_idx += len;

	return OpStatus::OK;
}

struct TextRunInfo
{
	SVGNumberPair startpoint_trans;
	SVGNumber font_to_user_scale;
	SVGNumber fontsize;
	SVGNumber letter_spacing;
	SVGNumber font_to_user_and_glyph_scale;
	BOOL draw_to_canvas;
	BOOL allow_draw;
	BOOL update_bbox;
	BOOL is_vertical;
};

struct GlyphOutline
{
	GlyphOutline() : outline_cell(NULL), owns_outline(FALSE) {}
	~GlyphOutline() { Clean(); }

	void SetOutline(const OpBpath* o, BOOL take_ownership = TRUE)
	{
		if (owns_outline)
			OP_DELETE(const_cast<OpBpath*>(outline));
		outline = o;
		owns_outline = take_ownership;
	}
#ifdef SVG_FULL_11
	OP_STATUS CreateCellPath()
	{
		OP_ASSERT(has_cell);

		OpBpath* gcell;
		RETURN_IF_ERROR(OpBpath::Make(gcell, FALSE, 5));
		SetOutlineCell(gcell);

		RETURN_IF_ERROR(gcell->MoveTo(cell.x, cell.y, FALSE));
		RETURN_IF_ERROR(gcell->LineTo(0, cell.height, TRUE));
		RETURN_IF_ERROR(gcell->LineTo(cell.width, 0, TRUE));
		RETURN_IF_ERROR(gcell->LineTo(0, -cell.height, TRUE));
		return gcell->Close();
	}
#endif // SVG_FULL_11
	void SetOutlineCell(OpBpath* oc)
	{
		OP_DELETE(outline_cell);
		outline_cell = oc;
	}
	void Clean()
	{
		SetOutline(NULL, FALSE);
		SetOutlineCell(NULL);
		has_cell = FALSE;
	}

	SVGRect cell;

	const OpBpath* outline;
	OpBpath* outline_cell;

	SVGNumber advance;
	SVGNumber rotation;

	int logical_start;
	int logical_end;

	BOOL visible;
	BOOL selected;
	BOOL has_cell;

	BOOL owns_outline;
};

//
// Transforms (mostly translations) and their order of application:
//
// ctp - Translates to current CTP
//
// startpoint_trans - Text Startpoint modification
//                    Principal use is for vertical text
//
// pathtrans - Position and orientation on 'textPath'
//
// rottrans - Supplemental rotation transform (i.e. 'rotate')
//
// text_scale{_xfrm} - Glyph coordinate system fixup
//              		(Reversed y-axis, scale compensation)
//
// For non-textPaths:
//
// T(ctp) * T(startpoint) * M(rotate) * T(baseline-shift) * S(text_scale)
//
// For textPaths:
//
// M(pathtrans) * T(startpoint) * M(rotate) * T(baseline-shift) * S(text_scale)
//
void SVGTextRenderer::CalculateGlyphAdjustment(const TextRunInfo& tri,
											   SVGMatrix& ustrans, GlyphOutline& go,
											   const SVGNumberObject* rotate)
{
	SVGNumber rotate_val;
	if (rotate)
		rotate_val = rotate->value;

	if (go.rotation > 0)
		rotate_val += go.rotation;

	if (rotate_val.NotEqual(0))
	{
		SVGMatrix rottrans;
		rottrans.LoadRotation(rotate_val);

		ustrans.Multiply(rottrans);
	}

	// The startpoint translation will only be non-zero for vertical text
	OP_ASSERT(tri.is_vertical || tri.startpoint_trans.x.Equal(0) && tri.startpoint_trans.y.Equal(0));

	if (tri.is_vertical)
	{
		SVGNumberPair startpoint_trans = tri.startpoint_trans;

		if (go.rotation.Equal(90) || go.rotation.Equal(270))
		{
			startpoint_trans.x = -startpoint_trans.y / 2;
			startpoint_trans.y = -startpoint_trans.x / 2;
		}

		ustrans.MultTranslation(startpoint_trans.x, startpoint_trans.y);
	}
}

#ifdef SVG_FULL_11
void SVGTextRenderer::CalculateGlyphPositionOnPath(const TextRunInfo& tri, SVGMatrix& ustrans,
												   GlyphOutline& go, SVGMotionPath* path,
												   BOOL warp_outline, SVGNumber path_pos,
												   SVGNumber half_advance, SVGNumber path_displacement)
{
	// Don't draw glyphs off either end of the path
	if (path_pos < 0 || path_pos > path->GetLength())
	{
		go.visible = FALSE;
	}
	else
	{
		if (warp_outline)
		{
			if (go.outline)
			{
				// Warp the glyph
				OpBpath* warped = NULL;
				if (OpStatus::IsSuccess(path->Warp(go.outline, &warped, path_pos,
												   tri.font_to_user_scale, half_advance)))
					go.SetOutline(warped);
			}
			if (go.has_cell && OpStatus::IsSuccess(go.CreateCellPath()))
			{
				// Warp the glyph (cell)
				OpBpath* warped = NULL;
				if (OpStatus::IsSuccess(path->Warp(go.outline_cell, &warped, path_pos,
												   tri.font_to_user_scale, half_advance)))
				{
					go.SetOutlineCell(warped);
					go.cell = go.outline_cell->GetBoundingBox().GetSVGRect();
				}
			}
		}

		// pathtrans = pathT * T(-half_advance, 0) * T(pathDisplacement)
		SVGMatrix pathtrans;
		path->CalculateTransformAtDistance(path_pos, pathtrans);

		pathtrans.PostMultTranslation(-half_advance, path_displacement);

		ustrans.Multiply(pathtrans);
	}
}
#endif // SVG_FULL_11

OP_STATUS SVGTextRenderer::DrawOutline(const TextRunInfo& tri, const GlyphOutline& go,
									   SVGMatrix& ustrans, SVGTextArguments& tparams)
{
	BOOL draw_glyph = go.visible && tri.allow_draw;
	BOOL should_draw_selected = go.selected && tri.allow_draw;

	if (go.has_cell)
	{
		SVGMatrix saved_usxfrm = m_canvas->GetTransform();

		if (tri.is_vertical)
		{
			// Vertical alignment
			SVGMatrix aligntrans;
			aligntrans.LoadTranslation(-go.cell.width / 2, 0);
			m_canvas->ConcatTransform(aligntrans);
		}

		// Draw the glyph cell - used for intersection testing and selection
		SVGCanvasStateStack selectedcs;
		if (should_draw_selected)
		{
			RETURN_IF_ERROR(selectedcs.Save(m_canvas));
			m_canvas->SetFillColorRGB(0,0,0);
			m_canvas->SetFillOpacity(255);
			m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
			m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
		}

		if (draw_glyph && tparams.is_layout)
		{
			OP_NEW_DBG("SVGTextRenderer::DrawOutline", "svg_textselection");

			SVGRect tr = m_canvas->GetTransform().ApplyToRect(go.cell);
			OpRect pixel_glyph_box = tr.GetEnclosingRect();

			OP_DBG(("Drawing glyph cell [%g %g w%g h%g] Transformed rect: [%g %g w%g h%g].",
					go.cell.x.GetFloatValue(), go.cell.y.GetFloatValue(),
					go.cell.width.GetFloatValue(), go.cell.height.GetFloatValue(),
					tr.x.GetFloatValue(), tr.y.GetFloatValue(),
					tr.width.GetFloatValue(), tr.height.GetFloatValue()));

			m_canvas->AddToDirtyRect(pixel_glyph_box); // Add glyphcell to dirtyrect
		}

		if (!tri.update_bbox)
		{
			if (draw_glyph)
			{
				if (go.outline_cell)
					RETURN_IF_ERROR(m_canvas->DrawPath(go.outline_cell));
				else
					RETURN_IF_ERROR(m_canvas->DrawRect(go.cell.x, go.cell.y,
													   go.cell.width, go.cell.height, 0, 0));
			}

			if (m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_INTERSECT_POINT)
			{
				int start, end;
				m_canvas->GetLastIntersectedElementIndex(start, end);

				if (start == tparams.current_char_idx)
				{
					SVGNumberPair pt(m_canvas->GetIntersectionPoint());
					SVGNumberPair midpoint(go.advance/2, tri.fontsize/2);
					midpoint = m_canvas->GetTransform().ApplyToCoordinate(midpoint);

					BOOL hit_end_half = (pt.x >= midpoint.x);

					m_canvas->SetLastIntersectedElementHitEndHalf(hit_end_half);
				}
			}
		}

		m_canvas->GetTransform().Copy(saved_usxfrm);
	}

	// Draw the glyph
	OP_STATUS result = OpStatus::OK;
	if (draw_glyph || tri.update_bbox)
	{
		SVGBoundingBox glyph_bbox;
		if (go.outline)
			glyph_bbox = go.outline->GetBoundingBox();

		SVGCanvasStateStack selectedcs;
		if (should_draw_selected)
		{
			RETURN_IF_ERROR(selectedcs.Save(m_canvas));
			m_canvas->SetFillColorRGB(255,255,255);
			m_canvas->SetFillOpacity(255);
			m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
			m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
		}

		if (tri.is_vertical && go.outline)
		{
			// Vertical alignment (Use the outline bbox for this since we
			// don't want a selection to influence the adjustment)
			SVGNumber glyph_width = glyph_bbox.maxx - glyph_bbox.minx;
			SVGMatrix aligntrans;
			aligntrans.LoadTranslation(-glyph_width/2, 0);
			m_canvas->ConcatTransform(aligntrans);
			ustrans.PostMultTranslation(-glyph_width/2, 0);
		}

		if (tri.update_bbox && go.has_cell)
			// If we're computing the bounding box use the bounding box of the
			// cell. The cell has had the bounding box of the outline factored
			// in already (if the glyph had a outline), so it should always be
			// at least as large.
			glyph_bbox = go.cell;

		if (!glyph_bbox.IsEmpty())
		{
			glyph_bbox = ustrans.ApplyToNonEmptyBoundingBox(glyph_bbox);
			tparams.bbox.Add(glyph_bbox);
		}

		if (tri.draw_to_canvas && go.outline)
			result = m_canvas->DrawPath(go.outline);

		SVGTextData* data = tparams.data;
		if (data)
		{
			if (data->SetCharNumAtPosition())
			{
				int start, end;
				m_canvas->GetLastIntersectedElementIndex(start, end);
				if (start == go.logical_start && end == go.logical_end)
				{
					data->range.index = tparams.current_char_idx;
				}
			}
			if (data->SetBBox())
			{
				if (!data->range.IsEmpty() &&
					data->range.Contains(tparams.current_char_idx))
				{
					data->bbox.UnionWith(glyph_bbox);
				}
			}
			if (should_draw_selected && data->SetSelectionRectList())
				RETURN_IF_ERROR(tparams.AddSelectionRect(glyph_bbox.GetSVGRect()));
		}
	}
	return result;
}

void SVGTextRenderer::TextRunSetup(TextRunInfo& tri, SVGMatrix& glyph_adj_xfrm,
								   SVGTextArguments& tparams)
{
	// letter_spacing is in the same unit and coordinate system as advance
	tri.letter_spacing = tparams.letter_spacing;
	// Apply any additional letter spacing (from textLength)
	tri.letter_spacing += tparams.extra_spacing;

	// Text scale factor (glyph flipping/glyph scaling/font->user adjustment)
	SVGNumberPair text_scale(1, -1);

	if (tparams.glyph_scale.NotEqual(1))
	{
		if (tri.is_vertical)
		{
			// Vertical
			text_scale.y *= tparams.glyph_scale;
		}
		else
		{
			// Horizontal
			text_scale.x *= tparams.glyph_scale;
		}
	}

	tri.font_to_user_and_glyph_scale = tparams.glyph_scale;
	tri.font_to_user_scale = m_fontdesc->GetFontToUserScale();
	if (tri.font_to_user_scale.NotEqual(1))
	{
		// The glyph paths are scaled. Here we undo that scale
		text_scale = text_scale.Scaled(tri.font_to_user_scale);

		// Accumulate into the advance scaling factor
		tri.font_to_user_and_glyph_scale *= tri.font_to_user_scale;
	}

	// scale the linewidth, and don't divide by zero
	if (tri.font_to_user_scale > 0)
		m_canvas->SetLineWidth(m_canvas->GetStrokeLineWidth() / tri.font_to_user_scale);

	if (tri.is_vertical)
	{
		// Vertical
		tri.startpoint_trans.y = tri.font_to_user_scale * m_fontdesc->GetFont()->Ascent();
	}

	// The pointer events are hacky for text so we "patch"
	// the different flags to emulate that
	if (m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_INTERSECT_POINT)
	{
		OP_NEW_DBG("SVGTextRenderer::TextRunSetup", "svg_intersection");
		SVGPointerEvents intersection_mode = m_canvas->GetPointerEvents();

		if ((intersection_mode == SVGPOINTEREVENTS_VISIBLEPAINTED ||
			 intersection_mode == SVGPOINTEREVENTS_PAINTED) &&
			m_canvas->UseStroke())
			m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
		else if (intersection_mode == SVGPOINTEREVENTS_VISIBLEFILL ||
				 intersection_mode == SVGPOINTEREVENTS_VISIBLESTROKE)
			m_canvas->SetPointerEvents(SVGPOINTEREVENTS_VISIBLE);
		else if (intersection_mode == SVGPOINTEREVENTS_FILL ||
				 intersection_mode == SVGPOINTEREVENTS_STROKE)
			m_canvas->SetPointerEvents(SVGPOINTEREVENTS_ALL);

		OP_DBG(("Intersectionmode: %d. CanvasState new mode: %d",
				intersection_mode, m_canvas->GetPointerEvents()));
	}

	// Setup transform for glyph adjustment (scaling, baseline-shift)
	glyph_adj_xfrm.LoadScale(text_scale.x, text_scale.y);
	if (tparams.baseline_shift.NotEqual(0))
	{
		switch (tparams.writing_mode)
		{
		default:
		case SVGWRITINGMODE_LR:   /* ( 1, 0 ) */
		case SVGWRITINGMODE_LR_TB:
			glyph_adj_xfrm.MultTranslation(0, -tparams.baseline_shift);
			break;
		case SVGWRITINGMODE_RL:   /* (-1, 0 ) */
		case SVGWRITINGMODE_RL_TB:
			glyph_adj_xfrm.MultTranslation(0, tparams.baseline_shift);
			break;
		case SVGWRITINGMODE_TB:   /* ( 0, 1 ) */
		case SVGWRITINGMODE_TB_RL:
			glyph_adj_xfrm.MultTranslation(tparams.baseline_shift, 0);
			break;
		}
	}
}

OP_STATUS SVGTextRenderer::RenderGlyphRun(GlyphRun& grun, SVGTextArguments& tparams, BOOL reverse_word)
{
	OP_NEW_DBG("SVGTextRenderer::RenderGlyphRun", "svg_font");
	TextRunInfo tri;
	OpFont* vectorfont = m_fontdesc->GetFont();

	tri.update_bbox = tparams.is_layout || m_calculate_bbox;
	tri.draw_to_canvas = tparams.is_layout || !m_calculate_bbox;
	tri.is_vertical = tparams.IsVertical();

	// If visibility is not visible, we still need to update the CTP
	BOOL really_draw = m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_DRAW;
	tri.allow_draw = m_canvas->AllowDraw(really_draw);

	tri.fontsize = GetFontSize();
	OP_DBG(("Font height: %d. Canvas fontsize: %d. Renderer fontsize: %g. ",
			vectorfont->Height(), m_canvas->GetLogFont().GetHeight(),
			tri.fontsize.GetFloatValue()));

	const HTMLayoutProperties& props = *tparams.props;
	SVGNumber decoration_width, saved_ctp_x;
	if (tri.draw_to_canvas)
	{
		if (props.linethrough_color != USE_DEFAULT_COLOR ||
			props.underline_color != USE_DEFAULT_COLOR ||
			props.overline_color != USE_DEFAULT_COLOR)
		{
			unsigned int glyph_count = grun.GetGlyphCount();
			decoration_width = grun.GetExtent(0, glyph_count, tparams.letter_spacing,
											  m_fontdesc->GetFontToUserScale());

			decoration_width *= tparams.glyph_scale;
			decoration_width += tparams.extra_spacing * (glyph_count - 1);

			saved_ctp_x = tparams.ctp.x; // Used for line-through
		}

		// "All text decorations except line-through should be
		// drawn before the text is filled and stroked; thus,
		// the text is rendered on top of these decorations."
		if (props.underline_color != USE_DEFAULT_COLOR ||
			props.overline_color != USE_DEFAULT_COLOR)
		{
			// Hmm, underline on vertical text, what does that look like? This is likely to be a long
			// line extending horizontally from the first letter.
			DrawTextDecorations(tparams, decoration_width);
		}
	}

	SVGCanvasStateStack cs;
	RETURN_IF_ERROR(cs.Save(m_canvas));

	SVGMatrix glyph_adj_xfrm;
	TextRunSetup(tri, glyph_adj_xfrm, tparams);

	SVGMatrix saved_transform;
	m_canvas->GetTransform(saved_transform);

	GlyphOutline go;
	go.Clean();

	const SVGVectorStack& rotatelist = tparams.rotatelist;
	const SVGNumberObject *glob_rotate = static_cast<const SVGNumberObject*>(rotatelist.Get(tparams.total_char_sum + tparams.current_char_idx, TRUE));

	OP_STATUS result = OpStatus::OK;

	unsigned int glyph_count = grun.GetGlyphCount();
	for (unsigned int i = 0; i < glyph_count; i++, tparams.current_glyph_idx++)
	{
		const int glyph_index = reverse_word ? glyph_count - i - 1 : i;

		const GlyphInfo* glyph_info = grun.Get(glyph_index);
		OP_ASSERT(glyph_info);
		go.SetOutline(static_cast<const OpBpath*>(glyph_info->GetSVGPath()),
					  FALSE /* cache or glyphinfo-vector owns outline */);

		// This is already scaled since we ask for a font_size*scale
		// font. Maybe we should have a neutral unit here.
		go.advance = glyph_info->GetAdvance();
		go.advance *= tri.font_to_user_and_glyph_scale;

		go.rotation = glyph_info->GetRotation();

		if (reverse_word)
			tparams.current_char_idx -= glyph_info->GetNumChars();

		if (tparams.SelectionActive())
		{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
			if (tparams.current_char_idx >= tparams.selected_max_idx)
				tparams.GetNextSelection(tparams.doc_ctx, tparams.current_element, *tparams.props);
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
			go.selected = (tparams.current_char_idx >= tparams.selected_min_idx &&
						   tparams.current_char_idx < tparams.selected_max_idx);
		}
		else
		{
			go.selected = FALSE;
		}

#ifdef _DEBUG
		{
#ifdef SVG_DEBUG_TEXTSELECTION
			OP_NEW_DBG("RenderTextRunWithVectorFont", "svg_textselection");
#endif // SVG_DEBUG_TEXTSELECTION
			OP_DBG(("Logical char idx: %d. Glyph idx: %d: %s.",
					tparams.current_char_idx, tparams.current_glyph_idx,
					go.selected ? " is selected" : " is not selected"));
		}
#endif // _DEBUG

		go.logical_start = tparams.current_char_idx;
		go.logical_end = tparams.current_char_idx;

#ifdef SVG_SUPPORT_TEXTSELECTION
		{
			go.logical_end += glyph_info->GetNumChars();
			if (go.logical_end < 0)
				go.logical_end = 0;
		}
#endif // SVG_SUPPORT_TEXTSELECTION

		m_canvas->SetCurrentElementIndex(go.logical_start, go.logical_end);

		if (m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_INTERSECT_POINT ||
			tri.update_bbox || go.selected)
		{
			// For text we intersect with a letter if we are in that letter's "cell".
			// We emulate the cell by making a filled rect for the
			// whole glyph's area

			// Making a "fake" glyph so it will have to be in glyph coordinates
			go.cell.x = 0;
			go.cell.y = -SVGNumber(vectorfont->Descent());
			go.cell.width = go.advance / tri.font_to_user_scale; // Too simple?
			go.cell.height = tri.fontsize / tri.font_to_user_scale;

			if (go.outline)
			{
				SVGBoundingBox bbox = go.outline->GetBoundingBox();
				bbox.UnionWith(go.cell);
				go.cell = bbox.GetSVGRect();
			}

			go.has_cell = TRUE;
		}

		SVGMatrix ustrans(glyph_adj_xfrm);

		go.visible = TRUE;
		if (go.rotation > 0 || glob_rotate || tri.is_vertical)
		{
			const SVGNumberObject *rotate = static_cast<const SVGNumberObject*>(rotatelist.Get(tparams.total_char_sum + tparams.current_char_idx, TRUE));

			if (rotate)
			{
				SVGTextData* data = tparams.data;
				if (data && data->SetAngle() && tparams.current_glyph_idx == data->range.index)
					data->angle += rotate->value;

				tparams.EmitRotation(rotate->value);
			}

			CalculateGlyphAdjustment(tri, ustrans, go, rotate);
		}

#ifdef SVG_FULL_11
		if (tparams.path)
		{
			SVGNumber half_advance = go.advance / 2;
			SVGNumber path_pos = tparams.pathDistance + half_advance;

			SVGTextData* data = tparams.data;
			if (data && data->SetAngle() && tparams.current_glyph_idx == data->range.index)
			{
				data->angle += tparams.path->CalculateRotationAtDistance(path_pos) * 180 / SVGNumber::pi();
			}

			CalculateGlyphPositionOnPath(tri, ustrans, go, tparams.path,
										 tparams.packed.method == SVGMETHOD_STRETCH,
										 path_pos, half_advance, tparams.pathDisplacement);
		}
		else
#endif // SVG_FULL_11
		{
			ustrans.MultTranslation(tparams.ctp.x, tparams.ctp.y);
		}

		m_canvas->GetTransform().Copy(saved_transform);
		m_canvas->ConcatTransform(ustrans);

		result = DrawOutline(tri, go, ustrans, tparams);

#ifdef SVG_SUPPORT_EDITABLE
		if (tparams.packed.has_painted_caret == 0 && tparams.DrawCaret())
		{
			SetupAndDrawCaret(tparams, m_canvas, vectorfont,
							  go.advance, tri.fontsize, tri.font_to_user_scale);
		}
#endif // SVG_SUPPORT_EDITABLE

		if (OpStatus::IsError(result))
			break;

		// Advance to next glyph position
		tparams.AdvanceCTP(go.advance + tri.letter_spacing);

		if (!reverse_word)
			tparams.current_char_idx += glyph_info->GetNumChars();

		go.Clean();
	}

	if (OpStatus::IsSuccess(result))
	{
		result = cs.Restore();
	}

	if (tri.draw_to_canvas)
	{
		if (props.linethrough_color != USE_DEFAULT_COLOR)
			DrawStrikeThrough(tparams, saved_ctp_x, decoration_width);
	}

	return result;
}

SVGNumber SVGTextRenderer::GetAlternateGlyphExtent(OpVector<GlyphInfo>& glyphs,
												   SVGTextArguments& tparams)
{
	OP_NEW_DBG("SVGTextRenderer::GetAlternateGlyphExtent", "svg_text");

	if (GetFontSize() <= 0)
		return 0;

	SVGNumber pixel_to_user = GetExpansion();
	if (pixel_to_user.Close(0))
		return 0;

	GlyphRun grun(glyphs.GetCount());
	RETURN_VALUE_IF_ERROR(grun.Append(glyphs), 0);

	SVGNumber extent = GetGlyphRunExtent(grun, tparams);

	tparams.current_glyph_idx += grun.GetGlyphCount();

	return extent;
}

OP_STATUS SVGTextRenderer::RenderAlternateGlyphs(OpVector<GlyphInfo>& glyphs,
												 SVGTextArguments& tparams)
{
	GlyphRun grun(glyphs.GetCount());
	RETURN_IF_ERROR(grun.Append(glyphs));

	return RenderGlyphRun(grun, tparams, FALSE);
}

/**
 * Algorithm:
 * 1) Lookup the glyphs corresponding to the word, and assume that the
 *    text is horizontal and that glyph-orientation-horizontal doesn't
 *    apply
 * 2) Apply transforms to make the text appear correctly (lower left
 *    corner, not upside-down)
 * 3) For each glyph:
 *    3.1) Render path
 *    3.2) Advance the drawing position
 */
/* static */
OP_STATUS SVGTextRenderer::SVGFontTxtOut(SVGPainter* painter, OpFont* svgfont, const uni_char* txt, int len, int char_spacing_extra/* = 0*/)
{
	OpVector<GlyphInfo> glyphs;
	SVGGlyphLookupContext lookup_cxt;

	lookup_cxt.horizontal = TRUE;
	lookup_cxt.orientation = 0;

	RETURN_IF_ERROR(g_svg_manager_impl->GetGlyphCache().GetGlyphsForWord(svgfont, txt, len, lookup_cxt, glyphs));

	SVGMatrix ctm = painter->GetTransform();
	ctm.PostMultTranslation(0, svgfont->Ascent());
	ctm.PostMultScale(1, -1);

	unsigned int glyph_count = glyphs.GetCount();
	for (unsigned int i = 0; i < glyph_count; i++)
	{
		painter->SetTransform(ctm);

		const GlyphInfo* glyph_info = glyphs.Get(i);
		OP_ASSERT(glyph_info);

		const SVGPath* svg_path = glyph_info->GetSVGPath();
		if (svg_path)
			RETURN_IF_ERROR(painter->DrawPath(static_cast<const OpBpath*>(svg_path)));

		// Advance to next glyph position
		ctm.PostMultTranslation(glyph_info->GetAdvance() + char_spacing_extra, 0);
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS SVGTextRenderer::SVGFontGetStringBoundingBox(SVGBoundingBox *bb_out, OpFont* svgfont, const uni_char* txt, int len, int char_spacing_extra/* = 0*/)
{
	OpVector<GlyphInfo> glyphs;
	SVGGlyphLookupContext lookup_cxt;

	lookup_cxt.horizontal = TRUE;
	lookup_cxt.orientation = 0;

	RETURN_IF_ERROR(g_svg_manager_impl->GetGlyphCache().GetGlyphsForWord(svgfont, txt, len, lookup_cxt, glyphs));

	bb_out->Clear();
	SVGNumber x_pos(0);

	unsigned int glyph_count = glyphs.GetCount();
	for (unsigned int i = 0; i < glyph_count; i++)
	{
		const GlyphInfo* glyph_info = glyphs.Get(i);
		OP_ASSERT(glyph_info);

		const SVGPath* svg_path = glyph_info->GetSVGPath();
		if (svg_path)
		{
			SVGBoundingBox glyph_bb = (static_cast<const OpBpath*>(svg_path))->GetBoundingBox();
			glyph_bb.minx += x_pos;
			glyph_bb.maxx += x_pos;
			bb_out->UnionWith(glyph_bb);
		}

		// Advance to next glyph position
		x_pos += glyph_info->GetAdvance() + char_spacing_extra;
	}

	return OpStatus::OK;
}

#if 0
void SVGTextRenderer::RenderArbitrarySVGGlyph(SVGTraversalObject* traversal_object,
											  HTML_Element* glyph_element)
{
	BOOL old_layer_hint = canvas->GetLayerHint();
	// Don't use layer hint, because the layout boxes might not be correct
	canvas->UseLayerHint(FALSE);

	OP_STATUS status = SVGTraverser::Traverse(traversal_object, glyph_element,
											  NULL /* or build on the current cascade - can't recall */);

	// Restore layer hint
	canvas->UseLayerHint(old_layer_hint);

	return status;
}
#endif

void SVGFontDescriptor::SetFontNumber(int font_number)
{
	m_fontinfo.SetFontNumber(font_number);
}

void SVGFontDescriptor::Release()
{
	if (m_font == NULL)
		return;

	// We don't want to have m_font set when calling ReleaseFont
	// since that might confuse the releasing code
	// (finding a font active while it should have been unused)
	OpFont* font_to_release = m_font;
	m_font = NULL;

	g_svg_manager_impl->ReleaseFont(font_to_release);
}

#endif // SVG_SUPPORT
