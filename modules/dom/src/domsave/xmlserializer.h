/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_XMLSERIALIZER_H
#define DOM_XMLSERIALIZER_H

#ifdef DOM3_SAVE

#include "modules/dom/src/domobj.h"

class DOM_XMLSerializer
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_XMLSerializer *&xmlserializer, DOM_EnvironmentImpl *environment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XMLSERIALIZER || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(serializeToString);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_XMLSerializer_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_XMLSerializer_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::XMLSERIALIZER_PROTOTYPE) {}
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM3_SAVE
#endif // DOM_XMLSERIALIZER_H
