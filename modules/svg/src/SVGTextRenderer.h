/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _SVG_TEXT_RENDERER_
#define _SVG_TEXT_RENDERER_

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGGlyphCache.h" // For GlyphInfo
#include "modules/svg/src/SVGFontImpl.h" // For SVGFontImpl::ATTRIBUTE

class HTMLayoutProperties;
class SVGFontDescriptor;

struct GlyphOutline;
class GlyphRun;
struct TextRunInfo;
class SVGCanvas;
class SVGPainter;
class SVGMotionPath;

class FragmentTransform
{
public:
	FragmentTransform() : m_allocated(NULL) {}
	~FragmentTransform() { OP_DELETEA(m_allocated); }

	OP_STATUS Apply(const uni_char* txt, unsigned len, int format);

	uni_char* GetTransformed() const { return m_output; }
	unsigned GetTransformedLength() const { return m_output_len; }

private:
	uni_char* m_allocated;
	uni_char* m_output;
	unsigned m_output_len;
};

class SVGSmallCaps
{
public:
	SVGSmallCaps(SVGFontDescriptor* fontdesc,
				 SVGDocumentContext* font_doc_ctx,
				 SVGNumber expansion, SVGNumber font_size,
				 int glyph_count) :
		m_fontdesc(fontdesc),
		m_font_doc_ctx(font_doc_ctx),
		m_expansion(expansion),
		m_font_size(font_size),
		m_glyph_count(glyph_count) {}

	OP_STATUS ApplyTransform(uni_char* txt, int len, BOOL reverse_word);

protected:
	virtual OP_STATUS HandleSegment(const uni_char* txt, int len, BOOL reverse_word) = 0;

	static unsigned GetCapsSegmentLength(const uni_char* str, unsigned len)
	{
		unsigned count = 1;
		while (len > 1 && *str && *(str+1) && uni_isupper(*str) == uni_isupper(*(str+1)))
		{
			len--;
			str++;
			count++;
		}
		return count;
	}

	int GlyphsRemaining() const { return m_glyph_count; }

	SVGFontDescriptor* m_fontdesc;
	SVGDocumentContext* m_font_doc_ctx; // this is meant to be used for font lookups only
	SVGNumber m_expansion;
	SVGNumber m_font_size;
	int m_glyph_count;
};

class SVGTextRenderer
{
public:
	SVGTextRenderer(SVGCanvas* canvas, SVGDocumentContext* font_doc_ctx, BOOL calc_bbox) :
		m_fontdesc(NULL), m_font_doc_ctx(font_doc_ctx), m_canvas(canvas),
		m_calculate_bbox(calc_bbox) {}

	/**
	 * The current font is set and then the textdrawing is directed at either RenderSystemTextRun or
	 * RenderGlyphRun depending on whether the font in question can get outlines or not.
	 * The latter case is true for SVGFonts and platforms that have implemented OpFont::GetOutline().
	 *
	 * @param text The string to draw
	 * @param len The number of characters from string to draw
	 * @param tparams Current SVGTextArguments inside
	 * @param reverse_word Set to TRUE if the word should be reversed before being output.
	 * @param max_glyph_count The maximum number of glyphs to output.
	 * @param out_consumed_chars In case we hit the max_glyph_count limit this will contain the
	 *  number of chars consumed from the string.
	 */
	OP_STATUS TxtOut(const uni_char* txt, int len, SVGTextArguments& tparams,
					 BOOL reverse_word, int max_glyph_count, int& out_consumed_chars);

	/**
	 * Similar to VisualDevice::TxtOutEX. Performs text
	 * transformations, and then calls TxtOut with the result.
	 */
	OP_STATUS TxtOutEx(const uni_char* txt, unsigned len, int& consumed_chars,
					   int max_glyph_count, int format, SVGTextArguments& tparams);

	/**
	 * Render an alternate glyph (as described by an 'altGlyph' element)
	 *
	 * @param altglyph Path of glyph
	 * @param advance The advance of the glyph
	 * @param units_per_em The units/EM for glyph
	 * @param char_len Number of characters that maps to this glyph
	 * @param tparams Current SVGTextArguments
	 */
	OP_STATUS RenderAlternateGlyphs(OpVector<GlyphInfo>& glyphs, SVGTextArguments& tparams);

	/**
	 * Same as TxtOut but doesn't draw only measures the string.
	 *
	 * @param text The string to measure
	 * @param len The number of characters from string to measure
	 * @param tparams Current SVGTextArguments
	 */
	SVGNumber GetTxtExtent(const uni_char* text, unsigned len, int max_glyph_count,
						   int& out_consumed_chars, int& out_glyphcount, const SVGTextArguments& tparams);

	/**
	 * The analog to TxtOutEx.
	 */
	SVGNumber GetTxtExtentEx(const uni_char* txt, unsigned len, int format,
							 int max_glyph_count, int& out_consumed_chars,
							 int& out_glyphcount, const SVGTextArguments& tparams);

	/**
	 * Like RenderAlternateGlyphs, but only measures the glyph.
	 *
	 * @param advance The advance of the glyph
	 * @param units_per_em The units/EM for glyph
	 * @param tparams Current SVGTextArguments
	 */
	SVGNumber GetAlternateGlyphExtent(OpVector<GlyphInfo>& glyphs, SVGTextArguments& tparams);

	/**
	 * Set the current exact font size.
	 */
	void SetFontSize(SVGNumber exact_font_size) { m_fontsize = exact_font_size; }

	/**
	 * Returns the current font size in user coordinates. This is the exact font-size, not
	 * taking into account which actual font is selected.
	 */
	SVGNumber GetFontSize() const { return m_fontsize; }

	SVGDocumentContext* GetFontDocument() const { return m_font_doc_ctx; }

