/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/dom/src/userjs/userjsevent.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/util/str.h"

#include "modules/logdoc/htm_elm.h"

#include "modules/forms/formmanager.h"

/* Defined in domeventthread.cpp. */
extern BOOL IsDOMNode(DOM_Object *obj);

OP_STATUS
DOM_EventListener::CompileHandler(DOM_Object *target, ES_Thread *interrupt_thread)
{
	DOM_Node *node = IsDOMNode(target) ? static_cast<DOM_Node *>(target) : NULL;
	HTML_Element *element = node ? node->GetThisElement() : NULL;
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	ES_Object *scope_chain[3];
	int scope_chain_length = 0;

	if (node)
		if (DOM_Document *document = node->GetOwnerDocument())
			scope_chain[scope_chain_length++] = *document;

	if (element)
	{
		HTML_Element *form_element = FormManager::IsFormElement(element) ? FormManager::FindFormElm(environment->GetFramesDocument(), element) : NULL;
		if (!form_element)
		{
			form_element = element->ParentActual();

			while (form_element && !form_element->IsMatchingType(HE_FORM, NS_HTML))
				form_element = form_element->ParentActual();
		}

		if (form_element)
		{
			DOM_Node *form_node;
			RETURN_IF_ERROR(environment->ConstructNode(form_node, form_element, node->GetOwnerDocument()));
			scope_chain[scope_chain_length++] = *form_node;
		}
	}

	if (node)
		scope_chain[scope_chain_length++] = *node;

	const uni_char *argument_names[3]; /* ARRAY OK 2011-09-16 jl */
	unsigned argc = 0;
	argument_names[argc++] = UNI_L("event");

	if (data.native.known_type == ONERROR && target->IsA(DOM_TYPE_WINDOW))
	{
		argument_names[argc++] = UNI_L("source");
		argument_names[argc++] = UNI_L("lineno");
	}

#ifdef SVG_DOM
	// In svg the Event object is called 'evt', according to svg11/REC-SVG11-20030114/svgdom.html#EventListeners
	if (element && element->GetNsType() == NS_SVG)
		argument_names[0] = UNI_L("evt");
#endif // SVG_DOM

	ES_Runtime *runtime = environment->GetRuntime();
	URL script_url = target->GetRuntime()->GetOriginURL();

	ES_Runtime::CreateFunctionOptions options;
	options.script_type = SCRIPT_TYPE_EVENT_HANDLER;
	options.script_url = &script_url;
	options.when = UNI_L("in event handler");
#ifdef ECMASCRIPT_DEBUGGER
	options.reformat_source = g_ecmaManager->GetWantReformatScript(runtime);
#endif // ECMASCRIPT_DEBUGGER

	if (interrupt_thread)
		runtime->GetESScheduler()->SetErrorHandlerInterruptThread(interrupt_thread);

	OP_STATUS status = runtime->CreateFunction(scope_chain, scope_chain_length,
	                                           data.native.handler_text, uni_strlen(data.native.handler_text),
	                                           &data.native.handler, argc, argument_names, options);

	if (interrupt_thread)
		runtime->GetESScheduler()->ResetErrorHandlerInterruptThread();

	if (OpStatus::IsSuccess(status))
	{
		OP_DELETEA(data.native.handler_text);
		data.native.handler_text = NULL;
	}

	return status;
}

DOM_EventListener::DOM_EventListener()
	: listener_type(UNKNOWN),
#ifdef ECMASCRIPT_DEBUGGER
	  caller_script_guid(~0u),
	  caller_line_no(~0u),
#endif // ECMASCRIPT_DEBUGGER
	  ref_count(0)
{
}

DOM_EventListener::~DOM_EventListener()
{
	OP_ASSERT(ref_count == 0);

	if (listener_type == NATIVE)
	{
		if (data.native.known_type == DOM_EVENT_CUSTOM)
			OP_DELETEA(data.native.type);

		OP_DELETEA(data.native.handler_text);
	}
#ifdef DOM2_EVENTS_API
	else if (listener_type == EXTERNAL)
		OP_DELETE(data.external.handler);
#endif // DOM2_EVENTS_API
}

