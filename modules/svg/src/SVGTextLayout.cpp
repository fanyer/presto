/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGTextLayout.h"
#include "modules/svg/src/SVGTextTraverse.h"
#include "modules/svg/src/SVGTextRenderer.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/svgpaintnode.h"

#include "modules/logdoc/src/textdata.h"
#include "modules/layout/box/box.h"

#include "modules/unicode/unicode.h"
#include "modules/unicode/unicode_bidiutils.h"

#define SVG_TXTOUT_USE_TMPBUFFER

#include "modules/svg/src/AttrValueStore.h"

struct GlyphAbsPosProps
{
	void Fetch(SVGTextArguments& tparams, unsigned int glyph_idx)
	{
		OP_NEW_DBG("GlyphAbsPosProps", "svg_text_abspos");
		const SVGVectorStack& xlist = tparams.xlist;
		const SVGVectorStack& ylist = tparams.ylist;

		xl = static_cast<const SVGLengthObject*>(xlist.Get(glyph_idx));
		yl = static_cast<const SVGLengthObject*>(ylist.Get(glyph_idx));

		OP_DBG(("Fetched index: %d : xl: %p yl: %p",
				glyph_idx, xl, yl));
	}

	void Fetch(SVGTextArguments& tparams)
	{
		Fetch(tparams, tparams.total_char_sum + tparams.current_char_idx);
	}

	void Preload(SVGTextArguments& tparams, int consumed_chars)
	{
		Fetch(tparams, tparams.total_char_sum + tparams.current_char_idx + consumed_chars);
	}

