/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XMLUTILS_XMLTREEACCESSOR_SUPPORT

#include "modules/xmlutils/xmltreeaccessor.h"

#ifdef XMLUTILS_XMLTREEACCESSOR_FALLBACK_SUPPORT

#include "modules/xmlutils/xmlnames.h"
#include "modules/util/OpHashTable.h"

class XMLFallbackTreeAccessor_Namespaces
	: public XMLTreeAccessor::Namespaces
{
public:
	XMLFallbackTreeAccessor_Namespaces()
		: node(0),
		  nsdeclaration(0)
	{
	}
	~XMLFallbackTreeAccessor_Namespaces()
	{
	}

	XMLTreeAccessor::Node *node;
	XMLNamespaceDeclaration::Reference nsdeclaration;

	virtual unsigned GetCount();
	virtual void GetNamespace(unsigned index, const uni_char *&uri, const uni_char *&prefix);
};

class XMLFallbackTreeAccessor
	: public XMLTreeAccessor
{
private:
	XMLTreeAccessor *tree;

	virtual BOOL IsCaseSensitive();

	virtual NodeType GetNodeType(Node *node);
	virtual Node *GetRoot();

	virtual Node *GetRootElement();
	virtual Node *GetParent(Node *from);
	virtual Node *GetAncestor(Node *from);
	virtual Node *GetFirstChild(Node *from);
	virtual Node *GetLastChild(Node *from);
	virtual Node *GetNextSibling(Node *from);
	virtual Node *GetPreviousSibling(Node *from);
	virtual Node *GetNext(Node *from);
	virtual Node *GetNextDescendant(Node *from, Node *ancestor);
	virtual Node *GetNextNonDescendant(Node *from);
	virtual Node *GetPrevious(Node *from);
	virtual Node *GetPreviousNonAncestor(Node *from, Node *origin);

	virtual BOOL IsAncestorOf(Node *parent, Node *child);
	virtual BOOL IsEmptyText(Node *node);
	virtual BOOL IsWhitespaceOnly(Node *node);
	virtual BOOL Precedes(Node *node1, Node *node2);

	struct Filter
	{
		Filter()
			: disabled(0),
			  stop_at(0),
			  nodetype(static_cast<XMLTreeAccessor::NodeType>(-1)),
			  has_elementname(FALSE),
			  has_attributename(FALSE),
			  attributevalue(0)
		{
		}

		unsigned disabled;
		Node *stop_at;
		NodeType nodetype;
		BOOL join_text_and_cdata_section;
		BOOL has_elementname;
		XMLExpandedName elementname;
		BOOL has_attributename;
		XMLExpandedName attributename;
		const uni_char *attributevalue;
		OpString attributevalue_storage;
	} filter;

	virtual void SetStopAtFilter(Node *node);
	virtual void SetNodeTypeFilter(NodeType nodetype, BOOL join_text_and_cdata_section);
	virtual OP_STATUS SetElementNameFilter(const XMLExpandedName &name, BOOL copy = TRUE, unsigned *data1 = NULL, unsigned *data2 = NULL);
	virtual OP_STATUS SetAttributeNameFilter(const XMLExpandedName &name, BOOL copy = TRUE, unsigned *data1 = NULL, unsigned *data2 = NULL);
	virtual OP_STATUS SetAttributeValueFilter(const uni_char *value, BOOL copy = TRUE);
	virtual void ResetFilters();
	virtual BOOL FilterNode(Node *node);
	BOOL FilterNodeInternal(Node *node);

	virtual void GetName(XMLCompleteName &name, Node *node);
	virtual const uni_char* GetPITarget(Node *node);

	virtual void GetAttributes(Attributes *&attributes, Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations);
	virtual OP_STATUS GetAttribute(Node *node, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer);
	virtual OP_STATUS GetAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer);
	virtual unsigned GetAttributeIndex(Node *node, const XMLExpandedName &name);
	virtual unsigned GetAttributeIndex(Attributes *attributes, const XMLExpandedName &name);
	virtual BOOL HasAttribute(Node *node, const XMLExpandedName &name, const uni_char *value = NULL);
	virtual BOOL HasAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *value = NULL);

	XMLFallbackTreeAccessor_Namespaces namespaces;
	virtual OP_STATUS GetNamespaces(Namespaces *&namespaces, Node *node);
	virtual unsigned GetNamespaceIndex(Node *node, const uni_char *uri, const uni_char *prefix);

	virtual OP_STATUS GetData(const uni_char *&data, Node *node, TempBuffer *buffer);
	virtual OP_STATUS GetCharacterDataContent(const uni_char *&data, Node *node, TempBuffer *buffer);

	OpGenericStringHashTable idtable;
	BOOL idtable_built;
	virtual OP_STATUS GetElementById(Node *&node, const uni_char *id);

	virtual URL GetDocumentURL();
	virtual const XMLDocumentInformation *GetDocumentInformation();

