/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#include "modules/svg/src/svgpch.h"

#if defined(SVG_SUPPORT)

#include "modules/svg/src/SVGTextTraverse.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/svg.h" // SVG_Lex

#include "modules/svg/src/svgpaintnode.h"
#include "modules/svg/src/svgpaintserver.h"

#include "modules/layout/cascade.h"
#include "modules/layout/layoutprops.h"

#include "modules/display/color.h" // for CheckColorContrast
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

#include "modules/doc/searchinfo.h"

SVGTextArguments::SVGTextArguments()
: ctp(0,0)
, xlist()
, ylist()
, dxlist()
, dylist()
, rotatelist()
, baseline_shift(0)
, writing_mode(SVGWRITINGMODE_LR_TB)
, force_chunk(FALSE)
, pending_run(FALSE)
, last_run(NULL)
, rotation_count(0)
, current_glyph_idx(0)
, current_char_idx(0)
, total_char_sum(0)
, selected_min_idx(-1)
, selected_max_idx(-1)
, path(NULL)
, area(NULL)
, bbox()
, chunk_list(NULL)
, current_chunk(0)
, total_extent(0)
, data(NULL)
, max_glyph_idx(-1)
, extra_spacing(0)
, glyph_scale(1)
#ifdef SVG_SUPPORT_EDITABLE
, editable(NULL)
#endif // SVG_SUPPORT_EDITABLE
, current_text_cache(NULL)
, is_layout(FALSE)
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
, first_selection_elm(NULL)
, current_sel_elm(NULL)
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
, doc_ctx(NULL)
, current_element(NULL)
, props(NULL)
, content_paint_node(NULL)
, glyph_rotation_keeper(NULL)
, text_root_container(NULL)
, packed_init(0)
{}

SVGTextArguments::~SVGTextArguments()
{
	OP_DELETE(path);
	path = NULL;
}

OP_STATUS SVGTextArguments::NewChunk(OpVector<SVGTextChunk>& chunk_list)
{
	SVGTextChunk* chunk = OP_NEW(SVGTextChunk, ());
	if (!chunk || OpStatus::IsMemoryError(chunk_list.Add(chunk)))
	{
		OP_DELETE(chunk);
		return OpStatus::ERR_NO_MEMORY;
	}
	chunk->initial_ctp = ctp;
	return OpStatus::OK;
}

void SVGTextArguments::AdvanceCTPExtent(SVGNumber adv)
{
	AdvanceCTP(adv);
	AddExtent(adv);
}

void SVGTextArguments::AdvanceCTP(SVGNumber adv)
{
	switch (writing_mode)
	{
	default:
	case SVGWRITINGMODE_LR:   /* ( 1, 0 ) */
	case SVGWRITINGMODE_LR_TB:
		ctp.x += adv;
		break;
	case SVGWRITINGMODE_RL:   /* (-1, 0 ) */
	case SVGWRITINGMODE_RL_TB:
		ctp.x += -adv;
		break;
	case SVGWRITINGMODE_TB:   /* ( 0, 1 ) */
	case SVGWRITINGMODE_TB_RL:
		ctp.y += adv;
		break;
	}

#ifdef SVG_FULL_11
	if (path)
		pathDistance += adv;
#endif // SVG_FULL_11
}

void SVGTextArguments::AdjustCTP()
{
	OP_ASSERT(chunk_list);

	if (!NeedsAlignment())
		return;

	if (SVGTextChunk* chunk = chunk_list->Get(current_chunk))
	{
		ctp = chunk->initial_ctp;

		/**
		 * FIXME: the extent traverser should ideally handle ellipsis, for more exact alignment.
		 * The below solution will lead to slightly incorrect results, but should be better than
		 * adjusting based on the unclipped extents. See CORE-44705.
		 */
		SVGNumber chunk_extent = packed.use_ellipsis ? SVGNumber::min_of(max_extent, chunk->extent) : chunk->extent;
		if (packed.anchor == SVGTEXTANCHOR_MIDDLE)
			chunk_extent /= 2;

		AdvanceCTP(-chunk_extent);
	}
}

void SVGTextArguments::AdvanceBlock()
{
	area->AdvanceBlock(ctp, linepos);

	if (is_layout)
		pending_run = TRUE;
}

void SVGTextArguments::AddExtent(SVGNumber extent)
{
	total_extent += extent;
	if (data && data->SetExtent() && data->chunk_list)
		if (SVGTextChunk* chunk = data->chunk_list->Get(current_chunk))
			chunk->extent += extent;
}

