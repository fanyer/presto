/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domobj.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domdefines.h"
#include "modules/dom/src/domevents/domeventdata.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domglobaldata.h"

#include "modules/ecmascript_utils/essched.h"
#include "modules/doc/frm_doc.h"

#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpKeys.h"

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT
#include "modules/pi/OpDragObject.h"

#ifdef USE_OP_CLIPBOARD
#include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

#ifdef PI_UIINFO_TOUCH_EVENTS
#include "modules/pi/ui/OpUiInfo.h"
#endif // PI_UIINFO_TOUCH_EVENTS

#ifdef DOM2_EVENTS_API

class DOM_ExternalEvent
	: public DOM_EventsAPI::Event
{
private:
	DOM_Event *event;
	int protected_count;

public:
	DOM_ExternalEvent(DOM_Event *event)
		: event(event),
		  protected_count(0)
	{
	}

	virtual DOM_EventType GetType() { return event->GetKnownType(); }
	virtual const uni_char *GetTypeString() { return event->GetType(); }

	virtual DOM_Object *GetTarget() { return event->GetTarget(); }
	virtual DOM_Object *GetCurrentTarget() { return event->GetCurrentTarget(); }
	virtual ES_EventPhase GetEventPhase() { return event->GetEventPhase(); }
	virtual BOOL GetBubbles() { return event->GetBubbles(); }
	virtual BOOL GetCancellable() { return event->GetCancelable(); }
	virtual double GetTimeStamp() { return event->GetTimeStamp(); }
	virtual ES_Thread *GetThread() { return event->GetThread(); }

#ifdef DOM3_EVENTS
	// DOM_Event::SetStopPropagation can only fail if the event listener is
	// in a group other than the default, and external event listeners can't
	// be, so the OpStatus::Ignore is safe.
	virtual void StopPropagation() { OpStatus::Ignore(event->SetStopPropagation(TRUE, FALSE)); }
	virtual void StopImmediatePropagation() { OpStatus::Ignore(event->SetStopPropagation(TRUE, TRUE)); }
#else // DOM3_EVENTS
	virtual void StopPropagation() { event->SetStopPropagation(TRUE); }
	virtual void StopImmediatePropagation() { event->SetStopPropagation(TRUE); }
#endif // DOM3_EVENTS

	virtual void PreventDefault() { event->SetPreventDefault(TRUE); }

	virtual OP_STATUS Protect() { return protected_count++ > 0 || event->GetRuntime()->Protect(*event) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }
	virtual void Unprotect() { if (--protected_count == 0) event->GetRuntime()->Unprotect(*event); }
};

#endif // DOM2_EVENTS_API

#ifdef DOM3_EVENTS

class DOM_EventStoppedForGroup
{
public:
	ES_Object *group;
	BOOL effective;

	DOM_EventStoppedForGroup *next;
};

#endif // DOM3_EVENTS

HTML_Element *
DOM_Event::GetTargetElement()
{
	if (real_target)
		return ((DOM_Node *) real_target)->GetThisElement();
	else if (!target)
		return NULL;
	else if (target->IsA(DOM_TYPE_DOCUMENT) && ((DOM_Document *) target)->IsMainDocument())
		return ((DOM_Document *) target)->GetPlaceholderElement();
	else
		return ((DOM_Node *) target)->GetThisElement();
}

DOM_Event::DOM_Event()
	: known_type(DOM_EVENT_NONE),
	  type(NULL),
	  bubbles(0),
	  stop_propagation(STOP_PROPAGATION_NONE),
	  cancelable(0),
	  prevent_default(0),
	  event_return_value_set(0),
	  synthetic(0),
	  signal_document_finished(0),
	  window_event(0),
	  event_phase(ES_PHASE_CAPTURING),
	  timestamp(op_truncate(g_op_time_info->GetTimeUTC())),
#ifdef DOM2_EVENTS_API
	  external_event(NULL),
#endif // DOM2_EVENTS_API
	  dispatch_target(NULL),
	  target(NULL),
	  real_target(NULL),
	  current_target(NULL),
#ifdef DOM3_EVENTS
	  current_group(NULL),
	  stopped_for_group(NULL),
	  stop_propagation_later(0),
#endif // DOM3_EVENTS
	  thread(NULL),
	  bubbling_path(NULL),
	  bubbling_path_len(-1)
{
}

/* virtual */
DOM_Event::~DOM_Event()
{
	if (known_type == DOM_EVENT_CUSTOM)
		OP_DELETEA(type);

#ifdef DOM2_EVENTS_API
	OP_DELETE(external_event);
#endif // DOM2_EVENTS_API

#ifdef DOM3_EVENTS
	DOM_EventStoppedForGroup *stopped = stopped_for_group;
	while (stopped)
	{
		DOM_EventStoppedForGroup *next = stopped->next;
		OP_DELETE(stopped);
		stopped = next;
	}
#endif // DOM3_EVENTS

	DOMFreeValue(event_return_value);
}

void
DOM_Event::InitEvent(DOM_EventType new_type, DOM_Object *new_target, DOM_Object *new_real_target /* = NULL */, DOM_Object *new_dispatch_target /* = NULL */)
{
	OP_ASSERT(new_dispatch_target != new_target); // In this case a NULL dispatch_target should be used.
	OP_ASSERT(new_type != DOM_EVENT_NONE);

	known_type = new_type;
	if (new_type != DOM_EVENT_CUSTOM)
	{
		bubbles = DOM_EVENT_BUBBLES(new_type);
		cancelable = DOM_EVENT_IS_CANCELABLE(new_type);
	}
	target = new_target;
	real_target = new_real_target;
	dispatch_target = new_dispatch_target;

	/* A couple of events bubble to window when targetted at the document (but don't bubble when
	 * targetted at elements), see DOM_Event::IsDocumentEvent() */
	if (target->IsA(DOM_TYPE_DOCUMENT) && IsDocumentEvent(new_type))
		bubbles = TRUE;
}

#ifdef DOM3_EVENTS

void
DOM_Event::SetCurrentTarget(DOM_Object *new_current_target)
{
	current_target = new_current_target;

	if (stop_propagation_later)
		stop_propagation = TRUE;

	DOM_EventStoppedForGroup *stopped = stopped_for_group;
	while (stopped)
	{
		stopped->effective = TRUE;
		stopped = stopped->next;
	}
}

#endif // DOM3_EVENTS

OP_STATUS
DOM_Event::SetType(const uni_char *new_type)
{
	if (known_type == DOM_EVENT_NONE)
	{
		known_type = DOM_Event::GetEventType(new_type, FALSE);
		if (known_type == DOM_EVENT_NONE)
			known_type = DOM_EVENT_CUSTOM;
	}
	else
		OP_ASSERT(known_type == DOM_EVENT_CUSTOM);

	if (known_type == DOM_EVENT_CUSTOM)
		return UniSetStr(type, new_type);
	else
		return OpStatus::OK;
}

void
DOM_Event::SetBubblingPath(HTML_Element **path, int path_len)
{
	OP_ASSERT(path && path[0] && path_len > 0);
	bubbling_path = path;
	bubbling_path_len = path_len;

	if (bubbling_path)
		if (GetTargetElement() == bubbling_path[0])
		{
			--bubbling_path_len;
			++bubbling_path;
		}
}

/* virtual */ ES_GetState
DOM_Event::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_type:
		if (value)
		{
			const uni_char *result;

			if (known_type == DOM_EVENT_NONE)
				result = NULL;
			else if (known_type == DOM_EVENT_CUSTOM)
				result = type;
			else
			{
				TempBuffer *buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(DOM_EVENT_NAME(GetKnownType())));
				result = buffer->GetStorage();
			}

			DOMSetString(value, result);
		}
		return GET_SUCCESS;

	case OP_ATOM_srcElement:
		return DOMSetElementParent(value, target);

	case OP_ATOM_target:
		DOMSetObject(value, target);
        return GET_SUCCESS;

	case OP_ATOM_currentTarget:
		DOMSetObject(value, current_target);
        return GET_SUCCESS;

	case OP_ATOM_eventPhase:
		DOMSetNumber(value, target && target == current_target ? ES_PHASE_AT_TARGET : event_phase);
        return GET_SUCCESS;

	case OP_ATOM_bubbles:
		DOMSetBoolean(value, bubbles);
        return GET_SUCCESS;

    case OP_ATOM_cancelable:
		DOMSetBoolean(value, cancelable);
        return GET_SUCCESS;

	case OP_ATOM_returnValue:
		if (!event_return_value_set)
			return GET_FAILED;
		if (value)
			*value = event_return_value;
        return GET_SUCCESS;

	case OP_ATOM_cancelBubble:
		DOMSetBoolean(value, stop_propagation);
		return GET_SUCCESS;

	case OP_ATOM_timeStamp:
		DOMSetNumber(value, timestamp);
		return GET_SUCCESS;

	case OP_ATOM_defaultPrevented:
		DOMSetBoolean(value, prevent_default);
		return GET_SUCCESS;

	case OP_ATOM_isTrusted:
		DOMSetBoolean(value, !synthetic);
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_Event::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_cancelBubble:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;

#ifdef DOM3_EVENTS
		SetStopPropagation(value->value.boolean, FALSE);
#else // DOM3_EVENTS
		SetStopPropagation(value->value.boolean);
#endif // DOM3_EVENTS

		return PUT_SUCCESS;
	case OP_ATOM_returnValue:
	{
		DOMFreeValue(event_return_value);
		DOMCopyValue(event_return_value, *value);
		event_return_value_set = TRUE;
		BOOL ret_val_bool = TRUE;
		switch (value->type)
		{
		case VALUE_BOOLEAN:
			ret_val_bool = value->value.boolean;
			break;
		case VALUE_STRING:
			ret_val_bool = *value->value.string != '\0';
			break;
		case VALUE_NUMBER:
			ret_val_bool = !(value->value.number == 0.0 || op_isnan(value->value.number));
			break;
		case VALUE_OBJECT:
			ret_val_bool = TRUE;
			break;
		default:
			ret_val_bool = FALSE;
			break;
		}

		SetPreventDefault(!ret_val_bool);
		return PUT_SUCCESS;
	}
	default:
		if (GetName(property_name, NULL, origining_runtime) == GET_SUCCESS)
			return PUT_READ_ONLY;
		else
			return PUT_FAILED;
	}
}

/* virtual */ void
DOM_Event::GCTrace()
{
	GCMark(target);
	GCMark(real_target);
	GCMark(dispatch_target);
	GCMark(current_target);
	GCMark(event_return_value);

#ifdef DOM3_EVENTS
	DOM_EventStoppedForGroup *stopped = stopped_for_group;
	while (stopped)
	{
		GCMark(GetRuntime(), stopped->group);
		stopped = stopped->next;
	}
#endif // DOM3_EVENTS
}

