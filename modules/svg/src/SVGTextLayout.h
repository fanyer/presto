/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef _SVG_TEXT_LAYOUT_
#define _SVG_TEXT_LAYOUT_

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGTextRenderer.h"

#include "modules/display/vis_dev.h"

class SVGAltGlyphNode;

struct FragmentContext;
struct FragmentPosition;

/**
 * Handles the 'traversal' of a block of text (usually the text from a
 * textnode, but possibly also the contents pointed to by a tref, or
 * the contents of a textgroup).
 *
 * HandleAbsolutePositioning
 *   Called to allow the traverser to handle any absolute positioning
 *   of glyphs. Is slated to move down the hierarchy to be specific
 *   to <text>.
 *
 * HandleFragment
 *   Called for each fragment of the text block (in visual order).
 *
 * FlushPendingChunks
 *   Called before walking the fragments (before any call to
 *   HandleFragment), but after handling absolute positioning. Should
 *   perform any pending CTP adjustments (which there usually
 *   shouldn't be any if absolute position has occured). Also slated
 *   for moving down the hierarchy.
 */
class SVGTextBlockTraverser
{
public:
	SVGTextBlockTraverser(const SVGValueContext& vcxt,
						  SVGDocumentContext* doc_ctx,
						  SVGTextRenderer* tr,
						  int format_options = 0)
		: m_value_context(vcxt),
		  m_doc_ctx(doc_ctx),
		  m_fontdesc(tr->GetFontDescriptor()),
		  m_textrenderer(tr) {}
	virtual ~SVGTextBlockTraverser() {} /* To keep the noise down */

	OP_STATUS Traverse(SVGTextBlock& block, SVGElementInfo& info,
					   SVGTextArguments& tparams, BOOL allow_raster_fonts);

	virtual OP_STATUS HandleAbsolutePositioning(const SVGTextBlock& block, WordInfo* wordinfo_array,
												unsigned int wordinfo_array_len, int& char_count,
												SVGTextArguments& tparams) = 0;
	virtual OP_STATUS HandleFragment(const uni_char* word, int word_length, const WordInfo& wi,
									 int text_format, SVGTextArguments& tparams) = 0;

	virtual void FlushPendingChunks(SVGTextArguments& tparams) {}

	virtual BOOL IsDone() = 0;

private:
	OP_STATUS WalkFragments(SVGTextBlock& block, SVGTextArguments& tparams);
	OP_STATUS DoDataSetString(SVGTextBlock& block, SVGTextArguments& tparams);

protected:
	SVGNumber GetEllipsisWidth(SVGTextArguments& tparams);
	unsigned CalculateEllipsisPositionInWord(const uni_char* word, unsigned word_length,
											 int format, SVGNumber remaining,
											 SVGTextArguments& tparams);

	SVGValueContext m_value_context;
	SVGDocumentContext* m_doc_ctx;
	SVGFontDescriptor* m_fontdesc;
	SVGTextRenderer* m_textrenderer;
};

/**
 * Handles the 'traversal' of a group of alterate glyphs
 * ('altGlyph's).
 *
 * HandleGlyphs
 *   Called with a vector of glyphs.
 */
class SVGAltGlyphTraverser
{
public:
	SVGAltGlyphTraverser(const SVGValueContext& vcxt,
						 SVGDocumentContext* doc_ctx,
						 SVGTextRenderer* tr)
		: m_value_context(vcxt), m_doc_ctx(doc_ctx),
		  m_fontdesc(tr->GetFontDescriptor()), m_textrenderer(tr),
		  m_paint_node(NULL) {}
	virtual ~SVGAltGlyphTraverser() {} /* To keep the noise down */

	OP_STATUS Traverse(OpVector<GlyphInfo>& glyphs, SVGElementInfo& info, SVGTextArguments& tparams);

	virtual OP_STATUS HandleGlyphs(OpVector<GlyphInfo>& glyphs, SVGTextArguments& tparams)/* = 0*/;

