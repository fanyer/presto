/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined(KEYBOARD_SELECTION_SUPPORT) || defined(DOCUMENT_EDIT_SUPPORT)

#include "modules/doc/caret_manager.h"

#include "modules/logdoc/logdoc_util.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/box/blockbox.h"
#include "modules/doc/html_doc.h"
#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/documentedit/OpDocumentEditUtils.h"
#include "modules/doc/caret_painter.h"
#include "modules/widgets/OpEditCommon.h"

static BOOL GetOldStyleFocusPoint(FramesDocument* frames_doc, HTML_Element*& old_style_elm, int& old_style_offset)
{
	if (TextSelection* sel = frames_doc->GetTextSelection())
		if (sel->GetFocusPoint())
		{
			TextSelection::ConvertPointToOldStyle(*sel->GetFocusPoint(), old_style_elm, old_style_offset);
			OP_ASSERT(!old_style_elm || old_style_elm->Type() != HE_DOC_ROOT);
			return TRUE;
		}
	return FALSE;
}

HTML_Element* CaretManager::GetElement()
{
	HTML_Element* elm;
	int offset;
	if (GetOldStyleFocusPoint(frames_doc, elm, offset))
		return elm;

	return NULL;
}

int CaretManager::GetOffset()
{
	HTML_Element* elm;
	int offset;
	if (GetOldStyleFocusPoint(frames_doc, elm, offset))
		return offset;

	return 0;
}

BOOL CaretManager::GetIsLineEnd()
{
	if (TextSelection* sel = frames_doc->GetTextSelection())
		if (sel->GetFocusPoint())
			return sel->GetFocusPoint()->GetBindDirection() == SelectionBoundaryPoint::BIND_BACKWARD;
	return FALSE;
}


#ifdef SUPPORT_TEXT_DIRECTION

BOOL CaretManager::GetRTL(HTML_Element *helm)
{
	if (helm)
	{
		Head prop_list;
		HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
		LayoutProperties* lprops = LayoutProperties::CreateCascade(helm, prop_list, hld_profile);
		if (lprops && (lprops->GetProps()->direction == CSS_VALUE_rtl))
		{
			prop_list.Clear();
			return TRUE;
		}
		prop_list.Clear();
	}
	return FALSE;
}

#else // SUPPORT_TEXT_DIRECTION

BOOL CaretManager::GetRTL(HTML_Element *helm)
{
	return FALSE;
}

#endif // SUPPORT_TEXT_DIRECTION


BOOL CaretManager::Place(const AffinePos &ctm, INT32 x1, INT32 y1, INT32 x2, INT32 y2, BOOL remember_x, BOOL enter_all /* = FALSE */, HTML_Element *selection_container_element /* = NULL */, BOOL same_pos_is_success /* = FALSE */, BOOL search_whole_viewport /* = FALSE */)
{
	// A logical "coordinate" for the caret consists of an HTML element, an
	// offset within it, and a preference as to where to place the caret when
	// the same offset corresponds to two visual positions (e.g. at the end of
	// one line vs. at the beginning of the following line). The following
	// three variables record new values for these coordinate components.
	HTML_Element *new_helm;
	int new_ofs;
	BOOL new_line_end;

	// Find a SelectionBoundaryPoint

	OpPoint p1(x1, y1);
	ctm.Apply(p1);

	OpPoint p2(x2, y2);
	ctm.Apply(p2);

	TextSelectionObject text_selection(frames_doc, LayoutCoord(p1.x), LayoutCoord(p1.y), LayoutCoord(p2.x), LayoutCoord(p2.y), search_whole_viewport);

	if (selection_container_element && selection_container_element->Type() != HE_BODY)
		text_selection.SetSelectionContainer(selection_container_element);

	Head props_list;
	text_selection.SetEnterAll(enter_all);
	if (OpStatus::IsMemoryError(text_selection.Traverse(frames_doc->GetLogicalDocument()->GetRoot(), &props_list)))
	{
		frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return FALSE;
	}

	// If the selection was at/past the end of the line, we should prefer the
	// first visual position to prevent the caret from going to the beginning
	// of the next line for <pre> tags, which use the same logical position for
	// the end of one line and the beginning of the next.
	new_line_end = text_selection.IsAtEndOfLine();

	if (!text_selection.HasNearestBoundaryPoint())
	{
		props_list.Clear();

		HTML_Document *html_doc = frames_doc->GetHtmlDocument();
		if (html_doc && html_doc->HasSelectedText()
#ifdef DRAG_SUPPORT
			// If we drag something try to show the caret even if there's a selection.
			&& !frames_doc->GetCaretPainter()->IsDragCaret()
#endif // DRAG_SUPPORT
			)
			return FALSE;

		// Don't modify the document if we're in keyboard selection mode so just give up.
		if (frames_doc->GetKeyboardSelectionMode())
			return FALSE;

#ifdef DOCUMENT_EDIT_SUPPORT
		if (frames_doc->GetDocumentEdit())
		{
			LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
			HTML_Element *root = logdoc->GetRoot();
			Box *inner_box = root->GetInnerBox((x2 - x1)/2 + x1, (y2 - y1)/2 + y1, frames_doc);
			HTML_Element *inner_helm = inner_box ? inner_box->GetHtmlElement() : NULL;
			if (frames_doc->GetDocumentEdit()->IsUsingDesignMode() && inner_helm && inner_helm->IsAncestorOf(logdoc->GetBodyElm()))
			{
				inner_helm = logdoc->GetBodyElm();
				inner_box = inner_helm->GetLayoutBox();
			}
			if (inner_box && frames_doc->GetDocumentEdit()->m_caret.IsElementEditable(inner_helm))
			{
				// We couldn't find something to put the caret on, but we found a block.
				// Check if it's a block we want content in, and if it's empty we should put the caret in it.
				if ((inner_box->IsBlockBox() || inner_box->IsTableCell()) && !frames_doc->GetDocumentEdit()->IsStandaloneElement(inner_helm))
				{
					HTML_Element* editable_elm = frames_doc->GetDocumentEdit()->FindEditableElement(inner_helm, TRUE, FALSE, TRUE);
					if (!editable_elm || !inner_helm->IsAncestorOf(editable_elm)
#ifdef DRAG_SUPPORT
					    || frames_doc->GetCaretPainter()->IsDragCaret()
#endif // DRAG_SUPPORT
					   )
					{
						HTML_Element *temporary_caret_helm = frames_doc->GetDocumentEdit()->m_caret.CreateTemporaryCaretHelm(inner_helm, NULL);
						if (temporary_caret_helm)
						{
							frames_doc->GetDocumentEdit()->m_caret.Place(temporary_caret_helm, 0);
							return TRUE;
						}
					}
				}
			}
		}
#endif //DOCUMENT_EDIT_SUPPORT
		return FALSE;
	}

	SelectionBoundaryPoint nearest_boundary_point = text_selection.GetNearestBoundaryPoint();
	nearest_boundary_point.SetBindDirection(new_line_end ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);
	frames_doc->GetVisualDevice()->Reset();

	// Get position/lineinfo from the LayoutSelectionPoint

	CalcSelPointPositionObject selection_update(frames_doc, &nearest_boundary_point);

	(static_cast<BlockBox*>(frames_doc->GetLogicalDocument()->GetRoot()->GetLayoutBox()))->Traverse(&selection_update, static_cast<LayoutProperties*>(props_list.First()), NULL);
	props_list.Clear();

	// Update current element and offset
	TextSelection::ConvertPointToOldStyle(nearest_boundary_point, new_helm, new_ofs);

	if (new_helm->Type() == HE_TEXT)
	{
		CaretManagerWordIterator iter(new_helm, frames_doc);
		int last_valid_ofs;
		if (OpStatus::IsSuccess(iter.GetStatus()) && iter.GetLastValidCaretOfs(last_valid_ofs))
			new_ofs = MIN(new_ofs, last_valid_ofs);
	}

	if (!GetValidCaretPosFrom(frames_doc, new_helm, new_ofs, new_helm, new_ofs))
		return FALSE;

	if (new_helm == GetElement() && new_ofs == GetOffset() && new_line_end == GetIsLineEnd())
	{
		TextSelection *sel = frames_doc->GetTextSelection();
		SelectionBoundaryPoint caret_point(*sel->GetFocusPoint());
#ifdef DRAG_SUPPORT
		CaretPainter* caret_painter = frames_doc->GetCaretPainter();
		if (caret_painter->IsDragCaret())
			caret_painter->SetDragCaretPoint(caret_point);
		else
#endif // DRAG_SUPPORT
			frames_doc->SetSelection(&caret_point, &caret_point, FALSE, !remember_x);
		return same_pos_is_success;
	}

#ifdef DOCUMENT_EDIT_SUPPORT
	if (frames_doc->GetDocumentEdit())
		Place(new_helm, new_ofs, new_line_end, TRUE, FALSE, remember_x, FALSE);
#endif // DOCUMENT_EDIT_SUPPORT
	SelectionBoundaryPoint caret_point(new_helm, new_ofs);
	caret_point.SetBindDirection(new_line_end ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);
#ifdef DRAG_SUPPORT
	CaretPainter* caret_painter = frames_doc->GetCaretPainter();
	if (caret_painter->IsDragCaret())
		caret_painter->SetDragCaretPoint(caret_point);
	else
#endif // DRAG_SUPPORT
		frames_doc->SetSelection(&caret_point, &caret_point, FALSE, !remember_x);

	return TRUE;
}

