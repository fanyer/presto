/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/probetools/probepoints.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domcore/entity.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"
#include "modules/dom/src/domsvg/domsvgelementinstance.h"
#include "modules/dom/src/domselectors/domselector.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/util/tempbuf.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/xmlnames.h"

/* Used for delaying reading of properties whose values might depend
   on style information that the layout engine is still waiting for. */
DOM_DelayedLayoutListener::DOM_DelayedLayoutListener(FramesDocument *frames_doc, ES_Thread* thread)
		: frames_doc(frames_doc), thread(thread)
{
	frames_doc->AddDelayedLayoutListener(this);
	thread->AddListener(this);
	thread->Block();
}

/* virtual */ void
DOM_DelayedLayoutListener::LayoutContinued()
{
	if (thread)
		OpStatus::Ignore(thread->Unblock());
	if (frames_doc)
	{
		frames_doc->RemoveDelayedLayoutListener(this);
		frames_doc = NULL;
	}
}

/* virtual */ OP_STATUS
DOM_DelayedLayoutListener::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED)
	{
		thread = NULL;
		LayoutContinued();
	}
	return OpStatus::OK;
}

OP_STATUS
DOM_AttrMapImpl::Start(DOM_Attr *&attr, int &ns_idx, const uni_char *ns_uri, const uni_char *name, BOOL case_sensitive)
{
	if (ns_uri)
		ns_idx = element->GetThisElement()->DOMGetNamespaceIndex(element->GetEnvironment(), ns_uri);
	else
		ns_idx = NS_IDX_ANY_NAMESPACE;

	return element->GetAttributeNode(attr, ATTR_XML, name, ns_idx, TRUE, FALSE, case_sensitive);
}

/* virtual */ int
DOM_AttrMapImpl::Length()
{
	return element->GetThisElement()->DOMGetAttributeCount();
}

/* virtual */ int
DOM_AttrMapImpl::Item(int index, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_Object::DOMSetNull(return_value);

	if (index < 0 || index >= Length())
		return ES_VALUE;

	const uni_char *name;
	int ns_idx;

	TempBuffer buffer;
	element->GetThisElement()->DOMGetAttributeName(element->GetEnvironment(), index, &buffer, name, ns_idx);

	if (!name)
		return ES_VALUE;

	DOM_Attr *attr;
	CALL_FAILED_IF_ERROR(element->GetAttributeNode(attr, ATTR_XML, name, ns_idx, TRUE, FALSE, TRUE));

	DOM_Object::DOMSetObject(return_value, attr);
	return ES_VALUE;
}

/* virtual */ int
DOM_AttrMapImpl::GetNamedItem(const uni_char *ns_uri, const uni_char *name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_Attr *attr;
	int ns_idx;

	if(element->GetOwnerDocument()->IsXML())
	{
		XMLCompleteNameN completename(name, uni_strlen(name));
		if (!ns_uri && completename.GetPrefixLength())
		{
			if (XMLNamespaceDeclaration::ResolveNameInScope(element->GetThisElement(), completename))
			{
				ns_idx = completename.GetNsIndex();
				ns_uri = completename.GetUri();
				name = completename.GetLocalPart();
			}
		}
	}

	CALL_FAILED_IF_ERROR(Start(attr, ns_idx, ns_uri, name, ns || element->HasCaseSensitiveNames()));

	DOM_Object::DOMSetObject(return_value, attr);
	return ES_VALUE;
}

/* virtual */ int
DOM_AttrMapImpl::RemoveNamedItem(const uni_char *ns_uri, const uni_char *name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_Attr *attr;
	int ns_idx;

	if(element->GetOwnerDocument()->IsXML())
	{
		XMLCompleteNameN completename(name, uni_strlen(name));
		if (!ns_uri && completename.GetPrefixLength())
		{
			if (XMLNamespaceDeclaration::ResolveNameInScope(element->GetThisElement(), completename))
			{
				ns_idx = completename.GetNsIndex();
				ns_uri = completename.GetUri();
				name = completename.GetLocalPart();
			}
		}
	}

	BOOL case_sensitive = ns || element->HasCaseSensitiveNames();
	CALL_FAILED_IF_ERROR(Start(attr, ns_idx, ns_uri, name, case_sensitive));

	if (!attr)
		return element->CallDOMException(DOM_Object::NOT_FOUND_ERR, return_value);

	CALL_FAILED_IF_ERROR(element->SetAttribute(ATTR_XML, name, ns_idx, NULL, 0, case_sensitive, origining_runtime));
	DOM_Object::DOMSetObject(return_value, attr);
	return ES_VALUE;
}

/* virtual */ int
DOM_AttrMapImpl::SetNamedItem(DOM_Node *node, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	if (node->GetNodeType() != ATTRIBUTE_NODE)
		return node->CallDOMException(DOM_Object::NOT_FOUND_ERR, return_value);

	ES_Value arguments[1];
	DOM_Object::DOMSetObject(&arguments[0], node);

	return DOM_Element::setAttributeNode(element, arguments, 1, return_value, origining_runtime, ns ? 1 : 0);
}

DOM_Element::DOM_Element()
	: DOM_Node(ELEMENT_NODE),
	  this_element(NULL),
	  first_attr(NULL)
#ifdef DOM3_XPATH
	  ,first_ns(NULL)
#endif // DOM3_XPATH
{
}

ES_GetState
DOM_Element::GetCollection(ES_Value *value, DOM_HTMLElement_Group group, const char *class_name, int private_name)
{
	if (value)
	{
		ES_GetState result = DOMSetPrivate(value, private_name);

		if (result != GET_FAILED)
			return result;

		DOM_SimpleCollectionFilter filter(group);
		DOM_Collection *collection;

		/* Note: this collection does not attempt sharing of node collection; the sharing is handled via the private property instead. */
		GET_FAILED_IF_ERROR(DOM_Collection::Make(collection, GetEnvironment(), class_name, this, FALSE, FALSE, filter));

		collection->SetCreateSubcollections();

		GET_FAILED_IF_ERROR(PutPrivate(private_name, *collection));

		DOMSetObject(value, collection);
	}

	return GET_SUCCESS;
}

/* virtual */
DOM_Element::~DOM_Element()
{
	if (this_element)
		FreeElement(this_element);
}

