/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT_EDITABLE

#include "modules/doc/html_doc.h"

#include "modules/pi/OpClipboard.h"

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGEditable.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGCanvas.h"

#include "modules/widgets/optextfragmentlist.h"
#include "modules/widgets/optextfragment.h"

#include "modules/logdoc/xmlenum.h" // For XMLA_SPACE
#include "modules/dochand/winman.h"
#include "modules/windowcommander/src/WindowCommander.h"

SVGEditable::SVGEditable()
	: m_root(NULL)
	, m_caret(this)
#if 0 //def WIDGETS_IME_SUPPORT
	, m_imstring(NULL)
	, im_is_composing(FALSE)
#endif
	, m_wants_tab(FALSE)
	, m_listener(NULL)
{
}

OP_STATUS SVGEditable::Construct(SVGEditable** obj, HTML_Element* root_elm)
{
	*obj = OP_NEW(SVGEditable, ());
	if (*obj == NULL || OpStatus::IsError((*obj)->Init(root_elm)))
	{
		OP_DELETE(*obj);
		*obj = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

SVGEditable::~SVGEditable()
{
	// OpInputContext::~OpInputContext does this too, but then
	// it's to late for our virtual OnKeyboardInputLost to be
	// called.
	SetParentInputContext(NULL);
#ifdef USE_OP_CLIPBOARD
	g_clipboard_manager->UnregisterListener(this);
#endif // USE_OP_CLIPBOARD
}

OP_STATUS SVGEditable::Init(HTML_Element* root_elm)
{
	OP_ASSERT(SVGUtils::IsTextRootElement(root_elm));
	m_root = root_elm;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_root);
	if (!doc_ctx)
		return OpStatus::ERR;

	VisualDevice* vis_dev = doc_ctx->GetVisualDevice();
	SetParentInputContext(vis_dev);
	if (vis_dev->IsFocused())
		SetFocus(FOCUS_REASON_OTHER);

	return m_caret.Init(TRUE, root_elm);
}

SVGCaretPoint SVGEditable::ToCaret(const SVGEditPoint& ep)
{
	SVGCaretPoint cp;
	cp.elm = ep.elm;
	cp.ofs = ep.ofs - CalculateLeadingWhitespace(ep.elm);
	cp.ofs = MAX(0, cp.ofs);
	return cp;
}

SVGEditPoint SVGEditable::ToEdit(const SVGCaretPoint& cp)
{
	SVGEditPoint ep;
	ep.elm = cp.elm;
	ep.ofs = cp.ofs + CalculateLeadingWhitespace(cp.elm);
	return ep;
}

#if 0
void SVGEditable::InitEditableRoot(HTML_Element* root)
{
	// Init caret in this root
	if (!m_caret.m_helm)
		m_caret.Init(TRUE, root);
}

void SVGEditable::UninitEditableRoot(HTML_Element* root)
{
	if (root->IsAncestorOf(m_caret.m_helm))
		ReleaseFocus();
}
#endif // 0

SVG_DOCUMENT_CLASS* SVGEditable::GetDocument()
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_root);
	if (!doc_ctx)
		return NULL;

	return doc_ctx->GetDocument();
}

void SVGEditable::FocusEditableRoot(HTML_Element* helm, FOCUS_REASON reason)
{
	m_caret.RestartBlink();
	SetFocus(reason);
}

OP_STATUS SVGEditable::Clear()
{
	HTML_Element* root_elm = m_root;
	while (root_elm && root_elm->FirstChild())
		DeleteElement(root_elm->FirstChild());

	return m_caret.Init(TRUE, m_root);
}

void SVGEditable::Paint(SVGCanvas* canvas)
{
	m_caret.Paint(canvas);
}

#if 0
BOOL SVGEditable::HandleMouseEvent(HTML_Element* helm, DOM_EventType event, int x, long y, MouseButton button)
{
	BOOL handled = FALSE;

	if (event == ONMOUSEDOWN)
	{
		FocusEditableRoot(helm, FOCUS_REASON_MOUSE);

#if defined(_X11_SELECTION_POLICY_)
		if (button == MOUSE_BUTTON_3/* && GetEditableContainer(helm)*/)
		{
			g_clipboard_manager->SetMouseSelectionMode(TRUE);
			if (g_clipboard_manager->HasText())
			{
				Paste();
			}
			g_clipboard_manager->SetMouseSelectionMode(FALSE);
			handled = TRUE; // Must set to TRUE, otherwise we get bug #217331
		}
#endif
	}

	return handled;
}
#endif // 0