public:
	XMLFallbackTreeAccessor(XMLTreeAccessor *tree)
		: tree(tree),
		  idtable(TRUE),
		  idtable_built(FALSE)
	{
	}

	virtual ~XMLFallbackTreeAccessor();
};

/* virtual */ BOOL
XMLFallbackTreeAccessor::IsCaseSensitive()
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return TRUE;
}

/* virtual */ XMLTreeAccessor::NodeType
XMLFallbackTreeAccessor::GetNodeType(Node *node)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return TYPE_ROOT;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetRoot()
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetRootElement()
{
	Node *root = tree->GetRoot();
	if (root)
	{
		++filter.disabled;
		Node *child = tree->GetFirstChild(root);
		while (child)
			if (tree->GetNodeType(child) == TYPE_ELEMENT)
			{
				--filter.disabled;
				return child;
			}
			else
				child = tree->GetNextSibling(child);
		--filter.disabled;
	}
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetParent(XMLTreeAccessor::Node *from)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetAncestor(XMLTreeAccessor::Node *from)
{
	++filter.disabled;
	Node *iter = tree->GetParent(from);
	while (iter)
		if (iter == filter.stop_at)
			break;
		else if (XMLFallbackTreeAccessor::FilterNodeInternal(iter))
		{
			--filter.disabled;
			return iter;
		}
		else
			iter = tree->GetParent(iter);
	--filter.disabled;
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetFirstChild(XMLTreeAccessor::Node *from)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetLastChild(XMLTreeAccessor::Node *from)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetNextSibling(XMLTreeAccessor::Node *from)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetPreviousSibling(XMLTreeAccessor::Node *from)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetNext(XMLTreeAccessor::Node *from)
{
	Node *iter = from, *next;
	++filter.disabled;
	while (iter)
		if ((next = tree->GetFirstChild(iter)) != 0 ||
			(next = tree->GetNextSibling(iter)) != 0)
			if (next == filter.stop_at)
			{
				--filter.disabled;
				return 0;
			}
			else if (XMLFallbackTreeAccessor::FilterNodeInternal(next))
			{
				--filter.disabled;
				return next;
			}
			else
				iter = next;
		else if ((iter = XMLFallbackTreeAccessor::GetNextNonDescendant(iter)) != 0)
		{
			--filter.disabled;
			return iter;
		}
	--filter.disabled;
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetNextDescendant(XMLTreeAccessor::Node *from, XMLTreeAccessor::Node *ancestor)
{
	OP_ASSERT(XMLFallbackTreeAccessor::IsAncestorOf(ancestor, from));
	++filter.disabled;
	Node *iter = from, *stop = XMLFallbackTreeAccessor::GetNextNonDescendant(ancestor);
	while ((iter = XMLFallbackTreeAccessor::GetNext(iter)) != stop)
		if (iter == filter.stop_at)
			break;
		else if (XMLFallbackTreeAccessor::FilterNodeInternal(iter))
		{
			--filter.disabled;
			return iter;
		}
	filter.disabled = FALSE;
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetNextNonDescendant(XMLTreeAccessor::Node *from)
{
	Node *iter = from;
	++filter.disabled;
	while (iter)
		if (Node *next = tree->GetNextSibling(iter))
		{
			--filter.disabled;
			if (next == filter.stop_at)
				return 0;
			else if (XMLFallbackTreeAccessor::FilterNodeInternal(next))
				return next;
			else
				return XMLFallbackTreeAccessor::GetNext(next);
		}
		else if ((iter = tree->GetParent(iter)) == filter.stop_at)
			break;
	--filter.disabled;
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetPrevious(XMLTreeAccessor::Node *from)
{
	Node *iter = from;
	++filter.disabled;
	while (iter)
	{
		if (Node *sibling = tree->GetPreviousSibling(iter))
			if (sibling == filter.stop_at)
				break;
			else
			{
				while (Node *child = tree->GetLastChild(sibling))
					if (child == filter.stop_at)
					{
						--filter.disabled;
						return 0;
					}
					else
						sibling = child;
				iter = sibling;
			}
		else
		{
			iter = tree->GetParent(iter);
			if (iter == filter.stop_at)
				break;
		}
		if (XMLFallbackTreeAccessor::FilterNodeInternal(iter))
		{
			--filter.disabled;
			return iter;
		}
	}
	--filter.disabled;
	return 0;
}

/* virtual */ XMLTreeAccessor::Node *
XMLFallbackTreeAccessor::GetPreviousNonAncestor(XMLTreeAccessor::Node *from, XMLTreeAccessor::Node *origin)
{
	Node *iter = from;
	while ((iter = XMLFallbackTreeAccessor::GetPrevious(iter)) != 0)
		if (!XMLFallbackTreeAccessor::IsAncestorOf(iter, origin))
			return iter;
	return 0;
}

/* virtual */ BOOL
XMLFallbackTreeAccessor::IsAncestorOf(XMLTreeAccessor::Node *parent, XMLTreeAccessor::Node *child)
{
	OP_ASSERT(parent);
	OP_ASSERT(child);
	++filter.disabled;
	while (child && child != parent)
		child = tree->GetParent(child);
	--filter.disabled;
	return child == parent;
}

/* virtual */ BOOL
XMLFallbackTreeAccessor::IsEmptyText(XMLTreeAccessor::Node *node)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return FALSE;
}

/* virtual */ BOOL
XMLFallbackTreeAccessor::IsWhitespaceOnly(XMLTreeAccessor::Node *node)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return FALSE;
}

static unsigned
XMLFallbackTreeAccessor_Level(XMLTreeAccessor *tree, XMLTreeAccessor::Node *node)
{
	unsigned level = 1;
	while ((node = tree->GetParent(node)) != NULL)
		++level;
	return level;
}

/* virtual */ BOOL
XMLFallbackTreeAccessor::Precedes(XMLTreeAccessor::Node *node1, XMLTreeAccessor::Node *node2)
{
	++filter.disabled;

	unsigned level1 = XMLFallbackTreeAccessor_Level(tree, node1), level2 = XMLFallbackTreeAccessor_Level(tree, node2);

	while (level1 - 1 > level2)
		node1 = tree->GetParent(node1), --level1;
	while (level2 - 1 > level1)
		node2 = tree->GetParent(node2), --level2;

	Node *parent1 = tree->GetParent(node1), *parent2 = tree->GetParent(node2);

	if (level1 == level2 + 1 && parent1 == node2)
		goto return_FALSE;
	if (level2 == level1 + 1 && parent2 == node1)
		goto return_TRUE;

	if (level1 > level2)
	{
		node1 = parent1;
		parent1 = tree->GetParent(node1);
	}
	if (level2 > level1)
	{
		node2 = parent2;
		parent2 = tree->GetParent(node2);
	}

	while (parent1 != parent2)
	{
		node1 = parent1;
		parent1 = tree->GetParent(node1);
		node2 = parent2;
		parent2 = tree->GetParent(node2);
	}

	while ((node1 = tree->GetPreviousSibling(node1)) != 0)
		if (node1 == node2)
			goto return_FALSE;

 return_TRUE:
	--filter.disabled;
	return TRUE;

 return_FALSE:
	--filter.disabled;
	return FALSE;
}

/* virtual */ void
XMLFallbackTreeAccessor::SetStopAtFilter(XMLTreeAccessor::Node *node)
{
	filter.stop_at = node;
}

/* virtual */ void
XMLFallbackTreeAccessor::SetNodeTypeFilter(NodeType nodetype, BOOL join_text_and_cdata_section)
{
	filter.nodetype = nodetype;
	filter.join_text_and_cdata_section = join_text_and_cdata_section;
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::SetElementNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2)
{
	filter.has_elementname = TRUE;
	if (copy)
		return filter.elementname.Set(name);
	else
	{
		filter.elementname = name;
		return OpStatus::OK;
	}
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::SetAttributeNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2)
{
	filter.has_attributename = TRUE;
	if (copy)
		return filter.attributename.Set(name);
	else
	{
		filter.attributename = name;
		return OpStatus::OK;
	}
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::SetAttributeValueFilter(const uni_char *value, BOOL copy)
{
	if (copy)
	{
		RETURN_IF_ERROR(filter.attributevalue_storage.Set(value));
		filter.attributevalue = filter.attributevalue_storage.CStr();
	}
	else
	{
		filter.attributevalue_storage.Empty();
		filter.attributevalue = value;
	}
	return OpStatus::OK;
}

/* virtual */ void
XMLFallbackTreeAccessor::ResetFilters()
{
	filter.stop_at = 0;
	filter.nodetype = static_cast<NodeType>(-1);
	filter.has_elementname = FALSE;
	filter.elementname = XMLExpandedName();
	filter.has_attributename = FALSE;
	filter.attributename = XMLExpandedName();
	filter.attributevalue = 0;
	filter.attributevalue_storage.Empty();
}

static BOOL
XMLFallbackTreeAccessor_CompareNames(const XMLCompleteName &actual, const XMLExpandedName &filter)
{
	if (filter.GetLocalPart() && *filter.GetLocalPart())
		if (!uni_str_eq(actual.GetLocalPart(), filter.GetLocalPart()))
			return FALSE;
	if (filter.GetUri() && *filter.GetUri())
		if (!actual.GetUri() || !uni_str_eq(actual.GetUri(), filter.GetUri()))
			return FALSE;
	return TRUE;
}

/* virtual */ BOOL
XMLFallbackTreeAccessor::FilterNode(XMLTreeAccessor::Node *node)
{
	return filter.disabled > 0 || FilterNodeInternal(node);
}

BOOL
XMLFallbackTreeAccessor::FilterNodeInternal(XMLTreeAccessor::Node *node)
{
	if (filter.disabled > 1)
		return TRUE;

	XMLTreeAccessor::NodeType nodetype = tree->GetNodeType(node);
	if (filter.nodetype != static_cast<NodeType>(-1) && nodetype != filter.nodetype)
		return FALSE;
	if (filter.has_elementname)
	{
		if (nodetype == XMLTreeAccessor::TYPE_ELEMENT)
		{
			XMLCompleteName name;
			tree->GetName(name, node);
			if (!XMLFallbackTreeAccessor_CompareNames(name, filter.elementname))
				return FALSE;
		}
		else
			return FALSE;
	}
	if (filter.has_attributename || filter.attributevalue)
	{
		if (nodetype == XMLTreeAccessor::TYPE_ELEMENT)
		{
			TempBuffer buffer, *bufferp = filter.attributevalue ? &buffer : 0;
			Attributes *attributes;
			BOOL found = FALSE;
			tree->GetAttributes(attributes, node, FALSE, TRUE);
			for (unsigned index = 0, count = attributes->GetCount(); index < count; ++index)
			{
				XMLCompleteName name;
				const uni_char *value;
				BOOL id, specified;
				if (OpStatus::IsSuccess(attributes->GetAttribute(index, name, value, id, specified, bufferp)))
					if (!filter.has_attributename || XMLFallbackTreeAccessor_CompareNames(name, filter.attributename))
						if (!filter.attributevalue || uni_str_eq(value, filter.attributevalue))
						{
							found = TRUE;
							break;
						}
				buffer.Clear ();
			}
			if (!found)
				return FALSE;
		}
		else
			return FALSE;
	}
	return TRUE;
}

/* virtual */ void
XMLFallbackTreeAccessor::GetName(XMLCompleteName &name, Node *node)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
}

/* virtual */ const uni_char *
XMLFallbackTreeAccessor::GetPITarget(Node *node)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */ void
XMLFallbackTreeAccessor::GetAttributes(Attributes *&attributes, Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::GetAttribute(XMLTreeAccessor::Node *node, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
	Attributes *attributes;
	tree->GetAttributes(attributes, node, FALSE, TRUE);
	return GetAttribute(attributes, name, value, id, specified, buffer);
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::GetAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
	for (unsigned index = 0, count = attributes->GetCount(); index < count; ++index)
	{
		XMLCompleteName actual;
		RETURN_IF_ERROR(attributes->GetAttribute(index, actual, value, id, specified, buffer));
		if (name == actual)
			return OpStatus::OK;
		else if (buffer)
			buffer->Clear();
	}
	return OpStatus::ERR;
}

/* virtual */ unsigned
XMLFallbackTreeAccessor::GetAttributeIndex(XMLTreeAccessor::Node *node, const XMLExpandedName &name)
{
	Attributes *attributes;
	tree->GetAttributes(attributes, node, FALSE, TRUE);
	return GetAttributeIndex(attributes, name);
}

/* virtual */ unsigned
XMLFallbackTreeAccessor::GetAttributeIndex(Attributes *attributes, const XMLExpandedName &name)
{
	for (unsigned index = 0, count = attributes->GetCount(); index < count; ++index)
	{
		XMLCompleteName actual;
		const uni_char *value;
		BOOL id, specified;
		RETURN_VALUE_IF_ERROR(attributes->GetAttribute(index, actual, value, id, specified, NULL), 0);
		if (name == actual)
			return index;
	}
	OP_ASSERT(!"Invalid use: asked-for attribute must exist!");
	return 0;
}

/* virtual */ BOOL
XMLFallbackTreeAccessor::HasAttribute(XMLTreeAccessor::Node *node, const XMLExpandedName &name, const uni_char *value)
{
	Attributes *attributes;
	tree->GetAttributes(attributes, node, FALSE, TRUE);
	return HasAttribute(attributes, name, value);
}

/* virtual */ BOOL
XMLFallbackTreeAccessor::HasAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *value)
{
	TempBuffer buffer, *bufferp = value ? &buffer : 0;
	for (unsigned index = 0, count = attributes->GetCount(); index < count; ++index)
	{
		XMLCompleteName actual_name;
		const uni_char *actual_value;
		BOOL id, specified;
		if (OpStatus::IsSuccess(attributes->GetAttribute(index, actual_name, actual_value, id, specified, bufferp)))
			if (name == actual_name && (!value || uni_str_eq(value, actual_value)))
				return TRUE;
		buffer.Clear ();
	}
	return FALSE;
}

/* virtual */ unsigned
XMLFallbackTreeAccessor_Namespaces::GetCount()
{
	return XMLNamespaceDeclaration::CountDeclaredPrefixes(nsdeclaration) + (XMLNamespaceDeclaration::FindDefaultDeclaration(nsdeclaration) != 0 ? 1 : 0);
}

/* virtual */ void
XMLFallbackTreeAccessor_Namespaces::GetNamespace(unsigned index, const uni_char *&uri, const uni_char *&prefix)
{
	XMLNamespaceDeclaration *declaration;
	if ((declaration = XMLNamespaceDeclaration::FindDeclarationByIndex(nsdeclaration, index)) != 0 ||
	    (declaration = XMLNamespaceDeclaration::FindDefaultDeclaration(nsdeclaration)) != 0)
	{
		uri = declaration->GetUri();
		prefix = declaration->GetPrefix();
	}
}

static OP_STATUS
XMLFallbackTreeAccessor_AddNamespaces(XMLNamespaceDeclaration::Reference &nsdeclaration, XMLTreeAccessor *tree, XMLTreeAccessor::Node *node)
{
	if (XMLTreeAccessor::Node *parent = tree->GetParent(node))
		RETURN_IF_ERROR(XMLFallbackTreeAccessor_AddNamespaces(nsdeclaration, tree, parent));

	if (tree->GetNodeType(node) == XMLTreeAccessor::TYPE_ELEMENT)
	{
		TempBuffer buffer;
		XMLTreeAccessor::Attributes *attributes;
		tree->GetAttributes(attributes, node, TRUE, FALSE);
		for (unsigned index = 0, count = attributes->GetCount(); index < count; ++index)
		{
			XMLCompleteName name;
			const uni_char *value;
			BOOL id, specified;
			RETURN_IF_ERROR(attributes->GetAttribute(index, name, value, id, specified, &buffer));
			RETURN_IF_ERROR(XMLNamespaceDeclaration::ProcessAttribute(nsdeclaration, name, value, uni_strlen(value), 0));
			buffer.Clear();
		}
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::GetNamespaces(Namespaces *&namespaces0, Node *node)
{
	if (node != namespaces.node)
	{
		namespaces.node = node;
		namespaces.nsdeclaration = (XMLNamespaceDeclaration *) 0;
		++filter.disabled;
		OP_STATUS status = XMLFallbackTreeAccessor_AddNamespaces(namespaces.nsdeclaration, tree, node);
		--filter.disabled;
		RETURN_IF_ERROR(status);
	}

	namespaces0 = &namespaces;
	return OpStatus::OK;
}

/* virtual */ unsigned
XMLFallbackTreeAccessor::GetNamespaceIndex(Node *node, const uni_char *uri, const uni_char *prefix)
{
	Namespaces *namespaces;
	if (OpStatus::IsSuccess(GetNamespaces(namespaces, node)))
		for (unsigned index = 0, count = namespaces->GetCount(); index < count; ++index)
		{
			const uni_char *actual_uri, *actual_prefix;
			namespaces->GetNamespace(index, actual_uri, actual_prefix);
			if (uni_str_eq(actual_uri, uri) && (actual_prefix == prefix || actual_prefix && prefix && uni_str_eq(actual_prefix, prefix)))
				return index;
		}
	OP_ASSERT(!"Invalid use: asked-for namespace must exist!");
	return 0;
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::GetData(const uni_char *&data, Node *node, TempBuffer *buffer)
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return OpStatus::ERR;
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::GetCharacterDataContent(const uni_char *&data, Node *node, TempBuffer *buffer)
{
	OP_ASSERT(node);

	++filter.disabled;

	Node *iter = node;
	Node *stop = NULL;
	while (iter)
	{
		Node *sibling = tree->GetNextSibling(iter);
		if (sibling)
		{
			stop = sibling;
			break;
		}
		iter = tree->GetParent(iter);
	}

	data = NULL;
	while (node != stop)
	{
		NodeType type = tree->GetNodeType(node);
		if (type == TYPE_TEXT || type == TYPE_CDATA_SECTION)
		{
			if (data)
			{
				if (!buffer)
				{
					data = NULL; // Signal that a buffer was needed
					--filter.disabled;
					return OpStatus::OK;
				}
				else if (data != buffer->GetStorage())
					if (OpStatus::IsMemoryError(buffer->Append(data)))
					{
						--filter.disabled;
						return OpStatus::ERR_NO_MEMORY;
					}
			}

			if (OpStatus::IsMemoryError(tree->GetData(data, node, buffer)))
			{
				--filter.disabled;
				return OpStatus::ERR_NO_MEMORY;
			}
			else if (!data)
			{
				OP_ASSERT(!buffer); // Only reason for data to be NULL
				--filter.disabled;
				return OpStatus::OK;
			}
		}

		Node *iter = node;

		while (iter)
		{
			Node *next = tree->GetFirstChild(iter);
			if (next)
			{
				node = next;
				goto process_node;
			}

			while (iter)
			{
				next = tree->GetNextSibling(iter);
				if (next)
				{
					node = next;
					goto process_node;
				}
				iter = tree->GetParent(iter);
			}
			if (!iter)
				node = NULL;
		}

	process_node:
		;
	}

	if (!data)
		data = UNI_L("");

	--filter.disabled;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLFallbackTreeAccessor::GetElementById(Node *&node, const uni_char *id)
{
	if (!idtable_built)
	{
		Node *iter = tree->GetRoot();
		while (iter)
		{
			if (tree->GetNodeType(iter) == XMLTreeAccessor::TYPE_ELEMENT)
			{
				Attributes *attributes;
				tree->GetAttributes(attributes, iter, FALSE, TRUE);
				for (unsigned index = 0, count = attributes->GetCount(); index < count; ++index)
				{
					XMLCompleteName name;
					const uni_char *value;
					BOOL is_id, specified;
					OpStatus::Ignore(attributes->GetAttribute(index, name, value, is_id, specified, 0));
					if (is_id)
					{
						if (value && OpStatus::IsMemoryError(idtable.Add(value, iter)))
							return OpStatus::ERR_NO_MEMORY;
						break;
					}
				}
			}

			Node *next;

			++filter.disabled;

			if ((next = tree->GetFirstChild(iter)) != 0 ||
				(next = tree->GetNextSibling(iter)) != 0)
				iter = next;
			else
				while ((iter = tree->GetParent(iter)) != 0)
					if ((next = tree->GetNextSibling(iter)) != 0)
					{
						iter = next;
						break;
					}

			--filter.disabled;
		}
	}

	void *data;
	if (idtable.GetData(id, &data) == OpStatus::OK)
		node = static_cast<XMLTreeAccessor::Node *>(data);
	else
		node = 0;

	return OpStatus::OK;
}

/* virtual */ URL
XMLFallbackTreeAccessor::GetDocumentURL()
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return URL();
}

/* virtual */ const XMLDocumentInformation *
XMLFallbackTreeAccessor::GetDocumentInformation()
{
	OP_ASSERT(!"Invalid use: this function cannot be called!");
	return 0;
}

/* virtual */
XMLFallbackTreeAccessor::~XMLFallbackTreeAccessor()
{
}

/* virtual */ XMLTreeAccessor::Node *
XMLTreeAccessor::GetRootElement()
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetRootElement();
}

/* virtual */ XMLTreeAccessor::Node *
XMLTreeAccessor::GetAncestor(Node *from)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetAncestor(from);
}

/* virtual */ XMLTreeAccessor::Node *
XMLTreeAccessor::GetNext(Node *from)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetNext(from);
}

/* virtual */ XMLTreeAccessor::Node *
XMLTreeAccessor::GetNextDescendant(Node *from, Node *ancestor)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetNextDescendant(from, ancestor);
}

/* virtual */ XMLTreeAccessor::Node *
XMLTreeAccessor::GetNextNonDescendant(Node *from)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetNextNonDescendant(from);
}

/* virtual */ XMLTreeAccessor::Node *
XMLTreeAccessor::GetPrevious(Node *from)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetPrevious(from);
}