/* virtual */ ES_GetState
DOM_Element::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
	if (result != GET_FAILED)
		return result;

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_Element::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeName:
	case OP_ATOM_tagName:
		if (value)
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			DOM_Document *dom_doc = GetOwnerDocument();
			const uni_char *localpart = GetTagName(buffer, !dom_doc || !dom_doc->IsXML(), TRUE);
			if (!localpart)
				return GET_NO_MEMORY;

			DOMSetString(value, localpart);

			/* Cache the value on the native object. */
			const uni_char *property_name_string = property_name == OP_ATOM_nodeName ? UNI_L("nodeName") : UNI_L("tagName");
			if (!GetRuntime()->IsSpecialProperty(*this, property_name_string))
				GET_FAILED_IF_ERROR(GetRuntime()->PutName(*this, property_name_string, *value, PROP_READ_ONLY | PROP_HOST_PUT));
		}
		return GET_SUCCESS;

	case OP_ATOM_childElementCount:
		if (value)
		{
			unsigned count = 0;

			for (HTML_Element *child = this_element->FirstChildActual(); child; child = child->SucActual())
				if (Markup::IsRealElement(child->Type()))
					++count;

			DOMSetNumber(value, count);
		}
		return GET_SUCCESS;

	case OP_ATOM_firstElementChild:
		{
			DOM_Node *node;
			GET_FAILED_IF_ERROR(GetFirstChild(node));
			while (node)
			{
				if(ELEMENT_NODE == node->GetNodeType())
				{
					break;
				}

				GET_FAILED_IF_ERROR(node->GetNextSibling(node));
			}

			DOMSetObject(value, node);
			return GET_SUCCESS;
		}
		break;

	case OP_ATOM_lastElementChild:
		{
			DOM_Node *node;
			GET_FAILED_IF_ERROR(GetLastChild(node));
			while (node)
			{
				if(ELEMENT_NODE == node->GetNodeType())
				{
					break;
				}

				GET_FAILED_IF_ERROR(node->GetPreviousSibling(node));
			}

			DOMSetObject(value, node);
			return GET_SUCCESS;
		}
		break;

	case OP_ATOM_nextElementSibling:
		{
			DOM_Node *node;
			GET_FAILED_IF_ERROR(GetNextSibling(node));
			while(node)
			{
				if(node && ELEMENT_NODE == node->GetNodeType())
				{
					break;
				}

				GET_FAILED_IF_ERROR(node->GetNextSibling(node));
			}

			DOMSetObject(value, node);
			return GET_SUCCESS;
		}
		break;

	case OP_ATOM_previousElementSibling:
		{
			DOM_Node *node;
			GET_FAILED_IF_ERROR(GetPreviousSibling(node));

			while(node)
			{
				if(node && ELEMENT_NODE == node->GetNodeType())
				{
					break;
				}

				GET_FAILED_IF_ERROR(node->GetPreviousSibling(node));
			}

			DOMSetObject(value, node);
			return GET_SUCCESS;
		}
		break;

	case OP_ATOM_firstChild:
		return DOMSetElement(value, this_element->FirstChildActual());

	case OP_ATOM_lastChild:
		return DOMSetElement(value, this_element->LastChildActual());

	case OP_ATOM_previousSibling:
		return DOMSetElement(value, this_element->PredActual());

	case OP_ATOM_nextSibling:
		return DOMSetElement(value, this_element->SucActual());

	case OP_ATOM_parentNode:
		return DOMSetParentNode(value);

	case OP_ATOM_all:
		return GetCollection(value, ALL, "HTMLCollection", DOM_PRIVATE_all);

	case OP_ATOM_children:
		return GetCollection(value, CHILDREN, "HTMLCollection", DOM_PRIVATE_children);

	case OP_ATOM_childNodes:
		return GetCollection(value, CHILDNODES, "NodeList", DOM_PRIVATE_childNodes);

	case OP_ATOM_attributes:
		switch (DOMSetPrivate(value, DOM_PRIVATE_attributes))
		{
		case GET_SUCCESS:
			return GET_SUCCESS;

		case GET_NO_MEMORY:
			return GET_NO_MEMORY;

		default:
			DOM_NamedNodeMap *map;
			DOM_NamedNodeMapImpl *impl = OP_NEW(DOM_AttrMapImpl, (this));

			if (!impl)
				return GET_NO_MEMORY;

			GET_FAILED_IF_ERROR(DOM_NamedNodeMap::Make(map, this, impl));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_attributes, map));

			DOMSetObject(value, map);
			return GET_SUCCESS;
		}

	case OP_ATOM_namespaceURI:
	case OP_ATOM_prefix:
		if (value)
		{
			DOMSetNull(value);

			const uni_char *ns_uri, *ns_prefix;

			if (this_element->DOMGetNamespaceData(GetEnvironment(), this_element->GetNsIdx(), ns_uri, ns_prefix))
				if (property_name == OP_ATOM_namespaceURI && *ns_uri)
					DOMSetString(value, ns_uri);
				else if (property_name == OP_ATOM_prefix && *ns_prefix)
					DOMSetString(value, ns_prefix);
		}
		return GET_SUCCESS;

	case OP_ATOM_offsetHeight:
	case OP_ATOM_offsetLeft:
	case OP_ATOM_offsetParent:
	case OP_ATOM_offsetTop:
	case OP_ATOM_offsetWidth:
	case OP_ATOM_clientHeight:
	case OP_ATOM_clientLeft:
	case OP_ATOM_clientTop:
	case OP_ATOM_clientWidth:
	case OP_ATOM_scrollHeight:
	case OP_ATOM_scrollLeft:
	case OP_ATOM_scrollTop:
	case OP_ATOM_scrollWidth:
#ifdef PAGED_MEDIA_SUPPORT
	case OP_ATOM_currentPage:
	case OP_ATOM_pageCount:
#endif // PAGED_MEDIA_SUPPORT
		if (value)
		{
			if (!this_element->IsMatchingType(HE_EMBED, NS_HTML) && !this_element->IsMatchingType(HE_OBJECT, NS_HTML))
				if (ShouldBlockWaitingForStyle(GetFramesDocument(), origining_runtime))
				{
					DOM_DelayedLayoutListener *listener = OP_NEW(DOM_DelayedLayoutListener, (GetFramesDocument(), GetCurrentThread(origining_runtime)));
					if (!listener)
						return GET_NO_MEMORY;

					DOMSetNull(value);
					return GET_SUSPEND;
				}

			return DOM_Element::GetNameRestart(property_name, value, origining_runtime, NULL);
		}
		return GET_SUCCESS;

	default:
		return DOM_Node::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_Element::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState state = PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (state != PUT_FAILED)
			return state;
	}

	return DOM_Node::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_Element::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_tagName:
		return PUT_READ_ONLY;

	case OP_ATOM_scrollLeft:
	case OP_ATOM_scrollTop:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else
		{
			int int_value = (int) value->value.number, *left = NULL, *top = NULL;

			if (property_name == OP_ATOM_scrollLeft)
				left = &int_value;
			else
				top = &int_value;

			PUT_FAILED_IF_ERROR(this_element->DOMSetPositionAndSize(GetEnvironment(), HTML_Element::DOM_PS_SCROLL, left, top, NULL, NULL));
		}
		return PUT_SUCCESS;

#ifdef PAGED_MEDIA_SUPPORT
	case OP_ATOM_currentPage:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else
		{
			int current_page = static_cast<int>(value->value.number);

			if (current_page < 0)
				return DOM_PUTNAME_DOMEXCEPTION(INVALID_MODIFICATION_ERR);

			PUT_FAILED_IF_ERROR(this_element->DOMSetCurrentPage(GetEnvironment(), current_page));
			return PUT_SUCCESS;
		}

	case OP_ATOM_pageCount:
		return PUT_READ_ONLY;
#endif // PAGED_MEDIA_SUPPORT

	default:
		return DOM_Node::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_Element::GetNameRestart(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return DOM_Object::GetNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

/* virtual */ ES_GetState
DOM_Element::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	HTML_Element::DOMPositionAndSizeType type;

	switch (property_name)
	{
	default:
		return GET_FAILED;

	case OP_ATOM_clientHeight:
	case OP_ATOM_clientLeft:
	case OP_ATOM_clientTop:
	case OP_ATOM_clientWidth:
		type = HTML_Element::DOM_PS_CLIENT;
		break;

	case OP_ATOM_offsetHeight:
	case OP_ATOM_offsetLeft:
	case OP_ATOM_offsetTop:
	case OP_ATOM_offsetWidth:
		type = HTML_Element::DOM_PS_OFFSET;
		break;

	case OP_ATOM_scrollHeight:
	case OP_ATOM_scrollLeft:
	case OP_ATOM_scrollTop:
	case OP_ATOM_scrollWidth:
		type = HTML_Element::DOM_PS_SCROLL;
		break;

#ifdef PAGED_MEDIA_SUPPORT
	case OP_ATOM_currentPage:
	case OP_ATOM_pageCount:
	{
		unsigned int current_page, page_count;
		GET_FAILED_IF_ERROR(this_element->DOMGetPageInfo(environment, current_page, page_count));
		DOMSetNumber(value, property_name == OP_ATOM_pageCount ? page_count : current_page);
		return GET_SUCCESS;
	}
#endif // PAGED_MEDIA_SUPPORT

	case OP_ATOM_offsetParent:
		HTML_Element *parent;
		GET_FAILED_IF_ERROR(this_element->DOMGetOffsetParent(environment, parent));
		DOMSetElement(value, parent);
		return GET_SUCCESS;
	}

	int left, top, width, height;

	GET_FAILED_IF_ERROR(this_element->DOMGetPositionAndSize(environment, type, left, top, width, height));

	int result;

	switch (property_name)
	{
	case OP_ATOM_clientHeight:
	case OP_ATOM_offsetHeight:
	case OP_ATOM_scrollHeight:
		result = height;
		break;

	case OP_ATOM_clientLeft:
	case OP_ATOM_offsetLeft:
	case OP_ATOM_scrollLeft:
		result = left;
		break;

	case OP_ATOM_clientTop:
	case OP_ATOM_offsetTop:
	case OP_ATOM_scrollTop:
		result = top;
		break;

	default:
		result = width;
	}

	DOMSetNumber(value, result);
	return GET_SUCCESS;
}

/* virtual */ ES_DeleteStatus
DOM_Element::DeleteName(const uni_char* property_name, ES_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	int index;
	BOOL case_sensitive = HasCaseSensitiveNames();
	if (this_element->DOMHasAttribute(environment, ATTR_XML, property_name, NS_IDX_ANY_NAMESPACE, case_sensitive, index))
		DELETE_FAILED_IF_MEMORY_ERROR(this_element->DOMRemoveAttribute(environment, property_name, NS_IDX_ANY_NAMESPACE, case_sensitive));

	return DELETE_OK;
}

/* virtual */ void
DOM_Element::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);

	if (!via_tree)
		TraceElementTree(this_element);

	DOM_Attr *attr = first_attr;
	while (attr)
	{
		GCMarkAndTrace(GetRuntime(), attr);
		attr = attr->GetNextAttr();
	}

#ifdef DOM3_XPATH
	DOM_XPathNamespace *ns = first_ns;
	while (ns)
	{
		GCMarkAndTrace(GetRuntime(), ns);
		ns = ns->GetNextNS();
	}
#endif // DOM3_XPATH
}

/* virtual */ void
DOM_Element::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_Node::DOMChangeRuntime(new_runtime);

	DOM_Attr *attr = first_attr;
	while (attr)
	{
		attr->DOMChangeRuntime(new_runtime);
		attr = attr->GetNextAttr();
	}

#ifdef DOM3_XPATH
	DOM_XPathNamespace *ns = first_ns;
	while (ns)
	{
		ns->DOMChangeRuntime(new_runtime);
		ns = ns->GetNextNS();
	}
#endif // DOM3_XPATH

	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_all);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_children);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_childNodes);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_attributes);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_style);
	DOMChangeRuntimeOnPrivate(DOM_PRIVATE_currentStyle);
}

/* virtual */ void
DOM_Element::DOMChangeOwnerDocument(DOM_Document *new_ownerDocument)
{
	DOM_Node::DOMChangeOwnerDocument(new_ownerDocument);

	DOM_Attr *attr = first_attr;
	while (attr)
	{
		attr->DOMChangeOwnerDocument(new_ownerDocument);
		attr = attr->GetNextAttr();
	}

#ifdef DOM3_XPATH
	DOM_XPathNamespace *ns = first_ns;
	while (ns)
	{
		ns->DOMChangeOwnerDocument(new_ownerDocument);
		ns = ns->GetNextNS();
	}
#endif // DOM3_XPATH
}

