/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef XML_EVENTS_SUPPORT

#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/doc/domevent.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domevents.h"
#include "modules/dom/domutils.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/optreecallback.h"
#include "modules/logdoc/xmlevents.h"
#include "modules/util/str.h"
#include "modules/util/opautoptr.h"
#include "modules/url/url_api.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlserializer.h"
#include "modules/xmlutils/xmltokenhandler.h"

class XML_Events_Activator
{
protected:
	XML_Events_Registration *registration;
	HTML_Element *handler;

public:
	XML_Events_Activator(XML_Events_Registration *r, HTML_Element *h);
	virtual ~XML_Events_Activator();

	HTML_Element *GetHandler() { return handler; }

	/* Override these functions. */
	virtual OP_STATUS Construct(FramesDocument *frames_doc, URL url) = 0;
	virtual OP_STATUS Activate(DOM_EventsAPI::Event *event) = 0;

	/* Don't care about this one. */
	OP_STATUS ActivateWrapper(DOM_EventsAPI::Event *event);

	static OP_STATUS Create(XML_Events_Registration *registration, HTML_Element *handler, XML_Events_Activator *&activator);
};

class XML_Events_ScriptActivator
	: public XML_Events_Activator
{
protected:
	ES_Program *program;
	unsigned int text_idx;

public:
	XML_Events_ScriptActivator(XML_Events_Registration *r, HTML_Element *h);
	~XML_Events_ScriptActivator();

	OP_STATUS Construct(FramesDocument *frames_doc, URL url);
	OP_STATUS Activate(DOM_EventsAPI::Event *event);
};

XML_Events_Activator::XML_Events_Activator(XML_Events_Registration *r, HTML_Element *h)
	: registration(r),
	  handler(h)
{
}

XML_Events_Activator::~XML_Events_Activator()
{
}

OP_STATUS
XML_Events_Activator::ActivateWrapper(DOM_EventsAPI::Event *event)
{
	OP_STATUS status = Activate(event);

	if (registration->GetStopPropagation())
		event->StopPropagation();

	if (registration->GetPreventDefault())
		event->PreventDefault();

	return status;
}

