/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/dom/src/domperformance/domperformance.h"
#ifdef DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT

#include "modules/doc/frm_doc.h"

#ifdef DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT
/* static */ int
DOM_Performance::now(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(performance, DOM_TYPE_OBJECT, DOM_Object);
	double runtime_ms = g_op_time_info->GetRuntimeMS();
	double start_navigation_time = 0.0;
	if (FramesDocument* doc = performance->GetFramesDocument())
		start_navigation_time = doc->GetTopDocument()->GetStartNavigationTimeMS();
	DOMSetNumber(return_value, runtime_ms - start_navigation_time);
	return ES_VALUE;
}
#endif // DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Performance)
#ifdef DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT
	DOM_FUNCTIONS_FUNCTION(DOM_Performance, DOM_Performance::now, "now", NULL)
#endif // DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT
DOM_FUNCTIONS_END(DOM_Performance)

#endif // DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT
