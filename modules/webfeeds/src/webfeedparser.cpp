// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeed-parsestates.h"
#include "modules/webfeeds/src/webfeedparser.h"
#include "modules/webfeeds/src/xmlelement.h"
#include "modules/webfeeds/src/webfeed.h"
#include "modules/webfeeds/src/webfeedentry.h"
#include "modules/webfeeds/src/webfeedutil.h"
#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeed_load_manager.h"

#include "modules/encodings/decoders/iso-8859-1-decoder.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlnames.h"

#define XML_TOKEN_LITERAL_LENGTH   (512)

WebFeedParserListener::~WebFeedParserListener()
{
}

WebFeedParser::WebFeedParser(WebFeedParserListener* listener,
		BOOL delete_when_done) :
	m_xml_parser(NULL), m_feed(NULL), m_entry(NULL), m_listener(listener),
			m_delete_when_done(delete_when_done), m_parser_is_being_deleted(
					FALSE), m_parsing_started(FALSE), m_is_expecting_more_data(
					FALSE)
{
}

WebFeedParser::~WebFeedParser()
{
	m_parser_is_being_deleted = TRUE;

	if (m_feed)
		m_feed->DecRef();
	if (m_entry)
		m_entry->DecRef();

	m_xml_parser.reset(); // frees it

#ifdef _DEBUG
	m_feed = (WebFeed*)0xdeadbeaf;
	m_entry = (WebFeedEntry*)0xdeadbeaf;
	m_listener = (WebFeedParserListener*)0xdeadbeaf;
#endif
}

OP_STATUS WebFeedParser::Init(const OpString& feed_content_location)
{
	// Store the content location.
	m_feed_content_location.Set(feed_content_location);

	// Init the state member.
	OP_STATUS status = OpStatus::OK;
	TRAP(status, ChangeStateIfNeededL(WebFeedParseState::StateInitial));
	RETURN_IF_ERROR(status);

	// Set namespace prefixes for the namespaces we care about during the
	// parsing.
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("xml"), UNI_L("http://www.w3.org/XML/1998/namespace")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("atom"), UNI_L("http://purl.org/atom/ns#")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("atom"), UNI_L("http://www.w3.org/2005/Atom")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("content"), UNI_L("http://purl.org/rss/1.0/modules/content/")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("dc"), UNI_L("http://purl.org/dc/elements/1.1/")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("rdf"), UNI_L("http://www.w3.org/1999/02/22-rdf-syntax-ns#")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("rss"), UNI_L("http://purl.org/rss/1.0/")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("sy"), UNI_L("http://purl.org/rss/1.0/modules/syndication/")));
	RETURN_IF_ERROR(AddNamespacePrefix(UNI_L("xhtml"), UNI_L("http://www.w3.org/1999/xhtml")));

	RETURN_IF_ERROR(m_content_buffer.SetSize(512));

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::CreateFeed()
{
	if (m_feed) // If we already have a feed object, just continue using it
	{
		m_feed->SetURL(m_feed_url);
		return OpStatus::OK;
	}

	m_feed = OP_NEW(WebFeed, ());
	if (!m_feed)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_feed->Init());
	m_feed->SetURL(m_feed_url);

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::CreateEntry()
{
	OP_ASSERT(!m_entry);
	OP_ASSERT(m_feed);
	if (!m_feed)
		return OpStatus::ERR_NULL_POINTER;

	m_entry = OP_NEW(WebFeedEntry, ());
	if (!m_entry)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_entry->Init());

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::AddEntryToFeed()
{
	OP_ASSERT(m_feed && m_entry);
	if (!m_feed || !m_entry)
		return OpStatus::ERR_NULL_POINTER;

	// Transfer entry to feed:
	BOOL new_added = FALSE;
	WebFeedEntry* actual_entry = NULL;
	RETURN_IF_LEAVE(new_added = m_feed->AddEntryL(m_entry, 0, &actual_entry));
	HandleEntryParsed(*m_entry);

	if (m_listener)
	{
		if (actual_entry)
			m_listener->OnEntryLoaded(actual_entry, OpFeedEntry::STATUS_OK, new_added);
	}

	m_entry->DecRef();
	m_entry = NULL;

	return OpStatus::OK;
}

