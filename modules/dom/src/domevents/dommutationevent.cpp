/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM2_MUTATION_EVENTS

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domevents/dommutationevent.h"
#include "modules/dom/src/domevents/domeventdata.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/util/str.h"

/* static */ OP_STATUS
DOM_MutationEvent::CreateEvent(DOM_MutationEvent *&mutation_event, DOM_EnvironmentImpl *environment, DOM_EventType type)
{
	if (!environment->IsEnabled() || !environment->HasEventHandlers(type))
	{
		mutation_event = NULL;
		return OpStatus::OK;
	}
	else
		return DOM_MutationEvent::Make(mutation_event, environment, type);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendEvent(DOM_MutationEvent *mutation_event, DOM_EnvironmentImpl *environment, ES_Thread *interrupt_thread)
{
	if (mutation_event)
		RETURN_IF_ERROR(environment->SendEvent(mutation_event, interrupt_thread));

	return OpStatus::OK;
}

DOM_MutationEvent::DOM_MutationEvent()
	: attr_name(NULL),
	  attr_change(MODIFICATION),
	  prev_value(NULL),
	  new_value(NULL),
	  related_node(NULL)
{
}

DOM_MutationEvent::~DOM_MutationEvent()
{
	OP_DELETEA(prev_value);
	OP_DELETEA(new_value);
	OP_DELETEA(attr_name);
}

/* static */ OP_STATUS
DOM_MutationEvent::Make(DOM_MutationEvent *&mutation_event, DOM_EnvironmentImpl *environment, DOM_EventType type)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(mutation_event = OP_NEW(DOM_MutationEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::MUTATIONEVENT_PROTOTYPE), "MutationEvent"));

	mutation_event->known_type = type;
    mutation_event->bubbles = DOM_EVENT_BUBBLES(type);
    mutation_event->cancelable = DOM_EVENT_IS_CANCELABLE(type);

    return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_MutationEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_attrChange:
		DOMSetNumber(value, attr_change);
		return GET_SUCCESS;

	case OP_ATOM_attrName:
		DOMSetString(value, attr_name);
		return GET_SUCCESS;

	case OP_ATOM_newValue:
		DOMSetString(value, new_value);
		return GET_SUCCESS;

	case OP_ATOM_prevValue:
		DOMSetString(value, prev_value);
		return GET_SUCCESS;

	case OP_ATOM_relatedNode:
		DOMSetObject(value, related_node);
		return GET_SUCCESS;

	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_MutationEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(related_node);
}