TextAttributes::~TextAttributes()
{
	SVGPaintServer::DecRef(deco_paints[0].pserver);
	SVGPaintServer::DecRef(deco_paints[1].pserver);

	OP_DELETEA(language);
}

void TextAttributes::SetLanguage(const uni_char* lang)
{
	OpStatus::Ignore(UniSetStr(language, lang));
}

TextPathAttributes::~TextPathAttributes()
{
	SVGObject::DecRef(path);
}

void SVGTextArguments::SetRotationKeeper(SVGTextRotate* rotation_keeper)
{
	if (!is_layout)
		return;

	glyph_rotation_keeper = rotation_keeper;
}

void SVGTextArguments::SetContentNode(SVGElementInfo& info)
{
	if (!is_layout)
		return;

	if (info.layouted->IsMatchingType(Markup::SVGE_TREF, NS_SVG))
	{
		SVGTextReferencePaintNode* tref_paint_node =
			static_cast<SVGTextReferencePaintNode*>(info.paint_node);

		content_paint_node = tref_paint_node->GetContentNode();
	}
	else
	{
		content_paint_node = static_cast<SVGTextContentPaintNode*>(info.paint_node);
	}

	glyph_rotation_keeper = content_paint_node;
}

TextPathAttributes* SVGTextArguments::GetCurrentTextPathAttributes()
{
	for (int i = text_attr_stack.GetCount() - 1; i >= 0; i--)
	{
		SVGTextAttributePaintNode* attr_paint_node = text_attr_stack.Get(i);
		if (TextPathAttributes* text_path_attr = attr_paint_node->GetTextPathAttributes())
			return text_path_attr;
	}
	return NULL;
}

void SVGTextArguments::UpdateContentNode(SVGDocumentContext* font_doc_ctx)
{
	if (!is_layout)
		return;

	content_paint_node->ResetRuns();
	content_paint_node->SetTextCache(current_text_cache);
	content_paint_node->SetFontDocument(font_doc_ctx);
	content_paint_node->SetTextSelectionDocument(doc_ctx);
	content_paint_node->SetTextSelectionElement(current_element);
#ifdef SVG_SUPPORT_EDITABLE
	content_paint_node->SetEditable(editable);
#endif // SVG_SUPPORT_EDITABLE

	int text_format = 0;
	switch (props->text_transform)
	{
	case TEXT_TRANSFORM_CAPITALIZE:
		text_format |= TEXT_FORMAT_CAPITALIZE;
		break;
	case TEXT_TRANSFORM_UPPERCASE:
		text_format |= TEXT_FORMAT_UPPERCASE;
		break;
	case TEXT_TRANSFORM_LOWERCASE:
		text_format |= TEXT_FORMAT_LOWERCASE;
		break;
	}

	if (props->small_caps == CSS_VALUE_small_caps)
		text_format |= TEXT_FORMAT_SMALL_CAPS;

	content_paint_node->SetTextFormat(text_format);

	SVGTextAttributePaintNode* attr_paint_node = text_attr_stack.Get(text_attr_stack.GetCount() - 1);
	content_paint_node->SetAttributes(attr_paint_node->GetAttributes());
	content_paint_node->SetTextPathAttributes(GetCurrentTextPathAttributes());
}

OP_STATUS SVGTextArguments::PushAttributeNode(SVGTextAttributePaintNode* attr_paint_node)
{
	pending_run = TRUE;

	return text_attr_stack.Add(attr_paint_node);
}

void SVGTextArguments::PopAttributeNode()
{
	unsigned stack_depth = text_attr_stack.GetCount();
	if (stack_depth == 0)
		return;

	text_attr_stack.Remove(stack_depth - 1);

	pending_run = TRUE;
}

void SVGTextArguments::SetRunBBox()
{
	if (is_layout)
		// FIXME: Not exactly correct, but should be close enough for now
		content_paint_node->UpdateRunBBox(bbox.Get());
}

void SVGTextArguments::DoEmitRun(BOOL has_ellipsis)
{
	TextRun* run = OP_NEW(TextRun, ());
	if (!run)
		return;

	run->position = current_run.position;
	run->start = current_run.start.index;
	run->length = current_run.end.index - current_run.start.index + 1;

	// If at the absolute start of the ending fragment, we don't need
	// to include it.
	if (current_run.end.offset == 0)
		run->length--;

	OP_ASSERT(run->length > 0);

	run->start_offset = current_run.start.offset;
	run->end_offset = current_run.end.offset;

	run->has_ellipsis = !!has_ellipsis;

	content_paint_node->AppendRun(run);

	last_run = run;
}

