/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/opera/domselection.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domrange/range.h"
#include "modules/dom/src/domglobaldata.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/markup.h"
#ifdef SVG_SUPPORT
#include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

#ifdef DOM_SELECTION_SUPPORT

DOM_WindowSelection::DOM_WindowSelection(DOM_Document *document)
	: DOM_BuiltInConstructor(DOM_Runtime::WINDOWSELECTION_PROTOTYPE)
	, owner_document(document)
	, range(NULL)
	, forwards(TRUE)
	, is_updating_range(FALSE)
	, lock_updates_from_range(FALSE)
{
}

DOM_WindowSelection::~DOM_WindowSelection()
{
}

OP_STATUS
DOM_WindowSelection::IsCollapsed(BOOL &is_collapsed)
{
	RETURN_IF_ERROR(UpdateRange());

	if (range)
		is_collapsed = range->IsCollapsed();
	else
		is_collapsed = TRUE;

	return OpStatus::OK;
}

/**
 * Selection can span generated content but those *must never* be visible
 * to a script.
 *
 * This moves a selection point out of the generated data (if it's in such)
 * into the most reasonable approximation outside.
 *
 * @param[in,out] element The element. May be modified.
 *
 * @param[in,out] offset The offset. May be modified.

 * @returns OpStatus::OK or OpStatus:ERR if no dom visible element could be found.
 */
static OP_STATUS
MoveOutOfGeneratedContent(HTML_Element*& element, unsigned& offset)
{
	if (element->GetInserted() >= HE_INSERTED_FIRST_HIDDEN_BY_ACTUAL)
	{
		HTML_Element* real_element = element->ParentActual();
		if (!real_element)
			return OpStatus::ERR; // No real element to return

		HTML_Element* pseudo_element = element;
		while (pseudo_element->Parent() != real_element)
			pseudo_element = pseudo_element->Parent();

		element = real_element;

		HTML_Element *last_real_child = real_element->LastChildActual();
		if (pseudo_element->GetIsAfterPseudoElement() && last_real_child)
		{
			// Put the point at the end of the real element
			offset = DOM_Range::CalculateOffset(last_real_child) + 1;
		}
		else
			offset = 0; // Point at the start of the real element
	}

	return OpStatus::OK;
}

/**
 * Text groups should appear as a simple text node to scripts.  So make
 * sure we're just pointing to a text group, and not to any of the text nodes
 * within.  Will move the selection point out to the text group if it's
 * a text node which is a child of a text group.
 *
 * @param[in,out] element The element. May be modified.
 *
 * @param[in,out] offset The offset. May be modified.
 *
 */
static void
MoveOutToTextGroup(HTML_Element*& element, unsigned& offset)
{
	if (element->Type() == Markup::HTE_TEXT)
	{
		HTML_Element* parent = element->ParentActual();

		if (parent->Type() == Markup::HTE_TEXTGROUP)
		{
			for (HTML_Element* sibling = element->PredActual(); sibling; sibling = sibling->PredActual())
				offset += sibling->GetTextContentLength();

			element = parent;
		}
	}
}

void
DOM_WindowSelection::GetAnchorAndFocus(DOM_Range::BoundaryPoint &anchorBP, DOM_Range::BoundaryPoint &focusBP)
{
	if (forwards)
		range->GetEndPoints(anchorBP, focusBP);
	else
		range->GetEndPoints(focusBP, anchorBP);
}