/* virtual */ XMLTreeAccessor::Node *
XMLTreeAccessor::GetPreviousNonAncestor(Node *from, Node *origin)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetPreviousNonAncestor(from, origin);
}

/* virtual */ BOOL
XMLTreeAccessor::IsAncestorOf(Node *parent, Node *child)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->IsAncestorOf(parent, child);
}

/* virtual */ BOOL
XMLTreeAccessor::Precedes(Node *node1, Node *node2)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->Precedes(node1, node2);
}

/* virtual */ void
XMLTreeAccessor::SetStopAtFilter(Node *node)
{
	OP_ASSERT(fallback_impl);
	fallback_impl->SetStopAtFilter(node);
}

/* virtual */ void
XMLTreeAccessor::SetNodeTypeFilter(NodeType nodetype, BOOL join_text_and_cdata_section)
{
	OP_ASSERT(fallback_impl);
	fallback_impl->SetNodeTypeFilter(nodetype, join_text_and_cdata_section);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::SetElementNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->SetElementNameFilter(name, copy, data1, data2);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::SetAttributeNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->SetAttributeNameFilter(name, copy, data1, data2);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::SetAttributeValueFilter(const uni_char *value, BOOL copy)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->SetAttributeValueFilter(value, copy);
}

/* virtual */ void
XMLTreeAccessor::ResetFilters()
{
	OP_ASSERT(fallback_impl);
	fallback_impl->ResetFilters();
}