/* virtual */ BOOL
DOM_Event::AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
{
	if (uni_str_eq(property_name, "isTrusted"))
		return FALSE;
	else
		return TRUE;
}

/* virtual */ OP_STATUS
DOM_Event::DefaultAction(BOOL cancelled)
{
	if (known_type != DOM_EVENT_NONE && known_type != DOM_EVENT_CUSTOM && target->IsA(DOM_TYPE_NODE))
		if (HTML_Element *target_element = GetTargetElement())
			if (FramesDocument *frames_doc = thread->GetScheduler()->GetFramesDocument())
			{
				HTML_Element::HandleEventOptions opts;
				opts.modifiers = GetModifiers();
				opts.thread = thread;
				opts.cancelled = cancelable && prevent_default;
				opts.synthetic = synthetic;
				target_element->HandleEvent(known_type, frames_doc, target_element, opts, NULL, bubbling_path, bubbling_path_len);
			}

	if (signal_document_finished)
		return GetEnvironment()->SignalDocumentFinished();

	return OpStatus::OK;
}

/* virtual */ ShiftKeyState
DOM_Event::GetModifiers()
{
	return SHIFTKEY_NONE;
}

DOM_EventType
DOM_Event::GetKnownType()
{
	if (known_type == ONMOUSEWHEELH || known_type == ONMOUSEWHEELV)
		return ONMOUSEWHEEL;
	else
		return known_type;
}

#ifdef DOM3_EVENTS

BOOL
DOM_Event::GetStopPropagation(ES_Object *group)
{
	if (group == NULL)
		return stop_propagation;
	else
	{
		DOM_EventStoppedForGroup *stopped = stopped_for_group;
		while (stopped)
			if (stopped->group == group)
				return stopped->effective;
			else
				stopped = stopped->next;
		return FALSE;
	}
}

OP_STATUS
DOM_Event::SetStopPropagation(BOOL value, BOOL immediate)
{
	if (current_group == NULL)
		if (immediate)
			stop_propagation = value;
		else
			stop_propagation_later = value;
	else if (value && !GetStopPropagation(current_group))
	{
		DOM_EventStoppedForGroup *stopped = OP_NEW(DOM_EventStoppedForGroup, ());
		if (!stopped)
			return OpStatus::ERR_NO_MEMORY;

		stopped->group = current_group;
		stopped->effective = immediate;
		stopped->next = stopped_for_group;
		stopped_for_group = stopped;
	}
#ifdef USER_JAVASCRIPT
	else
	{
		DOM_EventStoppedForGroup **stopped = &stopped_for_group;
		while (*stopped)
			if ((*stopped)->group == current_group)
			{
				DOM_EventStoppedForGroup *next = (*stopped)->next;
				OP_DELETE(*stopped);
				*stopped = next;
			}
	}
#endif // USER_JAVASCRIPT

	return OpStatus::OK;
}

#endif // DOM3_EVENTS

#ifdef DOM2_EVENTS_API

OP_STATUS
DOM_Event::GetExternalEvent(DOM_EventsAPI::Event *&event)
{
	if (!external_event && !(external_event = OP_NEW(DOM_ExternalEvent, (this))))
		return OpStatus::ERR_NO_MEMORY;

	event = external_event;
	return OpStatus::OK;
}

#endif // DOM2_EVENTS_API

/* static */ void
DOM_Event::FetchNamedHTMLElmEventPropertiesL(ES_PropertyEnumerator *enumerator, HTML_Element *elm)
{
	// List the properties this element has that are not directly
	// connected to an OP_ATOM so that they can be iterated over
	// and listed.

	OpString8 property_name; ANCHOR(OpString8, property_name);
	property_name.SetL("on");

	const DOM_EventData *eventdata = g_DOM_eventData;
	for (int index = 0; eventdata[index].name; ++index)
	{
		if (DOM_EVENT_AS_HTMLELM_PROPERTY(index) ||
			(elm->IsFormElement() &&
			DOM_EVENT_AS_HTMLFORMELM_PROPERTY(index)))
		{
			property_name.AppendL(eventdata[index].name);
			enumerator->AddPropertyL(property_name.CStr());
			property_name.CStr()[2] = 0;
		}
	}
}

/* static */ void
DOM_Event::FetchNamedHTMLDocEventPropertiesL(ES_PropertyEnumerator *enumerator)
{
	// List the properties this element has that are not directly
	// connected to an OP_ATOM so that they can be iterated over
	// and listed.

	OpString8 property_name; ANCHOR(OpString8, property_name);
	property_name.SetL("on");

	const DOM_EventData *eventdata = g_DOM_eventData;
	for (int index = 0; eventdata[index].name; ++index)
	{
		if (DOM_EVENT_AS_HTMLDOC_PROPERTY(index))
		{
			property_name.AppendL(eventdata[index].name);
			enumerator->AddPropertyL(property_name.CStr());
			property_name.CStr()[2] = 0;
		}
	}
}

/* static */ BOOL
DOM_Event::IsWindowEvent(DOM_EventType type)
{
	switch (type)
	{
	case ONLOAD:
	case ONUNLOAD:
	case ONFOCUS:
	case ONBLUR:
	case ONFOCUSIN:
	case ONFOCUSOUT:
	case ONERROR:
	case ONMESSAGE:
	case ONHASHCHANGE:
#ifdef CLIENTSIDE_STORAGE_SUPPORT
	case STORAGE:
#endif // CLIENTSIDE_STORAGE_SUPPORT
	case ONPOPSTATE:
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	case ONDEVICEORIENTATION:
	case ONCOMPASSNEEDSCALIBRATION:
	case ONDEVICEMOTION:
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
		return TRUE;
	case SEARCHSUGGESTS:
		return TRUE;

	default:
		return FALSE;
	}
}

/* static */ BOOL
DOM_Event::IsWindowEventAsBodyAttr(DOM_EventType type)
{
	return DOM_Event::IsWindowEvent(type) || type == ONSCROLL || type == ONRESIZE;
}

/* static */ BOOL
DOM_Event::IsDocumentEvent(DOM_EventType type)
{
	switch (type)
	{
	case DOMCONTENTLOADED:
	case ONRESIZE:
	case ONSCROLL:
		return TRUE;

	default:
		return FALSE;
	}
}

/* static */ BOOL
DOM_Event::IsAlwaysPresentAsProperty(DOM_Object *target, DOM_EventType type)
{
#ifdef TOUCH_EVENTS_SUPPORT
# ifdef PI_UIINFO_TOUCH_EVENTS
	if ((type == TOUCHSTART || type == TOUCHMOVE || type == TOUCHEND || type == TOUCHCANCEL) &&
		!g_op_ui_info->IsTouchEventSupportWanted())
		return FALSE;
# endif // PI_UIINFO_TOUCH_EVENTS
#endif // TOUCH_EVENTS_SUPPORT

	if (target->IsA(DOM_TYPE_ELEMENT))
	{
		if (HTML_Element* elm = static_cast<DOM_Node *>(target)->GetThisElement())
			return DOM_EVENT_AS_HTMLELM_PROPERTY(type) || (elm->IsFormElement() && DOM_EVENT_AS_HTMLFORMELM_PROPERTY(type));
	}
	else if (target->IsA(DOM_TYPE_HTML_DOCUMENT) && DOM_EVENT_AS_HTMLDOC_PROPERTY(type))
		return TRUE;
	else if (target->IsA(DOM_TYPE_SVG_DOCUMENT) && DOM_EVENT_AS_SVGDOC_PROPERTY(type))
		return TRUE;
	else if (target->IsA(DOM_TYPE_WINDOW) && DOM_EVENT_AS_HTMLWINDOW_PROPERTY(type))
		return TRUE;
	else if (target->IsA(DOM_TYPE_FILEREADER))
	{
		switch (type)
		{
		case ONLOAD:
#ifdef PROGRESS_EVENTS_SUPPORT
		case ONLOADSTART:
		case ONLOADEND:
		case ONPROGRESS:
#endif // PROGRESS_EVENTS_SUPPORT
		case ONABORT:
		case ONERROR:
			return TRUE;
		}
	}
#ifdef DOM_HTTP_SUPPORT
	else if (target->IsA(DOM_TYPE_XMLHTTPREQUEST) || target->IsA(DOM_TYPE_XMLHTTPREQUEST_UPLOAD))
	{
		switch (type)
		{
		case ONREADYSTATECHANGE:
			if (target->IsA(DOM_TYPE_XMLHTTPREQUEST_UPLOAD))
				return FALSE;
		case ONERROR:
		case ONLOAD:
#ifdef PROGRESS_EVENTS_SUPPORT
		case ONLOADSTART:
		case ONLOADEND:
		case ONPROGRESS:
		case ONTIMEOUT:
		case ONABORT:
#endif // PROGRESS_EVENTS_SUPPORT
			return TRUE;
		}
	}
#endif // DOM_HTTP_SUPPORT
#ifdef EVENT_SOURCE_SUPPORT
	else if (target->IsA(DOM_TYPE_EVENTSOURCE))
	{
		switch (type)
		{
		case ONERROR:
		case ONMESSAGE:
		case ONOPEN:
			return TRUE;
		}
	}
#endif // EVENT_SOURCE_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
	else if (target->IsA(DOM_TYPE_WEBSOCKET))
	{
		switch (type)
		{
		case ONOPEN:
		case ONERROR:
		case ONCLOSE:
		case ONMESSAGE:
			return TRUE;
		}
	}
#endif // WEBSOCKETS_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
	else if (target->IsA(DOM_TYPE_TEXTTRACK) && type == MEDIACUECHANGE)
		return TRUE;
	else if (target->IsA(DOM_TYPE_TEXTTRACKLIST) && (type == MEDIAADDTRACK || type == MEDIAREMOVETRACK))
		return TRUE;
	else if (target->IsA(DOM_TYPE_TEXTTRACKCUE) && (type == MEDIACUEENTER || type == MEDIACUEEXIT))
		return TRUE;
#endif // MEDIA_HTML_SUPPORT

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	else if (target->IsA(DOM_TYPE_EXTENSION_MENUCONTEXT))
	{
		switch (type)
		{
		case ONCLICK:
			return TRUE;
		}
	}
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

	return FALSE;
}

