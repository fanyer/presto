// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.
// It may not be distributed under any circumstances.

#ifndef _XMLELEMENT_H_
#define _XMLELEMENT_H_

#include "modules/util/OpHashTable.h"
#include "modules/xmlutils/xmltoken.h"

class WebFeedParser;

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
//	XMLElement
//
//****************************************************************************

class XMLElement
{
public:
	// Construction / destruction.
	XMLElement(WebFeedParser& owner);
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
	WebFeedParser& m_owner;

	OpString m_namespace_uri;
	OpString m_local_name;
	OpString m_qualified_name;
	OpAutoStringHashTable<XMLAttributeQN> m_attributes;

	OpString m_base_uri;
};

#endif //_XMLELEMENT_H_
