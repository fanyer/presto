/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/selection.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/scrollable.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/doc/caret_manager.h"
#include "modules/doc/caret_painter.h"

/** Update the document's navigation element */

void
TextSelection::UpdateNavigationElement(FramesDocument* doc, HTML_Element* element)
{
	if (HTML_Document* html_doc = doc->GetHtmlDocument())
	{
		if (IsEmpty())
		{
			html_doc->SetNavigationElement(NULL, FALSE);
		}
		else
		{
			if (html_doc->GetNavigationElement() != element)
			{
				html_doc->SetNavigationElement(element, FALSE);

				HTML_Element* anchor_element = element->GetAnchorTags(doc);

				if (anchor_element)
					html_doc->SetNavigationElement(anchor_element, FALSE);
			}
		}
	}
}

/* static */ HTML_Element*
TextSelection::GetFirstElementAfterSelection(const SelectionBoundaryPoint& stop_point, BOOL exclude_text_stop)
{
	BOOL stop_is_text = stop_point.GetElement()->Type() == Markup::HTE_TEXT;
	HTML_Element* last_child_actual = NULL;

	int offset = stop_point.GetOffset();
	if (offset > 0)
	{
		last_child_actual = stop_point.GetElement()->FirstChildActualStyle();
		while (last_child_actual && offset > 1)
		{
			--offset;
			last_child_actual = last_child_actual->SucActualStyle();
		}
	}
	if (stop_is_text)
		return exclude_text_stop ? stop_point.GetElement() : stop_point.GetElement()->Next();
	else
		return last_child_actual ? last_child_actual->NextSibling() : stop_point.GetElement()->Next();
}

/* static */ void
TextSelection::ForEachElement(const SelectionBoundaryPoint& start_point, const SelectionBoundaryPoint& stop_point, TextSelection::ElementOperation& operation, BOOL exclude_text_start, BOOL exclude_text_stop)
{
	OP_ASSERT(start_point.GetElement());
	OP_ASSERT(stop_point.GetElement());
	OP_ASSERT(start_point.Precedes(stop_point) || (start_point.GetElement() == stop_point.GetElement() && start_point.GetOffset() == stop_point.GetOffset()));

	BOOL start_is_text = start_point.GetElement()->Type() == Markup::HTE_TEXT;

	HTML_Element* after_selection = GetFirstElementAfterSelection(stop_point, exclude_text_stop);

	HTML_Element* element = start_point.GetElement()->FirstChildActualStyle();
	HTML_Element* last_actual_before_selection = start_point.GetElement();
	if (element)
	{
		int offset = start_point.GetOffset();
		while (element && offset > 0)
		{
			--offset;
			last_actual_before_selection = element;
			element = element->SucActualStyle();
		}
	}

	BOOL element_is_before_selection = FALSE;

	if (!element)
	{
		if (start_is_text && !exclude_text_start)
			element = start_point.GetElement();
		else
		{
			HTML_Element* next_sibling = start_point.GetElement()->NextSiblingActualStyle();
			if (start_point.GetElement() == stop_point.GetElement() || !next_sibling)
				return; // Empty selection
			else
			{
				element_is_before_selection = TRUE;

				// There may be some non-actual-style elements between last_actual_before_selection
				// and the first element in selection (because selection starts at an actual-style
				// element). Skip them.
				HTML_Element* last_element_before_selection = last_actual_before_selection;
				do
				{
					element = last_element_before_selection;
					last_element_before_selection = last_element_before_selection->Next();
				} while(last_element_before_selection && last_element_before_selection != after_selection && !last_element_before_selection->IsIncludedActualStyle());
			}
		}
	}

	while (element && element != after_selection)
	{
		if (element_is_before_selection)
			element_is_before_selection = FALSE;
		else
			operation.Call(element);
		element = element->Next();
		OP_ASSERT(element || !after_selection || !"Loop is going too far most likely because non-actual elements weren't taken into account somewhere");
	}
}

/** A utility class for SetElementsIsInSelection.
 *
 * It sets element's is_in_selection flag to the given value, unless
 * the element is a list marker pseudo element or a child of one,
 */
class SetIsInSelectionOperation : public TextSelection::ElementOperation
{
private:
	BOOL m_select;
public:
	SetIsInSelectionOperation(BOOL select) : m_select(select) { }

	virtual void Call(HTML_Element* element)
	{
		if (!(element->GetIsListMarkerPseudoElement() || (element->Type() == Markup::HTE_TEXT && element->Parent()->GetIsListMarkerPseudoElement())))
			element->SetIsInSelection(m_select);
	}
};

/* static */ void
TextSelection::SetElementsIsInSelection(const SelectionBoundaryPoint& start, const SelectionBoundaryPoint& stop, BOOL value, BOOL exclude_text_start, BOOL exclude_text_stop)
{
	SetIsInSelectionOperation operation(value);
	ForEachElement(start, stop, operation, exclude_text_start, exclude_text_stop);
}

/** Clear selection */

void
TextSelection::Clear(FramesDocument* doc)
{
	if (doc->IsBeingDeleted())
		/* Don't waste time on traversal. The document (and the layout
		   structure) is going away soon anyway. It's not exactly safe to
		   traverse at this point either, since the FramesDocument is probably
		   partially destroyed already. */

		return;

	LogicalDocument* logdoc = doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot() && logdoc->GetRoot()->GetLayoutBox())
	{
		if (!IsEmpty())
		{
			// Unselect whole area

			SetElementsIsInSelection(start_selection, end_selection, FALSE);
			is_being_updated = TRUE;
			logdoc->GetLayoutWorkplace()->UpdateSelectionRange(&start_selection, &end_selection);
			is_being_updated = FALSE;
			NotifyDOMAboutSelectionChange(logdoc->GetFramesDocument());
		}

		SetFocusPoint(*GetAnchorPoint());
	}
}

/** Is selection empty? */

BOOL
TextSelection::IsEmpty()
{
	HTML_Element* start_elm = start_selection.GetElement();

	return !start_elm || start_selection == end_selection;
}

void
TextSelection::ValidateStartAndEndPoints()
{
	// Make sure that 'start_selection' precedes 'end_selection'

	if (end_selection.Precedes(start_selection))
	{
		// Swap and remember which selection point is the anchor one.

		SelectionBoundaryPoint tmp_selection = start_selection;
		start_selection = end_selection;
		end_selection = tmp_selection;

		start_is_anchor = !start_is_anchor;
	}
}