void WebFeedParser::Continue(XMLParser* parser) {}

void WebFeedParser::Stopped(XMLParser* parser) {}

void WebFeedParser::ParsingStopped(XMLParser *parser)
{
	OP_ASSERT(parser == m_xml_parser.get()); // Should only handle our own parser

	if (m_parser_is_being_deleted) // we stopped the parsing ourselves
		return;

	if (m_is_expecting_more_data && !parser->IsFailed()) // We're not done yet
		return;

	if (m_listener)
	{
		if (!parser->IsFailed())
			m_listener->ParsingDone(m_feed, OpFeed::STATUS_OK);
		else
			m_listener->ParsingDone(m_feed, OpFeed::STATUS_PARSING_ERROR);

		if (m_feed)
			m_feed->DecRef();
		m_feed = NULL;
	}

	// No one will call this parser anymore, delete it
	if (m_delete_when_done)
	{
		m_xml_parser.release(); // it deletes itself, release pointer so that it's not double deleted
		OP_DELETE(this);
	}
}

BOOL WebFeedParser::Redirected(XMLParser *parser)
{
	OP_ASSERT(m_feed_url.Type() == URL_HTTP || m_feed_url.Type() == URL_HTTPS); // no other protocols support redirect?

	if (m_feed_url.GetAttribute(URL::KHTTP_Response_Code, FALSE) == HTTP_MOVED) // store new URL if feed has moved permanently
	{
		m_feed_url = m_feed_url.GetAttribute(URL::KMovedToURL, TRUE);
		if (m_feed)
			m_feed->SetURL(m_feed_url);
	}

	return TRUE;
}

XMLTokenHandler::Result WebFeedParser::HandleToken(XMLToken &token)
{
	OP_STATUS status = OpStatus::OK;

	// Dispatch to proper handler:
	switch (token.GetType())
	{
	case XMLToken::TYPE_CDATA:
		status = HandleCDATAToken(token);
		break;
	case XMLToken::TYPE_Text:
		status = HandleTextToken(token);
		break;
	case XMLToken::TYPE_STag:
		TRAP(status, HandleStartTagTokenL(token));
		break;
	case XMLToken::TYPE_ETag:
		TRAP(status, HandleEndTagTokenL(token));
		break;
	case XMLToken::TYPE_EmptyElemTag:
		TRAP(status, HandleStartTagTokenL(token));
		if (OpStatus::IsSuccess(status))
			TRAP(status, HandleEndTagTokenL(token));
		break;
	case XMLToken::TYPE_Finished:
		break;
	default:
		break;
	}

	if (OpStatus::IsMemoryError(status))
		return XMLTokenHandler::RESULT_OOM;
	else if (OpStatus::IsError(status))
		return XMLTokenHandler::RESULT_ERROR;

	return XMLTokenHandler::RESULT_OK;
}