void CaretManager::MoveCaret(OpWindowCommander::CaretMovementDirection direction, HTML_Element *containing_element, HTML_Element *root_element)
{
	if (!GetElement())
		return;

	AffinePos ctm;
	RECT rect;
	GetContentRectOfElement(containing_element, ctm, rect);
	rect.right--;
	rect.bottom--;

	// This code relies on the current caret position in CaretPainter so
	// if that is wrong, this will do the wrong things.

	switch (direction)
	{
		default:
			OP_ASSERT(!"Missing case in CaretManager::MoveCaret()");
			// Fall through.
		case OpWindowCommander::CARET_RIGHT:
		case OpWindowCommander::CARET_LEFT:
		case OpWindowCommander::CARET_WORD_RIGHT:
		case OpWindowCommander::CARET_WORD_LEFT:
			{
				BOOL move_left = direction == OpWindowCommander::CARET_LEFT || direction == OpWindowCommander::CARET_WORD_LEFT;
				BOOL move_forward = move_left == GetRTL(GetCaretContainer(GetElement()));
				BOOL move_word = direction == OpWindowCommander::CARET_WORD_RIGHT || direction == OpWindowCommander::CARET_WORD_LEFT;
				Move(move_forward, move_word);
			}
			break;
		case OpWindowCommander::CARET_DOCUMENT_HOME:
			PlaceAtCoordinates(ctm, rect.left + 1, rect.top + 1);
			break;
		case OpWindowCommander::CARET_DOCUMENT_END:
			if (!PlaceAtCoordinates(ctm, rect.right - 1, rect.bottom - 1))
			{
				int extra_step = 10;
				int y = rect.bottom - extra_step;
				for (; y > frames_doc->GetCaretPainter()->GetY(); y -= extra_step)
				{
					if (Place(ctm, rect.right - 1, y, rect.right - 1, y, TRUE, FALSE, containing_element))
						break;
				}
			}
			break;
		case OpWindowCommander::CARET_LINE_HOME:
			{
				PlaceAtCoordinates(ctm, rect.left, frames_doc->GetCaretPainter()->GetY() + frames_doc->GetCaretPainter()->GetHeight()/2);

				// Try entering all boxes too. We might find something
				// that is outside its content (unbroken line)
				RECT helm_rect;
				GetContentRectOfElement(GetElement(), ctm, helm_rect);
				PlaceAtCoordinates(ctm, MIN(rect.left, helm_rect.left + 1), frames_doc->GetCaretPainter()->GetY() + frames_doc->GetCaretPainter()->GetHeight()/2, TRUE, TRUE);
			}
			break;
		case OpWindowCommander::CARET_LINE_END:
			{
			AffinePos ctm;
			RECT helm_rect;
			GetContentRectOfElement(GetElement(), ctm, helm_rect);
			int try1 = MIN(rect.right, helm_rect.right - 1);
			int try2 = MAX(rect.right, helm_rect.right - 1);

			PlaceAtCoordinates(ctm, try1, frames_doc->GetCaretPainter()->GetY() + frames_doc->GetCaretPainter()->GetHeight()/2);
			// Try entering all boxes too. We might find something
			// that is outside its content (unbroken line)
			PlaceAtCoordinates(ctm, try2, frames_doc->GetCaretPainter()->GetY() + frames_doc->GetCaretPainter()->GetHeight()/2, TRUE, TRUE);
			}
			break;
		case OpWindowCommander::CARET_PARAGRAPH_UP: // Not really supported but this is better than doing nothing.
		case OpWindowCommander::CARET_PARAGRAPH_DOWN: // Not really supported but this is better than doing nothing.
		case OpWindowCommander::CARET_UP:
		case OpWindowCommander::CARET_DOWN:
			{
				// Iterate step (in pixels). Must be smaller than the smallest lineheight.
				long extra_step = 10;
				long extra = extra_step;
				long old_line_y = frames_doc->GetCaretPainter()->GetLineY();
				long old_line_height = frames_doc->GetCaretPainter()->GetLineHeight();
				BOOL direction_down = direction == OpWindowCommander::CARET_DOWN || direction == OpWindowCommander::CARET_PARAGRAPH_DOWN;
				long search_limit = frames_doc->Height();

				// Iterate and test if caret can be placed anywhere
				while (extra < search_limit)
				{
					long y1, y2;
					if (direction_down)
						y1 = old_line_y + old_line_height/2 + extra - extra_step;
					else
						y1 = old_line_y - 1 - extra;
					y2 = y1 + extra_step;

					BOOL found = FALSE;

					// Try using the wanted x-position.
					long wanted_x = frames_doc->GetCaretPainter()->GetWantedX();
					if (Place(ctm, wanted_x, y1, wanted_x, y1, FALSE, FALSE, root_element) ||
						Place(ctm, wanted_x, y1, wanted_x, y2, FALSE, FALSE, root_element))
					{
						found = TRUE;
					}
					else if (Place(ctm, 0, y1, 10000, y2, FALSE, FALSE, root_element))
					{
						// There is something on this line. Try again with increasing testzone.
						for (int i = 3; i < 50; i += 5)
						{
							if (Place(ctm, wanted_x - i, y1, wanted_x + i, y2, FALSE, FALSE, root_element, TRUE))
							{
								found = TRUE;
								break;
							}
						}
					}
					if (found)
					{
						if (direction_down)
						{
							if (frames_doc->GetCaretPainter()->GetLineY() > old_line_y)
								break;
						}
						else
						{
							if (frames_doc->GetCaretPainter()->GetLineY() < old_line_y)
								break;
						}
					}
					extra += extra_step;
				}
#ifdef DOCUMENT_EDIT_SUPPORT
				if (extra >= search_limit && frames_doc->GetDocumentEdit())
					frames_doc->GetDocumentEdit()->m_caret.MoveSpatialVertical(direction == OpWindowCommander::CARET_DOWN);
#endif // DOCUMENT_EDIT_SUPPORT
			}
			break;
		case OpWindowCommander::CARET_PAGEUP:
		case OpWindowCommander::CARET_PAGEDOWN:
			{
				long step = 10;
				// Height of an area which will be searched during the one pass of the algorithm.
				long one_pass_search_limit = frames_doc->GetVisualViewport().height;
				// Total limit of the search area when searching it down.
				long limit = frames_doc->Height();
				// Total limit of the search area when searching it up.
				long start = 0;
				ScrollableArea* sc = NULL;
				if (root_element)
				{
					sc = root_element->GetLayoutBox()->GetScrollable();
					if (sc)
					{
						GetContentRectOfElement(root_element, ctm, rect);
						// One pass search limit is height of the scrollable.
						one_pass_search_limit = rect.bottom - rect.top;
						// Limit is the same since we'll be scrolling the scrollable and searching though it over again.
						limit = one_pass_search_limit - sc->GetHorizontalScrollbarWidth();
						start = rect.top;
					}
				}

				long old_line_y = frames_doc->GetCaretPainter()->GetLineY();
				long old_line_height = frames_doc->GetCaretPainter()->GetLineHeight();

				long stop_y, iter_y, y2;
				BOOL placed = FALSE;
				long wanted_x = frames_doc->GetCaretPainter()->GetWantedX();

				do
				{
					if (direction == OpWindowCommander::CARET_PAGEDOWN)
					{
						stop_y = old_line_y + old_line_height;
						iter_y = old_line_y + one_pass_search_limit;
					}
					else
					{
						stop_y = old_line_y - old_line_height;
						iter_y = old_line_y - one_pass_search_limit;
					}

					y2 = iter_y + step;

					// First we try to place the caret as nearest the edge of the current view port/scrollable as possible.
					// We move closer to the start point while searching the place for caret (if none is found earlier), though.
					while ((direction == OpWindowCommander::CARET_PAGEDOWN && iter_y > stop_y) || (direction == OpWindowCommander::CARET_PAGEUP && iter_y < stop_y))
					{
						if (Place(ctm, wanted_x, iter_y, wanted_x, y2, FALSE))
						{
							placed = TRUE;
							break;
						}

						if (direction == OpWindowCommander::CARET_PAGEDOWN)
							iter_y -= step;
						else
							iter_y += step;
						y2 = iter_y + step;
					}

					if (sc && !placed)
					{
						int current_scroll = sc->GetViewY();
						int scroll_to = direction == OpWindowCommander::CARET_PAGEDOWN ? current_scroll + one_pass_search_limit : current_scroll - one_pass_search_limit;
						scroll_to = MAX(0, scroll_to);
						sc->SetViewY(LayoutCoord(scroll_to), TRUE);
						// Could not scroll any more.
						if (current_scroll == sc->GetViewY())
							break;
					}
				} while (sc && !placed);

				// We didn't find any place with the view port so let's search further (beyond the viewport).
				if (!placed)
				{
					iter_y = direction == OpWindowCommander::CARET_PAGEDOWN ? old_line_y + one_pass_search_limit + step : old_line_y - one_pass_search_limit - step;
					iter_y = MAX(iter_y, 0);
					y2 = iter_y + step;
					stop_y = direction == OpWindowCommander::CARET_PAGEDOWN ? limit : start;

					while ((direction == OpWindowCommander::CARET_PAGEDOWN && iter_y <= stop_y) || (direction == OpWindowCommander::CARET_PAGEUP && iter_y >= start))
					{
						if (Place(ctm, wanted_x, iter_y, wanted_x, y2, FALSE))
							break;
						if (direction == OpWindowCommander::CARET_PAGEDOWN)
							iter_y += step;
						else
							iter_y -= step;
						y2 = iter_y + step;
					}
				}
			}
			break;
	}

	if (direction != OpWindowCommander::CARET_UP && direction != OpWindowCommander::CARET_DOWN && direction != OpWindowCommander::CARET_PAGEUP && direction != OpWindowCommander::CARET_PAGEDOWN && direction != OpWindowCommander::CARET_PARAGRAPH_UP && direction != OpWindowCommander::CARET_PARAGRAPH_DOWN)
		frames_doc->GetCaretPainter()->UpdateWantedX();

	// After moving the caret we want it to be in the "on" state so the user can clearly see where it ended up.
	frames_doc->GetCaretPainter()->RestartBlink();
}

