/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef TOUCH_EVENTS_SUPPORT

#include "modules/dom/src/domevents/domtouchevent.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domevents/domeventthread.h"

#include "modules/doc/frm_doc.h"
#include "modules/display/vis_dev.h"

#define DEFAULT_RADIUS 64
#define MAGNITUDE(x, y) (op_pow(x, 2) + op_pow(y, 2))

DOM_Touch::DOM_Touch()
	: identifier(0)
	, target(NULL)
	, radius(0)
{
}

/* static */ OP_STATUS
DOM_Touch::Make(DOM_Touch *&touch, DOM_Object *new_target, int new_identifier, int new_client_x, int new_client_y,
                int new_offset_x, int new_offset_y, int new_page_x, int new_page_y, int new_screen_x, int new_screen_y, int new_radius, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(touch = OP_NEW(DOM_Touch, ()), runtime, runtime->GetPrototype(DOM_Runtime::TOUCH_PROTOTYPE), "Touch"));

	touch->target = new_target;
	touch->identifier = new_identifier;
	touch->client_x = new_client_x;
	touch->client_y = new_client_y;
	touch->page_x = new_page_x;
	touch->page_y = new_page_y;
	touch->screen_x = new_screen_x;
	touch->screen_y = new_screen_y;
	touch->radius = new_radius ? new_radius : DEFAULT_RADIUS;

	int compute_page = touch->page_x == -1 && touch->page_y == -1;
	int compute_client = touch->client_x == -1 && touch->client_y == -1;

	if (compute_page + compute_client == 1)
		if (FramesDocument *frames_doc = touch->GetFramesDocument())
		{
			OpPoint scroll_offset = frames_doc->DOMGetScrollOffset();
			if (compute_page)
			{
				touch->page_x = touch->client_x + scroll_offset.x;
				touch->page_y = touch->client_y + scroll_offset.y;
			}
			else
			{
				touch->client_x = touch->page_x - scroll_offset.x;
				touch->client_y = touch->page_y - scroll_offset.y;
			}
		}

	touch->start_offset_x = new_offset_x;
	touch->start_offset_y = new_offset_y;
	touch->start_client_x = touch->client_x;
	touch->start_client_y = touch->client_y;

	touch->tap = TRUE;
	touch->prevented = FALSE;

	return OpStatus::OK;
}

void
DOM_Touch::UpdateTapStatus(BOOL had_tap)
{
#ifdef DOC_TOUCH_SMARTPHONE_COMPATIBILITY
	if (!had_tap || MAGNITUDE(client_x - start_client_x, client_y - start_client_y) > radius)
		tap = FALSE;
#endif // DOC_TOUCH_SMARTPHONE_COMPATIBILITY
}

/* virtual */ ES_GetState
DOM_Touch::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_clientX:
		DOMSetNumber(value, client_x);
		return GET_SUCCESS;

	case OP_ATOM_clientY:
		DOMSetNumber(value, client_y);
		return GET_SUCCESS;

	case OP_ATOM_identifier:
		DOMSetNumber(value, identifier);
		return GET_SUCCESS;

	case OP_ATOM_pageX:
		DOMSetNumber(value, page_x);
		return GET_SUCCESS;

	case OP_ATOM_pageY:
		DOMSetNumber(value, page_y);
		return GET_SUCCESS;

	case OP_ATOM_screenX:
		DOMSetNumber(value, screen_x);
		return GET_SUCCESS;

	case OP_ATOM_screenY:
		DOMSetNumber(value, screen_y);
		return GET_SUCCESS;

	case OP_ATOM_target:
		DOMSetObject(value, target);
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_Touch::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_clientX:
	case OP_ATOM_clientY:
	case OP_ATOM_identifier:
	case OP_ATOM_pageX:
	case OP_ATOM_pageY:
	case OP_ATOM_screenX:
	case OP_ATOM_screenY:
	case OP_ATOM_target:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	};
}

HTML_Element *
DOM_Touch::GetTargetElement()
{
	if (target)
		return static_cast<DOM_Node *>(target)->GetThisElement();

	return NULL;
}

/* virtual */ void
DOM_Touch::GCTrace()
{
	GCMark(target);
}

