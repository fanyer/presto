/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT_EDITABLE

#include "modules/display/vis_dev.h"

#include "modules/doc/frm_doc.h"

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGEditable.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGManagerImpl.h"

SVGEditableCaret::SVGEditableCaret(SVGEditable *edit)
	: m_edit(edit)
	, m_pos(0, 0, 0, 0)
	, m_glyph_pos(0, 0, 0, 0)
	, m_line(0)
	, m_on(FALSE)
	, m_update_pos_lock(0)
	, m_update_pos_needed(FALSE)
	, m_prefer_first(FALSE)
{
}

SVGEditableCaret::~SVGEditableCaret()
{
	m_timer.Stop();
}

void SVGEditableCaret::LockUpdatePos(BOOL lock)
{
	if (lock)
		m_update_pos_lock++;
	else
	{
		m_update_pos_lock--;
		if (m_update_pos_lock == 0 && m_update_pos_needed)
		{
			UpdatePos(m_prefer_first);
		}
	}
}

void SVGEditableCaret::PlaceFirst(HTML_Element* edit_root)
{
	OP_ASSERT(!edit_root || SVGUtils::IsEditable(edit_root));

	SVGCaretPoint new_cp;

	if (edit_root)
	{
		HTML_Element* editable_elm = m_edit->FindEditableElement(edit_root, TRUE, FALSE);
		if (edit_root->IsAncestorOf(editable_elm))
			new_cp.elm = editable_elm;
	}

	Set(new_cp);
}

OP_STATUS SVGEditableCaret::Init(BOOL create_line_if_empty, HTML_Element* edit_root)
{
	OP_ASSERT(SVGUtils::IsEditable(edit_root));

	PlaceFirst(edit_root);

	if (create_line_if_empty && !m_point.IsValid())
	{
		// We have nothing to put the caret on. Create a empty textelement.
		HTML_Element *first_text_elm = m_edit->NewTextElement(UNI_L(""), 0);
		if (!first_text_elm)
			return OpStatus::ERR_NO_MEMORY;

		first_text_elm->UnderSafe(m_edit->GetDocument(), edit_root);

		m_point.elm = first_text_elm;
	}

	Invalidate();

	m_on = FALSE;

	UpdatePos();

	return OpStatus::OK;
}

void SVGEditableCaret::Invalidate()
{
	HTML_Element* elm = m_point.elm;
	while (elm && (elm->IsText() ||
				   elm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG)))
		elm = elm->Parent();

	// Invalidating the element the caret is on
	// FIXME: Might be possible to only invalidate the caret area
	if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm))
		SVGDynamicChangeHandler::RepaintElement(doc_ctx, elm);
}

BOOL SVGEditableCaret::UpdatePos(BOOL prefer_first)
{
	if (!m_point.IsValid())
	{
		PlaceFirst();
		if (!m_point.IsValid())
			return FALSE;
	}

	// FIX: m_prefer_first is still not used from all places calling updatepos.

	m_prefer_first = prefer_first;
	m_update_pos_needed = TRUE;

	if (m_update_pos_lock > 0)
	{
		// Update is locked or the document is dirty and the caret
		// will be update very soon (after the reflow)
		return FALSE;
	}

	Invalidate();

	if (m_point.ofs > m_point.elm->GetTextContentLength())
	{
		//OP_ASSERT(0); // If this happens, something has changed the textcontent without
					  // our knowledge (probably DOM).
		m_point.ofs = m_point.elm->GetTextContentLength();
	}

	Invalidate();

	m_update_pos_needed = FALSE;

	SVGEditableListener* listener = m_edit->GetListener();
	if (listener)
		listener->OnCaretMoved();

	return TRUE;
}

void SVGEditableCaret::MoveWord(BOOL forward)
{
	INT32 SeekWord(const uni_char* str, INT32 start, INT32 step, INT32 max_len); // widgets/OpEdit.cpp

	if (!m_point.IsValid())
		return;

	SVGEditPoint ep = m_edit->ToEdit(m_point);
	SVGEditPoint new_ep = ep;

	if (!forward)
	{
		if (m_point.ofs != 0)
			// Jump within current element
			new_ep.ofs = ep.ofs + SeekWord(ep.elm->TextContent(), ep.ofs, -1, ep.elm->GetTextContentLength());
		if (new_ep.ofs == ep.ofs)
			// Jump to previous element
			m_edit->GetNearestCaretPos(ep.elm, new_ep, FALSE, FALSE);
	}
	else
	{
		if (m_point.ofs < ep.elm->GetTextContentLength())
			// Jump within current element
			new_ep.ofs = ep.ofs + SeekWord(ep.elm->TextContent(), ep.ofs, 1, ep.elm->GetTextContentLength());
		if (new_ep.ofs == ep.ofs)
		{
			// Jump to next element
			if (m_edit->GetNearestCaretPos(ep.elm, new_ep, TRUE, FALSE))
			{
				new_ep.ofs = SeekWord(new_ep.elm->TextContent(), 0, 1, new_ep.elm->GetTextContentLength());
			}
		}
	}

	Place(m_edit->ToCaret(new_ep), TRUE, FALSE);
}