BOOL CaretManager::PlaceAtCoordinates(const AffinePos &ctm, INT32 x, INT32 y, BOOL remember_x /* = TRUE */, BOOL enter_all /* = FALSE */, BOOL search_whole_viewport /* = FALSE */)
{
	if (!GetElement()
#ifdef DRAG_SUPPORT
		&& !frames_doc->GetCaretPainter()->IsDragCaret()
#endif // DRAG_SUPPORT
	    )
		return FALSE;

	// Only find children of the editable_container for the given position.
	Box *inner_box = frames_doc->GetLogicalDocument()->GetRoot()->GetInnerBox(x, y, frames_doc);
	HTML_Element *inner_helm = inner_box ? inner_box->GetHtmlElement() : NULL;

	HTML_Element *container = NULL;
	if (inner_helm)
	{
		container = GetCaretContainer(inner_helm);
		if (!container)
			container = inner_helm;
	}

	if (Place(ctm, x, y, x, y, remember_x, enter_all, container, FALSE, search_whole_viewport))
		return TRUE;
	// Failed to get exact caret position. But try more...

	if (container)
	{
		// If the caret is already inside the editable container, we will return FALSE unless a user d'n'd.
		if (HTML_Element* caret_elm = GetElement())
			if (container->IsAncestorOf(caret_elm)
#ifdef DRAG_SUPPORT
			    && !frames_doc->GetCaretPainter()->IsDragCaret()
#endif // DRAG_SUPPORT
			   )
				return FALSE;

		// Keep x and y within the rect of the editable container so we'll find the closest line if we click the empty space below/around the content.
		AffinePos dummy;
		RECT editable_container_rect;
		if (!container->GetLayoutBox())
			return FALSE;
		if (!container->GetLayoutBox()->GetRect(frames_doc, BOUNDING_BOX, dummy, editable_container_rect))
			return FALSE;
		x = MAX(editable_container_rect.left, x);
		y = MAX(editable_container_rect.top, y);
		x = MIN(editable_container_rect.right - 1, x);
		y = MIN(editable_container_rect.bottom - 1, y);
	}
	return Place(ctm, x, y, x, y, remember_x, enter_all, container, FALSE, search_whole_viewport);
}

