/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM2_RANGE

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domrange/range.h"
#include "modules/dom/src/opera/domselection.h"

#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domcore/comment.h"
#include "modules/dom/src/domcore/cdatasection.h"
#include "modules/dom/src/domhtml/htmlcoll.h"

#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domeventtarget.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/util/simset.h"
#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/logdoc/optreecallback.h"

#define START_CALL \
	do { \
		int return_code; \
		if (!range->StartCall(return_code, return_value, origining_runtime)) \
			return return_code; \
	} while (0)

#define DETACH_AND_RETURN_IF_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
		{ \
			Detach(); \
			return RETURN_IF_ERROR_TMP; \
		} \
	} while (0)

/* virtual */ void
DOM_Range::DOM_RangeMutationListener::OnTreeDestroyed(HTML_Element *tree_root)
{
	if (tree_root == range->GetTreeRoot())
		/* Note: this deletes this listener. */
		range->Detach();
}

/* virtual */ OP_STATUS
DOM_Range::DOM_RangeMutationListener::OnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime)
{
	if (!range->tree_root || !range->tree_root->IsAncestorOf(child))
		return OpStatus::OK;

	DOM_Node *parent_node, *child_node;
	OP_STATUS status;
	if (OpStatus::IsError(status = range->GetEnvironment()->ConstructNode(child_node, child, range->GetOwnerDocument())) ||
		OpStatus::IsError(status = child_node->GetParentNode(parent_node)) ||
		OpStatus::IsError(status = range->HandleNodeInserted(parent_node, child_node)))
		range->Detach();
	return status;
}

/* virtual */ OP_STATUS
DOM_Range::DOM_RangeMutationListener::OnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime)
{
	if (!range->tree_root || !range->tree_root->IsAncestorOf(child))
		return OpStatus::OK;

	if (child->Parent() && child->Parent()->Type() == Markup::HTE_TEXTGROUP)
		return OpStatus::OK;

	DOM_Node *parent_node, *child_node;
	OP_STATUS status;
	if (OpStatus::IsError(status = range->GetEnvironment()->ConstructNode(child_node, child, range->GetOwnerDocument())) ||
		OpStatus::IsError(status = child_node->GetParentNode(parent_node)) ||
		OpStatus::IsError(status = range->HandleRemovingNode(parent_node, child_node)))
		range->Detach();
	return status;
}

/* virtual */ OP_STATUS
DOM_Range::DOM_RangeMutationListener::OnAfterValueModified(HTML_Element *element, const uni_char *new_value, ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime)
{
	if (!range->tree_root || !range->tree_root->IsAncestorOf(element))
		return OpStatus::OK;

	DOM_Node *element_node;
	RETURN_IF_ERROR(range->GetEnvironment()->ConstructNode(element_node, element, range->GetOwnerDocument()));
	return range->HandleNodeValueModified(element_node, new_value, type, offset, length1, length2);
}

OP_STATUS
DOM_Range::BoundaryPoint::CalculateUnit()
{
	if (!node->IsA(DOM_TYPE_CHARACTERDATA))
		return DOM_Range::GetOffsetNode(unit, *this);
	else
		return OpStatus::OK;
}

void
DOM_Range::UnregisterMutationListener()
{
	if (listeners_environment)
	{
		listeners_environment->RemoveMutationListener(&listener);
		listeners_environment = NULL;
	}
}

void
DOM_Range::ValidateMutationListener()
{
	DOM_Node *node = start.node ? start.node : end.node;
	if (node)
	{
		DOM_EnvironmentImpl *node_environment = node->GetEnvironment();
		if (node_environment != listeners_environment)
		{
			UnregisterMutationListener();

			node_environment->AddMutationListener(&listener);
			listeners_environment = node_environment;
		}
	}
	else
		UnregisterMutationListener();
}

void
DOM_Range::SetStartInternal(BoundaryPoint &new_bp, HTML_Element *new_root)
{
	BOOL collapse = new_root != tree_root;

	if (!collapse)
	{
		int result = CompareBoundaryPoints(new_bp, end);
		if (result == 1)
			collapse = TRUE;
	}

	if (collapse)
	{
		tree_root = new_root;
		end = new_bp;
	}

	start = new_bp;

	ValidateMutationListener();

#ifdef DEBUG_ENABLE_OPASSERT
	HTML_Element *node_elm = start.node ? DOM_Range::GetAssociatedElement(start.node) : NULL;
	OP_ASSERT(node_elm && tree_root->IsAncestorOf(node_elm));
	HTML_Element *unit_elm = start.unit ? DOM_Range::GetAssociatedElement(start.unit) : NULL;
	OP_ASSERT(!start.unit || (unit_elm && unit_elm->ParentActual() == node_elm));
#endif // DEBUG_ENABLE_OPASSERT
}

void
DOM_Range::SetEndInternal(BoundaryPoint &new_bp, HTML_Element *new_root)
{
	BOOL collapse = new_root != tree_root;

	if (!collapse)
	{
		int result = CompareBoundaryPoints(new_bp, start);
		if (result == -1)
			collapse = TRUE;
	}

	if (collapse)
	{
		tree_root = new_root;
		start = new_bp;
	}

	end = new_bp;

	ValidateMutationListener();

#ifdef DEBUG_ENABLE_OPASSERT
	HTML_Element *node_elm = end.node ? DOM_Range::GetAssociatedElement(end.node) : NULL;
	OP_ASSERT(node_elm && tree_root->IsAncestorOf(node_elm));
	HTML_Element *unit_elm = end.unit ? DOM_Range::GetAssociatedElement(end.unit) : NULL;
	OP_ASSERT(!end.unit || (unit_elm && unit_elm->ParentActual() == node_elm));
#endif // DEBUG_ENABLE_OPASSERT
}

OP_STATUS
DOM_Range::AdjustBoundaryPoint(DOM_Node *parent, DOM_Node *child, BoundaryPoint &point, unsigned &childs_index, unsigned &moved_count)
{
	if (parent == point.node)
	{
		if (child != point.unit)
		{
			if (childs_index == UINT_MAX)
				childs_index = CalculateOffset(child);
			if (childs_index < point.offset)
				point.offset--;
		}
	}
	else if (child->IsAncestorOf(point.node))
	{
		if (childs_index == UINT_MAX)
			childs_index = CalculateOffset(child);

		BoundaryPoint child_bp(parent, childs_index, child);
		if (point == start)
			SetStartInternal(child_bp, tree_root);
		else
			SetEndInternal(child_bp, tree_root);

		moved_count++;
	}

	if (child == point.unit)
		return child->GetNextSibling(point.unit);
	else
		return OpStatus::OK;
}

DOM_Range::DOM_Range(DOM_Document *document, DOM_Node* initial_unit)
	:
#ifdef DOM_SELECTION_SUPPORT
	  selection(NULL),
#endif // DOM_SELECTION_SUPPORT
	  start(document, 0, initial_unit),
	  end(document, 0, initial_unit),
	  common_ancestor(document),
	  detached(FALSE),
	  busy(FALSE),
	  abort(FALSE),
	  tree_root(document->GetPlaceholderElement()),
	  listener(this),
	  listeners_environment(NULL)
{
}

DOM_Range::~DOM_Range()
{
	Detach();
}

/* static */ OP_STATUS
DOM_Range::Make(DOM_Range *&range, DOM_Document *document)
{
	DOM_Runtime *runtime = document->GetRuntime();
	ES_Object *prototype = runtime->GetPrototype(DOM_Runtime::RANGE_PROTOTYPE);
	ES_Value dummy;

	if (runtime->GetName(prototype, UNI_L("END_TO_END"), &dummy) == OpBoolean::IS_FALSE)
		RETURN_IF_ERROR(ConstructRangeObject(prototype, runtime));

	// The initial state of the Range returned from this method is
	// such that both of its boundary-points are positioned at the
	// beginning of the corresponding Document, before any content.
	// In other words, the container of each boundary-point is the
	// Document node and the offset within that node is 0.
	DOM_Node* initial_unit;
	RETURN_IF_ERROR(document->GetFirstChild(initial_unit));
	RETURN_IF_ERROR(DOMSetObjectRuntime(range = OP_NEW(DOM_Range, (document, initial_unit)), runtime, runtime->GetPrototype(DOM_Runtime::RANGE_PROTOTYPE), "Range"));


	return range->UpdateCommonAncestor();
}

/* static */ void
DOM_Range::ConstructRangeObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "START_TO_START", START_TO_START, runtime);
	DOM_Object::PutNumericConstantL(object, "START_TO_END", START_TO_END, runtime);
	DOM_Object::PutNumericConstantL(object, "END_TO_END", END_TO_END, runtime);
	DOM_Object::PutNumericConstantL(object, "END_TO_START", END_TO_START, runtime);
}

/* static */ OP_STATUS
DOM_Range::ConstructRangeObject(ES_Object *object, DOM_Runtime *runtime)
{
	TRAPD(status, ConstructRangeObjectL(object, runtime));
	return status;
}

/* virtual */ ES_GetState
DOM_Range::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_startContainer:
		DOMSetObject(value, start.node);
		break;

	case OP_ATOM_startOffset:
		DOMSetNumber(value, start.offset);
		break;

	case OP_ATOM_endContainer:
		DOMSetObject(value, end.node);
		break;

	case OP_ATOM_endOffset:
		DOMSetNumber(value, end.offset);
		break;

	case OP_ATOM_collapsed:
		DOMSetBoolean(value, IsCollapsed());
		break;

	case OP_ATOM_commonAncestorContainer:
		DOMSetObject(value, common_ancestor);
		break;

	default:
		return GET_FAILED;
	}

	if (value && detached)
		return DOM_GETNAME_DOMEXCEPTION(INVALID_STATE_ERR);
	else
		return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_Range::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_startContainer:
	case OP_ATOM_startOffset:
	case OP_ATOM_endContainer:
	case OP_ATOM_endOffset:
	case OP_ATOM_collapsed:
	case OP_ATOM_commonAncestorContainer:
		if (detached)
			return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
		else if (!OriginCheck(origining_runtime))
			return PUT_SECURITY_VIOLATION;
		else
			return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}

