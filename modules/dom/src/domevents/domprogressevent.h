/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

// http://www.w3.org/TR/progress-events/

#ifndef DOM_PROGRESSEVENT_H
#define DOM_PROGRESSEVENT_H

#ifdef PROGRESS_EVENTS_SUPPORT

#include "modules/dom/src/domevents/domevent.h"

class DOM_ProgressEvent_Constructor;

class DOM_ProgressEvent
	: public DOM_Event
{
public:
	DOM_ProgressEvent()
		: lengthComputable(FALSE),
		  loaded(0),
		  total(0)
	{
	}

	static OP_STATUS Make(DOM_ProgressEvent *&progress_event, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_PROGRESSEVENT || DOM_Event::IsA(type); }

	void InitProgressEvent(BOOL lengthComputable, OpFileLength loaded, OpFileLength total);

	DOM_DECLARE_FUNCTION(initProgressEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

protected:
	friend class DOM_ProgressEvent_Constructor;

	BOOL lengthComputable;
	OpFileLength loaded;
	OpFileLength total;
};

class DOM_ProgressEvent_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_ProgressEvent_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::PROGRESSEVENT_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);
};

#endif // PROGRESS_EVENTS_SUPPORT
#endif // DOM_PROGRESSEVENT_H
