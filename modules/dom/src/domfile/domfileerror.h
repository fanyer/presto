/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_FILEERROR_H
#define DOM_FILEERROR_H

#include "modules/dom/src/domobj.h"

/**
 * The FileError interface in the W3C File API.
 */
class DOM_FileError : public DOM_Object
{
public:
	enum ErrorCode
	{
		// Not from the spec.
		WORKING_JUST_FINE = 0,

		// From the spec.
		NOT_FOUND_ERR = 1,
		SECURITY_ERR = 2,
		ABORT_ERR = 3,
		NOT_READABLE_ERR = 4,
		ENCODING_ERR = 5
	};

protected:
	unsigned short m_code;

	DOM_FileError(ErrorCode code) : m_code(code) {}

public:

	static OP_STATUS Make(DOM_FileError*& file_error, ErrorCode code, DOM_Runtime* runtime);

	/** Adding the constants for prototypes and constructors. */
	static void ConstructFileErrorObjectL(ES_Object* object, DOM_Runtime* runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

#endif // DOM_FILEERROR_H