OP_STATUS
DOM_WindowSelection::GetBoundaryNode(DOM_Node *&node, BOOL anchor)
{
	node = NULL;
	RETURN_IF_ERROR(UpdateRange());

	if (range)
	{
		DOM_Range::BoundaryPoint anchorBP, focusBP;
		GetAnchorAndFocus(anchorBP, focusBP);
		node = anchor ? anchorBP.node : focusBP.node;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_WindowSelection::GetBoundaryOffset(unsigned &offset, BOOL anchor)
{
	offset = 0;
	RETURN_IF_ERROR(UpdateRange());

	if (range)
	{
		DOM_Range::BoundaryPoint anchorBP, focusBP;
		GetAnchorAndFocus(anchorBP, focusBP);
		offset = anchor ? anchorBP.offset : focusBP.offset;
	}

	return OpStatus::OK;
}

/** Adjust the point to not use HTE_TEXTGROUP nor HTE_COMMENT as its base element. */
static BOOL
DOM_AdjustBoundaryPoint(SelectionBoundaryPoint &point)
{
	int text_offset = point.GetTextElementCharacterOffset();
	HTML_Element *unit = point.GetElement();
	if (unit->Type() == Markup::HTE_TEXTGROUP || unit->Type() == Markup::HTE_COMMENT)
	{
		HTML_Element *unit_iter = unit;
		HTML_Element *stop = unit->NextSibling();

		if (unit->Type() == Markup::HTE_TEXTGROUP)
			unit_iter = unit->FirstChild();

		while (unit_iter != stop)
		{
			if (unit_iter->Type() == HE_TEXT)
			{
				int len = unit_iter->GetTextContentLength();
				if (text_offset > len)
					text_offset -= len;
				else
				{
					point.SetLogicalPosition(unit_iter, text_offset);
					return FALSE;
				}
			}
			unit_iter = unit_iter->Next();
		}
	}

	BOOL passed_end = FALSE;

	return passed_end;
}

static OP_STATUS
DOM_SetBoundaryPoint(DOM_EnvironmentImpl *environment, DOM_Document *document, DOM_Range::BoundaryPoint &bp, HTML_Element *elm, unsigned offset, BOOL place_after)
{
	OP_ASSERT(elm);

	DOM_Node *node;
	RETURN_IF_ERROR(environment->ConstructNode(node, elm, document));

	if (!node->IsA(DOM_TYPE_CHARACTERDATA) && !elm->FirstChildActual())
	{
		HTML_Element *parent = elm->ParentActual();
		if (parent)
		{
			RETURN_IF_ERROR(environment->ConstructNode(bp.node, parent, document));

			bp.offset = DOM_Range::CalculateOffset(elm);
			if (place_after)
			{
				bp.offset++;
				bp.unit = NULL;
			}
			else
				bp.unit = node;

			return OpStatus::OK;
		}
	}

	bp.node = node;
	bp.offset = offset;
	bp.unit = NULL;

	return OpStatus::OK;
}

class UpdateFromRangeAutoLocker
{
	BOOL& m_lock_val;
public:
	UpdateFromRangeAutoLocker(BOOL &lock_val) : m_lock_val(lock_val)
	{
		m_lock_val = TRUE;
	}

	~UpdateFromRangeAutoLocker()
	{
		Release();
	}

	void Release() { m_lock_val = FALSE; }
};

OP_STATUS
DOM_WindowSelection::UpdateRangeFromSelection()
{
	range = NULL;
	UpdateFromRangeAutoLocker locker(lock_updates_from_range);
	if (HTML_Document *htmldoc = owner_document->GetHTML_Document())
	{
		OP_STATUS oom = OpStatus::OK;
		SelectionBoundaryPoint anchorPoint, focusPoint;
		if (htmldoc->GetSelection(anchorPoint, focusPoint))
		{
			RETURN_IF_ERROR(DOM_Range::Make(range, owner_document));
			range->selection = this;
			unsigned anchorOffset = anchorPoint.GetElementCharacterOffset();
			unsigned focusOffset = focusPoint.GetElementCharacterOffset();
			HTML_Element *anchorElement = anchorPoint.GetElement();
			HTML_Element *focusElement = focusPoint.GetElement();

			RETURN_IF_ERROR(MoveOutOfGeneratedContent(anchorElement, anchorOffset));
			RETURN_IF_ERROR(MoveOutOfGeneratedContent(focusElement, focusOffset));

			MoveOutToTextGroup(anchorElement, anchorOffset);
			MoveOutToTextGroup(focusElement, focusOffset);

			DOM_Range::BoundaryPoint anchorBP;
			DOM_Range::BoundaryPoint focusBP;
			DOM_EnvironmentImpl *environment = GetEnvironment();

			RETURN_IF_ERROR(DOM_SetBoundaryPoint(environment, owner_document, anchorBP, anchorElement, anchorOffset, FALSE));
			RETURN_IF_ERROR(DOM_SetBoundaryPoint(environment, owner_document, focusBP, focusElement, focusOffset, TRUE));

			DOM_Object::DOMException dummy_exception = DOM_Object::DOMEXCEPTION_NO_ERR;
			if (anchorElement == focusElement && anchorBP.offset <= focusBP.offset || anchorElement->Precedes(focusElement))
			{
				oom = range->SetStartAndEnd(anchorBP, focusBP, dummy_exception);
				forwards = TRUE;
			}
			else
			{
				oom = range->SetStartAndEnd(focusBP, anchorBP, dummy_exception);
				forwards = FALSE;
			}
		}
		else
		{
			HTML_Element* caret_elm = htmldoc->GetElementWithSelection();
			if (caret_elm)
			{
				RETURN_IF_ERROR(DOM_Range::Make(range, owner_document));
				range->selection = this;

				// Put an empty DOM_Range at the caret. And where is the caret?
				DOM_Range::BoundaryPoint caretBP;
				RETURN_IF_ERROR(GetEnvironment()->ConstructNode(caretBP.node, caret_elm, owner_document));

				DOM_Object::DOMException dummy_exception = DOM_Object::DOMEXCEPTION_NO_ERR;
				oom = range->SetStartAndEnd(caretBP, caretBP, dummy_exception);
			}
		}
		return oom;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_WindowSelection::UpdateSelectionFromRange()
{
	if (lock_updates_from_range)
		return OpStatus::OK;

	if (HTML_Document *htmldoc = owner_document->GetHTML_Document())
	{
		if (range && !range->IsDetached())
		{
			DOM_Range::BoundaryPoint start_bp, end_bp;
			range->GetEndPoints(start_bp, end_bp);

			if (!owner_document->IsAncestorOf(start_bp.node))
				return OpStatus::OK;

			RETURN_IF_ERROR(owner_document->GetFramesDocument()->Reflow(FALSE, TRUE));

			SelectionBoundaryPoint start_point, end_point;
			start_point.SetLogicalPosition(DOM_Range::GetAssociatedElement(start_bp.node), start_bp.offset);
			end_point.SetLogicalPosition(DOM_Range::GetAssociatedElement(end_bp.node), end_bp.offset);

			is_updating_range = TRUE;

			DOM_AdjustBoundaryPoint(start_point);
			DOM_AdjustBoundaryPoint(end_point);
			OP_STATUS oom = htmldoc->GetFramesDocument()->DOMSetSelection(&start_point, &end_point, TRUE);

			is_updating_range = FALSE;

			return oom;
		}
		else
		{
			is_updating_range = TRUE;

			SelectionBoundaryPoint dummy_anchor, dummy_focus;
			if (htmldoc->GetSelection(dummy_anchor, dummy_focus))
				htmldoc->GetFramesDocument()->ClearDocumentSelection(FALSE, FALSE, TRUE);

			if (HTML_Element* elm_with_selection = htmldoc->GetElementWithSelection())
			{
				if (elm_with_selection->IsFormElement())
					elm_with_selection->GetFormValue()->SetSelection(elm_with_selection, 0, 0);
#ifdef SVG_SUPPORT
				else if (elm_with_selection->GetNsType() == NS_SVG)
					g_svg_manager->ClearTextSelection(elm_with_selection);
#endif // SVG_SUPPORT

				htmldoc->SetElementWithSelection(NULL);
			}

			is_updating_range = FALSE;
		}
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_WindowSelection::Make(DOM_WindowSelection *&selection, DOM_Document *document)
{
	DOM_Runtime *runtime = document->GetRuntime();
	return DOMSetObjectRuntime(selection = OP_NEW(DOM_WindowSelection, (document)), runtime, runtime->GetPrototype(DOM_Runtime::WINDOWSELECTION_PROTOTYPE), "Selection");
}

/* virtual */ ES_GetState
DOM_WindowSelection::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_anchorNode:
	case OP_ATOM_focusNode:
		{
			DOM_Node *node;
			GET_FAILED_IF_ERROR(GetBoundaryNode(node, property_name == OP_ATOM_anchorNode));
			DOMSetObject(value, node);
		}
		return GET_SUCCESS;

	case OP_ATOM_anchorOffset:
	case OP_ATOM_focusOffset:
		{
			unsigned offset;
			GET_FAILED_IF_ERROR(GetBoundaryOffset(offset, property_name == OP_ATOM_anchorOffset));
			DOMSetNumber(value, offset);
		}
		return GET_SUCCESS;

	case OP_ATOM_isCollapsed:
		{
			BOOL is_collapsed;
			GET_FAILED_IF_ERROR(IsCollapsed(is_collapsed));
			DOMSetBoolean(value, is_collapsed);
		}
		return GET_SUCCESS;

	case OP_ATOM_rangeCount:
		GET_FAILED_IF_ERROR(UpdateRange());
		DOMSetNumber(value, range ? 1 : 0);
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_WindowSelection::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_anchorNode:
	case OP_ATOM_anchorOffset:
	case OP_ATOM_focusNode:
	case OP_ATOM_focusOffset:
	case OP_ATOM_isCollapsed:
	case OP_ATOM_rangeCount:
		return PUT_SUCCESS;

	default:
		return PUT_FAILED;
	}
}

/* virtual */ void
DOM_WindowSelection::GCTrace()
{
	GCMark(owner_document);
	GCMark(range);
}

/* static */ int
DOM_WindowSelection::getRangeAt(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("n");

	CALL_FAILED_IF_ERROR(selection->UpdateRange());

	if (argv[0].value.number != 0. || !selection->range)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	DOMSetObject(return_value, selection->range);
	return ES_VALUE;
}

/* static */ int
DOM_WindowSelection::collapse(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("on");
	DOM_ARGUMENT_OBJECT(parentNode, 0, DOM_TYPE_NODE, DOM_Node);

	DOM_Range *new_range;
	CALL_FAILED_IF_ERROR(DOM_Range::Make(new_range, parentNode->GetOwnerDocument()));

	ES_Value value;
	int result = DOM_Range::setStartOrEnd(new_range, argv, argc, &value, origining_runtime, 0);

	if (result == ES_NO_MEMORY)
		return result;
	else if (result == ES_EXCEPTION)
	{
		*return_value = value;
		return result;
	}
	else if (result == ES_FAILED) // ES_FAILED means success from setStartOrEnd
	{
		selection->range = new_range;
		selection->range->selection = selection;
		CALL_FAILED_IF_ERROR(selection->UpdateSelectionFromRange());
	}

	return ES_FAILED;
}

/* static */ int
DOM_WindowSelection::extend(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("on");
	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);

	CALL_FAILED_IF_ERROR(selection->UpdateRange());

	if (!selection->range)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	DOM_Range *new_range;
	BOOL forwards;
	OP_STATUS oom = OpStatus::OK;
	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
	DOM_Range::BoundaryPoint newBP(node, TruncateDoubleToUInt(argv[1].value.number), NULL);

	CALL_FAILED_IF_ERROR(DOM_Range::Make(new_range, node->GetOwnerDocument()));

	if (DOM_Range::FindTreeRoot(node) != selection->range->GetTreeRoot())
	{
		oom = new_range->SetStartAndEnd(newBP, newBP, exception);
		forwards = TRUE;
	}
	else
	{
		DOM_Range::BoundaryPoint anchorBP;
		DOM_Range::BoundaryPoint focusBP;
		selection->GetAnchorAndFocus(anchorBP, focusBP);

		int result = DOM_Range::CompareBoundaryPoints(anchorBP, newBP);
		if (result <= 0)
		{
			oom = new_range->SetStartAndEnd(anchorBP, newBP, exception);
			forwards = TRUE;
		}
		else
		{
			oom = new_range->SetStartAndEnd(newBP, anchorBP, exception);
			forwards = FALSE;
		}
	}

	if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
		return this_object->CallDOMException(exception, return_value);
	else if (OpStatus::IsMemoryError(oom))
		return ES_NO_MEMORY;

	selection->range = new_range;
	selection->range->selection = selection;
	selection->forwards = forwards;

	CALL_FAILED_IF_ERROR(selection->UpdateSelectionFromRange());

	return ES_FAILED;
}

/* static */ int
DOM_WindowSelection::containsNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("ob");
	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);

	CALL_FAILED_IF_ERROR(selection->UpdateRange());

	DOMSetBoolean(return_value, FALSE);
	if (selection->range)
	{
		BOOL allow_partially_contained = argv[1].value.boolean;
		DOM_Range *range = selection->range;

		HTML_Element *node_root = DOM_Range::FindTreeRoot(node);
		if (node_root && node_root == range->GetTreeRoot())
		{
			DOM_Range::BoundaryPoint rangeStart, rangeEnd;
			range->GetEndPoints(rangeStart, rangeEnd);

			DOM_Range::BoundaryPoint refBP(node, 0, NULL);

			int result = DOM_Range::CompareBoundaryPoints(rangeStart, refBP);

			if (result == -1) // start before reference
			{
				DOM_Range::CountChildUnits(refBP.offset, node);
				result = DOM_Range::CompareBoundaryPoints(rangeEnd, refBP);

				if (result != -1) // end is before reference
					DOMSetBoolean(return_value, result == 1 || allow_partially_contained);
			}
			else // reference before or equal to start
				DOMSetBoolean(return_value, result == 0 && allow_partially_contained);
		}
	}

	return ES_VALUE;
}

/* static */ int
DOM_WindowSelection::selectAllChildren(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(parentNode, 0, DOM_TYPE_NODE, DOM_Node);

	HTML_Element *element = DOM_Range::GetAssociatedElement(parentNode);
	if (!element)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	DOM_Range *new_range;
	CALL_FAILED_IF_ERROR(DOM_Range::Make(new_range, parentNode->GetOwnerDocument()));

	unsigned child_count = 0;
	HTML_Element *child_iter = element->FirstChildActual();
	while (child_iter)
		child_count++, child_iter = child_iter->SucActual();

	DOM_Range::BoundaryPoint anchorBP(parentNode, 0, NULL);
	DOM_Range::BoundaryPoint focusBP(parentNode, child_count, NULL);

	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
	OP_STATUS oom = new_range->SetStartAndEnd(anchorBP, focusBP, exception);

	if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
		return this_object->CallDOMException(exception, return_value);
	else if (OpStatus::IsMemoryError(oom))
		return ES_NO_MEMORY;

	selection->range = new_range;
	selection->range->selection = selection;
	selection->forwards = TRUE;

	CALL_FAILED_IF_ERROR(selection->UpdateSelectionFromRange());

	return ES_FAILED;
}

/* static */ int
DOM_WindowSelection::addRange(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(range, 0, DOM_TYPE_RANGE, DOM_Range);

	selection->range = range;
	selection->range->selection = selection;

	CALL_FAILED_IF_ERROR(selection->UpdateSelectionFromRange());

	return ES_FAILED;
}

/* static */ int
DOM_WindowSelection::deleteFromDocument(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("");

	CALL_FAILED_IF_ERROR(selection->UpdateRange());

	if (selection->range)
	{
		ES_Value value;
		// Prevent range code call from clobbering the range. The delete
		// operation in DOM_Range can have the side effect of updating the
		// document selection which will set the selections range to NULL.
		// In this case, that updating will be done explicitly in the call
		// to UpdateSelectionFromRange() below instead.
		selection->is_updating_range = TRUE;

		selection->lock_updates_from_range = TRUE;
		int result = DOM_Range::deleteOrExtractOrCloneContents(selection->range, NULL, 0, &value, origining_runtime, 0);
		selection->lock_updates_from_range = FALSE;

		selection->is_updating_range = FALSE;
		if (result == ES_NO_MEMORY)
			return result;
		else if (result == ES_EXCEPTION)
		{
			*return_value = value;
			return result;
		}
		else if (result == ES_VALUE) // ES_VALUE means success from deleteOrExtractOrCloneContents
			CALL_FAILED_IF_ERROR(selection->UpdateSelectionFromRange());
	}

	return ES_FAILED;
}

/* static */ int
DOM_WindowSelection::toString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);
	DOM_CHECK_ARGUMENTS("");

	CALL_FAILED_IF_ERROR(selection->UpdateRange());
	if (selection->range)
		return DOM_Range::toString(selection->range, argv, argc, return_value, origining_runtime);

	DOMSetString(return_value);
	return ES_VALUE;
}

