// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "modules/encodings/detector/charsetdetector.h"
#include "modules/xmlutils/xmldocumentinfo.h"

#include "xmlparser_tracker.h"

#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/desktop_util/adt/hashiterator.h"

using namespace M2XMLUtils;

//****************************************************************************
//
//	XMLAttributeQN
//
//****************************************************************************

XMLAttributeQN::XMLAttributeQN()
{}


XMLAttributeQN::~XMLAttributeQN()
{}


OP_STATUS XMLAttributeQN::Init ( const OpStringC& namespace_uri,
                                 const OpStringC& local_name, const OpStringC& qualified_name,
                                 const OpStringC& value )
{
	RETURN_IF_ERROR ( m_namespace_uri.Set ( namespace_uri ) );
	RETURN_IF_ERROR ( m_local_name.Set ( local_name ) );
	RETURN_IF_ERROR ( m_qualified_name.Set ( qualified_name ) );
	RETURN_IF_ERROR ( m_value.Set ( value ) );

	return OpStatus::OK;
}


//****************************************************************************
//
//	XMLElement
//
//****************************************************************************

XMLElement::XMLElement ( XMLParserTracker& owner )
		:	m_owner ( owner )
{}


XMLElement::~XMLElement()
{}


OP_STATUS XMLElement::Init ( const OpStringC& namespace_uri,
                             const OpStringC& local_name, const XMLToken::Attribute* attributes,
                             UINT attributes_count )
{
	RETURN_IF_ERROR ( m_namespace_uri.Set ( namespace_uri ) );
	RETURN_IF_ERROR ( m_local_name.Set ( local_name ) );
	RETURN_IF_ERROR ( m_owner.QualifiedName ( m_qualified_name, m_namespace_uri, m_local_name ) );

	// Loop the passed attributes and create new XMLAttributeQN objects
	// from them.
	for ( UINT index = 0; index < attributes_count; ++index )
	{
		const XMLCompleteNameN& attrname = attributes[index].GetName();

		OpString namespace_uri;
		OpString attribute_name;
		OpString attribute_value;

		RETURN_IF_ERROR ( namespace_uri.Set ( attrname.GetUri(), attrname.GetUriLength() ) );
		RETURN_IF_ERROR ( attribute_name.Set ( attrname.GetLocalPart(), attrname.GetLocalPartLength() ) );
		RETURN_IF_ERROR ( attribute_value.Set ( attributes[index].GetValue(), attributes[index].GetValueLength() ) );

		if ( attrname.GetNsIndex() == NS_IDX_XMLNS )
		{
			if ( !m_owner.HasNamespacePrefix ( attribute_value ) )
			{
				// The user have not set a specific prefix for this uri, so we use
				// the prefix the xml creator desired.
				RETURN_IF_ERROR ( m_owner.AddNamespacePrefix ( attribute_name, attribute_value ) );
			}
		}

		OpAutoPtr<XMLAttributeQN> attribute_qn ( OP_NEW(XMLAttributeQN, ()) );
		if ( attribute_qn.get() == 0 )
			return OpStatus::ERR_NO_MEMORY;

		OpString qualified_name;
		RETURN_IF_ERROR ( m_owner.QualifiedName ( qualified_name, namespace_uri, attribute_name ) );

		RETURN_IF_ERROR ( attribute_qn->Init ( namespace_uri, attribute_name, qualified_name, attribute_value ) );
		RETURN_IF_ERROR ( m_attributes.Add ( attribute_qn->QualifiedName().CStr(), attribute_qn.get() ) );

		attribute_qn.release();
	}

	// Check for xml:base attribute.
	if ( m_attributes.Contains ( UNI_L ( "xml:base" ) ) )
	{
		XMLAttributeQN* attribute = 0;
		RETURN_IF_ERROR ( m_attributes.GetData ( UNI_L ( "xml:base" ), &attribute ) );

		OP_ASSERT ( attribute != 0 );
		const OpStringC& value = attribute->Value();

		// Convert the value into an absolute uri.
		OpString base_uri;
		RETURN_IF_ERROR ( m_owner.BaseURI ( base_uri ) );

		if ( base_uri.HasContent() )
			RETURN_IF_ERROR ( OpMisc::RelativeURItoAbsoluteURI ( m_base_uri, value, base_uri ) );
		else
			RETURN_IF_ERROR ( m_base_uri.Set ( value ) );
	}

	return OpStatus::OK;
}


