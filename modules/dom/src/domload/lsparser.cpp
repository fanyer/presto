/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domconfiguration.h"
#include "modules/dom/src/domcore/domxmldocument.h"
#include "modules/dom/src/domcore/domerror.h"
#include "modules/dom/src/domcore/domlocator.h"
#include "modules/dom/src/domcore/entity.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domload/lscontenthandler.h"
#include "modules/dom/src/domload/lsevents.h"
#include "modules/dom/src/domload/lsexception.h"
#include "modules/dom/src/domload/lsinput.h"
#include "modules/dom/src/domload/lsloader.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domload/lsthreads.h"
#include "modules/dom/src/opera/domhttp.h"
#include "modules/dom/src/js/window.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"

#include "modules/dochand/win.h"

#include "modules/security_manager/include/security_manager.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"

class DOM_LSParser_ThreadListener
	: public ES_ThreadListener
{
private:
	DOM_LSParser *parser;
	ES_Thread *thread;

public:
	DOM_LSParser_ThreadListener(DOM_LSParser *parser, ES_Thread *thread)
		: parser(parser)
		, thread(thread)
	{
		thread->AddListener(this);
	}

	virtual ~DOM_LSParser_ThreadListener()
	{
		ES_ThreadListener::Remove();
	}

	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_CANCELLED)
			parser->CallingThreadCancelled();

		return OpStatus::OK;
	}
};

class DOM_LSParser_LoadHandler
	: public DOM_LSLoadHandler
{
public:
	DOM_LSParser_LoadHandler(DOM_Runtime *origining_runtime, DOM_LSParser *parser, DOM_LSLoader *loader)
		: origining_runtime(origining_runtime)
		, parser(parser)
		, loader(loader)
		, headers_handled(FALSE)
		, security_callback(NULL)
	{
	}

	virtual ~DOM_LSParser_LoadHandler()
	{
		if (security_callback)
			security_callback->Release();
	}

	virtual OP_STATUS OnHeadersReceived(URL &url);
	virtual OP_BOOLEAN OnRedirect(URL &url, URL &moved_to_url);
	virtual OP_STATUS OnLoadingFailed(URL &url, BOOL is_network_error);
#ifdef PROGRESS_EVENTS_SUPPORT
	virtual OP_BOOLEAN OnTimeout();
	virtual unsigned GetTimeoutMS();
	virtual OP_STATUS OnProgressTick();
	virtual OP_STATUS OnUploadProgress(OpFileLength bytes);
#endif // PROGRESS_EVENTS_SUPPORT

private:
	DOM_Runtime *origining_runtime;
	DOM_LSParser *parser;
	DOM_LSLoader *loader;
	BOOL headers_handled;

	OpSecurityCallbackHelper *security_callback;
};

/* Loading an external resource is gated on passing a DOM_LOADSAVE security
   check. This callback handles the interaction with the security manager
   during that check. */
class DOM_LSParser_SecurityCallback
	: public OpSecurityCheckCallback
	, public ES_ThreadListener
{
public:
	DOM_LSParser_SecurityCallback(DOM_LSParser *parser)
		: parser(parser)
		, error(OpStatus::OK)
		, is_finished(FALSE)
		, is_allowed(FALSE)
		, thread(NULL)
	{
	}

	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE)
	{
		is_allowed = allowed;
		if (parser)
			parser->OnSecurityCheckResult(allowed);
		Finish();
	}

	virtual void OnSecurityCheckError(OP_STATUS status)
	{
		error = status;
		if (parser)
			parser->OnSecurityCheckResult(FALSE);
		Finish();
	}

	BOOL IsFinished() { return is_finished; }
	BOOL IsAllowed() { return is_allowed; }

	void Cancel()
	{
		OP_ASSERT(!is_finished);
		thread = NULL;
		parser = NULL;
		ES_ThreadListener::Remove();
	}

	void BlockOnSecurityCheck(ES_Thread *t)
	{
		OP_ASSERT(!thread || !t);
		thread = t;
		if (thread)
		{
			thread->AddListener(this);
			thread->Block();
		}
	}

	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_CANCELLED || signal == ES_SIGNAL_FAILED)
		{
			ES_ThreadListener::Remove();
			thread = NULL;
		}

		return OpStatus::OK;
	}

private:
	void Finish()
	{
		if (!is_finished)
		{
			is_finished = TRUE;
			if (thread)
			{
				ES_ThreadListener::Remove();
				thread->Unblock();
			}
			thread = NULL;
		}
		if (!parser)
			OP_DELETE(this);
	}

	DOM_LSParser *parser;
	OP_STATUS error;
	BOOL is_finished;
	BOOL is_allowed;
	ES_Thread *thread;
};

OP_BOOLEAN
DOM_LSParser_LoadHandler::OnHeadersReceived(URL &url)
{
#if defined(CORS_SUPPORT) && defined(DOM_HTTP_SUPPORT)
	if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
		if (OpCrossOrigin_Request *cross_origin_request = xmlhttprequest->GetCrossOriginRequest())
		{
			/* Perform final resource sharing check for CORS. */
			BOOL allowed = FALSE;
			URL target_url = url.GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
			if (!target_url.IsValid())
				target_url = url;
			OpSecurityContext source_context(origining_runtime, cross_origin_request);
			OpSecurityContext target_context(target_url);
			/* This is a sync security check. */
			OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::CORS_RESOURCE_ACCESS, source_context, target_context, allowed);
			if (OpStatus::IsMemoryError(status))
				return status;

			if (OpStatus::IsError(status))
				allowed = FALSE;

			if (parser)
				parser->OnSecurityCheckResult(allowed);

			if (allowed)
				RETURN_IF_ERROR(xmlhttprequest->SetStatus(url));
			else
			{
				ES_Thread *interrupt_thread = parser->GetContentHandler()->GetInterruptThread();
				xmlhttprequest->SetNetworkErrorFlag();
				RETURN_IF_ERROR(xmlhttprequest->SetReadyState(DOM_XMLHttpRequest::READYSTATE_LOADED, interrupt_thread));
				return OpStatus::ERR;
			}
		}
