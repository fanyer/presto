/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"

/* virtual */
XMLTokenHandler::~XMLTokenHandler()
{
}

/* virtual */ void
XMLTokenHandler::ParsingStopped(XMLParser *parser)
{
}

/* virtual */ void
XMLTokenHandler::SetSourceCallback(SourceCallback *)
{
}

#ifdef XMLUTILS_XMLLANGUAGEPARSER_SUPPORT

#include "modules/xmlutils/src/xmltokenhandlerimpl.h"
#include "modules/xmlutils/src/xmlparserimpl.h"
#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/xmlutils/xmlnames.h"

/* virtual */ void
XMLToLanguageParserTokenHandler::SourceCallbackImpl::Continue(XMLLanguageParser::SourceCallback::Status status)
{
	tokenhandler->Continue(status);
}

void
XMLToLanguageParserTokenHandler::Continue(XMLLanguageParser::SourceCallback::Status status)
{
	if (source_callback)
		source_callback->Continue((XMLTokenHandler::SourceCallback::Status) status);
}

XMLToLanguageParserTokenHandler::XMLToLanguageParserTokenHandler(XMLLanguageParser *parser, uni_char *fragment_id)
	: parser(parser),
	  fragment_found(FALSE),
	  fragment_id(fragment_id),
	  ignore_element_depth(0),
	  finished(FALSE),
	  entity_signalled(FALSE),
	  current_entity_depth(0),
	  source_callback(0)
{
	sourcecallbackimpl.tokenhandler = this;
}

XMLToLanguageParserTokenHandler::~XMLToLanguageParserTokenHandler()
{
	OP_DELETEA(fragment_id);
}