/* virtual */ void
DOM_Range::GCTrace()
{
	GCMark(start.node);
	GCMark(start.unit);
	GCMark(end.node);
	GCMark(end.unit);
	GCMark(common_ancestor);
#ifdef DOM_SELECTION_SUPPORT
	GCMark(selection);
#endif // DOM_SELECTION_SUPPORT
}

BOOL
DOM_Range::StartCall(int &return_code, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	if (busy)
		return_code = CallInternalException(RESOURCE_BUSY_ERR, return_value);
	else if (detached)
		return_code = CallDOMException(INVALID_STATE_ERR, return_value);
	else
	{
		Reset();
		return TRUE;
	}

	return FALSE;
}

BOOL
DOM_Range::Reset()
{
	BOOL do_abort = abort;
	busy = abort = FALSE;
	return !do_abort;
}

void
DOM_Range::Detach()
{
	UnregisterMutationListener();
	detached = TRUE;
	start.node = end.node = NULL;
	start.unit = end.unit = common_ancestor = NULL;
	tree_root = NULL;

	if (busy)
		abort = TRUE;
}

DOM_Document*
DOM_Range::GetOwnerDocument()
{
	return start.node ? start.node->GetOwnerDocument() : NULL;
}

/* static */ HTML_Element*
DOM_Range::FindTreeRoot(DOM_Node *node)
{
	HTML_Element *root_iter = GetAssociatedElement(node);

	while (root_iter && root_iter->Parent())
		root_iter = root_iter->Parent();

	return root_iter;
}

/* static */ HTML_Element*
DOM_Range::GetAssociatedElement(DOM_Node *node)
{
	switch (node->GetNodeType())
	{
	case ATTRIBUTE_NODE:
	case ENTITY_NODE:
	case DOCUMENT_NODE:
	case DOCUMENT_FRAGMENT_NODE:
	case NOTATION_NODE:
		return node->GetPlaceholderElement();

	default:
		return node->GetThisElement();
	}
}