#ifdef SVG_SUPPORT_TEXTSELECTION
SVGTextSelection* SVGEditable::GetTextSelection()
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_root);
	if (!doc_ctx)
		return NULL;

	return &doc_ctx->GetTextSelection();
}

void SVGEditable::Select(SelectionBoundaryPoint *startpoint, SelectionBoundaryPoint *endpoint)
{
	SVGTextSelection* ts = GetTextSelection();
	SelectionBoundaryPoint *start, *end;
	if (startpoint->Precedes(*endpoint))
	{
		start = startpoint;
		end = endpoint;
	}
	else
	{
		start = endpoint;
		end = startpoint;
	}

	ts->SetSelection(start, end);
}

SelectionBoundaryPoint SVGEditable::GetTextSelectionPoint(HTML_Element* helm, int character_offset, BOOL prefer_first)
{
	SelectionBoundaryPoint sel_point;
#if 0
	const uni_char* start_of_word = NULL;
	int word_char_offset = 0;

	SVGCaretPoint cp;
	cp.elm = helm;
	cp.ofs = character_offset;

	SVGEditPoint ep = ToEdit(cp);

	BOOL ret = FindWordStartAndOffset(ep, start_of_word, word_char_offset, !prefer_first);
	if (!ret && helm->Type() == HE_TEXT && !start_of_word)
	{
		start_of_word = helm->TextContent();
		word_char_offset = 0;
	}
 	sel_point.SetTextElement(helm);
	sel_point.SetStartOfWord(start_of_word);
	sel_point.SetWordCharacterOffset(word_char_offset);
	sel_point.SetFindByOffset(TRUE);
	sel_point.SetNewEndPoint(TRUE);
	sel_point.SetVirtualPosition(LONG_MIN);
#endif // 0
	sel_point.SetLogicalPosition(helm, character_offset);

	return sel_point;
}

void SVGEditable::SelectToCaret(SVGCaretPoint& from)
{
	SelectionBoundaryPoint sel_point1, sel_point2;
	BOOL new_selection = TRUE;
	SVGTextSelection* ts = GetTextSelection();

	if (ts && !ts->IsEmpty())
	{
#if 0
		HTML_Element* start_elm = ts->GetStartTextElement();
		HTML_Element* stop_elm = ts->GetEndTextElement();
		INT32 start_ofs = ts->GetStartText() ? ts->GetStartText() - start_elm->TextContent() : 0;
		INT32 stop_ofs = ts->GetEndText() ? ts->GetEndText() - stop_elm->TextContent() : 0;
		if (from.elm == start_elm && from.ofs == start_ofs)
		{
			sel_point1 = GetTextSelectionPoint(stop_elm, stop_ofs);
			sel_point2 = GetTextSelectionPoint(m_caret.m_point.elm, m_caret.m_point.ofs);
			new_selection = FALSE;
		}
		else if (from.elm == stop_elm && from.ofs == stop_ofs)
		{
			sel_point1 = GetTextSelectionPoint(start_elm, start_ofs);
			sel_point2 = GetTextSelectionPoint(m_caret.m_point.elm, m_caret.m_point.ofs);
			new_selection = FALSE;
		}
#else
		SelectionBoundaryPoint& start = ts->GetLogicalStartPoint();
		SelectionBoundaryPoint& stop = ts->GetLogicalEndPoint();

		if (from.elm == start.GetElement() && from.ofs == start.GetElementCharacterOffset())
		{
			sel_point1 = stop;
			sel_point2 = GetTextSelectionPoint(m_caret.m_point.elm, m_caret.m_point.ofs);
			new_selection = FALSE;
		}
		else if (from.elm == stop.GetElement() && from.ofs == stop.GetElementCharacterOffset())
		{
			sel_point1 = start;
			sel_point2 = GetTextSelectionPoint(m_caret.m_point.elm, m_caret.m_point.ofs);
			new_selection = FALSE;
		}
#endif // 0
	}
	if (new_selection)
	{
		sel_point1 = GetTextSelectionPoint(from.elm, from.ofs);
		sel_point2 = GetTextSelectionPoint(m_caret.m_point.elm, m_caret.m_point.ofs);
	}

	Select(&sel_point1, &sel_point2);
}
#endif // SVG_SUPPORT_TEXTSELECTION

BOOL SVGEditable::HasSelectedText()
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection* ts = GetTextSelection();
	return ts && ts->GetElement() == GetEditRoot() && !ts->IsEmpty();