	/**
	 * Returns the SVGCanvas that this renderer will render to
	 */
	SVGCanvas* GetCanvas() const { return m_canvas; }

	/**
	 * Returns the expansion factor of the CTM of the SVGCanvas
	 */
	SVGNumber GetExpansion() const;

	void SetFontDescriptor(SVGFontDescriptor* fontdesc) { m_fontdesc = fontdesc; }
	SVGFontDescriptor* GetFontDescriptor() { return m_fontdesc; }

	static OP_STATUS SVGFontTxtOut(SVGPainter* painter, OpFont* svgfont, const uni_char* txt, int len, int char_spacing_extra = 0);

	/**
	 * Calculate the bounding box for the rendered pixels of a string with the given font.
	 *
	 * @param bb_out Pointer to the bounding box that will contain the result.
	 * @param svgfont The font to be used.
	 * @param txt Pointer to the string to measure.
	 * @param len The length of the string to measure.
	 * @param char_spacing_extra Extra spacing for each character.
	 */
	static OP_STATUS SVGFontGetStringBoundingBox(SVGBoundingBox *bb_out, OpFont* svgfont, const uni_char* txt, int len, int char_spacing_extra = 0);

private:
	/**
	 * Renders a run of glyphs.
	 *
	 * @param grun The glyph run. In logical order.
	 * @param tparams Current SVGTextArguments
	 * @param reverse_word If the run should be drawn right-to-left.
	 */
	OP_STATUS RenderGlyphRun(GlyphRun& grun, SVGTextArguments& tparams, BOOL reverse_word);

	/**
	 * Renders a text fragment with an OpPainter, meaning no filling or stroking will be done on the
	 * text. Also rotation and skewing is disabled.
	 *
	 * @param text The string to draw. Letters in logical order.
	 * @param len The number of characters from string to draw
	 * @param tparams Current SVGTextArguments
	 * @param reverse_word If the word should be drawn right-to-left.
	 */
	OP_STATUS RenderSystemTextRun(const uni_char* text, unsigned len, SVGTextArguments& tparams, BOOL reverse_word);

#ifdef SVG_SUPPORT_TEXTSELECTION
	/**
	 * Renders the selected text (if any) or retrieves a list of rectangles enclosing it
	 * (if tparams.data->selection_rect_list != NULL)
	 *
	 * @param text The selected string to draw. If reverse_word == TRUE it contains the reversed text
	 * @param len The number of string's characters
	 * @param reverse_word If the word should be drawn right-to-left
	 * @param orig_text If reverse_word == TRUE it contains the original (not reversed) text. Otherwise it's the same as text
	 * @param tparams Current SVGTextArguments
	 * @param int_pos A point where the selected text should be rendered at
	 * @param text_height A height of the text
	 */
	OP_STATUS ProcessSelection(const uni_char* text, unsigned len, BOOL reverse_word, const uni_char* orig_text,
							  SVGTextArguments& tparams, OpPoint int_pos, INT32 text_height);
#endif // SVG_SUPPORT_TEXTSELECTION

	/**
	 * Returns the adjusted width of a string taking platform gridfitting and other adjustments into consideration.
	 *
	 * @param text The text
	 * @param len The number of characters to read from text.
	 * @param letter_spacing The amount of space to insert between any pair of characters, in user space coordinates.
	 * @param pixel_to_userscale The scalefactor for scaling the pixel units to userspace
	 *
	 * @returns The amount of space used by the string in user space coordinates.
	 */
	SVGNumber DrawStringWidth(const uni_char* text, unsigned len, SVGNumber letter_spacing, SVGNumber pixel_to_userscale);

	/**
	 * Draws overline and underline with the current fill/stroke on canvas.
	 */
	void DrawTextDecorations(SVGTextArguments& tparams, SVGNumber width);

	/**
	 * Draws a strikethrough over the current text, starting at start_x and extending width.
	 */
	void DrawStrikeThrough(SVGTextArguments& tparams, SVGNumber start_x, SVGNumber width);

	SVGNumber GetGlyphRunExtent(GlyphRun& grun, const SVGTextArguments& tparams);
	SVGNumber GetSystemTextExtent(const uni_char* text, unsigned len, const SVGTextArguments& tparams, SVGNumber pixel_to_user);

	/**
	 * Helpers for RenderGlyphRun
	 */
	void TextRunSetup(TextRunInfo& tri, SVGMatrix& glyph_adj_xfrm, SVGTextArguments& tparams);
	void CalculateGlyphAdjustment(const TextRunInfo& tri, SVGMatrix& ustrans,
								  GlyphOutline& go, const SVGNumberObject* rotate);
#ifdef SVG_FULL_11
	void CalculateGlyphPositionOnPath(const TextRunInfo& tri, SVGMatrix& ustrans,
									  GlyphOutline& go, SVGMotionPath* path, BOOL warp_outline,
									  SVGNumber path_pos, SVGNumber half_advance,
									  SVGNumber path_displacement);
#endif // SVG_FULL_11
	OP_STATUS DrawOutline(const TextRunInfo& tri, const GlyphOutline& go,
						  SVGMatrix& ustrans, SVGTextArguments& tparams);

	/**
	 * Returns FALSE if a font value could not be gotten.
	 */
	BOOL GetSVGFontAttribute(SVGFontData::ATTRIBUTE attribute, SVGNumber fontheight,
							 SVGNumber& out_value);

	SVGFontDescriptor*	m_fontdesc;
	SVGDocumentContext*	m_font_doc_ctx; // this is meant to be used for font lookups only
	SVGCanvas*			m_canvas;
	SVGNumber			m_fontsize;

	BOOL				m_calculate_bbox;
};

#endif // SVG_SUPPORT
#endif // _SVG_TEXT_RENDERER_