#endif // CORS_SUPPORT && DOM_HTTP_SUPPORT
#ifdef DOM_HTTP_SUPPORT
	if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
	{
		/* CORS completed, allowing the response to be used. */
		if (!headers_handled)
		{
			headers_handled = TRUE;
			RETURN_IF_ERROR(xmlhttprequest->SetResponseHeaders(url));
		}
		/* If HEAD, stop loading after having processed the payload. */
		if (uni_stri_eq(xmlhttprequest->GetMethod(), "HEAD"))
			return OpBoolean::IS_FALSE;
	}
#endif // DOM_HTTP_SUPPORT
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
DOM_LSParser_LoadHandler::OnRedirect(URL &url, URL &moved_to_url)
{
	/* Assume that there will only be one redirect "in flight" at any one time. */
	OP_ASSERT(!security_callback || security_callback->IsFinished());
	if (!security_callback)
		RETURN_OOM_IF_NULL(security_callback = OP_NEW(OpSecurityCallbackHelper, (loader)));
	else
		security_callback->Reset();

#ifdef DOM_HTTP_SUPPORT
	DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest();
#endif // DOM_HTTP_SUPPORT
#if defined(CORS_SUPPORT) && defined(DOM_HTTP_SUPPORT)
	OpCrossOrigin_Request *cross_origin_request = xmlhttprequest ? xmlhttprequest->GetCrossOriginRequest() : NULL;
	if (!cross_origin_request && xmlhttprequest)
	{
		OP_BOOLEAN result = xmlhttprequest->CheckIfCrossOriginCandidate(moved_to_url);
		RETURN_IF_ERROR(result);
		if (result == OpBoolean::IS_TRUE)
		{
			/* Check if a cross-origin redirect has been initiated for the redirect target,
			   the original request being to a same-origin target but which became cross-origin
			   when redirecting. */
			if (OpCrossOrigin_Manager *manager = g_secman_instance->GetCrossOriginManager(url))
				cross_origin_request = manager->FindNetworkRedirect(url);

			if (cross_origin_request)
				RETURN_IF_ERROR(xmlhttprequest->SetCrossOriginRequest(cross_origin_request));
			else
			{
				RETURN_IF_ERROR(xmlhttprequest->CreateCrossOriginRequest(loader->GetURL(), moved_to_url));
				cross_origin_request = xmlhttprequest->GetCrossOriginRequest();
			}

			security_callback->OnSecurityCheckSuccess(TRUE);
			return OpBoolean::IS_TRUE;
		}
	}
	if (cross_origin_request)
		cross_origin_request->SetInRedirect();

	OpSecurityContext source_context(origining_runtime, cross_origin_request);
#else
	OpSecurityContext source_context(origining_runtime);
#endif // CORS_SUPPORT && DOM_HTTP_SUPPORT
	OpSecurityContext redirect_context(moved_to_url, url);
	OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE, source_context, redirect_context, security_callback);

	if (OpStatus::IsError(status) || !security_callback->IsAllowed() && security_callback->IsFinished())
	{
#ifdef DOM_HTTP_SUPPORT
		if (xmlhttprequest)
		{
			RETURN_IF_ERROR(xmlhttprequest->SetStatus(url));
			xmlhttprequest->SetNetworkErrorFlag();
			RETURN_IF_ERROR(xmlhttprequest->SetReadyState(DOM_XMLHttpRequest::READYSTATE_LOADED, parser->GetContentHandler()->GetInterruptThread()));
		}
#endif // DOM_HTTP_SUPPORT
		return OpStatus::ERR;
	}
	else if (!security_callback->IsFinished())
		return OpBoolean::IS_FALSE;
	else
		return OpBoolean::IS_TRUE;
}

OP_STATUS
DOM_LSParser_LoadHandler::OnLoadingFailed(URL &url, BOOL is_network_error)
{
#ifdef DOM_HTTP_SUPPORT
	DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest();
#endif // DOM_HTTP_SUPPORT
#if defined(CORS_SUPPORT) && defined(DOM_HTTP_SUPPORT)
	OpCrossOrigin_Request *cross_origin_request = NULL;
	if (is_network_error && xmlhttprequest)
	{
		cross_origin_request = xmlhttprequest->GetCrossOriginRequest();
		if (!cross_origin_request)
		{
			/* Check if a cross-origin redirect was attempted for 'url', which
			   failed. */
			if (OpCrossOrigin_Manager *manager = g_secman_instance->GetCrossOriginManager(url))
			{
				cross_origin_request = manager->FindNetworkRedirect(url);
				if (cross_origin_request)
					RETURN_IF_ERROR(xmlhttprequest->SetCrossOriginRequest(cross_origin_request));
			}
		}
	}
	if (cross_origin_request)
	{
		cross_origin_request->SetNetworkError();
		/* The cross-origin access ran into a network error; forcibly
		   signal this as a security error failure to propagate the
		   required network error back to the user. */
		parser->OnSecurityCheckResult(FALSE);
	}
#endif // CORS_SUPPORT && DOM_HTTP_SUPPORT

#ifdef DOM_HTTP_SUPPORT
	if (xmlhttprequest)
	{
		if (is_network_error)
			RETURN_IF_ERROR(xmlhttprequest->SetNetworkError(parser->GetContentHandler()->GetInterruptThread()));
		else
			RETURN_IF_ERROR(xmlhttprequest->SetStatus(url));
		if (!headers_handled)
		{
			headers_handled = TRUE;
			return xmlhttprequest->SetResponseHeaders(url);
		}
	}
#endif // DOM_HTTP_SUPPORT
	return OpStatus::OK;
}