void WebFeedParser::HandleStartTagTokenL(const XMLToken& token)
{
	if (!m_parsing_started)
	{
		if (m_listener)
			m_listener->ParsingStarted();
		m_parsing_started = TRUE;
	}

	// Fetch information about the element.
	const XMLCompleteNameN &elemname = token.GetName();

	OpString namespace_uri; ANCHOR(OpString, namespace_uri);
	OpString element_name; ANCHOR(OpString, element_name); // XML tag name for the token being processed

	namespace_uri.SetL(elemname.GetUri(), elemname.GetUriLength());
	element_name.SetL(elemname.GetLocalPart(), elemname.GetLocalPartLength());

	// Create an XML-element and insert the data into it
	OpStackAutoPtr<XMLElement> element(OP_NEW_L(XMLElement, (*this)));
	OP_ASSERT(element.get());

	LEAVE_IF_ERROR(element->Init(namespace_uri, element_name,
					token.GetAttributes(), token.GetAttributesCount()));

	if (IsKeepingTags())
	{
		if (m_keep_tags->KeepThisTag())
		{
			// Create a string with the attributes.
			OpString attribute_string; ANCHOR(OpString, attribute_string);

			OpStackAutoPtr<OpHashIterator> iterator(element->Attributes().GetIterator());
			OP_ASSERT(iterator.get());

			// If keeping tags we must add the right qualified name 
			// (i.e. including namespace) for the tag and its attributes
			BOOL done = (iterator->First() == OpStatus::ERR);
			while (!done)
			{
				XMLAttributeQN* attribute = (XMLAttributeQN *) (iterator->GetData());
				OP_ASSERT(attribute);

				LEAVE_IF_ERROR(attribute_string.AppendFormat(UNI_L("%s=\"%s\" "),
								attribute->QualifiedName().CStr(),
								attribute->Value().CStr()));

				done = (iterator->Next() == OpStatus::ERR);
			}

			attribute_string.Strip();
			LEAVE_IF_ERROR(m_content_buffer.EnsureSpace(elemname.GetLocalPartLength() + attribute_string.Length() + 4));
			m_content_buffer.Append(UNI_L("<"), 1);
			if (attribute_string.HasContent())
			{
				m_content_buffer.Append(element_name.CStr(), elemname.GetLocalPartLength());
				m_content_buffer.Append(UNI_L(" "), 1);
				m_content_buffer.Append(attribute_string.CStr(), attribute_string.Length());
			}
			else
			{
				m_content_buffer.Append(element_name.CStr(), elemname.GetLocalPartLength());
			}

			if (token.GetType() == XMLToken::TYPE_EmptyElemTag)
				m_content_buffer.Append(UNI_L("/>"), 2);
			else
				m_content_buffer.Append(UNI_L(">"), 1);
		}
		m_keep_tags->IncTagDepth();
	}
	else
	{
		PushElementStackL(element.get());
		XMLElement* elm = element.release(); // ownership transfered to element stack, but still need pointer a little longer

		m_content_buffer.Clear();

		OP_ASSERT(m_parse_state.get());
		LEAVE_IF_NULL(m_parse_state.get());

		WebFeedParseState::StateType next_state = m_parse_state->Type();

		OpStatus::Ignore(m_parse_state->OnStartElement(elm->NamespaceURI(),
				elm->LocalName(), elm->QualifiedName(), elm->Attributes(),
				next_state));

		if (ChangeStateIfNeededL(next_state))
			m_parse_state->UpdateAncestorCount(AncestorCount());
	}
}

void WebFeedParser::HandleEndTagTokenL(const XMLToken& token)
{
	// Fetch information about the element.
	const XMLCompleteNameN &elemname = token.GetName();

	OpString namespace_uri; ANCHOR(OpString, namespace_uri);
	OpString element_name; ANCHOR(OpString, element_name); // XML tag name of token

	LEAVE_IF_ERROR(namespace_uri.Set(elemname.GetUri(), elemname.GetUriLength()));
	LEAVE_IF_ERROR(element_name.Set(elemname.GetLocalPart(), elemname.GetLocalPartLength()));

	// Process the element.
	if (IsKeepingTags())
	{
		if (m_keep_tags->IsEndTag(namespace_uri, element_name))
		{
			m_keep_tags = NULL;
		}
		else
		{
			m_keep_tags->DecTagDepth();

			if (m_keep_tags->KeepThisTag() && token.GetType() != XMLToken::TYPE_EmptyElemTag)
			{
				LEAVE_IF_ERROR(m_content_buffer.EnsureSpace(elemname.GetLocalPartLength() + 3));
				m_content_buffer.Append(UNI_L("</"), 2);
				m_content_buffer.Append(element_name.CStr(),
						elemname.GetLocalPartLength());
				m_content_buffer.Append(UNI_L(">"), 1);
			}
		}
	}

	if (!IsKeepingTags())
	{
		OpStackAutoPtr<XMLElement> element(PopElementStackL());

		OP_ASSERT(element->LocalName() == element_name); // illegally nested XML, or bug in code...

		LEAVE_IF_NULL(m_parse_state.get());

		WebFeedParseState::StateType next_state = m_parse_state->Type();
		OpStatus::Ignore(m_parse_state->OnEndElement(element->NamespaceURI(),
				element->LocalName(), element->QualifiedName(), next_state));

		BOOL change_status = ChangeStateIfNeededL(next_state);

		if (change_status && m_parse_state.get())
			m_parse_state->UpdateAncestorCount(AncestorCount());
	}
}