/* static */
BOOL CaretManager::GetNearestCaretPos(FramesDocument* doc, HTML_Element* from_helm, HTML_Element** nearest_helm, int* nearest_ofs, BOOL forward, BOOL include_current, HTML_Element* helm_to_remove)
{
	HTML_Element* helm = FindEditableElement(doc, from_helm, forward, include_current, TRUE, FALSE, helm_to_remove);
	if (!helm)
		return FALSE;

	if (forward)
	{
		*nearest_ofs = 0;
	}
	else
	{
		if (helm->Type() == HE_TEXT)
			*nearest_ofs = helm->ContentLength();
		else
			*nearest_ofs = helm->Type() == HE_BR ? 0 : 1;
	}
	*nearest_helm = helm;
	return TRUE;
}

/* static */
BOOL CaretManager::GetBestCaretPosFrom(FramesDocument* doc, HTML_Element *helm, HTML_Element *&new_helm, int &new_ofs, BOOL3 prefer_first /* = MAYBE */, BOOL must_be_friends /* = FALSE */, HTML_Element* helm_to_remove /* = NULL */)
{
	HTML_Element *prev_helm = NULL, *next_helm = NULL;
	int prev_ofs = 0, next_ofs = 0;
	GetNearestCaretPos(doc, helm, &prev_helm, &prev_ofs, FALSE, FALSE, helm_to_remove);
	if (must_be_friends && !doc->GetCaret()->IsTightlyConnectedInlineElements(prev_helm, helm, TRUE, TRUE))
	{
		prev_helm = NULL;
		prev_ofs = 0;
	}
	GetNearestCaretPos(doc, helm->NextSiblingActual(), &next_helm, &next_ofs, TRUE, TRUE, helm_to_remove);
	if (must_be_friends && !doc->GetCaret()->IsTightlyConnectedInlineElements(helm, next_helm, TRUE, TRUE))
	{
		next_helm = NULL;
		next_ofs = 0;
	}
	if (!prev_helm || !next_helm)
	{
		if (prev_helm)
		{
			new_helm = prev_helm;
			new_ofs = prev_ofs;
		}
		else
		{
			new_helm = next_helm;
			new_ofs = next_ofs;
		}
		return new_helm != NULL;
	}
	BOOL pick_first = doc->GetCaret()->IsTightlyConnectedInlineElements(prev_helm, helm, TRUE, TRUE, TRUE);
	if (prefer_first == NO)
		pick_first = pick_first && !doc->GetCaret()->IsTightlyConnectedInlineElements(helm, next_helm, TRUE, TRUE);
	if (pick_first)
	{
		new_helm = prev_helm;
		new_ofs = prev_ofs;
	}
	else
	{
		new_helm = next_helm;
		new_ofs = next_ofs;
	}
	return TRUE;
}

/* static */
BOOL CaretManager::GetValidCaretPosFrom(FramesDocument* doc, HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs)
{
	if (!helm)
		return FALSE;

	if (IsElementValidForCaret(doc, helm))
	{
		new_helm = helm;
		if (helm->Type() == HE_TEXT)
		{
			CaretManagerWordIterator iter(helm, doc);
			iter.SnapToValidCaretOfs(ofs, new_ofs);
		}
		else if (helm->Type() == HE_BR)
			new_ofs = 0;
		else
			new_ofs = MAX(MIN(ofs, 1), 0);
		return TRUE;
	}
	HTML_Element *candidate_helm = NULL;
	int candidate_ofs = 0;
	BOOL found = FALSE;

	if (helm->FirstChildActual())
	{
		BOOL beginning = ofs <= 0;
		if (beginning)
		{
			if (GetNearestCaretPos(doc, helm,&candidate_helm,&candidate_ofs, TRUE, FALSE))
				found = helm->IsAncestorOf(candidate_helm);
		}
		else
		{
			if (GetNearestCaretPos(doc, helm->LastLeafActual(), &candidate_helm, &candidate_ofs, FALSE, TRUE))
				found = helm->IsAncestorOf(candidate_helm);
		}
	}
	if (!found && !GetBestCaretPosFrom(doc, helm, candidate_helm, candidate_ofs))
	{
		candidate_helm = NULL;
		candidate_ofs = 0;
	}
	else
		found = TRUE;
	new_helm = candidate_helm;
	new_ofs = candidate_ofs;
	return found;
}

/* static */
BOOL CaretManager::IsStandaloneElement(HTML_Element* helm)
{
	if (IsReplacedElement(helm))
		return TRUE;

	// Maby IsHtmlInlineElm or CanBeTerminatedByOtherInline ?
	switch(helm->Type())
	{
	case HE_TEXT:
	case HE_BR:
	case HE_HR:
	// ...There is more
		return TRUE;
	}
	return FALSE;
}

/* static */
BOOL CaretManager::IsReplacedElement(HTML_Element *helm, BOOL must_have_replaced_content /* =FALSE */)
{
	if (helm->GetLayoutBox())
		if (helm->GetLayoutBox()->GetContent())
			if (helm->GetLayoutBox()->GetContent()->IsReplaced())
				return TRUE;

	if (!must_have_replaced_content)
	{
		switch(helm->Type())
		{
		case HE_IMG:
		case HE_INPUT:
		//case HE_BUTTON:
		case HE_IFRAME:
#ifdef MEDIA_HTML_SUPPORT
		case HE_VIDEO:
#endif // MEDIA_HTML_SUPPORT
		// ...There is more
			return TRUE;
		}
	}
	return FALSE;
}

/* static */
BOOL CaretManager::IsBlockElement(FramesDocument* doc, HTML_Element *helm)
{
	if (IsHtmlInlineElm(helm->Type(), doc->GetHLDocProfile()))
		return FALSE;
	switch(helm->Type())
	{
	case HE_BUTTON:
	case HE_IFRAME:
#ifdef MEDIA_HTML_SUPPORT
	case HE_VIDEO:
#endif // MEDIA_HTML_SUPPORT
		return FALSE;
	}
	return TRUE;
}

