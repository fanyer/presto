/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLUTILS_CAPABILITIES_H
#define XMLUTILS_CAPABILITIES_H

/* The header xmldocumentinfo.h and the class XMLDocumentInfo are
   supported. */
#define XMLUTILS_CAP_XMLDOCUMENTINFO

/* The class XMLSerializer::Configuration is supported. */
#define XMLUTILS_CAP_XMLSERIALIZER_CONFIGURATION

/* The class XMLParser::Configuration is supported. */
#define XMLUTILS_CAP_XMLPARSER_CONFIGURATION

/* The class XMLTokenBackend is supported. */
#define XMLUTILS_CAP_XMLTOKENBACKEND

/* The function XMLDocumentInformation::GetDefaultNamespaceURI is supported. */
#define XMLUTILS_CAP_XMLDOCUMENTINFO_GETDEFAULTNAMESPACEURI

/* The function XMLDocumentInformation::GetRootElementName is supported. */
#define XMLUTILS_CAP_XMLDOCUMENTINFO_GETROOTELEMENTNAME

/* The option XMLParser::Configuration::store_internal_subset is supported. */
#define XMLUTILS_CAP_STOREINTERNALSUBSET

/* The function XMLParser::GetURL is supported. */
#define XMLUTILS_CAP_XMLPARSER_GETURL

/* The function XMLNamespaceDeclaration::PushAllInScope is supported. */
#define XMLUTILS_CAP_XMLNSDECL_PUSHALLINSCOPE

/* The function XMLToken::Attribute::OwnsValue is supported. */
#define XMLUTILS_CAP_XMLTOKEN_ATTR_OWNSVALUE

/* The parser can report how much it consumed instead of consumed everything. */
#define XMLUTILS_CAP_XMLPARSER_CONSUMED

/* The function XMLParser::IsPaused is supported. */
#define XMLUTILS_CAP_XMLPARSER_ISPAUSED

/* The option XMLParser::Configuration::max_tokens_per_call is supported. */
#define XMLUTILS_CAP_MAX_TOKENS_PER_CALL

/* The class XMLErrorReport is supported. */
#define XMLUTILS_CAP_XMLERRORREPORT

/* The function XMLFragment::Parse(OpFileDescriptor *fd, const char *charset) is supported. */
#define XMLUTILS_CAP_XMLFRAGMENT_PARSE_OPFILEDESCRIPTOR

/* The function XMLToken::RemoveAttribute is supported. */
#define XMLUTILS_CAP_XMLTOKEN_REMOVEATTRIBUTE

/* The function XMLParser::ReportConsoleMessage is supported. */
#define XMLUTILS_CAP_XMLPARSER_REPORTCONSOLEERROR

/* The functions XMLDocumentInformation::GetKnownDefaultAttribute{,Count} are supported. */
#define XMLUTILS_CAP_XMLDOCUMENTINFO_GETKNOWNDEFAULTATTRIBUTE

/* The function XMLParser::GetErrorDescription is supported. */
#define XMLUTILS_CAP_XMLPARSER_GETERRORDESCRIPTION

/* The function XMLLanguageParser::SetLocation is supported. */
#define XMLUTILS_CAP_XMLLANGUAGEPARSER_SETLOCATION

/* The function XMLParser::Load has a second argument, 'delete_when_finished'. */
#define XMLUTILS_CAP_XMLPARSER_DELETEWHENFINISHED

/* The functions XMLParser::SetOwnsListener and XMLParser::SetOwnsTokenHandler are supported. */
#define XMLUTILS_CAP_XMLPARSER_SETOWNSLISTENER
#define XMLUTILS_CAP_XMLPARSER_SETOWNSTOKENHANDLER

/* The functions XMLToken::GetNameForUpdate and XMLToken::Attribute::GetNameForUpdate are supported. */
#define XMLUTILS_CAP_XMLTOKEN_GETNAMEFORUPDATE

/* Variants of the functions XMLTreeAccessor::GetAttribute, XMLTreeAccessor::GetAttributeIndex
   and XMLTreeAccessor::HasAttribute that take a XMLTreeAccessor::Attributes argument instead
   of a XMLTreeAccessor::Node argument are supported. */
#define XMLUTILS_CAP_XMLTREEACCESSOR_NEW_ATTRIBUTE_ACCESS

/* The function XMLParser::IsBlockedByTokenHandler is supported. */
#define XMLUTILS_CAP_XMLPARSER_ISBLOCKEDBYTOKENHANDLER

/* The function XMLParser::SignalInvalidEncodingError is supported. */
#define XMLUTILS_CAP_SIGNAL_INVALID_ENCODING_ERROR

/* The functions XMLFragment::GetError{Description,Location} are supported. */
#define XMLUTILS_CAP_XMLFRAGMENT_GETERRORDESCRIPTION

/* XMLDataSourceImplGrow has been replaced by GrowL which leaves on OOM */
#define XMLUTILS_CAP_GROW_LEAVES

/* Load has timeout after which loading will abort and failed message returned to listener */
#define XMLUTILS_CAP_HAS_LOADING_TIMEOUT

/* XMLParserImpl::GetLastHTTPResponse is supported */
#define XMLUTILS_CAP_HTTP_RESPONSE_CODE

#endif // XMLUTILS_CAPABILITIES_H