/* virtual */ OP_STATUS
DOM_Element::GetBoundingClientRect(DOM_Object *&object)
{
	RECT rect;

	int left, top, width, height;

	RETURN_IF_ERROR(this_element->DOMGetPositionAndSize(GetEnvironment(), HTML_Element::DOM_PS_BORDER, left, top, width, height));

	rect.left = left;
	rect.top = top;
	rect.right = left + width;
	rect.bottom = top + height;

	return MakeClientRect(object, rect, GetRuntime());
}

/* virtual */ OP_STATUS
DOM_Element::GetClientRects(DOM_ClientRectList *&object)
{
	RETURN_IF_ERROR(DOM_ClientRectList::Make(object, GetRuntime()));

	RETURN_IF_ERROR(this_element->DOMGetPositionAndSizeList(GetEnvironment(), HTML_Element::DOM_PS_BORDER, object->GetList()));

	return OpStatus::OK;
}

OP_STATUS
DOM_Element::MakeClientRect(DOM_Object *&object, RECT &rect, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(object = OP_NEW(DOM_Object, ()),
													runtime,
													runtime->GetPrototype(DOM_Runtime::CLIENTRECT_PROTOTYPE),
													"ClientRect"));

	ES_Value val;
	val.type = VALUE_NUMBER;

	val.value.number = rect.left;
	object->PutL("left", val, TRUE);

	val.value.number = rect.top;
	object->PutL("top", val, TRUE);

	val.value.number = rect.right;
	object->PutL("right", val, TRUE);

	val.value.number = rect.bottom;
	object->PutL("bottom", val, TRUE);

	val.value.number = rect.right - rect.left;
	object->PutL("width", val, TRUE);

	val.value.number = rect.bottom - rect.top;
	object->PutL("height", val, TRUE);

	return OpStatus::OK;
}

OP_STATUS
DOM_Element::GetAttributeNode(DOM_Attr *&node, Markup::AttrType attr, const uni_char *name, int ns_idx, BOOL create_if_has_attribute, BOOL create_always, BOOL case_sensitive)
{
	BOOL dummy_has_attr;
	int  dummy_index;

	return GetAttributeNode(node, attr, name, ns_idx, dummy_has_attr, dummy_index, create_if_has_attribute, create_always, case_sensitive);
}

OP_STATUS
DOM_Element::GetAttributeNode(DOM_Attr *&node, Markup::AttrType attr, const uni_char *name, int ns_idx, BOOL &has_attribute, int &index, BOOL create_if_has_attribute, BOOL create_always, BOOL case_sensitive)
{
	node = first_attr;

	if (create_if_has_attribute || create_always || node)
	{
		const uni_char *ns_uri, *ns_prefix;
		has_attribute = FALSE;

		if (attr != ATTR_XML)
			name = HTM_Lex::GetAttributeString(attr, g_ns_manager->GetNsTypeAt(this_element->ResolveNsIdx(ns_idx)));

		const uni_char *colon = uni_strchr(name, ':');
		if (colon)
		{
			ns_idx = this_element->ResolveNameNs(name, colon - name, colon + 1);
			if (ns_idx != NS_IDX_DEFAULT)
				name = colon + 1;
		}

		if (this_element->DOMHasAttribute(GetEnvironment(), attr, name, ns_idx, case_sensitive, index))
		{
			has_attribute = TRUE;

			int attr_ns_idx = this_element->DOMGetAttributeNamespaceIndex(GetEnvironment(), name, ns_idx);
			if (attr_ns_idx != NS_IDX_DEFAULT && attr_ns_idx != NS_IDX_NOT_FOUND)
				ns_idx = attr_ns_idx;
		}

		if (ns_idx == NS_IDX_ANY_NAMESPACE || !this_element->DOMGetNamespaceData(GetEnvironment(), ns_idx, ns_uri, ns_prefix))
			ns_uri = ns_prefix = NULL;

		while (node)
		{
			if (node->HasName(name, ns_uri))
				return OpStatus::OK;

			/* If the attribute is without namespace URI, try to
			   match its qualified name against ns_prefix. */
			if (!node->GetNsUri() && ns_prefix)
			{
				BOOL is_case_sensitive = node->IsCaseSensitive();
				const uni_char *node_name = node->GetName();
				const uni_char *node_ns_prefix = node->GetNsPrefix();
				if (node_ns_prefix)
				{
					if (is_case_sensitive ? uni_str_eq(ns_prefix, node_ns_prefix) : uni_stri_eq(ns_prefix, node_ns_prefix))
						if (is_case_sensitive ? uni_str_eq(name, node_name) : uni_stri_eq(name, node_name))
							return OpStatus::OK;
				}
				else
				{
					const uni_char *colon = uni_strchr(node_name, ':');
					if (colon && (is_case_sensitive ? uni_str_eq(name, colon + 1) : uni_stri_eq(name, colon + 1)))
						if (is_case_sensitive ? uni_strncmp(ns_prefix, node_name, colon - node_name) == 0 : uni_strnicmp(ns_prefix, node_name, colon - node_name) == 0)
							return OpStatus::OK;
				}
			}
			node = node->GetNextAttr();
		}

		if (create_if_has_attribute && has_attribute || create_always)
		{
			RETURN_IF_ERROR(DOM_Attr::Make(node, this, name, ns_uri, ns_prefix, FALSE));

			if (has_attribute)
				node->AddToElement(this, case_sensitive);
		}
	}

	return OpStatus::OK;
}

#ifdef DOM3_XPATH

OP_STATUS
DOM_Element::GetXPathNamespaceNode(DOM_XPathNamespace *&nsnode, const uni_char *prefix, const uni_char *uri)
{
	nsnode = first_ns;

	while (nsnode)
	{
		if (nsnode->HasName(prefix, uri))
			return OpStatus::OK;

		nsnode = nsnode->GetNextNS();
	}

	RETURN_IF_ERROR(DOM_XPathNamespace::Make(nsnode, this, prefix, uri));

	nsnode->SetNextNS(first_ns);
	first_ns = nsnode;

	return OpStatus::OK;
}

