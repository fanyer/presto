/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOM_PERFORMANCE_H
#define DOM_PERFORMANCE_H

#include "modules/dom/src/domobj.h"

#ifdef DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT
class DOM_Performance : public DOM_Object
{
public:
#ifdef DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT
	DOM_DECLARE_FUNCTION(now);
#endif // DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT

	enum { FUNCTIONS_ARRAY_BASIC
#ifdef DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT
	     , FUNCTIONS_now
#endif // DOM_HIGH_RESOLUTION_TIMER_API_SUPPORT
	     , FUNCTIONS_ARRAY_SIZE
	     };
};
#endif // DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT

#endif // DOM_PERFORMANCE_H
