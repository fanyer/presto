/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILOBJECT_H
#define DOM_DOMJILOBJECT_H

#include "modules/dom/src/domsecurepropertyobject.h"

#include "modules/dom/src/domcallstate.h"

class DOM_JILObject : public DOM_SecurePropertyObject
{
protected:
	/**	 * Creates JIL exception.
	 * @param[in] exception_type
	 * @param[out] return_value Placeholder for the newly created exception.
	 * @param[in] runtime
	 * @param[in] message Message of the new exception. May be NULL, in this case a
	 * default message for given exception_type is set.
	 *
	 * @return ES_EXCEPTION on success
	 */
	static int CallException(DOM_Object::JILException exception_type, ES_Value* return_value, DOM_Runtime* runtime, const uni_char* message = NULL);

	/** Creates JIL exception for common error codes.
	 * @param[in] error_code
	 * @param[out] return_value [out] Placeholder for the newly created exception.
	 * @param[in] origining_runtime
	 *
	 * @return ES_EXCEPTION on success, ES_NO_MEMORY on ERR_NO_MEMORY, ES_FAILED if error not handled
	 */
	static int HandleJILError(OP_STATUS error_code, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/** Helper for PutName when handling with callbacks. Checks if input value
	 * is of proper type and places it in callback_storage.
	 * @param[in] input_value input
	 * @param[in] callback_storage variable in which the callback will be stored
	 *
	 * @return PUT_SUCCESS if successful
	 *	       DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR) - if the input type was wrong
	 */
	ES_PutState PutCallback(ES_Value* input_value, ES_Object*& callback_storage);

	/** Helper for calling callbacks. Calls callback object with argv parameters.
	 * @param[in] callback callback object to call
	 * @param[in] argv parameters to callback
	 * @param[in] argc number of parameters to callback
	 *
	 * @return PUT_SUCCESS if successful
	 *	       DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR) - if the input type was wrong
	 */
	OP_STATUS CallCallback(ES_Object* callback, ES_Value* argv, int argc);

	/// helper for storing date values in members
	ES_PutState PutDate(ES_Value* value, ES_Object*& storage);
};

#define RETURN_IF_ERROR_WITH_HANDLER(exp, handler, converter, return_value, origining_runtime)	\
do {																										\
	OP_STATUS __error = exp;																					\
	if (OpStatus::IsError(__error))																			\
		return converter(handler(__error, return_value, origining_runtime), return_value);					\
} while (0)

#define CHOOSE_FIRST(first, second) (first)

#define CALL_FAILED_IF_ERROR_WITH_HANDLER(exp, handler)											\
do {																							\
	RETURN_IF_ERROR_WITH_HANDLER(exp, handler, CHOOSE_FIRST, return_value, origining_runtime);			\
} while (0)

#define GET_FAILED_IF_ERROR_WITH_HANDLER(exp, handler)											\
do {																							\
	RETURN_IF_ERROR_WITH_HANDLER(exp, handler, ConvertCallToGetName, value, origining_runtime);	\
} while (0)

#define PUT_FAILED_IF_ERROR_WITH_HANDLER(exp, handler)											\
do {																							\
	RETURN_IF_ERROR_WITH_HANDLER(exp, handler, ConvertCallToPutName, value, origining_runtime);	\
} while (0)

#endif // DOM_DOMJILOBJECT_H
