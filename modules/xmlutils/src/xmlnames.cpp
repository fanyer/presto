/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlutils/xmlnames.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/util/str.h"

static UINT32
XMLUtils_HashStringN(const uni_char *string, unsigned string_length)
{
	if (string)
    {
		UINT32 hash = string_length, index = hash;

		while (index-- > 0)
			hash = hash + hash + hash + string[index];

		return hash;
    }
	else
		return 0;
}

static UINT32
XMLUtils_HashString(const uni_char *string)
{
	return XMLUtils_HashStringN(string, uni_strlen(string));
}

void
XMLExpandedName::Free()
{
	if (owns_strings)
	{
		OP_DELETEA(uri);
		OP_DELETEA(localpart);
		owns_strings = FALSE;
	}
}

void
XMLExpandedName::Reset()
{
	Free();

	uri = localpart = 0;
	owns_strings = TRUE;
}

XMLExpandedName::XMLExpandedName()
	: uri(0),
	  localpart(0),
	  owns_strings(FALSE)
{
}

XMLExpandedName::XMLExpandedName(const uni_char *localpart)
	: uri(NULL),
	  localpart((uni_char *) localpart),
	  owns_strings(FALSE)
{
}

XMLExpandedName::XMLExpandedName(const uni_char *uri, const uni_char *localpart)
	: uri((uni_char *) uri),
	  localpart((uni_char *) localpart),
	  owns_strings(FALSE)
{
}

XMLExpandedName::XMLExpandedName(const XMLExpandedName &name)
	: uri(name.uri),
	  localpart(name.localpart),
	  owns_strings(FALSE)
{
}

XMLExpandedName::XMLExpandedName(HTML_Element *element)
	: uri((uni_char *) g_ns_manager->GetElementAt(element->GetNsIdx())->GetUri()),
	  localpart((uni_char *) element->GetTagName()),
	  owns_strings(FALSE)
{
	if (uri && !*uri)
		uri = 0;
}

XMLExpandedName::XMLExpandedName(NS_Element *nselement, const uni_char *localpart)
	: uri((uni_char *) (nselement ? nselement->GetUri() : NULL)),
	  localpart((uni_char *) localpart),
	  owns_strings(FALSE)
{
	if (uri && !*uri)
		uri = 0;
}

XMLExpandedName::~XMLExpandedName()
{
	Free();
}

NS_Type
XMLExpandedName::GetNsType() const
{
	return uri ? g_ns_manager->FindNsType(uri, uni_strlen(uri)) : NS_NONE;
}

int
XMLExpandedName::GetNsIndex() const
{
	return uri ? g_ns_manager->GetNsIdx(uri, uni_strlen(uri), 0, 0) : NS_IDX_DEFAULT;
}

BOOL
XMLExpandedName::IsXHTML() const
{
	return XMLExpandedNameN(*this).IsXHTML();
}

BOOL
XMLExpandedName::IsXML() const
{
	return XMLExpandedNameN(*this).IsXML();
}

BOOL
XMLExpandedName::IsXMLNamespaces() const
{
	return XMLExpandedNameN(*this).IsXMLNamespaces();
}

#ifdef SVG_SUPPORT

BOOL
XMLExpandedName::IsSVG() const
{
	return XMLExpandedNameN(*this).IsSVG();
}

#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_

BOOL
XMLExpandedName::IsWML() const
{
	return XMLExpandedNameN(*this).IsWML();
}

#endif // _WML_SUPPORT_
#ifdef XSLT_SUPPORT

BOOL
XMLExpandedName::IsXSLT() const
{
	return XMLExpandedNameN(*this).IsXSLT();
}

#endif // XSLT_SUPPORT

BOOL
XMLExpandedName::IsId(const XMLExpandedName &elementname) const
{
	return XMLExpandedNameN(*this).IsId(elementname);
}

BOOL
XMLExpandedName::operator==(const XMLExpandedName &other) const
{
	return (uri == other.uri || uri && other.uri && uni_strcmp(uri, other.uri) == 0) && uni_strcmp(localpart, other.localpart) == 0;
}

BOOL
XMLExpandedName::operator==(const XMLExpandedNameN &other) const
{
	const uni_char *other_uri = other.GetUri();
	unsigned other_uri_length = other.GetUriLength();
	const uni_char *other_localpart = other.GetLocalPart();
	unsigned other_localpart_length = other.GetLocalPartLength();

	return (uri == other_uri || uri && other_uri && uni_strlen(uri) == other_uri_length && uni_strncmp(uri, other_uri, other_uri_length) == 0) && uni_strlen(localpart) == other_localpart_length && uni_strncmp(localpart, other_localpart, other_localpart_length) == 0;
}

XMLExpandedName &
XMLExpandedName::operator=(const XMLExpandedName &other)
{
	Free();

	uri = other.uri;
	localpart = other.localpart;

    return *this;
}