void
TextSelection::SetAnchorPoint(const SelectionBoundaryPoint& new_point)
{
	if (start_is_anchor)
		start_selection = new_point;
	else
		end_selection = new_point;

	ValidateStartAndEndPoints();
}

void
TextSelection::SetFocusPoint(const SelectionBoundaryPoint& new_point)
{
	if (start_is_anchor)
		end_selection = new_point;
	else
		start_selection = new_point;

	ValidateStartAndEndPoints();
}

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
void
TextSelection::SetBoundingRect(RECT& new_rect)
{
	bounding_rect.left = new_rect.left;
	bounding_rect.top = new_rect.top;
	bounding_rect.right = new_rect.right;
	bounding_rect.bottom = new_rect.bottom;
}
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT

/** Mark selection dirty. */

void
TextSelection::MarkDirty(HTML_Element* element)
{
	if (start_selection.GetElement())
		is_dirty = TRUE;
}

static BOOL IsElementOffsetLessThan(HTML_Element* element, int comparison_offset)
{
	// FIXME: THIS MAKES INSERTIONS AND REMOVALS OF n ELEMENTS O(n^2).
	int element_offset = 0;
	HTML_Element* count_it = element;
	while (element_offset < comparison_offset && (count_it = count_it->PredActualStyle()) != NULL)
		element_offset++;
	return element_offset < comparison_offset;
}

static SelectionBoundaryPoint GetElementRemovalAdjustedSelectionPoint(const SelectionBoundaryPoint& selection_point, HTML_Element* element_to_be_removed)
{
	if (element_to_be_removed->ParentActualStyle() == selection_point.GetElement())
	{
		// Might have to reduce the offset by one if the removed element had an offset less than offset.
		// FIXME: THIS MAKES REMOVAL OF n ELEMENTS O(n^2).
		if (IsElementOffsetLessThan(element_to_be_removed, selection_point.GetOffset()))
		{
			SelectionBoundaryPoint return_value = selection_point;
			return_value.SetLogicalPosition(return_value.GetElement(), return_value.GetOffset() - 1);
			return return_value;
		}
	}
	return selection_point;
}

/** Remove element from selection. */

void
TextSelection::RemoveElement(HTML_Element* element)
{
	BOOL start_is_going_away = start_selection.GetElement() && element->IsAncestorOf(start_selection.GetElement());
	BOOL end_is_going_away = end_selection.GetElement() && element->IsAncestorOf(end_selection.GetElement());
	if (start_is_going_away || end_is_going_away)
	{
		SelectionBoundaryPoint removal_point(element->ParentActualStyle(), element);
		if (!start_is_going_away || !end_is_going_away) // Part of the selection will remain
		{
			if (start_is_going_away)
				start_selection = removal_point;
			else
			{
				OP_ASSERT(end_is_going_away);
				end_selection = removal_point;
			}
		}
		else
		{
			start_selection = removal_point;
			end_selection = removal_point;
		}

		if (element)
			element->SetIsInSelection(FALSE);
	}

	// FIXME: Currently O(n^2) if inserting/removing n elements.
	if (!start_is_going_away)
		start_selection = GetElementRemovalAdjustedSelectionPoint(start_selection, element);
	if (!end_is_going_away)
		end_selection = GetElementRemovalAdjustedSelectionPoint(end_selection, element);
}

static SelectionBoundaryPoint GetElementInsertionAdjustedSelectionPoint(const SelectionBoundaryPoint& selection_point, HTML_Element* inserted_element)
{
	if (inserted_element->ParentActualStyle() == selection_point.GetElement())
	{
		if (IsElementOffsetLessThan(inserted_element, selection_point.GetOffset()))
		{
			SelectionBoundaryPoint return_value = selection_point;
			return_value.SetLogicalPosition(return_value.GetElement(), return_value.GetOffset() + 1);
			return return_value;
		}
	}
	return selection_point;
}


/** Insert element in selection. */

void
TextSelection::InsertElement(HTML_Element* element)
{
	// Adjust offsets in selection end points.
	// FIXME: THIS MAKES INSERTION OF n ELEMENTS USE O(n^2) TIME.
	start_selection = GetElementInsertionAdjustedSelectionPoint(start_selection, element);
	end_selection = GetElementInsertionAdjustedSelectionPoint(end_selection, element);

	// We will copy the selection flag from our neighbours. Safari takes pred&&suc,
	// MSIE takes suc, Mozilla fails completely
	// This will check suc (or rather NextSibling) since that
	// is cheapest.
	BOOL inserted_tree_is_selected = FALSE;

	HTML_Element* next_sibling = element->NextSibling();
	// Notice that (next_sibling->Parent()->IsAncestorOf(element) == TRUE) &&
	// (next_sibling->Pred() == element || next_sibling->Pred()->IsAncestorOf(element))
	if (next_sibling && next_sibling->IsInSelection())
	{
		// Check if the left edge of the element is selected
		HTML_Element *sel_start_elm = GetStartElement();

		// If the start of the selection is an element with no children, then we can simply check if the start-element
		// is the next sibling, if it's not, this implies that the start element is another element that must
		// be inserted before element in the tree, hence element must also be selected (it's located between
		// sel_start_elm and next sibling in the tree).
		if (!sel_start_elm || !sel_start_elm->FirstChild())
		{
			inserted_tree_is_selected = sel_start_elm != next_sibling;
		}
		// If the start of the selection is not a text-element, then is the element in the selection-point
		// the parent of the first selected element (and the offset is the "index" of the first selected child).
		else
		{
			// If sel_start_elm is not the parent of next_sibling, this simply implies that next_sibling is
			// NOT the first selected element, the last possible element in the tree that can be the start element
			// of the selection is then the element that was next_sibling->Prev() before element was inserted. This
			// element is now located Prev()ious element because next_sibling->Prev() is now element (or one of
			// element's children).
			if(sel_start_elm != next_sibling->Parent())
				inserted_tree_is_selected = TRUE;
			// If sel_start_elm is the parent of next_sibling, then let's check out if there is any selected element
			// Pred() of next_sibling (that is not element itself), if it is, this element must be element->Pred() of element
			// (see if(pred == element) check below to go to Pred() of element), or an ancestor of element. In both these cases
			// should element be selected.
			else
			{
				HTML_Element *pred = next_sibling->Pred();
				if(pred == element)
					pred = pred->Pred();
				inserted_tree_is_selected = pred && pred->IsInSelection();
			}
		}
	}

	HTML_Element* it = element;
	while (it != next_sibling)
	{
		it->SetIsInSelection(inserted_tree_is_selected);
		it = it->Next();
	}
}

