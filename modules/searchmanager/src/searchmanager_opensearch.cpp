/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef OPENSEARCH_SUPPORT

#include "modules/searchmanager/src/searchmanager_opensearch.h"

#include "modules/content_filter/content_filter.h"
#include "modules/searchmanager/searchmanager.h"
#include "modules/url/uamanager/ua.h"
#include "modules/url/url2.h"
#include "modules/url/url_api.h"


OpenSearchParser::OpenSearchParser()
: XMLTokenHandler(),
  XMLParser::Listener(),
  m_is_finished(FALSE),
  m_xml_parser(NULL),
  m_search_element(NULL),
  m_report_to_search_manager(NULL),
  m_favicon_size(0),
  m_indexOffset(1),
  m_pageOffset(1),
  m_opensearch_description_depth(0),
  m_current_element(UnknownElement)
{
}

OpenSearchParser::~OpenSearchParser()
{
	OP_DELETE(m_xml_parser);
	OP_DELETE(m_search_element);
}

OP_STATUS OpenSearchParser::Construct(URL& xml_url, SearchManager* search_manager)
{
	OP_STATUS ret;

	if ((m_report_to_search_manager = search_manager) == NULL)
		return OpStatus::ERR_NULL_POINTER;

	m_search_element = OP_NEW(SearchElement, ());
	if (!m_search_element)
		return OpStatus::ERR_NO_MEMORY;

	OpString xml_url_string;
	if ((ret=xml_url.GetAttribute(URL::KUniName_With_Fragment, xml_url_string)) != OpStatus::OK)
		return ret;

	if ((ret=m_search_element->Construct(UNI_L(""), UNI_L(""), UNI_L(""), SEARCH_TYPE_SEARCH, UNI_L(""), FALSE, UNI_L(""), UNI_L("utf-8"), UNI_L(""), xml_url_string)) != OpStatus::OK)
	{
		return ret;
	}

	// Make a parser for result
	if ((ret=XMLParser::Make(m_xml_parser, this, g_main_message_handler, this, xml_url)) != OpStatus::OK)
		return ret;

	XMLParser::Configuration configuration;
	configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
	configuration.max_tokens_per_call = 0;  // unlimited

	m_xml_parser->SetConfiguration(configuration);

#ifdef URL_FILTER
	g_urlfilter->SetBlockMode(URLFilter::BlockModeOff);
#endif // URL_FILTER
	ret = m_xml_parser->Load(xml_url, TRUE);
#ifdef URL_FILTER
	g_urlfilter->SetBlockMode(URLFilter::BlockModeNormal);
#endif // URL_FILTER

	return ret;
}

