/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef M2_UTIL_XMLPARSERTRACKER
#define M2_UTIL_XMLPARSERTRACKER

#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"

namespace M2XMLUtils
{
	class XMLParserTracker;

	//****************************************************************************
	//
	//	XMLAttributeQN
	//
	//****************************************************************************

	class XMLAttributeQN
	{
	public:
		// Construction / destruction.
		XMLAttributeQN();
		virtual ~XMLAttributeQN();

		OP_STATUS Init(const OpStringC& namespace_uri, const OpStringC& local_name,
			const OpStringC& qualified_name, const OpStringC& value);

		// Methods.
		const OpStringC& NamespaceURI() const { return m_namespace_uri; }
		const OpStringC& LocalName() const { return m_local_name; }
		const OpStringC& QualifiedName() const { return m_qualified_name; }
		const OpStringC& Value() const { return m_value; }

	private:
		// No copy or assignment.
		XMLAttributeQN(const XMLAttributeQN& attribute);
		XMLAttributeQN& operator=(const XMLAttributeQN& attribute);

		// Members.
		OpString m_namespace_uri;
		OpString m_local_name;
		OpString m_qualified_name;
		OpString m_value;
	};


	//****************************************************************************
	//
	//	XMLNamespacePrefix
	//
	//****************************************************************************

	class XMLNamespacePrefix
	{
	public:
		// Construction / destruction.
		XMLNamespacePrefix();
		~XMLNamespacePrefix();

		OP_STATUS Init(const OpStringC& prefix, const OpStringC& uri);

		// Methods.
		const OpStringC& Prefix() const { return m_prefix; }
		const OpStringC& URI() const { return m_uri; }

	private:
		// No copy or assignment.
		XMLNamespacePrefix(const XMLNamespacePrefix& prefix);
		XMLNamespacePrefix& operator=(const XMLNamespacePrefix& prefix);

		// Members.
		OpString m_prefix;
		OpString m_uri;
	};


	//****************************************************************************
	//
	//	XMLElement
	//
	//****************************************************************************

	class XMLElement
	{
	public:
		// Construction / destruction.
		XMLElement(XMLParserTracker& owner);
		~XMLElement();

		OP_STATUS Init(const OpStringC& namespace_uri, const OpStringC& local_name, const XMLToken::Attribute* attributes, UINT attributes_count);

		// Methods.
		const OpStringC& NamespaceURI() const { return m_namespace_uri; }
		const OpStringC& LocalName() const { return m_local_name; }
		const OpStringC& QualifiedName() const { return m_qualified_name; }
		OpAutoStringHashTable<XMLAttributeQN>& Attributes() { return m_attributes; }
		const OpAutoStringHashTable<XMLAttributeQN>& Attributes() const { return m_attributes; }

		BOOL HasBaseURI() const { return m_base_uri.HasContent(); }
		const OpStringC& BaseURI() const { return m_base_uri; }

	private:
		// No copy or assignment.
		XMLElement(const XMLElement& element);
		XMLElement& operator=(const XMLElement& element);

		// Members.
		XMLParserTracker& m_owner;

		OpString m_namespace_uri;
		OpString m_local_name;
		OpString m_qualified_name;
		OpAutoStringHashTable<XMLAttributeQN> m_attributes;

		OpString m_base_uri;
	};


	//****************************************************************************
	//
	//	XMLParserTracker
	//
	//****************************************************************************

	/*!	A helper class (ment to be derived from) for dealing with SAX parsing of
		XML. It will do stuff like:

		- Automatically store the content of all elements.
		- Let you add namespace aliases (example: let's you say that the namespace
		URI http://www.example.com/foobar/ always should be prefixed as foobar,
		even if the xml document you parse uses another alias).
		- Figure out the relationship an item has to another item (child, etc.).
		- Treat markup as content, so that you, for example, can keep xhtml markup
		inside an xml document.
		- Handle xml:base attributes, so that you can retrieve the base uri for
		any element.
	*/