OP_STATUS
DOM_EventListener::SetNativeText(DOM_EnvironmentImpl *environment, DOM_EventType known_type, const uni_char *handler_text, unsigned handler_text_length, ES_Object *this_object)
{
	OP_ASSERT(listener_type == UNKNOWN);
	listener_type = NATIVE;

	data.native.known_type = known_type;
	data.native.capture = FALSE;
	data.native.return_value_controls_propagation = TRUE;
	data.native.from_attribute = TRUE;
	data.native.this_object = this_object;
	data.native.handler = NULL;
	data.native.handler_text = UniSetNewStrN(handler_text, handler_text_length);

	if (!data.native.handler_text)
		return OpStatus::ERR_NO_MEMORY;

#ifdef DOM3_EVENTS
	data.native.group = NULL;
#endif // DOM3_EVENTS

	return OpStatus::OK;
}

OP_STATUS
DOM_EventListener::SetNative(DOM_EnvironmentImpl *environment, DOM_EventType known_type, const uni_char *type, BOOL capture, BOOL old_style, ES_Object *this_object, ES_Object *handler)
{
	OP_ASSERT(listener_type == UNKNOWN);
	listener_type = NATIVE;

	data.native.handler_text = NULL;
	data.native.capture = capture;
	data.native.return_value_controls_propagation = old_style;
	data.native.from_attribute = old_style;
	data.native.this_object = this_object;
	data.native.handler = handler;

	if ((data.native.known_type = known_type) == DOM_EVENT_CUSTOM)
		if (!(data.native.type = UniSetNewStr(type)))
			return OpStatus::ERR_NO_MEMORY;

#ifdef DOM3_EVENTS
	data.native.group = group;
#endif // DOM3_EVENTS

	return OpStatus::OK;
}


#ifdef WEBSOCKETS_SUPPORT
void DOM_EventListener::ResetNativeHandler(ES_Object *handler)
{
	OP_ASSERT(listener_type == NATIVE);
	data.native.handler = handler;
}
#endif //WEBSOCKETS_SUPPORT


#ifdef DOM2_EVENTS_API

void
DOM_EventListener::SetExternal(DOM_EventsAPI::EventHandler *handler)
{
	OP_ASSERT(listener_type == UNKNOWN);
	listener_type = EXTERNAL;

	data.external.handler = handler;
}

#endif // DOM2_EVENTS_API

#ifdef ECMASCRIPT_DEBUGGER
void
DOM_EventListener::SetCallerInformation(unsigned script_guid, unsigned line_no)
{
	caller_script_guid = script_guid;
	caller_line_no = line_no;
}
#endif // ECMASCRIPT_DEBUGGER

void
DOM_EventListener::Reset()
{
	if (listener_type != NATIVE)
		listener_type = UNKNOWN;
}

BOOL
DOM_EventListener::HandlesEvent(DOM_Event *event, ES_EventPhase phase)
{
#ifdef DOM3_EVENTS
	return HandlesEvent(event->GetKnownType(), event->GetNamespaceURI(), event->GetType(), phase);
#else // DOM3_EVENTS
	return HandlesEvent(event->GetKnownType(), event->GetType(), phase);
#endif // DOM3_EVENTS
}

BOOL
DOM_EventListener::HandlesEvent(DOM_EventType known_type, const uni_char *type, ES_EventPhase phase)
{
	if (!InList())
		return FALSE;
	else if (listener_type == NATIVE)
	{
		if (phase != ES_PHASE_ANY && (data.native.capture ? phase == ES_PHASE_BUBBLING : phase == ES_PHASE_CAPTURING))
			return FALSE;

		if (known_type != data.native.known_type)
			return FALSE;

		if (known_type == DOM_EVENT_CUSTOM && uni_stricmp(type, data.native.type) != 0)
			return FALSE;

		return TRUE;
	}
#ifdef DOM2_EVENTS_API
	else if (listener_type == EXTERNAL)
		return data.external.handler->HandlesEvent(known_type, type, phase);
#endif // DOM2_EVENTS_API
	else
		return FALSE;
}

BOOL
DOM_EventListener::CanOverride(DOM_EventListener *other)
{
	if (listener_type == NATIVE)
		return other->listener_type == NATIVE &&
		       data.native.known_type == other->data.native.known_type &&
		       (data.native.known_type != DOM_EVENT_CUSTOM || uni_str_eq(data.native.type, other->data.native.type)) &&
		       data.native.from_attribute == other->data.native.from_attribute && (data.native.from_attribute || data.native.handler == other->data.native.handler && data.native.capture == other->data.native.capture);
#ifdef DOM2_EVENTS_API
	else if (listener_type == EXTERNAL)
		return other->listener_type == EXTERNAL && data.external.handler == other->data.external.handler;
#endif // DOM2_EVENTS_API
	else
		return FALSE;
}

