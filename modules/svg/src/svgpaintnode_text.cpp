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

#include "modules/display/vis_dev.h"
#include "modules/display/styl_man.h"

#include "modules/layout/box/box.h"

#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGFontImpl.h"
#include "modules/svg/src/SVGTextRenderer.h"
#include "modules/svg/src/SVGMotionPath.h"

#include "modules/svg/src/svgpaintnode.h"
#include "modules/svg/src/svgpaintserver.h"

#ifdef SVG_SUPPORT_EDITABLE
# include "modules/svg/src/SVGEditable.h"
#endif // SVG_SUPPORT_EDITABLE

#include "modules/display/color.h" // for CheckColorContrast
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

#include "modules/pi/ui/OpUiInfo.h"

#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
# include "modules/display/textshaper.h"
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT


class SVGDocumentContext;

enum SelectionColor
{
	SELECTED = 0,
	ACTIVE_SEARCH_HIT,
	SEARCH_HIT,
	NUM_SELECTION_TYPES
};

#ifdef SVG_SUPPORT_TEXTSELECTION
struct PaintSelectionAttributes
{
	PaintSelectionAttributes() :
	      doc_ctx(NULL)
	    , current_element(NULL)
#ifdef SVG_SUPPORT_EDITABLE
	    , editable(NULL)
#endif // SVG_SUPPORT_EDITABLE
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
	    , first_selection_elm(NULL)
	    , current_sel_elm(NULL)
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
	    {}

	int 			current_char_idx; // current logical char index
	int 			selected_min_idx;
	int 			selected_max_idx;

	// This is the current context, only to be used within a text-traversal
	SVGDocumentContext*	doc_ctx;				///< Current document context
	HTML_Element*		current_element;		///< Current layouted element (potentially a textnode/group)
#ifdef SVG_SUPPORT_EDITABLE
	SVGEditable*		editable;				///< The editing context of the current text root, or NULL if inactive
#endif // SVG_SUPPORT_EDITABLE

#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
	AutoDeleteList<SelectionElm>
					dummy_selection;		///< Keeps track of any allocated SelectionElm:s
	SelectionElm*	first_selection_elm;	///< The first selection element
	SelectionElm*	current_sel_elm;		///< The current selection element
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
	unsigned int	selection_active:1;
	unsigned int 	svg_selection:1;
	unsigned int 	has_painted_caret:1;		///< TRUE if the caret has been painted in this text root
	COLORREF		selection_colors[NUM_SELECTION_TYPES][2];

	BOOL SelectionEmpty() const
	{
		return (selected_min_idx == selected_max_idx);
	}
	BOOL SelectionActive() const { return (selection_active==1 && selected_min_idx != -1); }

	void Clear()
	{
		selected_min_idx = -1;
		selected_max_idx = 0;
		selection_active = 0;
	}

#ifdef SVG_SUPPORT_EDITABLE
	BOOL DrawCaret(HTML_Element* layouted_elm) const
	{
		return (!SelectionActive() &&
				editable &&
				layouted_elm == editable->m_caret.m_point.elm &&
				(editable->m_caret.m_point.ofs == current_char_idx ||
				 current_char_idx == editable->m_caret.m_point.ofs-1));
	}
	BOOL DrawCaret() const
	{
		return DrawCaret(current_element);
	}
	BOOL DrawCaret(int len) const
	{
		return !SelectionActive() &&
			editable &&
			editable->m_caret.m_point.elm == current_element &&
			editable->m_caret.m_point.ofs <= current_char_idx + len &&
			editable->m_caret.m_point.ofs >= (int)current_char_idx;
	}
	BOOL DrawCaretOnRightSide() const
	{
		OP_ASSERT(editable); // This method may only be called after DrawCaret has returned TRUE
		return(current_char_idx == editable->m_caret.m_point.ofs-1);
	}
#endif // SVG_SUPPORT_EDITABLE

	void AdjustSelectionColor(COLORREF& fg, COLORREF& bg, const ServerName *sn)
	{
		/* if the original text is not readable on the highlight
		background, use a predefined color with more contrast */

		int min_color_contrast = 70;
		long light_font_color = OP_RGB(255, 255, 255);
		long dark_font_color = OP_RGB(0, 0, 0);

		FramesDocument* frm_doc = doc_ctx->GetDocument();
		LayoutMode layout_mode = frm_doc->GetLayoutMode();

		if (layout_mode != LAYOUT_NORMAL)
		{
			PrefsCollectionDisplay::RenderingModes rendering_mode;
			switch (layout_mode)
			{
			case LAYOUT_SSR:
				rendering_mode = PrefsCollectionDisplay::SSR;
				break;
			case LAYOUT_CSSR:
				rendering_mode = PrefsCollectionDisplay::CSSR;
				break;
			case LAYOUT_AMSR:
				rendering_mode = PrefsCollectionDisplay::AMSR;
				break;
#ifdef TV_RENDERING
			case LAYOUT_TVR:
				rendering_mode = PrefsCollectionDisplay::TVR;
				break;
#endif // TV_RENDERING
			default:
				rendering_mode = PrefsCollectionDisplay::MSR;
				break;
			}

			const uni_char *hostname = frm_doc->GetHostName();
			min_color_contrast = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::MinimumTextColorContrast), hostname);
			light_font_color = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TextColorLight), hostname);
			dark_font_color = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TextColorDark), hostname);
		}

		if (CheckColorContrast(fg, bg, min_color_contrast, light_font_color, dark_font_color) != fg)
			fg = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS, sn);
	}

	void SetupSelectionColors()
	{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		const ServerName *sn =
			reinterpret_cast<const ServerName *>(doc_ctx->GetURL().GetAttribute(URL::KServerName, NULL));
		selection_colors[ACTIVE_SEARCH_HIT][0] = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED, sn);
		selection_colors[ACTIVE_SEARCH_HIT][1] = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED, sn);
		selection_colors[SEARCH_HIT][0] = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS, sn);
		selection_colors[SEARCH_HIT][1] = doc_ctx->GetVisualDevice()->GetColor();	// use original text color

		AdjustSelectionColor(selection_colors[SEARCH_HIT][0], selection_colors[SEARCH_HIT][1], sn);
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
	}

	void SetupTextSelection(SVGDocumentContext* doc_ctx, HTML_Element* textselection_elm)
	{
		this->doc_ctx = doc_ctx;
		this->current_element = textselection_elm;

		SetupSelectionColors();

		SVGTextSelection& sel = doc_ctx->GetTextSelection();
		BOOL has_selection = sel.IsValid() && (sel.IsSelecting() || !sel.IsEmpty());
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		SelectionElm* sel_elm = NULL;
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

		if (!has_selection)
		{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
			if (textselection_elm->IsInSelection())
				sel_elm = doc_ctx->GetDocument()->GetHtmlDocument()->GetFirstSelectionElm();
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
		}
		else
		{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
			// Remove any previously allocated SelectionElm
			dummy_selection.Clear();

			sel_elm = OP_NEW(SelectionElm, (doc_ctx->GetDocument(),
											&sel.GetLogicalStartPoint(), &sel.GetLogicalEndPoint()));
			if (!sel_elm)
				return; // OOM

			sel_elm->Into(&dummy_selection);
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

			svg_selection = 1;
		}

#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		first_selection_elm = sel_elm;
		current_sel_elm = NULL; // To make sure Setup...Fragment is called as it should
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
	}

	void SetupTextSelectionForFragment(int current_wordinfo_start)
	{
		current_char_idx = current_wordinfo_start;
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		// Restart with the first SelectionElm
		current_sel_elm = first_selection_elm;

		BOOL has_more_selections;
		do
		{
			has_more_selections = GetNextSelection(doc_ctx, current_element);
		} while (selection_active == 0 && has_more_selections);
#else
		GetNextSelection(doc_ctx, current_element); // Setup the first (and only) selection
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
	}

	BOOL GetNextSelectionForFragment()
	{
		// For last line, calling GetNextSelection will ruin active
		// selection - cannot let that happen
		if (selection_active)
		{
			int old_selected_min = selected_min_idx;
			int old_selected_max = selected_max_idx;

			BOOL has_more_ranges = GetNextSelection(doc_ctx, current_element);

			if (!selection_active)
			{
				selection_active = TRUE;
				selected_min_idx = old_selected_min;
				selected_max_idx = old_selected_max;
			}

			return has_more_ranges;
		}
		else
		{
			return GetNextSelection(doc_ctx, current_element);
		}
	}
	BOOL GetNextSelection(SVGDocumentContext* doc_ctx,
						  HTML_Element* layouted_elm)
	{
		OP_NEW_DBG("SVGTextPainter::GetNextSelection", "svg_selections");

		Clear();

#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		if (!current_sel_elm)
			return FALSE;
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

		SetupSelectionFromCurrentHighlight(layouted_elm);

#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		current_sel_elm = static_cast<SelectionElm*>(current_sel_elm->Suc());
		return current_sel_elm != NULL;
#else
		return FALSE;
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
	}

	COLORREF GetTextColor()
	{
		if (!current_sel_elm)
		{
			current_sel_elm = first_selection_elm;
		}
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		int h_type = current_sel_elm->GetSelection()->GetHighlightType();
#else
		int h_type = 0;
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

		return selection_colors[h_type][1];
	}

	COLORREF GetSelectionColor()
	{
		if (!current_sel_elm)
		{
			current_sel_elm = first_selection_elm;
		}
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		int h_type = current_sel_elm->GetSelection()->GetHighlightType();
#else
		int h_type = 0;
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

		return selection_colors[h_type][0];
	}

	void SetupSelectionFromCurrentHighlight(HTML_Element* layouted_elm)
	{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		OP_ASSERT(current_sel_elm);

		TextSelection* highlight = current_sel_elm->GetSelection();

		const SelectionBoundaryPoint* highlight_end = &highlight->GetEndSelectionPoint();
		const SelectionBoundaryPoint* highlight_start = &highlight->GetStartSelectionPoint();

		/* In case we have overlapping multiple selection matches, we may have to
		end this selection earlier, unless this is the active selection in
		which case it should overlap later selections. */

		if (highlight->GetHighlightType() == TextSelection::HT_SEARCH_HIT)
		{
			if (current_sel_elm->Suc())
			{
				// make sure the active one is not overlapped since it should be on top
				if (SelectionElm* prev = (SelectionElm*) current_sel_elm->Pred())
				{
					TextSelection* prev_highlight = prev->GetSelection();
					if (prev_highlight->GetHighlightType() == TextSelection::HT_ACTIVE_SEARCH_HIT)
					{
						const SelectionBoundaryPoint* prev_end = &prev_highlight->GetEndSelectionPoint();

						if (highlight_start->GetElement() == prev_end->GetElement() &&
							highlight_start->Precedes(*prev_end))
						{
							highlight_start = prev_end;
						}
					}
				}
			}
		}
#else
		SVGTextSelection& sel = doc_ctx->GetTextSelection();
		OP_ASSERT(!sel.IsEmpty());

		const SelectionBoundaryPoint* highlight_end = &sel.GetLogicalEndPoint();
		const SelectionBoundaryPoint* highlight_start = &sel.GetLogicalStartPoint();
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

		BOOL starts_in_element = highlight_start->GetElement() == layouted_elm;
		BOOL ends_in_element = highlight_end->GetElement() == layouted_elm;

		// Selections originating from outside SVG (or, not determined by
		// the SVG module's traversal), will not have been accounted for
		// any potentially collapsed leading whitespace.
		// Calculate the leading whitespace of the current element if it
		// is one of the end-points of the selection.
		int leading_ws = 0;
		if (svg_selection == 0 && (starts_in_element || ends_in_element))
			leading_ws = SVGEditable::CalculateLeadingWhitespace(layouted_elm);

		selected_min_idx = INT_MAX;

		if (starts_in_element)
		{
			selected_min_idx = SVGTextArguments::SelectionPointToOffset(highlight_start, leading_ws);
		}
		else if (highlight_start->GetElement()->Precedes(layouted_elm))
		{
			selected_min_idx = 0;
		}
		selected_max_idx = 0;

		if (ends_in_element)
		{
			selected_max_idx = SVGTextArguments::SelectionPointToOffset(highlight_end, leading_ws);
		}
		else if (!highlight_end->GetElement()->Precedes(layouted_elm))
		{
			selected_max_idx = layouted_elm->GetTextContentLength();
		}

		if (selected_max_idx > selected_min_idx)
		{
			selection_active = 1;
		}
		else
		{
			Clear();
		}
	}
};
#endif // SVG_SUPPORT_TEXTSELECTION