#endif // DOM3_XPATH

const uni_char*
DOM_Element::GetTagName(TempBuffer *buffer, BOOL uppercase, BOOL include_prefix/*=TRUE*/)
{
	const uni_char* localpart;
	int ns_idx;

	if (!(localpart = this_element->DOMGetElementName(GetEnvironment(), buffer, ns_idx, uppercase)))
		return NULL;

	if (include_prefix)
	{
		NS_Element *ns_elm = g_ns_manager->GetElementAt(ns_idx);
		const uni_char* prefix = ns_elm ? ns_elm->GetPrefix() : NULL;
		if (prefix && *prefix)
		{
			/* Gathering the element name will not have resorted to using the 'buffer'; only required
			   for HE_UNKNOWNs when in NS_IDX_HTML. Which doesn't have a prefix. */
			OP_ASSERT(localpart != buffer->GetStorage());

			if (OpStatus::IsError(buffer->Append(prefix)) ||
			    OpStatus::IsError(buffer->Append(UNI_L(":"))) ||
			    OpStatus::IsError(buffer->Append(localpart)))
				return NULL;

			return buffer->GetStorage();
		}
	}

	return localpart;
}

#ifdef DOM2_MUTATION_EVENTS

BOOL
DOM_Element::HasAttrModifiedHandlers()
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	return environment->HasEventHandlers(DOMATTRMODIFIED) && environment->IsEnabled() && environment->HasEventHandler(GetThisElement(), DOMATTRMODIFIED);
}

OP_STATUS
DOM_Element::SendAttrModified(ES_Thread *interrupt_thread, DOM_Attr *attr, const uni_char *prevValue, const uni_char *newValue)
{
	DOM_MutationEvent::AttrChangeType attrChange;

	if (!prevValue && !newValue)
		return OpStatus::OK;
	else if (!prevValue)
	{
		attrChange = DOM_MutationEvent::ADDITION;
		prevValue = UNI_L("");
	}
	else if (!newValue)
	{
		attrChange = DOM_MutationEvent::REMOVAL;
		newValue = UNI_L("");
	}
	else
		attrChange = DOM_MutationEvent::MODIFICATION;

	return DOM_MutationEvent::SendAttrModified(interrupt_thread, this, attr, attrChange, attr->GetName(), prevValue, newValue);
}

OP_STATUS
DOM_Element::SendAttrModified(ES_Thread *interrupt_thread, const uni_char *name, int ns_idx, const uni_char *prevValue, const uni_char *newValue)
{
	DOM_Attr *attr;
	RETURN_IF_ERROR(GetAttributeNode(attr, ATTR_XML, name, ns_idx, TRUE, TRUE, HasCaseSensitiveNames()));
	return SendAttrModified(interrupt_thread, attr, prevValue, newValue);
}

#endif // DOM2_MUTATION_EVENTS

OP_STATUS
DOM_Element::SetAttribute(Markup::AttrType attr, const uni_char *name, int ns_idx, const uni_char *value, unsigned value_length, BOOL case_sensitive, ES_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();

	DOM_Attr *attrnode = NULL; // Initialized to silence compiler.
	BOOL has_attribute;
	int index;
	OP_STATUS status = OpStatus::OK;

#ifdef DOM2_MUTATION_EVENTS
	BOOL send_attrmodified = HasAttrModifiedHandlers(), had_attribute;
	uni_char *prevValue = NULL;

	if (send_attrmodified)
	{
		had_attribute = this_element->DOMHasAttribute(environment, attr, name, ns_idx, case_sensitive, index);

		if (had_attribute)
		{
			TempBuffer buffer;

			DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);
			state.SetTempBuffer(&buffer);

			RETURN_IF_ERROR(UniSetStr(prevValue, this_element->DOMGetAttribute(environment, attr, name, ns_idx, case_sensitive, FALSE, index)));
		}

		status = GetAttributeNode(attrnode, attr, name, ns_idx, has_attribute, index, FALSE, TRUE, case_sensitive);
	}

	if (OpStatus::IsSuccess(status))
#endif // DOM2_MUTATION_EVENTS
		if (value)
		{
			DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);
			state.SetIsSettingAttribute();

			status = this_element->DOMSetAttribute(environment, attr, name, ns_idx, value, value_length, case_sensitive);
		}
		else
		{
#ifdef DOM2_MUTATION_EVENTS
			if (!send_attrmodified)
#endif // DOM2_MUTATION_EVENTS
				status = GetAttributeNode(attrnode, attr, name, ns_idx, has_attribute, index, FALSE, FALSE, case_sensitive);

			if (OpStatus::IsSuccess(status))
			{
				if (attrnode && attrnode->GetOwnerElement())
					status = attrnode->RemoveFromElement();

				if (OpStatus::IsSuccess(status))
				{
					DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);
					status = this_element->DOMRemoveAttribute(environment, name, ns_idx, case_sensitive);
				}
			}
		}

#ifdef DOM2_MUTATION_EVENTS
	if (send_attrmodified)
	{
		if (OpStatus::IsSuccess(status))
		{
			DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);
			state.SetTempBuffer();

			TempBuffer buffer;
			const uni_char *newValue;

			if (this_element->DOMHasAttribute(environment, attr, name, ns_idx, case_sensitive, index))
			{
				if (attrnode && !attrnode->GetOwnerElement())
					status = GetAttributeNode(attrnode, attr, name, ns_idx, TRUE, TRUE, case_sensitive);

				state.SetTempBuffer(&buffer);
				newValue = this_element->DOMGetAttribute(environment, attr, name, ns_idx, case_sensitive, FALSE);
			}
			else
				newValue = NULL;

			if (!prevValue && newValue || (prevValue && (!newValue || !uni_str_eq(prevValue, newValue))))
			{
				if (!attrnode)
					status = GetAttributeNode(attrnode, attr, name, ns_idx, TRUE, TRUE, case_sensitive);

				if (OpStatus::IsSuccess(status))
					status = SendAttrModified(GetCurrentThread(origining_runtime), attrnode, prevValue, newValue);
			}
		}

		OP_DELETEA(prevValue);
	}
#endif // DOM2_MUTATION_EVENTS

	return status;
}