void SVGTextArguments::EmitEllipsis(unsigned end_offset)
{
	if (!is_layout || packed.clip_runs)
		return;

	current_run.end = frag;
	current_run.end.offset += end_offset;

	if (current_run.start.index == current_run.end.index &&
		current_run.start.offset == current_run.end.offset)
		return;

	DoEmitRun(TRUE);

	current_run.start = current_run.end;

	AddPendingRun();

	packed.clip_runs = 1;
}

void SVGTextArguments::UpdateTextAreaEllipsis(unsigned end_offset, BOOL ws_only)
{
	OP_ASSERT(packed.use_ellipsis);
	OP_ASSERT(area);

	area->ellipsis_end_offset = end_offset;
	area->info.has_ws_ellipsis = !!ws_only;
	area->info.has_ellipsis = TRUE;
}

void SVGTextArguments::FlushTextAreaEllipsis()
{
	OP_ASSERT(packed.use_ellipsis);
	OP_ASSERT(area);

	if (!area->info.has_ellipsis)
		return;

	area->info.has_ellipsis = FALSE;

	OP_ASSERT(last_run);

	last_run->has_ellipsis = TRUE;

	if (area->info.has_ws_ellipsis)
	{
		// Trim the entire fragment
		OP_ASSERT(last_run->length > 0);
		last_run->length--;
	}
	else if (area->ellipsis_end_offset)
	{
		last_run->end_offset = area->ellipsis_end_offset;
	}
}

void SVGTextArguments::EmitRunStart()
{
	if (!is_layout || packed.clip_runs)
		return;

	// Set start attributes (CTP/path-pos, char.index, fragment)
#ifdef SVG_FULL_11
	if (path)
	{
		current_run.position.x = pathDistance;
		current_run.position.y = pathDisplacement;
	}
	else
#endif // SVG_FULL_11
	{
		current_run.position = ctp;
	}

	current_run.start = frag;

	pending_run = FALSE;
}

void SVGTextArguments::EmitRunEnd()
{
	if (!is_layout || packed.clip_runs || pending_run)
		return;

	current_run.end = frag;

	if (current_run.start.index == current_run.end.index &&
		current_run.start.offset == current_run.end.offset)
		return;

	DoEmitRun(FALSE);

	current_run.start = current_run.end;
}

void SVGTextArguments::EmitRotation(SVGNumber angle)
{
    if (!is_layout || !glyph_rotation_keeper)
		return;

	if (rotation_list.length == 0)
	{
		OP_ASSERT(rotation_count == 0 && rotation_list.angles == NULL);
		OP_ASSERT(glyph_rotation_keeper);

		unsigned estimated_size = glyph_rotation_keeper->GetEstimatedSize();

		GlyphRotationList* pn_rot_list = glyph_rotation_keeper->GetRotationList();
		if (pn_rot_list && pn_rot_list->length == estimated_size)
		{
			OP_ASSERT(pn_rot_list->angles);

			// Re-use the current list from the paint node
			rotation_list.angles = pn_rot_list->angles;
			pn_rot_list->angles = NULL;
		}
		else
		{
			rotation_list.angles = OP_NEWA(SVGNumber, estimated_size);
			if (!rotation_list.angles)
				return;
		}

		rotation_list.length = estimated_size;
	}

	rotation_list.angles[rotation_count++] = angle;
}

void SVGTextArguments::CommitRotations()
{
	if (!is_layout)
		return;

	GlyphRotationList* rot_list = NULL;
	if (rotation_count > 0)
	{
		OP_ASSERT(rotation_list.angles);
		OP_ASSERT(rotation_list.length >= rotation_count);

		rot_list = OP_NEW(GlyphRotationList, ());
		if (rot_list)
		{
			rot_list->angles = OP_NEWA(SVGNumber, rotation_count);
			if (rot_list->angles)
			{
				rot_list->length = rotation_count;

				op_memcpy(rot_list->angles, rotation_list.angles,
						  sizeof(*rotation_list.angles) * rotation_count);
			}
			else
			{
				OP_DELETE(rot_list);
				rot_list = NULL;
			}
		}

		rotation_count = 0;

		OP_DELETEA(rotation_list.angles);
		rotation_list.angles = NULL;
		rotation_list.length = 0;
	}

	glyph_rotation_keeper->SetRotationList(rot_list);
}

