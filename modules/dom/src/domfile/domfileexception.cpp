/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

// Only workers use exceptions, the normal API uses async error messages.

#include "modules/dom/src/domfile/domfileexception.h"

/* static */ int
DOM_FileException::CreateException(ES_Value* return_value, DOM_FileError::ErrorCode code, DOM_Runtime* runtime)
{
	DOM_FileException* exception;
	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(exception = OP_NEW(DOM_FileException, (code)), runtime, runtime->GetPrototype(DOM_Runtime::FILEEXCEPTION_PROTOTYPE), "FileException"));
	DOMSetObject(return_value, exception);
	return ES_EXCEPTION;
}

/* static */ void
DOM_FileException::ConstructFileExceptionObjectL(ES_Object* object, DOM_Runtime* runtime)
{
	ConstructFileErrorObjectL(object, runtime);
}

/* virtual */ ES_PutState
DOM_FileException::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_code)
	{
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else
			// ToUint16()
			m_code = TruncateDoubleToUInt(value->value.number) & 0xffff;

		return PUT_SUCCESS;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}
