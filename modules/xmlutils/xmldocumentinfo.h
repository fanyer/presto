/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLDOCUMENTINFO_H
#define XMLDOCUMENTINFO_H

#include "modules/xmlutils/xmltypes.h"

class XMLDoctype;

class XMLDocumentInformation
{
public:
	XMLDocumentInformation();
	~XMLDocumentInformation();

	BOOL GetXMLDeclarationPresent() const { return xml_declaration_present; }
	XMLVersion GetVersion() const { return version; }
	XMLStandalone GetStandalone() const { return standalone; }
	const uni_char *GetEncoding() const { return encoding; }

	BOOL GetDoctypeDeclarationPresent() const { return doctype_declaration_present; }
	const uni_char *GetDoctypeName() const { return doctype_name; }
	const uni_char *GetPublicId() const { return public_id; }
	const uni_char *GetSystemId() const { return system_id; }
	const uni_char *GetInternalSubset() const { return internal_subset; }

	BOOL IsXHTML() const;
#ifdef SVG_SUPPORT
	BOOL IsSVG() const;
#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_
	BOOL IsWML() const;
#endif // _WML_SUPPORT_

	const uni_char *GetResolvedSystemId() const;
	/**< Get the system identifier that is known to represent the
	     declared public identifier, or the declared system identifier
	     if no such system identifier is known. */

	const uni_char *GetRootElementName() const;
	/**< Returns the normal root element name for the document type
	     ("html" for the XHTML document type, for instance) or NULL if
	     no such element name is known. */

	const uni_char *GetDefaultNamespaceURI() const;
	/**< If the document type is known, and it declares an attribute
	     named "xmlns" on the normal root element with a default
	     value, returns that default value, otherwise returns NULL. */

	unsigned GetKnownDefaultAttributeCount(const uni_char *qname, unsigned qname_length = ~0u) const;
	void GetKnownDefaultAttribute(unsigned index, const uni_char *&qname, const uni_char *&value, BOOL &fixed) const;

	static BOOL IsXHTML(const uni_char *public_id, const uni_char *system_id);
#ifdef SVG_SUPPORT
	static BOOL IsSVG(const uni_char *public_id, const uni_char *system_id);
#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_
	static BOOL IsWML(const uni_char *public_id, const uni_char *system_id);
#endif // _WML_SUPPORT_

	static const uni_char *GetResolvedSystemId(const uni_char *public_id, const uni_char *system_id);
	static const uni_char *GetDefaultNamespaceURI(const uni_char *public_id, const uni_char *system_id);
	static const uni_char *GetRootElementName(const uni_char *public_id, const uni_char *system_id);

	static unsigned GetKnownDefaultAttributeCount(const uni_char *public_id, const uni_char *system_id, const uni_char *qname, unsigned qname_length = ~0u);
	static void GetKnownDefaultAttribute(const uni_char *public_id, const uni_char *system_id, unsigned index, const uni_char *&qname, const uni_char *&value, BOOL &fixed);

	OP_STATUS Copy(const XMLDocumentInformation &information);
	OP_STATUS SetXMLDeclaration(XMLVersion version, XMLStandalone standalone, const uni_char *encoding);
	OP_STATUS SetDoctypeDeclaration(const uni_char *doctype_name, const uni_char *public_id, const uni_char *system_id, const uni_char *internal_subset);

	XMLDoctype *GetDoctype() const { return doctype; }
	void SetDoctype(XMLDoctype *doctype);

protected:
	BOOL xml_declaration_present;
	XMLVersion version;
	XMLStandalone standalone;
	uni_char *encoding;

	BOOL doctype_declaration_present;
	uni_char *doctype_name, *public_id, *system_id, *internal_subset;

	XMLDoctype *doctype;
};

#endif // XMLDOCUMENTINFO_H