/* static */ OP_STATUS
DOM_Range::CountChildUnits(unsigned &child_units, DOM_Node *node)
{
	child_units = 0;

	ES_Value value;
	if (node->IsA(DOM_TYPE_CHARACTERDATA))
	{
		TempBuffer value;
		RETURN_IF_ERROR(((DOM_CharacterData *) node)->GetValue(&value));
		child_units = value.Length();
	}
	else
		switch (node->GetName(OP_ATOM_childNodes, &value, node->GetRuntime()))
	{
		case GET_NO_MEMORY:
			return OpStatus::ERR_NO_MEMORY;

		case GET_SUCCESS:
			child_units = (unsigned) DOM_VALUE2OBJECT(value, DOM_Collection)->Length();
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_Range::GetOffsetNode(DOM_Node *&node, BoundaryPoint &bp)
{
	if (bp.node->IsA(DOM_TYPE_CHARACTERDATA))
		node = bp.node;
	else
	{
		DOM_Collection *child_nodes;
		ES_Value value;

		switch (bp.node->GetName(OP_ATOM_childNodes, &value, bp.node->GetRuntime()))
		{
		case GET_NO_MEMORY:
			return OpStatus::ERR_NO_MEMORY;

		case GET_SUCCESS:
			child_nodes = DOM_VALUE2OBJECT(value, DOM_Collection);
			break;

		default:
			return OpStatus::ERR;
		}

		HTML_Element *element = child_nodes->Item(bp.offset);
		if (element)
			RETURN_IF_ERROR(bp.node->GetEnvironment()->ConstructNode(node, element, bp.node->GetOwnerDocument()));
		else
			node = NULL;
	}

	return OpStatus::OK;
}

/* static */ unsigned
DOM_Range::CalculateOffset(DOM_Node *node)
{
	unsigned offset = 0;

	if (HTML_Element *this_element = node->GetThisElement())
		while ((this_element = this_element->PredActual()) != NULL)
			offset++;

	return offset;
}

/* static */ unsigned
DOM_Range::CalculateOffset(HTML_Element *element)
{
	unsigned offset = 0;

	while ((element = element->PredActual()) != NULL)
		offset++;

	return offset;
}

OP_STATUS
DOM_Range::UpdateCommonAncestor()
{
	if (start.node == end.node)
		common_ancestor = start.node;
	else
	{
		HTML_Element *parent;
		unsigned start_height = 0;
		unsigned end_height = 0;
		HTML_Element *start_iter = DOM_Range::GetAssociatedElement(start.node);
		HTML_Element *end_iter = DOM_Range::GetAssociatedElement(end.node);

		OP_ASSERT(start_iter && end_iter);

		parent = start_iter;
		while (parent)
		{
			++start_height;
			parent = parent->ParentActual();
		}

		parent = end_iter;
		while (parent)
		{
			++end_height;
			parent = parent->ParentActual();
		}

		while (end_height < start_height - 1)
		{
			--start_height;
			start_iter = start_iter->ParentActual();
		}

		while (start_height < end_height - 1)
		{
			--end_height;
			end_iter = end_iter->ParentActual();
		}

		while (start_iter != end_iter)
		{
			if (start_height >= end_height)
				start_iter = start_iter->ParentActual();

			if (end_height >= start_height)
				end_iter = end_iter->ParentActual();

			start_height = end_height = 0;
		}

		DETACH_AND_RETURN_IF_ERROR(GetEnvironment()->ConstructNode(common_ancestor, start_iter, GetOwnerDocument()));
	}

	tree_root = DOM_Range::FindTreeRoot(common_ancestor);

	return OpStatus::OK;
}

/* static */ int
DOM_Range::CompareBoundaryPoints(BoundaryPoint &first, BoundaryPoint &second)
{
	if (first.node == second.node)
		return first.offset < second.offset ? -1 : first.offset > second.offset ? 1 : 0;
	else
	{
		HTML_Element *elm1 = GetAssociatedElement(first.node);
		HTML_Element *elm2 = GetAssociatedElement(second.node);

		OP_ASSERT(elm1 && elm2 && DOM_Range::FindTreeRoot(first.node) == DOM_Range::FindTreeRoot(second.node));

		HTML_Element *parent_iter = elm1;
		unsigned height1 = 0;
		unsigned height2 = 0;

		while (parent_iter)
		{
			++height1;
			parent_iter = parent_iter->ParentActual();
		}

		parent_iter = elm2;
		while (parent_iter)
		{
			++height2;
			parent_iter = parent_iter->ParentActual();
		}

		HTML_Element *iter1 = elm1;
		HTML_Element *iter2 = elm2;

		while (height2 < height1 - 1)
		{
			--height1;
			iter1 = iter1->ParentActual();
		}

		while (height1 < height2 - 1)
		{
			--height2;
			iter2 = iter2->ParentActual();
		}

		HTML_Element *last1 = NULL;
		HTML_Element *last2 = NULL;

		while (iter1 != iter2)
		{
			if (height1 >= height2)
			{
				last1 = iter1;
				iter1 = iter1->ParentActual();
			}
			if (height2 >= height1)
			{
				last2 = iter2;
				iter2 = iter2->ParentActual();
			}

			height1 = height2 = 0;
		}

		unsigned offset1_actual = first.offset;
		unsigned offset2_actual = second.offset;

		if (last1)
			offset1_actual = CalculateOffset(last1);

		if (last2)
			offset2_actual = CalculateOffset(last2);

		return offset1_actual < offset2_actual ? -1 : offset1_actual > offset2_actual ? 1 : !last1 && last2 ? -1 : last1 && !last2 ? 1 : 0;
	}
}

OP_STATUS
DOM_Range::SetStart(BoundaryPoint &new_bp, DOM_Object::DOMException &exception)
{
	if (new_bp.node->GetNodeType() == DOCUMENT_TYPE_NODE)
	{
		exception = DOM_Object::INVALID_NODE_TYPE_ERR;
		return OpStatus::OK;
	}

	unsigned child_units;
	RETURN_IF_MEMORY_ERROR(CountChildUnits(child_units, new_bp.node));
	if (new_bp.offset > child_units)
	{
		exception = DOM_Object::INDEX_SIZE_ERR;
		return OpStatus::OK;
	}

	HTML_Element *new_root = DOM_Range::FindTreeRoot(new_bp.node);
	if (!new_root)
	{
		exception = DOM_Object::INVALID_STATE_ERR;
		return OpStatus::OK;
	}

	if (!new_bp.unit)
		RETURN_IF_ERROR(new_bp.CalculateUnit());

	SetStartInternal(new_bp, new_root);

#ifdef _DEBUG
	if (new_bp.node->IsA(DOM_TYPE_CHARACTERDATA))
	{
		TempBuffer value;
		((DOM_CharacterData *) new_bp.node)->GetValue(&value);
		unsigned length = value.Length();

		OP_ASSERT(new_bp.offset <= length);
	}
#endif // _DEBUG

	return OpStatus::OK;
}

OP_STATUS
DOM_Range::SetEnd(BoundaryPoint &new_bp, DOM_Object::DOMException &exception)
{
	if (new_bp.node->GetNodeType() == DOCUMENT_TYPE_NODE)
	{
		exception = DOM_Object::INVALID_NODE_TYPE_ERR;
		return OpStatus::OK;
	}

	unsigned child_units;
	RETURN_IF_MEMORY_ERROR(CountChildUnits(child_units, new_bp.node));
	if (new_bp.offset > child_units)
	{
		exception = DOM_Object::INDEX_SIZE_ERR;
		return OpStatus::OK;
	}

	HTML_Element *new_root = DOM_Range::FindTreeRoot(new_bp.node);
	if (!new_root)
	{
		exception = DOM_Object::INVALID_STATE_ERR;
		return OpStatus::OK;
	}

	if (!new_bp.unit)
		RETURN_IF_ERROR(new_bp.CalculateUnit());

	SetEndInternal(new_bp, new_root);

#ifdef _DEBUG
	if (new_bp.node->IsA(DOM_TYPE_CHARACTERDATA))
	{
		TempBuffer value;
		((DOM_CharacterData *) new_bp.node)->GetValue(&value);
		unsigned length = value.Length();

		OP_ASSERT(new_bp.offset <= length);
	}
#endif // _DEBUG

	return OpStatus::OK;
}

OP_STATUS
DOM_Range::SetStartAndEnd(BoundaryPoint &new_start_bp, BoundaryPoint &new_end_bp, DOM_Object::DOMException &exception)
{
	OP_STATUS oom = SetStart(new_start_bp, exception);
	if (exception == DOM_Object::DOMEXCEPTION_NO_ERR && OpStatus::IsSuccess(oom))
	{
		oom = SetEnd(new_end_bp, exception);
		if (OpStatus::IsMemoryError(UpdateCommonAncestor()))
			oom = OpStatus::ERR_NO_MEMORY;
	}

	return oom;
}

OP_STATUS
DOM_Range::HandleNodeInserted(DOM_Node *parent, DOM_Node *child)
{
	if (busy && common_ancestor->IsAncestorOf(child))
		abort = TRUE;

	OP_ASSERT(!parent->IsA(DOM_TYPE_TEXT) || !"Parent is a text group, and the children of text groups are invisible to DOM");

	if (tree_root == DOM_Range::GetAssociatedElement(child))
		tree_root = DOM_Range::FindTreeRoot(parent);

	if (parent == start.node)
	{
		unsigned offset = CalculateOffset(child);

		if (offset < start.offset)
			start.offset++;
		else if (offset == start.offset)
			start.unit = child;
	}

	if (parent == end.node)
	{
		unsigned offset = CalculateOffset(child);

		if (offset < end.offset)
			end.offset++;
		else if (offset == end.offset)
			end.unit = child;
	}

#ifdef _DEBUG
	DebugCheckState(TRUE);
#endif // _DEBUG

	return OpStatus::OK;
}

OP_STATUS
DOM_Range::HandleRemovingNode(DOM_Node *parent, DOM_Node *child)
{
	if (busy && common_ancestor->IsAncestorOf(child))
		abort = TRUE;

	// Only need to recalculate the common ancestor if both start and end has
	// changed when adjusting the boundary points, since if just one has been
	// changed, it will still be under the previous common ancestor. This value
	// counts the number of boundary points moved.
	unsigned points_moved_to_another_subtree = 0;
	unsigned childs_index = UINT_MAX; // Cached index of child, UINT_MAX means uninitialized.

	OP_ASSERT(!parent->IsA(DOM_TYPE_TEXT) || !"Parent is a text group, and the children of text groups are invisible to DOM");

	DETACH_AND_RETURN_IF_ERROR(AdjustBoundaryPoint(parent, child, start, childs_index, points_moved_to_another_subtree));
	DETACH_AND_RETURN_IF_ERROR(AdjustBoundaryPoint(parent, child, end, childs_index, points_moved_to_another_subtree));

	if (points_moved_to_another_subtree == 2)
	{
		if (!common_ancestor->IsAncestorOf(start.node) || !common_ancestor->IsAncestorOf(end.node))
			RETURN_IF_ERROR(UpdateCommonAncestor());
	}

#ifdef _DEBUG
	DebugCheckState(TRUE);
#endif // _DEBUG

	return OpStatus::OK;
}

OP_STATUS
DOM_Range::HandleNodeValueModified(DOM_Node *node, const uni_char *value, DOM_MutationListener::ValueModificationType type, unsigned offset, unsigned length1, unsigned length2)
{
	if (node == start.node || node == end.node)
	{
		switch (type)
		{
		case DOM_MutationListener::DELETING:
			if (node == start.node && start.offset > offset)
			{
				if (offset + length1 <= start.offset)
					start.offset -= length1;
				else
					start.offset = offset;
			}

			if (node == end.node && end.offset > offset)
			{
				if (offset + length1 <= end.offset)
					end.offset -= length1;
				else
					end.offset = offset;
			}
			break;

		case DOM_MutationListener::INSERTING:
			if (node == start.node && start.offset > offset)
				start.offset += length1;

			if (node == end.node && end.offset > offset)
				end.offset += length1;
			break;

		case DOM_MutationListener::REPLACING:
			if (node == start.node && start.offset > offset)
			{
				if (start.offset <= offset + length1)
					start.offset = offset;
				else
					start.offset += length2 - length1;
			}

			if (node == end.node && end.offset > offset)
			{
				if (end.offset <= offset + length1)
					end.offset = offset;
				else
					end.offset += length2 - length1;
			}
			break;

		case DOM_MutationListener::SPLITTING:
			{
				BOOL must_update_common_ancestor = FALSE;
				BOOL has_parent = node->GetThisElement()->ParentActual() != NULL;

				if (node == start.node && start.offset > offset)
					if (has_parent)
					{
						BoundaryPoint new_start_bp;
						DETACH_AND_RETURN_IF_ERROR(node->GetNextSibling(new_start_bp.node));

						new_start_bp.offset = start.offset - offset;
						SetStartInternal(new_start_bp, tree_root);

						if (node == common_ancestor)
							must_update_common_ancestor = TRUE;
					}
					else
						start.offset = offset;

				if (node == end.node && end.offset > offset)
					if (has_parent)
					{
						BoundaryPoint new_end_bp;
						DETACH_AND_RETURN_IF_ERROR(node->GetNextSibling(new_end_bp.node));

						new_end_bp.offset = end.offset - offset;
						SetEndInternal(new_end_bp, tree_root);

						if (node == common_ancestor)
							must_update_common_ancestor = TRUE;
					}
					else
						end.offset = offset;

				if (must_update_common_ancestor)
					UpdateCommonAncestor();
			}
			break;

		case DOM_MutationListener::REPLACING_ALL:
			{
				unsigned delete_length = MAX(node == start.node ? start.offset : 0, node == end.node ? end.offset : 0);
				unsigned insert_length = uni_strlen(value);

				if (node == start.node && offset < start.offset)
				{
					if (offset + delete_length <= start.offset)
						start.offset -= delete_length;
					else
						start.offset = offset;

					start.offset += insert_length;
				}

				if (node == end.node && offset < end.offset)
				{
					if (offset + delete_length <= end.offset)
						end.offset -= delete_length;
					else
						end.offset = offset;

					end.offset += insert_length;
				}
			}
		}
	}

#ifdef _DEBUG
	DebugCheckState(TRUE);
#endif // _DEBUG

	return OpStatus::OK;
}

class DOM_RangeOperation
	: public ListElement<DOM_RangeOperation>
{
public:
	enum Type { SPLIT_START, SPLIT_MIDDLE, SPLIT_END, SET_VALUE, CLONE_SHALLOW, CLONE_DEEP, REMOVE, INSERT, INSERT_AFTER, SET_NODE, SET_TARGET, SET_TARGET_PARENT, CALCULATE_NEW_OFFSET, SET_END } m_type;
	DOM_Range::BoundaryPoint bp;

	DOM_RangeOperation(Type type, DOM_Node *node, unsigned offset)
		: m_type(type), bp(node, offset, NULL)
	{
	}

	void GCTrace()
	{
		DOM_Object::GCMark(bp.node);
	}
};

class DOM_RangeState
	: public DOM_Object,
	  public ES_ThreadListener
{
public:
	enum SplitContainerType { SPLIT_NONE = -1, SPLIT_START, SPLIT_MIDDLE, SPLIT_END };

	/// The type of operation to be done on the range.
	enum Method { METHOD_DELETE, METHOD_EXTRACT, METHOD_CLONE, METHOD_INSERTNODE } method;
	/// List of sub operations to perform during execution.
	List<DOM_RangeOperation> operations;
	/// The range to do the operations on.
	DOM_Range *range;
	/// Resulting fragment for clone and extract.
	DOM_DocumentFragment *docfrag;
	/// Temporaries used for storing nodes during execution.
	DOM_Node *parent, *last;
	/// Used to store return values when calling sub functions.
	ES_Value return_value;
	/// If TRUE, the range will collapse on new_bp at the end.
	BOOL collapse;
	/// Used to store newly created node during splitting.
	DOM_CharacterData *stored_node;
	/// Stored chardata value from the last split operation.
	TempBuffer stored_value;
	/// Stored length in uni_chars of deleted value from the last split operation.
	unsigned stored_length;
	/// Position to collapse on or set as end point.
	DOM_Range::BoundaryPoint new_bp;

	DOM_RangeState(DOM_Range *range, Method method)
		: method(method), range(range), docfrag(NULL), parent(NULL), last(NULL), collapse(FALSE), stored_node(NULL), stored_length(0)
	{
	}

	virtual ~DOM_RangeState()
	{
		operations.Clear();
	}

	virtual void GCTrace()
	{
		GCMark(range);
		GCMark(docfrag);
		GCMark(parent);
		GCMark(last);
		GCMark(stored_node);
		GCMark(return_value);
		GCMark(new_bp.node);
		GCMark(new_bp.unit);

		for (DOM_RangeOperation *operation = operations.First(); operation; operation = operation->Suc())
			operation->GCTrace();
	}

	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_CANCELLED || signal == ES_SIGNAL_FAILED)
			Reset();

		return OpStatus::OK;
	}

	void Reset()
	{
		ES_ThreadListener::Remove();
		range->Reset();
	}

	static void CheckInvalidDoctypeL(DOM_Node *node)
	{
		if (node && node->GetNodeType() == DOCUMENT_TYPE_NODE)
			LEAVE(OpStatus::ERR_NOT_SUPPORTED);
	}

	void AddOperationL(DOM_RangeOperation::Type type, DOM_Node *node = NULL, unsigned offset = 0)
	{
		DOM_RangeOperation *operation = OP_NEW_L(DOM_RangeOperation, (type, node, offset));
		operation->Into(&operations);
	}


	OP_STATUS SplitContainer(SplitContainerType split_type, DOM_Range::BoundaryPoint &bp, DOM_Runtime *origining_runtime)
	{
		TempBuffer deleted;
		TempBuffer buf;

		OP_ASSERT(bp.node && bp.node->IsA(DOM_TYPE_CHARACTERDATA));
		stored_node = static_cast<DOM_CharacterData *>(bp.node);

		RETURN_IF_ERROR(stored_node->GetValue(&buf));
		const uni_char* value = buf.GetStorage() ? buf.GetStorage() : UNI_L("");

		if (split_type == SPLIT_START)
		{
			if (method != METHOD_CLONE)
				RETURN_IF_ERROR(stored_value.Append(value, bp.offset));

			OP_ASSERT(bp.offset <= uni_strlen(value) || !"The value has been changed without notifying the range of it. Add calls to DOM_Environment::ElementCharacterDataChanged()");
			RETURN_IF_ERROR(deleted.Append(value + bp.offset));
		}
		else if (split_type == SPLIT_MIDDLE)
		{
			DOM_Range::BoundaryPoint dummy_bp;
			DOM_Range::BoundaryPoint end_bp;
			range->GetEndPoints(dummy_bp, end_bp);

			if (method != METHOD_CLONE)
			{
				RETURN_IF_ERROR(stored_value.Append(value, bp.offset));
				OP_ASSERT(end_bp.offset <= uni_strlen(value) || !"The value has been changed without notifying the range of it. Add calls to DOM_Environment::ElementCharacterDataChanged()");
				RETURN_IF_ERROR(stored_value.Append(value + end_bp.offset));
			}

			OP_ASSERT(bp.offset <= uni_strlen(value) || !"The value has been changed without notifying the range of it. Add calls to DOM_Environment::ElementCharacterDataChanged()");
			RETURN_IF_ERROR(deleted.Append(value + bp.offset, end_bp.offset - bp.offset));
		}
		else
		{
			if (method != METHOD_CLONE)
			{
				OP_ASSERT(bp.offset <= uni_strlen(value) || !"The value has been changed without notifying the range of it. Add calls to DOM_Environment::ElementCharacterDataChanged()");
				RETURN_IF_ERROR(stored_value.Append(value + bp.offset));
			}
			RETURN_IF_ERROR(deleted.Append(value, bp.offset));
			bp.offset = 0;
		}

		stored_length = deleted.Length();

		OP_STATUS status = OpStatus::OK;
		if (method != METHOD_DELETE)
		{
			const uni_char *contents = deleted.GetStorage() ? deleted.GetStorage() : UNI_L("");

			if (stored_node->IsA(DOM_TYPE_TEXT))
			{
				DOM_Text *text;
				status = DOM_Text::Make(text, stored_node, contents);
				last = text;
			}
			else if (stored_node->IsA(DOM_TYPE_COMMENT))
			{
				DOM_Comment *comment;
				status = DOM_Comment::Make(comment, stored_node, contents);
				last = comment;
			}
			else
			{
				DOM_CDATASection *cdatasection;
				status = DOM_CDATASection::Make(cdatasection, stored_node, contents);
				last = cdatasection;
			}
		}
		else
			last = NULL;

		return status;
	}

	int Execute(BOOL restarted, ES_Value *real_return_value, DOM_Runtime *origining_runtime)
	{
		while (!operations.Empty())
		{
			DOM_RangeOperation *operation = operations.First();
			SplitContainerType split_type = SPLIT_NONE;
			ES_Value arguments[2];
			int result = ES_FAILED;
			BOOL call = FALSE;

			if (method == METHOD_CLONE && operation->m_type == DOM_RangeOperation::REMOVE)
				operation->m_type = DOM_RangeOperation::CLONE_DEEP;

			switch (operation->m_type)
			{
			case DOM_RangeOperation::SPLIT_START:
				split_type = SPLIT_START;
				break;

			case DOM_RangeOperation::SPLIT_MIDDLE:
				split_type = SPLIT_MIDDLE;
				break;

			case DOM_RangeOperation::SPLIT_END:
				split_type = SPLIT_END;
				break;

			case DOM_RangeOperation::SET_VALUE:
				{
					const uni_char *kept_str = stored_value.GetStorage();
					if (!kept_str)
						kept_str = UNI_L("");

					CALL_FAILED_IF_ERROR(stored_node->SetValue(kept_str, origining_runtime, method == METHOD_INSERTNODE ? DOM_MutationListener::SPLITTING : DOM_MutationListener::DELETING, operation->bp.offset, stored_length));
					stored_value.Clear();
					stored_length = 0;
					stored_node = NULL;
				}
				break;

			case DOM_RangeOperation::CLONE_SHALLOW:
			case DOM_RangeOperation::CLONE_DEEP:
				call = TRUE;
				DOMSetBoolean(&arguments[0], operation->m_type == DOM_RangeOperation::CLONE_DEEP);
				result = DOM_Node::cloneNode(operation->bp.node, arguments, 1, &return_value, origining_runtime);
				break;

			case DOM_RangeOperation::REMOVE:
				call = TRUE;
				if (!restarted)
				{
					DOM_Node *parent;
					CALL_FAILED_IF_ERROR(operation->bp.node->GetParentNode(parent));
					DOMSetObject(&arguments[0], operation->bp.node);
					result = DOM_Node::removeChild(parent, arguments, 1, &return_value, origining_runtime);
				}
				else
					result = DOM_Node::removeChild(NULL, NULL, -1, &return_value, origining_runtime);
				break;

			case DOM_RangeOperation::INSERT:
			case DOM_RangeOperation::INSERT_AFTER:
				call = TRUE;
				if (!restarted)
				{
					DOMSetObject(&arguments[0], last);
					if (operation->m_type == DOM_RangeOperation::INSERT_AFTER)
					{
						DOM_Node *before = operation->bp.node ? operation->bp.node : last;
						if (before)
							CALL_FAILED_IF_ERROR(before->GetNextSibling(before));
						DOMSetObject(&arguments[1], before);
					}
					else
						DOMSetObject(&arguments[1], operation->bp.node);
					result = DOM_Node::insertBefore(parent, arguments, 2, &return_value, origining_runtime);
				}
				else
					result = DOM_Node::insertBefore(NULL, NULL, -1, &return_value, origining_runtime);
				break;

			case DOM_RangeOperation::SET_NODE:
				last = operation->bp.node;
				break;

			case DOM_RangeOperation::SET_TARGET:
				parent = operation->bp.node ? operation->bp.node : last;
				break;

			case DOM_RangeOperation::SET_TARGET_PARENT:
				if (operation->bp.node)
					parent = operation->bp.node;
				CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
				break;

			case DOM_RangeOperation::CALCULATE_NEW_OFFSET:
				{
					DOM_Node *ref_node;
					// Set reference node.
					if (operation->bp.node)
						ref_node = operation->bp.node;
					else
						ref_node = last;

					// Set parent.
					if (ref_node)
						ref_node->GetParentNode(new_bp.node);
					else
						new_bp.node = parent; // Range start.

					// Set new offset.
					if (ref_node)
						new_bp.offset = DOM_Range::CalculateOffset(ref_node);
					else
						CALL_FAILED_IF_ERROR(DOM_Range::CountChildUnits(new_bp.offset, new_bp.node));

					new_bp.offset += operation->bp.offset;
				}
				break;

			case DOM_RangeOperation::SET_END:
				{
					DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
					OP_STATUS oom = range->SetEnd(new_bp, exception);
					if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
						return range->CallDOMException(exception, real_return_value);
					CALL_FAILED_IF_ERROR(oom);
					CALL_FAILED_IF_ERROR(range->UpdateCommonAncestor());
				}
				break;
			}

			if (call)
			{
				if (result == (ES_SUSPEND | ES_RESTART))
					goto suspend;
				if (result == ES_EXCEPTION)
					*real_return_value = return_value;
				if (result != ES_VALUE)
					return result;
				last = DOM_VALUE2OBJECT(return_value, DOM_Node);
			}

			if (split_type != SPLIT_NONE && !restarted)
			{
				CALL_FAILED_IF_ERROR(SplitContainer(split_type, operation->bp, origining_runtime));
				if (GetCurrentThread(origining_runtime)->IsBlocked())
					goto suspend;
			}

			operation->Out();
			OP_DELETE(operation);

			restarted = FALSE;
		}

		if (collapse)
		{
			DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
			OP_STATUS oom = range->SetStartAndEnd(new_bp, new_bp, exception);

			if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
				return range->CallDOMException(exception, real_return_value);
			CALL_FAILED_IF_ERROR(oom);
		}

		Reset();

		if (method != METHOD_INSERTNODE)
		{
			DOMSetObject(real_return_value, docfrag);
			return ES_VALUE;
		}
		else
			return ES_FAILED;

	suspend:
		range->SetBusy(TRUE);
		DOMSetObject(real_return_value, this);
		return ES_SUSPEND | ES_RESTART;
	}
};