	void SetPaintNode(SVGAltGlyphNode* ag_node) { m_paint_node = ag_node; }

protected:
	void HandlePositioning(SVGTextArguments& tparams);

	virtual void HandleChunk(SVGTextArguments& tparams);

	SVGValueContext m_value_context;
	SVGDocumentContext* m_doc_ctx;
	SVGFontDescriptor* m_fontdesc;
	SVGTextRenderer* m_textrenderer;
	SVGAltGlyphNode* m_paint_node;
};

/**
 * altGlyph traverser that calculate extents
 */
class SVGAltGlyphExtentTraverser : public SVGAltGlyphTraverser
{
public:
	SVGAltGlyphExtentTraverser(const SVGValueContext& vcxt,
							   SVGDocumentContext* doc_ctx,
							   SVGTextRenderer* tr)
		: SVGAltGlyphTraverser(vcxt, doc_ctx, tr) {}

	virtual OP_STATUS HandleGlyphs(OpVector<GlyphInfo>& glyphs, SVGTextArguments& tparams);

protected:
	virtual void HandleChunk(SVGTextArguments& tparams);
};

struct SVGTextChunk
{
	SVGNumberPair initial_ctp;
	SVGNumber extent;
};

/**
 * Block traverser for blocks with a <text> root
 *
 * Adds the following additional hooks:
 *
 * HandleWord
 *   Called for every fragment.
 *
 * HandleSpace
 *   Called for spaces when xml:space is not 'preserve'.
 *
 * HandleChunk
 *   Called when a new chunk is created/encountered.
 *
 * HandleAbsGlyph
 *   Called for each single absolutely positioned glyph.
 *
 * HandleDelta
 *   Called for each relative CTP adjustment.
 *
 * HandleRelGlyph
 *   Called for each single relatively positioned glyph.
 */
class SVGTextTraverser : public SVGTextBlockTraverser
{
public:
	SVGTextTraverser(const SVGValueContext& vcxt,
					 SVGDocumentContext* doc_ctx,
					 SVGTextRenderer* tr)
		: SVGTextBlockTraverser(vcxt, doc_ctx, tr) {}

	virtual OP_STATUS HandleAbsolutePositioning(const SVGTextBlock& block, WordInfo* wordinfo_array,
												unsigned int wordinfo_array_len, int& char_count,
												SVGTextArguments& tparams);
	virtual OP_STATUS HandleFragment(const uni_char* word, int word_length, const WordInfo& wi,
									 int text_format, SVGTextArguments& tparams);

protected:
	virtual OP_STATUS HandleWord(FragmentContext& frag, int format, SVGTextArguments& tparams) = 0;
	virtual OP_STATUS HandleSpace(int format, SVGTextArguments& tparams) = 0;

	virtual void HandleChunk(SVGTextArguments& tparams) = 0;
	virtual OP_STATUS HandleAbsGlyph(FragmentContext& frag, SVGTextArguments& tparams) = 0;

	/**
	 * Apply dx/dy (relative- or delta-adjustments)
	 */
	OP_STATUS HandleRelativeAdjustments(FragmentContext& frag, int format,
										SVGTextArguments& tparams);

	/**
	 * Helper hooks for the above
	 */
	virtual void HandleDelta(const SVGNumberPair& delta, SVGTextArguments& tparams) = 0;
	virtual OP_STATUS HandleRelGlyph(FragmentContext& frag, int format, SVGTextArguments& tparams) = 0;
};

/**
 * Block traverser for extent calculations. Will in general not output
 * anything, but just measure the length of words/glyphs et.c.
 * Also used for some kinds of text data collection.
 */
class SVGTextExtentTraverser : public SVGTextTraverser
{
public:
	SVGTextExtentTraverser(const SVGValueContext& vcxt,
						   SVGDocumentContext* doc_ctx,
						   SVGTextRenderer* tr)
		: SVGTextTraverser(vcxt, doc_ctx, tr),
		  m_traverser_done(FALSE) {}

