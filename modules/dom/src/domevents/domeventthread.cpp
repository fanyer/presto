/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domevents/domeventthread.h"

#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventdata.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/logdoc/htm_elm.h"

#ifdef SCOPE_SUPPORT
# include "modules/scope/scope_readystate_listener.h"
#endif // SCOPE_SUPPORT

DOM_EventThread::DOM_EventThread()
	: ES_Thread(NULL),
	  current_event_node(-1),
	  event(NULL),
	  path_head(NULL),
	  path_body(NULL),
#ifdef DOM_NOT_USED
	  path_tail(NULL),
#endif // DOM_NOT_USED
	  nodes_in_path(0),
	  nodes_in_path_head(0),
	  nodes_in_path_body(0),
#ifdef DOM_NOT_USED
	  nodes_in_path_tail(0),
#endif // DOM_NOT_USED
#ifdef USER_JAVASCRIPT
	  has_fired_afterevent(FALSE),
#endif // USER_JAVASCRIPT
	  is_performing_default(FALSE),
	  has_performed_default(FALSE),
	  has_updated_window_event(FALSE)
{
}

DOM_EventThread::~DOM_EventThread()
{
	if (event)
	{
		event->GetRuntime()->Unprotect(*event);
		if (has_updated_window_event && window_event.type == VALUE_OBJECT)
			event->GetRuntime()->Unprotect(window_event.value.object);
		event->SetThread(NULL);
	}
	if (path_head)
	{
		for (int index = 0; index < nodes_in_path_head; ++index)
			path_head[index]->GetRuntime()->Unprotect(*path_head[index]);

		OP_DELETEA(path_head);
	}

	/* Unprotect the individual HTML elements and delete any that was removed
	   from the document while the event propagated. */
	if (path_body)
	{
		for (int index = 0; index < nodes_in_path_body; ++index)
			path_body[index]->GetESElement()->GetRuntime()->Unprotect(*path_body[index]->GetESElement());

		OP_DELETEA(path_body);
	}

#ifdef DOM_NOT_USED
	if (path_tail)
	{
		for (int index = 0; index < nodes_in_path_tail; ++index)
			path_tail[index]->GetRuntime()->Unprotect(*path_tail[index]);

		OP_DELETEA(path_tail);
	}
#endif // DOM_NOT_USED

	DOM_Object::DOMFreeValue(window_event);
}

struct Toggle
{
	Toggle(BOOL& b, BOOL new_value) : b(b), old_value(b) { b = new_value; }
	~Toggle() { b = old_value; }
	BOOL& b;
	BOOL old_value;
};