DOM_EventType
DOM_Event::GetEventType(const uni_char *type_string, BOOL as_property)
{
	size_t length = (int) uni_strlen(type_string);
	const DOM_EventData *eventdata = g_DOM_eventData;

	/* No known event type names are shorter than 4 or longer than 32 characters. */
	if (length >= 3 && length <= 32)
	{
		int index;

		if (as_property)
		{
			/* Use case-sensitive matching and ignore namespaces. */

			for (index = 0; eventdata[index].name; ++index)
				if (DOM_EVENT_IS_PROPERTYX(index, eventdata) && uni_str_eq(type_string, DOM_EVENT_NAMEX(index, eventdata)))
					return (DOM_EventType) index;
		}
		else
		{
			char name[33]; /* ARRAY OK 2008-02-28 jl */

			for (index = 0; index < (int) length; ++index)
			{
				if (type_string[index] >= 'A' && type_string[index] <= 'Z' ||
				    type_string[index] >= 'a' && type_string[index] <= 'z')
					name[index] = (char) op_toupper(type_string[index]);
				else
					return DOM_EVENT_NONE;
			}

			name[index] = 0;

			for (index = 0; eventdata[index].name; ++index)
				if (op_strlen(DOM_EVENT_NAMEX(index, eventdata)) == length && strni_eq(DOM_EVENT_NAMEX(index, eventdata), name, length))
					return (DOM_EventType) index;
		}
	}

	return DOM_EVENT_NONE;
}

/* static */
void
DOM_Event::ConstructEventObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "CAPTURING_PHASE", ES_PHASE_CAPTURING, runtime);
	PutNumericConstantL(object, "AT_TARGET", ES_PHASE_AT_TARGET, runtime);
	PutNumericConstantL(object, "BUBBLING_PHASE", ES_PHASE_BUBBLING, runtime);
}

/* static */ int
DOM_Event::preventDefault(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_EVENT, DOM_Event);
	if (event->GetCancelable())
		event->SetPreventDefault(TRUE);
	return ES_FAILED;
}

void
DOM_Event::SetStopPropagation(BOOL value)
{
	if (value)
		stop_propagation |= STOP_PROPAGATION_DEFERRED;
	else
		stop_propagation &= (~STOP_PROPAGATION_DEFERRED);
}

void
DOM_Event::SetStopImmediatePropagation(BOOL value)
{
	if (value)
		stop_propagation |= STOP_PROPAGATION_IMMEDIATE;
	else
		stop_propagation &= (~STOP_PROPAGATION_IMMEDIATE);
}

/* static */ int
DOM_Event::stopPropagation(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_EVENT, DOM_Event);

#ifdef DOM3_EVENTS
	CALL_FAILED_IF_ERROR(event->SetStopPropagation(TRUE, data == 1));
#else // DOM3_EVENTS
	if (data == 0)
		event->SetStopPropagation(TRUE);
	else
		event->SetStopImmediatePropagation(TRUE);
#endif // DOM3_EVENTS

	return ES_FAILED;
}

/* static */ int
DOM_Event::initEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_EVENT, DOM_Event);

	if (event->GetThread())
		return ES_FAILED;

	event->SetSynthetic();

	DOM_CHECK_ARGUMENTS("sbb");

	const uni_char *type = argv[0].value.string;

	event->known_type = DOM_Event::GetEventType(type, FALSE);
	event->bubbles = argv[1].value.boolean;
	event->cancelable = argv[2].value.boolean;

	if (event->known_type == DOM_EVENT_NONE)
	{
		event->known_type = DOM_EVENT_CUSTOM;
		CALL_FAILED_IF_ERROR(event->SetType(type));
	}

	return ES_FAILED;
}

ES_GetState DOM_Event::DOMSetElementParent(ES_Value *value, DOM_Object *obj)
{
	// Some methods should never return text nodes (MSIE methods)
	if (value)
	{
		if (obj && obj->IsA(DOM_TYPE_NODE))
		{
			DOM_Node *node = static_cast<DOM_Node *>(obj);
			if (HTML_Element* elm = node->GetThisElement())
			{
				while (elm->IsText() && elm->ParentActual())
					elm = elm->ParentActual();

				return node->DOMSetElement(value, elm);
			}
		}
		DOMSetObject(value, obj);
	}
	return GET_SUCCESS;
}

/* virtual */ int
DOM_Event_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_Event *event;

	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), GetRuntime(), GetPrototypeObject(), "Event"));
	CALL_FAILED_IF_ERROR(event->SetType(argv[0].value.string));

	event->SetSynthetic();
	event->SetEventPhase(ES_PHASE_AT_TARGET);

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));
	}

	DOMSetObject(return_value, event);
	return ES_VALUE;
}

/* static */ void
DOM_Event_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_Event_Constructor *event_constructor = OP_NEW_L(DOM_Event_Constructor, ());
	object->PutConstructorL(event_constructor, "s{bubbles:b,cancelable:b}-");
	DOM_Event::ConstructEventObjectL(*event_constructor, object->GetRuntime());
}

DOM_FUNCTIONS_START(DOM_Event)
	DOM_FUNCTIONS_FUNCTION(DOM_Event, DOM_Event::preventDefault, "preventDefault", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Event, DOM_Event::initEvent, "initEvent", "sbb-")
DOM_FUNCTIONS_END(DOM_Event)

DOM_FUNCTIONS_WITH_DATA_START(DOM_Event)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Event, DOM_Event::stopPropagation, 0, "stopPropagation", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Event, DOM_Event::stopPropagation, 1, "stopImmediatePropagation", 0)
DOM_FUNCTIONS_WITH_DATA_END(DOM_Event)

DOM_UIEvent::DOM_UIEvent()
	: view(NULL),
	  detail(0)
{
}

void
DOM_UIEvent::InitUIEvent(DOM_Object *new_view, int new_detail)
{
	view = new_view;
	detail = new_detail;
}

/* virtual */ ES_GetState
DOM_UIEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_view:
		DOMSetObject(value, view);
		return GET_SUCCESS;
	case OP_ATOM_detail:
		DOMSetNumber(value, detail);
		return GET_SUCCESS;
	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

/* static */ int
DOM_UIEvent::initUIEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_UIEVENT, DOM_UIEvent);
	DOM_CHECK_ARGUMENTS("sbbOn");

	ES_Value& view_argument = argv[3];
	if (view_argument.type == VALUE_OBJECT)
	{
		DOM_Object *view = DOM_VALUE2OBJECT(view_argument, DOM_Object);
		if (view && view->IsA(DOM_TYPE_WINDOW))
			event->view = view;
	}

	event->detail = TruncateDoubleToInt(argv[4].value.number);

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

DOM_FUNCTIONS_START(DOM_UIEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_UIEvent, DOM_UIEvent::initUIEvent, "initUIEvent", "sbb-n-")
DOM_FUNCTIONS_END(DOM_UIEvent)

/* virtual */ void
DOM_UIEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(view);
}

void
DOM_MouseEvent::TranslatePropertyName(OpAtom &property_name, BOOL& hide_text_elements)
{
	if (property_name == OP_ATOM_toElement)
	{
		if (known_type == ONMOUSEOVER || known_type == ONMOUSEENTER)
			property_name = OP_ATOM_target;
		else if (known_type == ONMOUSEOUT || known_type == ONMOUSELEAVE)
			property_name = OP_ATOM_relatedTarget;
		else
			property_name = OP_ATOM_UNASSIGNED;
		hide_text_elements = TRUE;
	}
	else if (property_name == OP_ATOM_fromElement)
	{
		if (known_type == ONMOUSEOVER || known_type == ONMOUSEENTER)
			property_name = OP_ATOM_relatedTarget;
		else if (known_type == ONMOUSEOUT || known_type == ONMOUSELEAVE)
			property_name = OP_ATOM_target;
		else
			property_name = OP_ATOM_UNASSIGNED;
		hide_text_elements = TRUE;
	}
}

DOM_MouseEvent::DOM_MouseEvent()
	: screen_x(0),
	  screen_y(0),
	  client_x(0),
	  client_y(0),
	  calculate_offset_lazily(FALSE),
	  offset_x(0),
	  offset_y(0),
	  ctrl_key(FALSE),
	  alt_key(FALSE),
	  shift_key(FALSE),
	  meta_key(FALSE),
	  button(0),
	  might_be_click(FALSE),
	  has_keyboard_origin(FALSE),
	  related_target(NULL)
{
}

void
DOM_MouseEvent::InitMouseEvent(long new_screen_x, long new_screen_y, long new_client_x, long new_client_y,
                               BOOL new_calculate_offset_lazily, long new_offset_x, long new_offset_y,
                               ShiftKeyState new_modifiers, int new_button, BOOL new_might_be_click,
                               BOOL new_has_keyboard_origin, DOM_Node *new_related_target)
{
	screen_x = new_screen_x;
	screen_y = new_screen_y;
	client_x = new_client_x;
	client_y = new_client_y;
	calculate_offset_lazily = new_calculate_offset_lazily;
	offset_x = new_offset_x;
	offset_y = new_offset_y;
	ctrl_key = (new_modifiers & SHIFTKEY_CTRL) != 0;
	alt_key = (new_modifiers & SHIFTKEY_ALT) != 0;
	shift_key = (new_modifiers & SHIFTKEY_SHIFT) != 0;
	meta_key = (new_modifiers & SHIFTKEY_META) != 0;
	button = new_button;
	might_be_click = new_might_be_click;
	related_target = new_related_target;
	has_keyboard_origin = new_has_keyboard_origin;
}