static void MoveSelectionPointToTextGroup(SelectionBoundaryPoint& point)
{
	OP_ASSERT(point.GetElement()->Type() == Markup::HTE_TEXT);
	OP_ASSERT(point.GetElement()->ParentActualStyle()->Type() == Markup::HTE_TEXTGROUP);

	int offset = point.GetOffset();
	HTML_Element* elm = point.GetElement()->PredActualStyle();
	while (elm)
	{
		offset += elm->GetTextContentLength();
		elm = elm->PredActualStyle();
	}
	point.SetLogicalPosition(point.GetElement()->ParentActualStyle(), offset);
}

void TextSelection::BeforeTextDataChange(HTML_Element* text_node)
{
	OP_ASSERT(before_text_change_start_offset < 0 || !"The selection update state isn't in the 'before' state");
	// Temporarily we must let SelectionBoundaryPoints point to HE_TEXTGROUPS if needed.
	if (text_node->Type() == Markup::HTE_TEXTGROUP)
	{
		if (text_node->IsAncestorOf(start_selection.GetElement()))
		{
			MoveSelectionPointToTextGroup(start_selection);
			OP_ASSERT(start_selection.GetElement() == text_node);
		}

		if (text_node->IsAncestorOf(end_selection.GetElement()))
		{
			MoveSelectionPointToTextGroup(end_selection);
			OP_ASSERT(end_selection.GetElement() == text_node);
		}
	}

	if (text_node == start_selection.GetElement())
		before_text_change_start_offset = start_selection.GetOffset();

	if (text_node == end_selection.GetElement())
		before_text_change_end_offset = end_selection.GetOffset();
}

static void MoveSelectionIntoTextGroup(SelectionBoundaryPoint& point)
{
	int local_offset = point.GetOffset();

	HTML_Element* text = point.GetElement()->FirstChildActualStyle();

	while (text)
	{
		int length = text->GetTextContentLength();
		if (length >= local_offset)
		{
			point.SetLogicalPosition(text, local_offset);
			return;
		}

		local_offset -= length;
		text = text->SucActualStyle();
	}
}

void TextSelection::AbortedTextDataChange(HTML_Element* text_node)
{
	if (text_node->Type() == Markup::HTE_TEXTGROUP)
	{
		if (start_selection.GetElement() == text_node)
			MoveSelectionIntoTextGroup(start_selection);

		if (end_selection.GetElement() == text_node)
			MoveSelectionIntoTextGroup(end_selection);
	}
	before_text_change_start_offset = -1;
}


static void AdjustSelectionPoint(SelectionBoundaryPoint& point, HTML_Element::ValueModificationType modification_type, unsigned offset, unsigned length1, unsigned length2)
{
	OP_ASSERT(point.GetElement() && point.GetElement()->IsText());
	OP_ASSERT(point.GetOffset() >= 0);

	unsigned point_offset = static_cast<unsigned>(point.GetOffset());
	HTML_Element* point_element = point.GetElement();

	switch (modification_type)
	{
	case HTML_Element::MODIFICATION_DELETING:
		if (point_offset > offset)
		{
			if (offset + length1 <= point_offset)
				point_offset -= length1;
			else
				point_offset = offset;
		}
		break;

	case HTML_Element::MODIFICATION_INSERTING:
		if (point_offset > offset)
			point_offset += length1;
		break;

	case HTML_Element::MODIFICATION_REPLACING:
		if (point_offset > offset)
		{
			if (point_offset <= offset + length1)
				point_offset = offset;
			else
				point_offset += length2 - length1;
		}
		break;

	case HTML_Element::MODIFICATION_SPLITTING:
		{
			OP_ASSERT(point_element->SucActualStyle()->IsText());

			if (point_offset > offset)
			{
				point_element = point_element->SucActualStyle();
				point_offset -= offset;
			}
		}
		break;

	case HTML_Element::MODIFICATION_REPLACING_ALL:
		{
			unsigned max_length = point_element->GetTextContentLength();

			if (point_offset > max_length)
				point_offset = max_length;
		}
	}

	point.SetLogicalPosition(point_element, static_cast<int>(point_offset));
	if (point_element->Type() == Markup::HTE_TEXTGROUP)
		MoveSelectionIntoTextGroup(point);
}

void TextSelection::TextDataChange(HTML_Element* text_element, HTML_Element::ValueModificationType modification_type, unsigned offset, unsigned length1, unsigned length2)
{
	if (start_selection.GetElement() == text_element)
	{
		start_selection.SetLogicalPosition(start_selection.GetElement(), before_text_change_start_offset);
		AdjustSelectionPoint(start_selection, modification_type, offset, length1, length2);
	}

	if (end_selection.GetElement() == text_element)
	{
		end_selection.SetLogicalPosition(end_selection.GetElement(), before_text_change_end_offset);
		AdjustSelectionPoint(end_selection, modification_type, offset, length1, length2);
	}

	before_text_change_start_offset = -1;
}

void TextSelection::OnTextConvertedToTextGroup(HTML_Element* elm)
{
	OP_ASSERT(elm->FirstChild() && elm->FirstChild()->Type() == HE_TEXT || !"The textgroup must have an equvalent new element as child");

	// If before_text_change_start_offset is != -1 then we're
	// right now in the process of rebuilding a text node and
	// we better let the selection points stay where they are
	// to not harm the TextSelection::TextDataChange process.
	if (before_text_change_start_offset < 0)
	{
		if (start_selection.GetElement() == elm)
		{
			start_selection.SetLogicalPosition(elm->FirstChild(), start_selection.GetOffset());
			elm->FirstChild()->SetIsInSelection(TRUE);
			elm->SetIsInSelection(FALSE);
		}

		if (end_selection.GetElement() == elm)
		{
			end_selection.SetLogicalPosition(elm->FirstChild(), end_selection.GetOffset());
			elm->FirstChild()->SetIsInSelection(TRUE);
		}
	}
}

/** Find if there is a container of helm that should contain the selection. */

HTML_Element* GetSelectionContainer(HTML_Element* helm)
{
	helm = helm->ParentActual();
	while (helm)
	{
		if (Box* box = helm->GetLayoutBox())
			if (ScrollableArea* scrollable = box->GetScrollable())
				if (scrollable->HasHorizontalScrollbar() || scrollable->HasVerticalScrollbar())
					return helm;

#ifdef DOCUMENT_EDIT_SUPPORT
		if (helm->IsContentEditable())
			return helm;
#endif
		helm = helm->ParentActual();
	}
	return NULL;
}