class DOM_EventReturnValueListener
	: public ES_ThreadListener,
	  public ES_AsyncCallback
{
public:
	DOM_Event *event;
	BOOL check_return_value;

#ifdef USER_JAVASCRIPT
	ES_Thread *thread;
	DOM_UserJSEvent *userjsevent1, *userjsevent2;

	DOM_EventReturnValueListener(DOM_Event *event, DOM_UserJSEvent *userjsevent1, DOM_UserJSEvent *userjsevent2, BOOL check_return_value)
		: event(event),
		  check_return_value(check_return_value),
		  thread(NULL),
		  userjsevent1(userjsevent1),
		  userjsevent2(userjsevent2)
#else // USER_JAVASCRIPT
	DOM_EventReturnValueListener(DOM_Event *event, BOOL check_return_value)
		: event(event),
		  check_return_value(check_return_value)
#endif // USER_JAVASCRIPT
	{
	}

#ifdef USER_JAVASCRIPT
	~DOM_EventReturnValueListener()
	{
		if (userjsevent1)
			userjsevent1->GetRuntime()->Unprotect(*userjsevent1);
		if (userjsevent2)
			userjsevent2->GetRuntime()->Unprotect(*userjsevent2);
	}
#endif // USER_JAVASCRIPT

	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		if (signal == ES_SIGNAL_FINISHED && check_return_value && thread->ReturnedValue() && !thread->IsFailed())
		{
			ES_Value return_value;
			RETURN_IF_ERROR(thread->GetReturnedValue(&return_value));

			if (return_value.type == VALUE_BOOLEAN && !return_value.value.boolean)
				event->SetPreventDefault(TRUE);
		}

#ifdef USER_JAVASCRIPT
		return SendAfterEventListener();
#else // USER_JAVASCRIPT
		return OpStatus::OK;
#endif // USER_JAVASCRIPT
	}

	OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
	{
		if (status == ES_ASYNC_SUCCESS && check_return_value && result.type == VALUE_BOOLEAN && !result.value.boolean)
			event->SetPreventDefault(TRUE);

#ifdef USER_JAVASCRIPT
		OP_STATUS stat = SendAfterEventListener();

		OP_DELETE(this);
		return stat;
#else // USER_JAVASCRIPT
		OP_DELETE(this);
		return OpStatus::OK;
#endif // USER_JAVASCRIPT
	}

#ifdef USER_JAVASCRIPT
	OP_STATUS SendAfterEventListener()
	{
		if (thread && userjsevent1 && userjsevent2)
		{
			DOM_EnvironmentImpl *environment = ((DOM_Runtime *) thread->GetScheduler()->GetRuntime())->GetEnvironment();
			RETURN_IF_ERROR(environment->GetUserJSManager()->SendAfterEventListener(userjsevent1, userjsevent2, thread));
		}

		return OpStatus::OK;
	}
#endif // USER_JAVASCRIPT
};