OP_STATUS
DOM_EventThread::EvaluateThread()
{
	if (!is_started)
	{
		RETURN_IF_ERROR(ConstructEventPath());
		is_started = TRUE;

		if (!event->GetSynthetic())
		{
			DOM_EventType event_type = event->GetKnownType();

			switch (event_type)
			{
			case ONKEYPRESS:
			{
				DOM_KeyboardEvent *key_event = static_cast<DOM_KeyboardEvent *>(event);
				OpKey::Code key = key_event->GetKeyCode();
				// Need to check if the corresponding keydown was cancelled and this is the first opportunity to do that because this is the
				// first time we know that the keydown handlers have run.
				if (FramesDocument* frames_doc = GetScheduler()->GetFramesDocument())
					if (frames_doc->GetWindow()->HasRecentlyCancelledKeyDown(key))
					{
						has_performed_default = TRUE;
						key_event->SetPreventDefault(TRUE);
					}

				// Check if the key event really should fire keypress. The reason why
				// the event thread is even initiated for the ones that don't is to
				// sequence the default action at the right time -- the keydown event
				// may have called preventDefault(), in which case the default action
				// shouldn't even be performed.
				if (!has_performed_default && !key_event->FiresKeypress())
				{
					has_performed_default = TRUE;
					return key_event->DefaultAction(FALSE);
				}

				break;
			}

#ifdef SCOPE_SUPPORT
			case ONLOAD:
				if (!event->GetWindowEvent())
					break;
				// fall-through
			case DOMCONTENTLOADED:
				if (FramesDocument *frames_doc = GetScheduler()->GetFramesDocument())
					OpScopeReadyStateListener::OnReadyStateChanged(frames_doc, event_type == ONLOAD
						? OpScopeReadyStateListener::READY_STATE_BEFORE_ONLOAD
						: OpScopeReadyStateListener::READY_STATE_BEFORE_DOM_CONTENT_LOADED);

				break;
#endif // SCOPE_SUPPORT
			}
		}
	}

	if (is_performing_default)
		return OpStatus::OK;

	if (has_performed_default)
	{
		is_completed = TRUE;
		return OpStatus::OK;
	}

#ifdef USER_JAVASCRIPT
	if (has_fired_afterevent)
	{
		has_performed_default = TRUE;
		Toggle flag(is_performing_default, TRUE);
		return event->DefaultAction(FALSE);
	}
#endif // USER_JAVASCRIPT

	while (!IsBlocked())
	{
		DOM_Object *current_target;

		if (GetNextCurrentTarget(current_target))
		{
#ifndef DOM_NO_UGLY_NESTED_FORMS_HACK
			if (event->GetKnownType() == ONSUBMIT && current_target != event->GetTarget() &&
			    current_target->IsA(DOM_TYPE_HTML_FORMELEMENT) &&
			    event->GetTarget()->IsA(DOM_TYPE_HTML_FORMELEMENT))
				continue;
#endif // DOM_NO_UGLY_NESTED_FORMS_HACK

			event->SetCurrentTarget(current_target);

			if (DOM_EventTarget *event_target = current_target->GetEventTarget())
			{
				OP_STATUS status = event_target->HandleEvent(event);

				if (OpStatus::IsError(status))
					if (OpStatus::IsMemoryError(status))
						return status;
					else
					{
						event->SetCurrentTarget(NULL);

						is_failed = TRUE;
						return OpStatus::OK;
					}
			}
		}
		else
		{
			event->SetCurrentTarget(NULL);

#ifdef USER_JAVASCRIPT
			DOM_EnvironmentImpl *environment = ((DOM_Runtime *) scheduler->GetRuntime())->GetEnvironment();

			if (DOM_UserJSManager *user_js_manager = environment->GetUserJSManager())
				if (user_js_manager->GetIsEnabled())
				{
					has_fired_afterevent = TRUE;

					RETURN_IF_ERROR(user_js_manager->AfterEvent(event, this));

					if (IsBlocked())
						return OpStatus::OK;
				}
#endif // USER_JAVASCRIPT

			/* DefaultAction is called regardless of whether the event has
			   been cancelled.  For some events we may do something even
			   though it was cancelled. */

			// Threads migrate when someone does document.write. That makes some event threads somewhat useless
			BOOL has_migrated = GetScheduler()->GetRuntime() != event->GetTarget()->GetRuntime();

			has_performed_default = TRUE;
			if (has_migrated)
			{
				// Sending events into the wrong document, a document that doesn't
				// contain target, will not make anyone happy
				return OpStatus::OK;
			}

			Toggle flag(is_performing_default, TRUE);
			return event->DefaultAction(FALSE);
		}
	}

	return OpStatus::OK;
}

/* virtual */
ES_ThreadType
DOM_EventThread::Type()
{
	return ES_THREAD_EVENT;
}

/* virtual */
ES_ThreadInfo
DOM_EventThread::GetInfo()
{
	ES_ThreadInfo info = ES_Thread::GetInfo();

	info.data.event.modifiers = event->GetModifiers();

	info.data.event.is_window_event = event->GetWindowEvent();

	switch (info.data.event.type = GetEventType())
	{
	case ONMOUSEUP:
	case ONMOUSEDOWN:
	case ONCLICK:
		if (event->IsA(DOM_TYPE_MOUSEEVENT))
			info.data.event.data.button = static_cast<DOM_MouseEvent *>(event)->GetButton();
		else
			info.data.event.data.button = 0;
		break;

	case ONKEYDOWN:
	case ONKEYUP:
	case ONKEYPRESS:
		if (event->IsA(DOM_TYPE_KEYBOARDEVENT))
			info.data.event.data.keycode = static_cast<DOM_KeyboardEvent *>(event)->GetKeyCode();
		else
			info.data.event.data.keycode = 0;
		break;
	}

	return info;
}