BOOL SVGEditableCaret::CheckElementOffset(SVGEditPoint& ep, BOOL forward)
{
	if (SVGEditable::IsXMLSpacePreserve(ep.elm))
		// No need to go looking for words, just check bounds on the text element
		return ep.IsValidTextOffset();
	else
	{
		// Test if we can place the caret at new_ofs.
		const uni_char* word;
		int word_ofs;
		return m_edit->FindWordStartAndOffset(ep, word, word_ofs, forward);
	}
}

void SVGEditableCaret::Move(BOOL forward, BOOL force_stop_on_break /* = FALSE */)
{
	SVGEditPoint ep = m_edit->ToEdit(m_point);

	ep.ofs += (forward ? 1 : -1);

	// Test if we can place the caret at the new offset.
	if ((forward || m_point.ofs > 0) && CheckElementOffset(ep, forward))
	{
		Place(m_edit->ToCaret(ep), TRUE, forward);
	}
	else
	{
		// We couldn't. Search for the nearest caretpos in other elements.
		SVGEditPoint nearest_ep;

		BOOL moved = m_edit->GetNearestCaretPos(m_point.elm, nearest_ep, forward, FALSE, FALSE);
		if (!moved)
			return;

		OP_ASSERT(nearest_ep.IsValid());

		// When moving forward, we don't want to stop on the first
		// break that we discover, since then we will remain on the
		// same line as previously, so look for the next editable
		// element, and place the caret there
		BOOL passed_break = FALSE;
		if (!force_stop_on_break && forward && !nearest_ep.IsText())
		{
			SVGEditPoint next_ep;
			if (m_edit->GetNearestCaretPos(nearest_ep.elm, next_ep, forward, FALSE, FALSE))
			{
				passed_break = TRUE;
				nearest_ep = next_ep;
			}
		}

		if (forward && !passed_break && nearest_ep.ofs == 0 && nearest_ep.IsText() &&
			!m_point.elm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG))
			nearest_ep.ofs = 1;

		Place(m_edit->ToCaret(nearest_ep), !forward || (!force_stop_on_break && !passed_break), forward);
	}
}