class SVGTextPainter
{
public:
	SVGTextPainter(SVGPainter* painter, SVGDocumentContext* font_doc_ctx) :
		m_attrs(NULL)
		,m_rotation_list(NULL)
		,m_glyph_idx(0)
		,m_font_doc_ctx(font_doc_ctx)
		,m_painter(painter)
#ifdef SVG_FULL_11
		,m_path(NULL)
#endif // SVG_FULL_11
#ifdef SVG_SUPPORT_TEXTSELECTION
		,m_allow_text_selection(TRUE)
#endif // SVG_SUPPORT_TEXTSELECTION
	{}

	void SetPosition(const SVGNumberPair& pos)
	{
		m_position = pos;

#ifdef SVG_FULL_11
		if (m_path)
		{
			m_path_distance = pos.x;
			m_path_displacement = pos.y;
		}
#endif // SVG_FULL_11
	}

#ifdef SVG_FULL_11
	void SetPath(SVGMotionPath* path, BOOL warp) { m_path = path; m_path_warp = !!warp; }
#endif // SVG_FULL_11

	void SetTextAttributes(TextAttributes* text_attrs);
	void SetRotationList(GlyphRotationList* rot_list) { m_rotation_list = rot_list; }
	void SetForceOutlines(BOOL force_outlines)
	{
		m_use_outlines = force_outlines || NodeNeedsVectorFonts();
	}

	void SetFillPaint(const SVGPaintDesc& paint)
	{
		m_paints[0] = paint;
		m_painter->SetFillPaint(paint);
	}
	void SetStrokePaint(const SVGPaintDesc& paint)
	{
		m_paints[1] = paint;
		m_painter->SetStrokePaint(paint);
	}
	void SetObjectProperties(const SVGObjectProperties& oprops)
	{
		m_obj_props = oprops;
		m_painter->SetObjectProperties(&m_obj_props);
	}
	void SetStrokeProperties(const SVGStrokeProperties* strokeprops)
	{
		m_stroke_props = *strokeprops;
		m_painter->SetStrokeProperties(&m_stroke_props);
	}

	void SetFontNumber(int font_number) { m_fontdesc.SetFontNumber(font_number); }
	void SetFontSize(SVGNumber fontsize) { m_fontsize = fontsize; }
	SVGNumber GetFontSize() const { return m_fontsize; }

#ifdef SVG_SUPPORT_TEXTSELECTION
	void SetupTextSelection(SVGDocumentContext* doc_ctx, HTML_Element* textselection_elm) { m_sel_attrs.SetupTextSelection(doc_ctx, textselection_elm); }
	void SetupTextSelectionForFragment(int current_wordinfo_start)
	{
		if (m_allow_text_selection)
			m_sel_attrs.SetupTextSelectionForFragment(current_wordinfo_start);
	}
	void AllowSelectionOfFragment(BOOL allow)
	{
		m_allow_text_selection = allow;
		if (!allow)
			m_sel_attrs.Clear();
	}
# ifdef SVG_SUPPORT_EDITABLE
	void SetEditable(SVGEditable* editable) { m_sel_attrs.editable = editable; m_sel_attrs.has_painted_caret = 0; }
# endif // SVG_SUPPORT_EDITABLE
#endif // SVG_SUPPORT_TEXTSELECTION

	enum
	{
		DECO_UNDERLINE		= 1 << 0,
		DECO_OVERLINE		= 1 << 1,
		DECO_LINETHROUGH	= 1 << 2
	};

	void SetGlyphProperties();

	SVGNumber GetExpansion() const { return m_painter->GetTransform().GetExpansionFactor(); }

	OP_STATUS CheckFont(SVGNumber expansion, SVGNumber fontsize)
	{
		return m_fontdesc.Check(m_font_doc_ctx, expansion, fontsize);
	}

	OP_STATUS OutputEx(const uni_char* txt, int len, int format);
	OP_STATUS Output(const uni_char* txt, int len, BOOL reverse_word);
	OP_STATUS OutputSpace(BOOL reverse_word);

	OP_STATUS RenderAlternateGlyphs(OpVector<GlyphInfo>& glyphs);

private:
	struct TextRunInfo
	{
		SVGNumberPair startpoint_trans;
		SVGNumber font_to_user_scale;
		SVGNumber font_to_user_and_glyph_scale;
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

	BOOL FragmentNeedsVectorFonts() const;
	BOOL NodeNeedsVectorFonts() const;
	BOOL CurrentGlyphHasRotation() const
	{
		return m_rotation_list && m_glyph_idx < m_rotation_list->length;
	}

	OP_STATUS RenderGlyphRun(GlyphRun& grun, BOOL reverse_word);