/* static */
BOOL CaretManager::IsElementEditable(FramesDocument *doc, HTML_Element* helm)
{
	OP_ASSERT(doc);
	if (!helm || !(helm->IsIncludedActual() || helm->GetInserted() == HE_INSERTED_BY_IME))
		return FALSE;
#ifdef DOCUMENT_EDIT_SUPPORT
	if (OpDocumentEdit* documentedit = doc->GetDocumentEdit())
	{
		HTML_Element* body = documentedit->GetBody();
		return (documentedit->m_body_is_root || helm->IsContentEditable(TRUE)) && body && body->IsAncestorOf(helm);
	}
#endif // DOCUMENT_EDIT_SUPPORT
	return FALSE;
}


/* static */
HTML_Element* CaretManager::FindEditableElement(FramesDocument* doc, HTML_Element* from_helm, BOOL forward, BOOL include_current, BOOL require_box, BOOL include_ending_br /* = FALSE */, HTML_Element* helm_to_remove /* = NULL */)
{
	HTML_Element *editable_helm = NULL;
	HTML_Element *helm = from_helm;
	HTML_Element *body = doc->GetLogicalDocument()->GetBodyElm();
	if (!body || !helm)
		return NULL;
	if (!include_current)
	{
		helm = (forward ? helm->NextActual() : helm->PrevActual());
		if (helm_to_remove && helm_to_remove->IsAncestorOf(helm))
			helm = (forward ? helm->NextActual() : helm->PrevActual());
	}

	// Exit if helm is outside of body, but not if helm is (temporarily) detached from the tree.
	if (!body->IsAncestorOf(helm) && doc->GetDocRoot()->IsAncestorOf(helm))
		return NULL;

	while (helm && helm != body)
	{
		if (IsElementValidForCaret(doc, helm, TRUE, include_ending_br, FALSE, helm_to_remove))
		{
			if (!require_box || helm->GetLayoutBox())
			{
				editable_helm = helm;
				break;
			}
		}
		helm = (forward ? helm->NextActual() : helm->PrevActual());
		if (helm_to_remove && helm_to_remove->IsAncestorOf(helm))
			helm = (forward ? helm->NextActual() : helm->PrevActual());
	}
	return editable_helm;
}

/* static */
BOOL CaretManager::IsEndingBr(FramesDocument* doc, HTML_Element* helm, HTML_Element* helm_to_remove)
{
	if (helm->Type() != HE_BR)
		return FALSE;
	// Handles the case when the BR is ending but not in the same branch.
	// <I>gggggggggggggg</I><BR>
	HTML_Element* prev_helm = FindEditableElement(doc, helm, FALSE, FALSE, TRUE, TRUE, helm_to_remove);
	return (prev_helm && prev_helm->Type() != HE_BR && doc->GetCaret()->IsTightlyConnectedInlineElements(prev_helm, helm, TRUE, TRUE, TRUE));
}

/* static */
BOOL CaretManager::IsElementValidForCaret(FramesDocument* doc, HTML_Element *helm, BOOL is_in_tree /* = TRUE */, BOOL ending_br_is_ok /* = FALSE */, BOOL valid_if_possible /* = FALSE */, HTML_Element* helm_to_remove /* = NULL */)
{
	if (!helm || !helm->IsIncludedActual())
		return FALSE;
#ifdef DOCUMENT_EDIT_SUPPORT
	if (!doc->GetKeyboardSelectionMode())
	{
		if (is_in_tree && !IsElementEditable(doc, helm))
			return FALSE;
	}
#endif // DOCUMENT_EDIT_SUPPORT
	HTML_Element *parent = helm->Parent();
	for (; parent && !IsReplacedElement(parent); parent = parent->Parent()) {}
	if (parent)
		return FALSE;
	if (helm->Type() == HE_BR)
		return ending_br_is_ok || !IsEndingBr(doc, helm, helm_to_remove);
	if (helm->Type() == HE_TEXT)
	{
		if (!is_in_tree)
			return TRUE;
#ifdef DOCUMENT_EDIT_SUPPORT
		if (doc->GetDocumentEdit())
			if (doc->GetDocumentEdit()->IsInWsPreservingOperation(helm, NO)) // This element might for the moment not be valid, but WILL be later
				return TRUE;
#endif // DOCUMENT_EDIT_SUPPORT
		CaretManagerWordIterator iter(helm, doc);
		return OpStatus::IsSuccess(iter.GetStatus()) && iter.IsValidForCaret(valid_if_possible);
	}
	if (IsReplacedElement(helm))
		return TRUE;

	return FALSE;
}

/* static */
BOOL CaretManager::MightBeTightlyConnectedToOtherElement(FramesDocument* doc, OpDocumentEdit* documentedit, HTML_Element* helm, BOOL include_replaced /* = FALSE */, BOOL br_is_friendly /* = FALSE */, BOOL non_editable_is_friendly /* = FALSE */)
{
	if (!doc->GetKeyboardSelectionMode() && !IsElementEditable(doc, helm) && !non_editable_is_friendly)
		return FALSE;
	if (!include_replaced && IsStandaloneElement(helm) && helm->Type() != HE_TEXT && helm->Type() != HE_BR)
		return FALSE;
	if (helm->Type() == HE_BR && !br_is_friendly)
		return FALSE;
	return !IsBlockElement(doc, helm);
}

BOOL CaretManager::IsTightlyConnectedInlineElements(HTML_Element* helm_a, HTML_Element* helm_b, BOOL include_helm_b /* = FALSE */, BOOL include_replaced /* = FALSE */, BOOL br_is_friendly /* = FALSE */)
{
	if (!helm_a || !helm_b)
		return FALSE;

	HTML_Element* unfriendly_parent_a = helm_a->Parent();
	HTML_Element* unfriendly_parent_b = helm_b->Parent();

	BOOL a_editable = IsElementEditable(frames_doc, helm_a);
	BOOL b_editable = IsElementEditable(frames_doc, helm_b);

	while (unfriendly_parent_a && MightBeTightlyConnectedToOtherElement(frames_doc, NULL, unfriendly_parent_a, include_replaced, br_is_friendly, a_editable))
		unfriendly_parent_a = unfriendly_parent_a->Parent();
	while (unfriendly_parent_b && MightBeTightlyConnectedToOtherElement(frames_doc, NULL, unfriendly_parent_b, include_replaced, br_is_friendly, b_editable))
		unfriendly_parent_b = unfriendly_parent_b->Parent();

	if (unfriendly_parent_a != unfriendly_parent_b)
		return FALSE;

	BOOL friendly = TRUE;

	if (helm_b->Precedes(helm_a))
	{
		HTML_Element* tmp = helm_a;
		helm_a = helm_b;
		helm_b = tmp;
	}

	// Check if there is a unfriendly element between. If there is, return FALSE.
	HTML_Element* tmp = helm_a;
	while (tmp)
	{
		if (tmp == helm_b && !include_helm_b)
			break;
		if (!MightBeTightlyConnectedToOtherElement(frames_doc, NULL, tmp, include_replaced, br_is_friendly))
		{
			friendly = FALSE;
			break;
		}
		if (tmp == helm_b)
			break;
		tmp = tmp->Next();
	}

	return friendly;
}


