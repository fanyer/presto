/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domload/lscontenthandler.h"
#include "modules/dom/src/domload/lsloader.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domload/lsthreads.h"

#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/xmlutils/xmldocumentinfo.h"

class DOM_LSContentHandler_AsyncCallback
	: public ES_AsyncCallback
{
protected:
	DOM_LSContentHandler *contenthandler;

public:
	DOM_LSContentHandler_AsyncCallback(DOM_LSContentHandler *contenthandler);

	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

	void Detach() { contenthandler = NULL; }
};

DOM_LSContentHandler_AsyncCallback::DOM_LSContentHandler_AsyncCallback(DOM_LSContentHandler *contenthandler)
	: contenthandler(contenthandler)
{
}

OP_STATUS
DOM_LSContentHandler_AsyncCallback::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	if (contenthandler)
	{
		ES_Value value = result;

		if (status != ES_ASYNC_SUCCESS)
			DOM_Object::DOMSetNumber(&value, 4);

		return contenthandler->FilterFinished(value);
	}
	else
	{
		DOM_LSContentHandler_AsyncCallback *callback = this;
		OP_DELETE(callback);
		return OpStatus::OK;
	}
}

class DOM_LSContentHandler_Thread
	: public ES_Thread
{
protected:
	DOM_LSContentHandler *contenthandler;

public:
	DOM_LSContentHandler_Thread(DOM_LSContentHandler *lscontenthandler);
	~DOM_LSContentHandler_Thread();

	virtual OP_STATUS EvaluateThread();
	virtual OP_STATUS Signal(ES_ThreadSignal signal);
};

DOM_LSContentHandler_Thread::DOM_LSContentHandler_Thread(DOM_LSContentHandler *contenthandler)
	: ES_Thread(NULL),
	  contenthandler(contenthandler)
{
}

DOM_LSContentHandler_Thread::~DOM_LSContentHandler_Thread()
{
	contenthandler->ThreadRemoved();
}