OP_STATUS
TextSelection::SetCollapsedPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y,
								  BOOL find_nearest, BOOL only_alphanum)
{
	OP_NEW_DBG("TextSelection::SetCollapsedPosition", "selection");
	LogicalDocument* logdoc = doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot() && logdoc->GetRoot()->GetLayoutBox())
	{
		TextSelectionObject text_selection(doc, x, y, x, y,
										   find_nearest,
										   only_alphanum);

		text_selection.SetSelectionContainer(NULL);

		RETURN_IF_ERROR(text_selection.Traverse(logdoc->GetRoot()));

		if (text_selection.HasNearestBoundaryPoint())
		{
			// Set 'new_focus_point' to the new position

			SelectionBoundaryPoint new_focus_point = text_selection.GetNearestBoundaryPoint();

			OP_DBG(("Selection traversal found cursor to be at (%p, %d)", new_focus_point.GetElement(), new_focus_point.GetElementCharacterOffset()));

			new_focus_point.SetBindDirection(text_selection.IsAtEndOfLine() ? SelectionBoundaryPoint::BIND_BACKWARD : SelectionBoundaryPoint::BIND_FORWARD);

			// This is the initial collapsed position - set both start and end points (both focus and anchor).

			start_selection = new_focus_point;
			end_selection = new_focus_point;

			if (text_selection.HasNearestBoundaryPoint())
			{
				HTML_Element* old_style_elm;
				int old_style_offset;
				TextSelection::ConvertPointToOldStyle(text_selection.GetNearestBoundaryPoint(), old_style_elm, old_style_offset);
				UpdateNavigationElement(doc, old_style_elm);
			}
			else
				UpdateNavigationElement(doc, NULL);

			NotifyDOMAboutSelectionChange(doc);

#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
			CaretPainter* caret_painter = doc->GetCaretPainter();
			if (doc->GetTextSelection() == this && caret_painter)
			{
				caret_painter->UpdatePos();
				caret_painter->UpdateWantedX();
				caret_painter->RestartBlink();
			}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT
		}
		else
		{
			/* This is the first click and if we couldn't make it to a
			   selection start point, we save it for use when we do have a
			   selection point. */

			previous_point.x = x;
			previous_point.y = y;
		}
	}

	return OpStatus::OK;
}

void
TextSelection::SetNewPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y,
							  const SelectionBoundaryPoint* moving_point)
{
	OP_NEW_DBG("TextSelection::SetNewPosition", "selection");
	LogicalDocument* logdoc = doc->GetLogicalDocument();

	OP_ASSERT(moving_point == &start_selection || moving_point == &end_selection);

	const SelectionBoundaryPoint* anchor_point = *moving_point == start_selection ? &end_selection : &start_selection;
	const SelectionBoundaryPoint* focus_point = moving_point;

	if (logdoc && logdoc->GetRoot() && logdoc->GetRoot()->GetLayoutBox())
	{
		HTML_Element* selection_container = NULL;

		const SelectionBoundaryPoint *start_point = anchor_point->Precedes(*focus_point) ? anchor_point : focus_point;
		const SelectionBoundaryPoint *end_point = start_point == anchor_point ? focus_point : anchor_point;

		if (start_point->GetElement())
		{
			HTML_Element* container = selection_container = GetSelectionContainer(anchor_point->GetElement());

#ifdef DOCUMENT_EDIT_SUPPORT
			if (!container && doc->GetDesignMode() && doc->GetDocumentEdit())
				container = doc->GetDocumentEdit()->GetBody();
#endif // DOCUMENT_EDIT_SUPPORT

			if (container && container->GetLayoutBox())
			{
				// Keep x and y within the rect of the container so we will keep selecting (inside the box) when dragging outside it.

				RECT container_rect;
				if (container->GetLayoutBox()->GetRect(doc, PADDING_BOX, container_rect))
				{
					x = MAX(LayoutCoord(container_rect.left), x);
					x = MIN(LayoutCoord(container_rect.right - 1), x);
					y = MAX(LayoutCoord(container_rect.top), y);
					y = MIN(LayoutCoord(container_rect.bottom - 1), y);
				}
			}
		}

		LayoutCoord previous_point_x = previous_point.x ? LayoutCoord(previous_point.x) : x;
		LayoutCoord previous_point_y = previous_point.y ? LayoutCoord(previous_point.y) : y;

		TextSelectionObject text_selection(doc, x, y,
										   previous_point_x,
										   previous_point_y);

		text_selection.SetSelectionContainer(selection_container);

		if (OpStatus::IsMemoryError(text_selection.Traverse(logdoc->GetRoot())))
		{
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}

		BOOL was_selected = end_point->GetElement() != NULL;

		if (text_selection.HasNearestBoundaryPoint())
		{
			OP_ASSERT(anchor_point == focus_point || focus_point->Precedes(*anchor_point) || anchor_point->Precedes(*focus_point) || *anchor_point == *focus_point);
			SetNewPosition(
				doc,
				anchor_point, focus_point,
				text_selection.GetNearestBoundaryPoint(),
				text_selection.GetNearestWord(),
				text_selection.GetNearestWordCharacterOffset(),
				text_selection.GetNearestTextBoxX());
		}
		else if (!was_selected)
		{
			/* If this was the first click we got but we couldn't
			   make it to a selection start point, we save it
			   for use when we do have a selection point. */

			previous_point.x = x;
			previous_point.y = y;
		}
	}
}