/* virtual */ XMLTokenHandler::Result
XMLToLanguageParserTokenHandler::HandleToken(XMLToken &token)
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (finished)
		return RESULT_OK;

	if (ignore_element_depth != 0)
	{
		if (token.GetType() == XMLToken::TYPE_STag)
			++ignore_element_depth;
		else if (token.GetType() == XMLToken::TYPE_ETag)
			--ignore_element_depth;
	}

	XMLParserImpl *xmlparser = (XMLParserImpl *) token.GetParser();
	BOOL block = FALSE;

	if (ignore_element_depth == 0)
	{
#ifdef XML_ERRORS
		XMLRange location;
#endif // XML_ERRORS

		switch (token.GetType())
		{
		case XMLToken::TYPE_PI:
#ifdef XML_ERRORS
			token.GetTokenRange(location);
			parser->SetLocation(location);
#endif // XML_ERRORS

			status = parser->AddProcessingInstruction(token.GetName().GetLocalPart(), token.GetName().GetLocalPartLength(), token.GetData(), token.GetDataLength());
			break;

		case XMLToken::TYPE_CDATA:
		case XMLToken::TYPE_Text:
		case XMLToken::TYPE_Comment:
		{
			XMLLanguageParser::CharacterDataType cdatatype = XMLLanguageParser::CHARACTERDATA_TEXT;
			if (token.GetType() != XMLToken::TYPE_Comment)
				if (token.GetType() == XMLToken::TYPE_CDATA)
					cdatatype = XMLLanguageParser::CHARACTERDATA_CDATA_SECTION;
				else if (token.GetLiteralIsWhitespace())
				{
					if (!entity_signalled)
						/* Don't generate calls to the language parser before the
						   first call to StartEntity. */
						break;
					cdatatype = XMLLanguageParser::CHARACTERDATA_TEXT_WHITESPACE;
				}

			const uni_char *simplevalue;
			uni_char *allocatedvalue;

			if ((simplevalue = token.GetLiteralSimpleValue()) == 0)
			{
				simplevalue = allocatedvalue = token.GetLiteralAllocatedValue();
				if (!allocatedvalue)
					status = OpStatus::ERR_NO_MEMORY;
			}
			else
				allocatedvalue = 0;

			if (simplevalue)
			{
#ifdef XML_ERRORS
				token.GetTokenRange(location);
				parser->SetLocation(location);
#endif // XML_ERRORS

				if (token.GetType() == XMLToken::TYPE_Comment)
					status = parser->AddComment(simplevalue, token.GetLiteralLength());
				else
					status = parser->AddCharacterData(cdatatype, simplevalue, token.GetLiteralLength());

				OP_DELETEA(allocatedvalue);
			}
			break;
		}
		case XMLToken::TYPE_STag:
		case XMLToken::TYPE_ETag:
		case XMLToken::TYPE_EmptyElemTag:
			if (!(xmlparser->GetCurrentEntityUrl() == current_entity_url) && xmlparser->GetCurrentEntityDepth() <= current_entity_depth)
				status = parser->EndEntity();

			if (OpStatus::IsSuccess(status) && (!(xmlparser->GetCurrentEntityUrl() == current_entity_url) || current_entity_url.IsEmpty() && !entity_signalled) && xmlparser->GetCurrentEntityDepth() > current_entity_depth)
			{
				status = parser->StartEntity(xmlparser->GetCurrentEntityUrl(), xmlparser->GetDocumentInformation(), xmlparser->GetCurrentEntityDepth() > 1);
				entity_signalled = TRUE;
			}

			current_entity_url = xmlparser->GetCurrentEntityUrl();
			current_entity_depth = xmlparser->GetCurrentEntityDepth();

			if (token.GetType() != XMLToken::TYPE_ETag)
			{
				if (OpStatus::IsSuccess(status))
				{
					BOOL fragment_start = FALSE;

					if (!fragment_found)
					{
						if (!fragment_id)
							fragment_start = TRUE;
						else
						{
							XMLToken::Attribute *attributes = token.GetAttributes();
							unsigned attributes_count = token.GetAttributesCount();
							unsigned fragment_id_length = uni_strlen(fragment_id);

							for (unsigned index = 0; index < attributes_count; ++index)
								if (attributes[index].GetId())
									if (attributes[index].GetValueLength() == fragment_id_length && uni_strncmp(attributes[index].GetValue(), fragment_id, fragment_id_length) == 0)
									{
										fragment_start = TRUE;
										break;
									}
						}

						if (fragment_start)
							fragment_found = TRUE;
					}

#ifdef XML_ERRORS
					token.GetTokenRange(location);
					parser->SetLocation(location);
#endif // XML_ERRORS

					BOOL ignore_element = FALSE;
					status = parser->StartElement(token.GetName(), fragment_start, ignore_element);

					if (ignore_element)
					{
						if (token.GetType() == XMLToken::TYPE_STag)
							ignore_element_depth = 1;
					}
					else
					{
						XMLToken::Attribute *attributes = token.GetAttributes();
						unsigned attributes_count = token.GetAttributesCount();

						for (unsigned index = 0; OpStatus::IsSuccess(status) && index < attributes_count; ++index)
						{
#ifdef XML_ERRORS
							token.GetAttributeRange(location, index);
							parser->SetLocation(location);
#endif // XML_ERRORS

							XMLToken::Attribute &attribute = attributes[index];
							status = parser->AddAttribute(attribute.GetName(), attribute.GetValue(), attribute.GetValueLength(), attribute.GetSpecified(), attribute.GetId());
						}

						if (OpStatus::IsSuccess(status))
							status = parser->StartContent();
					}
				}
			}

			if (OpStatus::IsSuccess(status) && token.GetType() != XMLToken::TYPE_STag)
			{
#ifdef XML_ERRORS
				token.GetTokenRange(location);
				parser->SetLocation(location);
#endif // XML_ERRORS

				status = parser->EndElement(block, finished);

				if (OpStatus::IsSuccess(status) && block)
					parser->SetSourceCallback(&sourcecallbackimpl);
			}
			break;

		case XMLToken::TYPE_Finished:
			status = parser->EndEntity();
		}
	}

	if (OpStatus::IsSuccess(status))
		if (block)
			return RESULT_BLOCK;
		else
			return RESULT_OK;
	else if (OpStatus::IsMemoryError(status))
		return RESULT_OOM;
	else
		return RESULT_ERROR;
}

/* virtual */ void
XMLToLanguageParserTokenHandler::ParsingStopped(XMLParser *xmlparser)
{
	if (xmlparser->IsFailed())
		OpStatus::Ignore(parser->XMLError());
}

/* virtual */ void
XMLToLanguageParserTokenHandler::SetSourceCallback(XMLTokenHandler::SourceCallback *callback)
{
	source_callback = callback;
}

#endif // XMLUTILS_XMLLANGUAGEPARSER_SUPPORT