OP_STATUS
XML_Events_Activator::Create(XML_Events_Registration *registration, HTML_Element *handler, XML_Events_Activator *&activator)
{
	if (handler->IsMatchingType(HE_SCRIPT, NS_HTML)
#ifdef SVG_SUPPORT
		|| handler->IsMatchingType(Markup::SVGE_HANDLER, NS_SVG)
#endif // SVG_SUPPORT
		)
		activator = OP_NEW(XML_Events_ScriptActivator, (registration, handler));
	else
	{
		activator = NULL;
		return OpStatus::OK;
	}

	return activator ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

XML_Events_ScriptActivator::XML_Events_ScriptActivator(XML_Events_Registration *r, HTML_Element *h)
	: XML_Events_Activator(r, h),
	  program(NULL),
	  text_idx(~0u)
{
}

XML_Events_ScriptActivator::~XML_Events_ScriptActivator()
{
	if (program)
		ES_Runtime::DeleteProgram(program);
}

OP_STATUS
XML_Events_ScriptActivator::Construct(FramesDocument *frames_doc, URL url)
{
	return OpStatus::OK;
}

OP_STATUS
XML_Events_ScriptActivator::Activate(DOM_EventsAPI::Event *event)
{
	ES_ThreadScheduler *scheduler = event->GetThread()->GetScheduler();

	unsigned int idx = (unsigned int)handler->GetSpecialNumAttr(ATTR_JS_TEXT_IDX, SpecialNs::NS_LOGDOC, ~0);

	if (program && (text_idx == ~0u || text_idx != idx))
	{
		ES_Runtime::DeleteProgram(program);
		program = NULL;
	}

	if (!program)
	{
		ES_Runtime *runtime = scheduler->GetRuntime();
		ES_ProgramText *program_text;
		int program_text_len;

		RETURN_IF_ERROR(handler->ConstructESProgramText(program_text,
			program_text_len, runtime->GetFramesDocument()->GetURL(),
			runtime->GetFramesDocument()->GetLogicalDocument()));

		OP_ASSERT(!!program_text == !!program_text_len); // Must be 0/NULL at the same time
		if (!program_text) // No script there
			return OpStatus::OK;

#ifdef SVG_SUPPORT
		// Hack to make the evt variable available in SVG event handlers.
		const uni_char* evt_event_alias = UNI_L("var evt=event;");
		if (handler->IsMatchingType(Markup::SVGE_HANDLER, NS_SVG))
		{
			ES_ProgramText* newprogram = OP_NEWA(ES_ProgramText, program_text_len+1);
			if(!newprogram)
			{
				OP_DELETEA(program_text);
				return OpStatus::ERR_NO_MEMORY;
			}

			newprogram[0].program_text = evt_event_alias;
			newprogram[0].program_text_length = uni_strlen(evt_event_alias);
			for(int i = 0; i < program_text_len; i++)
			{
				newprogram[i+1].program_text = program_text[i].program_text;
				newprogram[i+1].program_text_length = program_text[i].program_text_length;
			}
			OP_DELETEA(program_text);
			program_text_len++;
			program_text = newprogram;
		}
#endif // SVG_SUPPORT

		ES_Runtime::CompileProgramOptions options;
		options.global_scope = FALSE;
		options.script_type = SCRIPT_TYPE_EVENT_HANDLER;
		options.when = UNI_L("while handling event");

		OP_STATUS status = runtime->CompileProgram(program_text, program_text_len, &program, options);

		OP_DELETEA(program_text);
		RETURN_IF_ERROR(status);

		text_idx = handler->GetSpecialNumAttr(ATTR_JS_TEXT_IDX, SpecialNs::NS_LOGDOC, ~0);
	}

	if (program)
	{
		ES_Context *context = scheduler->GetRuntime()->CreateContext(NULL);
		if (!context)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = scheduler->GetRuntime()->PushProgram(context, program);
		if (OpStatus::IsError(status))
		{
			ES_Runtime::DeleteContext(context);
			return status;
		}

		ES_Thread *thread = OP_NEW(ES_Thread, (context));
		if (!thread)
		{
			ES_Runtime::DeleteContext(context);
			return OpStatus::ERR_NO_MEMORY;
		}

		return scheduler->AddRunnable(thread, scheduler->GetCurrentThread());
	}

	return OpStatus::OK;
}

class XML_Events_ExternalHandlerThread
	: public ES_Thread
{
public:
	XML_Events_ExternalHandlerThread(DOM_EventsAPI::Event *event, XML_Events_Registration *registration);

	virtual OP_STATUS EvaluateThread();

	void SetActivator(XML_Events_Activator *activator);

protected:
	DOM_EventsAPI::Event *event;
	XML_Events_Registration *registration;
};

XML_Events_ExternalHandlerThread::XML_Events_ExternalHandlerThread(DOM_EventsAPI::Event *event, XML_Events_Registration *registration)
	: ES_Thread(NULL),
	  event(event),
	  registration(registration)
{
}

OP_STATUS
XML_Events_ExternalHandlerThread::EvaluateThread()
{
	if (!is_started)
	{
		is_started = TRUE;

		if (XML_Events_Activator *activator = registration->GetActivator())
			return activator->ActivateWrapper(event);
	}

	is_completed = TRUE;
	return OpStatus::OK;
}

class XML_Events_ExternalElementCallback
	: public OpTreeCallback,
	  public XMLParser::Listener
{
public:
	XML_Events_ExternalElementCallback(XML_Events_Registration *registration, FramesDocument *frames_doc, URL url);
	~XML_Events_ExternalElementCallback();

	void SetParser(XMLParser *parser, XMLTokenHandler *tokenhandler);

	OP_STATUS AddThread(XML_Events_ExternalHandlerThread *thread);
	BOOL IsLoading();

	/* From OpTreeCallback. */
	virtual OP_STATUS ElementFound(HTML_Element *element);
	virtual OP_STATUS ElementNotFound();

	/* From XMLParser::Listener. */
	virtual void Continue(XMLParser *parser);
	virtual void Stopped(XMLParser *parser);

protected:
	class Waiting
		: public ES_ThreadListener
	{
	public:
		XML_Events_ExternalHandlerThread *thread;
		Waiting *next;

		/* From ES_ThreadListener. */
		virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal);
	};

	XML_Events_Registration *registration;
	FramesDocument *frames_doc;
	URL url;
	XMLParser *parser;
	XMLTokenHandler *tokenhandler;
	Waiting *first_waiting;
	HTML_Element *handler;
	BOOL is_loading;
};

