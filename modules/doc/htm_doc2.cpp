/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/doc/html_doc.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/caret_painter.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/winman.h"
#include "modules/forms/formvalue.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/selection.h"
#include "modules/locale/locale-enum.h"
#include "modules/windowcommander/src/WindowCommander.h"

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
# include "modules/doc/searchinfo.h"
#endif

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#ifdef DOCUMENT_EDIT_SUPPORT
#include "modules/documentedit/OpDocumentEdit.h"
#endif // DOCUMENT_EDIT_SUPPORT

#include "modules/url/url_tools.h"

#ifdef SDK
# define INVERT_BORDER_SIZE 4
#endif // SDK

#ifdef SVG_SUPPORT
#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_tree_iterator.h"
#include "modules/svg/svg_image.h"
#include "modules/svg/svg_workplace.h"
#endif // SVG_SUPPORT

# include "modules/media/media.h"

#if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
#include "modules/doc/caret_manager.h"
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

void HTML_Document::InvertFormBorderRect(HTML_Element* helm)
{
	OP_ASSERT(navigation_element.GetElm());

	navigation_element_is_highlighted = TRUE;
	image_form_selected = TRUE;
	SetCurrentFormElement(helm);
	InvalidateHighlightArea();
}

HTML_Element* HTML_Document::CalculateElementToHighlight()
{
	HTML_Element *highlight_it = NULL;

#ifdef DOCUMENT_EDIT_SUPPORT
	if (navigation_element.GetElm() && GetVisualDevice()->GetDrawFocusRect())
	{
		if (navigation_element->IsContentEditable() && frames_doc->GetDocumentEdit())
			highlight_it = navigation_element.GetElm();

		if (navigation_element->Type() == HE_IFRAME)
		{
			FramesDocElm *fde = FramesDocElm::GetFrmDocElmByHTML(navigation_element.GetElm());
			if (fde && fde->GetCurrentDoc() && fde->GetCurrentDoc()->GetDesignMode())
				highlight_it = navigation_element.GetElm();

		}
	}
#endif

	if (area_selected && navigation_element.GetElm() && current_area_element.GetElm())
		highlight_it = navigation_element.GetElm();
	else if (image_form_selected && current_form_element.GetElm())
		highlight_it = current_form_element.GetElm();

	// Don't highlight things that aren't in the main document tree
	if (highlight_it && !frames_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(highlight_it))
	{
		highlight_it = NULL;
	}

	return highlight_it;
}

OP_STATUS HTML_Document::DrawHighlight(OpRect *highlight_rects, int num_highlight_rects, const OpRect& highlight_clip_rect)
{
	HTML_Element* highlight_it = CalculateElementToHighlight();

#ifdef SVG_SUPPORT
	// FIXME: The navigation element shouldn't be automatically highlighted.
	// I'm adding a check of VisualDevice::GetDrawFocusRect now, but this
	// should be redesigned and probably removed.
	BOOL maybe_highlight_navigation_element = GetVisualDevice()->GetDrawFocusRect();
	if (navigation_element.GetElm() && maybe_highlight_navigation_element)
	{
		if (navigation_element->GetNsType() == NS_SVG)
			g_svg_manager->UpdateHighlight(GetVisualDevice(), navigation_element.GetElm(), TRUE);
	}
#endif // SVG_SUPPORT

	if (highlight_it)
	{
		// Clear area or image form highlighting

		// FIXME: check if inside a fixed element

		OP_ASSERT(!frames_doc->IsReflowing());

		RETURN_IF_MEMORY_ERROR(GetFramesDocument()->GetLogicalDocument()->GetLayoutWorkplace()->DrawHighlight(highlight_it, highlight_rects, num_highlight_rects, highlight_clip_rect, area_selected ? current_area_element.GetElm() : NULL));
	}

	return OpStatus::OK;
}

void HTML_Document::InvalidateHighlightArea()
{
	if (frames_doc->IsUndisplaying())
		return;

	HTML_Element* highlight_it = CalculateElementToHighlight();

#ifdef SVG_SUPPORT
	// FIXME: The navigation element shouldn't be automatically highlighted.
	// I'm adding a check of VisualDevice::GetDrawFocusRect now, but this
	// should be redesigned and probably removed.
	BOOL maybe_highlight_navigation_element = GetVisualDevice()->GetDrawFocusRect();
	if (navigation_element.GetElm() && maybe_highlight_navigation_element)
	{
		if (navigation_element->GetNsType() == NS_SVG)
			g_svg_manager->UpdateHighlight(GetVisualDevice(), navigation_element.GetElm(), FALSE);
	}
#endif // SVG_SUPPORT

	if (highlight_it)
	{
		OP_ASSERT(frames_doc->GetLogicalDocument());
		OP_ASSERT(!frames_doc->IsReflowing()); // Can't call GetBoxRect during a reflow

		if (frames_doc->IsReflowing())
		{
			// Gah! We need to figure out how to handle this nicely. Would like
			// to make it async but code in for instance ClearFocusAndHighlight
			// depends on the update being done immediately before they
			// change document state
			GetVisualDevice()->UpdateAll(); // better than to leave highlight residue
			return;
		}

		// Clear area or image form highlighting
		GetVisualDevice()->IncludeHighlightInUpdates(TRUE);
		highlight_it->MarkDirty(frames_doc, TRUE, TRUE);
	}
}

#ifndef HAS_NOTEXTSELECTION

OP_BOOLEAN HTML_Document::SelectAll(BOOL select /* = TRUE */, HTML_Element* parent /* = NULL */)
{
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	BOOL had_selection = (GetTextSelection() && !GetTextSelection()->IsEmpty())
		|| !selections.Empty();
#else // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
	BOOL had_selection = (text_selection && !text_selection->IsEmpty());
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

	BOOL clear_focus_and_highlight = parent == NULL; // Don't touch focus if script is selecting a specific element
	frames_doc->ClearDocumentSelection(clear_focus_and_highlight, TRUE, TRUE);

	if (select)
	{
		LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

		if (!parent && logdoc)
		{
			// See http://dvcs.w3.org/hg/editing/raw-file/tip/editing.html#the-selectall-command for the
			// documentation for an unqualified "Select All" command.
			parent = logdoc->GetBodyElm();
			if (!parent)
			{
				parent = logdoc->GetDocRoot();
				if (!parent)
					parent = logdoc->GetRoot();
			}
		}

		if (parent)
		{
			frames_doc->SetTextSelectionPointer(OP_NEW(TextSelection, ()));

			if (GetTextSelection())
				GetTextSelection()->SetNewSelection(frames_doc, parent, TRUE, FALSE, TRUE);
			else
				return OpStatus::ERR_NO_MEMORY;
		}

		return OpBoolean::IS_TRUE;
	}
	else
	{
		if (had_selection)
			return OpBoolean::IS_TRUE;
		else
			return OpBoolean::IS_FALSE;
	}
}

OP_STATUS HTML_Document::SetSelection(const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL end_is_focus)
{
	return frames_doc->SetSelection(start, end, end_is_focus, FALSE);
}

BOOL HTML_Document::GetSelection(SelectionBoundaryPoint &anchor, SelectionBoundaryPoint &focus)
{
#ifdef DOCUMENT_EDIT_SUPPORT
	if (OpDocumentEdit* de = GetFramesDocument()->GetDocumentEdit())
	{
		BOOL ret = de->GetSelection(anchor, focus);
		if (ret)
			return TRUE;
	}
#endif // DOCUMENT_EDIT_SUPPORT
#ifdef SVG_SUPPORT
	if (LogicalDocument* logdoc = GetFramesDocument()->GetLogicalDocument())
	{
		SVGWorkplace* wp = logdoc->GetSVGWorkplace();
		if (wp->HasSelectedText())
		{
			return wp->GetSelection(anchor, focus);
		}
	}
#endif // SVG_SUPPORT

	if (!GetTextSelection())
		return FALSE;
	const SelectionBoundaryPoint* sel_anchor = GetTextSelection()->GetAnchorPoint();
	if (!sel_anchor->GetElement())
		return FALSE;
	anchor = *sel_anchor;
	focus = *GetTextSelection()->GetFocusPoint();
	return TRUE;
}