	virtual void FlushPendingChunks(SVGTextArguments& tparams);

	BOOL IsDone() { return m_traverser_done; }

protected:
	OP_STATUS HandleWord(FragmentContext& frag, int format, SVGTextArguments& tparams);
	OP_STATUS HandleSpace(int format, SVGTextArguments& tparams);

	void HandleChunk(SVGTextArguments& tparams);
	OP_STATUS HandleAbsGlyph(FragmentContext& frag, SVGTextArguments& tparams);

	void HandleDelta(const SVGNumberPair& delta, SVGTextArguments& tparams);
	OP_STATUS HandleRelGlyph(FragmentContext& frag, int format, SVGTextArguments& tparams);

	BOOL m_traverser_done;
};

/**
 * Block traverser for painting and layout of <text>. Will in general
 * output glyphs et.c using the SVGTextRenderer (and the SVGCanvas
 * referenced by it).
 */
class SVGTextPaintTraverser : public SVGTextTraverser
{
public:
	SVGTextPaintTraverser(const SVGValueContext& vcxt,
						  SVGDocumentContext* doc_ctx,
						  SVGTextRenderer* tr)
		: SVGTextTraverser(vcxt, doc_ctx, tr) {}

	virtual void FlushPendingChunks(SVGTextArguments& tparams);

	BOOL IsDone() { return FALSE; }

protected:
	void CheckEllipsis(const uni_char* word, unsigned word_length, int format,
					   int glyph_limit, SVGTextArguments& tparams);

	OP_STATUS HandleWord(FragmentContext& frag, int format, SVGTextArguments& tparams);
	OP_STATUS HandleSpace(int format, SVGTextArguments& tparams);

	void HandleChunk(SVGTextArguments& tparams);
	OP_STATUS HandleAbsGlyph(FragmentContext& frag, SVGTextArguments& tparams);

	void HandleDelta(const SVGNumberPair& delta, SVGTextArguments& tparams);
	OP_STATUS HandleRelGlyph(FragmentContext& frag, int format, SVGTextArguments& tparams);
};

struct SVGLineInfo
{
	SVGLineInfo() : num_fragments(0), forced_break(0) {}

	void Update(SVGNumber frag_width, SVGNumber frag_descent, SVGNumber line_height)
	{
		width += frag_width;
		height = SVGNumber::max_of(height, line_height);
		// FIXME: Should depend on font size
		descent = SVGNumber::max_of(descent, frag_descent);
	}
	void Update(const SVGLineInfo& li)
	{
		width += li.width;
		height = SVGNumber::max_of(height, li.height);
		descent = SVGNumber::max_of(descent, li.descent);
		num_fragments += li.num_fragments;
	}
	void Reset()
	{
		width = 0;
		height = 0;
		descent = 0;
		num_fragments = forced_break = 0;
	}

	SVGNumber width;		///< Width of this line
	SVGNumber height;		///< Height of this line
	SVGNumber descent;		///< Descent of the largest font used on this line

	unsigned num_fragments:31;	///< Number of fragments on this line
	unsigned forced_break:1;	///< A break was forced on this line
};

class LinebreakState
{
public:
	LinebreakState() { Reset(); }

	void Reset()
	{
		last_char = '\0'; last_had_trailing_ws = FALSE; last_was_css_punct = FALSE;
	}

	void UpdateChar(const uni_char* word, unsigned wordlen)
	{
		last_char = word[wordlen - 1];
		last_was_css_punct = uni_ispunct(word[0]);
	}
	void UpdateWS(BOOL has_trailing_whitespace)
	{
		last_had_trailing_ws = has_trailing_whitespace;
	}