static DOM_Node *
GetParentNodeL(DOM_Node *node)
{
	DOM_Node *parent;
	LEAVE_IF_ERROR(node->GetParentNode(parent));
	return parent;
}

static DOM_Node *
GetNextSiblingL(DOM_Node *node)
{
	OP_ASSERT(node);
	DOM_Node *next_sibling;
	LEAVE_IF_ERROR(node->GetNextSibling(next_sibling));
	return next_sibling;
}

void
DOM_Range::ExtractStartParentChainL(DOM_RangeState *state, DOM_Node *container)
{
	if (common_ancestor != container)
	{
		DOM_Node *parent = GetParentNodeL(container);
		ExtractStartParentChainL(state, parent);

		if (!container->IsA(DOM_TYPE_CHARACTERDATA))
		{
			if (state->method != DOM_RangeState::METHOD_DELETE)
			{
				DOM_RangeState::CheckInvalidDoctypeL(container);
				state->AddOperationL(DOM_RangeOperation::CLONE_SHALLOW, container);
				state->AddOperationL(DOM_RangeOperation::INSERT);
				state->AddOperationL(DOM_RangeOperation::SET_TARGET);
			}
		}
	}
}

void
DOM_Range::ExtractStartBranchL(DOM_RangeState *state, DOM_Node *container, DOM_Node *unit)
{
	if (common_ancestor != container)
	{
		if (container == start.node)
			ExtractStartParentChainL(state, container);

		DOM_Node *parent = GetParentNodeL(container);

		if (container->IsA(DOM_TYPE_CHARACTERDATA))
		{
			state->AddOperationL(DOM_RangeOperation::SPLIT_START, start.node, start.offset);
			if (state->method != DOM_RangeState::METHOD_DELETE)
				state->AddOperationL(DOM_RangeOperation::INSERT);
			if (state->method != DOM_RangeState::METHOD_CLONE)
				state->AddOperationL(DOM_RangeOperation::SET_VALUE, NULL, start.offset);
		}
		else
		{
			if (unit != start.unit)
				unit = GetNextSiblingL(unit);

			if (state->method == DOM_RangeState::METHOD_DELETE)
				while (unit)
				{
					state->AddOperationL(DOM_RangeOperation::REMOVE, unit);
					unit = GetNextSiblingL(unit);
				}
			else
			{
				while (unit)
				{
					DOM_RangeState::CheckInvalidDoctypeL(unit);
					state->AddOperationL(DOM_RangeOperation::REMOVE, unit);
					state->AddOperationL(DOM_RangeOperation::INSERT);

					unit = GetNextSiblingL(unit);
				}

				state->AddOperationL(DOM_RangeOperation::SET_TARGET_PARENT);
			}
		}

		ExtractStartBranchL(state, parent, container);
	}
}

