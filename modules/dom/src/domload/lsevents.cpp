/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domload/lsevents.h"
#include "modules/dom/src/domload/lsparser.h"

/* static */ OP_STATUS
DOM_LSLoadEvent::Make(DOM_LSLoadEvent *&event, DOM_LSParser *target, DOM_Document *document, ES_Object *input)
{
	DOM_Runtime *runtime = target->GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_LSLoadEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "LSLoadEvent"));
	event->InitEvent(ONLOAD, target);

	RETURN_IF_ERROR(event->PutPrivate(DOM_PRIVATE_document, *document));
	RETURN_IF_ERROR(event->PutPrivate(DOM_PRIVATE_input, input));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_LSLoadEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_newDocument:
		return DOMSetPrivate(value, DOM_PRIVATE_document);

	case OP_ATOM_input:
		return DOMSetPrivate(value, DOM_PRIVATE_input);

	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

DOM_LSProgressEvent::DOM_LSProgressEvent(unsigned position, unsigned total)
	: position(position),
	  total(total)
{
}

/* static */ OP_STATUS
DOM_LSProgressEvent::Make(DOM_LSProgressEvent *&event, DOM_LSParser *target, ES_Object *input, unsigned position, unsigned total)
{
	DOM_Runtime *runtime = target->GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_LSProgressEvent, (position, total)), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "LSProgressEvent"));
	event->InitEvent(DOM_EVENT_CUSTOM, target);

#ifdef DOM3_EVENTS
	RETURN_IF_ERROR(event->SetType(UNI_L("http://www.w3.org/2002/DOMLS"), UNI_L("progress")));
#else // DOM3_EVENTS
	RETURN_IF_ERROR(event->SetType(UNI_L("progress")));
#endif // DOM3_EVENTS

	RETURN_IF_ERROR(event->PutPrivate(DOM_PRIVATE_input, input));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_LSProgressEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_input:
		return DOMSetPrivate(value, DOM_PRIVATE_input);

	case OP_ATOM_position:
		DOMSetNumber(value, position);
		return GET_SUCCESS;

	case OP_ATOM_total:
		DOMSetNumber(value, total);
		return GET_SUCCESS;

	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

#endif // DOM3_LOAD