XML_Events_ExternalElementCallback::XML_Events_ExternalElementCallback(XML_Events_Registration *registration, FramesDocument *frames_doc, URL url)
	: registration(registration),
	  frames_doc(frames_doc),
	  url(url),
	  parser(NULL),
	  tokenhandler(NULL),
	  first_waiting(NULL),
	  handler(NULL),
	  is_loading(TRUE)
{
}

XML_Events_ExternalElementCallback::~XML_Events_ExternalElementCallback()
{
	if (handler)
		if (handler->Clean(frames_doc))
			handler->Free(frames_doc);

	Waiting *waiting = first_waiting, *next_waiting;
	while (waiting)
	{
		next_waiting = waiting->next;

		if (waiting->thread)
			waiting->Remove();

		OP_DELETE(waiting);

		waiting = next_waiting;
	}

	OP_DELETE(parser);
	OP_DELETE(tokenhandler);
}

void
XML_Events_ExternalElementCallback::SetParser(XMLParser *new_parser, XMLTokenHandler *new_tokenhandler)
{
	parser = new_parser;
	tokenhandler = new_tokenhandler;
}

OP_STATUS
XML_Events_ExternalElementCallback::AddThread(XML_Events_ExternalHandlerThread *new_thread)
{
	Waiting *waiting = OP_NEW(Waiting, ());
	if (!waiting)
		return OpStatus::ERR_NO_MEMORY;

	waiting->thread = new_thread;
	waiting->next = first_waiting;
	first_waiting = waiting;

	new_thread->AddListener(waiting);
	return OpStatus::OK;
}

BOOL
XML_Events_ExternalElementCallback::IsLoading()
{
	return is_loading;
}

/* virtual */ OP_STATUS
XML_Events_ExternalElementCallback::ElementFound(HTML_Element *element)
{
	XML_Events_Activator *activator = NULL;

	if (element->IsScriptSupported(frames_doc) &&
		OpStatus::IsSuccess(XML_Events_Activator::Create(registration, element, activator)) &&
	    (!activator || OpStatus::IsSuccess(activator->Construct(frames_doc, url))))
		registration->SetActivator(activator);
	else
	{
		OP_DELETE(activator);
		activator = NULL;
	}

	Waiting *waiting = first_waiting;
	while (waiting)
	{
		if (waiting->thread)
			OpStatus::Ignore(waiting->thread->Unblock());

		waiting->Remove();
		waiting->thread = NULL;

		waiting = waiting->next;
	}

	handler = element;
	is_loading = FALSE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XML_Events_ExternalElementCallback::ElementNotFound()
{
	Waiting *waiting = first_waiting;
	while (waiting)
	{
		if (waiting->thread)
			OpStatus::Ignore(waiting->thread->Unblock());

		waiting->Remove();
		waiting->thread = NULL;

		waiting = waiting->next;
	}

	is_loading = FALSE;
	return OpStatus::OK;
}

/* virtual */ void
XML_Events_ExternalElementCallback::Continue(XMLParser *parser)
{
	/* Should never be called. */
	OP_ASSERT(FALSE);
}

/* virtual */ void
XML_Events_ExternalElementCallback::Stopped(XMLParser *parser)
{
	/* Fail silently, same as if the element was not found. */
	if (is_loading && parser->IsFailed())
		OpStatus::Ignore(ElementNotFound());
}

OP_STATUS
XML_Events_ExternalElementCallback::Waiting::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_CANCELLED:
		Remove();
		thread = NULL;
	}

	return OpStatus::OK;
}

class XML_Events_EventHandler
	: public DOM_EventsAPI::EventHandler
{
protected:
	XML_Events_Registration *registration;
	DOM_EventsAPI::EventTarget *target;

	DOM_EventType type;
	const uni_char *type_string;
	BOOL capture;

public:
	XML_Events_EventHandler(XML_Events_Registration *registration);
	virtual ~XML_Events_EventHandler();

	DOM_EventType GetType() { return type; }

	virtual BOOL HandlesEvent(DOM_EventType type, const uni_char *type_string, ES_EventPhase phase);

	virtual OP_STATUS HandleEvent(DOM_EventsAPI::Event *event);

	virtual void RegisterHandlers(DOM_Environment *environment);
	virtual void UnregisterHandlers(DOM_Environment *environment);

	OP_STATUS AddToTarget(DOM_EventsAPI::EventTarget *target);
	void RemoveFromTarget();
};