static BOOL GetTextAreaBoundraries(HTML_Element* element,
								   SVGNumber& start, SVGNumber& end)
{
	SVGProxyObject* proxy = NULL;
	SVGLengthObject* len = NULL;
	AttrValueStore::GetProxyObject(element, Markup::SVGA_WIDTH, &proxy);
	if (proxy &&
		proxy->GetRealObject() &&
		proxy->GetRealObject()->Type() == SVGOBJECT_LENGTH)
	{
		len = static_cast<SVGLengthObject*>(proxy->GetRealObject());
	}
	else
		return FALSE; // 'auto'

	SVGValueContext vcxt; // FIXME: Need correct setup

	SVGLengthObject* xl = NULL;
	AttrValueStore::GetLength(element, Markup::SVGA_X, &xl);
	if (xl)
		start =	SVGUtils::ResolveLength(xl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

	end = start + SVGUtils::ResolveLength(len->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
	return TRUE;
}

void SVGEditableCaret::FindBoundrary(SVGLineInfo* lineinfo, SVGNumber boundrary_max)
{
	HTML_Element* editroot = m_edit->GetEditRoot();
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(editroot);

	SVGMatrix ctm;
	RETURN_VOID_IF_ERROR(SVGUtils::GetElementCTM(editroot, doc_ctx, &ctm));
	SVGNumber test_start_x = m_pos.x;
	SVGNumber test_end_x = boundrary_max;

	SelectionBoundaryPoint pt;

	SVGNumberPair curr_testpos((test_start_x + test_end_x) / 2, m_pos.y - lineinfo->height / 2);

	SVGNumber diff = (curr_testpos.x - test_start_x).abs();
	SVGNumber limit = lineinfo->height / 8; // FIXME: Pretty random

	int old_line = m_line; // FindTextPosition overwrites m_line

	while (diff > limit)
	{
		SVGNumberPair testpos = ctm.ApplyToCoordinate(curr_testpos);

		if (g_svg_manager_impl->FindTextPosition(doc_ctx,
												 testpos, pt) == OpBoolean::IS_TRUE &&
			pt.GetElement() && pt.GetElement()->Type() == HE_TEXT)
		{
			// Hit something
			SVGCaretPoint new_cp;
			new_cp.elm = pt.GetElement();
			new_cp.ofs = pt.GetElementCharacterOffset();

			Set(new_cp);
			StickToPreceding();

			test_start_x = curr_testpos.x;
		}
		else
		{
			// Missed
			test_end_x = curr_testpos.x;
		}

		curr_testpos.x = (test_start_x + test_end_x) / 2;
		diff = (curr_testpos.x - test_start_x).abs();
	}

	m_line = old_line;
}

void SVGEditableCaret::Place(Placement place)
{
	if (!m_point.elm)
		return;

	BOOL is_multiline = m_edit->IsMultiLine();
	OpVector<SVGLineInfo>* lineinfo = NULL;
	SVGElementContext* elm_ctx = AttrValueStore::AssertSVGElementContext(m_edit->GetEditRoot());
	SVGTextRootContainer* text_root_cont = elm_ctx ? elm_ctx->GetAsTextRootContainer() : NULL;
	if (is_multiline && text_root_cont)
	{
		lineinfo = static_cast<SVGTextAreaElement*>(text_root_cont)->GetLineInfo();
	}

	switch(place)
	{
	case PLACE_START:
		PlaceFirst(m_edit->GetEditRoot());
		break;
	case PLACE_LINESTART:
	case PLACE_LINEEND:
		{
			SVGNumber line_start_min, line_end_max;
			if (is_multiline && lineinfo &&
				GetTextAreaBoundraries(m_edit->GetEditRoot(),
									   line_start_min, line_end_max))
			{
				SVGLineInfo* li = lineinfo->Get(m_line);

				SVGNumber boundrary = place == PLACE_LINESTART ?
					// Potential optimization (for short lines)
					SVGNumber::max_of(line_start_min, m_pos.x - li->width) :
					SVGNumber::min_of(line_end_max, m_pos.x + li->width);

				FindBoundrary(li, boundrary);
				break;
			}
			else if (place == PLACE_LINESTART)
			{
				PlaceFirst(m_edit->GetEditRoot());
				break;
			}
			// Fall-through to handle non-multiline PLACE_LINEEND the same as PLACE_END
		}
	case PLACE_END:
		{
			SVGEditPoint ep;
			ep.elm = m_edit->FindEditableElement(m_edit->GetEditRoot()->LastChildActual(), FALSE, TRUE);
			if (ep.IsText())
			{
				ep.ofs = ep.elm->GetTextContentLength();
				Set(m_edit->ToCaret(ep));
			}
		}
		break;
	case PLACE_LINEPREVIOUS:
	case PLACE_LINENEXT:
		{
			if (!is_multiline || !lineinfo)
				// 'Single line' text or broken layout
				break;

			SVGDocumentContext* doc_ctx =
				AttrValueStore::GetSVGDocumentContext(m_edit->GetEditRoot());

			SVGMatrix ctm;
			RETURN_VOID_IF_ERROR(SVGUtils::GetElementCTM(m_edit->GetEditRoot(),
														 doc_ctx, &ctm));
			SVGLineInfo* li = lineinfo->Get(m_line);
			BOOL use_fallback = TRUE;
			if(li)
			{
				SVGNumberPair newpos(m_pos.x, m_pos.y);

				if (place == PLACE_LINENEXT)
				{
					newpos.y += li->height / 2;
				}
				else
				{
					newpos.y -= (li->height * 3)/2;
				}

				SelectionBoundaryPoint pt;

				int old_line = m_line; // FindTextPosition overwrites m_line

				newpos = ctm.ApplyToCoordinate(newpos);
				OP_BOOLEAN result = g_svg_manager_impl->FindTextPosition(doc_ctx, newpos, pt);

				m_line = old_line;

				if (result == OpBoolean::IS_TRUE &&
					pt.GetElement() != m_edit->GetEditRoot() &&
					m_edit->GetEditRoot()->IsAncestorOf(pt.GetElement()))
				{
					SVGCaretPoint new_cp;
					new_cp.elm = pt.GetElement();
					new_cp.ofs = pt.GetElementCharacterOffset();

					if (new_cp.IsText())
					{
						Set(new_cp);
						StickToPreceding();
					}
					use_fallback = FALSE;
				}
			}
			
			if(use_fallback)
			{
				// No hit - try some other methods
				if ((place == PLACE_LINEPREVIOUS && m_line > 0) ||
					(place == PLACE_LINENEXT && m_line+1 < (int)lineinfo->GetCount()))
				{
					SVGLineInfo* target_li = lineinfo->Get(m_line + (place == PLACE_LINENEXT ? 1 : -1));
					if (target_li && (target_li->forced_break || (place == PLACE_LINEPREVIOUS && (unsigned int)m_line >= lineinfo->GetCount())))
					{
						// There _should_ be a line to place on
						// Try finding a break
						HTML_Element* elm = m_point.elm;
						do 
						{
							elm = m_edit->FindEditableElement(elm, place == PLACE_LINENEXT, FALSE);
						} while (elm && !elm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG));

						if (elm && elm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG))
						{
							SVGCaretPoint new_cp;
							new_cp.elm = elm;

							Set(new_cp);

							if (place == PLACE_LINENEXT)
							{
								SVGEditPoint new_ep;
								new_ep.elm = m_edit->FindEditableElement(m_point.elm, place == PLACE_LINENEXT, FALSE);
								if (new_ep.elm)
									Place(m_edit->ToCaret(new_ep), FALSE, FALSE);
							}
							else
							{
								StickToPreceding();
							}
						}
					}
				}
			}
		}
		break;
	}
}