#else
	return FALSE;
#endif // SVG_SUPPORT_TEXTSELECTION
}

#ifdef SVG_SUPPORT_TEXTSELECTION
void SVGEditable::SelectNothing()
{
	SVGTextSelection* ts = GetTextSelection();
	if(ts)
		ts->ClearSelection();
}

#ifdef USE_OP_CLIPBOARD
void SVGEditable::OnCopy(OpClipboard* clipboard)
{
	SVGTextSelection* selection = GetTextSelection();
	if(!selection)
		return;
	if (selection->IsEmpty())
		return;

	TempBuffer buffer;
	if(0 == selection->GetText(&buffer))
		return;

	RAISE_IF_MEMORY_ERROR(clipboard->PlaceText(buffer.GetStorage(), GetDocument() && GetDocument()->GetWindow() ?
	GetDocument()->GetWindow()->GetWindowCommander()->GetURLContextID() : 0));
}

void SVGEditable::OnCut(OpClipboard* clipboard)
{
	if (!HasSelectedText())
		return;
	OnCopy(clipboard);
	OpInputAction cutaction(OpInputAction::ACTION_DELETE);
	OnInputAction(&cutaction);
}
#endif // USE_OP_CLIPBOARD

void SVGEditable::Copy()
{
#ifdef USE_OP_CLIPBOARD
	RAISE_IF_MEMORY_ERROR(g_clipboard_manager->Copy(this, 0, GetDocument(), GetEditRoot()));
#endif // USE_OP_CLIPBOARD
}

void SVGEditable::Cut()
{
#ifdef USE_OP_CLIPBOARD
	RAISE_IF_MEMORY_ERROR(g_clipboard_manager->Cut(this, 0, GetDocument(), GetEditRoot()));
#endif // USE_OP_CLIPBOARD
}
#endif // SVG_SUPPORT_TEXTSELECTION

void SVGEditable::Paste()
{
#ifdef USE_OP_CLIPBOARD
	RAISE_IF_MEMORY_ERROR(g_clipboard_manager->Paste(this, GetDocument(), GetEditRoot()));
#endif // USE_OP_CLIPBOARD
}

#ifdef USE_OP_CLIPBOARD
void SVGEditable::OnPaste(OpClipboard* clipboard)
{
	OpString text;

	if (clipboard->HasText())
	{
		RAISE_IF_MEMORY_ERROR((clipboard->GetText(text)));
		RAISE_IF_MEMORY_ERROR(InsertText(text.CStr(), text.Length()));
	}
	else
		return;

	ScrollIfNeeded();
}
#endif // USE_OP_CLIPBOARD

void SVGEditable::Invalidate()
{
	// Invalidating the element
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_root);
	if (doc_ctx)
		SVGDynamicChangeHandler::HandleAttributeChange(doc_ctx, m_root, Markup::SVGA_FONT_SIZE,
													   NS_SVG, FALSE);
}

void SVGEditable::InsertBreak(BOOL break_list, BOOL new_paragraph)
{
	OP_ASSERT(m_caret.m_point.IsValid());
	if (!m_caret.m_point.IsValid())
		return; // Sanitycheck. Should not happen but sometimes *everything* was removed above.

	HTML_Element *new_elm = NewElement(Markup::SVGE_TBREAK);
	if (!new_elm)
	{
		ReportOOM();
		return;
	}

	InsertElement(new_elm);

	m_caret.RestartBlink();
}

void SVGEditable::InsertElement(HTML_Element* helm)
{
	SVG_DOCUMENT_CLASS* doc = GetDocument();
	if (!doc)
		return;

	// If the caret is inside a textelement we have to split the textelement in 2.
	BOOL did_split = SplitElement(m_caret.m_point);

	// Insert helm
	if (m_caret.m_point.ofs == 0 && !did_split)
		helm->PrecedeSafe(doc, m_caret.m_point.elm);
	else
		helm->FollowSafe(doc, m_caret.m_point.elm);

	Invalidate();

	HTML_Element* old_caret_elm = m_caret.m_point.elm;

	BOOL was_break = helm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG);
	// When inserting a break, we want to move past it
	if (was_break && (did_split || m_caret.m_point.ofs != 0))
		m_caret.Move(TRUE);

	// If the old caretelement was a empty textelement, we can now delete it.
	if (old_caret_elm != m_caret.m_point.elm && old_caret_elm->Type() == HE_TEXT &&
		old_caret_elm->GetTextContentLength() == 0)
		DeleteElement(old_caret_elm);
}