#ifdef PROGRESS_EVENTS_SUPPORT
OP_BOOLEAN
DOM_LSParser_LoadHandler::OnTimeout()
{
#ifdef DOM_HTTP_SUPPORT
	if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
		return xmlhttprequest->HandleTimeout();
#endif // DOM_HTTP_SUPPORT

	return OpBoolean::IS_FALSE;
}

unsigned
DOM_LSParser_LoadHandler::GetTimeoutMS()
{
#ifdef DOM_HTTP_SUPPORT
	if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
		return xmlhttprequest->GetTimeoutMS();
#endif // DOM_HTTP_SUPPORT
	return 0;
}

OP_STATUS
DOM_LSParser_LoadHandler::OnProgressTick()
{
#ifdef DOM_HTTP_SUPPORT
	if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
		return xmlhttprequest->HandleProgressTick();
#endif // DOM_HTTP_SUPPORT

	return OpStatus::OK;
}

OP_STATUS
DOM_LSParser_LoadHandler::OnUploadProgress(OpFileLength bytes)
{
#ifdef DOM_HTTP_SUPPORT
	if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
		return xmlhttprequest->HandleUploadProgress(bytes);
#endif // DOM_HTTP_SUPPORT

	return OpStatus::OK;
}
#endif // PROGRESS_EVENTS_SUPPORT

DOM_LSParser::DOM_LSParser(BOOL async)
	: loader(NULL)
	, contenthandler(NULL)
	, async(async)
	, busy(FALSE)
	, failed(FALSE)
	, oom(FALSE)
	, need_reset(FALSE)
	, is_fragment(FALSE)
	, is_document_load(FALSE)
	, document_info_handled(FALSE)
	, with_dom_config(TRUE)
	, is_parsing_error_document(FALSE)
	, config(NULL)
	, filter(NULL)
	, whatToShow(static_cast<unsigned>(SHOW_ALL))
	, action(ACTION_APPEND_AS_CHILDREN)
	, calling_thread(NULL)
	, calling_thread_listener(NULL)
	, eventtarget(NULL)
	, document(NULL)
	, first_top_node(NULL)
	, context(NULL)
	, parent(NULL)
	, before(NULL)
	, input(NULL)
	, uri(NULL)
	, string(NULL)
	, keep_alive_id(0)
	, restart_systemId(NULL)
	, restart_stringData(NULL)
	, restart_parent(NULL)
	, restart_before(NULL)
	, has_started_parsing(FALSE)
	, security_check_passed(FALSE)
	, security_callback(NULL)
	, load_handler(NULL)
#ifdef DOM_HTTP_SUPPORT
	, xmlhttprequest(NULL)
#endif // DOM_HTTP_SUPPORT
	, parse_document(TRUE)
{
}

DOM_LSParser::~DOM_LSParser()
{
	keep_alive_id = 0;
	need_reset = TRUE;

	Reset();

	if (security_callback && !security_callback->IsFinished())
		security_callback->Cancel();
	else
		OP_DELETE(security_callback);

	OP_DELETE(load_handler);
}

OP_STATUS
DOM_LSParser::Start(URL url, const uni_char *uri_, const uni_char *string_, DOM_Node *parent, DOM_Node *before, DOM_Runtime *origining_runtime, BOOL &is_security_error)
{
	if (uri_)
		RETURN_IF_ERROR(UniSetStr(uri, uri_));

	if (string_)
		RETURN_IF_ERROR(UniSetStr(string, string_));

	is_security_error = TRUE;
	BOOL allowed = FALSE;
	URL ref_url;
	if (uri || !url.IsEmpty())
	{
		DOM_Runtime* object_runtime = GetRuntime();

		if (FramesDocument *obj_doc = object_runtime->GetFramesDocument())
			ref_url = obj_doc->GetURL();
		else
		{
#ifdef DOM_WEBWORKERS_SUPPORT
			if (GetEnvironment()->GetWorkerController()->GetWorkerObject())
				ref_url = GetEnvironment()->GetWorkerController()->GetWorkerObject()->GetLocationURL();
			else
#endif // DOM_WEBWORKERS_SUPPORT
				ref_url = origining_runtime->GetFramesDocument()->GetURL();
		}

		if (uri)
			url = g_url_api->GetURL(ref_url, uri, FALSE);

#if defined(CORS_SUPPORT) && defined(DOM_HTTP_SUPPORT)
		if (xmlhttprequest && xmlhttprequest->WantCrossOriginRequest() && !xmlhttprequest->GetCrossOriginRequest())
			CALL_FAILED_IF_ERROR(xmlhttprequest->CreateCrossOriginRequest(ref_url, url));
		OpCrossOrigin_Request *cross_origin_request = xmlhttprequest ? xmlhttprequest->GetCrossOriginRequest() : NULL;
#endif // CORS_SUPPORT && DOM_HTTP_SUPPORT

		// The request will be made with the object's security rather than the scripts.
		// See CORE-15635 for the background
#ifdef CORS_SUPPORT
		OpSecurityContext source_context(object_runtime, cross_origin_request);
#else
		OpSecurityContext source_context(object_runtime);
#endif // CORS_SUPPORT
		OpSecurityContext target_context(url, ref_url);

		if (!security_callback)
			RETURN_OOM_IF_NULL(security_callback = OP_NEW(DOM_LSParser_SecurityCallback, (this)));

		OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE, source_context, target_context, security_callback);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(security_callback);
			security_callback = NULL;
#if defined(CORS_SUPPORT) && defined(DOM_HTTP_SUPPORT)
			if (xmlhttprequest && cross_origin_request && cross_origin_request->IsNetworkError())
				/* Accessing a non-existent URL is better reported as a non-security error. */
				RETURN_IF_ERROR(xmlhttprequest->UpdateCrossOriginStatus(url, FALSE, is_security_error));
#endif // CORS_SUPPORT && DOM_HTTP_SUPPORT
			return status;
		}

		if (!security_callback->IsFinished())
		{
			security_callback->BlockOnSecurityCheck(GetCurrentThread(origining_runtime));
			return OpStatus::ERR;
		}

		allowed = security_callback->IsAllowed();
