/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef USER_JAVASCRIPT

#include "modules/dom/src/userjs/userjsevent.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/dom/src/domevents/domeventthread.h"

class DOM_UserJSEvent_ThreadListener
	: public ES_ThreadListener
{
protected:
	DOM_UserJSEvent *event;

public:
	DOM_UserJSEvent_ThreadListener(DOM_UserJSEvent *event)
		: event(event)
	{
		event->GetJavascriptURLThread()->AddListener(this);
	}

	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		switch (signal)
		{
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
		case ES_SIGNAL_CANCELLED:
			event->ResetJavascriptURLThread();
			event->GetRuntime()->Unprotect(*event);
		}
		return OpStatus::OK;
	}
};

DOM_UserJSEvent::DOM_UserJSEvent(DOM_UserJSManager *manager)
	: manager(manager),
	  node(NULL),
	  event(NULL),
	  listener(NULL),
	  thread(NULL),
	  returnValueSet(FALSE),
	  was_stopped_before(FALSE)
{
}

/* static */ OP_STATUS
DOM_UserJSEvent::Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, const uni_char *type)
{
	DOM_Runtime *runtime = manager->GetEnvironment()->GetDOMRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(userjsevent = OP_NEW(DOM_UserJSEvent, (manager)), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "UserJSEvent"));

	DOM_Object *target;
	RETURN_IF_ERROR(manager->GetObject(target));
	userjsevent->InitEvent(DOM_EVENT_CUSTOM, target);

	if (type && uni_strncmp(type, "Before", 6) == 0)
		userjsevent->SetCancelable(TRUE);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_UserJSEvent::Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, DOM_Event *event, const uni_char *type, ES_Object *listener)
{
	RETURN_IF_ERROR(Make(userjsevent, manager, type));
	if (type)
	{
#ifdef DOM3_EVENTS
		RETURN_IF_ERROR(userjsevent->SetType(UNI_L("http://www.opera.com/userjs"), type));
#else // DOM3_EVENTS
		RETURN_IF_ERROR(userjsevent->SetType(type));
#endif // DOM3_EVENTS
	}

	userjsevent->event = event;
	userjsevent->listener = listener;

#ifdef DOM3_EVENTS
	userjsevent->was_stopped_before = event->GetStopPropagation(event->GetCurrentGroup());
#else // DOM3_EVENTS
	userjsevent->was_stopped_before = event->GetStopPropagation();
#endif // DOM3_EVENTS

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_UserJSEvent::Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, DOM_Node *node, const uni_char *type)
{
	RETURN_IF_ERROR(Make(userjsevent, manager, type));

#ifdef DOM3_EVENTS
	RETURN_IF_ERROR(userjsevent->SetType(UNI_L("http://www.opera.com/userjs"), type));
#else // DOM3_EVENTS
	RETURN_IF_ERROR(userjsevent->SetType(type));
#endif // DOM3_EVENTS

	userjsevent->node = node;

	if (uni_str_eq(type, "AfterScript"))
		node->GetThisElement()->StealScriptSource(&userjsevent->script_source);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_UserJSEvent::Make(DOM_UserJSEvent *&userjsevent, DOM_UserJSManager *manager, ES_JavascriptURLThread *thread, const uni_char *type)
{
	RETURN_IF_ERROR(Make(userjsevent, manager, type));

#ifdef DOM3_EVENTS
	RETURN_IF_ERROR(userjsevent->SetType(UNI_L("http://www.opera.com/userjs"), type));
#else // DOM3_EVENTS
	RETURN_IF_ERROR(userjsevent->SetType(type));
#endif // DOM3_EVENTS

	userjsevent->thread = thread;

	DOM_Runtime *runtime = manager->GetEnvironment()->GetDOMRuntime();
	if (runtime->Protect(*userjsevent))
	{
		if (!OP_NEW(DOM_UserJSEvent_ThreadListener, (userjsevent)))
		{
			runtime->Unprotect(*userjsevent);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* virtual */
DOM_UserJSEvent::~DOM_UserJSEvent()
{
	DOMFreeValue(source);
	DOMFreeValue(returnValue);
}

/* virtual */ ES_GetState
DOM_UserJSEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_element:
		DOMSetObject(value, node);
		return GET_SUCCESS;

	case OP_ATOM_event:
		DOMSetObject(value, event);
		return GET_SUCCESS;

	case OP_ATOM_listener:
		DOMSetObject(value, listener);
		return GET_SUCCESS;

	case OP_ATOM_eventCancelled:
		DOMSetBoolean(value, event && event->GetPreventDefault());
		return GET_SUCCESS;

	case OP_ATOM_propagationStopped:
#ifdef DOM3_EVENTS
		DOMSetBoolean(value, event && event->GetStopPropagation(event->GetCurrentGroup()));
#else // DOM3_EVENTS
		DOMSetBoolean(value, event && event->GetStopPropagation());
#endif // DOM3_EVENTS
		return GET_SUCCESS;

	case OP_ATOM_source:
		if (thread)
			DOMSetString(value, thread->GetSource());
		else if (source.type == VALUE_STRING)
			DOMSetString(value, source.value.string);
		return GET_SUCCESS;

	case OP_ATOM_returnValue:
		if (value)
			if (returnValueSet)
				*value = returnValue;
			else if (thread && thread->ReturnedValue())
				GET_FAILED_IF_ERROR(thread->GetReturnedValue(value));
			else
				DOMSetUndefined(value);
		return GET_SUCCESS;

	case OP_ATOM_cssText:
		if (HTML_Element *he = node ? node->GetThisElement() : NULL)
			if (he->IsLinkElement() || he->IsStyleElement())
				return DOM_Node::GetCssContents(node, value, static_cast<DOM_Runtime *>(origining_runtime), GetEmptyTempBuf());

	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_UserJSEvent::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_source:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			PUT_FAILED_IF_ERROR(DOMCopyValue(source, *value));

			if (thread)
				PUT_FAILED_IF_ERROR(thread->SetSource(value->value.string));

			return PUT_SUCCESS;
		}
		break;

	case OP_ATOM_returnValue:
		PUT_FAILED_IF_ERROR(DOMCopyValue(returnValue, *value));
		returnValueSet = TRUE;

		if (thread && value->type == VALUE_STRING)
			PUT_FAILED_IF_ERROR(thread->SetResult(value->value.string));

		return PUT_SUCCESS;

	case OP_ATOM_cssText:
		if (HTML_Element *he = node ? node->GetThisElement() : NULL)
			if (he->IsLinkElement() || he->IsStyleElement())
				return DOM_Node::SetCssContents(node, value, static_cast<DOM_Runtime *>(origining_runtime));
		break;
	}

	return DOM_Event::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_UserJSEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(event);
	GCMark(returnValue);
	GCMark(node);
}

/* virtual */ OP_STATUS
DOM_UserJSEvent::DefaultAction(BOOL cancelled)
{
	if (!cancelled)
		return manager->EventFinished(this);
	else
		return manager->EventCancelled(this);
}

OP_STATUS
DOM_UserJSEvent::GetEventScriptSource(TempBuffer *buffer)
{
	if (uni_str_eq(GetType(), "AfterScript"))
	{
		// Script data has left the tree, but it can be found in the event.
		for (DataSrcElm *src_elm = script_source.First(); src_elm; src_elm = src_elm->Suc())
			RETURN_IF_ERROR(buffer->Append(src_elm->GetSrc(), src_elm->GetSrcLen()));
	}
	else if (HTML_Element* elm = node ? node->GetThisElement() : NULL)
		RETURN_IF_ERROR(elm->DOMGetContents(node->GetEnvironment(), buffer));

	return OpStatus::OK;
}

/* static */ OP_BOOLEAN
DOM_UserJSEvent::GetScriptSource(ES_Runtime *runtime, TempBuffer *buffer)
{
	if (DOM_UserJSManager::IsActiveInRuntime(runtime))
	{
		ES_Thread *thread = GetCurrentThread(runtime);
		while (thread)
		{
			if (thread->Type() == ES_THREAD_EVENT)
			{
				DOM_Event *dom_event = static_cast<DOM_EventThread *>(thread)->GetEvent();
				if (dom_event->IsA(DOM_TYPE_USERJSEVENT))
				{
					DOM_UserJSEvent *userjs_event = static_cast<DOM_UserJSEvent *>(dom_event);
					RETURN_IF_ERROR(userjs_event->GetEventScriptSource(buffer));
					return OpBoolean::IS_TRUE;
				}
			}
			thread = thread->GetInterruptedThread();
		}
	}
	return OpBoolean::IS_FALSE;
}

#endif // USER_JAVASCRIPT