	void TextRunSetup(TextRunInfo& tri, SVGMatrix& glyph_adj_xfrm);
	void CalculateGlyphAdjustment(const TextRunInfo& tri, SVGMatrix& ustrans, GlyphOutline& go);
#ifdef SVG_FULL_11
	void CalculateGlyphPositionOnPath(const TextRunInfo& tri, SVGMatrix& ustrans, GlyphOutline& go);
#endif // SVG_FULL_11
	OP_STATUS DrawOutline(const TextRunInfo& tri, const GlyphOutline& go, SVGMatrix& ustrans);

	OP_STATUS RenderSystemTextRun(const uni_char* text, unsigned len, BOOL reverse_word);
	void RenderSystemTextRunWithHighlight(const OpPoint& int_pos, const uni_char* text, const uni_char* orig_text, unsigned len, BOOL reverse_word, int pixel_extra_spacing);

	void DrawStrikeThrough(SVGNumber start_x, SVGNumber width);
	void DrawTextDecorations(SVGNumber width);

	void SetupAndDrawCaret(OpFont* font, SVGNumber advance, SVGNumber fontsize, SVGNumber font_to_user_scale);

	BOOL GetSVGFontAttribute(SVGFontData::ATTRIBUTE attribute, SVGNumber fontheight, SVGNumber& out_value);

	BOOL IsVertical() const
	{
		return m_attrs->writing_mode == SVGWRITINGMODE_TB_RL || m_attrs->writing_mode == SVGWRITINGMODE_TB;
	}

	void Advance(SVGNumber adv)
	{
		switch (m_attrs->writing_mode)
		{
		default:
		case SVGWRITINGMODE_LR:   /* ( 1, 0 ) */
		case SVGWRITINGMODE_LR_TB:
			m_position.x += adv;
			break;
		case SVGWRITINGMODE_RL:   /* (-1, 0 ) */
		case SVGWRITINGMODE_RL_TB:
			m_position.x += -adv;
			break;
		case SVGWRITINGMODE_TB:   /* ( 0, 1 ) */
		case SVGWRITINGMODE_TB_RL:
			m_position.y += adv;
			break;
		}

#ifdef SVG_FULL_11
		if (m_path)
			m_path_distance += adv;
#endif // SVG_FULL_11
	}

	static inline BOOL OpFontIsSVGFont(OpFont* font)
	{
		return font->Type() == OpFontInfo::SVG_WEBFONT;
	}

	TextAttributes*		m_attrs;
	GlyphRotationList*	m_rotation_list;
	unsigned			m_glyph_idx;

	SVGPaintDesc		m_paints[2];
	SVGObjectProperties	m_obj_props;
	SVGStrokeProperties	m_stroke_props;

	SVGGlyphLookupContext m_glyph_props;
	SVGFontDescriptor	m_fontdesc;

	SVGDocumentContext*	m_font_doc_ctx;
	SVGPainter*			m_painter;

	SVGNumberPair		m_position;

#ifdef SVG_FULL_11
	SVGMotionPath*		m_path;
	SVGNumber			m_path_distance;
	SVGNumber			m_path_displacement;
	unsigned			m_path_warp:1;
#endif // SVG_FULL_11

