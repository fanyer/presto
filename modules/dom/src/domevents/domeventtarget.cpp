/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domobj.h"

#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/util/simset.h"
#include "modules/doc/frm_doc.h"

class DOM_EventTargetThread
	: public ES_Thread
{
public:
	DOM_EventTargetThread(DOM_Event *event);

	virtual ~DOM_EventTargetThread();
	virtual OP_STATUS EvaluateThread();

	OP_STATUS AddListener(DOM_EventListener *listener);

private:
	class ListenerElement
		: public ListElement<ListenerElement>
	{
	public:
		DOM_EventListener *listener;
	};

	List<ListenerElement> listener_elements;
	DOM_Event *event;
};

DOM_EventTargetThread::DOM_EventTargetThread(DOM_Event *event)
	: ES_Thread(NULL),
	  event(event)
{
}

DOM_EventTargetThread::~DOM_EventTargetThread()
{
	ListenerElement *listener_element = (ListenerElement *) listener_elements.First(); 
	while (listener_element)
	{
		ListenerElement *next = listener_element->Suc();
		DOM_EventListener::DecRefCount(listener_element->listener);
		listener_element = next;
	}
	listener_elements.Clear();
}

/* virtual */ OP_STATUS
DOM_EventTargetThread::EvaluateThread()
{
	while (ListenerElement *listener_element = (ListenerElement *) listener_elements.First())
	{
		DOM_EventListener *listener = listener_element->listener;

		listener_element->Out();
		OP_DELETE(listener_element);

		OP_STATUS status = OpStatus::OK;
		if (listener->IsRegistered())
			status = listener->HandleEvent(event, this);

		DOM_EventListener::DecRefCount(listener);

		RETURN_IF_ERROR(status);

		if (IsBlocked())
			return OpStatus::OK;
	}

	is_completed = TRUE;
	return OpStatus::OK;
}

OP_STATUS
DOM_EventTargetThread::AddListener(DOM_EventListener *listener)
{
	ListenerElement *listener_element = OP_NEW(ListenerElement, ());
	if (!listener_element)
		return OpStatus::ERR_NO_MEMORY;

	listener_element->listener = listener;
	listener_element->Into(&listener_elements);

	DOM_EventListener::IncRefCount(listener);
	return OpStatus::OK;
}


#ifdef EXTENSION_SUPPORT
#ifdef DEBUG_ENABLE_OPASSERT
static BOOL
HasEventHandlersCounter(DOM_EventType type)
{
	/* Must stay up-to-date with DOM_EnvironmentImpl::GetEventHandlersCounter() */
	if (type >= ONMOUSEOVER && type <= ONMOUSEOUT)
		return TRUE;

#ifdef DOM2_MUTATION_EVENTS
	if (type >= DOMSUBTREEMODIFIED && type <= DOMCHARACTERDATAMODIFIED)
		return TRUE;
#endif // DOM2_MUTATION_EVENTS

	return FALSE;
}
#endif // DEBUG_ENABLE_OPASSERT
#endif // EXTENSION_SUPPORT

DOM_EventTarget::DOM_EventTarget(DOM_Object *owner)
	: owner(owner)
{
}

DOM_EventTarget::~DOM_EventTarget()
{
	while (DOM_EventListener *listener = static_cast<DOM_EventListener *>(listeners.First()))
	{
		listener->Out();
#ifdef EXTENSION_SUPPORT
		if (owner->GetRuntime()->HasSharedEnvironment())
			OP_ASSERT(DOM_EventListener::IsNativeHandler(listener) && !HasEventHandlersCounter(DOM_EventListener::GetNativeKnownType(listener)));
		else
#endif // EXTENSION_SUPPORT
			listener->UnregisterHandlers(owner->GetEnvironment());

		DOM_EventListener::DecRefCount(listener);
	}
}

DOM_EventTarget::NativeIterator::NativeIterator(const DOM_EventTarget &target)
	: listener(static_cast<DOM_EventListener *>(target.listeners.First()))
{
	while (listener && !listener->IsNativeHandler())
		listener = static_cast<DOM_EventListener *>(listener->Suc());
}

