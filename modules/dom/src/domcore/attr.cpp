/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domhtml/htmlcoll.h"

#include "modules/util/str.h"
#include "modules/util/tempbuf.h"
#include "modules/logdoc/htm_elm.h"

class DOM_AttrMutationListener
	: public DOM_MutationListener
{
private:
	DOM_Attr *attr;

public:
	DOM_AttrMutationListener(DOM_Attr *attr)
		: attr(attr)
	{
	}

	virtual OP_STATUS OnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime)
	{
		if (attr->GetPlaceholderElement()->IsAncestorOf(child))
			return attr->UpdateValueFromValueTree(NULL, origining_runtime);
		else
			return OpStatus::OK;
	}
	virtual OP_STATUS OnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime)
	{
		if (attr->GetPlaceholderElement()->IsAncestorOf(child))
			return attr->UpdateValueFromValueTree(child, origining_runtime);
		else
			return OpStatus::OK;
	}
	virtual OP_STATUS OnAfterValueModified(HTML_Element *element, const uni_char *new_value, ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime)
	{
		if (attr->GetPlaceholderElement()->IsAncestorOf(element))
			return attr->UpdateValueFromValueTree(NULL, origining_runtime);
		else
			return OpStatus::OK;
	}
	virtual OP_STATUS OnAttrModified(HTML_Element *element, const uni_char *name, int ns_idx, DOM_Runtime *origining_runtime)
	{
		if (attr->GetOwnerElement() && attr->GetOwnerElement()->GetThisElement() == element)
		{
			const uni_char *uri = ns_idx > 0 ? g_ns_manager->GetElementAt(ns_idx)->GetUri() : NULL;
			if (uri && !*uri)
				uri = NULL;
			if (attr->HasName(name, uri))
				return attr->UpdateValueTreeFromValue(origining_runtime);
		}
		return OpStatus::OK;
	}
};

DOM_Attr::DOM_Attr(uni_char *name)
	: DOM_Node(ATTRIBUTE_NODE),
	  owner_element(NULL),
	  name(name),
	  value(NULL),
	  ns_uri(NULL),
	  ns_prefix(NULL),
	  dom1node(FALSE),
	  is_case_sensitive(FALSE),
	  next_attr(NULL),
	  value_root(NULL),
	  listener(NULL),
	  is_syncing_tree_and_value(FALSE),
	  valid_childnode_collection(FALSE)
{
}