#if defined(CORS_SUPPORT) && defined(DOM_HTTP_SUPPORT)
		if (xmlhttprequest && cross_origin_request)
			RETURN_IF_ERROR(xmlhttprequest->UpdateCrossOriginStatus(url, allowed, is_security_error));
		else
#endif // CORS_SUPPORT && DOM_HTTP_SUPPORT
#ifdef DOM_HTTP_SUPPORT
			if (xmlhttprequest && !allowed)
				xmlhttprequest->SetNetworkErrorFlag();
#endif // DOM_HTTP_SUPPORT

		if (!allowed)
			return OpStatus::ERR;
	}

	RETURN_OOM_IF_NULL(loader = OP_NEW(DOM_LSLoader, (this)));

	if (!parse_document)
		loader->DisableParsing(TRUE);

	if (parent && !parent->IsA(DOM_TYPE_DOCUMENT))
		loader->SetParsingFragment();

	RETURN_OOM_IF_NULL(contenthandler = OP_NEW(DOM_LSContentHandler, (this, loader, whatToShow, filter)));

	loader->SetContentHandler(contenthandler);
	contenthandler->SetInsertionPoint(parent, before);

	if (!load_handler)
		RETURN_OOM_IF_NULL(load_handler = OP_NEW(DOM_LSParser_LoadHandler, (origining_runtime, this, loader)));
	loader->SetLoadHandler(load_handler);

	OP_STATUS status;

	if (uri || !url.IsEmpty())
	{
		parsedurl = url;

		if (!allowed)
			if (url.Type() == URL_FILE && ref_url.Type() == URL_FILE)
				/* If we refused to load a file from another file,
				   pretend it was there but was empty. */
				status = loader->Construct(NULL, NULL, UNI_L(""), is_fragment, load_handler);
			else
				return OpStatus::ERR;
#ifdef CORS_SUPPORT
		if (OpCrossOrigin_Request *cross_origin_request = xmlhttprequest ? xmlhttprequest->GetCrossOriginRequest() : NULL)
			url = cross_origin_request->GetURL();
#endif // CORS_SUPPORT
		status = loader->Construct(&url, &ref_url, NULL, is_fragment, load_handler);
	}
	else
		status = loader->Construct(NULL, NULL, string, is_fragment, load_handler);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(loader);
		loader = NULL;
		OP_DELETE(load_handler);
		load_handler = NULL;
		is_security_error = FALSE;

		return status;
	}

	RETURN_IF_ERROR(SetKeepAlive());

	busy = TRUE;
	return OpStatus::OK;
}

OP_STATUS
DOM_LSParser::SetKeepAlive()
{
	if (keep_alive_id != 0)
		return OpStatus::OK;

	DOM_Object *window = GetEnvironment()->GetWindow();
	if (window->IsA(DOM_TYPE_WINDOW))
		RETURN_IF_ERROR(static_cast<JS_Window *>(window)->AddKeepAlive(this, &keep_alive_id));
#ifdef DOM_WEBWORKERS_SUPPORT
	else
	{
		OP_ASSERT(window->IsA(DOM_TYPE_WEBWORKERS_WORKER));
		RETURN_IF_ERROR(static_cast<DOM_WebWorker *>(window)->AddKeepAlive(this, &keep_alive_id));
	}
#else
	else
	{
		OP_ASSERT(!"Unexpected window object; cannot happen");
		return OpStatus::ERR;
	}
#endif // DOM_WEBWORKERS_SUPPORT

	OP_ASSERT(keep_alive_id);

	return OpStatus::OK;
}

void
DOM_LSParser::UnsetKeepAlive()
{
	if (keep_alive_id == 0)
		return;

	DOM_Object *window = GetEnvironment()->GetWindow();
	if (window->IsA(DOM_TYPE_WINDOW))
		static_cast<JS_Window *>(window)->RemoveKeepAlive(keep_alive_id);
#ifdef DOM_WEBWORKERS_SUPPORT
	else
	{
		OP_ASSERT(window->IsA(DOM_TYPE_WEBWORKERS_WORKER));
		static_cast<DOM_WebWorker *>(window)->RemoveKeepAlive(keep_alive_id);
	}
#else
	else
	{
		OP_ASSERT(!"Unexpected window object; cannot happen");
	}
#endif // DOM_WEBWORKERS_SUPPORT
	keep_alive_id = 0;
}

OP_STATUS
DOM_LSParser::ResetCallingThread()
{
	OP_NEW_DBG("DOM_LSParser::ResetCallingThread", "DOM3LS");

	if (calling_thread)
	{
		ES_Thread *thread = calling_thread;

		if (calling_thread_listener)
			calling_thread_listener->Remove();

		OP_DELETE(calling_thread_listener);

		calling_thread = NULL;
		calling_thread_listener = NULL;

		return thread->Unblock();
	}
	else
		return OpStatus::OK;
}

OP_STATUS
DOM_LSParser::UnblockCallingThread()
{
	if (calling_thread)
	{
		OP_STATUS status = calling_thread->Unblock();
		calling_thread = NULL;
		return status;
	}
	else
		return OpStatus::OK;
}

/* static */ OP_STATUS
#ifdef DOM_HTTP_SUPPORT
DOM_LSParser::Make(DOM_LSParser *&parser, DOM_EnvironmentImpl *environment, BOOL async, DOM_XMLHttpRequest *xmlhttprequest)
#else
DOM_LSParser::Make(DOM_LSParser *&parser, DOM_EnvironmentImpl *environment, BOOL async)
#endif // DOM_HTTP_SUPPORT
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(parser = OP_NEW(DOM_LSParser, (async)), runtime, runtime->GetPrototype(DOM_Runtime::LSPARSER_PROTOTYPE), "LSParser"));

