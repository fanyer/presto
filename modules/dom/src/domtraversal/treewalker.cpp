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

#include "modules/dom/src/domtraversal/treewalker.h"
#include "modules/dom/src/domtraversal/nodefilter.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"

class DOM_TreeWalker_State
	: public DOM_Object
{
private:
	class NodeItem
	{
	public:
		DOM_Node *node;
		NodeItem *next;
	};

	NodeItem *first_tried;
	NodeItem *first_rejected;
	unsigned private_name;

public:
	DOM_TreeWalker_State();
	virtual ~DOM_TreeWalker_State();

	OP_STATUS AddTried(DOM_Node *node);
	BOOL HasTried(DOM_Node *node);

	OP_STATUS AddRejected(DOM_Node *node);
	BOOL HasAnyRejected();
	BOOL HasRejected(DOM_Node *node);
};

DOM_TreeWalker_State::DOM_TreeWalker_State()
	: first_tried(NULL),
	  first_rejected(NULL),
	  private_name(0)
{
}

DOM_TreeWalker_State::~DOM_TreeWalker_State()
{
	NodeItem *item;

	item = first_tried;
	while (item)
	{
		NodeItem *next = item->next;
		OP_DELETE(item);
		item = next;
	}

	item = first_rejected;
	while (item)
	{
		NodeItem *next = item->next;
		OP_DELETE(item);
		item = next;
	}
}

OP_STATUS
DOM_TreeWalker_State::AddTried(DOM_Node *node)
{
	RETURN_IF_ERROR(PutPrivate(private_name++, *node));

	NodeItem *item = OP_NEW(NodeItem, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	item->node = node;
	item->next = first_tried;
	first_tried = item;

	return OpStatus::OK;
}

BOOL
DOM_TreeWalker_State::HasTried(DOM_Node *node)
{
	NodeItem *item = first_tried;
	while (item)
	{
		if (item->node == node)
			return TRUE;
		item = item->next;
	}
	return FALSE;
}

OP_STATUS
DOM_TreeWalker_State::AddRejected(DOM_Node *node)
{
	RETURN_IF_ERROR(PutPrivate(private_name++, *node));

	NodeItem *item = OP_NEW(NodeItem, ());
	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	item->node = node;
	item->next = first_rejected;
	first_rejected = item;

	return OpStatus::OK;
}

BOOL
DOM_TreeWalker_State::HasAnyRejected()
{
	return first_rejected != NULL;
}

BOOL
DOM_TreeWalker_State::HasRejected(DOM_Node *node)
{
	NodeItem *item = first_rejected;
	while (item)
	{
		if (item->node == node)
			return TRUE;
		item = item->next;
	}
	return FALSE;
}

DOM_TreeWalker::DOM_TreeWalker()
	: current_node(NULL),
	  current_candidate(NULL),
	  best_candidate(NULL),
	  state(NULL)
{
}

/* static */ OP_STATUS
DOM_TreeWalker::Make(DOM_TreeWalker *&tree_walker, DOM_EnvironmentImpl *environment, DOM_Node *root,
                     unsigned what_to_show, ES_Object *filter, BOOL entity_reference_expansion)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(tree_walker = OP_NEW(DOM_TreeWalker, ()), runtime, runtime->GetPrototype(DOM_Runtime::TREEWALKER_PROTOTYPE), "TreeWalker"));

	if (filter)
		RETURN_IF_ERROR(tree_walker->PutPrivate(DOM_PRIVATE_filter, filter));

	tree_walker->root = root;
	tree_walker->what_to_show = what_to_show;
	tree_walker->filter = filter;
	tree_walker->entity_reference_expansion = entity_reference_expansion;
	tree_walker->current_node = root;

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_TreeWalker::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_root:
		DOMSetObject(value, root);
		return GET_SUCCESS;

	case OP_ATOM_whatToShow:
		DOMSetNumber(value, what_to_show);
		return GET_SUCCESS;

	case OP_ATOM_filter:
		DOMSetObject(value, filter);
		return GET_SUCCESS;

	case OP_ATOM_expandEntityReferences:
		DOMSetBoolean(value, entity_reference_expansion);
		return GET_SUCCESS;

	case OP_ATOM_currentNode:
		DOMSetObject(value, current_node);
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_TreeWalker::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_root:
	case OP_ATOM_whatToShow:
	case OP_ATOM_filter:
	case OP_ATOM_expandEntityReferences:
		return PUT_READ_ONLY;

	case OP_ATOM_currentNode:
		if (state)
			return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
		else if (value->type == VALUE_OBJECT)
		{
			DOM_HOSTOBJECT_SAFE(new_node, value->value.object, DOM_TYPE_NODE, DOM_Node);

			if (new_node && new_node->GetEnvironment() == GetEnvironment())
			{
				current_node = new_node;
				return PUT_SUCCESS;
			}
		}
		return DOM_PUTNAME_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	default:
		return PUT_FAILED;
	}
}

