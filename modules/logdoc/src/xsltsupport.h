/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef XSLTSUPPORT_H
#define XSLTSUPPORT_H

#ifdef XSLT_SUPPORT
# include "modules/xslt/xslt.h"
# include "modules/xmlutils/xmlparser.h"

class LogdocXSLTHandlerTreeCollector;
class LogicalDocument;

class LogdocXSLTHandler
	: public XSLT_Handler,
	  public XMLParser::Listener
{
public:
	LogdocXSLTHandler(LogicalDocument *logdoc)
		: logdoc(logdoc),
		  source_tree_root_element(NULL),
		  error_message(NULL)
	{
	}

	virtual ~LogdocXSLTHandler();

	/* From XSLT_Handler: */
	virtual LogicalDocument *GetLogicalDocument();
	virtual URL GetDocumentURL();
	virtual OP_STATUS LoadResource(ResourceType resource_type, URL resource_url, XMLTokenHandler *token_handler);
	virtual void CancelLoadResource(XMLTokenHandler *token_handler);
	virtual OP_STATUS StartCollectingSourceTree(XSLT_Handler::TreeCollector *&tree_collector);
	virtual OP_STATUS OnXMLOutput(XMLTokenHandler *&tokenhandler, BOOL &destroy_when_finished);
	virtual OP_STATUS OnHTMLOutput(XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished);
	virtual OP_STATUS OnTextOutput(XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished);
	virtual void OnFinished();
	virtual void OnAborted();
#ifdef XSLT_ERRORS
	virtual OP_BOOLEAN HandleMessage(XSLT_Handler::MessageType type, const uni_char *message);
#endif // XSLT_ERRORS

	/* From XMLParser::Listener: */
	virtual void Continue(XMLParser *parser);
	virtual void Stopped(XMLParser *parser);
	virtual BOOL Redirected(XMLParser *parser);

	void SetSourceTreeRootElement(HTML_Element *element) { source_tree_root_element = element; }

	static void StripWhitespace(LogicalDocument *logdoc, HTML_Element *element, XSLT_Stylesheet *stylesheet);

	OpString *GetErrorMessage() { OpString *message = error_message; error_message = NULL; return message; }
	/**< Returns an error message or NULL if none has been recorded.  Note: the
	     caller assumes ownership of the string! */

private:
	BOOL AllowStylesheetInclusion(ResourceType resource_type, URL resource_url);

	class StylesheetParserElm
		: public Link,
		  public XMLParser::Listener
	{
	public:
		StylesheetParserElm(LogdocXSLTHandler *handler, URL url) : handler(handler), parser(NULL), url(url) {}
		virtual ~StylesheetParserElm();

		/* From XMLParser::Listener: */
		virtual void Continue(XMLParser *parser);
		virtual void Stopped(XMLParser *parser);
		virtual BOOL Redirected(XMLParser *parser);

		XSLT_Handler::ResourceType resource_type;
		LogdocXSLTHandler *handler;
		XMLParser *parser;
		XMLTokenHandler *token_handler;
		URL url;
	};
	friend class StylesheetParserElm;

	Head stylesheet_parser_elms;

	LogicalDocument *logdoc;
	HTML_Element *source_tree_root_element;
	OpString *error_message;
};

class LogdocXSLTStringDataCollector
	: public XSLT_Stylesheet::Transformation::StringDataCollector
{
public:
	LogdocXSLTStringDataCollector(LogicalDocument *logdoc)
		: logdoc(logdoc),
		  buffer_length(0),
		  is_calling(FALSE),
		  is_finished(FALSE)
	{
	}

	BOOL IsCalling() { return is_calling; }
	BOOL IsFinished() { return is_finished && buffer_length == 0; }
	OP_BOOLEAN ProcessCollectedData();

private:
	virtual OP_STATUS CollectStringData(const uni_char *string, unsigned string_length);
	virtual OP_STATUS StringDataFinished();

	LogicalDocument *logdoc;
	TempBuffer buffer;
	unsigned buffer_length;
	BOOL is_calling, is_finished;
};

#endif // XSLT_SUPPORT
#endif // XSLTSUPPORT_H