#define SPANS_MULTIPLE

#ifdef SVG_SUPPORT_TEXTSELECTION
BOOL SVGTextArguments::GetNextSelection(SVGDocumentContext* doc_ctx,
										HTML_Element* layouted_elm, const HTMLayoutProperties& props)
{
	OP_NEW_DBG("GetNextSelection", "svg_selections");

	// Clear selection
	selected_min_idx = -1;
	selected_max_idx = 0;
	packed.selection_active = 0;

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

BOOL SVGTextArguments::GetNextSelectionForFragment()
{
	// For last line, calling GetNextSelection will ruin active
	// selection - cannot let that happen
	if (packed.selection_active)
	{
		int old_selected_min = selected_min_idx;
		int old_selected_max = selected_max_idx;

		BOOL has_more_ranges = GetNextSelection(doc_ctx, current_element, *props);

		if (!packed.selection_active)
		{
			packed.selection_active = TRUE;
			selected_min_idx = old_selected_min;
			selected_max_idx = old_selected_max;
		}

		return has_more_ranges;
	}
	else
	{
		return GetNextSelection(doc_ctx, current_element, *props);
	}
}

int SVGTextArguments::SelectionPointToOffset(const SelectionBoundaryPoint* selpoint, int leading_ws_comp)
{
	int offset = selpoint->GetElementCharacterOffset();
	offset -= leading_ws_comp;

	if (offset < 0)
		offset = 0;

	return offset;
}

void SVGTextArguments::SetupSelectionFromCurrentHighlight(HTML_Element* layouted_elm)
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

	SelectionBoundaryPoint* highlight_end = &sel.GetLogicalEndPoint();
	SelectionBoundaryPoint* highlight_start = &sel.GetLogicalStartPoint();
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

	BOOL starts_in_element = highlight_start->GetElement() == layouted_elm;
	BOOL ends_in_element = highlight_end->GetElement() == layouted_elm;

	// Selections originating from outside SVG (or, not determined by
	// the SVG module's traversal), will not have been accounted for
	// any potentially collapsed leading whitespace.
	// Calculate the leading whitespace of the current element if it
	// is one of the end-points of the selection.
	int leading_ws = 0;
	if (packed.svg_selection == 0 && (starts_in_element || ends_in_element))
		leading_ws = SVGEditable::CalculateLeadingWhitespace(layouted_elm);

	selected_min_idx = INT_MAX;

	if (starts_in_element)
	{
		selected_min_idx = SelectionPointToOffset(highlight_start, leading_ws);
	}
	else if (highlight_start->GetElement()->Precedes(layouted_elm))
	{
		selected_min_idx = 0;
	}

	selected_max_idx = 0;

	if (ends_in_element)
	{
		selected_max_idx = SelectionPointToOffset(highlight_end, leading_ws);
	}
	else if (!highlight_end->GetElement()->Precedes(layouted_elm))
	{
		selected_max_idx = layouted_elm->GetTextContentLength();
	}

	if (selected_max_idx > selected_min_idx)
	{
		packed.selection_active = 1;
	}
	else
	{
		selected_min_idx = -1;
		selected_max_idx = 0;

		packed.selection_active = 0;
	}
}

void SVGTextArguments::SetupTextSelection(SVGDocumentContext* doc_ctx, HTML_Element* layouted_elm)
{
	SVGTextSelection& sel = doc_ctx->GetTextSelection();
	BOOL has_selection = sel.IsValid() && (sel.IsSelecting() || !sel.IsEmpty());
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
	SelectionElm* sel_elm = NULL;
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

	if (!has_selection)
	{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		if (layouted_elm->IsInSelection())
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

		packed.svg_selection = 1;
	}

#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
	first_selection_elm = sel_elm;
	current_sel_elm = NULL; // To make sure Setup...Fragment is called as it should
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
}

void SVGTextArguments::SetupTextSelectionForFragment()
{
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
	// Restart with the first SelectionElm
	current_sel_elm = first_selection_elm;

	BOOL has_more_selections;
	do
	{
		has_more_selections = GetNextSelection(doc_ctx, current_element, *props);
	} while (packed.selection_active == 0 && has_more_selections);
#else
	GetNextSelection(doc_ctx, current_element, *props); // Setup the first (and only) selection
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
}
#endif // SVG_SUPPORT_TEXTSELECTION