void
TextSelection::SetNewPosition(FramesDocument* doc,
							  const SelectionBoundaryPoint* anchor_point,
							  const SelectionBoundaryPoint* focus_point,
							  const SelectionBoundaryPoint& nearest_boundary_point,
							  const uni_char* nearest_word, int nearest_word_char_offset,
							  long nearest_text_box_x)
{
	OP_NEW_DBG("TextSelection::SetNewPosition (internal)", "selection");
	OP_ASSERT(anchor_point == focus_point || focus_point->Precedes(*anchor_point) || anchor_point->Precedes(*focus_point) || *anchor_point == *focus_point);

	// Only if we found a text box ...

	BOOL was_selected = end_selection.GetElement() != NULL;

	// If new area is selected or unselected

	BOOL select = TRUE;

	// Logical start and end of changed area

	const SelectionBoundaryPoint* start_changed_range;
	const SelectionBoundaryPoint* end_changed_range;

	// We no longer need to consider the previous pointer when we have found a point

	previous_point.x = LayoutCoord(0);
	previous_point.y = LayoutCoord(0);

	// Set 'new_focus_point' to the new position

	SelectionBoundaryPoint new_focus_point = nearest_boundary_point;

	OP_DBG(("Selection traversal found cursor to be at (%p, %d)", new_focus_point.GetElement(), new_focus_point.GetElementCharacterOffset()));

	// It's possible that the new focus point (specified to this function)
	// is the old anchor point, adjust accordingly.
	BOOL start_is_moving = focus_point == &start_selection;

	BOOL update_selection = !was_selected || !(new_focus_point == *focus_point);

	//OP_DBG(("Update selection after moving to (%d, %d): %s", x, y, update_selection ? "yes" : "no"));

	if (update_selection)
	{
		const SelectionBoundaryPoint* toggle_point = NULL; // In case of an inverted selection, this indicates where the inversion point is (the selection anchor).

		if (!was_selected)
		{
			// This is an initial empty selection.

			start_changed_range = &new_focus_point;
			end_changed_range = &new_focus_point;
		}
		else
		{
			/* start_is_moving == TRUE -> start is focus point, end is anchor point
			   start_is_moving == FALSE -> start is anchor point, end is focus point */

			if (new_focus_point.Precedes(*focus_point))
			{
				// Moved backward from previous end position

				OP_DBG(("Moved backward from previous end position"));

				start_changed_range = &new_focus_point;
				end_changed_range = focus_point;
				select = TRUE;
				if (!start_is_moving)
				{
					if (new_focus_point.Precedes(*anchor_point)) // Inverted the selection. Need to select one part and clear another
						toggle_point = anchor_point;
					else // shrinking a selection
						select = FALSE;
				}
			}
			else
			{
				// Moved forward from previous end position

				OP_DBG(("Moved forward from previous end position"));

				start_changed_range = focus_point;
				end_changed_range = &new_focus_point;
				select = FALSE;
				if (!start_is_moving) // Just extended a selection forward
					select = TRUE;
				else if (anchor_point->Precedes(new_focus_point)) // Inverted the selection. Need to deselect one part and select another
					toggle_point = anchor_point;
			}
		}

		if (start_changed_range != end_changed_range)
		{
			if (toggle_point)
			{
				SetElementsIsInSelection(*toggle_point, *end_changed_range, !select, select, FALSE);
				SetElementsIsInSelection(*start_changed_range, *toggle_point, select, FALSE, !select);
			}
			else
				SetElementsIsInSelection(*start_changed_range, *end_changed_range, select, !start_is_moving && !select, start_is_moving && !select);

			is_being_updated = TRUE;

			// Update changed area

			doc->GetLayoutWorkplace()->UpdateSelectionRange(start_changed_range, end_changed_range);

			is_being_updated = FALSE;

			NotifyDOMAboutSelectionChange(doc);
		}

		if (was_selected)
		{
			// Update start or end selection point to the new end point

			if (start_is_moving == start_is_anchor)
				SetAnchorPoint(new_focus_point);
			else
				SetFocusPoint(new_focus_point);
		}
		else
		{
			// This was the first time - set both start and end points

			start_selection = new_focus_point;
			end_selection = new_focus_point;
		}

		HTML_Element* old_style_element;
		int old_style_offset;
		ConvertPointToOldStyle(nearest_boundary_point, old_style_element, old_style_offset);
		UpdateNavigationElement(doc, old_style_element);
	}
#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
	CaretPainter* caret_painter = doc->GetCaretPainter();
	if (doc->GetTextSelection() == this && caret_painter)
	{
		caret_painter->UpdatePos();
		caret_painter->UpdateWantedX();
		caret_painter->RestartBlink();
	}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT
}

void
TextSelection::SetAnchorPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y)
{
	SetNewPosition(doc, x, y, GetAnchorPoint());
}

void
TextSelection::SetFocusPosition(FramesDocument* doc, LayoutCoord x, LayoutCoord y, BOOL end_selection)
{
	SetNewPosition(doc, x, y, GetFocusPoint());

	// If this is our last chance to set the selection (end_selection == TRUE) and
	// we still haven't decided a point, then put the selection in body+0.
	if (end_selection && !start_selection.GetElement())
	{
		if (doc->GetLogicalDocument() && doc->GetLogicalDocument()->GetBodyElm())
		{
			SelectionBoundaryPoint start_body;
			start_body.SetLogicalPosition(doc->GetLogicalDocument()->GetBodyElm(), 0);
			SetNewSelection(doc, &start_body, &start_body, TRUE, FALSE, TRUE);
		}
	}
}

/* static */ OP_STATUS
TextSelection::ReflowDocument(FramesDocument* doc)
{
	LayoutWorkplace* workplace = doc->GetLayoutWorkplace();

	// Let's not mess up the yielding here
	if (workplace->GetYieldElement())
		return OpStatus::ERR;

	BOOL can_yield = workplace->CanYield();
	workplace->SetCanYield(FALSE);

	HTML_Element* root = doc->GetDocRoot();

	OP_STATUS status = OpStatus::OK;

	TextSelection* sel = doc->GetTextSelection();

	if (sel)
		sel->is_being_updated = TRUE;

	if (root->IsDirty() || root->HasDirtyChildProps() || root->IsPropsDirty())
		status = doc->Reflow(FALSE, TRUE);

	if (sel)
		sel->is_being_updated = FALSE;

	workplace->SetCanYield(can_yield);
	return status;
}

void TextSelection::NotifyDOMAboutSelectionChange(FramesDocument* doc)
{
#ifndef HAS_NOTEXTSELECTION
#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
	if (GetHighlightType() == TextSelection::HT_SELECTION)
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
		doc->DOMOnSelectionUpdated();
#endif // !HAS_NOTEXTSELECTION
}

static int CountSelectionChildren(HTML_Element* element)
{
	int count = 0;
	element = element->FirstChildActualStyle();
	while (element)
	{
		count++;
		element = element->SucActualStyle();
	}
	return count;
}

/** Set new selection to whole element */