	BOOL BreakAllowed(uni_char next_char)
	{
		// FIXME: last_was_css_punct ?
		return (LB_NO != Unicode::IsLinebreakOpportunity(last_char, next_char,
														 last_had_trailing_ws));
	}

private:
	uni_char last_char;
	BOOL last_had_trailing_ws;
	BOOL last_was_css_punct;
};

struct SVGTextAreaInfo
{
	SVGNumber GetStart(SVGLineInfo* li) const
	{
		if (li && (text_align == CSS_VALUE_center ||
				   text_align == CSS_VALUE_end))
		{
			SVGNumber adjust = vis_area.width - li->width;
			if (text_align == CSS_VALUE_center)
				adjust /= 2;
			return vis_area.x + adjust;
		}
		return vis_area.x;
	}

	SVGNumber GetTotalTextHeight() const
	{
		SVGNumber total_height;
		for (unsigned int i = 0; i < lineinfo->GetCount(); ++i)
		{
			SVGLineInfo* li = lineinfo->Get(i);
			if (li)
				total_height += li->height;
		}
		return total_height;
	}

	BOOL BreakNeeded(SVGNumber test_frag_width)
	{
		if (info.width_auto)
			return FALSE;

		SVGLineInfo* li = lineinfo->Get(lineinfo->GetCount() - 1);
		return li->width + test_frag_width > vis_area.width;
	}

	OP_STATUS NewBlock(const HTMLayoutProperties& props);
	OP_STATUS NewBlock();
	void AdvanceBlock(SVGNumberPair& ctp, SVGNumberPair& linepos);

	OP_STATUS Commit()
	{
		OP_ASSERT(lineinfo->GetCount() > 0);

		SVGLineInfo* li = lineinfo->Get(lineinfo->GetCount() - 1);
		if (BreakNeeded(uncommitted.width) &&
			!(li->num_fragments == 0 && !li->forced_break &&
			  uncommitted.width > vis_area.width))
		{
			RETURN_IF_ERROR(NewBlock());
			li = lineinfo->Get(lineinfo->GetCount() - 1);
		}

		li->Update(uncommitted);
		return OpStatus::OK;
	}

	OP_STATUS FlushUncommitted()
	{
		if (lineinfo->GetCount() == 0)
			return OpStatus::OK;

		RETURN_IF_ERROR(Commit());

		uncommitted.Reset();

		uncommitted.Update(uncommitted_space);
		uncommitted_space.Reset();
		return OpStatus::OK;
	}

	OP_STATUS FlushLine()
	{
		if (lineinfo->GetCount() == 0)
			return OpStatus::OK;

		RETURN_IF_ERROR(Commit());

		uncommitted.Reset();
		uncommitted_space.Reset();
		return OpStatus::OK;
	}

	OP_STATUS ForceLineBreak(const HTMLayoutProperties& props)
	{
		RETURN_IF_ERROR(FlushLine());

		SVGLineInfo* li = lineinfo->Get(lineinfo->GetCount() - 1);
		if (li)
			li->forced_break = 1;

		return NewBlock(props);
	}

	SVGRect vis_area;
	OpVector<SVGLineInfo>* lineinfo;

	SVGLineInfo uncommitted;
	SVGLineInfo uncommitted_space;

	LinebreakState lbstate;

	unsigned int current_line;
	unsigned int remaining_fragments_on_line;

	unsigned int ellipsis_end_offset;

	union
	{
		struct
		{
			unsigned width_auto:1;
			unsigned height_auto:1;
			unsigned has_ellipsis:1;
			unsigned has_ws_ellipsis:1;
		} info;
		unsigned short info_init;
	};

	short text_align;
};

/**
 * Block traverser encapsulating commonalities for traversersing
 * <textArea>'s
 */
class SVGTextAreaTraverser : public SVGTextBlockTraverser
{
public:
	SVGTextAreaTraverser(const SVGValueContext& vcxt,
						 SVGDocumentContext* doc_ctx,
						 SVGTextRenderer* tr)
		: SVGTextBlockTraverser(vcxt, doc_ctx, tr) {}

