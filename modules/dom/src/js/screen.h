/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _JS11_SCREEN_
#define _JS11_SCREEN_

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"

class JS_Screen
	: public DOM_Object
{
public:
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

#endif /* _JS11_WINDOW_ */