OP_BOOLEAN HTML_Document::IsWithinSelection(int x, int y)
{
	if (!HasSelectedText())
		return OpBoolean::IS_FALSE;

	OpPoint point(x, y);
	CalculateSelectionRectsObject selection_rects_object(frames_doc, &GetTextSelection()->GetStartSelectionPoint(), &GetTextSelection()->GetEndSelectionPoint(), point);
	selection_rects_object.Traverse(frames_doc->GetDocRoot());
	if (selection_rects_object.IsOutOfMemory())
		return OpStatus::ERR_NO_MEMORY;

	if (selection_rects_object.IsPointContained())
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

OP_STATUS HTML_Document::StartSelection(int x, int y)
{
	frames_doc->ClearDocumentSelection(FALSE, TRUE, TRUE);
	OP_ASSERT(!GetTextSelection());

	// Try to find the point in the document that the user intended to start his selection
	// at. We will (for historical reasons) be a little more forgiving in documentedit
	// and keyboard selection mode so that we are more likely to find a text node. It
	// is quite possible we should always be more forgiving.
	TextSelection* new_selection = OP_NEW(TextSelection, ());
	if (new_selection)
	{
		frames_doc->SetTextSelectionPointer(new_selection);
		frames_doc->Reflow(FALSE); // Reflow may cause documentedit to remove the selection.
		if (GetTextSelection() && OpStatus::IsError(GetTextSelection()->SetCollapsedPosition(frames_doc, LayoutCoord(x), LayoutCoord(y), FALSE, FALSE)))
			frames_doc->ClearDocumentSelection(FALSE, FALSE, TRUE);
	}

#if defined KEYBOARD_SELECTION_SUPPORT || defined DOCUMENT_EDIT_SUPPORT
	if (frames_doc->GetCaretPainter())
	{
		frames_doc->GetCaretPainter()->UpdatePos();
		frames_doc->GetCaretPainter()->UpdateWantedX();
	}
#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT

	return GetTextSelection() ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#ifdef KEYBOARD_SELECTION_SUPPORT
OP_STATUS HTML_Document::SetCollapsedSelection(int x, int y)
{
	frames_doc->ClearDocumentSelection(FALSE, TRUE, TRUE);
	OP_ASSERT(!GetTextSelection());

	// Try to find the point in the document that the user intended to start his selection
	// at. We will (for historical reasons) be a little more forgiving in documentedit
	// and keyboard selection mode so that we are more likely to find a text node. It
	// is quite possible we should always be more forgiving.
	TextSelection* new_selection = OP_NEW(TextSelection, ());
	if (new_selection)
	{
		frames_doc->SetTextSelectionPointer(new_selection);
		if (OpStatus::IsError(new_selection->SetCollapsedPosition(frames_doc, LayoutCoord(x), LayoutCoord(y), TRUE, FALSE)))
			frames_doc->ClearDocumentSelection(FALSE, FALSE, TRUE);

		if (element_with_selection)
			SetElementWithSelection(NULL);
	}

	return GetTextSelection() ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}
#endif // KEYBOARD_SELECTION_SUPPORT

OP_STATUS HTML_Document::MoveSelectionAnchorPoint(int x, int y)
{
	if (GetTextSelection())
	{
		GetTextSelection()->SetAnchorPosition(frames_doc, LayoutCoord(x), LayoutCoord(y));

		if (element_with_selection && GetTextSelection() && !GetTextSelection()->IsEmpty())
			SetElementWithSelection(NULL);

		return OpStatus::OK;
	}

	return StartSelection(x,y);
}

OP_STATUS HTML_Document::MoveSelectionFocusPoint(int x, int y, BOOL end_selection)
{
	if (GetTextSelection())
	{
		GetTextSelection()->SetFocusPosition(frames_doc, LayoutCoord(x), LayoutCoord(y), end_selection);

		if (element_with_selection && GetTextSelection() && !GetTextSelection()->IsEmpty())
			SetElementWithSelection(NULL);
	}

	return OpStatus::OK;
}

OP_STATUS HTML_Document::SelectElement(HTML_Element* elm)
{
	frames_doc->ClearDocumentSelection(FALSE, TRUE, TRUE);
	OP_ASSERT(!GetTextSelection());

	frames_doc->SetTextSelectionPointer(OP_NEW(TextSelection, ()));
	RETURN_OOM_IF_NULL(GetTextSelection());

	GetTextSelection()->SetNewSelection(frames_doc, elm, TRUE, TRUE, TRUE); // TODO: Check if it should do a full selection instead of the visual selection.

	if (element_with_selection && GetTextSelection() && !GetTextSelection()->IsEmpty())
		SetElementWithSelection(NULL);

	return HasSelectedText() ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS HTML_Document::SelectWord(int x, int y)
{
	frames_doc->ClearDocumentSelection(FALSE, TRUE, TRUE);
	OP_ASSERT(!GetTextSelection());

	TextSelection* new_selection = OP_NEW(TextSelection, ());
	RETURN_OOM_IF_NULL(new_selection);

	if (OpStatus::IsError(new_selection->SetCollapsedPosition(frames_doc, LayoutCoord(x), LayoutCoord(y), TRUE, TRUE)))
	{
		OP_DELETE(new_selection);
		return OpStatus::ERR_NO_MEMORY;
	}
	frames_doc->SetTextSelectionPointer(new_selection);

	ExpandSelection(TEXT_SELECTION_WORD);

	if (element_with_selection && GetTextSelection() && !GetTextSelection()->IsEmpty())
		SetElementWithSelection(NULL);

	return OpStatus::OK;
}

#endif // !HAS_NOTEXTSELECTION

void HTML_Document::ClearDocumentSelection(BOOL clear_focus_and_highlight, BOOL clear_search)
{
	if (clear_search && search_selection)
	{
		search_selection->Clear(frames_doc);
		OP_DELETE(search_selection);
		search_selection = NULL;
	}

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
	ClearSelectionElms();
# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT

	// Invert any highlighted area or image form elements

	InvalidateHighlightArea();
	area_selected = FALSE;
	image_form_selected = FALSE;

	if (clear_focus_and_highlight)
		ClearFocusAndHighlight(TRUE, FALSE);

#ifdef DOCUMENT_EDIT_SUPPORT
	if (OpDocumentEdit* de = GetFramesDocument()->GetDocumentEdit())
	{
		de->m_layout_modifier.Unactivate();
	}
#endif // DOCUMENT_EDIT_SUPPORT
}

#ifndef HAS_NOTEXTSELECTION

void HTML_Document::RemoveSearchHits()
{
	if (search_selection)
	{
		search_selection->Clear(frames_doc);
		OP_DELETE(search_selection);
		search_selection = NULL;
	}

# ifdef SEARCH_MATCHES_ALL
	ClearSelectionElms();
# endif // SEARCH_MATCHES_ALL
}

static void AdjustSelection(const uni_char *text, int text_len, int &start_sel, int &end_sel, FormValue::FormValueType type)
{
	if (type == FormValue::VALUE_TEXTAREA)
		for (int i = 0; i < end_sel && i < text_len; i++)
			if (text[i] == '\r')
			{
				if (i < start_sel)
					start_sel++;
				if (i < end_sel)
					end_sel++;
			}
}

BOOL HTML_Document::GetSelectedText(uni_char *buf, long buf_len, BOOL include_element_selection /* = FALSE */)
{
	OP_ASSERT(buf_len >= 1 || !"Not even room for a trailing NULL?");

	if (include_element_selection)
	{
		HTML_Element* elm_with_internal_selection = GetElementWithSelection();
		if (elm_with_internal_selection && elm_with_internal_selection->IsFormElement())
		{
			INT32 start_sel, end_sel;
			FormValue* value = elm_with_internal_selection->GetFormValue();
			value->GetSelection(elm_with_internal_selection, start_sel, end_sel);
			if (start_sel == end_sel)
				return FALSE;

			OpString full_text; // This is very inefficient if it's a huge textarea with one selected word
			if (OpStatus::IsError(value->GetValueAsText(elm_with_internal_selection, full_text)))
				return FALSE;

			int full_text_len = full_text.Length();

			AdjustSelection(full_text.CStr(), full_text_len, start_sel, end_sel, value->GetType());

			int start_text = MIN(full_text_len, start_sel);
			int end_text = MIN(full_text_len, end_sel);
			int sel_len = end_text - start_text;
			OP_ASSERT(buf_len >= sel_len+1); // Too small buffer, truncated selection.
			sel_len = MIN(sel_len, buf_len-1);
			op_memcpy(buf, full_text.CStr()+start_text, sel_len*sizeof(uni_char));
			buf[sel_len] = '\0';
			return TRUE;
		}
#ifdef SVG_SUPPORT
		else if (elm_with_internal_selection && elm_with_internal_selection->GetNsType() == NS_SVG)
		{
			TempBuffer tempbuf;

			if (OpStatus::IsError(tempbuf.Expand(buf_len)))
				return FALSE;

			if (g_svg_manager->GetTextSelection(elm_with_internal_selection, tempbuf))
			{
				OP_ASSERT(static_cast<int>(tempbuf.Length()) <= buf_len);
				buf_len = MIN(static_cast<int>(tempbuf.Length()), buf_len - 1);
				op_memcpy(buf, tempbuf.GetStorage(), buf_len * sizeof(uni_char));
				buf[buf_len] = '\0';

				return TRUE;
			}
		}
#endif // SVG_SUPPORT
	}

	if (GetTextSelection())
	{
		GetTextSelection()->GetSelectionAsText(frames_doc, buf, buf_len);
		return TRUE;
	}

	return FALSE;
}

BOOL HTML_Document::HasSelectedText(BOOL include_element_selection /* = FALSE */)
{
	if (include_element_selection)
	{
		HTML_Element* elm_with_internal_selection = GetElementWithSelection();
		if (elm_with_internal_selection && elm_with_internal_selection->IsFormElement())
		{
			INT32 start_sel, end_sel;
			elm_with_internal_selection->GetFormValue()->GetSelection(elm_with_internal_selection, start_sel, end_sel);
			return start_sel != end_sel; // Empty selection or not?
		}
#ifdef SVG_SUPPORT
		else if (elm_with_internal_selection && elm_with_internal_selection->GetNsType() == NS_SVG)
		{
			unsigned length = 0;
			return g_svg_manager->GetTextSelectionLength(elm_with_internal_selection, length) && length > 0;
		}
#endif // SVG_SUPPORT
	}

	if (GetTextSelection())
		return !GetTextSelection()->IsEmpty();

	return FALSE;
}

long HTML_Document::GetSelectedTextLen(BOOL include_element_selection /* = FALSE */)
{
	if (include_element_selection)
	{
		HTML_Element* elm_with_internal_selection = GetElementWithSelection();
		if (elm_with_internal_selection && elm_with_internal_selection->IsFormElement())
		{
			INT32 start_sel, end_sel;
			FormValue* value = elm_with_internal_selection->GetFormValue();
			value->GetSelection(elm_with_internal_selection, start_sel, end_sel);
			FormValue::FormValueType type = value->GetType();
			if (type == FormValue::VALUE_TEXTAREA)
			{
				OpString full_text; // This is very inefficient if it's a huge textarea with one selected word
				if (OpStatus::IsError(value->GetValueAsText(elm_with_internal_selection, full_text)))
					return 0;

				int full_text_len = full_text.Length();
				AdjustSelection(full_text.CStr(), full_text_len, start_sel, end_sel, value->GetType());
			}
			return end_sel - start_sel;
		}
#ifdef SVG_SUPPORT
		else if (elm_with_internal_selection && elm_with_internal_selection->GetNsType() == NS_SVG)
		{
			unsigned length;
			if (g_svg_manager->GetTextSelectionLength(elm_with_internal_selection, length))
				return length;
		}
#endif // SVG_SUPPORT
	}

	if (GetTextSelection())
		return GetTextSelection()->GetSelectionAsText(frames_doc, NULL, 0);

	return 0;
}

void HTML_Document::ExpandSelection(TextSelectionType selection_type)
{
	if (GetTextSelection() && GetTextSelection()->GetStartElement())
		GetTextSelection()->ExpandSelection(frames_doc, selection_type);
}
#endif // !HAS_NOTEXTSELECTION

void HTML_Document::RemoveFromSearchSelection(HTML_Element* element)
{
	if (search_selection)
	{
		search_selection->RemoveElement(element);

		if (search_selection->GetSelectedFormObject() == element->GetFormObject())
			search_selection->SetFormObjectHasSelection(NULL);
	}
}

void HTML_Document::RemoveLayoutCacheFromSearchHits(HTML_Element* element)
{
	if (search_selection)
	{
		if (search_selection->GetSelectedFormObject() == element->GetFormObject())
			search_selection->SetFormObjectHasSelection(NULL);
	}
}

#ifndef HAS_NO_SEARCHTEXT

OP_BOOLEAN HTML_Document::SearchText(const uni_char* txt, int len,
                                     BOOL forward, BOOL match_case,
                                     BOOL words, BOOL next, BOOL wrap,
                                     BOOL only_links, int &left_x,
                                     long &top_y, int &right_x,
                                     long &bottom_y)
{
	HTML_Element* doc_root = frames_doc->GetDocRoot();
	if (doc_root && doc_root->GetLayoutBox())
	{
		SearchTextObject search_object(frames_doc, txt, forward, match_case, words, !next, only_links);

		if (search_object.Init())
		{
			BOOL ret_val = FALSE;

			if (GetTextSelection() && !GetTextSelection()->IsEmpty())
			{
				// continue search from selected text

				if (!search_selection)
				{
					search_selection = OP_NEW(TextSelection, ());

					if (!search_selection)
						return OpStatus::ERR_NO_MEMORY;
				}

				search_selection->Copy(GetTextSelection());
			}

			frames_doc->ClearDocumentSelection(TRUE, FALSE, TRUE);

			if (search_selection)
			{
				frames_doc->SetTextSelectionPointer(OP_NEW(TextSelection, ()));

				if (!GetTextSelection())
					return OpStatus::ERR_NO_MEMORY;

				GetTextSelection()->Copy(search_selection);
			}

			if (GetTextSelection() && !GetTextSelection()->IsEmpty())
				search_object.SetStartPoint(GetTextSelection()->GetStartSelectionPoint());

			// See if we have a formobject we should start from.
			FormObject* start_form_object = NULL;

			if (GetTextSelection() && GetTextSelection()->GetSelectedFormObject())
				start_form_object = GetTextSelection()->GetSelectedFormObject();
			else
				if (frames_doc->current_focused_formobject)
					start_form_object = frames_doc->current_focused_formobject;
				else
					if (frames_doc->unfocused_formelement)
						start_form_object = frames_doc->unfocused_formelement->GetFormObject();

			search_object.SetStartFormObject(start_form_object);
			search_object.Traverse(doc_root);

			// Unselect previous selection in formobject
			FormObject* form_object = search_object.GetMatchFormObject();
			if (GetTextSelection())
			{
				FormObject* old_form_object = GetTextSelection()->GetSelectedFormObject();

				if (old_form_object && old_form_object != form_object)
					old_form_object->GetWidget()->SelectNothing();
				//frames_doc->unfocused_formobject = NULL;
			}

			if (search_object.GetTextFoundInForm())
			{
				frames_doc->ClearDocumentSelection(TRUE, FALSE, TRUE);

				frames_doc->SetTextSelectionPointer(OP_NEW(TextSelection, ()));

				if (GetTextSelection())
				{
					GetTextSelection()->SetFormObjectHasSelection(form_object);

					// Get this object focused when the document gets the focus.
					// (Also works as a hack to continue next search from inside the formobject)
					if (frames_doc->current_focused_formobject == NULL)
						frames_doc->unfocused_formelement = form_object->GetHTML_Element();

					SEARCH_TYPE type = forward ? SEARCH_FROM_BEGINNING : SEARCH_FROM_END;
					if (start_form_object == form_object)
						type = SEARCH_FROM_CARET;
					BOOL found = form_object->GetWidget()->SearchText(txt, len, forward, match_case, words, type, TRUE);
					OP_ASSERT(found); // since GetTextFoundInForm returned TRUE
					if (found)
					{
						RECT rect;
						if (frames_doc->GetLogicalDocument()->GetBoxRect(form_object->GetHTML_Element(), BOUNDING_BOX, rect))
						{
							left_x = rect.left;
							top_y = rect.top;
							right_x = rect.right;
							bottom_y = rect.bottom;

							ret_val = TRUE;
						}
						// else invisible search hit. What to do?
					}
				}
				else
					return OpStatus::ERR_NO_MEMORY;
			}
			else if (search_object.GetTextFound())
			{
				frames_doc->ClearDocumentSelection(TRUE, FALSE, TRUE);

				frames_doc->SetTextSelectionPointer(OP_NEW(TextSelection, ()));

				if (GetTextSelection())
				{
					GetTextSelection()->SetNewSelection(frames_doc, search_object.GetStart(), search_object.GetEnd(), TRUE, FALSE, TRUE);

					const RECT& bounding_rect = GetTextSelection()->GetBoundingRect();
					left_x = bounding_rect.left;
					top_y = bounding_rect.top;
					right_x = bounding_rect.right;
					bottom_y = bounding_rect.bottom;

					ret_val = TRUE;
				}
				else
					return OpStatus::ERR_NO_MEMORY;
			}
			else if (wrap && (GetTextSelection() && !GetTextSelection()->IsEmpty()))
			{
				frames_doc->ClearDocumentSelection(TRUE, FALSE, TRUE);
				if (search_selection)
				{
					search_selection->Clear(frames_doc);
					OP_DELETE(search_selection);
					search_selection = NULL;
				}
				frames_doc->unfocused_formelement = NULL;
				return SearchText(txt, len, forward, match_case, words, next, FALSE, only_links, left_x, top_y, right_x, bottom_y);
			}

			if (!search_selection)
			{
				search_selection = OP_NEW(TextSelection, ());

				if (!search_selection)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (GetTextSelection())
				search_selection->Copy(GetTextSelection());

			return ret_val ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
		}
	}

	return OpBoolean::IS_FALSE;
}
#endif // !HAS_NO_SEARCHTEXT

int CountTextLen(HTML_Element *elm)
{
	if (elm->Type() == HE_TEXT)
		return elm->GetTextContentLength();

	int ret_val = 0;
	HTML_Element *child = elm->FirstChild();
	while (child)
	{
		ret_val += CountTextLen(child);
		child = child->Suc();
	}
	return ret_val;
}

BOOL HTML_Document::HighlightNextElm(HTML_ElementType lower_tag, HTML_ElementType upper_tag, BOOL forward, BOOL all_anchors /*=FALSE*/)
{
	BOOL get_first_visible = navigation_element.GetElm() == NULL;

	frames_doc->ClearDocumentSelection(FALSE, FALSE, TRUE);

	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot() && logdoc->GetRoot()->GetLayoutBox())
	{
		RECT element_rect;
		BOOL first_pass = TRUE;
		BOOL include_atag = (lower_tag <= HE_A && upper_tag >= HE_A) || all_anchors;
		BOOL atag_only = (lower_tag == HE_A && upper_tag == HE_A) && !all_anchors;
		BOOL headings_only = lower_tag == HE_H1 && upper_tag == HE_H6;
		HTML_Element *candidate_elm = navigation_element.GetElm();
		HTML_Element *candidate_area = current_area_element.GetElm();
		OpRect visual_viewport = GetFramesDocument()->GetVisualViewport();

		if (candidate_elm && candidate_elm->GetLayoutBox())
		{
			// Check if 'candidate_elm' is in view

			if (logdoc->GetBoxRect(candidate_elm, BOUNDING_BOX, element_rect) &&
				(element_rect.bottom <= visual_viewport.y || visual_viewport.y + visual_viewport.height <= element_rect.top))
			{
				// 'candidate_elm' is not in view so start with first visible element

				candidate_elm = NULL;
				candidate_area = NULL;
				area_selected = FALSE;
				get_first_visible = TRUE;
			}
		}

		// Reset 'candidate_area' if we are not interested in links

		if (candidate_area && !include_atag)
			candidate_area = NULL;

		if (candidate_area)
		{
			// Get next area within same map element

			HTML_Element* map_element = candidate_area->Parent();
			while (map_element && !map_element->IsMatchingType(HE_MAP, NS_HTML))
				map_element = map_element->Parent();

			candidate_area = map_element ? candidate_area->GetNextLinkElementInMap(forward, map_element) : NULL;
		}

		// Remember what was previous selected element ...

		HTML_Element* previous_element = navigation_element.GetElm();

		// ... and look for next

		if (candidate_elm && !candidate_area)
			candidate_elm = forward ? candidate_elm->NextActualStyle() : candidate_elm->PrevActualStyle();

		BOOL search_parents_first = FALSE;
		HTML_Element* search_sideways_start_element = NULL;
		if (!candidate_elm)
		{
			if (get_first_visible)
			{
				FirstTextObject first_text(frames_doc,
										   LayoutCoord(visual_viewport.x),
										   LayoutCoord(visual_viewport.y));
				first_text.Traverse(logdoc->GetRoot());
				if (first_text.HasNearestBoundaryPoint())
				{
					SelectionBoundaryPoint point = first_text.GetNearestBoundaryPoint();
					int dummy_offset;
					TextSelection::ConvertPointToOldStyle(point, candidate_elm, dummy_offset);
				}

				if (candidate_elm)
				{
					search_parents_first = TRUE;
					search_sideways_start_element = candidate_elm;
				}
			}

			if (!candidate_elm)
			{
				candidate_elm = forward ? static_cast<HTML_Element*>(logdoc->GetRoot()->FirstLeaf()) : logdoc->GetRoot()->LastLeaf();
				while (candidate_elm && !candidate_elm->IsIncludedActualStyle())
					candidate_elm = forward ? candidate_elm->NextActualStyle() : candidate_elm->PrevActualStyle();
			}
		}

		if (candidate_elm && !candidate_area)
		{
			while (candidate_elm)
			{
				HTML_ElementType element_type = candidate_elm->Type();
				NS_Type element_ns = candidate_elm->GetNsType();

				if ((element_ns != NS_HTML || (element_ns == NS_HTML && element_type != HE_MAP && element_type != HE_AREA))
					&& !candidate_elm->GetIsPseudoElement())
				{
					BOOL show_usemap = FALSE;
					if (element_ns == NS_HTML && (element_type == HE_OBJECT || element_type == HE_IMG || element_type == HE_INPUT))
					{
						// Check if candidate_elm is an usemap image

						URL* usemap_url = candidate_elm->GetUrlAttr(ATTR_USEMAP, NS_IDX_HTML, logdoc);
						show_usemap = include_atag && usemap_url && !usemap_url->IsEmpty() && show_img;
						if (show_usemap && candidate_elm->GetLayoutBox() && candidate_elm->GetLayoutBox()->IsContentImage())
						{
							ImageContent *content = (ImageContent*)candidate_elm->GetLayoutBox()->GetContent();
							if (content && content->IsUsingAltText())
								show_usemap = FALSE;
						}

						if (show_usemap)
						{
							// Find next area element to use - if any.

							HTML_Element* map_element = logdoc->GetNamedHE(usemap_url->UniRelName());
							if (map_element)
								candidate_area = map_element->GetNextLinkElementInMap(forward, map_element);

							if (candidate_area)
								break;
						}
					}

					if (!show_usemap)
					{
						HTML_Element* element = NULL;

						if (all_anchors)
						{
							// FIXME: add css links here too
							if (candidate_elm->GetAnchor_HRef(frames_doc))
								element = candidate_elm;
						}
						else if (headings_only && element_ns == NS_HTML)
						{
							if (element_type >= HE_H1 && element_type <= HE_H6)
								element = candidate_elm;
							else if (element_type != HE_TEXT)
							{
								Head prop_list;
								short body_font_size = 0;
								LayoutProperties* lprops = NULL;

								HTML_Element *body_elm = logdoc->GetBodyElm();
								if (body_elm)
								{
									if (body_elm->Type() == HE_TEXT)
										body_elm = body_elm->Parent();

									lprops = LayoutProperties::CreateCascade(body_elm, prop_list, logdoc->GetHLDocProfile());
									body_font_size = lprops ? lprops->GetProps()->font_size : 0;
									prop_list.Clear();
								}

								lprops = LayoutProperties::CreateCascade(candidate_elm, prop_list, logdoc->GetHLDocProfile());
								int text_len = CountTextLen(candidate_elm);
								if (lprops && lprops->GetProps()->font_size > body_font_size && text_len > 3 && text_len < 80
									&& candidate_elm->Type() != HE_OPTION && !candidate_elm->IsFormElement())
									element = candidate_elm;

								prop_list.Clear();
							}
						}
						else
							if (element_type >= lower_tag && element_type <= upper_tag)
								element = candidate_elm;

						if (element && element != previous_element && (!atag_only || element->GetAnchor_HRef(frames_doc)))
						{
							// This is a matching element that is different from previous
							BOOL already_highlighted = element && previous_element
								&& ((forward && element->ParentActualStyle() == previous_element && element->SucActualStyle() == element->PredActualStyle())
									|| (!forward && previous_element->ParentActualStyle() == element && previous_element->SucActualStyle() == previous_element->PredActualStyle()));

							if (!already_highlighted && HighlightElement(element))
							{
								break;
							}
						}
					}
				}

				// Look for next element

				if (search_parents_first)
				{
					candidate_elm = candidate_elm->ParentActualStyle();
					if (!candidate_elm)
					{
						candidate_elm = search_sideways_start_element;
						search_parents_first = FALSE;
					}
				}
				else
				{
					candidate_elm = forward ? candidate_elm->NextActualStyle() : candidate_elm->PrevActualStyle();

					if (!candidate_elm && first_pass)
					{
						first_pass = FALSE;
						if (forward)
							candidate_elm = logdoc->GetRoot()->FirstChildActualStyle();
						else
						{
							candidate_elm = logdoc->GetRoot()->LastLeaf();
							while (candidate_elm && !candidate_elm->IsIncludedActualStyle())
								candidate_elm = candidate_elm->PrevActualStyle();
						}
					}
				}
			}
		}

		// We have to set the candidate_elm to highlight the area
		if (candidate_area)
			candidate_elm = candidate_area;

		// HighlightElement() will set both current_element and current_area_element
		if (candidate_elm)
		{
			HighlightElement(candidate_elm);
			return TRUE;
		}
	}

	// Clear any previously set highlight by spatnav if we couldn't find any
	// candidate. Fix for bug 250129
	frames_doc->ClearDocumentSelection(TRUE, FALSE, TRUE);

	return FALSE;
}

#ifdef SAVE_DOCUMENT_AS_TEXT_SUPPORT
OP_STATUS HTML_Document::SaveDocAsText(UnicodeOutputStream* stream, const uni_char* fname /* = NULL */, const char *force_encoding /* = NULL */)
{
	URL url = frames_doc->GetURL();
	if (fname && url.ContentType() == URL_TEXT_CONTENT) // No need to try to format text
	{
		unsigned long err = 0;
		err = url.PrepareForViewing(TRUE);
		if (err == MSG_OOM_CLEANUP)
			return OpStatus::ERR_NO_MEMORY;
		if ((err = url.SaveAsFile(fname)) == 0)
		{
			return OpStatus::OK;
		}
		else
		{
			OpString msg;
			if (OpStatus::IsSuccess(g_languageManager->GetString(ConvertUrlStatusToLocaleString(err), msg)))
			{
				WindowCommander *wic(GetWindow()->GetWindowCommander());
				URL url(frames_doc->GetURL());
				wic->GetDocumentListener()->OnGenericError(wic,
					url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr(),
					msg.CStr(), url.GetAttribute(URL::KInternalErrorString).CStr());
			}
			return ConvertUrlStatusToOpStatus(err);
		}
	}

	Window *window = frames_doc ? frames_doc->GetWindow() : NULL;

	//Charset will already be set on a UnicodeOutputStream
	char charset[24] = "";
	if (!stream)
	{
		if (force_encoding && *force_encoding &&
			!strni_eq(force_encoding, "AUTODETECT-", 11))
		{
			// Window's forced encoding is used if defined,
			// and not an autodetection setting.
			op_strncpy(charset, force_encoding, 24);
			charset[23] = '\0';
		}
		else
		{
			// Otherwise use URL's encoding, if available.
			const char *mime_charset = url.GetAttribute(URL::KMIME_CharSet).CStr();
			if (mime_charset)
			{
				op_strncpy(charset, mime_charset, 24);
				charset[23] = '\0';
			}
		}
		if (!charset[0])
		{
			// If that fails, get encoding from content
			URL_DataDescriptor *url_dd = url.GetDescriptor(NULL, TRUE, TRUE, TRUE, window);
			if (!url_dd || url_dd->Init(FALSE, NULL) == OpStatus::ERR_NO_MEMORY)
			{
				OP_DELETE(url_dd);
				return OpStatus::ERR_NO_MEMORY;
			}
			BOOL more = FALSE;
#ifdef OOM_SAFE_API
			TRAPD(err,url_dd->RetrieveDataL(more));
			if (OpStatus::IsMemoryError(err))
			{
				OP_DELETE(url_dd);
				return OpStatus::ERR_NO_MEMORY;
			}
#else
			url_dd->RetrieveData(more);//FIXME:OOM Error handling needed
#endif //OOM_SAFE_API
			const char *dd_charset = url_dd->GetCharacterSet();
			if (dd_charset)
			{
				op_strncpy(charset, url_dd->GetCharacterSet(), 24);
				charset[23] = '\0';
			}
			OP_DELETE(url_dd);
		}
		if (!charset[0])
		{
			// And if *that* failed, use the default encoding
			op_strncpy(charset, g_pcdisplay->GetDefaultEncoding(), 24);
			charset[23] = '\0';
		}
	}

	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot())
	{
		OP_STATUS status;
		UnicodeOutputStream* out = stream;

		//If we don't have a stream, create one
		if (!stream)
		{
			out = OP_NEW(UnicodeFileOutputStream, ());
			if (!out)
				return OpStatus::ERR_NO_MEMORY;

			status = ((UnicodeFileOutputStream*)out)->Construct(fname, charset);

			if (OpStatus::IsError(status))
			{
				OP_DELETE(out);
				return status;
			}
		}

		//Write to the stream
		status = logdoc->WriteAsText(out, g_pcdoc->GetIntegerPref(PrefsCollectionDoc::TxtCharsPerLine, frames_doc->GetHostName()));

		//If we have created a stream, delete it
		if (!stream)
		{
			OP_DELETE(out);
		}

		return status;
	}

	return OpStatus::OK;
}
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

BOOL HTML_Document::GetSelectionBoundingRect(int &x, int &y, int &width, int &height)
{
	if (GetTextSelection())
	{
		RECT bounding_rect = GetTextSelection()->GetBoundingRect();
		x = bounding_rect.left - GetVisualDevice()->GetRenderingViewX();
		y = bounding_rect.top - GetVisualDevice()->GetRenderingViewY();
		width = bounding_rect.right - bounding_rect.left;
		height = bounding_rect.bottom - bounding_rect.top;
		return TRUE;
	}
	else
		return FALSE;
}

BOOL HTML_Document::NextTabElm(BOOL forward)
{
	BOOL found = FALSE, quit = FALSE, wrapped = FALSE, take_first = FALSE;
	LogicalDocument *logdoc = GetFramesDocument()->GetLogicalDocument();
	if (!logdoc)
		return FALSE;
	HTML_Element *he = GetFocusedElement();

	if (he)
	{
		FormObject* he_form_obj = he->GetFormObject();
		if (he_form_obj && !he_form_obj->IsAtLastInternalTabStop(forward))
		{
			// Let it tab internally first
			he_form_obj->FocusNextInternalTabStop(forward);
			return TRUE;
		}

#ifdef MEDIA_SUPPORT
		// Let media controls tab internally
		Media* media_elm = he->GetMedia();
		if (media_elm && media_elm->IsFocusable())
		{
			if (media_elm->FocusNextInternalTabStop(forward))
				return TRUE;
		}
#endif // MEDIA_SUPPORT
	}

	if (!he)
		he = navigation_element.GetElm();

	HTML_Element *candidate = NULL;
	INT current_tabindex, candidate_tabindex;

#ifdef _WML_SUPPORT_
	// All _WML_SUPPORT_ ifdefs in this method is to ensure that the
	// tab navigation always stays inside the current wml card.
	BOOL is_wml = GetFramesDocument() && GetFramesDocument()->GetHLDocProfile()
		&& GetFramesDocument()->GetHLDocProfile()->IsWml()
		&& GetFramesDocument()->GetHLDocProfile()->WMLGetCurrentCard();
#endif //_WML_SUPPORT_

	if (!he)
	{
#ifdef _WML_SUPPORT_
		if (is_wml)
			he = GetFramesDocument()->GetHLDocProfile()->WMLGetCurrentCard();
		else
#endif //_WML_SUPPORT_
			he = GetFramesDocument()->GetLogicalDocument()->GetRoot();
		current_tabindex = 1;
	}
	else
	{
		current_tabindex = he->GetTabIndex();
		if (current_tabindex < 0)
			current_tabindex = 0;
	}

	HTML_Element* start = he;

#ifdef SVG_SUPPORT
	if (start->GetNsType() == NS_SVG)
	{
		found = g_svg_manager->HasNavTargetInDirection(start, forward ? 270 : 90, 2, candidate);
		if (!found)
		{
			SVGTreeIterator* navigator = NULL;
			if (OpStatus::IsSuccess(g_svg_manager->GetNavigationIterator(start, NULL, NULL, &navigator)))
			{
				candidate = forward ? navigator->Next() : navigator->Prev();
				if (candidate)
					found = TRUE;
				g_svg_manager->ReleaseIterator(navigator);
			}
			// If OOM was encountered, rely on the IsFocusableElement() test below,
			// since it will give a decent result in most cases.
		}
	}
#endif // SVG_SUPPORT

	candidate_tabindex = forward ? INT_MAX : -1;

	while (he && !found)
	{
		if (forward)
		{
			HTML_Element* next_he = he->NextActual();
			if (next_he
#ifdef _WML_SUPPORT_
				&& (!is_wml || !next_he->IsMatchingType(WE_CARD, NS_WML) || next_he->GetIsPseudoElement())
#endif //_WML_SUPPORT_
				)
			{
				he = next_he;
			}
			else if (!wrapped && !quit)
			{
#ifdef _WML_SUPPORT_
				if (is_wml)
					he = GetFramesDocument()->GetHLDocProfile()->WMLGetCurrentCard();
				else
#endif // _WML_SUPPORT_
				{
					while (HTML_Element *p = he->ParentActual())
						he = p;
				}

				wrapped = TRUE;
				take_first = TRUE;
			}
			else break;
		}
		else
		{
			HTML_Element *prev;
			if ((prev = he->PrevActual())
#ifdef _WML_SUPPORT_
			   && (!is_wml || !he->IsMatchingType(WE_CARD, NS_WML) || he->GetIsPseudoElement())
#endif // _WML_SUPPORT_
				)
				he = prev;
			else if (!wrapped && !quit)
			{
				while (HTML_Element *lc = he->LastChildActual())
					he = lc;
				wrapped = TRUE;
				take_first = TRUE;
			}
			else break;
		}

		if (he == start && !candidate && wrapped)
		{
			current_tabindex = forward ? 0 : INT_MAX;
			wrapped = FALSE;
			quit = TRUE;

#ifdef _WML_SUPPORT_
			if (is_wml)
				he = GetFramesDocument()->GetHLDocProfile()->WMLGetCurrentCard();
			else
#endif // _WML_SUPPORT_
			{
				while (HTML_Element *p = he->ParentActual())
					he = p;
			}

			if (!forward)
			{
				while (HTML_Element *lc = he->LastChild())
					he = lc;
			}
		}

		if (he)
		{
#ifdef SVG_SUPPORT
			if(he->GetNsType() == NS_SVG)
			{
				found = g_svg_manager->IsFocusableElement(GetFramesDocument(), he);
				if(found)
				{
					candidate = he;
				}
			}
			else
#endif // SVG_SUPPORT
			if(he->GetNsType() == NS_HTML)
			{
				// This returns a calculated tabindex depending on the
				// element, which is identical to DOM's tabIndex property.
				int this_tabindex = he->GetTabIndex(GetFramesDocument());

				switch (he->Type())
				{
#ifdef DOCUMENT_EDIT_SUPPORT
				default: // Lots of elements can be reached by tabbing if we have document edit enabled
#endif // DOCUMENT_EDIT_SUPPORT
				case HE_INPUT:
				case HE_TEXTAREA:
				case HE_BUTTON:
#ifdef _SSL_SUPPORT_
				case HE_KEYGEN:
#endif
				case HE_SELECT:
				case HE_A:
					/* Skip radiobuttons in groups where one is selected, except for the selected one. */
					if (this_tabindex >= 0)
					{
						// Filter out things with a good tabIndex
						// but that still are not good to tab to
						if (he->GetInputType() == INPUT_RADIO &&
							!FormManager::IsSelectedRadioInGroup(this, he))
							this_tabindex = -1;
						else if (he->GetInputType() == INPUT_HIDDEN)
							this_tabindex = -1;
						else if (he->Type() == HE_A)
						{
#if 1	// Disabling this code block makes us compatible with MSIE
		// and Mozilla ("Other browsers") but it makes some people go
		// nuts since there are too many links on pages.
		// See bug 248617 and duplicates. Now we only
		// navigate to links with explicit tabindex.
							if (!he->HasAttr(ATTR_TABINDEX))
								this_tabindex = -1;
#endif // 1
							if (this_tabindex >= 0 && !logdoc->GetLayoutWorkplace()->IsRendered(he)) // Don't include invisible links
								this_tabindex = -1;
						}
						else
						{
							if (!he->GetLayoutBox() || !logdoc->GetLayoutWorkplace()->IsRendered(he) || he->IsDisabled(GetFramesDocument()))
								this_tabindex = -1; // Don't tab to invisible elements
						}
					}
					break;
				case HE_AREA:
					if (this_tabindex >= 0)
					{
						// Check if the img-element this area belongs to is visible
						HTML_Element *map = he->ParentActual();
						while (map && map->Type() != HE_MAP)
							map = map->ParentActual();

						HTML_Element *usemap_elm = FindAREAObjectElement(map, frames_doc->GetLogicalDocument()->GetRoot());
						if (!usemap_elm || !usemap_elm->GetLayoutBox() || !logdoc->GetLayoutWorkplace()->IsRendered(usemap_elm))
							this_tabindex = -1;
					}
					break;
				}

				if (this_tabindex >= 0 &&
					(!wrapped || this_tabindex != current_tabindex) &&
					(this_tabindex == current_tabindex ||
					(forward && (this_tabindex > current_tabindex && (this_tabindex < candidate_tabindex || (take_first && this_tabindex == candidate_tabindex)))) ||
					(!forward && (this_tabindex < current_tabindex && (this_tabindex > candidate_tabindex || (take_first && this_tabindex == candidate_tabindex))))))
				{
					take_first = FALSE;
					candidate = he;
					candidate_tabindex = this_tabindex;

					if (this_tabindex == current_tabindex)
						found = TRUE;
				}

			}
		}
	}

	if (candidate)
	{
		BOOL clear_textselection = TRUE;
#if defined(DOCUMENT_EDIT_SUPPORT) || defined(KEYBOARD_SELECTION_SUPPORT)
		if (frames_doc->GetCaretPainter())
		{
			HTML_Element *caret_elm = frames_doc->GetCaret()->GetElement();
			if (candidate->IsAncestorOf(caret_elm))
				clear_textselection = FALSE;
		}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

		BOOL applied_focus = FocusElement(candidate, FOCUS_ORIGIN_KEYBOARD, clear_textselection);
		if (applied_focus)
		{
			FramesDocument* frames_doc = GetFramesDocument();
			AffinePos transform;
			RECT rect;
			Box* b = candidate->GetLayoutBox();
			if (b && b->GetRect(frames_doc, CONTENT_BOX, transform, rect))
			{
				WindowCommander *win_com = GetWindow()->GetWindowCommander();
				OpDocumentListener* listener = win_com->GetDocumentListener();
				OpPoint position;
				position.x = rect.left + (rect.right - rect.left) / 2;
				position.y = rect.top  + (rect.bottom - rect.top) / 2;
				transform.Apply(position);
				OpAutoPtr<OpDocumentContext> context(frames_doc->CreateDocumentContextForDocPos(position, candidate));

				// What to do on OOM?
				if (context.get())
					listener->OnTabFocusChanged(win_com, *context.get());
			}
		}

		// For elements with internal tab stops we want to return to
		// the first/last internal tab stop if the element is
		// refocused.
		if (applied_focus || focused_element.GetElm() == candidate)
		{
			// Move focus to first or last internal tab stop depending on forward.
			if (FormObject *he_form_obj = candidate->GetFormObject())
			{
				he_form_obj->FocusInternalEdge(forward);

				applied_focus = TRUE;
			}
#ifdef MEDIA_SUPPORT
			// Move internal media controls focus to the first or last internal tab stop.
			else if (Media* media_elm = he->GetMedia())
			{
				media_elm->FocusInternalEdge(forward);

				applied_focus = TRUE;
			}
#endif // MEDIA_SUPPORT
		}
		return applied_focus;
	}
	else
		return FALSE;
}

#ifdef LINK_TRAVERSAL_SUPPORT
void
HTML_Document::GetLinkElements(OpAutoVector<OpElementInfo>* vector)
{
	OP_ASSERT(vector);

	HTML_Element* doc_root = frames_doc->GetDocRoot();

	if (doc_root && doc_root->GetLayoutBox())
	{
		LinkTraversalObject link_traversal(frames_doc, vector);

		link_traversal.Traverse(doc_root);
	}
}
#endif // LINK_TRAVERSAL_SUPPORT

#ifndef HAS_NO_SEARCHTEXT
# ifdef SEARCH_MATCHES_ALL
#  include "modules/debug/debug.h"

/* private */
OP_BOOLEAN
HTML_Document::FindAllMatches(SearchData *data, HTML_Element *highlighted_elm, int highlight_pos)
{
	RETURN_IF_MEMORY_ERROR(ClearSelectionElms());

	HTML_Element* doc_root = frames_doc->GetDocRoot();
	if (doc_root && doc_root->GetLayoutBox())
	{
		if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
		{
#if defined SVG_SUPPORT && defined SVG_SUPPORT_SEARCH_HIGHLIGHTING
			// Don't search inside non-interactive svg:s, e.g css backgrounds
			SVGImage* svgimage = g_svg_manager->GetSVGImage(logdoc, logdoc->GetDocRoot());
			if(svgimage && !svgimage->IsInteractive())
			{
				return OpBoolean::IS_FALSE;
			}
#endif // SVG_SUPPORT &&SVG_SUPPORT_SEARCH_HIGHLIGHTING

			RETURN_IF_MEMORY_ERROR(frames_doc->GetLogicalDocument()->FindAllMatches(data, highlighted_elm, highlight_pos));
		}

		// Repaint everything that got highlighted (doesn't include form
		// elements but those got invalidated in the search process anyway).
		HighlightUpdateObject highlight_object(frames_doc, (SelectionElm*) selections.First(), TRUE);
		highlight_object.Traverse(doc_root);
	}

	if (selections.Empty())
		return OpBoolean::IS_FALSE;
	else
		GetVisualDevice()->UpdateAll();

	return OpBoolean::IS_TRUE;
}

void
GetSearchHitBoundingBox(RECT &union_rect, TextSelection *selection, FramesDocument *frm_doc)
{
	union_rect.left = LONG_MAX;
	union_rect.top = LONG_MAX;
	union_rect.right = LONG_MIN;
	union_rect.bottom = LONG_MIN;

	const SelectionBoundaryPoint& start_point = selection->GetStartSelectionPoint();
	const SelectionBoundaryPoint& end_point = selection->GetEndSelectionPoint();

	HTML_Element *stop_elm = end_point.GetElement()->NextSibling();
	for (HTML_Element *iter = start_point.GetElement(); iter && iter != stop_elm; iter = iter->Next())
	{
		if (Box *layout_box = iter->GetLayoutBox())
		{
			RECT b_rect;
			BOOL found = FALSE;
			if (iter->Type() == HE_TEXT)
			{
				int start_offset = iter == start_point.GetElement() ? start_point.GetElementCharacterOffset() : 0;
				int end_offset = iter == end_point.GetElement() ? end_point.GetElementCharacterOffset() : -1;

				found = layout_box->GetRect(frm_doc, BOUNDING_BOX, b_rect, start_offset, end_offset);
			}
			else
			{
				if (FormObject *form_obj = iter->GetFormObject())
				{
					form_obj->SelectSearchHit(start_point.GetElementCharacterOffset(),
						end_point.GetElementCharacterOffset(), FALSE);
				}

				found = layout_box->GetRect(frm_doc, BOUNDING_BOX, b_rect);
			}

			if (found)
			{
				if (b_rect.left < union_rect.left)
					union_rect.left = b_rect.left;

				if (b_rect.top < union_rect.top)
					union_rect.top = b_rect.top;

				if (b_rect.right > union_rect.right)
					union_rect.right = b_rect.right;

				if (b_rect.bottom > union_rect.bottom)
					union_rect.bottom = b_rect.bottom;
			}
		}
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		else if (iter->Parent() && iter->Parent()->GetNsType() == NS_SVG)
		{
			OpRect b_rect;
			if(OpStatus::IsSuccess(g_svg_manager->GetElementRect(iter->Parent(), b_rect)))
			{
				if (b_rect.Left() < union_rect.left)
					union_rect.left = b_rect.Left();

				if (b_rect.Top() < union_rect.top)
					union_rect.top = b_rect.Top();

				if (b_rect.Right() > union_rect.right)
					union_rect.right = b_rect.Right();

				if (b_rect.Bottom() > union_rect.bottom)
					union_rect.bottom = b_rect.Bottom();

				OpStatus::Ignore(g_svg_manager->RepaintElement(frm_doc, iter->Parent()));
			}
		}
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING
	}
}

OP_BOOLEAN
HTML_Document::HighlightNextMatch(SearchData *data, OpRect& rect)
{
	BOOL new_search = selections.Empty() || data->IsNewSearch();
	if (new_search)
	{
		int old_pos = 0;
		HTML_Element *old_nav_elm = GetNavigationElement();
		if (!old_nav_elm && GetTextSelection())
		{
			old_nav_elm = GetTextSelection()->GetStartElement();
			old_pos = GetTextSelection()->GetStartSelectionPoint().GetElementCharacterOffset();
		}

		frames_doc->ClearDocumentSelection(TRUE, FALSE, TRUE); // clear all other text selection first

		OP_BOOLEAN found = FindAllMatches(data, old_nav_elm, old_pos);
		if (found != OpBoolean::IS_TRUE)
			return found;
	}

	if (data->GetMatchingDoc() && data->GetMatchingDoc()->GetHtmlDocument() != this)
		return OpBoolean::IS_FALSE;

	BOOL had_active = FALSE;

	if (active_selection && !new_search)
	{
		TextSelection *selection = active_selection->GetSelection();
		selection->SetHighlightType(TextSelection::HT_SEARCH_HIT);

		RECT active_bounding_rect;
		GetSearchHitBoundingBox(active_bounding_rect, selection, frames_doc);

		active_selection = (SelectionElm*) (data->GetForward() ? active_selection->Suc() : active_selection->Pred());

		if (active_bounding_rect.left != LONG_MAX)
			GetVisualDevice()->Update(active_bounding_rect.left,
				active_bounding_rect.top,
				active_bounding_rect.right - active_bounding_rect.left,
				active_bounding_rect.bottom - active_bounding_rect.top, FALSE);

		had_active = TRUE;
	}

	if (!active_selection && !had_active)
	{
		active_selection = (SelectionElm*) (data->GetForward() ? selections.First() : selections.Last());
	}

	if (active_selection)
	{
		TextSelection *selection = active_selection->GetSelection();
		selection->SetHighlightType(TextSelection::HT_ACTIVE_SEARCH_HIT);
		selection->UpdateNavigationElement(GetFramesDocument(), selection->GetStartElement());

		RECT active_bounding_rect;
		GetSearchHitBoundingBox(active_bounding_rect, selection, frames_doc);

		if (active_bounding_rect.left != LONG_MAX)
		{
			OpRect scroll_rect(&active_bounding_rect);

			GetVisualDevice()->Update(active_bounding_rect.left,
				active_bounding_rect.top,
				active_bounding_rect.right - active_bounding_rect.left,
				active_bounding_rect.bottom - active_bounding_rect.top,
				FALSE);

			frames_doc->ScrollToRect(scroll_rect,
									 SCROLL_ALIGN_CENTER,
									 FALSE, VIEWPORT_CHANGE_REASON_FIND_IN_PAGE,
									 selection->GetStartElement());
			rect = scroll_rect;
		}

		data->SetMatchingDoc(GetFramesDocument());

		if (active_selection) // could be changed if deleted in ScrollToRect()
		{
			selection = active_selection->GetSelection(); // fetch it again

			HTML_Element* start_elm = selection->GetStartElement();
			FormObject* fobj = start_elm->GetFormObject();
			if (fobj)
			{
				FormValue* form_value = start_elm->GetFormValue();
				form_value->SelectSearchHit(start_elm, selection->GetStartOffset(), selection->GetEndOffset(), TRUE);
			}
		}

		return OpBoolean::IS_TRUE;
	}

	data->SetMatchingDoc(NULL);

	return OpBoolean::IS_FALSE;
}

OP_STATUS
HTML_Document::AddSearchHit(const SelectionBoundaryPoint *start, const SelectionBoundaryPoint *end, BOOL set_active/*=FALSE*/)
{
	SelectionElm *new_hit = OP_NEW(SelectionElm, (GetFramesDocument(), start, end));
	if (!new_hit)
		return OpStatus::ERR_NO_MEMORY;

	TextSelection *selection = new_hit->GetSelection();

	if (FormObject *form_obj = start->GetElement()->GetFormObject())
		selection->SetFormObjectHasSelection(form_obj);

	if (set_active)
	{
		active_selection = new_hit;
		selection->SetHighlightType(TextSelection::HT_ACTIVE_SEARCH_HIT);
	}
	else
		selection->SetHighlightType(TextSelection::HT_SEARCH_HIT);

	RETURN_IF_MEMORY_ERROR(UpdateSearchHit(new_hit, TRUE));

	new_hit->Into(&selections);

	return OpStatus::OK;
}

OP_STATUS
HTML_Document::UpdateSearchHit(SelectionElm *selection, BOOL enable)
{
	OP_ASSERT(selection && selection->GetSelection());

	TextSelection *txt_selection = selection->GetSelection();

	HTML_Element *iter = txt_selection->GetStartSelectionPoint().GetElement();
	HTML_Element *stop_elm = txt_selection->GetEndSelectionPoint().GetElement()->NextSibling();

	while (iter && iter != stop_elm)
	{
		// Search hits in form elements are maintained through the FormValue interface.
		if (iter->IsFormElement() &&
		    iter == txt_selection->GetStartSelectionPoint().GetElement() &&
		    iter == txt_selection->GetEndSelectionPoint().GetElement())
		{
			FormValue* form_value = iter->GetFormValue();
			if (enable)
			{
				// Select the first hit in a form element (because it makes more sense than selecting the last).
				if (!selections_map.Contains(iter))
					form_value->SelectSearchHit(iter, txt_selection->GetStartSelectionPoint().GetElementCharacterOffset(), txt_selection->GetEndSelectionPoint().GetElementCharacterOffset(), FALSE);
			}
			else
				form_value->ClearSelection(iter);
		}

		if (iter->GetLayoutBox())
		{
			iter->GetLayoutBox()->SetSelected(enable);
			if (enable)
			{
				RETURN_IF_MEMORY_ERROR(MapSearchHit(iter, selection));
			}
		}
#ifdef SVG_SUPPORT_SEARCH_HIGHLIGHTING
		else if (iter->Parent() && iter->Parent()->GetNsType() == NS_SVG)
		{
			iter->SetIsInSelection(enable);
			if (enable)
			{
				RETURN_IF_MEMORY_ERROR(MapSearchHit(iter, selection));
			}
			OpStatus::Ignore(g_svg_manager->RepaintElement(GetFramesDocument(), iter->Parent()));
		}
#endif // SVG_SUPPORT_SEARCH_HIGHLIGHTING

		iter = iter->Next();
	}

	return OpStatus::OK;
}

OP_STATUS
HTML_Document::MapSearchHit(HTML_Element *elm, SelectionElm *selection)
{
#ifdef _DEBUG
	HTML_Element *last_elm = static_cast<HTML_Element*>(selection->GetSelection()->GetEndElement()->NextSibling());
	BOOL inside_selection = FALSE;
	for (HTML_Element *iter_elm = selection->GetSelection()->GetStartElement();
		iter_elm && iter_elm != last_elm;
		iter_elm = iter_elm->Next())
	{
		if (iter_elm == elm)
		{
			inside_selection = TRUE;
			break;
		}
	}
	OP_ASSERT(inside_selection || !"Mapping the selection to an element that isn't in the selection. We will fail to cleanup references and crash. Just so that you know.");
#endif // _DEBUG

	RETURN_IF_MEMORY_ERROR(selections_map.Add(elm, selection));
	elm->SetReferenced(TRUE);

	return OpStatus::OK;
}

OP_STATUS
HTML_Document::ClearSelectionElms()
{
	last_search_txt.Empty();
	active_selection = NULL;

	if (!selections.Empty())
	{
		SelectionElm *selection_iter = static_cast<SelectionElm*>(selections.First());
		while (selection_iter)
		{
			RETURN_IF_MEMORY_ERROR(UpdateSearchHit(selection_iter, FALSE));
			selection_iter = static_cast<SelectionElm*>(selection_iter->Suc());
		}

		selections_map.RemoveAll();
		selections.Clear();

		GetVisualDevice()->UpdateAll();
	}

	return OpStatus::OK;
}

SelectionElm*
HTML_Document::GetSelectionElmFromHe(HTML_Element *he)
{
	SelectionElm *found_match = NULL;

	OP_STATUS oom = selections_map.GetData(he, &found_match);
	if (OpStatus::IsSuccess(oom))
		return found_match;

	return NULL;
}

# endif // SEARCH_MATCHES_ALL
#endif // HAS_NO_SEARCHTEXT