	SVGNumber			m_fontsize;
	BOOL				m_use_outlines;
	BOOL				m_allow_text_selection;

#ifdef SVG_SUPPORT_TEXTSELECTION
	PaintSelectionAttributes m_sel_attrs;
#endif // SVG_SUPPORT_TEXTSELECTION
};

void TextRunList::Append(TextRun* run)
{
	if (!first)
		first = run;
	else
		last->next = run;

	last = run;
}

void TextRunList::Clear()
{
	TextRun* run = first;
	while (run)
	{
		TextRun* next = run->next;
		OP_DELETE(run);
		run = next;
	}
	first = last = NULL;
}

static const WordInfo* AdjustFragment(const TextRun* textrun, unsigned i,
									  const WordInfo* wi, WordInfo& syn_wi)
{
	if (textrun->start_offset == 0 && textrun->end_offset == 0)
		return wi;

	unsigned start = wi->GetStart();
	unsigned end = start + wi->GetLength();

	if (i == 0 && textrun->start_offset)
	{
		start += textrun->start_offset;
	}
	if (i + 1 == textrun->length && textrun->end_offset)
	{
		end = wi->GetStart() + textrun->end_offset;
	}

	syn_wi = *wi;
	syn_wi.SetStart(start);
	syn_wi.SetLength(end - start);

	if (i + 1 == textrun->length && syn_wi.HasTrailingWhitespace())
		// If the original fragment had trailing whitespace, and has
		// no specified end offset (==0), then keep the trailing
		// whitespace flag set.
		syn_wi.SetHasTrailingWhitespace(textrun->end_offset == 0);

	return &syn_wi;
}

void SVGTextContentPaintNode::PaintRuns(SVGTextPainter& text_painter)
{
	TempBuffer text_buffer;
	OP_ASSERT(m_text_cache);

	HTML_Element* text_content_elm = m_text_cache->GetTextContentElement();
	unsigned textlen = text_content_elm->GetTextContentLength();
	OP_ASSERT(textlen != 0);

	RETURN_VOID_IF_ERROR(text_buffer.Expand(textlen + 1));
	uni_char* text_buf = text_buffer.GetStorage();
	textlen = text_content_elm->GetTextContent(text_buf, textlen + 1);

#ifdef SVG_SUPPORT_TEXTSELECTION
	text_painter.SetupTextSelection(m_textselection_doc_ctx, m_textselection_element);
#endif // SVG_SUPPORT_TEXTSELECTION

	// Normalize spaces (if needed)
	SVGTextBlock text_block;
	text_block.SetText(text_buf, textlen);
	text_block.NormalizeSpaces(m_text_cache->GetPreserveSpaces(),
							   m_text_cache->GetKeepLeadingWS(),
							   m_text_cache->GetKeepTrailingWS());
	const uni_char* text = text_block.GetText();
	const WordInfo* words = m_text_cache->GetWords();

	WordInfo syn_wi;

	// Iterate runs, paint runs
	// FIXME: Visibility determination
	for (TextRun* textrun = m_runs.First(); textrun; textrun = textrun->Suc())
	{
		// Better to not have invisible runs in the list at all?
		if (!textrun->is_visible)
			continue;

		// Set run attributes
		text_painter.SetPosition(textrun->position);
		text_painter.SetGlyphProperties();

#ifdef SUPPORT_TEXT_DIRECTION
		const SVGTextFragment* fraglist = m_text_cache->GetTextFragments();

		if (fraglist)
		{
			unsigned fragnum = textrun->start;
			for (unsigned i = 0; i < textrun->length; ++i, ++fragnum)
			{
				const SVGTextFragment* frag = fraglist + fraglist[fragnum].idx.next;
				const WordInfo* wi = words + frag->idx.wordinfo;

				// If there are further limits, apply those
				wi = AdjustFragment(textrun, i, wi, syn_wi);

				int text_format = m_node_text_format;
				if (frag->packed.bidi & BIDI_FRAGMENT_ANY_MIRRORED)
					text_format |= TEXT_FORMAT_REVERSE_WORD;
				if (wi->IsStartOfWord())
					text_format |= TEXT_FORMAT_IS_START_OF_WORD;

				text_painter.SetFontNumber(wi->GetFontNumber());
				text_painter.SetStrokeProperties(&stroke_props);

				PaintFragment(&text_painter, text, wi, text_format);
			}
		}
		else
#endif // SUPPORT_TEXT_DIRECTION
		{
			unsigned fragnum = textrun->start;
			for (unsigned i = 0; i < textrun->length; ++i, ++fragnum)
			{
				const WordInfo* wi = words + fragnum;

				// If there are further limits, apply those
				wi = AdjustFragment(textrun, i, wi, syn_wi);

				int text_format = m_node_text_format;
				if (wi->IsStartOfWord())
					text_format |= TEXT_FORMAT_IS_START_OF_WORD;

				text_painter.SetFontNumber(wi->GetFontNumber());
				text_painter.SetStrokeProperties(&stroke_props);

				PaintFragment(&text_painter, text, wi, text_format);
			}
		}

		// Any ellipsis on this run? - should always be at the end of
		// the run, and the ending fragment of the run should be
		// clipped if needed. (FIXME: RTL)
		if (textrun->has_ellipsis)
		{
			SVG_DECLARE_ELLIPSIS;

			syn_wi.SetStart(0);
			syn_wi.SetLength(ellipsis_length);
			syn_wi.SetHasTrailingWhitespace(FALSE);

#ifdef SVG_SUPPORT_TEXTSELECTION
			text_painter.AllowSelectionOfFragment(FALSE); // don't allow selection of the ellipsis
#endif // SVG_SUPPORT_TEXTSELECTION

			PaintFragment(&text_painter, ellipsis_string, &syn_wi, m_node_text_format);

#ifdef SVG_SUPPORT_TEXTSELECTION
			text_painter.AllowSelectionOfFragment(TRUE);
#endif // SVG_SUPPORT_TEXTSELECTION
		}
	}
}

void SVGTextContentPaintNode::UpdateState(SVGCanvasState* state)
{
	SVGGeometryNode::UpdateState(state);

	obj_props.aa_quality = VEGA_DEFAULT_QUALITY;
}

void SVGTextContentPaintNode::PaintContent(SVGPainter* painter)
{
	SVGBoundingBox extents;
	GetLocalExtents(extents);

	// Setup bounds for painter use
	painter->SetPainterRect(painter->GetTransform().ApplyToBoundingBox(extents).GetEnclosingRect());

	// Create a text-painter
	SVGTextPainter text_painter(painter, m_font_doc_ctx);
	text_painter.SetTextAttributes(m_attributes);
	text_painter.SetRotationList(m_rotation_list);

	text_painter.SetFillPaint(paints[0]);
	text_painter.SetStrokePaint(paints[1]);
	text_painter.SetObjectProperties(obj_props);
#ifdef SVG_SUPPORT_EDITABLE
	text_painter.SetEditable(m_editable);
#endif // SVG_SUPPORT_EDITABLE

#ifdef SVG_FULL_11
	SVGMotionPath* mp = NULL;
	if (m_textpath_attributes && m_textpath_attributes->path)
	{
		mp = OP_NEW(SVGMotionPath, ());
		if (mp)
		{
			mp->SetTransform(m_textpath_attributes->path_transform);
			mp->Set(m_textpath_attributes->path, -1);
		}
		text_painter.SetPath(mp, m_textpath_attributes->path_warp);
		text_painter.SetForceOutlines(TRUE);
	}
	else
#endif // SVG_FULL_11
	{
		text_painter.SetForceOutlines(FALSE);
	}

	painter->BeginDrawing(this);

	PaintRuns(text_painter);

	painter->EndDrawing();

	if (painter->IsPainterActive())
		painter->ReleasePainter();

#ifdef SVG_FULL_11
	OP_DELETE(mp);
#endif // SVG_FULL_11
}

#ifdef SVG_SUPPORT_STENCIL
void SVGTextContentPaintNode::ClipContent(SVGPainter* painter)
{
	TransferObjectProperties(painter);

	SVGBoundingBox extents;
	GetLocalExtents(extents);

	// Setup bounds for painter use
	painter->SetPainterRect(painter->GetTransform().ApplyToBoundingBox(extents).GetEnclosingRect());

	// Create a text-painter
	SVGTextPainter text_painter(painter, m_font_doc_ctx);
	text_painter.SetTextAttributes(m_attributes);

	text_painter.SetFillPaint(paints[0]);
	text_painter.SetStrokePaint(paints[1]);
	text_painter.SetObjectProperties(obj_props);

	text_painter.SetForceOutlines(TRUE);

	PaintRuns(text_painter);
}
#endif // SVG_SUPPORT_STENCIL

void SVGTextContentPaintNode::GetLocalExtents(SVGBoundingBox& extents)
{
	extents.UnionWith(m_stored_bbox);

	if (obj_props.stroked)
		extents.Extend(stroke_props.Extents());
}

SVGBoundingBox SVGTextContentPaintNode::GetBBox()
{
	// It could be possible/needed to be able to actually recalculate
	// the bounding box by walking the runs. It will depend on other
	// state being clean though, but that probably isn't much of an
	// issue in the context where it would apply.
	return m_stored_bbox;
}

void SVGTextContentPaintNode::UpdateRunBBox(const SVGBoundingBox& run_bbox)
{
	m_stored_bbox.UnionWith(run_bbox);
}

OP_STATUS SVGTextContentPaintNode::PaintFragment(SVGTextPainter* text_painter, const uni_char* text,
												 const WordInfo* fragment, int format)
{
	RETURN_IF_ERROR(text_painter->CheckFont(text_painter->GetExpansion(),
											text_painter->GetFontSize()));

	BOOL preserve_spaces = m_text_cache->GetPreserveSpaces();
	unsigned int frag_start = fragment->GetStart();
	const uni_char* frag_text = text + frag_start;
	unsigned frag_len = fragment->GetLength();
	BOOL has_trailing_ws = !preserve_spaces && fragment->HasTrailingWhitespace();
	BOOL reverse_fragment = (format & TEXT_FORMAT_REVERSE_WORD) != 0;

	if (reverse_fragment && has_trailing_ws)
	{
#ifdef SVG_SUPPORT_TEXTSELECTION
		text_painter->SetupTextSelectionForFragment(frag_start-1);
#endif // SVG_SUPPORT_TEXTSELECTION
		text_painter->OutputSpace(reverse_fragment);
	}

	OP_STATUS status = OpStatus::OK;
	if (frag_len)
	{
#ifdef SVG_SUPPORT_TEXTSELECTION
		text_painter->SetupTextSelectionForFragment(frag_start);
#endif // SVG_SUPPORT_TEXTSELECTION
		text_painter->OutputEx(frag_text, frag_len, format);
	}

	if (!reverse_fragment && has_trailing_ws)
	{
#ifdef SVG_SUPPORT_TEXTSELECTION
		text_painter->SetupTextSelectionForFragment(frag_start+frag_len);
#endif // SVG_SUPPORT_TEXTSELECTION
		text_painter->OutputSpace(reverse_fragment);
	}

	return status;
}

void SVGTextReferencePaintNode::Free()
{
	// Detach our own content node to avoid it getting free'd.
	m_content_node.Detach();

	SVGTextAttributePaintNode::Free();
}

SVGAltGlyphNode::~SVGAltGlyphNode()
{
	SVGPaintServer::DecRef(paints[0].pserver);
	SVGPaintServer::DecRef(paints[1].pserver);
	SVGResolvedDashArray::DecRef(stroke_props.dash_array);
}

unsigned SVGTextContentPaintNode::GetEstimatedSize()
{
	OP_ASSERT(m_text_cache && m_text_cache->IsValid());
	WordInfo* words = m_text_cache->GetWords();
	unsigned word_count = m_text_cache->GetWordCount();
	WordInfo& last_word = words[word_count - 1];

	// Estimate the size of the rotation list. Estimate because it
	// could be larger than the actual number of rotations.
	unsigned estimated_size = last_word.GetStart() + last_word.GetLength();
	if (last_word.HasTrailingWhitespace() && !m_text_cache->GetPreserveSpaces())
		estimated_size++;

	return estimated_size;
}

unsigned SVGAltGlyphNode::GetEstimatedSize()
{
	// fudged values, hoping that we will render the altGlyph and not the fallback
	return m_glyphs ? m_glyphs->GetCount() : 4;
}

void SVGAltGlyphNode::UpdateState(SVGCanvasState* state)
{
	obj_props.stroked = state->UseStroke();
	obj_props.filled = state->UseFill();
	obj_props.fillrule = state->GetFillRule();
	obj_props.aa_quality = VEGA_DEFAULT_QUALITY;

	if (obj_props.stroked)
	{
		SVGResolvedDashArray::AssignRef(stroke_props.dash_array, state->GetStrokeDashArray());
		stroke_props.dash_offset = state->GetStrokeDashOffset();
		stroke_props.width = state->GetStrokeLineWidth();
		stroke_props.miter_limit = state->GetStrokeMiterLimit();
		stroke_props.cap = state->GetStrokeCapType();
		stroke_props.join = state->GetStrokeJoinType();
		stroke_props.non_scaling = state->GetVectorEffect() == SVGVECTOREFFECT_NON_SCALING_STROKE;

		paints[1].color = state->GetActualStrokeColor();
		paints[1].opacity = state->GetStrokeOpacity();
		SVGPaintServer::AssignRef(paints[1].pserver, state->GetStrokePaintServer());
	}

	if (obj_props.filled)
	{
		paints[0].color = state->GetActualFillColor();
		paints[0].opacity = state->GetFillOpacity();
		SVGPaintServer::AssignRef(paints[0].pserver, state->GetFillPaintServer());
	}
}

void SVGAltGlyphNode::PaintGlyphs(SVGPainter* painter)
{
	// Create a text-painter
	SVGTextPainter text_painter(painter, m_font_doc_ctx);
	text_painter.SetTextAttributes(&m_attributes);

	text_painter.SetFillPaint(paints[0]);
	text_painter.SetStrokePaint(paints[1]);
	text_painter.SetObjectProperties(obj_props);
	text_painter.SetStrokeProperties(&stroke_props);
#ifdef SVG_SUPPORT_EDITABLE
	text_painter.SetEditable(m_editable);
#endif // SVG_SUPPORT_EDITABLE
	text_painter.SetRotationList(m_rotation_list);


#ifdef SVG_FULL_11
	SVGMotionPath* mp = NULL;
	if (m_textpath_attributes && m_textpath_attributes->path)
	{
		mp = OP_NEW(SVGMotionPath, ());
		if (mp)
		{
			mp->Set(m_textpath_attributes->path, -1);
			mp->SetTransform(m_textpath_attributes->path_transform);
		}
		text_painter.SetPath(mp, m_textpath_attributes->path_warp);
		text_painter.SetForceOutlines(TRUE);
	}
	else
#endif // SVG_FULL_11
	{
		text_painter.SetForceOutlines(FALSE);
	}

	painter->BeginDrawing(this);

#ifdef SVG_SUPPORT_TEXTSELECTION
	text_painter.SetupTextSelection(m_textselection_doc_ctx, m_textselection_element);
	text_painter.SetupTextSelectionForFragment(0);
#endif // SVG_SUPPORT_TEXTSELECTION

	text_painter.SetPosition(m_position);
	if (OpStatus::IsSuccess(text_painter.CheckFont(1, text_painter.GetFontSize())))
		text_painter.RenderAlternateGlyphs(*m_glyphs);

	painter->EndDrawing();

#ifdef SVG_FULL_11
	OP_DELETE(mp);
#endif // SVG_FULL_11
}

void SVGAltGlyphNode::PaintContent(SVGPainter* painter)
{
	if (!m_glyphs)
	{
		SVGTextAttributePaintNode::PaintContent(painter);
		return;
	}

	PaintGlyphs(painter);
}

#ifdef SVG_SUPPORT_STENCIL
void SVGAltGlyphNode::ClipContent(SVGPainter* painter)
{
	if (!m_glyphs)
	{
		SVGTextAttributePaintNode::PaintContent(painter);
		return;
	}

	PaintGlyphs(painter);
}
#endif // SVG_SUPPORT_STENCIL

void SVGAltGlyphNode::GetLocalExtents(SVGBoundingBox& extents)
{
	if (!m_glyphs)
	{
		SVGTextAttributePaintNode::GetLocalExtents(extents);
		return;
	}

	extents.UnionWith(m_stored_bbox);

	if (obj_props.stroked)
		extents.Extend(stroke_props.Extents());
}

SVGBoundingBox SVGAltGlyphNode::GetBBox()
{
	if (!m_glyphs)
		return SVGTextAttributePaintNode::GetBBox();

	// It could be possible/needed to be able to actually recalculate
	// the bounding box by walking the glyph vector.
	return m_stored_bbox;
}

BOOL SVGTextPainter::GetSVGFontAttribute(SVGFontData::ATTRIBUTE attribute,
										 SVGNumber fontheight, SVGNumber& out_value)
{
	OpFont* current_font = m_fontdesc.GetFont();
	if (OpFontIsSVGFont(current_font))
	{
		SVGFontImpl* svg_font = static_cast<SVGFontImpl*>(current_font);
		out_value = svg_font->GetSVGFontAttribute(attribute) * m_fontdesc.GetFontToUserScale();
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

BOOL SVGTextPainter::FragmentNeedsVectorFonts() const
{
	if (m_use_outlines)
		return TRUE;

	if (CurrentGlyphHasRotation())
		return TRUE;

	return FALSE;
}

BOOL SVGTextPainter::NodeNeedsVectorFonts() const
{
	const SVGMatrix& current_transform = m_painter->GetTransform();
	BOOL has_trivial_transform =
		current_transform.IsScaledTranslation() &&
		current_transform[0] == current_transform[3];

	if (!has_trivial_transform)
		return TRUE;

	// Using stroke
	if (m_obj_props.stroked)
		return TRUE;

	// Using paint server (for fill)
	if (m_paints[0].pserver)
		return TRUE;

#ifdef SVG_FULL_11
	if (m_path)
		return TRUE;
#endif // SVG_FULL_11

	if (m_attrs->extra_spacing.NotEqual(0) || m_attrs->glyph_scale.NotEqual(1))
		return TRUE;

	// Platforms (e.g. StringWidth) can't generally handle negative
	// letter spacing
	if (m_attrs->letter_spacing < 0)
		return TRUE;

	if (m_attrs->geometric_precision)
		return TRUE;

	if (IsVertical())
		return TRUE;

	// Horizontal text, but with a forced glyph orientation.
	if (m_attrs->orientation != 0)
		return TRUE;

	return FALSE;
}

void SVGTextPainter::SetGlyphProperties()
{
	m_glyph_props.horizontal = !IsVertical();
	m_glyph_props.orientation = m_attrs->orientation;
	m_glyph_props.is_auto = m_attrs->orientation_is_auto;
}

void SVGTextPainter::SetTextAttributes(TextAttributes* text_attrs)
{
	m_attrs = text_attrs;

#ifdef SVG_SUPPORT_TEXTSELECTION
	// FIXME: avoid data duplication
	m_sel_attrs.selection_colors[SELECTED][0] = text_attrs->selection_bg_color;
	m_sel_attrs.selection_colors[SELECTED][1] = text_attrs->selection_fg_color;
#endif // SVG_SUPPORT_TEXTSELECTION

	SetFontSize(text_attrs->fontsize);

	FontAtt& fa = m_fontdesc.GetFontAttributes();
	fa.SetWeight(text_attrs->weight);
	fa.SetItalic(text_attrs->italic);
	fa.SetSmallCaps(text_attrs->smallcaps);
}

OP_STATUS SVGTextPainter::OutputSpace(BOOL reverse_word)
{
	const uni_char space_char = ' ';

	OP_STATUS status = Output(&space_char, 1, reverse_word);
	Advance(m_attrs->word_spacing);
	return status;
}

class SVGSmallCapsPainter : public SVGSmallCaps
{
public:
	SVGSmallCapsPainter(SVGTextPainter* text_painter,
						SVGFontDescriptor* fontdesc,
						SVGDocumentContext* doc_ctx) :
		SVGSmallCaps(fontdesc, doc_ctx,
					 text_painter->GetExpansion(),
					 text_painter->GetFontSize(),
					 1 /* just set to non-zero */),
		m_textpainter(text_painter) {}

protected:
	virtual OP_STATUS HandleSegment(const uni_char* txt, int len, BOOL reverse_word)
	{
		return m_textpainter->Output(txt, len, reverse_word);
	}

	SVGTextPainter* m_textpainter;
};

OP_STATUS SVGTextPainter::OutputEx(const uni_char* txt, int len, int format)
{
	FragmentTransform fragxfrm;
	RETURN_IF_ERROR(fragxfrm.Apply(txt, len, format | TEXT_FORMAT_BIDI_PRESERVE_ORDER));

	uni_char* txt_out = fragxfrm.GetTransformed();
	len = fragxfrm.GetTransformedLength();

	if (!txt_out || !len)
		return OpStatus::OK;

	BOOL reverse_word = (format & TEXT_FORMAT_REVERSE_WORD) != 0;
	if (format & TEXT_FORMAT_SMALL_CAPS)
	{
		SVGSmallCapsPainter sc_painter(this, &m_fontdesc, m_font_doc_ctx);
		return sc_painter.ApplyTransform(txt_out, len, reverse_word);
	}
	else
	{
		return Output(txt_out, len, reverse_word);
	}
}

OP_STATUS SVGTextPainter::Output(const uni_char* txt, int len, BOOL reverse_word)
{
	OP_ASSERT(txt && len > 0);

	OpFont* current_font = m_fontdesc.GetFont();
	OP_ASSERT(current_font);

	if (m_fontsize <= 0)
		return OpStatus::OK;

	BOOL is_svgfont = FALSE;
	BOOL render_with_painter = !FragmentNeedsVectorFonts();
	if (render_with_painter)
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
			render_with_painter = FALSE;
		}
	}
	else
	{
		// Check if the current font supports outlines by asking with a NULL path pointer
		// Note: if the platform returns ERR_NOT_SUPPORTED then
		// fallback to using the painter, and it will look broken in
		// many cases.
		SVGNumber adv;
		UINT32 strpos = 0;
		render_with_painter = (current_font->GetOutline(txt, len, strpos, strpos, TRUE, adv, NULL) == OpStatus::ERR_NOT_SUPPORTED);
	}

	OP_STATUS err = OpStatus::OK;

	if (render_with_painter)
	{
		// Only get the OpPainter once
		if (!m_painter->IsPainterActive())
		{
			err = m_painter->GetPainter(current_font->Overhang());

			RETURN_IF_MEMORY_ERROR(err);

			if (OpStatus::IsError(err))
				// We failed to acquire a painter from the
				// bitmap, make one last try with vector fonts
				render_with_painter = FALSE;
		}
	}

	if (render_with_painter)
	{
		err = RenderSystemTextRun(txt, len, reverse_word);
	}
	else
	{
		GlyphRun grun(len);

		if (is_svgfont)
			static_cast<SVGFontImpl*>(current_font)->SetMatchLang(m_attrs->language);

		OP_ASSERT(!!m_glyph_props.horizontal == !IsVertical());

		err = grun.MapToGlyphs(current_font, txt, len, m_glyph_props);

		if (is_svgfont)
			static_cast<SVGFontImpl*>(current_font)->SetMatchLang(NULL);

		RETURN_IF_ERROR(err);

		err = RenderGlyphRun(grun, reverse_word);
	}
	return err;
}

void SVGTextPainter::DrawStrikeThrough(SVGNumber start_x, SVGNumber width)
{
	SVGNumber thickness, position;
	GetSVGFontAttribute(SVGFontData::STRIKETHROUGH_THICKNESS, m_fontsize, thickness);
	GetSVGFontAttribute(SVGFontData::STRIKETHROUGH_POSITION, m_fontsize, position);

	SVGNumber linepos = m_position.y - m_attrs->baseline_shift - position - thickness/2;

	m_painter->SetFillPaint(m_attrs->deco_paints[0]);
	m_painter->SetStrokePaint(m_attrs->deco_paints[1]);

	m_painter->DrawRect(start_x, linepos, width, thickness, 0, 0);

	m_painter->SetFillPaint(m_paints[0]);
	m_painter->SetStrokePaint(m_paints[1]);
}

void SVGTextPainter::DrawTextDecorations(SVGNumber width)
{
	// Markup::SVGA_UNDERLINE_THICKNESS/Markup::SVGA_OVERLINE_THICKNESS (a property of the font),
	// arbitrary good looking number

	m_painter->SetFillPaint(m_attrs->deco_paints[0]);
	m_painter->SetStrokePaint(m_attrs->deco_paints[1]);

	if (m_attrs->decorations & TextAttributes::DECO_UNDERLINE)
	{
		SVGNumber thickness, position;
		GetSVGFontAttribute(SVGFontData::UNDERLINE_THICKNESS, m_fontsize, thickness);
		GetSVGFontAttribute(SVGFontData::UNDERLINE_POSITION, m_fontsize, position);

		SVGNumber linepos = m_position.y - m_attrs->baseline_shift - position - thickness/2;
		m_painter->DrawRect(m_position.x, linepos, width, thickness, 0, 0);
	}

	if (m_attrs->decorations & TextAttributes::DECO_OVERLINE)
	{
		SVGNumber thickness, position;
		GetSVGFontAttribute(SVGFontData::OVERLINE_THICKNESS, m_fontsize, thickness);
		GetSVGFontAttribute(SVGFontData::OVERLINE_POSITION, m_fontsize, position);

		SVGNumber linepos = m_position.y - m_attrs->baseline_shift - position - thickness/2;
		m_painter->DrawRect(m_position.x, linepos, width, thickness, 0, 0);
	}

	m_painter->SetFillPaint(m_paints[0]);
	m_painter->SetStrokePaint(m_paints[1]);
}

#ifdef SVG_SUPPORT_TEXTSELECTION
void SVGTextPainter::RenderSystemTextRunWithHighlight(const OpPoint& int_pos, const uni_char* text, const uni_char* orig_text,
														   unsigned len, BOOL reverse_word, int pixel_extra_spacing)
{
	OpFont* opfont = m_fontdesc.GetFont();
#ifdef _DEBUG
	OP_ASSERT(!OpFontIsSVGFont(opfont));
#endif // _DEBUG
	int text_height = opfont->Height();
	BOOL has_more_ranges = FALSE;
	BOOL done_with_selection = FALSE;

	int logical_start = m_sel_attrs.current_char_idx;
	int logical_end = reverse_word ? (logical_start - len) : (logical_start + len);

	do
	{
		done_with_selection = !m_sel_attrs.SelectionActive() || m_sel_attrs.SelectionEmpty();

		if (!done_with_selection)
		{
			int word_bound = reverse_word ? logical_end : logical_start;
			int word_start_idx = MAX(0, m_sel_attrs.selected_min_idx - word_bound);
			int word_stop_idx = MIN((int)len, m_sel_attrs.selected_max_idx - word_bound);
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

				m_painter->DrawSelectedStringPainter(selrect, m_sel_attrs.GetSelectionColor(), m_sel_attrs.GetTextColor(), opfont, int_pos, text, len, pixel_extra_spacing);
			}
		}

		BOOL had_more_ranges = m_sel_attrs.GetNextSelectionForFragment();

		done_with_selection = !had_more_ranges;

	} while (!done_with_selection || has_more_ranges);
}
#endif // SVG_SUPPORT_TEXTSELECTION

