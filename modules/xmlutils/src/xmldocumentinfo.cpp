/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/util/str.h"
#include "modules/xmlparser/xmldoctype.h"
#include "modules/xmlutils/xmldocumentinfo.h"

XMLDocumentInformation::XMLDocumentInformation()
	: xml_declaration_present(FALSE),
	  version(XMLVERSION_1_0),
	  standalone(XMLSTANDALONE_NONE),
	  encoding(0),
	  doctype_declaration_present(FALSE),
	  doctype_name(0),
	  public_id(0),
	  system_id(0),
	  internal_subset(0),
	  doctype(0)
{
}

XMLDocumentInformation::~XMLDocumentInformation()
{
	OP_DELETEA(encoding);
	OP_DELETEA(doctype_name);
	OP_DELETEA(public_id);
	OP_DELETEA(system_id);
	OP_DELETEA(internal_subset);

	XMLDoctype::DecRef(doctype);
}

static OP_STATUS
XMLUtils_UniSetStr(uni_char *&target, const uni_char *source)
{
	if (source)
		return UniSetStr(target, source);
	else
	{
		OP_DELETEA(target);
		target = 0;
		return OpStatus::OK;
	}
}

const uni_char *
XMLDocumentInformation::GetResolvedSystemId() const
{
	return GetResolvedSystemId(public_id, system_id);
}

const uni_char *
XMLDocumentInformation::GetDefaultNamespaceURI() const
{
	return GetDefaultNamespaceURI(public_id, system_id);
}

const uni_char *
XMLDocumentInformation::GetRootElementName() const
{
	return GetRootElementName(public_id, system_id);
}

unsigned
XMLDocumentInformation::GetKnownDefaultAttributeCount(const uni_char *qname, unsigned qname_length) const
{
	return GetKnownDefaultAttributeCount(public_id, system_id, qname, qname_length);
}

void
XMLDocumentInformation::GetKnownDefaultAttribute(unsigned index, const uni_char *&qname, const uni_char *&value, BOOL &fixed) const
{
	GetKnownDefaultAttribute(public_id, system_id, index, qname, value, fixed);
}

BOOL
XMLDocumentInformation::IsXHTML() const
{
	return IsXHTML(public_id, system_id);
}

#ifdef SVG_SUPPORT

BOOL
XMLDocumentInformation::IsSVG() const
{
	return IsSVG(public_id, system_id);
}

#endif // SVG_SUPPORT

#ifdef _WML_SUPPORT_

BOOL
XMLDocumentInformation::IsWML() const
{
	return IsWML(public_id, system_id);
}

#endif // _WML_SUPPORT_

/* static */ const uni_char *
XMLDocumentInformation::GetResolvedSystemId(const uni_char *public_id, const uni_char *system_id)
{
	if (public_id)
		if (uni_str_eq(public_id, "-//W3C//DTD XHTML 1.0 Strict//EN"))
			return UNI_L("http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd");
		else if (uni_str_eq(public_id, "-//W3C//DTD XHTML 1.0 Transitional//EN"))
			return UNI_L("http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd");
		else if (uni_str_eq(public_id, "-//W3C//DTD XHTML 1.0 Frameset//EN"))
			return UNI_L("http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd");
		else if (uni_str_eq(public_id, "-//W3C//DTD XHTML 1.1//EN"))
			if (uni_str_eq(system_id, "http://www.w3.org/TR/xhtml11/DTD/xhtml11-flat.dtd"))
				return system_id;
			else
				return UNI_L("http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd");
		else if (uni_str_eq(public_id, "-//WAPFORUM//DTD XHTML Mobile 1.0//EN"))
			if (uni_str_eq(system_id, "http://www.wapforum.org/DTD/xhtml-mobile10-flat.dtd"))
				return system_id;
			else
				return UNI_L("http://www.wapforum.org/DTD/xhtml-mobile10.dtd");
#ifdef SVG_SUPPORT
		else if (uni_str_eq(public_id, "-//W3C//DTD SVG 1.1//EN"))
			return UNI_L("http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd");
		else if (uni_str_eq(public_id, "-//W3C//DTD SVG 1.1 Basic//EN"))
			return UNI_L("http://www.w3.org/Graphics/SVG/1.1/DTD/svg11-basic.dtd");
		else if (uni_str_eq(public_id, "-//W3C//DTD SVG 1.1 Tiny//EN"))
			return UNI_L("http://www.w3.org/Graphics/SVG/1.1/DTD/svg11-tiny.dtd");
		else if (uni_str_eq(public_id, "-//W3C//DTD SVG 1.0//EN"))
			return UNI_L("http://www.w3.org/TR/SVG10/DTD/svg10.dtd");
#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_
		else if (uni_str_eq(public_id, "-//WAPFORUM//DTD WML 1.1//EN"))
			return UNI_L("http://www.wapforum.org/DTD/wml_1_1.dtd");
		else if (uni_str_eq(public_id, "-//WAPFORUM//DTD WML 1.2//EN"))
			return UNI_L("http://www.wapforum.org/DTD/wml12.dtd");
		else if (uni_str_eq(public_id, "-//WAPFORUM//DTD WML 1.3//EN"))
			return UNI_L("http://www.wapforum.org/DTD/wml13.dtd");
		else if (uni_str_eq(public_id, "-//WAPFORUM//DTD WML 2.0//EN"))
			return UNI_L("http://www.wapforum.org/DTD/wml20.dtd");
#endif // _WML_SUPPORT_

	return system_id;
}