OP_STATUS SVGEditable::CreateTemporaryCaretElement()
{
	SVG_DOCUMENT_CLASS* doc = GetDocument();
	if (!doc)
		return OpStatus::ERR;

	HTML_Element* new_elm = NewTextElement(UNI_L(""), 1);
	if (!new_elm)
		return OpStatus::ERR_NO_MEMORY;

	if (m_caret.m_point.ofs == 1)
		new_elm->FollowSafe(doc, m_caret.m_point.elm);
	else
		new_elm->PrecedeSafe(doc, m_caret.m_point.elm);

	Invalidate();

	SVGCaretPoint new_cp;
	new_cp.elm = new_elm;
	m_caret.Set(new_cp);
	return OpStatus::OK;
}

OP_STATUS SVGEditable::InsertText(const uni_char *text, int len, BOOL allow_append)
{
	if (!m_caret.m_point.IsText())
	{
		// We could probably just move the caret to the next element
		// if one existed, and then prepend the text there
		RETURN_IF_ERROR(CreateTemporaryCaretElement());
	}

	// Take out the text from the element, add the text we should
	// insert and set it back to the element.

	OpString str;
	if (OpStatus::IsError(str.Set(m_caret.m_point.elm->TextContent())))
		return OpStatus::ERR_NO_MEMORY;

	SVGEditPoint ep = ToEdit(m_caret.m_point);

	int pos = MIN(str.Length(), ep.ofs);

	RETURN_IF_ERROR(str.Insert(pos, text, len));

	SetElementText(ep.elm, str.CStr());

	// Update
	ep.ofs += len;

	m_caret.Place(ToCaret(ep), TRUE, FALSE);

	return OpStatus::OK;
}

#if 0
void SVGEditable::OnScaleChanged()
{
	m_caret.UpdatePos();
	ScrollIfNeeded();
}
#endif // 0

void SVGEditable::ReportOOM()
{
	g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

void SVGEditable::ReportIfOOM(OP_STATUS status)
{
	if (OpStatus::IsMemoryError(status))
		ReportOOM();
}

void SVGEditable::ScrollIfNeeded()
{
	// FIXME
}

void SVGEditable::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	SVG_DOCUMENT_CLASS* doc = GetDocument();
	if(!doc)
		return;

	if (focus && !m_caret.m_point.IsValid())
		m_caret.Init(TRUE, m_root);

	if (focus && doc->GetHtmlDocument())
		doc->GetHtmlDocument()->SetFocusedElement(m_root, FALSE);

	if (focus)
		m_caret.RestartBlink();
	else
		m_caret.StopBlink();

	if (HTML_Document* html_doc = doc->GetHtmlDocument())
		html_doc->SetElementWithSelection(NULL);

#if 0 //def WIDGETS_IME_SUPPORT
	VisualDevice* vd = doc->GetVisualDevice();
	if (vd->GetView())
	{
		if (focus)
			vd->GetView()->GetOpView()->SetInputMethodMode(OpView::IME_MODE_TEXT, NULL);
		else
			vd->GetView()->GetOpView()->AbortInputMethodComposing();
	}
#endif // WIDGETS_IME_SUPPORT
}

void SVGEditable::OnElementRemoved(HTML_Element* elm)
{
	if (elm == m_caret.m_point.elm || elm->IsAncestorOf(m_caret.m_point.elm))
		// Dont let the caret point to the removed element.
		m_caret.Init(FALSE, m_root);
}

#if 0
void SVGEditable::OnElementInserted(HTML_Element* elm)
{
}

void SVGEditable::OnElementDeleted(HTML_Element* elm)
{
	if (elm == m_caret.m_helm)
		// Dont let the caret point to the removed element.
		m_caret.Set(NULL, 0);

	if (!m_root->IsAncestorOf(elm))
		return;
}
#endif // 0

HTML_Element* SVGEditable::FindEditableElement(HTML_Element* from_helm, BOOL forward,
											   BOOL include_current, BOOL text_only /* = FALSE */)
{
	if (!from_helm)
		return NULL;

	HTML_Element* helm = from_helm;
	if (!include_current)
		helm = forward ? helm->NextActual() : helm->PrevActual();
	while (helm)
	{
		if (helm->Type() == HE_TEXT)
			break;

		if (!text_only && helm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG))
			break;

		helm = forward ? helm->NextActual() : helm->PrevActual();
	}

	if (!m_root->IsAncestorOf(helm))
		helm = NULL;

	return helm;
}

