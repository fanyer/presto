/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domload/lsexception.h"

/* static */ void
DOM_LSException::ConstructLSExceptionObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	ES_Value val;
	val.type = VALUE_NUMBER;

	val.value.number = PARSE_ERR;
	DOM_Object::PutL(object, "PARSE_ERR", val, runtime);

	val.value.number = SERIALIZE_ERR;
	DOM_Object::PutL(object, "SERIALIZE_ERR", val, runtime);
}

/* static */ int
DOM_LSException::CallException(DOM_Object *object, Code code, ES_Value *value)
{
	OP_ASSERT(code == SERIALIZE_ERR || code == PARSE_ERR);
	const char *message = code == SERIALIZE_ERR ? "SERIALIZE_ERR" : "PARSE_ERR";
	return object->CallCustomException("LSException", message, code, value, object->GetRuntime()->GetPrototype(DOM_Runtime::LSEXCEPTION_PROTOTYPE));
}

#endif // DOM3_LOAD