BOOL CaretManager::SetToValidPos()
{
	HTML_Element *new_helm = NULL;
	int new_ofs = 0;
	if (!CaretManager::GetValidCaretPosFrom(frames_doc, GetElement(), GetOffset(), new_helm, new_ofs))
	{
		return FALSE;
	}
	if (new_helm != GetElement() || new_ofs != GetOffset())
	{
#ifdef DOCUMENT_EDIT_SUPPORT
		if (!frames_doc->GetKeyboardSelectionMode() &&
			frames_doc->GetDocumentEdit() &&
			!IsElementEditable(frames_doc, new_helm))
		{
			return FALSE;
		}
#endif // DOCUMENT_EDIT_SUPPORT
		Place(new_helm, new_ofs);
	}
	return TRUE;
}

void CaretManager::Move(BOOL forward, BOOL word)
{
	if (!SetToValidPos())
		return;

	if (!forward && word)
	{
		HTML_Element* helm = GetElement();
		int ofs = GetOffset();

		HTML_Element *sel_elm =  frames_doc->GetTextSelection()->GetFocusPoint()->GetElement();
		int sel_ofs = frames_doc->GetTextSelection()->GetFocusPoint()->GetOffset();

		if (GetOffset() != 0)
		{
			int newofs;
			// Jump within current element
			const uni_char* text = helm->TextContent();
			if (text)
				newofs = GetOffset() + SeekWord(text, GetOffset(), -1, 0);
			else
				newofs = 0;

			Place(helm, newofs, FALSE, TRUE, TRUE);
		}

		HTML_Element *new_elm;
		int new_ofs;
		CaretManager::GetValidCaretPosFrom(frames_doc, GetElement(), GetOffset(), new_elm, new_ofs);

		if (ofs == new_ofs && helm == new_elm || sel_ofs == new_ofs && sel_elm == new_elm)
		{
			// Jump to previous element
			GetNearestCaretPos(frames_doc, GetElement(), &new_elm, &new_ofs, FALSE, FALSE);
			if (ofs == new_ofs && helm == new_elm || sel_ofs == new_ofs && sel_elm == new_elm)
				GetOneStepBeside(forward, GetElement(), GetOffset(), new_elm, new_ofs);
			Place(new_elm, new_ofs, FALSE, TRUE, TRUE);
		}
	}
	else if (forward && word)
	{
		HTML_Element* helm = GetElement();
		int ofs = GetOffset();
		const uni_char* text = helm->TextContent();
		int text_len = text ? uni_strlen(text) : 0;

		HTML_Element *sel_elm =  frames_doc->GetTextSelection()->GetFocusPoint()->GetElement();
		int sel_ofs = frames_doc->GetTextSelection()->GetFocusPoint()->GetOffset();

		if (text && ofs < text_len)
		{
			// Jump within current element
			int newofs = ofs + SeekWord(text, GetOffset(), 1, text_len);
			Place(helm, newofs, FALSE, TRUE, TRUE);
		}

		HTML_Element *new_elm;
		int new_ofs;
		CaretManager::GetValidCaretPosFrom(frames_doc, GetElement(), GetOffset(), new_elm, new_ofs);

		if (ofs == new_ofs && helm == new_elm || sel_ofs == new_ofs && sel_elm == new_elm)
		{
			// Jump to next element
			if (GetNearestCaretPos(frames_doc, GetElement(), &new_elm, &new_ofs, TRUE, FALSE))
			{
				if (IsTightlyConnectedInlineElements(GetElement(), new_elm))
				{
					const uni_char* text = new_elm->TextContent();
					if (text)
					{
						int text_len =  uni_strlen(text);
						new_ofs = SeekWord(text, 0, 1, text_len);
					}
					else
						new_ofs = 0;
				}
			}

			if (ofs == new_ofs && helm == new_elm || sel_ofs == new_ofs && sel_elm == new_elm)
				GetOneStepBeside(forward, GetElement(), GetOffset(), new_elm, new_ofs);

			Place(new_elm, new_ofs, FALSE, TRUE, TRUE);
		}
	}
	else
	{
		HTML_Element *new_helm = NULL;
		int new_ofs = 0;
		if (GetOneStepBeside(forward, GetElement(), GetOffset(), new_helm, new_ofs))
			Place(new_helm, new_ofs, FALSE, TRUE, TRUE);
#ifdef DOCUMENT_EDIT_SUPPORT
		else if (frames_doc->GetDocumentEdit() && frames_doc->GetDocumentEdit()->m_caret.IsElementEditable(GetElement()))
			frames_doc->GetDocumentEdit()->m_caret.MakeRoomAndMoveCaret(new_helm, new_ofs, forward);
#endif // DOCUMENT_EDIT_SUPPORT
		else
		{
			TextSelection *sel = frames_doc->GetTextSelection();
			SelectionBoundaryPoint caret_point(*sel->GetFocusPoint());
			frames_doc->SetSelection(&caret_point, &caret_point, FALSE);
		}
	}
}

BOOL CaretManager::GetOneStepBeside(BOOL forward, HTML_Element *helm, int ofs, HTML_Element *&new_helm, int &new_ofs, BOOL snap /* = TRUE */, BOOL accept_no_diff /* = FALSE */)
{
	HTML_Element *candidate_helm = NULL;
	int candidate_ofs = 0;
	if (!helm)
		return FALSE;

	// Try inside helm itself
	if (helm->Type() == HE_TEXT)
	{
		CaretManagerWordIterator iter(helm, frames_doc);
		if (OpStatus::IsSuccess(iter.GetStatus()) && iter.GetValidCaretOfsFrom(ofs, candidate_ofs, forward))
		{
			candidate_helm = helm;
		}
	}
	else if (helm->Type() != HE_BR && ((forward && ofs <= 0) || (!forward && ofs > 0)))
	{
		candidate_helm = helm;
		candidate_ofs = forward ? 1 : 0;
	}

	// Try outside of helm...
	if (!candidate_helm)
		if (!GetNearestCaretPos(frames_doc, helm, &candidate_helm, &candidate_ofs, forward, FALSE))
			return FALSE;

	// Make sure there is a "visible" difference if !accept_no_diff
	if (!accept_no_diff && candidate_helm != helm && IsOnSameLine(helm, candidate_helm, forward))
		if (!GetOneStepBeside(forward, candidate_helm, candidate_ofs, candidate_helm, candidate_ofs, FALSE, TRUE))
			return FALSE;

	// Possibly "snap"
	if (snap && candidate_helm->Type() != HE_BR && IsOnSameLine(helm, candidate_helm, forward))
	{
		if (candidate_helm->Type() != HE_TEXT && ((forward && candidate_ofs == 1) || (!forward && candidate_ofs == 0)))
		{
			HTML_Element *snap_helm = NULL;
			int snap_ofs = 0;
			if (GetNearestCaretPos(frames_doc, candidate_helm, &snap_helm, &snap_ofs, forward, FALSE))
			{
				if (snap_helm->Type() != HE_BR && IsOnSameLine(candidate_helm, snap_helm, forward))
				{
					candidate_helm = snap_helm;
					candidate_ofs = snap_ofs;
				}
			}
		}
	}

	new_helm = candidate_helm;
	new_ofs = candidate_ofs;
	return TRUE;
}