/* virtual */ ES_GetState
DOM_MouseEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	BOOL hide_text_elements = FALSE;
	TranslatePropertyName(property_name, hide_text_elements);

	switch (property_name)
	{
	case OP_ATOM_screenX:
		DOMSetNumber(value, screen_x);
		return GET_SUCCESS;

	case OP_ATOM_screenY:
		DOMSetNumber(value, screen_y);
		return GET_SUCCESS;

	case OP_ATOM_clientX:
		DOMSetNumber(value, client_x);
		return GET_SUCCESS;

	case OP_ATOM_clientY:
		DOMSetNumber(value, client_y);
		return GET_SUCCESS;

	case OP_ATOM_offsetX:
	case OP_ATOM_offsetY:
		if (value)
		{
			if (calculate_offset_lazily)
				GET_FAILED_IF_ERROR(CalculateOffset());
			DOMSetNumber(value, property_name == OP_ATOM_offsetX ? offset_x : offset_y);
		}
		return GET_SUCCESS;

	case OP_ATOM_ctrlKey:
		DOMSetBoolean(value, ctrl_key);
        return GET_SUCCESS;

	case OP_ATOM_shiftKey:
		DOMSetBoolean(value, shift_key);
        return GET_SUCCESS;

	case OP_ATOM_altKey:
		DOMSetBoolean(value, alt_key);
        return GET_SUCCESS;

    case OP_ATOM_metaKey:
	    DOMSetBoolean(value, meta_key);
        return GET_SUCCESS;

	case OP_ATOM_button:
		DOMSetNumber(value, button);
        return GET_SUCCESS;

	case OP_ATOM_which:
		DOMSetNumber(value, button + 1);
        return GET_SUCCESS;

	case OP_ATOM_target:
	case OP_ATOM_relatedTarget:
		{
			DOM_Object* t = property_name == OP_ATOM_relatedTarget ? related_target : target;
			if (hide_text_elements)
				return DOMSetElementParent(value, t);

			DOMSetObject(value, t);
		}
        return GET_SUCCESS;

	case OP_ATOM_y:
	case OP_ATOM_x:
		if (value)
		{
			int result = 0;

			if (target)
				if (HTML_Element *target_element = GetTargetElement())
				{
					int left;
					int top;

					BOOL relative_found;

					GET_FAILED_IF_ERROR(target_element->DOMGetDistanceToRelative(GetEnvironment(), left, top, relative_found));

					if (relative_found)
					{
						if (calculate_offset_lazily)
							GET_FAILED_IF_ERROR(CalculateOffset());
						left += offset_x;
						top += offset_y;
					}
					else
					{
						left = client_x;
						top = client_y;
					}

					if (property_name == OP_ATOM_x)
						result = left;
					else
						result = top;
				}

			DOMSetNumber(value, result);
		}
		return GET_SUCCESS;

	case OP_ATOM_pageX:
	case OP_ATOM_pageY:
		if (value)
		{
			int result = 0;

			if (FramesDocument *frames_doc = GetFramesDocument())
				if (property_name == OP_ATOM_pageX)
					result = client_x + frames_doc->DOMGetScrollOffset().x;
				else
					result = client_y + frames_doc->DOMGetScrollOffset().y;

			DOMSetNumber(value, result);
		}
		return GET_SUCCESS;

	case OP_ATOM_keyCode:
		DOMSetNumber(value, 0);
		return GET_SUCCESS;

	case OP_ATOM_wheelDelta:
	case OP_ATOM_wheelDeltaX:
	case OP_ATOM_wheelDeltaY:
		if (GetKnownType() == ONMOUSEWHEEL)
		{
			if (property_name == OP_ATOM_wheelDelta ||
				property_name == OP_ATOM_wheelDeltaX && known_type == ONMOUSEWHEELH ||
				property_name == OP_ATOM_wheelDeltaY && known_type == ONMOUSEWHEELV)
				/* Our API has delta inverted relative to what the spec expects. */
				DOMSetNumber(value, -40 * detail);
			else
				DOMSetNumber(value, 0);

			return GET_SUCCESS;
		}
		return GET_FAILED;

	default:
		return DOM_UIEvent::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_MouseEvent::GCTrace()
{
	DOM_UIEvent::GCTrace();
	GCMark(related_target);
}

/* virtual */ OP_STATUS
DOM_MouseEvent::DefaultAction(BOOL cancelled)
{
	if (known_type != DOM_EVENT_NONE && known_type != DOM_EVENT_CUSTOM)
		if (HTML_Element *target_element = GetTargetElement())
			if (FramesDocument *frames_doc = thread->GetScheduler()->GetFramesDocument())
			{
				HTML_Element *related_target_element = related_target ? related_target->GetThisElement() : NULL;

				int internal_button;

				if (GetKnownType() == ONMOUSEWHEEL)
					internal_button = detail;
				else if (button == 0)
					internal_button = MOUSE_BUTTON_1;
				else if (button == 2)
					internal_button = MOUSE_BUTTON_2;
				else
					internal_button = MOUSE_BUTTON_3;

				if (known_type == ONMOUSEDOWN || known_type == ONMOUSEUP ||
					known_type == ONCLICK || known_type == ONDBLCLICK)
					internal_button = MAKE_SEQUENCE_COUNT_CLICK_INDICATOR_AND_BUTTON(detail, might_be_click, internal_button);

				OpPoint scroll_offset = frames_doc->DOMGetScrollOffset();

				HTML_Element::HandleEventOptions opts;
				opts.related_target = related_target_element;
				opts.offset_x = calculate_offset_lazily ? 0 : offset_x;
				opts.offset_y = calculate_offset_lazily ? 0 : offset_y;
				opts.document_x = client_x + scroll_offset.x;
				opts.document_y = client_y + scroll_offset.y;
				opts.sequence_count_and_button_or_key_or_delta = internal_button;
				opts.modifiers = GetModifiers();
				opts.has_keyboard_origin = has_keyboard_origin;
				opts.thread = thread;
				opts.cancelled = cancelable && prevent_default;
				opts.synthetic = synthetic;
				target_element->HandleEvent(known_type, frames_doc, target_element, opts, NULL, bubbling_path, bubbling_path_len);
			}

	return OpStatus::OK;
}

OP_STATUS
DOM_MouseEvent::CalculateOffset()
{
	OP_ASSERT(calculate_offset_lazily);

	if (target)
		if (HTML_Element *target_element = GetTargetElement())
		{
			int left, top, width, height;
			RETURN_IF_ERROR(target_element->DOMGetPositionAndSize(GetEnvironment(), HTML_Element::DOM_PS_CONTENT, left, top, width, height));

			// If calculate_offset_lazily is TRUE, then we have stored
			// the page coordinate of the event in offset_x/y so that
			// we can calculate the rest later.
			int mouse_doc_x = offset_x;
			int mouse_doc_y = offset_y;

			offset_x = mouse_doc_x - left;
			offset_y = mouse_doc_y - top;
			calculate_offset_lazily = FALSE;

			return OpStatus::OK;
		}

	// Apparently we'll never be able to get the offset so just set them
	// to dummy values and give up.
	offset_x = 0;
	offset_y = 0;
	calculate_offset_lazily = FALSE;

	return OpStatus::OK;
}


/* virtual */ ShiftKeyState
DOM_MouseEvent::GetModifiers()
{
	return ((ctrl_key ? SHIFTKEY_CTRL : SHIFTKEY_NONE) |
			(alt_key ? SHIFTKEY_ALT : SHIFTKEY_NONE) |
			(shift_key ? SHIFTKEY_SHIFT : SHIFTKEY_NONE) |
			(meta_key ? SHIFTKEY_META : SHIFTKEY_NONE));
}

/* static */ int
DOM_MouseEvent::initMouseEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_MOUSEEVENT, DOM_MouseEvent);
	DOM_CHECK_ARGUMENTS("sbbOnnnnnbbbbnO");

	event->screen_x = TruncateDoubleToInt(argv[5].value.number);
	event->screen_y = TruncateDoubleToInt(argv[6].value.number);
	event->client_x = TruncateDoubleToInt(argv[7].value.number);
	event->client_y = TruncateDoubleToInt(argv[8].value.number);
	event->ctrl_key = argv[9].value.boolean;
	event->alt_key = argv[10].value.boolean;
	event->shift_key = argv[11].value.boolean;
	event->meta_key = argv[12].value.boolean;
	event->button = TruncateDoubleToInt(argv[13].value.number);

	DOM_ARGUMENT_OBJECT_EXISTING(event->related_target, 14, DOM_TYPE_NODE, DOM_Node);

	return initUIEvent(this_object, argv, argc, return_value, origining_runtime);
}

DOM_FUNCTIONS_START(DOM_MouseEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_MouseEvent, DOM_MouseEvent::initMouseEvent, "initMouseEvent", "sbb-nnnnnbbbbn-")
DOM_FUNCTIONS_END(DOM_MouseEvent)

DOM_KeyboardEvent::DOM_KeyboardEvent()
	: key_code(OP_KEY_FIRST),
	  key_value(NULL),
	  ctrl_key(FALSE),
	  alt_key(FALSE),
	  shift_key(FALSE),
	  meta_key(FALSE),
	  repeat(FALSE),
	  location(OpKey::LOCATION_STANDARD),
	  data(0),
	  key_event_data(NULL)
{
}

DOM_KeyboardEvent::~DOM_KeyboardEvent()
{
	OP_DELETEA(key_value);
	if (key_event_data)
		OpPlatformKeyEventData::DecRef(key_event_data);
}

OP_STATUS
DOM_KeyboardEvent::InitKeyboardEvent(OpKey::Code new_key_code, const uni_char *new_value, unsigned new_modifiers, BOOL new_repeat, OpKey::Location new_location, unsigned new_data, OpPlatformKeyEventData *platform_event_data)
{
	key_code = new_key_code;

	alt_key = (new_modifiers & SHIFTKEY_ALT) != 0;
	shift_key = (new_modifiers & SHIFTKEY_SHIFT) != 0;
#ifdef DOM_REVERSE_CTRL_AND_META_IN_DOM_EVENTS
	meta_key = (new_modifiers & SHIFTKEY_CTRL) != 0;
	ctrl_key = (new_modifiers & SHIFTKEY_META) != 0;
#else
	ctrl_key = (new_modifiers & SHIFTKEY_CTRL) != 0;
	meta_key = (new_modifiers & SHIFTKEY_META) != 0;
#endif
	repeat = new_repeat;
	location = new_location;
	data = new_data;
	if (new_value)
	{
		OP_DELETEA(key_value);
		key_value = UniSetNewStr(new_value);
		RETURN_OOM_IF_NULL(key_value);
	}
	if (platform_event_data)
		OpPlatformKeyEventData::IncRef(platform_event_data);

	key_event_data = platform_event_data;
	return OpStatus::OK;
}

/* static */
void
DOM_KeyboardEvent::ConstructKeyboardEventObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Event::ConstructEventObjectL(object, runtime);

	PutNumericConstantL(object, "DOM_KEY_LOCATION_STANDARD", 0, runtime);
	PutNumericConstantL(object, "DOM_KEY_LOCATION_LEFT", 1, runtime);
	PutNumericConstantL(object, "DOM_KEY_LOCATION_RIGHT", 2, runtime);
	PutNumericConstantL(object, "DOM_KEY_LOCATION_NUMPAD", 3, runtime);
	PutNumericConstantL(object, "DOM_KEY_LOCATION_MOBILE", 4, runtime);
	PutNumericConstantL(object, "DOM_KEY_LOCATION_JOYSTICK", 5, runtime);
}

unsigned
DOM_KeyboardEvent::TranslateKey(OpAtom property_name)
{
	if (synthetic)
		return key_code;

	if (known_type == ONKEYPRESS)
		if (unsigned c = OpKey::CharValue(key_code))
			return c;
		else if (key_value && key_value[0] && !key_value[1])
			return static_cast<unsigned>(key_value[0]);

	return (property_name == OP_ATOM_charCode ? 0 : key_code);
}

BOOL
DOM_KeyboardEvent::FiresKeypress()
{
	if (OpKey::IsFunctionKey(key_code) || OpKey::IsModifier(key_code))
		return FALSE;
	else if (key_code == OP_KEY_TAB && static_cast<DOM_KeyEventOrigin>(data) != DOM_KEYEVENT_DOCUMENTEDIT)
		return FALSE;
	else if (ctrl_key && alt_key)
		return (key_value && *key_value);

	return TRUE;
}

