/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

# ifdef SVG_SUPPORT_TEXTSELECTION

//#define SVG_DEBUG_TEXTSELECTION

#include "modules/svg/src/SVGTextSelection.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGImageImpl.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGEditable.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/doc/html_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

SVGTextSelection::SVGTextSelection()
	: m_text_element(NULL),
	textselection_info_init(0)
{
}

void
SVGTextSelection::HandleRemovedSubtree(HTML_Element* subroot)
{
	if (subroot->IsAncestorOf(GetElement()) ||
		subroot->IsAncestorOf(m_start.GetElement()) ||
		subroot->IsAncestorOf(m_end.GetElement()))
	{
		ClearSelection(FALSE);
	}
}

SVGTextRootContainer*
SVGTextSelection::GetTextRootContainer() const
{
	SVGElementContext *ctx = AttrValueStore::GetSVGElementContext(GetElement());
	return ctx ? ctx->GetAsTextRootContainer() : NULL;
}

OpRect
SVGTextSelection::GetSelectionScreenRect()
{
	OP_NEW_DBG("GetSelectionScreenRect", "svg_textselection_invalidation");
	OpRect res;
	if (m_text_element && !IsEmpty())
	{
		SVGTextRootContainer* text_root_cont = GetTextRootContainer();
		res.UnionWith(text_root_cont->GetScreenExtents());
	}

	return res;
}

void
SVGTextSelection::AddSelectionToInvalidRect()
{
	m_invalid_screenrect.UnionWith(GetSelectionScreenRect());
}

void
SVGTextSelection::Update(BOOL invalidate_element /* = FALSE */)
{
	if(GetElement())
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(GetElement());
		if(!doc_ctx)
		{
			return;
		}

		// Force an immediate (as fast as possible) repaint of the element
		SVGDynamicChangeHandler::HandleAttributeChange(doc_ctx, GetElement(), Markup::SVGA_FILL, NS_SVG, FALSE);
	}
}

void
SVGTextSelection::ClearSelection(BOOL invalidate /* = TRUE */)
{
	if (invalidate)
	  AddSelectionToInvalidRect();

	m_start.Reset();
	m_end.Reset();
	m_cursor.Reset();

	textselection_info_init = 0;

	if (invalidate)
	  Update();

	m_text_element = NULL;
}

void
SVGTextSelection::SetCurrentIndex(HTML_Element* last_intersected,
								  int logical_start_char_index,
								  int logical_end_char_index,
								  BOOL hit_end_half)
{
	OP_NEW_DBG("SetCurrentIndex", "svg_textselection_invalidation");
#ifdef SVG_DEBUG_TEXTSELECTION
	if((hit_end_half && (logical_end_char_index != m_last_index)) ||
		(!hit_end_half && (logical_start_char_index != m_last_index)))
	{
		OP_DBG(("%d", hit_end_half ? logical_end_char_index : logical_start_char_index));
	}
#endif // SVG_DEBUG_TEXTSELECTION
	OP_DBG(("intersected %p [%d] index: %d - %d", last_intersected, last_intersected->Type(), logical_start_char_index, logical_end_char_index));

	int index = hit_end_half ? logical_end_char_index : logical_start_char_index;
	if(index >= 0 && last_intersected && (last_intersected->IsText() ||
										  last_intersected->IsMatchingType(Markup::SVGE_TREF, NS_SVG) ||
										  last_intersected->IsMatchingType(Markup::SVGE_ALTGLYPH, NS_SVG)))
	{
		m_cursor.SetLogicalPosition(last_intersected, index);
	}
}