BOOL CaretManager::IsOnSameLine(HTML_Element *a, HTML_Element *b, BOOL a_first)
{
	return (a_first && IsTightlyConnectedInlineElements(a, b , b->Type() != HE_BR, TRUE)) ||
	      (!a_first && IsTightlyConnectedInlineElements(b, a , a->Type() != HE_BR, TRUE));
}


void CaretManager::Place(HTML_Element* helm, int ofs, BOOL prefer_first /* = TRUE */, BOOL allow_snap /* = TRUE */, BOOL keep_within_current_context /* = FALSE */, BOOL remember_x /* = TRUE */, BOOL set_selection /* = TRUE */)
{
#ifdef DOCUMENT_EDIT_SUPPORT
	if (frames_doc->GetDocumentEdit())
		frames_doc->GetDocumentEdit()->CheckLogTreeChanged();
#endif // DOCUMENT_EDIT_SUPPORT

	if (!helm)
	{
		frames_doc->GetCaretPainter()->StopBlink();
		return;
	}

#ifdef DOCUMENT_EDIT_SUPPORT
	if (!frames_doc->GetKeyboardSelectionMode()
#ifdef DRAG_SUPPORT
		|| frames_doc->GetCaretPainter()->IsDragCaret()
#endif // DRAG_SUPPORT
	   )
	{
		if (!IsElementEditable(frames_doc, helm) ||
			frames_doc->GetDocumentEdit() && !frames_doc->GetDocumentEdit()->m_caret.FixAndCheckCaretMovement(helm, ofs, allow_snap, keep_within_current_context))
		{
			frames_doc->GetCaretPainter()->StopBlink();
			return;
		}

		if (frames_doc->GetDocumentEdit())
			frames_doc->GetDocumentEdit()->m_caret.Set(helm, ofs, prefer_first, remember_x);
		else
		{
			SelectionBoundaryPoint caret_point(helm, ofs);
			caret_point.SetBindDirection(prefer_first ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);
			frames_doc->SetSelection(&caret_point, &caret_point);
		}

		if (frames_doc->GetDocumentEdit())
		{
			frames_doc->GetDocumentEdit()->m_caret.DeleteRemoveWhenMoveIfUntouched(helm);
			//frames_doc->GetDocumentEdit()->m_caret.StickToPreceding();
		}
		const BOOL update =
			frames_doc->GetCaretPainter()->UpdatePos();

		if (frames_doc->GetDocumentEdit())
		{
			frames_doc->GetDocumentEdit()->m_caret.RestartBlink();
			frames_doc->GetDocumentEdit()->ClearPendingStyles();
			if (update)
				frames_doc->GetDocumentEdit()->m_layout_modifier.Unactivate();
		}

		return;
	}
#endif // DOCUMENT_EDIT_SUPPORT

	if (set_selection)
	{
		SelectionBoundaryPoint caret_point(helm, ofs);
		caret_point.SetBindDirection(prefer_first ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);
		frames_doc->SetSelection(&caret_point, &caret_point);
	}
}


HTML_Element* CaretManager::GetCaretContainer(HTML_Element* search_origin)
{
#ifdef KEYBOARD_SELECTION_SUPPORT
	if (frames_doc->GetKeyboardSelectionMode() &&
		frames_doc->GetLogicalDocument() &&
		frames_doc->GetLogicalDocument()->GetBodyElm())
	{
		return frames_doc->GetLogicalDocument()->GetBodyElm();
	}
#endif // KEYBOARD_SELECTION_SUPPORT

#ifdef DOCUMENT_EDIT_SUPPORT
	if (frames_doc->GetDocumentEdit())
		return frames_doc->GetDocumentEdit()->GetEditableContainer(search_origin);
#endif // DOCUMENT_EDIT_SUPPORT

	return NULL;
}


BOOL CaretManager::GetContentRectOfElement(HTML_Element *elm, AffinePos &ctm, RECT &rect, BoxRectType type)
{
	if (!elm || !elm->GetLayoutBox())
		return FALSE;
	return elm->GetLayoutBox()->GetRect(frames_doc, type, ctm, rect);
}

HTML_Element* CaretManager::GetContainingElement(HTML_Element* elm)
{
	return Box::GetContainingElement(elm);
}

HTML_Element* CaretManager::GetContainingElementActual(HTML_Element* elm)
{
	HTML_Element* body = frames_doc->GetLogicalDocument()->GetBodyElm();

	elm = GetContainingElement(elm);
	while (elm && !elm->IsIncludedActual() && !elm->IsAncestorOf(body))
		elm = GetContainingElement(elm);

	if (elm && elm->IsAncestorOf(body))
		return body;
	else
		return elm;
}

CaretManagerWordIterator::CaretManagerWordIterator(HTML_Element* helm, FramesDocument* doc) :
		WordInfoIterator(doc, helm, &m_surround_checker),
		m_surround_checker(doc),
		m_status(OpStatus::OK),
		m_doc(doc),
		m_is_valid_for_caret(MAYBE)
{
	if (doc->GetDocRoot()->IsDirty())
		m_status = doc->Reflow(FALSE);

	if (OpStatus::IsSuccess(m_status))
		m_status = WordInfoIterator::Init();
}

/* virtual */
BOOL WordIteratorSurroundChecker::HasWsPreservingElmBeside(HTML_Element* helm, BOOL before)
{
	int i;
	HTML_Element *next = helm;
	if (!next)
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}
	for (;;)
	{
		next = before ? next->Prev() : next->Next();
		if (!next)
			return FALSE;
		if (before && !m_doc->GetCaret()->IsTightlyConnectedInlineElements(next, helm, TRUE, TRUE, FALSE))
			return FALSE;
		if (!before && !m_doc->GetCaret()->IsTightlyConnectedInlineElements(helm, next, TRUE, TRUE, TRUE))
			return FALSE;
		if (!next->GetLayoutBox())
			continue; // or return FALSE???
		if (CaretManager::IsReplacedElement(next))
			return TRUE;
		if (next->Type() != HE_TEXT || !next->GetLayoutBox()->IsTextBox())
			continue;
		Text_Box *next_box = static_cast<Text_Box*>(next->GetLayoutBox());
		WordInfo *words = next_box->GetWords();
		int count = next_box->GetWordCount();
		if (!words || !count)
			continue;
		if (before && words[count-1].HasTrailingWhitespace())
			return FALSE;
		for (i = 0; i < count; i++)
			if (words[i].GetLength() && !words[i].IsCollapsed())
				return TRUE;
	}
}