OP_STATUS SVGTextPainter::RenderSystemTextRun(const uni_char* text, unsigned len, BOOL reverse_word)
{
	if (m_painter->GetTransform().GetExpansionFactor().Close(0))
		// Don't render text that should be invisible
		return OpStatus::OK;

	OpFont* opfont = m_fontdesc.GetFont();
#ifdef _DEBUG
	OP_ASSERT(!OpFontIsSVGFont(opfont));
#endif // _DEBUG

#ifdef SVG_SUPPORT_TEXTSELECTION
	const uni_char* orig_text = text;
#endif // SVG_SUPPORT_TEXTSELECTION
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
	SVGNumberPair bsl_adj_ctp = m_position;
	if (m_attrs->baseline_shift.NotEqual(0))
		bsl_adj_ctp.y -= m_attrs->baseline_shift;

	INT32 pixel_extra_spacing = (m_attrs->letter_spacing / m_fontdesc.GetFontToUserScale()).GetIntegerValue();

	// Since we require ctm[0] == ctm[3] to take this codepath, we can
	// use one or the other here.
	SVGNumber recip_axis_scale = SVGNumber(1) / m_painter->GetTransform()[0];
	// Userspace advance calculation
	SVGNumber user_width = recip_axis_scale * opfont->StringWidth(text, len, pixel_extra_spacing);

	// "All text decorations except line-through should be
	// drawn before the text is filled and stroked; thus,
	// the text is rendered on top of these decorations."
	if (m_attrs->decorations & (TextAttributes::DECO_UNDERLINE | TextAttributes::DECO_OVERLINE))
		DrawTextDecorations(user_width);

	int* caret_x = NULL;
#ifdef SVG_SUPPORT_EDITABLE
	int caret_x_actual;

	if (m_sel_attrs.DrawCaret((int)len) && m_sel_attrs.has_painted_caret == 0)
	{
		int caret_ofs = m_sel_attrs.editable->m_caret.m_point.ofs;
		if (caret_ofs == m_sel_attrs.current_char_idx)
		{
			caret_x_actual = 0;
		}
		else
		{
			int ofs = caret_ofs - m_sel_attrs.current_char_idx;
			ofs = MIN((int)len, ofs);
			caret_x_actual = opfont->StringWidth(orig_text, ofs) - 1;
		}

		m_sel_attrs.editable->m_caret.m_pos.x = bsl_adj_ctp.x;
		m_sel_attrs.editable->m_caret.m_pos.x += recip_axis_scale * caret_x_actual;
		m_sel_attrs.editable->m_caret.m_pos.y = bsl_adj_ctp.y;
		m_sel_attrs.editable->m_caret.m_pos.y += recip_axis_scale * opfont->Descent();
		m_sel_attrs.editable->m_caret.m_pos.height = m_fontsize;

		m_sel_attrs.has_painted_caret = 1;
		m_sel_attrs.editable->m_caret.m_line = /*tparams.area ? tparams.area->current_line :*/ 0;
		if (m_sel_attrs.editable->m_caret.m_on)
			caret_x = &caret_x_actual;
	}
#endif // SVG_SUPPORT_EDITABLE

	// Reset painter position
	SVGNumberPair pixel_pos = m_painter->GetTransform().ApplyToCoordinate(bsl_adj_ctp);

	// Note that y position is negated because OpPainter works
	// from top-left corner, not lower-left
	OpPoint int_pos(pixel_pos.x.GetIntegerValue(),
					pixel_pos.y.GetIntegerValue() - opfont->Ascent());

	m_painter->DrawStringPainter(opfont, int_pos, text, len, pixel_extra_spacing, caret_x);

	// Draw line-through decoration if applicable
	if (m_attrs->decorations & TextAttributes::DECO_LINETHROUGH)
		DrawStrikeThrough(m_position.x, user_width);

#ifdef SVG_SUPPORT_TEXTSELECTION
	RenderSystemTextRunWithHighlight(int_pos, text, orig_text, len, reverse_word, pixel_extra_spacing);
#endif // SVG_SUPPORT_TEXTSELECTION

	m_position.x += user_width;

	return OpStatus::OK;
}

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
void SVGTextPainter::CalculateGlyphAdjustment(const TextRunInfo& tri, SVGMatrix& ustrans, GlyphOutline& go)
{
	SVGNumber rotate_val;
	if (CurrentGlyphHasRotation())
		rotate_val = m_rotation_list->angles[m_glyph_idx];

	if (go.rotation > 0)
		rotate_val += go.rotation;

	if (rotate_val.NotEqual(0))
	{
		SVGMatrix rottrans;
		rottrans.LoadRotation(rotate_val);

		ustrans.Multiply(rottrans);
	}

	// The startpoint translation will only be non-zero for vertical text
	OP_ASSERT(IsVertical() || tri.startpoint_trans.x.Equal(0) && tri.startpoint_trans.y.Equal(0));

	if (IsVertical())
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
void SVGTextPainter::CalculateGlyphPositionOnPath(const TextRunInfo& tri, SVGMatrix& ustrans, GlyphOutline& go)
{
	SVGNumber half_advance = go.advance / 2;
	SVGNumber path_pos = m_path_distance + half_advance;

	// Don't draw glyphs off either end of the path
	if (path_pos < 0 || path_pos > m_path->GetLength())
	{
		go.visible = FALSE;
	}
	else
	{
		if (m_path_warp)
		{
			if (go.outline)
			{
				// Warp the glyph
				OpBpath* warped = NULL;
				if (OpStatus::IsSuccess(m_path->Warp(go.outline, &warped, path_pos,
													 tri.font_to_user_scale, half_advance)))
					go.SetOutline(warped);
			}
			if (go.has_cell && OpStatus::IsSuccess(go.CreateCellPath()))
			{
				// Warp the glyph (cell)
				OpBpath* warped = NULL;
				if (OpStatus::IsSuccess(m_path->Warp(go.outline_cell, &warped, path_pos,
													 tri.font_to_user_scale, half_advance)))
				{
					go.SetOutlineCell(warped);
					go.cell = go.outline_cell->GetBoundingBox().GetSVGRect();
				}
			}
		}

		// pathtrans = pathT * T(-half_advance, 0) * T(m_path_displacement)
		SVGMatrix pathtrans;

		m_path->CalculateTransformAtDistance(path_pos, pathtrans);
		pathtrans.PostMultTranslation(-half_advance, m_path_displacement);

		ustrans.Multiply(pathtrans);
	}
}
#endif // SVG_FULL_11

OP_STATUS SVGTextPainter::DrawOutline(const TextRunInfo& tri, const GlyphOutline& go,
									  SVGMatrix& ustrans)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGPaintDesc selected_fill;
	SVGObjectProperties selected_props;
#endif // SVG_SUPPORT_TEXTSELECTION

	OP_STATUS result = OpStatus::OK;
	if (go.has_cell)
	{
		SVGMatrix saved_usxfrm = m_painter->GetTransform();

		if (IsVertical())
		{
			// Vertical alignment
			SVGMatrix ctm = saved_usxfrm;
			ctm.PostMultTranslation(-go.cell.width / 2, 0);
			m_painter->SetTransform(ctm);
		}
#ifdef SVG_SUPPORT_TEXTSELECTION
		// Draw the glyph cell
		if (go.selected)
		{
			UINT32 selectioncolor = HTM_Lex::GetColValByIndex(m_sel_attrs.GetSelectionColor());
			selectioncolor = 0xFF000000 | (GetBValue(selectioncolor) << 16) | (GetGValue(selectioncolor) << 8) | GetRValue(selectioncolor);
			selected_fill.color = selectioncolor;
			selected_fill.opacity = 255;
			selected_fill.pserver = NULL;

			selected_props.fillrule = SVGFILL_NON_ZERO;
			selected_props.filled = TRUE;
			selected_props.stroked = FALSE;
			selected_props.aa_quality = m_obj_props.aa_quality;

			m_painter->SetObjectProperties(&selected_props);
			m_painter->SetFillPaint(selected_fill);
		}
#endif // SVG_SUPPORT_TEXTSELECTION
		if (go.visible)
		{
			if (go.outline_cell)
				result = m_painter->DrawPath(go.outline_cell);
			else
				result = m_painter->DrawRect(go.cell.x, go.cell.y,
													go.cell.width, go.cell.height, 0, 0);
		}
#ifdef SVG_SUPPORT_TEXTSELECTION
		if (go.selected)
		{
			m_painter->SetObjectProperties(&m_obj_props);
			m_painter->SetFillPaint(m_paints[0]);
		}
#endif // SVG_SUPPORT_TEXTSELECTION

		m_painter->SetTransform(saved_usxfrm);
		RETURN_IF_ERROR(result);
	}

	// Draw the glyph
	if (go.visible && go.outline)
	{
#ifdef SVG_SUPPORT_TEXTSELECTION
		if (go.selected)
		{
			UINT32 selectioncolor = HTM_Lex::GetColValByIndex(m_sel_attrs.GetTextColor());
			selectioncolor = 0xFF000000 | (GetBValue(selectioncolor) << 16) | (GetGValue(selectioncolor) << 8) | GetRValue(selectioncolor);
			selected_fill.color = selectioncolor;
			selected_fill.opacity = 255;
			selected_fill.pserver = NULL;

			selected_props.fillrule = SVGFILL_NON_ZERO;
			selected_props.filled = TRUE;
			selected_props.stroked = FALSE;
			selected_props.aa_quality = m_obj_props.aa_quality;

			m_painter->SetObjectProperties(&selected_props);
			m_painter->SetFillPaint(selected_fill);
		}
#endif // SVG_SUPPORT_TEXTSELECTION

		if (IsVertical())
		{
			SVGBoundingBox glyph_bbox = go.outline->GetBoundingBox();

			// Vertical alignment
			SVGNumber glyph_width = glyph_bbox.maxx - glyph_bbox.minx;
			SVGMatrix ctm = m_painter->GetTransform();
			ctm.PostMultTranslation(-glyph_width/2, 0);
			m_painter->SetTransform(ctm);

			ustrans.PostMultTranslation(-glyph_width/2, 0);
		}

		result = m_painter->DrawPath(go.outline);
#ifdef SVG_SUPPORT_TEXTSELECTION
		if (go.selected)
		{
			m_painter->SetObjectProperties(&m_obj_props);
			m_painter->SetFillPaint(m_paints[0]);
		}
#endif // SVG_SUPPORT_TEXTSELECTION
	}
	return result;
}