void
DOM_Range::ExtractEndBranchL(DOM_RangeState *state, DOM_Node *container, DOM_Node *unit)
{
	if (common_ancestor != container)
	{
		DOM_Node *parent = GetParentNodeL(container);
		ExtractEndBranchL(state, parent, container);

		if (container->IsA(DOM_TYPE_CHARACTERDATA))
		{
			state->AddOperationL(DOM_RangeOperation::SPLIT_END, end.node, end.offset);
			if (state->method != DOM_RangeState::METHOD_DELETE)
				state->AddOperationL(DOM_RangeOperation::INSERT);
			if (state->method != DOM_RangeState::METHOD_CLONE)
				state->AddOperationL(DOM_RangeOperation::SET_VALUE, NULL, 0);
		}
		else
		{
			if (state->method != DOM_RangeState::METHOD_DELETE)
			{
				DOM_RangeState::CheckInvalidDoctypeL(unit);
				state->AddOperationL(DOM_RangeOperation::CLONE_SHALLOW, container);
				state->AddOperationL(DOM_RangeOperation::INSERT);
				state->AddOperationL(DOM_RangeOperation::SET_TARGET);
			}

			DOM_Node *iter;
			LEAVE_IF_ERROR(container->GetFirstChild(iter));

			if (state->method == DOM_RangeState::METHOD_DELETE)
				while (iter != unit)
				{
					state->AddOperationL(DOM_RangeOperation::REMOVE, iter);
					iter = GetNextSiblingL(iter);
				}
			else
				while (iter != unit)
				{
					DOM_RangeState::CheckInvalidDoctypeL(iter);
					state->AddOperationL(DOM_RangeOperation::REMOVE, iter);
					state->AddOperationL(DOM_RangeOperation::INSERT);

					iter = GetNextSiblingL(iter);
				}
		}
	}
}

void
DOM_Range::ExtractContentsL(DOM_RangeState *state)
{
	if (start.node == end.node)
		if (start.node->IsA(DOM_TYPE_CHARACTERDATA))
		{
			state->AddOperationL(DOM_RangeOperation::SPLIT_MIDDLE, start.node, start.offset);
			if (state->method != DOM_RangeState::METHOD_DELETE)
				state->AddOperationL(DOM_RangeOperation::INSERT);
			if (state->method != DOM_RangeState::METHOD_CLONE)
				state->AddOperationL(DOM_RangeOperation::SET_VALUE, NULL, start.offset);
		}
		else
		{
			OP_ASSERT(start.unit || !end.unit);
			DOM_Node *unit = start.unit;
			if (state->method == DOM_RangeState::METHOD_DELETE)
				while (unit != end.unit)
				{
					OP_ASSERT(unit);
					state->AddOperationL(DOM_RangeOperation::REMOVE, unit);
					unit = GetNextSiblingL(unit);
					OP_ASSERT(unit || !end.unit);
				}
			else
				while (unit != end.unit)
				{
					OP_ASSERT(unit);
					DOM_RangeState::CheckInvalidDoctypeL(unit);
					state->AddOperationL(DOM_RangeOperation::REMOVE, unit);
					state->AddOperationL(DOM_RangeOperation::INSERT);

					unit = GetNextSiblingL(unit);
					OP_ASSERT(unit || !end.unit);
				}
		}
	else
	{
		ExtractStartBranchL(state, start.node, start.unit);

		DOM_Node *container;
		DOM_Node *iter = start.unit;
		DOM_Node *stop = end.unit;

		container = start.node;
		while (container != common_ancestor)
		{
			iter = container;
			container = GetParentNodeL(container);
		}

		container = end.node;
		while (container != common_ancestor)
		{
			stop = container;
			container = GetParentNodeL(container);
		}

		if (iter != start.unit)
			// Leave the tree extracted by ExtractStartBranchL().
			iter = GetNextSiblingL(iter);

#ifdef _DEBUG
		if (stop && iter->GetThisElement() && stop->GetThisElement() && iter->GetThisElement() != stop->GetThisElement())
			OP_ASSERT(iter->GetThisElement()->Precedes(stop->GetThisElement()));
#endif // _DEBUG

		if (state->method == DOM_RangeState::METHOD_DELETE)
			while (iter != stop)
			{
				state->AddOperationL(DOM_RangeOperation::REMOVE, iter);
				iter = GetNextSiblingL(iter);
			}
		else
			while (iter != stop)
			{
				DOM_RangeState::CheckInvalidDoctypeL(iter);
				state->AddOperationL(DOM_RangeOperation::REMOVE, iter);
				state->AddOperationL(DOM_RangeOperation::INSERT);

				iter = GetNextSiblingL(iter);
			}

		ExtractEndBranchL(state, end.node, end.unit);
	}
}

OP_STATUS
DOM_Range::ExtractContents(DOM_RangeState *state)
{
	TRAPD(status, ExtractContentsL(state));
	return status;
}

void
DOM_Range::InsertNodeL(DOM_RangeState *state, DOM_Node *new_node)
{
	BOOL equal_end_points = IsCollapsed();
	unsigned new_offset_addition = 1;
	if (equal_end_points && new_node->GetNodeType() == DOCUMENT_FRAGMENT_NODE)
	{
		unsigned frag_length;
		LEAVE_IF_ERROR(DOM_Range::CountChildUnits(frag_length, new_node));
		new_offset_addition = frag_length;
	}

	if (start.node->IsA(DOM_TYPE_CHARACTERDATA))
	{
		state->AddOperationL(DOM_RangeOperation::SET_TARGET_PARENT, start.node);
		state->AddOperationL(DOM_RangeOperation::SPLIT_START, start.node, start.offset);
		state->AddOperationL(DOM_RangeOperation::INSERT_AFTER, start.node);
		state->AddOperationL(DOM_RangeOperation::SET_VALUE, NULL, start.offset);
		if (equal_end_points)
			state->AddOperationL(DOM_RangeOperation::CALCULATE_NEW_OFFSET, NULL, new_offset_addition);
		state->AddOperationL(DOM_RangeOperation::SET_NODE, new_node);
		state->AddOperationL(DOM_RangeOperation::INSERT_AFTER, start.node);
		if (equal_end_points)
			state->AddOperationL(DOM_RangeOperation::SET_END, NULL, 0);
	}
	else
	{
		state->AddOperationL(DOM_RangeOperation::SET_TARGET, start.node);
		if (equal_end_points)
			state->AddOperationL(DOM_RangeOperation::CALCULATE_NEW_OFFSET, start.unit, new_offset_addition);
		state->AddOperationL(DOM_RangeOperation::SET_NODE, new_node);
		state->AddOperationL(DOM_RangeOperation::INSERT, start.unit);
		if (equal_end_points)
			state->AddOperationL(DOM_RangeOperation::SET_END, NULL, 0);
	}
}

OP_STATUS
DOM_Range::InsertNode(DOM_RangeState *state, DOM_Node *new_node)
{
	TRAPD(status, InsertNodeL(state, new_node));
	return status;
}

#ifdef _DEBUG
void
DOM_Range::DebugCheckState(BOOL inside_tree_operation /* = FALSE */)
{
	if (detached)
	{
		OP_ASSERT(!start.node);
		OP_ASSERT(start.offset == 0);
		OP_ASSERT(!start.unit);
		OP_ASSERT(!end.node);
		OP_ASSERT(end.offset == 0);
		OP_ASSERT(!end.unit);
		return;
	}

	OP_ASSERT(start.node && end.node); // Either the range is detached, or it needs to have a start and end.
	OP_ASSERT(start.node != end.node || start.unit || !end.unit); // Can't have start NULL and end non-NULL unit if they're in the same container.
	OP_ASSERT(start.node != end.node || end.offset >= start.offset);

	HTML_Element* debug_start = DOM_Range::GetAssociatedElement(start.node);
	HTML_Element* debug_end = DOM_Range::GetAssociatedElement(end.node);
	HTML_Element* debug_common_ancestor = DOM_Range::GetAssociatedElement(common_ancestor);

	OP_ASSERT(tree_root); // Must have a tree_root if we are not detached.
	OP_ASSERT(debug_start && tree_root->IsAncestorOf(debug_start)); // Must have a start that is a descendant of tree_root.
	OP_ASSERT(debug_end && tree_root->IsAncestorOf(debug_end)); // Must have an end that is a descendant of tree_root.
	OP_ASSERT(debug_common_ancestor->IsAncestorOf(debug_start) && debug_common_ancestor->IsAncestorOf(debug_end)); // The common ancestor must be an ancestor of both the start and end.

	if (debug_start && debug_end && debug_start != debug_end)
	{
		OP_ASSERT(debug_start->Precedes(debug_end) || debug_end->IsAncestorOf(debug_start));
		if (debug_end->IsAncestorOf(debug_start))
		{
			// Find the child of debug_end that holds the tree that start belongs to...
			HTML_Element *iter = debug_start;
			while (iter->ParentActual() != debug_end)
				iter = iter->ParentActual();

			// ...calculate the index of that child...
			unsigned start_index = 0;
			while (iter->PredActual())
			{
				start_index++;
				iter = iter->PredActual();
			}

			// ...and check that the end offset is after that child.
			OP_ASSERT(end.offset >= start_index || !"End point must be after the start point of the range.");
		}
	}

	if (debug_start && debug_start == debug_end)
	{
		OP_ASSERT(start.offset <= end.offset);
		OP_ASSERT(start.unit || !end.unit);
	}

	if (!inside_tree_operation)
	{
		DOM_Node* unit;
		if (start.node->IsA(DOM_TYPE_CHARACTERDATA))
			OP_ASSERT(!start.unit);
		else
		{
			DOM_Range::GetOffsetNode(unit, start);
			OP_ASSERT(unit == start.unit);
		}
		if (end.node->IsA(DOM_TYPE_CHARACTERDATA))
			OP_ASSERT(!end.unit);
		else
		{
			DOM_Range::GetOffsetNode(unit, end);
			OP_ASSERT(unit == end.unit);
		}
	}
}
#endif // _DEBUG