XML_Events_EventHandler::XML_Events_EventHandler(XML_Events_Registration *registration)
	: registration(registration),
	  target(NULL),
	  type_string(registration->GetEventType()),
	  capture(registration->GetCapture())
{
	type = DOM_EventsAPI::GetEventType(type_string);

	if (type == DOM_EVENT_NONE)
		type = DOM_EVENT_CUSTOM;
}

/* virtual */
XML_Events_EventHandler::~XML_Events_EventHandler()
{
	if (target)
		target->RemoveEventHandler(this);

	registration->ResetEventHandler();
}

/* virtual */ BOOL
XML_Events_EventHandler::HandlesEvent(DOM_EventType check_type, const uni_char *check_type_string, ES_EventPhase phase)
{
	return type == check_type && (check_type != DOM_EVENT_CUSTOM || uni_str_eq(type_string, check_type_string)) &&
	       (phase == ES_PHASE_ANY || capture && phase == ES_PHASE_CAPTURING || !capture && phase != ES_PHASE_CAPTURING);
}

/* virtual */ OP_STATUS
XML_Events_EventHandler::HandleEvent(DOM_EventsAPI::Event *event)
{
	ES_ThreadScheduler *scheduler = event->GetThread()->GetScheduler();
	FramesDocument *frames_doc = scheduler->GetFramesDocument();

	if (registration->GetTargetId())
	{
		HTML_Element *element = DOM_Utils::GetHTML_Element(event->GetTarget());
		BOOL is_xml = frames_doc->GetHLDocProfile()->IsXml();

		if (!element || !element->GetId() || is_xml && !uni_stri_eq(element->GetId(), registration->GetTargetId()) ||
		    !is_xml && !uni_stri_eq(element->GetId(), registration->GetTargetId()))
			return OpStatus::OK;
	}

	HTML_Element *handler = NULL;
	XML_Events_Activator *activator = registration->GetActivator();

	if (registration->GetHandlerURI())
	{
		if (registration->GetHandlerIsExternal(frames_doc->GetURL()))
		{
			if (activator)
				return activator->ActivateWrapper(event);
			else
			{
				XML_Events_ExternalElementCallback *callback = (XML_Events_ExternalElementCallback *) registration->GetExternalElementCallback();

				if (!callback || !callback->IsLoading())
					return OpStatus::OK;

				XML_Events_ExternalHandlerThread *thread = OP_NEW(XML_Events_ExternalHandlerThread, (event, registration));

				if (!thread)
					return OpStatus::ERR_NO_MEMORY;

				OP_BOOLEAN result = scheduler->AddRunnable(thread, event->GetThread());

				if (result == OpBoolean::IS_TRUE)
				{
					RETURN_IF_ERROR(callback->AddThread(thread));
					thread->Block();
				}

				return OpStatus::IsMemoryError(result) ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
			}
		}
		else
		{
			const uni_char *handler_id = uni_strchr(registration->GetHandlerURI(), '#');
			if (handler_id && uni_strlen(handler_id) > 1)
				handler = frames_doc->GetLogicalDocument()->GetNamedHE(handler_id + 1);
		}
	}
	else
		handler = registration->GetOwner();

	if (!handler)
		return OpStatus::OK;

	if (!activator || activator->GetHandler() != handler)
	{
		registration->SetActivator(NULL);

		if (handler && handler->IsScriptSupported(frames_doc))
		{
			RETURN_IF_ERROR(XML_Events_Activator::Create(registration, handler, activator));
			registration->SetActivator(activator);
		}
	}

	if (activator)
		return activator->ActivateWrapper(event);

	return OpStatus::OK;
}

/* virtual */ void
XML_Events_EventHandler::RegisterHandlers(DOM_Environment *environment)
{
	if (type != DOM_EVENT_CUSTOM)
		environment->AddEventHandler(type);
}

/* virtual */ void
XML_Events_EventHandler::UnregisterHandlers(DOM_Environment *environment)
{
	if (type != DOM_EVENT_CUSTOM)
		environment->RemoveEventHandler(type);
}

OP_STATUS
XML_Events_EventHandler::AddToTarget(DOM_EventsAPI::EventTarget *new_target)
{
	target = new_target;
	return target->AddEventHandler(this);
}

void
XML_Events_EventHandler::RemoveFromTarget()
{
	target->RemoveEventHandler(this);
	target = NULL;
}

