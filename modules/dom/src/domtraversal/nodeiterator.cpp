/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM2_TRAVERSAL

#include "modules/dom/src/domtraversal/nodeiterator.h"
#include "modules/dom/src/domtraversal/nodefilter.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"

#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domeventtarget.h"

#include "modules/logdoc/htm_elm.h"

class DOM_NodeIteratorMutationListener
	: public DOM_MutationListener
{
protected:
	DOM_NodeIterator *node_iterator;

public:
	DOM_NodeIteratorMutationListener(DOM_NodeIterator *node_iterator)
		: node_iterator(node_iterator)
	{
	}

	virtual OP_STATUS OnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime)
	{
		return node_iterator->ElementRemoved(child);
	}
};

class DOM_NodeIterator_State
	: public DOM_Object
{
private:
	class Tried
	{
	public:
		DOM_Node *node;
		Tried *next;
	};

	Tried *first_tried;
	unsigned private_name;

public:
	DOM_NodeIterator_State();
	virtual ~DOM_NodeIterator_State();
	virtual void GCTrace();

	OP_STATUS AddTried(DOM_Node *node);
	BOOL HasTried(DOM_Node *node);
};

DOM_NodeIterator_State::DOM_NodeIterator_State()
	: first_tried(NULL),
	  private_name(0)
{
}

DOM_NodeIterator_State::~DOM_NodeIterator_State()
{
	Tried *tried = first_tried;
	while (tried)
	{
		Tried *next_tried = tried->next;
		OP_DELETE(tried);
		tried = next_tried;
	}
}

void
DOM_NodeIterator_State::GCTrace()
{
	Tried *tried = first_tried;
	while (tried)
	{
		GCMark(tried->node);
		tried = tried->next;
	}
}

OP_STATUS
DOM_NodeIterator_State::AddTried(DOM_Node *node)
{
	RETURN_IF_ERROR(PutPrivate(private_name++, *node));

	Tried *tried = OP_NEW(Tried, ());
	if (!tried)
		return OpStatus::ERR_NO_MEMORY;

	tried->node = node;
	tried->next = first_tried;
	first_tried = tried;

	return OpStatus::OK;
}

BOOL
DOM_NodeIterator_State::HasTried(DOM_Node *node)
{
	Tried *tried = first_tried;
	while (tried)
	{
		if (tried->node == node)
			return TRUE;
		tried = tried->next;
	}
	return FALSE;
}

DOM_NodeIterator::DOM_NodeIterator()
	: reference(NULL),
	  current_candidate(NULL),
	  state(NULL),
	  listener(NULL),
	  forward(FALSE),
	  reference_node_changed_by_filter(FALSE)
{
}

DOM_NodeIterator::~DOM_NodeIterator()
{
	Detach();
}

/* static */ OP_STATUS
DOM_NodeIterator::Make(DOM_NodeIterator *&node_iterator, DOM_EnvironmentImpl *environment, DOM_Node *root,
                       unsigned what_to_show, ES_Object *filter, BOOL entity_reference_expansion)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(node_iterator = OP_NEW(DOM_NodeIterator, ()), runtime, runtime->GetPrototype(DOM_Runtime::NODEITERATOR_PROTOTYPE), "NodeIterator"));

	if (filter)
		RETURN_IF_ERROR(node_iterator->PutPrivate(DOM_PRIVATE_filter, filter));

	node_iterator->root = root;
	node_iterator->what_to_show = what_to_show;
	node_iterator->filter = filter;
	node_iterator->entity_reference_expansion = entity_reference_expansion;
	node_iterator->reference = root;

	if (!(node_iterator->listener = OP_NEW(DOM_NodeIteratorMutationListener, (node_iterator))))
		return OpStatus::ERR_NO_MEMORY;

	environment->AddMutationListener(node_iterator->listener);

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_NodeIterator::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_root:
		DOMSetObject(value, root);
		break;

	case OP_ATOM_whatToShow:
		DOMSetNumber(value, what_to_show);
		break;

	case OP_ATOM_filter:
		DOMSetObject(value, filter);
		break;

	case OP_ATOM_expandEntityReferences:
		DOMSetBoolean(value, entity_reference_expansion);
		break;

	default:
		return GET_FAILED;
	}

	if (root)
		return GET_SUCCESS;
	else
		return DOM_GETNAME_DOMEXCEPTION(INVALID_STATE_ERR);
}

/* virtual */ ES_PutState
DOM_NodeIterator::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_root:
	case OP_ATOM_whatToShow:
	case OP_ATOM_filter:
	case OP_ATOM_expandEntityReferences:
		if (!root)
			return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
		else
			return PUT_READ_ONLY;

	default:
		return PUT_FAILED;
	}
}

/* virtual */ void
DOM_NodeIterator::GCTrace()
{
	DOM_TraversalObject::GCTrace();

	if (state) GetRuntime()->GCMark(state);
	if (reference) GetRuntime()->GCMark(reference);
	if (current_candidate) GetRuntime()->GCMark(current_candidate);
}

void
DOM_NodeIterator::Detach()
{
	root = NULL;

	if (listener)
	{
		GetEnvironment()->RemoveMutationListener(listener);
		OP_DELETE(listener);
		listener = NULL;
	}
}