#ifdef DOM_HTTP_SUPPORT
	if (xmlhttprequest)
	{
		parser->with_dom_config = FALSE;
		parser->xmlhttprequest = xmlhttprequest;
	}
	else
#endif // DOM_HTTP_SUPPORT
		/* Create DOMConfiguration on demand; a good candidate as it is not the lightest and not often used. */
		parser->with_dom_config = TRUE;

#ifdef DOM_WEBWORKERS_SUPPORT
	if (environment->GetWorkerController()->GetWorkerObject())
		parser->parse_document = FALSE;
#endif // DOM_WEBWORKERS_SUPPORT
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_LSParser::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	Reset();

	switch (property_name)
	{
	case OP_ATOM_async:
		DOMSetBoolean(value, async);
		return GET_SUCCESS;

	case OP_ATOM_busy:
		DOMSetBoolean(value, busy);
		return GET_SUCCESS;

	case OP_ATOM_domConfig:
		if (with_dom_config && !config)
			GET_FAILED_IF_ERROR(DOM_DOMConfiguration::Make(config, GetEnvironment()));

		DOMSetObject(value, config);
		return GET_SUCCESS;

	case OP_ATOM_filter:
		DOMSetObject(value, filter);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_LSParser::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	Reset();

	if (GetName(property_name, NULL, origining_runtime) == GET_SUCCESS)
		if (property_name == OP_ATOM_filter)
			if (value->type != VALUE_OBJECT)
				return PutNameDOMException(TYPE_MISMATCH_ERR, value);
			else
			{
				filter = value->value.object;

				ES_Value wts;
				OP_BOOLEAN result;

				PUT_FAILED_IF_ERROR(result = origining_runtime->GetName(filter, UNI_L("whatToShow"), &wts));

				if (result == OpBoolean::IS_TRUE && wts.type == VALUE_NUMBER)
					whatToShow = (int) wts.value.number;
				else
					whatToShow = static_cast<unsigned>(SHOW_ALL);

				return PUT_SUCCESS;
			}
		else
			return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_LSParser::GCTrace()
{
	Reset();

	if (contenthandler)
		contenthandler->GCTrace();

	if (eventtarget)
		eventtarget->GCTrace();

	GCMark(filter);
	GCMark(config);
	GCMark(document);
	GCMark(first_top_node);
	GCMark(context);
	GCMark(parent);
	GCMark(before);

	if (loader)
		loader->GCTrace();

	GCMark(restart_parent);
	GCMark(restart_before);

#ifdef DOM_HTTP_SUPPORT
	GCMark(xmlhttprequest);
#endif // DOM_HTTP_SUPPORT
}

DOM_Document *
DOM_LSParser::GetOwnerDocument()
{
	return parent ? parent->GetOwnerDocument() : document;
}

OP_STATUS
DOM_LSParser::HandleDocumentInfo(const XMLDocumentInformation &docinfo)
{
	DOM_Node *doc;

	if (!document_info_handled)
	{
		document_info_handled = TRUE;

		if ((doc = document) || (doc = context) && context->IsA(DOM_TYPE_DOCUMENT) && action == ACTION_REPLACE_CHILDREN)
		{
			RETURN_IF_ERROR(contenthandler->HandleDocumentInfo(docinfo));

			if (!parsedurl.IsEmpty())
				((DOM_Document *) doc)->SetURL(parsedurl);
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_LSParser::HandleFinished()
{
	UnsetKeepAlive();

	ES_Thread *interrupt_thread = calling_thread;
	RETURN_IF_ERROR(ResetCallingThread());

#ifdef DOM_HTTP_SUPPORT
	if (xmlhttprequest)
	{
		if (parse_document)
			xmlhttprequest->SetResponseXML(document);
		RETURN_IF_ERROR(xmlhttprequest->SetReadyState(DOM_XMLHttpRequest::READYSTATE_LOADED, interrupt_thread));
	}
#endif // DOM_HTTP_SUPPORT

	if (async)
	{
		if (!context && eventtarget)
		{
			if (!input)
			{
				RETURN_IF_ERROR(DOM_LSInput::Make(input, GetEnvironment(), string, uri));
				RETURN_IF_ERROR(PutPrivate(DOM_PRIVATE_input, input));
			}

			DOM_LSLoadEvent *event;
			RETURN_IF_ERROR(DOM_LSLoadEvent::Make(event, this, document, input));

			RETURN_IF_ERROR(GetEnvironment()->SendEvent(event));
		}

#ifdef DOM_HTTP_SUPPORT
		if (!xmlhttprequest || !xmlhttprequest->HasEventHandlerAndUnsentEvents())
			need_reset = TRUE;
#else
		need_reset = TRUE;
#endif // DOM_HTTP_SUPPORT

	}

	if (is_document_load)
	{
		DOM_Runtime *runtime = GetEnvironment()->GetDOMRuntime();
		DOM_Event *event;

		RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));

		event->InitEvent(ONLOAD, context);

		if (async)
			RETURN_IF_ERROR(GetEnvironment()->SendEvent(event));
		else
			RETURN_IF_ERROR(GetEnvironment()->SendEvent(event, interrupt_thread));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_LSParser::HandleProgress(unsigned position, unsigned total)
{
	if (async && !context && eventtarget)
	{
		DOM_LSProgressEvent *event;
		RETURN_IF_ERROR(DOM_LSProgressEvent::Make(event, this, input, position, total));

		OP_BOOLEAN result = GetEnvironment()->SendEvent(event);
		if (OpStatus::IsError(result))
			return result;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_LSParser::SignalError(const uni_char *message, const uni_char *type, unsigned line, unsigned column, unsigned byteOffset, unsigned charOffset)
{
	failed = need_reset = TRUE;

	UnsetKeepAlive();

	ES_Thread *interrupt_thread = calling_thread;
	RETURN_IF_ERROR(ResetCallingThread());

	/* Note: the default error-handler is 'null', hence no need to force creation
	   of the DOMConfiguration object here to look that up. */
	if (config)
	{
		ES_Value value;
		RETURN_IF_ERROR(config->GetParameter(UNI_L("error-handler"), &value));

		if (value.type == VALUE_OBJECT)
		{
			DOM_EnvironmentImpl *environment = GetEnvironment();
			ES_AsyncInterface *asyncif = environment->GetAsyncInterface();

			ES_Object *location;
			RETURN_IF_ERROR(DOM_DOMLocator::Make(location, environment, line, column, byteOffset, charOffset, NULL, uri));

			ES_Object *error;
			RETURN_IF_ERROR(DOM_DOMError::Make(error, environment, DOM_DOMError::SEVERITY_FATAL_ERROR, message, type, NULL, NULL, location));

			ES_Value arguments[1];
			DOMSetObject(&arguments[0], error);

			if (op_strcmp(ES_Runtime::GetClass(value.value.object), "Function") == 0)
				RETURN_IF_ERROR(asyncif->CallFunction(value.value.object, NULL, 1, arguments, NULL, interrupt_thread));
			else
				RETURN_IF_ERROR(asyncif->CallMethod(value.value.object, UNI_L("handleError"), 1, arguments, NULL, interrupt_thread));
		}
	}

	return OpStatus::OK;
}

void
DOM_LSParser::SignalOOM()
{
	oom = need_reset = TRUE;

	UnsetKeepAlive();

	OpStatus::Ignore(ResetCallingThread());
}

void
DOM_LSParser::NewTopNode(DOM_Node *node)
{
	if (!first_top_node)
		first_top_node = node;
}

void
DOM_LSParser::Reset()
{
	if (need_reset)
	{
		UnsetKeepAlive();

		OP_DELETE(loader);
		loader = NULL;

		OP_DELETE(contenthandler);
		contenthandler = NULL;

		busy = FALSE;
		need_reset = FALSE;
		document = NULL;
		first_top_node = context = parent = before = NULL;
		input = NULL;

		OP_DELETEA(uri);
		uri = NULL;

		OP_DELETEA(string);
		string = NULL;

		restart_url = URL();
		OP_DELETEA(restart_systemId);
		restart_systemId = NULL;
		OP_DELETEA(restart_stringData);
		restart_stringData = NULL;
		restart_parent = NULL;
		restart_before = NULL;
	}
}

void
DOM_LSParser::CallingThreadCancelled()
{
	calling_thread = NULL;
	calling_thread_listener = NULL;

	if (contenthandler)
		contenthandler->SetCallingThread(NULL);

	if (loader)
	{
		OP_DELETE(loader);
		loader = NULL;
	}
}

#ifdef DOM_HTTP_SUPPORT
DOM_XMLHttpRequest *
DOM_LSParser::GetXMLHttpRequest()
{
	return xmlhttprequest;
}

#endif // DOM_HTTP_SUPPORT

void
DOM_LSParser::SetParseLoadedData(BOOL enable)
{
	parse_document = enable;
	if (loader)
		loader->DisableParsing(!parse_document);
}

/* static */ void
DOM_LSParser::ConstructLSParserObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "ACTION_APPEND_AS_CHILDREN", ACTION_APPEND_AS_CHILDREN, runtime);
	PutNumericConstantL(object, "ACTION_REPLACE_CHILDREN", ACTION_REPLACE_CHILDREN, runtime);
	PutNumericConstantL(object, "ACTION_INSERT_BEFORE", ACTION_INSERT_BEFORE, runtime);
	PutNumericConstantL(object, "ACTION_INSERT_AFTER", ACTION_INSERT_AFTER, runtime);
	PutNumericConstantL(object, "ACTION_REPLACE", ACTION_REPLACE, runtime);
}

/* static */ void
DOM_LSParser::ConstructLSParserFilterObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "FILTER_ACCEPT", FILTER_ACCEPT, runtime);
	PutNumericConstantL(object, "FILTER_REJECT", FILTER_REJECT, runtime);
	PutNumericConstantL(object, "FILTER_SKIP", FILTER_SKIP, runtime);
	PutNumericConstantL(object, "FILTER_INTERRUPT", FILTER_INTERRUPT, runtime);
}

/* static */ void
DOM_LSParser::ConstructDOMImplementationLSObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "MODE_SYNCHRONOUS", MODE_SYNCHRONOUS, runtime);
	PutNumericConstantL(object, "MODE_ASYNCHRONOUS", MODE_ASYNCHRONOUS, runtime);
}