void
SVGTextSelection::SetMousePosition(const OpPoint& screenpos)
{
	OP_NEW_DBG("SetMousePosition", "svg_textselection");
	if(IsSelecting() && !(m_end == m_cursor))
	{
		if(!MouseWasMoved() && MouseHasMoved(m_start_screen_mousepos, screenpos))
		{
			SetMouseWasMoved(TRUE);
		}

		if(MouseWasMoved())
		{
			if (m_text_element && m_text_element->IsAncestorOf(m_cursor.GetElement()))
			{
				AddSelectionToInvalidRect();
				m_end = m_cursor;
				AddSelectionToInvalidRect();
			}
			Update();

			//OP_DBG(("Selection is: %d to %d. [Real values: %d - %d]", GetStartIndex(), GetEndIndex(), m_start_index, m_end_index));
		}
	}

	m_last_screen_mousepos = screenpos;
}

void SVGTextSelection::SelectAll(HTML_Element* text_root)
{
	if(!SVGUtils::IsTextRootElement(text_root))
		return;

	HTML_Element* start = text_root->FirstChild();
	HTML_Element* stop_iter = text_root->NextSiblingActual();
	while(start && start != stop_iter)
	{
		if(start->Type() == HE_TEXT || start->IsMatchingType(Markup::SVGE_TREF, NS_SVG))
			break;

		start = start->Next();
	}

	if(!start)
		return;

	HTML_Element* stop = text_root->LastChild();
	while(stop && stop->LastChild())
		stop = stop->LastChild();

	int textcontentlength = 0;
	while(stop)
	{
		if(stop->Type() == HE_TEXT)
		{
			textcontentlength = stop->GetTextContentLength();
			break;
		}
		else if(stop->IsMatchingType(Markup::SVGE_TREF, NS_SVG))
		{
			SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(text_root);
			HTML_Element* target = doc_ctx ? SVGUtils::FindHrefReferredNode(NULL, doc_ctx, stop) : NULL;
			if(target)
			{
				textcontentlength = target->GetTextContentLength();
			}
			break;
		}

		if(stop == start)
			break;

		stop = stop->Prev();
	}

	if(!stop)
		return;

	SelectionBoundaryPoint ss, se;
	ss.SetLogicalPosition(start,MAX(0, 0-SVGEditable::CalculateLeadingWhitespace(start)));
	se.SetLogicalPosition(stop,MAX(0,textcontentlength-SVGEditable::CalculateLeadingWhitespace(stop)));
	SetSelection(&ss,&se);
}

OP_STATUS SVGTextSelection::SetSelection(SelectionBoundaryPoint *startpoint, SelectionBoundaryPoint *endpoint)
{
	HTML_Element* textelm = startpoint->GetElement();
	while (textelm && !SVGUtils::IsTextRootElement(textelm))
	{
		textelm = textelm->Parent();
	}
	if(!textelm)
	{
		OP_ASSERT(!"The selection start and endpoints must have a textroot element as ancestor.");
		return OpStatus::ERR;
	}

	OP_ASSERT(textelm->IsAncestorOf(startpoint->GetElement()));
	OP_ASSERT(textelm->IsAncestorOf(endpoint->GetElement()));

	ClearSelection(); // Clear old selection

	m_text_element = textelm;

	textselection_info.is_selecting = 0;
	textselection_info.is_valid = 1;

	m_start = *startpoint;
	m_end = *endpoint;

	AddSelectionToInvalidRect();
	Update();

	return OpStatus::OK;
}

