/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef LOGDOCXMLTREEACCESSOR_SUPPORT

#include "modules/logdoc/src/logdocxmltreeaccessor.h"

#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmlutils.h"

#undef IMPORT
#define IMPORT(node) static_cast<HTML_Element *>(node)

#undef EXPORT
#define EXPORT(element) static_cast<XMLTreeAccessor::Node *>(element)

BOOL
LogdocXMLTreeAccessor::IsIncludedByFilters(HTML_Element *element)
{
	Markup::Type type = element->Type();

	if (has_nodetype_filter)
	{
		if (type != nodetype_filter)
		{
			if (nodetype_filter == HE_UNKNOWN)
			{
				if (!Markup::IsRealElement(type))
					return FALSE;
			}
			else if (nodetype_filter == HE_TEXT)
			{
				if (type != HE_TEXTGROUP)
					return FALSE;
			}
			else
				return FALSE;
		}
		else if (type == HE_TEXT)
		{
			// Only the text group should be accessible to the caller and not its children.
			if (element->Parent() && element->Parent()->Type() == Markup::HTE_TEXTGROUP)
				return FALSE;

			if (element->GetIsCDATA() ? cdata_section_filter == 0 : cdata_section_filter == 1)
				return FALSE;
		}
	}
	if (has_element_name_filter || has_attribute_name_filter || has_attribute_value_filter)
	{
		if (!Markup::IsRealElement(type))
			return FALSE;

		BOOL is_html = element->GetNsIdx() == NS_IDX_HTML;

		if (has_element_name_filter)
		{
			if (is_html && element_name_filter_nsindex == NS_IDX_DEFAULT)
			{
				if (!uni_stri_eq(element->GetTagName(), element_name_filter.GetLocalPart()))
					return FALSE;
			}
			else if (element_name_filter_type != type && element_name_filter_type != -2)
				return FALSE;
			else
			{
				int nsidx = element->GetNsIdx();

				if (element_name_filter_type == HE_UNKNOWN || element_name_filter_nsindex != nsidx)
				{
					if (element_name_filter_nsindex != nsidx && (element_name_filter_nsindex == NS_IDX_DEFAULT || nsidx == NS_IDX_DEFAULT))
						return FALSE;

					if (element_name_filter_type == HE_UNKNOWN)
						if (!(uni_strcmp(element_name_filter.GetLocalPart(), element->GetTagName()) == 0))
							return FALSE;

					if (element_name_filter_nsindex != NS_IDX_DEFAULT)
						if (!(uni_strcmp(element_name_filter.GetUri(), g_ns_manager->GetElementAt(nsidx)->GetUri()) == 0))
							return FALSE;
				}
			}
		}

		if (has_attribute_name_filter || has_attribute_value_filter)
		{
			HTML_AttrIterator iterator(element);

			const uni_char *filter_name = NULL, *filter_ns_uri = NULL;
			int filter_ns_idx = -1;
			NS_Type filter_ns_type = NS_NONE;
			if (has_attribute_name_filter)
			{
				if (attribute_name_filter.GetLocalPart() && *attribute_name_filter.GetLocalPart())
					filter_name = attribute_name_filter.GetLocalPart();
				if (attribute_name_filter.GetUri() && *attribute_name_filter.GetUri())
				{
					filter_ns_uri = attribute_name_filter.GetUri();
					filter_ns_idx = attribute_name_filter.GetNsIndex();
					filter_ns_type = g_ns_manager->GetNsTypeAt(filter_ns_idx);
				}
				else
					filter_ns_idx = NS_IDX_DEFAULT;
			}
			const uni_char *filter_value = NULL;
			if (has_attribute_value_filter)
				filter_value = attribute_value_filter_cstr;

			const uni_char *name, *value;
			int ns_idx;
			BOOL specified, id;

			while (iterator.GetNext(name, value, ns_idx, specified, id))
			{
				if (filter_ns_idx != -1 && filter_ns_idx != ns_idx && (!is_html || !(filter_ns_idx == NS_IDX_DEFAULT && ns_idx == NS_IDX_HTML)) && (filter_ns_idx == NS_IDX_DEFAULT || ns_idx == NS_IDX_DEFAULT || g_ns_manager->GetNsTypeAt(ns_idx) != filter_ns_type || filter_ns_type == NS_USER && uni_strcmp(filter_ns_uri, g_ns_manager->GetElementAt(ns_idx)->GetUri()) != 0))
					continue;

				if (filter_name && (is_html ? !uni_stri_eq(filter_name, name) : !uni_str_eq(filter_name, name)))
					continue;

				if (filter_value && !(value && uni_strcmp(filter_value, value) == 0))
					continue;

				return TRUE;
			}

			return FALSE;
		}
	}

	return TRUE;
}