BOOL SVGEditable::GetNearestCaretPos(HTML_Element* from_helm, SVGEditPoint& nearest_ep,
									 BOOL forward, BOOL include_current, BOOL text_only /* = FALSE */)
{
	HTML_Element* helm = FindEditableElement(from_helm, forward, include_current, text_only);
	if (!helm)
		return FALSE;

	if (forward || helm->Type() != HE_TEXT)
		nearest_ep.ofs = 0;
	else
		nearest_ep.ofs = helm->GetTextContentLength();

	nearest_ep.elm = helm;
	return TRUE;
}

HTML_Element* SVGEditable::NewTextElement(const uni_char* text, int len)
{
	SVG_DOCUMENT_CLASS* doc = GetDocument();
	if (!doc)
		return NULL;
	HTML_Element *new_elm = NEW_HTML_Element();
	HLDocProfile *hld_profile = doc->GetHLDocProfile();
	if (!new_elm)
		return NULL;
	if (OpStatus::IsMemoryError(new_elm->Construct(hld_profile, text, len)))
	{
		DeleteElement(new_elm);
		return NULL;
	}
	return new_elm;
}

HTML_Element* SVGEditable::NewElement(Markup::Type type)
{
	SVG_DOCUMENT_CLASS* doc = GetDocument();
	if (!doc)
		return NULL;
	HTML_Element *new_elm = NEW_HTML_Element();
	HLDocProfile *hld_profile = doc->GetHLDocProfile();
	if (!new_elm)
		return NULL;
	if (OpStatus::IsMemoryError(new_elm->Construct(hld_profile, NS_IDX_SVG, type,
												   NULL, HE_INSERTED_BY_DOM)))
	{
		DeleteElement(new_elm);
		return NULL;
	}
	new_elm->SetEndTagFound();
	return new_elm;
}

void SVGEditable::SetElementText(HTML_Element* helm, const uni_char* text)
{
	OP_ASSERT(helm->Type() == HE_TEXT);
	SVG_DOCUMENT_CLASS* doc = GetDocument();
	if (!doc)
		return;
	if (OpStatus::IsError(helm->SetText(doc, text, uni_strlen(text))))
		ReportOOM();
}

BOOL SVGEditable::IsStandaloneElement(HTML_Element* helm)
{
	switch(helm->Type())
	{
	case Markup::HTE_TEXT:
	case Markup::SVGE_TBREAK:
	case Markup::SVGE_TREF:
		return TRUE;
	}
	return FALSE;
}

#if 0
BOOL SVGEditable::KeepWhenTidy(HTML_Element* helm)
{
	return FALSE;
}

BOOL SVGEditable::KeepWhenTidyIfEmpty(HTML_Element* helm)
{
	switch(helm->Type())
	{
	case Markup::SVGE_TEXT:
	case Markup::SVGE_TEXTAREA:
	case Markup::SVGE_TSPAN:
	case Markup::SVGE_TEXTPATH:
		return TRUE;
	};
	//if (helm->IsContentEditable())
	//	return TRUE;
	return FALSE;
}

BOOL SVGEditable::IsFriendlyElement(HTML_Element* helm, BOOL include_replaced, BOOL br_is_friendly)
{
	/*if (!include_replaced && IsStandaloneElement(helm) && helm->Type() != HE_TEXT)
		return FALSE;
	if (helm->Type() == HE_BR && !br_is_friendly)
		return FALSE;
	return IsHtmlInlineElm(helm->Type(), m_doc->GetHLDocProfile());*/
	return FALSE;
}
#endif // 0

BOOL SVGEditable::IsEnclosedBy(HTML_Element* helm, HTML_Element* start, HTML_Element* stop)
{
	if (start->Precedes(helm))
	{
		if (!helm->IsAncestorOf(stop))
			return TRUE;
	}
	return FALSE;
}

BOOL SVGEditable::ContainsTextBetween(HTML_Element* start, HTML_Element* stop)
{
	start = (HTML_Element*) start->Next();
	while(start != stop)
	{
		if (start->Type() == HE_TEXT)
			return TRUE;
		start = (HTML_Element*) start->Next();
	}
	return FALSE;
}

