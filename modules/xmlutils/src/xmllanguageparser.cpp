/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XMLUTILS_XMLLANGUAGEPARSER_SUPPORT

#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/src/xmlserializerimpl.h"
#include "modules/xmlutils/src/xmltokenhandlerimpl.h"
#include "modules/util/tempbuf.h"
#include "modules/url/url2.h"

class XMLLanguageParserState
{
protected:
	class Item
	{
	public:
		enum Type { TYPE_SPACE, TYPE_LANG, TYPE_BASE };

		Item *previous;
		Type type;
		XMLWhitespaceHandling whitespacehandling;
		uni_char *lang;
		URL baseurl;
		unsigned level;

		Item(Item *previous);
		~Item();
	};

	XMLLanguageParserState *previous;
	XMLVersion version;
	XMLNamespaceDeclaration::Reference declaration;
	Item *item;
	URL baseurl;
	unsigned level;

public:
	XMLLanguageParserState(XMLLanguageParserState *previous, XMLVersion version, XMLNamespaceDeclaration *declaration, const URL &baseurl);
	~XMLLanguageParserState();

	OP_STATUS HandleStartElement();
	OP_STATUS HandleAttribute(const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length);
	OP_STATUS HandleEndElement();

	XMLVersion GetVersion() { return version; }
	XMLNamespaceDeclaration *GetCurrentNamespaceDeclaration() { return const_cast<const XMLNamespaceDeclaration::Reference &>(declaration); }
	XMLNamespaceDeclaration::Reference &GetNamespaceDeclaration() { return declaration; }
	XMLWhitespaceHandling GetCurrentWhitespaceHandling(XMLWhitespaceHandling use_default);
	const uni_char *GetCurrentLanguage();
	const URL &GetCurrentBaseUrl();
	XMLLanguageParserState *GetPrevious() { return previous; }
};

XMLLanguageParserState::Item::Item(Item *previous)
	: previous(previous),
	  lang(0)
{
}

XMLLanguageParserState::Item::~Item()
{
	OP_DELETEA(lang);
}

XMLLanguageParserState::XMLLanguageParserState(XMLLanguageParserState *previous, XMLVersion version, XMLNamespaceDeclaration *declaration, const URL &baseurl)
	: previous(previous),
	  version(version),
	  declaration(declaration),
	  item(0),
	  baseurl(baseurl),
	  level(0)
{
}

XMLLanguageParserState::~XMLLanguageParserState()
{
	while (item)
	{
		Item *discard_item = item;
		item = item->previous;
		OP_DELETE(discard_item);
	}
}

OP_STATUS
XMLLanguageParserState::HandleStartElement()
{
	++level;
	return OpStatus::OK;
}

OP_STATUS
XMLLanguageParserState::HandleAttribute(const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length)
{
	if (completename.IsXML())
	{
		const uni_char *localpart = completename.GetLocalPart();
		unsigned localpart_length = completename.GetLocalPartLength();

		if (localpart_length != 4 && localpart_length != 5)
			return OpStatus::OK;

		Item *new_item = OP_NEW(Item, (item));

		if (!new_item)
			return OpStatus::ERR_NO_MEMORY;

		new_item->previous = item;
		new_item->level = level;

		if (localpart_length == 4)
			if (uni_strncmp(localpart, UNI_L("lang"), 4) == 0)
			{
				new_item->type = Item::TYPE_LANG;
				new_item->lang = UniSetNewStrN(value, value_length);
			}
			else if (uni_strncmp(localpart, UNI_L("base"), 4) == 0)
			{
				TempBuffer buffer;

				if (OpStatus::IsMemoryError(buffer.Append(value, value_length)))
				{
					OP_DELETE(new_item);
					return OpStatus::ERR_NO_MEMORY;
				}

				URL baseurl = GetCurrentBaseUrl();

				new_item->type = Item::TYPE_BASE;
				new_item->baseurl = g_url_api->GetURL(baseurl, buffer.GetStorage(), FALSE);
			}
			else
				goto discard;
		else if (uni_strncmp(localpart, UNI_L("space"), 5) == 0)
		{
			new_item->type = Item::TYPE_SPACE;

			if (value_length == 7 && uni_strncmp(value, UNI_L("default"), 7) == 0)
				new_item->whitespacehandling = XMLWHITESPACEHANDLING_DEFAULT;
			else if (value_length == 8 && uni_strncmp(value, UNI_L("preserve"), 8) == 0)
				new_item->whitespacehandling = XMLWHITESPACEHANDLING_PRESERVE;
			else
				goto discard;
		}

		item = new_item;
		return OpStatus::OK;

	discard:
		OP_DELETE(new_item);
		return OpStatus::OK;
	}
	else
		return XMLNamespaceDeclaration::ProcessAttribute(declaration, completename, value, value_length, level);
}

OP_STATUS
XMLLanguageParserState::HandleEndElement()
{
	XMLNamespaceDeclaration::Pop(declaration, level);

	--level;

	while (item && item->level > level)
	{
		Item *discard_item = item;
		item = item->previous;
		OP_DELETE(discard_item);
	}

	return OpStatus::OK;
}

XMLWhitespaceHandling
XMLLanguageParserState::GetCurrentWhitespaceHandling(XMLWhitespaceHandling use_default)
{
	Item *search_item = item;

	while (search_item && search_item->type != Item::TYPE_SPACE)
		search_item = search_item->previous;

	return search_item ? search_item->whitespacehandling : use_default;
}