#ifdef DOM_INSPECT_NODE_SUPPORT

OP_STATUS
DOM_Element::InspectAttributes(DOM_Utils::InspectNodeCallback *callback)
{
	ES_Value attributes_value;
	switch (GetName(OP_ATOM_attributes, &attributes_value, GetRuntime()))
	{
	case GET_SUCCESS: break;
	case GET_NO_MEMORY: return OpStatus::ERR_NO_MEMORY;
	default: return OpStatus::OK;
	}
	DOM_NamedNodeMap *map = DOM_VALUE2OBJECT(attributes_value, DOM_NamedNodeMap);

	ES_Value attributes_count_value;
	switch (map->GetName(OP_ATOM_length, &attributes_count_value, map->GetRuntime()))
	{
	case GET_SUCCESS: break;
	case GET_NO_MEMORY: return OpStatus::ERR_NO_MEMORY;
	default: return OpStatus::OK;
	}
	unsigned attributes_count = static_cast<unsigned>(attributes_count_value.value.number);

	for (unsigned attribute_index = 0; attribute_index < attributes_count; ++attribute_index)
	{
		ES_Value attribute_value;
		switch (map->GetIndex(attribute_index, &attribute_value, map->GetRuntime()))
		{
		case GET_SUCCESS:
			callback->AddAttribute(this, DOM_VALUE2OBJECT(attribute_value, DOM_Attr));
			break;

		case GET_NO_MEMORY:
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

#endif // DOM_INSPECT_NODE_SUPPORT

BOOL
DOM_Element::HasCaseSensitiveNames()
{
	BOOL case_sensitive = GetThisElement()->GetNsIdx() != NS_IDX_HTML;
	if (DOM_Document *dom_doc = GetOwnerDocument())
		case_sensitive = case_sensitive || dom_doc->IsXML();
	return case_sensitive;
}

int
DOM_Element::removeAttributeNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(attr, 0, DOM_TYPE_ATTR, DOM_Attr);

	if (!element->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	if (attr->GetOwnerElement() != element)
		return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);

	CALL_FAILED_IF_ERROR(element->SetAttribute(ATTR_XML, attr->GetName(), attr->GetNsIndex(), NULL, 0, element->HasCaseSensitiveNames(), origining_runtime));

	*return_value = argv[0];
	return ES_VALUE;
}

int
DOM_Element::scrollIntoView(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);

	if (argc != 0)
		DOM_CHECK_ARGUMENTS("b");

	BOOL align_to_top = argc == 0 || argv[0].value.boolean;
	CALL_FAILED_IF_ERROR(element->this_element->DOMScrollIntoView(element->GetEnvironment(), align_to_top));

	return ES_FAILED;
}

int
DOM_Element::accessAttribute(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);

	if (!element->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOM_EnvironmentImpl *environment = element->GetEnvironment();
	const uni_char *name;
	const uni_char *value = NULL;
	unsigned value_length = 0;
	int ns_idx;

	if ((data & 1) == 0)
	{
		if (data != 4)
			DOM_CHECK_ARGUMENTS("s");
		else
		{
			DOM_CHECK_ARGUMENTS("ss");
			value = argv[1].value.string;
			value_length = argv[1].GetStringLength();
		}

		name = argv[0].value.string;

		const uni_char *colon = uni_strchr(name, ':');
		if (colon)
		{
			ns_idx = element->this_element->ResolveNameNs(name, colon - name, colon + 1);
			if (ns_idx != NS_IDX_DEFAULT)
				name = colon + 1;
		}
		else
			/* The behaviour for setAttribute() differs in that it will match any
			   namespace. Otherwise the name is resolved in the default namespace. */
			ns_idx = data == SET_ATTRIBUTE ? NS_IDX_ANY_NAMESPACE : element->this_element->ResolveNameNs(UNI_L(""), 0, name);
	}
	else
	{
		if (data != 5)
			DOM_CHECK_ARGUMENTS("Ss");
		else
		{
			DOM_CHECK_ARGUMENTS("Sss");
			value = argv[2].value.string;
			value_length = argv[2].GetStringLength();
		}

		name = argv[1].value.string;
		const uni_char *ns_uri = argv[0].type == VALUE_STRING ? argv[0].value.string : NULL, *ns_prefix;

		if (ns_uri && !*ns_uri)
			ns_uri = NULL;

		const uni_char *colon = uni_strchr(name, ':');
		if (colon)
		{
			if (!ns_uri)
				return DOM_CALL_DOMEXCEPTION(NAMESPACE_ERR);

			TempBuffer *buffer = element->GetEmptyTempBuf();
			CALL_FAILED_IF_ERROR(buffer->Append(name, colon - name));
			ns_prefix = buffer->GetStorage();
			name = colon + 1;
		}
		else
			ns_prefix = NULL;

		ns_idx = ns_uri ? element->this_element->DOMGetNamespaceIndex(environment, ns_uri, ns_prefix) : NS_IDX_DEFAULT;
	}

	BOOL case_sensitive = (data & 1) || element->HasCaseSensitiveNames();

	if (data < 2)
	{
		int index;

		DOM_EnvironmentImpl::CurrentState state(environment, origining_runtime);
		state.SetTempBuffer();

		if (element->this_element->DOMHasAttribute(environment, ATTR_XML, name, ns_idx, case_sensitive, index))
			DOMSetString(return_value, element->this_element->DOMGetAttribute(environment, ATTR_XML, name, ns_idx, case_sensitive, FALSE, index));
		else
			/* This breaks the specification, but is what the competition does. */
			DOMSetNull(return_value);

		return ES_VALUE;
	}
	else if (data < 4)
	{
		int index;

		DOMSetBoolean(return_value, element->this_element->DOMHasAttribute(environment, ATTR_XML, name, ns_idx, case_sensitive, index));
		return ES_VALUE;
	}
	else if (data < 6)
	{
		if (!XMLUtils::IsValidName(XMLVERSION_1_1, name)) // Fix for bug 358541 (and dom2core compatibility)
			return element->CallDOMException(DOM_Object::INVALID_CHARACTER_ERR, return_value);

		CALL_FAILED_IF_ERROR(element->SetAttribute(ATTR_XML, name, ns_idx, value, value_length, case_sensitive, origining_runtime));
	}
	else
		CALL_FAILED_IF_ERROR(element->SetAttribute(ATTR_XML, name, ns_idx, NULL, 0, case_sensitive, origining_runtime));

	return ES_FAILED;
}

int
DOM_Element::getAttributeNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);

	if (!element->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	const uni_char *name;
	int ns_idx;

	if (data == 0)
	{
		DOM_CHECK_ARGUMENTS("s");

		name = argv[0].value.string;
		const uni_char *colon = uni_strchr(name, ':');
		if (colon)
		{
			ns_idx = element->this_element->ResolveNameNs(name, colon - name, colon + 1);
			if (ns_idx != NS_IDX_DEFAULT)
				name = colon + 1;
		}
		else
			ns_idx = element->this_element->ResolveNameNs(UNI_L(""), 0, name);
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Ss");

		name = argv[1].value.string;
		ns_idx = argv[0].type == VALUE_STRING ? element->this_element->DOMGetNamespaceIndex(element->GetEnvironment(), argv[0].value.string) : NS_IDX_DEFAULT;
	}

	BOOL case_sensitive = (data == 1) || element->HasCaseSensitiveNames();
	DOM_Attr *attr;
	CALL_FAILED_IF_ERROR(element->GetAttributeNode(attr, ATTR_XML, name, ns_idx, TRUE, FALSE, case_sensitive));

	DOM_Object::DOMSetObject(return_value, attr);
	return ES_VALUE;
}