	class XMLParserTracker
	:	public XMLTokenHandler
	{
	public:
		// Methods.
		BOOL HasNamespacePrefix(const OpStringC& uri) const;
		OP_STATUS AddNamespacePrefix(const OpStringC& prefix, const OpStringC& uri);
		OP_STATUS NamespacePrefix(OpString& prefix, const OpStringC& uri) const;
		OP_STATUS QualifiedName(OpString& qualified_name, const OpStringC& namespace_uri, const OpStringC& local_name) const;

		OP_STATUS Parse(const char* buffer, UINT buffer_length, BOOL is_final);
		OP_STATUS Parse(const uni_char *buffer, UINT buffer_length, BOOL is_final);

		BOOL HasParent() const { return m_element_stack.GetCount() > 1; }
		BOOL HasGrandparent() const { return m_element_stack.GetCount() > 2; }
		const OpStringC& ParentName() const;
		const OpStringC& GrandparentName() const;

		UINT AncestorCount() const { return MAX(m_element_stack.GetCount() - 1, 0); }
		BOOL IsChildOf(const OpStringC& parent_name) const;
		BOOL IsGrandchildOf(const OpStringC& grandparent_name) const;
		BOOL IsDescendantOf(const OpStringC& descendant_name) const;

		OP_STATUS SetKeepTags(const OpStringC& namespace_uri, const OpStringC& local_name, BOOL ignore_first_tag = FALSE);

		OP_STATUS BaseURI(OpString& base_uri, const OpStringC& document_uri = OpStringC()) const;

		OP_STATUS Version(OpString8& version) const;
		OP_STATUS Encoding(OpString8& encoding) const;

		const OpStringC ElementContent() const { return m_content_string.GetStorage(); }

	protected:
		// Constructor / destructor.
		XMLParserTracker();
		virtual ~XMLParserTracker();

		OP_STATUS Init();

		// Virtual functions to override.
		virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes) { return OpStatus::OK; }
		virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes) { return OpStatus::OK; }
		virtual OP_STATUS OnCharacterData(const OpStringC& data) { return OpStatus::OK; }
		virtual OP_STATUS OnStartCDATASection() { return OpStatus::OK; }
		virtual OP_STATUS OnEndCDATASection() { return OpStatus::OK; }

	private:
		// XMLTokenHandler.
		virtual Result HandleToken(XMLToken &token);

		// Methods.
		OP_STATUS HandleStartTagToken(const XMLToken& token);
		OP_STATUS HandleEndTagToken(const XMLToken& token);
		OP_STATUS HandleTextToken(const XMLToken& token);
		OP_STATUS HandleCDATAToken(const XMLToken& token);

		OP_STATUS ResetParser();

		BOOL ElementStackHasContent() const { return m_element_stack.GetCount() > 0; }
		BOOL IsKeepingTags() const { return m_keep_tags.get() != 0; }
		OP_STATUS PushElementStack(XMLElement* element);
		OP_STATUS PopElementStack();
		OP_STATUS TopElementStack(XMLElement*& element) const;

		// Internal class for ensuring that our element stack is always popped
		// when it should be.
		class ElementStackPopper
		{
		public:
			// Construction / destruction.
			ElementStackPopper(XMLParserTracker& parent) : m_parent(parent) { }
			~ElementStackPopper() { m_parent.PopElementStack(); }

		private:
			// No copy or assignment.
			ElementStackPopper(const ElementStackPopper& stack_popper);
			ElementStackPopper& operator=(const ElementStackPopper& stack_popper);

			// Members.
			XMLParserTracker& m_parent;
		};

		friend class ElementStackPopper;

		// Internal class for information about when we should keep tags.
		class KeepTags
		{
		public:
			// Construction.
			KeepTags(BOOL ignore_first_tag);
			OP_STATUS Init(const OpStringC& namespace_uri, const OpStringC& local_name);

			// Methods.
			BOOL IsEndTag(const OpStringC& namespace_uri, const OpStringC& local_name) const;
			BOOL KeepThisTag() const { return !(m_ignore_first_tag && m_tag_depth == 0); }

			UINT IncTagDepth() { return ++m_tag_depth; }
			UINT DecTagDepth();

		private:
			// No copy or assignment.
			KeepTags(const KeepTags& other);
			KeepTags& operator=(const KeepTags& other);

			// Members.
			UINT m_tag_depth;
			BOOL m_ignore_first_tag;
			OpString m_namespace_uri;
			OpString m_local_name;
		};

		// Members.
		OpAutoPtr<XMLParser> m_xml_parser;

		OpAutoStringHashTable<XMLNamespacePrefix> m_namespace_prefixes;
		OpAutoVector<XMLElement> m_element_stack;

		TempBuffer m_content_string;
		OpAutoPtr<KeepTags> m_keep_tags;
	};
};

#endif