const uni_char *
XMLLanguageParserState::GetCurrentLanguage()
{
	Item *search_item = item;

	while (search_item && search_item->type != Item::TYPE_LANG)
		search_item = search_item->previous;

	return search_item ? search_item->lang : 0;
}

const URL &
XMLLanguageParserState::GetCurrentBaseUrl()
{
	Item *search_item = item;

	while (search_item && search_item->type != Item::TYPE_BASE)
		search_item = search_item->previous;

	return search_item ? search_item->baseurl : baseurl;
}

/* virtual */ OP_STATUS
XMLLanguageParser::StartEntity(const URL &url, const XMLDocumentInformation &documentinfo, BOOL entity_reference)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::StartElement(const XMLCompleteNameN &completename, BOOL fragment_start, BOOL &ignore_element)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::AddAttribute(const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL specified, BOOL id)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::StartContent()
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::AddCharacterData(CharacterDataType type, const uni_char *value, unsigned value_length)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::AddProcessingInstruction(const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::AddComment(const uni_char *value, unsigned value_length)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::EndElement(BOOL &block, BOOL &finished)
{
	return OpStatus::OK;
}

/* virtual */ void
XMLLanguageParser::SetSourceCallback(SourceCallback *new_source_callback)
{
	source_callback = new_source_callback;
}

/* virtual */ OP_STATUS
XMLLanguageParser::EndEntity()
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLLanguageParser::XMLError()
{
	return OpStatus::OK;
}

#ifdef XML_ERRORS

/* virtual */ void
XMLLanguageParser::SetLocation(const XMLRange &new_location)
{
	location = new_location;
}

#endif // XML_ERRORS

/* static */ OP_STATUS
XMLLanguageParser::MakeTokenHandler(XMLTokenHandler *&tokenhandler, XMLLanguageParser *parser, const uni_char *fragment_id)
{
	uni_char *fragment_id_copy = 0;

	if (fragment_id && *fragment_id)
		RETURN_IF_ERROR(UniSetStr(fragment_id_copy, fragment_id));

	tokenhandler = OP_NEW(XMLToLanguageParserTokenHandler, (parser, fragment_id_copy));

	if (tokenhandler)
		return OpStatus::OK;
	else
	{
		OP_DELETEA(fragment_id_copy);
		return OpStatus::ERR_NO_MEMORY;
	}
}

#ifdef XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT

/* static */ OP_STATUS
XMLLanguageParser::MakeSerializer(XMLSerializer *&serializer, XMLLanguageParser *parser)
{
	serializer = OP_NEW(XMLToLanguageParserSerializer, (parser));
	return serializer ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#endif // XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT

OP_STATUS
XMLLanguageParser::HandleStartEntity(const URL &url, const XMLDocumentInformation &documentinfo, BOOL entity_reference)
{
	XMLLanguageParserState *new_state = OP_NEW(XMLLanguageParserState, (state, documentinfo.GetVersion(), entity_reference ? GetCurrentNamespaceDeclaration() : 0, url));

	if (new_state)
	{
		if (!entity_reference)
			if (OpStatus::IsError(XMLNamespaceDeclaration::Push(new_state->GetNamespaceDeclaration(), XMLNSURI_XML, XMLNSURI_XML_LENGTH, UNI_L("xml"), 3, 0)) ||
			    OpStatus::IsError(XMLNamespaceDeclaration::Push(new_state->GetNamespaceDeclaration(), XMLNSURI_XMLNS, XMLNSURI_XMLNS_LENGTH, UNI_L("xmlns"), 5, 0)))
			{
				OP_DELETE(new_state);
				return OpStatus::ERR_NO_MEMORY;
			}

		state = new_state;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
XMLLanguageParser::HandleStartElement()
{
	return state ? state->HandleStartElement() : OpStatus::OK;
}

OP_STATUS
XMLLanguageParser::HandleAttribute(const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
	return state ? state->HandleAttribute(name, value, value_length) : OpStatus::OK;
}

OP_STATUS
XMLLanguageParser::HandleEndElement()
{
	return state ? state->HandleEndElement() : OpStatus::OK;
}

OP_STATUS
XMLLanguageParser::HandleEndEntity()
{
	if (state)
	{
		XMLLanguageParserState *previous = state->GetPrevious();
		OP_DELETE(state);
		state = previous;
	}

	return OpStatus::OK;
}

XMLVersion
XMLLanguageParser::GetCurrentVersion()
{
	return state ? state->GetVersion() : XMLVERSION_1_0;
}

XMLNamespaceDeclaration *
XMLLanguageParser::GetCurrentNamespaceDeclaration()
{
	return state ? state->GetCurrentNamespaceDeclaration() : 0;
}

XMLWhitespaceHandling
XMLLanguageParser::GetCurrentWhitespaceHandling(XMLWhitespaceHandling use_default)
{
	return state ? state->GetCurrentWhitespaceHandling(use_default) : use_default;
}

const uni_char *
XMLLanguageParser::GetCurrentLanguage()
{
	return state ? state->GetCurrentLanguage() : 0;
}

URL
XMLLanguageParser::GetCurrentBaseUrl()
{
	URL empty;
	return state ? state->GetCurrentBaseUrl() : empty;
}

XMLLanguageParser::XMLLanguageParser()
	: state(0),
	  source_callback(0)
{
}

/* virtual */
XMLLanguageParser::~XMLLanguageParser()
{
	while (state)
		HandleEndEntity();
}

#endif // XMLUTILS_XMLLANGUAGEPARSER_SUPPORT