BOOL
DOM_EventTarget::NativeIterator::AtEnd() const
{
	return !listener;
}

const DOM_EventListener *
DOM_EventTarget::NativeIterator::Current() const
{
	OP_ASSERT(!AtEnd());
	return listener;
}

void
DOM_EventTarget::NativeIterator::Next()
{
	OP_ASSERT(!AtEnd());
	listener = static_cast<DOM_EventListener *>(listener->Suc());
	while (listener && !listener->IsNativeHandler())
		listener = static_cast<DOM_EventListener *>(listener->Suc());
}

BOOL
DOM_EventTarget::HasListeners(DOM_Event *event, ES_EventPhase phase)
{
#ifdef DOM3_EVENTS
	return HasListeners(event->GetKnownType(), event->GetNamespaceURI(), event->GetType(), phase);
#else // DOM3_EVENTS
	return HasListeners(event->GetKnownType(), event->GetType(), phase);
#endif // DOM3_EVENTS
}

BOOL
#ifdef DOM3_EVENTS
DOM_EventTarget::HasListeners(DOM_EventType known_type, const uni_char *namespaceURI, const uni_char *type, ES_EventPhase phase)
#else // DOM3_EVENTS
DOM_EventTarget::HasListeners(DOM_EventType known_type, const uni_char *type, ES_EventPhase phase)
#endif // DOM3_EVENTS
{
	for (DOM_EventListener *listener = (DOM_EventListener *) listeners.First();
	     listener;
	     listener = (DOM_EventListener *) listener->Suc())
#ifdef DOM3_EVENTS
		if (listener->HandlesEvent(known_type, namespaceURI, type, phase))
#else // DOM3_EVENTS
		if (listener->HandlesEvent(known_type, type, phase))
#endif // DOM3_EVENTS
			return TRUE;

	return FALSE;
}