OP_STATUS
DOM_KeyboardEvent::GetCharValue(const uni_char *&result, TempBuffer *buffer)
{
	if (!IS_OP_KEY_MODIFIER(key_code) && !OpKey::IsInValueSet(key_code))
	{
		uni_char char_value = OpKey::CharValue(key_code);
		if (char_value != 0)
		{
			RETURN_IF_ERROR(buffer->Append(char_value));
			result = buffer->GetStorage();
			return OpStatus::OK;
		}
	}
	result = key_value;
	return OpStatus::OK;
}

const uni_char *
DOM_KeyboardEvent::GetKeyString()
{
	if (OpKey::IsInValueSet(key_code))
	{
		const uni_char *s = OpKey::ToString(key_code);
		if (s)
			return s;
	}
	return key_value;
}

/* virtual */ ES_GetState
DOM_KeyboardEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_which:
	case OP_ATOM_keyCode:
		DOMSetNumber(value, TranslateKey(property_name));
		return GET_SUCCESS;

	case OP_ATOM_charCode:
		if (known_type == ONKEYPRESS)
			DOMSetNumber(value, TranslateKey(property_name));
		else
			DOMSetNumber(value, 0);
		return GET_SUCCESS;

	case OP_ATOM_char:
		if (value)
		{
			const uni_char *string;
			TempBuffer *buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(GetCharValue(string, buffer));
			DOMSetString(value, string);
		}
		return GET_SUCCESS;

	case OP_ATOM_key:
		DOMSetString(value, GetKeyString());
		return GET_SUCCESS;

	case OP_ATOM_location:
		DOMSetNumber(value, known_type == ONKEYPRESS ? 0: (location != 0 ? location : OpKey::ToLocation(key_code)));
		return GET_SUCCESS;

	case OP_ATOM_ctrlKey:
		DOMSetBoolean(value, ctrl_key);
		return GET_SUCCESS;

	case OP_ATOM_shiftKey:
		DOMSetBoolean(value, shift_key);
		return GET_SUCCESS;

	case OP_ATOM_altKey:
		DOMSetBoolean(value, alt_key);
		return GET_SUCCESS;

	case OP_ATOM_metaKey:
		DOMSetBoolean(value, meta_key);
		return GET_SUCCESS;

	case OP_ATOM_repeat:
		DOMSetBoolean(value, repeat);
		return GET_SUCCESS;

	case OP_ATOM_locale:
		/* The spec doesn't state too convincing a case for this property; leave it as empty. */
		DOMSetString(value);
		return GET_SUCCESS;

	default:
		return DOM_UIEvent::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ OP_STATUS
DOM_KeyboardEvent::DefaultAction(BOOL cancelled)
{
	if (known_type != DOM_EVENT_NONE && known_type != DOM_EVENT_CUSTOM)
		if (HTML_Element *target_element = GetTargetElement())
			if (FramesDocument *frames_doc = thread->GetScheduler()->GetFramesDocument())
			{
				if (known_type == ONKEYDOWN && !synthetic && prevent_default)
					frames_doc->GetWindow()->SetRecentlyCancelledKeyDown(key_code);

				HTML_Element::HandleEventOptions opts;
				opts.sequence_count_and_button_or_key_or_delta = key_code;
				opts.modifiers = GetModifiers();
				opts.thread = thread;
				opts.cancelled = cancelable && prevent_default;
				opts.synthetic = synthetic;
				opts.user_data = const_cast<uni_char *>(key_value);
				opts.key_event_data = key_event_data;
				target_element->HandleEvent(known_type, frames_doc, target_element, opts, NULL, bubbling_path, bubbling_path_len);
			}

	return OpStatus::OK;
}

/* virtual */ ShiftKeyState
DOM_KeyboardEvent::GetModifiers()
{
	return ((alt_key ? SHIFTKEY_ALT : SHIFTKEY_NONE) |
			(shift_key ? SHIFTKEY_SHIFT : SHIFTKEY_NONE) |
#ifdef DOM_REVERSE_CTRL_AND_META_IN_DOM_EVENTS
			(meta_key ? SHIFTKEY_CTRL : SHIFTKEY_NONE) |
			(ctrl_key ? SHIFTKEY_META : SHIFTKEY_NONE)
#else
			(ctrl_key ? SHIFTKEY_CTRL : SHIFTKEY_NONE) |
			(meta_key ? SHIFTKEY_META : SHIFTKEY_NONE)
#endif
	);
}

/* static */ int
DOM_KeyboardEvent::getModifierState(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(key_event, DOM_TYPE_KEYBOARDEVENT, DOM_KeyboardEvent);
	DOM_CHECK_ARGUMENTS("s");
	const uni_char *modifier = argv[0].value.string;

	if (uni_str_eq(modifier, "Alt"))
		DOMSetBoolean(return_value, key_event->alt_key);
	else if (uni_str_eq(modifier, "Shift"))
		DOMSetBoolean(return_value, key_event->shift_key);
#ifdef DOM_REVERSE_CTRL_AND_META_IN_DOM_EVENTS
	else if (uni_str_eq(modifier, "Control"))
		DOMSetBoolean(return_value, key_event->meta_key);
	else if (uni_str_eq(modifier, "Meta"))
		DOMSetBoolean(return_value, key_event->ctrl_key);
#else
	else if (uni_str_eq(modifier, "Control"))
		DOMSetBoolean(return_value, key_event->ctrl_key);
	else if (uni_str_eq(modifier, "Meta"))
		DOMSetBoolean(return_value, key_event->meta_key);
#endif
	else if (uni_str_eq(modifier, "AltGraph"))
		DOMSetBoolean(return_value, key_event->alt_key && key_event->ctrl_key);
	else
		DOMSetBoolean(return_value, FALSE);

	return ES_VALUE;
}

/* static */ int
DOM_KeyboardEvent::initKeyboardEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_KEYBOARDEVENT);

	DOM_CHECK_ARGUMENTS("sbbOssnsbs");

	// We don't allow scripts to modify keyboard events right now. We might
	// allow it after a thorough security review of our key event handling.
	// For now, handle it like a generic UIEvent.

	ES_Value initUIargv[5]; // ARRAY OK bratell 2011-10-11.
	for (int i = 0; i < 4; i++)
		initUIargv[i] = argv[i];
	DOMSetNumber(initUIargv + 4, 0);
	return initUIEvent(this_object, initUIargv, 5, return_value, origining_runtime);
}

/* virtual */ int
DOM_KeyboardEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_KeyboardEvent *key_event;

	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(key_event = OP_NEW(DOM_KeyboardEvent, ()), GetRuntime(), GetPrototypeObject(), "KeyboardEvent"));
	CALL_FAILED_IF_ERROR(key_event->SetType(argv[0].value.string));

	key_event->SetSynthetic();
	key_event->SetEventPhase(ES_PHASE_AT_TARGET);

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		key_event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		key_event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));

		ES_Value value;
		CALL_FAILED_IF_ERROR(GetRuntime()->GetName(eventInitDict, UNI_L("detail"), &value));
		if (value.type == VALUE_NUMBER)
			key_event->detail = TruncateDoubleToInt(value.value.number);

		CALL_FAILED_IF_ERROR(GetRuntime()->GetName(eventInitDict, UNI_L("view"), &value));
		if (value.type == VALUE_OBJECT)
			if (DOM_Object *view = DOM_HOSTOBJECT(argv[0].value.object, DOM_Object))
				key_event->view = view;

		/* The following are not allowed initialized (see initKeyboardEvent() comment):

		   DOMString char;
		   DOMString key;
		   unsigned long location;
		   boolean ctrlKey;
		   boolean shiftKey;
		   boolean altKey;
		   boolean metaKey;
		   boolean repeat;
		   DOMString locale;
		 */
	}

	DOMSetObject(return_value, key_event);
	return ES_VALUE;
}

/* static */ void
DOM_KeyboardEvent_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_KeyboardEvent_Constructor *key_event_constructor = OP_NEW_L(DOM_KeyboardEvent_Constructor, ());
	object->PutConstructorL(key_event_constructor, "s{bubbles:b,cancelable:b,detail:n,view:o}-");
	DOM_KeyboardEvent::ConstructKeyboardEventObjectL(*key_event_constructor, object->GetRuntime());
}

#ifdef DOM_EVENT_VIRTUAL_KEY_CONSTANTS
/* virtual */ ES_GetState
DOM_KeyboardEvent_Constructor::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	/* Delay the creation of the block of DOM_VK_ constants. */
	if (uni_strncmp(property_name, "DOM_VK_", 7) == 0)
	{
		if (!added_constants)
		{
			added_constants = TRUE;
			TRAPD(status, AddKeyConstantsL(*this, static_cast<DOM_Runtime *>(origining_runtime)));
			GET_FAILED_IF_ERROR(status);
		}
		OP_BOOLEAN result;
		GET_FAILED_IF_ERROR(result = origining_runtime->GetName(*this, property_name, value));
		return result == OpBoolean::IS_TRUE ? GET_SUCCESS : GET_FAILED;
	}
	else
		return DOM_BuiltInConstructor::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_KeyboardEvent_Constructor::PutName(const uni_char* property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	/* Delay the creation of the block of DOM_VK_ constants. */
	if (uni_strncmp(property_name, "DOM_VK_", 7) == 0)
	{
		if (!added_constants)
		{
			added_constants = TRUE;
			TRAPD(status, AddKeyConstantsL(*this, static_cast<DOM_Runtime *>(origining_runtime)));
			PUT_FAILED_IF_ERROR(status);
		}
		PUT_FAILED_IF_ERROR(origining_runtime->PutName(*this, property_name, *value));
		/* Not correct, strictly speaking. The above PutName() does let the caller
		   observe if the [[Put]] was to a non-writable property. */
		return PUT_SUCCESS;
	}

	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_KeyboardEvent_Constructor::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	if (!added_constants)
	{
		added_constants = TRUE;
		AddKeyConstantsL(*this, static_cast<DOM_Runtime *>(origining_runtime));
	}
	return DOM_BuiltInConstructor::FetchPropertiesL(enumerator, origining_runtime);
}

#include "modules/dom/src/domevents/domevent_keys.cpp"

#endif // DOM_EVENT_VIRTUAL_KEY_CONSTANTS

DOM_FUNCTIONS_START(DOM_KeyboardEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_KeyboardEvent, DOM_KeyboardEvent::getModifierState, "getModifierState", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_KeyboardEvent, DOM_KeyboardEvent::initKeyboardEvent, "initKeyboardEvent", "sbb-ssnsbs-")
DOM_FUNCTIONS_END(DOM_KeyboardEvent)

DOM_TextEvent::DOM_TextEvent()
	: text_data(NULL),
	  input_method(DOM_INPUT_METHOD_UNKNOWN),
	  locale(NULL)
{
}