/* static */ BOOL
XMLDocumentInformation::IsXHTML(const uni_char *public_id, const uni_char *system_id)
{
	system_id = GetResolvedSystemId(public_id, system_id);

	if (system_id && (uni_str_eq(system_id, "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/TR/xhtml11/DTD/xhtml11-flat.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/TR/xhtml-basic/xhtml-basic10.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/TR/xhtml-basic/xhtml-basic11.dtd") ||
	                  uni_str_eq(system_id, "http://www.openmobilealliance.org/tech/DTD/xhtml-mobile11.dtd") ||
#ifdef _WML_SUPPORT_
					  // WML 2.0 is XHTML
					  uni_str_eq(system_id, "http://www.wapforum.org/DTD/wml20.dtd") ||
#endif // _WML_SUPPORT_
					  uni_str_eq(system_id, "http://www.wapforum.org/DTD/xhtml-mobile10.dtd") ||
					  uni_str_eq(system_id, "http://www.wapforum.org/DTD/xhtml-mobile10-flat.dtd")))
		return TRUE;
	else
		return FALSE;
}

#ifdef SVG_SUPPORT

/* static */ BOOL
XMLDocumentInformation::IsSVG(const uni_char *public_id, const uni_char *system_id)
{
	system_id = GetResolvedSystemId(public_id, system_id);

	if (system_id && (uni_str_eq(system_id, "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11-flat.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11-basic.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11-tiny.dtd") ||
	                  uni_str_eq(system_id, "http://www.w3.org/TR/SVG10/DTD/svg10.dtd")))
		return TRUE;
	else
		return FALSE;
}

#endif // SVG_SUPPORT

#ifdef _WML_SUPPORT_

/* static */ BOOL
XMLDocumentInformation::IsWML(const uni_char *public_id, const uni_char *system_id)
{
	system_id = GetResolvedSystemId(public_id, system_id);

	if (system_id && (uni_str_eq(system_id, "http://www.wapforum.org/DTD/wml_1_1.dtd") ||
	                  uni_str_eq(system_id, "http://www.wapforum.org/DTD/wml12.dtd") ||
	                  uni_str_eq(system_id, "http://www.wapforum.org/DTD/wml13.dtd")))
		return TRUE;
	else
		return FALSE;
}

#endif // _WML_SUPPORT_

/* static */ const uni_char *
XMLDocumentInformation::GetDefaultNamespaceURI(const uni_char *public_id, const uni_char *system_id)
{
	system_id = GetResolvedSystemId(public_id, system_id);

	if (IsXHTML(0, system_id))
		return UNI_L("http://www.w3.org/1999/xhtml");
#ifdef SVG_SUPPORT
	else if (IsSVG(0, system_id))
		return UNI_L("http://www.w3.org/2000/svg");
#endif // SVG_SUPPORT
	else
		return 0;
}