OP_STATUS
DOM_Attr::BuildValueTree(DOM_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	const uni_char *value = GetValue();

	if (!value_root)
	{
		listener = OP_NEW(DOM_AttrMutationListener, (this));
		if (!listener || OpStatus::IsMemoryError(HTML_Element::DOMCreateNullElement(value_root, environment)))
		{
			OP_DELETE(listener);
			listener = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		value_root->SetESElement(this);
		environment->AddMutationListener(listener);

		/* This node is now significant in the value tree, meaning that if a
		   child node is referenced, then this node needs to be traced via that
		   tree, so that this node and its owner element are kept alive as long
		   as one of the child nodes are referenced. */
		SetIsSignificant();
	}

	if (value && *value && !value_root->FirstChild())
	{
		HTML_Element *text;

		RETURN_IF_ERROR(HTML_Element::DOMCreateTextNode(text, environment, value, FALSE, FALSE));

		DOM_EnvironmentImpl::CurrentState state(environment, origining_runtime);
		if (OpStatus::IsMemoryError(value_root->DOMInsertChild(environment, text, NULL)))
		{
			HTML_Element::DOMFreeElement(text, environment);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS
DOM_Attr::Make(DOM_Attr *&attr, DOM_Node *reference, const uni_char *name, const uni_char *ns_uri, const uni_char *ns_prefix, BOOL dom1node)
{
	attr = NULL;

	uni_char *name_copy = UniSetNewStr(name);
	if (!name_copy)
		return OpStatus::ERR_NO_MEMORY;

	attr = OP_NEW(DOM_Attr, (name_copy));
	if (!attr)
	{
		OP_DELETEA(name_copy);
		return OpStatus::ERR_NO_MEMORY;
	}

	DOM_Runtime *runtime = reference->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(attr, runtime, runtime->GetPrototype(DOM_Runtime::ATTR_PROTOTYPE), "Attr"));

	if (ns_uri && *ns_uri)
		RETURN_IF_ERROR(UniSetStr(attr->ns_uri, ns_uri));

	if (ns_prefix && *ns_prefix)
		RETURN_IF_ERROR(UniSetStr(attr->ns_prefix, ns_prefix));

	attr->dom1node = dom1node;
	attr->is_case_sensitive = reference->GetOwnerDocument()->IsXML();
	attr->SetOwnerDocument(reference->GetOwnerDocument());
	return OpStatus::OK;
}

/* virtual */
DOM_Attr::~DOM_Attr()
{
	OP_DELETEA(name);
	OP_DELETEA(value);
	OP_DELETEA(ns_uri);
	OP_DELETEA(ns_prefix);

	if (value_root)
		FreeElement(value_root);

	if (listener)
	{
		GetEnvironment()->RemoveMutationListener(listener);
		OP_DELETE(listener);
	}
}

void
DOM_Attr::SetOwnerElement(DOM_Element *element)
{
	owner_element = element;
	is_case_sensitive = is_case_sensitive || element->HasCaseSensitiveNames();
	element->SetIsSignificant();
}

BOOL
DOM_Attr::HasName(const uni_char *name_, const uni_char *ns_uri_)
{
	if ((is_case_sensitive ? uni_strcmp(name, name_) : uni_stricmp(name, name_)) == 0)
		if (ns_uri == ns_uri_ || ns_uri && ns_uri_ && uni_strcmp(ns_uri, ns_uri_) == 0)
			return TRUE;

	return FALSE;
}

OP_STATUS
DOM_Attr::CopyValueIn()
{
	if (owner_element)
	{
		const uni_char *tmp_value = GetValue();
		if (!tmp_value)
			tmp_value = UNI_L("");

		RETURN_IF_ERROR(UniSetStr(value, tmp_value));

		if (!ns_uri)
		{
			const uni_char *tmp_ns_uri, *tmp_ns_prefix;

			int ns_idx = owner_element->GetThisElement()->DOMGetAttributeNamespaceIndex(GetEnvironment(), name);

			if (ns_idx != NS_IDX_NOT_FOUND && ns_idx != NS_IDX_DEFAULT && owner_element->GetThisElement()->DOMGetNamespaceData(GetEnvironment(), ns_idx, tmp_ns_uri, tmp_ns_prefix))
			{
				RETURN_IF_ERROR(UniSetStr(ns_uri, tmp_ns_uri ? tmp_ns_uri : UNI_L("")));
				RETURN_IF_ERROR(UniSetStr(ns_prefix, tmp_ns_prefix ? tmp_ns_prefix : UNI_L("")));
			}
		}
	}

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_Attr::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_name:
	case OP_ATOM_nodeName:
		if (value)
			if (ns_prefix)
			{
				TempBuffer *buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(GetName(buffer));
				DOMSetString(value, buffer);
			}
			else
				DOMSetString(value, name);
		return GET_SUCCESS;

	case OP_ATOM_specified:
		int dummy_index;
		DOMSetBoolean(value, !owner_element || owner_element->GetThisElement()->DOMHasAttribute(GetEnvironment(), ATTR_XML, name, GetNsIndex(), is_case_sensitive, dummy_index, TRUE));
		return GET_SUCCESS;

	case OP_ATOM_nodeValue:
	case OP_ATOM_value:
	case OP_ATOM_textContent:
	case OP_ATOM_text:
		DOMSetString(value, GetValue());
		return GET_SUCCESS;

	case OP_ATOM_ownerElement:
		DOMSetObject(value, owner_element);
		return GET_SUCCESS;

	case OP_ATOM_namespaceURI:
		if (!dom1node && ns_uri && *ns_uri)
			DOMSetString(value, ns_uri);
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_prefix:
		if (!dom1node && ns_prefix && *ns_prefix)
			DOMSetString(value, ns_prefix);
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_localName:
		if (!dom1node)
			DOMSetString(value, name);
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_firstChild:
	case OP_ATOM_lastChild:
	case OP_ATOM_childNodes:
		if (value)
		{
			GET_FAILED_IF_ERROR(CreateValueTree(static_cast<DOM_Runtime *>(origining_runtime)));

			switch (property_name)
			{
			case OP_ATOM_firstChild:
				return DOMSetElement(value, value_root->FirstChildActual());

			case OP_ATOM_lastChild:
				return DOMSetElement(value, value_root->LastChildActual());

			default:
				OP_ASSERT(property_name == OP_ATOM_childNodes);
				if (valid_childnode_collection)
					return DOMSetPrivate(value, DOM_PRIVATE_childNodes);

				DOM_SimpleCollectionFilter filter(CHILDNODES);
				DOM_Collection *collection;

				GET_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, GetEnvironment(), this, FALSE, FALSE, filter));

				collection->SetCreateSubcollections();

				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_childNodes, *collection));
				valid_childnode_collection = TRUE;

				DOMSetObject(value, collection);
				return GET_SUCCESS;
			}
		}
		else
			return GET_SUCCESS;
	}

	return DOM_Node::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_Attr::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeValue:
	case OP_ATOM_value:
	case OP_ATOM_textContent:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		PUT_FAILED_IF_ERROR(SetValue(value->value.string, static_cast<DOM_Runtime *>(origining_runtime)));
		return PUT_SUCCESS;

	case OP_ATOM_name:
	case OP_ATOM_specified:
	case OP_ATOM_ownerElement:
		return PUT_READ_ONLY;
	}

	return DOM_Node::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_Attr::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);

	/* If 'via_tree' is TRUE, we're traced because a tree of elements with a
	   reference to us was traced.  If 'value_root' is NULL, that means for sure
	   that we're traced via our owner element, and we don't need to mark it.
	   If 'value_root' isn't NULL, we might have been traced via either the tree
	   of our owner element, or via the value tree, so we need to trace both our
	   owner element and our value tree either way. */
	if (!via_tree || value_root)
		GCMark(owner_element);

	if (value_root)
		if (HTML_Element *element = value_root->FirstChild())
			/* This loop is copied from DOM_Node::TraceElementTree().  We can't
			   use that function because it would inevitably cause an infinite
			   recursion by calling this function right back, since this node is
			   returned by 'value_root->GetESElement()'. */
			do
				if (DOM_Node *node = static_cast<DOM_Node *>(element->GetESElement()))
					DOM_Node::GCMarkAndTrace(GetRuntime(), node, TRUE);
			while ((element = static_cast<HTML_Element *>(element->Next())) != NULL);
}

