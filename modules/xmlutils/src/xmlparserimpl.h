/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSERIMPL_H
#define XMLPARSERIMPL_H

#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmltypes.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/url/url2.h"
#include "modules/util/tempbuf.h"

#include "modules/doc/frm_doc.h"

class MessageHandler;
class XMLTokenHandler;

class XMLInternalParser;
class XMLDataSourceHandlerImpl;

class XMLParserImpl
	: public XMLParser,
	  public XMLTokenHandler,
	  public ExternalInlineListener,
	  public MessageObject
{
protected:
	Listener *listener;
	FramesDocument *document;
	MessageHandler *mh;
	XMLTokenHandler *tokenhandler;
	Configuration configuration;
	URL url, parsed_url;
	IAmLoadingThisURL url_loading;
	URL_DataDescriptor *urldd;
	BOOL is_standalone;
	BOOL is_loading_stopped;
	BOOL is_listener_signalled;
	BOOL is_calling_loadinline;
	BOOL is_blocked;
	BOOL is_busy;
	BOOL is_parsing;
	BOOL delete_when_finished;
	BOOL owns_listener;
	BOOL owns_tokenhandler;
	BOOL is_waiting_for_timeout;

	XMLDocumentInformation information;
	BOOL is_finished;
	BOOL is_failed;
	BOOL is_oom;
	BOOL is_paused;
	
	/// Last HTTP response code
	int http_response;

	class SourceCallbackImpl
		: public XMLTokenHandler::SourceCallback
	{
	private:
		virtual void Continue(XMLTokenHandler::SourceCallback::Status status);

	public:
		XMLParserImpl *parser;
	} sourcecallbackimpl;
	friend class SourceCallbackImpl;

#ifdef XML_ERRORS
	XMLErrorReport *errorreport;
	ErrorDataProvider *errordataprovider;
	unsigned previous_length;
#endif // XML_ERRORS

	XMLInternalParser *parser;
	XMLDataSourceHandlerImpl *datasourcehandler;
	BOOL constructed;

	OP_STATUS SetCallbacks();
	void LoadFromUrl();

	OP_STATUS SetLoadingTimeout(unsigned timeout_ms);
	void CancelLoadingTimeout();
	
	/* From XMLTokenHandler. */
	virtual XMLTokenHandler::Result HandleToken(XMLToken &token);

	/* From ExternalInlineListener. */
	virtual void LoadingProgress(const URL &url);
	virtual void LoadingStopped(const URL &url);
	virtual void LoadingRedirected(const URL &from, const URL &to);

	/* From MessageObject. */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/* From XMLTokenHandler::SourceCallback (via SourceCallbackImpl). */
	void Continue(XMLTokenHandler::SourceCallback::Status status);

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
	void ConvertTokenToStringL(TempBuffer &buffer, XMLToken &token);
	OP_STATUS ConvertTokenToString(TempBuffer &buffer, XMLToken &token);
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

	/* Internal helper for ReportConsoleError. */
	void ReportConsoleErrorL();
#ifdef XML_ERRORS
	/* Adds a description to message sent by ReportConsoleErrorL. */
	void AddErrorDescriptionL(OpString &message);
	/* Adds details to message sent by ReportConsoleErrorL. */
	void AddDetailedReportL(OpString &message);
#endif // XML_ERRORS

public:
	XMLParserImpl(Listener *listener, FramesDocument *document, MessageHandler *mh, XMLTokenHandler *tokenhandler, const URL &url);
	~XMLParserImpl();

	OP_STATUS Construct();

	virtual void SetConfiguration(const Configuration &configuration);
	virtual void SetOwnsListener();
	virtual void SetOwnsTokenHandler();
	virtual OP_STATUS Load(const URL &referrer_url, BOOL delete_when_finished, unsigned load_timeout, BOOL bypass_proxy = FALSE);
#if defined XMLUTILS_PARSE_RAW_DATA || defined XMLUTILS_PARSE_UTF8_DATA
	virtual OP_STATUS Parse(const char *data, unsigned data_length, BOOL more, unsigned *consumed);
#endif // XMLUTILS_PARSE_RAW_DATA || XMLUTILS_PARSE_UTF8_DATA
#ifdef XMLUTILS_PARSE_UNICODE_DATA
	virtual OP_STATUS Parse(const uni_char *data, unsigned data_length, BOOL more, unsigned *consumed);
#endif // XMLUTILS_PARSE_UNICODE_DATA
#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN
	virtual OP_STATUS ProcessToken(XMLToken &token);
#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN
	virtual OP_STATUS SignalInvalidEncodingError();
	virtual BOOL IsFinished();
	virtual BOOL IsFailed();
	virtual BOOL IsOutOfMemory();
	virtual BOOL IsPaused();
	virtual BOOL IsBlockedByTokenHandler();
	virtual URL GetURL();
	virtual int GetLastHTTPResponse() {return http_response; };
	virtual const XMLDocumentInformation &GetDocumentInformation();
	virtual XMLVersion GetXMLVersion();
	virtual XMLStandalone GetXMLStandalone();

#ifdef XML_ERRORS
	virtual const XMLRange &GetErrorPosition();
	virtual void GetErrorDescription(const char *&error, const char *&uri, const char *&fragment_id);
	virtual const XMLErrorReport *GetErrorReport();
	virtual XMLErrorReport *TakeErrorReport();
	virtual void SetErrorDataProvider(ErrorDataProvider *provider);

	BOOL ErrorDataAvailable();
	OP_STATUS RetrieveErrorData(const uni_char *&data, unsigned &data_length);
#endif // XML_ERRORS

	virtual URL GetCurrentEntityUrl();

#ifdef OPERA_CONSOLE
	virtual OP_STATUS ReportConsoleError();
	virtual OP_STATUS ReportConsoleMessage(OpConsoleEngine::Severity severity, const XMLRange &position, const uni_char *message);
#endif // OPERA_CONSOLE

	FramesDocument *GetDocument() { return document; }
	MessageHandler *GetMessageHandler() { return mh; }
	URL GetParsedURL() { return parsed_url; }
	unsigned GetCurrentEntityDepth();

	void Continue();
	void Stopped();

	void HandleOOM();

	XMLInternalParser *GetInternalParser() { return parser; }

#ifdef XML_VALIDATING
	virtual OP_STATUS MakeValidatingTokenHandler(XMLTokenHandler *&tokenhandler, XMLTokenHandler *secondary, XMLValidator::Listener *listener);
	virtual OP_STATUS MakeValidatingSerializer(XMLSerializer *&serializer, XMLValidator::Listener *listener);
#endif // XML_VALIDATING
};

#endif // XMLPARSERIMPL_H