OP_STATUS
XMLExpandedName::Set(const XMLExpandedName &other)
{
	Reset();

	if ((!other.uri || (uri = UniSetNewStr(other.uri)) != 0) && (localpart = UniSetNewStr(other.localpart)) != 0)
		return OpStatus::OK;
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
XMLExpandedName::Set(const XMLExpandedNameN &other)
{
	Reset();

	const uni_char *other_uri = other.GetUri();

	if ((!other_uri || (uri = UniSetNewStrN(other_uri, other.GetUriLength())) != 0) && (localpart = UniSetNewStrN(other.GetLocalPart(), other.GetLocalPartLength())) != 0)
		return OpStatus::OK;
	else
		return OpStatus::ERR_NO_MEMORY;
}

void
XMLExpandedName::SetL(const XMLExpandedName &other)
{
	LEAVE_IF_ERROR(Set(other));
}

void
XMLExpandedName::SetL(const XMLExpandedNameN &other)
{
	LEAVE_IF_ERROR(Set(other));
}

/* virtual */ UINT32
XMLExpandedName::HashFunctions::Hash(const void *key)
{
	const XMLExpandedName *name = (const XMLExpandedName *) key;
	UINT32 urihash = name->GetUri() ? XMLUtils_HashString(name->GetUri()) : 0;
	UINT32 localparthash = XMLUtils_HashString(name->GetLocalPart());
	return urihash + (localparthash << 20);
}

/* virtual */ BOOL
XMLExpandedName::HashFunctions::KeysAreEqual(const void *key1, const void *key2)
{
	return *((const XMLExpandedName *) key1) == *((const XMLExpandedName *) key2);
}

void
XMLCompleteName::Free()
{
	if (owns_strings)
	{
		uni_char *non_const = const_cast<uni_char *>(prefix);
		OP_DELETEA(non_const);
		XMLExpandedName::Free();
	}
}

void
XMLCompleteName::Reset()
{
	Free();

	XMLExpandedName::Reset();
	prefix = 0;
}

OP_STATUS
XMLCompleteName::CopyStrings()
{
	OP_ASSERT(!owns_strings);

	uni_char *new_uri = NULL, *new_prefix = NULL, *new_localpart = NULL;

	if ((!uri || (new_uri = UniSetNewStr(uri)) != 0) &&
		(!prefix || (new_prefix = UniSetNewStr(prefix)) != 0) &&
		(new_localpart = UniSetNewStr(localpart)) != 0)
	{
		uri = new_uri;
		prefix = new_prefix;
		localpart = new_localpart;
		owns_strings = TRUE;
		return OpStatus::OK;
	}
	else
	{
		OP_DELETEA(new_uri);
		OP_DELETEA(new_prefix);
		OP_DELETEA(new_localpart);
		return OpStatus::ERR_NO_MEMORY;
	}
}

XMLCompleteName::XMLCompleteName()
	: prefix(0)
{
}

XMLCompleteName::XMLCompleteName(const uni_char *localpart)
	: XMLExpandedName(localpart),
	  prefix(NULL)
{
}

XMLCompleteName::XMLCompleteName(const uni_char *uri, const uni_char *prefix, const uni_char *localpart)
	: XMLExpandedName(uri, localpart),
	  prefix((uni_char *) prefix)
{
}

XMLCompleteName::XMLCompleteName(const XMLExpandedName &name)
	: XMLExpandedName(name),
	  prefix(0)
{
}

XMLCompleteName::XMLCompleteName(const XMLCompleteName &name)
	: XMLExpandedName(name),
	  prefix(name.prefix)
{
}

XMLCompleteName::XMLCompleteName(HTML_Element *element)
	: XMLExpandedName(element),
	  prefix((uni_char *) g_ns_manager->GetPrefixAt(element->GetNsIdx()))
{
	if (prefix && !*prefix)
		prefix = 0;
}

XMLCompleteName::XMLCompleteName(NS_Element *nselement, const uni_char *localpart)
	: XMLExpandedName(nselement, localpart),
	  prefix((uni_char *) (nselement ? nselement->GetPrefix() : NULL))
{
	if (prefix && !*prefix)
		prefix = 0;
}

XMLCompleteName::~XMLCompleteName()
{
	Free();
}

int
XMLCompleteName::GetNsIndex() const
{
	return uri ? g_ns_manager->GetNsIdx(uri, uni_strlen(uri), prefix, prefix ? uni_strlen(prefix) : 0) : NS_IDX_DEFAULT;
}

BOOL
XMLCompleteName::operator==(const XMLCompleteName &other) const
{
	return XMLExpandedName::operator==(other) && (prefix == other.prefix || prefix && other.prefix && uni_strcmp(prefix, other.prefix) == 0);
}

BOOL
XMLCompleteName::operator==(const XMLCompleteNameN &other) const
{
	if (XMLExpandedName::operator==(other))
	{
		const uni_char *other_prefix = other.GetPrefix();
		unsigned other_prefix_length = other.GetPrefixLength();

		return prefix == other_prefix || prefix && other_prefix && uni_strlen(prefix) == other_prefix_length && uni_strncmp(prefix, other_prefix, other_prefix_length) == 0;
	}
	else
		return FALSE;
}

BOOL
XMLCompleteName::SameQName(const XMLCompleteName &other) const
{
	return uni_strcmp(localpart, other.localpart) == 0 && (prefix == other.prefix || prefix && other.prefix && uni_strcmp(prefix, other.prefix) == 0);
}

XMLCompleteName &
XMLCompleteName::operator=(const XMLExpandedName &other)
{
	Free();

	XMLExpandedName::operator=(other);
	prefix = 0;

    return *this;
}

XMLCompleteName &
XMLCompleteName::operator=(const XMLCompleteName &other)
{
	Free();

	XMLExpandedName::operator=(other);
	prefix = other.prefix;

    return *this;
}

OP_STATUS
XMLCompleteName::Set(const XMLExpandedName &other)
{
	Reset();

	return XMLExpandedName::Set(other);
}

OP_STATUS
XMLCompleteName::Set(const XMLExpandedNameN &other)
{
	Reset();

	return XMLExpandedName::Set(other);
}

OP_STATUS
XMLCompleteName::Set(const XMLCompleteName &other)
{
	Reset();

	if (XMLExpandedName::Set(other) == OpStatus::OK && (!other.prefix || (prefix = UniSetNewStr(other.prefix)) != 0))
		return OpStatus::OK;
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
XMLCompleteName::Set(const XMLCompleteNameN &other)
{
	Reset();

	const uni_char *other_prefix = other.GetPrefix();

	if (XMLExpandedName::Set(other) == OpStatus::OK && (!other_prefix || (prefix = UniSetNewStrN(other_prefix, other.GetPrefixLength())) != 0))
		return OpStatus::OK;
	else
		return OpStatus::ERR_NO_MEMORY;
}

void
XMLCompleteName::SetL(const XMLExpandedName &other)
{
	LEAVE_IF_ERROR(Set(other));
}

void
XMLCompleteName::SetL(const XMLExpandedNameN &other)
{
	LEAVE_IF_ERROR(Set(other));
}

void
XMLCompleteName::SetL(const XMLCompleteName &other)
{
	LEAVE_IF_ERROR(Set(other));
}

void
XMLCompleteName::SetL(const XMLCompleteNameN &other)
{
	LEAVE_IF_ERROR(Set(other));
}

OP_STATUS
XMLCompleteName::SetUri(const uni_char *new_uri)
{
	OP_ASSERT(!new_uri || new_uri != uri);

	if (uri && owns_strings)
		OP_DELETEA(uri);
	uri = NULL;

	if (!new_uri)
		return OpStatus::OK;

	if (!owns_strings)
		RETURN_IF_ERROR(CopyStrings());

	return UniSetStr(uri, new_uri);
}

OP_STATUS
XMLCompleteName::SetPrefix(const uni_char *new_prefix)
{
	OP_ASSERT(new_prefix != prefix);

	if (prefix && owns_strings)
		OP_DELETEA(prefix);
	prefix = NULL;

	if (!new_prefix)
		return OpStatus::OK;

	if (!owns_strings)
		RETURN_IF_ERROR(CopyStrings());

	return UniSetStr(prefix, new_prefix);
}

XMLExpandedNameN::XMLExpandedNameN()
	: uri(0),
	  localpart(0),
	  uri_length(0),
	  localpart_length(0)
{
}

XMLExpandedNameN::XMLExpandedNameN(const uni_char *uri, unsigned uri_length, const uni_char *localpart, unsigned localpart_length)
	: uri(uri),
	  localpart(localpart),
	  uri_length(uri_length),
	  localpart_length(localpart_length)
{
}

XMLExpandedNameN::XMLExpandedNameN(const XMLExpandedName &other)
	: uri(other.GetUri()),
	  localpart(other.GetLocalPart()),
	  uri_length(uri ? uni_strlen(uri) : 0),
	  localpart_length(localpart ? uni_strlen(localpart) : 0)
{
}

XMLExpandedNameN::XMLExpandedNameN(const XMLExpandedNameN &other)
	: uri(other.uri),
	  localpart(other.localpart),
	  uri_length(other.uri_length),
	  localpart_length(other.localpart_length)
{
}

NS_Type
XMLExpandedNameN::GetNsType() const
{
	return uri ? g_ns_manager->FindNsType(uri, uri_length) : NS_NONE;
}

int
XMLExpandedNameN::GetNsIndex() const
{
	return uri ? g_ns_manager->GetNsIdx(uri, uri_length, 0, 0) : NS_IDX_DEFAULT;
}

BOOL
XMLExpandedNameN::IsXHTML() const
{
	OP_ASSERT(op_strlen("http://www.w3.org/1999/xhtml") == 28);
	return uri_length == 28 && op_memcmp(uri, UNI_L("http://www.w3.org/1999/xhtml"), 28 * sizeof uri[0]) == 0;
}

BOOL
XMLExpandedNameN::IsXML() const
{
	return uri_length == XMLNSURI_XML_LENGTH && op_memcmp(uri, XMLNSURI_XML, XMLNSURI_XML_LENGTH * sizeof uri[0]) == 0;
}

BOOL
XMLExpandedNameN::IsXMLNamespaces() const
{
	return uri_length == XMLNSURI_XMLNS_LENGTH && op_memcmp(uri, XMLNSURI_XMLNS, XMLNSURI_XMLNS_LENGTH * sizeof uri[0]) == 0;
}

#ifdef SVG_SUPPORT

BOOL
XMLExpandedNameN::IsSVG() const
{
	OP_ASSERT(op_strlen("http://www.w3.org/2000/svg") == 26);
	return uri_length == 26 && op_memcmp(uri, UNI_L("http://www.w3.org/2000/svg"), 26 * sizeof uri[0]) == 0;
}

#endif // SVG_SUPPORT
#ifdef _WML_SUPPORT_

BOOL
XMLExpandedNameN::IsWML() const
{
	OP_ASSERT(op_strlen("http://www.wapforum.org/2001/wml") == 32);
	return uri_length == 32 && op_memcmp(uri, UNI_L("http://www.wapforum.org/2001/wml"), 32 * sizeof uri[0]) == 0;
}

#endif // _WML_SUPPORT_
#ifdef XSLT_SUPPORT

BOOL
XMLExpandedNameN::IsXSLT() const
{
	return uri_length == 36 && op_memcmp(uri, UNI_L("http://www.w3.org/1999/XSL/Transform"), 36 * sizeof uri[0]) == 0;
}

#endif // XSLT_SUPPORT

BOOL
XMLExpandedNameN::IsId(const XMLExpandedNameN &elementname) const
{
	if (localpart_length == 2 && localpart[0] == 'i' && localpart[1] == 'd')
	{
		BOOL is_null_ns = GetUri() == 0;

		if (IsXML())
			return TRUE;

		if (is_null_ns)
		{
			if (elementname.IsXHTML())
				return TRUE;

#ifdef SVG_SUPPORT
			if (elementname.IsSVG())
				return TRUE;
#endif // SVG_SUPPORT

#ifdef _WML_SUPPORT_
			if (elementname.IsWML())
				return TRUE;
#endif // _WML_SUPPORT_

#ifdef XSLT_SUPPORT
			if (elementname.IsXSLT() && (elementname.localpart_length == 10 && uni_strncmp(elementname.localpart, "stylesheet", 10) == 0 ||
			                             elementname.localpart_length == 9 && uni_strncmp(elementname.localpart, "transform", 9) == 0))
				return TRUE;
#endif // XSLT_SUPPORT
		}
	}

	return FALSE;
}

BOOL
XMLExpandedNameN::operator==(const XMLExpandedNameN &other) const
{
	return uri_length == other.uri_length && localpart_length == other.localpart_length && (uri_length == 0 || uni_strncmp(uri, other.uri, uri_length) == 0) && uni_strncmp(localpart, other.localpart, localpart_length) == 0;
}

void
XMLExpandedNameN::SetUri(const uni_char *new_uri, unsigned new_uri_length)
{
	uri = new_uri;
	uri_length = new_uri_length;
}

/* virtual */ UINT32
XMLExpandedNameN::HashFunctions::Hash(const void *key)
{
	const XMLExpandedNameN *name = (const XMLExpandedNameN *) key;
	UINT32 urihash = XMLUtils_HashStringN(name->GetUri(), name->GetUriLength ());
	UINT32 localparthash = XMLUtils_HashStringN(name->GetLocalPart(), name->GetLocalPartLength());
	return urihash + (localparthash << 20);
}

/* virtual */ BOOL
XMLExpandedNameN::HashFunctions::KeysAreEqual(const void *key1, const void *key2)
{
	return *((const XMLExpandedNameN *) key1) == *((const XMLExpandedNameN *) key2);
}

XMLCompleteNameN::XMLCompleteNameN()
	: prefix(0),
	  prefix_length(0)
{
}

XMLCompleteNameN::XMLCompleteNameN(const uni_char *qname, unsigned qname_length)
{
	if (qname_length != 0)
		for (unsigned index = 1; index < qname_length - 1; ++index)
			if (qname[index] == ':')
			{
				prefix = qname;
				prefix_length = index;
				++index;
				localpart = qname + index;
				localpart_length = qname_length - index;
				return;
			}

	prefix = 0;
	prefix_length = 0;
	localpart = qname;
	localpart_length = qname_length;
}

XMLCompleteNameN::XMLCompleteNameN(const uni_char *uri, unsigned uri_length, const uni_char *prefix, unsigned prefix_length, const uni_char *localpart, unsigned localpart_length)
	: XMLExpandedNameN(uri, uri_length, localpart, localpart_length),
	  prefix(prefix),
	  prefix_length(prefix_length)
{
}

XMLCompleteNameN::XMLCompleteNameN(const XMLCompleteName &other)
	: XMLExpandedNameN(other),
	  prefix(other.GetPrefix ()),
	  prefix_length(prefix ? uni_strlen(prefix) : 0)
{
}

XMLCompleteNameN::XMLCompleteNameN(const XMLCompleteNameN &other)
	: XMLExpandedNameN(other),
	  prefix(other.GetPrefix ()),
	  prefix_length(other.GetPrefixLength ())
{
}

int
XMLCompleteNameN::GetNsIndex() const
{
	return uri ? g_ns_manager->GetNsIdx(uri, uri_length, prefix, prefix_length) : NS_IDX_DEFAULT;
}

BOOL
XMLCompleteNameN::operator==(const XMLCompleteNameN &other) const
{
	return XMLExpandedNameN::operator==(other) && prefix_length == other.prefix_length && (prefix_length == 0 || uni_strncmp(prefix, other.prefix, prefix_length) == 0);
}

BOOL
XMLCompleteNameN::SameQName(const XMLCompleteNameN &other) const
{
	return localpart_length == other.localpart_length && uni_strncmp(localpart, other.localpart, localpart_length) == 0 &&
	       prefix_length == other.prefix_length && (prefix_length == 0 || uni_strncmp(prefix, other.prefix, prefix_length) == 0);
}

XMLNamespaceDeclaration::XMLNamespaceDeclaration(XMLNamespaceDeclaration *previous, unsigned level)
	: previous(IncRef(previous)),
	  uri(0),
	  prefix(0),
	  level(level),
	  refcount(0)
{
}

XMLNamespaceDeclaration::~XMLNamespaceDeclaration()
{
	OP_ASSERT(refcount == 0);

	DecRef(previous);

	OP_DELETEA(uri);
	OP_DELETEA(prefix);
}

NS_Type
XMLNamespaceDeclaration::GetNsType()
{
	if (uri)
		return g_ns_manager->FindNsType(uri, 0);
	else
		return NS_NONE;
}

int
XMLNamespaceDeclaration::GetNsIndex()
{
	if (uri)
		return g_ns_manager->GetNsIdx(uri, uni_strlen(uri), prefix, prefix ? uni_strlen(prefix) : 0, FALSE, FALSE, TRUE);
	else
		return NS_IDX_DEFAULT;
}

/* static */ BOOL
XMLNamespaceDeclaration::IsDeclared(XMLNamespaceDeclaration *declaration, const uni_char *prefix, const uni_char *uri)
{
	if (XMLNamespaceDeclaration *decl = FindDeclaration(declaration, prefix))
		return decl->GetUri() && uni_strcmp(uri, decl->GetUri()) == 0;
	else
		return FALSE;
}

/* static */ unsigned
XMLNamespaceDeclaration::CountDeclaredPrefixes(XMLNamespaceDeclaration *declaration)
{
	unsigned count = 0;
	XMLNamespaceDeclaration *iter = declaration;
	while (iter)
	{
		if (iter->prefix && FindDeclaration(declaration, iter->prefix) == iter)
			++count;
		iter = iter->previous;
	}
	return count;
}

/* static */ XMLNamespaceDeclaration *
XMLNamespaceDeclaration::FindDeclaration(XMLNamespaceDeclaration *current, const uni_char *prefix, unsigned prefix_length)
{
	if (prefix_length == ~0u)
		if (prefix)
			prefix_length = uni_strlen(prefix);
		else
			prefix_length = 0;

	XMLNamespaceDeclaration *declaration = current;
	while (declaration)
		if (prefix == declaration->prefix || prefix && declaration->prefix && prefix_length == uni_strlen(declaration->prefix) && uni_strncmp(prefix, declaration->prefix, prefix_length) == 0)
			return declaration;
		else
			declaration = declaration->previous;
	return 0;
}

/* static */ XMLNamespaceDeclaration *
XMLNamespaceDeclaration::FindDeclarationByIndex(XMLNamespaceDeclaration *declaration, unsigned index)
{
	XMLNamespaceDeclaration *iter = declaration;
	while (iter)
	{
		if (iter->prefix && FindDeclaration(declaration, iter->prefix) == iter)
			if (index == 0)
				return iter;
			else
				--index;
		iter = iter->previous;
	}
	return 0;
}

/* static */ XMLNamespaceDeclaration *
XMLNamespaceDeclaration::FindDefaultDeclaration(XMLNamespaceDeclaration *current, BOOL return_undeclaration)
{
	if (current && (return_undeclaration || current->defaultns && current->defaultns->uri))
		return current->defaultns;
	else
		return 0;
}

/* static */ const uni_char *
XMLNamespaceDeclaration::FindUri(XMLNamespaceDeclaration *current, const uni_char *prefix, unsigned prefix_length)
{
	if (XMLNamespaceDeclaration *declaration = FindDeclaration(current, prefix, prefix_length))
		return declaration->uri;
	else
		return 0;
}

/* static */ const uni_char *
XMLNamespaceDeclaration::FindDefaultUri(XMLNamespaceDeclaration *current)
{
	if (XMLNamespaceDeclaration *declaration = FindDefaultDeclaration(current))
		return declaration->uri;
	else
		return 0;
}

/* static */ const uni_char *
XMLNamespaceDeclaration::FindPrefix(XMLNamespaceDeclaration *current, const uni_char *uri, unsigned uri_length)
{
	/* Would mean "find an undeclared prefix," but that hardly seems useful. */
	OP_ASSERT(uri != 0);

	if (uri_length == ~0u)
		uri_length = uni_strlen(uri);

	XMLNamespaceDeclaration *declaration = current;
	while (declaration)
		if (declaration->uri && uri_length == uni_strlen(declaration->uri) && uni_strncmp(uri, declaration->uri, uri_length) == 0 && FindUri(current, declaration->prefix) == declaration->uri)
			return declaration->prefix;
		else
			declaration = declaration->previous;
	return 0;
}

/* static */ OP_BOOLEAN
XMLNamespaceDeclaration::ResolveName(XMLNamespaceDeclaration *current, XMLCompleteName &name, BOOL use_default)
{
	XMLCompleteNameN temporary(name);

	if (!XMLNamespaceDeclaration::ResolveName(current, temporary, use_default))
		return OpBoolean::IS_FALSE;

	if (temporary.GetUri() != name.GetUri())
	{
		RETURN_IF_ERROR(name.SetUri(temporary.GetUri()));
	}

	return OpBoolean::IS_TRUE;
}

/* static */ BOOL
XMLNamespaceDeclaration::ResolveName(XMLNamespaceDeclaration *current, XMLCompleteNameN &name, BOOL use_default)
{
	const uni_char *prefix = name.GetPrefix();
	unsigned uri_length = 0;

	if (!prefix && name.GetLocalPartLength() == 5 && op_memcmp(name.GetLocalPart(), UNI_L("xmlns"), 5 * sizeof(uni_char)) == 0)
		name.SetUri(XMLNSURI_XMLNS, XMLNSURI_XMLNS_LENGTH);
	else if (prefix || use_default)
	{
		const uni_char *uri;

		if (!prefix)
			uri = FindDefaultUri(current);
		else
		{
			unsigned prefix_length = name.GetPrefixLength();

			if (prefix_length == 3 && op_memcmp(prefix, UNI_L("xml"), 3 * sizeof(uni_char)) == 0)
			{
				uri = XMLNSURI_XML;
				uri_length = XMLNSURI_XML_LENGTH;
			}
			else if (prefix_length == 5 && op_memcmp(prefix, UNI_L("xmlns"), 5 * sizeof(uni_char)) == 0)
			{
				uri = XMLNSURI_XMLNS;
				uri_length = XMLNSURI_XMLNS_LENGTH;
			}
			else
			{
				uri = FindUri(current, prefix, prefix_length);

				if (!uri)
					return FALSE;
			}
		}

		if (uri)
		{
			if (!uri_length)
				uri_length = uni_strlen(uri);

			name.SetUri(uri, uri_length);
		}
	}

	return TRUE;
}

/* static */ OP_STATUS
XMLNamespaceDeclaration::Push(XMLNamespaceDeclaration::Reference &current, const uni_char *uri, unsigned uri_length, const uni_char *prefix, unsigned prefix_length, unsigned level)
{
	XMLNamespaceDeclaration *declaration = OP_NEW(XMLNamespaceDeclaration, (current, level));

	if (uri_length == ~0u)
		uri_length = uni_strlen(uri);
	if (prefix_length == ~0u)
		prefix_length = uni_strlen(prefix);

	if (declaration && (!uri || (declaration->uri = UniSetNewStrN(uri, uri_length)) != 0) && (!prefix || (declaration->prefix = UniSetNewStrN(prefix, prefix_length)) != 0))
	{
		if (!declaration->prefix)
			declaration->defaultns = declaration;
		else
			declaration->defaultns = current ? current->defaultns : 0;

		current = declaration;
		return OpStatus::OK;
	}
	else
	{
		if (declaration)
			OP_DELETE(declaration);
		return OpStatus::ERR_NO_MEMORY;
	}
}

/* static */ void
XMLNamespaceDeclaration::PushL(XMLNamespaceDeclaration::Reference &current, const uni_char *uri, unsigned uri_length, const uni_char *prefix, unsigned prefix_length, unsigned level)
{
	LEAVE_IF_ERROR(Push(current, uri, uri_length, prefix, prefix_length, level));
}

/* static */ void
XMLNamespaceDeclaration::Pop(XMLNamespaceDeclaration::Reference &current, unsigned level)
{
	while (current && level == current->level)
	{
		XMLNamespaceDeclaration *declaration = current;
		current = declaration->previous;
	}
}

/* static */ OP_STATUS
XMLNamespaceDeclaration::ProcessAttribute(XMLNamespaceDeclaration::Reference &current, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length, unsigned level)
{
	const uni_char *string;
	unsigned length = name.GetPrefixLength();

	if (length == 0)
	{
		string = name.GetLocalPart();
		length = name.GetLocalPartLength();
	}
	else
		string = name.GetPrefix();

	if (length == 5 && op_memcmp(string, UNI_L("xmlns"), 5 * sizeof(uni_char)) == 0)
	{
		const uni_char *uri;

		if (value_length == 0)
			uri = 0;
		else
			uri = value;

		const uni_char *prefix;
		unsigned prefix_length;

		BOOL is_xml_namespace = value_length == XMLNSURI_XML_LENGTH && op_memcmp(value, XMLNSURI_XML, XMLNSURI_XML_LENGTH * sizeof(uni_char)) == 0;
		BOOL is_xmlns_namespace = value_length == XMLNSURI_XMLNS_LENGTH && op_memcmp(value, XMLNSURI_XMLNS, XMLNSURI_XMLNS_LENGTH * sizeof(uni_char)) == 0;

		if (name.GetPrefix())
		{
			prefix = name.GetLocalPart();
			prefix_length = name.GetLocalPartLength();

			/* Can't declare xml prefix to anything but http://www.w3.org/XML/1998/namespace
			   and can't declare any other prefix to that URI. */
			if (!(prefix_length == 3 && op_memcmp(prefix, UNI_L("xml"), 3 * sizeof(uni_char)) == 0) != !is_xml_namespace)
				return OpStatus::ERR;

			/* Can't declare xmlns prefix at all, and can't declare any other prefix to the
			   URI http://www.w3.org/2000/xmlns/. */
			if (prefix_length == 5 && op_memcmp(prefix, UNI_L("xmlns"), 5 * sizeof(uni_char)) == 0 || is_xmlns_namespace)
				return OpStatus::ERR;

			/* Only default namespace declarations can be empty. */
			if (value_length == 0)
				return OpStatus::ERR;
		}
		else
		{
			prefix = 0;
			prefix_length = 0;

			/* See above. */
			if (is_xml_namespace || is_xmlns_namespace)
				return OpStatus::ERR;
		}

		return Push(current, uri, value_length, prefix, prefix_length, level);
	}
	else
		return OpStatus::OK;
}

static void
GetNamespaceAndPrefix(int nsidx, const uni_char *&uri, const uni_char *&prefix)
{
	NS_Element *nselement = g_ns_manager->GetElementAt(nsidx);

	uri = nselement->GetUri();
	prefix = nselement->GetPrefix();

	if (uri && !*uri)
		uri = 0;

	if (prefix && !*prefix)
		prefix = 0;
}

static OP_STATUS
XMLNamespaceDeclaration_PushAllInScope(XMLNamespaceDeclaration::Reference &current, HTML_Element *element, unsigned &level, BOOL elements_actual_ns)
{
	if (HTML_Element *parent = element->ParentActual())
		if (parent->Type() != HE_DOC_ROOT)
		{
			RETURN_IF_ERROR(XMLNamespaceDeclaration_PushAllInScope(current, parent, level, elements_actual_ns));
			++level;
		}

	HTML_AttrIterator attributes(element);

	const uni_char *name, *value;
	const uni_char *uri, *prefix;
	int nsidx;

	while (attributes.GetNext(name, value, &nsidx))
	{
		GetNamespaceAndPrefix(nsidx, uri, prefix);

		XMLCompleteName completename(uri, prefix, name);

		RETURN_IF_ERROR(XMLNamespaceDeclaration::ProcessAttribute(current, completename, value, uni_strlen(value), level));
	}

	if (elements_actual_ns)
	{
		GetNamespaceAndPrefix(element->GetNsIdx(), uri, prefix);
		if (uri)
			return XMLNamespaceDeclaration::Push(current, uri, uni_strlen(uri), prefix, prefix ? uni_strlen(prefix) : 0, level);
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
XMLNamespaceDeclaration::PushAllInScope(XMLNamespaceDeclaration::Reference &current, HTML_Element *element, BOOL elements_actual_ns)
{
	unsigned level = 0;
	return XMLNamespaceDeclaration_PushAllInScope(current, element, level, elements_actual_ns);
}

/* static */ BOOL
XMLNamespaceDeclaration::ResolveNameInScope(HTML_Element *element, XMLCompleteNameN &name)
{
	while (element && element->Type() != HE_DOC_ROOT)
	{
		int attr_nsidx;
		const uni_char *attr_name, *attr_value;
		HTML_AttrIterator attributes(element);

		while (attributes.GetNext(attr_name, attr_value, &attr_nsidx))
		{
			if (attr_nsidx == NS_IDX_XMLNS &&
				uni_strlen(attr_name) == name.GetPrefixLength() &&
				uni_strncmp(attr_name, name.GetPrefix(), name.GetPrefixLength()) == 0)
			{
				name.SetUri(attr_value, uni_strlen(attr_value));
				return TRUE;
			}
		}

		element = element->ParentActual();
	}

	return FALSE;
}

/* static */ XMLNamespaceDeclaration *
XMLNamespaceDeclaration::IncRef(XMLNamespaceDeclaration *declaration)
{
	if (declaration)
		++declaration->refcount;
	return declaration;
}

/* static */ void
XMLNamespaceDeclaration::DecRef(XMLNamespaceDeclaration *declaration)
{
	if (declaration && --declaration->refcount == 0)
		OP_DELETE(declaration);
}

XMLNamespaceDeclaration *
XMLNamespaceDeclaration::Reference::operator= (XMLNamespaceDeclaration *new_declaration)
{
	if (declaration != new_declaration)
	{
		XMLNamespaceDeclaration *old_declaration = declaration;
		declaration = XMLNamespaceDeclaration::IncRef (new_declaration);
		XMLNamespaceDeclaration::DecRef (old_declaration);
	}

	return declaration;
}

#ifdef XMLUTILS_XMLNAMESPACENORMALIZER_SUPPORT

XMLNamespaceNormalizer::XMLNamespaceNormalizer(BOOL copy_strings, BOOL perform_normalization)
	: copy_strings(copy_strings),
	  perform_normalization(perform_normalization),
	  depth(0),
	  attributes(NULL),
	  attributes_count(0),
	  attributes_total(0)
{
}

XMLNamespaceNormalizer::~XMLNamespaceNormalizer()
{
	Reset();
	for (unsigned index = 0; index < attributes_total; ++index)
		OP_DELETE(attributes[index]);
	OP_DELETEA(attributes);
}

OP_STATUS
XMLNamespaceNormalizer::StartElement(const XMLCompleteName &name)
{
	Reset();

	++depth;

	if (copy_strings)
		return element_name.Set(name);
	else
	{
		element_name = name;
		return OpStatus::OK;
	}
}

OP_STATUS
XMLNamespaceNormalizer::AddAttribute(const XMLCompleteName &name0, const uni_char *value, BOOL overwrite_existing)
{
	XMLCompleteName name;

	if (name0.GetPrefix() ? uni_strcmp(name0.GetPrefix(), "xmlns") == 0 : uni_strcmp(name0.GetLocalPart(), "xmlns") == 0)
		name = XMLCompleteName(XMLNSURI_XMLNS, name0.GetPrefix(), name0.GetLocalPart());
	else
		name = name0;

	Attribute **attribute_ptr;

	for (unsigned index = 0; index < attributes_count; ++index)
		if (static_cast<const XMLExpandedName &>(name) == attributes[index]->name)
			if (overwrite_existing)
			{
				attribute_ptr = &attributes[index];
				goto set_attribute;
			}
			else
				return OpStatus::ERR;

	if (attributes_count == attributes_total)
	{
		unsigned new_attributes_total = attributes_total ? attributes_total + attributes_total : 8;
		Attribute **new_attributes = OP_NEWA(Attribute *, new_attributes_total);
		if (!new_attributes)
			return OpStatus::ERR_NO_MEMORY;
		op_memcpy(new_attributes, attributes, attributes_total * sizeof(Attribute *));
		op_memset(new_attributes + attributes_total, 0, (new_attributes_total - attributes_total) * sizeof(Attribute *));
		attributes_total = new_attributes_total;
		OP_DELETEA(attributes);
		attributes = new_attributes;
	}

	attribute_ptr = &attributes[attributes_count];

	if (!*attribute_ptr && !(*attribute_ptr = OP_NEW(Attribute, ())))
		return OpStatus::ERR_NO_MEMORY;

	++attributes_count;

 set_attribute:
	Attribute *&attribute = *attribute_ptr;

	if (copy_strings)
	{
		RETURN_IF_ERROR(attribute->name.Set(name));
		RETURN_IF_ERROR(UniSetStr(const_cast<uni_char *&>(attribute->value), value));
	}
	else
	{
		attribute->name = name;
		attribute->value = value;
	}

	return OpStatus::OK;
}

BOOL
XMLNamespaceNormalizer::RemoveAttribute(const XMLExpandedName &name)
{
	for (unsigned index = 0; index < attributes_count; ++index)
		if (name == attributes[index]->name)
		{
			Attribute *attribute = attributes[index];
			attribute->name = XMLCompleteName();
			if (copy_strings)
			{
				uni_char *non_const = const_cast<uni_char *>(attribute->value);
				OP_DELETEA(non_const);
			}
			attribute->value = NULL;
			op_memmove(attributes + index, attributes + index + 1, (attributes_count - (index + 1)) * sizeof(Attribute *));
			attributes[--attributes_count] = attribute;
			return TRUE;
		}
	return FALSE;
}

OP_STATUS
XMLNamespaceNormalizer::FinishAttributes()
{
	unsigned attribute_index = 0;

 again:
	XMLNamespaceDeclaration::Pop(ns, depth);

	for (unsigned index = 0; index < attributes_count; ++index)
		RETURN_IF_ERROR(XMLNamespaceDeclaration::ProcessAttribute(ns, attributes[index]->name, attributes[index]->value, uni_strlen(attributes[index]->value), depth));

	if (!perform_normalization)
		return OpStatus::OK;

	if (attribute_index == 0)
	{
		XMLNamespaceDeclaration *decl = XMLNamespaceDeclaration::FindDeclaration(ns, element_name.GetPrefix());

		if (element_name.GetUri())
		{
			/* Element should have a namespace URI, so we must make sure a
			   declaration is there to provide it. */

			BOOL add_declaration = FALSE, remove_declaration = FALSE;

			if (!decl || !decl->GetUri())
				/* There is no declaration (or the default namespace has been
				   "undeclared").  Just add one. */
				add_declaration = TRUE;
			else if (uni_strcmp(decl->GetUri(), element_name.GetUri()) != 0)
				/* There was a declaration but it set the wrong URI.  If it's
				   local, try simply removing it.  If that helps, it means the
				   local declaration was overriding an inherited, better,
				   declaration.  It it doesn't help, the next time around we'll
				   try something else. */
				if (decl->GetLevel() == depth)
					remove_declaration = TRUE;
				else
					add_declaration = TRUE;

			if (add_declaration)
			{
				XMLCompleteName name;

				if (element_name.GetPrefix())
					name = XMLCompleteName(XMLNSURI_XMLNS, UNI_L("xmlns"), element_name.GetPrefix());
				else
					name = XMLCompleteName(XMLNSURI_XMLNS, NULL, UNI_L("xmlns"));

				RETURN_IF_ERROR(AddAttribute(name, element_name.GetUri(), TRUE));
				goto again;
			}

			if (remove_declaration)
			{
				RemoveAttribute(XMLExpandedName(XMLNSURI_XMLNS, element_name.GetPrefix() ? element_name.GetPrefix() : UNI_L("xmlns")));
				goto again;
			}
		}
		else
		{
			/* Element should not have a namespace URI, so we must remove the
			   current declaration if there is one. */
			if (decl && decl->GetUri())
			{
				if (decl->GetLevel() < depth)
					/* A default namespace declaration is inherited from our ancestors, so
					   we need to undeclare it. */
					RETURN_IF_ERROR(AddAttribute(XMLCompleteName(XMLNSURI_XMLNS, NULL, UNI_L("xmlns")), UNI_L(""), TRUE));
				else
					/* Remove any default namespace declaration attribute from this
					   element. */
					RemoveAttribute(XMLExpandedName(XMLNSURI_XMLNS, UNI_L("xmlns")));
				goto again;
			}
		}
	}

	while (attribute_index < attributes_count)
	{
		XMLCompleteName &attribute_name = attributes[attribute_index++]->name;

		if (attribute_name.GetUri() && !attribute_name.IsXML() && !attribute_name.IsXMLNamespaces())
		{
			/* The attribute name has a namespace URI that we might have to
			   declare. */
			if (!attribute_name.GetPrefix() || !XMLNamespaceDeclaration::IsDeclared(ns, attribute_name.GetPrefix(), attribute_name.GetUri()))
			{
				const uni_char *found_prefix;

				if ((found_prefix = XMLNamespaceDeclaration::FindPrefix(ns, attribute_name.GetUri())) != NULL)
					/* Found a prefix mapped to the attribute name's URI. */
					RETURN_IF_ERROR(attribute_name.SetPrefix(found_prefix));
				else
				{
					if (!attribute_name.GetPrefix() || XMLNamespaceDeclaration::FindDeclaration(ns, attribute_name.GetPrefix()))
					{
						/* The attribute name has a namespace URI but no prefix,
						   or it has a prefix but that prefix has been
						   associated with another URI already.  Invent a new
						   prefix. */

						unsigned nsindex = 1;
						uni_char generated_prefix[2 + 10 + 1]; /* ARRAY OK jl 2007-10-20 */

						do
							uni_sprintf(generated_prefix, UNI_L("NS%u"), nsindex++);
						while (XMLNamespaceDeclaration::FindDeclaration(ns, generated_prefix));

						RETURN_IF_ERROR(attribute_name.SetPrefix(generated_prefix));
					}

					/* The attribute now has a prefix that is not associated
					   with a URI already.  Add such an association. */
					RETURN_IF_ERROR(AddAttribute(XMLCompleteName(XMLNSURI_XMLNS, UNI_L("xmlns"), attribute_name.GetPrefix()), attribute_name.GetUri(), TRUE));
					goto again;
				}
			}
		}
	}

	return OpStatus::OK;
}

void
XMLNamespaceNormalizer::EndElement()
{
	XMLNamespaceDeclaration::Pop(ns, depth--);
	Reset();
}

void
XMLNamespaceNormalizer::Reset()
{
	for (unsigned index = 0; index < attributes_count; ++index)
	{
		attributes[index]->name = XMLCompleteName();
		if (copy_strings)
		{
			uni_char *non_const = const_cast<uni_char *>(attributes[index]->value);
			OP_DELETEA(non_const);
		}
		attributes[index]->value = NULL;
	}
	element_name = XMLCompleteName();
	attributes_count = 0;
}

#endif // XMLUTILS_XMLNAMESPACENORMALIZER_SUPPORT