/* virtual */ BOOL
XMLTreeAccessor::FilterNode (Node *node)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->FilterNode(node);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::GetAttribute(Node *node, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetAttribute(node, name, value, id, specified, buffer);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::GetAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetAttribute(attributes, name, value, id, specified, buffer);
}

/* virtual */ unsigned
XMLTreeAccessor::GetAttributeIndex(Node *node, const XMLExpandedName &name)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetAttributeIndex(node, name);
}

/* virtual */ unsigned
XMLTreeAccessor::GetAttributeIndex(Attributes *attributes, const XMLExpandedName &name)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetAttributeIndex(attributes, name);
}

/* virtual */ BOOL
XMLTreeAccessor::HasAttribute(Node *node, const XMLExpandedName &name, const uni_char *value)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->HasAttribute(node, name, value);
}

/* virtual */ BOOL
XMLTreeAccessor::HasAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *value)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->HasAttribute(attributes, name, value);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::GetNamespaces(Namespaces *&namespaces, Node *node)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetNamespaces(namespaces, node);
}

/* virtual */ unsigned
XMLTreeAccessor::GetNamespaceIndex(Node *node, const uni_char *uri, const uni_char *prefix)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetNamespaceIndex(node, uri, prefix);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::GetCharacterDataContent(const uni_char *&data, Node *node, TempBuffer *buffer)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetCharacterDataContent(data, node, buffer);
}

