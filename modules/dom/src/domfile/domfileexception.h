/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_FILEEXCEPTION_H
#define DOM_FILEEXCEPTION_H

#include "modules/dom/src/domobj.h"

#include "modules/dom/src/domfile/domfileerror.h"

/**
 * The FileException interface in the W3C File API.
 */
class DOM_FileException : public DOM_FileError
{
private:
	DOM_FileException(ErrorCode code) : DOM_FileError(code) {}

public:
	/** Returns ES_VALUE or ES_NO_MEMORY. */
	static int CreateException(ES_Value* return_value, DOM_FileError::ErrorCode code, DOM_Runtime* runtime);

	/** Adding the constants for prototypes and constructors. */
	static void ConstructFileExceptionObjectL(ES_Object* object, DOM_Runtime* runtime);

	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

#endif // DOM_FILEEXCEPTION_H