//****************************************************************************
//
//	XMLNamespacePrefix
//
//****************************************************************************

XMLNamespacePrefix::XMLNamespacePrefix()
{}


XMLNamespacePrefix::~XMLNamespacePrefix()
{}


OP_STATUS XMLNamespacePrefix::Init ( const OpStringC& prefix, const OpStringC& uri )
{
	RETURN_IF_ERROR ( m_prefix.Set ( prefix ) );
	RETURN_IF_ERROR ( m_uri.Set ( uri ) );

	return OpStatus::OK;
}


//****************************************************************************
//
//	XMLParserTracker
//
//****************************************************************************

BOOL XMLParserTracker::HasNamespacePrefix ( const OpStringC& uri ) const
{
	XMLParserTracker* non_const_this = ( XMLParserTracker * ) ( this );
	return non_const_this->m_namespace_prefixes.Contains ( uri.CStr() );
}


OP_STATUS XMLParserTracker::AddNamespacePrefix ( const OpStringC& prefix,
        const OpStringC& uri )
{
	OP_ASSERT ( !HasNamespacePrefix ( uri ) );

	OpAutoPtr<XMLNamespacePrefix> namespace_prefix ( OP_NEW(XMLNamespacePrefix, ()) );
	if ( namespace_prefix.get() == 0 )
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR ( namespace_prefix->Init ( prefix, uri ) );
	RETURN_IF_ERROR ( m_namespace_prefixes.Add ( namespace_prefix->URI().CStr(),
	                  namespace_prefix.get() ) );

	namespace_prefix.release();
	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::NamespacePrefix ( OpString& prefix, const OpStringC& uri ) const
{
	XMLParserTracker* non_const_this = ( XMLParserTracker * ) ( this );

	XMLNamespacePrefix* namespace_prefix = 0;
	non_const_this->m_namespace_prefixes.GetData ( uri.CStr(), &namespace_prefix );

	if ( namespace_prefix != 0 )
		RETURN_IF_ERROR ( prefix.Set ( namespace_prefix->Prefix() ) );

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::QualifiedName ( OpString& qualified_name,
        const OpStringC& namespace_uri, const OpStringC& local_name ) const
{
	if ( namespace_uri.HasContent() )
	{
		OpString prefix;
		RETURN_IF_ERROR ( NamespacePrefix ( prefix, namespace_uri ) );

		if ( prefix.HasContent() )
		{
			qualified_name.Empty();
			RETURN_IF_ERROR ( qualified_name.AppendFormat ( UNI_L ( "%s:%s" ),
			                  prefix.CStr(), local_name.CStr() ) );
		}
	}

	if ( qualified_name.IsEmpty() )
		RETURN_IF_ERROR ( qualified_name.Set ( local_name ) );

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::Parse ( const char* buffer, UINT buffer_length,
                                    BOOL is_final )
{
	OpString8 encoding;

	// Detect the encoding used if this is the first parse attempt.
	if ( m_xml_parser.get() == 0 )
	{
		RETURN_IF_ERROR ( encoding.Set ( CharsetDetector::GetXMLEncoding ( buffer, buffer_length ) ) );

		if ( encoding.IsEmpty() )
			RETURN_IF_ERROR ( encoding.Set ( "utf-8" ) );

		// Check if we have a BOM (Byte-order-mark) at the start of utf-8. This has no influence,
		// but is allowed at the start of a UTF-8 file
		if (!encoding.Compare("utf-8") && buffer_length >= 3 && !op_strncmp(buffer, "\xef\xbb\xbf", 3))
		{
			buffer += 3;
			buffer_length -= 3;
		}
	}
	else
		RETURN_IF_ERROR ( Encoding ( encoding ) );

	// Convert the data to utf16.
	// This is broken in so many ways, but try to work around most of it by
	// passing the buffer size directly (compare DSK-270822).
	OpStringC8 input_buffer(buffer);

	OpString utf16_buffer;
	RETURN_IF_ERROR ( MessageEngine::GetInstance()->GetInputConverter().ConvertToChar16 ( encoding, input_buffer, utf16_buffer, TRUE, FALSE, buffer_length ) );

	// Parse it as utf16.
	RETURN_IF_ERROR ( Parse ( utf16_buffer.CStr(), utf16_buffer.Length(), is_final ) );
	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::Parse ( const uni_char *buffer, UINT buffer_length,
                                    BOOL is_final )
{
	if ( m_xml_parser.get() == 0 )
		RETURN_IF_ERROR ( ResetParser() );

	const OP_STATUS parse_status = m_xml_parser->Parse ( buffer, buffer_length, !is_final );

	if ( OpStatus::IsError ( parse_status ) ||
	        is_final || m_xml_parser->IsFailed() )
	{
		RETURN_IF_ERROR ( ResetParser() );
	}

	RETURN_IF_ERROR ( parse_status );
	return OpStatus::OK;
}


const OpStringC& XMLParserTracker::ParentName() const
{
	OP_ASSERT ( HasParent() );
	return m_element_stack.Get ( 1 )->QualifiedName();
}


const OpStringC& XMLParserTracker::GrandparentName() const
{
	OP_ASSERT ( HasGrandparent() );
	return m_element_stack.Get ( 2 )->QualifiedName();
}


BOOL XMLParserTracker::IsChildOf ( const OpStringC& parent_name ) const
{
	BOOL is_child = FALSE;

	if ( HasParent() )
	{
		if ( ParentName().Compare ( parent_name ) == 0 )
			is_child = TRUE;
	}

	return is_child;
}


BOOL XMLParserTracker::IsGrandchildOf ( const OpStringC& grandparent_name ) const
{
	BOOL is_grandchild = FALSE;

	if ( HasGrandparent() )
	{
		if ( GrandparentName().Compare ( grandparent_name ) == 0 )
			is_grandchild = TRUE;
	}

	return is_grandchild;
}


BOOL XMLParserTracker::IsDescendantOf ( const OpStringC& descendant_name ) const
{
	BOOL is_descendant = FALSE;

	const UINT stack_size = m_element_stack.GetCount();
	for ( UINT index = 1; index < stack_size; ++index )
	{
		if ( m_element_stack.Get ( index )->QualifiedName().Compare ( descendant_name ) == 0 )
		{
			is_descendant = TRUE;
			break;
		}
	}

	return is_descendant;
}


OP_STATUS XMLParserTracker::BaseURI ( OpString& base_uri,
                                      const OpStringC& document_uri ) const
{
	for ( UINT index = 0, stack_size = m_element_stack.GetCount();
	        index < stack_size; ++index )
	{
		XMLElement* current_element = m_element_stack.Get ( index );
		OP_ASSERT ( current_element != 0 );

		if ( current_element->HasBaseURI() )
		{
			RETURN_IF_ERROR ( base_uri.Set ( current_element->BaseURI() ) );
			break;
		}
	}

	if ( base_uri.IsEmpty() && document_uri.HasContent() )
		RETURN_IF_ERROR ( base_uri.Set ( document_uri ) );

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::Version ( OpString8& version ) const
{
	if ( m_xml_parser.get() == 0 )
		return OpStatus::ERR_NULL_POINTER;

	const XMLDocumentInformation& xmldocinfo = m_xml_parser->GetDocumentInformation();
	if ( xmldocinfo.GetXMLDeclarationPresent() )
	{
		if ( xmldocinfo.GetVersion() == XMLVERSION_1_0 )
			RETURN_IF_ERROR ( version.Set ( "1.0" ) );
		else if ( xmldocinfo.GetVersion() == XMLVERSION_1_1 )
			RETURN_IF_ERROR ( version.Set ( "1.1" ) );
	}

	if ( version.IsEmpty() )
		RETURN_IF_ERROR ( version.Set ( "1.0" ) );

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::Encoding ( OpString8& encoding ) const
{
	if ( m_xml_parser.get() == 0 )
		return OpStatus::ERR_NULL_POINTER;

	const XMLDocumentInformation& xmldocinfo = m_xml_parser->GetDocumentInformation();
	if ( xmldocinfo.GetXMLDeclarationPresent() )
		RETURN_IF_ERROR ( encoding.Set ( xmldocinfo.GetEncoding() ) );

	if ( encoding.IsEmpty() )
		RETURN_IF_ERROR ( encoding.Set ( "utf-8" ) );

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::SetKeepTags ( const OpStringC& namespace_uri,
        const OpStringC& local_name, BOOL ignore_first_tag )
{
	OP_ASSERT ( !IsKeepingTags() );

	if ( IsKeepingTags() )
		return OpStatus::ERR;

	m_keep_tags = OP_NEW(KeepTags, ( ignore_first_tag ));
	if ( m_keep_tags.get() == 0 )
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR ( m_keep_tags->Init ( namespace_uri, local_name ) );
	return OpStatus::OK;
}


XMLParserTracker::XMLParserTracker()
		:	m_xml_parser ( 0 )
{
	m_content_string.SetExpansionPolicy ( TempBuffer::AGGRESSIVE );
	m_content_string.SetCachedLengthPolicy ( TempBuffer::TRUSTED );
}


XMLParserTracker::~XMLParserTracker()
{}


OP_STATUS XMLParserTracker::Init()
{
	// The xml namespace does not need to be declared before use; thus we add
	// it to our internal namespace list here.
	RETURN_IF_ERROR ( AddNamespacePrefix ( UNI_L ( "xml" ),
	                                       UNI_L ( "http://www.w3.org/XML/1998/namespace" ) ) );

	return OpStatus::OK;
}


XMLTokenHandler::Result XMLParserTracker::HandleToken ( XMLToken &token )
{
	OP_STATUS status = OpStatus::OK;

	switch ( token.GetType() )
	{
		case XMLToken::TYPE_CDATA :
		{
			status = HandleCDATAToken ( token );
			break;
		}
		case XMLToken::TYPE_Text :
		{
			status = HandleTextToken ( token );
			break;
		}
		case XMLToken::TYPE_STag :
		{
			status = HandleStartTagToken ( token );
			break;
		}
		case XMLToken::TYPE_ETag :
		{
			status = HandleEndTagToken ( token );
			break;
		}
		case XMLToken::TYPE_EmptyElemTag :
		{
			status = HandleStartTagToken ( token );
			if ( OpStatus::IsSuccess ( status ) )
				status = HandleEndTagToken ( token );

			break;
		}
	}

	if ( OpStatus::IsMemoryError ( status ) )
		return XMLTokenHandler::RESULT_OOM;
	else if ( OpStatus::IsError ( status ) )
		return XMLTokenHandler::RESULT_ERROR;

	return XMLTokenHandler::RESULT_OK;
}


OP_STATUS XMLParserTracker::HandleStartTagToken ( const XMLToken& token )
{
	// Fetch information about the element.
	const XMLCompleteNameN &elemname = token.GetName();

	OpString namespace_uri;
	OpString element_name;

	RETURN_IF_ERROR ( namespace_uri.Set ( elemname.GetUri(), elemname.GetUriLength() ) );
	RETURN_IF_ERROR ( element_name.Set ( elemname.GetLocalPart(), elemname.GetLocalPartLength() ) );

	// Process the element.
	OpAutoPtr<XMLElement> element ( OP_NEW(XMLElement, ( *this )) );
	if ( element.get() == 0 )
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR ( element->Init ( namespace_uri, element_name,
	                                  token.GetAttributes(), token.GetAttributesCount() ) );

	if ( IsKeepingTags() )
	{
		if ( m_keep_tags->KeepThisTag() )
		{
			// Create a string with the attributes.
			OpString attribute_string;

			for (StringHashIterator<XMLAttributeQN> it(element->Attributes()); it; it++)
			{
				XMLAttributeQN* attribute = it.GetData();
				OP_ASSERT ( attribute != 0 );

				RETURN_IF_ERROR ( attribute_string.AppendFormat ( UNI_L ( "%s=\"%s\" " ),
				                  attribute->QualifiedName().CStr(),
				                  attribute->Value().CStr() ) );
			}

			attribute_string.Strip();
			if ( attribute_string.HasContent() )
			{
				RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( "<" ) ) );
				RETURN_IF_ERROR ( m_content_string.Append ( element_name.CStr() ) );
				RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( " " ) ) );
				RETURN_IF_ERROR ( m_content_string.Append ( attribute_string.CStr() ) );
				RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( ">" ) ) );
			}
			else
			{
				RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( "<" ) ) );
				RETURN_IF_ERROR ( m_content_string.Append ( element_name.CStr() ) );
				RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( ">" ) ) );
			}
		}

		m_keep_tags->IncTagDepth();
	}
	else
	{
		RETURN_IF_ERROR ( PushElementStack ( element.get() ) );
		const OP_STATUS status = OnStartElement ( element->NamespaceURI(),
		                         element->LocalName(), element->QualifiedName(), element->Attributes() );

		element.release();
		m_content_string.Clear();
		m_content_string.SetExpansionPolicy ( TempBuffer::AGGRESSIVE );
		m_content_string.SetCachedLengthPolicy ( TempBuffer::TRUSTED );

		RETURN_IF_ERROR ( status );
	}

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::HandleEndTagToken ( const XMLToken& token )
{
	// Fetch information about the element.
	const XMLCompleteNameN &elemname = token.GetName();

	OpString namespace_uri;
	OpString element_name;

	RETURN_IF_ERROR ( namespace_uri.Set ( elemname.GetUri(), elemname.GetUriLength() ) );
	RETURN_IF_ERROR ( element_name.Set ( elemname.GetLocalPart(), elemname.GetLocalPartLength() ) );

	// Process the element.
	if ( IsKeepingTags() )
	{
		if ( m_keep_tags->IsEndTag ( namespace_uri, element_name ) )
		{
			RETURN_IF_ERROR ( OnCharacterData ( m_content_string.GetStorage() ) );
			m_keep_tags = 0;
		}
		else
		{
			m_keep_tags->DecTagDepth();
			if ( m_keep_tags->KeepThisTag() )
			{
				if ( token.GetType() == XMLToken::TYPE_EmptyElemTag )
				{
					m_content_string.GetStorage() [m_content_string.Length() - 1] = 0x2f; // '/'
					RETURN_IF_ERROR ( m_content_string.Append ( 0x3e ) ); // '>'
				}
				else
				{
					RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( "</" ) ) );
					RETURN_IF_ERROR ( m_content_string.Append ( element_name.CStr() ) );
					RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( ">" ) ) );
				}
			}
		}
	}

	if ( !IsKeepingTags() )
	{
		ElementStackPopper stack_popper ( *this ); // Pop stack at the end.

		XMLElement* element = 0;
		RETURN_IF_ERROR ( TopElementStack ( element ) );

		RETURN_IF_ERROR ( OnEndElement ( element->NamespaceURI(),
		                                 element->LocalName(), element->QualifiedName() , element->Attributes()) );
	}

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::HandleTextToken ( const XMLToken& token )
{
	uni_char* allocated_value = token.GetLiteralAllocatedValue();
	if ( allocated_value == 0 )
		return OpStatus::ERR_NO_MEMORY;

	ANCHOR_ARRAY ( uni_char, allocated_value );

	if ( IsKeepingTags() &&
	        token.GetType() != XMLToken::TYPE_CDATA )
	{
		// When we are keeping the tags we want content containing '&', '<'
		// and '>' to be escaped.

		// Walk through string, find occurrences of characters we want to replace
		uni_char* start_scan = allocated_value;
		uni_char* scanner    = allocated_value;

		while ( *scanner )
		{
			switch ( *scanner )
			{
				case 0x26: // '&'
					RETURN_IF_ERROR ( m_content_string.Append ( start_scan, scanner - start_scan ) );
					RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( "&amp;" ) ) );
					start_scan = scanner + 1;
					break;
				case 0x3c: // '<'
					RETURN_IF_ERROR ( m_content_string.Append ( start_scan, scanner - start_scan ) );
					RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( "&lt;" ) ) );
					start_scan = scanner + 1;
					break;
				case 0x3e: // '>'
					RETURN_IF_ERROR ( m_content_string.Append ( start_scan, scanner - start_scan ) );
					RETURN_IF_ERROR ( m_content_string.Append ( UNI_L ( "&gt;" ) ) );
					start_scan = scanner + 1;
					break;
			}
			scanner++;
		}

		RETURN_IF_ERROR ( m_content_string.Append ( start_scan, scanner - start_scan ) );
	}
	else
	{
		RETURN_IF_ERROR ( m_content_string.Append ( allocated_value ) );
		RETURN_IF_ERROR ( OnCharacterData ( allocated_value ) );
	}

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::HandleCDATAToken ( const XMLToken& token )
{
	if ( IsKeepingTags() )
	{
		RETURN_IF_ERROR ( m_content_string.Append ( "<![CDATA[" ) );
		RETURN_IF_ERROR ( OnCharacterData ( UNI_L ( "<![CDATA[" ) ) );

		RETURN_IF_ERROR ( HandleTextToken ( token ) );

		RETURN_IF_ERROR ( m_content_string.Append ( "]]>" ) );
		RETURN_IF_ERROR ( OnCharacterData ( UNI_L ( "]]>" ) ) );
	}
	else
	{
		RETURN_IF_ERROR ( OnStartCDATASection() );
		RETURN_IF_ERROR ( HandleTextToken ( token ) );
		RETURN_IF_ERROR ( OnEndCDATASection() );
	}

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::ResetParser()
{
	// Recreate / initialize the xml parser.
	if ( m_xml_parser.get() != 0 )
		m_xml_parser = 0;

	{
		URL empty_url;

		XMLParser* new_xml_parser = 0;
		RETURN_IF_ERROR ( XMLParser::Make ( new_xml_parser, 0, g_main_message_handler, this, empty_url ) );

		m_xml_parser = new_xml_parser;
	}

	XMLParser::Configuration configuration;
	configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	configuration.max_tokens_per_call = 4096;

	m_xml_parser->SetConfiguration ( configuration );

	// Clear / reset the internal variables.
	m_element_stack.DeleteAll();
	m_content_string.Clear();
	m_content_string.SetExpansionPolicy ( TempBuffer::AGGRESSIVE );
	m_content_string.SetCachedLengthPolicy ( TempBuffer::TRUSTED );
	m_keep_tags = 0;

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::PushElementStack ( XMLElement* element )
{
	OP_ASSERT ( element != 0 );
	RETURN_IF_ERROR ( m_element_stack.Insert ( 0, element ) );

	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::PopElementStack()
{
	OP_ASSERT ( ElementStackHasContent() );
	if ( !ElementStackHasContent() )
		return OpStatus::ERR;

	m_element_stack.Delete ( 0, 1 );
	return OpStatus::OK;
}


OP_STATUS XMLParserTracker::TopElementStack ( XMLElement*& element ) const
{
	OP_ASSERT ( ElementStackHasContent() );
	if ( !ElementStackHasContent() )
		return OpStatus::ERR;

	element = m_element_stack.Get ( 0 );
	return OpStatus::OK;
}


//****************************************************************************
//
//	XMLParserTracker::KeepTags
//
//****************************************************************************

XMLParserTracker::KeepTags::KeepTags ( BOOL ignore_first_tag )
		:	m_tag_depth ( 0 ),
		m_ignore_first_tag ( ignore_first_tag )
{}


OP_STATUS XMLParserTracker::KeepTags::Init ( const OpStringC& namespace_uri,
        const OpStringC& local_name )
{
	RETURN_IF_ERROR ( m_namespace_uri.Set ( namespace_uri ) );
	RETURN_IF_ERROR ( m_local_name.Set ( local_name ) );

	return OpStatus::OK;
}


BOOL XMLParserTracker::KeepTags::IsEndTag ( const OpStringC& namespace_uri,
        const OpStringC& local_name ) const
{
	return ( m_tag_depth == 0 ) &&
	       ( m_namespace_uri == namespace_uri ) &&
	       ( m_local_name == local_name );
}


UINT XMLParserTracker::KeepTags::DecTagDepth()
{
	OP_ASSERT ( m_tag_depth > 0 );
	if ( m_tag_depth > 0 )
		--m_tag_depth;

	return m_tag_depth;
}

#endif //M2_SUPPORT
