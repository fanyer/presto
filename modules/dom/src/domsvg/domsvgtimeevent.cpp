/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007- Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_TINY_12

#include "modules/dom/src/domsvg/domsvgtimeevent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domdefines.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventdata.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domglobaldata.h"

#include "modules/ecmascript_utils/essched.h"
#include "modules/doc/frm_doc.h"

#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

DOM_TimeEvent::DOM_TimeEvent()
	: view(NULL),
	  detail(0)
{
}

void
DOM_TimeEvent::InitTimeEvent(DOM_Object *new_view, int new_detail)
{
	view = new_view;
	detail = new_detail;
}

/* virtual */ ES_GetState
DOM_TimeEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_view)
	{
		DOMSetObject(value, view);
		return GET_SUCCESS;
	}
	else if (property_name == OP_ATOM_detail)
	{
		DOMSetNumber(value, detail);
		return GET_SUCCESS;
	}
	else
		return DOM_Event::GetName(property_name, value, origining_runtime);
}

/* static */ int
DOM_TimeEvent::initTimeEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event, DOM_TYPE_TIMEEVENT, DOM_TimeEvent);
	DOM_CHECK_ARGUMENTS("sOn");

	ES_Value& view_argument = argv[3];
	if (view_argument.type == VALUE_OBJECT)
	{
		DOM_Object *view = DOM_VALUE2OBJECT(view_argument, DOM_Object);
		if (view && view->IsA(DOM_TYPE_WINDOW))
			event->view = view;
	}

	event->detail = (int) argv[4].value.number;

	return initEvent(this_object, argv, argc, return_value, origining_runtime);
}

DOM_FUNCTIONS_START(DOM_TimeEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_TimeEvent, DOM_TimeEvent::initTimeEvent, "initTimeEvent", "s-n-")
DOM_FUNCTIONS_END(DOM_TimeEvent)

/* virtual */ void
DOM_TimeEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(view);
}

#endif // SVG_TINY_12