OP_STATUS
DOM_EventTarget::HandleEvent(DOM_Event *event)
{
	ES_EventPhase phase = event->GetEventPhase();
#ifdef DOCUMENT_EDIT_SUPPORT
	BOOL is_designmode_document = event->GetFramesDocument() && event->GetFramesDocument()->GetDesignMode();
#endif // DOCUMENT_EDIT_SUPPORT

	DOM_EventListener *listener = (DOM_EventListener *) listeners.First(), *first = NULL;
	DOM_EventTargetThread *thread = NULL;

	while (listener)
	{
		DOM_EventListener *next = (DOM_EventListener *) listener->Suc();

#ifdef DOCUMENT_EDIT_SUPPORT
		if (is_designmode_document && listener->IsFromAttribute())
		{
			// Ignore this handler
		}
		else
#endif // DOCUMENT_EDIT_SUPPORT
		if (listener->HandlesEvent(event, phase))
			if (thread)
			{
				if (OpStatus::IsMemoryError(thread->AddListener(listener)))
				{
					OP_DELETE(thread);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			else if (first)
			{
				thread = OP_NEW(DOM_EventTargetThread, (event));
				if (!thread)
					return OpStatus::ERR_NO_MEMORY;

				if (OpStatus::IsMemoryError(thread->AddListener(first)) ||
				    OpStatus::IsMemoryError(thread->AddListener(listener)))
				{
					OP_DELETE(thread);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			else
				first = listener;

		listener = next;
	}

	if (thread)
	{
		ES_Thread *event_thread = event->GetThread();
		RETURN_IF_ERROR(event_thread->GetScheduler()->AddRunnable(thread, event_thread));
	}
	else if (first)
		RETURN_IF_ERROR(first->HandleEvent(event));

	return OpStatus::OK;
}

void
DOM_EventTarget::AddListener(DOM_EventListener *new_listener)
{
	RemoveListener(new_listener);
	new_listener->Into(&listeners);
#ifdef EXTENSION_SUPPORT
	if (owner->GetRuntime()->HasSharedEnvironment())
		OP_ASSERT(DOM_EventListener::IsNativeHandler(new_listener) && !HasEventHandlersCounter(DOM_EventListener::GetNativeKnownType(new_listener)));
	else
#endif // EXTENSION_SUPPORT
		new_listener->RegisterHandlers(owner->GetEnvironment());

	DOM_EventListener::IncRefCount(new_listener);
}

void
DOM_EventTarget::RemoveListener(DOM_EventListener *old_listener)
{
	if (old_listener->InList())
	{
		old_listener->Out();
#ifdef EXTENSION_SUPPORT
		if (owner->GetRuntime()->HasSharedEnvironment())
			OP_ASSERT(DOM_EventListener::IsNativeHandler(old_listener) && !HasEventHandlersCounter(DOM_EventListener::GetNativeKnownType(old_listener)));
		else
#endif // EXTENSION_SUPPORT
			old_listener->UnregisterHandlers(owner->GetEnvironment());

		DOM_EventListener::DecRefCount(old_listener);
	}
	else
	{
		for (DOM_EventListener *listener = (DOM_EventListener *) listeners.First();
		     listener;
		     listener = (DOM_EventListener *) listener->Suc())
			if (old_listener->CanOverride(listener))
			{
				listener->Out();
#ifdef EXTENSION_SUPPORT
				if (owner->GetRuntime()->HasSharedEnvironment())
					OP_ASSERT(DOM_EventListener::IsNativeHandler(listener) && !HasEventHandlersCounter(DOM_EventListener::GetNativeKnownType(listener)));
				else
#endif // EXTENSION_SUPPORT
					listener->UnregisterHandlers(owner->GetEnvironment());
				listener->Reset();

				DOM_EventListener::DecRefCount(listener);
				break;
			}
	}
}

OP_BOOLEAN
DOM_EventTarget::FindOldStyleHandler(DOM_EventType type, ES_Object **handler)
{
	for (DOM_EventListener *listener = (DOM_EventListener *) listeners.First();
	     listener;
	     listener = (DOM_EventListener *) listener->Suc())
#ifdef DOM3_EVENTS
		if (listener->IsFromAttribute() && listener->HandlesEvent(type, NULL, NULL, ES_PHASE_ANY))
#else // DOM3_EVENTS
		if (listener->IsFromAttribute() && listener->HandlesEvent(type, NULL, ES_PHASE_ANY))
#endif // DOM3_EVENTS
		{
			if (handler)
			{
				*handler = listener->GetNativeHandler();

				if (!*handler)
				{
					RETURN_IF_ERROR(listener->CompileHandler(owner));
					*handler = listener->GetNativeHandler();
				}
			}

			return OpBoolean::IS_TRUE;
		}

	if (handler)
		*handler = NULL;

	return OpBoolean::IS_FALSE;
}

#ifdef DOM2_EVENTS_API

/* virtual */ DOM_Object *
DOM_EventTarget::GetNode()
{
	return owner;
}

/* virtual */ OP_STATUS
DOM_EventTarget::AddEventHandler(DOM_EventsAPI::EventHandler *handler)
{
	DOM_EventListener *listener = OP_NEW(DOM_EventListener, ());
	if (!listener)
		return OpStatus::ERR_NO_MEMORY;

	listener->SetExternal(handler);
	AddListener(listener);

	return OpStatus::OK;
}

/* virtual */ void
DOM_EventTarget::RemoveEventHandler(DOM_EventsAPI::EventHandler *handler)
{
	DOM_EventListener listener;
	listener.SetExternal(handler);
	RemoveListener(&listener);
	listener.Reset();
}

#endif // DOM2_EVENTS_API

void
DOM_EventTarget::GCTrace()
{
	for (DOM_EventListener *listener = (DOM_EventListener *) listeners.First();
	     listener;
	     listener = (DOM_EventListener *) listener->Suc())
		listener->GCTrace(owner->GetRuntime());
}

/* virtual */ DOM_EventTarget *
DOM_EventTargetOwner::FetchEventTarget()
{
	return event_target;
}

/* virtual */ OP_STATUS
DOM_EventTargetOwner::CreateEventTarget()
{
	if (!event_target)
	{
		event_target = OP_NEW(DOM_EventTarget, (GetOwnerObject()));
		if (!event_target)
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