OP_STATUS
SVGTextSelection::DOMSetSelection(HTML_Element* selected_elm, UINT32 logical_startindex, UINT32 num_chars)
{
	ClearSelection(); // Clear old selection

	// Can't select from DOM in shadow trees
	if (SVGUtils::IsShadowNode(selected_elm))
		return OpStatus::OK;

	OP_STATUS err = OpStatus::ERR;

	NS_Type ns = selected_elm->GetNsType();
	Markup::Type type = selected_elm->Type();
	if (ns == NS_SVG && SVGUtils::IsTextClassType(type))
	{
		HTML_Element* textelm = selected_elm;

		while (textelm && !SVGUtils::IsTextRootElement(textelm))
		{
			textelm = textelm->Parent();
		}

		m_text_element = textelm;

		// Make sure it has a context
		if (!AttrValueStore::AssertSVGElementContext(m_text_element))
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		if(!textelm)
			err = OpStatus::ERR;
		else
		{
			textselection_info.is_selecting = 0;
			textselection_info.is_valid = 1;
			SetTextSelectionPoint(m_start, m_text_element, selected_elm, logical_startindex);
			SetTextSelectionPoint(m_end, m_text_element, selected_elm, logical_startindex + num_chars);

			err = OpStatus::OK;

			// Do we need to update focus here? Need to check if element is focusable if so.
			// Also SetElementWithSelection perhaps?

			AddSelectionToInvalidRect();
			Update();

			if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(textelm))
				if (FramesDocument* frm_doc = doc_ctx->GetDocument())
					if (HTML_Document* html_doc = frm_doc->GetHtmlDocument())
						html_doc->SetElementWithSelection(textelm);
		}
	}

	return err;
}

BOOL
SVGTextSelection::Contains(SVG_DOCUMENT_CLASS* doc, const OpPoint& doc_point)
{
	if(!IsValid() || IsSelecting() || IsEmpty())
	{
		return FALSE;
	}

	HTML_Element* text_element = GetElement();

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(text_element);
	RETURN_VALUE_IF_NULL(doc_ctx, FALSE);

	Box* svg_box = doc_ctx->GetSVGRoot()->GetLayoutBox();
	RETURN_VALUE_IF_NULL(svg_box, FALSE);

	OpPoint svg_pos(doc_point.x, doc_point.y);
	RECT offsetRect = {0,0,0,0};

	svg_box->GetRect(doc, CONTENT_BOX, offsetRect);
	svg_pos.x -= offsetRect.left;
	svg_pos.y -= offsetRect.top;

	svg_pos = doc->GetVisualDevice()->ScaleToScreen(svg_pos);

	SelectionBoundaryPoint sel_point;
	SVGNumberPair intersection_point(SVGNumber(svg_pos.x), SVGNumber(svg_pos.y));
	if (g_svg_manager_impl->FindTextPosition(doc_ctx, intersection_point, sel_point) == OpBoolean::IS_TRUE)
		return Contains(sel_point);

	return FALSE;
}

BOOL
SVGTextSelection::Contains(const SelectionBoundaryPoint& sel_point)
{
	HTML_Element* iter = m_start.GetElement();
	HTML_Element* stop = m_end.GetElement() ? m_end.GetElement()->NextSiblingActual() : NULL;

	int range_start = m_start.GetElementCharacterOffset();
	int range_end = m_end.GetElementCharacterOffset();
	if (range_start > range_end)
	{
		int tmp = range_end;
		range_end = range_start;
		range_start = tmp;
	}

	while (iter && iter != stop)
	{
		if (sel_point.GetElement() == iter)
		{
			if (iter == m_start.GetElement() && iter == m_end.GetElement())
				return sel_point.GetElementCharacterOffset() >= range_start &&
						sel_point.GetElementCharacterOffset() <= range_end;
			if (iter == m_start.GetElement())
				return sel_point.GetElementCharacterOffset() >= range_start;
			else if (iter == m_end.GetElement())
				return sel_point.GetElementCharacterOffset() <= range_end;

			return TRUE;
		}

		iter = iter->NextActual();
	}

	return FALSE;
}

void
SVGTextSelection::GetRects(OpVector<SVGRect>& list)
{
	if(!IsValid() || IsSelecting() || IsEmpty())
	{
		return;
	}

	SVGTextData data(SVGTextData::SELECTIONRECTLIST);
	data.selection_rect_list = &list;

	HTML_Element* text_element = GetElement();

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(text_element);
	if (doc_ctx)
	{
		SVGNumberPair viewport;
		if (OpStatus::IsSuccess(SVGUtils::GetViewportForElement(text_element, doc_ctx, viewport)))
		{
			SVGUtils::GetTextElementExtent(GetElement(), doc_ctx,
														   0, -1, data, viewport);
		}
	}
}