/* static */ const uni_char *
XMLDocumentInformation::GetRootElementName(const uni_char *public_id, const uni_char *system_id)
{
	system_id = GetResolvedSystemId(public_id, system_id);

	if (IsXHTML(0, system_id))
		return UNI_L("html");
#ifdef SVG_SUPPORT
	else if (IsSVG(0, system_id))
		return UNI_L("svg");
#endif // SVG_SUPPORT
	else
		return 0;
}

/* static */ unsigned
XMLDocumentInformation::GetKnownDefaultAttributeCount(const uni_char *public_id, const uni_char *system_id, const uni_char *qname, unsigned qname_length)
{
	if (qname_length == ~0u)
		qname_length = uni_strlen(qname);

	if (qname_length == 4 && op_memcmp(qname, UNI_L("html"), 4 * sizeof(uni_char)) == 0 && IsXHTML(public_id, system_id))
		return 1;
#ifdef SVG_SUPPORT
	else if (qname_length == 3 && op_memcmp(qname, UNI_L("svg"), 3 * sizeof(uni_char)) == 0 && IsSVG(public_id, system_id))
		return 2;
#endif // SVG_SUPPORT

	return 0;
}

/* static */ void
XMLDocumentInformation::GetKnownDefaultAttribute(const uni_char *public_id, const uni_char *system_id, unsigned index, const uni_char *&qname, const uni_char *&value, BOOL &fixed)
{
	system_id = GetResolvedSystemId(public_id, system_id);

	if (IsXHTML(0, system_id))
	{
		if (index == 0)
		{
			qname = UNI_L("xmlns");
			value = UNI_L("http://www.w3.org/1999/xhtml");
			fixed = TRUE;
		}
	}
#ifdef SVG_SUPPORT
	else if (IsSVG(0, system_id))
	{
		if (index == 0)
		{
			qname = UNI_L("xmlns");
			value = UNI_L("http://www.w3.org/2000/svg");
		}
		else if (index == 1)
		{
			qname = UNI_L("xmlns:xlink");
			value = UNI_L("http://www.w3.org/1999/xlink");
		}
		fixed = TRUE;
	}
#endif // SVG_SUPPORT
}

OP_STATUS
XMLDocumentInformation::Copy(const XMLDocumentInformation &information)
{
	xml_declaration_present = information.xml_declaration_present;
	version = information.version;
	standalone = information.standalone;
	RETURN_IF_ERROR(XMLUtils_UniSetStr(encoding, information.encoding));
	doctype_declaration_present = information.doctype_declaration_present;
	RETURN_IF_ERROR(XMLUtils_UniSetStr(doctype_name, information.doctype_name));
	RETURN_IF_ERROR(XMLUtils_UniSetStr(public_id, information.public_id));
	RETURN_IF_ERROR(XMLUtils_UniSetStr(system_id, information.system_id));
	RETURN_IF_ERROR(XMLUtils_UniSetStr(internal_subset, information.internal_subset));

	doctype = XMLDoctype::IncRef(information.doctype);

	return OpStatus::OK;
}

OP_STATUS
XMLDocumentInformation::SetXMLDeclaration(XMLVersion new_version, XMLStandalone new_standalone, const uni_char *new_encoding)
{
	xml_declaration_present = TRUE;
	version = new_version;
	standalone = new_standalone;
	return XMLUtils_UniSetStr(encoding, new_encoding);
}

OP_STATUS
XMLDocumentInformation::SetDoctypeDeclaration(const uni_char *new_doctype_name, const uni_char *new_public_id, const uni_char *new_system_id, const uni_char *new_internal_subset)
{
	doctype_declaration_present = TRUE;
	RETURN_IF_ERROR(XMLUtils_UniSetStr(doctype_name, new_doctype_name));
	RETURN_IF_ERROR(XMLUtils_UniSetStr(public_id, new_public_id));
	RETURN_IF_ERROR(XMLUtils_UniSetStr(system_id, new_system_id));
	return XMLUtils_UniSetStr(internal_subset, new_internal_subset);
}

void
XMLDocumentInformation::SetDoctype(XMLDoctype *new_doctype)
{
	doctype = XMLDoctype::IncRef(new_doctype);
}