// Same as below, but expects offsets excluding whitespace
void SVGEditable::RemoveContentCaret(SVGCaretPoint& start, SVGCaretPoint& stop, BOOL aggressive /* = TRUE */)
{
	SVGEditPoint start_ep = ToEdit(start);
	SVGEditPoint stop_ep = ToEdit(stop);

	RemoveContent(start_ep, stop_ep);

	if (start_ep.elm->GetTextContentLength() == 0)
	{
		if (start_ep.elm->Parent()->FirstChild() == start_ep.elm->Parent()->LastChild())
		{
			start_ep.ofs = 0;
		}
		else if (aggressive)
		{
			// Remove start_elm if empty and if there is another
			// textelement to place the caret on.

			SVGEditPoint nearest_ep;

			// Use FindEditableElement instead of GetNearestCaretPos
			// forward, because we want to include ending BR's.
			if ((nearest_ep.elm = FindEditableElement(start_ep.elm, TRUE, FALSE)) ||
				GetNearestCaretPos(start_ep.elm, nearest_ep, FALSE, FALSE))
			{
				HTML_Element* tmp = start_ep.elm;
				start_ep = nearest_ep;

				// Delete the element and it's parents if they become empty.

				HTML_Element* parent = tmp->Parent();
				DeleteElement(tmp);

				while(!parent->FirstChild() && parent != m_root)
				{
					tmp = parent;
					parent = parent->Parent();

					DeleteElement(tmp);
				}
			}
		}
	}

	// Update caret
	m_caret.Place(ToCaret(start_ep), aggressive, TRUE);
}

void SVGEditable::RemoveContent(SVGEditPoint& start, SVGEditPoint& stop)
{
	// FIX: will crash when start or stop returns something bad. (When
	// selection goes first or last and there is no word is one
	// example)
	OP_ASSERT(start.IsValid() && stop.IsValid());
	OP_ASSERT(start.elm == stop.elm || start.elm->Precedes(stop.elm));

	if (start.elm == stop.elm)
	{
		DeleteTextInElement(start, stop.ofs - start.ofs);
	}
	else
	{
		// Crop first element
		DeleteTextInElement(start, start.elm->GetTextContentLength() - start.ofs);

		// Remove elements between
		HTML_Element* tmp = (HTML_Element *) start.elm->Next();
		while (tmp != stop.elm)
		{
			HTML_Element* next_elm = (HTML_Element *) tmp->Next();

			if (IsStandaloneElement(tmp))
			{
				// Remove all standalone elements. Eventual empty
				// parents will be removed in Tidy if they should.
				DeleteElement(tmp);
			}
			else if (IsEnclosedBy(tmp, start.elm, stop.elm))
			{
				// If a non-standalone element is completly enclosed
				// by selection, we can remove it.
				next_elm = (HTML_Element*) tmp->LastLeaf()->Next();
				DeleteElement(tmp);
			}

			tmp = next_elm;
		}

		// Crop last element
		SVGEditPoint stop_beginning = stop;
		stop_beginning.ofs = 0;
		DeleteTextInElement(stop_beginning, stop.ofs);

		// Tidy up all garbage between start and stop
		//m_edit->Tidy(start_elm, stop_elm, FALSE);

		// Remove stop_elm if empty
		if ((stop.IsText() && stop.elm->GetTextContentLength() == 0) ||
			(!stop.IsText() && IsStandaloneElement(stop.elm)))
		{
			DeleteElement(stop.elm);
			stop.elm = NULL;
		}
	}
}

/* static */
BOOL SVGEditable::IsXMLSpacePreserve(HTML_Element* element)
{
	while (element)
	{
		if (element->HasAttr(XMLA_SPACE, NS_IDX_XML))
			return element->GetBoolAttr(XMLA_SPACE, NS_IDX_XML);

		element = element->ParentActual();
	}
	return FALSE;
}