/* virtual */
ES_ThreadInfo
DOM_EventThread::GetOriginInfo()
{
	/* Normally, unload events are sent from a terminating thread.  This is
	   not very interesting and really just confusing to whomever is watching,
	   so we pretend the unload event thread was its own origin thread in
	   that case. */

	if (GetEventType() == ONUNLOAD && interrupted_thread && interrupted_thread->Type() == ES_THREAD_TERMINATING)
		return GetInfo();

#ifdef USER_JAVASCRIPT
	/* Also, 'AfterEvent' User Javascript events are implemented so
	   that the real event thread interrupts the 'AfterEvent' event
	   thread.  This should not affect the origin info of the real
	   event thread. */

	if (interrupted_thread && interrupted_thread->Type() == ES_THREAD_EVENT && ((DOM_EventThread *) interrupted_thread)->GetEvent()->IsA(DOM_TYPE_USERJSEVENT) && !interrupted_thread->GetInterruptedThread())
		return GetInfo();
#endif // USER_JAVASCRIPT

	return ES_Thread::GetOriginInfo();
}

/* virtual */
const uni_char *
DOM_EventThread::GetInfoString()
{
	uni_char *buffer = (uni_char *) g_memory_manager->GetTempBuf();
	op_strcpy((char *) buffer, "Event thread: ");

	if (event->GetKnownType() != DOM_EVENT_CUSTOM)
		op_strcat((char *) buffer, DOM_EVENT_NAME(event->GetKnownType()));

	make_doublebyte_in_place(buffer, op_strlen((char *) buffer));

	if (event->GetKnownType() == DOM_EVENT_CUSTOM)
		uni_strcat(buffer, event->GetType());

	return buffer;
}

OP_STATUS
DOM_EventThread::InitEventThread(DOM_Event *new_event, DOM_EnvironmentImpl *new_environment)
{
	if (!new_event->GetRuntime()->Protect(*new_event))
		return OpStatus::ERR_NO_MEMORY;

	event = new_event;
	environment = new_environment;

	if (!new_event->GetSynthetic())
		switch (new_event->GetKnownType())
		{
		case ONCLICK:
			{
				ShiftKeyState local_modifiers = 0;
				if (static_cast<DOM_MouseEvent*>(event)->shift_key)
					local_modifiers |= SHIFTKEY_SHIFT;
				if (static_cast<DOM_MouseEvent*>(event)->ctrl_key)
					local_modifiers |= SHIFTKEY_CTRL;
				if (SHIFTKEY_OPEN_IN_NEW_WINDOW & local_modifiers)
					SetOpenInNewWindow();
				BOOL toggle_open_in_background = ((local_modifiers & SHIFTKEY_TOGGLE_OPEN_IN_BACKGROUND)
												 && !(local_modifiers & SHIFTKEY_PREVENT_OPEN_IN_BACKGROUND));
				if (toggle_open_in_background)
					SetOpenInBackground();
			}
		case ONMOUSEDOWN:
		case ONMOUSEUP:
		case ONKEYPRESS:
		case ONKEYDOWN:
		case ONKEYUP:
		case TEXTINPUT:
		case ONCHANGE:
		case ONDBLCLICK:
		case ONSUBMIT:
			SetIsUserRequested();
			break;
		}

	event->SetThread(this);
	return OpStatus::OK;
}

DOM_EventType
DOM_EventThread::GetEventType()
{
	return event->GetKnownType();
}

DOM_Object *
DOM_EventThread::GetEventTarget()
{
	return event->GetTarget();
}

DOM_Event *
DOM_EventThread::GetEvent()
{
	return event;
}

/* virtual */
OP_STATUS
DOM_EventThread::Signal(ES_ThreadSignal signal)
{
	OP_STATUS status = ES_Thread::Signal(signal);

	if (signal == ES_SIGNAL_CANCELLED && !has_performed_default)
		if (OpStatus::IsMemoryError(event->DefaultAction(TRUE)))
			status = OpStatus::ERR_NO_MEMORY;

	return status;
}

/* Also used in domeventlistener.cpp. */
BOOL
IsDOMNode(DOM_Object* obj)
{
	BOOL is_node = obj->IsA(DOM_TYPE_NODE);
#ifdef SVG_DOM
	is_node = is_node || obj->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE);
#endif // SVG_DOM
	return is_node;
}