/* static */ int
DOM_Range::collapse(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("b");
	START_CALL;

	OP_STATUS oom;
	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;

	if (argv[0].value.boolean)
		oom = range->SetEnd(range->start, exception);
	else
		oom = range->SetStart(range->end, exception);

	if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
		return this_object->CallDOMException(exception, return_value);
	CALL_FAILED_IF_ERROR(oom);

	range->common_ancestor = range->start.node;
#ifdef DOM_SELECTION_SUPPORT
	if (range->selection)
		CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

	OP_ASSERT(range->start.node != range->end.node || range->start.unit || !range->end.unit);
	OP_ASSERT(range->tree_root->IsAncestorOf(DOM_Range::GetAssociatedElement(range->start.node)));

	return ES_FAILED;
}

/* static */ int
DOM_Range::selectNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);
	START_CALL;

	switch (node->GetNodeType())
	{
	case ATTRIBUTE_NODE:
	case ENTITY_NODE:
	case DOCUMENT_NODE:
	case DOCUMENT_FRAGMENT_NODE:
	case NOTATION_NODE:
		return DOM_CALL_DOMEXCEPTION(INVALID_NODE_TYPE_ERR);
	}

	DOM_Node *parent;
	CALL_FAILED_IF_ERROR(node->GetParentNode(parent));

	if (!parent)
		return DOM_CALL_DOMEXCEPTION(INVALID_NODE_TYPE_ERR);

	BoundaryPoint new_start_bp(parent, CalculateOffset(node), node);
	BoundaryPoint new_end_bp(parent, new_start_bp.offset + 1, NULL);

	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
	OP_STATUS oom = range->SetStartAndEnd(new_start_bp, new_end_bp, exception);
	if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
		return this_object->CallDOMException(exception, return_value);

	OP_ASSERT(range->start.node != range->end.node || range->start.unit || !range->end.unit);
	OP_ASSERT(range->tree_root->IsAncestorOf(DOM_Range::GetAssociatedElement(range->start.node)));

#ifdef DOM_SELECTION_SUPPORT
	if (range->selection)
		CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

	return OpStatus::IsMemoryError(oom) ? ES_NO_MEMORY : ES_FAILED;
}

/* static */ int
DOM_Range::selectNodeContents(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);
	START_CALL;

	if (node->GetNodeType() == DOCUMENT_TYPE_NODE)
		return DOM_CALL_DOMEXCEPTION(INVALID_NODE_TYPE_ERR);

	unsigned child_units;
	CALL_FAILED_IF_ERROR(CountChildUnits(child_units, node));

	BoundaryPoint new_start_bp(node, 0, NULL);
	BoundaryPoint new_end_bp(node, child_units, NULL);

	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
	OP_STATUS oom = range->SetStartAndEnd(new_start_bp, new_end_bp, exception);

	if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
		return this_object->CallDOMException(exception, return_value);
	CALL_FAILED_IF_ERROR(oom);

#ifdef DOM_SELECTION_SUPPORT
	if (range->selection)
		CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

	OP_ASSERT(range->start.node != range->end.node || range->start.unit || !range->end.unit);
	OP_ASSERT(range->tree_root->IsAncestorOf(DOM_Range::GetAssociatedElement(range->start.node)));

	return ES_FAILED;
}

/* static */ int
DOM_Range::compareBoundaryPoints(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("No");
	DOM_ARGUMENT_OBJECT(sourceRange, 1, DOM_TYPE_RANGE, DOM_Range);
	START_CALL;

	if (sourceRange->detached)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	unsigned short how = TruncateDoubleToUShort(argv[0].value.number);

	if (how > END_TO_START)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	if (sourceRange->GetTreeRoot() != range->GetTreeRoot())
		return DOM_CALL_DOMEXCEPTION(WRONG_DOCUMENT_ERR);

	BoundaryPoint this_bp;
	BoundaryPoint other_bp;

	if (how == START_TO_START || how == END_TO_START)
		this_bp = range->start;
	else
		this_bp = range->end;

	if (how == START_TO_START || how == START_TO_END)
		other_bp = sourceRange->start;
	else
		other_bp = sourceRange->end;

	int result = CompareBoundaryPoints(this_bp, other_bp);

	DOMSetNumber(return_value, result);
	return ES_VALUE;
}

/* static */ int
DOM_Range::insertNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_Range *range;
	DOM_RangeState *state;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(range, DOM_TYPE_RANGE, DOM_Range);
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT(new_node, 0, DOM_TYPE_NODE, DOM_Node);
		START_CALL;

#ifdef _DEBUG
		range->DebugCheckState();
#endif // _DEBUG
		if (range->start.node->GetNodeType() == COMMENT_NODE)
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		else if (range->start.node->GetNodeType() == TEXT_NODE)
		{
			HTML_Element *parent = DOM_Range::GetAssociatedElement(range->start.node);
			if (!parent->ParentActual())
				return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		}

		CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(state = OP_NEW(DOM_RangeState, (range, DOM_RangeState::METHOD_INSERTNODE)), range->GetRuntime()));
		GetCurrentThread(origining_runtime)->AddListener(state);

		CALL_FAILED_IF_ERROR(range->InsertNode(state, new_node));

		int ret_val = state->Execute(FALSE, return_value, origining_runtime);
#ifdef _DEBUG
		range->DebugCheckState();
#endif // _DEBUG
#ifdef DOM_SELECTION_SUPPORT
		if (ret_val != (ES_SUSPEND | ES_RESTART) && range->selection)
			CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

		return ret_val;
	}
	else if (argc < 0)
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_RangeState);
		range = state->range;

		if (!range->Reset())
			return range->CallInternalException(UNEXPECTED_MUTATION_ERR, return_value);

		int ret_val = state->Execute(TRUE, return_value, origining_runtime);
#ifdef _DEBUG
		range->DebugCheckState();
#endif // _DEBUG
#ifdef DOM_SELECTION_SUPPORT
		if (range->selection)
			CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

		return ret_val;
	}
	else
		return ES_FAILED;
}

class DOM_SurroundContentsState
	: public DOM_Object
{
public:
	enum { EXTRACT_CONTENTS, INSERT_NODE, APPEND_CHILD, SELECT_NODE } point;

	DOM_Range *range;
	DOM_Node *newParent;
	DOM_DocumentFragment *fragment;
	ES_Value return_value;
	BOOL restarted;

	DOM_SurroundContentsState(DOM_Range *range)
		: point(EXTRACT_CONTENTS), range(range), newParent(NULL), fragment(NULL), restarted(FALSE)
	{
	}

	virtual void GCTrace()
	{
		GCMark(range);
		GCMark(newParent);
		GCMark(fragment);
		GCMark(return_value);
	}
};