INT32 SVGEditable::CalculateLeadingWhitespace(HTML_Element* elm)
{
	if (!elm || elm->Type() != HE_TEXT || IsXMLSpacePreserve(elm))
		return 0;

	// Find the previous text element that was not all whitespace
	HTML_Element* prev_actual_text_elm = elm->PrevActual();
	while (prev_actual_text_elm &&
		   ((!prev_actual_text_elm->IsText() &&
			 !prev_actual_text_elm->IsMatchingType(Markup::SVGE_TREF, NS_SVG) &&
			 !prev_actual_text_elm->IsMatchingType(Markup::SVGE_TBREAK, NS_SVG)) ||
			prev_actual_text_elm->Type() == HE_TEXT &&
			SVGUtils::IsAllWhitespace(prev_actual_text_elm->TextContent(),
							prev_actual_text_elm->GetTextContentLength())))
	{
		prev_actual_text_elm = prev_actual_text_elm->PrevActual();
	}

	BOOL prev_kept_trailing_whitespace = FALSE;
	HTML_Element* svgtext_element = elm;
	while(svgtext_element && svgtext_element->Type() == HE_TEXT)
	{
		svgtext_element = svgtext_element->ParentActual();
	}

	HTML_Element* textroot_element = SVGUtils::GetTextRootElement(svgtext_element);
	if (prev_actual_text_elm &&
		textroot_element &&
		(prev_actual_text_elm->IsText() || prev_actual_text_elm->IsMatchingType(Markup::SVGE_TREF, NS_SVG)) &&
		textroot_element->IsAncestorOf(prev_actual_text_elm))
	{
		if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(textroot_element))
			SVGTraversalObject::CalculateSurroundingWhiteSpace(prev_actual_text_elm,
															   doc_ctx, prev_kept_trailing_whitespace);
	}

	const uni_char* text = elm->TextContent();
	while ((*text == ' ' || *text == '\n' || *text == '\r' || *text == '\t') &&
		   (!prev_kept_trailing_whitespace || *(text+1) == ' '))
	{
		text++; // strip off leading whitespace
	}

	return (INT32)(text - elm->TextContent());
}

BOOL SVGEditable::DeleteTextInElement(SVGEditPoint& ep, int len)
{
	if (!ep.IsText() || len <= 0)
		return FALSE;

	OpString str;
	if (OpStatus::IsError(str.Set(ep.elm->TextContent())))
		ReportOOM();

	OP_ASSERT(ep.IsValidTextOffset());
	OP_ASSERT(ep.ofs + len <= ep.elm->GetTextContentLength());

	str.Delete(ep.ofs, len);

	SetElementText(ep.elm, str.CStr());
	return TRUE;
}

BOOL SVGEditable::SplitElement(SVGCaretPoint& cp)
{
	if (!cp.IsText())
		return FALSE;

	SVGEditPoint ep = ToEdit(cp);

	// Don't split if we are not at an edge
	if (ep.ofs != 0 && ep.ofs != ep.elm->GetTextContentLength())
	{
		SVG_DOCUMENT_CLASS* doc = GetDocument();
		if (!doc)
			return FALSE;

		// Create second part element
		const uni_char* second_part = ep.elm->TextContent() + ep.ofs;
		HTML_Element* new_elm = NewTextElement(second_part, uni_strlen(second_part));
		if (!new_elm)
		{
			ReportOOM();
			return FALSE;
		}

		// Delete second part from the first element.
		DeleteTextInElement(ep, ep.elm->GetTextContentLength() - ep.ofs);
		// Insert second part
		new_elm->FollowSafe(doc, ep.elm);

		Invalidate();

		return TRUE;
	}
	return FALSE;
}

void SVGEditable::DeleteElement(HTML_Element* helm, SVGEditable* edit, BOOL check_caret)
{
	if (!helm)
		return;

	SVGEditPoint new_ep;
	BOOL snap_forward = FALSE;

	SVG_DOCUMENT_CLASS* doc = NULL;
	if (edit && edit->GetEditRoot()->IsAncestorOf(helm))
	{
		doc = edit->GetDocument();
		if (check_caret && helm == edit->m_caret.m_point.elm)
		{
			if (edit->GetNearestCaretPos(helm, new_ep, TRUE, FALSE))
				snap_forward = TRUE;
			else if (edit->GetNearestCaretPos(helm, new_ep, FALSE, FALSE))
				snap_forward = FALSE;
			else
				edit->m_caret.Set(SVGCaretPoint());

			OP_ASSERT(!helm->IsAncestorOf(new_ep.elm));
		}
	}
	if (doc && helm->Parent())
	{
		edit->Invalidate();

		HTML_Document *html_doc = doc->GetHtmlDocument();
		if (html_doc && (helm == html_doc->GetHoverHTMLElement() ||
						 helm->IsAncestorOf(html_doc->GetHoverHTMLElement())))
			// Avoid crash when lowlevelapplypseudoclasses
			// traverse the hoverelement when it is not in the
			// document anymore.
			html_doc->SetHoverHTMLElement(NULL);
	}

	helm->Remove(doc);
	if (helm->Clean(doc))
		helm->Free(doc);

	if (new_ep.IsValid())
		edit->m_caret.Place(edit->ToCaret(new_ep), TRUE, snap_forward);
}