/* static */
void
DOM_MutationEvent::ConstructMutationEventObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "MODIFICATION", MODIFICATION, runtime);
	DOM_Object::PutNumericConstantL(object, "ADDITION", ADDITION, runtime);
	DOM_Object::PutNumericConstantL(object, "REMOVAL", REMOVAL, runtime);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendSubtreeModified(ES_Thread *interrupt_thread, DOM_Node *target)
{
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	DOM_MutationEvent *mutation_event;
	RETURN_IF_ERROR(CreateEvent(mutation_event, environment, DOMSUBTREEMODIFIED));

	if (mutation_event)
		mutation_event->target = target;

	return SendEvent(mutation_event, environment, interrupt_thread);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendNodeInserted(ES_Thread *interrupt_thread, DOM_Node *target, DOM_Node *parent)
{
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	DOM_MutationEvent *mutation_event;
	RETURN_IF_ERROR(CreateEvent(mutation_event, environment, DOMNODEINSERTED));

	if (mutation_event)
	{
		mutation_event->target = target;
		mutation_event->related_node = parent;
	}

	return SendEvent(mutation_event, environment, interrupt_thread);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendNodeRemoved(ES_Thread *interrupt_thread, DOM_Node *target, DOM_Node *parent)
{
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	DOM_MutationEvent *mutation_event;
	RETURN_IF_ERROR(CreateEvent(mutation_event, environment, DOMNODEREMOVED));

	if (mutation_event)
	{
		mutation_event->target = target;
		mutation_event->related_node = parent;
	}

	return SendEvent(mutation_event, environment, interrupt_thread);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendNodeRemovedFromDocument(ES_Thread *interrupt_thread, DOM_Node *target)
{
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	DOM_MutationEvent *mutation_event;
	RETURN_IF_ERROR(CreateEvent(mutation_event, environment, DOMNODEREMOVEDFROMDOCUMENT));

	if (mutation_event)
		mutation_event->target = target;

	return SendEvent(mutation_event, environment, interrupt_thread);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendNodeInsertedIntoDocument(ES_Thread *interrupt_thread, DOM_Node *target)
{
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	DOM_MutationEvent *mutation_event;
	RETURN_IF_ERROR(CreateEvent(mutation_event, environment, DOMNODEINSERTEDINTODOCUMENT));

	if (mutation_event)
		mutation_event->target = target;

	return SendEvent(mutation_event, environment, interrupt_thread);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendAttrModified(ES_Thread *interrupt_thread, DOM_Node *target, DOM_Attr *attr, AttrChangeType type, const uni_char *attrName, const uni_char *prevValue, const uni_char *newValue)
{
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	DOM_MutationEvent *mutation_event;
	RETURN_IF_ERROR(CreateEvent(mutation_event, environment, DOMATTRMODIFIED));

	if (mutation_event)
	{
		mutation_event->target = target;
		mutation_event->attr_change = type;
		mutation_event->related_node = attr;

		if (!(mutation_event->attr_name = UniSetNewStr(attrName)) ||
			!(mutation_event->prev_value = UniSetNewStr(prevValue)) ||
			!(mutation_event->new_value = UniSetNewStr(newValue)))
			return OpStatus::ERR_NO_MEMORY;
	}

	return SendEvent(mutation_event, environment, interrupt_thread);
}

/* static */ OP_STATUS
DOM_MutationEvent::SendCharacterDataModified(ES_Thread *interrupt_thread, DOM_Node *target, const uni_char *prevValue, const uni_char *newValue)
{
	DOM_EnvironmentImpl *environment = target->GetEnvironment();

	DOM_MutationEvent *mutation_event;
	RETURN_IF_ERROR(CreateEvent(mutation_event, environment, DOMCHARACTERDATAMODIFIED));

	if (mutation_event)
	{
		mutation_event->target = target;

		if (!(mutation_event->prev_value = UniSetNewStr(prevValue)) ||
			!(mutation_event->new_value = UniSetNewStr(newValue)))
			return OpStatus::ERR_NO_MEMORY;
	}

	return SendEvent(mutation_event, environment, interrupt_thread);
}

/* virtual */ int
DOM_MutationEvent::initMutationEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_MUTATIONEVENT, DOM_MutationEvent);
	DOM_CHECK_ARGUMENTS("sbbOsssn");

	int result = initEvent(this_object, argv, argc, return_value, origining_runtime);
	if (result != ES_FAILED)
		return result;

	if (event->known_type < DOMSUBTREEMODIFIED || event->known_type > DOMCHARACTERDATAMODIFIED)
		return ES_FAILED;

	// If it's ok to set the related_node to NULL if the below isn't true, then
	// we can remove the check and leave it all to the macro.
	if (argv[3].type == VALUE_OBJECT)
		DOM_ARGUMENT_OBJECT_EXISTING(event->related_node, 3, DOM_TYPE_NODE, DOM_Node);

	int attrChange = TruncateDoubleToInt(argv[7].value.number);
	if (attrChange == ADDITION)
		event->attr_change = ADDITION;
	else if (attrChange == REMOVAL)
		event->attr_change = REMOVAL;
	else
		event->attr_change = MODIFICATION;

	if (!(event->prev_value = UniSetNewStr(argv[4].value.string)) ||
		!(event->new_value = UniSetNewStr(argv[5].value.string)) ||
		!(event->attr_name = UniSetNewStr(argv[6].value.string)))
	    return ES_NO_MEMORY;

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_MutationEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_MutationEvent, DOM_MutationEvent::initMutationEvent, "initMutationEvent", "sbb-sssn-")
DOM_FUNCTIONS_END(DOM_MutationEvent)

#endif // DOM2_MUTATION_EVENTS