/* static */ int
DOM_Range::surroundContents(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_Range *range;
	DOM_SurroundContentsState *state;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(range, DOM_TYPE_RANGE, DOM_Range);
		DOM_CHECK_ARGUMENTS("o");
		START_CALL;

		CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(state = OP_NEW(DOM_SurroundContentsState, (range)), range->GetRuntime()));
		DOM_ARGUMENT_OBJECT_EXISTING(state->newParent, 0, DOM_TYPE_NODE, DOM_Node);

		/* INVALID_STATE_ERR: Raised if the Range partially selects a non-text node. */
		if (range->start.node != range->common_ancestor || range->end.node != range->common_ancestor)
		{
			BOOL start_is_ok = FALSE, end_is_ok = FALSE;

			if (range->start.node->IsA(DOM_TYPE_TEXT))
			{
				DOM_Node *parent_node;
				CALL_FAILED_IF_ERROR(range->start.node->GetParentNode(parent_node));
				if (parent_node == range->common_ancestor)
					start_is_ok = TRUE;
			}
			else
				start_is_ok = range->start.node->IsAncestorOf(range->end.node);

			if (range->end.node->IsA(DOM_TYPE_TEXT))
			{
				DOM_Node *parent_node;
				CALL_FAILED_IF_ERROR(range->end.node->GetParentNode(parent_node));
				if (parent_node == range->common_ancestor)
					end_is_ok = TRUE;
			}
			else
				end_is_ok = range->end.node->IsAncestorOf(range->start.node);

			if (!start_is_ok || !end_is_ok)
				return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
		}

		switch (state->newParent->GetNodeType())
		{
		case ATTRIBUTE_NODE:
		case ENTITY_NODE:
		case DOCUMENT_TYPE_NODE:
		case NOTATION_NODE:
		case DOCUMENT_NODE:
		case DOCUMENT_FRAGMENT_NODE:
			return DOM_CALL_DOMEXCEPTION(INVALID_NODE_TYPE_ERR);
		}
	}
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_SurroundContentsState);
		state->restarted = TRUE;
		this_object = range = state->range;

		if (!range->Reset())
			return range->CallInternalException(UNEXPECTED_MUTATION_ERR, return_value);
	}

	if (range->detached)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	int result = ES_FAILED;

	switch (state->point)
	{
	case DOM_SurroundContentsState::EXTRACT_CONTENTS:
		{
			result = DOM_Range::deleteOrExtractOrCloneContents(range, NULL, !state->restarted ? 0 : -1, &state->return_value, origining_runtime, 1);
			if (result != ES_VALUE)
				break;

			state->fragment = DOM_VALUE2OBJECT(state->return_value, DOM_DocumentFragment);
			state->restarted = FALSE;

			DOM_Node *child;
			CALL_FAILED_IF_ERROR(state->newParent->GetFirstChild(child));

			while (child)
			{
				child->RemoveFromParent(origining_runtime);
				CALL_FAILED_IF_ERROR(state->newParent->GetFirstChild(child));
			}

			state->point = DOM_SurroundContentsState::INSERT_NODE;
			state->restarted = FALSE;
		}
		// fall through

	case DOM_SurroundContentsState::INSERT_NODE:
		{
			ES_Value argument;
			DOMSetObject(&argument, state->newParent);

			result = insertNode(range, &argument, !state->restarted ? 1 : -1, &state->return_value, origining_runtime);
			if (result != ES_FAILED)
				break;

			DOMSetObject(&state->return_value, state->fragment); // set the argument for appendChild
			state->point = DOM_SurroundContentsState::APPEND_CHILD;
			state->restarted = FALSE;
		}
		// fall through

	case DOM_SurroundContentsState::APPEND_CHILD:
		result = DOM_Node::appendChild(state->newParent, &state->return_value, !state->restarted ? 1 : -1, &state->return_value, origining_runtime);
		if (result != ES_VALUE)
			break;

		state->point = DOM_SurroundContentsState::SELECT_NODE;
		state->restarted = FALSE;
		// fall through

	case DOM_SurroundContentsState::SELECT_NODE:
		{
			ES_Value argument;
			DOMSetObject(&argument, state->newParent);

			result = DOM_Range::selectNode(range, &argument, !state->restarted ? 1 : -1, &state->return_value, origining_runtime);
		}
	}

	if (result == (ES_SUSPEND | ES_RESTART))
	{
		DOMSetObject(return_value, state);
		range->busy = TRUE;
	}
	else if (result == ES_EXCEPTION)
		*return_value = state->return_value;
	else if (result == ES_VALUE)
	{
#ifdef DOM_SELECTION_SUPPORT
		if (range->selection)
			CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT
		result = ES_FAILED;
	}

	return result;
}


/* static */ int
DOM_Range::cloneRange(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	if (argc == 0)
	{
		DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);

		if (range->detached)
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

		DOM_Range *clone;
		CALL_FAILED_IF_ERROR(DOM_Range::Make(clone, range->GetOwnerDocument()));

		DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
		OP_STATUS oom = clone->SetStartAndEnd(range->start, range->end, exception);

		if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
			return this_object->CallDOMException(exception, return_value);
		CALL_FAILED_IF_ERROR(oom);

		OP_ASSERT(clone->start.node != clone->end.node || clone->start.unit || !clone->end.unit);
		OP_ASSERT(clone->tree_root->IsAncestorOf(DOM_Range::GetAssociatedElement(clone->start.node)));

		DOMSetObject(return_value, clone);
		return ES_VALUE;
	}

	return ES_FAILED;
}

/* static */ int
DOM_Range::toString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);

	if (range->detached)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	TempBuffer *buffer = range->GetEmptyTempBuf();
	DOM_Node *iter;
	DOM_Node *stop;

	if (!range->start.node->IsA(DOM_TYPE_CHARACTERDATA))
	{
		CALL_FAILED_IF_ERROR(GetOffsetNode(iter, range->start));

		if (!iter)
			CALL_FAILED_IF_ERROR(range->start.node->GetNextNode(iter, TRUE));
	}
	else
		iter = range->start.node;

	if (!range->end.node->IsA(DOM_TYPE_CHARACTERDATA))
		CALL_FAILED_IF_ERROR(GetOffsetNode(stop, range->end));
	else
		stop = NULL;

	if (!stop)
		CALL_FAILED_IF_ERROR(range->end.node->GetNextNode(stop, TRUE));

	while (iter != stop)
	{
		if (iter->IsA(DOM_TYPE_TEXT))
		{
			TempBuffer value;
			CALL_FAILED_IF_ERROR(((DOM_CharacterData *) iter)->GetValue(&value));
			unsigned offset, length;

			if (iter == range->start.node)
				offset = range->start.offset;
			else
				offset = 0;

			if (iter == range->end.node)
				length = range->end.offset - offset;
			else
				length = value.Length() - offset;

			/* Check that we have sane values. */
			OP_ASSERT(offset <= value.Length());
			OP_ASSERT(offset + length <= value.Length());

			CALL_FAILED_IF_ERROR(buffer->Append(value.GetStorage() + offset, length));
		}

		CALL_FAILED_IF_ERROR(iter->GetNextNode(iter));
	}

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

/* static */ int
DOM_Range::detach(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	START_CALL;

	range->Detach();
#ifdef DOM_SELECTION_SUPPORT
	if (range->selection)
		CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT
	return ES_FAILED;
}

class DOM_createContextualFragment_Callback
	: public OpTreeCallback
{
public:
	virtual OP_STATUS ElementFound(HTML_Element *found_element)
	{
		element = found_element;
		return OpStatus::OK;
	}

	virtual OP_STATUS ElementNotFound()
	{
		return OpStatus::OK;
	}

	HTML_Element *element;
};

/* static */ int
DOM_Range::createContextualFragment(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("z");
	START_CALL;

	/* It is completely bizarre that this function is a member of the
	   Range interface.  All the context information it actually needs
	   is a node.  Mozilla's implementation seems to pick start.m_node,
	   so so will I. */

	DOM_Document *document = range->GetOwnerDocument();
	DOM_Node *node = range->start.node;
	DOM_DocumentFragment *fragment;

	HTML_Element *element = node->GetThisElement() ? node->GetThisElement() : node->GetPlaceholderElement();
	while (element && !Markup::IsRealElement(element->Type()))
		element = element->ParentActual();

	if (document->IsXML() && document->GetLogicalDocument())
	{
		DOM_createContextualFragment_Callback callback;
		callback.element = NULL;

		XMLNamespaceDeclaration::Reference nsdeclaration;
		if (element)
		{
			CALL_FAILED_IF_ERROR(XMLNamespaceDeclaration::PushAllInScope(nsdeclaration, element));

			if (!XMLNamespaceDeclaration::FindDefaultDeclaration(nsdeclaration, TRUE))
			{
				// If there is no default ns, use the one from the context element.
				NS_Element *ns_elm = g_ns_manager->GetElementAt(element->GetNsIdx());
				const uni_char *ns_uri = ns_elm->GetUri();
				if (ns_uri)
					CALL_FAILED_IF_ERROR(XMLNamespaceDeclaration::Push(nsdeclaration, ns_uri, uni_strlen(ns_uri), NULL, 0, 0));
			}
		}

		XMLTokenHandler *tokenhandler;
		CALL_FAILED_IF_ERROR(OpTreeCallback::MakeTokenHandler(tokenhandler, document->GetLogicalDocument(), &callback));

		URL url;
		XMLParser *parser;
		if (OpStatus::IsMemoryError(XMLParser::Make(parser, NULL, (MessageHandler *) NULL, tokenhandler, url)))
		{
			OP_DELETE(tokenhandler);
			return ES_NO_MEMORY;
		}

		XMLParser::Configuration configuration;
		configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
		configuration.parse_mode = XMLParser::PARSEMODE_FRAGMENT;
		configuration.nsdeclaration = nsdeclaration;
		parser->SetConfiguration(configuration);

		OP_STATUS status = parser->Parse(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length, FALSE);
		BOOL successful = OpStatus::IsSuccess(status) && parser->IsFinished();

		OP_DELETE(parser);
		OP_DELETE(tokenhandler);

		if (!successful && callback.element)
		{
			HTML_Element::DOMFreeElement(callback.element, range->GetEnvironment());
			callback.element = NULL;
		}

		CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(fragment, document, callback.element));
	}
	else
	{
		CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(fragment, document));

		DOM_EnvironmentImpl::CurrentState state(range->GetEnvironment(), origining_runtime);
		state.SetOwnerDocument(document);

		CALL_FAILED_IF_ERROR(fragment->GetPlaceholderElement()->DOMSetInnerHTML(range->GetEnvironment(), argv[0].value.string_with_length->string, argv[0].value.string_with_length->length, element));
	}

	DOMSetObject(return_value, fragment);
	return ES_VALUE;
}

/* static */ int
DOM_Range::intersectsNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);

	if (argc == 2)
		DOM_CHECK_ARGUMENTS("o-");
	else
		DOM_CHECK_ARGUMENTS("o");

	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);
	START_CALL;

	HTML_Element *node_root = DOM_Range::FindTreeRoot(node);
	if (node_root && node_root == range->GetTreeRoot())
	{
		DOM_Node *parent_node;
		CALL_FAILED_IF_ERROR(node->GetParentNode(parent_node));

		if (parent_node)
		{
			BoundaryPoint ref_bp(parent_node, DOM_Range::CalculateOffset(node), node);

			int result = CompareBoundaryPoints(ref_bp, range->end);

			if (result == -1) // Reference is before end.
			{
				ref_bp.offset++;
				result = CompareBoundaryPoints(ref_bp, range->start);
				DOMSetBoolean(return_value, result == 1);
			}
			else
				DOMSetBoolean(return_value, FALSE);
		}
		else
			DOMSetBoolean(return_value, TRUE);
	}
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOM_Range::getClientRects(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	START_CALL;

	DOM_ClientRectList *list = NULL;
	CALL_FAILED_IF_ERROR(DOM_ClientRectList::Make(list, range->GetRuntime()));

	if (range->start != range->end)
	{
		OpVector<RECT> &rect_list = list->GetList();
		HTML_Element *start_elm = DOM_Range::GetAssociatedElement(range->start.node);
		HTML_Element *last_elm = DOM_Range::GetAssociatedElement(range->end.node);
		if (start_elm && last_elm)
		{
			DOM_Environment *env = range->GetEnvironment();
			CALL_FAILED_IF_ERROR(start_elm->DOMGetClientRects(env, NULL, &rect_list, last_elm, range->start.offset, range->end.offset));
		}
	}

	DOMSetObject(return_value, list);

	return ES_VALUE;
}