static inline bool
IsValidSelectionElementType(Markup::Type elm_type)
{
	return
		SVGUtils::IsTextClassType(elm_type) ||
		elm_type == Markup::SVGE_A ||
		elm_type == Markup::SVGE_ALTGLYPH;
}

void
SVGTextSelection::MaybeStartSelection(const SVGManager::EventData& data)
{
	OpPoint point(data.clientX + data.frm_doc->GetVisualViewX(), data.clientY + data.frm_doc->GetVisualViewY());
	if (data.type == ONMOUSEDOWN &&
		data.modifiers == SHIFTKEY_NONE &&
		data.button == MOUSE_BUTTON_1)
	{
		HTML_Element* target_elm = SVGUtils::GetElementToLayout(data.target);
		BOOL selectable = target_elm->GetNsType() == NS_SVG &&
			IsValidSelectionElementType(target_elm->Type());
#ifdef DRAG_SUPPORT
		if (selectable)
			if (Contains(data.frm_doc, point))
				return;
#endif // DRAG_SUPPORT

		BOOL can_start_textselection = !g_pcdoc->GetIntegerPref(PrefsCollectionDoc::DisableTextSelect, data.frm_doc->GetHostName())
#ifdef GRAB_AND_SCROLL
				&& (data.frm_doc->GetWindow() && !data.frm_doc->GetWindow()->GetScrollIsPan())
#endif // GRAB_AND_SCROLL
			;
		if (can_start_textselection && selectable)
		{
			if(OpStatus::IsSuccess(StartSelection(data)))
			{
				return;
			}
		}

		// Clear selection:
		// 1) if mousedown happened on some non-selectable element
		// 2) if there was an error when starting the selection (avoids leaving it in a bad state).
		// 3) if we aren't allowed to start a selection (and we have one already)
		ClearSelection();
	}
}