	void Resolve(const SVGValueContext& vcxt)
	{
		if (xl)
			pos.x = SVGUtils::ResolveLength(xl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

		if (yl)
			pos.y = SVGUtils::ResolveLength(yl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);
	}

	void Apply(SVGTextArguments& tparams);

	BOOL HasPos() const { return !!xl || !!yl; }

	SVGNumberPair pos;

	const SVGLengthObject *xl;
	const SVGLengthObject *yl;
};

void GlyphAbsPosProps::Apply(SVGTextArguments& tparams)
{
	if (xl)
		tparams.ctp.x = pos.x;
	if (yl)
		tparams.ctp.y = pos.y;

#ifdef SVG_FULL_11
	if (tparams.path)
	{
		BOOL is_vertical = tparams.IsVertical();
		if (xl && !is_vertical)
		{
			// horizontal text-direction
			// XXX pathDistance is in path coordinates. What is pos in?
			tparams.pathDistance = tparams.ctp.x;
		}
		else if (yl && is_vertical)
		{
			// vertical text-direction
			// XXX pathDistance is in path coordinates. What is pos in?
			tparams.pathDistance = tparams.ctp.y;
		}
	}
#endif // SVG_FULL_11
}

void SVGAltGlyphTraverser::HandlePositioning(SVGTextArguments& tparams)
{
	GlyphAbsPosProps gpprops;
	tparams.current_char_idx = 0;
	gpprops.Fetch(tparams);

	if (gpprops.HasPos())
	{
		gpprops.Resolve(m_value_context);
		gpprops.Apply(tparams);

		// Handle alignment et.c below
		tparams.force_chunk = TRUE;
	}

	if (tparams.force_chunk)
		HandleChunk(tparams);
}

void SVGAltGlyphTraverser::HandleChunk(SVGTextArguments& tparams)
{
	// Adjust with current chunk extent
	tparams.AdjustCTP();

	tparams.packed.check_ellipsis = 0;

	if (tparams.is_layout)
	{
		// Should we apply ellipsis for <altGlyph>? Sounds weird.
		if (tparams.packed.use_ellipsis)
		{
			SVGTextChunk* chunk = tparams.chunk_list->Get(tparams.current_chunk);
			SVGNumber chunk_extent = chunk->extent;

			if (tparams.current_extent + chunk_extent >= tparams.max_extent)
				tparams.packed.check_ellipsis = 1;
			else
				tparams.current_extent += chunk_extent;
		}
	}

	tparams.current_chunk++;

	tparams.force_chunk = FALSE;
}

OP_STATUS SVGAltGlyphTraverser::HandleGlyphs(OpVector<GlyphInfo>& glyphs, SVGTextArguments& tparams)
{
	if (tparams.is_layout && m_paint_node)
		m_paint_node->SetPosition(tparams.ctp);

	return m_textrenderer->RenderAlternateGlyphs(glyphs, tparams);
}

void SVGAltGlyphExtentTraverser::HandleChunk(SVGTextArguments& tparams)
{
	// Fundamentally the same as SVGTextExtentTraverser::HandleChunk
	SVGTextData* data = tparams.data;

	// Adjust with current chunk extent, then start new one
	if (data && data->SetExtent() && data->chunk_list)
	{
		BOOL increment_chunk = data->chunk_list->GetCount() > 0;

		RETURN_VOID_IF_ERROR(tparams.NewChunk(*data->chunk_list));

		if (increment_chunk)
			tparams.current_chunk++;
	}
	else if (tparams.chunk_list)
	{
		tparams.AdjustCTP();

		tparams.current_chunk++;
	}

	tparams.force_chunk = FALSE;
}

OP_STATUS SVGAltGlyphExtentTraverser::HandleGlyphs(OpVector<GlyphInfo>& glyphs, SVGTextArguments& tparams)
{
	SVGNumber extent = m_textrenderer->GetAlternateGlyphExtent(glyphs, tparams);
	tparams.AdvanceCTPExtent(extent);
	return OpStatus::OK;
}

OP_STATUS SVGAltGlyphTraverser::Traverse(OpVector<GlyphInfo>& glyphs, SVGElementInfo& info,
										 SVGTextArguments& tparams)
{
	const HTMLayoutProperties& props = *info.props->GetProps();

	m_textrenderer->SetFontSize(props.svg->GetFontSize(info.layouted));

	tparams.UpdateContext(m_doc_ctx, info);
	tparams.SetRotationKeeper(static_cast<SVGAltGlyphNode*>(info.paint_node));

	HandlePositioning(tparams);

	// Passing an expansion of 1.0 here, since we don't want to adjust the font-size
	RETURN_IF_ERROR(m_fontdesc->Check(m_doc_ctx, 1, m_textrenderer->GetFontSize()));

	unsigned int text_len = info.layouted->GetTextContentLength();
	tparams.current_char_idx = 0;

	SVGTextData* data = tparams.data;
	if (data && data->SetStartPosition())
	{
		int global_char_idx = tparams.total_char_sum + 0 /* always at index 0 (?) */;
		if (data->range.Intersecting(global_char_idx, (int)text_len))
		{
			data->startpos = tparams.ctp;
		}
	}

	// Distribute the characters somewhat even over the glyphs
	unsigned cpg = text_len / glyphs.GetCount();
	unsigned cpg_rem = glyphs.GetCount() - text_len % glyphs.GetCount();

	for (unsigned int i = 0; i < glyphs.GetCount(); ++i)
	{
		GlyphInfo* gi = glyphs.Get(i);
		gi->GetNumCharsRef() = MIN(255, cpg + (i >= cpg_rem ? 1 : 0));
	}

	OP_STATUS status = HandleGlyphs(glyphs, tparams);

	// No matter how many chars/codepoints the text content is made up
	// of, just account for one, to allow the case where the altGlyph
	// element is empty to be handled correctly.
	tparams.total_char_sum += 1;

	tparams.CommitRotations();

	if (data)
	{
		if (data->SetNumChars())
		{
			data->numchars += text_len;
		}
		if (data->SetEndPosition())
		{
			data->endpos = tparams.ctp;
		}
	}

	return status;
}

BOOL SVGAltGlyphMatcher::FindFontMetrics(HTML_Element* glyph_elm, FontMetricInfo& fmi)
{
	HTML_Element* font_elm = glyph_elm->Parent();
	while (font_elm && !font_elm->IsMatchingType(Markup::SVGE_FONT, NS_SVG))
	{
		font_elm = font_elm->Parent();
	}

	HTML_Element* font_face_elm = glyph_elm->Parent();
	while (font_face_elm && !font_face_elm->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
	{
		font_face_elm = font_face_elm->Parent();
	}

	if (!font_face_elm && font_elm)
	{
		// Look for 'font-face' element
		font_face_elm = font_elm->FirstChild();

		while (font_face_elm)
		{
			if (font_face_elm->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
				break;

			font_face_elm = font_face_elm->Suc();
		}
	}

	if (font_elm)
	{
		AttrValueStore::GetNumber(font_elm, Markup::SVGA_HORIZ_ADV_X, fmi.advance_x);
		AttrValueStore::GetNumber(font_elm, Markup::SVGA_VERT_ADV_Y, fmi.advance_y);
	}

	if (font_face_elm)
	{
		AttrValueStore::GetNumber(font_face_elm, Markup::SVGA_UNITS_PER_EM, fmi.units_per_em, 1000);
	}

	return font_face_elm != NULL || font_elm != NULL;
}

OP_BOOLEAN SVGAltGlyphMatcher::FetchGlyphData(BOOL horizontal, SVGNumber fontsize,
											  OpVector<GlyphInfo>& glyphs)
{
	OP_ASSERT(m_glyphelms.GetCount());
	OP_ASSERT(glyphs.GetCount() == 0);

	for (unsigned int i = 0; i < m_glyphelms.GetCount(); ++i)
	{
		FontMetricInfo fmi;

		// Pointed at a 'glyph' element
		HTML_Element* glyph_elm = m_glyphelms.Get(i);
		OP_ASSERT(glyph_elm && glyph_elm->IsMatchingType(Markup::SVGE_GLYPH, NS_SVG));

		if (!FindFontMetrics(glyph_elm, fmi))
			return OpBoolean::IS_FALSE;

		OpAutoPtr<GlyphInfo> ginfo(OP_NEW(GlyphInfo, ()));
		if (!ginfo.get())
			return OpStatus::ERR_NO_MEMORY;

		SVGNumber glyph_advance;
		AttrValueStore::GetNumber(glyph_elm,
								  horizontal ? Markup::SVGA_HORIZ_ADV_X : Markup::SVGA_VERT_ADV_Y,
								  glyph_advance,
								  horizontal ? fmi.advance_x : fmi.advance_y);

		ginfo->GetAdvanceRef() = glyph_advance * fontsize / fmi.units_per_em;

		OpBpath* glyph_path = AttrValueStore::GetPath(glyph_elm, Markup::SVGA_D);
		if (glyph_path)
		{
			OpBpath* cloned_path = static_cast<OpBpath*>(glyph_path->Clone());
			if (!cloned_path)
				return OpStatus::ERR_NO_MEMORY;

			*ginfo->GetPathPtr() = cloned_path; // GlyphInfo owns cloned path

			RETURN_IF_ERROR(SVGUtils::RescaleOutline(cloned_path, fontsize / fmi.units_per_em));
		}

		RETURN_IF_ERROR(glyphs.Add(ginfo.get()));
		ginfo.release();
	}
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN SVGAltGlyphMatcher::FetchGlyphElements(HTML_Element* glyphref_elm)
{
	while (glyphref_elm)
	{
		if (glyphref_elm->IsMatchingType(Markup::SVGE_GLYPHREF, NS_SVG))
		{
			HTML_Element* glyph_elm = SVGUtils::FindHrefReferredNode(m_resolver, m_doc_ctx, glyphref_elm);
			if (!glyph_elm || !glyph_elm->IsMatchingType(Markup::SVGE_GLYPH, NS_SVG))
			{
				m_glyphelms.Clear();
				return OpBoolean::IS_FALSE;
			}

			RETURN_IF_ERROR(m_glyphelms.Add(glyph_elm));
		}

		glyphref_elm = glyphref_elm->Suc();
	}

	return m_glyphelms.GetCount() > 0 ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_BOOLEAN SVGAltGlyphMatcher::Match(HTML_Element* match_root)
{
	OP_ASSERT(match_root);

	OP_BOOLEAN status = OpBoolean::IS_FALSE;
	if (match_root->IsMatchingType(Markup::SVGE_GLYPH, NS_SVG))
	{
		RETURN_IF_ERROR(m_glyphelms.Add(match_root));

		status = OpBoolean::IS_TRUE;
	}
	else if (match_root->IsMatchingType(Markup::SVGE_ALTGLYPHDEF, NS_SVG))
	{
		// Look for one of the relevant children
		HTML_Element* elm = match_root->FirstChildActual();
		while (elm &&
			   !elm->IsMatchingType(Markup::SVGE_GLYPHREF, NS_SVG) &&
			   !elm->IsMatchingType(Markup::SVGE_ALTGLYPHITEM, NS_SVG))
			elm = elm->SucActual();

		if (!elm)
			return OpBoolean::IS_FALSE; // No glyphs, no match

		if (elm->IsMatchingType(Markup::SVGE_GLYPHREF, NS_SVG))
		{
			status = FetchGlyphElements(elm);
		}
		else if (elm->IsMatchingType(Markup::SVGE_ALTGLYPHITEM, NS_SVG))
		{		
			while (elm)
			{
				status = FetchGlyphElements(elm->FirstChild());
				if (status == OpBoolean::IS_TRUE)
					break;

				RETURN_IF_ERROR(status);

				// Find next 'altGlyphItem' element
				do 
				{
					elm = elm->SucActual();
				} while (elm && !elm->IsMatchingType(Markup::SVGE_ALTGLYPHITEM, NS_SVG));
			}
		}
	}
	return status;
}

OP_STATUS SVGTextBlockTraverser::DoDataSetString(SVGTextBlock& block, SVGTextArguments& tparams)
{
	SVGTextData* data = tparams.data;
	OP_ASSERT(tparams.selected_max_idx > tparams.selected_min_idx);
	int cpylen = tparams.selected_max_idx - tparams.selected_min_idx;
	OP_STATUS err = OpStatus::OK;

	int textlength = block.GetLength();

	if (cpylen > textlength)
		cpylen = textlength;

	if (cpylen > 0)
	{
		if (tparams.selected_min_idx > 0)
		{
			cpylen = MIN(cpylen, (tparams.current_char_idx + textlength - tparams.selected_min_idx));
		}
		
		if (data->buffer)
			err = data->buffer->Append(block.GetText() + tparams.selected_min_idx, cpylen);

		data->range.length += cpylen;
	}

	tparams.current_char_idx += textlength;
	return err;
}

struct GlyphRelPosProps
{
	void Fetch(SVGTextArguments& tparams, unsigned int glyph_idx)
	{
		const SVGVectorStack& dxlist = tparams.dxlist;
		const SVGVectorStack& dylist = tparams.dylist;

		dxl = static_cast<const SVGLengthObject*>(dxlist.Get(glyph_idx));
		dyl = static_cast<const SVGLengthObject*>(dylist.Get(glyph_idx));

		OP_NEW_DBG("GlyphRelPosProps", "svg_text_relpos");
		OP_DBG(("Fetched index: %d : dxl: %p dyl: %p",
				glyph_idx, dxl, dyl));
	}

	void Fetch(SVGTextArguments& tparams)
	{
		Fetch(tparams, tparams.total_char_sum + tparams.current_char_idx);
	}

	void Preload(SVGTextArguments& tparams, int consumed_chars)
	{
		Fetch(tparams, tparams.total_char_sum + tparams.current_char_idx + consumed_chars);
	}

	void Resolve(const SVGValueContext& vcxt)
	{
		if (dxl)
			delta.x = SVGUtils::ResolveLength(dxl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

		if (dyl)
			delta.y = SVGUtils::ResolveLength(dyl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);
	}

	BOOL HasDelta() const { return !!dxl || !!dyl; }

	SVGNumberPair delta;

	const SVGLengthObject *dxl;
	const SVGLengthObject *dyl;
};

struct FragmentContext
{
	FragmentContext(const uni_char* in_text, unsigned in_length, int in_glyph_limit) :
		text(in_text), length(in_length),
		consumed(0), glyph_limit(in_glyph_limit) {}

	void Consume(int chars_to_consume)
	{
		consumed += chars_to_consume;
		text += chars_to_consume;
		length -= chars_to_consume;
	}

	const uni_char* text;
	unsigned length;

	int consumed;
	int glyph_limit;
};

OP_STATUS SVGTextTraverser::HandleRelativeAdjustments(FragmentContext& frag, int format,
													  SVGTextArguments& tparams)
{
	GlyphRelPosProps gpprops;

	gpprops.Fetch(tparams);

	while (gpprops.HasDelta())
	{
		gpprops.Resolve(m_value_context);

		HandleDelta(gpprops.delta, tparams);

		// There can't be any more relative positioning for this text run
		if (frag.length <= 1)
			break;

		// Peek at the next possible positional change (current_glyph
		// + 1), to determine whether we should do something more
		// here, or just fallthrough
		gpprops.Preload(tparams, 1);

		if (!gpprops.HasDelta())
			break;

		// Check and set font
		RETURN_IF_ERROR(m_fontdesc->Check(m_textrenderer->GetFontDocument(),
										  m_textrenderer->GetExpansion(),
										  m_textrenderer->GetFontSize()));

		int consumed_chars = frag.consumed;
		RETURN_IF_ERROR(HandleRelGlyph(frag, format, tparams));

		if (frag.glyph_limit == 0)
			return OpStatus::OK;

		consumed_chars = frag.consumed - consumed_chars;

		if (consumed_chars > 1)
			gpprops.Preload(tparams, consumed_chars - tparams.current_char_idx);

		// No need to fetch again, Preload already did that
	}
	return OpStatus::OK;
}

void SVGTextExtentTraverser::HandleDelta(const SVGNumberPair& delta, SVGTextArguments& tparams)
{
	tparams.ctp.x += delta.x;
	tparams.ctp.y += delta.y;

	SVGNumber extent_delta = !tparams.IsVertical() ? delta.x : delta.y;

	tparams.AddExtent(extent_delta);

	SVGTextData* data = tparams.data;
	if (data && data->SetExtent() && !data->range.IsEmpty())
		data->extent += extent_delta;
}

OP_STATUS SVGTextExtentTraverser::HandleRelGlyph(FragmentContext& frag, int format,
												 SVGTextArguments& tparams)
{
	int consumed_chars = 1;
	int glyphcount = 0;
	SVGNumber charextent = m_textrenderer->GetTxtExtent(frag.text, frag.length, 1, consumed_chars, glyphcount, tparams);
	tparams.current_glyph_idx += glyphcount;
	tparams.current_char_idx += consumed_chars;
	tparams.AdvanceCTPExtent(charextent);
	tparams.frag.offset += consumed_chars;
	frag.Consume(consumed_chars);
	frag.glyph_limit--;
	return OpStatus::OK;
}

void SVGTextPaintTraverser::HandleDelta(const SVGNumberPair& delta, SVGTextArguments& tparams)
{
	tparams.ctp.x += delta.x;
	tparams.ctp.y += delta.y;

#ifdef SVG_FULL_11
	if (tparams.path)
	{
		if (tparams.IsVertical())
		{
			tparams.pathDistance += delta.y;
			tparams.pathDisplacement += delta.x;
		}
		else
		{
			tparams.pathDistance += delta.x;
			tparams.pathDisplacement += delta.y;
		}
	}
#endif // SVG_FULL_11

	tparams.FlushRun();
	tparams.EmitRunStart();
}

OP_STATUS SVGTextPaintTraverser::HandleRelGlyph(FragmentContext& frag, int format,
												SVGTextArguments& tparams)
{
	if (tparams.packed.check_ellipsis)
		CheckEllipsis(frag.text, frag.length, 0, 1, tparams);

	int consumed_chars = 1;
	BOOL reverse_word = (format & TEXT_FORMAT_REVERSE_WORD) != 0;
	RETURN_IF_ERROR(m_textrenderer->TxtOut(frag.text, frag.length, tparams, reverse_word, 1, consumed_chars));

	tparams.frag.offset += consumed_chars;
	frag.Consume(consumed_chars);
	frag.glyph_limit--;

	tparams.EmitRunEnd();
	return OpStatus::OK;
}

#ifdef _DEBUG
#define DEBUG_FRAGMENT(frag,ltrfrag,current_char_idx,dbg_key) \
do { \
	TmpSingleByteString tmpstr;											\
	tmpstr.Set(&m_text[ltrfrag->wi.GetStart()], ltrfrag->wi.GetLength()); \
	OP_NEW_DBG("SVGTextBlockTraverser::WalkFragments", dbg_key);					\
	OP_DBG(("Fragment(vis=%d): %s. Ofs: %d. Len: %d. Mirrored: %s BiDi: %d. TrailingWS: %s. Current char idx: %d.", \
			frag, tmpstr.GetString(), ltrfrag->wi.GetStart(), ltrfrag->wi.GetLength(), \
			(ltrfrag->packed.bidi & BIDI_FRAGMENT_ANY_MIRRORED) ? "yes" : "no", \
			ltrfrag->packed.bidi,										\
			ltrfrag->wi.HasTrailingWhitespace() ? "yes" : "no",	\
			current_char_idx));											\
} while(0)
#define DEBUG_WORDINFO(frag,wi,current_char_idx,dbg_key) \
do { \
	TmpSingleByteString tmpstr;											\
	tmpstr.Set(&m_text[wi.GetStart()], wi.GetLength()); \
	OP_NEW_DBG("SVGTextBlockTraverser::WalkFragments", dbg_key);					\
	OP_DBG(("Fragment(vis=%d): \"%s\". Ofs: %d. Len: %d. TrailingWS: %s. Current char idx: %d.", \
			frag, wi.GetLength() == 0 && wi.HasTrailingWhitespace() ? " " : tmpstr.GetString(), wi.GetStart(), wi.GetLength(),	\
			wi.HasTrailingWhitespace() ? "yes" : "no",	\
			current_char_idx));											\
} while(0)
#define DEBUG_WORD(frag,wi,word,len,current_char_idx,dbg_key) \
do { \
	TmpSingleByteString tmpstr;											\
	tmpstr.Set(word, len); \
	OP_NEW_DBG("SVGTextBlockTraverser::WalkFragments", dbg_key);					\
	OP_DBG(("Fragment(vis=%d): \"%s\". Ofs: %d. Len: %d. TrailingWS: %s. Current char idx: %d.", \
			frag, wi.GetLength() == 0 && wi.HasTrailingWhitespace() ? " " : tmpstr.GetString(), wi.GetStart(), wi.GetLength(),	\
			wi.HasTrailingWhitespace() ? "yes" : "no",	\
			current_char_idx));											\
} while(0)
#else
#define DEBUG_FRAGMENT(frag,ltrfrag,current_char_idx,dbg_key) do{}while(0)
#define DEBUG_WORDINFO(frag,wi,current_char_idx,dbg_key) do{}while(0)
#define DEBUG_WORD(frag,ltrfrag,word,len,current_char_idx,dbg_key) do{}while(0)
#endif // _DEBUG

class TempWordInfoArray
{
public:
	TempWordInfoArray() : m_wi_array(g_opera->layout_module.tmp_word_info_array) {}
	~TempWordInfoArray()
	{
		if (m_wi_array != g_opera->layout_module.tmp_word_info_array)
			OP_DELETEA(m_wi_array);
	}

	OP_STATUS EnsureCapacity(unsigned capacity)
	{
		if (capacity <= WORD_INFO_MAX_SIZE)
			return OpStatus::OK;

		/* Probably allocates way too much, but it's only temporary.
		   May be an issue in documents with lots of text; may
		   cause new/delete performance issues. */
		m_wi_array = OP_NEWA(WordInfo, capacity);
		if (!m_wi_array)
			return OpStatus::ERR_NO_MEMORY;

		return OpStatus::OK;
	}

	WordInfo* GetArray() { return m_wi_array; }

private:
	WordInfo* m_wi_array;
};

OP_STATUS SVGTextBlock::RefreshFragments(const HTMLayoutProperties& props, BOOL preserve_spaces,
										 FramesDocument* frames_doc, SVGTextCache* text_cache)
{
	OP_NEW_DBG("SVGTextBlock::RefreshFragments", "svg_bidi");
	unsigned short new_word_count = 0;

	TempWordInfoArray tmp_wi_array;

	// Prepare the word info array
	RETURN_IF_ERROR(tmp_wi_array.EnsureCapacity(m_textlength));

	WordInfo* tmp_word_info_array = tmp_wi_array.GetArray();

	// Split content into non-breakable fragments

	BOOL stop_at_whitespace_separated_words = TRUE;
	const uni_char* tmp = m_text;
	BOOL leading_whitespace = FALSE;
	UnicodePoint last_base_character = 0;
	CharacterClass last_base_character_class = CC_Unknown;
#ifdef SUPPORT_TEXT_DIRECTION
	BidiCategory bidicategory = BIDI_UNDEFINED;
#endif // SUPPORT_TEXT_DIRECTION
	int original_fontnumber = props.font_number;
	BOOL original_font_had_ligatures = FALSE;
	BOOL is_ltr_only = TRUE;

	for (;;)
	{
		FontSupportInfo font_info(original_fontnumber);
		const uni_char* word = tmp;

#ifdef SVG_GET_NEXT_TEXTFRAGMENT_LIGATURE_HACK
		if (!new_word_count)
		{
			original_font_had_ligatures = font_info.current_font && font_info.current_font->HasLigatures();
			OP_DBG(("Original font had ligatures: %d.", original_font_had_ligatures ? 1 : 0));
		}
#endif //SVG_GET_NEXT_TEXTFRAGMENT_LIGATURE_HACK

		WordInfo tmp_word_info;
		int start = word - m_text;
		tmp_word_info.Reset();
		tmp_word_info.SetStart(MIN(WORD_INFO_START_MAX, start));
		tmp_word_info.SetFontNumber(font_info.current_font ? font_info.current_font->GetFontNumber() : original_fontnumber);

		if (!GetNextTextFragment(tmp, m_textlength - tmp_word_info.GetStart(),
									last_base_character, last_base_character_class, leading_whitespace,
									tmp_word_info, preserve_spaces ? CSS_VALUE_pre_wrap : CSS_VALUE_nowrap, stop_at_whitespace_separated_words, 
									FALSE /*info.treat_nbsp_as_space */, font_info, frames_doc, props.current_script))
			break;

		WordInfo& word_info = tmp_word_info_array[new_word_count];
		word_info = tmp_word_info;

#ifdef SUPPORT_TEXT_DIRECTION
		bidicategory = Unicode::GetBidiCategory(word[0]);
#endif // SUPPORT_TEXT_DIRECTION
		leading_whitespace = word_info.HasTrailingWhitespace();

		int dummy;
		const UnicodePoint puc = GetPreviousUnicodePoint(m_text, word - m_text, dummy);

		if ((new_word_count && word > m_text && uni_isspace(puc)) ||
			(!new_word_count && !preserve_spaces /*container->GetCollapseWhitespace()*/))
			word_info.SetIsStartOfWord(TRUE);

#ifdef SVG_GET_NEXT_TEXTFRAGMENT_LIGATURE_HACK
		// Ligature recombination hack - does break fontswitching in some cases, but makes us pass Acid3
		if (new_word_count && original_font_had_ligatures)
		{
			WordInfo& prev_wordinfo = tmp_word_info_array[new_word_count-1];
			if (!prev_wordinfo.HasTrailingWhitespace() && !word_info.HasTrailingWhitespace())
			{
				if (word_info.GetStart() == prev_wordinfo.GetStart() + prev_wordinfo.GetLength())
				{
					prev_wordinfo.SetLength(prev_wordinfo.GetLength() + word_info.GetLength());
					OP_DBG(("Word #%d collapsed.", new_word_count));
					prev_wordinfo.SetFontNumber(original_fontnumber);
					new_word_count--;
				}
			}
		}
#endif // SVG_GET_NEXT_TEXTFRAGMENT_LIGATURE_HACK

		word_info.SetWidth(0);
		new_word_count++;

		OP_DBG(("%d: start: %d len: %d trailingws: %d font: %d",
			new_word_count,
			word_info.GetStart(),
			word_info.GetLength(),
			word_info.HasTrailingWhitespace(),
			word_info.GetFontNumber()
		));

		DEBUG_WORDINFO(new_word_count,word_info,-1 /*tparams.current_char_idx*/,"svg_bidi");
#ifdef SUPPORT_TEXT_DIRECTION
		if (is_ltr_only &&
			(bidicategory == BIDI_R ||
				bidicategory == BIDI_AL ||
				bidicategory == BIDI_RLE ||
				bidicategory == BIDI_RLO))
		{
			is_ltr_only = FALSE;
		}
#endif // SUPPORT_TEXT_DIRECTION
	}

	if (text_cache->GetWordCount() != new_word_count)
	{
		text_cache->DeleteWordInfoArray();

		if (!text_cache->AllocateWordInfoArray(new_word_count))
			return OpStatus::ERR_NO_MEMORY;
	}

	// Copy array
	if (WordInfo* wordinfo_array = text_cache->GetWords())
		for (int i = 0; i < new_word_count; ++i)
			wordinfo_array[i] = tmp_word_info_array[i];

	text_cache->SetIsValid();
	text_cache->SetHasRTLFragments(!is_ltr_only);
	text_cache->SetPreserveSpaces(preserve_spaces);
	return OpStatus::OK;
}

#ifdef SUPPORT_TEXT_DIRECTION
static OP_STATUS CalculateBidi(SVGTextRootContainer* text_root_container, const HTMLayoutProperties& props,
							   const uni_char* textdata, SVGTextCache* text_cache,
							   unsigned int startfragidx)
{
	BidiCalculation* bidicalc = text_root_container->GetBidiCalculation();
	BOOL new_bidicalc = (bidicalc == NULL);
	if (!bidicalc)
	{
		if (!text_root_container->PushBidiProperties(&props))
		{
			text_cache->RemoveCachedInfo();
			return OpStatus::ERR_NO_MEMORY;
		}

		bidicalc = text_root_container->GetBidiCalculation();
	}

	WordInfo* wordinfo_array = text_cache->GetWords();
	unsigned int wordinfo_array_len = text_cache->GetWordCount();

	// FIXME: We really only need to do the paragraph segments here,
	// as an optimization. Now we get more stretches than necessary.
	for (unsigned int i = 0; i < startfragidx; i++)
	{
		const WordInfo& wi = wordinfo_array[i];

		bidicalc->AppendStretch(BIDI_L, wi.GetLength() + (wi.HasTrailingWhitespace() ? 1 : 0),
								NULL, wi.GetStart());
	}
	for (unsigned int i = startfragidx; i < wordinfo_array_len; i++)
	{
		const WordInfo& wi = wordinfo_array[i];

		const uni_char* word = textdata + wi.GetStart();
		BidiCategory bidicategory = Unicode::GetBidiCategory(word[0]);
		bidicalc->AppendStretch(bidicategory, wi.GetLength() + (wi.HasTrailingWhitespace() ? 1 : 0),
								NULL, wi.GetStart());
	}

	bidicalc->PopLevel();
	bidicalc->HandleNeutralsAtRunChange();

	SVGTextFragment* fraglist = NULL;
	OP_STATUS status = text_root_container->ResolveBidiSegments(wordinfo_array, wordinfo_array_len, fraglist);

	text_cache->AdoptTextFragmentList(fraglist);

	if (new_bidicalc)
		text_root_container->ResetBidi();

	return status;
}
#endif // SUPPORT_TEXT_DIRECTION

OP_STATUS SVGTextTraverser::HandleAbsolutePositioning(const SVGTextBlock& block,
													  WordInfo* wordinfo_array, unsigned int wordinfo_array_len,
													  int& char_count, SVGTextArguments& tparams)
{
	GlyphAbsPosProps gpprops;
	gpprops.Fetch(tparams);

	if (!gpprops.HasPos())
		return OpStatus::OK;

	const uni_char* text = block.GetText();
	unsigned textlength = block.GetLength();

	WordInfo& wi = wordinfo_array[tparams.frag.index];
	unsigned current_textpos = wi.GetStart();
	unsigned remaining_frag_length = wi.GetLength();
	BOOL has_trailing_ws = !tparams.packed.preserve_spaces && wi.HasTrailingWhitespace();

	while (gpprops.HasPos())
	{
		gpprops.Resolve(m_value_context);
		gpprops.Apply(tparams);

		HandleChunk(tparams);

		// There can't be any more absolute positioning for this text run
		if (textlength - current_textpos <= 1)
			break;

		// Peek at the next possible positional change (current_glyph
		// + 1), to determine whether we should do something more
		// here, or just fallthrough
		gpprops.Preload(tparams, 1);

		if (!gpprops.HasPos())
			break;

		if (tparams.frag.index >= wordinfo_array_len)
			break;

		tparams.current_char_idx = current_textpos;

		// Check and set font
		RETURN_IF_ERROR(m_fontdesc->Check(m_textrenderer->GetFontDocument(),
										  m_textrenderer->GetExpansion(),
										  m_textrenderer->GetFontSize()));

		if (remaining_frag_length != 0)
		{
			FragmentContext frag(text + current_textpos, remaining_frag_length, 1);
			RETURN_IF_ERROR(HandleAbsGlyph(frag, tparams));

			int consumed_chars = frag.consumed;

			if (IsDone())
			{
				tparams.current_char_idx += consumed_chars;
				break;
			}

			// specialcase for ligatures, we should skip entries in the abs.position arrays
			if (consumed_chars > 1)
				gpprops.Preload(tparams, consumed_chars-tparams.current_char_idx);

			OP_ASSERT(consumed_chars <= (int)remaining_frag_length);

			current_textpos += consumed_chars;
			remaining_frag_length -= consumed_chars;

			char_count += consumed_chars;
		}
		else if (has_trailing_ws)
		{
			const uni_char space = ' ';

			FragmentContext frag(&space, 1, 1);
			RETURN_IF_ERROR(HandleAbsGlyph(frag, tparams));

			has_trailing_ws = FALSE;

			char_count++;
		}

		// Advance to next word if we consumed the entire fragment
		if (remaining_frag_length == 0 && !has_trailing_ws)
		{
			tparams.frag.index++;
			tparams.frag.offset = 0;

			if (tparams.frag.index < wordinfo_array_len)
			{
				WordInfo& wi = wordinfo_array[tparams.frag.index];
				remaining_frag_length = wi.GetLength();
				current_textpos = wi.GetStart();
				has_trailing_ws = !tparams.packed.preserve_spaces && wi.HasTrailingWhitespace();
			}
		}
	}

	OP_ASSERT(current_textpos <= textlength);
	return OpStatus::OK;
}

OP_STATUS SVGTextBlockTraverser::WalkFragments(SVGTextBlock& block, SVGTextArguments& tparams)
{
	OP_NEW_DBG("SVGTextBlockTraverser::WalkFragments", "svg_bidi");

	SVGTextCache* tc_context = tparams.current_text_cache;
	WordInfo* wordinfo_array = tc_context->GetWords();
	unsigned int wordinfo_array_len = tc_context->GetWordCount();
	int char_count = 0;

	tparams.current_char_idx = 0;
	tparams.UpdateContentNode(m_textrenderer->GetFontDocument());

	RETURN_IF_ERROR(HandleAbsolutePositioning(block, wordinfo_array, wordinfo_array_len,
											  char_count, tparams));

	// All fragments consumed by absolute positioning
	if (tparams.frag.index >= wordinfo_array_len)
		return OpStatus::OK;

	const HTMLayoutProperties& props = *tparams.props;
	const uni_char* text = block.GetText();

#ifdef SUPPORT_TEXT_DIRECTION
	// Rearrange to visual order if needed
	if (tc_context->HasRTLFragments() && !tc_context->GetTextFragments())
	{
		OP_DBG(("### No tflist"));

		RETURN_IF_ERROR(CalculateBidi(tparams.text_root_container, props, text,
									  tc_context, tparams.frag.index));
	}

	SVGTextFragment* tflist = tc_context->GetTextFragments();
#endif // SUPPORT_TEXT_DIRECTION

	FlushPendingChunks(tparams);

#ifdef SUPPORT_TEXT_DIRECTION
	if (!tflist)
#endif // SUPPORT_TEXT_DIRECTION
	{
		OP_DBG(("All LTR text. Start frag idx: %d. Fragcount: %d. delta: %d word_start: %d word_length: %d",
				tparams.frag.index, wordinfo_array_len, tparams.frag.offset,
				wordinfo_array[tparams.frag.index].GetStart(),
				wordinfo_array[tparams.frag.index].GetLength()));

		// simplified LTR handling - run straight through wordinfo array
		for (unsigned int frag = tparams.frag.index; frag < wordinfo_array_len; frag++)
		{
			const WordInfo& wi = wordinfo_array[frag];
			const uni_char* word = text + wi.GetStart();
			int word_length = wi.GetLength();

			if (frag == tparams.frag.index && tparams.frag.offset > 0)
			{
				word += tparams.frag.offset;
				word_length -= tparams.frag.offset;

				OP_DBG(("Shifted: Old length: %d New length: %d",
						wi.GetLength(), word_length));
			}

			int text_format = GetTextFormat(props, wi);

			m_fontdesc->SetFontNumber(wi.GetFontNumber());

			DEBUG_WORD(frag,wi,word,word_length,tparams.current_char_idx,"svg_bidi");

			RETURN_IF_ERROR(HandleFragment(word, word_length, wi, text_format, tparams));

			if (IsDone())
				break;

			if (!tparams.packed.preserve_spaces && wi.HasTrailingWhitespace())
				char_count++;

			char_count += word_length;
		}
	}
#ifdef SUPPORT_TEXT_DIRECTION
	else if (tflist)
	{
		OP_DBG(("BiDi textfragments. Start frag idx: %d. Fragcount: %d. delta: %d word_start: %d word_length: %d",
				tparams.frag.index, wordinfo_array_len, tparams.frag.offset,
				wordinfo_array[tparams.frag.index].GetStart(),
				wordinfo_array[tparams.frag.index].GetLength()));

		for (unsigned int frag = tparams.frag.index; frag < wordinfo_array_len; frag++)
		{
			SVGTextFragment* ltrfrag = &tflist[tflist[frag].idx.next];
			const WordInfo& wi = wordinfo_array[ltrfrag->idx.wordinfo];
			const uni_char* word = text + wi.GetStart();
			int word_length = wi.GetLength();

			if (frag == tparams.frag.index && tparams.frag.offset > 0)
			{
				word += tparams.frag.offset;
				word_length -= tparams.frag.offset;

				OP_DBG(("Shifted: Old length: %d New length: %d Delta: %d",
						wi.GetLength(), word_length, tparams.frag.offset));
			}

			int text_format = GetTextFormat(props, wi);

			BOOL any_mirrored = !!(ltrfrag->packed.bidi & BIDI_FRAGMENT_ANY_MIRRORED);
			if (any_mirrored)
				text_format |= TEXT_FORMAT_REVERSE_WORD;

			m_fontdesc->SetFontNumber(wi.GetFontNumber());

			DEBUG_WORD(frag, wi, word, word_length, tparams.current_char_idx, "svg_bidi");

			RETURN_IF_ERROR(HandleFragment(word, word_length, wi, text_format, tparams));

			if (IsDone())
				break;

			if (!tparams.packed.preserve_spaces && wi.HasTrailingWhitespace())
				char_count++;

			char_count += word_length;
		}
	}
#endif // SUPPORT_TEXT_DIRECTION

	OP_DBG(("\n"));

	tparams.SetRunBBox();
	tparams.FlushRun();
	tparams.CommitRotations();

	if (tparams.data && tparams.data->SetNumChars())
	{
		tparams.data->numchars += char_count;
	}

	return OpStatus::OK;
}

void SVGTextBlock::NormalizeSpaces(BOOL preserve_spaces,
								   BOOL keep_leading_whitespace, BOOL keep_trailing_whitespace)
{
	StrReplaceChars(m_text, '\n', ' ');
	StrReplaceChars(m_text, '\r', ' ');
	StrReplaceChars(m_text, '\t', ' ');

	if (preserve_spaces)
	{
		/**
		 * When xml:space="preserve", the SVG user agent will do the
		 * following using a copy of the original character data
		 * content. It will convert all newline and tab characters
		 * into space characters.  Then, it will draw all space
		 * characters, including leading, trailing and multiple
		 * contiguous space characters. Thus, when drawn with
		 * xml:space="preserve", the string "a   b" (three spaces
		 * between "a" and "b") will produce a larger separation
		 * between "a" and "b" than "a b" (one space between "a" and
		 * "b").
		 */

		// NOTE: While this seems useless to me, I'm leaving it for
		// the lack of knowledge of the greater good...
		m_textlength = uni_strlen(m_text);
	}
	else
	{
		/**
		 * When xml:space="default", the SVG user agent will do the
		 * following using a copy of the original character data
		 * content.
		 * First, it will remove all newline characters.
		 * Then it will convert all tab characters into space characters.
		 * Then, it will strip off all leading and trailing space characters.
		 * Then, all contiguous space characters will be consolidated
		 */

		// The algorithm above is unusable since it's impossible to
		// put style on a single word with <tspan> without all whitespace
		// around the word collapsing.
		// If you still want it, make sure
		// FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE is defined.

		while (*m_text == ' ')
		{
#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
			if (keep_leading_whitespace && *(m_text+1) != ' ')
				break;
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE

			m_text++;
			m_textlength--;
		}

		while (m_textlength > 0 && m_text[m_textlength-1] == ' ')
		{
#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
			// Either 1) Trim off all trailing whitespace (keep_trailing_whitespace == FALSE)
			// Or     2) Trim all but the last whitespace (keep_trailing_whitespace == TRUE)
			if (keep_trailing_whitespace && (m_textlength == 1 || m_text[m_textlength-2] != ' '))
				break;
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE

			m_textlength--;
		}
	}
}

SVGNumber SVGTextBlockTraverser::GetEllipsisWidth(SVGTextArguments& tparams)
{
	SVG_DECLARE_ELLIPSIS;

	int dummy_consumed;
	return m_textrenderer->GetTxtExtent(ellipsis_string, ellipsis_length, ellipsis_length,
										dummy_consumed, dummy_consumed, tparams);
}

unsigned SVGTextBlockTraverser::CalculateEllipsisPositionInWord(const uni_char* word,
																unsigned word_length,
																int format, SVGNumber remaining,
																SVGTextArguments& tparams)
{
	remaining = SVGNumber::max_of(0, remaining); // Paranoia

	unsigned tmp_frag_length = word_length;
	while (tmp_frag_length > 0)
	{
		tmp_frag_length--;

		int dummy_consumed;
		SVGNumber part_extent = m_textrenderer->GetTxtExtentEx(word, tmp_frag_length,
															   format, tmp_frag_length,
															   dummy_consumed, dummy_consumed, tparams);
		if (part_extent <= remaining)
			break;
	}
	return tmp_frag_length;
}

void SVGTextPaintTraverser::CheckEllipsis(const uni_char* word, unsigned word_length, int format,
										  int glyph_limit,
										  SVGTextArguments& tparams)
{
	int dummy_consumed;
	SVGNumber extent = m_textrenderer->GetTxtExtentEx(word, word_length, format,
													  glyph_limit, dummy_consumed, dummy_consumed, tparams);

	SVGNumber ellipsis_width = GetEllipsisWidth(tparams);

	if (tparams.current_extent + extent + ellipsis_width >= tparams.max_extent)
	{
		// Determine where the ellipsis should be placed
		SVGNumber remaining = tparams.max_extent - (tparams.current_extent + ellipsis_width);
		unsigned ellipsis_frag_length = CalculateEllipsisPositionInWord(word, word_length,
																		format, remaining,
																		tparams);

		// Emit ellipsis
		tparams.packed.check_ellipsis = 0;
		tparams.EmitEllipsis(ellipsis_frag_length);
	}
	else
	{
		tparams.current_extent += extent;
	}
}

OP_STATUS SVGTextTraverser::HandleFragment(const uni_char* word, int word_length,
										   const WordInfo& wi, int text_format,
										   SVGTextArguments& tparams)
{
	tparams.FlushPendingRun();

	BOOL any_mirrored = (text_format & TEXT_FORMAT_REVERSE_WORD) != 0;
	BOOL handle_space = !tparams.packed.preserve_spaces && wi.HasTrailingWhitespace();
	if (handle_space && any_mirrored)
	{
		tparams.current_char_idx = wi.GetStart() + wi.GetLength();

		OpStatus::Ignore(HandleSpace(text_format, tparams));

		// Not entirely accurate for a possible first (split) fragment?
		tparams.current_char_idx = wi.GetStart();
	}

	if (word_length > 0)
	{
		FragmentContext frag(word, word_length, word_length);
		RETURN_IF_ERROR(HandleWord(frag, text_format, tparams));
	}

	if (handle_space && !any_mirrored)
		OpStatus::Ignore(HandleSpace(text_format, tparams));

	tparams.frag.index++;
	tparams.frag.offset = 0;
	return OpStatus::OK;
}

OP_STATUS SVGTextExtentTraverser::HandleWord(FragmentContext& frag,
											 int format, SVGTextArguments& tparams)
{
	BOOL terminate_after = FALSE;

	if (tparams.max_glyph_idx > 0)
	{
		int in_fraglen = frag.length;
		if (tparams.max_glyph_idx > tparams.current_glyph_idx)
			frag.length = MIN((int)frag.length, tparams.max_glyph_idx - tparams.current_glyph_idx);
		else
			frag.length = 0;

		terminate_after = in_fraglen != (int)frag.length;
	}
	else if (tparams.max_glyph_idx == 0)
	{
		m_traverser_done = TRUE;
		return OpStatus::OK;
	}

	if (tparams.GlyphLimitReached())
	{
		m_traverser_done = TRUE;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(HandleRelativeAdjustments(frag, format, tparams));

	if (frag.glyph_limit > 0)
	{
		int consumed = 1;
		int glyphcount = 0;
		SVGNumber charextent = m_textrenderer->GetTxtExtentEx(frag.text, frag.length, format,
															  frag.glyph_limit, consumed, glyphcount, tparams);
		tparams.current_glyph_idx += glyphcount;
		tparams.current_char_idx += consumed;
		tparams.AdvanceCTPExtent(charextent);

		tparams.frag.offset += consumed;

		frag.Consume(consumed);
	}

	if (terminate_after)
		m_traverser_done = TRUE;

	return OpStatus::OK;
}

OP_STATUS SVGTextExtentTraverser::HandleSpace(int format, SVGTextArguments& tparams)
{
	if (tparams.GlyphLimitReached())
		return OpStatus::OK;

	const uni_char space = ' ';

	FragmentContext frag(&space, 1, 1);
	OP_STATUS status = HandleWord(frag, format, tparams);

 	tparams.AddExtent(tparams.word_spacing);

	return status;
}

void SVGTextExtentTraverser::FlushPendingChunks(SVGTextArguments& tparams)
{
	if (tparams.force_chunk)
		HandleChunk(tparams);
}

void SVGTextExtentTraverser::HandleChunk(SVGTextArguments& tparams)
{
	SVGTextData* data = tparams.data;

	// Adjust with current chunk extent, then start new one
	if (data && data->SetExtent() && data->chunk_list)
	{
		BOOL increment_chunk = data->chunk_list->GetCount() > 0;

		RETURN_VOID_IF_ERROR(tparams.NewChunk(*data->chunk_list));

		if (increment_chunk)
			tparams.current_chunk++;
	}
	else if (tparams.chunk_list)
	{
		tparams.AdjustCTP();

		tparams.current_chunk++;
	}

	tparams.force_chunk = FALSE;
}

OP_STATUS SVGTextExtentTraverser::HandleAbsGlyph(FragmentContext& frag, SVGTextArguments& tparams)
{
	return HandleWord(frag, 0, tparams);
}

OP_STATUS SVGTextPaintTraverser::HandleWord(FragmentContext& frag,
											int format, SVGTextArguments& tparams)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	tparams.SetupTextSelectionForFragment();
#endif // SVG_SUPPORT_TEXTSELECTION

	RETURN_IF_ERROR(HandleRelativeAdjustments(frag, format, tparams));

	OP_STATUS status = OpStatus::OK;
	if (frag.glyph_limit > 0)
	{
		if (tparams.packed.check_ellipsis)
			CheckEllipsis(frag.text, frag.length, format, frag.glyph_limit, tparams);

		int consumed = 1;
		status = m_textrenderer->TxtOutEx(frag.text, frag.length, consumed, frag.glyph_limit,
										  format | TEXT_FORMAT_BIDI_PRESERVE_ORDER, tparams);

		tparams.frag.offset += consumed;
		frag.Consume(consumed);
	}
	return status;
}

OP_STATUS SVGTextPaintTraverser::HandleSpace(int format, SVGTextArguments& tparams)
{
	const uni_char space = ' ';

	FragmentContext frag(&space, 1, 1);
	OP_STATUS status = HandleWord(frag, format, tparams);

	tparams.AdvanceCTP(tparams.word_spacing);

	return status;
}

void SVGTextPaintTraverser::FlushPendingChunks(SVGTextArguments& tparams)
{
	if (tparams.force_chunk)
		HandleChunk(tparams);
}

void SVGTextPaintTraverser::HandleChunk(SVGTextArguments& tparams)
{
	// Adjust with current chunk extent
	tparams.AdjustCTP();

	tparams.packed.check_ellipsis = 0;

	if (tparams.is_layout)
	{
		if (tparams.packed.use_ellipsis)
		{
			SVGTextChunk* chunk = tparams.chunk_list->Get(tparams.current_chunk);
			SVGNumber chunk_extent = chunk->extent;

			if (tparams.current_extent + chunk_extent >= tparams.max_extent)
				tparams.packed.check_ellipsis = 1;
			else
				tparams.current_extent += chunk_extent;
		}

		tparams.FlushRun();
		tparams.EmitRunStart();
	}

	tparams.current_chunk++;

	tparams.force_chunk = FALSE;
}

OP_STATUS SVGTextPaintTraverser::HandleAbsGlyph(FragmentContext& frag, SVGTextArguments& tparams)
{
	return HandleWord(frag, 0, tparams);
}

OP_STATUS SVGTextAreaInfo::NewBlock()
{
	SVGLineInfo* new_li = OP_NEW(SVGLineInfo, ());
	if (!new_li || OpStatus::IsError(lineinfo->Add(new_li)))
	{
		OP_DELETE(new_li);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS SVGTextAreaInfo::NewBlock(const HTMLayoutProperties& props)
{
	RETURN_IF_ERROR(NewBlock());

	SVGLineInfo* li = lineinfo->Get(lineinfo->GetCount() - 1);
	li->height = props.svg->GetLineIncrement();
	return OpStatus::OK;
}

void SVGTextAreaInfo::AdvanceBlock(SVGNumberPair& ctp, SVGNumberPair& linepos)
{
	// Advance in block-progression-direction
	if (SVGLineInfo* current_li = lineinfo->Get(current_line))
		linepos.y += current_li->height;

	current_line++;

	if (SVGLineInfo* next_li = lineinfo->Get(current_line))
	{
		// Advance the line position
		linepos.x = GetStart(next_li);

		ctp = linepos;
		ctp.y += next_li->height - next_li->descent;

		remaining_fragments_on_line = next_li->num_fragments;
	}
	else
	{
		OP_ASSERT(current_line == lineinfo->GetCount());
		remaining_fragments_on_line = INT_MAX;
	}
}

static SVGNumber GetTxtExtentExAndConsumeInput(SVGTextRenderer* textrenderer, const uni_char* word, int word_length,
											   int text_format, SVGTextArguments& tparams)
{
	int consumed_chars;
	int glyphcount;
	SVGNumber word_width = textrenderer->GetTxtExtentEx(word, word_length, text_format,
														word_length, consumed_chars, glyphcount, tparams);

	tparams.current_glyph_idx += glyphcount;
	tparams.current_char_idx += consumed_chars;
	if (tparams.IsVertical())
		tparams.ctp.y += word_width;
	else
		tparams.ctp.x += word_width;

	return word_width;
}

OP_STATUS SVGTextAreaMeasurementTraverser::HandleFragment(const uni_char* word, int word_length,
														  const WordInfo& wi, int text_format,
														  SVGTextArguments& tparams)
{
	SVGTextAreaInfo& tainfo = *tparams.area;
	const HTMLayoutProperties& props = *tparams.props;

	// Check and set font
	RETURN_IF_ERROR(m_fontdesc->Check(m_textrenderer->GetFontDocument(),
									  m_textrenderer->GetExpansion(),
									  m_textrenderer->GetFontSize()));

	SVGNumber descent = m_fontdesc->GetDescent();
	SVGNumber line_height = props.svg->GetLineIncrement();

	BOOL any_mirrored = (text_format & TEXT_FORMAT_REVERSE_WORD) != 0;
	BOOL handle_space = !tparams.packed.preserve_spaces && wi.HasTrailingWhitespace();

	SVGNumber space_width;
	if (handle_space)
	{
		// Calculate extents of space (including word-spacing et.c)
		if (any_mirrored)
			tparams.current_char_idx = wi.GetStart() + wi.GetLength();

		const uni_char space = ' ';

		space_width = GetTxtExtentExAndConsumeInput(m_textrenderer, &space, 1, text_format, tparams);
		space_width += tparams.word_spacing;

		if (any_mirrored)
			tparams.current_char_idx = wi.GetStart();
	}

	SVGNumber word_width;
	if (word_length > 0)
	{
		word_width = GetTxtExtentExAndConsumeInput(m_textrenderer, word, word_length, text_format, tparams);

		if (tainfo.lbstate.BreakAllowed(word[0]))
		{
			// It's OK to break here
			tainfo.FlushUncommitted();

			if (tainfo.BreakNeeded(word_width))
			{
				tainfo.uncommitted.Reset();

				// ...and we have to do it...
				RETURN_IF_ERROR(tainfo.NewBlock(props));
			}
		}

		tainfo.lbstate.UpdateChar(word, word_length);
	}

	tainfo.lbstate.UpdateWS(handle_space);

	if (handle_space)
		tainfo.uncommitted_space.Update(space_width, descent, line_height);

	if (word_length > 0)
	{
		if (word[0] == 0x00AD /* soft-hyphen */)
			word_width = 0;

		tainfo.uncommitted.Update(word_width, descent, line_height);
	}

	tainfo.uncommitted.num_fragments++;

	tparams.frag.index++;
	tparams.frag.offset = 0;
	return OpStatus::OK;
}

void SVGTextAreaPaintTraverser::CheckEllipsis(const uni_char* word, int word_length,
											  const WordInfo& wi, int text_format,
											  const SVGLineInfo* li, SVGTextArguments& tparams)
{
	SVGNumber ellipsis_width = GetEllipsisWidth(tparams);
	SVGNumber textarea_width = tparams.area->vis_area.width;

	unsigned end_offset = 0;
	BOOL ws_only = FALSE;
	if (li->width + ellipsis_width <= textarea_width)
	{
		// No need to trim the word, just add the ellipsis to
		// the run. Trim the trailing WS if there is one.
		if (!tparams.packed.preserve_spaces && wi.HasTrailingWhitespace())
		{
			end_offset = word_length;

			ws_only = (word_length == 0);
		}
	}
	else
	{
		// Need to trim the last word on the line.
		int dummy_consumed;
		SVGNumber word_extent = m_textrenderer->GetTxtExtentEx(word, word_length, text_format,
															   word_length, dummy_consumed,
															   dummy_consumed, tparams);

		SVGNumber remaining = textarea_width;
		remaining -= li->width - word_extent + ellipsis_width;

		end_offset = CalculateEllipsisPositionInWord(word, word_length, text_format,
													 remaining, tparams);
	}

	tparams.UpdateTextAreaEllipsis(end_offset, ws_only);
}

OP_STATUS SVGTextAreaPaintTraverser::HandleFragment(const uni_char* word, int word_length,
													const WordInfo& wi, int text_format,
													SVGTextArguments& tparams)
{
	SVGTextAreaInfo& tainfo = *tparams.area;
	SVGLineInfo* li = tainfo.lineinfo->Get(tainfo.current_line);

	OP_ASSERT(li || !"HandleFragment");
	if (!li)
		return OpStatus::OK;

	OP_ASSERT(tainfo.remaining_fragments_on_line > 0);

	BOOL line_is_visible = TRUE;

	if (!tainfo.info.height_auto)
	{
		// Check if the line fits (in the block-progression-direction)
		if (tparams.linepos.y < tainfo.vis_area.y ||
			tparams.linepos.y + li->height > tainfo.vis_area.y + tainfo.vis_area.height)
			line_is_visible = FALSE;
	}

	if (line_is_visible)
	{
		text_format |= TEXT_FORMAT_BIDI_PRESERVE_ORDER;

		tparams.FlushPendingRun();

		const uni_char space = ' ';

		BOOL any_mirrored = (text_format & TEXT_FORMAT_REVERSE_WORD) != 0;
		BOOL handle_space = !tparams.packed.preserve_spaces && wi.HasTrailingWhitespace();

		if (handle_space && any_mirrored)
		{
			tparams.current_char_idx = wi.GetStart() + wi.GetLength();

#ifdef SVG_SUPPORT_TEXTSELECTION
			tparams.SetupTextSelectionForFragment();
#endif // SVG_SUPPORT_TEXTSELECTION

			int consumed_chars;
			OpStatus::Ignore(m_textrenderer->TxtOutEx(&space, 1, consumed_chars,
													  1, text_format, tparams));

			tparams.AdvanceCTP(tparams.word_spacing);

			tparams.current_char_idx = wi.GetStart();
		}

		if (word_length > 0)
		{
			BOOL is_softhyphen = (word[0] == 0x00AD);

			// Soft hyphens shouldn't be drawn if they are not the last on the line
			if (!is_softhyphen || tainfo.remaining_fragments_on_line == 1)
			{
#ifdef SVG_SUPPORT_TEXTSELECTION
				tparams.SetupTextSelectionForFragment();
#endif // SVG_SUPPORT_TEXTSELECTION

				int consumed_chars;
				OpStatus::Ignore(m_textrenderer->TxtOutEx(word, word_length, consumed_chars,
														  word_length, text_format, tparams));
			}
		}

		if (handle_space && !any_mirrored)
		{
#ifdef SVG_SUPPORT_TEXTSELECTION
			tparams.SetupTextSelectionForFragment();
#endif // SVG_SUPPORT_TEXTSELECTION

			int consumed_chars;
			OpStatus::Ignore(m_textrenderer->TxtOutEx(&space, 1, consumed_chars,
													  1, text_format, tparams));

			tparams.AdvanceCTP(tparams.word_spacing);
		}
	}
	else
	{
		tparams.AddPendingRun();
	}

	tainfo.remaining_fragments_on_line--;

	tparams.frag.index++;
	tparams.frag.offset = 0;

	if (tainfo.remaining_fragments_on_line == 0)
	{
		if (tparams.is_layout && tparams.packed.use_ellipsis)
		{
			if (line_is_visible)
				CheckEllipsis(word, word_length, wi, text_format, li, tparams);
			else
				tparams.FlushTextAreaEllipsis();
		}

		if (!li->forced_break)
		{
			if (line_is_visible)
				tparams.EmitRunEnd();

			tparams.AdvanceBlock();
		}
	}

	return OpStatus::OK;
}

// Check the need for vector fonts (on element/textnode basis)
static BOOL ElementNeedsVectorFonts(SVGTextArguments& tparams, SVGCanvas* canvas,
									const HTMLayoutProperties& props)
{
	const SVGMatrix& current_transform = canvas->GetTransform();
	BOOL has_trivial_transform =
		current_transform[1] == 0 &&
		current_transform[2] == 0 &&
		current_transform[0] == current_transform[3] &&
		current_transform[0] >= 0;

	if (!has_trivial_transform)
	{
		// Can't render rotated/sheared strings with DrawString and if we rotate the
		// bitmap it will be soooo ugly (see FireFox 1.5 which did exactly that)
		return TRUE;
	}

	int fill = canvas->GetFill();
	if (!(fill == SVGCanvasState::USE_COLOR || fill == SVGCanvasState::USE_NONE))
	{
		// Can't render patterned (for instance) texts with DrawString
		return TRUE;
	}

	int stroke = canvas->GetStroke();
	if (stroke != SVGCanvasState::USE_NONE)
	{
		// Can't render stroked text with DrawString
		return TRUE;
	}

#ifdef SVG_FULL_11
	if (tparams.path)
	{
		// Can't render on a path with DrawString
		return TRUE;
	}
#endif // SVG_FULL_11

	if (tparams.extra_spacing.NotEqual(0) || tparams.glyph_scale.NotEqual(1))
	{
		// Char spacing doesn't work with DrawString since we then
		// can't detect if clicks are between characters or on them.
		// Glyph scale should stretch glyphs which is not supported
		// in painters.
		return TRUE;
	}

	// Platforms (e.g. StringWidth) can't generally handle negative
	// letter spacing
	if (tparams.letter_spacing < 0)
		return TRUE;

	BOOL optimize_precision = (props.svg->textinfo.textrendering == SVGTEXTRENDERING_GEOMETRICPRECISION);
	if (optimize_precision)
	{
		/* Indicates that the user agent shall emphasize geometric
		   precision over legibility and rendering speed. This
		   option will usually cause the user agent to suspend
		   the use of hinting so that glyph outlines are drawn
		   with comparable geometric precision to the rendering
		   of path data.
		*/
		return TRUE;
	}

	if (tparams.IsVertical())
	{
		// Currently no support for vertical text with DrawString
		return TRUE;
	}

	// Horizontal text, but with a forced glyph orientation.
	if (props.svg->textinfo.glyph_orientation_horizontal != 0)
		return TRUE;

#ifdef SVG_SUPPORT_STENCIL
	// Clippath's and masks dont have "dirty rects" per se, thus their
	// screenboxes can generally(?) not be trusted.
	if (canvas->GetClipMode() == SVGCanvas::STENCIL_CONSTRUCT ||
		canvas->GetMaskMode() == SVGCanvas::STENCIL_CONSTRUCT)
	{
		return TRUE;
	}
#endif // SVG_SUPPORT_STENCIL

	return FALSE;
}

OP_STATUS SVGTextBlockTraverser::Traverse(SVGTextBlock& block, SVGElementInfo& info,
										  SVGTextArguments& tparams, BOOL allow_raster_fonts)
{
	OP_NEW_DBG("SVGTextBlockTraverser::Traverse", "svg_text");

	SVGTextData* data = tparams.data;

	const HTMLayoutProperties& props = *info.props->GetProps();

	// Setup properties for the text renderer
	m_textrenderer->SetFontSize(props.svg->GetFontSize(info.layouted));

	tparams.UpdateContext(m_doc_ctx, info);
	tparams.SetContentNode(info);

	tparams.frag.index = 0;
	tparams.frag.offset = 0;

	tparams.current_run.start = tparams.frag;
	tparams.current_run.end = tparams.frag;

	// Pre data
#ifdef SVG_SUPPORT_TEXTSELECTION
	if (data)
	{
		if (data->SetString())
		{
			tparams.SetupTextSelectionForFragment();

			if (tparams.selected_min_idx == -1 ||
				tparams.selected_min_idx == tparams.selected_max_idx)
				return OpStatus::OK;

			return DoDataSetString(block, tparams);
		}
	}
#endif // SVG_SUPPORT_TEXTSELECTION

	// Setup parameters that won't change during the processing of this textnode
	tparams.word_spacing = SVGUtils::GetSpacing(info.layouted, Markup::SVGA_WORD_SPACING, props);
	tparams.letter_spacing = SVGUtils::GetSpacing(info.layouted, Markup::SVGA_LETTER_SPACING, props);

	tparams.elm_needs_vector_fonts =
		!allow_raster_fonts ||
		ElementNeedsVectorFonts(tparams, m_textrenderer->GetCanvas(), props);

	SVGTextCache* tc_context = info.context->GetTextCache();

	OP_DBG(("element: %p context: %p tc_context valid: %d. wordinfo_array: %p tflist: %p",
			info.layouted, info.context,
			tc_context->IsValid() ? 1 : 0,
			tc_context->GetWords(), tc_context->GetTextFragments()));

#if defined(_DEBUG) && defined(SUPPORT_TEXT_DIRECTION)
	if (tparams.text_root_container)
		tparams.text_root_container->PrintDebugInfo();
#endif // _DEBUG && SUPPORT_TEXT_DIRECTION

	if (!tc_context->IsValid())
	{
		OP_ASSERT((short)m_fontdesc->GetFontAttributes().GetFontNumber() == props.font_number);
		RETURN_IF_ERROR(block.RefreshFragments(props, tparams.packed.preserve_spaces,
											   m_doc_ctx->GetDocument(), tc_context));
	}

	tparams.SetCurrentTextCache(tc_context);
	tparams.AddPendingRun();

	RETURN_IF_ERROR(WalkFragments(block, tparams));

	// Post data
	if (data)
	{
		if (data->SetExtent() && data->range.index < 0)
		{
			data->extent += tparams.total_extent;

#ifdef _DEBUG
			TmpSingleByteString tmpstr;
			tmpstr.Set(block.GetText(), block.GetLength());
			OP_DBG(("String: \'%s\'. Extent: %g. Accumulated extent: %g.",
					tmpstr.GetString(),
					tparams.total_extent.GetFloatValue(), data->extent.GetFloatValue()));
#endif // _DEBUG
		}
		if (data->SetEndPosition())
		{
			data->endpos = tparams.ctp;
		}
	}

	tparams.total_char_sum += block.GetLength();

	return OpStatus::OK;
}

#endif // SVG_SUPPORT
