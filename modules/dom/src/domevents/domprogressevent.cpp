/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef PROGRESS_EVENTS_SUPPORT

#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domevents/domprogressevent.h"

/* static */ OP_STATUS
DOM_ProgressEvent::Make(DOM_ProgressEvent *&progress_event, DOM_Runtime *runtime)
{
	progress_event = OP_NEW(DOM_ProgressEvent, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(progress_event, runtime, runtime->GetPrototype(DOM_Runtime::PROGRESSEVENT_PROTOTYPE), "ProgressEvent"));
	return OpStatus::OK;
}

void
DOM_ProgressEvent::InitProgressEvent(BOOL new_lengthComputable, OpFileLength new_loaded, OpFileLength new_total)
{
	lengthComputable = new_lengthComputable;
	loaded = new_loaded;
	total = new_total;
}

/* virtual */ ES_GetState
DOM_ProgressEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_lengthComputable:
		DOMSetBoolean(value, lengthComputable);
		return GET_SUCCESS;

	case OP_ATOM_loaded:
		DOMSetNumber(value, static_cast<double>(loaded));
		return GET_SUCCESS;

	case OP_ATOM_total:
		DOMSetNumber(value, static_cast<double>(total));
		return GET_SUCCESS;

	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

/* static */ int
DOM_ProgressEvent::initProgressEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_PROGRESSEVENT, DOM_ProgressEvent);
	DOM_CHECK_ARGUMENTS("sbbbnn");

	event->lengthComputable = argv[3].value.boolean;

	if (op_isfinite(argv[4].value.number) && argv[4].value.number >= 0)
		event->loaded = static_cast<OpFileLength>(argv[4].value.number);
	else
		event->loaded = 0;

	if (op_isfinite(argv[5].value.number) && argv[5].value.number >= 0)
		event->total = static_cast<OpFileLength>(argv[5].value.number);
	else
		event->lengthComputable = FALSE;

	if (!event->lengthComputable)
		event->total = 0;

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

/* virtual */ int
DOM_ProgressEvent_Constructor::Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s|o");

	DOM_ProgressEvent *progress_event;

	CALL_FAILED_IF_ERROR(DOM_ProgressEvent::Make(progress_event, static_cast<DOM_Runtime *>(origining_runtime)));

	progress_event->SetSynthetic();
	progress_event->SetEventPhase(ES_PHASE_AT_TARGET);

	CALL_FAILED_IF_ERROR(progress_event->SetType(argv[0].value.string));

	if (argc >= 2)
	{
		ES_Object *eventInitDict = argv[1].value.object;

		progress_event->SetBubbles(DOMGetDictionaryBoolean(eventInitDict, UNI_L("bubbles"), FALSE));
		progress_event->SetCancelable(DOMGetDictionaryBoolean(eventInitDict, UNI_L("cancelable"), FALSE));
		progress_event->lengthComputable = DOMGetDictionaryBoolean(eventInitDict, UNI_L("lengthComputable"), FALSE);

		double loaded = DOMGetDictionaryNumber(eventInitDict, UNI_L("loaded"), 0);
		if (op_isfinite(loaded) && loaded >= 0)
			progress_event->loaded = static_cast<OpFileLength>(loaded);

		double total = DOMGetDictionaryNumber(eventInitDict, UNI_L("total"), 0);
		if (op_isfinite(total) && total >= 0)
			progress_event->total = static_cast<OpFileLength>(total);
		else
			progress_event->lengthComputable = FALSE;

		if (!progress_event->lengthComputable)
			progress_event->total = 0;
	}

	DOMSetObject(return_value, progress_event);
	return ES_VALUE;
}

/* static */ void
DOM_ProgressEvent_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_ProgressEvent_Constructor *event_constructor = OP_NEW_L(DOM_ProgressEvent_Constructor, ());
	object->PutConstructorL(event_constructor, "s{bubbles:b,cancelable:b,lengthComputable:b,loaded:n,total:n}-");
}

DOM_FUNCTIONS_START(DOM_ProgressEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_ProgressEvent, DOM_ProgressEvent::initProgressEvent, "initProgressEvent", "sbbbnn-")
DOM_FUNCTIONS_END(DOM_ProgressEvent)

#endif // PROGRESS_EVENTS_SUPPORT
