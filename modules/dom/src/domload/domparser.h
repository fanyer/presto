/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMDOMPARSER_H
#define DOM_DOMDOMPARSER_H

#ifdef DOM3_LOAD

#include "modules/dom/src/domobj.h"

class DOM_DOMParser
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_DOMParser *&domparser, DOM_EnvironmentImpl *environment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOMPARSER || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(parseFromString);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_DOMParser_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_DOMParser_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::DOMPARSER_PROTOTYPE) {}
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM3_LOAD
#endif // DOM_DOMDOMPARSER_H