OP_STATUS
DOM_LSContentHandler_Thread::EvaluateThread()
{
	while (!IsBlocked())
	{
		OP_STATUS status = contenthandler->HandleElements();

		if (OpStatus::IsMemoryError(status))
			return status;
		if (OpStatus::IsError(status) || contenthandler->IsDone() || contenthandler->IsEmpty())
		{
			is_completed = TRUE;
			break;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_LSContentHandler_Thread::Signal(ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED)
		contenthandler->ThreadCancelled();

	return ES_Thread::Signal(signal);
}

class DOM_LSContentHandler_InsertionPoint
{
public:
	~DOM_LSContentHandler_InsertionPoint()
	{
		OP_DELETE(previous);
	}

	void GCTrace()
	{
		DOM_Object::GCMark(parent);
		DOM_Object::GCMark(before);
		if (previous)
			previous->GCTrace();
	}

	DOM_Node *parent, *before;
	BOOL skip, reject;
	DOM_LSContentHandler_InsertionPoint *previous;
};

OP_STATUS
DOM_LSContentHandler::StartElement(HTML_Element *element)
{
	DOM_EnvironmentImpl *environment = parser->GetEnvironment();
	DOM_Node *node;

	RETURN_IF_ERROR(environment->ConstructNode(node, element, parser->GetOwnerDocument()));

	if (!after_filter)
	{
		if (reject)
		{
			if (node->IsA(DOM_TYPE_ELEMENT))
				RETURN_IF_ERROR(PushInsertionPoint(NULL, NULL));

			return OpStatus::OK;
		}

		if (node->IsA(DOM_TYPE_ELEMENT))
		{
			after_filter = FALSE;

			RETURN_IF_ERROR(PushInsertionPoint(node, NULL));

			if (filter)
			{
				if (!(callback || (callback = OP_NEW(DOM_LSContentHandler_AsyncCallback, (this)))))
					return OpStatus::ERR_NO_MEMORY;

				ES_Value arguments[1];
				DOM_Object::DOMSetObject(&arguments[0], node);

				ES_AsyncInterface *asyncif = environment->GetAsyncInterface();
				OP_STATUS status = asyncif->CallMethod(filter, UNI_L("startElement"), 1, arguments, callback, handler_thread);

				if (OpStatus::IsMemoryError(status))
					return status;
				else if (OpStatus::IsSuccess(status))
				{
					blocked = after_filter = callback_in_use = TRUE;
					return status;
				}
			}

			return OpStatus::OK;
		}
		else
			return EndElement(node);
	}
	else if (node->IsA(DOM_TYPE_ELEMENT))
	{
		after_filter = FALSE;
		return OpStatus::OK;
	}
	else
		return EndElement(node);
}

OP_STATUS
DOM_LSContentHandler::EndElement(DOM_Node *node)
{
	DOM_EnvironmentImpl *environment = parser->GetEnvironment();

	if (!after_filter)
	{
		BOOL do_skip = skip, do_reject = reject;

		if (!node || node->IsA(DOM_TYPE_ELEMENT))
			if (do_skip)
			{
				PopInsertionPoint();
				return OpStatus::OK;
			}

		if (do_reject)
		{
			PopInsertionPoint();
			return OpStatus::OK;
		}

		OP_ASSERT(node);

		if (filter && (!insertionpoint || !insertionpoint->skip && !insertionpoint->reject))
		{
			if (!(callback || (callback = OP_NEW(DOM_LSContentHandler_AsyncCallback, (this)))))
				return OpStatus::ERR_NO_MEMORY;

			ES_Value arguments[1];
			DOM_Object::DOMSetObject(&arguments[0], node);

			ES_AsyncInterface *asyncif = environment->GetAsyncInterface();
			OP_STATUS status = asyncif->CallMethod(filter, UNI_L("acceptNode"), 1, arguments, callback, handler_thread);

			if (OpStatus::IsMemoryError(status))
				return status;
			else if (OpStatus::IsSuccess(status))
			{
				blocked = after_filter = callback_in_use = TRUE;
				return status;
			}
		}
	}

	BOOL insert = TRUE, insert_children = FALSE;

	if (after_filter)
	{
		if (reject)
			insert = FALSE;
		else if (skip)
			insert_children = TRUE;

		after_filter = FALSE;
	}

	if (!node || node->IsA(DOM_TYPE_ELEMENT))
		PopInsertionPoint();

	if (insert)
	{
		DOM_Node *top_node;

		if (insert_children && node)
			RETURN_IF_ERROR(node->GetFirstChild(top_node));
		else
			top_node = node;

		if (!insertionpoint && top_node)
			parser->NewTopNode(top_node);

		DOM_Node *target = (skip && insertionpoint) ? insertionpoint->parent : parent;

		if (!insert_children)
		{
			ES_Value arguments[2];
			DOM_Object::DOMSetObject(&arguments[0], node);
			DOM_Object::DOMSetObject(&arguments[1], before);

			int result = DOM_Node::insertBefore(target, arguments, 2, &return_value, environment->GetDOMRuntime());

			if (result == (ES_SUSPEND | ES_RESTART))
				blocked = restart_insertBefore = TRUE;
			else if (result == ES_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;

			OP_ASSERT(blocked || result == ES_VALUE);
		}
		else if (top_node)
		{
			ES_Thread *thread = OP_NEW(DOM_LSParser_InsertThread, (parent, before, node, TRUE));
			if (!thread)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(environment->GetScheduler()->AddRunnable(thread, handler_thread));
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_LSContentHandler::PushInsertionPoint(DOM_Node *parent_, DOM_Node *before_)
{
	DOM_LSContentHandler_InsertionPoint *point = OP_NEW(DOM_LSContentHandler_InsertionPoint, ());
	if (!point)
		return OpStatus::ERR_NO_MEMORY;

	point->parent = parent;
	point->before = before;
	point->skip = skip;
	point->reject = reject;
	point->previous = insertionpoint;

	parent = parent_;
	before = before_;
	skip = FALSE;
	insertionpoint = point;
	return OpStatus::OK;
}

void
DOM_LSContentHandler::PopInsertionPoint()
{
	OP_ASSERT(insertionpoint);

	DOM_LSContentHandler_InsertionPoint *point = insertionpoint;

	parent = point->parent;
	before = point->before;
	skip = point->skip;
	reject = point->reject;
	insertionpoint = point->previous;

	point->previous = NULL;
	OP_DELETE(point);
}

DOM_LSContentHandler::DOM_LSContentHandler(DOM_LSParser *parser, DOM_LSLoader *loader, unsigned whatToShow, ES_Object *filter)
	: parser(parser),
	  loader(loader),
	  whatToShow(whatToShow),
	  filter(filter),
	  handler_thread(NULL),
	  calling_thread(NULL),
	  callback(NULL),
	  callback_in_use(FALSE),
	  mutationlistener(NULL),
	  parent(NULL),
	  before(NULL),
	  done(FALSE),
	  blocked(FALSE),
	  empty(TRUE),
	  restart_insertBefore(FALSE),
	  after_filter(FALSE),
	  skip(FALSE),
	  reject(FALSE),
	  interrupt(FALSE),
	  insertionpoint(NULL),
	  docinfo(NULL)
{
	return_value.type = VALUE_UNDEFINED;
}

DOM_LSContentHandler::~DOM_LSContentHandler()
{
	Abort();
}

void
DOM_LSContentHandler::SetInsertionPoint(DOM_Node *parent_, DOM_Node *before_)
{
	parent = parent_;
	before = before_;
}

void
DOM_LSContentHandler::SetCallingThread(ES_Thread *calling_thread_)
{
	calling_thread = calling_thread_;
	if (calling_thread_ == NULL && handler_thread)
		handler_thread->GetScheduler()->CancelThread(handler_thread);
}

void
DOM_LSContentHandler::Abort()
{
	if (handler_thread)
		handler_thread->GetScheduler()->CancelThread(handler_thread);

	OP_DELETE(insertionpoint);
	insertionpoint = NULL;

	OP_DELETE(docinfo);
	docinfo = NULL;

	if (callback_in_use)
		callback->Detach();
	else
		OP_DELETE(callback);
	callback = NULL;
}

void
DOM_LSContentHandler::GCTrace()
{
	parent->GCMark(return_value);
	parent->GCMark(parent);
	parent->GCMark(before);

	if (insertionpoint)
		insertionpoint->GCTrace();
}

void
DOM_LSContentHandler::ThreadRemoved()
{
	handler_thread = NULL;
}

void
DOM_LSContentHandler::ThreadCancelled()
{
	handler_thread = NULL;
}

OP_STATUS
DOM_LSContentHandler::HandleElements()
{
	if (docinfo)
	{
		DOM_Document *doc = parser->GetOwnerDocument();

		RETURN_IF_ERROR(doc->SetXMLDocumentInfo(docinfo));

		OP_DELETE(docinfo);
		docinfo = NULL;
	}

	if (restart_insertBefore)
	{
		int result = DOM_Node::insertBefore(NULL, NULL, -1, &return_value, parser->GetEnvironment()->GetDOMRuntime());

		if (result == (ES_SUSPEND | ES_RESTART))
			return OpStatus::OK;
		else if (result == ES_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

		restart_insertBefore = FALSE;
	}
	else if (after_filter)
	{
		if (return_value.type == VALUE_NUMBER)
			switch ((int) return_value.value.number)
			{
			case DOM_LSParser::FILTER_SKIP:
				skip = TRUE;
				break;

			case DOM_LSParser::FILTER_REJECT:
				reject = TRUE;
				break;

			case DOM_LSParser::FILTER_INTERRUPT:
				interrupt = TRUE;
				break;
			}
	}

	HTML_Element **elements;
	unsigned index, count;
	BOOL dont_track_updates = !parser->GetContext() || !parser->GetEnvironment()->HasCollections();

	loader->GetElements(elements, count);

	if (dont_track_updates)
		parser->GetEnvironment()->SetTrackCollectionChanges(FALSE);

	for (index = 0, blocked = FALSE; index < count; ++index)
	{
		HTML_Element *element = elements[index];

		if (element)
			RETURN_IF_ERROR(StartElement(element));
		else
			RETURN_IF_ERROR(EndElement(parent));

		if (blocked)
			break;
	}

	if (index > 0)
		loader->ConsumeElements(index);

	if (dont_track_updates)
		parser->GetEnvironment()->SetTrackCollectionChanges(TRUE);

	if (index == count)
		empty = TRUE;

	return OpStatus::OK;
}

OP_STATUS
DOM_LSContentHandler::HandleFinished()
{
	return parser->HandleFinished();
}

OP_STATUS
DOM_LSContentHandler::HandleDocumentInfo(const XMLDocumentInformation &new_docinfo)
{
	OP_DELETE(docinfo);

	docinfo = OP_NEW(XMLDocumentInformation, ());
	if (!docinfo)
		return OpStatus::ERR_NO_MEMORY;

	return docinfo->Copy(new_docinfo);
}

OP_STATUS
DOM_LSContentHandler::MoreElementsAvailable()
{
	empty = FALSE;

	if (!handler_thread)
	{
		handler_thread = OP_NEW(DOM_LSContentHandler_Thread, (this));
		if (!handler_thread)
			return OpStatus::ERR_NO_MEMORY;

		OP_BOOLEAN result = parser->GetRuntime()->GetESScheduler()->AddRunnable(handler_thread, calling_thread);

		if (OpStatus::IsError(result))
			return result;
		else if (result == OpBoolean::IS_FALSE)
		{
			handler_thread = NULL;
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_LSContentHandler::FilterFinished(const ES_Value &result)
{
	return_value = result;
	callback_in_use = FALSE;
	return OpStatus::OK;
}

ES_Thread *
DOM_LSContentHandler::GetInterruptThread()
{
	return calling_thread;
}

#endif // DOM3_LOAD
