/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSEVENTS
#define DOM_LSEVENTS

#ifdef DOM3_LOAD

#include "modules/dom/src/domevents/domevent.h"

class DOM_LSParser;
class DOM_Document;
class ES_Object;

class DOM_LSLoadEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_LSLoadEvent *&event, DOM_LSParser *target, DOM_Document *document, ES_Object *input);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
};

class DOM_LSProgressEvent
	: public DOM_Event
{
protected:
	DOM_LSProgressEvent(unsigned position, unsigned total);

	unsigned position, total;

public:
	static OP_STATUS Make(DOM_LSProgressEvent *&event, DOM_LSParser *target, ES_Object *input, unsigned position, unsigned total);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
};

#endif // DOM3_LOAD
#endif // DOM_LSEVENTS