OP_STATUS
DOM_Attr::Clone(DOM_Attr *&clone)
{
	return Import(clone, owner_document);
}

OP_STATUS
DOM_Attr::GetName(TempBuffer *buffer)
{
	if (ns_prefix)
	{
		GET_FAILED_IF_ERROR(buffer->Expand(uni_strlen(ns_prefix) + uni_strlen(name) + 2));

		OpStatus::Ignore(buffer->Append(ns_prefix));
		OpStatus::Ignore(buffer->Append(':'));
		OpStatus::Ignore(buffer->Append(name));
	}
	else
		return buffer->Append(name);

	return OpStatus::OK;
}

OP_STATUS
DOM_Attr::Import(DOM_Attr *&imported, DOM_Document *document)
{
	RETURN_IF_ERROR(Make(imported, document, name, ns_uri, ns_prefix, dom1node));
	OP_STATUS status;

	if (owner_element)
	{
		imported->SetOwnerElement(owner_element);
		status = imported->CopyValueIn();
		imported->owner_element = NULL;
	}
	else
		status = UniSetStr(imported->value, value);

	return status;
}

OP_STATUS
DOM_Attr::CreateValueTree(DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	if (!is_syncing_tree_and_value)
	{
		is_syncing_tree_and_value = TRUE;

		status = BuildValueTree(origining_runtime);

		is_syncing_tree_and_value = FALSE;
	}

	return status;
}

OP_STATUS
DOM_Attr::UpdateValueTreeFromValue(DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	if (!is_syncing_tree_and_value)
	{
		is_syncing_tree_and_value = TRUE;

		if (value_root)
		{
			if (OpStatus::IsMemoryError(value_root->DOMRemoveAllChildren(GetEnvironment())) ||
				OpStatus::IsMemoryError(BuildValueTree(origining_runtime)))
				status = OpStatus::ERR_NO_MEMORY;
		}

		is_syncing_tree_and_value = FALSE;
	}

	return status;
}