XML_Events_Registration::XML_Events_Registration(HTML_Element *owner)
	: owner(owner),
	  observer(NULL),
	  event_type(NULL),
	  observer_id(NULL),
	  target_id(NULL),
	  handler_uri(NULL),
	  prevent_default(FALSE),
	  stop_propagation(FALSE),
	  capture(FALSE),
	  handler_prepared(FALSE),
	  disabled(TRUE),
	  event_handler(NULL),
	  activator(NULL),
	  external_element_callback(NULL)
{
	HTML_Element *parent = owner->Parent();
	while (parent)
	{
		if (parent->Type() == HE_DOC_ROOT)
			disabled = FALSE;
		parent = parent->Parent();
	}
}

XML_Events_Registration::~XML_Events_Registration()
{
	if (InList())
		Out();

	OP_DELETEA(event_type);
	OP_DELETEA(observer_id);
	OP_DELETEA(target_id);
	OP_DELETEA(handler_uri);
	OP_DELETE(event_handler);
	OP_DELETE(activator);
	OP_DELETE(external_element_callback);
}

OP_STATUS
XML_Events_Registration::SetEventType(FramesDocument *frames_doc, const uni_char *type, int length)
{
	RETURN_IF_ERROR(UniSetStrN(event_type, type, length));

	if (event_handler)
	{
		OP_DELETE(event_handler);
		event_handler = NULL;
		return Update(frames_doc);
	}
	else
		return OpStatus::OK;
}

OP_STATUS
XML_Events_Registration::SetObserverId(FramesDocument *frames_doc, const uni_char *id, int length)
{
	if (id)
		RETURN_IF_ERROR(UniSetStrN(observer_id, id, length));
	else
		observer_id = NULL;

	observer = NULL;
	return Update(frames_doc);
}

OP_STATUS
XML_Events_Registration::SetTargetId(FramesDocument *frames_doc, const uni_char *id, int length)
{
	return UniSetStrN(target_id, id, length);
}

OP_STATUS
XML_Events_Registration::SetHandlerURI(FramesDocument *frames_doc, const uni_char *uri, int length)
{
	BOOL need_update = FALSE;

	if (uri == NULL || handler_uri == NULL)
	{
		observer = NULL;
		need_update = TRUE;
	}

	if (uri == NULL)
	{
		OP_DELETEA(handler_uri);
		handler_uri = NULL;
	}
	else
		RETURN_IF_ERROR(UniSetStrN(handler_uri, uri, length));

	handler_prepared = FALSE;

	if (handler_uri && !disabled)
		PrepareHandler(frames_doc);

	if (need_update)
		return Update(frames_doc);
	else
		return OpStatus::OK;
}

void
XML_Events_Registration::SetActivator(XML_Events_Activator *new_activator)
{
	if (activator != new_activator)
		OP_DELETE(activator);

	activator = new_activator;
}

OP_STATUS
XML_Events_Registration::HandleElementInsertedIntoDocument(FramesDocument *frames_doc, HTML_Element *inserted)
{
	BOOL need_update = FALSE;

	if (inserted->IsAncestorOf(owner))
	{
		RETURN_IF_ERROR(PrepareHandler(frames_doc));

		disabled = FALSE;
		need_update = TRUE;
	}

	if (observer_id)
		if (HTML_Element *new_observer = inserted->GetElmById(observer_id))
			if (!observer || new_observer->Precedes(observer))
			{
				observer = new_observer;
				need_update = TRUE;
			}

	if (need_update)
		return Update(frames_doc);
	else
		return OpStatus::OK;
}

OP_STATUS
XML_Events_Registration::HandleElementRemovedFromDocument(FramesDocument *frames_doc, HTML_Element *removed)
{
	BOOL need_update = FALSE;

	if (removed->IsAncestorOf(owner))
	{
		disabled = TRUE;
		need_update = TRUE;
	}
	else if (observer && removed->IsAncestorOf(observer))
	{
		observer = NULL;
		need_update = TRUE;
	}

	if (need_update)
		return Update(frames_doc);
	else
		return OpStatus::OK;
}

OP_STATUS
XML_Events_Registration::HandleIdChanged(FramesDocument *frames_doc, HTML_Element *changed)
{
	BOOL need_update = FALSE;

	if (observer_id)
		if (observer == changed)
		{
			observer = NULL;
			need_update = TRUE;
		}
		else
		{
			const uni_char *id = changed->GetId();
			if (id)
				if (uni_strcmp(id, observer_id) == 0)
					if (!observer || changed->Precedes(observer))
					{
						observer = changed;
						need_update = TRUE;
					}
		}

	if (need_update)
		return Update(frames_doc);
	else
		return OpStatus::OK;
}