/* static */ int
DOM_LSParser::abort(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(parser, DOM_TYPE_LSPARSER, DOM_LSParser);

	parser->need_reset = TRUE;
	parser->Reset();

	CALL_FAILED_IF_ERROR(parser->ResetCallingThread());

	return ES_FAILED;
}

void
DOM_LSParser::OnSecurityCheckResult(BOOL allowed)
{
	security_check_passed = allowed;
}

#ifdef DOM_HTTP_SUPPORT
void
DOM_LSParser::SetBaseURL()
{
	if (FramesDocument* frames_doc = GetRuntime()->GetFramesDocument())
		if (frames_doc->GetLogicalDocument() && frames_doc->GetLogicalDocument()->GetBaseURL())
			base_url = *frames_doc->GetLogicalDocument()->GetBaseURL();
		else
			base_url = frames_doc->GetURL();
#ifdef DOM_WEBWORKERS_SUPPORT
	else if (GetEnvironment()->GetWorkerController()->GetWorkerObject())
		base_url = GetEnvironment()->GetWorkerController()->GetWorkerObject()->GetLocationURL();
#endif // DOM_WEBWORKERS_SUPPORT
	else
		base_url = URL();
}
#endif // DOM_HTTP_SUPPORT

/* static */ int
DOM_LSParser::parse(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_LSParser *parser;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(parser, DOM_TYPE_LSPARSER, DOM_LSParser);

		if (parser->busy  ||
		    !parser->GetFramesDocument()
#ifdef DOM_WEBWORKERS_SUPPORT
		    && !parser->GetEnvironment()->GetWorkerController()->GetWorkerObject()
#endif // DOM_WEBWORKERS_SUPPORT
			)
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

		parser->failed = FALSE;
		parser->document_info_handled = FALSE;

		DOM_EnvironmentImpl *environment = parser->GetEnvironment();
		const uni_char *stringData = NULL, *systemId = NULL;

		URL url;

		if (data == 0 || data == 2)
		{
			if (data == 0)
				DOM_CHECK_ARGUMENTS("o");
			else
				DOM_CHECK_ARGUMENTS("oon");

			parser->input = argv[0].value.object;
			CALL_FAILED_IF_ERROR(parser->PutPrivate(DOM_PRIVATE_input, parser->input));

#ifdef DOM_HTTP_SUPPORT
			ES_Object *byteStream;
			CALL_FAILED_IF_ERROR(DOM_LSInput::GetByteStream(byteStream, parser->input, environment));
			if (byteStream)
			{
				if (DOM_Object *object = DOM_HOSTOBJECT(byteStream, DOM_Object))
					if (object->IsA(DOM_TYPE_HTTPINPUTSTREAM))
					{
						/* An XMLHttpRequest URL is resolved in the document it was created, see CORE-2975. */
						if (parser->base_url.IsEmpty())
						{
							parser->SetBaseURL();
							if (parser->base_url.IsEmpty())
								return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
						}
						DOM_HTTPInputStream *input_stream = static_cast<DOM_HTTPInputStream *>(object);
						CALL_FAILED_IF_ERROR(input_stream->GetRequest()->GetURL(url, parser->base_url));
						CALL_FAILED_IF_ERROR(parser->xmlhttprequest->SetCredentials(url));
					}

				if (url.IsEmpty())
					return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
			}
			else
#endif // DOM_HTTP_SUPPORT
			{
				CALL_FAILED_IF_ERROR(DOM_LSInput::GetStringData(stringData, parser->input, environment));
				if (!stringData)
				{
					CALL_FAILED_IF_ERROR(DOM_LSInput::GetSystemId(systemId, parser->input, environment));
					if (!systemId)
						return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
				}
			}
		}
		else
		{
			DOM_CHECK_ARGUMENTS("s");
			systemId = argv[0].value.string;
		}

		DOM_Node *parent = NULL, *before = NULL, *context = NULL;
		BOOL removeContext = FALSE, removeContextChildren = FALSE;

		if (data == 2)
		{
			DOM_ARGUMENT_OBJECT_EXISTING(context, 1, DOM_TYPE_NODE, DOM_Node);

			unsigned actionArg = (unsigned) argv[2].value.number;
			if (actionArg >= ACTION_APPEND_AS_CHILDREN && actionArg <= ACTION_REPLACE)
				parser->action = actionArg;

			parser->is_fragment = TRUE;

			switch (parser->action)
			{
			case ACTION_APPEND_AS_CHILDREN:
				parent = context;
				break;

			case ACTION_REPLACE_CHILDREN:
				parent = context;
				removeContextChildren = TRUE;
				if (context->IsA(DOM_TYPE_DOCUMENT))
					parser->is_fragment = FALSE;
				break;

			case ACTION_INSERT_BEFORE:
				CALL_FAILED_IF_ERROR(context->GetParentNode(parent));
				before = context;
				break;

			case ACTION_REPLACE:
				removeContext = TRUE;

			case ACTION_INSERT_AFTER:
				CALL_FAILED_IF_ERROR(context->GetParentNode(parent));
				CALL_FAILED_IF_ERROR(context->GetNextSibling(before));
			}

			if (!parent->IsA(DOM_TYPE_ELEMENT) && !parent->IsA(DOM_TYPE_DOCUMENTFRAGMENT))
				if (parser->action != ACTION_REPLACE_CHILDREN || !parent->IsA(DOM_TYPE_DOCUMENT))
#ifdef DOM_SUPPORT_ENTITY
					if (!parent->IsA(DOM_TYPE_ENTITY) || !((DOM_Entity *) parent)->IsBeingParsed())
#endif // DOM_SUPPORT_ENTITY
						return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		}
		else if (parser->parse_document)
		{
			CALL_FAILED_IF_ERROR(environment->ConstructDocumentNode());
			CALL_FAILED_IF_ERROR(DOM_XMLDocument::Make(parser->document, ((DOM_Document *) environment->GetDocument())->GetDOMImplementation(), TRUE));
			parent = parser->document;
			context = NULL;
		}

		parser->context = context;
		parser->parent = parent;
		parser->before = before;

#ifdef DOM2_MUTATION_EVENTS
		if (removeContext || removeContextChildren)
		{
			ES_Thread *thread = OP_NEW(DOM_LSParser_RemoveThread, (context, removeContextChildren));
			if (!thread)
				return ES_NO_MEMORY;

			ES_Thread *interrupt_thread = GetCurrentThread(origining_runtime);
			CALL_FAILED_IF_ERROR(interrupt_thread->GetScheduler()->AddRunnable(thread, interrupt_thread));
		}
#else // DOM2_MUTATION_EVENTS
		if (removeContext)
			CALL_FAILED_IF_ERROR(context->RemoveFromParent(origining_runtime));
		else if (removeContextChildren)
			while (1)
			{
				DOM_Node *child;
				CALL_FAILED_IF_ERROR(context->GetFirstChild(child));

				if (!child)
					break;

				CALL_FAILED_IF_ERROR(child->RemoveFromParent(origining_runtime));
			}
#endif // DOM2_MUTATION_EVENTS

		BOOL is_security_error;
		OP_STATUS status = parser->Start(url, systemId, stringData, parent, before, origining_runtime, is_security_error);

		if (OpStatus::IsError(status))
		{
			if (OpStatus::IsMemoryError(status))
				return ES_NO_MEMORY;
			else if (!is_security_error)
				return ES_FAILED;
			else
			{
				if (parser->security_callback && !parser->security_callback->IsFinished())
				{
					/* The security callback has blocked the calling thread. */
					parser->calling_thread = NULL;
					parser->restart_url = url;
					CALL_FAILED_IF_ERROR(UniSetStr(parser->restart_systemId, systemId));
					CALL_FAILED_IF_ERROR(UniSetStr(parser->restart_stringData, stringData));
					parser->restart_parent = parent;
					parser->restart_before = before;

					DOMSetObject(return_value, parser);
					return parser->xmlhttprequest ? ES_SUSPEND | ES_ABORT : ES_SUSPEND | ES_RESTART;
				}

				return ES_EXCEPT_SECURITY;
			}
		}
		else if (parser->security_callback && parser->security_callback->IsFinished())
		{
			parser->has_started_parsing = TRUE;
			parser->security_check_passed = parser->security_callback->IsAllowed();
			parser->calling_thread = NULL;
			parser->restart_url = url;
			CALL_FAILED_IF_ERROR(UniSetStr(parser->restart_systemId, systemId));
			CALL_FAILED_IF_ERROR(UniSetStr(parser->restart_stringData, stringData));
			parser->restart_parent = parent;
			parser->restart_before = before;
		}
		else if (!parser->security_callback)
		{
			parser->has_started_parsing = TRUE;
			parser->security_check_passed = TRUE;
		}
		else
			OP_ASSERT(!"Security check not completed, but security check reported success? Cannot happen.");

		goto start_parsing;
	}
	else
	{
		parser = DOM_VALUE2OBJECT(*return_value, DOM_LSParser);
		int result;

		if (!parser->has_started_parsing)
		{
			OP_DELETE(parser->security_callback);
			parser->has_started_parsing = TRUE;
			parser->security_callback = NULL;
			BOOL is_security_error;
			OP_STATUS status = parser->Start(parser->restart_url, parser->restart_systemId, parser->restart_stringData, parser->restart_parent, parser->restart_before, origining_runtime, is_security_error);

			parser->restart_url = URL();
			OP_DELETEA(parser->restart_systemId);
			parser->restart_systemId = NULL;
			OP_DELETEA(parser->restart_stringData);
			parser->restart_stringData = NULL;
			parser->restart_parent = NULL;
			parser->restart_before = NULL;

			if (OpStatus::IsError(status))
			{
				if (OpStatus::IsMemoryError(status))
					return ES_NO_MEMORY;
				else if (!is_security_error)
					return ES_FAILED;
				else
					return ES_EXCEPT_SECURITY;
			}

			goto start_parsing;
		}

		if (parser->oom)
			result = ES_NO_MEMORY;
		if (!parser->security_check_passed)
			result = ES_EXCEPT_SECURITY;
		else if (parser->failed)
			result = DOM_LSException::CallException(parser, DOM_LSException::PARSE_ERR, return_value);
#ifdef DOM_HTTP_SUPPORT
		/* Caller will appropriately handle the timeout error / exception.
		   (The XHR2 spec has the "request error" step as throwing for all
		   sync errors, but this is incompatible with what others implement.) */
		else if (parser->xmlhttprequest && parser->xmlhttprequest->HasTimeoutError())
			result = ES_EXCEPTION;
#endif // DOM_HTTP_SUPPORT
		else
		{
			if (parser->document)
				DOMSetObject(return_value, parser->document);
			else
				DOMSetObject(return_value, parser->first_top_node);

			result = ES_VALUE;
		}

		parser->need_reset = TRUE;
		parser->Reset();

		return result;
	}