/* static */ int
DOM_WindowSelection::collapseToStartOrEnd(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	// data == 0: collapseToStart, data == 1: collapseToEnd
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);

	CALL_FAILED_IF_ERROR(selection->UpdateRange());

	if (!selection->range)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	DOM_Range::BoundaryPoint startBP;
	DOM_Range::BoundaryPoint endBP;
	selection->range->GetEndPoints(startBP, endBP);

	DOM_Range *new_range;
	CALL_FAILED_IF_ERROR(DOM_Range::Make(new_range, selection->owner_document));

	OP_STATUS oom;
	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
	if (data == 0)
		oom = new_range->SetStartAndEnd(startBP, startBP, exception);
	else
		oom = new_range->SetStartAndEnd(endBP, endBP, exception);

	if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
		return this_object->CallDOMException(exception, return_value);
	else if (OpStatus::IsMemoryError(oom))
		return ES_NO_MEMORY;

	selection->range = new_range;
	selection->range->selection = selection;
	CALL_FAILED_IF_ERROR(selection->UpdateSelectionFromRange());

	return ES_FAILED;
}

/* static */ int
DOM_WindowSelection::removeRange(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	// data == 0: removeRange, data == 1: removeAllRanges
	DOM_THIS_OBJECT(selection, DOM_TYPE_WINDOWSELECTION, DOM_WindowSelection);

	if (data == 0)
	{
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT(range, 0, DOM_TYPE_RANGE, DOM_Range);

		if (range != selection->range)
			return ES_FAILED;

		selection->range->selection = NULL;
	}

	selection->range = NULL;

	if (data == 1)
		selection->forwards = TRUE;

	CALL_FAILED_IF_ERROR(selection->UpdateSelectionFromRange());

	return ES_FAILED;
}

extern DOM_FunctionImpl DOM_dummyMethod;

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_WindowSelection)
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::getRangeAt, "getRangeAt", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::collapse, "collapse", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::extend, "extend", "-n-")
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::containsNode, "containsNode", "ob-")
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::selectAllChildren, "selectAllChildren", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::addRange, "addRange", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::deleteFromDocument, "deleteFromDocument", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_dummyMethod, "selectionLanguageChange", "b-")
	DOM_FUNCTIONS_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::toString, "toString", 0)
DOM_FUNCTIONS_END(DOM_WindowSelection)

DOM_FUNCTIONS_WITH_DATA_START(DOM_WindowSelection)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::removeRange, 0, "removeRange", "-o-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::removeRange, 1, "removeAllRanges", "-o-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::collapseToStartOrEnd, 0, "collapseToStart", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WindowSelection, DOM_WindowSelection::collapseToStartOrEnd, 1, "collapseToEnd", 0)
DOM_FUNCTIONS_WITH_DATA_END(DOM_WindowSelection)

#endif // DOM_SELECTION_SUPPORT