/* virtual */ BOOL
LogdocXMLTreeAccessor::IsCaseSensitive()
{
	return !rootelement || rootelement->GetNsIdx() != NS_IDX_HTML;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetRoot()
{
	return EXPORT(rootnode);
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetRootElement()
{
	if (IsIncludedByFilters(rootelement))
		return EXPORT(rootelement);
	else
		return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetParent(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	if (from == rootnode)
		return NULL;

	HTML_Element *parent = from->ParentActual();

	if (!parent)
		parent = rootnode;

	if (IsIncludedByFilters(parent))
		return EXPORT(parent);
	else
		return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetAncestor(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	if (from == rootnode)
		return NULL;

	while (HTML_Element *parent = from->ParentActual())
	{
		if (stop_at_filter == parent)
			return NULL;
		else if (IsIncludedByFilters(parent))
			return EXPORT(parent);
		else
			from = parent;
	}

	return IsIncludedByFilters(rootnode) ? EXPORT(rootnode) : NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetFirstChild(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	if (from == rootnode && !rootnode->FirstChildActual())
		if (stop_at_filter != rootelement && IsIncludedByFilters(rootelement))
			return EXPORT(rootelement);
		else
			return NULL;

	HTML_Element *child = from->FirstChildActual();
	while (child)
		if (child == stop_at_filter)
			break;
		else if (IsIncludedByFilters(child))
			return EXPORT(child);
		else
			child = child->SucActual();

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetLastChild(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	if (from == rootnode && !rootnode->LastChildActual())
		if (stop_at_filter != rootelement && IsIncludedByFilters(rootelement))
			return EXPORT(rootelement);
		else
			return NULL;

	HTML_Element *child = from->LastChildActual();
	while (child)
		if (stop_at_filter == child)
			break;
		else if (IsIncludedByFilters(child))
			return EXPORT(child);
		else
			child = child->PredActual();

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetNextSibling(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	while (HTML_Element *sibling = from->SucActual())
		if (stop_at_filter == sibling)
			break;
		else if (IsIncludedByFilters(sibling))
			return EXPORT(sibling);
		else
			from = sibling;

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetPreviousSibling(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	while (HTML_Element *sibling = from->PredActual())
		if (stop_at_filter == sibling)
			break;
		else if (IsIncludedByFilters(sibling))
			return EXPORT(sibling);
		else
			from = sibling;

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetNext(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	if (from == rootnode && !rootnode->NextActual())
		if (stop_at_filter != rootelement && IsIncludedByFilters(rootelement))
			return EXPORT(rootelement);
		else
			return NULL;

	while (HTML_Element *element = from->NextActual())
		if (stop_at_filter == element)
			break;
		else if (IsIncludedByFilters(element))
			return EXPORT(element);
		else
			from = element;

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetNextDescendant(XMLTreeAccessor::Node *from0, XMLTreeAccessor::Node *ancestor0)
{
	HTML_Element *from = IMPORT(from0), *ancestor = IMPORT(ancestor0), *stop = ancestor->NextSiblingActual();

	OP_ASSERT(ancestor->IsAncestorOf(from));

	while (HTML_Element *element = from->NextActual())
		if (element == stop || stop_at_filter == element)
			break;
		else if (IsIncludedByFilters(element))
			return EXPORT(element);
		else
			from = element;

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetNextNonDescendant(XMLTreeAccessor::Node *from0)
{
	HTML_Element *iter = IMPORT(from0)->NextSiblingActual();

	while (iter)
		if (stop_at_filter == iter)
			break;
		else if (IsIncludedByFilters(iter))
			return EXPORT(iter);
		else
			iter = iter->NextActual();

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetPrevious(XMLTreeAccessor::Node *from0)
{
	HTML_Element *from = IMPORT(from0);

	while (HTML_Element *element = from->PrevActual())
		if (stop_at_filter == element)
			break;
		else if (IsIncludedByFilters(element))
			return EXPORT(element);
		else
			from = element;

	return NULL;
}

/* virtual */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetPreviousNonAncestor(XMLTreeAccessor::Node *from0, XMLTreeAccessor::Node *origin0)
{
	HTML_Element *from = IMPORT(from0), *origin = IMPORT(origin0), *ancestor = NULL;

	while (TRUE)
	{
		HTML_Element *iter = from->PredActual();

		if (iter)
		{
			while (HTML_Element *child = iter->LastChildActual())
				iter = child;
		}
		else
		{
			if (!ancestor)
			{
				ancestor = origin;

				while (!ancestor->IsAncestorOf(from))
					ancestor = ancestor->ParentActual();

				if (ancestor == from)
					ancestor = ancestor->ParentActual();
			}

			iter = from->ParentActual();

			if (!iter || stop_at_filter == iter)
				break;
			else
			{
				/* Nodes from different trees? */
				OP_ASSERT(ancestor);

				if (iter == ancestor)
				{
					ancestor = ancestor->ParentActual();
					goto skip;
				}
			}
		}

		if (stop_at_filter == iter)
			break;
		else if (IsIncludedByFilters(iter))
			return EXPORT(iter);

	skip:
		from = iter;
	}

	return NULL;
}

/* virtual */ BOOL
LogdocXMLTreeAccessor::IsAncestorOf(Node *parent, Node *child)
{
	return IMPORT(parent)->IsAncestorOf(IMPORT(child));
}

/* virtual*/ BOOL
LogdocXMLTreeAccessor::Precedes(Node *node1, Node *node2)
{
	return IMPORT(node1)->Precedes(IMPORT(node2));
}

/* virtual */ void
LogdocXMLTreeAccessor::SetStopAtFilter(Node *node)
{
	stop_at_filter = IMPORT(node);
}

/* virtual*/
BOOL LogdocXMLTreeAccessor::IsEmptyText(Node *node)
{
	HTML_Element* element = IMPORT(node);
	if (element->Type() == HE_TEXT)
		return !element->Content () || !*element->Content();
	else if (element->Type() == HE_TEXTGROUP)
    {
		HTML_Element *iter = element->FirstChild();
		while (iter)
        {
			if (iter->Type() == HE_TEXT)
            {
				if (iter->Content() && *iter->Content())
					return FALSE;
            }
			iter = iter->Suc();
        }
		return TRUE;
    }
	else
		return FALSE;
}

/* virtual*/
BOOL LogdocXMLTreeAccessor::IsWhitespaceOnly(Node *node)
{
	HTML_Element* element = IMPORT(node);
	if (element->Type () == HE_TEXT)
		return !element->Content() || XMLUtils::IsWhitespace(element->Content());
	else if (element->Type() == HE_TEXTGROUP)
    {
		HTML_Element *iter = element->FirstChild();
		while (iter)
        {
			if (iter->Type() == HE_TEXT)
				if (iter->Content() && !XMLUtils::IsWhitespace(iter->Content()))
					return FALSE;
			iter = iter->Suc();
        }
		return TRUE;
    }
	else
		return FALSE;
}

/* virtual */ void
LogdocXMLTreeAccessor::SetNodeTypeFilter(NodeType nodetype, BOOL join_text_and_cdata_section)
{
	has_nodetype_filter = TRUE;
	switch (nodetype)
	{
	case TYPE_ROOT:
		nodetype_filter = HE_DOC_ROOT;
		break;

	case TYPE_ELEMENT:
		nodetype_filter = HE_UNKNOWN;
		break;

	case TYPE_TEXT:
		nodetype_filter = HE_TEXT;
		cdata_section_filter = join_text_and_cdata_section ? 2 : 0;
		break;

	case TYPE_CDATA_SECTION:
		nodetype_filter = HE_TEXT;
		cdata_section_filter = 1;
		break;

	case TYPE_COMMENT:
		nodetype_filter = HE_COMMENT;
		break;

	case TYPE_PROCESSING_INSTRUCTION:
		nodetype_filter = HE_PROCINST;
		break;

	case TYPE_DOCTYPE:
		nodetype_filter = HE_DOCTYPE;
	}
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::SetElementNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2)
{
	has_element_name_filter = TRUE;
	if (copy)
		RETURN_IF_ERROR(element_name_filter.Set(name));
	else
		element_name_filter = name;

	if (data2 && *data2 != ~0u)
		element_name_filter_nsindex = static_cast<int> (*data2);
	else
	{
		const uni_char *uri = name.GetUri();
		if (uri && *uri)
		{
			element_name_filter_nsindex = g_ns_manager->FindNsIdx(uri, uni_strlen(uri));
			if (element_name_filter_nsindex < 0)
				element_name_filter_nsindex = -2;
		}
		else
			element_name_filter_nsindex = 0;
		if (data2)
			*data2 = static_cast<unsigned> (element_name_filter_nsindex);
	}

	if (data1 && *data1 != ~0u)
		element_name_filter_type = static_cast<int> (*data1);
	else
	{
		const uni_char *localpart = name.GetLocalPart();
		if (localpart && *localpart)
		{
			BOOL case_sensitive = element_name_filter_nsindex != NS_IDX_HTML || logdoc->IsXml();
			element_name_filter_type = HTM_Lex::GetElementType(localpart, name.GetNsType(), case_sensitive);
		}
		else
			element_name_filter_type = -2;
		if (data1)
			*data1 = static_cast<unsigned> (element_name_filter_type);
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::SetAttributeNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2)
{
	has_attribute_name_filter = TRUE;
	if (copy)
		return attribute_name_filter.Set(name);
	else
	{
		attribute_name_filter = name;
		return OpStatus::OK;
	}
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::SetAttributeValueFilter(const uni_char *value, BOOL copy)
{
	has_attribute_value_filter = TRUE;
	if (copy)
	{
		RETURN_IF_ERROR(attribute_value_filter.Set(value));
		attribute_value_filter_cstr = attribute_value_filter.CStr();
	}
	else
		attribute_value_filter_cstr = value;
	return OpStatus::OK;
}

/* virtual */ void
LogdocXMLTreeAccessor::ResetFilters()
{
	stop_at_filter = NULL;
	has_nodetype_filter = has_element_name_filter = has_attribute_name_filter = has_attribute_value_filter = FALSE;
	element_name_filter = XMLExpandedName();
	attribute_name_filter = XMLExpandedName();
	attribute_value_filter.Empty();
	attribute_value_filter_cstr = NULL;
}

/* virtual */ BOOL
LogdocXMLTreeAccessor::FilterNode(Node *node)
{
	return IsIncludedByFilters(IMPORT(node));
}

/* virtual */ XMLTreeAccessor::NodeType
LogdocXMLTreeAccessor::GetNodeType(XMLTreeAccessor::Node *node)
{
	HTML_Element *element = IMPORT(node);

	switch (element->Type())
	{
	case HE_DOC_ROOT:
		return XMLTreeAccessor::TYPE_ROOT;

	case HE_TEXT:
	case HE_TEXTGROUP:
		return element->GetIsCDATA() ? XMLTreeAccessor::TYPE_CDATA_SECTION : XMLTreeAccessor::TYPE_TEXT;

	case HE_COMMENT:
		return XMLTreeAccessor::TYPE_COMMENT;

	case HE_PROCINST:
		return XMLTreeAccessor::TYPE_PROCESSING_INSTRUCTION;

	case HE_DOCTYPE:
		return XMLTreeAccessor::TYPE_DOCTYPE;

	default:
		return XMLTreeAccessor::TYPE_ELEMENT;
	}
}

/* virtual */ void
LogdocXMLTreeAccessor::GetName(XMLCompleteName &name, XMLTreeAccessor::Node *node)
{
	name = XMLCompleteName(IMPORT(node));
}

/* virtual */const uni_char*
LogdocXMLTreeAccessor::GetPITarget(Node *node)
{
	return IMPORT(node)->GetStringAttr(ATTR_TARGET);
}

void
LogdocXMLTreeAccessor::LogdocAttributes::Initialize(HTML_Element *attr_element, BOOL ignore_default0, BOOL ignore_nsdeclarations0)
{
	if (!Markup::IsRealElement(attr_element->Type()))
		empty = TRUE;
	else
	{
		ignore_default = ignore_default0;
		ignore_nsdeclarations = ignore_nsdeclarations0;
		empty = FALSE;
		element = attr_element;
	}
}

/* virtual */ unsigned
LogdocXMLTreeAccessor::LogdocAttributes::GetCount()
{
	if (empty)
		return 0;
	else
	{
		unsigned count = 0;

		const uni_char *name, *value;
		int ns_idx;
		BOOL specified, id;

		iterator.Reset(element);
		while (iterator.GetNext(name, value, ns_idx, specified, id))
			if ((!ignore_default || specified) && !(ignore_nsdeclarations && (ns_idx == NS_IDX_XMLNS || ns_idx == NS_IDX_XMLNS_DEF)))
				++count;

		return count;
	}
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::LogdocAttributes::GetAttribute(unsigned index, XMLCompleteName &name0, const uni_char *&value0, BOOL &id0, BOOL &specified0, TempBuffer *buffer)
{
	if (!empty)
	{
		const uni_char *name, *value;
		int ns_idx;
		BOOL specified, id;

		iterator.Reset(element);
		while (iterator.GetNext(name, value, ns_idx, specified, id))
		{
			if ((!ignore_default || specified) && !(ignore_nsdeclarations && (ns_idx == NS_IDX_XMLNS || ns_idx == NS_IDX_XMLNS_DEF)))
			{
				if (index-- == 0)
				{
					name0 = XMLCompleteName(g_ns_manager->GetElementAt(ns_idx), name);
					value0 = value;
					id0 = id;
					specified0 = specified;
					return OpStatus::OK;
				}
			}
		} // end while
	}

	OP_ASSERT(!"Trying to read index that doesn't exist");
	return OpStatus::OK;
}

/* virtual */ void
LogdocXMLTreeAccessor::GetAttributes(Attributes *&attributes0, XMLTreeAccessor::Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations)
{
	attributes.Initialize(IMPORT(node), ignore_default, ignore_nsdeclarations);
	attributes0 = &attributes;
}

/* virtual */OP_STATUS
LogdocXMLTreeAccessor::GetAttribute(Node *node, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
	attributes.Initialize(IMPORT(node), FALSE, FALSE);
	return GetAttribute(&attributes, name, value, id, specified, buffer);
}

static BOOL IsEqualNames(const XMLExpandedName& name, const XMLExpandedName& other, BOOL case_sensitive)
{
	if (case_sensitive)
		return name == other;

	return 	(name.GetUri() == other.GetUri() || 
				name.GetUri() && other.GetUri() && uni_str_eq(name.GetUri(), other.GetUri())) &&
			uni_stri_eq(name.GetLocalPart(), other.GetLocalPart());
}

/* virtual */OP_STATUS
LogdocXMLTreeAccessor::GetAttribute(Attributes *attributes0, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
	OP_ASSERT(attributes0 == &attributes);

	unsigned count = attributes.GetCount();
	for (unsigned i = 0; i < count; i++)
	{
		XMLCompleteName attr_name0;

		RETURN_IF_ERROR(attributes.GetAttribute(i, attr_name0, value, id, specified, buffer));

		XMLExpandedName &attr_name = static_cast<XMLExpandedName &>(attr_name0);

		if (IsEqualNames(attr_name, name, attributes.IsCaseSensitive()))
			return OpStatus::OK;
	}

	return OpStatus::ERR;
}

/* virtual */ unsigned
LogdocXMLTreeAccessor::GetAttributeIndex(Node *node, const XMLExpandedName &name)
{
	attributes.Initialize(IMPORT(node), FALSE, FALSE);
	return GetAttributeIndex(&attributes, name);
}

/* virtual */ unsigned
LogdocXMLTreeAccessor::GetAttributeIndex(Attributes *attributes0, const XMLExpandedName &name)
{
	OP_ASSERT(attributes0 == &attributes);

	for (unsigned index = 0, count = attributes.GetCount(); index < count; ++index)
	{
		XMLCompleteName attr_name0;
		const uni_char *value;
		BOOL id, specified;

		RETURN_VALUE_IF_ERROR(attributes.GetAttribute(index, attr_name0, value, id, specified, NULL), ~0u);

		XMLExpandedName &attr_name = static_cast<XMLExpandedName &>(attr_name0);
		if (IsEqualNames(attr_name, name, attributes.IsCaseSensitive()))
			return index;
	}

	return ~0u;
}

/* virtual */ BOOL
LogdocXMLTreeAccessor::HasAttribute(Node *node, const XMLExpandedName &name, const uni_char *value)
{
	attributes.Initialize(IMPORT(node), FALSE, FALSE);
	return HasAttribute(&attributes, name, value);
}

/* virtual */ BOOL
LogdocXMLTreeAccessor::HasAttribute(Attributes *attributes0, const XMLExpandedName &name, const uni_char *value)
{
	OP_ASSERT(attributes0 == &attributes);

	TempBuffer buffer;

	for (unsigned index = 0, count = attributes.GetCount(); index < count; ++index)
	{
		XMLCompleteName attr_name0;
		const uni_char *attr_value;
		BOOL id, specified;

		RETURN_VALUE_IF_ERROR(attributes.GetAttribute(index, attr_name0, attr_value, id, specified, &buffer), FALSE);

		XMLExpandedName &attr_name = static_cast<XMLExpandedName &>(attr_name0);
		if (IsEqualNames(attr_name, name, attributes.IsCaseSensitive()))
		{
			if (!value || attr_value && uni_str_eq (value, attr_value))
				return TRUE;
		}

		buffer.Clear ();
	}

	return FALSE;
}

OP_STATUS
LogdocXMLTreeAccessor::LogdocNamespaces::Initialize(HTML_Element *new_element)
{
	if (element != new_element)
	{
		element = new_element;
		nsdeclaration = static_cast<XMLNamespaceDeclaration *>(NULL);

		if (!Markup::IsRealElement(element->Type()))
			empty = TRUE;
		else
		{
			empty = FALSE;
			RETURN_IF_ERROR(XMLNamespaceDeclaration::Push(nsdeclaration, UNI_L("http://www.w3.org/XML/1998/namespace"), 36, UNI_L("xml"), 3, 0));
			return XMLNamespaceDeclaration::PushAllInScope(nsdeclaration, element);
		}
	}
	return OpStatus::OK;
}

/* virtual */ unsigned
LogdocXMLTreeAccessor::LogdocNamespaces::GetCount()
{
	XMLNamespaceDeclaration *default_declaration = XMLNamespaceDeclaration::FindDefaultDeclaration(const_cast<const XMLNamespaceDeclaration::Reference &>(nsdeclaration));
	return empty ? 0 : XMLNamespaceDeclaration::CountDeclaredPrefixes(const_cast<const XMLNamespaceDeclaration::Reference &>(nsdeclaration)) + (default_declaration && default_declaration->GetUri() ? 1 : 0);
}

/* virtual */ void
LogdocXMLTreeAccessor::LogdocNamespaces::GetNamespace(unsigned index, const uni_char *&uri, const uni_char *&prefix)
{
	if (XMLNamespaceDeclaration *default_declaration = XMLNamespaceDeclaration::FindDefaultDeclaration(const_cast<const XMLNamespaceDeclaration::Reference &>(nsdeclaration)))
		if (default_declaration->GetUri())
			if (index == 0)
			{
				uri = default_declaration->GetUri();
				prefix = NULL;
				return;
			}
			else
				--index;

	XMLNamespaceDeclaration *declaration = XMLNamespaceDeclaration::FindDeclarationByIndex(const_cast<const XMLNamespaceDeclaration::Reference &>(nsdeclaration), index);
	uri = declaration->GetUri();
	prefix = declaration->GetPrefix();
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::GetNamespaces(Namespaces *&out_namespaces, XMLTreeAccessor::Node *node)
{
	RETURN_IF_ERROR(namespaces.Initialize(IMPORT(node)));
	out_namespaces = &namespaces;
	return OpStatus::OK;
}

/* virtual */ unsigned
LogdocXMLTreeAccessor::GetNamespaceIndex(Node *node, const uni_char *uri, const uni_char *prefix)
{
	/* FIXME: This is O(n^3) or something... */
	if (prefix && !*prefix)
		prefix = NULL;
	if (OpStatus::IsSuccess(namespaces.Initialize(IMPORT(node))))
	{
		for (unsigned index = 0, count = namespaces.GetCount(); index < count; ++index)
		{
			const uni_char *uri0, *prefix0;
			namespaces.GetNamespace(index, uri0, prefix0);
			if ((uri == uri0 || uni_strcmp(uri, uri0) == 0) && (prefix == prefix0 || prefix && prefix0 && uni_strcmp(prefix, prefix0) == 0))
				return index;
		}
	}
	OP_ASSERT(FALSE);
	return ~0u;
}

static OP_STATUS
AppendTextDescendants(HTML_Element *ancestor, const uni_char *&data, TempBuffer *buffer)
{
	buffer->Clear();
	for (HTML_Element *iter = ancestor->NextActual(), *stop = ancestor->NextSiblingActual(); iter != stop; iter = iter->NextActual())
		if (iter->Type() == HE_TEXT)
			RETURN_IF_ERROR(buffer->Append(iter->Content(), iter->ContentLength()));
	if (buffer->GetStorage())
		data = buffer->GetStorage();
	else
		data = UNI_L("");
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::GetData(const uni_char *&data, XMLTreeAccessor::Node *node, TempBuffer *buffer)
{
	HTML_Element *element = IMPORT(node);

	switch (element->Type())
	{
	case HE_TEXT:
		data = element->Content();
		return OpStatus::OK;

	case HE_TEXTGROUP:
		return AppendTextDescendants(element, data, buffer);

	case HE_COMMENT:
	case HE_PROCINST:
		data = element->GetStringAttr(ATTR_CONTENT);
		return OpStatus::OK;

	case HE_DOCTYPE:
	{
		const XMLDocumentInformation* information = GetDocumentInformation();
		if (information)
		{
			data = information->GetInternalSubset();
			return OpStatus::OK;
		}
		/* fall through */
	}

	default:
		data = NULL;
		return OpStatus::OK;
	}
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::GetCharacterDataContent(const uni_char *&data, XMLTreeAccessor::Node *node, TempBuffer *buffer)
{
	HTML_Element *element = IMPORT(node);

	switch (element->Type())
	{
	case HE_TEXT:
	case HE_TEXTGROUP:
		return GetData(data, node, buffer);

	case HE_COMMENT:
	case HE_PROCINST:
	case HE_DOCTYPE:
		data = UNI_L("");
		return OpStatus::OK;

	default:
		return AppendTextDescendants(element, data, buffer);
	}
}

/* virtual */ OP_STATUS
LogdocXMLTreeAccessor::GetElementById(XMLTreeAccessor::Node *&node, const uni_char *id)
{
	if (rootnode == logdoc->GetRoot())
	{
		NamedElementsIterator result;

		int count = logdoc->SearchNamedElements(result, NULL, id, TRUE, FALSE);

		if (count > 0)
		{
			node = EXPORT(result.GetNextElement());
			return OpStatus::OK;
		}
		else if (count == 0)
		{
			node = NULL;
			return OpStatus::OK;
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		/* FIXME: This breaks the expectations expressed in the interface
		   documentation.  Should build a table of named elements locally and
		   search it. */

		for (HTML_Element *iter = rootnode, *stop = rootnode->NextSiblingActual(); iter != stop; iter = iter->NextActual())
			if (Markup::IsRealElement(iter->Type()))
			{
				const uni_char* element_id = iter->GetId();
				if (element_id && uni_strcmp(id, element_id) == 0)
				{
					node = EXPORT(iter);
					return OpStatus::OK;
				}
			}

		node = NULL;
		return OpStatus::OK;
	}
}

/* virtual */ URL
LogdocXMLTreeAccessor::GetDocumentURL()
{
	return documenturl;
}

/* virtual */ const uni_char *
LogdocXMLTreeAccessor::GetDocumentURI()
{
	return documenturl.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
}

/* virtual */ const XMLDocumentInformation *
LogdocXMLTreeAccessor::GetDocumentInformation()
{
	return docinfo;
}

LogdocXMLTreeAccessor::LogdocXMLTreeAccessor(LogicalDocument *logdoc, HTML_Element *element)
	: logdoc(logdoc),
	  docinfo(NULL),
	  stop_at_filter(NULL),
	  has_nodetype_filter(FALSE),
	  has_element_name_filter(FALSE),
	  has_attribute_name_filter(FALSE),
	  has_attribute_value_filter(FALSE),
	  element_name_filter_type(-2),
	  element_name_filter_nsindex(-2)
{
	// Doesn't matter if it fails. We're only needing the namespace reference counter update anyway since it will be DecRefed in the destructor
	OpStatus::Ignore(dummy_rootnode.Construct(logdoc->GetHLDocProfile(), 0 /*nsidx */, HE_DOC_ROOT, NULL /* attributes */, HE_NOT_INSERTED));

	if (!element)
	{
		root = rootnode = logdoc->GetRoot();
		rootelement = logdoc->GetDocRoot();
	}
	else if (element->Type() == HE_DOC_ROOT)
	{
		root = rootnode = element;
		rootelement = NULL;

		for (HTML_Element *child = element->FirstChildActual(); child; child = child->SucActual())
			if (Markup::IsRealElement(child->Type()))
			{
				rootelement = child;
				break;
			}
	}
	else
	{
		root = element;
		rootnode = &dummy_rootnode;

		if (Markup::IsRealElement(element->Type()))
			rootelement = element;
		else
			rootelement = NULL;
	}
}

/* virtual */
LogdocXMLTreeAccessor::~LogdocXMLTreeAccessor()
{
	OP_DELETE(docinfo);
}

OP_STATUS
LogdocXMLTreeAccessor::SetDocumentInformation(const XMLDocumentInformation *new_docinfo)
{
	OP_DELETE(docinfo);

	if (!(docinfo = OP_NEW(XMLDocumentInformation, ())) || OpStatus::IsMemoryError(docinfo->Copy(*new_docinfo)))
	{
		OP_DELETE(docinfo);
		docinfo = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

void
LogdocXMLTreeAccessor::SetDocumentURL(URL url)
{
	documenturl = url;
}

/* static */ XMLTreeAccessor::Node *
LogdocXMLTreeAccessor::GetElementAsNode(HTML_Element *element)
{
	return EXPORT(element);
}

/* static */ HTML_Element *
LogdocXMLTreeAccessor::GetNodeAsElement(Node *treenode)
{
	return IMPORT(treenode);
}

#endif // LOGDOCXMLTREEACCESSOR_SUPPORT