/* static */ int
DOM_Touch::createTouch(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_DOCUMENT);
	DOM_CHECK_ARGUMENTS("OOnnnnn");

	DOM_Node *target;
	DOM_ARGUMENT_OBJECT_EXISTING(target, 1, DOM_TYPE_NODE, DOM_Node);
	int identifier = TruncateDoubleToInt(argv[2].value.number);
	int page_x = TruncateDoubleToInt(argv[3].value.number);
	int page_y = TruncateDoubleToInt(argv[4].value.number);
	int screen_x = TruncateDoubleToInt(argv[5].value.number);
	int screen_y = TruncateDoubleToInt(argv[6].value.number);

	DOM_Touch *touch;
	CALL_FAILED_IF_ERROR(Make(touch, target, identifier, -1, -1, 0, 0, page_x, page_y, screen_x, screen_y, 0, origining_runtime));

	DOMSetObject(return_value, touch);
	return ES_VALUE;
}

/* static */ OP_STATUS
DOM_TouchList::Make(DOM_TouchList *&list, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(list = OP_NEW(DOM_TouchList, ()), runtime, runtime->GetPrototype(DOM_Runtime::TOUCHLIST_PROTOTYPE), "TouchList"));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_TouchList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, touches.GetCount());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_TouchList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_TouchList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0 && static_cast<unsigned>(property_index) < touches.GetCount())
	{
		DOMSetObject(value, touches.Get(property_index));
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_TouchList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	/*
	 * This list is essentially read-only, but for compatibility reasons we pretend
	 * to allow changes to our known touch elements, and allow writes to indices
	 * beyond those. We never change our length property.
	 */
	if (property_index >= 0 && static_cast<unsigned>(property_index) < touches.GetCount())
		return PUT_READ_ONLY;

	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_TouchList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = touches.GetCount();
	return GET_SUCCESS;
}

/* virtual */ void
DOM_TouchList::GCTrace()
{
	for (unsigned i = 0; i < touches.GetCount(); i++)
		GCMark(touches.Get(i));
}

/* static */ int
DOM_TouchList::getItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(list, DOM_TYPE_TOUCHLIST, DOM_TouchList);

	unsigned index = TruncateDoubleToUInt(argv[0].value.number);
	if (list->GetIndex(index, return_value, origining_runtime) == GET_FAILED)
			DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
DOM_TouchList::getIdentifier(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(list, DOM_TYPE_TOUCHLIST, DOM_TouchList);

	int identifier = TruncateDoubleToInt(argv[0].value.number);
	for (unsigned i = 0; i < list->GetCount(); i++)
	{
		DOM_Touch *touch = list->Get(i);
		if (touch->GetIdentifier() == identifier)
		{
			DOMSetObject(return_value, touch);
			return ES_VALUE;
		}
	}

	DOMSetNull(return_value);
	return ES_VALUE;
}

/* static */ int
DOM_TouchList::createTouchList(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("|o");

	DOM_TouchList *list;
	CALL_FAILED_IF_ERROR(Make(list, origining_runtime));

	/* Note: an extension to allow createTouchList() to return
	   the empty TouchList; i.e. same as createTouchList([]) but
	   also compatible with earlier implementation. */
	if (argc > 0)
	{
		DOM_HOSTOBJECT_SAFE(touch, argv[0].value.object, DOM_TYPE_TOUCH, DOM_Touch);
		if (touch)
			CALL_FAILED_IF_ERROR(list->Add(touch));
		else
		{
			ES_Value value;
			ES_Object *arg_array = argv[0].value.object;
			unsigned length = 0;
			if (!document->DOMGetArrayLength(arg_array, length))
				return DOM_CALL_NATIVEEXCEPTION(ES_Native_TypeError, UNI_L("Expected array of Touch objects"));

			for (unsigned i = 0; i < length; i++)
			{
				CALL_FAILED_IF_ERROR(origining_runtime->GetIndex(arg_array, i, &value));
				BOOL valid = value.type == VALUE_OBJECT;

				if (valid)
				{
					DOM_HOSTOBJECT_SAFE(arg, value.value.object, DOM_TYPE_TOUCH, DOM_Touch);
					if (arg)
						CALL_FAILED_IF_ERROR(list->Add(arg));
					else
						valid = FALSE;
				}
				if (!valid)
					return DOM_CALL_NATIVEEXCEPTION(ES_Native_TypeError, UNI_L("Expected array of only Touch objects"));
			}
		}
	}
	DOMSetObject(return_value, list);
	return ES_VALUE;
}

DOM_TouchEvent::DOM_TouchEvent()
	: alt_key(FALSE)
	, ctrl_key(FALSE)
	, meta_key(FALSE)
	, shift_key(FALSE)
	, touches(NULL)
	, target_touches(NULL)
	, changed_touches(NULL)
	, scale(1.0)
	, rotation(0.0)
	, user_data(NULL)
{
}

/* virtual */
DOM_TouchEvent::~DOM_TouchEvent()
{
	changed_touches = NULL;
}

