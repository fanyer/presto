/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _JS11_HISTORY_
#define _JS11_HISTORY_

class DocumentManager;

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"

class JS_History
  : public DOM_Object
{
public:
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HISTORY || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION_WITH_DATA(walk); // back, forward and go
	DOM_DECLARE_FUNCTION_WITH_DATA(putState); // pushState and replaceState
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 6 };
};

#endif /* _JS11_HISTORY_ */