void
TextSelection::SetNewSelection(FramesDocument* doc, HTML_Element* element, BOOL mark_tree_selected, BOOL req_inside_doc, BOOL do_visual_selection)
{
	OP_ASSERT(doc->GetLogicalDocument());
	if (do_visual_selection)
	{
		SelectionBoundaryPoint visual_start, visual_end;
		is_being_updated = TRUE;
		OP_STATUS status = doc->GetLayoutWorkplace()->GetVisualSelectionRange(element, req_inside_doc, visual_start, visual_end);
		is_being_updated = FALSE;
		RETURN_VOID_IF_ERROR(status);
		start_selection = visual_start;
		end_selection = visual_end;
	}
	else
	{
		HTML_Element* start_element;
		HTML_Element* end_element;
		if (element->Type() == HE_DOC_ROOT)
		{
			// Our internal way of saying "everything".
			if (!element->FirstChildActualStyle())
				return;
			start_element = element->FirstChildActualStyle();
			end_element = element->LastChildActualStyle();
		}
		else
		{
			start_element = element;
			end_element = element;
		}
		start_selection.SetLogicalPosition(start_element, 0);
		int end_offset;
		if (end_element->Type() == HE_TEXT)
			end_offset = end_element->ContentLength();
		else
			end_offset = CountSelectionChildren(end_element);
		end_selection.SetLogicalPosition(end_element, end_offset);
	}

	if (start_selection.GetElement())
	{
		if (mark_tree_selected)
			SetElementsIsInSelection(start_selection, end_selection, TRUE);

		is_being_updated = TRUE;

		/* Traverse to invalidate the selection.
			Reflow is not allowed before
			traversal and shouldn't be needed either.  The layout tree
			should be intact even before start_selection and
			end_selection is selected. */

		doc->GetLogicalDocument()->GetLayoutWorkplace()->UpdateSelectionRange(&start_selection, &end_selection);

		is_being_updated = FALSE;

		NotifyDOMAboutSelectionChange(doc);

#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
		CaretPainter* caret_painter = doc->GetCaretPainter();
		if (doc->GetTextSelection() == this && caret_painter)
		{
			caret_painter->UpdatePos();
			caret_painter->UpdateWantedX();
			caret_painter->RestartBlink();
		}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT
	}
}

/** Set new selection to new selection points */

void
TextSelection::SetNewSelection(FramesDocument* doc, const SelectionBoundaryPoint* start, const SelectionBoundaryPoint* end, BOOL update/*=TRUE*/, BOOL remember_old_wanted_x_position, BOOL end_is_focus)
{
	OP_ASSERT(start && start->GetElement());
	OP_ASSERT(end && end->GetElement());
	OP_ASSERT(start != &start_selection || !"You cannot use pointers to the TextSelection internal objects when setting selections#1");
	OP_ASSERT(start != &end_selection || !"You cannot use pointers to the TextSelection internal objects when setting selections#2");
	OP_ASSERT(end != &start_selection || !"You cannot use pointers to the TextSelection internal objects when setting selections#3");
	OP_ASSERT(end != &end_selection || !"You cannot use pointers to the TextSelection internal objects when setting selections#4");

	start_selection = *start;
	end_selection = *end;
	start_is_anchor = end_is_focus;

	OP_ASSERT(!end->Precedes(*start));
	if (!(start_selection == end_selection))
	{
		SetElementsIsInSelection(*start, *end, TRUE);

#if defined SEARCH_MATCHES_ALL && !defined HAS_NO_SEARCHTEXT
		if (update)
#endif // SEARCH_MATCHES_ALL && !HAS_NO_SEARCHTEXT
		{
			is_being_updated = TRUE;
			doc->GetLogicalDocument()->GetLayoutWorkplace()->UpdateSelectionRange(&start_selection, &end_selection, &bounding_rect);
			is_being_updated = FALSE;

			NotifyDOMAboutSelectionChange(doc);

			UpdateNavigationElement(doc, start->GetElement());
		}
	}
#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
	CaretPainter* caret_painter = doc->GetCaretPainter();
	if (doc->GetTextSelection() == this && caret_painter)
	{
		caret_painter->UpdatePos();
		if (!remember_old_wanted_x_position)
			caret_painter->UpdateWantedX();
		caret_painter->RestartBlink();
#ifdef DOCUMENT_EDIT_SUPPORT
		if (!doc->GetKeyboardSelectionMode() && doc->GetDocumentEdit())
		{
			HTML_Element* focus_point_elm = doc->GetCaret()->GetElement();
			if (!doc->GetDocumentEdit()->IsFocused() ||
				!doc->GetDocumentEdit()->IsElementValidForCaret(focus_point_elm))
			{
				caret_painter->StopBlink();
			}
		}
#endif // DOCUMENT_EDIT_SUPPORT
	}
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

	NotifyDOMAboutSelectionChange(doc);
}

#ifndef HAS_NOTEXTSELECTION
/** Set new selection to new selection points */

void
TextSelection::SetNewSelectionPoints(FramesDocument* doc, const SelectionBoundaryPoint& anchor, const SelectionBoundaryPoint& focus, BOOL update/*=TRUE*/, BOOL remember_old_wanted_x_position)
{
	OP_ASSERT(anchor.GetElement());
	OP_ASSERT(focus.GetElement());

	if (anchor.Precedes(focus))
		SetNewSelection(doc, &anchor, &focus, update, remember_old_wanted_x_position, TRUE);
	else
		SetNewSelection(doc, &focus, &anchor, update, remember_old_wanted_x_position, FALSE);
}
#endif // !HAS_NOTEXTSELECTION