/* virtual */ ES_GetState
DOM_TouchEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_altKey:
		DOMSetBoolean(value, alt_key);
		return GET_SUCCESS;

	case OP_ATOM_ctrlKey:
		DOMSetBoolean(value, ctrl_key);
		return GET_SUCCESS;

	case OP_ATOM_metaKey:
		DOMSetBoolean(value, meta_key);
		return GET_SUCCESS;

	case OP_ATOM_shiftKey:
		DOMSetBoolean(value, shift_key);
		return GET_SUCCESS;

	case OP_ATOM_touches:
		DOMSetObject(value, touches);
		return GET_SUCCESS;

	case OP_ATOM_targetTouches:
		DOMSetObject(value, target_touches);
		return GET_SUCCESS;

	case OP_ATOM_changedTouches:
		DOMSetObject(value, changed_touches);
		return GET_SUCCESS;

	case OP_ATOM_scale:
		DOMSetNumber(value, scale);
		return GET_SUCCESS;

	case OP_ATOM_rotation:
		DOMSetNumber(value, rotation);
			return GET_SUCCESS;

	default:
		return DOM_UIEvent::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_TouchEvent::GCTrace()
{
	DOM_UIEvent::GCTrace();
	GCMark(touches);
	GCMark(target_touches);
	GCMark(changed_touches);
}

/* static */ int
DOM_TouchEvent::initTouchEvent(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_TOUCHEVENT, DOM_TouchEvent);
	DOM_CHECK_ARGUMENTS("sbbOnnnnnbbbbOOOnn");

#define OBJECT_ARGUMENT(host_object_type, index) \
	(argv[index].type != VALUE_NULL && argv[index].type != VALUE_UNDEFINED) ? \
		static_cast<host_object_type *>(DOM_GetHostObject(argv[index].value.object)) \
	: \
		NULL;

	/*
	 * Whether initTouchEvent or initTouchEventNS was called is expressed by data being 0 or 1.
	 *
	 * The initial data+5 arguments ([ns,] type, canBubble, cancelable, view, detail) are handled in initUIEvent.
	 *
	 * The four arguments that follow them (screenX, screenY, clientX, clientY) are part of the documented
	 * signature (see link in .h), but have no place to live in the TouchEvent object structure, so we ignore them.
	 *
	 * The remaining arguments are interpreted as follows.
	 */
	event->ctrl_key = argv[9].value.boolean;
	event->alt_key = argv[10].value.boolean;
	event->shift_key = argv[11].value.boolean;
	event->meta_key = argv[12].value.boolean;
	event->touches = OBJECT_ARGUMENT(DOM_TouchList, 13);
	event->target_touches = OBJECT_ARGUMENT(DOM_TouchList, 14);
	event->changed_touches = OBJECT_ARGUMENT(DOM_TouchList, 15);
	event->scale = argv[16].value.number;
	event->rotation = argv[17].value.number;

	return initUIEvent(this_object, argv, 5, return_value, origining_runtime);
}

