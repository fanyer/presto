/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007- Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_TIMEEVENT_H
#define DOM_TIMEEVENT_H

# ifdef SVG_TINY_12

#include "modules/dom/src/domevents/domevent.h"

class DOM_TimeEvent
	: public DOM_Event
{
protected:
	DOM_Object *view;
	int detail;

public:
	DOM_TimeEvent();

	void InitTimeEvent(DOM_Object *view, int detail);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_TIMEEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(initTimeEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

# endif // SVG_TINY_12
#endif // DOM_TIMEEVENT_H
