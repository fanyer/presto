/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_ANIMATIONEVENT_H
#define DOM_ANIMATIONEVENT_H

#ifdef CSS_ANIMATIONS

#include "modules/dom/src/domevents/domevent.h"

class DOM_AnimationEvent
	: public DOM_Event
{
protected:
	double elapsed_time;
	OpString animation_name;

public:
	DOM_AnimationEvent(double el = 0) : elapsed_time(el) {}

	static OP_STATUS Make(DOM_Event*& event, double elapsed_time, const uni_char* animation_name, DOM_Runtime* runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_ANIMATIONEVENT || DOM_Event::IsA(type); }

	DOM_DECLARE_FUNCTION(initAnimationEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // CSS_ANIMATIONS
#endif // !DOM_ANIMATIONEVENT_H