OP_STATUS
DOM_TouchEvent::InitTouchEvent(int client_x, int client_y, int new_offset_x, int new_offset_y, int screen_x, int screen_y,
							   ShiftKeyState new_modifiers, int identifier, DOM_Object *target,
							   DOM_EventType type, int radius, DOM_TouchList *active_touches, DOM_Runtime *runtime,
							   void* new_user_data /* = NULL */)
{
	OP_ASSERT(active_touches && "Please submit a list of touches active in the current environment.");

	offset_x = new_offset_x;
	offset_y = new_offset_y;
	shift_key = (new_modifiers & SHIFTKEY_SHIFT) != 0;
	ctrl_key = (new_modifiers & SHIFTKEY_CTRL) != 0;
	alt_key = (new_modifiers & SHIFTKEY_ALT) != 0;
	user_data = new_user_data;

	/* Create new touch object. */
	DOM_Touch* touch = NULL;
	RETURN_IF_ERROR(DOM_Touch::Make(touch, target, identifier, client_x, client_y, offset_x, offset_y, -1, -1, screen_x, screen_y, radius, runtime));

	/* Take a snapshot of the list of active touches. */
	RETURN_IF_ERROR(DOM_TouchList::Make(touches, runtime));
	DOM_Touch *existing_touch = NULL;
	for (int i = active_touches->GetCount() - 1; i >= 0; i--)
	{
		DOM_Touch *the_touch = active_touches->Get(i);
		if (the_touch->GetIdentifier() == identifier)
		{
			OP_ASSERT(!existing_touch && "The list of active touches contains two touch objects with the same identifier.");
			existing_touch = the_touch;

			/* If this event signals the touch leaving the surface, it should not be part of active_touches or touches. */
			if (type == TOUCHEND || type == TOUCHCANCEL)
			{
				active_touches->Remove(i);
				continue;
			}
			else if (type == TOUCHSTART)
				OP_ASSERT(!"The list of active touches already contains a touchstart object with this identifier.");

			RETURN_IF_ERROR(touches->Add(touch));
			RETURN_IF_ERROR(active_touches->Replace(i, touch));
			touch->UpdateTapStatus(existing_touch->GetWithinTapRadius());
		}
		else
			RETURN_IF_ERROR(touches->Add(the_touch));
	}

	OP_ASSERT(type == TOUCHSTART || existing_touch && target == existing_touch->GetTarget() && identifier == existing_touch->GetIdentifier() && "A touch may not change characteristics.");

	/* Add the initial touchstart. */
	if (type == TOUCHSTART)
	{
		RETURN_IF_ERROR(touches->Add(touch));
		RETURN_IF_ERROR(active_touches->Add(touch));
	}

	/* Mark touch as changed. */
	if (!changed_touches)
		RETURN_IF_ERROR(DOM_TouchList::Make(changed_touches, runtime));
	RETURN_IF_ERROR(changed_touches->Add(touch));

	/* Re-create target_touches as a subset of our snapshot of active_touches. */
	RETURN_IF_ERROR(DOM_TouchList::Make(target_touches, runtime));
	for (unsigned i = 0; i < touches->GetCount(); i++)
		if (touches->Get(i)->GetTarget() == target)
			RETURN_IF_ERROR(target_touches->Add(touches->Get(i)));

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_TouchEvent::DefaultAction(BOOL cancelled)
{
	if (changed_touches && changed_touches->GetCount() > 0)
		if (HTML_Element *target_element = GetTargetElement())
		{
			FramesDocument *frames_doc = thread->GetScheduler()->GetFramesDocument();

			for (unsigned i = 0; i < changed_touches->GetCount(); i++)
			{
				DOM_Touch* touch = changed_touches->Get(i);
				BOOL prevented = touch->prevented || cancelable && prevent_default;

				/* A touch is prevented by preventing the touchstart event introducing it. */
				if (known_type == TOUCHSTART)
					touch->prevented = prevented;

				int off_x = offset_x;
				int off_y = offset_y;
				int client_x = touch->client_x;
				int client_y = touch->client_y;

#ifdef DOC_TOUCH_SMARTPHONE_COMPATIBILITY
				if (known_type == TOUCHEND)
				{
					/* Do not generate mouse events for swipes in smartphone mode. */
					if (!touch->tap)
						prevented = TRUE;

					if (!prevented)
					{
						/* Use the touchstart event's coordinates for generated mouse events. */
						off_x = touch->start_offset_x;
						off_y = touch->start_offset_y;
						client_x = touch->start_client_x;
						client_y = touch->start_client_y;
					}
				}
#endif // DOC_TOUCH_SMARTPHONE_COMPATIBILITY

				OpRect visual_vp = frames_doc->GetVisualViewport();

				HTML_Element::HandleEventOptions opts;
				opts.offset_x = off_x;
				opts.offset_y = off_y;
				opts.document_x = client_x + visual_vp.Left();
				opts.document_y = client_y + visual_vp.Top();
				opts.sequence_count_and_button_or_key_or_delta = touch->identifier;
				opts.modifiers = GetModifiers();
				opts.radius = touch->radius;
				opts.user_data = user_data;
				opts.thread = thread;
				opts.cancelled = prevented;
				opts.synthetic = synthetic;
				target_element->HandleEvent(known_type, frames_doc,target_element, opts, NULL, bubbling_path, bubbling_path_len);
			}
		}

	return OpStatus::OK;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_TouchList)
DOM_FUNCTIONS_FUNCTION(DOM_TouchList, DOM_TouchList::getItem, "item", "n-")
DOM_FUNCTIONS_FUNCTION(DOM_TouchList, DOM_TouchList::getIdentifier, "identifiedTouch", "n-")
DOM_FUNCTIONS_END(DOM_TouchList)

DOM_FUNCTIONS_START(DOM_TouchEvent)
DOM_FUNCTIONS_FUNCTION(DOM_TouchEvent, DOM_TouchEvent::initTouchEvent, "initTouchEvent", "sbbOnnnnnbbbbOOOnn-")
DOM_FUNCTIONS_END(DOM_TouchEvent)

#endif // TOUCH_EVENTS_SUPPORT
