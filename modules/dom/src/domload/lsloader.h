/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSLOADER
#define DOM_LSLOADER

#ifdef DOM3_LOAD

#include "modules/hardcore/mh/messobj.h"
#include "modules/url/url2.h"
#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/logdoc/opelementcallback.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/util/adt/opvector.h"

#include "modules/doc/frm_doc.h"

class DOM_EnvironmentImpl;
class DOM_LSParser;
class DOM_LSContentHandler;
class MessageHandler;
class HTML_Element;
class ES_Thread;
class ES_ThreadListener;
class XMLParser;
class XMLTokenHandler;

/** Upon receiving data, processing redirects and performing security
    checks, the loader will notify whoever is using it. The notifications
    may alter the state of the loader, or abort, as a result of being
    notified. */
class DOM_LSLoadHandler
{
public:
	virtual OP_STATUS OnHeadersReceived(URL &url) = 0;
	/**< Notification of headers received. If the caller
	     returns OpStatus::OK, the loader must continue.
	     OpStatus::ERR if the loading operation must be aborted. */

	virtual OP_BOOLEAN OnRedirect(URL &url, URL &moved_to_url) = 0;
	/**< Notification of request redirection. If the caller
	     returns OpBoolean::IS_TRUE, the loader should continue.
	     OpBoolean::IS_FALSE if the loading operation must block
	     processing of the loader messages. OpStatus::ERR if
	     the redirect should not be followed and the load aborted.
	     OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS OnLoadingFailed(URL &url, BOOL is_network_error) = 0;
	/**< Notification of loader failure while requesting 'url.' is_network_error
	     is TRUE if the failure was due to a network error, not a protocol error.
	     (The former will cause a network error to be reported for XMLHttpRequest,
	     the latter will simply set the response status accordingly.)

	     The loader will propagate OOM if OpStatus::ERR_NO_MEMORY is returned,
	     otherwise the load operation will be finished and shutdown after having
	     called OnLoadingFailed(). */

#ifdef PROGRESS_EVENTS_SUPPORT
	virtual OP_BOOLEAN OnTimeout() = 0;
	/**< Notification of expiry of the supplied timeout. The loader
	     will propagate OOM on OpStatus::ERR_NO_MEMORY. If the handler
	     returns a value of OpBoolean::IS_TRUE, the load operation
	     is interpreted as cancelled. OpBoolean::IS_FALSE signals that
	     the timeout has no impact on the load operation and should
	     continue. */

	virtual unsigned GetTimeoutMS() = 0;
	/**< Return the timeout limit in milliseconds that the loader is
	     allowed to use for the operation. 0 if no timeout restriction. */

	virtual OP_STATUS OnProgressTick() = 0;
	/**< Notification of a progress tick. The loader will propagate
	     OOM on OpStatus::ERR_NO_MEMORY, otherwise the load operation
	     will continue. */

	virtual OP_STATUS OnUploadProgress(OpFileLength bytes) = 0;
	/**< Notification of upload progress. The loader will signal
	     OOM if the result is OpStatus::ERR_NO_MEMORY. */
#endif // PROGRESS_EVENTS_SUPPORT
};

