/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSCONTENTHANDLER
#define DOM_LSCONTENTHANDLER

#ifdef DOM3_LOAD

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domload/lsparser.h"

class DOM_LSLoader;
class ES_Thread;
class ES_ThreadListener;
class DOM_LSContentHandler_AsyncCallback;
class DOM_LSContentHandler_MutationListener;
class DOM_LSContentHandler_InsertionPoint;
class XMLDocumentInformation;

class DOM_LSContentHandler
{
protected:
	DOM_LSParser *parser;
	DOM_LSLoader *loader;
	unsigned whatToShow;
	ES_Object *filter;
	ES_Thread *handler_thread, *calling_thread;
	ES_Value return_value;
	DOM_LSContentHandler_AsyncCallback *callback;
	BOOL callback_in_use;
	DOM_LSContentHandler_MutationListener *mutationlistener;
	DOM_Node *parent, *before;
	BOOL done, blocked, empty, restart_insertBefore, after_filter, skip, reject, interrupt;
	DOM_LSContentHandler_InsertionPoint *insertionpoint;
	XMLDocumentInformation *docinfo;

	OP_STATUS StartElement(HTML_Element *element);
	OP_STATUS EndElement(DOM_Node *node);

	OP_STATUS PushInsertionPoint(DOM_Node *parent, DOM_Node *before);
	void PopInsertionPoint();

public:
	DOM_LSContentHandler(DOM_LSParser *parser, DOM_LSLoader *loader, unsigned whatToShow, ES_Object *filter);
	~DOM_LSContentHandler();

	void SetInsertionPoint(DOM_Node *parent, DOM_Node *before);
	void SetCallingThread(ES_Thread *calling_thread);
	void Abort();
	void GCTrace();

	void ThreadRemoved();
	void ThreadCancelled();

	BOOL IsDone() { return done; }
	BOOL IsEmpty() { return empty; }
	BOOL IsRunning() { return handler_thread != NULL; }
	void DisableParsing() { parser->DisableParsing(); }

	OP_STATUS HandleElements();
	OP_STATUS HandleFinished();
	OP_STATUS HandleDocumentInfo(const XMLDocumentInformation &docinfo);
	OP_STATUS MoreElementsAvailable();
	OP_STATUS FilterFinished(const ES_Value &result);

	ES_Thread *GetInterruptThread();
};

#endif // DOM3_LOAD
#endif // DOM_LSCONTENTHANDLER
