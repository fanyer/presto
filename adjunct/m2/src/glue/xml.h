// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef XML_H
#define XML_H

# include "modules/util/OpHashTable.h"

//****************************************************************************
//
//	XMLAttribute
//
//****************************************************************************

class XMLAttribute
{
public:
	// Construction / destruction.
	XMLAttribute() { }
	virtual ~XMLAttribute() { }

	OP_STATUS Init(const OpStringC& namespace_uri, const OpStringC& local_name,
		const OpStringC& value)
	{
		RETURN_IF_ERROR(m_namespace_uri.Set(namespace_uri));
		RETURN_IF_ERROR(m_local_name.Set(local_name));
		RETURN_IF_ERROR(m_value.Set(value));

		return OpStatus::OK;
	}

	// Methods.
	const OpStringC& NamespaceURI() const { return m_namespace_uri; }
	const OpStringC& LocalName() const { return m_local_name; }
	const OpStringC& Value() const { return m_value; }

private:
	// No copy or assignment.
	XMLAttribute(const XMLAttribute& attribute);
	XMLAttribute& operator=(const XMLAttribute& attribute);

	// Members.
	OpString m_namespace_uri;
	OpString m_local_name;
	OpString m_value;
};


// ***************************************************************************
//
//	M2XMLParser
//
// ***************************************************************************

class M2XMLParser
{
public:
	// Callback methods for the client.
    class Client
	{
	public:
		virtual OP_STATUS XMLDeclaration(const OpStringC& version, const OpStringC& encoding) = 0;

		virtual OP_STATUS StartElement(const OpStringC& namespace_uri, const OpStringC& name, OpAutoStringHashTable<XMLAttribute> &attributes) = 0;
		virtual OP_STATUS EndElement(const OpStringC& namespace_uri, const OpStringC& name, BOOL is_empty_element) = 0;
		virtual OP_STATUS CharData(const OpStringC& data) = 0;

		virtual OP_STATUS StartCDATASection() = 0;
		virtual OP_STATUS EndCDATASection() = 0;

		virtual OP_STATUS StartNamespaceDeclaration(const OpStringC& prefix, const OpStringC& uri) = 0;
		virtual OP_STATUS EndNamespaceDeclaration(const OpStringC& prefix) = 0;
	};

	// Must be implemented by the underlying xml parser.
	virtual OP_STATUS SetClient(Client* client) = 0;
	virtual OP_STATUS Parse(const char* buf, size_t len, BOOL is_final) = 0;
};

#endif // XML_H
