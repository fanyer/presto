/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMCALLSECUREFUNCTIONWRAPPER_H
#define DOM_DOMCALLSECUREFUNCTIONWRAPPER_H

#ifdef DOM_ACCESS_SECURITY_RULES

#include "modules/dom/src/domobj.h"

/**
 * A wrapper for ES function objects that adds a security check to the call.
 * It has a Construct method that forwards to the Call method so that it may be used to wrap a constructor object.
 */
class DOM_FunctionWithSecurityRuleWrapper
  : public DOM_Object
{
public:
	/**
	 * Constructs the wrapper.
	 *
	 * @param wrapped_object the object whose Call method will be protected by the security check
	 * @param operation_name name of the operation the wrapped object represents. Used only for security rule identification. Not copied.
	 */
	DOM_FunctionWithSecurityRuleWrapper(DOM_Object* wrapped_object, const char* operation_name);

	virtual ~DOM_FunctionWithSecurityRuleWrapper() { }

	virtual BOOL IsA(int type)
		{ return m_wrapped_object->IsA(type); }

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime)
		{ m_wrapped_object->DOMChangeRuntime(new_runtime); }

	/**
	 * Performs a security check and, provided that it has passed, calls the wrapped object's Call method.
	 */
	virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

	/**
	 * Calls the Call method. Intended to pose as ES object constructors.
	 */
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

	virtual BOOL SecurityCheck(ES_Runtime *origining_runtime)
		{ return m_wrapped_object->SecurityCheck(origining_runtime); }

	virtual BOOL SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
		{ return m_wrapped_object->SecurityCheck(property_name, value, origining_runtime); }

	virtual ES_DeleteStatus DeleteIndex(int property_index, ES_Runtime* origining_runtime)
		{ return m_wrapped_object->DeleteIndex(property_index, origining_runtime); }
	virtual ES_DeleteStatus DeleteName(const uni_char* property_name, ES_Runtime* origining_runtime)
		{ return m_wrapped_object->DeleteName(property_name, origining_runtime); }
	virtual BOOL AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
		{ return m_wrapped_object->AllowOperationOnProperty(property_name, op); }

	virtual BOOL TypeofYieldsObject() { return m_wrapped_object->TypeofYieldsObject(); }

	virtual void GCTrace();

protected:
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
		{ return m_wrapped_object->GetName(property_name, property_code, value, origining_runtime); }
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
		{ return m_wrapped_object->GetName(property_name, value, origining_runtime); }
	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
		{ return m_wrapped_object->GetNameRestart(property_name, property_code, value, origining_runtime, restart_object); }
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
		{ return m_wrapped_object->GetNameRestart(property_name, value, origining_runtime, restart_object); }

	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
		{ return m_wrapped_object->PutName(property_name, property_code, value, origining_runtime); }
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
		{ return m_wrapped_object->PutName(property_name, value, origining_runtime); }
	virtual ES_PutState PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
		{ return m_wrapped_object->PutNameRestart(property_name, property_code, value, origining_runtime, restart_object); }
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
		{ return m_wrapped_object->PutNameRestart(property_name, value, origining_runtime, restart_object); }

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
		{ return m_wrapped_object->FetchPropertiesL(enumerator, origining_runtime); }

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime* origining_runtime )
		{ return m_wrapped_object->GetIndexedPropertiesLength(count, origining_runtime); }

private:
	DOM_Object* m_wrapped_object;
	const char* m_operation_name;
};

#endif // DOM_ACCESS_SECURITY_RULES

#endif // DOM_DOMCALLSECUREFUNCTIONWRAPPER_H
