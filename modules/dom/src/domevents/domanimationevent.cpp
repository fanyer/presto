/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CSS_ANIMATIONS

#include "modules/dom/src/domevents/domanimationevent.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/style/css.h"

/* virtual */ ES_GetState
DOM_AnimationEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_elapsedTime:
		DOMSetNumber(value, elapsed_time);
		return GET_SUCCESS;

	case OP_ATOM_animationName:
		DOMSetString(value, animation_name.CStr());
		return GET_SUCCESS;

	default:
		break;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_AnimationEvent::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_elapsedTime:
	case OP_ATOM_animationName:
		return PUT_READ_ONLY;
	default:
		break;
	}
	return DOM_Event::PutName(property_name, value, origining_runtime);
}

/* static */ int
DOM_AnimationEvent::initAnimationEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_ANIMATIONEVENT, DOM_AnimationEvent);
	DOM_CHECK_ARGUMENTS("sbbsn");

	CALL_FAILED_IF_ERROR(event->animation_name.Set(argv[3].value.string));

	if (argv[4].value.number >= 0)
		event->elapsed_time = argv[4].value.number;
	else
		event->elapsed_time = 0;

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

/* static */ OP_STATUS
DOM_AnimationEvent::Make(DOM_Event*& event, double elapsed_time, const uni_char* animation_name, DOM_Runtime* runtime)
{
	DOM_AnimationEvent *animation_event;
	RETURN_IF_ERROR(DOMSetObjectRuntime(animation_event = OP_NEW(DOM_AnimationEvent, (elapsed_time)), runtime, runtime->GetPrototype(DOM_Runtime::ANIMATIONEVENT_PROTOTYPE), "AnimationEvent"));

	RETURN_IF_ERROR(animation_event->animation_name.Set(animation_name));

	event = animation_event;
	return OpStatus::OK;
}

DOM_FUNCTIONS_START(DOM_AnimationEvent)
DOM_FUNCTIONS_FUNCTION(DOM_AnimationEvent, DOM_AnimationEvent::initAnimationEvent, "initAnimationEvent", "sbbsn")
DOM_FUNCTIONS_END(DOM_AnimationEvent)

#endif // CSS_ANIMATIONS