start_parsing:
	if (parser->async && (data != 2 || parser->is_document_load))
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	else
	{
		ES_Thread *calling_thread = GetCurrentThread(origining_runtime);

		if (!(parser->calling_thread_listener = OP_NEW(DOM_LSParser_ThreadListener, (parser, calling_thread))))
			return ES_NO_MEMORY;

		parser->calling_thread = calling_thread;
		if (parser->contenthandler)
			parser->contenthandler->SetCallingThread(calling_thread);

		DOMSetObject(return_value, parser);

		calling_thread->Block();
		return ES_SUSPEND | ES_RESTART;
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_LSParser)
	DOM_FUNCTIONS_FUNCTION(DOM_LSParser, DOM_LSParser::abort, "abort", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_LSParser, DOM_Node::dispatchEvent, "dispatchEvent", 0)
DOM_FUNCTIONS_END(DOM_LSParser)

DOM_FUNCTIONS_WITH_DATA_START(DOM_LSParser)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSParser, DOM_LSParser::parse, 0, "parse", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSParser, DOM_LSParser::parse, 1, "parseURI", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSParser, DOM_LSParser::parse, 2, "parseWithContext", "--n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSParser, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSParser, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
#ifdef DOM3_EVENTS
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSParser, DOM_Node::accessEventListener, 2, "addEventListenerNS", "ss-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_LSParser, DOM_Node::accessEventListener, 3, "removeEventListenerNS", "ss-b-")
#endif // DOM3_EVENTS
DOM_FUNCTIONS_WITH_DATA_END(DOM_LSParser)

#endif // DOM3_LOAD
