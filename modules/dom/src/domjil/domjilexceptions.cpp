/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilexceptions.h"
#include "modules/dom/src/domenvironmentimpl.h"

class DOM_EnvironmentImpl;

DOM_JILException_Constructor::DOM_JILException_Constructor()
	: DOM_BuiltInConstructor(DOM_Runtime::JIL_EXCEPTION_PROTOTYPE)
{}

/*virtual*/ int
DOM_JILException_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	DOM_Runtime* dom_runtime = GetEnvironment()->GetDOMRuntime();
	CALL_FAILED_IF_ERROR(DOM_Object::CreateJILException(JIL_UNKNOWN_ERR, return_value, dom_runtime));
	return ES_VALUE;
}

#endif // DOM_JIL_API_SUPPORT