DOM_TextEvent::~DOM_TextEvent()
{
	OP_DELETEA(text_data);
	OP_DELETEA(locale);
}

/* static */
void
DOM_TextEvent::ConstructTextEventObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Event::ConstructEventObjectL(object, runtime);

	PutNumericConstantL(object, "DOM_INPUT_METHOD_UNKNOWN", DOM_INPUT_METHOD_UNKNOWN, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_KEYBOARD", DOM_INPUT_METHOD_KEYBOARD, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_PASTE", DOM_INPUT_METHOD_PASTE, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_DROP", DOM_INPUT_METHOD_DROP, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_IME", DOM_INPUT_METHOD_IME, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_OPTION", DOM_INPUT_METHOD_OPTION, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_HANDWRITING", DOM_INPUT_METHOD_HANDWRITING, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_VOICE", DOM_INPUT_METHOD_VOICE, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_MULTIMODAL", DOM_INPUT_METHOD_MULTIMODAL, runtime);
	PutNumericConstantL(object, "DOM_INPUT_METHOD_SCRIPT", DOM_INPUT_METHOD_SCRIPT, runtime);
}

OP_STATUS
DOM_TextEvent::InitTextEvent(const uni_char *new_text, InputMethod new_method, const uni_char *new_locale)
{
	if (new_text)
	{
		OP_DELETEA(text_data);
		text_data = UniSetNewStr(new_text);
		RETURN_OOM_IF_NULL(text_data);
	}
	if (new_locale)
	{
		OP_DELETEA(locale);
		locale = UniSetNewStr(new_locale);
		RETURN_OOM_IF_NULL(locale);
	}
	input_method = new_method;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_TextEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_data:
		DOMSetString(value, text_data);
		return GET_SUCCESS;
	case OP_ATOM_inputMethod:
		DOMSetNumber(value, input_method);
		return GET_SUCCESS;
	case OP_ATOM_locale:
		DOMSetString(value, locale);
		return GET_SUCCESS;
	default:
		return DOM_UIEvent::GetName(property_name, value, origining_runtime);
	}
}

/* static */ int
DOM_TextEvent::initTextEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_TEXTEVENT);

	DOM_CHECK_ARGUMENTS("sbbOsnS");

	/* Like for KeyboardEvent, do not allow

	   DOMString data;
	   unsigned long inputMethod;
	   DOMString locale;

	   to be set via scripts. */

	ES_Value initUIargv[5]; // ARRAY OK 2012-02-15 sof
	for (int i = 0; i < 4; i++)
		initUIargv[i] = argv[i];
	DOMSetNumber(initUIargv + 4, 0);
	return initUIEvent(this_object, initUIargv, 5, return_value, origining_runtime);
}

/* virtual */ int
DOM_TextEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_TextEvent *text_event;

	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(text_event = OP_NEW(DOM_TextEvent, ()), GetRuntime(), GetPrototypeObject(), "TextEvent"));
	CALL_FAILED_IF_ERROR(text_event->SetType(argv[0].value.string));

	text_event->SetSynthetic();
	text_event->SetEventPhase(ES_PHASE_AT_TARGET);

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		text_event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		text_event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));

		ES_Value value;
		CALL_FAILED_IF_ERROR(GetRuntime()->GetName(eventInitDict, UNI_L("detail"), &value));
		if (value.type == VALUE_NUMBER)
			text_event->detail = TruncateDoubleToInt(value.value.number);

		CALL_FAILED_IF_ERROR(GetRuntime()->GetName(eventInitDict, UNI_L("view"), &value));
		if (value.type == VALUE_OBJECT)
			if (DOM_Object *view = DOM_HOSTOBJECT(argv[0].value.object, DOM_Object))
				text_event->view = view;

		/* The following are not allowed initialized (see initKeyboardEvent() comment):

		   DOMString data;
		   unsigned input_method;
		   DOMString locale;
		 */
	}

	DOMSetObject(return_value, text_event);
	return ES_VALUE;
}

/* static */ void
DOM_TextEvent_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_TextEvent_Constructor *text_event_constructor = OP_NEW_L(DOM_TextEvent_Constructor, ());
	object->PutConstructorL(text_event_constructor, "s{bubbles:b,cancelable:b,detail:n,view:o}-");
	DOM_TextEvent::ConstructTextEventObjectL(*text_event_constructor, object->GetRuntime());
}

DOM_FUNCTIONS_START(DOM_TextEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_TextEvent, DOM_TextEvent::initTextEvent, "initTextEvent", "sbb-sns-")
DOM_FUNCTIONS_END(DOM_TextEvent)

OP_STATUS
DOM_ErrorEvent::InitErrorEvent(const uni_char* msg, const uni_char* res_url, unsigned int res_line)
{
	RETURN_IF_ERROR(message.Set(msg));
	RETURN_IF_ERROR(resource_url.Set(res_url));
	resource_line_number = res_line;
	resource_line.Empty();
	return resource_line.AppendFormat(UNI_L("%u"), res_line);
}

/* static */ int
DOM_ErrorEvent::initErrorEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_ERROREVENT, DOM_ErrorEvent);
	DOM_CHECK_ARGUMENTS("sbbssn");

	int status = initEvent(this_object, argv, 3, return_value, origining_runtime);
	if (status != ES_FAILED)
		return status;
	RETURN_IF_ERROR(event->InitErrorEvent(argv[3].value.string, argv[4].value.string, (unsigned int)argv[5].value.number));
	return ES_FAILED;
}

ES_GetState
DOM_ErrorEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_message:
		DOMSetString(value, message.CStr());
		return GET_SUCCESS;
	case OP_ATOM_filename:
		DOMSetString(value, resource_url.CStr());
		return GET_SUCCESS;
	case OP_ATOM_lineno:
		DOMSetNumber(value, (double)resource_line_number);
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

DOM_FUNCTIONS_START(DOM_ErrorEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_ErrorEvent, DOM_ErrorEvent::initErrorEvent, "initErrorEvent", "sbbssn-")
DOM_FUNCTIONS_END(DOM_ErrorEvent)

OP_STATUS
DOM_HashChangeEvent::Make(DOM_Event *&event,
		const uni_char *old_fragment,
		const uni_char *new_fragment,
		DOM_Runtime *runtime)
{
	DOM_HashChangeEvent *hc_event;

	RETURN_IF_ERROR(DOMSetObjectRuntime(hc_event = OP_NEW(DOM_HashChangeEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::HASHCHANGEEVENT_PROTOTYPE), "HashChangeEvent"));

	hc_event->fragments_are_urls = FALSE;
	RETURN_IF_ERROR(hc_event->old_fragment.Set(old_fragment));
	RETURN_IF_ERROR(hc_event->new_fragment.Set(new_fragment));

	event = hc_event;

	return OpStatus::OK;
}

/* virtual */
ES_GetState DOM_HashChangeEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_newURL || property_name == OP_ATOM_oldURL)
	{
		if (value)
		{
			OpStringC &fragment = property_name == OP_ATOM_newURL ? new_fragment : old_fragment;
			if (fragments_are_urls)
				DOMSetString(value, fragment.CStr());
			else if (FramesDocument *frames_doc = GetFramesDocument())
			{
				TempBuffer *buf = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buf->Append(frames_doc->GetURL().GetAttribute(URL::KUniName).CStr()));
				if (!fragment.IsEmpty())
				{
					GET_FAILED_IF_ERROR(buf->Append('#'));
					GET_FAILED_IF_ERROR(buf->Append(fragment.CStr()));
				}
				DOMSetString(value, buf);
			}
			else
				DOMSetString(value);
		}
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* static */ int
DOM_HashChangeEvent::initHashChangeEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_HASHCHANGEEVENT, DOM_HashChangeEvent);
	DOM_CHECK_ARGUMENTS("sbbss");

	int result = initEvent(this_object, argv, 3, return_value, origining_runtime);

	if (result != ES_FAILED)
		return result;

	event->fragments_are_urls = TRUE;
	CALL_FAILED_IF_ERROR(event->old_fragment.Set(argv[3].value.string));
	CALL_FAILED_IF_ERROR(event->new_fragment.Set(argv[4].value.string));

	return result;
}

DOM_FUNCTIONS_START(DOM_HashChangeEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_HashChangeEvent, DOM_HashChangeEvent::initHashChangeEvent, "initHashChangeEvent", "sbbss-")
DOM_FUNCTIONS_END(DOM_HashChangeEvent)

DOM_CustomEvent::~DOM_CustomEvent()
{
	DOMFreeValue(detail);
}

ES_GetState
DOM_CustomEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_detail)
	{
		if (value)
			*value = detail;
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

void
DOM_CustomEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(detail);
}

/* static */ int
DOM_CustomEvent::initCustomEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_CUSTOMEVENT, DOM_CustomEvent);
	DOM_CHECK_ARGUMENTS("sbb-");

	int result = initEvent(this_object, argv, 3, return_value, origining_runtime);

	if (result != ES_FAILED)
		return result;

	DOMFreeValue(event->detail);
	CALL_FAILED_IF_ERROR(DOMCopyValue(event->detail, argv[3]));

	return result;
}

/* virtual */ int
DOM_CustomEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_CustomEvent *event;

	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_CustomEvent, ()), GetRuntime(), GetPrototypeObject(), "CustomEvent"));
	CALL_FAILED_IF_ERROR(event->SetType(argv[0].value.string));

	event->SetSynthetic();
	event->SetEventPhase(ES_PHASE_AT_TARGET);

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));

		ES_Value value;

		CALL_FAILED_IF_ERROR(GetRuntime()->GetName(eventInitDict, UNI_L("detail"), &value));
		CALL_FAILED_IF_ERROR(DOMCopyValue(event->GetDetail(), value));
	}
	else
		event->GetDetail().type = VALUE_NULL;

	DOMSetObject(return_value, event);
	return ES_VALUE;
}

/* static */ void
DOM_CustomEvent_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_CustomEvent_Constructor *customevent_constructor = OP_NEW_L(DOM_CustomEvent_Constructor, ());
	object->PutConstructorL(customevent_constructor, "s{bubbles:b,cancelable:b,detail:-}-");
}

DOM_FUNCTIONS_START(DOM_CustomEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_CustomEvent, DOM_CustomEvent::initCustomEvent, "initCustomEvent", "sbb-")
DOM_FUNCTIONS_END(DOM_CustomEvent)

#ifdef WEBSOCKETS_SUPPORT