void SVGEditableCaret::Place(const SVGCaretPoint& cp, BOOL allow_snap, BOOL snap_forward)
{
	if (!cp.IsValid())
	{
		OP_ASSERT(0);
		PlaceFirst();
		return;
	}

	OP_ASSERT(cp.ofs >= 0);
// 	if (ofs < 0) // Just to be sure.
// 		ofs = 0;

	Set(cp);

	if (allow_snap)
		StickToPreceding();

	UpdatePos(!snap_forward);
	RestartBlink();
}

void SVGEditableCaret::Set(const SVGCaretPoint& cp)
{
	OP_ASSERT(!cp.IsValid() || cp.IsText() || cp.elm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG));
	m_point = cp;
}

void SVGEditableCaret::StickToPreceding()
{
	if (m_point.IsText() && m_point.elm->GetTextContentLength() == 0)
		return; // Stay inside dummyelements
	if (m_point.ofs == 0)
	{
		SVGEditPoint new_ep;
		new_ep.elm = m_edit->FindEditableElement(m_point.elm, FALSE, FALSE, FALSE);
		if (new_ep.IsText())
		{
			const uni_char* word;
			int word_ofs;
			new_ep.ofs = new_ep.elm->GetTextContentLength();

			// Check if it is possible to place in this element.
			if (m_edit->FindWordStartAndOffset(new_ep, word, word_ofs, TRUE))
			{
				Place(m_edit->ToCaret(new_ep));
			}
		}
	}
}

void SVGEditableCaret::StickToDummy()
{
	if (m_point.IsText() && m_point.elm->GetTextContentLength() == m_point.ofs)
	{
		SVGCaretPoint new_cp;
		new_cp.elm = m_edit->FindElementAfterOfType(m_point.elm, HE_TEXT);
		if (new_cp.elm && new_cp.elm->GetTextContentLength() == 0)
		{
			Place(new_cp);
		}
	}
}

void SVGEditableCaret::Paint(SVGCanvas* canvas)
{
	if(m_on && canvas && OpStatus::IsSuccess(canvas->SaveState()))
	{
		canvas->EnableStroke(SVGCanvasState::USE_COLOR);
		canvas->SetStrokeColor(0xFF000000);
		canvas->SetLineWidth(0.5);
		canvas->SetVectorEffect(SVGVECTOREFFECT_NON_SCALING_STROKE);
		canvas->DrawLine(m_glyph_pos.x, m_glyph_pos.y,
						 m_glyph_pos.x, m_glyph_pos.y+m_glyph_pos.height);
		canvas->RestoreState();
	}
}

void SVGEditableCaret::BlinkNow()
{
	m_on = !m_on;
	Invalidate();
}

void SVGEditableCaret::RestartBlink()
{
	StopBlink();

	if (!m_edit->IsFocused())
		return;

	m_on = TRUE;
	Invalidate();

	m_timer.SetTimerListener(this);
	m_timer.Start(500);
}

void SVGEditableCaret::StopBlink()
{
	m_timer.Stop();

	if(m_on)
	{
		m_on = FALSE;
		Invalidate();
	}
}

void SVGEditableCaret::OnTimeOut(OpTimer* timer)
{
	timer->Start(500);
	BlinkNow();
}

#endif // SVG_SUPPORT_EDITABLE
#endif // SVG_SUPPORT
