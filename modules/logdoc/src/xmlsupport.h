/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _XMLSUPPORT_H_
#define _XMLSUPPORT_H_

#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"

class LogicalDocument;
class HLDocProfile;
class HTML_Element;
class LayoutProperties;
class OpTreeCallback;
class OpElementCallback;
class URL_DataDescriptor;

class LogdocXMLTokenHandler
	: public XMLTokenHandler
{
public:
	LogdocXMLTokenHandler(LogicalDocument *logdoc);
	~LogdocXMLTokenHandler();

	void SetParser(XMLParser *new_parser) { parser = new_parser; }

	/* From XMLTokenHandler. */
	virtual Result HandleToken(XMLToken &token);
	virtual void SetSourceCallback(XMLTokenHandler::SourceCallback *callback);

	void ContinueIfBlocked();

	HTML_Element *GetParentElement();

	void RemovingElement(HTML_Element *element);

#ifdef OPELEMENTCALLBACK_SUPPORT
	void SetElementCallback(OpElementCallback *callback) { element_callback = callback; }
#endif // OPELEMENTCALLBACK_SUPPORT

#ifdef OPTREECALLBACK_SUPPORT
	OP_STATUS SetTreeCallback(OpTreeCallback *callback, const uni_char *fragment);
#endif // OPTREECALLBACK_SUPPORT

protected:
	Result HandleDoctype();
	Result HandlePIToken(XMLToken &token);
	Result HandleStartElementToken(XMLToken &token);
	Result HandleEndElementToken(XMLToken &token);
	Result HandleLiteralToken(XMLToken &token);

	LogicalDocument *logdoc;
	XMLParser *parser;
	XMLTokenHandler::SourceCallback *source_callback;

#ifdef OPELEMENTCALLBACK_SUPPORT
	OpElementCallback *element_callback;
#endif // OPELEMENTCALLBACK_SUPPORT
#ifdef OPTREECALLBACK_SUPPORT
	OpTreeCallback *tree_callback;
	uni_char *id;
	BOOL subtree_found, subtree_finished;
	HTML_Element *current_element;
#endif // OPTREECALLBACK_SUPPORT

	/* Current parsed depth. */
	short current_depth;

private:

	HTML_Element *m_parse_elm;
	/**
	 * @returns FALSE if the element was handled. Returns TRUE if there was an OOM situation.
	 */
	BOOL InsertElement(HLDocProfile *hld_profile, HTML_Element *parent_element, HTML_Element *element);
};

class LogdocXMLParserListener
	: public XMLParser::Listener
{
private:
	LogicalDocument *logdoc;

	/* From XMLParser::Listener. */
	virtual void Continue(XMLParser *parser);
	virtual void Stopped(XMLParser *parser);

public:
	LogdocXMLParserListener(LogicalDocument *logdoc)
		: logdoc(logdoc)
	{
	}
};

#ifdef XML_ERRORS

class LogdocXMLErrorDataProvider
	: public XMLParser::ErrorDataProvider
{
protected:
	LogicalDocument *logdoc;
	URL_DataDescriptor *urldd;
	unsigned previous_length;

public:
	LogdocXMLErrorDataProvider(LogicalDocument *logdoc)
		: logdoc(logdoc),
		  urldd(NULL),
		  previous_length(0)
	{
	}

	~LogdocXMLErrorDataProvider();

	virtual BOOL IsAvailable();
	virtual OP_STATUS RetrieveData(const uni_char *&data, unsigned &data_length);
};

#endif // XML_ERRORS

#endif // _XMLSUPPORT_H_