OP_STATUS
SVGTextSelection::StartSelection(const SVGManager::EventData& data)
{
	OP_NEW_DBG("StartSelection", "svg_textselection");

	SelectionBoundaryPoint cursor = m_cursor; // Save cursor, it contains the point we want to use

	ClearSelection(); // Clear old selection

	OP_ASSERT(!IsSelecting()); // Guard against StartSelection being called without EndSelection

	HTML_Element* selected_elm = data.target;
	HTML_Element* layouted_selected_elm = SVGUtils::GetElementToLayout(data.target);
	m_start_screen_mousepos = OpPoint(data.screenX, data.screenY);
	m_last_screen_mousepos = m_start_screen_mousepos;

	OP_STATUS err = OpStatus::ERR;
	NS_Type ns = layouted_selected_elm->GetNsType();
	Markup::Type type = layouted_selected_elm->Type();
	if (ns == NS_SVG && IsValidSelectionElementType(type))
	{
		HTML_Element* textelm = selected_elm;
		HTML_Element* layouted_textelm = layouted_selected_elm;

		err = OpStatus::OK;

		BOOL findParent = TRUE;
		if (type == Markup::SVGE_A)
		{
			HTML_Element* layouted_parent = layouted_textelm->Parent();
			if(!layouted_parent || layouted_parent->GetNsType() != NS_SVG ||
			   !SVGUtils::IsTextClassType(layouted_parent->Type()))
			{
				findParent = FALSE; // Look for text children
			}
		}

		if (findParent && layouted_textelm->Parent() && textelm->Parent())
		{
			while (textelm && !SVGUtils::IsTextRootElement(layouted_textelm))
			{
				textelm = textelm->Parent();
				layouted_textelm = layouted_textelm->Parent();
			}
		}
		else
		{
			if (!SVGUtils::IsTextRootElement(layouted_textelm))
			{
				textelm = textelm->FirstChild();
				layouted_textelm = layouted_textelm->FirstChild();
				HTML_Element* stop_elm = layouted_textelm->LastChild();

				while (layouted_textelm != stop_elm)
				{
					if (SVGUtils::IsTextRootElement(layouted_textelm))
						break;

					textelm = static_cast<HTML_Element*>(textelm->NextSibling());
					layouted_textelm = static_cast<HTML_Element*>(layouted_textelm->NextSibling());
				}
			}
		}

		if (!textelm || !layouted_textelm || !SVGUtils::IsTextRootElement(layouted_textelm))
			err = OpStatus::ERR;

		data.frm_doc->GetHtmlDocument()->SetElementWithSelection(NULL);
		data.frm_doc->GetHtmlDocument()->FocusElement(data.target, HTML_Document::FOCUS_ORIGIN_UNKNOWN, FALSE, FALSE);

		if (OpStatus::IsSuccess(err))
		{
			m_text_element = textelm;

			// Make sure it has a context
			SVGElementContext* elm_ctx = AttrValueStore::AssertSVGElementContext(m_text_element);
			if (!elm_ctx)
			{
				return OpStatus::ERR_NO_MEMORY;
			}

			textselection_info.is_selecting = 1;
			textselection_info.is_valid = 1;

			//SetTextSelectionPoint(m_start, m_start_index);

#ifdef SVG_SUPPORT_EDITABLE
			SVGTextRootContainer* text_root_cont = elm_ctx->GetAsTextRootContainer();
			if (text_root_cont && text_root_cont->IsEditing())
			{
				data.frm_doc->GetHtmlDocument()->SetElementWithSelection(data.target);
				SVGEditable* editable = text_root_cont->GetEditable();
				if (!cursor.GetElement() || !m_text_element->IsAncestorOf(cursor.GetElement()))
				{
					cursor.SetLogicalPosition(editable->m_caret.m_point.elm, editable->m_caret.m_point.ofs);
				}
				else if (cursor.GetElement()->Type() == HE_TEXT)
				{
					SVGCaretPoint cp;
					cp.elm = cursor.GetElement();
					cp.ofs = cursor.GetElementCharacterOffset();
					editable->m_caret.Set(cp);
				}
			}
#endif // SVG_SUPPORT_EDITABLE

			// This can happen if elements are removed during a selection, bug #331322.
			if(!cursor.GetElement())
			{
				ClearSelection(FALSE); // reset everything
				return OpStatus::ERR;
			}

			m_start = cursor;
			m_end = m_start;
			m_cursor = m_start;

			//OP_DBG(("Selection is: %d to %d. [Real values: %d - %d]", GetStartIndex(), GetEndIndex(), m_start_index, m_end_index));

			SetMouseWasMoved(FALSE);

			err = OpStatus::OK;
		}
	}

	return err;
}

void
SVGTextSelection::SetTextSelectionPoint(SelectionBoundaryPoint& point, HTML_Element* textroot_elm, HTML_Element* sel_elm, int index)
{
	OP_ASSERT(m_text_element);
	HTML_Element* textcontent_elm = sel_elm->FirstChild();
	HTML_Element* textcontent_proxy_elm = textcontent_elm;
	HTML_Element* stop_elm = (HTML_Element*)textroot_elm->NextSibling();
	int logical_length = 0;
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_text_element);
	BOOL text_found = FALSE;
	while(textcontent_elm && textcontent_elm != stop_elm)
	{
		text_found = FALSE;

		if(textcontent_elm->Type() == HE_TEXT)
		{
			textcontent_proxy_elm = textcontent_elm;
			text_found = TRUE;
		}
		else if(textcontent_elm->IsMatchingType(Markup::SVGE_TREF, NS_SVG) && doc_ctx)
		{
			HTML_Element* target = SVGUtils::FindHrefReferredNode(NULL, doc_ctx, textcontent_elm);
			if(target)
			{
				textcontent_proxy_elm = textcontent_elm;
				textcontent_elm = target;
				text_found = TRUE;
			}
		}

		if(text_found)
		{
			logical_length += textcontent_elm->GetTextContentLength();
			if(index <= logical_length)
			{
				point.SetLogicalPosition(textcontent_proxy_elm, index - (logical_length-textcontent_elm->GetTextContentLength()));
				break;
			}
		}

		textcontent_elm = textcontent_elm->Next();
	}

}

