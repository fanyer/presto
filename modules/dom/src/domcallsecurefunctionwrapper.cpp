/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/dominternaltypes.h"
#ifdef DOM_ACCESS_SECURITY_RULES

#include "modules/dom/src/domcallsecurefunctionwrapper.h"

#include "modules/dom/src/domcallstate.h"

DOM_FunctionWithSecurityRuleWrapper::DOM_FunctionWithSecurityRuleWrapper(DOM_Object* wrapped_object, const char* operation_name)
	: m_wrapped_object(wrapped_object)
	, m_operation_name(operation_name)
{
	OP_ASSERT(wrapped_object);
	OP_ASSERT(m_operation_name);
}

void
DOM_FunctionWithSecurityRuleWrapper::GCTrace()
{
	GCMark(m_wrapped_object);
}

int
DOM_FunctionWithSecurityRuleWrapper::Call(ES_Object* es_this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* es_origining_runtime)
{
	DOM_Runtime* origining_runtime = static_cast<DOM_Runtime*>(es_origining_runtime);
	DOM_CHECK_OR_RESTORE_ARGUMENTS("");	// Required because PropertyAccessSecurityCheck may suspend

	int security_check_status = PropertyAccessSecurityCheck(return_value, argv, argc, origining_runtime, return_value, DOM_Object::ACCESS_TYPE_FUNCTION_INVOCATION, m_operation_name, NULL, NULL);
	if (security_check_status != ES_VALUE)
		return security_check_status;

	return m_wrapped_object->Call(es_this_object, argv, argc, return_value, es_origining_runtime);
}

int
DOM_FunctionWithSecurityRuleWrapper::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* es_origining_runtime)
{
	DOM_Runtime* origining_runtime = static_cast<DOM_Runtime*>(es_origining_runtime);
	DOM_CHECK_OR_RESTORE_ARGUMENTS("");	// Required because PropertyAccessSecurityCheck may suspend

	int security_check_status = PropertyAccessSecurityCheck(return_value, argv, argc, origining_runtime, return_value, DOM_Object::ACCESS_TYPE_FUNCTION_INVOCATION, m_operation_name, NULL, NULL);
	if (security_check_status != ES_VALUE)
		return security_check_status;

	return m_wrapped_object->Construct(argv, argc, return_value, es_origining_runtime);
}

#endif // DOM_ACCESS_SECURITY_RULES