OP_STATUS
DOM_Attr::UpdateValueFromValueTree(HTML_Element *exclude, DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	if (!is_syncing_tree_and_value)
	{
		is_syncing_tree_and_value = TRUE;

		TempBuffer buffer;

		for (HTML_Element *iter = value_root->FirstChild(); iter; iter = iter->NextSiblingActual())
		{
			if (iter != exclude && iter->IsText())
				if (OpStatus::IsMemoryError(iter->DOMGetContents(GetEnvironment(), &buffer)))
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
		}

		if (OpStatus::IsSuccess(status))
		{
			const uni_char *new_value = buffer.GetStorage();
			if (!new_value)
				new_value = UNI_L("");

			const uni_char *old_value = GetValue();
			if (!old_value)
				old_value = UNI_L("");

			if (!uni_str_eq(new_value, old_value))
				if (OpStatus::IsMemoryError(SetValue(new_value, origining_runtime)))
					status = OpStatus::ERR_NO_MEMORY;
		}

		is_syncing_tree_and_value = FALSE;
	}

	return status;
}

void
DOM_Attr::AddToElement(DOM_Element *element, BOOL case_sensitive)
{
	OP_ASSERT(!owner_element);

	next_attr = element->GetFirstAttributeNode();
	element->SetFirstAttributeNode(this);

	is_case_sensitive = case_sensitive;
	SetOwnerElement(element);

	OP_DELETEA(value);
	value = NULL;
}

OP_STATUS
DOM_Attr::RemoveFromElement()
{
	OP_ASSERT(owner_element);

	OP_STATUS status = CopyValueIn();

	if (owner_element->GetFirstAttributeNode() == this)
		owner_element->SetFirstAttributeNode(next_attr);
	else
	{
		DOM_Attr *previous = owner_element->GetFirstAttributeNode();
		while (previous->next_attr != this)
			previous = previous->next_attr;
		previous->next_attr = next_attr;
	}

	owner_element = NULL;
	next_attr = NULL;

	return status;
}

int
DOM_Attr::GetNsIndex()
{
	if (ns_uri)
	{
		/* Calling this function on ownerless attribute nodes is invalid.  Fix! */
		OP_ASSERT(owner_element);

		return owner_element->GetThisElement()->DOMGetNamespaceIndex(GetEnvironment(), ns_uri, ns_prefix);
	}
	else
		return NS_IDX_ANY_NAMESPACE;
}

const uni_char *
DOM_Attr::GetValue()
{
	if (owner_element)
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();

		DOM_EnvironmentImpl::CurrentState state(environment);
		state.SetTempBuffer();

		return owner_element->GetThisElement()->DOMGetAttribute(environment, ATTR_XML, name, GetNsIndex(), is_case_sensitive, FALSE);
	}
	else
		return value;
}

OP_STATUS
DOM_Attr::SetValue(const uni_char *new_value, DOM_Runtime *origining_runtime)
{
	if (owner_element)
		return owner_element->SetAttribute(ATTR_XML, name, GetNsIndex(), new_value, uni_strlen(new_value), is_case_sensitive, origining_runtime);
	else
	{
		RETURN_IF_ERROR(UniSetStr(value, new_value));
		return UpdateValueTreeFromValue(origining_runtime);
	}
}

void
DOM_Attr::ClearPlaceholderElement()
{
	value_root = NULL;

	if (listener)
	{
		GetEnvironment()->RemoveMutationListener(listener);
		OP_DELETE(listener);
		listener = NULL;
	}

	// This collection is based on value_root and it is not able to keep working
	// as is. Remove it so that we create a new collection next time someone
	// needs it.
	// FIXME: Fix the collection so that it keeps working.
	valid_childnode_collection = FALSE;
}

HTML_Element *
DOM_Attr::GetPlaceholderElement()
{
	// We create it on demand. No way to signal oom though which is a problem but
	// it's used in methods like appendChild without really passing this class.
	OpStatus::Ignore(CreateValueTree(NULL));

	return value_root;
}