BOOL
SVGTextSelection::MouseHasMoved(const OpPoint& oldpos, const OpPoint& newpos)
{
	return (oldpos.x + MouseMovePixelThreshold <= newpos.x ||
			oldpos.x - MouseMovePixelThreshold >= newpos.x ||
			oldpos.y + MouseMovePixelThreshold <= newpos.y ||
			oldpos.y - MouseMovePixelThreshold >= newpos.y);
}

OP_STATUS
SVGTextSelection::EndSelection(const SVGManager::EventData& data)
{
	if(m_text_element)
	{
		textselection_info.is_selecting = 0;
	}

	if(!MouseHasMoved(m_start_screen_mousepos, OpPoint(data.screenX, data.screenY)))
	{
		if(MouseWasMoved())
		{
			ClearSelection();
		}
		else
		{
			m_end = m_start;
			m_cursor.Reset();
		}
	}
	else
	{
		HTML_Document* html_doc = data.frm_doc->GetHtmlDocument();
		if (html_doc)
		{
			/* The focused element set in StartSelection() should be set as the one with
			   a selection, if present. Otherwise if the mouse down was done in
			   e.g. <text>, all its content was selected but the mouse up was done over
			   <svg>, the root element was set as the one with the selection
			   (data.target == <svg>) which is wrong obviously. */
			HTML_Element* focused_elm = html_doc->GetFocusedElement();
			if (!focused_elm)
				focused_elm = data.target;
			html_doc->SetElementWithSelection(focused_elm);
		}
	}

	if (m_text_element && !(m_end == m_cursor) &&
		m_text_element->IsAncestorOf(m_cursor.GetElement()))
	{
		m_end = m_cursor;
		//SetTextSelectionPoint(m_end, m_end_index);
	}

#ifdef SVG_DEBUG_TEXTSELECTION
//	OP_NEW_DBG("SVGTextSelection::EndSelection", "svg_textselection");
//	OP_DBG(("Selection is: %d to %d. [Real values: %d - %d]", GetStartIndex(), GetEndIndex(), m_start_index, m_end_index));
#endif // SVG_DEBUG_TEXTSELECTION

	return OpStatus::OK;
}

int
SVGTextSelection::GetText(TempBuffer* buffer)
{
	if(!IsValid() || IsSelecting() || IsEmpty())
	{
		return 0;
	}

	SVGTextData data(SVGTextData::SELECTEDSTRING);
	data.buffer = buffer;
	data.range.length = 0;

	HTML_Element* layouted_elm = SVGUtils::GetElementToLayout(GetElement());
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(layouted_elm);
	if (doc_ctx)
	{
		SVGNumberPair dummy_viewport;
		OP_STATUS err = SVGUtils::GetTextElementExtent(layouted_elm, doc_ctx,
													   0, -1, data, dummy_viewport);

		if(OpStatus::IsSuccess(err))
		{
			if(!buffer)
				return data.range.length;
			else
				return buffer->Length();
		}
	}

	return 0;
}

OP_STATUS
SVGTextSelection::GetIntersected(const OpPoint& screenpos, HTML_Element*& intersected_elm, int& intersected_glyph_index)
{
	OP_NEW_DBG("SVGTextSelection::GetIntersected", "svg_textselection_invalidation");

	intersected_elm = NULL;
	intersected_glyph_index = 0;

	return OpStatus::ERR;
}

BOOL SVGTextSelection::IsEmpty()
{
	if(!m_start.GetElement() || !m_end.GetElement())
		return TRUE;

	return (m_start == m_end);
}
# endif // SVG_SUPPORT_TEXTSELECTION
#endif // SVG_SUPPORT
