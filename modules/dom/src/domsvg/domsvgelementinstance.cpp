/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM
#include "modules/dom/src/domsvg/domsvgelementinstance.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/svg/SVGManager.h"


/* static */ OP_STATUS
DOM_SVGElementInstance::Make(DOM_SVGElementInstance *&element, HTML_Element *html_element, DOM_EnvironmentImpl *environment)
{
	OP_ASSERT(html_element);

	// We must keep the real node alive now since the shadow node is owned by DOM and can't be removed by SVG
	HTML_Element* real_elm = g_svg_manager->GetEventTarget(html_element);
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	element = OP_NEW(DOM_SVGElementInstance, ());
	if(!element)
		return OpStatus::ERR_NO_MEMORY;

	DOM_Object* object;
	OP_STATUS status = environment->ConstructNode(object, real_elm);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(element);
		return status;
	}

	element->the_real_node = object;

	RETURN_IF_ERROR(DOMSetObjectRuntime(element, runtime, runtime->GetPrototype(DOM_Runtime::SVGELEMENTINSTANCE_PROTOTYPE), "SVGElementInstance"));

	return OpStatus::OK;
}

/* virtual */ void
DOM_SVGElementInstance::GCTraceSpecial(BOOL via_tree)
{
	DOM_Element::GCTraceSpecial(via_tree);
	GCMark(the_real_node);
}

/* virtual */ ES_GetState
DOM_SVGElementInstance::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	// Note: We knowingly skip calling the DOM_Element baseclass because it will introduce unwanted sideeffects.
	//       We only want the event-handling parts of DOM_Element.
	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_SVGElementInstance::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
#if defined(SVG_TINY_12) || defined(SVG_FULL_11)
	case OP_ATOM_correspondingElement:
		DOMSetObject(value, the_real_node);
		return GET_SUCCESS;

	case OP_ATOM_correspondingUseElement:
		if (value)
		{
			HTML_Element* use_element = this_element->ParentActual(); // Skipping past all "inner" use tags.
			while(use_element && !use_element->IsMatchingType(Markup::SVGE_USE, NS_SVG))
			{
				use_element = use_element->Parent();
			}

			// If use_element is NULL, then this node has been removed from the tree and we return NULL.
			if (use_element)
			{
				DOM_Node *node;
				GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, use_element, owner_document));
				DOMSetObject(value, node);
			}
			else
			{
				DOMSetNull(value);
			}
		}
		return GET_SUCCESS;
#endif // SVG_TINY_12 || SVG_FULL_11
#ifdef SVG_FULL_11
	case OP_ATOM_parentNode:
		if (value)
		{
			HTML_Element* parent = this_element->Parent();
			if (!parent || parent->IsMatchingType(Markup::SVGE_USE, NS_SVG))
			{
				DOMSetNull(value);
			}
			else
			{
				DOM_Node *node;
				GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, parent, owner_document));
				OP_ASSERT(node->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE));
				DOMSetObject(value, node);
			}
		}
		return GET_SUCCESS;
	case OP_ATOM_childNodes:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_childNodes);
			if (result != GET_FAILED)
				return result;
			else
			{
				DOM_SVGElementInstanceList *collection;

				GET_FAILED_IF_ERROR(DOM_SVGElementInstanceList::Make(collection, this, GetEnvironment()));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_childNodes, *collection));

				DOMSetObject(value, collection);
			}
		}
		return GET_SUCCESS;
	case OP_ATOM_firstChild:
		if (value)
		{
			HTML_Element* child = this_element->FirstChild();
			if (!child)
			{
				DOMSetNull(value);
			}
			else
			{
				DOM_Node *node;
				GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, child, owner_document));
				OP_ASSERT(node->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE));
				DOMSetObject(value, node);
			}
		}
		return GET_SUCCESS;
	case OP_ATOM_lastChild:
		if (value)
		{
			HTML_Element* child = this_element->LastChild();
			if (!child)
			{
				DOMSetNull(value);
			}
			else
			{
				DOM_Node *node;
				GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, child, owner_document));
				OP_ASSERT(node->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE));
				DOMSetObject(value, node);
			}
		}
		return GET_SUCCESS;
	case OP_ATOM_previousSibling:
		if (value)
		{
			HTML_Element* sibling = this_element->Pred();
			if (!sibling)
			{
				DOMSetNull(value);
			}
			else
			{
				DOM_Node *node;
				GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, sibling, owner_document));
				OP_ASSERT(node->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE));
				DOMSetObject(value, node);
			}
		}
		return GET_SUCCESS;
	case OP_ATOM_nextSibling:
		if (value)
		{
			HTML_Element* sibling = this_element->Suc();
			if (!sibling)
			{
				DOMSetNull(value);
			}
			else
			{
				DOM_Node *node;
				GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, sibling, owner_document));
				OP_ASSERT(node->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE));
				DOMSetObject(value, node);
			}
		}
		return GET_SUCCESS;