void SVGTextPainter::TextRunSetup(TextRunInfo& tri, SVGMatrix& glyph_adj_xfrm)
{
	// Text scale factor (glyph flipping/glyph scaling/font->user adjustment)
	SVGNumberPair text_scale(1, -1);

	if (m_attrs->glyph_scale.NotEqual(1))
	{
		if (IsVertical())
		{
			// Vertical
			text_scale.y *= m_attrs->glyph_scale;
		}
		else
		{
			// Horizontal
			text_scale.x *= m_attrs->glyph_scale;
		}
	}

	tri.font_to_user_and_glyph_scale = m_attrs->glyph_scale;
	tri.font_to_user_scale = m_fontdesc.GetFontToUserScale();
	if (tri.font_to_user_scale.NotEqual(1))
	{
		// The glyph paths are scaled. Here we undo that scale
		text_scale = text_scale.Scaled(tri.font_to_user_scale);

		// Accumulate into the advance scaling factor
		tri.font_to_user_and_glyph_scale *= tri.font_to_user_scale;
	}

	// scale the linewidth, and don't divide by zero
	if (tri.font_to_user_scale > 0)
		m_stroke_props.width = m_stroke_props.width / tri.font_to_user_scale;

	if (IsVertical())
	{
		// Vertical
		tri.startpoint_trans.y = tri.font_to_user_scale * m_fontdesc.GetFont()->Ascent();
	}

	// Setup transform for glyph adjustment (scaling, baseline-shift)
	glyph_adj_xfrm.LoadScale(text_scale.x, text_scale.y);
	if (m_attrs->baseline_shift.NotEqual(0))
	{
		switch (m_attrs->writing_mode)
		{
		default:
		case SVGWRITINGMODE_LR:   /* ( 1, 0 ) */
		case SVGWRITINGMODE_LR_TB:
			glyph_adj_xfrm.MultTranslation(0, -m_attrs->baseline_shift);
			break;
		case SVGWRITINGMODE_RL:   /* (-1, 0 ) */
		case SVGWRITINGMODE_RL_TB:
			glyph_adj_xfrm.MultTranslation(0, m_attrs->baseline_shift);
			break;
		case SVGWRITINGMODE_TB:   /* ( 0, 1 ) */
		case SVGWRITINGMODE_TB_RL:
			glyph_adj_xfrm.MultTranslation(m_attrs->baseline_shift, 0);
			break;
		}
	}
}

