/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLTOKENHANDLERIMPL_H
#define XMLTOKENHANDLERIMPL_H

#ifdef XMLUTILS_XMLLANGUAGEPARSER_SUPPORT

#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/url/url2.h"

class XMLToLanguageParserTokenHandler
	: public XMLTokenHandler
{
protected:
	XMLLanguageParser *parser;
	BOOL fragment_found;
	uni_char *fragment_id;
	unsigned ignore_element_depth;
	BOOL finished;
	BOOL entity_signalled;
	URL current_entity_url;
	unsigned current_entity_depth;
	XMLTokenHandler::SourceCallback *source_callback;

	class SourceCallbackImpl
		: public XMLLanguageParser::SourceCallback
	{
	private:
		virtual void Continue(XMLLanguageParser::SourceCallback::Status status);

	public:
		XMLToLanguageParserTokenHandler *tokenhandler;
	} sourcecallbackimpl;
	friend class SourceCallbackImpl;

	/* From XMLLanguageParser::SourceCallback (via SourceCallbackImpl). */
	void Continue(XMLLanguageParser::SourceCallback::Status status);

public:
	XMLToLanguageParserTokenHandler(XMLLanguageParser *parser, uni_char *fragment_id);
	~XMLToLanguageParserTokenHandler();

	virtual Result HandleToken(XMLToken &token);
	virtual void ParsingStopped(XMLParser *parser);
	virtual void SetSourceCallback(XMLTokenHandler::SourceCallback *callback);
};

#endif // XMLUTILS_XMLLANGUAGEPARSER_SUPPORT
#endif // XMLTOKENHANDLERIMPL_H