#endif // SVG_FULL_11
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_SVGElementInstance::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	// Note: We knowingly skip calling the DOM_Element baseclass because it will introduce unwanted sideeffects.
	//       We only want the event-handling parts of DOM_Element.
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_SVGElementInstance::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
#if defined(SVG_TINY_12) || defined(SVG_FULL_11)
	case OP_ATOM_correspondingElement:
	case OP_ATOM_correspondingUseElement:
#endif // SVG_TINY_12 || SVG_FULL_11
#ifdef SVG_FULL_11
	case OP_ATOM_parentNode:
	case OP_ATOM_childNodes:
	case OP_ATOM_firstChild:
	case OP_ATOM_lastChild:
	case OP_ATOM_previousSibling:
	case OP_ATOM_nextSibling:
#endif // SVG_FULL_11
		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}

/* static */ int
DOM_SVGElementInstance::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(elm_instance, DOM_TYPE_SVG_ELEMENT_INSTANCE, DOM_SVGElementInstance);
	return DOM_Node::accessEventListener(elm_instance->the_real_node, argv, argc, return_value, origining_runtime, data);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_SVGElementInstance)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGElementInstance, DOM_Node::dispatchEvent, "dispatchEvent", 0)
DOM_FUNCTIONS_END(DOM_SVGElementInstance)

DOM_FUNCTIONS_WITH_DATA_START(DOM_SVGElementInstance)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SVGElementInstance, DOM_SVGElementInstance::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_SVGElementInstance, DOM_SVGElementInstance::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_SVGElementInstance)


#ifdef SVG_FULL_11
// ChildNodes below

/* virtual */
void DOM_SVGElementInstanceList::GCTrace()
{
	GCMark(m_dom_node);
}

/* static */
int DOM_SVGElementInstanceList::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_ELEMENT_INSTANCE_LIST, DOM_SVGElementInstanceList);
	DOM_CHECK_ARGUMENTS("n");

	ES_GetState result = list->GetNodeAtIndex(static_cast<int>(argv[0].value.number), return_value);
	if (result == GET_NO_MEMORY)
		return ES_NO_MEMORY;

	if (result == GET_FAILED)
		DOMSetNull(return_value);

	return ES_VALUE;
}

ES_GetState DOM_SVGElementInstanceList::GetNodeAtIndex(int index, ES_Value* value)
{
	// This is a naive implementation, but we don't expect this method to be used much anyway.
	HTML_Element* child = m_dom_node->GetThisElement()->FirstChild();
	while (child && index > 0)
	{
		child = child->Suc();
		index--;
	}

	if (child && index == 0)
	{
		if (value)
		{
			DOM_Object* object;
			GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(object, child));
			DOMSetObject(value, object);
		}
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_SVGElementInstanceList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return GetNodeAtIndex(property_index, value);
}

/* virtual */ ES_GetState
DOM_SVGElementInstanceList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = CalculateNodeCount();
	return GET_SUCCESS;
}

int
DOM_SVGElementInstanceList::CalculateNodeCount()
{
	// This is a naive implementation, but we don't expect this method to be used much anyway.
	int count = 0;
	HTML_Element* child = m_dom_node->GetThisElement()->FirstChild();
	while (child)
	{
		child = child->Suc();
		count++;
	}
	return count;
}

/* virtual */
ES_GetState DOM_SVGElementInstanceList::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime )
{
	if (property_name == OP_ATOM_length)
	{
		if (value)
			DOMSetNumber(value, CalculateNodeCount());

		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* static */ OP_STATUS
DOM_SVGElementInstanceList::Make(DOM_SVGElementInstanceList*& list, DOM_SVGElementInstance* list_parent, DOM_EnvironmentImpl *environment)
{
	OP_ASSERT(list_parent);

	DOM_Runtime *runtime = environment->GetDOMRuntime();
	list = OP_NEW(DOM_SVGElementInstanceList, (list_parent));
	if(!list)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(DOMSetObjectRuntime(list, runtime, runtime->GetPrototype(DOM_Runtime::SVGELEMENTINSTANCELIST_PROTOTYPE), "SVGElementInstanceList"));

	return OpStatus::OK;
}

DOM_FUNCTIONS_START(DOM_SVGElementInstanceList)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGElementInstanceList, DOM_SVGElementInstanceList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_SVGElementInstanceList)
#endif // SVG_FULL_11

#endif // SVG_DOM