/* static */ OP_STATUS
DOM_CloseEvent::Make(DOM_CloseEvent *&event, DOM_Runtime *runtime, const uni_char *type)
{
	event = OP_NEW(DOM_CloseEvent, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::CLOSEEVENT_PROTOTYPE), "CloseEvent"));
	ES_Value argv[3];
	DOMSetString(&argv[0], type);
	DOMSetBoolean(&argv[1], DOM_EVENT_BUBBLES(ONCLOSE));
	DOMSetBoolean(&argv[2], DOM_EVENT_IS_CANCELABLE(ONCLOSE));
	int result = initEvent(event, argv, ARRAY_SIZE(argv), NULL, runtime);
	if (result == ES_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_CloseEvent::Make(DOM_CloseEvent *&event, DOM_Object *target, BOOL clean, unsigned short code, const uni_char *reason)
{
	event = OP_NEW(DOM_CloseEvent, ());
	DOM_Runtime *runtime = target->GetRuntime();
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::CLOSEEVENT_PROTOTYPE), "CloseEvent"));
	event->InitEvent(ONCLOSE, target);
	event->close_clean = clean;
	event->close_code = code;
	if (reason)
		RETURN_IF_ERROR(event->close_reason.Set(reason));

	return OpStatus::OK;
}

/* virtual */ BOOL
DOM_CloseEvent::IsA(int type)
{
	return type == DOM_TYPE_CLOSEEVENT || DOM_Event::IsA(type);
}

/* virtual */ ES_GetState
DOM_CloseEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_wasClean:
		DOMSetBoolean(value, close_clean);
		return GET_SUCCESS;
	case OP_ATOM_code:
		DOMSetNumber(value, close_code);
		return GET_SUCCESS;
	case OP_ATOM_reason:
		DOMSetString(value, close_reason.CStr());
		return GET_SUCCESS;
	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

/* static */ int
DOM_CloseEvent::initCloseEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_CLOSEEVENT, DOM_CloseEvent);
	DOM_CHECK_ARGUMENTS("sbbb");

	event->close_clean = argv[3].value.boolean;

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

/* virtual */ int
DOM_CloseEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_CloseEvent *close_event;

	CALL_FAILED_IF_ERROR(DOM_CloseEvent::Make(close_event, static_cast<DOM_Runtime *>(origining_runtime), argv[0].value.string));

	close_event->SetSynthetic();
	close_event->SetEventPhase(ES_PHASE_AT_TARGET);

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		close_event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		close_event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));
		close_event->close_clean = DOMGetDictionaryBoolean(eventInitDict, UNI_L("wasClean"), FALSE);
		close_event->close_code = TruncateDoubleToUShort(DOMGetDictionaryNumber(eventInitDict, UNI_L("code"), 0));

		const uni_char *reason = DOMGetDictionaryString(eventInitDict, UNI_L("reason"), NULL);
		if (reason)
			CALL_FAILED_IF_ERROR(close_event->close_reason.Set(reason));
	}

	DOMSetObject(return_value, close_event);
	return ES_VALUE;
}

/* static */ void
DOM_CloseEvent_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_CloseEvent_Constructor *close_event_constructor = OP_NEW_L(DOM_CloseEvent_Constructor, ());
	object->PutConstructorL(close_event_constructor, "s{bubbles:b,cancelable:b,wasClean:b,code:n,reason:s}-");
}

DOM_FUNCTIONS_START(DOM_CloseEvent)
 	DOM_FUNCTIONS_FUNCTION(DOM_CloseEvent, DOM_CloseEvent::initCloseEvent, "initCloseEvent", "sbbb")
DOM_FUNCTIONS_END(DOM_CloseEvent)

#endif //WEBSOCKETS_SUPPORT

/* static */ OP_STATUS
DOM_PopStateEvent::Make(DOM_Event *&event, DOM_Runtime *runtime)
{
	event = OP_NEW(DOM_PopStateEvent, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::POPSTATEEVENT_PROTOTYPE), "PopStateEvent"));
	if (runtime->GetFramesDocument())
	{
#ifdef ES_PERSISTENT_SUPPORT
		DocListElm* curr_doc_elm = runtime->GetFramesDocument()->GetDocManager()->CurrentDocListElm();
		OP_ASSERT(curr_doc_elm);
		if (curr_doc_elm->GetData())
		{
			ES_Value value;
			RETURN_IF_ERROR(runtime->CloneFromPersistent(curr_doc_elm->GetData(), value));
			RETURN_IF_ERROR(DOMCopyValue(static_cast<DOM_PopStateEvent *>(event)->state, value));
		}
		else
#endif // ES_PERSISTENT_SUPPORT
			DOMSetNull(&(static_cast<DOM_PopStateEvent *>(event)->state));

	}
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_PopStateEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_state)
	{
		if (value)
			*value = state;
		return GET_SUCCESS;
	}
	else
		return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* static */ int
DOM_PopStateEvent::initPopStateEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_POPSTATEEVENT, DOM_PopStateEvent);
	DOM_CHECK_ARGUMENTS("sbb-");

	DOMFreeValue(event->state);
	CALL_FAILED_IF_ERROR(DOMCopyValue(event->state, argv[3]));

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

DOM_FUNCTIONS_START(DOM_PopStateEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_PopStateEvent, DOM_PopStateEvent::initPopStateEvent, "initPopStateEvent", "sbb-")
DOM_FUNCTIONS_END(DOM_PopStateEvent)

#ifdef PAGED_MEDIA_SUPPORT

/* static */ OP_STATUS
DOM_PageEvent::Make(DOM_Event *&event, DOM_Runtime *runtime, unsigned int current_page, unsigned int page_count)
{
	DOM_PageEvent* page_event = OP_NEW(DOM_PageEvent, ());

	RETURN_IF_ERROR(DOMSetObjectRuntime(page_event, runtime, runtime->GetPrototype(DOM_Runtime::PAGEEVENT_PROTOTYPE), "PageEvent"));

	page_event->current_page = current_page;
	page_event->page_count = page_count;
	event = page_event;

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_PageEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_currentPage)
	{
		DOMSetNumber(value, current_page);
		return GET_SUCCESS;
	}
	else if (property_name == OP_ATOM_pageCount)
	{
		DOMSetNumber(value, page_count);
		return GET_SUCCESS;
	}
	else
		return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* static */ int
DOM_PageEvent::initPageEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_PAGEEVENT, DOM_PageEvent);

	if (event->GetThread())
		return ES_FAILED;

	DOM_CHECK_ARGUMENTS("sbbNN");

	event->current_page = TruncateDoubleToInt(argv[3].value.number);
	event->page_count = TruncateDoubleToInt(argv[4].value.number);

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

/* virtual */ int
DOM_PageEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("so");

	ES_Object *eventInitDict = argv[1].value.object;
	ES_Value current_page;
	ES_Value page_count;
	OP_BOOLEAN result;

	result = GetRuntime()->GetName(eventInitDict, UNI_L("currentPage"), &current_page);

	if (result == OpBoolean::IS_TRUE)
	{
		result = GetRuntime()->GetName(eventInitDict, UNI_L("pageCount"), &page_count);

		if (result == OpBoolean::IS_TRUE)
		{
			DOM_PageEvent *event;

			CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_PageEvent, (TruncateDoubleToUInt(current_page.value.number), TruncateDoubleToUInt(page_count.value.number))), GetRuntime(), GetPrototypeObject(), "PageEvent"));
			CALL_FAILED_IF_ERROR(event->SetType(argv[0].value.string));

			event->SetSynthetic();
			event->SetEventPhase(ES_PHASE_AT_TARGET);
			event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
			event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));

			DOMSetObject(return_value, event);
			return ES_VALUE;
		}
	}

	CALL_FAILED_IF_ERROR(result);

	return CallDOMException(DOM_Object::TYPE_MISMATCH_ERR, return_value);
}

/* static */ void
DOM_PageEvent_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_PageEvent_Constructor *pageevent_constructor = OP_NEW_L(DOM_PageEvent_Constructor, ());
	object->PutConstructorL(pageevent_constructor, "s{bubbles:b,cancelable:b,currentPage:n,pageCount:n}-");
}

DOM_FUNCTIONS_START(DOM_PageEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_PageEvent, DOM_PageEvent::initPageEvent, "initPageEvent", "sbbNN-")
DOM_FUNCTIONS_END(DOM_PageEvent)

#endif // PAGED_MEDIA_SUPPORT

#ifdef MEDIA_HTML_SUPPORT

/* virtual */ ES_GetState
DOM_TrackEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_track)
	{
		DOMSetObject(value, track);
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_TrackEvent::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_track)
		return PUT_READ_ONLY;

	return DOM_Event::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_TrackEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(track);
}

/* virtual */ int
DOM_TrackEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_TrackEvent *event;

	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_TrackEvent, ()), GetRuntime(), GetPrototypeObject(), "TrackEvent"));
	CALL_FAILED_IF_ERROR(event->SetType(argv[0].value.string));

	event->SetSynthetic();
	event->SetEventPhase(ES_PHASE_AT_TARGET);

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));

		ES_Value value;
		CALL_FAILED_IF_ERROR(GetRuntime()->GetName(eventInitDict, UNI_L("track"), &value));
		event->SetTrack(value.type == VALUE_OBJECT ? value.value.object : NULL);
	}

	DOMSetObject(return_value, event);
	return ES_VALUE;
}

/* static */ void
DOM_TrackEvent_Constructor::AddConstructorL(DOM_Object* object)
{
	object->PutConstructorL(OP_NEW_L(DOM_TrackEvent_Constructor, ()), "s{bubbles:b,cancelable:b,track:-}-");
}

#endif // MEDIA_HTML_SUPPORT

#ifdef DRAG_SUPPORT
/* static */ OP_STATUS
DOM_DragEvent::Make(DOM_Event *&event, DOM_Runtime *runtime, DOM_EventType event_type)
{
	OP_ASSERT(g_drag_manager->GetDragObject());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_DragEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::DRAGEVENT_PROTOTYPE), "DragEvent"));
	DOM_DataTransfer::DataStoreInfo data_store_info;
	data_store_info.type = DOM_DataTransfer::DataStoreType_Global;
	data_store_info.info.id = 0;
	data_store_info.drag_and_drops = TRUE;
	return DOM_DataTransfer::Make(static_cast<DOM_DragEvent*>(event)->m_data_transfer, runtime, event_type == ONDRAGSTART, data_store_info);
}

/* virtual */ ES_GetState
DOM_DragEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_dataTransfer:
		DOMSetObject(value, m_data_transfer);
		return GET_SUCCESS;
	}
	return DOM_MouseEvent::GetName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_DragEvent::GCTrace()
{
	DOM_MouseEvent::GCTrace();
	GCMark(m_data_transfer);
}