OP_STATUS
TextSelection::GetSelectionPointPosition(FramesDocument *frames_doc, const SelectionBoundaryPoint &sel_pt, OpPoint &doc_pt, int &line_height)
{
	HTML_Element* old_style_element;
	int old_style_offset;
	ConvertPointToOldStyle(sel_pt, old_style_element, old_style_offset);
	RECT text_rect;
	if (old_style_element && old_style_element->GetLayoutBox() &&
		old_style_element->GetLayoutBox()->GetRect(frames_doc, CONTENT_BOX, text_rect,
													old_style_offset,
													old_style_offset))
	{
		doc_pt.x = text_rect.left;
		doc_pt.y = text_rect.top;
		line_height = text_rect.bottom - text_rect.top;

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_STATUS
TextSelection::GetAnchorPointPosition(FramesDocument *frames_doc, OpPoint &pos, int &line_height)
{
	return GetSelectionPointPosition(frames_doc, *GetAnchorPoint(), pos, line_height);
}

OP_STATUS
TextSelection::GetFocusPointPosition(FramesDocument *frames_doc, OpPoint &pos, int &line_height)
{
	return GetSelectionPointPosition(frames_doc, *GetFocusPoint(), pos, line_height);
}



/** Expand selection to given type */

void
TextSelection::ExpandSelection(FramesDocument* doc, TextSelectionType selection_type)
{
	if ((selection_type != TEXT_SELECTION_WORD || type != TEXT_SELECTION_CHARACTERS) &&
		(selection_type != TEXT_SELECTION_SENTENCE || type != TEXT_SELECTION_WORD) &&
		(selection_type != TEXT_SELECTION_PARAGRAPH || type != TEXT_SELECTION_SENTENCE))
		return;

	HTML_Element* root = doc->GetDocRoot();
	if (start_selection.GetElement() && end_selection.GetElement() && root)
	{
		ExpandSelectionObject expand_selection(doc, start_selection, end_selection, selection_type);

		if (OpStatus::IsMemoryError(expand_selection.Traverse(root)))
		{
			doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}

		if (expand_selection.GetFinished())
		{
			type = selection_type;
			SetNewSelection(doc, expand_selection.GetStartSelection(), expand_selection.GetEndSelection(), TRUE, FALSE, start_is_anchor);
		}
	}
}

/** Get selected text.

	Returns number of characters selected and a copy in 'buffer' if it's
	not NULL. */

int
TextSelection::GetSelectionAsText(FramesDocument* doc, uni_char* buffer, int buffer_length, BOOL unused, BOOL blockquotes_as_text)
{
	LogicalDocument* logdoc = doc->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot() && logdoc->GetRoot()->GetLayoutBox())
	{
		SelectionTextCopyObject selection_copy(doc, &start_selection, &end_selection, buffer, buffer_length, blockquotes_as_text);

		OpStatus::Ignore(selection_copy.Traverse(logdoc->GetRoot()));

		int length = selection_copy.GetLength();

		if (buffer)
		{
			if (length >= buffer_length)
				length = buffer_length - 1;

			if (length >= 0)
				buffer[length] = '\0';

			for (int i=0; i<length; i++)
			{
				switch (buffer[i])
				{
				case NBSP_CHAR:
					// Replace nbsp's with a regular space in the text. Some editors can
					// not properly handle the nbsp character
					buffer[i] = ' ';
					break;
				}
			}
		}

		return length;
	}

	if (buffer && buffer_length >= 1)
		buffer[0] = '\0';

	return 0;
}

/** Copies all members of ts. */

void
TextSelection::Copy(TextSelection *ts)
{
	start_selection = ts->start_selection;
	end_selection = ts->end_selection;

	start_is_anchor = ts->start_is_anchor;
	type = ts->type;
	form_object = ts->form_object;
	is_dirty = ts->is_dirty;

	bounding_rect.bottom = ts->bounding_rect.bottom;
	bounding_rect.left = ts->bounding_rect.left;
	bounding_rect.right = ts->bounding_rect.right;
	bounding_rect.top = ts->bounding_rect.top;
}


// COPIED FROM DOM - TO BE MERGED AFTER stighal's DOM RANGE FIXES HAVE LANDED

static HTML_Element*
logdoc_DOM_GetOffsetElement(HTML_Element *parent, int offset)
{
	HTML_Element *iter = parent->FirstChildActual();
	while (iter && offset > 0)
	{
		offset--;
		iter = iter->SucActual();
	}

	return iter;
}

// COPIED FROM DOM - TO BE MERGED AFTER stighal's DOM RANGE FIXES HAVE LANDED

/* This function returns TRUE for all other elements then HE_TEXT, HE_BR and
   elements with replaced content. */
static BOOL
logdoc_AvoidUsingAsBoundaryPoint(HTML_Element *helm)
{
	if (helm->Type() == HE_TEXT || helm->Type() == HE_BR)
		return FALSE;

	// Important to allow selections after HE_A so that it becomes possible to write text after a link.
	// See the implementation of OpDocumentEdit::InsertText() for all the details.
	if (helm->IsMatchingType(HE_A, NS_HTML))
		return FALSE;

	if (Box *box = helm->GetLayoutBox())
		return !box->IsContentReplaced();

	/* Maybe the element didn't have any layout-box, so let's use the "backup
	   solution." */
	switch(helm->Type())
	{
	case HE_IMG:
	case HE_INPUT:
	case HE_IFRAME:
#ifdef MEDIA_HTML_SUPPORT
	case HE_VIDEO:
#endif // MEDIA_HTML_SUPPORT
		return FALSE;

	default:
		return TRUE;
	}
}

/* This function returns TRUE for all other elements then HE_TEXT, HE_BR and
   elements with replaced content. */
BOOL
logdoc_DOM_AvoidUsingAsBoundaryPoint_newer(HTML_Element *helm)
{
	if (!helm)
		return TRUE;

	Box *box = helm->GetLayoutBox();
	if (!box)
		return TRUE;

	if ((helm->Type() == HE_TEXT && helm->ContentLength() > 0) || helm->Type() == HE_BR)
		return FALSE;

	return !box->IsContentReplaced();
}

// COPIED FROM DOM - TO BE MERGED AFTER stighal's DOM RANGE FIXES HAVE LANDED

static void
logdoc_DOM_AdjustBoundaryPoint(HTML_Element *&element, unsigned &offset)
{
	HTML_Element* first_child;
	if (element->Type() != HE_TEXT && (first_child = element->FirstChildActual()) != NULL)
	{
		if (element->Type() == HE_TEXTGROUP)
		{
			HTML_Element *stop = element->NextSiblingActual();
			HTML_Element *iter = first_child;
			while (iter != stop)
			{
				if (iter->Type() == HE_TEXT)
				{
					unsigned len = iter->GetTextContentLength();
					if (offset > len)
						offset -= len;
					else
					{
						element = iter;
						return;
					}
				}
				iter = iter->NextActual();
			}

			return;
		}

		element = first_child;
		HTML_Element* suc;
		while (offset > 0 && (suc = element->SucActual()) != NULL)
			--offset, element = suc;

		OP_ASSERT(offset == 0 || offset == 1);

		HTML_Element *iter = element;

		if (logdoc_AvoidUsingAsBoundaryPoint(iter))
		{
			if (offset == 0)
				iter = iter->NextActual();
			else while (HTML_Element *lastchild = iter->LastChildActual())
			{
				iter = lastchild;
				if (!logdoc_AvoidUsingAsBoundaryPoint(iter))
					break;
			}

			while (iter && element->IsAncestorOf(iter) && logdoc_AvoidUsingAsBoundaryPoint(iter))
				iter = offset == 0 ? iter->NextActual() : iter->PrevActual();
		}

		if (iter && (iter->Type() == HE_TEXT || iter->IsMatchingType(HE_BR, NS_HTML)))
		{
			element = iter;
			if (offset != 0 && iter->Type() == HE_TEXT)
				offset = element->GetTextContentLength();
		}
	}
}

static BOOL
logdoc_DOM_AdjustBoundaryPoint_newer(HTML_Element*& element, unsigned& offset, HTML_Element* const stop_point_element, const unsigned stop_point_offset, const BOOL forward)
{
	int text_offset = offset;
	HTML_Element *unit = element;
	if (unit->IsText() || unit->Type() == Markup::HTE_COMMENT)
	{
		HTML_Element *unit_iter = unit;
		HTML_Element *stop = unit->NextSiblingActual();
		HTML_Element *last = unit;

		if (unit->Type() == Markup::HTE_TEXTGROUP)
			unit_iter = unit->FirstChildActual();

		while (unit_iter != stop)
		{
			last = unit_iter;
			if (unit_iter->Type() == HE_TEXT)
			{
				int len = unit_iter->GetTextContentLength();
				if (text_offset > len)
					text_offset -= len;
				else
				{
					// check that it is laid out
					if (unit_iter->GetLayoutBox() && len > 0)
					{
						element = unit_iter;
						offset = text_offset;
						return FALSE;
					}
					break;
				}
			}
			unit_iter = unit_iter->NextActual();
		}

		// We have found the text offset and it is not displayed, or it
		// doesn't exist, so we have to continue searching for an element,
		// between point and stopPoint, that is displayed and set the
		// logical start point at the the start or end of that element
		// depending on the direction of our search.
		text_offset = forward ? 0 : INT_MAX;
		unit = last;
	}
	else
	{
		HTML_Element *candidate = logdoc_DOM_GetOffsetElement(unit, text_offset);
		if (candidate)
		{
			unit = candidate;
			text_offset = 0;
		}
		else
		{
			candidate = text_offset == 0 ? unit->NextSiblingActual() : unit->LastLeafActual();

			if (candidate)
				unit = candidate;
		}
	}

	BOOL passed_end = FALSE;
	if (unit && logdoc_DOM_AvoidUsingAsBoundaryPoint_newer(unit))
	{
		HTML_Element *stop_element = logdoc_DOM_GetOffsetElement(stop_point_element, stop_point_offset);
		if (stop_element == unit)
		{
			passed_end = TRUE;
			stop_element = NULL;
		}
		else if (!stop_element)
			stop_element = forward ? stop_element->NextSiblingActual() : stop_element->PrevActual();

		if (text_offset == 0)
		{
			if (forward)
				unit = unit->NextActual();
			else
			{
				unit = unit->PrevActual();
				text_offset = INT_MAX;
			}
		}
		else if (HTML_Element *leaf = unit->LastLeafActual())
			unit = leaf;
		else
			unit = unit->PrevActual();

		while (logdoc_DOM_AvoidUsingAsBoundaryPoint_newer(unit))
		{
			if (unit == stop_element)
				passed_end = TRUE;

			if (!unit)
				break;

			unit = text_offset == 0 ? unit->NextActual() : unit->PrevActual();
		}
	}

	if (unit)
	{
		if (text_offset != 0)
		{
			if (unit->Type() == Markup::HTE_TEXT)
				text_offset = unit->GetTextContentLength();
			else
				text_offset = 1;
		}
		element = unit;
		offset = text_offset;
	}

	return passed_end;
}

static void
logdoc_DOM_AdjustBoundaryPoint(HTML_Element *&element, int &offset)
{
	OP_ASSERT(offset >= 0);
	unsigned offset_u = static_cast<unsigned>(offset);
	logdoc_DOM_AdjustBoundaryPoint(element, offset_u);
	OP_ASSERT(offset_u <= INT_MAX);
	offset = static_cast<int>(offset_u);
}

static void
logdoc_DOM_AdjustBoundaryPoint_newer(HTML_Element*& element, int& offset, HTML_Element* const stop_element, const int stop_offset, const BOOL forward)
{
	OP_ASSERT(offset >= 0);
	unsigned offset_u = static_cast<unsigned>(offset);
	OP_ASSERT(stop_offset >= 0);
	unsigned stop_offset_u = static_cast<unsigned>(stop_offset);
	logdoc_DOM_AdjustBoundaryPoint_newer(element, offset_u, stop_element, stop_offset_u, forward);
	OP_ASSERT(offset_u <= INT_MAX);
	offset = static_cast<int>(offset_u);
}

/* static */ void
TextSelection::ConvertPointToOldStyle(const SelectionBoundaryPoint& point, HTML_Element*& old_style_elm, int& old_style_offset)
{
	old_style_elm = point.GetElement();
	old_style_offset = point.GetOffset();
	if (old_style_elm)
		logdoc_DOM_AdjustBoundaryPoint(old_style_elm, old_style_offset);
}

/* static */ void
TextSelection::ConvertPointToOldStyle(const SelectionBoundaryPoint& point, HTML_Element*& old_style_elm, int& old_style_offset, BOOL scan_forward, const SelectionBoundaryPoint& scan_block_point)
{
	old_style_elm = point.GetElement();
	old_style_offset = point.GetOffset();
	if (old_style_elm)
		logdoc_DOM_AdjustBoundaryPoint_newer(old_style_elm, old_style_offset, scan_block_point.GetElement(), scan_block_point.GetOffset(), scan_forward);
}

/* static */
SelectionBoundaryPoint
TextSelection::ConvertFromOldStyle(HTML_Element* old_style_elm, int old_style_offset)
{
	HTML_Element* elm = old_style_elm;
	int offset = old_style_offset;
	if (!elm)
		return SelectionBoundaryPoint();

	// We have a slight incompatibility between the caret representation here and the selection
	// implementation so we need to do some conversion. Will be removed when (if?) the caret
	// is moved into the normal selection.
	if (!elm->IsText() && !elm->FirstChildActual())
	{
		// The caret is after an element and we need to return that relative the parent per
		// standard W3C selection ranges.
		HTML_Element* parent = elm->ParentActual();
		int text_selection_element_offset = 0;
		if (parent)
		{
			HTML_Element* count_it = elm->PredActual();
			text_selection_element_offset = offset; // 0 or 1 (as in before or after)
			while (count_it)
			{
				text_selection_element_offset++;
				count_it = count_it->PredActual();
			}
			elm = parent;
			offset = text_selection_element_offset;
		}
	}

	return SelectionBoundaryPoint(elm, offset);
}