#ifdef SVG_SUPPORT_EDITABLE
void SVGTextPainter::SetupAndDrawCaret(OpFont* font,
							  SVGNumber advance, SVGNumber fontsize,
							  SVGNumber font_to_user_scale)
{
	// Caret in glyph coordinates
	SVGRect glyph_caret(0,
						-SVGNumber(font->Descent()),
						0 /* determined by drawing code */,
						fontsize / font_to_user_scale);
	// Caret in userspace
	SVGRect us_caret(m_position.x,
					 m_position.y + font_to_user_scale * font->Descent(),
					 0,
					 fontsize);

	if (m_sel_attrs.DrawCaretOnRightSide())
	{
		glyph_caret.x = advance / font_to_user_scale;
		us_caret.x += advance; // FIXME: Letter-spacing?
	}

	m_sel_attrs.editable->m_caret.m_pos = us_caret;
	m_sel_attrs.editable->m_caret.m_glyph_pos = glyph_caret;

	if(m_sel_attrs.editable->m_caret.m_on)
	{
		SVGPaintDesc caret_stroke;
		SVGObjectProperties caret_props;
		SVGStrokeProperties caret_stroke_props;

		caret_stroke_props.non_scaling = TRUE;
		caret_stroke_props.width = 1;

		caret_stroke.color = 0xFF000000;
		caret_stroke.opacity = 255;
		caret_stroke.pserver = NULL;

		caret_props.fillrule = SVGFILL_NON_ZERO;
		caret_props.filled = FALSE;
		caret_props.stroked = TRUE;
		caret_props.aa_quality = m_obj_props.aa_quality;

		m_painter->SetObjectProperties(&caret_props);
		m_painter->SetStrokeProperties(&caret_stroke_props);
		m_painter->SetStrokePaint(caret_stroke);

		m_painter->DrawLine(glyph_caret.x, glyph_caret.y,
							glyph_caret.x, glyph_caret.y+glyph_caret.height);

		m_painter->SetObjectProperties(&m_obj_props);
		m_painter->SetStrokeProperties(&m_stroke_props);
		m_painter->SetStrokePaint(m_paints[1]);
	}

	m_sel_attrs.has_painted_caret = 1;

	m_sel_attrs.editable->m_caret.m_line = /*tparams.area ? tparams.area->current_line :*/ 0;
}
#endif // SVG_SUPPORT_EDITABLE