/** Returns TRUE if helm is a non-collapsed empty text-element. */
static BOOL IsNonCollapsedEmptyTextElm(HTML_Element *helm)
{
	Text_Box *box;
	if (!helm || helm->Type() != HE_TEXT || helm->GetTextContentLength() || !helm->GetLayoutBox() || !helm->GetLayoutBox()->IsTextBox())
		return FALSE;
	box = static_cast<Text_Box*>(helm->GetLayoutBox());
	return box->GetWordCount() == 1 && box->GetWords() && !box->GetWords()->IsCollapsed();
}

BOOL CaretManagerWordIterator::IsValidForCaret(BOOL valid_if_possible /* = FALSE */)
{
	if (m_is_valid_for_caret != MAYBE)
		return m_is_valid_for_caret == YES;
	/*
	  samuelp rocked :) if i understand this code correctly the
	  intention here is to assign correct state to
	  m_is_valid_for_caret and return the corresponding result using
	  only one statement.
	 */
	if (HasUnCollapsedChar())
		return (m_is_valid_for_caret = YES) == YES;
	if (!HasUnCollapsedWord())
		return (m_is_valid_for_caret = NO) != NO;
	if (valid_if_possible)
		return (m_is_valid_for_caret = YES) == YES;

	HTML_Element *tmp;
	for (tmp = GetElement()->NextActual(); tmp; tmp = tmp->NextActual())
	{
		if (tmp->Type() == HE_BR || !m_doc->GetCaret()->IsTightlyConnectedInlineElements(tmp, GetElement(), TRUE, FALSE, FALSE))
			break;
		if (tmp->Type() == HE_TEXT)
		{
			if (IsNonCollapsedEmptyTextElm(tmp))
			{
				if (GetFullLength())
					return (m_is_valid_for_caret = NO) != NO;
			}
			else
			{
				CaretManagerWordIterator iter(tmp, m_doc);
				if (iter.HasUnCollapsedChar())
					return (m_is_valid_for_caret = NO) != NO;
			}
		}
	}
	for (tmp = GetElement()->PrevActual(); tmp; tmp = tmp->PrevActual())
	{
		if (tmp->Type() == HE_BR || !m_doc->GetCaret()->IsTightlyConnectedInlineElements(GetElement(), tmp, TRUE, FALSE, FALSE))
			break;
		if (tmp->Type() == HE_TEXT)
		{
			if (IsNonCollapsedEmptyTextElm(tmp))
				return (m_is_valid_for_caret = NO) != NO;
			else
			{
				CaretManagerWordIterator iter(tmp, m_doc);
				if (iter.HasUnCollapsedWord() && !(!iter.HasUnCollapsedChar() && !GetFullLength() && iter.GetFullLength()))
					return (m_is_valid_for_caret = NO) != NO;
			}
		}
	}
	return (m_is_valid_for_caret = YES) == YES;
}

BOOL CaretManagerWordIterator::GetFirstValidCaretOfs(int &res_ofs)
{
	res_ofs = 0;
	if (!IsValidForCaret())
		return FALSE;
	if (HasUnCollapsedChar())
		res_ofs = FirstUnCollapsedOfs();
	return TRUE;
}

BOOL CaretManagerWordIterator::GetLastValidCaretOfs(int &res_ofs)
{
	res_ofs = 0;
	if (!IsValidForCaret())
		return FALSE;
	if (HasUnCollapsedChar())
		res_ofs = LastUnCollapsedOfs() + 1;
	return TRUE;
}

BOOL CaretManagerWordIterator::GetValidCaretOfsFrom(int ofs, int &res_ofs, BOOL forward)
{
	if (!IsValidForCaret())
		return FALSE;
	if (!HasUnCollapsedChar())
	{
		if ((forward && ofs >= 0) || (!forward && ofs <= 0))
			return FALSE;
		res_ofs = 0;
		return TRUE;
	}
#ifdef DEBUG_ENABLE_OPASSERT
	BOOL success = TRUE;
#endif // DEBUG_ENABLE_OPASSERT
	if (forward)
	{
		if (ofs > LastUnCollapsedOfs())
			return FALSE;
#ifdef DEBUG_ENABLE_OPASSERT
		success =
#endif // DEBUG_ENABLE_OPASSERT
			GetOfsSnappedToUnCollapsed(ofs, res_ofs, TRUE);
		OP_ASSERT(success);
		if (res_ofs == LastUnCollapsedOfs())
			res_ofs++;
		else
#ifdef DEBUG_ENABLE_OPASSERT
			success =
#endif // DEBUG_ENABLE_OPASSERT
				GetNextUnCollapsedOfs(res_ofs, res_ofs);
	}
	else
	{
		if (ofs <= FirstUnCollapsedOfs())
			return FALSE;
		if (ofs > LastUnCollapsedOfs())
		{
			res_ofs = LastUnCollapsedOfs();
			return TRUE;
		}
		if (IsCharCollapsed(ofs) && !IsPreFormatted())
#ifdef DEBUG_ENABLE_OPASSERT
			success =
#endif // DEBUG_ENABLE_OPASSERT
				GetOfsSnappedToUnCollapsed(ofs, res_ofs, FALSE);
		else
#ifdef DEBUG_ENABLE_OPASSERT
			success =
#endif // DEBUG_ENABLE_OPASSERT
				GetPrevUnCollapsedOfs(ofs, res_ofs);
	}
	OP_ASSERT(success);
	return TRUE;
}

BOOL CaretManagerWordIterator::SnapToValidCaretOfs(int ofs, int &res_ofs)
{
	res_ofs = 0;
	if (!IsValidForCaret())
		return FALSE;
	if (!HasUnCollapsedChar())
		return TRUE;
	if (ofs > LastUnCollapsedOfs())
		res_ofs = LastUnCollapsedOfs() + 1;
	else
		GetOfsSnappedToUnCollapsed(ofs, res_ofs, TRUE);
	return TRUE;
}

BOOL CaretManagerWordIterator::GetCaretOfsWithoutCollapsed(int ofs, int &res_ofs)
{
	res_ofs = 0;
	if (!IsValidForCaret())
		return FALSE;
	if (!HasUnCollapsedChar())
		return TRUE;
	if (ofs >= LastUnCollapsedOfs())
		res_ofs = UnCollapsedCount();
	else
	{
		SnapToValidCaretOfs(ofs, ofs);
		CollapsedToUnCollapsedOfs(ofs, res_ofs);
	}
	return TRUE;
}

#endif // KEYBOARD_SELECTION_SUPPORT || DOCUMENT_EDIT_SUPPORT