OP_STATUS OpenSearchParser::ParsingDone()
{
	if (!m_is_finished)
	{
		m_is_finished = TRUE;
		g_main_message_handler->PostMessage(MSG_SEARCHMANAGER_DELETE_OPENSEARCH, 0, 0);

		if (m_report_to_search_manager && m_report_to_search_manager->AssignIdAndAddSearch(m_search_element) == OpStatus::OK)
		{
			g_main_message_handler->PostMessage(MSG_SEARCHMANAGER_OPENSEARCH_ADDED, m_search_element->GetId(), 0);
			m_search_element = NULL; //This is now owned by SearchManager
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpenSearchParser::SetQueryFromTemplate(const OpStringC& search_template)
{
	if (search_template.IsEmpty())
		return OpStatus::OK; //Nothing to do

	OP_STATUS ret;
	OpString query;
	BOOL optional = FALSE;

	const uni_char* start_pos = search_template.CStr();
	const uni_char* parameter_start = uni_strchr(start_pos, '{');
	const uni_char* parameter_end;
	int parameter_length;
	const uni_char* tmp;
	while (parameter_start != NULL)
	{
		parameter_end = uni_strchr(parameter_start, '}');
		if (!parameter_end)
			break;

		if ((parameter_start-start_pos)>0 && (ret=query.Append(start_pos, parameter_start-start_pos)) != OpStatus::OK)
			return ret;

		tmp = parameter_end - 1;

		optional = (*tmp == '?');

		//Very simple parsing. Actually, only these characters are allowed:
		//ALPHA / DIGIT / ("%" HEXDIG HEXDIG) /
		//"-" / "." / "_" / "~" "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "=" / ":"(sic!) / "@"
		while (tmp>parameter_start && *tmp!=':') tmp--;
		tmp++; //Skip '{' or ':'

		parameter_length = parameter_end - tmp - (optional ? 1 : 0);

		if (parameter_length > 0)
		{
			//Standard parameters
			if (*tmp=='s' && uni_strncmp(tmp, UNI_L("searchTerms"), parameter_length) == 0)
			{
				ret=query.Append(UNI_L("%s"));
			}
			else if (*tmp=='c' && uni_strncmp(tmp, UNI_L("count"), parameter_length) == 0)
			{
				ret=query.Append(UNI_L("%i"));
			}
			else if (*tmp=='s' && uni_strncmp(tmp, UNI_L("startIndex"), parameter_length) == 0)
			{
				ret=query.AppendFormat(UNI_L("%i"), m_indexOffset);
			}
			else if (*tmp=='s' && uni_strncmp(tmp, UNI_L("startPage"), parameter_length) == 0)
			{
				ret=query.AppendFormat(UNI_L("%i"), m_pageOffset);
			}
			else if (*tmp=='l' && uni_strncmp(tmp, UNI_L("language"), parameter_length) == 0)
			{
				ret=query.Append(UNI_L("*"));
			}
			else if (*tmp=='i' && uni_strncmp(tmp, UNI_L("inputEncoding"), parameter_length) == 0)
			{
				ret=query.Append(UNI_L("UTF-8"));
			}
			else if (*tmp=='o' && uni_strncmp(tmp, UNI_L("outputEncoding"), parameter_length) == 0)
			{
				ret=query.Append(UNI_L("UTF-8"));
			}
			//Extensions
			else if (*tmp=='s' && uni_strncmp(tmp, UNI_L("source"), parameter_length) == 0)
			{
				ret=query.Append(UNI_L("opera"));
			}
			else
			{
				OP_ASSERT(optional==TRUE && !"All non-optional parameters must set a value!");
			}
	
			if (ret != OpStatus::OK)
				return ret;
		}

		start_pos = parameter_end + 1; //Skip past '}'
		parameter_start = uni_strchr(start_pos, '{');
	}

	//Append remaining text
	if (start_pos && *start_pos != 0)
	{
		if ((ret=query.Append(start_pos)) != OpStatus::OK)
			return ret;
	}
	
	return m_search_element->SetURL(query);
}

OP_STATUS OpenSearchParser::HandleTextToken(XMLToken& token)
{
	if (m_current_element==UnknownElement || m_opensearch_description_depth!=1)
		return OpStatus::OK;

	const uni_char* literal_value = token.GetLiteralSimpleValue();
	unsigned literal_length = token.GetLiteralLength();

	OP_STATUS ret = OpStatus::OK;
	OpString value;
	if (literal_length>0 && literal_value)
	{
		if ((ret=value.Set(literal_value, literal_length)) != OpStatus::OK)
			return ret;

		switch(m_current_element)
		{
		case ShortNameElement:
			ret = m_search_element->SetName(value);
			break;

		case LongNameElement: //Or maybe description?
			ret = m_search_element->SetDescription(value);
			break;

//		case UrlElement: //Url is an empty xml element. Value is set in HandleStartTagToken

		case ImageElement:
			ret = m_search_element->SetIconURL(value);
			break;

		case InputEncodingElement:
			m_search_element->SetCharset(value);
			break;

		default: break;
		}
	}

	return OpStatus::OK;	
}

OP_STATUS OpenSearchParser::HandleStartTagToken(XMLToken& token)
{
	// Fetch information about the element.
	const XMLCompleteNameN& elemname = token.GetName();
	const uni_char* localpart = elemname.GetLocalPart();
	unsigned localpart_length = elemname.GetLocalPartLength();

	if ((*localpart=='O' || *localpart=='O') && uni_strni_eq(localpart, "OPENSEARCHDESCRIPTION", localpart_length))
	{
		m_opensearch_description_depth++;
		m_current_element = UnknownElement;
	}
	else if (m_opensearch_description_depth == 1)
	{
		if ((*localpart=='s' || *localpart=='S') && uni_strni_eq(localpart, "SHORTNAME", localpart_length))
		{
			m_current_element = ShortNameElement;
		}
#ifdef UNUSED_CODE
		else if ((*localpart=='d' || *localpart=='D') && uni_strni_eq(localpart, "DESCRIPTION", localpart_length))
		{
			m_current_element = DescriptionElement;
		}
#endif // UNUSED_CODE
		else if ((*localpart=='u' || *localpart=='U') && uni_strni_eq(localpart, "URL", localpart_length))
		{
			XMLToken::Attribute* type = token.GetAttribute(UNI_L("type"));
			if (type && type->GetValue() && uni_strni_eq(type->GetValue(), "TEXT/HTML", type->GetValueLength())) //For time being, only type="text/html" is supported
			{
				const uni_char* attribute_value;
				unsigned attribute_length;
				unsigned index;

				XMLToken::Attribute* index_offset_attr = token.GetAttribute(UNI_L("indexOffset"));
				if (index_offset_attr && index_offset_attr->GetValue())
				{
					index = 0;
					int index_offset = 0;
					attribute_value = index_offset_attr->GetValue();
					attribute_length = index_offset_attr->GetValueLength();
					while (attribute_value && index<attribute_length && *(attribute_value+index)>='0' && *(attribute_value+index)<='9')
						index_offset = index_offset*10 + *(attribute_value+index++) - '0';

					if (index_offset != 0)
						m_indexOffset = index_offset;
				}

				XMLToken::Attribute* page_offset_attr = token.GetAttribute(UNI_L("pageOffset"));
				if (page_offset_attr && page_offset_attr->GetValue())
				{
					index = 0;
					int page_offset = 0;
					attribute_value = page_offset_attr->GetValue();
					attribute_length = page_offset_attr->GetValueLength();
					while (attribute_value && index<attribute_length && *(attribute_value+index)>='0' && *(attribute_value+index)<='9')
						page_offset = page_offset*10 + *(attribute_value+index++) - '0';

					if (page_offset != 0)
						m_pageOffset = page_offset;
				}

				XMLToken::Attribute* search_template = token.GetAttribute(UNI_L("template"));
				if (search_template && search_template->GetValue())
				{
					OP_STATUS ret;
					OpString search_query;
					if ((ret=search_query.Set(search_template->GetValue(), search_template->GetValueLength()))!=OpStatus::OK ||
				        (ret=SetQueryFromTemplate(search_query))!=OpStatus::OK)
					{
						return ret;
					}
				}
			}
			m_current_element = UrlElement;
		}
#ifdef UNUSED_CODE
		else if ((*localpart=='c' || *localpart=='C') && uni_strni_eq(localpart, "CONTACT", localpart_length))
		{
			m_current_element = ContactElement;
		}
		else if ((*localpart=='t' || *localpart=='T') && uni_strni_eq(localpart, "TAGS", localpart_length))
		{
			m_current_element = TagsElement;
		}
#endif // UNUSED_CODE
		else if ((*localpart=='l' || *localpart=='L') && uni_strni_eq(localpart, "LONGNAME", localpart_length))
		{
			m_current_element = LongNameElement;
		}
		else if ((*localpart=='i' || *localpart=='I') && uni_strni_eq(localpart, "IMAGE", localpart_length))
		{
			const uni_char* attribute_value;
			unsigned attribute_length;
			unsigned index;

			int favicon_height = 0;
			int favicon_width = 0;
			XMLToken::Attribute* height = token.GetAttribute(UNI_L("height"));
			if (height)
			{
				index = 0;
				attribute_value = height->GetValue();
				attribute_length = height->GetValueLength();
				while (attribute_value && index<attribute_length && *(attribute_value+index)>='0' && *(attribute_value+index)<='9')
					favicon_height = favicon_height*10 + *(attribute_value+index++) - '0';
			}

			XMLToken::Attribute* width = token.GetAttribute(UNI_L("width"));
			if (width)
			{
				index = 0;
				attribute_value = width->GetValue();
				attribute_length = width->GetValueLength();
				while (attribute_value && index<attribute_length && *(attribute_value+index)>='0' && *(attribute_value+index)<='9')
					favicon_width = favicon_width*10 + *(attribute_value+index++) - '0';
			}

			int favicon_size = favicon_height*favicon_width;
			if (favicon_size < m_favicon_size) //We want the largest image. Ignore smaller ones
			{
				m_current_element = UnknownElement;
			}
			else
			{
				m_favicon_size = favicon_size;
				m_current_element = ImageElement;
			}
		}
#ifdef UNUSED_CODE
		else if ((*localpart=='q' || *localpart=='Q') && uni_strni_eq(localpart, "QUERY", localpart_length))
		{
			m_current_element = QueryElement;
		}
		else if ((*localpart=='d' || *localpart=='D') && uni_strni_eq(localpart, "DEVELOPER", localpart_length))
		{
			m_current_element = DeveloperElement;
		}
		else if ((*localpart=='a' || *localpart=='A') && uni_strni_eq(localpart, "ATTRIBUTION", localpart_length))
		{
			m_current_element = AttributionElement;
		}
		else if ((*localpart=='s' || *localpart=='S') && uni_strni_eq(localpart, "SYNDICATIONRIGHT", localpart_length))
		{
			m_current_element = SyndicationRightElement;
		}
		else if ((*localpart=='a' || *localpart=='A') && uni_strni_eq(localpart, "ADULTCONTENT", localpart_length))
		{
			m_current_element = AdultContentElement;
		}
		else if ((*localpart=='l' || *localpart=='L') && uni_strni_eq(localpart, "LANGUAGE", localpart_length))
		{
			m_current_element = LanguageElement;
		}
#endif // UNUSED_CODE
		else if ((*localpart=='i' || *localpart=='I') && uni_strni_eq(localpart, "INPUTENCODING", localpart_length))
		{
			m_current_element = InputEncodingElement;
		}
#ifdef UNUSED_CODE
		else if ((*localpart=='o' || *localpart=='O') && uni_strni_eq(localpart, "OUTPUTENCODING", localpart_length))
		{
			m_current_element = OutputEncodingElement;
		}
#endif // UNUSED_CODE
	}

	return OpStatus::OK;
}

OP_STATUS OpenSearchParser::HandleEndTagToken(XMLToken& token)
{
	// Fetch information about the element.
	const XMLCompleteNameN& elemname = token.GetName();
	const uni_char* localpart = elemname.GetLocalPart();
	unsigned localpart_length = elemname.GetLocalPartLength();

	if ((*localpart=='O' || *localpart=='O') && uni_strni_eq(localpart, "OPENSEARCHDESCRIPTION", localpart_length) && m_opensearch_description_depth>=1)
	{
		m_opensearch_description_depth--;
		if (m_opensearch_description_depth == 0)
		{
			ParsingDone();
		}
	}

	m_current_element = UnknownElement;

	return OpStatus::OK;
}

XMLTokenHandler::Result OpenSearchParser::HandleToken(XMLToken &token)
{
	OP_STATUS status = OpStatus::OK;
	
	switch (token.GetType())
	{
	case XMLToken::TYPE_CDATA :
	case XMLToken::TYPE_Text :
		status = HandleTextToken(token);
		break;
	case XMLToken::TYPE_STag :
		status = HandleStartTagToken(token);
		break;
	case XMLToken::TYPE_ETag :
		status = HandleEndTagToken(token);
		break;
	case XMLToken::TYPE_EmptyElemTag :
		status = HandleStartTagToken(token);
		if (OpStatus::IsSuccess(status))
			status = HandleEndTagToken(token);
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

void OpenSearchParser::Continue(XMLParser* parser)
{
}

void OpenSearchParser::Stopped(XMLParser* parser)
{
	OP_ASSERT(parser->IsFinished() || parser->IsFailed());
	
	m_xml_parser = NULL;  // The XML parser deletes itself, and we can't do it here anyway, as we're in a callback from it

	ParsingDone();
}

#endif //OPENSEARCH_SUPPORT