OP_STATUS SVGTextPainter::RenderGlyphRun(GlyphRun& grun, BOOL reverse_word)
{
	SVGNumber decoration_width, decoration_x;
	if (m_attrs->decorations != 0)
	{
		unsigned int glyph_count = grun.GetGlyphCount();
		decoration_width = grun.GetExtent(0, glyph_count, m_attrs->letter_spacing,
										  m_fontdesc.GetFontToUserScale());

		decoration_width *= m_attrs->glyph_scale;
		decoration_width += m_attrs->extra_spacing * (glyph_count - 1);

		decoration_x = m_position.x; // Used for line-through
	}

	// "All text decorations except line-through should be
	// drawn before the text is filled and stroked; thus,
	// the text is rendered on top of these decorations."
	if (m_attrs->decorations & (TextAttributes::DECO_UNDERLINE | TextAttributes::DECO_OVERLINE))
	{
		// Hmm, underline on vertical text, what does that look like? This is likely to be a long
		// line extending horizontally from the first letter.
		DrawTextDecorations(decoration_width);
	}

	SVGMatrix mtx;
	RETURN_IF_ERROR(m_painter->PushTransform(mtx));

	SVGMatrix glyph_adj_xfrm;
	TextRunInfo tri;
	TextRunSetup(tri, glyph_adj_xfrm);

	SVGMatrix saved_transform = m_painter->GetTransform();

	GlyphOutline go;
	go.Clean();

	//const SVGVectorStack& rotatelist = tparams.rotatelist;
	//const SVGNumberObject *glob_rotate = static_cast<const SVGNumberObject*>(rotatelist.Get(tparams.total_char_sum + tparams.current_char_idx, TRUE));

	OP_STATUS result = OpStatus::OK;

#ifdef SVG_SUPPORT_TEXTSELECTION
	OpFont* vectorfont = m_fontdesc.GetFont();
#endif // SVG_SUPPORT_TEXTSELECTION

	unsigned int glyph_count = grun.GetGlyphCount();
	for (unsigned int i = 0; i < glyph_count; i++)
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
#ifdef SVG_SUPPORT_TEXTSELECTION
		if (reverse_word)
			m_sel_attrs.current_char_idx -= glyph_info->GetNumChars();

		if (m_sel_attrs.SelectionActive())
		{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
			if (m_sel_attrs.current_char_idx >= m_sel_attrs.selected_max_idx)
				m_sel_attrs.GetNextSelectionForFragment(); //GetNextSelection();
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
			go.selected = (m_sel_attrs.current_char_idx >= m_sel_attrs.selected_min_idx &&
						   m_sel_attrs.current_char_idx < m_sel_attrs.selected_max_idx);
		}
		else
		{
			go.selected = FALSE;
		}

		go.logical_start = m_sel_attrs.current_char_idx;
		go.logical_end = m_sel_attrs.current_char_idx;

		{
			go.logical_end += glyph_info->GetNumChars();
			if (go.logical_end < 0)
				go.logical_end = 0;
		}

		if (go.selected)
		{
			// For text we intersect with a letter if we are in that letter's "cell".
			// We emulate the cell by making a filled rect for the
			// whole glyph's area

			// Making a "fake" glyph so it will have to be in glyph coordinates
			SVGNumber font_height = m_fontsize / tri.font_to_user_scale;
			go.cell.x = 0;
			go.cell.y = -SVGNumber(vectorfont->Descent());
			go.cell.width = go.advance / tri.font_to_user_scale; // Too simple?
			go.cell.height = font_height;

			if (go.outline)
			{
				SVGBoundingBox bbox = go.outline->GetBoundingBox();
				bbox.UnionWith(go.cell);
				go.cell = bbox.GetSVGRect();
			}

			go.has_cell = TRUE;
		}
#endif // SVG_SUPPORT_TEXTSELECTION
		SVGMatrix ustrans(glyph_adj_xfrm);

		go.visible = TRUE;
		if (go.rotation > 0 || CurrentGlyphHasRotation() || IsVertical())
			CalculateGlyphAdjustment(tri, ustrans, go);

#ifdef SVG_FULL_11
		if (m_path)
		{
			CalculateGlyphPositionOnPath(tri, ustrans, go);
		}
		else
#endif // SVG_FULL_11
		{
			ustrans.MultTranslation(m_position.x, m_position.y);
		}

		SVGMatrix glyph_ctm = saved_transform;
		glyph_ctm.PostMultiply(ustrans);

		m_painter->SetTransform(glyph_ctm);

		result = DrawOutline(tri, go, ustrans);

#ifdef SVG_SUPPORT_EDITABLE
		if (m_sel_attrs.has_painted_caret == 0 && m_sel_attrs.DrawCaret())
		{
			SetupAndDrawCaret(vectorfont,
							  go.advance, m_fontsize, tri.font_to_user_scale);
		}
#endif // SVG_SUPPORT_EDITABLE

		if (OpStatus::IsError(result))
			break;

		// Advance to next glyph position
		Advance(go.advance + m_attrs->letter_spacing + m_attrs->extra_spacing);

		m_glyph_idx++;

#ifdef SVG_SUPPORT_TEXTSELECTION
		if (!reverse_word)
			m_sel_attrs.current_char_idx += glyph_info->GetNumChars();
#endif // SVG_SUPPORT_TEXTSELECTION
		go.Clean();
	}

	m_painter->PopTransform();

	if (m_attrs->decorations & TextAttributes::DECO_LINETHROUGH)
		DrawStrikeThrough(decoration_x, decoration_width);

	return result;
}

OP_STATUS SVGTextPainter::RenderAlternateGlyphs(OpVector<GlyphInfo>& glyphs)
{
	GlyphRun grun(glyphs.GetCount());
	RETURN_IF_ERROR(grun.Append(glyphs));

	return RenderGlyphRun(grun, FALSE);
}

#endif // SVG_SUPPORT