static OP_STATUS
SetDataTransferFromValue(DOM_DragEvent* event, const ES_Value& value, DOM_Runtime* origining_runtime)
{
	DOM_DataTransfer* data_transfer = NULL;
	if (value.type == VALUE_OBJECT)
	{
		DOM_Object* dom_object = DOM_VALUE2OBJECT(value, DOM_Object);

		if (dom_object && dom_object->IsA(DOM_TYPE_DATA_TRANSFER))
		{
			data_transfer = static_cast<DOM_DataTransfer*>(dom_object);
			if (data_transfer->OperatesOnGlobalDataStore())
				data_transfer->OverrideProtectionMode(DOM_DataTransfer::GetDataStoreMode(origining_runtime, TRUE));
		}
		else
		{
			OpDragObject* drag_object = NULL;
			RETURN_IF_ERROR(OpDragObject::Create(drag_object, OpTypedObject::DRAG_TYPE_HTML_ELEMENT));
			RETURN_OOM_IF_NULL(drag_object);
			OpAutoPtr<OpDragObject> auto_drag_object(drag_object);
			DOM_DataTransfer::DataStoreInfo data_store_info;
			data_store_info.type = DOM_DataTransfer::DataStoreType_Local;
			data_store_info.info.object = auto_drag_object.get();
			data_store_info.drag_and_drops = TRUE;
			RETURN_IF_ERROR(DOM_DataTransfer::Make(data_transfer, origining_runtime, event->GetKnownType() == ONDRAGSTART, data_store_info, value.value.object));
			auto_drag_object.release();
		}
	}

	event->SetDataTransfer(data_transfer);
	return OpStatus::OK;
}

/* static */ int
DOM_DragEvent::initDragEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_DRAGEVENT, DOM_DragEvent);

	DOM_CHECK_ARGUMENTS("sbbOnnnnnbbbbnOO");

	CALL_FAILED_IF_ERROR(SetDataTransferFromValue(event, argv[15], origining_runtime));

	return initMouseEvent(this_object, argv, argc, return_value, origining_runtime);
}

OpDragObject*
DOM_DragEvent::GetDataStore()
{
	return m_data_transfer ? m_data_transfer->GetDataStore() : NULL;
}

/* virtual */ int
DOM_DragEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_DragEvent *event;

	CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_DragEvent, ()), GetRuntime(), GetPrototypeObject(), "DragEvent"));
	CALL_FAILED_IF_ERROR(event->SetType(argv[0].value.string));

	event->SetSynthetic();
	event->SetEventPhase(ES_PHASE_AT_TARGET);

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));
		int detail = TruncateDoubleToInt(DOMGetDictionaryNumber(eventInitDict, UNI_L("detail"), 0));
		ES_Value value;

		DOM_Object* view = NULL;
		if (GetRuntime()->GetName(eventInitDict, UNI_L("view"), &value) == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
			view = DOM_VALUE2OBJECT(value, DOM_Object);
		event->InitUIEvent(view, detail);

		int screen_x = TruncateDoubleToInt(DOMGetDictionaryNumber(eventInitDict, UNI_L("screenX"), 0));
		int screen_y = TruncateDoubleToInt(DOMGetDictionaryNumber(eventInitDict, UNI_L("screenY"), 0));
		int client_x = TruncateDoubleToInt(DOMGetDictionaryNumber(eventInitDict, UNI_L("clientX"), 0));
		int client_y = TruncateDoubleToInt(DOMGetDictionaryNumber(eventInitDict, UNI_L("clientY"), 0));
		BOOL ctrl = DOMGetDictionaryBoolean(eventInitDict, UNI_L("ctrlKey"), FALSE);
		BOOL shift = DOMGetDictionaryBoolean(eventInitDict, UNI_L("shiftKey"), FALSE);
		BOOL alt = DOMGetDictionaryBoolean(eventInitDict, UNI_L("altKey"), FALSE);
		BOOL meta = DOMGetDictionaryBoolean(eventInitDict, UNI_L("metaKey"), FALSE);
		int button = TruncateDoubleToInt(DOMGetDictionaryNumber(eventInitDict, UNI_L("button"), 0));
		DOM_Node* related_target = NULL;
		if (GetRuntime()->GetName(eventInitDict, UNI_L("relatedTarget"), &value) == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		{
			DOM_Object* object = DOM_VALUE2OBJECT(value, DOM_Object);
			if (object->IsA(DOM_TYPE_NODE))
				related_target = static_cast<DOM_Node*>(object);
		}
		ShiftKeyState modifiers = (ctrl ? SHIFTKEY_CTRL : 0) | (shift ? SHIFTKEY_SHIFT : 0) |
		                          (alt ? SHIFTKEY_ALT : 0) | (meta ? SHIFTKEY_META : 0) | 0;
		event->InitMouseEvent(screen_x, screen_y, client_x, client_y, FALSE, 0, 0, modifiers, button, FALSE, FALSE, related_target);
		if (GetRuntime()->GetName(eventInitDict, UNI_L("dataTransfer"), &value) == OpBoolean::IS_TRUE)
			CALL_FAILED_IF_ERROR(SetDataTransferFromValue(event, value, GetRuntime()));
	}

	DOMSetObject(return_value, event);
	return ES_VALUE;
}

/* static */ void
DOM_DragEvent_Constructor::AddConstructorL(DOM_Object* object)
{
	if (!object)
		LEAVE(OpStatus::ERR);

	DOM_DragEvent_Constructor *drag_event_constructor = OP_NEW_L(DOM_DragEvent_Constructor, ());
	object->PutConstructorL(drag_event_constructor, "s{bubbles:b,cancelable:b,view:o,screenX:n,screenY:n,clientX:n,clientY:n,ctrlKey:b,shiftKey:b,altKey:b,metaKey:b,button:n,buttons:n,relatedTarget:o,dataTransfer:o}-");
}

DOM_FUNCTIONS_START(DOM_DragEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_DragEvent, DOM_DragEvent::initDragEvent, "initDragEvent", "sbb-nnnnnbbbbn-?(#DataTransfer|{dropEffect:s,effectAllowed:s})")
DOM_FUNCTIONS_END(DOM_DragEvent)
#endif // DRAG_SUPPORT

#ifdef USE_OP_CLIPBOARD
/* static */ OP_STATUS
DOM_ClipboardEvent::Make(DOM_Event *&event, DOM_Runtime *runtime, unsigned int id)
{
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_ClipboardEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::CLIPBOARDEVENT_PROTOTYPE), "ClipboardEvent"));
	static_cast<DOM_ClipboardEvent*>(event)->m_id = id;
	DOM_DataTransfer::DataStoreInfo data_store_info;
	data_store_info.type = DOM_DataTransfer::DataStoreType_Global;
	data_store_info.info.id = id;
	data_store_info.drag_and_drops = FALSE;
	return DOM_DataTransfer::Make(static_cast<DOM_ClipboardEvent*>(event)->m_clipboard_data, runtime, FALSE, data_store_info);
}

/* virtual */ ES_GetState
DOM_ClipboardEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_clipboardData:
		DOMSetObject(value, m_clipboard_data);
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_ClipboardEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(m_clipboard_data);
}

/* virtual */ OP_STATUS
DOM_ClipboardEvent::DefaultAction(BOOL cancelled)
{
	if (HTML_Element *target_element = GetTargetElement())
	{
		FramesDocument *frames_doc = thread->GetScheduler()->GetFramesDocument();

		// If there's no FramesDocument, no progress possible; quietly leave.
		// (DOM environments may legitimately be without a FramesDocument).
		if (!frames_doc)
			return OpStatus::OK;

		HTML_Element::HandleEventOptions opts;
		opts.modifiers = GetModifiers();
		opts.thread = thread;
		opts.cancelled = cancelable && prevent_default;
		opts.synthetic = synthetic;
		opts.id = m_id;
		target_element->HandleEvent(known_type, frames_doc, target_element, opts, NULL, bubbling_path, bubbling_path_len);
	}

	if (signal_document_finished)
		return GetEnvironment()->SignalDocumentFinished();
	else
		return OpStatus::OK;
}

static OP_STATUS CreateAndSetData(DOM_ClipboardEvent *event, DOM_Runtime *origining_runtime, const uni_char *type, const uni_char *data)
{
	DOM_DataTransfer *data_transfer = NULL;
	OpDragObject *drag_object = NULL;
	RETURN_IF_ERROR(OpDragObject::Create(drag_object, OpTypedObject::DRAG_TYPE_TEXT));
	OpAutoPtr<OpDragObject> auto_drag_object(drag_object);
	if (type && *type && data)
		RETURN_IF_ERROR(drag_object->SetData(data, type, FALSE, TRUE));
	DOM_DataTransfer::DataStoreInfo data_store_info;
	data_store_info.type = DOM_DataTransfer::DataStoreType_Local;
	data_store_info.info.object = auto_drag_object.get();
	data_store_info.drag_and_drops = FALSE;
	RETURN_IF_ERROR(DOM_DataTransfer::Make(data_transfer, origining_runtime, FALSE, data_store_info));
	auto_drag_object.release();
	event->SetData(data_transfer);

	return OpStatus::OK;
}

/* static */ int
DOM_ClipboardEvent::initClipboardEvent(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_CLIPBOARDEVENT, DOM_ClipboardEvent);

	DOM_CHECK_ARGUMENTS("sbbss");

	CALL_FAILED_IF_ERROR(CreateAndSetData(event, origining_runtime, argv[3].value.string, argv[4].value.string));

	// Pretend argc == 3 in order to not send additional data to initEvent().
	return initEvent(event, argv, 3, return_value, origining_runtime);
}

/* virtual */ int
DOM_ClipboardEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_ClipboardEvent *event;

	CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_ClipboardEvent, ()), GetRuntime(), GetPrototypeObject(), "ClipboardEvent"));
	CALL_FAILED_IF_ERROR(event->SetType(argv[0].value.string));

	event->SetSynthetic();
	event->SetEventPhase(ES_PHASE_AT_TARGET);

	const uni_char *type = NULL;
	const uni_char *data = NULL;
	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));
		type = DOMGetDictionaryString(eventInitDict, UNI_L("dataType"), NULL);
		data = DOMGetDictionaryString(eventInitDict, UNI_L("data"), NULL);
	}

	CALL_FAILED_IF_ERROR(CreateAndSetData(event, GetRuntime(), type, data));
	DOMSetObject(return_value, event);
	return ES_VALUE;
}

/* static */ void
DOM_ClipboardEvent_Constructor::AddConstructorL(DOM_Object *object)
{
	if (!object)
		LEAVE(OpStatus::ERR);

	DOM_ClipboardEvent_Constructor *clipboard_event_constructor = OP_NEW_L(DOM_ClipboardEvent_Constructor, ());
	object->PutConstructorL(clipboard_event_constructor, "s{bubbles:b,cancelable:b,dataType:s,data:s}-");
}

DOM_FUNCTIONS_START(DOM_ClipboardEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_ClipboardEvent, DOM_ClipboardEvent::initClipboardEvent, "initClipboardEvent", "sbbss-")
DOM_FUNCTIONS_END(DOM_ClipboardEvent)
#endif // USE_OP_CLIPBOARD