/* virtual */ OP_STATUS
XMLTreeAccessor::GetElementById(Node *&node, const uni_char *id)
{
	OP_ASSERT(fallback_impl);
	return fallback_impl->GetElementById(node, id);
}

/* virtual */
XMLTreeAccessor::~XMLTreeAccessor()
{
	XMLFallbackTreeAccessor *fallback = static_cast<XMLFallbackTreeAccessor *>(fallback_impl);
	OP_DELETE(fallback);
}

OP_STATUS
XMLTreeAccessor::ConstructFallbackImplementation()
{
	fallback_impl = OP_NEW(XMLFallbackTreeAccessor, (this));
	return fallback_impl ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#else // XMLUTILS_XMLTREEACCESSOR_FALLBACK_SUPPORT

/* virtual */
XMLTreeAccessor::~XMLTreeAccessor()
{
}

#endif // XMLUTILS_XMLTREEACCESSOR_FALLBACK_SUPPORT

/* virtual */ XMLCompleteName
XMLTreeAccessor::GetName(Node *node)
{
	XMLCompleteName name;
	GetName(name, node);
	return name;
}

/* virtual */ XMLTreeAccessor::Attributes *
XMLTreeAccessor::GetAttributes(Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations)
{
	XMLTreeAccessor::Attributes *attributes;
	GetAttributes(attributes, node, ignore_default, ignore_nsdeclarations);
	return attributes;
}

#endif // XMLUTILS_XMLTREEACCESSOR_SUPPORT