class DOM_LSLoader
	: public XMLParser::Listener
	, public OpElementCallback
	, public MessageObject
	, public ListElement<DOM_LSLoader>
	, public OpSecurityCallbackOwner
{
protected:
	DOM_EnvironmentImpl *environment;
	DOM_LSParser *parser;
	DOM_LSContentHandler *contenthandler;
	MessageHandler *mh;
	URL url;
	URL_InUse url_in_use;
	URL ref_url;
	URL_DataDescriptor *url_dd;
	uni_char *string;
	BOOL string_consumed;
	XMLParser *xml_parser;
	XMLTokenHandler *xml_tokenhandler;
	HTML_Element **queue;
	unsigned queue_used, queue_size;
	TempBuffer text;
	BOOL is_cdata_section;
	BOOL finished;
	BOOL stopped;
	BOOL oom;
	BOOL parsing_failed;
	BOOL is_parsing_fragment;
	BOOL headers_handled;
	BOOL has_doctype_or_root;
	BOOL headers_loaded_received;
	BOOL no_parsing;

	DOM_LSLoadHandler *load_handler;

#ifdef PROGRESS_EVENTS_SUPPORT
	BOOL with_progress_ticks;
	/**< TRUE if listening for progress ticks. */
#endif // PROGRESS_EVENTS_SUPPORT

	/** If the loader has to suspend waiting on the completion of a
	    security check on a redirect, record any messages delivered
	    until the security check finishes. */
	class RecordedMessage
	{
	public:
		RecordedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
			: msg(msg)
			, par1(par1)
			, par2(par2)
		{
		}

		OpMessage msg;
		MH_PARAM_1 par1;
		MH_PARAM_2 par2;
	};

	OpAutoVector<RecordedMessage> recorded_messages;
	BOOL blocked_on_security_check;

	void LoadData();
#ifdef DOM_HTTP_SUPPORT
	void HandleResponseHeaders();
#endif // DOM_HTTP_SUPPORT
	OP_STATUS SetCallbacks(DOM_LSLoadHandler *handler);

	/* From OpElementCallback. */
	virtual OP_STATUS AddElement(HTML_Element *element);
	virtual OP_STATUS EndElement();

	OP_STATUS SetElement(HTML_Element *element);

	/**
	 * Make sure we have a MessageHandler and xmlparser and similar
	 * objects needed by the load.
	 */
	OP_STATUS CreateLoadRelatedObjects(URL local_url, BOOL is_fragment);

	/**
	 * Deletes objects used and needed during loading to save memory and
	 * prevent dangling pointers in case this object out-lives for instance
	 * the Window object used in the MessageHandler we have.
	 */
	void DeleteLoadRelatedObjects();

	/**
	 * @param abort TRUE to abort loading of the URL, FALSE if this is unnecessary.
	 */
	void HandleFinished(BOOL abort = TRUE);
	void HandleError(BOOL from_parser);
	void HandleOOM();

	/* From XMLParser::Listener. */
	virtual void Continue(XMLParser *parser);
	virtual void Stopped(XMLParser *parser);

	/* From MessageObject. */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
		@param content_type Content Type of the URL
		@param url Optional URL used to check the MIME type. It will NOT retrieve the content type from this URL

		@return TRUE if, for a XHR requests, the URL is considered containing XML (responseXML will be populated), FALSE if it is not (responseXML will be NULL).
			This method follows the W3C specifications for XHR.
	*/
	static BOOL ValidXHRXMLType(URLContentType content_type, URL *url);

public:
	DOM_LSLoader(DOM_LSParser *parser);

	~DOM_LSLoader();

	void SetContentHandler(DOM_LSContentHandler *contenthandler);
	void SetLoadHandler(DOM_LSLoadHandler *handler);

#ifdef PROGRESS_EVENTS_SUPPORT
	OP_STATUS PostProgressTick(unsigned millisecs);
	/**< Schedule the delivery of a progress tick 'millisecs'
	     into the future. A progress tick notification is
	     reported through the loader's handler object. */

#endif // PROGRESS_EVENTS_SUPPORT

	/**
	 * @return OpStatus::ERR_NO_MEMORY on OOM, and OpStatus::ERR
	 *         if the loader failed to create, but it should not
	 *         be reported as a security error (just a general one.)
	 *         OpStatus::OK if the loader has been created and
	 *         started.
	 */
	OP_STATUS Construct(URL *url, URL *ref_url, const uni_char *string, BOOL is_fragment, DOM_LSLoadHandler *handler);
	void Abort();

	void GetElements(HTML_Element **&elements, unsigned &count);
	void ConsumeElements(unsigned count);

	URL GetURL() { return url; }
	/**< Return the URL the loader started with. */

	DOM_LSParser *GetLSParser() { return parser; }

	void SetParsingFragment() { is_parsing_fragment = TRUE; }
	void DisableParsing(BOOL f) { no_parsing = f; }
	/**
	 * This is not a DOM_Object GCTrace, but a help method for the parser
	 * GCTrace so that elements in the queue are kept alive in case they
	 * have somehow (by HTML_Element::Construct) been given a DOM node.
	 */
	void GCTrace();

	virtual void SecurityCheckCompleted(BOOL allowed, OP_STATUS error);
	/**< Notification of a security check for a redirect having completed. */
};

#endif // DOM3_LOAD
#endif // DOM_LSLOADER
