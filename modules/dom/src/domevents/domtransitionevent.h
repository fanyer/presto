/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

// http://dev.w3.org/csswg/css3-transitions/

#ifndef DOM_TRANSITIONEVENT_H
#define DOM_TRANSITIONEVENT_H

#ifdef CSS_TRANSITIONS

#include "modules/dom/src/domevents/domevent.h"

class DOM_TransitionEvent
	: public DOM_Event
{
protected:
	double m_elapsed;
	short  m_property;

public:
	DOM_TransitionEvent(double elapsed, short property) : m_elapsed(elapsed), m_property(property) {}

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_TRANSITIONEVENT || DOM_Event::IsA(type); }

	DOM_DECLARE_FUNCTION(initTransitionEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // CSS_TRANSITIONS
#endif // DOM_TRANSITIONEVENT_H
