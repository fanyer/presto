/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSPARSER_H
#define DOM_LSPARSER_H

#ifdef DOM3_LOAD

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/url/url2.h"
#include "modules/security_manager/include/security_manager.h"

class DOM_LSLoader;
class DOM_LSContentHandler;
class DOM_DOMConfiguration;
class DOM_EventTarget;
class DOM_Document;
class DOM_DocumentFragment;
class DOM_Node;
class ES_Thread;
class ES_ThreadListener;
class XMLDocumentInformation;

#ifdef DOM_HTTP_SUPPORT
class DOM_XMLHttpRequest;
#endif // DOM_HTTP_SUPPORT

class DOM_LSParser_ThreadListener;
class DOM_LSParser_SecurityCallback;
class DOM_LSParser_LoadHandler;

class DOM_LSParser
	: public DOM_Object,
	  public DOM_EventTargetOwner
{
protected:
	DOM_LSParser(BOOL async);

	virtual ~DOM_LSParser();

	DOM_LSLoader *loader;
	DOM_LSContentHandler *contenthandler;

	BOOL async;
	BOOL busy;
	BOOL failed;
	BOOL oom;
	BOOL need_reset;
	BOOL is_fragment;
	BOOL is_document_load;
	BOOL document_info_handled;
	BOOL with_dom_config;
	BOOL is_parsing_error_document; ///< See DOM_DOMParser::parseFromString().
	DOM_DOMConfiguration *config;
	ES_Object *filter;
	unsigned whatToShow;
	unsigned action;

	ES_Thread *calling_thread;
	DOM_LSParser_ThreadListener *calling_thread_listener;
	DOM_EventTarget *eventtarget;
	DOM_Document *document;
	DOM_Node *first_top_node, *context, *parent, *before;
	ES_Object *input;
	uni_char *uri, *string;
	int keep_alive_id;
	URL parsedurl;

#ifdef DOM_HTTP_SUPPORT
	URL base_url;
	/**< URLs are resolved with respect to the document's base.
	     For XMLHttpRequest uses, it is the base at the time of
	     open(). */
#endif // DOM_HTTP_SUPPORT

	URL restart_url;
	uni_char *restart_systemId;
	uni_char *restart_stringData;
	DOM_Node *restart_parent;
	DOM_Node *restart_before;

	BOOL has_started_parsing;
	/**< TRUE iff the parser has completed the security check and started
	     parsing. Upon restart from completion of the parse, the
	     parser outcome must be checked for (and no repeating
	     of the security check nor re-initialisation of the parser.)

	     [This flag serves the purpose of disambiguating restart
	      reasons; resumption from parser completion or security check
	      completion.] */

	BOOL security_check_passed;
	/**< TRUE when the security check (via the callback) reports that
	     the loading of the document is allowed. FALSE at any other time.
	     MUST only be set upon notification from that callback. */

	DOM_LSParser_SecurityCallback *security_callback;
	/**< The document load must pass a DOM_LOADSAVE security check to go
	     ahead; the outcome is signalled through the security callback. */

	DOM_LSParser_LoadHandler *load_handler;
	/**< Once activated, the loader will delegate the handling of operations
	     to the parser through notifications/upcalls. */

#ifdef DOM_HTTP_SUPPORT
	DOM_XMLHttpRequest *xmlhttprequest;
#endif // DOM_HTTP_SUPPORT

	BOOL parse_document;

	/* Copied from DOM_NodeFilter. */
	enum
	{
		SHOW_ALL                       = 0xfffffffful, // Might be stored as -1 depending on compiler.
		SHOW_ELEMENT                   = 0x00000001ul,
		SHOW_ATTRIBUTE                 = 0x00000002ul,
		SHOW_TEXT                      = 0x00000004ul,
		SHOW_CDATA_SECTION             = 0x00000008ul,
		SHOW_ENTITY_REFERENCE          = 0x00000010ul,
		SHOW_ENTITY                    = 0x00000020ul,
		SHOW_PROCESSING_INSTRUCTION    = 0x00000040ul,
		SHOW_COMMENT                   = 0x00000080ul,
		SHOW_DOCUMENT                  = 0x00000100ul,
		SHOW_DOCUMENT_TYPE             = 0x00000200ul,
		SHOW_DOCUMENT_FRAGMENT         = 0x00000400ul,
		SHOW_NOTATION                  = 0x00000800ul
	};

	OP_STATUS Start(URL url, const uni_char *uri, const uni_char *string, DOM_Node *parent, DOM_Node *before, DOM_Runtime *origining_runtime, BOOL &is_security_error);

	OP_STATUS SetKeepAlive();
	/**< Pins this object to the runtime, so it's
	     not gc'ed while there is a network request running. */
	void UnsetKeepAlive();
	/**< Unpins this object from the runtime, so it can be gc'ed. */

	// From the DOM_EventTargetOwner interface.
	virtual DOM_Object *GetOwnerObject() { return this; }

public:
	enum
	{
		MODE_SYNCHRONOUS = 1,
		MODE_ASYNCHRONOUS
	};

	enum
	{
		ACTION_APPEND_AS_CHILDREN = 1,
		ACTION_REPLACE_CHILDREN,
		ACTION_INSERT_BEFORE,
		ACTION_INSERT_AFTER,
		ACTION_REPLACE
	};

	enum
	{
		FILTER_ACCEPT = 1,
		FILTER_REJECT,
		FILTER_SKIP,
		FILTER_INTERRUPT
	};

#ifdef DOM_HTTP_SUPPORT
	static OP_STATUS Make(DOM_LSParser *&parser, DOM_EnvironmentImpl *environment, BOOL async, DOM_XMLHttpRequest *xmlhttprequest = NULL);
#else // DOM_HTTP_SUPPORT
	static OP_STATUS Make(DOM_LSParser *&parser, DOM_EnvironmentImpl *environment, BOOL async);
#endif // DOM_HTTP_SUPPORT

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_LSPARSER || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_EventTarget *GetEventTarget(BOOL create);
	DOM_Document *GetOwnerDocument();
	DOM_Node *GetContext() { return context; }
	unsigned GetAction() { return action; }
	void DisableParsing() { parse_document = FALSE; }

	OP_STATUS HandleDocumentInfo(const XMLDocumentInformation &docinfo);
	OP_STATUS HandleFinished();
	OP_STATUS HandleProgress(unsigned position, unsigned total);
	OP_STATUS SignalError(const uni_char *message, const uni_char *type, unsigned line, unsigned column, unsigned byteOffset, unsigned charOffset);
	void SignalOOM();
	void NewTopNode(DOM_Node *node);
	void Reset();
	void CallingThreadCancelled();
	OP_STATUS ResetCallingThread();
	OP_STATUS UnblockCallingThread();

	void OnSecurityCheckResult(BOOL allowed);

#ifdef DOM_HTTP_SUPPORT
	DOM_XMLHttpRequest *GetXMLHttpRequest();

	void SetBaseURL();
	/**< Fix the base URL with respect to the document's current base. */
#endif // DOM_HTTP_SUPPORT

	void SetIsDocumentLoad() { is_document_load = TRUE; }

	void SetIsParsingErrorDocument() { is_parsing_error_document = TRUE; }
	BOOL GetIsParsingErrorDocument() { return is_parsing_error_document; }

	DOM_LSContentHandler *GetContentHandler() { return contenthandler; }

	DOM_LSLoader *GetLoader() { return loader; }

	void SetParseLoadedData(BOOL enable);
	/**< If the provided flag is FALSE, do not parse the loaded data, but have
	     the parser function as a loader of raw data. The default is
	     to parse the incoming data. */

	static void ConstructLSParserObjectL(ES_Object *object, DOM_Runtime *runtime);
	static void ConstructLSParserFilterObjectL(ES_Object *object, DOM_Runtime *runtime);
	static void ConstructDOMImplementationLSObjectL(ES_Object *object, DOM_Runtime *runtime);
	virtual BOOL IsAsync() { return async; }

	// Also: dispatchEvent
	DOM_DECLARE_FUNCTION(abort);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

	// Also: {add,remove}EventListener{,NS}
	DOM_DECLARE_FUNCTION_WITH_DATA(parse); // parse, parseURI and parseWithContext
	enum {
		FUNCTIONS_WITH_DATA_BASIC = 5,
#ifdef DOM3_EVENTS
		FUNCTIONS_WITH_DATA_addEventListenerNS,
		FUNCTIONS_WITH_DATA_removeEventListenerNS,
#endif // DOM3_EVENTS
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

#endif // DOM3_LOAD
#endif // DOM_LSPARSER_H