/* static */ int
DOM_Range::getBoundingClientRect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	START_CALL;

	RECT bounding_rect;
	bounding_rect.bottom = LONG_MIN;

	if (range->start != range->end)
	{
		HTML_Element *start_elm = DOM_Range::GetAssociatedElement(range->start.node);
		HTML_Element *last_elm = DOM_Range::GetAssociatedElement(range->end.node);
		if (start_elm && last_elm)
		{
			DOM_Environment *env = range->GetEnvironment();
			CALL_FAILED_IF_ERROR(start_elm->DOMGetClientRects(env, &bounding_rect, NULL, last_elm, range->start.offset, range->end.offset));
		}
	}

	if (bounding_rect.bottom == LONG_MIN) // If no rects have been found return empty rect.
		bounding_rect.bottom = bounding_rect.top = bounding_rect.left = bounding_rect.right = 0;

	DOM_Object *object;
	CALL_FAILED_IF_ERROR(DOM_Element::MakeClientRect(object, bounding_rect, range->GetRuntime()));
	DOMSetObject(return_value, object);

	return ES_VALUE;
}

/* static */ int
DOM_Range::setStartOrEnd(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("on");
	DOM_ARGUMENT_OBJECT(container, 0, DOM_TYPE_NODE, DOM_Node);
	START_CALL;

	BoundaryPoint new_bp(container, TruncateDoubleToUInt(argv[1].value.number), NULL);

	OP_STATUS oom;
	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
	if (data == 0)
		oom = range->SetStart(new_bp, exception);
	else
		oom = range->SetEnd(new_bp, exception);

	if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
		return this_object->CallDOMException(exception, return_value);
	CALL_FAILED_IF_ERROR(oom);

	CALL_FAILED_IF_ERROR(range->UpdateCommonAncestor());
#ifdef DOM_SELECTION_SUPPORT
	if (range->selection)
		CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

	OP_ASSERT(range->start.node != range->end.node || range->start.unit || !range->end.unit);
	OP_ASSERT(range->tree_root->IsAncestorOf(DOM_Range::GetAssociatedElement(range->start.node)));

	return ES_FAILED;
}

/* static */ int
DOM_Range::setStartOrEndBeforeOrAfter(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	// data == 0: setStartBefore, data == 1: setStartAfter
	// data == 2: setEndBefore, data == 3: setEndAfter
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(container, 0, DOM_TYPE_NODE, DOM_Node);
	START_CALL;

	DOM_Node *parent;
	CALL_FAILED_IF_ERROR(container->GetParentNode(parent));

	if (!parent)
		return DOM_CALL_DOMEXCEPTION(INVALID_NODE_TYPE_ERR);

	BoundaryPoint new_bp(parent, CalculateOffset(container), container);

	if (data == 1 || data == 3)
	{
		new_bp.offset++;
		new_bp.unit = NULL;
	}

	BOOL set_start = data == 0 || data == 1;

	DOM_Object::DOMException exception = DOM_Object::DOMEXCEPTION_NO_ERR;
	if (set_start)
	{
		OP_STATUS oom = range->SetStart(new_bp, exception);

		if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
			return this_object->CallDOMException(exception, return_value);
		CALL_FAILED_IF_ERROR(oom);
	}
	else
	{
		OP_STATUS oom = range->SetEnd(new_bp, exception);

		if (exception != DOM_Object::DOMEXCEPTION_NO_ERR)
			return this_object->CallDOMException(exception, return_value);
		CALL_FAILED_IF_ERROR(oom);
	}

	CALL_FAILED_IF_ERROR(range->UpdateCommonAncestor());
#ifdef DOM_SELECTION_SUPPORT
	if (range->selection)
		CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

	OP_ASSERT(range->start.node != range->end.node || range->start.unit || !range->end.unit);

	return ES_FAILED;
}

/* static */ int
DOM_Range::deleteOrExtractOrCloneContents(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	/* data: 0 => deleteContents, 1 => extractContents, 2 => cloneContents */

	DOM_Range *range;
	DOM_RangeState *state;

	if (argc == 0)
	{
		DOM_THIS_OBJECT_EXISTING(range, DOM_TYPE_RANGE, DOM_Range);
		START_CALL;

#ifdef _DEBUG
		range->DebugCheckState();
#endif // _DEBUG

		DOM_RangeState::Method method;
		if (data == 0)
			method = DOM_RangeState::METHOD_DELETE;
		else if (data == 1)
			method = DOM_RangeState::METHOD_EXTRACT;
		else
			method = DOM_RangeState::METHOD_CLONE;

		DOM_DocumentFragment* docfrag = NULL;
		if (data != 0)
			CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(docfrag, range->GetOwnerDocument()));

		if (range->IsCollapsed())
		{
			DOMSetObject(return_value, docfrag);
			return ES_VALUE;
		}

		CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(state = OP_NEW(DOM_RangeState, (range, method)), range->GetRuntime()));
		GetCurrentThread(origining_runtime)->AddListener(state);

		if (data != 0)
			state->parent = state->docfrag = docfrag;

		if (data != 2)
		{
			DOM_Node *unit = NULL;
			state->new_bp = range->start;

			while (state->new_bp.node != range->common_ancestor)
			{
				unit = state->new_bp.node;
				CALL_FAILED_IF_ERROR(unit->GetParentNode(state->new_bp.node));
			}

			state->collapse = TRUE;
			if (unit)
				state->new_bp.offset = CalculateOffset(unit) + 1;
			state->new_bp.unit = NULL;
		}

#ifdef _DEBUG
		range->DebugCheckState();
#endif // _DEBUG

		OP_STATUS exctract_status = range->ExtractContents(state);

		if (exctract_status == OpStatus::ERR_NOT_SUPPORTED) // Illegal doctype encountered.
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);

		CALL_FAILED_IF_ERROR(exctract_status);

		int ret_val = state->Execute(FALSE, return_value, origining_runtime);
#ifdef DOM_SELECTION_SUPPORT
		if (ret_val != (ES_SUSPEND | ES_RESTART) && range->selection)
			CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT
#ifdef _DEBUG
		range->DebugCheckState();
#endif // _DEBUG
		return ret_val;
	}
	else if (argc < 0)
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_RangeState);
		range = state->range;

		if (!range->Reset())
			return range->CallInternalException(UNEXPECTED_MUTATION_ERR, return_value);

		int ret_val = state->Execute(TRUE, return_value, origining_runtime);
#ifdef DOM_SELECTION_SUPPORT
		if (range->selection)
			CALL_FAILED_IF_ERROR(range->selection->UpdateSelectionFromRange());
#endif // DOM_SELECTION_SUPPORT

#ifdef _DEBUG
		range->DebugCheckState();
#endif // _DEBUG
		return ret_val;
	}
	else
		return ES_FAILED;
}

/* static */ int
DOM_Range::comparePoint(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	// data == 0: comparePoint, data == 1: isPointInRange
	DOM_THIS_OBJECT(range, DOM_TYPE_RANGE, DOM_Range);
	DOM_CHECK_ARGUMENTS("on");
	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);
	START_CALL;

	HTML_Element *node_root = DOM_Range::FindTreeRoot(node);
	if (node_root && node_root == range->GetTreeRoot())
	{
		if (node->GetNodeType() == DOCUMENT_TYPE_NODE)
			return DOM_CALL_DOMEXCEPTION(INVALID_NODE_TYPE_ERR);

		BoundaryPoint ref_bp(node, TruncateDoubleToUInt(argv[1].value.number), NULL);

		unsigned length;
		CALL_FAILED_IF_ERROR(CountChildUnits(length, node));
		if (ref_bp.offset > length)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

		int result = CompareBoundaryPoints(range->start, ref_bp);

		if (result == -1) // Start before reference.
		{
			result = CompareBoundaryPoints(range->end, ref_bp);
			if (data == 0)
				DOMSetNumber(return_value, result == -1 ? 1 : 0);
			else
				DOMSetBoolean(return_value, result != -1);
		}
		else // Reference before or equal to start.
		{
			if (data == 0)
				DOMSetNumber(return_value, -result);
			else
				DOMSetBoolean(return_value, result == 0);
		}
	}
	else if (data == 0)
		return DOM_CALL_DOMEXCEPTION(WRONG_DOCUMENT_ERR);
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Range)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::collapse, "collapse", "b-")
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::selectNode, "selectNode", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::selectNodeContents, "selectNodeContents", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::compareBoundaryPoints, "compareBoundaryPoints", "no-")
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::insertNode, "insertNode", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::surroundContents, "surroundContents", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::cloneRange, "cloneRange", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::toString, "toString", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::detach, "detach", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::createContextualFragment, "createContextualFragment", "z-")
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::intersectsNode, "intersectsNode", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::getClientRects, "getClientRects", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Range, DOM_Range::getBoundingClientRect, "getBoundingClientRect", 0)
DOM_FUNCTIONS_END(DOM_Range)

DOM_FUNCTIONS_WITH_DATA_START(DOM_Range)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::setStartOrEnd, 0, "setStart", "-n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::setStartOrEnd, 1, "setEnd", "-n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::setStartOrEndBeforeOrAfter, 0, "setStartBefore", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::setStartOrEndBeforeOrAfter, 1, "setStartAfter", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::setStartOrEndBeforeOrAfter, 2, "setEndBefore", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::setStartOrEndBeforeOrAfter, 3, "setEndAfter", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::deleteOrExtractOrCloneContents, 0, "deleteContents", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::deleteOrExtractOrCloneContents, 1, "extractContents", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::deleteOrExtractOrCloneContents, 2, "cloneContents", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::comparePoint, 0, "comparePoint", "on-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Range, DOM_Range::comparePoint, 1, "isPointInRange", "on-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_Range)

#endif // DOM2_RANGE