OP_STATUS
DOM_EventListener::HandleEvent(DOM_Event *event, ES_Thread *interrupt_thread)
{
#ifdef DOM3_EVENTS
	event->SetCurrentGroup(NULL);
#endif // DOM3_EVENTS

	if (listener_type == NATIVE)
	{
		if (!interrupt_thread)
			interrupt_thread = event->GetThread();

		DOM_EnvironmentImpl *environment = event->GetTarget()->GetEnvironment();

		ES_Value arguments[3];
		int argument_count = 1;

		if (event->GetKnownType() == ONERROR && event->GetWindowEvent() && event->IsA(DOM_TYPE_ERROREVENT))
		{
			DOM_ErrorEvent *error_event = static_cast<DOM_ErrorEvent *>(event);

			DOM_Object::DOMSetString(&arguments[0], error_event->GetMessage());
			DOM_Object::DOMSetString(&arguments[1], error_event->GetResourceUrl());
			DOM_Object::DOMSetString(&arguments[2], error_event->GetResourceLine());

			argument_count = 3;
		}
		else
			DOM_Object::DOMSetObject(&arguments[0], event);

#ifdef DOM3_EVENTS
		if (event->GetStopPropagation(data.native.group))
			return OpStatus::OK;

		event->SetCurrentGroup(data.native.group);
#else // DOM3_EVENTS
		if (event->GetStopImmediatePropagation())
			return OpStatus::OK;
#endif // DOM3_EVENTS

		if (!data.native.handler)
		{
			if (data.native.handler_text)
				RETURN_IF_ERROR(CompileHandler(event->GetCurrentTarget(), interrupt_thread));

			if (!data.native.handler)
				return OpStatus::OK;
		}

#ifdef USER_JAVASCRIPT
		DOM_UserJSManager *user_js_manager = environment->GetUserJSManager();
		DOM_UserJSEvent *userjsevent1 = NULL, *userjsevent2 = NULL;

		if (user_js_manager && user_js_manager->GetIsEnabled())
		{
			OP_BOOLEAN result = user_js_manager->HasBeforeOrAfterEventListener(event->GetKnownType(), event->GetType());
			RETURN_IF_ERROR(result);

			if (result == OpBoolean::IS_TRUE)
				RETURN_IF_ERROR(user_js_manager->CreateAfterEventListener(userjsevent1, userjsevent2, event, data.native.handler));
		}

		DOM_EventReturnValueListener *listener = OP_NEW(DOM_EventReturnValueListener, (event, userjsevent1, userjsevent2, data.native.return_value_controls_propagation));

		if (!listener)
		{
			if (userjsevent1)
				userjsevent1->GetRuntime()->Unprotect(*userjsevent1);
			if (userjsevent2)
				userjsevent2->GetRuntime()->Unprotect(*userjsevent2);

			return OpStatus::ERR_NO_MEMORY;
		}
#else // USER_JAVASCRIPT
		DOM_EventReturnValueListener *listener = OP_NEW(DOM_EventReturnValueListener, (event, data.native.return_value_controls_propagation));

		if (!listener)
			return OpStatus::ERR_NO_MEMORY;
#endif // USER_JAVASCRIPT

		ES_AsyncInterface *asyncif = environment->GetAsyncInterface();
		OP_STATUS status;

		if (op_strcmp(ES_Runtime::GetClass(data.native.handler), "Function") == 0)
		{
			ES_Object *this_object = data.native.this_object ? data.native.this_object : *event->GetCurrentTarget();

			status = asyncif->CallFunction(data.native.handler, this_object, argument_count, arguments, listener, interrupt_thread);
		}
		else
			status = asyncif->CallMethod(data.native.handler, UNI_L("handleEvent"), argument_count, arguments, listener, interrupt_thread);

		if (status != OpStatus::OK)
		{
			OP_DELETE(listener);
			return status;
		}

#ifdef USER_JAVASCRIPT
		else if (user_js_manager && user_js_manager->GetIsEnabled())
			if (ES_Thread *last_thread = asyncif->GetLastStartedThread())
			{
				listener->thread = last_thread;

				RETURN_IF_ERROR(user_js_manager->BeforeEventListener(event, data.native.handler, last_thread));
			}
#endif // USER_JAVASCRIPT

		return status;
	}
#ifdef DOM2_EVENTS_API
	else if (listener_type == EXTERNAL)
	{
		DOM_EventsAPI::Event *external_event;
		RETURN_IF_ERROR(event->GetExternalEvent(external_event));
		return data.external.handler->HandleEvent(external_event);
	}
#endif // DOM2_EVENTS_API
	else
		return OpStatus::OK;
}

void
DOM_EventListener::RegisterHandlers(DOM_EnvironmentImpl *environment)
{
	if (listener_type == NATIVE)
		environment->AddEventHandler(data.native.known_type);
#ifdef DOM2_EVENTS_API
	else if (listener_type == EXTERNAL)
		data.external.handler->RegisterHandlers(environment);
#endif // DOM2_EVENTS_API
}

void
DOM_EventListener::UnregisterHandlers(DOM_EnvironmentImpl *environment)
{
	if (listener_type == NATIVE)
		environment->RemoveEventHandler(data.native.known_type);
#ifdef DOM2_EVENTS_API
	else if (listener_type == EXTERNAL)
		data.external.handler->UnregisterHandlers(environment);
#endif // DOM2_EVENTS_API
}

void
DOM_EventListener::GCTrace(DOM_Runtime *runtime)
{
	if (listener_type == NATIVE)
	{
		DOM_Object::GCMark(runtime, data.native.this_object);
		DOM_Object::GCMark(runtime, data.native.handler);

#ifdef DOM3_EVENTS
		DOM_Object::GCMark(runtime, data.native.group);
#endif // DOM3_EVENTS
	}
}

/* static */ void
DOM_EventListener::IncRefCount(DOM_EventListener *listener)
{
	++listener->ref_count;
}

/* static */ void
DOM_EventListener::DecRefCount(DOM_EventListener *listener)
{
	if (--listener->ref_count == 0)
		OP_DELETE(listener);
}