int
DOM_Element::setAttributeNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(new_attr, 0, DOM_TYPE_ATTR, DOM_Attr);

	if (!element->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	if (new_attr->GetOwnerElement() == element)
	{
		DOM_Object::DOMSetObject(return_value, new_attr);
		return ES_VALUE;
	}

	if (new_attr->GetOwnerElement())
		return DOM_CALL_DOMEXCEPTION(INUSE_ATTRIBUTE_ERR);

	if (new_attr->GetEnvironment() != element->GetEnvironment() || element->GetOwnerDocument() != new_attr->GetOwnerDocument())
		return DOM_CALL_DOMEXCEPTION(WRONG_DOCUMENT_ERR);

	int ns_idx;

	const uni_char *name = new_attr->GetName();
	if (data == 0 || !new_attr->GetNsUri())
	{
		const uni_char *colon = uni_strchr(name, ':');
		if (colon)
		{
			ns_idx = element->this_element->ResolveNameNs(name, colon - name, colon + 1);
			if (ns_idx != NS_IDX_DEFAULT)
				name = colon + 1;
		}
		else if (new_attr->GetNsUri())
			ns_idx = element->this_element->DOMGetNamespaceIndex(element->GetEnvironment(), new_attr->GetNsUri());
		else
			ns_idx = NS_IDX_ANY_NAMESPACE;
	}
	else
		ns_idx = element->this_element->DOMGetNamespaceIndex(element->GetEnvironment(), new_attr->GetNsUri());

	BOOL case_sensitive = (data == 1) || element->HasCaseSensitiveNames();
	DOM_Attr *old_attr;
	CALL_FAILED_IF_ERROR(element->GetAttributeNode(old_attr, ATTR_XML, name, ns_idx, TRUE, FALSE, case_sensitive));
	if (old_attr)
	{
		CALL_FAILED_IF_ERROR(element->SetAttribute(ATTR_XML, old_attr->GetName(), old_attr->GetNsIndex(), NULL, 0, case_sensitive, origining_runtime));
		DOM_Object::DOMSetObject(return_value, old_attr);
	}
	else
		DOM_Object::DOMSetNull(return_value);

	/* This is a little bit tricky.  DOM_Attr::GetNsIndex needs an owner element,
	   so DOM_Attr::AddToElement needs to be called first.  But DOM_Attr::GetValue
	   will read the value from the owner element if one is set, so it must be
	   called before DOM_Attr::AddToElement, and the value needs to be copied
	   because DOM_Attr::AddToElement frees it. */

	const uni_char *value = new_attr->GetValue();
	unsigned value_length;
	TempBuffer buffer;

	if (value && *value)
	{
		CALL_FAILED_IF_ERROR(buffer.Append(value));
		value = buffer.GetStorage();
		value_length = buffer.Length();
	}
	else
	{
		value = UNI_L("");
		value_length = 0;
	}

	new_attr->AddToElement(element, case_sensitive);

	CALL_FAILED_IF_ERROR(element->SetAttribute(ATTR_XML, name, ns_idx, value, value_length, case_sensitive, origining_runtime));
	return ES_VALUE;
}

int
DOM_Element::getElementsByTagName(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	/* This function is used both as Element.getElementsByTagName{,NS} (data == 0 or 1) and
	   Document.getElementsByTagName{,NS} (data == 2 or 3). */

	DOM_EnvironmentImpl *environment;
	DOM_Element *root;
	BOOL include_root, is_xml;

	if (data < 2)
	{
		DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);
		environment = element->GetEnvironment();
		root = element;
		include_root = FALSE;
		is_xml = element->GetOwnerDocument()->IsXML();
		OP_ASSERT(root);
	}
	else
	{
		DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
		environment = document->GetEnvironment();
		root = document->GetRootElement(); // May be NULL in case the LogicalDocument doesn't exist yet or is all empty
		include_root = TRUE;
		is_xml = document->IsXML();
	}

	OP_ASSERT(environment);

	const uni_char *ns_uri, *localname;
	if (data == 0 || data == 2)
	{
		DOM_CHECK_ARGUMENTS("s");
		ns_uri = UNI_L("*");
		localname = argv[0].value.string;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Ss");
		ns_uri = argv[0].type == VALUE_STRING ? argv[0].value.string : 0;
		localname = argv[1].value.string;
	}

	if (!this_object->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	if (ns_uri && !*ns_uri)
		ns_uri = NULL;

	if (localname && !*localname)
		localname = NULL;

	DOM_Collection *collection = NULL;

	/* Special case: if matching over (*|tag), not caring about namespaces, use ALLELEMENTS or a specialised
	   HTML_ElementType filter. A common case, so avoid name-based matching via the default TagsCollectionFilter. */
	if (!is_xml && localname && !(data & 0x1) && ns_uri && ns_uri[0] == '*' && !ns_uri[1])
	{
		if (localname[0] == '*' && !localname[1])
		{
			DOM_SimpleCollectionFilter filter(ALLELEMENTS);

			CALL_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, environment, root, include_root, TRUE, filter));
		}
		else
		{
			Markup::Type ty = HTM_Lex::GetElementType(localname, NS_HTML, FALSE);
			if (ty != HE_UNKNOWN && !g_html5_name_mapper->HasAmbiguousName(ty))
			{
				DOM_HTMLElementCollectionFilter filter(ty, HTM_Lex::GetTagString(ty), TRUE);

				CALL_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, environment, root, include_root, TRUE, filter));
			}
		}
	}

	if (!collection)
	{
		DOM_TagsCollectionFilter filter(NULL, ns_uri, localname, is_xml || data == 1);

		CALL_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, environment, root, include_root, TRUE, filter));
	}

	DOMSetObject(return_value, collection);
	return ES_VALUE;
}