OP_STATUS
XML_Events_Registration::Update(FramesDocument *frames_doc)
{
	if (disabled)
		observer = NULL;
	else if (observer_id)
	{
		if (!observer)
			if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
				if (HTML_Element *root = logdoc->GetRoot())
					observer = root->GetElmById(observer_id);
	}
	else if (handler_uri)
		observer = owner;
	else
		observer = owner->ParentActual();

	OP_DELETE(event_handler);
	event_handler = NULL;

	if (event_type && observer)
	{
		if (!(event_handler = OP_NEW(XML_Events_EventHandler, (this))))
			return OpStatus::ERR_NO_MEMORY;

		HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();

		BOOL is_window_event = DOM_EventsAPI::IsWindowEvent(event_handler->GetType());

		if (hld_profile->GetDocRoot()->IsAncestorOf(observer))
			if (!hld_profile->IsXml() || hld_profile->GetDocRoot()->IsMatchingType(HE_HTML, NS_HTML))
			{
				if (!(observer->IsMatchingType(HE_BODY, NS_HTML) || observer->IsMatchingType(HE_FRAMESET, NS_HTML) && observer->Parent() && observer->Parent()->IsMatchingType(HE_HTML, NS_HTML)))
					is_window_event = FALSE;
			}
			else if (!(observer == hld_profile->GetDocRoot()))
				is_window_event = FALSE;

		DOM_EventsAPI::EventTarget *event_target;

		if (is_window_event)
			RETURN_IF_ERROR(DOM_EventsAPI::GetEventTarget(event_target, frames_doc->GetDOMEnvironment()->GetWindow()));
		else
		{
			DOM_Object *observer_node;
			RETURN_IF_ERROR(frames_doc->GetDOMEnvironment()->ConstructNode(observer_node, observer));
			RETURN_IF_ERROR(DOM_EventsAPI::GetEventTarget(event_target, observer_node));
		}

		return event_handler->AddToTarget(event_target);
	}

	return OpStatus::OK;
}

void
XML_Events_Registration::ResetEventHandler()
{
	event_handler = NULL;
}

BOOL
XML_Events_Registration::GetHandlerIsExternal(URL &document_url)
{
	if (handler_uri)
	{
		const uni_char *handler_id = uni_strchr(handler_uri, '#');

		if (handler_id && handler_uri != handler_id)
		{
			URL url = g_url_api->GetURL(document_url, handler_uri);
			return url.GetRep() != document_url.GetRep();
		}
	}

	return FALSE;
}

OP_STATUS
XML_Events_Registration::PrepareHandler(FramesDocument *frm_doc)
{
	if (!handler_prepared)
	{
		handler_prepared = TRUE;

		if (external_element_callback)
		{
			OP_DELETE(external_element_callback);
			external_element_callback = NULL;
		}

		if (GetHandlerIsExternal(frm_doc->GetURL()))
		{
			URL baseurl = frm_doc->GetURL();
			URL url = g_url_api->GetURL(baseurl, handler_uri);

			external_element_callback = OP_NEW(XML_Events_ExternalElementCallback, (this, frm_doc, url));

			if (!external_element_callback)
				return OpStatus::ERR_NO_MEMORY;

			XMLTokenHandler *tokenhandler;

			RETURN_IF_ERROR(OpTreeCallback::MakeTokenHandler(tokenhandler, frm_doc->GetLogicalDocument(), external_element_callback));

			XMLParser *parser;

			if (OpStatus::IsMemoryError(XMLParser::Make(parser, external_element_callback, frm_doc, tokenhandler, url)))
			{
				OP_DELETE(tokenhandler);
				return OpStatus::ERR_NO_MEMORY;
			}

			external_element_callback->SetParser(parser, tokenhandler);

			RETURN_IF_ERROR(parser->Load(baseurl));
		}
	}

	return OpStatus::OK;
}

void
XML_Events_Registration::MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document)
{
	if (external_element_callback)
	{
		OP_DELETE(external_element_callback);
		external_element_callback = NULL;
	}

	Out();

	if (new_document)
	{
		new_document->SetHasXMLEvents();
		new_document->AddXMLEventsRegistration(this);
	}
}

#endif // XML_EVENTS_SUPPORT