OP_STATUS
DOM_NodeIterator::ElementRemoved(HTML_Element* element)
{
	HTML_Element* parent = reference->GetThisElement();
	if (!parent)
		parent = reference->GetPlaceholderElement();

	while (parent)
	{
		if (root && root->GetThisElement() == parent)
			break;
		if (element == parent)
		{
			// A change to a parent to the current node in the iterator
			DOM_Document* document = reference->GetOwnerDocument();
			RETURN_IF_ERROR(NextFrom(element, !forward, document, reference));

			if (!reference)
				RETURN_IF_ERROR(NextFrom(element, forward, document, reference));

			if (current_candidate)
				reference_node_changed_by_filter = TRUE;

			break;
		}

		parent = parent->Parent();
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_NodeIterator::NextFrom(HTML_Element* element, BOOL forward, DOM_Document* owner_document, DOM_Node *&node)
{
	HTML_Element *next_node = forward ? element->NextActual() : element->PrevActual();

	if (next_node)
		return GetEnvironment()->ConstructNode(node, next_node, owner_document);

	node = NULL;
	return OpStatus::OK;
}

/* static */ int
DOM_NodeIterator::detach(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(node_iterator, DOM_TYPE_NODEITERATOR, DOM_NodeIterator);
	node_iterator->Detach();
	return ES_FAILED;
}

/* static */ int
DOM_NodeIterator::move(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_NodeIterator *node_iterator;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(node_iterator, DOM_TYPE_NODEITERATOR, DOM_NodeIterator);

		/* Recursive call, probably from the filter's acceptNode function. */
		if (node_iterator->state)
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

		node_iterator->current_candidate = NULL;
	}
	else
		this_object = node_iterator = DOM_VALUE2OBJECT(*return_value, DOM_NodeIterator);

	if (!node_iterator->root)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	DOM_NodeIterator_State *state = node_iterator->state;
	node_iterator->state = NULL;

	DOM_Node *candidate = node_iterator->reference;
	int result = DOM_NodeFilter::FILTER_ACCEPT;

	DOM_Node *last_possible = NULL;

	OP_ASSERT(data == 0 || data == 1);
	BOOL forward = data == 1;

	if (forward)
	{
		DOM_Node *last_child = node_iterator->root;
		while (last_child)
		{
			last_possible = last_child;
			CALL_FAILED_IF_ERROR(last_child->GetLastChild(last_child));
		}
	}
	else
		last_possible = node_iterator->root;

	/* Yep, this is grotesque. */
	while (1)
	{
		if (node_iterator->current_candidate)
		{
			candidate = node_iterator->current_candidate;
			node_iterator->current_candidate = NULL;
		}
		else
			while (1)
			{
				if (!candidate || !last_possible || node_iterator->forward == forward && candidate == last_possible)
				{
					DOMSetNull(return_value);
					return ES_VALUE;
				}

				if (node_iterator->forward == forward || candidate != node_iterator->reference || result != DOM_NodeFilter::FILTER_ACCEPT)
				{
					OP_STATUS status;

					if (forward)
						status = candidate->GetNextNode(candidate);
					else
						status = candidate->GetPreviousNode(candidate);

					CALL_FAILED_IF_ERROR(status);
				}

				if (candidate && (!state || !state->HasTried(candidate)))
					break;
			}

		if (!node_iterator->DOM_TraversalObject::state)
			node_iterator->reference_node_changed_by_filter = FALSE;

		result = node_iterator->AcceptNode(candidate, origining_runtime, *return_value);

		if (result == DOM_NodeFilter::FILTER_ACCEPT)
		{
			node_iterator->reference_node_changed_by_filter = FALSE;
			DOMSetObject(return_value, candidate);
			node_iterator->forward = forward;

			// Don't move the reference element out of the tree
			BOOL candidate_is_in_root = candidate == node_iterator->root;
			if (!candidate_is_in_root)
			{
				// Different nodes so root has to have children and
				// candidate has to have parents
				HTML_Element* root_element = node_iterator->root->GetPlaceholderElement();
				HTML_Element* candidate_element = candidate->GetThisElement();
				OP_ASSERT(root_element);
				OP_ASSERT(candidate_element);
				candidate_is_in_root = root_element->IsAncestorOf(candidate_element);
			}
			if (candidate_is_in_root)
				node_iterator->reference = candidate;
			else
			{
				// Let the reference node stay where it is. This isn't actually controlled
				// by the spec, but is what the acid3 test suite accepts and it seems
				// to be a reasonable interpretation of the spec's intentions.
			}
			return ES_VALUE;
		}
		else if (result == DOM_NodeFilter::FILTER_FAILED)
			return ES_FAILED;
		else if (result == DOM_NodeFilter::FILTER_EXCEPTION)
			return ES_EXCEPTION;
		else if (result == DOM_NodeFilter::FILTER_OOM)
			return ES_NO_MEMORY;
		else if (result == DOM_NodeFilter::FILTER_DELAY)
		{
			if (!state)
				CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_NodeIterator_State, ()), node_iterator->GetRuntime()));

			if (OpStatus::IsMemoryError(state->AddTried(candidate)))
				return ES_NO_MEMORY;

			node_iterator->current_candidate = candidate;
			node_iterator->state = state;

			DOMSetObject(return_value, node_iterator);
			return ES_SUSPEND | ES_RESTART;
		}
		else if (node_iterator->reference_node_changed_by_filter)
		{
			// Reset iteration from the reference
			result = DOM_NodeFilter::FILTER_ACCEPT;
			candidate = node_iterator->reference;
		}
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_NodeIterator)
	DOM_FUNCTIONS_FUNCTION(DOM_NodeIterator, DOM_NodeIterator::detach, "detach", 0)
DOM_FUNCTIONS_END(DOM_NodeIterator)

DOM_FUNCTIONS_WITH_DATA_START(DOM_NodeIterator)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NodeIterator, DOM_NodeIterator::move, 1, "nextNode", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_NodeIterator, DOM_NodeIterator::move, 0, "previousNode", 0)
DOM_FUNCTIONS_WITH_DATA_END(DOM_NodeIterator)

#endif // DOM2_TRAVERSAL