#ifdef DOM_FULLSCREEN_MODE
int
DOM_Element::requestFullscreen(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);
	DOM_Document* doc = element->GetOwnerDocument();
	CALL_FAILED_IF_ERROR(doc->RequestFullscreen(element, origining_runtime));
	return ES_FAILED;
}
#endif // DOM_FULLSCREEN_MODE

int
DOM_Element::getBoundingClientRect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);

	DOM_Object *object;
	CALL_FAILED_IF_ERROR(element->GetBoundingClientRect(object));

	DOMSetObject(return_value, object);
	return ES_VALUE;
}

int
DOM_Element::getClientRects(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);

	DOM_ClientRectList *object;
	CALL_FAILED_IF_ERROR(element->GetClientRects(object));

	DOMSetObject(return_value, object);
	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Element)
	DOM_FUNCTIONS_FUNCTION(DOM_Element, DOM_Element::removeAttributeNode, "removeAttributeNode", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_Element, DOM_Element::scrollIntoView, "scrollIntoView", "b-")
	DOM_FUNCTIONS_FUNCTION(DOM_Element, DOM_Element::getBoundingClientRect, "getBoundingClientRect", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Element, DOM_Element::getClientRects, "getClientRects", 0)
#ifdef DOM_FULLSCREEN_MODE
	DOM_FUNCTIONS_FUNCTION(DOM_Element, DOM_Element::requestFullscreen, "requestFullscreen", 0)
#endif // DOM_FULLSCREEN_MODE
DOM_FUNCTIONS_END(DOM_Element)

DOM_FUNCTIONS_WITH_DATA_START(DOM_Element)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::GET_ATTRIBUTE, "getAttribute", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::GET_ATTRIBUTE_NS, "getAttributeNS", "Ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::HAS_ATTRIBUTE, "hasAttribute", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::HAS_ATTRIBUTE_NS, "hasAttributeNS", "Ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::SET_ATTRIBUTE, "setAttribute", "ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::SET_ATTRIBUTE_NS, "setAttributeNS", "Sss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::REMOVE_ATTRIBUTE, "removeAttribute", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::accessAttribute, DOM_Element::REMOVE_ATTRIBUTE_NS, "removeAttributeNS", "Ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::getAttributeNode, 0, "getAttributeNode", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::getAttributeNode, 1, "getAttributeNodeNS", "Ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::setAttributeNode, 0, "setAttributeNode", "o-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::setAttributeNode, 1, "setAttributeNodeNS", "o-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::getElementsByTagName, 0, "getElementsByTagName", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Element::getElementsByTagName, 1, "getElementsByTagNameNS", "Ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Document::getElementsByClassName, 1, "getElementsByClassName", "s-")
#ifdef DOM_SELECTORS_API
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Selector::querySelector, DOM_Selector::QUERY_ELEMENT, "querySelector", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Selector::querySelector, DOM_Selector::QUERY_ELEMENT | DOM_Selector::QUERY_ALL, "querySelectorAll", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Element, DOM_Selector::querySelector, DOM_Selector::QUERY_ELEMENT | DOM_Selector::QUERY_MATCHES, "oMatchesSelector", "s-")
#endif // DOM_SELECTORS_API
DOM_FUNCTIONS_WITH_DATA_END(DOM_Element)

/* static */ OP_STATUS
DOM_ClientRectList::Make(DOM_ClientRectList *&list, DOM_Runtime *runtime)
{
	return DOMSetObjectRuntime(list = OP_NEW(DOM_ClientRectList, ()), runtime, runtime->GetPrototype(DOM_Runtime::CLIENTRECTLIST_PROTOTYPE), "ClientRectList");
}

/* virtual */ ES_GetState
DOM_ClientRectList::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, rect_list.GetCount());
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_ClientRectList::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	if ((unsigned) property_index < rect_list.GetCount())
	{
		if (value)
		{
			if (!object_list)
			{
				object_list = OP_NEWA(DOM_Object *, rect_list.GetCount());
				if (!object_list)
					return GET_NO_MEMORY;
				for (unsigned index = 0, length = rect_list.GetCount(); index < length; ++index)
					object_list[index] = NULL;
			}

			if (!object_list[property_index])
			{
				RECT *rect = rect_list.Get(property_index);
				GET_FAILED_IF_ERROR(DOM_Element::MakeClientRect(object_list[property_index], *rect, GetRuntime()));
			}

			DOMSetObject(value, object_list[property_index]);
		}

		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ void
DOM_ClientRectList::GCTrace()
{
	if (object_list)
		for (unsigned index = 0, length = rect_list.GetCount(); index < length; ++index)
			GCMark(object_list[index]);
}

/* virtual */ ES_GetState
DOM_ClientRectList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = rect_list.GetCount();
	return GET_SUCCESS;
}

/* static */ int
DOM_ClientRectList::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_CLIENTRECTLIST, DOM_ClientRectList);
	DOM_CHECK_ARGUMENTS("n");

	double index = argv[0].value.number;
	if (op_isintegral(index) && index >= 0 && index < list->rect_list.GetCount())
		return ConvertGetNameToCall(list->GetIndex(static_cast<int>(argv[0].value.number), return_value, origining_runtime), return_value);
	else
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
}

DOM_FUNCTIONS_START(DOM_ClientRectList)
	DOM_FUNCTIONS_FUNCTION(DOM_ClientRectList, DOM_ClientRectList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_ClientRectList)