/* virtual */ void
DOM_TreeWalker::GCTrace()
{
	DOM_TraversalObject::GCTrace();

	if (state) GetRuntime()->GCMark(state);
	if (current_node) GetRuntime()->GCMark(current_node);
	if (current_candidate) GetRuntime()->GCMark(current_candidate);
	if (best_candidate) GetRuntime()->GCMark(best_candidate);
}

/* static */ int
DOM_TreeWalker::move(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_TreeWalker *tree_walker;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(tree_walker, DOM_TYPE_TREEWALKER, DOM_TreeWalker);

		/* Recursive call, probably from the filter's acceptNode function. */
		if (tree_walker->state)
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

		tree_walker->current_candidate = NULL;
	}
	else
		this_object = tree_walker = DOM_VALUE2OBJECT(*return_value, DOM_TreeWalker);

	DOM_TreeWalker_State *state = tree_walker->state;
	tree_walker->state = NULL;

	DOM_Node *candidate = NULL;
	int result;

	DOM_Node *last_possible = NULL;

	switch (data)
	{
	case PARENT_NODE:
	case PREVIOUS_NODE:
		last_possible = tree_walker->root;
		break;

	case FIRST_CHILD:
		CALL_FAILED_IF_ERROR(tree_walker->current_node->GetLastChild(last_possible));
		if (last_possible)
			while (1)
			{
				DOM_Node *last_child;
				CALL_FAILED_IF_ERROR(last_possible->GetLastChild(last_child));
				if (!last_child)
					break;
				else
					last_possible = last_child;
			}
		break;

	case LAST_CHILD:
		CALL_FAILED_IF_ERROR(tree_walker->current_node->GetFirstChild(last_possible));
		break;

	case PREVIOUS_SIBLING:
		if (tree_walker->current_node != tree_walker->root)
		{
			DOM_Node *parent = tree_walker->current_node;
			while (parent && parent != tree_walker->root)
			{
				DOM_Node *previous_sibling;
				CALL_FAILED_IF_ERROR(parent->GetPreviousSibling(previous_sibling));
				if (previous_sibling)
					last_possible = previous_sibling;
				CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
			}

			DOM_Node *previous_sibling = last_possible;
			while (previous_sibling)
			{
				last_possible = previous_sibling;
				CALL_FAILED_IF_ERROR(last_possible->GetPreviousSibling(previous_sibling));
			}
		}
		break;

	case NEXT_SIBLING:
		if (tree_walker->current_node != tree_walker->root)
		{
			DOM_Node *parent = tree_walker->current_node, *next_sibling;
			while (parent && parent != tree_walker->root)
			{
				CALL_FAILED_IF_ERROR(parent->GetNextSibling(next_sibling));
				if (next_sibling)
					last_possible = next_sibling;
				CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
			}

			next_sibling = last_possible;
			while (next_sibling)
			{
				last_possible = next_sibling;
				CALL_FAILED_IF_ERROR(last_possible->GetNextSibling(next_sibling));
			}

			next_sibling = last_possible;
			while (next_sibling)
			{
				last_possible = next_sibling;
				CALL_FAILED_IF_ERROR(last_possible->GetLastChild(next_sibling));
			}
		}
		break;

	case NEXT_NODE:
		{
			DOM_Node *last_child = tree_walker->root;
			last_possible = last_child;
			while (last_child)
			{
				CALL_FAILED_IF_ERROR(last_child->GetLastChild(last_child));
				if (last_child)
					last_possible = last_child;
			}
		}
	}

	if (last_possible == tree_walker->current_node)
		last_possible = NULL;

	BOOL reset_candidate = TRUE;

	/* Yep, this is grotesque. */
	while (1)
	{
		if (tree_walker->current_candidate)
		{
			candidate = tree_walker->current_candidate;
			tree_walker->current_candidate = NULL;
			reset_candidate = TRUE;
		}
		else
		{
			if (reset_candidate)
			{
				candidate = tree_walker->current_node;
				reset_candidate = FALSE;
			}

			while (1)
			{
				// Check if we've reached the end of the line in our search
				if (!candidate || candidate == last_possible || !last_possible)
				{
					if (tree_walker->best_candidate)
					{
						DOMSetObject(return_value, tree_walker->current_node = tree_walker->best_candidate);
						tree_walker->best_candidate = NULL;
					}
					else
						DOMSetNull(return_value);
					return ES_VALUE;
				}

				OP_STATUS status = OpStatus::OK;
				OpStatus::Ignore(status);

				switch (data)
				{
				case PARENT_NODE:
					status = candidate->GetParentNode(candidate);
					break;

				case FIRST_CHILD:
					status = candidate->GetNextNode(candidate, state && state->HasRejected(candidate));
					break;

				case LAST_CHILD:
					{
						DOM_Node *temp = NULL;

						if (!state || !state->HasRejected(candidate))
							CALL_FAILED_IF_ERROR(candidate->GetLastChild(temp));

						while (candidate && !temp)
						{
							CALL_FAILED_IF_ERROR(candidate->GetPreviousSibling(temp));

							if (!temp)
							{
								CALL_FAILED_IF_ERROR(candidate->GetParentNode(temp));

								if (temp)
								{
									candidate = temp;
									CALL_FAILED_IF_ERROR(candidate->GetPreviousSibling(temp));
								}
							}
						}

						candidate = temp;
					}
					break;

				case PREVIOUS_SIBLING:
				case PREVIOUS_NODE:
					{
						DOM_Node *temp;
						BOOL consider_children = candidate != tree_walker->current_node && (!state || !state->HasRejected(candidate));

						if (consider_children)
						{
							DOM_Node *parent = tree_walker->current_node;
							while (parent)
							{
								CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
								if (parent == candidate)
								{
									consider_children = FALSE;
									break;
								}
							}
						}

						if (consider_children)
							CALL_FAILED_IF_ERROR(candidate->GetLastChild(temp));
						else
							CALL_FAILED_IF_ERROR(candidate->GetPreviousSibling(temp));

						if ((!consider_children || !temp) && tree_walker->best_candidate == candidate)
						{
							DOMSetObject(return_value, tree_walker->current_node = candidate);
							tree_walker->best_candidate = NULL;
							return ES_VALUE;
						}

						while (candidate && !temp)
						{
							CALL_FAILED_IF_ERROR(candidate->GetPreviousSibling(temp));

							if (!temp)
							{
								CALL_FAILED_IF_ERROR(candidate->GetParentNode(temp));

								candidate = temp;

								if (temp)
								{
									DOM_Node *parent = tree_walker->current_node;
									while (parent)
									{
										CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
										if (parent == temp)
										{
											temp = NULL;
											break;
										}
									}

									if (temp)
										CALL_FAILED_IF_ERROR(candidate->GetPreviousSibling(temp));
									else
									{
										temp = candidate;
										break;
									}
								}
							}
						}

						candidate = temp;
					}
					break;

				case NEXT_SIBLING:
					{
						DOM_Node *temp;
						BOOL consider_children = candidate != tree_walker->current_node && (!state || !state->HasRejected(candidate));

						if (consider_children)
						{
							DOM_Node *parent = tree_walker->current_node;
							while (parent)
							{
								CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
								if (parent == candidate)
								{
									consider_children = FALSE;
									break;
								}
							}
						}

						if (consider_children)
							CALL_FAILED_IF_ERROR(candidate->GetFirstChild(temp));
						else
							CALL_FAILED_IF_ERROR(candidate->GetNextSibling(temp));

						while (candidate && !temp)
						{
							CALL_FAILED_IF_ERROR(candidate->GetNextSibling(temp));

							if (!temp)
							{
								CALL_FAILED_IF_ERROR(candidate->GetParentNode(temp));

								candidate = temp;

								if (temp)
								{
									DOM_Node *parent = tree_walker->current_node;
									while (parent)
									{
										CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
										if (parent == temp)
										{
											temp = NULL;
											break;
										}
									}

									if (temp)
										CALL_FAILED_IF_ERROR(candidate->GetNextSibling(temp));
									else
									{
										temp = candidate;
										break;
									}
								}
							}
						}

						candidate = temp;
					}
					break;

				case NEXT_NODE:
					status = candidate->GetNextNode(candidate, state && state->HasRejected(candidate));
				}

				CALL_FAILED_IF_ERROR(status);

				if (candidate && (!state || !state->HasTried(candidate)))
					break;
			}
		}

		result = tree_walker->AcceptNode(candidate, origining_runtime, *return_value);

		if (result == DOM_NodeFilter::FILTER_ACCEPT)
		{
			if (data == PREVIOUS_SIBLING || data == NEXT_SIBLING)
			{
				DOM_Node *parent = tree_walker->current_node;
				while (parent)
				{
					CALL_FAILED_IF_ERROR(parent->GetParentNode(parent));
					if (parent == candidate)
					{
						DOMSetNull(return_value);
						return ES_VALUE;
					}
				}
			}
			if (data == PREVIOUS_NODE)
				tree_walker->best_candidate = candidate;
			else
			{
				DOMSetObject(return_value, tree_walker->current_node = candidate);
				return ES_VALUE;
			}
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
				CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_TreeWalker_State, ()), tree_walker->GetRuntime()));

			if (OpStatus::IsMemoryError(state->AddTried(candidate)))
				return ES_NO_MEMORY;

			tree_walker->current_candidate = candidate;
			tree_walker->state = state;

			DOMSetObject(return_value, tree_walker);
			return ES_SUSPEND | ES_RESTART;
		}
		else if (result == DOM_NodeFilter::FILTER_REJECT)
		{
			OP_ASSERT(state);
			CALL_FAILED_IF_ERROR(state->AddRejected(candidate));
		}
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_TreeWalker)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TreeWalker, DOM_TreeWalker::move, DOM_TreeWalker::PARENT_NODE, "parentNode", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TreeWalker, DOM_TreeWalker::move, DOM_TreeWalker::FIRST_CHILD, "firstChild", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TreeWalker, DOM_TreeWalker::move, DOM_TreeWalker::LAST_CHILD, "lastChild", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TreeWalker, DOM_TreeWalker::move, DOM_TreeWalker::PREVIOUS_SIBLING, "previousSibling", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TreeWalker, DOM_TreeWalker::move, DOM_TreeWalker::NEXT_SIBLING, "nextSibling", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TreeWalker, DOM_TreeWalker::move, DOM_TreeWalker::PREVIOUS_NODE, "previousNode", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_TreeWalker, DOM_TreeWalker::move, DOM_TreeWalker::NEXT_NODE, "nextNode", 0)
DOM_FUNCTIONS_WITH_DATA_END(DOM_TreeWalker)

#endif // DOM2_TRAVERSAL
