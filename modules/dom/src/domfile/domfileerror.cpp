/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domfile/domfileerror.h"


/* static */ OP_STATUS
DOM_FileError::Make(DOM_FileError*& file_error, ErrorCode code, DOM_Runtime* runtime)
{
	return DOMSetObjectRuntime(file_error = OP_NEW(DOM_FileError, (code)), runtime, runtime->GetPrototype(DOM_Runtime::FILEERROR_PROTOTYPE), "FileError");
}

/* static */ void
DOM_FileError::ConstructFileErrorObjectL(ES_Object* object, DOM_Runtime* runtime)
{
	PutNumericConstantL(object, "NOT_FOUND_ERR", NOT_FOUND_ERR, runtime);
	PutNumericConstantL(object, "SECURITY_ERR", SECURITY_ERR, runtime);
	PutNumericConstantL(object, "ABORT_ERR", ABORT_ERR, runtime);
	PutNumericConstantL(object, "NOT_READABLE_ERR", NOT_READABLE_ERR, runtime);
	PutNumericConstantL(object, "ENCODING_ERR", ENCODING_ERR, runtime);
}

/* virtual */ ES_GetState
DOM_FileError::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_code)
	{
		DOMSetNumber(value, m_code);
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_FileError::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_code)
		return PUT_SUCCESS;

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