OP_STATUS WebFeedParser::HandleTextToken(const XMLToken& token)
{

	XMLToken::Literal literal;
	RETURN_IF_ERROR(token.GetLiteral(literal));

	unsigned nparts = literal.GetPartsCount();
	if (nparts == 0)
		return OpStatus::OK;

	size_t size = (nparts - 1) * XML_TOKEN_LITERAL_LENGTH + literal.GetPartLength(nparts - 1) + 1;
	RETURN_IF_ERROR(m_content_buffer.EnsureSpace(size));

	for (unsigned i = 0; i < nparts; i++)
	{
		// When we are keeping the tags we want content containing '&', '<'
		// and '>' to be escaped.
		if (IsKeepingTags() && token.GetType() != XMLToken::TYPE_CDATA)
		{
			// This is a fairly performance critical part (can be called often and with 
			// lots of data, so make a custom implementation instead of using 
			// existing replace functionallity 

			uni_char escape_char = 0;
			const uni_char* part = literal.GetPart(i);
			size_t start = 0;

			size_t c = start;
			while (c < literal.GetPartLength(i))
			{
				// find first escape able character
				for (; c < literal.GetPartLength(i); c++)
				{
					escape_char = part[c];
	
					if (escape_char == '&' || escape_char == '<' || escape_char == '>')
						break;
				}
	
				// write part up to escape char
				size_t write_len = c - start;
				if (write_len)
					m_content_buffer.Append(part + start, write_len);
	
				start += write_len;

				// Write escape for charactered needed to be escaped
				const uni_char* escape_entity = NULL;
				size_t escape_len = 0;
				switch (escape_char)
				{
				case '&':
					escape_entity = UNI_L("&amp;");
					escape_len = 5;
					break;
				case '<':
					escape_entity = UNI_L("&lt;");
					escape_len = 4;
					break;
				case '>':
					escape_entity = UNI_L("&gt;");
					escape_len = 4;
					break;
				}

				if (escape_entity)
				{
					RETURN_IF_ERROR(m_content_buffer.EnsureSpace(escape_len));
					m_content_buffer.Append(escape_entity, escape_len);
					c++;
					start++;
				}
			}
		}
		else
			m_content_buffer.Append(literal.GetPart(i), literal.GetPartLength(i));
	}

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::HandleCDATAToken(const XMLToken& token)
{
	if (IsKeepingTags())
	{
		RETURN_IF_ERROR(m_content_buffer.EnsureSpace(12));  // estra space needed for CDATA-tag
		m_content_buffer.Append(UNI_L("<![CDATA["), 9);
		RETURN_IF_ERROR(HandleTextToken(token));
		m_content_buffer.Append(UNI_L("]]>"), 3);
	}
	else
		RETURN_IF_ERROR(HandleTextToken(token));

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::HandleEntryParsed(WebFeedEntry& entry)
{
	WebFeedContentElement* content = (WebFeedContentElement*) entry.GetContent();
	RETURN_IF_ERROR(UpdateContentBaseURIIfNeeded(*content));

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::LoadFeed(URL& feed_url, WebFeed* existing_feed)
{
	// Create XML parser if none exist
	RETURN_IF_ERROR(ResetParser(feed_url, existing_feed));

	// Parse URL:
	URL empty_url;
	m_is_expecting_more_data = FALSE;
	RETURN_IF_ERROR(m_xml_parser->Load(empty_url, TRUE));

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::Parse(const char* buffer, UINT buffer_length, BOOL is_final)
{
	uni_char* utf16_buf = OP_NEWA(uni_char, buffer_length + 1);

	if (!utf16_buf)
		return OpStatus::ERR_NO_MEMORY;

	make_doublebyte_in_buffer(buffer, buffer_length, utf16_buf, buffer_length + 1);
	OP_STATUS stat = Parse(utf16_buf, buffer_length, is_final);

	OP_DELETEA(utf16_buf);
	return stat;
}

OP_STATUS WebFeedParser::Parse(const uni_char *buffer, UINT buffer_length, BOOL is_final, unsigned* consumed)
{
	if (IsFailed())
	{
		OP_ASSERT(!"don't call Parse when it has already failed");
		return OpStatus::ERR;
	}

	if (!m_xml_parser.get()) // create parser if none exist
		RETURN_IF_ERROR(ResetParser());

	m_is_expecting_more_data = !is_final;

	// Dispatch to XML parser
	return m_xml_parser->Parse(buffer, buffer_length, m_is_expecting_more_data,	consumed);
}

OP_STATUS WebFeedParser::ResetParser(URL url, WebFeed* existing_feed)
{
	// Recreate / initialize the xml parser.
	if (m_xml_parser.get())
		m_xml_parser = NULL;
	m_parsing_started = FALSE;

	if (url.IsEmpty())
		m_feed_url = g_url_api->GetURL(m_feed_content_location);
	else
		m_feed_url = url;

	XMLParser* new_xml_parser = NULL;
	RETURN_IF_ERROR(XMLParser::Make(new_xml_parser, this,
					g_main_message_handler, this, url));
	m_xml_parser.reset(new_xml_parser);

	// Set up XML configuration for parsing web feed:
	XMLParser::Configuration configuration;
	configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	configuration.max_tokens_per_call = 1024;
	configuration.preferred_literal_part_length = XML_TOKEN_LITERAL_LENGTH;

	m_xml_parser->SetConfiguration(configuration);

	// Clear / reset the internal variables.
	m_element_stack.DeleteAll();
	m_content_buffer.Clear();
	m_keep_tags = NULL;

	if (m_feed)
		m_feed->DecRef();
	m_feed = existing_feed;
	if (m_feed)
		m_feed->IncRef();

	if (m_entry)
		m_entry->DecRef();
	m_entry = NULL;

	return OpStatus::OK;
}

BOOL WebFeedParser::ChangeStateIfNeededL(WebFeedParseState::StateType next_state)
{
	OpAutoPtr<WebFeedParseState::WebFeedContext> context;
	ANCHOR(OpAutoPtr<WebFeedParseState::WebFeedContext>, context);

	if (m_parse_state.get()) // We're already in a parse state
		if (m_parse_state->Type() != next_state)
			context.reset(m_parse_state->ReleaseContext().release());
		else // Just continue using the state we're in
			return FALSE;

	switch (next_state)  // Switch to the next state we have to be in
	{
	case WebFeedParseState::StateInitial:
		m_parse_state = OP_NEW(InitialFeedParseState, (*this));
		break;
	case WebFeedParseState::StateRSSRoot:
		m_parse_state = OP_NEW(RSSRootParseState, (*this));
		break;
	case WebFeedParseState::StateDone:
		m_parse_state = NULL;
		break;
	case WebFeedParseState::StateRSSChannel:
		m_parse_state = OP_NEW(RSSChannelParseState, (*this));
		break;
	case WebFeedParseState::StateRSSItem:
	{
		RSSItemParseState* rss_item_parse_state = OP_NEW(RSSItemParseState, (*this));
		m_parse_state = rss_item_parse_state;

		if (rss_item_parse_state != 0)
			LEAVE_IF_ERROR(rss_item_parse_state->Init());

		if (m_entry)
		{
			// RSS 1.0 has guid as the the rdf:about attribute to item.
			// So set the guid from here if there is a rdf:about attribute.
			XMLElement* element = TopElementStackL();
			XMLAttributeQN* rdf_about = NULL;
			if (element && element->Attributes().GetData(UNI_L("rdf:about"), &rdf_about) == OpStatus::OK)
			{
				if (rdf_about && !rdf_about->Value().IsEmpty())
					m_entry->SetGuid(rdf_about->Value().CStr());
			}
		}

		break;
	}
	case WebFeedParseState::StateRSSImage:
		m_parse_state = OP_NEW(RSSImageParseState, (*this, m_parse_state->Type()));
		break;
	case WebFeedParseState::StateAtomRoot:
	{
		m_parse_state = OP_NEW(AtomRootParseState, (*this));
		break;
	}
	case WebFeedParseState::StateAtomAuthor:
	{
		m_parse_state = OP_NEW(AtomAuthorParseState, (*this,
						m_parse_state->Type() == WebFeedParseState::StateAtomEntry ?
						*m_entry->AuthorElement() : *m_feed->AuthorElement(), m_parse_state->Type()));
		break;
	}
	case WebFeedParseState::StateAtomEntry:
	{
		AtomEntryParseState* atom_entry_parse_state = OP_NEW(AtomEntryParseState, (*this));
		m_parse_state = atom_entry_parse_state;

		if (atom_entry_parse_state)
			LEAVE_IF_ERROR(atom_entry_parse_state->Init());

		break;
	}
	default:
		OP_ASSERT(!"Not implemented");
		break;
	}

	if (!m_parse_state.get() && next_state != WebFeedParseState::StateDone)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	if (m_parse_state.get())
		m_parse_state->SetContext(context.release());

	return TRUE;
}

OP_STATUS WebFeedParser::SetKeepTags(const OpStringC& namespace_uri,
		const OpStringC& local_name, BOOL ignore_first_tag)
{
	OP_ASSERT(!IsKeepingTags());

	m_keep_tags = OP_NEW(KeepTags, (ignore_first_tag));
	if (!m_keep_tags.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_keep_tags->Init(namespace_uri, local_name));
	return OpStatus::OK;
}

OP_STATUS WebFeedParser::ResolveAndUpdateLink(WebFeedLinkElement& link,
		const OpStringC& uri)
{
	// First we get the base URI from the parser, in case xml:base has been
	// used somewhere.
	OpString base_uri;
	RETURN_IF_ERROR(BaseURI(base_uri, m_feed_content_location));

	// Then resolve the passed URI from the base URI.
	OpString absolute_uri;
	RETURN_IF_ERROR(WebFeedUtil::RelativeToAbsoluteURL(absolute_uri, uri, base_uri));

	// And update the passed link.
	link.SetURI(absolute_uri);
	return OpStatus::OK;
}

OP_STATUS WebFeedParser::UpdateContentBaseURIIfNeeded(WebFeedContentElement& content)
{
	if (!content.HasBaseURI())
	{
		OpString base_uri;
		RETURN_IF_ERROR(BaseURI(base_uri, m_feed_content_location));

		RETURN_IF_ERROR(content.SetBaseURI(base_uri));
	}

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::ParseAuthorInformation(const OpStringC& input,
		OpString& email, OpString& name) const
{
	if (input.IsEmpty())
		return OpStatus::OK;

	const int input_length = input.Length();

	// Fetch the e-mail address.
	int email_start_pos = 0;
	int email_end_pos = input_length;

	const int at_pos = input.Find(UNI_L("@"));
	if (at_pos == KNotFound)
		email_end_pos = email_start_pos;
	else
	{
		// Find beginning of e-mail address.
		{
			for (int index = at_pos; index >= 0; --index)
			{
				const uni_char* subpart = 0;
				if (index - 6 >= 0)
					subpart = input.CStr() + index - 6;

				if (input[index] == ':' && subpart != 0 && 
					uni_strnicmp(subpart, UNI_L("mailto:"), 7) == 0)
				{
					// We found a colon, but it's just the end of a mailto:
				}
				else if (input[index] == ' ' ||
					input[index] == '<' ||
					input[index] == ':' ||
					input[index] == '[' ||
					input[index] == '(')
				{
					email_start_pos = index;
					break;
				}
			}
		}

		// Find end of e-mail address.
		{
			for (int index = at_pos; index < input_length; ++index)
			{
				if (input[index] == ' ' ||
					input[index] == ':' ||
					input[index] == '>' ||
					input[index] == ']' ||
					input[index] == ')')
				{
					email_end_pos = index + 1;
					break;
				}
			}
		}

		RETURN_IF_ERROR(email.Set(input));
		email.Delete(0, email_start_pos);
		email.Delete(email_end_pos - email_start_pos);

	}

	// Fetch the name.
	RETURN_IF_ERROR(name.Set(input));
	name.Delete(email_start_pos, email_end_pos - email_start_pos);

	// Remove parentheses from name if present
	int name_length = name.Length();

	if (name_length           >= 2 &&
		name[0]               == '(' &&
		name[name_length - 1] == ')')
	{
		name.Delete(name_length - 1);
		name.Delete(0, 1);
	}

	return OpStatus::OK;
}

// Namespace Prefix methods

BOOL WebFeedParser::HasNamespacePrefix(const OpStringC& uri) const
{
	WebFeedParser* non_const_this = (WebFeedParser *) (this);
	return non_const_this->m_namespace_prefixes.Contains(uri.CStr());
}

OP_STATUS WebFeedParser::AddNamespacePrefix(const OpStringC& prefix,
		const OpStringC& uri)
{
	OP_ASSERT(!HasNamespacePrefix(uri));

	OpStackAutoPtr<XMLNamespacePrefix> namespace_prefix(OP_NEW(XMLNamespacePrefix, ()));
	if (namespace_prefix.get() == 0)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(namespace_prefix->Init(prefix, uri));
	RETURN_IF_ERROR(m_namespace_prefixes.Add(namespace_prefix->URI().CStr(),
					namespace_prefix.get()));

	namespace_prefix.release();
	return OpStatus::OK;
}

OP_STATUS WebFeedParser::NamespacePrefix(OpString& prefix, const OpStringC& uri) const
{
	WebFeedParser* non_const_this = (WebFeedParser *) (this);

	XMLNamespacePrefix* namespace_prefix = 0;
	non_const_this->m_namespace_prefixes.GetData(uri.CStr(), &namespace_prefix);

	if (namespace_prefix != 0)
		RETURN_IF_ERROR(prefix.Set(namespace_prefix->Prefix()));

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::QualifiedName(OpString& qualified_name,
		const OpStringC& namespace_uri, const OpStringC& local_name) const
{
	if (namespace_uri.HasContent())
	{
		OpString prefix;
		RETURN_IF_ERROR(NamespacePrefix(prefix, namespace_uri));

		if (prefix.HasContent())
		{
			RETURN_IF_ERROR(qualified_name.AppendFormat(UNI_L("%s:%s"),
							prefix.CStr(), local_name.CStr()));
		}
	}

	if (qualified_name.IsEmpty())
		RETURN_IF_ERROR(qualified_name.Set(local_name));

	return OpStatus::OK;
}

OP_STATUS WebFeedParser::BaseURI(OpString& base_uri,
		const OpStringC& document_uri) const
{
	for (UINT index = 0, stack_size = m_element_stack.GetCount(); 
		index < stack_size; ++index)
	{
		XMLElement* current_element = m_element_stack.Get(index);
		OP_ASSERT(current_element != 0);

		if (current_element->HasBaseURI())
		{
			RETURN_IF_ERROR(base_uri.Set(current_element->BaseURI()));
			break;
		}
	}

	if (base_uri.IsEmpty() && document_uri.HasContent())
		RETURN_IF_ERROR(base_uri.Set(document_uri));

	return OpStatus::OK;
}

//	ElementStack operations

void WebFeedParser::PushElementStackL(XMLElement* element)
{
	OP_ASSERT(element);
	LEAVE_IF_ERROR(m_element_stack.Add(element));
}

XMLElement* WebFeedParser::PopElementStackL()
{
	OP_ASSERT(ElementStackHasContent());
	if (!ElementStackHasContent())
		LEAVE(OpStatus::ERR);

	return m_element_stack.Remove(m_element_stack.GetCount() - 1);
}

XMLElement* WebFeedParser::TopElementStackL() const
{
	OP_ASSERT(ElementStackHasContent());
	if (!ElementStackHasContent())
		LEAVE(OpStatus::ERR);

	return m_element_stack.Get(m_element_stack.GetCount() - 1);
}

//****************************************************************************
//
//	XMLNamespacePrefix
//
//****************************************************************************

XMLNamespacePrefix::XMLNamespacePrefix()
{
}

XMLNamespacePrefix::~XMLNamespacePrefix()
{
}

OP_STATUS XMLNamespacePrefix::Init(const OpStringC& prefix,
		const OpStringC& uri)
{
	RETURN_IF_ERROR(m_prefix.Set(prefix));
	RETURN_IF_ERROR(m_uri.Set(uri));

	return OpStatus::OK;
}

//****************************************************************************
//
//	WebFeedParser::KeepTags
//
//****************************************************************************

WebFeedParser::KeepTags::KeepTags(BOOL ignore_first_tag)
:	m_tag_depth(0), m_ignore_first_tag(ignore_first_tag)
{
}

OP_STATUS WebFeedParser::KeepTags::Init(const OpStringC& namespace_uri,
		const OpStringC& local_name)
{
	RETURN_IF_ERROR(m_namespace_uri.Set(namespace_uri));
	RETURN_IF_ERROR(m_local_name.Set(local_name));

	return OpStatus::OK;
}

BOOL WebFeedParser::KeepTags::IsEndTag(const OpStringC& namespace_uri,
		const OpStringC& local_name) const
{
	return (m_tag_depth == 0) &&
		(m_namespace_uri == namespace_uri) &&
		(m_local_name == local_name);
}

UINT WebFeedParser::KeepTags::DecTagDepth()
{
	OP_ASSERT(m_tag_depth> 0);
	if (m_tag_depth > 0)
		--m_tag_depth;

	return m_tag_depth;
}

OP_STATUS WebFeedParser::ContentBuffer::SetSize(size_t size)
{
	if (buffer_length >= size)
		return OpStatus::OK;

	uni_char* old_buffer = buffer;
	size_t new_size = buffer_length;

	if (buffer_length == 0)
		new_size = size;
	else
		while (new_size < size)
			new_size *= 2;

	buffer = OP_NEWA(uni_char, new_size);
	if (!buffer)
	{
		buffer = old_buffer;
		return OpStatus::ERR_NO_MEMORY;
	}

	if (old_buffer)
	{
		op_memcpy(buffer, old_buffer, buffer_length * sizeof(uni_char));
		OP_DELETEA(old_buffer);
	}

	buffer_length = new_size;

	return OpStatus::OK;
}

const uni_char* WebFeedParser::ContentBuffer::GetContent()
{
	if (!buffer)
		return NULL;

	OP_ASSERT(length < buffer_length);
	buffer[length] = '\0';

	return buffer;
}

uni_char* WebFeedParser::ContentBuffer::TakeOver()
{
	if (!buffer)
		return NULL;

	uni_char* res = buffer;
	OP_ASSERT(length < buffer_length);
	res[length] = '\0';

	buffer = NULL;
	buffer_length = 0;
	length = 0;

	return res;
}
#endif  // WEBFEEDS_BACKEND_SUPPORT
