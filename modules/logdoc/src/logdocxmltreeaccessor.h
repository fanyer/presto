/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOGDOCXMLTREEACCESSOR_H
#define LOGDOCXMLTREEACCESSOR_H

#ifdef LOGDOCXMLTREEACCESSOR_SUPPORT

#include "modules/xmlutils/xmltreeaccessor.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/logdoc/htm_elm.h"

class LogicalDocument;
class HTML_Element;

class LogdocXMLTreeAccessor
	: public XMLTreeAccessor
{
protected:
	LogicalDocument *logdoc;
	HTML_Element *root;
	/**< The element supplied to the constructor, or the LogicalDocument's
	     HE_DOC_ROOT element if none was supplied.  Defines the subtree that
	     is accessed. */
	HTML_Element *rootnode;
	/**< The HE_DOC_ROOT element returned by GetRoot().  If 'root' is a
	     HE_DOC_ROOT element, this is it, otherwise it is a pointer to
	     'dummy_rootnode'. */
	HTML_Element *rootelement;
	/**< The element returned by GetRootElement() or NULL.  If 'root' is a
	     regular element, this is it.  If 'root' is a HE_DOC_ROOT element,
	     this is its first regular element child, or NULL if it has none. */
	HTML_Element dummy_rootnode;
	/**< Dummy element used for the root node if no suitable HE_DOC_ROOT
	     element is provided. */
	XMLDocumentInformation *docinfo;
	URL documenturl;

	HTML_Element *stop_at_filter;
	BOOL has_nodetype_filter, has_element_name_filter, has_attribute_name_filter, has_attribute_value_filter;
	int nodetype_filter, cdata_section_filter, element_name_filter_type, element_name_filter_nsindex;
	XMLExpandedName element_name_filter, attribute_name_filter;
	OpString attribute_value_filter;
	const uni_char *attribute_value_filter_cstr;

	BOOL IsIncludedByFilters(HTML_Element *element);

	virtual BOOL IsCaseSensitive();

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

	virtual void SetStopAtFilter(Node *node);
	virtual void SetNodeTypeFilter(NodeType nodetype, BOOL join_text_and_cdata_section);
	virtual OP_STATUS SetElementNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2);
	virtual OP_STATUS SetAttributeNameFilter(const XMLExpandedName &name, BOOL copy, unsigned *data1, unsigned *data2);
	virtual OP_STATUS SetAttributeValueFilter(const uni_char *value, BOOL copy = FALSE);
	virtual void ResetFilters();
	virtual BOOL FilterNode(Node *node);

	virtual NodeType GetNodeType(Node *node);
	virtual void GetName(XMLCompleteName &name, Node *node);
	virtual const uni_char* GetPITarget(Node *node);

	class LogdocAttributes
		: public XMLTreeAccessor::Attributes
	{
	protected:
		HTML_AttrIterator iterator;
		BOOL empty, ignore_default, ignore_nsdeclarations;
		HTML_Element* element;

	public:
		LogdocAttributes() : iterator(NULL) {}
		void Initialize(HTML_Element *element, BOOL ignore_default, BOOL ignore_nsdeclarations);

		virtual unsigned GetCount();
		virtual OP_STATUS GetAttribute(unsigned index, XMLCompleteName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer);
		BOOL IsCaseSensitive() { return element->GetNsIdx() != NS_IDX_HTML; }
	} attributes;

	virtual void GetAttributes(Attributes *&attributes, Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations);
	virtual OP_STATUS GetAttribute(Node *node, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer);
	virtual OP_STATUS GetAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer);
	virtual unsigned GetAttributeIndex(Node *node, const XMLExpandedName &name);
	virtual unsigned GetAttributeIndex(Attributes *attributes, const XMLExpandedName &name);
	virtual BOOL HasAttribute(Node *node, const XMLExpandedName &name, const uni_char *value);
	virtual BOOL HasAttribute(Attributes *attributes, const XMLExpandedName &name, const uni_char *value);

	class LogdocNamespaces
		: public XMLTreeAccessor::Namespaces
	{
	protected:
		HTML_Element *element;
		XMLNamespaceDeclaration::Reference nsdeclaration;
		BOOL empty;

	public:
		LogdocNamespaces()
			: element(NULL)
		{
		}

		OP_STATUS Initialize(HTML_Element *element);

		virtual unsigned GetCount();
		virtual void GetNamespace(unsigned index, const uni_char *&uri, const uni_char *&prefix);
	};

	LogdocNamespaces namespaces;

	virtual OP_STATUS GetNamespaces(Namespaces *&namespaces, Node *node);
	virtual unsigned GetNamespaceIndex(Node *node, const uni_char *uri, const uni_char *prefix);

	virtual OP_STATUS GetData(const uni_char *&data, Node *node, TempBuffer *buffer);
	virtual OP_STATUS GetCharacterDataContent(const uni_char *&data, Node *node, TempBuffer *buffer);
	virtual OP_STATUS GetElementById(Node *&node, const uni_char *id);
	virtual URL GetDocumentURL();
	virtual const uni_char *GetDocumentURI();
	virtual const XMLDocumentInformation *GetDocumentInformation();

public:
	LogdocXMLTreeAccessor(LogicalDocument *logdoc, HTML_Element *element = NULL);
	virtual ~LogdocXMLTreeAccessor();

	OP_STATUS SetDocumentInformation(const XMLDocumentInformation *docinfo);
	void SetDocumentURL(URL url);

	static Node *GetElementAsNode(HTML_Element *element);
	static HTML_Element *GetNodeAsElement(Node *treenode);
};

#endif // LOGDOCXMLTREEACCESSOR_SUPPORT
#endif // LOGDOCXMLTREEACCESSOR_H
