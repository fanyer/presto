/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CSS_TRANSITIONS

#include "modules/dom/src/domevents/domtransitionevent.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/style/css.h"
#include "modules/style/src/css_aliases.h"


/**
 * Helper function to keep the TRAP/LEAVE out of the regular code.
 */
static OP_STATUS FormatCssPropertyName(short property, TempBuffer* buf)
{
	OP_ASSERT(property > 0);
	TRAPD(status, CSS::FormatCssPropertyNameL(property, buf));
	return status;
}


/* virtual */ ES_GetState
DOM_TransitionEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_elapsedTime:
		DOMSetNumber(value, m_elapsed);
		return GET_SUCCESS;

	case OP_ATOM_propertyName:
		if (value)
		{
			if (m_property >= 0)
			{
				TempBuffer* buf = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(FormatCssPropertyName(m_property, buf));
				DOMSetString(value, buf);
			}
			else
				DOMSetString(value);
		}

		return GET_SUCCESS;

	default:
		break;
	}
	return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* static */ int
DOM_TransitionEvent::initTransitionEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_TRANSITIONEVENT, DOM_TransitionEvent);
	DOM_CHECK_ARGUMENTS("sbbsn");

	const uni_char* prop_name = argv[3].value.string;
	event->m_property = static_cast<short>(GetAliasedProperty(GetCSS_Property(prop_name, uni_strlen(prop_name))));

	if (argv[4].value.number >= 0)
		event->m_elapsed = argv[4].value.number;
	else
		event->m_elapsed = 0;

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

DOM_FUNCTIONS_START(DOM_TransitionEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_TransitionEvent, DOM_TransitionEvent::initTransitionEvent, "initTransitionEvent", "sbbsn")
DOM_FUNCTIONS_END(DOM_TransitionEvent)

#endif // CSS_TRANSITIONS