BOOL SVGEditable::FindWordStartAndOffset(SVGEditPoint& ep, const uni_char* &word, int& word_ofs,
										 BOOL snap_forward, BOOL ending_whitespace)
{
	if (!ep.IsValid() || ep.ofs < 0)
		return FALSE;

	OpTextFragmentList tflist;
	TempBuffer buffer;
	unsigned textlen = ep.elm->GetTextContentLength();
	RETURN_VALUE_IF_ERROR(buffer.Expand(textlen + 1), FALSE);
	uni_char* text = buffer.GetStorage();
	textlen = ep.elm->GetTextContent(text, textlen+1);

	tflist.Update(text, textlen, /*tparams.packed.rtl == 0 ? FALSE : TRUE*/ FALSE,
				  /*bidi_override*/ FALSE, /*tparams.packed.preserve_spaces*/ FALSE, FALSE,
				  GetDocument(), -1);

	INT32 i;
	INT32 wordcount = tflist.GetNumFragments();

	// Testpage words:
	// Document-Centric Editin      g
	// 0000000012222222 333333      4
	if (snap_forward)
	{
		for(i = 0; i < wordcount; i++)
		{
			OP_TEXT_FRAGMENT* frag = tflist.Get(i);
			WordInfo& wi = frag->wi;
			int word_start = wi.GetStart();

			if (wi.IsCollapsed())
				continue;

			if (ep.ofs <= word_start + (int)wi.GetLength())
			{
				ep.ofs = MAX(ep.ofs, word_start);

				word = ep.elm->TextContent() + word_start;
				word_ofs = ep.ofs - word_start;
				return TRUE;
			}
			else if (ending_whitespace && wi.GetLength() == 0 &&
					 i == wordcount - 1 && ep.ofs <= ep.elm->GetTextContentLength())
			{
				// Special case for ending whitespace.
				if (ep.ofs > word_start + 1)
					return FALSE;

				word = ep.elm->TextContent() + word_start;
				word_ofs = ep.ofs - word_start;
				return TRUE;
			}
		}
	}
	else
	{
		for(i = wordcount - 1; i >= 0; i--)
		{
			OP_TEXT_FRAGMENT* frag = tflist.Get(i);
			WordInfo& wi = frag->wi;
			int word_start = wi.GetStart();

			if (wi.IsCollapsed())
				continue;

			if (ep.ofs >= word_start)
			{
				if (ending_whitespace && wi.GetLength() == 0 &&
					i == wordcount - 1 && ep.ofs <= ep.elm->GetTextContentLength())
				{
					// Special case for ending whitespace.
					ep.ofs = MIN(ep.ofs, word_start + 1);
				}
				else
					ep.ofs = MIN(ep.ofs, word_start + (int)wi.GetLength());

				word = ep.elm->TextContent() + word_start;
				word_ofs = ep.ofs - word_start;
				return TRUE;
			}
		}
	}
	return FALSE;
}

HTML_Element* SVGEditable::FindElementBetweenOfType(HTML_Element* start, HTML_Element* end,
													int type, NS_Type ns)
{
	OP_ASSERT(start && end && (start == end || start->Precedes(end)));

	while (start && start != end && !start->IsMatchingType(type, ns))
		start = start->NextActual();

	if (start == end)
		return NULL;

	return start;
}

HTML_Element* SVGEditable::FindElementBeforeOfType(HTML_Element* helm, Markup::Type type)
{
	helm = helm ? helm->PrevActual() : NULL;
	while(helm && (helm->Type() != type))
		helm = helm->PrevActual();
	return helm;
}

HTML_Element* SVGEditable::FindElementAfterOfType(HTML_Element* helm, Markup::Type type)
{
	helm = helm ? helm->NextActual() : NULL;
	while(helm && (helm->Type() != type))
		helm = helm->NextActual();
	return helm;
}

void SVGEditable::ShowCaret(HTML_Element* element, int offset)
{
	OP_ASSERT(element);
	SVGCaretPoint cp;
	cp.elm = element;
	cp.ofs = offset;
	m_caret.Set(cp);
	m_caret.m_on = TRUE;
	m_caret.Invalidate();
	Invalidate();
}

void SVGEditable::HideCaret()
{
	m_caret.m_on = FALSE;
	m_caret.Invalidate();
	Invalidate();
}

#endif // SVG_SUPPORT_EDITABLE
#endif // SVG_SUPPORT