OP_STATUS
DOM_EventThread::ConstructEventPath()
{
	DOM_Object* target_object = event->GetDispatchTarget();
	if (!IsDOMNode(target_object))
	{
		// Event is propagated to the target only.
		current_event_node = 0;
		return OpStatus::OK;
	}

	DOM_Node *target_node = static_cast<DOM_Node*>(target_object);
	DOM_Node *real_target_node = static_cast<DOM_Node*>(event->GetRealTarget());
	DOM_Object *head_node = NULL;

	HTML_Element *target_element = target_node->GetThisElement();
	if (!target_element && (target_node->IsA(DOM_TYPE_DOCUMENT) || target_node->IsA(DOM_TYPE_DOCUMENTFRAGMENT)))
	{
		target_element = target_node->GetPlaceholderElement();
		OP_ASSERT(target_element->Type() == HE_DOC_ROOT);
	}
	HTML_Element *real_target_element = real_target_node ? real_target_node->GetThisElement() : target_element;

	BOOL bubbles = event->GetBubbles();

	HTML_Element *element = target_element, *first_capturing_target = NULL;

	BOOL in_document= FALSE;
	while (element)
	{
		// Need to see the difference between documents and document fragments
		if (element->Type() == HE_DOC_ROOT && (!element->GetESElement() || element->GetESElement()->IsA(DOM_TYPE_DOCUMENT)))
			in_document = TRUE;

		if (DOM_EventTarget *event_target = DOM_Node::GetEventTargetFromElement(element))
			// No event_target -> no registered listeners
			if (element->GetESElement() != target_node && event_target->HasListeners(event, ES_PHASE_CAPTURING))
				first_capturing_target = element;

		element = DOM_Node::GetEventPathParent(element, real_target_element);
	}

	if (!event->GetWindowEvent() && event->GetKnownType() == ONLOAD)
	{
		// This is a special case that shouldn't reach
		// the window object in the capture phase because of
		// web compatibility (because of old Mozillas and now
		// all browsers).
		// If this onload (sent from iframes, images, ...)
		// would reach the window in the capture phase scripts
		// would probably think it's the document onload.
	}
	else if (in_document)
		head_node = target_node->GetEnvironment()->GetWindow();

	BOOL head_node_captures = FALSE, head_node_listens = FALSE;

	if (head_node)
		if (DOM_EventTarget *event_target = head_node->GetEventTarget())
			if (head_node == target_node && event_target->HasListeners(event, ES_PHASE_AT_TARGET))
				head_node_listens = TRUE;
			else if (head_node != target_node && event_target->HasListeners(event, ES_PHASE_CAPTURING))
			{
				head_node_captures = TRUE;
				first_capturing_target = NULL;
			}
			else if (bubbles && event_target->HasListeners(event, ES_PHASE_BUBBLING))
				head_node_listens = TRUE;

	element = target_element;
	while (element)
	{
		++nodes_in_path_body;
		element = DOM_Node::GetEventPathParent(element, real_target_element);
	}

	if (head_node && (head_node == target_node && head_node_listens || head_node_captures || bubbles && head_node_listens))
	{
		++nodes_in_path_head;
#ifdef DOM_TOO_COMPLEX
	}

	if (nodes_in_path_head > 0)
	{
#endif // DOM_TOO_COMPLEX
		path_head = OP_NEWA(DOM_Object *, nodes_in_path_head);
		if (!path_head)
			return OpStatus::ERR_NO_MEMORY;

		path_head[0] = NULL;

		if (!head_node->GetRuntime()->Protect(*head_node))
		{
			nodes_in_path_head = 0;
			return OpStatus::ERR_NO_MEMORY;
		}

		path_head[0] = head_node;

#ifdef DOM_TOO_COMPLEX
		/* Disabled this code since it's too complex: there is never more
		   than one node in the head anyway. */
		for (int index = 0; index < nodes_in_path_head; ++index)
			if (!path_head[index]->GetRuntime()->Protect(*path_head[index]))
			{
				while (index > 0)
				{
					--index;
					path_head[index]->GetRuntime()->Unprotect(*path_head[index]);
				}

				OP_DELETEA(path_head);
				path_head = NULL;

				return OpStatus::ERR_NO_MEMORY;
			}
#endif // DOM_TOO_COMPLEX
	}

	if (nodes_in_path_body > 0)
	{
		path_body = OP_NEWA(HTML_Element*, nodes_in_path_body);
		if (!path_body)
			return OpStatus::ERR_NO_MEMORY;

		for (int i = 0; i < nodes_in_path_body; ++i)
			path_body[i] = NULL;

		HTML_Element *element = target_element;
		int index = 0;
		while (element)
		{
			DOM_Node *node;
			if (OpStatus::IsMemoryError(target_node->GetEnvironment()->ConstructNode(node, element, target_node->GetOwnerDocument())) || !node->GetRuntime()->Protect(*node))
			{
				nodes_in_path_body = index;
				return OpStatus::ERR_NO_MEMORY;
			}

			if (element == first_capturing_target)
				current_event_node = -index;

			path_body[index++] = element;
			element = DOM_Node::GetEventPathParent(element, real_target_element);
		}

		event->SetBubblingPath(path_body, nodes_in_path_body);
	}

	nodes_in_path = nodes_in_path_head + nodes_in_path_body;
#ifdef DOM_NOT_USED
	nodes_in_path += nodes_in_path_tail;
#endif // DOM_NOT_USED

	if (head_node_captures)
		current_event_node = -(nodes_in_path - 1);
	else if (!first_capturing_target)
		current_event_node = 0;

#ifdef DOM_NOT_USED
	/* Disabled this code for now since this never happens. */
	if (nodes_in_path_tail > 0)
	{
		path_tail = OP_NEWA(DOM_Node*, nodes_in_path_tail);
		if (!path_tail)
			return OpStatus::ERR_NO_MEMORY;

		/* FIXME-DISABLED: more general code maybe? */
		path_tail[0] = target_node;

		for (int index = 0; index < nodes_in_path_tail; ++index)
			if (!path_tail[index]->GetRuntime()->Protect(*path_tail[index]))
			{
				while (index > 0)
				{
					--index;
					path_tail[index]->GetRuntime()->Unprotect(*path_tail[index]);
				}

				OP_DELETEA(path_tail);
				path_tail = NULL;

				return OpStatus::ERR_NO_MEMORY;
			}
	}
#endif // DOM_NOT_USED

	return OpStatus::OK;
}