	virtual OP_STATUS HandleAbsolutePositioning(const SVGTextBlock& block, WordInfo* wordinfo_array,
												unsigned int wordinfo_array_len, int& char_count,
												SVGTextArguments& tparams) { return OpStatus::OK; }

	BOOL IsDone() { return FALSE; }
};

/**
 * Block traverser to measure text in a textArea - main purpose being
 * to find line-breaking opportunities and calculate data used when
 * performing alignment.
 */
class SVGTextAreaMeasurementTraverser : public SVGTextAreaTraverser
{
public:
	SVGTextAreaMeasurementTraverser(const SVGValueContext& vcxt,
									SVGDocumentContext* doc_ctx,
									SVGTextRenderer* tr)
		: SVGTextAreaTraverser(vcxt, doc_ctx, tr) {}

	OP_STATUS HandleFragment(const uni_char* word, int word_length, const WordInfo& wi,
							 int text_format, SVGTextArguments& tparams);
};

/**
 * Block traverser used to draw (and layout) text inside a
 * <textArea>. Handles linebreaking and then passes control to its
 * superclass.
 */
class SVGTextAreaPaintTraverser : public SVGTextAreaTraverser
{
public:
	SVGTextAreaPaintTraverser(const SVGValueContext& vcxt,
							  SVGDocumentContext* doc_ctx,
							  SVGTextRenderer* tr)
		: SVGTextAreaTraverser(vcxt, doc_ctx, tr) {}

	OP_STATUS HandleFragment(const uni_char* word, int word_length, const WordInfo& wi,
							 int text_format, SVGTextArguments& tparams);

protected:
	void CheckEllipsis(const uni_char* word, int word_length, const WordInfo& wi,
					   int text_format, const SVGLineInfo* li, SVGTextArguments& tparams);
};

class SVGTextBlock
{
public:
	SVGTextBlock() : m_text(NULL), m_textlength(0) {}

	void SetText(uni_char* text, unsigned textlen) { m_text = text; m_textlength = textlen; }

	void NormalizeSpaces(BOOL preserve_spaces, BOOL keep_leading_whitespace, BOOL keep_trailing_whitespace);
	OP_STATUS RefreshFragments(const HTMLayoutProperties& props, BOOL preserve_spaces,
							   FramesDocument* frames_doc, SVGTextCache* text_cache);

	BOOL IsEmpty() const { return m_textlength == 0; }
#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
	BOOL EndsWithWhitespace() const
	{
		OP_ASSERT(m_textlength > 0);
		return m_text[m_textlength-1] == ' ';
	}
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
	const uni_char* GetText() const { return m_text; }
	unsigned GetLength() const { return m_textlength; }

private:
	uni_char* m_text;
	unsigned m_textlength;
};

class SVGAltGlyphMatcher
{
public:
	SVGAltGlyphMatcher(SVGDocumentContext* doc_ctx, SVGElementResolver* resolver)
		: m_doc_ctx(doc_ctx), m_resolver(resolver) {}

	OP_BOOLEAN Match(HTML_Element* match_root);
	OP_BOOLEAN FetchGlyphData(BOOL horizontal, SVGNumber fontsize,
							  OpVector<GlyphInfo>& glyphs);

private:
	OP_BOOLEAN FetchGlyphElements(HTML_Element* glyphref_elm);

	struct FontMetricInfo
	{
		SVGNumber units_per_em;

		SVGNumber advance_x;
		SVGNumber advance_y;
	};

	BOOL FindFontMetrics(HTML_Element* glyph_elm, FontMetricInfo& fmi);

	OpVector<HTML_Element> m_glyphelms;

	SVGDocumentContext* m_doc_ctx;
	SVGElementResolver* m_resolver;
};

#endif // SVG_SUPPORT
#endif // _SVG_TEXT_LAYOUT_
