/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifdef OPENSEARCH_SUPPORT

#ifndef OPENSEARCH_H
#define OPENSEARCH_H

#include "modules/hardcore/mh/mh.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"

class SearchElement;
class SearchManager;

class OpenSearchParser : public XMLTokenHandler, public XMLParser::Listener
{
friend class SearchManager;
public:
	OpenSearchParser();
	virtual ~OpenSearchParser();

private:
	OP_STATUS Construct(URL& xml_url, SearchManager* search_manager); //Don't try to call this directly, use SearchManager::AddOpenSearch

	OP_STATUS ParsingDone();
	BOOL IsFinished() const {return m_is_finished;}

	OP_STATUS SetQueryFromTemplate(const OpStringC& search_template);

	// Token handlers:
	OP_STATUS HandleTextToken(XMLToken &token);
	OP_STATUS HandleStartTagToken(XMLToken &token);
	OP_STATUS HandleEndTagToken(XMLToken &token);

public:
	// Callbacks from XML parser
	XMLTokenHandler::Result HandleToken(XMLToken &token);
	void Continue(XMLParser* parser);
	void Stopped(XMLParser* parser);

private:
	enum ElementType
	{
		UnknownElement
		, ShortNameElement
#ifdef UNUSED_CODE
		, DescriptionElement
#endif // UNUSED_CODE
		, UrlElement
#ifdef UNUSED_CODE
		, ContactElement
		, TagsElement
#endif // UNUSED_CODE
		, LongNameElement
		, ImageElement
#ifdef UNUSED_CODE
		, QueryElement
		, DeveloperElement
		, AttributionElement
		, SyndicationRightElement
		, AdultContentElement
		, LanguageElement
#endif // UNUSED_CODE
		, InputEncodingElement
#ifdef UNUSED_CODE
		, OutputEncodingElement
#endif // UNUSED_CODE
	};

	BOOL m_is_finished;
	XMLParser* m_xml_parser;

	SearchElement* m_search_element;
	SearchManager* m_report_to_search_manager; //Not owned by This

	int m_favicon_size;
	int m_indexOffset;
	int m_pageOffset;
	int m_opensearch_description_depth;
	ElementType m_current_element;
};

#endif // OPENSEARCH_H

#endif //OPENSEARCH_SUPPORT