OP_STATUS SVGTextArguments::AddSelectionRect(const OpRect& rect)
{
	OP_ASSERT(data);
	OP_ASSERT(data->selection_rect_list);

	SVGRect svg_rect(SVGNumber(rect.x), SVGNumber(rect.y), SVGNumber(rect.width), SVGNumber(rect.height));
	return AddSelectionRect(svg_rect);
}

OP_STATUS SVGTextArguments::AddSelectionRect(const SVGRect& rect)
{
	OP_ASSERT(data);
	OP_ASSERT(data->selection_rect_list);

	SVGRect* svg_rect = OP_NEW(SVGRect, (rect.x, rect.y, rect.width, rect.height));
	RETURN_OOM_IF_NULL(svg_rect);
	OpAutoPtr<SVGRect> ap_svg_rect(svg_rect);
	RETURN_IF_ERROR(data->selection_rect_list->Add(ap_svg_rect.get()));
	ap_svg_rect.release();

	return OpStatus::OK;
}

const SVGObject* SVGVectorStack::Get(UINT32 glyph_index, BOOL is_rotate /* = FALSE */) const
{
	OP_ASSERT(m_offsets.GetCount() == m_internal_vector.GetCount());
// This define enables interpretating the rotate attribute according to the SVG 1.1 spec
// Changed to use 1.2T way (2009-01-14), see http://dev.w3.org/SVG/profiles/1.1F2/errata/errata.xml#propogation-of-rotation-text
//#define SVG_ROTATE_ATTR_ACCORDING_TO_SVG11
	SVGObject *result = NULL;
	SVGObject *last = NULL;
	UINT32 lastidx = 0;
	for(UINT32 i = m_internal_vector.GetCount(); i > 0; i--)
	{
		SVGVector* v = m_internal_vector.Get(i-1);
		if(v)
		{
			UINT32 offset = m_offsets.Get(i-1);
			if(glyph_index < offset + v->GetCount())
			{
				result = v->Get(glyph_index - offset);
				break;
			}

			if(is_rotate)
			{
				if (v->GetCount() > 0 &&
					lastidx < offset + v->GetCount())
				{
					lastidx = offset + v->GetCount();
#ifdef SVG_ROTATE_ATTR_ACCORDING_TO_SVG11
					if(v->GetCount() == 1)
#endif // SVG_ROTATE_ATTR_ACCORDING_TO_SVG11
						last = v->Get(v->GetCount() - 1);
				}
			}
		}
	}

	if(!result && last && is_rotate)
	{
		result = last;
	}

	return result;
}

OP_STATUS SVGVectorStack::Push(SVGVector* vec, UINT32 offset)
{
#ifdef _DEBUG
	OP_NEW_DBG("SVGVectorStack::Push", "svg_vectorstack");
	if (vec && vec->GetCount())
	{
		SVGLengthObject* len = static_cast<SVGLengthObject*>(vec->Get(0));
		OP_DBG(("Pushed %d new values (first: %g) beginning at offset %d.",
				vec->GetCount(),
				len->GetScalar().GetFloatValue(),
				offset));
	}
	else
	{
		OP_DBG(("Pushed null at offset %d.", offset));
	}
#endif // _DEBUG

	OP_ASSERT((void*)vec != (void*)0xcccccccc);
	OP_ASSERT(m_offsets.GetCount() == m_internal_vector.GetCount());
	OP_STATUS err = m_offsets.Add(offset);
	RETURN_IF_ERROR(err);
	err = m_internal_vector.Add(vec);
	if(OpStatus::IsError(err))
	{
		m_offsets.Remove(m_offsets.GetCount()-1);
	}
	OP_ASSERT(m_offsets.GetCount() == m_internal_vector.GetCount());
	return err;
}

OP_STATUS SVGVectorStack::Pop()
{
	OP_ASSERT(m_offsets.GetCount() == m_internal_vector.GetCount());
	OP_ASSERT(m_offsets.GetCount() > 0);
	m_offsets.Remove(m_offsets.GetCount()-1);
	m_internal_vector.Remove(m_internal_vector.GetCount()-1);
	OP_ASSERT(m_offsets.GetCount() == m_internal_vector.GetCount());
	return OpStatus::OK;
}

#endif // SVG_SUPPORT