BOOL
DOM_EventThread::GetNextCurrentTarget(DOM_Object *&node)
{
	if (IsDOMNode(event->GetDispatchTarget()))
	{
#ifdef DOM3_EVENTS
		if (nodes_in_path == 0)
			return FALSE;
#else // DOM3_EVENTS
		if (event->GetStopPropagation() || nodes_in_path == 0)
			return FALSE;
#endif // DOM3_EVENTS

		if (current_event_node < 0)
		{
			node = GetNode(-current_event_node);
			event->SetEventPhase(ES_PHASE_CAPTURING);
		}
		else if (current_event_node < nodes_in_path)
		{
			node = GetNode(current_event_node);

			if (node == event->GetTarget())
				event->SetEventPhase(ES_PHASE_AT_TARGET);
			else if (event->GetBubbles())
				event->SetEventPhase(ES_PHASE_BUBBLING);
			else
				return FALSE;
		}
		else
			return FALSE;
	}
	else if (current_event_node == 0)
	{
		node = event->GetDispatchTarget();
		event->SetEventPhase(ES_PHASE_AT_TARGET);
	}
	else
		return FALSE;

	++current_event_node;
	return TRUE;
}

DOM_Object *
DOM_EventThread::GetNode(int index)
{
	if (index < nodes_in_path_body)
		return (DOM_Node *) path_body[index]->GetESElement();
	else
		return path_head[index - nodes_in_path_body];

#ifdef DOM_NOT_USED
	if (index < nodes_in_path_body)
		return (DOM_Node *) path_body[index]->GetESElement();
	else
		index -= nodes_in_path_body;

	OP_ASSERT(index < nodes_in_path_tail);
	return path_tail[index];
#endif // DOM_NOT_USED
}

OP_STATUS
DOM_EventThread::GetWindowEvent(ES_Value *value)
{
	if (!value || !GetEvent())
		return OpStatus::OK;

	if (has_updated_window_event)
	{
		*value = window_event;
		return OpStatus::OK;
	}
	else
		return DOM_Object::DOMSetObjectAsHidden(value, GetEvent());
}

OP_STATUS
DOM_EventThread::PutWindowEvent(ES_Value *value)
{
	if (!GetEvent())
		return OpStatus::OK;

	if (has_updated_window_event && window_event.type == VALUE_OBJECT)
		GetEvent()->GetRuntime()->Unprotect(window_event.value.object);

	has_updated_window_event = FALSE;
	DOM_Object::DOMFreeValue(window_event);
	window_event.type = VALUE_UNDEFINED;

	RETURN_IF_ERROR(DOM_Object::DOMCopyValue(window_event, *value));
	if (window_event.type == VALUE_OBJECT)
		if (!GetEvent()->GetRuntime()->Protect(window_event.value.object))
			return OpStatus::ERR_NO_MEMORY;

	has_updated_window_event = TRUE;
	return OpStatus::OK;
}
